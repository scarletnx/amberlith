#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""What the NVN drivers of games lack, read from the reports the mod made in them (docs/Mod.md).

    tools/mod-lacked.py [--csv FILE] REPORT...

A game's driver has the names of its own API version, and the mod looks up every version's. Each REPORT is a
report.jsonl of the mod, one driver's. For each, from the notes the mod leaves: the driver (its NVN API version, the
SDK and the build id of its module), the names it has no function for, and for each of those a case called, the
cases it made unavailable; then the lacked names no case calls. With more than one report, last, each lacked name
with the drivers that lack it. --csv writes a row for each lacked name and case (a row with no case for a name no
case calls): api, sdk, build id, function, case.

Exits 1, saying which and with what numbers, for a report with no note of a driver, for a list of lacked names that is
not whole (the note says how many, and its parts hold another number), and for a case the report has as unavailable
for a lacked function that no note names, or the other way round."""

import argparse
import csv
import json
import re
import sys

DRIVER = re.compile(r"^host: the NVN driver is the game's own, in a module of SDK (\S+) and build id (\S+)")
API = re.compile(r"^host: the game's NVN driver answers API (\d+\.\d+)$")
LACKED = re.compile(r"^host: the game's driver has no function for (\d+) of the (\d+) names this build looks up "
                    r"\((\d+) of (\d+)\): (.*)$")
NONE_LACKED = re.compile(r"^host: the game's driver has a function for each of the (\d+) names this build looks up$")
TOOK = re.compile(r"^host: (\w+), which the game's driver has not, made (\d+) cases? unavailable: (.*)$")
TOOK_MORE = re.compile(r"^host: (\w+), more of the cases it made unavailable: (.*)$")
DETAIL = "the game's driver has no "


class Refused(Exception):
    """A report this tool cannot read a driver from, with why."""


def records(text):
    """The report's lines as objects, a line that is no JSON object left out (a launch that ended inside one)."""
    out = []
    for line in text.splitlines():
        try:
            record = json.loads(line)
        except ValueError:
            continue
        if isinstance(record, dict):
            out.append(record)
    return out


def quoted(text):
    """The names of a note's list: "one"; "two"."""
    return re.findall(r'"(.*?)"(?=; "|$)', text)


def unique(items):
    """Each item once, in the order of its first place: a run of several launches leaves its notes again."""
    seen, out = set(), []
    for item in items:
        if item not in seen:
            seen.add(item)
            out.append(item)
    return out


def driver(text, where):
    """One report's driver: {api, sdk, build, names, lacked, took, uncalled}."""
    notes = [r.get("text", "") for r in records(text) if r.get("type") == "note"]
    found = {"api": None, "sdk": None, "build": None, "names": None}
    lacked, parts, told = [], {}, None
    took, declared = {}, {}
    for note in notes:
        m = DRIVER.match(note)
        if m:
            found["sdk"], found["build"] = m.group(1), m.group(2)
            continue
        m = API.match(note)
        if m:
            found["api"] = m.group(1)
            continue
        m = NONE_LACKED.match(note)
        if m:
            found["names"], told = int(m.group(1)), 0
            continue
        m = LACKED.match(note)
        if m:
            told, found["names"] = int(m.group(1)), int(m.group(2))
            parts[int(m.group(3))] = (int(m.group(4)), m.group(5).split(", "))
            continue
        m = TOOK.match(note)
        if m:
            declared[m.group(1)] = int(m.group(2))
            took.setdefault(m.group(1), []).extend(quoted(m.group(3)))
            continue
        m = TOOK_MORE.match(note)
        if m:
            took.setdefault(m.group(1), []).extend(quoted(m.group(2)))
    if found["sdk"] is None or told is None:
        raise Refused("%s has no note of a game's driver and its lacked names: not a report of the mod's nvn family"
                      % where)
    for part in sorted(parts):
        of, names = parts[part]
        if of != len(parts):
            raise Refused("%s: the lacked names are note %d of %d, and the report has %d of them" % (
                where, part, of, len(parts)))
        lacked.extend(names)
    lacked = unique(lacked)
    if len(lacked) != told:
        raise Refused("%s: the notes say the driver lacks %d names, and their lists hold %d" % (
            where, told, len(lacked)))
    took = {function: unique(cases) for function, cases in took.items()}
    for function, cases in took.items():
        if function not in lacked:
            raise Refused("%s: %s made cases unavailable, and the lacked names do not have it" % (where, function))
        if len(cases) != declared.get(function, len(cases)):
            raise Refused("%s: %s made %d cases unavailable by its note, whose list holds %d" % (
                where, function, declared[function], len(cases)))
    by_line = unique(r.get("name", "") for r in records(text)
                     if r.get("type") == "case" and r.get("result") == "skip" and r.get("reason") == "unavailable"
                     and r.get("detail", "").startswith(DETAIL))
    by_note = unique(case for cases in took.values() for case in cases)
    if sorted(by_line) != sorted(by_note):
        only_line = sorted(set(by_line) - set(by_note))
        only_note = sorted(set(by_note) - set(by_line))
        raise Refused("%s: %d cases are unavailable for a lacked function by their lines and %d by the notes: "
                      "%s" % (where, len(by_line), len(by_note), "; ".join(
                          ['only its line: "%s"' % c for c in only_line] +
                          ['only a note: "%s"' % c for c in only_note])))
    found.update(lacked=lacked, took=took, cases=by_line,
                 uncalled=[name for name in lacked if name not in took])
    return found


