#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Sends programs tools/sm86asm.py assembled through ScarletNX's own decoder and translator.

    tools/sm86-roundtrip.py --snx-sm86 TOOL --out DIR SOURCE...

TOOL is the snx-sm86 a ScarletNX build makes. Each source is assembled, written under DIR, and read by TOOL with
--list --translate --validate; the round trip holds when the decoder names the instructions the source wrote, in
their order with the branch that ends the program, and the program translates and its module passes spirv-val.

What that proves is in docs/Sm86.md: the operands are where the decoder looks for them, not that the table both
sides read is right about the hardware. Exit status 1 when a source fails, with what differed."""

import argparse
import importlib.util
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SPEC = importlib.util.spec_from_file_location("sm86asm", os.path.join(HERE, "sm86asm.py"))
asm = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(asm)

# a line of the decoder's listing: the index, the guard, the name, the form and the destination
LISTED = re.compile(r"^\s*(\d+)\s+\S+\s+([A-Z0-9_.]+(?: \([a-z ]+\))?)\s+f(\d)\s+d(\d+)\b")
# the decoder's name for a mnemonic whose modifier picks the table's row
LISTED_AS = {"SULD": "SULD.P", "SUST": "SUST.P", "TEX": "TEX (uniform handle)"}
SECONDS = 60


def stage_arguments(program, sph):
    """the tool's options for one program of a stage: a graphics program's header apart, and a task or mesh
    program's fields as its control section holds them"""
    if program.stage == "compute":
        return []
    out = ["--sph", sph]

    def get(name, unset):
        return program.settings[name][0] if name in program.settings else unset
    if program.stage == "task":
        out += ["--task", "%d,%d" % (get("threads", 1), get("payload", 0))]
    if program.stage == "mesh":
        out += ["--mesh", "%d,%d,%d,%d,%d" % (get("threads", 1), get("payload", 0), get("after-task", 0),
                                              get("vertices", 0), get("primitives", 0))]
    return out


def round_trip(tool, source, out):
    """None when the round trip holds, else what differed"""
    with open(source, encoding="ascii") as file:
        text = file.read()
    code, _, _ = asm.assemble(text)
    program = asm.parse(text, asm.read_table())
    wrote = [LISTED_AS.get(line[2], line[2]) for line in program.lines] + ["BRA"]
    name = os.path.splitext(os.path.basename(source))[0]
    body, sph = os.path.join(out, name + ".code"), os.path.join(out, name + ".sph")
    head = 0 if program.stage == "compute" else 128
    with open(body, "wb") as file:    # the instructions alone, as the tool takes one program: no padding after
        file.write(code[head:head + 16 * len(wrote)])
    if head:
        with open(sph, "wb") as file:
            file.write(code[:head])
    command = [tool, body] + stage_arguments(program, sph) + ["--list", "--translate", "--validate", "--out", out]
    try:
        ran = subprocess.run(command, capture_output=True, text=True, timeout=SECONDS, check=False)
    except subprocess.TimeoutExpired:
        return "%s did not end in %d seconds" % (" ".join(command), SECONDS)
    said = ran.stdout + ran.stderr
    read = [match.group(2) for match in map(LISTED.match, said.splitlines()) if match]
    if read != wrote:
        at = next((i for i, (a, b) in enumerate(zip(wrote, read)) if a != b), min(len(wrote), len(read)))
        return ("the decoder read %d instructions where the source has %d; the first that differs is %d: %s read, "
                "%s written" % (len(read), len(wrote), at, read[at] if at < len(read) else "nothing",
                                wrote[at] if at < len(wrote) else "nothing"))
    if ran.returncode != 0:
        return "%s ended with %d: %s" % (os.path.basename(tool), ran.returncode, said.strip().splitlines()[-1:])
    if "1 programs, 1 complete, 1 translated, 1 pass spirv-val" not in said:
        why = [line.strip() for line in said.splitlines() if "program" in line or "spirv-val" in line]
        return "the program did not translate and validate: %s" % "; ".join(why)
    return None


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--snx-sm86", required=True, help="the snx-sm86 of a ScarletNX build")
    parser.add_argument("--out", required=True, help="a folder for the programs and the translated modules")
    parser.add_argument("sources", nargs="+")
    args = parser.parse_args(argv)
    if not os.access(args.snx_sm86, os.X_OK):
        print("sm86-roundtrip: --snx-sm86 %s is not a program this user can run" % args.snx_sm86, file=sys.stderr)
        return 2
    os.makedirs(args.out, exist_ok=True)
    failed = 0
    for source in args.sources:
        try:
            why = round_trip(args.snx_sm86, source, args.out)
        except asm.Refused as refused:
            why = "the assembler refused it: %s" % refused
        print("%s  %s%s" % ("ok  " if why is None else "FAIL", source, "" if why is None else ": " + why))
        failed += why is not None
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
