# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""tools/c23-style.py: each spelling docs/CodeStyle.md refuses found where the code has it, none in a comment or a
string, a clean file passing; and this tree's C23 frontends clean."""

import importlib.util
import os
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SPEC = importlib.util.spec_from_file_location("c23_style", os.path.join(HERE, "c23-style.py"))
tool = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(tool)


class C23Style(unittest.TestCase):
    def test_each_spelling_refused(self):
        for line, said in (
            ("    void *p = NULL;\n", "NULL: nullptr"),
            ("static int x __attribute__((aligned(16)));\n", "__attribute__"),
            ('_Static_assert(sizeof(int) == 4, "int");\n', "_Static_assert: static_assert"),
            ("_Noreturn void Stop(void);\n", "_Noreturn: [[noreturn]]"),
            ("    __atomic_store_n(&x, 1, __ATOMIC_RELEASE);\n", "a GCC atomic builtin"),
            ("    __sync_synchronize();\n", "a GCC atomic builtin"),
            ("    (void)user;\n", "(void)name;: [[maybe_unused]]"),
            ("    memset(&frame, 0, sizeof(frame));\n", "memset of a local to 0"),
        ):
            with self.subTest(line=line):
                found = tool.check("f.c", line)
                self.assertEqual(len(found), 1, found)
                self.assertTrue(found[0].startswith("f.c:1: ") and said in found[0], found)

    def test_comments_and_strings_pass(self):
        text = (
            "/// nullptr, not NULL, when there is none\n"
            "/* (void)user; and memset(&x, 0, sizeof x) in a comment */\n"
            'static const char said[] = "NULL __attribute__ _Noreturn";\n'
            "char c = '\\'';\n"
            "// _Static_assert\n"
        )
        self.assertEqual(tool.check("f.c", text), [])

    def test_the_c23_way_passes(self):
        text = (
            "static_assert(sizeof(int) == 4, \"int\");\n"
            "[[noreturn]] void Stop(void);\n"
            "static void Callback([[maybe_unused]] void *user) {}\n"
            "void F(void) { Frame frame = {}; void *p = nullptr; (void)p; }\n"
            "void G(char *buffer, size_t n) { memset(buffer, 0, n); }\n"
        )
        self.assertEqual(tool.check("f.c", text), [])

    def test_line_numbers_follow_comments(self):
        found = tool.check("f.c", "/* one\n   two */\nvoid *p = NULL;\n")
        self.assertEqual(found, ["f.c:3: NULL: nullptr"])

    def test_this_tree_is_clean(self):
        self.assertEqual(tool.main([]), 0)


if __name__ == "__main__":
    unittest.main()
