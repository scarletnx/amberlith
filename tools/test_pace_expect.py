# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""The table source/tests/t_paceref.c holds is the one tools/pace-expect.py prints, and the constants the script
works from are the ones source/paceref.h and source/paceref.c give the program."""

import importlib.util
import io
import os
import re
import unittest
from contextlib import redirect_stdout

HERE = os.path.dirname(os.path.abspath(__file__))
SPEC = importlib.util.spec_from_file_location("pace_expect", os.path.join(HERE, "pace-expect.py"))
pace_expect = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(pace_expect)


def source(name):
    with open(os.path.join(HERE, "..", "source", name)) as f:
        return f.read()


def held():
    m = re.search(r"// BEGIN tools/pace-expect.py --c\n(.*)// END tools/pace-expect.py --c",
                  source(os.path.join("tests", "t_paceref.c")), re.S)
    return m.group(1) if m else None


def define(text, name):
    """a #define's value as a number: a literal, or 1 shifted"""
    value = re.search(r"#define %s (.+)" % name, text).group(1).strip()
    shift = re.fullmatch(r"\(1u(?:ll)? << (\d+)\)", value)
    return 1 << int(shift.group(1)) if shift else int(value.rstrip("ul"), 0)


def lanes(text, name):
    row = re.search(r"const uint32_t %s\[4\] = \{([^}]*)\}" % name, text).group(1)
    return tuple(int(v.strip().rstrip("u")) for v in row.split(","))


class PinnedTable(unittest.TestCase):
    def test_the_c_table_is_the_scripts(self):
        printed = io.StringIO()
        with redirect_stdout(printed):
            pace_expect.print_c()
        self.assertEqual(held(), printed.getvalue())

    def test_the_scripts_constants_are_the_programs(self):
        header = source("paceref.h")
        for name in ("INTEGER_A", "INTEGER_C", "INTEGER_X", "INTEGER_Y", "FLOAT_A", "FLOAT_C", "FLOAT_X",
                     "INTEGER_CHUNK", "FLOAT_CHUNK", "VECTOR_CHUNK", "WALK_SLOTS", "WALK_STEPS"):
            with self.subTest(name=name):
                self.assertEqual(define(header, "PACE_" + name), getattr(pace_expect, name))
        body = source("paceref.c")
        for name in ("VECTOR_A", "VECTOR_C", "VECTOR_X", "VECTOR_M", "VECTOR_D", "VECTOR_I"):
            with self.subTest(name=name):
                self.assertEqual(lanes(body, "PACE_" + name), getattr(pace_expect, name))


class Loops(unittest.TestCase):
    def test_one_iteration_by_hand(self):
        # x = X * A + C; y = Y ^ (x >> 29); z = y; y rotated right by 17; z is above x, so the sum takes y
        x = (pace_expect.INTEGER_X * pace_expect.INTEGER_A + pace_expect.INTEGER_C) % 2**64
        y = pace_expect.INTEGER_Y ^ (x >> 29)
        z = y
        y = ((y >> 17) | (y << 47)) % 2**64
        self.assertGreater(z, x)
        self.assertEqual(pace_expect.integer_loop(1), (y, z))
        # 12345 * 1664525 + 1013904223 is 21562465348, 3742788 past 1285 times 2^24; 2^24 less it is larger
        self.assertEqual(21562465348 - 1285 * 2**24, 3742788)
        self.assertEqual(pace_expect.float_loop(1), (2**24 - 3742788, 3742788))
        # lane 0: x = 1 * 3125 + 1013 = 4138, 42 past 4096; i = 1664525 + 1013904223; the sum (i >> 13) ^ 42
        self.assertEqual(pace_expect.vector_loop(1)[8], 42)
        self.assertEqual(pace_expect.vector_loop(1)[4], 1015568748)
        self.assertEqual(pace_expect.vector_loop(1)[0], (1015568748 >> 13) ^ 42)

    def test_the_walk_is_one_round(self):
        for slots in (2, 16, 4096):
            order = pace_expect.walk_order(slots)
            self.assertEqual(sorted(order), list(range(slots)))
            self.assertEqual(order[-1], 0)


if __name__ == "__main__":
    unittest.main()
