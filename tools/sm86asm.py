#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Assembles a Switch 2 shader program from its instructions, written out by hand.

    tools/sm86asm.py SOURCE --code OUT.bin --control OUT.ctl [--list]

The Switch 2's graphics unit runs the 128-bit instructions ScarletNX calls SM86, and no compiler a homebrew program
can use writes them. This assembler takes them one a line and writes the two things NVN takes a stage as: the code
(a graphics stage's program header first, then the instructions and the branch to itself that ends every program,
then the constants on a 0x100 boundary) and the control section that describes it (docs/Sm86.md has the syntax, the
instructions and the fields).

Where an encoding comes from. The opcode of every instruction, and the forms it has, are read from
tools/sm86/sm86.decode, ScarletNX's own decode table at the commit tools/sm86/PIN names: the assembler looks a
mnemonic up there and refuses a form the table does not hold. The operands' places are this file's, the common ones
(SPEC_ALU) and each instruction's own (the encoders below), as ScarletNX's decoder and translator read them back.
So a program assembled here and read by ScarletNX's decoder comes back as written when the two agree on the
operands, which the round trip checks (docs/Sm86.md); that the table is right about the hardware is for a
program's pixels to say, against what the processor works out.

--list prints each instruction with its two words, as it was assembled."""

import argparse
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TABLE = os.path.join(HERE, "sm86", "sm86.decode")

RZ = 255
URZ = 63
PT = 7


class Refused(Exception):
    """a line the assembler does not take, with the value it was given"""


# ---- the table

def read_table(path=TABLE):
    """{name: {form: 12-bit code}} of sm86.decode: each row is a name, four don't-care bits (the guard), the three
    bits of the form and the nine of the opcode"""
    table = {}
    with open(path, encoding="utf-8") as file:
        for number, line in enumerate(file, 1):
            fields = line.split("#")[0].split()
            if not fields:
                continue
            if len(fields) != 4 or fields[1] != "----" or not re.fullmatch("[01]{3}", fields[2]) \
                    or not re.fullmatch("[01]{9}", fields[3]):
                raise Refused("%s:%d: a row that is not NAME ---- FFF OOOOOOOOO: %s" % (path, number, line.strip()))
            form = int(fields[2], 2)
            forms = table.setdefault(fields[0], {})
            if form in forms:
                raise Refused("%s:%d: %s has form %d twice" % (path, number, fields[0], form))
            forms[form] = form << 9 | int(fields[3], 2)
    return table


# ---- an instruction's 128 bits

class Word:
    def __init__(self):
        self.value = 0
        self.taken = 0

    def put(self, low, count, value, what):
        """value into the count bits from low, each bit written once"""
        if not 0 <= value < 1 << count:
            raise Refused("%s %d (%#x) does not fit the %d bits at %d" % (what, value, value, count, low))
        mask = ((1 << count) - 1) << low
        if self.taken & mask:
            raise Refused("%s lands on bits %d to %d, which the instruction already holds"
                          % (what, low, low + count - 1))
        self.taken |= mask
        self.value |= value << low

    def force(self, low, count, value, what):
        """a field given by hand, over whatever is there"""
        if not 0 <= value < 1 << count:
            raise Refused("%s %d (%#x) does not fit the %d bits at %d" % (what, value, value, count, low))
        mask = ((1 << count) - 1) << low
        self.taken |= mask
        self.value = (self.value & ~mask) | value << low

    def words(self):
        return self.value & (1 << 64) - 1, self.value >> 64


# ---- operands

class Reg:
    def __init__(self, number, neg=False, absolute=False, uniform=False, wide=False):
        self.number, self.neg, self.abs, self.uniform, self.wide = number, neg, absolute, uniform, wide


class Imm:
    def __init__(self, value):
        self.value = value


class Cbuf:
    def __init__(self, bank, offset, index=None):
        self.bank, self.offset, self.index = bank, offset, index


class Pred:
    def __init__(self, number, inverted=False):
        self.number, self.inverted = number, inverted


class Mem:
    """[Ra + URb + offset], a[offset] or [Ra + offset]"""
    def __init__(self, kind, reg=RZ, ureg=None, offset=0, wide=False):
        self.kind, self.reg, self.ureg, self.offset, self.wide = kind, reg, ureg, offset, wide


class Name:
    def __init__(self, text):
        self.text = text


def number(text, what):
    try:
        if re.fullmatch(r"-?(0x[0-9a-fA-F]+|\d+)", text):
            return int(text, 0)
        if re.fullmatch(r"-?(\d+\.\d*|\.\d+|\d+)([eE][-+]?\d+)?f", text):
            return struct.unpack("<I", struct.pack("<f", float(text[:-1])))[0]
    except (ValueError, OverflowError, struct.error):
        pass
    raise Refused("%s %s is not a number: decimal, 0x hexadecimal, or a float ending in f" % (what, text))


def register(text):
    """a Reg for R12, RZ, UR4, URZ, with -, | | and .U32 around it, or None"""
    match = re.fullmatch(r"(-)?(\|)?(U?)R(Z|\d+)(\|)?(\.U32|\.64)?", text)
    if not match or bool(match.group(2)) != bool(match.group(5)):
        return None
    uniform = match.group(3) == "U"
    top = URZ if uniform else RZ
    value = top if match.group(4) == "Z" else int(match.group(4))
    if value > top:
        raise Refused("%s is past the last %sregister, %d" % (text, "uniform " if uniform else "", top - 1))
    return Reg(value, match.group(1) is not None, match.group(2) is not None, uniform, match.group(6) == ".64")


def predicate(text):
    match = re.fullmatch(r"(!)?(U?)P(T|\d)", text)
    if not match:
        return None
    value = PT if match.group(3) == "T" else int(match.group(3))
    if value > 6 and match.group(3) != "T":
        raise Refused("%s is past the last predicate, P6" % text)
    return Pred(value, match.group(1) is not None)


def operand(text):
    text = text.strip()
    found = register(text) or predicate(text)
    if found:
        return found
    match = re.fullmatch(r"c\[(\w+)\]\[([^\]]+)\]", text)
    if match:
        parts = [p.strip() for p in match.group(2).split("+")]
        index = register(parts[0]) if len(parts) == 2 else None
        if len(parts) > 2 or (len(parts) == 2 and index is None):
            raise Refused("%s: a constant buffer's place is c[bank][offset] or c[bank][R + offset]" % text)
        return Cbuf(number(match.group(1), "the bank"), number(parts[-1], "the offset"), index)
    match = re.fullmatch(r"a\[([^\]]+)\]", text)
    if match:
        return Mem("attribute", offset=number(match.group(1).strip(), "the attribute's address"))
    match = re.fullmatch(r"\[([^\]]+)\]", text)
    if match:
        reg, ureg, offset, wide = RZ, None, 0, False
        for part in (p.strip() for p in match.group(1).split("+")):
            found = register(part)
            if found is None:
                offset = number(part, "the offset")
            elif found.uniform:
                ureg = found.number
            else:
                reg, wide = found.number, found.wide
        return Mem("memory", reg, ureg, offset, wide)
    if re.fullmatch(r"-?(0x[0-9a-fA-F]+|\d+|(\d+\.\d*|\.\d+|\d+)([eE][-+]?\d+)?f)", text):
        return Imm(number(text, "the value") & 0xFFFFFFFF)
    if re.fullmatch(r"[A-Za-z_.][\w.]*", text):
        return Name(text)
    raise Refused("%s is no operand: a register, a predicate, a number, c[bank][offset], a[address], "
                  "[R + UR + offset] or a label" % text)


def want(value, kind, what, mnemonic):
    if not isinstance(value, kind):
        raise Refused("%s: %s has to be %s" % (mnemonic, what, kind.__doc__ or kind.__name__.lower()))
    return value


def reg_of(value, what, mnemonic, uniform=False):
    if not isinstance(value, Reg) or value.uniform != uniform or value.neg or value.abs:
        raise Refused("%s: %s has to be a plain %sregister" % (mnemonic, what, "uniform " if uniform else ""))
    return value.number


# ---- the encoders

class Program:
    def __init__(self, table):
        self.table = table
        self.lines = []      # (source line number, text, label or None, mnemonic, modifiers, operands, raw fields)
        self.labels = {}
        self.stage = None
        self.settings = {}
        self.constants = bytearray()

    def code(self, mnemonic, form, where):
        forms = self.table.get(mnemonic)
        if forms is None:
            raise Refused("%s is not in sm86.decode" % mnemonic)
        if form not in forms:
            raise Refused("%s has no form %d in sm86.decode, which holds its form%s %s%s"
                          % (mnemonic, form, "" if len(forms) == 1 else "s",
                             ", ".join(str(f) for f in sorted(forms)), where))
        return forms[form]


def start(word, code, guard):
    """what every instruction has: its code, its guard, and no barrier set or waited for"""
    word.put(0, 12, code, "the opcode")
    word.put(12, 3, guard.number, "the guard")
    word.put(15, 1, int(guard.inverted), "the guard's negation")
    word.put(110, 3, 7, "the write barrier")
    word.put(113, 3, 7, "the read barrier")


def sign_bits(word, value, neg_at, abs_at, signs, mnemonic):
    """a register's - and | | at their bits, where the instruction has them: signs holds "-" and "|" for the ones
    it does, and its other bits there mean something else"""
    for mark, given, at, what in (("-", value.neg, neg_at, "the negation"),
                                  ("|", value.abs, abs_at, "the absolute value")):
        if mark in signs:
            word.put(at, 1, int(given), what)
        elif given:
            raise Refused("%s: this source takes no %s" % (mnemonic, "-" if mark == "-" else "| |"))


def source_at_32(word, value, mnemonic, signs):
    """a source in the low word: a register (neg 63, abs 62), a uniform register (bit 91), an immediate or a
    constant buffer"""
    if isinstance(value, Reg):
        word.put(32, 6 if value.uniform else 8, value.number, "the register")
        if value.uniform:
            word.put(91, 1, 1, "the uniform datapath's bit")
        sign_bits(word, value, 63, 62, signs, mnemonic)
    elif isinstance(value, Imm):
        word.put(32, 32, value.value, "the immediate")
    elif isinstance(value, Cbuf):
        if value.index is not None:
            raise Refused("%s: an indexed constant buffer is LDC's, not a source's" % mnemonic)
        word.put(38, 16, value.offset, "the constant buffer's offset")
        word.put(54, 5, value.bank, "the constant buffer's bank")
    else:
        raise Refused("%s: a source has to be a register, a number or c[bank][offset]" % mnemonic)


def source_at_64(word, value, mnemonic, signs):
    if not isinstance(value, Reg) or value.uniform:
        raise Refused("%s: with these operands the other source has to be a register" % mnemonic)
    word.put(64, 8, value.number, "the register")
    sign_bits(word, value, 75, 74, signs, mnemonic)


def alu(program, mnemonic, guard, dst, a, b, c, signs=("", "", "")):
    """An ALU instruction: the destination at 16, a at 24, and b and c where the form puts them. The form follows
    from what b and c are, as Mesa's NAK chooses it: 1 both registers, 2 c a number, 3 c a constant buffer, 4 b a
    number, 5 b a constant buffer, 6 b a uniform register, 7 c a uniform register. signs: which of - and | | each
    of a, b and c takes in this instruction, whose bits mean something else where it takes none"""
    def kind(value):
        return "-" if value is None else ("u" if value.uniform else "r") if isinstance(value, Reg) \
            else "i" if isinstance(value, Imm) else "c" if isinstance(value, Cbuf) else "?"
    kinds = kind(b) + kind(c)
    form = {"rr": 1, "ri": 2, "rc": 3, "ir": 4, "cr": 5, "ur": 6, "ru": 7,
            "r-": 1, "i-": 4, "c-": 5, "u-": 6}.get(kinds)
    if form is None:
        raise Refused("%s: no form takes these sources together: %s"
                      % (mnemonic, " and ".join({"r": "a register", "u": "a uniform register", "i": "a number",
                                                 "c": "a constant buffer", "?": "neither a register, a number nor "
                                                 "a constant buffer"}[k] for k in kinds if k != "-")))
    word = Word()
    start(word, program.code(mnemonic, form, ""), guard)
    word.put(16, 8, dst, "the destination")
    if not isinstance(a, Reg) or a.uniform:
        raise Refused("%s: the first source has to be a register" % mnemonic)
    word.put(24, 8, a.number, "the first source")
    sign_bits(word, a, 72, 73, signs[0], mnemonic)
    if form in (2, 3, 7):
        source_at_64(word, b, mnemonic, signs[1])
        source_at_32(word, c, mnemonic, signs[2])
    else:
        source_at_32(word, b, mnemonic, signs[1])
        if c is not None:    # an instruction of two sources leaves the third's place nought, as Mesa's NAK does
            source_at_64(word, c, mnemonic, signs[2])
    return word


def modifiers(mnemonic, given, known):
    """{field: value} for the modifiers given, each one of known: {name: (group, value)}; a group named twice or a
    name that is none of them is refused"""
    out = {}
    for name in given:
        if name not in known:
            raise Refused("%s takes no .%s: its modifiers are %s"
                          % (mnemonic, name, ", ".join("." + k for k in known) or "none"))
        group, value = known[name]
        if group in out:
            raise Refused("%s.%s: two modifiers of one kind" % (mnemonic, ".".join(given)))
        out[group] = value
    return out


ROUND = {"RN": ("round", 0), "RM": ("round", 1), "RP": ("round", 2), "RZ": ("round", 3)}
FLOAT = dict(ROUND, FTZ=("ftz", 1), SAT=("sat", 1))
# Mesa's NAK numbers, which ScarletNX's translator compares by: integers in three bits, floats in four
INT_COMPARE = {"LT": 1, "EQ": 2, "LE": 3, "GT": 4, "NE": 5, "GE": 6}
FLOAT_COMPARE = dict(INT_COMPARE, NUM=7, NAN=8, LTU=9, EQU=10, LEU=11, GTU=12, NEU=13, GEU=14)
COMBINE = {"AND": 0, "OR": 1, "XOR": 2}
SPECIAL = {"SR_LANEID": 0x00, "SR_TID.X": 0x21, "SR_TID.Y": 0x22, "SR_TID.Z": 0x23, "SR_CTAID.X": 0x25,
           "SR_CTAID.Y": 0x26, "SR_CTAID.Z": 0x27}
# a load's or a store's size, bits 73 to 75
SIZE = {"U8": ("size", 0), "S8": ("size", 1), "U16": ("size", 2), "S16": ("size", 3), "32": ("size", 4),
        "64": ("size", 5)}


def float_fields(word, got):
    word.put(77, 1, got.get("sat", 0), ".SAT")
    word.put(78, 2, got.get("round", 0), "the rounding mode")
    word.put(80, 1, got.get("ftz", 0), ".FTZ")


def predicate_at(word, low, value, what, mnemonic):
    if not isinstance(value, Pred):
        raise Refused("%s: %s has to be a predicate" % (mnemonic, what))
    word.put(low, 3, value.number, what)
    word.put(low + 3, 1, int(value.inverted), what + "'s negation")


def plain_predicate(value, what, mnemonic):
    if not isinstance(value, Pred) or value.inverted:
        raise Refused("%s: %s has to be a predicate that is written, P0 to P6 or PT for none" % (mnemonic, what))
    return value.number


def count(mnemonic, operands, *allowed):
    if len(operands) not in allowed:
        raise Refused("%s takes %s operands, and has %d" % (mnemonic, " or ".join(str(n) for n in allowed),
                                                            len(operands)))


def encode(program, index, mnemonic, mods, guard, ops):
    """the Word of one instruction; index is its place in the program, for a branch"""
    if mnemonic == "MOV":
        count(mnemonic, ops, 2)
        modifiers(mnemonic, mods, {})
        word = alu(program, "MOV", guard, reg_of(ops[0], "the destination", mnemonic), Reg(RZ), ops[1], None)
        word.put(72, 4, 0xF, "the lanes")   # every lane of a quad written, Mesa's NAK's quad_lanes
        return word
    if mnemonic in ("FADD", "FMUL", "FFMA"):
        count(mnemonic, ops, 4 if mnemonic == "FFMA" else 3)
        got = modifiers(mnemonic, mods, FLOAT)
        word = alu(program, mnemonic, guard, reg_of(ops[0], "the destination", mnemonic), ops[1], ops[2],
                   ops[3] if mnemonic == "FFMA" else None, signs=("-|", "-|", "-|"))
        float_fields(word, got)
        if mnemonic == "FMUL":
            word.put(84, 3, 4, "the scale")   # 2^(field - 4): none
        return word
    if mnemonic in ("FSETP", "ISETP"):
        count(mnemonic, ops, 5)
        compare = FLOAT_COMPARE if mnemonic == "FSETP" else INT_COMPARE
        known = {name: ("compare", value) for name, value in compare.items()}
        known.update({name: ("combine", value) for name, value in COMBINE.items()})
        if mnemonic == "FSETP":
            known["FTZ"] = ("ftz", 1)
        else:
            known.update({"S32": ("signed", 1), "U32": ("signed", 0)})
        got = modifiers(mnemonic, mods, known)
        for need, words in (("compare", "a comparison, " + ", ".join("." + n for n in compare)),
                            ("combine", "how it joins its predicate, .AND, .OR or .XOR")) + \
                ((("signed", "its integers' kind, .S32 or .U32"),) if mnemonic == "ISETP" else ()):
            if need not in got:
                raise Refused("%s needs %s" % (mnemonic, words))
        word = alu(program, mnemonic, guard, RZ, ops[2], ops[3], None,
                   signs=("-|", "-|", "") if mnemonic == "FSETP" else ("", "", ""))
        if mnemonic == "FSETP":
            word.put(76, 4, got["compare"], "the comparison")
            word.put(80, 1, got.get("ftz", 0), ".FTZ")
        else:
            word.put(76, 3, got["compare"], "the comparison")
            word.put(73, 1, got["signed"], "the integers' kind")
        word.put(74, 2, got["combine"], "the combination")
        word.put(81, 3, plain_predicate(ops[0], "the predicate it sets", mnemonic), "the predicate it sets")
        word.put(84, 3, plain_predicate(ops[1], "the predicate it sets to the opposite", mnemonic),
                 "the predicate it sets to the opposite")
        predicate_at(word, 87, ops[4], "the predicate it joins", mnemonic)
        return word
    if mnemonic in ("IADD3", "IMAD"):
        count(mnemonic, ops, 4)
        modifiers(mnemonic, mods, {})
        word = alu(program, mnemonic, guard, reg_of(ops[0], "the destination", mnemonic), ops[1], ops[2], ops[3],
                   signs=("-", "-", "-") if mnemonic == "IADD3" else ("", "", "-"))
        if mnemonic == "IADD3":
            word.put(81, 3, PT, "the first carry")
            word.put(84, 3, PT, "the second carry")
        return word
    if mnemonic == "SHF":
        count(mnemonic, ops, 4)
        got = modifiers(mnemonic, mods, {"L": ("right", 0), "R": ("right", 1), "S64": ("type", 0), "U64": ("type", 1),
                                         "S32": ("type", 2), "U32": ("type", 3), "W": ("wrap", 1), "HI": ("high", 1)})
        if "right" not in got or "type" not in got:
            raise Refused("SHF needs a direction, .L or .R, and a type, .S32, .U32, .S64 or .U64")
        word = alu(program, mnemonic, guard, reg_of(ops[0], "the destination", mnemonic), ops[1], ops[2], ops[3])
        word.put(73, 2, got["type"], "the type")
        word.put(75, 1, got.get("wrap", 0), ".W")
        word.put(76, 1, got["right"], "the direction")
        word.put(80, 1, got.get("high", 0), ".HI")
        return word
    if mnemonic == "LOP3":
        count(mnemonic, ops, 5)
        modifiers(mnemonic, mods, {"LUT": ("lut", 1)})
        table = want(ops[4], Imm, "the truth table", mnemonic).value
        word = alu(program, mnemonic, guard, reg_of(ops[0], "the destination", mnemonic), ops[1], ops[2], ops[3])
        word.put(72, 8, table, "the truth table")
        word.put(81, 3, PT, "the predicate it sets")
        word.put(87, 4, 0xF, "the predicate operand")   # none: !PT
        return word
    if mnemonic in ("I2F", "F2I"):
        count(mnemonic, ops, 2)
        known = {"S32": ("signed", 1), "U32": ("signed", 0)}
        known.update(ROUND if mnemonic == "I2F" else
                     {"NEAREST": ("round", 0), "FLOOR": ("round", 1), "CEIL": ("round", 2), "TRUNC": ("round", 3),
                      "FTZ": ("ftz", 1)})
        got = modifiers(mnemonic, mods, known)
        if "signed" not in got or (mnemonic == "F2I" and "round" not in got):
            raise Refused("%s needs its integer's kind, .S32 or .U32%s"
                          % (mnemonic, ", and a rounding: .NEAREST, .FLOOR, .CEIL or .TRUNC"
                             if mnemonic == "F2I" else ""))
        word = alu(program, mnemonic, guard, reg_of(ops[0], "the destination", mnemonic), Reg(RZ), ops[1], None,
                   signs=("", "-|", "") if mnemonic == "F2I" else ("", "", ""))
        word.put(75, 2, 2, "the destination's size")   # log2 of its bytes
        word.put(84, 2, 2, "the source's size")
        word.put(78, 2, got.get("round", 0), "the rounding mode")
        if mnemonic == "I2F":
            word.put(74, 1, got["signed"], "the integer's kind")
        else:
            word.put(72, 1, got["signed"], "the integer's kind")
            word.put(80, 1, got.get("ftz", 0), ".FTZ")
        return word
    if mnemonic in ("SEL", "IMNMX", "FMNMX"):
        count(mnemonic, ops, 4)
        got = modifiers(mnemonic, mods, {"S32": ("signed", 1), "U32": ("signed", 0)} if mnemonic == "IMNMX"
                        else {"FTZ": ("ftz", 1)} if mnemonic == "FMNMX" else {})
        if mnemonic == "IMNMX" and "signed" not in got:
            raise Refused("IMNMX needs its integers' kind, .S32 or .U32")
        word = alu(program, mnemonic, guard, reg_of(ops[0], "the destination", mnemonic), ops[1], ops[2], None,
                   signs=("-|", "-|", "") if mnemonic == "FMNMX" else ("", "", ""))
        if mnemonic == "IMNMX":
            word.put(73, 1, got["signed"], "the integers' kind")
        if mnemonic == "FMNMX":
            word.put(80, 1, got.get("ftz", 0), ".FTZ")
        predicate_at(word, 87, ops[3], "the predicate that chooses", mnemonic)
        return word

    # ---- the instructions with one code and places of their own
    word = Word()
    if mnemonic in ("EXIT", "NOP", "KILL"):
        count(mnemonic, ops, 0)
        modifiers(mnemonic, mods, {})
        start(word, single(program, mnemonic), guard)
        if mnemonic == "EXIT":
            word.put(87, 3, PT, "the predicate operand")
        return word
    if mnemonic == "BRA":
        count(mnemonic, ops, 1)
        modifiers(mnemonic, mods, {})
        label = want(ops[0], Name, "the target", mnemonic).text
        if label not in program.labels:
            raise Refused("BRA to %s, which is no label of the program" % label)
        start(word, single(program, mnemonic), guard)
        word.put(87, 3, PT, "the predicate operand")
        # in 32-bit words from the instruction after this one, 48 bits two's complement
        word.put(34, 48, ((program.labels[label] - index - 1) * 4) & (1 << 48) - 1, "the offset")
        return word
    if mnemonic == "S2R":
        count(mnemonic, ops, 2)
        modifiers(mnemonic, mods, {})
        name = want(ops[1], Name, "the special register", mnemonic).text
        if name not in SPECIAL:
            raise Refused("S2R of %s: the special registers the assembler names are %s"
                          % (name, ", ".join(SPECIAL)))
        start(word, single(program, mnemonic), guard)
        word.put(16, 8, reg_of(ops[0], "the destination", mnemonic), "the destination")
        word.put(72, 8, SPECIAL[name], "the special register")
        return word
    if mnemonic in ("ULDC", "LDC"):
        count(mnemonic, ops, 2)
        got = modifiers(mnemonic, mods, SIZE)
        cbuf = want(ops[1], Cbuf, "the source", mnemonic)
        start(word, single(program, mnemonic), guard)
        if mnemonic == "ULDC":
            if cbuf.index is not None:
                raise Refused("ULDC takes c[bank][offset], with no register")
            word.put(16, 6, reg_of(ops[0], "the destination", mnemonic, uniform=True), "the destination")
        else:
            word.put(16, 8, reg_of(ops[0], "the destination", mnemonic), "the destination")
            word.put(24, 8, reg_of(cbuf.index, "the index", mnemonic) if cbuf.index is not None else RZ,
                     "the index")
        word.put(38, 16, cbuf.offset, "the constant buffer's offset")
        word.put(54, 5, cbuf.bank, "the constant buffer's bank")
        word.put(73, 3, got.get("size", 4), "the size")
        return word
    if mnemonic in ("LDG", "STG"):
        count(mnemonic, ops, 2)
        known = dict(SIZE, E=("e", 1))
        got = modifiers(mnemonic, mods, known)
        if "e" not in got:
            raise Refused("%s needs .E, the 64-bit address every program of the console forms" % mnemonic)
        memory = want(ops[1 if mnemonic == "LDG" else 0], Mem, "the address", mnemonic)
        if memory.kind != "memory" or memory.ureg is None:
            raise Refused("%s takes [R + UR + offset]: a register, the uniform register pair that holds the "
                          "buffer's address, and an offset" % mnemonic)
        start(word, single(program, mnemonic), guard)
        word.put(24, 8, memory.reg, "the address register")
        word.put(40, 24, memory.offset & 0xFFFFFF, "the offset")
        if not -(1 << 23) <= memory.offset < 1 << 23:
            raise Refused("%s: the offset %d is outside the 24 signed bits it has" % (mnemonic, memory.offset))
        word.put(72, 1, 1, ".E")
        word.put(73, 3, got.get("size", 4), "the size")
        word.put(91, 1, 1, "the uniform datapath's bit")
        if mnemonic == "LDG":
            word.put(16, 8, reg_of(ops[0], "the destination", mnemonic), "the destination")
            word.put(32, 6, memory.ureg, "the uniform register")
            word.put(81, 3, PT, "the predicate it sets")
        else:
            word.put(32, 8, reg_of(ops[1], "the value", mnemonic), "the value")
            word.put(64, 6, memory.ureg, "the uniform register")
        return word
    if mnemonic == "IPA":
        count(mnemonic, ops, 2)
        got = modifiers(mnemonic, mods, {"CENTROID": ("loc", 1), "CONSTANT": ("freq", 1)})
        attribute = want(ops[1], Mem, "the input", mnemonic)
        if attribute.kind != "attribute" or attribute.offset & 3 or not 0 <= attribute.offset < 0x400:
            raise Refused("IPA takes a[address], an input's address in whole words below 0x400")
        start(word, single(program, mnemonic), guard)
        word.put(16, 8, reg_of(ops[0], "the destination", mnemonic), "the destination")
        word.put(24, 8, 0, "the unused register")
        word.put(32, 8, RZ, "the offset register")
        word.put(64, 8, attribute.offset >> 2, "the input's address")
        word.put(76, 2, got.get("loc", 0), "the location")
        word.put(78, 2, got.get("freq", 0), "the frequency")
        word.put(81, 3, PT, "the predicate it sets")
        return word
    if mnemonic in ("ALD", "AST"):
        count(mnemonic, ops, 2)
        got = modifiers(mnemonic, mods, {"32": ("words", 0), "64": ("words", 1), "96": ("words", 2),
                                         "128": ("words", 3)})
        attribute = want(ops[1 if mnemonic == "ALD" else 0], Mem, "the attribute", mnemonic)
        if attribute.kind != "attribute" or not 0 <= attribute.offset < 0x400:
            raise Refused("%s takes a[address], an attribute's address below 0x400" % mnemonic)
        start(word, single(program, mnemonic), guard)
        word.put(24, 8, RZ, "the index register")
        word.put(40, 10, attribute.offset, "the attribute's address")
        word.put(74, 2, got.get("words", 0), "the words")
        if mnemonic == "ALD":
            word.put(16, 8, reg_of(ops[0], "the destination", mnemonic), "the destination")
            word.put(32, 8, RZ, "the vertex register")
        else:
            word.put(32, 8, reg_of(ops[1], "the value", mnemonic), "the value")
            word.put(64, 8, RZ, "the vertex register")
        return word
    if mnemonic in ("ISBERD", "ISBEWR"):
        count(mnemonic, ops, 2)
        got = modifiers(mnemonic, mods, {"MAP": ("access", 0), "PATCH": ("access", 1), "PRIM": ("access", 2),
                                         "ATTR": ("access", 3), "U8": ("size", 0), "U16": ("size", 1),
                                         "B32": ("size", 2), "SKEW": ("skew", 1), "O": ("output", 1)})
        if "access" not in got or "size" not in got:
            raise Refused("%s needs what it reaches, .MAP, .PATCH, .PRIM or .ATTR, and a size, .U8, .U16 or .B32"
                          % mnemonic)
        memory = want(ops[1 if mnemonic == "ISBERD" else 0], Mem, "the address", mnemonic)
        if memory.kind != "memory" or memory.ureg is not None or not 0 <= memory.offset < 1 << 16:
            raise Refused("%s takes [R + offset], the offset below 0x10000" % mnemonic)
        start(word, single(program, mnemonic), guard)
        word.put(16 if mnemonic == "ISBERD" else 32, 8,
                 reg_of(ops[0 if mnemonic == "ISBERD" else 1], "the register", mnemonic), "the register")
        word.put(24, 8, memory.reg, "the offset register")
        word.put(40, 16, memory.offset, "the offset")
        word.put(74, 2, got["size"], "the size")
        word.put(76, 2, got["access"], "what it reaches")
        word.put(78, 1, got.get("skew", 0), ".SKEW")
        word.put(79, 1, got.get("output", 0), ".O")
        return word
    raise Refused("%s is not an instruction the assembler writes yet: MOV, FADD, FMUL, FFMA, FSETP, ISETP, IADD3, "
                  "IMAD, SHF, LOP3, I2F, F2I, SEL, IMNMX, FMNMX, S2R, ULDC, LDC, LDG, STG, IPA, ALD, AST, ISBERD, "
                  "ISBEWR, BRA, EXIT, KILL and NOP" % mnemonic)


def single(program, mnemonic):
    """the code of an instruction the table gives one form"""
    forms = program.table.get(mnemonic)
    if forms is None:
        raise Refused("%s is not in sm86.decode" % mnemonic)
    if len(forms) != 1:
        raise Refused("%s has %d forms in sm86.decode, where the assembler writes it with one"
                      % (mnemonic, len(forms)))
    return next(iter(forms.values()))


# ---- a source file

STAGES = {"vertex": 0, "fragment": 1, "compute": 5, "task": 6, "mesh": 7}   # NVN's stage numbers
# a program header's first word, as ScarletNX's docs/platforms/nx/ounce.md has each stage's ("Stages")
SPH_WORD0 = {"vertex": 0x020C0481, "fragment": 0x000C5482, "task": 0x000C0481, "mesh": 0x000C0481,
             "mesh-after-task": 0x020C0C81}
SETTINGS = {
    "compute": {"local": 3, "shared": 1},
    "vertex": {"out": None},
    "fragment": {"in": None, "targets": 1},
    "mesh": {"out": None, "threads": 1, "vertices": 1, "primitives": 1, "after-task": 0, "payload": 1},
    "task": {"threads": 1, "payload": 1},
}


def split(text):
    """a line's operands: at commas outside brackets"""
    out, depth, piece = [], 0, ""
    for char in text:
        depth += char == "["
        depth -= char == "]"
        if char == "," and depth == 0:
            out.append(piece)
            piece = ""
        else:
            piece += char
    if piece.strip():
        out.append(piece)
    return out


