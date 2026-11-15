#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Whether the program reaches its NVN the way a runtime that runs NVN itself can take it.

    tools/nvn-lookups.py [--readelf PATH] [--objdump PATH] ELF

ScarletNX takes a program's NVN function when a call of the lookup returns its address, and runs its own in its
place for every call to that address (docs/NvnLayer.md). A call it cannot see takes nothing: the function keeps the
layer's body, beside others that are ScarletNX's, and the program's objects then belong to both. A compiler makes
such a call by itself, where it can tell which function a pointer holds. So, of ELF (`readelf --dyn-syms -W` and
`objdump -d`, the toolchain's by default), exit 1 naming each of:

- nvnBootstrapLoader missing from the dynamic symbols as a defined global function: ScarletNX finds it there;
- a call in NvnLoadProcs that is not through a register (`blr`), but the ones to the C library it makes
  (ALLOWED): each lookup has to be a call of the pointer the one before handed back;
- a `bl` or a `b` to one of the layer's entry points (`Nvnl_*`, `NvnLayerDeviceGetInteger`, `nvnBootstrapLoader`)
  from another function: an entry point is reached through the lookup's pointers and nothing else."""

import argparse
import os
import re
import subprocess
import sys

BIN = os.path.join(os.environ.get("DEVKITPRO", "/opt/devkitpro"), "devkitA64", "bin")
READELF = os.path.join(BIN, "aarch64-none-elf-readelf")
OBJDUMP = os.path.join(BIN, "aarch64-none-elf-objdump")
LOADER = "NvnLoadProcs"
BOOTSTRAP = "nvnBootstrapLoader"
# what NvnLoadProcs calls by name: the C library, nothing of the layer's
ALLOWED = {"memset", "snprintf"}
ENTRY = re.compile(r"^(Nvnl_\w+|NvnLayerDeviceGetInteger|nvnBootstrapLoader)$")


def exported(listing):
    """{symbol: (type, bind, section)} of `readelf --dyn-syms -W`'s lines."""
    out = {}
    for line in listing.splitlines():
        fields = line.split()
        # Num: Value Size Type Bind Vis Ndx Name
        if len(fields) >= 8 and fields[0].endswith(":") and fields[0][:-1].isdigit():
            out[fields[7]] = (fields[3], fields[4], fields[6])
    return out


def functions(disassembly):
    """[(function, [(address, mnemonic, target or None)])] of `objdump -d`'s text, a target being the function a
    branch names with no offset, `<name>`, or with one, `<name+0x10>`, as ("name", offset)."""
    out = []
    for line in disassembly.splitlines():
        head = re.match(r"^[0-9a-f]+ <([^>]+)>:$", line)
        if head:
            out.append((head.group(1), []))
            continue
        ins = re.match(r"^\s*([0-9a-f]+):\s+(?:[0-9a-f]{8}\s+)?(\S+)(?:\s+(.*))?$", line)
        if not ins or not out:
            continue
        target = None
        named = re.search(r"<([^>+]+)(?:\+0x([0-9a-f]+))?>", ins.group(3) or "")
        if named:
            target = (named.group(1), int(named.group(2) or "0", 16))
        out[-1][1].append((ins.group(1), ins.group(2), target))
    return out


def problems(table, listing):
    """Each thing a runtime could not take, as a line."""
    out = []
    entry = table.get(BOOTSTRAP)
    if entry is None:
        out.append("%s is not in the dynamic symbols: a runtime finds the program's NVN there" % BOOTSTRAP)
    elif entry[0] != "FUNC" or entry[1] != "GLOBAL" or entry[2] == "UND":
        out.append("%s is %s %s in section %s, not a defined global function" % ((BOOTSTRAP,) + entry))
    seen_loader = False
    by_name = {}   # (function, callee) of the loader's calls by name: [count, first address]
    reached = {}   # (function, callee, mnemonic) of the branches to an entry point: [count, first address]
    for name, code in listing:
        for address, mnemonic, target in code:
            if target is None or mnemonic not in ("bl", "b"):
                continue
            callee, offset = target
            if name == LOADER and mnemonic == "bl" and callee not in ALLOWED:
                seen = by_name.setdefault((name, callee), [0, address])
                seen[0] += 1
            # a branch inside a function names it with an offset; one to its entry from elsewhere is a call of it
            if ENTRY.match(callee) and offset == 0 and callee != name:
                seen = reached.setdefault((name, callee, mnemonic), [0, address])
                seen[0] += 1
        seen_loader = seen_loader or name == LOADER
    for (name, callee), (count, first) in by_name.items():
        out.append("%s calls %s by name %d times, the first at %s: lookups a runtime does not see, which take "
                   "nothing" % (name, callee, count, first))
    for (name, callee, mnemonic), (count, first) in reached.items():
        out.append("%s reaches %s with `%s` %d times, the first at %s, not through the lookup's pointer"
                   % (name, callee, mnemonic, count, first))
    if not seen_loader:
        out.append("%s is not in the program's code: nothing to check its lookups in" % LOADER)
    return out


def run(command):
    return subprocess.run(command, check=True, capture_output=True, text=True).stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--readelf", default=READELF)
    parser.add_argument("--objdump", default=OBJDUMP)
    parser.add_argument("elf")
    args = parser.parse_args()
    found = problems(exported(run([args.readelf, "--dyn-syms", "-W", args.elf])),
                     functions(run([args.objdump, "-d", "--no-show-raw-insn", args.elf])))
    for line in found:
        print("nvn-lookups: %s" % line, file=sys.stderr)
    if found:
        return 1
    print("nvn-lookups: %s is exported, %s calls every lookup through its pointer, and no entry point is reached "
          "by name" % (BOOTSTRAP, LOADER))
    return 0


if __name__ == "__main__":
    sys.exit(main())