def title(d):
    return "NVN API %s, SDK %s, build id %s" % (d["api"] or "unknown", d["sdk"], d["build"])


def described(d, where):
    """The lines for one driver."""
    out = ["driver: %s (%s)" % (title(d), where)]
    out.append("  lacks %d of the %d names the mod looks up; the cases call %d of them, which made %d case%s "
               "unavailable" % (len(d["lacked"]), d["names"], len(d["took"]), len(d["cases"]),
                               "" if len(d["cases"]) == 1 else "s"))
    for function in [name for name in d["lacked"] if name in d["took"]]:
        cases = d["took"][function]
        out.append("  %s: %d case%s" % (function, len(cases), "" if len(cases) == 1 else "s"))
        out.extend("      %s" % case for case in cases)
    if d["uncalled"]:
        out.append("  lacked, and called by no case (%d): %s" % (len(d["uncalled"]), ", ".join(d["uncalled"])))
    return out


def across(drivers):
    """Each lacked name with the drivers that lack it, a name more drivers lack first, then by name."""
    lacking = {}
    for d in drivers:
        for name in d["lacked"]:
            lacking.setdefault(name, []).append("%s (SDK %s)" % (d["api"] or "unknown", d["sdk"]))
    out = ["lacked names, and the drivers that lack each:"]
    out.extend("  %s: %s" % (name, ", ".join(who))
               for name, who in sorted(lacking.items(), key=lambda item: (-len(item[1]), item[0])))
    return out


def rows(drivers):
    """The CSV's rows: a lacked name with each case it took, or with none."""
    out = [("api", "sdk", "build id", "function", "case")]
    for d in drivers:
        for name in d["lacked"]:
            for case in d["took"].get(name, [""]):
                out.append((d["api"] or "", d["sdk"], d["build"], name, case))
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("reports", nargs="+", metavar="REPORT")
    parser.add_argument("--csv")
    args = parser.parse_args()
    drivers, lines = [], []
    try:
        for path in args.reports:
            with open(path, encoding="utf-8", errors="replace") as f:
                d = driver(f.read(), path)
            drivers.append(d)
            lines.extend(described(d, path))
    except Refused as refused:
        print("mod-lacked: %s" % refused, file=sys.stderr)
        return 1
    if len(drivers) > 1:
        lines.extend(across(drivers))
    print("\n".join(lines))
    if args.csv:
        with open(args.csv, "w", encoding="utf-8", newline="") as f:
            csv.writer(f).writerows(rows(drivers))
    return 0


if __name__ == "__main__":
    sys.exit(main())