def parse(text, table):
    program = Program(table)
    for number_, raw in enumerate(text.splitlines(), 1):
        line = raw.split(";")[0].strip()
        if not line:
            continue
        try:
            if line.startswith("."):
                directive(program, line)
                continue
            label = None
            match = re.match(r"([A-Za-z_]\w*):\s*(.*)$", line)
            if match:
                label, line = match.group(1), match.group(2)
                if label in program.labels:
                    raise Refused("the label %s is there twice" % label)
                program.labels[label] = len(program.lines)
                if not line:
                    continue
            fields = []
            if "!" in line and re.search(r"\s!\s", " " + line):
                line, tail = re.split(r"\s!\s", " " + line, maxsplit=1)
                for item in tail.split():
                    match = re.fullmatch(r"(\d+):(\d+)=(0x[0-9a-fA-F]+|\d+)", item)
                    if not match:
                        raise Refused("%s is not a field given by hand, LOW:BITS=VALUE" % item)
                    fields.append((int(match.group(1)), int(match.group(2)), int(match.group(3), 0)))
                line = line.strip()
            guard = Pred(PT)
            match = re.match(r"@(\S+)\s+(.*)$", line)
            if match:
                guard = predicate(match.group(1))
                if guard is None:
                    raise Refused("@%s is not a guard: @P0 to @P6, or @!P0" % match.group(1))
                line = match.group(2)
            head, _, rest = line.partition(" ")
            mnemonic, *mods = head.split(".")
            program.lines.append((number_, raw.strip(), mnemonic, mods, guard, [operand(o) for o in split(rest)],
                                  fields))
        except Refused as why:
            raise Refused("line %d: %s" % (number_, why)) from None
    return program


