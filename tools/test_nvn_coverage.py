# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""tools/nvn-coverage.py reads the hook, the layer's lists and the cases, and docs/NvnCoverage.md is what it gives."""

import contextlib
import hashlib
import importlib.util
import io
import os
import shutil
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SPEC = importlib.util.spec_from_file_location("nvn_coverage", os.path.join(HERE, "nvn-coverage.py"))
tool = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(tool)

HOOK = """# a comment
include "nx/gpu/nvn/nvn.h"
route nvnBootstrapLoader

family device
hook nvnDeviceFinalize(NVNdevice* d)
hook nvnDeviceSetDebugLabel(NVNdevice* d, const char* label)
family queue
hook nvnQueueFenceSync(NVNqueue* q, NVNsync* s, i32 condition, i32 flags)
hook nvnQueuePresentTexture(NVNqueue* q, NVNwindow* w, i32 index)
hook nvnQueueFlush(NVNqueue* q)
family command_buffer
hook nvnCommandBufferDrawArrays(NVNcommandBuffer* cb, i32 mode, i32 first, i32 count)
hook nvnCommandBufferPopDebugGroupGraphics(NVNcommandBuffer* cb, u32 domain) = nvn_CommandBufferPopDebugGroupId
"""

# A suite in the shapes the probe's have: a case that leaves when it is not to run, cases in a brace, a helper that
# takes the case's name, a helper that takes a constant, a table of functions and a teardown outside every case.
SUITE = r"""
static const char *const SUITE = "toy";
typedef struct { void (*draw)(void); } Way;
static void TableDraw(void) { nvn.nvnCommandBufferDrawArrays(0, 0, 0, 3); }
static const Way WAY = { TableDraw };
static void Use(const Way *way);
typedef enum { KIND_A, KIND_B } Kind;

static bool BringUp(void) { NvnLoadProcs(&nvn, missing, 64); /* nvnNotACall( */ return nvn.nvnQueueInitialize(&queue, "nvnInAString("); }
static void TearDown(void) { nvn.nvnQueueFinalize(&queue); }

static void Copy(Kind kind) {
    if (kind == KIND_A) {
        nvn.nvnCommandBufferCopyBufferToBuffer(0, 0, 0, 0, 0);
    } else if (kind == KIND_B || kind == KIND_C) {
        nvn.nvnCommandBufferCopyTextureToTexture(0, 0, 0, 0, 0, 0, 0, 0);
    } else {
        nvn.nvnCommandBufferCopyTextureToBuffer(0, 0, 0, 0, 0, 0);
    }
    if (kind != KIND_A && later) {
        nvn.nvnCommandBufferBarrier(0, 0);
    }
}

static void CopyCase(const char *name, Kind kind) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    Copy(kind);
    if (failed) {
        ReportCase(SUITE, name, CHECK_NONE, false, "early");
        return;
    }
    nvn.nvnQueueFinish(&queue);
    CHECK_TRUE(SUITE, name, CHECK_EXACT, nvn.nvnQueueGetError(&queue) == 0, "wrong");
    nvn.nvnQueueFlush(&queue);
}

void RunToy(void) {
    static const char *const UP = "it comes "
                                  "up";
    static const char *const DRAW = "it draws";
    if (!ReportStart(SUITE, UP)) {
        TearDown();
        return;
    }
    const bool up = BringUp();
    CHECK_TRUE(SUITE, UP, CHECK_NONE, up, "no");
    nvn.nvnDeviceGetInteger(&device, 0, &value);
    CopyCase("a copies", KIND_A);
    CopyCase("b copies", KIND_B);
    if (ReportStart(SUITE, DRAW)) {
        Use(&WAY);
        ReportCase(SUITE, DRAW, CHECK_NONE, true, "");
    } else if (ReportStart(SUITE, "it is skipped")) {
        nvn.nvnDeviceFinalize(&device);
    }
#if NVN_LAYER_API < 231
    nvn.nvnWindowFinalize(&window);
#else
    nvn.nvnProgramFinalize(&program);
#endif
    Guarded(WAY_OTHER);
    TearDown();
}

void Guarded(int way) { nvn.nvnSyncWait(0, 0); }
"""


