# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""tools/gba-report.py: the report from mGBA's log, a long line joined from its pieces of 256 bytes and one of exactly
256 from its empty last piece, mGBA's other lines left out; from the SRAM's ring, whole and wrapped; no summary, and a
file that is neither, refused by their exit codes."""

import importlib.util
import io
import os
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout

HERE = os.path.dirname(os.path.abspath(__file__))
SPEC = importlib.util.spec_from_file_location("gba_report", os.path.join(HERE, "gba-report.py"))
tool = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(tool)

NOTE = '{"type":"note","text":"Amberlith 2.0.0 (0123abc) starting"}'
LONG = '{"type":"case","suite":"libc","name":"x","detail":"' + "d" * 600 + '"}'
EXACT = '{"type":"note","text":"' + "e" * (256 - 25) + '"}'
SUMMARY = '{"type":"summary","passed":3,"failed":0,"skipped":0,"crashed":0}'
DONE = "AMBERLITH DONE passed=3 failed=0 skipped=0 crashed=0"


def sent(line):
    """a line as platform_gba.c sends it: pieces of 256 bytes and a shorter last one, each a log line of mGBA's"""
    pieces = [line[i : i + 256] for i in range(0, len(line), 256)]
    if len(line) % 256 == 0:
        pieces.append("")
    return [f"[INFO] GBA Debug: {piece}" for piece in pieces]


def sram(text, at=None, wrapped=False):
    """an SRAM save as platform_gba.c leaves it: the head, then the ring"""
    ring = bytearray(0x8000 - 16)
    data = text.encode()
    if wrapped:
        for i, byte in enumerate(data):
            ring[(at + i) % len(ring)] = byte
        at = (at + len(data)) % len(ring)
    else:
        ring[: len(data)] = data
        at = len(data)
    return b"AMBRLOG1" + at.to_bytes(4, "little") + bytes([1 if wrapped else 0]) + b"\0" * 3 + bytes(ring)


class GbaReport(unittest.TestCase):
    def test_the_exact_line_is_256_bytes(self):
        # the piece the reader joins with an empty one after it
        self.assertEqual(len(EXACT.encode()), 256)
        self.assertEqual(sent(EXACT)[-1], "[INFO] GBA Debug: ")

    def run_tool(self, content, binary=False):
        with tempfile.TemporaryDirectory() as folder:
            path = os.path.join(folder, "in")
            with open(path, "wb" if binary else "w") as f:
                f.write(content)
            out, err = io.StringIO(), io.StringIO()
            with redirect_stdout(out), redirect_stderr(err):
                code = tool.main([path])
            return code, out.getvalue().splitlines(), err.getvalue()

    def test_the_log_joined_from_its_pieces(self):
        log = ["GBA BIOS: the BIOS's own line", *sent(NOTE), *sent(LONG), "GBA I/O: a stub", *sent(EXACT)]
        log += [*sent(SUMMARY), *sent(DONE)]
        code, report, err = self.run_tool("\n".join(log) + "\n")
        self.assertEqual(code, 0, err)
        self.assertEqual(report, [NOTE, LONG, EXACT, SUMMARY])
        self.assertIn(f"logged: {DONE}", err)

    def test_the_sram_whole(self):
        code, report, err = self.run_tool(sram(f"{NOTE}\n{LONG}\n{SUMMARY}\n{DONE}\n"), binary=True)
        self.assertEqual(code, 0, err)
        self.assertEqual(report, [NOTE, LONG, SUMMARY])

    def test_the_sram_wrapped_leaves_the_cut_line_out(self):
        ring = 0x8000 - 16
        # the run's start written over: the ring keeps a cut line's last 10 bytes, then whole ones
        kept = f"{SUMMARY}\n{DONE}\n"
        filler = ring - len(kept) - 10
        text = "x" * (10 + filler) + "\n" + kept
        code, report, err = self.run_tool(sram(text[-ring:], at=100, wrapped=True), binary=True)
        self.assertEqual(code, 0, err)
        self.assertEqual(report, [SUMMARY])
        self.assertIn("the ring wrapped: the first", err)

    def test_no_summary_exits_1(self):
        code, report, err = self.run_tool("\n".join([*sent(NOTE), *sent(LONG)]) + "\n")
        self.assertEqual(code, 1)
        self.assertEqual(report, [NOTE, LONG])
        self.assertIn("2 lines and no summary at their end", err)

    def test_a_file_that_is_neither_exits_2(self):
        code, report, err = self.run_tool("nothing of the program's\n")
        self.assertEqual(code, 2)
        self.assertEqual(report, [])
        self.assertIn('holds neither the SRAM\'s head "AMBRLOG1" nor a line with "GBA Debug: "', err)


if __name__ == "__main__":
    unittest.main()