def directive(program, line):
    words = line.split()
    name = words[0][1:]
    if name == "stage":
        if len(words) != 2 or words[1] not in STAGES:
            raise Refused(".stage takes one of %s" % ", ".join(STAGES))
        if program.stage is not None:
            raise Refused(".stage is there twice")
        program.stage = words[1]
        return
    if program.stage is None:
        raise Refused(".stage comes before any other directive")
    if name == "const":
        for value in words[1:]:
            program.constants += struct.pack("<I", number(value, "a constant") & 0xFFFFFFFF)
        return
    known = SETTINGS[program.stage]
    if name not in known:
        raise Refused(".%s is no directive of a %s program: it has %s"
                      % (name, program.stage, ", ".join("." + k for k in known) + ", .const"))
    if name in program.settings:
        raise Refused(".%s is there twice" % name)
    if known[name] is None:
        program.settings[name] = words[1:]
    else:
        if len(words) - 1 != known[name]:
            raise Refused(".%s takes %d value%s, and has %d" % (name, known[name], "" if known[name] == 1 else "s",
                                                               len(words) - 1))
        program.settings[name] = [number(v, "." + name) for v in words[1:]]


def attribute_bits(names, what):
    """{(group, index): mask of components} for position, generic3.xy and the like"""
    out = {}
    for name in names:
        match = re.fullmatch(r"(position|generic(\d+))(?:\.([xyzw]{1,4}))?", name)
        if not match or (match.group(2) and int(match.group(2)) > 31):
            raise Refused("%s %s is not position or generic0 to generic31, with .xyzw components" % (what, name))
        mask = 0
        for component in match.group(3) or "xyzw":
            mask |= 1 << "xyzw".index(component)
        key = ("position", 0) if match.group(1) == "position" else ("generic", int(match.group(2)))
        out[key] = out.get(key, 0) | mask
    return out


