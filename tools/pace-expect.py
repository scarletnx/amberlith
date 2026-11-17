#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""What the pace family's loops and memory walk must leave, worked out apart from the program.

source/paceref.c restates in C what source/pacework.c's instructions do; this restates it once more,
sharing no code with either, and source/tests/t_paceref.c holds paceref.c to the table printed here
(tools/test_pace_expect.py holds that file to this script). The floating point loop is run twice,
in whole numbers and in this machine's doubles, and the two must agree: every value it holds is a
whole number a double keeps exactly, which is why no rounding rule decides its answer. The vector
loop's singles are checked to stay under 2^24, where a single keeps every whole number.

  tools/pace-expect.py --c    the table, as t_paceref.c holds it
"""

import math
import sys

MASK64 = (1 << 64) - 1
MASK32 = (1 << 32) - 1

INTEGER_A = 6364136223846793005
INTEGER_C = 1442695040888963407
INTEGER_X = 0x0123456789ABCDEF
INTEGER_Y = 0xFEDCBA9876543210

FLOAT_A = 1664525
FLOAT_C = 1013904223
FLOAT_X = 12345

VECTOR_A = (3125, 2053, 1229, 4093)
VECTOR_C = (1013, 2027, 3041, 4057)
VECTOR_X = (1, 22, 333, 4000)
VECTOR_M = (1664525, 22695477, 1103515245, 134775813)
VECTOR_D = (1013904223, 1, 12345, 1)
VECTOR_I = (1, 2, 3, 4)

# the iterations pinned: the first two by hand's reach, a thousand, and each loop's chunk
INTEGER_CHUNK = 1 << 18
FLOAT_CHUNK = 1 << 16
VECTOR_CHUNK = 1 << 16
WALK_SLOTS = 1 << 23
WALK_STEPS = 1 << 18


def integer_loop(iterations):
    x, y, z, total = INTEGER_X, INTEGER_Y, 0, 0
    for _ in range(iterations):
        x = (x * INTEGER_A + INTEGER_C) & MASK64
        y ^= x >> 29
        z = (z + y) & MASK64
        y = ((y >> 17) | (y << 47)) & MASK64
        total = (total + (y if z > x else z)) & MASK64
    return total, z


def float_loop(iterations):
    """in whole numbers, and in doubles the way the instructions go; they must agree"""
    x, total = FLOAT_X, 0
    fx, ftotal = float(FLOAT_X), 0.0
    for _ in range(iterations):
        x = (x * FLOAT_A + FLOAT_C) & 0xFFFFFF
        total += max(0x1000000 - x, x)
        t = fx * float(FLOAT_A) + float(FLOAT_C)
        q = float(math.trunc(t * 2.0**-24))
        fx = t - q * 2.0**24
        ftotal += max(2.0**24 - fx, fx)
        if fx != float(x) or ftotal != float(total) or total >= 1 << 53:
            raise AssertionError("the doubles left the whole numbers")
    return total, x


def vector_loop(iterations):
    total = [0, 0, 0, 0]
    whole = list(VECTOR_I)
    x = list(VECTOR_X)
    for _ in range(iterations):
        for lane in range(4):
            t = x[lane] * VECTOR_A[lane] + VECTOR_C[lane]
            if t >= 1 << 24:
                raise AssertionError("a single cannot keep %d whole" % t)
            x[lane] = t & 0xFFF
            whole[lane] = (whole[lane] * VECTOR_M[lane] + VECTOR_D[lane]) & MASK32
            total[lane] = (total[lane] + ((whole[lane] >> 13) ^ x[lane])) & MASK32
    return total + whole + x


def walk_next(slot, slots):
    return (slot * INTEGER_A + INTEGER_C) % slots


def walk_order(slots):
    """the slots in the order a walk from slot 0 reads them, one step a slot: the last is 0 again"""
    order, slot = [], 0
    for _ in range(slots):
        slot = walk_next(slot, slots)
        order.append(slot)
    return order


def walk_end(slot, steps, slots):
    for _ in range(steps):
        slot = walk_next(slot, slots)
    return slot


def print_c():
    print("static const struct {")
    print("    uint64_t iterations;")
    print("    uint64_t sums[2];")
    print("} INTEGER[] = {")
    for n in (1, 2, 1000, INTEGER_CHUNK):
        total, z = integer_loop(n)
        print("    { %dull, { 0x%016xull, 0x%016xull } }," % (n, total, z))
    print("}, FLOATING[] = {")
    for n in (1, 2, 1000, FLOAT_CHUNK):
        total, x = float_loop(n)
        print("    { %dull, { %dull, %dull } }," % (n, total, x))
    print("};")
    print("static const struct {")
    print("    uint64_t iterations;")
    print("    uint32_t sums[12];")
    print("} VECTOR[] = {")
    for n in (1, 2, 1000, VECTOR_CHUNK):
        row = vector_loop(n)
        print("    { %dull," % n)
        print("      { %s," % ", ".join("0x%08xu" % v for v in row[0:4]))
        print("        %s," % ", ".join("0x%08xu" % v for v in row[4:8]))
        print("        %s } }," % ", ".join("%uu" % v for v in row[8:12]))
    print("};")
    order = walk_order(16)
    if sorted(order) != list(range(16)):
        raise AssertionError("the walk of 16 slots misses one")
    print("static const uint8_t WALK_OF_16[16] = { %s };" % ", ".join(str(s) for s in order))
    print("static const uint64_t WALK_END = %dull;" % walk_end(0, WALK_STEPS, WALK_SLOTS))


def main(argv):
    if argv[1:] == ["--c"]:
        print_c()
        return 0
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
