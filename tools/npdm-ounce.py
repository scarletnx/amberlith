#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes the main.npdm of the probe's Ounce build, or says what one holds.

    tools/npdm-ounce.py write OUT [--name NAME] [--title HEX] [--stack BYTES] [--priority N] [--core N]
                                  [--priorities LOW-HIGH] [--cores FIRST-LAST]
    tools/npdm-ounce.py read FILE

A program's NPDM says what the system starts it with: its address space, its main thread, the cores and priorities
it may use. It is also where a machine tells a Switch 2 program from a Switch one, since nothing in the code says:
the access control descriptor (ACID) of a Switch 2 program has a signature and a key of 0x180 bytes each, where a
Switch one has 0x100, so its magic sits 0x300 into the descriptor and not 0x200 (Switchbrew's NPDM page; ScarletNX
reads exactly that, src/nx/loader/ounce.h). The toolchain's npdmtool writes the Switch's layout only.

`write` makes the smallest file a runtime takes: the META header, the descriptor with nought for its signature and
key (no machine that runs homebrew checks them, and no console starts this file), and the access control info with
one kernel capability, the thread one. Nothing else is granted, since nothing a runtime reads asks for more.

  --name NAME            the program's name, at most 16 bytes (Amberlith)
  --title HEX            its title number (0100000000001000, the one ScarletNX gives a program that has none, so the
                         game's folder is named by its recompile and not by a number)
  --stack BYTES          the main thread's stack (0x100000)
  --priority N           the main thread's priority, 0 to 63 (44, a program's default)
  --core N               the core it starts on (0)
  --priorities LOW-HIGH  the priorities its threads may take, the numerically lowest first (28-59)
  --cores FIRST-LAST     the cores they may run on (0-2, the three a program has)

`read` prints a file's fields, one a line, and whether its descriptor is a Switch 2's. Exit 1, with the reason, for
a value outside its range or a file that is no NPDM: nothing is written then."""

import argparse
import struct
import sys

META_BYTES = 0x80
ACID_BYTES = 0x340          # the signature and the key, then a header of 0x40
ACID_MAGIC_AT = 0x300       # a Switch's is at 0x200
ACI_BYTES = 0x40
DEFAULT_TITLE = 0x0100000000001000


class Refused(Exception):
    pass


def span(text, what, lowest, highest):
    """(first, last) of "A-B", both from lowest to highest and in order"""
    parts = text.split("-")
    if len(parts) != 2 or not all(p.isdigit() for p in parts):
        raise Refused("%s %s is not two numbers as FIRST-LAST" % (what, text))
    first, last = int(parts[0]), int(parts[1])
    if not lowest <= first <= last <= highest:
        raise Refused("%s %s is not a span inside %d-%d, the lower number first" % (what, text, lowest, highest))
    return first, last


def build(name="amberlith", title=DEFAULT_TITLE, stack=0x100000, priority=44, core=0, priorities=(28, 59),
          cores=(0, 2)):
    """the file's bytes"""
    raw = name.encode("ascii", "strict") if name.isascii() else None
    if raw is None or not 1 <= len(raw) <= 16:
        raise Refused("the name %r is not 1 to 16 ASCII bytes" % name)
    if not 0 < title < 1 << 64:
        raise Refused("the title %#x is not a number of 1 to 64 bits" % title)
    if not 0x1000 <= stack <= 0x10000000 or stack & 0xFFF:
        raise Refused("a stack of %#x bytes is not whole pages from 0x1000 to 0x10000000" % stack)
    if not priorities[0] <= priority <= priorities[1]:
        raise Refused("the main thread's priority %d is outside the %d-%d its threads may take"
                      % (priority, priorities[0], priorities[1]))
    if not cores[0] <= core <= cores[1]:
        raise Refused("the main thread's core %d is outside the %d-%d its threads may run on"
                      % (core, cores[0], cores[1]))
    meta = bytearray(META_BYTES)
    meta[0:4] = b"META"
    meta[0x0C] = 1 | 3 << 1                      # 64-bit code in the 39-bit address space
    meta[0x0E] = priority
    meta[0x0F] = core
    struct.pack_into("<I", meta, 0x1C, stack)
    meta[0x20:0x20 + len(raw)] = raw
    acid = bytearray(ACID_BYTES)
    acid[ACID_MAGIC_AT:ACID_MAGIC_AT + 4] = b"ACID"
    struct.pack_into("<I", acid, ACID_MAGIC_AT + 4, ACID_BYTES - 0x180)   # what the signature covers
    struct.pack_into("<QQ", acid, ACID_MAGIC_AT + 0x10, title, title)     # the titles it allows: this one
    aci = bytearray(ACI_BYTES)
    aci[0:4] = b"ACI0"
    struct.pack_into("<Q", aci, 0x10, title)
    # the kernel capabilities: one, ThreadInfo (three ones, then a nought): the numerically highest priority at
    # bits 4-9, the lowest at 10-15, the first core at 16-23 and the last at 24-31
    capability = 0x7 | priorities[1] << 4 | priorities[0] << 10 | cores[0] << 16 | cores[1] << 24
    struct.pack_into("<II", aci, 0x30, ACI_BYTES, 4)
    struct.pack_into("<II", meta, 0x78, META_BYTES, ACID_BYTES)
    struct.pack_into("<II", meta, 0x70, META_BYTES + ACID_BYTES, ACI_BYTES + 4)
    return bytes(meta + acid + aci + struct.pack("<I", capability))