def walked(text=SUITE, api=55, roots=("RunToy",), guards=None):
    sources = tool.Sources()
    sources.add("toy.c", tool.tokens_of(text, api, "toy.c"))
    sources.finish()
    walk = tool.Walk(sources)
    old = dict(tool.GUARDS)
    try:
        tool.GUARDS.clear()
        tool.GUARDS.update(guards or {})
        for root in roots:
            walk.run(root)
    finally:
        tool.GUARDS.clear()
        tool.GUARDS.update(old)
    return walk


def cases(walk, name):
    return sorted("outside" if case is tool.OUTSIDE else case[1] for case in walk.calls.get(name, ()))


class Hook(unittest.TestCase):
    def test_names_and_families(self):
        self.assertEqual(tool.read_hook(HOOK)[:3], [("nvnBootstrapLoader", "route"), ("nvnDeviceFinalize", "device"),
                                                    ("nvnDeviceSetDebugLabel", "device")])
        self.assertEqual(len(tool.read_hook(HOOK)), 8)

    def test_a_line_of_no_known_kind_is_refused_with_its_place(self):
        with self.assertRaisesRegex(tool.Refused, r"nvn.hook:3: neither a hook.*'alias nvnA nvnB'"):
            tool.read_hook("family device\nhook nvnA()\nalias nvnA nvnB\n")

    def test_a_name_twice_is_refused_with_both_lines(self):
        with self.assertRaisesRegex(tool.Refused, r"nvn.hook:3: nvnA is also on line 2"):
            tool.read_hook("family device\nhook nvnA()\nhook nvnA(i32 x)\n")

    def test_a_hook_before_any_family_is_refused(self):
        with self.assertRaisesRegex(tool.Refused, r"nvn.hook:1: nvnA comes before any family"):
            tool.read_hook("hook nvnA()\n")


class Groups(unittest.TestCase):
    def test_a_family_is_a_group_and_four_groups_go_by_name(self):
        group = dict((name, tool.group_of(name, family)) for name, family in tool.read_hook(HOOK))
        self.assertEqual(group, {
            "nvnBootstrapLoader": "device",
            "nvnDeviceFinalize": "device",
            "nvnDeviceSetDebugLabel": "debug",
            "nvnQueueFenceSync": "sync and events",
            "nvnQueuePresentTexture": "windows and present",
            "nvnQueueFlush": "queues",
            "nvnCommandBufferDrawArrays": "commands: draws and dispatches",
            "nvnCommandBufferPopDebugGroupGraphics": "debug",
        })

    def test_dlss_keeps_its_own_whatever_their_names(self):
        self.assertEqual(tool.group_of("nvnDlssParametersSetOverlayDebugLayer", "dlss"), "dlss")
        self.assertEqual(tool.group_of("nvnCommandBufferEvaluateDlss", "dlss"), "dlss")

    def test_a_name_no_group_takes_is_refused(self):
        with self.assertRaisesRegex(tool.Refused, r"nvnShinyNew \(family shiny\): no group"):
            tool.group_of("nvnShinyNew", "shiny")
        with self.assertRaisesRegex(tool.Refused, r"nvnCommandBufferFrobnicate \(family command_buffer\)"):
            tool.group_of("nvnCommandBufferFrobnicate", "command_buffer")

    def test_every_group_has_a_title_and_a_place(self):
        named = {group for group, _, _ in tool.GROUPS}
        self.assertEqual(named, set(tool.ORDER))
        self.assertEqual(named, set(tool.TITLES))
        self.assertEqual(len(tool.ORDER), len(set(tool.ORDER)))


