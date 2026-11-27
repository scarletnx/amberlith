# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""The table source/tests/t_paceref.c holds is the one tools/pace-code-expect.py prints, the script's constants are
the program's, and its crc, aes and sha kernels are the standards' own: zlib's CRC-32 and CRC-32C's check value,
FIPS-197's first round and hashlib's SHA-256."""

import hashlib
import importlib.util
import io
import os
import re
import struct
import unittest
import zlib
from contextlib import redirect_stdout

HERE = os.path.dirname(os.path.abspath(__file__))
SPEC = importlib.util.spec_from_file_location("pace_code_expect", os.path.join(HERE, "pace-code-expect.py"))
expect = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(expect)


def source(name):
    with open(os.path.join(HERE, "..", "source", name)) as f:
        return f.read()


class PinnedTable(unittest.TestCase):
    def test_the_c_table_is_the_scripts(self):
        held = re.search(r"// BEGIN tools/pace-code-expect.py --c\n(.*)// END tools/pace-code-expect.py --c",
                         source(os.path.join("tests", "t_paceref.c")), re.S)
        printed = io.StringIO()
        with redirect_stdout(printed):
            expect.print_c()
        self.assertIsNotNone(held)
        self.assertEqual(held.group(1), printed.getvalue())

    def test_the_scripts_constants_are_the_programs(self):
        header = source("paceref.h")
        self.assertEqual(int(re.search(r"#define PACE_INTEGER_A (\d+)ull", header).group(1)), expect.M)
        self.assertEqual(int(re.search(r"#define PACE_INTEGER_C (\d+)ull", header).group(1)), expect.C)
        code = source("pacecoderef.h")
        for name, value in (("TABLE_BYTES", expect.TABLE_BYTES), ("STREAM_BYTES", expect.STREAM_BYTES)):
            with self.subTest(name=name):
                shift = re.search(r"#define PACE_CODE_%s \((\d+)u << (\d+)\)" % name, code)
                self.assertEqual(int(shift.group(1)) << int(shift.group(2)), value)

    def test_every_kind_is_in_the_table(self):
        kinds = re.findall(r"^\s+PACE_CODE_(\w+),$", source("pacecoderef.h"), re.M)
        self.assertEqual(kinds, [name for name, _ in expect.KERNELS])


class Standards(unittest.TestCase):
    def test_crc32_is_zlibs_and_crc32c_its_check_value(self):
        word = struct.unpack("<Q", b"12345678")[0]
        self.assertEqual(expect.crc_step(0xFFFFFFFF, word, 0xEDB88320) ^ 0xFFFFFFFF, zlib.crc32(b"12345678"))
        crc = 0xFFFFFFFF
        table = expect.crc_table(0x82F63B78)
        for byte in b"123456789":
            crc = (crc >> 8) ^ table[(crc ^ byte) & 0xFF]
        self.assertEqual(crc ^ 0xFFFFFFFF, 0xE3069283)

    def test_one_aes_round_is_fips_197s_first(self):
        state = list(bytes.fromhex("3243f6a8885a308d313198a2e0370734"))
        key = bytes.fromhex("2b7e151628aed2a6abf7158809cf4f3c")
        self.assertEqual(bytes(expect.aes_round(state, key)).hex(), "046681e5e0cb199a48f8d37a2806264c")

    def test_the_compression_is_sha_256s(self):
        block = b"abc" + b"\x80" + b"\0" * 52 + struct.pack(">Q", 24)
        digest = b"".join(struct.pack(">I", word) for word in expect.compress(expect.SHA_IV, block))
        self.assertEqual(digest, hashlib.sha256(b"abc").digest())


class ByHand(unittest.TestCase):
    def test_one_iteration_of_the_chains(self):
        # four chains: a = (X + 0xABC) ^ (X << 7), and so on, each on its own
        a = ((expect.START + 0xABC) ^ (expect.START << 7)) % 2**64
        b = ((expect.OTHER ^ 0xFF) + (expect.OTHER >> 3)) % 2**64
        c = ((0x0F1E2D3C4B5A6978 - 0x7FF) ^ (0x0F1E2D3C4B5A6978 >> 11)) % 2**64
        d = ((0x8796A5B4C3D2E1F0 ^ (0x8796A5B4C3D2E1F0 << 13)) + 0x123) % 2**64
        self.assertEqual(expect.int4(1), ((a + b) ^ (c + d)) % 2**64)

    def test_one_iteration_of_the_singles(self):
        # a0 = {1,2,3,4} + {1,2,3,4} * {3,5,7,9}, and the other three chains as the kernel's comment says
        lanes = [4, 12, 24, 40, 16, 32, 52, 76, 28, 32, 36, 40, 42, 48, 54, 60]
        acc = 0
        for lane in lanes:
            acc = (acc * 31 + lane) % 2**64
        self.assertEqual(expect.vector(1), acc)


if __name__ == "__main__":
    unittest.main()