def fields(data):
    """[(name, value)] of an NPDM's bytes"""
    if len(data) < META_BYTES or data[0:4] != b"META":
        raise Refused("%d bytes that do not start with META" % len(data))
    aci, aci_size, acid, acid_size = struct.unpack_from("<4I", data, 0x70)
    if aci > len(data) or aci_size < 0x38 or aci_size > len(data) - aci or data[aci:aci + 4] != b"ACI0":
        raise Refused("the access control info, %#x bytes at %#x of %#x, is not there" % (aci_size, aci, len(data)))
    if acid > len(data) or acid_size > len(data) - acid:
        raise Refused("the descriptor, %#x bytes at %#x of %#x, is not there" % (acid_size, acid, len(data)))

    def magic_at(offset):
        return acid_size >= offset + 4 and data[acid + offset:acid + offset + 4] == b"ACID"

    out = [("name", data[0x20:0x30].split(b"\0")[0].decode("ascii", "replace")),
           ("64-bit", bool(data[0x0C] & 1)),
           ("address space", (data[0x0C] >> 1) & 7),
           ("priority", data[0x0E]),
           ("core", data[0x0F]),
           ("stack", "%#x" % struct.unpack_from("<I", data, 0x1C)[0]),
           ("title", "%016x" % struct.unpack_from("<Q", data, aci + 0x10)[0]),
           ("descriptor", "Switch 2, its magic at 0x300" if magic_at(0x300)
            else "Switch, its magic at 0x200" if magic_at(0x200) else "neither: no ACID magic at 0x200 or 0x300")]
    caps, caps_size = struct.unpack_from("<II", data, aci + 0x30)
    for at in range(0, caps_size - caps_size % 4, 4):
        if caps > aci_size or caps_size > aci_size - caps:
            break
        cap = struct.unpack_from("<I", data, aci + caps + at)[0]
        if cap & 0xF == 0x7:
            out.append(("priorities", "%d-%d" % (cap >> 10 & 63, cap >> 4 & 63)))
            out.append(("cores", "%d-%d" % (cap >> 16 & 255, cap >> 24 & 255)))
            break
    return out


def number(text, what):
    try:
        return int(text, 0)
    except ValueError:
        raise Refused("%s %s is not a number" % (what, text))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = parser.add_subparsers(dest="verb", required=True)
    write = sub.add_parser("write")
    write.add_argument("out")
    write.add_argument("--name", default="amberlith")
    write.add_argument("--title", default="%016x" % DEFAULT_TITLE)
    write.add_argument("--stack", default="0x100000")
    write.add_argument("--priority", default="44")
    write.add_argument("--core", default="0")
    write.add_argument("--priorities", default="28-59")
    write.add_argument("--cores", default="0-2")
    read = sub.add_parser("read")
    read.add_argument("file")
    args = parser.parse_args(argv)
    try:
        if args.verb == "read":
            with open(args.file, "rb") as file:
                data = file.read()
            for name, value in fields(data):
                print("%s: %s" % (name, value))
            return 0
        data = build(args.name, number("0x" + args.title.lower().removeprefix("0x"), "--title"),
                     number(args.stack, "--stack"), number(args.priority, "--priority"),
                     number(args.core, "--core"), span(args.priorities, "--priorities", 0, 63),
                     span(args.cores, "--cores", 0, 3))
    except Refused as why:
        print("npdm-ounce: %s" % why, file=sys.stderr)
        return 1
    with open(args.out, "wb") as out:
        out.write(data)
    return 0


if __name__ == "__main__":
    sys.exit(main())