class Lists(unittest.TestCase):
    TEXT = ("#define NVN_LAYER_PROCS_BASE(X) \\\n    X(void, nvnA, (int a)) \\\n    X(NVNboolean, nvnB,\\\n"
            "      (int a, int b))\n\n#define NVN_LAYER_PROCS_231(X) \\\n    X(void, nvnC, (void))\n"
            "#define NVN_LAYER_PROCS(X) NVN_LAYER_PROCS_BASE(X) NVN_LAYER_PROCS_231(X)\n")

    def test_each_list_with_its_versions(self):
        self.assertEqual(tool.read_lists(self.TEXT), {"nvnA": (55, 231), "nvnB": (55, 231), "nvnC": (231,)})

    def test_a_list_the_tool_has_no_versions_for_is_refused(self):
        with self.assertRaisesRegex(tool.Refused, "the list NVN_LAYER_PROCS_NEW is not one of"):
            tool.read_lists("#define NVN_LAYER_PROCS_NEW(X) \\\n    X(void, nvnA, (void))\n")

    def test_no_entry_at_all_is_refused(self):
        with self.assertRaisesRegex(tool.Refused, "no X"):
            tool.read_lists("// nothing\n")


class Tokens(unittest.TestCase):
    def text(self, source, api):
        return " ".join(token.text for token in tool.tokens_of(source, api, "x.c"))

    def test_a_build_keeps_its_branch(self):
        source = "a\n#if NVN_LAYER_API < 231\nb\n#else\nc\n#endif\nd\n#if NVN_LAYER_API >= 231\ne\n#endif\n"
        self.assertEqual(self.text(source, 55), "a b d")
        self.assertEqual(self.text(source, 231), "a c d e")

    def test_another_condition_keeps_both_branches(self):
        self.assertEqual(self.text("#ifdef X\na\n#else\nb\n#endif\n", 55), "a b")

    def test_comments_go_and_strings_join(self):
        tokens = tool.tokens_of('x = "a{" /* nvnA( */ "b"; // nvnB(\n', 55, "x.c")
        self.assertEqual([(t.kind, t.text) for t in tokens],
                         [("word", "x"), ("punct", "="), ("string", "a{b"), ("punct", ";")])

    def test_another_test_of_the_version_is_refused_with_its_line(self):
        with self.assertRaisesRegex(tool.Refused, r"x.c:2: a test of NVN_LAYER_API .*== 55"):
            tool.tokens_of("a\n#if NVN_LAYER_API == 55\nb\n#endif\n", 55, "x.c")

    def test_an_if_left_open_is_refused(self):
        with self.assertRaisesRegex(tool.Refused, "1 #if left open"):
            tool.tokens_of("#ifdef X\na\n", 55, "x.c")


