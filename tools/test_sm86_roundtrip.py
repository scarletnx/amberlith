# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""tools/sm86-roundtrip.py tells a round trip that holds from one that does not, a stand-in as ScarletNX's tool."""

import importlib.util
import os
import stat
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SPEC = importlib.util.spec_from_file_location("sm86_roundtrip", os.path.join(HERE, "sm86-roundtrip.py"))
trip = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(trip)

COMPUTE = ".stage compute\n.local 1 1 1\nS2R R0, SR_TID.X\nMOV R1, R0\nEXIT\n"
MESH = (".stage mesh\n.threads 1\n.payload 0x80\n.vertices 3\n.primitives 1\n.out position\n"
        "MOV R5, 1\nISBEWR.MAP.O.B32 [RZ + 0x3], R5\nEXIT\n")
PASSED = "1 programs, 1 complete, 1 translated, 1 pass spirv-val\n"


def listed(*names):
    """the decoder's listing of these instructions, in snx-sm86's own shape"""
    return "".join("%6d  T7 %-14s f4 d0    0000000000000000 0000000000000000\n" % (i, name)
                   for i, name in enumerate(names))


class RoundTrip(unittest.TestCase):
    def run_with(self, source_text, says, status=0):
        """what round_trip answers when the tool prints says, and the arguments the tool was given"""
        with tempfile.TemporaryDirectory() as folder:
            source = os.path.join(folder, "program.sm86")
            with open(source, "w", encoding="ascii") as file:
                file.write(source_text)
            with open(os.path.join(folder, "says"), "w", encoding="ascii") as file:
                file.write(says)
            tool = os.path.join(folder, "tool")
            with open(tool, "w", encoding="ascii") as file:
                file.write('#!/bin/sh\necho "$@" > "%s/arguments"\ncat "%s/says"\nexit %d\n'
                           % (folder, folder, status))
            os.chmod(tool, os.stat(tool).st_mode | stat.S_IXUSR)
            answer = trip.round_trip(tool, source, folder)
            with open(os.path.join(folder, "arguments"), encoding="ascii") as file:
                arguments = file.read().split()
            sizes = {name: os.path.getsize(os.path.join(folder, name)) for name in os.listdir(folder)}
        return answer, arguments, sizes

    def test_it_holds_when_the_decoder_names_what_was_written(self):
        answer, arguments, sizes = self.run_with(COMPUTE, listed("S2R", "MOV", "EXIT", "BRA") + PASSED)
        self.assertIsNone(answer)
        self.assertEqual(arguments[1:], ["--list", "--translate", "--validate", "--out", arguments[-1]])
        self.assertEqual(sizes["program.code"], 4 * 16)    # the instructions and the last branch, no padding
        self.assertNotIn("program.sph", sizes)             # a compute program has no header

    def test_a_mesh_program_goes_with_its_header_and_its_fields(self):
        answer, arguments, sizes = self.run_with(MESH, listed("MOV", "ISBEWR", "EXIT", "BRA") + PASSED)
        self.assertIsNone(answer)
        self.assertEqual(arguments[1], "--sph")
        self.assertEqual(arguments[3:5], ["--mesh", "1,128,0,3,1"])
        self.assertEqual((sizes["program.sph"], sizes["program.code"]), (128, 4 * 16))

    def test_another_instruction_read_back_fails_with_both_names(self):
        answer, _, _ = self.run_with(COMPUTE, listed("S2R", "FADD", "EXIT", "BRA") + PASSED)
        self.assertIn("the first that differs is 1: FADD read, MOV written", answer)

    def test_fewer_instructions_read_back_fails_with_the_counts(self):
        answer, _, _ = self.run_with(COMPUTE, listed("S2R", "MOV") + PASSED)
        self.assertIn("the decoder read 2 instructions where the source has 4", answer)
        self.assertIn("is 2: nothing read, EXIT written", answer)

    def test_a_program_that_does_not_translate_fails(self):
        says = listed("S2R", "MOV", "EXIT", "BRA") + "1 programs, 1 complete, 0 translated, 0 pass spirv-val\n"
        answer, _, _ = self.run_with(COMPUTE, says)
        self.assertIn("did not translate and validate: 1 programs, 1 complete, 0 translated", answer)

    def test_the_tool_s_own_failure_fails(self):
        answer, _, _ = self.run_with(COMPUTE, listed("S2R", "MOV", "EXIT", "BRA") + PASSED, status=3)
        self.assertIn("tool ended with 3", answer)


if __name__ == "__main__":
    unittest.main()
