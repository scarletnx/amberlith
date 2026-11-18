#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Whether programs compiled for NVN's layout (uam --nvn) read the driver's banks only where NVN's driver writes them.

    tools/dksh-banks.py FILE...   each a DKSH module compiled with uam --nvn (build-mod/shaders/*.dksh)

A program run through a game's NVN reads what that driver keeps in its constant buffers: in bank 0 each stage's storage
buffer table, 16 bytes an entry (vertex 0x110, tessellation control 0x210, tessellation evaluation 0x310, geometry
0x410, fragment 0x510), and for a compute program its dispatch size at 0x200, its uniform buffer table from 0x210 and
its storage buffer table from 0x310; in bank 2 the texture handles; and uniform block binding b in bank 3 + b
(ScarletNX reads the same layout from a game). A program compiled for deko3d's layout reads bank 0 elsewhere (its
storage buffers at 0xb0 of a vertex program, 0x730 of a fragment program, 0x140 of a compute one) and its first uniform
block from bank 2, which under NVN hold other things. One line for each read of bank 0 outside its stage's NVN tables
and each read of bank 2; the status is 1 when there is one, 2 when a file holds no DKSH module.

The reads are those an instruction names: an LDC (0xef90) and every instruction of the constant operand forms (top
nibble 0x4: bank in bits 34-38, the offset over four in bits 20-33).
"""

import struct
import sys

MAGIC = b"DKSH"
COMPUTE = 5  # DkshProgramType_Compute: no 0x50-byte header before the code
# The DKSH's program types, which are NVN's stage numbers, and each graphics stage's storage buffer table in bank 0.
STAGES = ("vertex", "fragment", "geometry", "tessellation control", "tessellation evaluation", "compute")
STORAGE = {0: 0x110, 3: 0x210, 4: 0x310, 2: 0x410, 1: 0x510}
EXIT = 0xE30000000007000F


def tables(stage):
    """bank 0's byte ranges NVN's driver writes for a program of stage (DKSH program type)"""
    if stage == COMPUTE:
        return ((0x200, 0x410),)
    if stage in STORAGE:
        return ((STORAGE[stage], STORAGE[stage] + 0x100),)
    return ()


def read(word):
    """the bank and byte offset a constant read of word names, or None"""
    if (word >> 48) & 0xFFF8 == 0xEF90:  # LDC: its offset is bytes, added to Ra
        offset = (word >> 20) & 0xFFFF
        return (word >> 36) & 0x1F, offset - 0x10000 if offset & 0x8000 else offset
    if word >> 60 == 0x4:
        return (word >> 34) & 0x1F, ((word >> 20) & 0x3FFF) << 2
    return None


def programs(data):
    """(module offset, program, stage, its instruction words in order, scheduling words left out) of data's modules"""
    at = data.find(MAGIC)
    while at >= 0:
        if at + 24 <= len(data):
            _, header, control, code, first, count = struct.unpack_from("<6I", data, at)
            end = at + control + code
            if header == 24 and count <= 8 and end <= len(data):
                for p in range(count):
                    kind, entry = struct.unpack_from("<2I", data, at + first + 64 * p)
                    start = at + control + entry + (0 if kind == COMPUTE else 0x50)
                    words = [struct.unpack_from("<Q", data, w)[0] for w in range(start, end - 7, 8)]
                    yield at, p, kind, [w for k, w in enumerate(words) if k % 4]
        at = data.find(MAGIC, at + 4)


def check(data):
    """every bank 0 read outside the stage's NVN tables and every bank 2 read: (module, program, stage, instruction,
    bank, offset)"""
    found = []
    for module, program, stage, words in programs(data):
        for i, word in enumerate(words):
            if word == EXIT:
                break
            named = read(word)
            if named is None:
                continue
            bank, offset = named
            if bank == 2 or (bank == 0 and not any(lo <= offset < hi for lo, hi in tables(stage))):
                found.append((module, program, stage, i, bank, offset))
    return found


def main(paths):
    status = 0
    for path in paths:
        with open(path, "rb") as f:
            data = f.read()
        if MAGIC not in data:
            print("%s: no DKSH module" % path)
            status = max(status, 2)
            continue
        found = check(data)
        for module, program, stage, i, bank, offset in found:
            stage_name = STAGES[stage] if stage < len(STAGES) else "type %d" % stage
            where = " or ".join("%#x to %#x" % (lo, hi - 1) for lo, hi in tables(stage) if stage < len(STAGES))
            print("%s: module at %#x, %s program %d, instruction %d reads c[%#x][%#x], where NVN's driver keeps %s" %
                  (path, module, stage_name, program, i, bank, offset,
                   "texture handles" if bank == 2 else "a %s program's tables at %s" % (stage_name, where)))
            status = max(status, 1)
        if not found:
            print("%s: every bank 0 read in NVN's tables, none of bank 2" % path)
    return status


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__.strip().splitlines()[2].strip(), file=sys.stderr)
        sys.exit(2)
    sys.exit(main(sys.argv[1:]))
