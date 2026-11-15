# SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""tools/npdm-ounce.py writes an NPDM a runtime takes for a Switch 2 program's, and reads one back."""

import importlib.util
import os
import struct
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SPEC = importlib.util.spec_from_file_location("npdm_ounce", os.path.join(HERE, "npdm-ounce.py"))
tool = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(tool)


def scarletnx_takes_for_ounce(data):
    """ScarletNX's test (src/nx/loader/ounce.h, snx_npdm_ounce), written out apart from the tool's own offsets"""
    if len(data) < 0x80 or data[:4] != b"META":
        return False
    acid, acid_size = struct.unpack_from("<II", data, 0x78)
    return acid < len(data) and 0x304 <= acid_size <= len(data) - acid and data[acid + 0x300:acid + 0x304] == b"ACID"


def scarletnx_reads(data):
    """what its loader and runtime take of the file (loader.c's snx_parse_npdm, main.c's read_npdm): the main
    thread and the title, then the first thread capability as core and priority masks, or None where it refuses"""
    aci, aci_size = struct.unpack_from("<II", data, 0x70)
    if aci > len(data) or aci_size > len(data) - aci or aci_size < 0x38 or data[aci:aci + 4] != b"ACI0":
        return None
    out = {"is64": data[0x0C] & 1, "aspace": (data[0x0C] >> 1) & 7, "prio": data[0x0E], "core": data[0x0F],
           "stack": struct.unpack_from("<I", data, 0x1C)[0], "name": data[0x20:0x30].split(b"\0")[0],
           "title": struct.unpack_from("<Q", data, aci + 0x10)[0]}
    caps, caps_size = struct.unpack_from("<II", data, aci + 0x30)
    for at in range(0, caps_size, 4):
        if not (caps < aci_size and caps_size <= aci_size - caps and at + 4 <= caps_size):
            break
        cap = struct.unpack_from("<I", data, aci + caps + at)[0]
        if cap & 0xF != 0x7:
            continue
        lowest, highest, first, last = cap >> 4 & 63, cap >> 10 & 63, cap >> 16 & 255, cap >> 24 & 255
        if highest > lowest or first > last or last >= 4:
            return None
        out["cores"] = ((2 << last) - 1) & ~((1 << first) - 1)
        out["priorities"] = ((2 << lowest) - 1) & ~((1 << highest) - 1)
        break
    return out


class Write(unittest.TestCase):
    def test_the_default_file_is_a_switch_2_programs(self):
        data = tool.build()
        self.assertTrue(scarletnx_takes_for_ounce(data))
        got = scarletnx_reads(data)
        self.assertEqual(got, {"is64": 1, "aspace": 3, "prio": 44, "core": 0, "stack": 0x100000,
                               "name": b"edenprobe", "title": 0x0100000000001000,
                               "cores": 0b111, "priorities": ((1 << 60) - 1) & ~((1 << 28) - 1)})
        # no magic where a Switch's descriptor has it: a machine that looks there sees none
        acid = struct.unpack_from("<I", data, 0x78)[0]
        self.assertNotEqual(data[acid + 0x200:acid + 0x204], b"ACID")

    def test_every_field_given(self):
        data = tool.build("probe-two", 0x0123456789ABCDEF, 0x200000, 40, 2, (30, 50), (1, 3))
        got = scarletnx_reads(data)
        self.assertEqual((got["name"], got["title"], got["stack"], got["prio"], got["core"]),
                         (b"probe-two", 0x0123456789ABCDEF, 0x200000, 40, 2))
        self.assertEqual(got["cores"], 0b1110)
        self.assertEqual(got["priorities"], ((1 << 51) - 1) & ~((1 << 30) - 1))
        self.assertTrue(scarletnx_takes_for_ounce(data))

    def test_what_is_refused_names_the_value(self):
        for arguments, says in (
                (dict(name=""), "''"),
                (dict(name="seventeen-letters"), "'seventeen-letters'"),
                (dict(name="café"), "'café'"),
                (dict(title=0), "0x0"),
                (dict(stack=0x800), "0x800"),
                (dict(stack=0x100800), "0x100800"),
                (dict(priority=20), "priority 20 is outside the 28-59"),
                (dict(core=3), "core 3 is outside the 0-2")):
            with self.subTest(arguments=arguments):
                with self.assertRaises(tool.Refused) as refused:
                    tool.build(**arguments)
                self.assertIn(says, str(refused.exception))
        for text, lowest, highest, says in (("5", 0, 63, "5 is not two numbers"), ("9-3", 0, 63, "9-3"),
                                            ("0-4", 0, 3, "0-4 is not a span inside 0-3")):
            with self.assertRaises(tool.Refused) as refused:
                tool.span(text, "--cores", lowest, highest)
            self.assertIn(says, str(refused.exception))


class Read(unittest.TestCase):
    def test_reads_back_what_it_wrote(self):
        got = dict(tool.fields(tool.build("probe-two", 0x0123456789ABCDEF, 0x200000, 40, 2, (30, 50), (1, 3))))
        self.assertEqual(got, {"name": "probe-two", "64-bit": True, "address space": 3, "priority": 40, "core": 2,
                               "stack": "0x200000", "title": "0123456789abcdef",
                               "descriptor": "Switch 2, its magic at 0x300", "priorities": "30-50",
                               "cores": "1-3"})

    def test_tells_a_switch_descriptor(self):
        data = bytearray(tool.build())
        acid = struct.unpack_from("<I", data, 0x78)[0]
        data[acid + 0x300:acid + 0x304] = b"\0\0\0\0"
        self.assertEqual(dict(tool.fields(bytes(data)))["descriptor"], "neither: no ACID magic at 0x200 or 0x300")
        self.assertFalse(scarletnx_takes_for_ounce(bytes(data)))
        data[acid + 0x200:acid + 0x204] = b"ACID"
        self.assertEqual(dict(tool.fields(bytes(data)))["descriptor"], "Switch, its magic at 0x200")
        self.assertFalse(scarletnx_takes_for_ounce(bytes(data)))

    def test_refuses_what_is_no_npdm(self):
        for data, says in ((b"", "0 bytes"), (b"NPDM" + bytes(0x100), "do not start with META"),
                           (tool.build()[:0x100], "the access control info, 0x44 bytes at 0x3c0 of 0x100")):
            with self.assertRaises(tool.Refused) as refused:
                tool.fields(data)
            self.assertIn(says, str(refused.exception))


class CommandLine(unittest.TestCase):
    def test_write_then_read(self):
        with tempfile.TemporaryDirectory() as folder:
            path = os.path.join(folder, "main.npdm")
            self.assertEqual(tool.main(["write", path, "--cores", "0-3"]), 0)
            data = open(path, "rb").read()
            self.assertTrue(scarletnx_takes_for_ounce(data))
            self.assertEqual(scarletnx_reads(data)["cores"], 0b1111)
            self.assertEqual(tool.main(["read", path]), 0)

    def test_a_refused_value_writes_nothing(self):
        with tempfile.TemporaryDirectory() as folder:
            path = os.path.join(folder, "main.npdm")
            self.assertEqual(tool.main(["write", path, "--priority", "70"]), 1)
            self.assertEqual(tool.main(["write", path, "--stack", "big"]), 1)
            self.assertFalse(os.path.exists(path))


if __name__ == "__main__":
    unittest.main()
