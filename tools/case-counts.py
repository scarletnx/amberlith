#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""The probe's cases, how many each suite and family has and how many of them a judge holds to an answer.

    tools/case-counts.py catalogue BUILD=REPORT... [--root DIR]
    tools/case-counts.py write [--check] [--root DIR]
    tools/case-counts.py unjudged [FAMILY] [--root DIR]

catalogue reads the reports of a run of each build (BUILD one of nro, ounce, mod, apk, host; the report.jsonl a run
left, the host build's edenprobe-host-report.jsonl) and writes docs/cases.tsv: a line a case, its suite, its family,
its judge and the builds that ran it. A build not given keeps its cases as the catalogue has them, so the host
report alone updates the host's part. A case's judge comes from its lines: the tiers that compare what the machine
produced (exact, ulp, position, level, bound), `none` when it passed comparing nothing (a device that opens, a
measurement), `unknown` when its lines only failed or skipped with no tier; a judge the catalogue had stays when
the new lines say nothing either way.

write puts the counts into every block of docs/ between `<!-- counts: KIND [NAME] -->` and `<!-- /counts -->`:
`families` a table of every family by category, `category NAME` a category's line, `family NAME` a table of its
suites, `suite NAME` a suite's line. --check writes nothing and exits 1 naming each block that is not what the
catalogue gives.

unjudged lists the cases with no judge, or none known yet, of a family or of all.

Anything it cannot place stops it, named: a suite no family takes, a build that is not one, a host report given as
another build's or another's as the host's, a block of a kind or a name there is not, a case name with a tab.
"""

import argparse
import collections
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CATALOGUE = "docs/cases.tsv"
UI = "source/ui.c"
DOCS = "docs"

BUILDS = ("nro", "ounce", "mod", "apk", "host")
# The tiers that compare what the machine produced, in source/report.h's order.
TIERS = ("exact", "ulp", "position", "level", "bound")
BUILD_WORDS = {
    "nro": "the NRO",
    "ounce": "the Switch 2 build",
    "mod": "the mod",
    "apk": "Android",
    "host": "the host build",
}
FROM_WORDS = {"console": "the console", "astcenc": "astcenc", "spec": "the specification"}

# Suites ui.c's FAMILIES has no row for: the Switch 2 build's own family, and Android's two.
EXTRA_FAMILIES = {"nvn2": "nvn2", "jni": "jni", "framework": "framework"}
# What a family is called in a table, and its page under docs/families/.
FAMILY_PAGES = collections.OrderedDict(
    [
        ("judge", ("The judge", "Judge.md")),
        ("cpu", ("The processor", "Cpu.md")),
        ("kernel", ("Kernel", "Kernel.md")),
        ("graphics", ("Graphics", "Graphics.md")),
        ("nvn", ("nvn", "Nvn.md")),
        ("nvn2", ("nvn2", "Nvn2.md")),
        ("game", ("Game", "Game.md")),
        ("demo", ("Demo", "Demo.md")),
        ("video", ("Video", "Video.md")),
        ("audio", ("Audio", "Audio.md")),
        ("music", ("Music", "Music.md")),
        ("fs", ("fs", "Fs.md")),
        ("display", ("Display", "Display.md")),
        ("service", ("Service", "Service.md")),
        ("net", ("Net", "Net.md")),
        ("libc", ("libc", "Libc.md")),
        ("jni", ("jni", "../Android.md")),
        ("framework", ("framework", "../Android.md")),
        ("pad", ("Pad", "Pad.md")),
        ("pace", ("Pace", "Pace.md")),
    ]
)
# The categories docs/README.md groups the families by. The judge holds the probe's own code to its references,
# so its cases are counted apart from the ones that judge a machine.
CATEGORIES = collections.OrderedDict(
    [
        ("cpu", ("CPU and kernel", ("cpu", "kernel"))),
        ("gpu", ("GPU and NVN", ("graphics", "nvn", "nvn2", "game", "demo"))),
        ("media", ("Video and audio", ("video", "audio", "music"))),
        ("system", ("System and services", ("fs", "display", "service", "net", "libc", "jni", "framework"))),
        ("input", ("Input", ("pad",))),
        ("speed", ("Speed", ("pace",))),
        ("tools", ("The judge and the tools", ("judge",))),
    ]
)
OWN = "judge"

BLOCK = re.compile(r"<!-- counts: ([a-z]+)(?: ([a-z0-9-]+))? -->\n(.*?)<!-- /counts -->\n?", re.S)


class Refused(Exception):
    """Something the tool cannot place; its text says what and where."""


def read_families(root):
    """Suite to family, in ui.c's FAMILIES order, then the suites it has no row for."""
    with open(os.path.join(root, UI), encoding="utf-8") as file:
        text = file.read()
    start = text.find("FAMILIES[]")
    if start < 0:
        raise Refused("%s: no FAMILIES[] table" % UI)
    begin = text.index("{", start)
    depth = 0
    for end in range(begin, len(text)):
        depth += {"{": 1, "}": -1}.get(text[end], 0)
        if depth == 0:
            break
    rows = re.findall(r'\{\s*"([a-z0-9-]+)",\s*UI_FAMILY_([A-Z0-9]+)\s*\}', text[begin : end + 1])
    if not rows:
        raise Refused("%s: FAMILIES[] has no rows this tool can read" % UI)
    families = collections.OrderedDict((suite, family.lower()) for suite, family in rows)
    for suite, family in EXTRA_FAMILIES.items():
        families.setdefault(suite, family)
    for family in set(families.values()):
        if family not in FAMILY_PAGES:
            raise Refused("%s: family %s has no page in case-counts.py's FAMILY_PAGES" % (UI, family))
    return families


Case = collections.namedtuple("Case", "suite name family judge source builds")


def read_report(path, build):
    """Every case line of a report: (suite, name, tier, result, expected_from)."""
    lines = []
    host = False
    with open(path, encoding="utf-8") as file:
        for number, line in enumerate(file, 1):
            line = line.strip()
            if not line:
                continue
            try:
                entry = json.loads(line)
            except ValueError as error:
                raise Refused("%s:%d: not a JSON line: %s" % (path, number, error))
            if entry.get("type") == "note" and entry.get("text") == "edenprobe host build":
                host = True
            if entry.get("type") != "case":
                continue
            suite, name = entry.get("suite"), entry.get("name")
            if not isinstance(suite, str) or not isinstance(name, str):
                raise Refused("%s:%d: a case line with no suite or no name" % (path, number))
            if "\t" in name or "\n" in name or "\t" in suite:
                raise Refused("%s:%d: case %r of %s has a tab or a line break in it" % (path, number, name, suite))
            lines.append((suite, name, entry.get("tier", "none"), entry.get("result"), entry.get("expected_from")))
    if build == "host" and not host:
        raise Refused("%s: given as host, and its notes have no 'edenprobe host build': another build's report" % path)
    if build != "host" and host:
        raise Refused("%s: given as %s, and it is the host build's report" % (path, build))
    if not lines:
        raise Refused("%s: no case lines" % path)
    return lines


def judge_of(lines):
    """A case's judge from its lines, or None when they say nothing either way."""
    tiers = {tier for _, _, tier, _, _ in lines if tier in TIERS}
    if tiers:
        return "+".join(tier for tier in TIERS if tier in tiers)
    if any(tier == "none" and result == "pass" for _, _, tier, result, _ in lines):
        return "none"
    return None


def read_catalogue(root):
    path = os.path.join(root, CATALOGUE)
    cases = collections.OrderedDict()
    if not os.path.exists(path):
        return cases, []
    header = []
    with open(path, encoding="utf-8") as file:
        for number, line in enumerate(file, 1):
            line = line.rstrip("\n")
            if line.startswith("#"):
                header.append(line)
                continue
            fields = line.split("\t")
            if fields[0] == "suite":
                continue
            if len(fields) != 6:
                raise Refused("%s:%d: %d fields, want 6" % (CATALOGUE, number, len(fields)))
            case = Case(fields[0], fields[1], fields[2], fields[3], fields[4], tuple(fields[5].split(",")))
            cases[(case.suite, case.name)] = case
    return cases, header


def catalogue(root, given):
    """The catalogue from the reports @p given ({build: [path]}) over the one in the tree."""
    families = read_families(root)
    old, old_header = read_catalogue(root)
    evidence = collections.OrderedDict()
    builds_of = collections.defaultdict(set)
    sources = []
    for build, paths in given.items():
        for path in paths:
            lines = read_report(path, build)
            sources.append("# %s: %s" % (build, describe(path)))
            for line in lines:
                key = (line[0], line[1])
                evidence.setdefault(key, []).append(line)
                builds_of[key].add(build)
    unplaced = sorted({suite for suite, _ in evidence if suite not in families})
    if unplaced:
        raise Refused(
            "suites no family takes: %s; give each a row in %s's FAMILIES (or EXTRA_FAMILIES)"
            % (", ".join(unplaced), UI)
        )
    kept = [line for line in old_header if line.startswith("# ") and line[2:].split(":")[0] in BUILDS]
    kept = [line for line in kept if line[2:].split(":")[0] not in given]
    cases = {}
    for key, case in old.items():
        builds = tuple(b for b in case.builds if b not in given)
        if builds:
            cases[key] = case._replace(family=families.get(case.suite, case.family), builds=builds)
    for key, lines in evidence.items():
        before = old.get(key)
        judge = judge_of(lines) or (before.judge if before else "unknown")
        found = sorted({source for *_, source in lines if source})
        source = ",".join(found) if found else (before.source if before else "-")
        builds = set(builds_of[key]) | set(cases[key].builds if key in cases else ())
        cases[key] = Case(key[0], key[1], families[key[0]], judge, source, tuple(b for b in BUILDS if b in builds))
    order = {suite: i for i, suite in enumerate(families)}
    first = {key: i for i, key in enumerate(list(old) + [k for k in evidence if k not in old])}
    rows = sorted(cases.values(), key=lambda c: (order.get(c.suite, len(order)), first[(c.suite, c.name)]))
    header = [
        "# The probe's cases, a line each: its suite, its name, its family, its judge (the tiers its lines compared",
        "# at, none when it passed comparing nothing, unknown when it only failed or skipped with no tier), where its",
        "# expected answer comes from when a line says, and the builds whose reports had it.",
        "# Written by tools/case-counts.py catalogue from these reports; the docs' counts by case-counts.py write.",
    ] + sorted(kept + sources)
    return header, rows


def describe(path):
    """A report's first notes: the version and the build, the machine the mode line names."""
    words = []
    with open(path, encoding="utf-8") as file:
        for line in file:
            entry = json.loads(line) if line.strip() else {}
            if entry.get("type") != "note":
                if entry.get("type") == "case":
                    break
                continue
            text = entry.get("text", "")
            if text.startswith("edenprobe "):
                words.append(text.replace(" starting", ""))
            match = re.search(r"machine ([a-z0-9-]+)(?:, version ([\w.-]+))?", text)
            if match and text.startswith("mode:"):
                words.append("machine " + match.group(1) + (" " + match.group(2) if match.group(2) else ""))
    return ", ".join(words) or os.path.basename(path)


def write_catalogue(root, header, rows):
    with open(os.path.join(root, CATALOGUE), "w", encoding="utf-8") as file:
        for line in header:
            file.write(line + "\n")
        file.write("suite\tcase\tfamily\tjudge\tfrom\tbuilds\n")
        for case in rows:
            file.write("\t".join((case.suite, case.name, case.family, case.judge, case.source, ",".join(case.builds))))
            file.write("\n")


class Tally:
    def __init__(self):
        self.cases = 0
        self.judged = 0
        self.none = 0
        self.unknown = 0
        self.tiers = collections.Counter()
        self.sources = collections.Counter()
        self.builds = collections.Counter()
        self.suites = []

    def add(self, case):
        self.cases += 1
        self.builds.update(case.builds)
        if case.suite not in self.suites:
            self.suites.append(case.suite)
        if case.judge == "none":
            self.none += 1
        elif case.judge == "unknown":
            self.unknown += 1
        else:
            self.judged += 1
            for tier in case.judge.split("+"):
                self.tiers[tier] += 1
        for source in case.source.split(","):
            if source != "-":
                self.sources[source] += 1


def tallies(rows, by):
    out = collections.OrderedDict()
    for case in rows:
        out.setdefault(by(case), Tally()).add(case)
    return out


def plural(n, word):
    if n == 1:
        return "%d %s" % (n, word)
    return "%d %s" % (n, word[:-1] + "ies" if word.endswith("y") else word + "s")


def judged_words(t):
    tiers = ", ".join("%s %d" % (tier, t.tiers[tier]) for tier in TIERS if t.tiers[tier])
    sources = ", ".join("%s %d" % (FROM_WORDS.get(s, s), n) for s, n in sorted(t.sources.items()))
    inside = tiers + ("; expected from " + sources if sources else "")
    return "%d with a judge%s" % (t.judged, " (%s)" % inside if inside else "")


def by_build(t):
    """What each build runs of these cases, when they do not all run every one."""
    if all(t.builds[b] in (0, t.cases) for b in BUILDS):
        return ""
    return "; %s" % ", ".join("%s runs %d" % (BUILD_WORDS[b], t.builds[b]) for b in BUILDS if t.builds[b])


def suite_line(t, own):
    if own:
        words = "%s, each held to an answer worked out by hand or by a reference program" % plural(t.cases, "case")
        if t.sources:
            words += " (%s)" % ", ".join("%s %d" % (FROM_WORDS.get(s, s), n) for s, n in sorted(t.sources.items()))
        return "**%s**: the probe's own code, no machine output judged%s." % (words, by_build(t))
    parts = [judged_words(t), "%d without one" % t.none]
    if t.unknown:
        parts.append("%d not yet seen judged" % t.unknown)
    return "**%s**: %s%s." % (plural(t.cases, "case"), ", ".join(parts), by_build(t))


def table(rows, first, by, link=None):
    head = "| %s | Cases | With a judge | Without one | Not yet known |\n|---|--:|--:|--:|--:|\n" % first
    body = ""
    total = Tally()
    for case in rows:
        total.add(case)
    for key, t in tallies(rows, by).items():
        name = link(key) if link else key
        body += "| %s | %d | %d | %d | %d |\n" % (name, t.cases, t.judged, t.none, t.unknown)
    body += "| **All** | **%d** | **%d** | **%d** | **%d** |\n" % (total.cases, total.judged, total.none, total.unknown)
    return head + body


def render(kind, name, rows, families):
    """The text a block holds, or Refused when the kind or the name is not one."""
    known_families = list(collections.OrderedDict.fromkeys(families.values()))
    if kind == "suite":
        if name not in families:
            raise Refused("suite %r is no suite of %s's FAMILIES nor EXTRA_FAMILIES" % (name, UI))
        mine = [c for c in rows if c.suite == name]
        if not mine:
            return "_No report in docs/cases.tsv has this suite yet._\n"
        t = Tally()
        for case in mine:
            t.add(case)
        return suite_line(t, families[name] == OWN) + "\n"
    if kind == "family":
        if name not in known_families:
            raise Refused("family %r is not one; the families are %s" % (name, ", ".join(known_families)))
        mine = [c for c in rows if c.family == name]
        if not mine:
            return "_No report in docs/cases.tsv has this family yet._\n"
        if name == OWN:
            builds = [b for b in BUILDS if any(b in c.builds for c in mine)]
            lines = ["| Suite | Cases | %s |" % " | ".join("On " + BUILD_WORDS[b] for b in builds)]
            lines.append("|---|--:|%s" % ("--:|" * len(builds)))
            total = Tally()
            for case in mine:
                total.add(case)
            for suite, t in list(tallies(mine, lambda c: c.suite).items()) + [("**All**", total)]:
                cells = " | ".join(("**%d**" if suite == "**All**" else "%d") % t.builds[b] for b in builds)
                cases = ("**%d**" if suite == "**All**" else "%d") % t.cases
                lines.append("| %s | %s | %s |" % (suite, cases, cells))
            return "\n".join(lines) + "\n"
        return table(mine, "Suite", lambda c: c.suite)
    if kind == "category":
        if name not in CATEGORIES:
            raise Refused("category %r is not one; the categories are %s" % (name, ", ".join(CATEGORIES)))
        members = CATEGORIES[name][1]
        mine = [c for c in rows if c.family in members]
        t = Tally()
        for case in mine:
            t.add(case)
        present = [f for f in members if any(c.family == f for c in mine)]
        if not mine:
            return "_No report in docs/cases.tsv has these families yet._\n"
        if members == (OWN,):
            return "_%s, %s: the probe's own code held to its references, before every run._\n" % (
                plural(len(t.suites), "suite"),
                plural(t.cases, "case"),
            )
        parts = [plural(len(present), "family"), plural(len(t.suites), "suite"), plural(t.cases, "case")]
        words = "%s: %s, %d without one" % (", ".join(parts), judged_words(t), t.none)
        if t.unknown:
            words += ", %d not yet seen judged" % t.unknown
        return "_%s._\n" % words
    if kind == "families":
        if name:
            raise Refused("families takes no name, and was given %r" % name)
        lines = [
            "| Category | Family | Suites | Cases | With a judge | Without one | Not yet known |",
            "|---|---|--:|--:|--:|--:|--:|",
        ]
        machine = Tally()
        for title, members in CATEGORIES.values():
            for family in members:
                mine = [c for c in rows if c.family == family]
                if not mine:
                    continue
                t = Tally()
                for case in mine:
                    t.add(case)
                label, page = FAMILY_PAGES[family]
                if family == OWN:
                    cells = "| %d | - | - | - |" % t.cases
                else:
                    for case in mine:
                        machine.add(case)
                    cells = "| %d | %d | %d | %d |" % (t.cases, t.judged, t.none, t.unknown)
                lines.append("| %s | [%s](%s) | %d %s" % (title, label, page, len(t.suites), cells))
        lines.append(
            "| **On a machine** | | **%d** | **%d** | **%d** | **%d** | **%d** |"
            % (len(machine.suites), machine.cases, machine.judged, machine.none, machine.unknown)
        )
        return "\n".join(lines) + "\n"
    raise Refused("block kind %r is not one; the kinds are families, category, family and suite" % kind)


def blocks(root):
    """Every (path, line, kind, name, text) of docs/."""
    found = []
    for directory, _, names in sorted(os.walk(os.path.join(root, DOCS))):
        for file_name in sorted(names):
            if not file_name.endswith(".md"):
                continue
            path = os.path.join(directory, file_name)
            with open(path, encoding="utf-8") as file:
                text = file.read()
            for match in BLOCK.finditer(text):
                line = text.count("\n", 0, match.start()) + 1
                found.append((path, line, match.group(1), match.group(2), match.group(3)))
    return found


def write(root, check):
    """The blocks written, or with @p check the ones that are stale, each as `path:line: kind name`."""
    families = read_families(root)
    # ui.c decides a suite's family, so moving one there moves its counts without a new catalogue
    rows = [c._replace(family=families.get(c.suite, c.family)) for c in read_catalogue(root)[0].values()]
    stale = []
    by_path = collections.OrderedDict()
    for path, line, kind, name, text in blocks(root):
        try:
            want = render(kind, name, rows, families)
        except Refused as error:
            raise Refused("%s:%d: %s" % (os.path.relpath(path, root), line, error))
        if want != text:
            stale.append("%s:%d: counts: %s%s" % (os.path.relpath(path, root), line, kind, " " + name if name else ""))
        by_path.setdefault(path, True)
    if check:
        return stale
    for path in by_path:
        with open(path, encoding="utf-8") as file:
            text = file.read()

        def fill(match):
            body = render(match.group(1), match.group(2), rows, families)
            name = " " + match.group(2) if match.group(2) else ""
            return "<!-- counts: %s%s -->\n%s<!-- /counts -->\n" % (match.group(1), name, body)

        new = BLOCK.sub(fill, text)
        if new != text:
            with open(path, "w", encoding="utf-8") as file:
                file.write(new)
    return stale


def unjudged(root, family):
    rows = list(read_catalogue(root)[0].values())
    families = list(collections.OrderedDict.fromkeys(read_families(root).values()))
    if family and family not in families:
        raise Refused("family %r is not one; the families are %s" % (family, ", ".join(families)))
    return [c for c in rows if c.judge in ("none", "unknown") and c.family != OWN and family in (None, c.family)]


def parse_given(items):
    given = collections.OrderedDict()
    for item in items:
        build, sep, path = item.partition("=")
        if not sep or not path:
            raise Refused("%r is not BUILD=REPORT" % item)
        if build not in BUILDS:
            raise Refused("build %r is not one; the builds are %s" % (build, ", ".join(BUILDS)))
        if not os.path.isfile(path):
            raise Refused("%s: no such report" % path)
        given.setdefault(build, []).append(path)
    return given


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--root", default=os.path.dirname(HERE), help="the probe's tree (this one)")
    commands = parser.add_subparsers(dest="command", required=True)
    make = commands.add_parser("catalogue", help="docs/cases.tsv from reports")
    make.add_argument("reports", nargs="+", metavar="BUILD=REPORT")
    out = commands.add_parser("write", help="the docs' blocks from docs/cases.tsv")
    out.add_argument("--check", action="store_true", help="write nothing; exit 1 naming each stale block")
    lack = commands.add_parser("unjudged", help="the cases with no judge, or none known yet")
    lack.add_argument("family", nargs="?")
    args = parser.parse_args(argv)
    try:
        if args.command == "catalogue":
            header, rows = catalogue(args.root, parse_given(args.reports))
            write_catalogue(args.root, header, rows)
            print("%s: %d cases" % (CATALOGUE, len(rows)))
            return 0
        if args.command == "write":
            stale = write(args.root, args.check)
            if args.check and stale:
                for line in stale:
                    print(line + ": not what %s gives; run tools/case-counts.py write" % CATALOGUE)
                return 1
            if not args.check and stale:
                print("rewrote %s" % plural(len(stale), "block"))
            return 0
        for case in unjudged(args.root, args.family):
            print("%s\t%s\t%s\t%s" % (case.family, case.suite, case.judge, case.name))
        return 0
    except Refused as error:
        print("case-counts.py: %s" % error, file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
