# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""tools/dksh-banks.py finds the reads of NVN's driver banks outside the tables a stage has there."""

import contextlib
import importlib.util
import io
import os
import struct
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SPEC = importlib.util.spec_from_file_location("dksh_banks", os.path.join(HERE, "dksh-banks.py"))
banks = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(banks)

SCHED = 0x001F8000FC0007E0
EXIT = 0xE30000000007000F
NOP = 0x50B0000000070F00


def ldc(bank, offset):
    """LDC R4, c[bank][R0 + offset]"""
    return 0xEF90 << 48 | bank << 36 | (offset & 0xFFFF) << 20 | 4


def const_operand(bank, offset):
    """an instruction of the constant operand form (MOV R4, c[bank][offset])"""
    return 0x4C98 << 48 | bank << 34 | (offset >> 2) << 20 | 4


def module(kind, *instructions):
    """a DKSH module of one program of type kind, its code the instructions with a scheduling word before each three"""
    words = []
    for i, w in enumerate(instructions + (EXIT,)):
        if i % 3 == 0:
            words.append(SCHED)
        words.append(w)
    code = (b"" if kind == banks.COMPUTE else b"\0" * 0x50) + b"".join(struct.pack("<Q", w) for w in words)
    code += b"\0" * (-len(code) % 256)
    program = struct.pack("<2I", kind, 0) + b"\0" * 56
    control = struct.pack("<6I", 0x48534B44, 24, 256, len(code), 24, 1) + program
    return control + b"\0" * (256 - len(control)) + code


class Check(unittest.TestCase):
    def reads(self, data):
        return [(stage, bank, offset) for _, _, stage, _, bank, offset in banks.check(data)]

    def test_nvn_compute_reads_its_tables(self):
        # The storage buffer table at 0x310, the uniform buffer table at 0x210, a uniform block in bank 3.
        data = module(banks.COMPUTE, ldc(0, 0x310), ldc(0, 0x314), const_operand(0, 0x210), const_operand(3, 0x10))
        self.assertEqual(self.reads(data), [])

    def test_deko3d_compute_is_found(self):
        # deko3d's storage buffers at 0x140 and its first uniform block in bank 2.
        data = module(banks.COMPUTE, ldc(0, 0x140), const_operand(2, 0x20), NOP)
        self.assertEqual(self.reads(data), [(banks.COMPUTE, 0, 0x140), (banks.COMPUTE, 2, 0x20)])

    def test_each_graphics_stage_has_its_table(self):
        # DKSH program types: vertex 0, fragment 1, geometry 2, tessellation control 3 and evaluation 4.
        for kind, table in ((0, 0x110), (1, 0x510), (2, 0x410), (3, 0x210), (4, 0x310)):
            with self.subTest(kind=kind):
                self.assertEqual(self.reads(module(kind, ldc(0, table), ldc(0, table + 0xF4))), [])
                self.assertEqual(self.reads(module(kind, ldc(0, table + 0x100))), [(kind, 0, table + 0x100)])
        # deko3d's fragment storage buffers, at 0x730.
        self.assertEqual(self.reads(module(1, ldc(0, 0x730))), [(1, 0, 0x730)])

    def test_reads_after_exit_are_not_code(self):
        data = module(banks.COMPUTE, NOP)
        data = data[:-8] + struct.pack("<Q", ldc(2, 0))
        self.assertEqual(self.reads(data), [])

    def test_main_says_each_read_and_the_status(self):
        with tempfile.TemporaryDirectory() as directory:
            good = os.path.join(directory, "good.dksh")
            bad = os.path.join(directory, "bad.dksh")
            empty = os.path.join(directory, "empty.bin")
            with open(good, "wb") as f:
                f.write(module(banks.COMPUTE, ldc(0, 0x310)))
            with open(bad, "wb") as f:
                f.write(module(1, ldc(0, 0x730)))
            with open(empty, "wb") as f:
                f.write(b"nothing")
            said = io.StringIO()
            with contextlib.redirect_stdout(said):
                self.assertEqual(banks.main([good]), 0)
                self.assertEqual(banks.main([good, bad]), 1)
                self.assertEqual(banks.main([empty]), 2)
            text = said.getvalue()
            self.assertIn("good.dksh: every bank 0 read in NVN's tables, none of bank 2", text)
            self.assertIn("fragment program 0, instruction 0 reads c[0x0][0x730], where NVN's driver keeps a fragment "
                          "program's tables at 0x510 to 0x60f", text)
            self.assertIn("empty.bin: no DKSH module", text)


if __name__ == "__main__":
    unittest.main()