class Cases(unittest.TestCase):
    def setUp(self):
        self.walk = walked()

    def test_the_cases_in_the_order_they_open(self):
        self.assertEqual(self.walk.cases, [("toy", "it comes up"), ("toy", "a copies"), ("toy", "b copies"),
                                           ("toy", "it draws"), ("toy", "it is skipped")])

    def test_a_case_that_leaves_holds_until_its_verdict(self):
        self.assertEqual(cases(self.walk, "nvnQueueInitialize"), ["it comes up"])
        self.assertEqual(cases(self.walk, "nvnDeviceGetInteger"), ["outside"])

    def test_the_loader_calls_the_two_functions_it_looks_the_others_up_with(self):
        self.assertEqual(cases(self.walk, "nvnBootstrapLoader"), ["it comes up"])
        self.assertEqual(cases(self.walk, "nvnDeviceGetProcAddress"), ["it comes up"])

    def test_the_way_out_of_a_case_is_not_the_case(self):
        self.assertEqual(cases(self.walk, "nvnQueueFinalize"), ["outside"])

    def test_a_name_passed_to_a_function_is_its_case(self):
        self.assertEqual(cases(self.walk, "nvnQueueFinish"), ["a copies", "b copies"])

    def test_a_verdict_in_an_inner_brace_does_not_end_the_case(self):
        # CopyCase's early ReportCase sits in an if of its own
        self.assertEqual(cases(self.walk, "nvnQueueGetError"), ["a copies", "b copies"])
        self.assertEqual(cases(self.walk, "nvnQueueFlush"), ["outside"])

    def test_a_constant_passed_chooses_the_way(self):
        self.assertEqual(cases(self.walk, "nvnCommandBufferCopyBufferToBuffer"), ["a copies"])
        self.assertEqual(cases(self.walk, "nvnCommandBufferCopyTextureToTexture"), ["b copies"])
        self.assertEqual(cases(self.walk, "nvnCommandBufferCopyTextureToBuffer"), [])

    def test_a_test_of_anything_else_counts_both_ways(self):
        self.assertEqual(cases(self.walk, "nvnCommandBufferBarrier"), ["a copies", "b copies"])

    def test_a_table_of_functions_counts_for_who_names_it(self):
        self.assertEqual(cases(self.walk, "nvnCommandBufferDrawArrays"), ["it draws"])

    def test_a_case_in_a_brace_ends_with_the_brace(self):
        self.assertEqual(cases(self.walk, "nvnDeviceFinalize"), ["it is skipped"])

    def test_comments_and_strings_call_nothing(self):
        self.assertNotIn("nvnNotACall", self.walk.calls)
        self.assertNotIn("nvnInAString", self.walk.calls)

    def test_a_build_reads_its_own_lines(self):
        self.assertIn("nvnWindowFinalize", self.walk.calls)
        self.assertNotIn("nvnProgramFinalize", self.walk.calls)
        other = walked(api=231)
        self.assertIn("nvnProgramFinalize", other.calls)
        self.assertNotIn("nvnWindowFinalize", other.calls)

    def test_a_guarded_function_needs_its_constant(self):
        self.assertEqual(cases(self.walk, "nvnSyncWait"), ["outside"])
        self.assertEqual(cases(walked(guards={"Guarded": "WAY_OTHER"}), "nvnSyncWait"), ["outside"])
        self.assertEqual(cases(walked(guards={"Guarded": "WAY_NVN"}), "nvnSyncWait"), [])

    def test_a_function_named_as_recording_needs_the_looked_up_way(self):
        text = ('static const char *const SUITE = "toy";\n'
                'static void ListRecorded(void) { nvn.nvnQueueFlush(0); }\n'
                'static void Case(int kind, const char *name) {\n'
                '    if (!ReportStart(SUITE, name)) { return; }\n    ListRecorded();\n'
                '    CHECK_TRUE(SUITE, name, 0, true, "");\n}\n'
                'void RunToy(void) {\n    Case(DRAW_PATH_LAYER, "own");\n    Case(DRAW_PATH_NVN, "looked up");\n}\n')
        self.assertEqual(cases(walked(text), "nvnQueueFlush"), ["looked up"])

    def test_a_case_name_that_is_no_string_is_refused_with_its_place(self):
        with self.assertRaisesRegex(tool.Refused, r"toy.c:4: ReportStart\(SUITE, names\[i\]|toy.c:4: ReportStart"):
            walked('static const char *const SUITE = "toy";\nvoid RunToy(void) {\n'
                   '    for (int i = 0; i < 2; ++i) {\n        if (ReportStart(SUITE, names[i])) { }\n    }\n}\n')
        with self.assertRaisesRegex(tool.Refused, r"toy.c:3: ReportStart\(SUITE, name\): the tool cannot follow "
                                                  r"the case's name to a string, in RunToy"):
            walked('static const char *const SUITE = "toy";\nvoid RunToy(const char *name) {\n'
                   '    if (ReportStart(SUITE, name)) { }\n}\n')

    def test_an_entry_that_is_not_there_is_refused(self):
        with self.assertRaisesRegex(tool.Refused, "RunOther, a suite's entry, is in none"):
            walked(roots=("RunOther",))