def header(program):
    """a graphics stage's program header, 32 words: the first names the stage, and the maps say which attribute
    words the program reads and writes, at the bits ScarletNX reads them from (sm86.h's sm86_omap_generics: the
    position's four at 428, generic i's at 432 + 4 i; a fragment program's inputs two bits a component from word 6)"""
    words = [0] * 32
    stage = program.stage
    key = "mesh-after-task" if stage == "mesh" and program.settings.get("after-task", [0])[0] else stage
    words[0] = SPH_WORD0[key]
    if stage in ("vertex", "mesh"):
        for (group, index), mask in attribute_bits(program.settings.get("out", []), ".out").items():
            low = 428 if group == "position" else 432 + 4 * index
            words[low >> 5] |= mask << (low & 31)
    if stage == "fragment":
        for (group, index), mask in attribute_bits(program.settings.get("in", []), ".in").items():
            if group == "position":
                continue      # the fragment's own coordinate, which no map names
            for component in range(4):
                if mask >> component & 1:
                    words[6 + index // 4] |= 2 << ((index % 4) * 8 + component * 2)   # 2: perspective
        targets = program.settings.get("targets", [1])[0]
        if not 1 <= targets <= 8:
            raise Refused(".targets %d is not 1 to 8" % targets)
        for target in range(targets):
            words[18] |= 0xF << (4 * target)          # each colour target's four components written
    return words


def assemble(text, table=None):
    """(code bytes, control section bytes, listing lines) of a source"""
    program = parse(text, table or read_table())
    if program.stage is None:
        raise Refused("the source names no .stage")
    if not program.lines:
        raise Refused("the source has no instruction")
    words, listing = [], []
    for index, (line, raw, mnemonic, mods, guard, ops, fields) in enumerate(program.lines):
        try:
            word = encode(program, index, mnemonic, mods, guard, ops)
            for low, bits, value in fields:
                if low + bits > 128:
                    raise Refused("the field %d:%d is past the instruction's 128 bits" % (low, bits))
                word.force(low, bits, value, "the field %d:%d" % (low, bits))
        except Refused as why:
            raise Refused("line %d: %s" % (line, why)) from None
        words.append(word.words())
        listing.append("%4d  %016x %016x  %s" % (index, word.words()[0], word.words()[1], raw))
    last = program.lines[-1]
    if last[2] not in ("EXIT", "BRA") or last[4].number != PT or last[4].inverted:
        raise Refused("line %d: the program ends on %s, where its last instruction has to be an EXIT or a BRA "
                      "every lane takes" % (last[0], last[2]))
    # the branch to itself that follows every program's last instruction
    end = Word()
    start(end, single(program, "BRA"), Pred(PT))
    end.put(87, 3, PT, "the predicate operand")
    end.put(34, 48, (-4) & (1 << 48) - 1, "the offset")
    words.append(end.words())
    listing.append("%4d  %016x %016x  (the branch to itself that ends the program)" % (len(words) - 1, *end.words()))

    body = b"".join(struct.pack("<QQ", low, high) for low, high in words)
    head = b"" if program.stage == "compute" else struct.pack("<32I", *header(program))
    code = head + body
    code_bytes = len(code)
    constants_at = (code_bytes + 0xFF) & ~0xFF
    area = code + bytes(constants_at - code_bytes) + bytes(program.constants)
    area += bytes(-len(area) % 0x100)
    return area, control(program, code_bytes, constants_at, len(area)), listing


CONTROL_BYTES = 0xD8


def control(program, code_bytes, constants_at, area_bytes):
    """The stage's control section, as ScarletNX reads an Ounce game's (its nvn_object.c and nvn_section.h): the
    magic and 0xC9 where the Switch's sections have 1, format 11, then at 0x30 the section's bytes, at 0x38 the
    code's, at 0x3C the constants', at 0x40 their offset, at 0x44 the code area's bytes, the stage in the halfword
    at 0x56, and from 0x58 a compute program's block and shared memory or a mesh or task program's fields"""
    section = bytearray(CONTROL_BYTES)
    struct.pack_into("<III", section, 0, 0x98761234, 0xC9, 11)
    struct.pack_into("<I", section, 0x30, CONTROL_BYTES)
    struct.pack_into("<III", section, 0x38, code_bytes, len(program.constants), constants_at)
    struct.pack_into("<I", section, 0x44, area_bytes)
    struct.pack_into("<H", section, 0x56, STAGES[program.stage])
    settings = program.settings
    if program.stage == "compute":
        local = settings.get("local")
        if local is None or not all(1 <= n <= 1024 for n in local):
            raise Refused("a compute program needs .local X Y Z, its block, each 1 to 1024%s"
                          % ("" if local is None else ", and has %s" % " ".join(str(n) for n in local)))
        struct.pack_into("<III", section, 0x58, *local)
        struct.pack_into("<I", section, 0x64, settings.get("shared", [0])[0])
    if program.stage in ("mesh", "task"):
        threads = settings.get("threads", [0])[0]
        if not 1 <= threads <= 32:
            raise Refused("a %s program needs .threads 1 to 32, one warp, and has %d" % (program.stage, threads))
        struct.pack_into("<HH", section, 0x58, threads, settings.get("payload", [0x80])[0])
    if program.stage == "mesh":
        vertices = settings.get("vertices", [0])[0]
        primitives = settings.get("primitives", [0])[0]
        if not 1 <= vertices <= 256 or not 1 <= primitives <= 256:
            raise Refused("a mesh program needs .vertices and .primitives, each 1 to 256, and has %d and %d"
                          % (vertices, primitives))
        section[0x5E] = 1 if settings.get("after-task", [0])[0] else 0
        section[0x5F] = 3
        struct.pack_into("<HH", section, 0x60, vertices, primitives)
    return bytes(section)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("source")
    parser.add_argument("--code", required=True)
    parser.add_argument("--control", required=True)
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args(argv)
    try:
        with open(args.source, encoding="ascii") as file:
            code, section, listing = assemble(file.read())
    except Refused as why:
        print("sm86asm: %s: %s" % (args.source, why), file=sys.stderr)
        return 1
    except UnicodeDecodeError as why:
        print("sm86asm: %s is not ASCII: %s" % (args.source, why), file=sys.stderr)
        return 1
    with open(args.code, "wb") as out:
        out.write(code)
    with open(args.control, "wb") as out:
        out.write(section)
    if args.list:
        print("\n".join(listing))
    return 0


if __name__ == "__main__":
    sys.exit(main())
