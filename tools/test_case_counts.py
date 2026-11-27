# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""tools/case-counts.py: the catalogue it makes from reports, the blocks it writes, what it refuses; and this tree's
docs/cases.tsv and blocks as current as the host build that just ran can tell.

host-run runs the binary before these tests, so amberlith-host-report.jsonl is this tree's: a case the host build
gains or loses fails here until docs/cases.tsv has it, and a block not rewritten after the catalogue changed fails
too."""

import collections
import contextlib
import importlib.util
import io
import json
import os
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SPEC = importlib.util.spec_from_file_location("case_counts", os.path.join(HERE, "case-counts.py"))
tool = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(tool)
HOST_REPORT = os.path.join(ROOT, "amberlith-host-report.jsonl")

UI = """static const struct { const char *suite; UiFamily family; } FAMILIES[] = {
    {"selfcheck", UI_FAMILY_JUDGE},
    {"clear", UI_FAMILY_GRAPHICS},
    {"depth", UI_FAMILY_GRAPHICS},
    {"fs", UI_FAMILY_FS},
};
"""

ORIGIN = """static const struct { const char *suite; ReportFrom from; } ORIGINS[] = {
    { "clear", REPORT_FROM_SPEC },
    { "depth", REPORT_FROM_REFERENCE },
    { "fs", REPORT_FROM_SPEC },
    { "nvn2", REPORT_FROM_SPEC },
    { "jni", REPORT_FROM_SPEC },
    { "framework", REPORT_FROM_SPEC },
    { "bionic", REPORT_FROM_SPEC },
    { "mod", REPORT_FROM_SPEC },
    { "gxm", REPORT_FROM_SPEC },
    { "gbavideo", REPORT_FROM_SPEC },
};
"""


def report(path, cases, host=False, name="Amberlith"):
    with open(path, "w", encoding="utf-8") as file:
        file.write(json.dumps({"type": "note", "text": name + (" host build" if host else " 9.9.9 (x) starting")}))
        file.write("\n")
        if not host:
            file.write(json.dumps({"type": "note", "text": "mode: quick; machine scarletnx, version abc1234"}) + "\n")
        for suite, name, tier, result, *source in cases:
            line = {"type": "case", "suite": suite, "name": name, "tier": tier, "result": result, "detail": ""}
            if source:
                line["expected_from"] = source[0]
            file.write(json.dumps(line) + "\n")
        file.write(json.dumps({"type": "summary", "passed": 0, "failed": 0, "skipped": 0, "crashed": 0}) + "\n")
    return path


class Tree:
    """A probe's tree with ui.c's table and docs/, in a folder of its own."""

    def __init__(self):
        self.folder = tempfile.TemporaryDirectory()
        self.root = self.folder.name
        os.makedirs(os.path.join(self.root, "source"))
        os.makedirs(os.path.join(self.root, "docs", "families"))
        with open(os.path.join(self.root, "source", "ui.c"), "w", encoding="utf-8") as file:
            file.write(UI)
        self.origins(ORIGIN)

    def origins(self, text):
        with open(os.path.join(self.root, "source", "origin.c"), "w", encoding="utf-8") as file:
            file.write(text)

    def path(self, *parts):
        return os.path.join(self.root, *parts)

    def doc(self, name, text):
        with open(self.path("docs", name), "w", encoding="utf-8") as file:
            file.write(text)

    def read(self, *parts):
        with open(self.path(*parts), encoding="utf-8") as file:
            return file.read()

    def catalogue(self, given):
        header, rows = tool.catalogue(self.root, given)
        tool.write_catalogue(self.root, header, rows)
        return {(c.suite, c.name): c for c in rows}


class Judge(unittest.TestCase):
    def test_the_tiers_a_case_compared_at_in_the_header_s_order(self):
        lines = [("s", "n", "ulp", "pass", None), ("s", "n", "exact", "fail", None), ("s", "n", "none", "fail", None)]
        self.assertEqual(tool.judge_of(lines), "exact+ulp")

    def test_a_pass_that_compared_nothing_has_no_judge(self):
        self.assertEqual(tool.judge_of([("s", "n", "none", "pass", None)]), "none")

    def test_a_fail_or_a_skip_with_no_tier_says_nothing_either_way(self):
        self.assertIsNone(tool.judge_of([("s", "n", "none", "fail", None), ("s", "n", "none", "skip", None)]))


class Catalogue(unittest.TestCase):
    def setUp(self):
        self.tree = Tree()
        self.addCleanup(self.tree.folder.cleanup)
        self.nro = report(
            self.tree.path("nro.jsonl"),
            [
                ("selfcheck", "integer arithmetic", "none", "pass"),
                ("clear", "graphics device opens", "none", "pass"),
                ("clear", "whole target red", "exact", "pass"),
                ("clear", "a rectangle", "none", "fail"),
                ("depth", "less at a half", "ulp", "pass", "console"),
                ("fs", "a read past the end", "none", "skip"),
            ],
        )
        self.host = report(
            self.tree.path("host.jsonl"),
            [("selfcheck", "integer arithmetic", "none", "pass"), ("selfcheck", "a host only seek", "none", "pass")],
            host=True,
        )

    def test_each_case_once_with_its_family_judge_source_and_builds(self):
        rows = self.tree.catalogue({"nro": [self.nro], "host": [self.host]})
        self.assertEqual(len(rows), 7)
        self.assertEqual(rows[("selfcheck", "integer arithmetic")].builds, ("nro", "host"))
        self.assertEqual(rows[("selfcheck", "a host only seek")].builds, ("host",))
        self.assertEqual(rows[("clear", "whole target red")][2:5], ("graphics", "exact", "-"))
        self.assertEqual(rows[("clear", "a rectangle")].judge, "unknown")
        self.assertEqual(rows[("depth", "less at a half")][3:5], ("ulp", "console"))
        self.assertEqual(rows[("fs", "a read past the end")][2:4], ("fs", "unknown"))
        self.assertIn("# nro: Amberlith 9.9.9 (x), machine scarletnx abc1234", self.tree.read("docs", "cases.tsv"))

    def test_a_report_of_a_build_from_before_amberlith_reads_as_one_of_amberlith(self):
        old_nro = report(self.tree.path("old.jsonl"), [("fs", "a read", "exact", "pass")], name="edenprobe")
        old_host = report(
            self.tree.path("old-host.jsonl"), [("selfcheck", "integer arithmetic", "none", "pass")], True, "edenprobe"
        )
        rows = self.tree.catalogue({"nro": [old_nro], "host": [old_host]})
        self.assertEqual(rows[("selfcheck", "integer arithmetic")].builds, ("host",))
        self.assertIn("# nro: edenprobe 9.9.9 (x), machine scarletnx abc1234", self.tree.read("docs", "cases.tsv"))
        with self.assertRaises(tool.Refused) as caught:
            tool.catalogue(self.tree.root, {"nro": [old_host]})
        self.assertIn("given as nro, and it is the host build", str(caught.exception))

    def test_the_rows_follow_ui_c_s_order_then_each_report_s(self):
        self.tree.catalogue({"host": [self.host], "nro": [self.nro]})
        text = self.tree.read("docs", "cases.tsv")
        suites = [line.split("\t")[0] for line in text.splitlines() if not line.startswith(("#", "suite\t"))]
        self.assertEqual(suites, ["selfcheck", "selfcheck", "clear", "clear", "clear", "depth", "fs"])

    def test_a_build_not_given_keeps_its_cases_and_a_judge_the_new_lines_do_not_settle(self):
        self.tree.catalogue({"nro": [self.nro], "host": [self.host]})
        again = report(self.tree.path("host2.jsonl"), [("selfcheck", "integer arithmetic", "none", "fail")], host=True)
        rows = self.tree.catalogue({"host": [again]})
        self.assertEqual(rows[("selfcheck", "integer arithmetic")].builds, ("nro", "host"))
        self.assertEqual(rows[("selfcheck", "integer arithmetic")].judge, "none")
        self.assertEqual(rows[("clear", "whole target red")].builds, ("nro",))
        self.assertNotIn(("selfcheck", "a host only seek"), rows)
        self.assertEqual(len(rows), 6)
        header = self.tree.read("docs", "cases.tsv")
        self.assertIn("# nro: Amberlith 9.9.9", header)
        self.assertEqual(header.count("# host: "), 1)

    def test_new_lines_that_settle_a_judge_replace_the_old_one(self):
        self.tree.catalogue({"nro": [self.nro]})
        judged = report(self.tree.path("nro2.jsonl"), [("clear", "a rectangle", "exact", "fail")])
        rows = self.tree.catalogue({"nro": [judged]})
        self.assertEqual(rows[("clear", "a rectangle")].judge, "exact")

    def test_refused(self):
        stray = report(self.tree.path("stray.jsonl"), [("warp", "a case", "exact", "pass")])
        cases = [
            (lambda: tool.catalogue(self.tree.root, {"nro": [stray]}), "suites no family takes: warp"),
            (lambda: tool.catalogue(self.tree.root, {"nro": [self.host]}), "given as nro, and it is the host build"),
            (lambda: tool.catalogue(self.tree.root, {"host": [self.nro]}), "given as host, and its notes have no"),
            (lambda: tool.parse_given(["switch=" + self.nro]), "build 'switch' is not one; the builds are nro"),
            (lambda: tool.parse_given([self.nro]), "is not BUILD=REPORT"),
            (lambda: tool.parse_given(["nro=" + self.tree.path("none.jsonl")]), "none.jsonl: no such report"),
        ]
        tabbed = report(self.tree.path("tab.jsonl"), [("clear", "a\tb", "exact", "pass")])
        cases.append((lambda: tool.catalogue(self.tree.root, {"nro": [tabbed]}), "has a tab or a line break"))
        for call, words in cases:
            with self.subTest(words=words):
                with self.assertRaises(tool.Refused) as caught:
                    call()
                self.assertIn(words, str(caught.exception))


class Blocks(unittest.TestCase):
    def setUp(self):
        self.tree = Tree()
        self.addCleanup(self.tree.folder.cleanup)
        nro = [
            ("selfcheck", "integer arithmetic", "none", "pass"),
            ("clear", "graphics device opens", "none", "pass"),
            ("clear", "whole target red", "exact", "pass", "spec"),
            ("clear", "whole target green", "exact", "pass", "console"),
            ("clear", "a rectangle", "none", "fail"),
            ("depth", "less at a half", "ulp", "pass", "reference"),
            ("fs", "a read", "exact", "pass", "spec"),
        ]
        self.tree.catalogue({"nro": [report(self.tree.path("nro.jsonl"), nro)]})
        self.rows = list(tool.read_catalogue(self.tree.root)[0].values())
        self.families = tool.read_families(self.tree.root)

    def render(self, kind, name=None):
        return tool.render(kind, name, self.rows, self.families)

    def test_a_suite_s_line(self):
        self.assertEqual(
            self.render("suite", "clear"),
            "**4 cases**: 2 with a judge (exact 2; expected from the console 1, the specification 1), 1 without"
            " one, 1 not yet seen judged.\n",
        )
        self.assertEqual(
            self.render("suite", "depth"),
            "**1 case**: 1 with a judge (ulp 1; expected from another reference 1), 0 without one.\n",
        )

    def test_the_judge_s_suites_are_the_probe_s_own(self):
        self.assertEqual(
            self.render("suite", "selfcheck"),
            "**1 case, each held to an answer worked out by hand or by a reference program**: the probe's own code,"
            " no machine output judged.\n",
        )

    def test_builds_that_run_different_cases_are_each_counted(self):
        host = [("selfcheck", "integer arithmetic", "none", "pass"), ("selfcheck", "a host only seek", "none", "pass")]
        self.tree.catalogue({"host": [report(self.tree.path("host.jsonl"), host, host=True)]})
        rows = list(tool.read_catalogue(self.tree.root)[0].values())
        self.assertEqual(
            tool.render("suite", "selfcheck", rows, self.families),
            "**2 cases, each held to an answer worked out by hand or by a reference program**: the probe's own code,"
            " no machine output judged; the NRO runs 1, the host build runs 2.\n",
        )
        self.assertEqual(
            tool.render("family", "judge", rows, self.families),
            "| Suite | Cases | On the NRO | On the host build |\n|---|--:|--:|--:|\n| selfcheck | 2 | 1 | 2 |\n"
            "| **All** | **2** | **1** | **2** |\n",
        )
        self.assertNotIn("runs", tool.render("suite", "clear", rows, self.families))

    def test_a_family_s_table(self):
        self.assertEqual(
            self.render("family", "graphics"),
            "| Suite | Cases | With a judge | Console | Driver | Reference | Emulator | Spec | Without one |"
            " Not yet known |\n|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|\n"
            "| clear | 4 | 2 | 1 | 0 | 0 | 0 | 1 | 1 | 1 |\n| depth | 1 | 1 | 0 | 0 | 1 | 0 | 0 | 0 | 0 |\n"
            "| **All** | **5** | **3** | **1** | **0** | **1** | **0** | **1** | **1** | **1** |\n",
        )

    def test_a_judged_case_that_says_no_source_is_counted_apart(self):
        nro = [("clear", "whole target blue", "exact", "pass"), ("depth", "less at a half", "ulp", "pass", "astcenc")]
        self.tree.catalogue({"nro": [report(self.tree.path("nro2.jsonl"), nro)]})
        rows = list(tool.read_catalogue(self.tree.root)[0].values())
        self.assertEqual(
            tool.render("suite", "clear", rows, self.families),
            "**1 case**: 1 with a judge (exact 1; expected from not said 1), 0 without one.\n",
        )
        self.assertEqual(
            tool.render("family", "graphics", rows, self.families),
            "| Suite | Cases | With a judge | Console | Driver | Reference | Emulator | Spec | Not said | Without one |"
            " Not yet known |\n|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|\n"
            "| clear | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 |\n| depth | 1 | 1 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 |\n"
            "| **All** | **2** | **2** | **0** | **0** | **1** | **0** | **0** | **1** | **0** | **0** |\n",
        )
        self.assertEqual([c.name for c in tool.unsaid(self.tree.root, None)], ["whole target blue"])
        self.assertEqual(tool.unsaid(self.tree.root, "fs"), [])

    def test_a_case_with_two_sources_counts_at_the_strongest(self):
        self.assertEqual(tool.strongest("spec,console"), "console")
        self.assertEqual(tool.strongest("emulator,reference"), "reference")
        self.assertEqual(tool.strongest("astcenc,spec"), "astcenc")
        self.assertEqual(tool.strongest("-"), None)

    def test_origin_c_has_a_row_for_every_machine_suite_and_none_for_the_judge(self):
        for text, words in (
            (ORIGIN.replace('    { "fs", REPORT_FROM_SPEC },\n', ""), "no row for fs"),
            (ORIGIN.replace("};", '    { "warp", REPORT_FROM_SPEC },\n};'), "rows for warp, which no family takes"),
            (ORIGIN.replace("};", '    { "selfcheck", REPORT_FROM_SPEC },\n};'), "rows for selfcheck, the judge's"),
            (ORIGIN.replace("REFERENCE", "HEARSAY"), "depth's source REPORT_FROM_HEARSAY is not one"),
            (ORIGIN.replace("};", '    { "fs", REPORT_FROM_CONSOLE },\n};'), "fs has two rows"),
            ("static const int nothing;\n", "no ORIGINS[] table"),
        ):
            with self.subTest(words=words):
                self.tree.origins(text)
                with self.assertRaises(tool.Refused) as caught:
                    tool.write(self.tree.root, check=True)
                self.assertIn(words, str(caught.exception))
                self.assertIn("source/origin.c", str(caught.exception))

    def test_a_category_s_line_and_the_families_table(self):
        self.assertEqual(
            self.render("category", "gpu"),
            "_1 family, 2 suites, 5 cases: 3 with a judge (exact 2, ulp 1; expected from the console 1, another"
            " reference 1, the specification 1), 1 without one, 1 not yet seen judged._\n",
        )
        table = self.render("families")
        self.assertIn(
            "| Category | Family | Suites | Cases | With a judge | Console | Driver | Reference | Emulator | Spec |"
            " Without one | Not yet known |\n|---|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|\n",
            table,
        )
        self.assertIn("| GPU and NVN | [Graphics](Graphics.md) | 2 | 5 | 3 | 1 | 0 | 1 | 0 | 1 | 1 | 1 |", table)
        self.assertIn(
            "| The judge and the tools | [The judge](Judge.md) | 1 | 1 | - | - | - | - | - | - | - | - |", table
        )
        self.assertIn(
            "| **On a machine** | | **3** | **6** | **4** | **1** | **0** | **1** | **0** | **2** | **1** | **1** |",
            table,
        )

    def test_plurals(self):
        self.assertEqual(
            [tool.plural(1, "family"), tool.plural(2, "family"), tool.plural(1, "case"), tool.plural(0, "suite")],
            ["1 family", "2 families", "1 case", "0 suites"],
        )

    def test_a_suite_or_family_no_report_had_yet(self):
        rows = [c for c in self.rows if c.suite != "fs"]
        self.assertIn("No report", tool.render("suite", "fs", rows, self.families))
        self.assertIn("No report", tool.render("family", "fs", rows, self.families))

    def test_refused_blocks(self):
        for kind, name, words in (
            ("suite", "warp", "suite 'warp' is no suite"),
            ("family", "warp", "family 'warp' is not one; the families are judge, graphics, fs"),
            ("category", "warp", "category 'warp' is not one; the categories are cpu, gpu"),
            ("families", "x", "families takes no name, and was given 'x'"),
            ("table", None, "block kind 'table' is not one"),
        ):
            with self.subTest(kind=kind):
                with self.assertRaises(tool.Refused) as caught:
                    self.render(kind, name)
                self.assertIn(words, str(caught.exception))

    def test_check_names_a_stale_block_and_write_makes_it_current(self):
        page = "# Graphics\n\n<!-- counts: family graphics -->\nold\n<!-- /counts -->\n\n## clear\n\n"
        page += "<!-- counts: suite clear -->\n<!-- /counts -->\nThe first cases.\n"
        self.tree.doc("Graphics.md", page)
        self.assertEqual(
            tool.write(self.tree.root, check=True),
            ["docs/Graphics.md:3: counts: family graphics", "docs/Graphics.md:9: counts: suite clear"],
        )
        tool.write(self.tree.root, check=False)
        self.assertEqual(tool.write(self.tree.root, check=True), [])
        text = self.tree.read("docs", "Graphics.md")
        self.assertIn("<!-- counts: suite clear -->\n**4 cases**: 2 with a judge", text)
        self.assertTrue(text.endswith("<!-- /counts -->\nThe first cases.\n"))

    def test_a_suite_ui_c_moves_to_another_family_moves_its_counts(self):
        self.tree.doc("Fs.md", "<!-- counts: family fs -->\n<!-- /counts -->\n")
        tool.write(self.tree.root, check=False)
        with open(self.tree.path("source", "ui.c"), "w", encoding="utf-8") as file:
            file.write(UI.replace('{"depth", UI_FAMILY_GRAPHICS}', '{"depth", UI_FAMILY_FS}'))
        self.assertEqual(tool.write(self.tree.root, check=True), ["docs/Fs.md:1: counts: family fs"])
        tool.write(self.tree.root, check=False)
        self.assertIn("| depth | 1 | 1 | 0 | 0 |", self.tree.read("docs", "Fs.md"))

    def test_a_refused_block_names_its_file_and_line(self):
        self.tree.doc("Graphics.md", "# Graphics\n\n<!-- counts: suite warp -->\n<!-- /counts -->\n")
        with self.assertRaises(tool.Refused) as caught:
            tool.write(self.tree.root, check=True)
        self.assertIn("docs/Graphics.md:3: suite 'warp'", str(caught.exception))

    def test_the_command_line(self):
        self.tree.doc("Fs.md", "<!-- counts: suite fs -->\n<!-- /counts -->\n")
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            self.assertEqual(tool.main(["--root", self.tree.root, "write", "--check"]), 1)
            self.assertEqual(tool.main(["--root", self.tree.root, "write"]), 0)
            self.assertEqual(tool.main(["--root", self.tree.root, "write", "--check"]), 0)
            self.assertEqual(tool.main(["--root", self.tree.root, "unjudged", "graphics"]), 0)
            self.assertEqual(tool.main(["--root", self.tree.root, "unjudged", "warp"]), 2)
            self.assertEqual(tool.main(["--root", self.tree.root, "unsaid"]), 0)
        self.assertIn("docs/Fs.md:1: counts: suite fs: not what docs/cases.tsv gives", out.getvalue())
        self.assertIn("graphics\tclear\tnone\tgraphics device opens", out.getvalue())
        self.assertIn("graphics\tclear\tunknown\ta rectangle", out.getvalue())
        self.assertNotIn("selfcheck", out.getvalue())
        self.assertIn("family 'warp' is not one", err.getvalue())


class ThisTree(unittest.TestCase):
    def test_every_block_is_what_the_catalogue_gives(self):
        self.assertEqual(tool.write(ROOT, check=True), [], "run tools/case-counts.py write")

    def test_every_family_the_catalogue_has_is_counted_on_its_page_and_in_the_index(self):
        rows = list(tool.read_catalogue(ROOT)[0].values())
        written = collections.Counter((kind, name) for _, _, kind, name, _ in tool.blocks(ROOT))
        for family in sorted({c.family for c in rows}):
            with self.subTest(family=family):
                self.assertGreaterEqual(written[("family", family)], 1, "no `counts: family %s` block" % family)
        for category in tool.CATEGORIES:
            with self.subTest(category=category):
                self.assertEqual(written[("category", category)], 1, "docs/README.md's `counts: category` blocks")
        self.assertEqual(written[("families", None)], 1)

    def test_the_host_build_ran_the_cases_the_catalogue_gives_it(self):
        if not os.path.exists(HOST_REPORT):
            self.fail("%s is missing: run make -f host.mk host-run" % HOST_REPORT)
        ran = collections.defaultdict(set)
        for suite, name, *_ in tool.read_report(HOST_REPORT, "host"):
            ran[suite].add(name)
        listed = collections.defaultdict(set)
        for case in tool.read_catalogue(ROOT)[0].values():
            if "host" in case.builds:
                listed[case.suite].add(case.name)
        for suite in sorted(set(ran) | set(listed)):
            with self.subTest(suite=suite):
                gained, lost = sorted(ran[suite] - listed[suite]), sorted(listed[suite] - ran[suite])
                self.assertEqual(
                    (gained, lost),
                    ([], []),
                    "%s: %s the host ran that docs/cases.tsv does not give it, %s it gives the host and the host did "
                    "not run; run tools/case-counts.py catalogue host=amberlith-host-report.jsonl, then write"
                    % (suite, tool.plural(len(gained), "case"), tool.plural(len(lost), "case")),
                )

    def test_the_host_build_s_lines_say_the_sources_the_catalogue_gives(self):
        if not os.path.exists(HOST_REPORT):
            self.fail("%s is missing: run make -f host.mk host-run" % HOST_REPORT)
        catalogue = tool.read_catalogue(ROOT)[0]
        for suite, name, tier, _, source in tool.read_report(HOST_REPORT, "host"):
            case = catalogue.get((suite, name))
            if case is None or tier not in tool.TIERS:
                continue
            with self.subTest(suite=suite, name=name):
                self.assertEqual(
                    tool.strongest(case.source),
                    source,
                    "the host's line says %s, docs/cases.tsv %s; run tools/case-counts.py catalogue "
                    "host=amberlith-host-report.jsonl, then write" % (source, case.source),
                )


if __name__ == "__main__":
    unittest.main()
