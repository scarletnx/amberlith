# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""tools/cover.py: the cover it writes from a drawing, what it refuses, and this tree's static/icon.svg as current as
static/wimsy-amberlith.svg."""

import contextlib
import importlib.util
import io
import os
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SPEC = importlib.util.spec_from_file_location("cover", os.path.join(HERE, "cover.py"))
tool = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(tool)

DRAWING = """<?xml version="1.0" encoding="UTF-8"?>
<!-- a comment before the drawing -->
<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" viewBox="0 0 512 512" fill="none">
  <metadata>
    <title>left out</title>
  </metadata>
  <circle cx="1" cy="2" r="3" fill="#ffb000"/>
</svg>
"""


class Cover(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.wimsy = os.path.join(self.folder.name, "wimsy.svg")
        self.out = os.path.join(self.folder.name, "icon.svg")
        with open(self.wimsy, "w", encoding="utf-8") as file:
            file.write(DRAWING)

    def test_the_drawing_whole_inside_the_cover_its_metadata_left_out(self):
        text = tool.cover(self.wimsy)
        self.assertTrue(text.startswith(tool.HEAD))
        self.assertTrue(text.endswith(tool.TAIL))
        self.assertIn('  <circle cx="1" cy="2" r="3" fill="#ffb000"/>\n</g>', text)
        self.assertNotIn("left out", text)
        self.assertNotIn("a comment before", text)
        self.assertIn(">Amberlith</text>", text)

    def test_check_says_stale_and_a_write_makes_it_current(self):
        with open(self.out, "w", encoding="utf-8") as file:
            file.write("old")
        quiet = io.StringIO()
        with contextlib.redirect_stdout(quiet):
            self.assertEqual(tool.main(["--wimsy", self.wimsy, "--out", self.out, "--check"]), 1)
            self.assertEqual(tool.main(["--wimsy", self.wimsy, "--out", self.out]), 0)
            self.assertEqual(tool.main(["--wimsy", self.wimsy, "--out", self.out, "--check"]), 0)
        self.assertIn("is not what", quiet.getvalue())

    def test_refused(self):
        with open(self.wimsy, "w", encoding="utf-8") as file:
            file.write("<g/>")
        said = io.StringIO()
        with contextlib.redirect_stderr(said):
            self.assertEqual(tool.main(["--wimsy", self.wimsy, "--out", self.out]), 2)
        self.assertIn("no svg element", said.getvalue())

    def test_this_tree_s_cover_is_what_its_drawing_gives(self):
        self.assertEqual(tool.main(["--check"]), 0, "run tools/cover.py, then make icon")


if __name__ == "__main__":
    unittest.main()