class Tree(unittest.TestCase):
    """This tree's own table."""

    @classmethod
    def setUpClass(cls):
        cls.table = tool.Table(ROOT)

    def test_the_document_is_what_the_tree_gives(self):
        with open(os.path.join(ROOT, tool.DOC), encoding="utf-8") as file:
            self.assertEqual(file.read(), self.table.document(),
                             "docs/NvnCoverage.md is stale: run tools/nvn-coverage.py")

    def test_every_function_of_the_hook_has_one_group(self):
        self.assertEqual(sum(len(self.table.names(group)) for group in tool.ORDER), len(self.table.hook))
        self.assertGreaterEqual(len(self.table.hook), 677)

    def test_the_copies_are_told_apart(self):
        names = lambda function: {case[1] for case in self.table.calls[function]}
        self.assertEqual(names("nvnCommandBufferCopyTextureToTexture"),
                         {"a texture copied into another keeps every texel",
                          "a texture copied into another with MIRROR_X lands mirrored"})

    def test_only_the_looked_up_way_of_a_scene_counts(self):
        # a scene's cases say their way in their names: deko3d's calls are no runtime's NVN, nor are
        # the layer's own functions a scene records through; the looked up functions are
        called = {case for cases_ in self.table.calls.values() for case in cases_ if case is not tool.OUTSIDE}
        own = [case for case in self.table.cases
               if "through deko3d" in case[1] or ("the layer's own" in case[1] and "scene" in case[1])]
        looked_up = [case for case in self.table.cases if "looks up" in case[1] or "looked up" in case[1]]
        self.assertGreaterEqual(len(own), 2)
        self.assertGreaterEqual(len(looked_up), 1)
        self.assertEqual([case for case in own if case in called], [])
        self.assertEqual([case for case in looked_up if case not in called], [])

    def test_the_layer_s_own_way_counts_for_nothing(self):
        # each case of the layer's own way has its looked up twin, and calls no runtime's NVN
        called = {case for cases_ in self.table.calls.values() for case in cases_ if case is not tool.OUTSIDE}
        own = [case for case in self.table.cases if case[1].startswith(tool.OWN_PREFIX)]
        self.assertGreaterEqual(len(own), 1)
        self.assertEqual([case for case in own if case in called], [])
        twins = [(suite, name[len(tool.OWN_PREFIX):]) for suite, name in own]
        self.assertEqual([case for case in twins if case not in self.table.cases], [])
        self.assertEqual([case for case in twins if case not in called], [])

    def test_a_case_calls_only_what_the_layer_has(self):
        for name in self.table.calls:
            self.assertIn(name, self.table.layer)


