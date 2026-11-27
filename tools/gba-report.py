#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""The GBA's report, read back from where its frontend leaves it (docs/GBA.md): mGBA's log, which holds every line the
program sent through mGBA's debug registers after "GBA Debug: ", a line longer than the registers' 256 bytes in pieces
of 256 and a shorter last one; or the cartridge's SRAM as an emulator saves it, the log's last 32 KiB as a ring after a
head of 16 bytes ("AMBRLOG1", where the next byte goes, whether it wrapped).

    python3 tools/gba-report.py LOG_OR_SAVE [-o REPORT]

Writes the report's lines (the JSON objects) to REPORT, standard output without -o, and says on standard error each
other line the program logged (its AMBERLITH DONE among them) and, for a ring that wrapped, the bytes of a line cut at
its start. Exits 0 when the report ends with its summary, 1 when it does not (the run did not finish, or the ring kept
only its end), 2 when the file is neither."""

import argparse
import json
import sys

MARK = "GBA Debug: "
PIECE = 256
SRAM_MARK = b"AMBRLOG1"
SRAM_HEAD = 16
SRAM_SIZE = 0x8000


def from_log(text):
    """the program's lines from mGBA's log, each joined from its pieces"""
    lines, held = [], []
    for raw in text.splitlines():
        at = raw.find(MARK)
        if at < 0:
            continue
        piece = raw[at + len(MARK) :]
        held.append(piece)
        if len(piece.encode()) < PIECE:
            lines.append("".join(held))
            held = []
    if held:
        lines.append("".join(held))
    return lines, 0


def from_sram(data):
    """the program's lines from the SRAM ring, and how many bytes of a first line the ring cut"""
    at = int.from_bytes(data[8:12], "little")
    wrapped = data[12] == 1
    ring = data[SRAM_HEAD:SRAM_SIZE]
    if at >= len(ring):
        raise ValueError(f"the SRAM's head says the next byte goes at {at}, past its ring of {len(ring)} bytes")
    text = (ring[at:] + ring[:at]) if wrapped else ring[:at]
    cut = 0
    if wrapped:
        newline = text.find(b"\n")
        cut = newline + 1 if newline >= 0 else len(text)
        text = text[cut:]
    return text.decode("utf-8", "replace").splitlines(), cut


def read(path):
    with open(path, "rb") as f:
        data = f.read()
    if data.startswith(SRAM_MARK):
        return from_sram(data)
    text = data.decode("utf-8", "replace")
    if MARK not in text:
        raise ValueError(f'{path} holds neither the SRAM\'s head "AMBRLOG1" nor a line with "{MARK}"')
    return from_log(text)


def is_summary(line):
    try:
        return json.loads(line).get("type") == "summary"
    except (ValueError, AttributeError):
        return False


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source", help="mGBA's log, or the cartridge's SRAM as an emulator saves it")
    parser.add_argument("-o", "--out", help="where the report goes (standard output without it)")
    args = parser.parse_args(argv)
    try:
        lines, cut = read(args.source)
    except (OSError, ValueError) as error:
        print(f"gba-report: {error}", file=sys.stderr)
        return 2
    report = [line for line in lines if line.startswith("{")]
    for line in lines:
        if not line.startswith("{"):
            print(f"gba-report: logged: {line}", file=sys.stderr)
    if cut:
        print(
            f"gba-report: the ring wrapped: the first {cut} bytes it kept were the end of a line, left out",
            file=sys.stderr,
        )
    text = "".join(line + "\n" for line in report)
    if args.out:
        with open(args.out, "w") as sink:
            sink.write(text)
    else:
        sys.stdout.write(text)
    if not report or not is_summary(report[-1]):
        print(f"gba-report: {len(report)} lines and no summary at their end: the run did not finish here",
              file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
