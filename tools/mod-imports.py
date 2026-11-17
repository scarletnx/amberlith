#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Whether the mod's module asks of a game only what mod/imports.txt lists, exports nothing and carries nothing of
libnx's start.

    tools/mod-imports.py [--readelf PATH] [--imports FILE] [--map FILE] ELF

The module is loaded into a game's process by the game's own dynamic linker (docs/Mod.md), which binds every
undefined symbol of its dynamic table to whichever module of the game exports that name, and binds the game's own
imports to a name the module exports before the SDK's. So the build is held to five things, and exits 1 naming each
that fails:

- every undefined dynamic symbol (`readelf --dyn-syms -W`, the toolchain's by default) is a line of the list, a
  required one global and an optional one weak, and every line of the list is imported: a function of the C library
  or of libnx that the link did not find would otherwise become a silent import, bound to the game's function of
  that name or to nothing;
- no dynamic symbol is defined: the module exports nothing;
- the dynamic section (`readelf -d -W`) has the module's INIT, which the game's linker calls, and no packed
  relocations (RELR), which a linker of an older SDK does not read;
- the word after the module's first instruction leads to its MOD0 header, read from ELF's bytes;
- of libnx, the link took the kernel's calls alone (the linker's map, with --map): any other member is part of a
  start that never runs in a game's process."""

import argparse
import os
import re
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMPORTS = os.path.join(ROOT, "mod", "imports.txt")
BIN = os.path.join(os.environ.get("DEVKITPRO", "/opt/devkitpro"), "devkitA64", "bin")
READELF = os.path.join(BIN, "aarch64-none-elf-readelf")
KINDS = ("required", "optional")
# the members of libnx.a a module of a game may hold: the supervisor calls
LIBNX_MEMBERS = {"svc.o"}


def listed(text):
    """({symbol: kind}, problems) of the list's text: a kind and a symbol a line, `#` to the line's end a comment."""
    out, problems = {}, []
    for number, line in enumerate(text.splitlines(), 1):
        fields = line.split("#", 1)[0].split()
        if not fields:
            continue
        if len(fields) != 2 or fields[0] not in KINDS:
            problems.append("line %d of the list is \"%s\": a line is %s and a symbol" % (
                number, line.strip(), " or ".join(KINDS)))
        elif fields[1] in out:
            problems.append("line %d of the list names %s again" % (number, fields[1]))
        else:
            out[fields[1]] = fields[0]
    return out, problems


def dynamic_symbols(listing):
    """[(symbol, bind, section)] of `readelf --dyn-syms -W`'s lines, less the table's empty first entry and its
    section symbols."""
    out = []
    for line in listing.splitlines():
        fields = line.split()
        # Num: Value Size Type Bind Vis Ndx Name
        if len(fields) >= 8 and fields[0].endswith(":") and fields[0][:-1].isdigit() and fields[3] != "SECTION":
            out.append((fields[7], fields[4], fields[6]))
    return out


def symbol_problems(wanted, symbols):
    """What the dynamic symbols have that the list does not allow, and what the list has that they lack."""
    out = []
    imported = {}
    for name, bind, section in symbols:
        if section != "UND":
            out.append("%s is exported (%s, section %s): the game's linker would bind the game's own import of that "
                       "name to it" % (name, bind, section))
        else:
            imported[name] = bind
    for name, bind in sorted(imported.items()):
        kind = wanted.get(name)
        if kind is None:
            out.append("%s is imported (%s) and the list does not have it: a function the link did not find, or a "
                       "new line for the list" % (name, bind))
        elif kind == "required" and bind != "GLOBAL":
            out.append("%s is required and imported %s, not GLOBAL" % (name, bind))
        elif kind == "optional" and bind != "WEAK":
            out.append("%s is optional and imported %s, not WEAK: a game without it would not load the module" % (
                name, bind))
    out.extend("%s is in the list (%s) and the module does not import it" % (name, wanted[name])
               for name in sorted(set(wanted) - set(imported)))
    return out


def dynamic_problems(listing):
    """What `readelf -d -W` shows that a game's linker cannot take: no INIT, or packed relocations."""
    tags = set(re.findall(r"^\s*0x[0-9a-f]+\s+\((\w+)\)", listing, re.M))
    out = []
    if "INIT" not in tags:
        out.append("the dynamic section has no INIT: the game's linker would call nothing of the module")
    out.extend("the dynamic section has %s: packed relocations, which a linker of an older SDK does not read" % tag
               for tag in sorted(tags) if tag.startswith("RELR"))
    return out


def header_problems(image):
    """What is wrong with the module's header in an ELF64 file's bytes: the word at 4 of the segment loaded at
    address nought has to lead to "MOD0"."""
    if len(image) < 0x40 or image[:4] != b"\x7fELF" or image[4] != 2:
        return ["the file is no ELF64"]
    phoff, = struct.unpack_from("<Q", image, 0x20)
    phentsize, phnum = struct.unpack_from("<HH", image, 0x36)
    loads = []
    for i in range(phnum):
        kind, _, offset, vaddr, _, filesz = struct.unpack_from("<IIQQQQ", image, phoff + i * phentsize)
        if kind == 1:
            loads.append((vaddr, offset, filesz))

    def at(address, size):
        for vaddr, offset, filesz in loads:
            if vaddr <= address and address + size <= vaddr + filesz:
                return image[offset + address - vaddr:offset + address - vaddr + size]
        return None

    word = at(4, 4)
    if word is None:
        return ["no segment is loaded at address nought, where a module's header starts"]
    where, = struct.unpack("<I", word)
    magic = at(where, 4)
    if magic != b"MOD0":
        return ["the word after the first instruction is 0x%x, and at 0x%x the file has %s, not MOD0" % (
            where, where, "nothing" if magic is None else repr(magic))]
    return []


def map_problems(text):
    """The members of libnx.a the linker's map says it took, but the kernel's calls."""
    members = set(re.findall(r"libnx\.a\(([^)]+)\)", text))
    return ["libnx's %s is linked: part of a start that never runs in a game's process" % member
            for member in sorted(members - LIBNX_MEMBERS)]


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("elf")
    parser.add_argument("--readelf", default=READELF)
    parser.add_argument("--imports", default=IMPORTS)
    parser.add_argument("--map")
    args = parser.parse_args()
    with open(args.imports, encoding="utf-8") as f:
        wanted, problems = listed(f.read())
    if not wanted and not problems:
        sys.exit("mod-imports: %s lists no symbol" % args.imports)
    symbols = dynamic_symbols(subprocess.run([args.readelf, "--dyn-syms", "-W", args.elf], check=True,
                                             capture_output=True, text=True).stdout)
    problems += symbol_problems(wanted, symbols)
    problems += dynamic_problems(subprocess.run([args.readelf, "-d", "-W", args.elf], check=True,
                                                capture_output=True, text=True).stdout)
    with open(args.elf, "rb") as f:
        problems += header_problems(f.read())
    if args.map:
        with open(args.map, encoding="utf-8", errors="replace") as f:
            problems += map_problems(f.read())
    for line in problems:
        print("mod-imports: %s" % line, file=sys.stderr)
    if not problems:
        kinds = list(wanted.values())
        print("mod-imports: %s imports the %d required and %d optional symbols of the list, exports nothing, has "
              "its INIT and its MOD0%s" % (args.elf, kinds.count("required"), kinds.count("optional"),
                                          ", and of libnx the kernel's calls alone" if args.map else ""))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