class Refusals(unittest.TestCase):
    """What stops the tool on a tree, with a copy of this one's files."""

    def setUp(self):
        self.root = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.root)
        kept = {tool.HOOK, tool.PIN, tool.PROCS, tool.LAYER_HEADER, tool.DOC}
        for build in tool.BUILDS:
            kept.update(build["files"])
        for where in kept:
            os.makedirs(os.path.dirname(os.path.join(self.root, where)), exist_ok=True)
            shutil.copy(os.path.join(ROOT, where), os.path.join(self.root, where))

    def run_tool(self, *arguments):
        err, out = io.StringIO(), io.StringIO()
        with contextlib.redirect_stderr(err), contextlib.redirect_stdout(out):
            code = tool.main(["--root", self.root] + list(arguments))
        return code, out.getvalue(), err.getvalue()

    def test_the_copy_is_current(self):
        self.assertEqual(self.run_tool("--check"), (0, "", ""))

    def test_a_stale_document_fails_the_check_and_is_left_alone(self):
        path = os.path.join(self.root, tool.DOC)
        with open(path, "a", encoding="utf-8") as file:
            file.write("more\n")
        code, _, err = self.run_tool("--check")
        self.assertEqual(code, 1)
        self.assertIn("docs/NvnCoverage.md is not what this tree gives", err)
        with open(path, encoding="utf-8") as file:
            self.assertTrue(file.read().endswith("more\n"))
        self.assertEqual(self.run_tool()[0], 0)
        self.assertEqual(self.run_tool("--check")[0], 0)

    def test_a_name_too_long_down_the_layer_s_own_way(self):
        # a name of its own, so the test holds whatever names the tree has
        path = os.path.join(self.root, "source", "tests", "t_nvn.c")
        with open(path, encoding="utf-8") as file:
            text = file.read()
        short = 'Way("the device initializes")'
        self.assertEqual(text.count(short), 1)
        with open(path, "w", encoding="utf-8") as file:
            file.write(text.replace(short, 'Way("%s")' % ("a device " * 13 + "comes up")))
        code, _, err = self.run_tool()
        self.assertEqual(code, 1)
        self.assertIn("is 142 characters down the layer's own way, and a report keeps 127", err)

    def test_a_copy_of_the_hook_that_is_not_the_pinned_one(self):
        with open(os.path.join(self.root, tool.HOOK), "a", encoding="utf-8") as file:
            file.write("hook nvnDeviceMore(NVNdevice* d)\n")
        with open(os.path.join(self.root, tool.PIN), encoding="ascii") as file:
            pinned = [line.split()[1] for line in file if line.startswith("sha256")][0]
        code, _, err = self.run_tool()
        self.assertEqual(code, 1)
        self.assertRegex(err, r"tools/nvn/nvn.hook has the SHA-256 [0-9a-f]{64}, and tools/nvn/PIN says %s" % pinned)

    def test_a_name_the_layer_has_and_the_hook_has_not(self):
        path = os.path.join(self.root, tool.PROCS)
        with open(path, encoding="utf-8") as file:
            text = file.read()
        with open(path, "w", encoding="utf-8") as file:
            file.write(text.replace("nvnQueueFlush,", "nvnQueueFlushHarder,"))
        code, _, err = self.run_tool()
        self.assertEqual(code, 1)
        self.assertIn("the layer has nvnQueueFlushHarder, which tools/nvn/nvn.hook at", err)

    def test_a_file_that_calls_nvn_and_no_build_lists(self):
        with open(os.path.join(self.root, "source", "tests", "t_more.c"), "w", encoding="utf-8") as file:
            file.write("void RunMore(void) {\n    nvn.nvnQueueFlush(&queue);\n}\n")
        code, _, err = self.run_tool()
        self.assertEqual(code, 1)
        self.assertIn("source/tests/t_more.c:2 calls nvnQueueFlush", err)

    def test_gaps_of_a_group(self):
        # a function of the hook's that no case can call, in a copy whose pin names the copy
        path = os.path.join(self.root, tool.HOOK)
        with open(path, encoding="utf-8") as file:
            text = file.read()
        with open(path, "w", encoding="utf-8") as file:
            file.write(text.replace("family queue\n", "family queue\nhook nvnQueueNeverCalled(NVNqueue* q)\n"))
        with open(path, "rb") as file:
            digest = hashlib.sha256(file.read()).hexdigest()
        with open(os.path.join(self.root, tool.PIN), "w", encoding="ascii") as file:
            file.write("commit %s\nsha256 %s\n" % ("0" * 40, digest))
        code, out, _ = self.run_tool("--gaps", "queues")
        self.assertEqual(code, 0)
        self.assertIn("- `nvnQueueNeverCalled` (not in the layer)\n", out)
        # one the cases call and the layer has is no gap
        self.assertNotIn("nvnQueueFinish`", out)
        self.assertNotIn("nvnQueueNeverCalled", self.run_tool("--gaps", "textures")[1])

    def test_gaps_of_what_is_no_group(self):
        code, out, err = self.run_tool("--gaps", "queue")
        self.assertEqual((code, out), (2, ""))
        self.assertIn("--gaps queue is not a group: device, memory pools", err)


if __name__ == "__main__":
    unittest.main()
