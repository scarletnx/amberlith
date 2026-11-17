#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Builds source/bcnimages.c: the astcimage suite's picture in every BCn format, for the bcnimage suite.

The picture is tools/astc-images.py's, the same 128x128 bytes, so the two suites ask one question of two
families of formats. It is encoded as a game's tools would: etcpak 0.9.15 for BC1, BC3, BC4 (red), BC5 (red
and green) and BC7 (its bc7enc, which picks modes 1, 5 and 6; perceptual weights for the sRGB picture), and
Pillow 12.3.0 for BC2. No encoder here writes the other BC7 modes or any BC6H block, so two more pictures are
encoded by this script with each block's mode chosen in turn: BC7's eight modes, block n in mode n % 8 with
the partitions, rotations and index selections in turn too, and BC6H's fourteen, block n in mode n % 14 with
its 32 partitions in turn. Their endpoints are fitted to the block (a bounding box a subset, quantized as the
mode allows, a transformed endpoint's delta cut to its bits) and their indices are the nearest of the
palette, so they look like the picture; how near does not matter, since the suite judges a texel by what its
block decodes to.

Then one block of every BC7 mode and every BC6H mode, and the four reserved BC6H mode codes, their bits after
the mode from a fixed hash, for the bcn suite: any bits after a mode's are a block of that mode.

Pillow decodes every block but BC6H's too, and bcdec (in imagecodecs) BC6H's; the C file carries an FNV-1a
hash of each decode, which the reference suite holds source/bcnref.c to: Pillow's bytes are RGBA (BC1, BC2,
BC3, BC7), L (BC4) and RGB with blue nought (BC5), BC1 to BC5 interpolated with exact fractions cut toward
nought; bcdec's are the halves, little endian, RGB. Pillow takes BC6H to bytes by interpolating without the
specification's rounding, which moves a byte here and there, so it is not the reference there.

    tools/bcn-images.py [--out source/bcnimages.c]

run with Pillow 12.3.0, etcpak 0.9.15 and imagecodecs 2026.11.11 (bcdec 0.985) installed
(pip install pillow==12.3.0 etcpak==0.9.15 imagecodecs==2026.11.11).
"""

import argparse
import importlib.util
import io
import math
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SIZE = 128
BLOCKS = (SIZE // 4) * (SIZE // 4)


def astc_images():
    """tools/astc-images.py, for its picture and its hash."""
    spec = importlib.util.spec_from_file_location("astc_images", os.path.join(HERE, "astc-images.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def lowbias32(s):
    s &= 0xFFFFFFFF
    s ^= s >> 16
    s = (s * 0x7FEB352D) & 0xFFFFFFFF
    s ^= s >> 15
    s = (s * 0x846CA68B) & 0xFFFFFFFF
    s ^= s >> 16
    return s


def block_texels(rgba, n):
    """The sixteen RGBA texels of block n of the picture, rows first."""
    bx, by = n % (SIZE // 4), n // (SIZE // 4)
    out = []
    for t in range(4):
        for s in range(4):
            i = ((by * 4 + t) * SIZE + bx * 4 + s) * 4
            out.append(tuple(rgba[i:i + 4]))
    return out


class Bits:
    """A block written from bit 0 up."""

    def __init__(self):
        self.value = 0
        self.at = 0

    def put(self, value, count):
        self.value |= (value & ((1 << count) - 1)) << self.at
        self.at += count

    def bytes(self):
        if self.at != 128:
            raise SystemExit("a block of %d bits, 128 wanted" % self.at)
        return self.value.to_bytes(16, "little")


# ---- what BC6H and BC7 share, as source/bcnref.c has it

PARTITION2 = (
    0xCCCC, 0x8888, 0xEEEE, 0xECC8, 0xC880, 0xFEEC, 0xFEC8, 0xEC80, 0xC800, 0xFFEC, 0xFE80, 0xE800, 0xFFE8,
    0xFF00, 0xFFF0, 0xF000, 0xF710, 0x008E, 0x7100, 0x08CE, 0x008C, 0x7310, 0x3100, 0x8CCE, 0x088C, 0x3110,
    0x6666, 0x366C, 0x17E8, 0x0FF0, 0x718E, 0x399C, 0xAAAA, 0xF0F0, 0x5A5A, 0x33CC, 0x3C3C, 0x55AA, 0x9696,
    0xA55A, 0x73CE, 0x13C8, 0x324C, 0x3BDC, 0x6996, 0xC33C, 0x9966, 0x0660, 0x0272, 0x04E4, 0x4E40, 0x2720,
    0xC936, 0x936C, 0x39C6, 0x639C, 0x9336, 0x9CC6, 0x817E, 0xE718, 0xCCF0, 0x0FCC, 0x7744, 0xEE22)
PARTITION3 = (
    0xaa685050, 0x6a5a5040, 0x5a5a4200, 0x5450a0a8, 0xa5a50000, 0xa0a05050, 0x5555a0a0, 0x5a5a5050,
    0xaa550000, 0xaa555500, 0xaaaa5500, 0x90909090, 0x94949494, 0xa4a4a4a4, 0xa9a59450, 0x2a0a4250,
    0xa5945040, 0x0a425054, 0xa5a5a500, 0x55a0a0a0, 0xa8a85454, 0x6a6a4040, 0xa4a45000, 0x1a1a0500,
    0x0050a4a4, 0xaaa59090, 0x14696914, 0x69691400, 0xa08585a0, 0xaa821414, 0x50a4a450, 0x6a5a0200,
    0xa9a58000, 0x5090a0a8, 0xa8a09050, 0x24242424, 0x00aa5500, 0x24924924, 0x24499224, 0x50a50a50,
    0x500aa550, 0xaaaa4444, 0x66660000, 0xa5a0a5a0, 0x50a050a0, 0x69286928, 0x44aaaa44, 0x66666600,
    0xaa444444, 0x54a854a8, 0x95809580, 0x96969600, 0xa85454a8, 0x80959580, 0xaa141414, 0x96960000,
    0xaaaa1414, 0xa05050a0, 0xa0a5a5a0, 0x96000000, 0x40804080, 0xa9a8a9a8, 0xaaaaaa44, 0x2a4a5254)
ANCHOR2 = (15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 2, 8, 2, 2, 8, 8, 15, 2, 8, 2, 2,
           8, 8, 2, 2, 15, 15, 6, 8, 2, 8, 15, 15, 2, 8, 2, 2, 2, 15, 15, 6, 6, 2, 6, 8, 15, 15, 2, 2, 15, 15,
           15, 15, 15, 2, 2, 15)
ANCHOR3 = ((3, 3, 15, 15, 8, 3, 15, 15, 8, 8, 6, 6, 6, 5, 3, 3, 3, 3, 8, 15, 3, 3, 6, 10, 5, 8, 8, 6, 8, 5, 15,
            15, 8, 15, 3, 5, 6, 10, 8, 15, 15, 3, 15, 5, 15, 15, 15, 15, 3, 15, 5, 5, 5, 8, 5, 10, 5, 10, 8, 13,
            15, 12, 3, 3),
           (15, 8, 8, 3, 15, 15, 3, 8, 15, 15, 15, 15, 15, 15, 15, 8, 15, 8, 15, 3, 15, 8, 15, 8, 3, 15, 6, 10,
            15, 15, 10, 8, 15, 3, 15, 10, 10, 8, 9, 10, 6, 15, 8, 15, 3, 6, 6, 8, 15, 3, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 3, 15, 15, 8))
WEIGHTS = {2: (0, 21, 43, 64), 3: (0, 9, 18, 27, 37, 46, 55, 64),
           4: (0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64)}


def subset_of(subsets, p, i):
    if subsets == 1:
        return 0
    if subsets == 2:
        return PARTITION2[p] >> i & 1
    return PARTITION3[p] >> (2 * i) & 3


def anchors(subsets, p):
    """The anchor texel of each subset."""
    if subsets == 1:
        return (0,)
    if subsets == 2:
        return (0, ANCHOR2[p])
    return (0, ANCHOR3[0][p], ANCHOR3[1][p])


def lerp64(a, b, w):
    return (a * (64 - w) + b * w + 32) >> 6


def nearest(palette, texel):
    """The index of the palette entry nearest @p texel, the lowest of equals."""
    best, best_d = 0, None
    for i, entry in enumerate(palette):
        d = sum((x - y) * (x - y) for x, y in zip(entry, texel))
        if best_d is None or d < best_d:
            best, best_d = i, d
    return best


def put_indices(bits, subsets, p, index, count):
    marks = anchors(subsets, p)
    for i in range(16):
        bits.put(index[i], count - (1 if i in marks else 0))


# ---- BC7, a block in a mode it is given

# subsets, partition bits, rotation bits, selection bits, colour bits, alpha bits, endpoint p-bits, shared
# p-bits, index bits, second index bits: source/bcnref.c's BC7_MODES
BC7_MODES = ((3, 4, 0, 0, 4, 0, 1, 0, 3, 0), (2, 6, 0, 0, 6, 0, 0, 1, 3, 0), (3, 6, 0, 0, 5, 0, 0, 0, 2, 0),
             (2, 6, 0, 0, 7, 0, 1, 0, 2, 0), (1, 0, 2, 1, 5, 6, 0, 0, 2, 3), (1, 0, 2, 0, 7, 8, 0, 0, 2, 2),
             (1, 0, 0, 0, 7, 7, 1, 0, 4, 0), (2, 6, 0, 0, 5, 5, 1, 0, 2, 0))


def expand(v, bits):
    return (v << (8 - bits) | v >> (2 * bits - 8)) & 0xFF


def quantize(value, bits, pbit):
    """The code of @p bits bits (below a p-bit of @p pbit when it is 0 or 1) nearest the byte @p value, and
    what it expands back to."""
    if pbit is None:
        top = (1 << bits) - 1
        code = min(top, max(0, round(value * top / 255)))
        return code, expand(code, bits)
    best = None
    for code in range(1 << bits):
        e = expand(code << 1 | pbit, bits + 1)
        if best is None or abs(e - value) < abs(best[1] - value):
            best = (code, e)
    return best


def bc7_block(texels, mode, p, rotation, selection):
    """One block of @p texels in BC7 mode @p mode, partition @p p, the rotation and index selection given."""
    subsets, pb, rb, sb, cb, ab, epb, spb, ib, ib2 = BC7_MODES[mode]
    rotated = []
    for t in texels:
        t = list(t)
        if rotation:
            t[3], t[rotation - 1] = t[rotation - 1], t[3]
        if ab == 0:
            t[3] = 255
        rotated.append(t)
    channels = 4 if ab else 3
    # A bounding box a subset, its corners the endpoints; modes 4 and 5 fit colour and alpha apart.
    ends = []
    for s in range(subsets):
        members = [rotated[i] for i in range(16) if subset_of(subsets, p, i) == s]
        ends.append(([min(t[c] for t in members) for c in range(4)], [max(t[c] for t in members) for c in range(4)]))
    # The p-bits: an endpoint's, or a subset's shared, chosen by which lands nearer.
    pbits = []
    codes = []
    expanded = []
    for s in range(subsets):
        pair_codes, pair_expanded, pair_pbits = [], [], []
        shared = None
        if spb:
            errors = []
            for candidate in (0, 1):
                err = 0
                for end in ends[s]:
                    for c in range(3):
                        err += abs(quantize(end[c], cb, candidate)[1] - end[c])
                errors.append(err)
            shared = 0 if errors[0] <= errors[1] else 1
        for end in ends[s]:
            if epb:
                errors = []
                for candidate in (0, 1):
                    err = sum(abs(quantize(end[c], cb if c < 3 else ab, candidate)[1] - end[c])
                              for c in range(channels))
                    errors.append(err)
                pbit = 0 if errors[0] <= errors[1] else 1
            else:
                pbit = shared
            code, value = [], []
            for c in range(4):
                bits = cb if c < 3 else ab
                if bits == 0:
                    code.append(None)
                    value.append(255)
                    continue
                q, e = quantize(end[c], bits, pbit)
                code.append(q)
                value.append(e)
            pair_codes.append(code)
            pair_expanded.append(value)
            pair_pbits.append(pbit)
        codes.append(pair_codes)
        expanded.append(pair_expanded)
        pbits.append(pair_pbits)
    # The index sets: one over every channel, or (modes 4 and 5) colour and alpha apart, the selection bit
    # giving colour the second set's bits.
    two_sets = ib2 != 0
    colour_bits = ib2 if (two_sets and selection) else ib
    alpha_bits = ib if (two_sets and selection) else ib2

    def choose(index_bits, chans, swap_ends):
        out = []
        for i in range(16):
            s = subset_of(subsets, p, i)
            e0, e1 = expanded[s]
            palette = [tuple(lerp64(e0[c], e1[c], w) for c in chans) for w in WEIGHTS[index_bits]]
            out.append(nearest(palette, tuple(rotated[i][c] for c in chans)))
        # An anchor's index keeps its top bit nought: swap the subset's ends and turn its indices round.
        top = 1 << (index_bits - 1)
        for s, a in enumerate(anchors(subsets if not two_sets else 1, p)):
            if out[a] & top:
                for c in chans:
                    for side in (codes, expanded):
                        side[s][0][c], side[s][1][c] = side[s][1][c], side[s][0][c]
                if swap_ends:
                    pbits[s][0], pbits[s][1] = pbits[s][1], pbits[s][0]
                for i in range(16):
                    if subset_of(subsets, p, i) == s:
                        out[i] = (1 << index_bits) - 1 - out[i]
        return out

    if two_sets:
        colour = choose(colour_bits, (0, 1, 2), False)
        alpha = choose(alpha_bits, (3,), False)
        primary, secondary = (alpha, colour) if selection else (colour, alpha)
    else:
        primary = choose(ib, tuple(range(channels)), True)
        secondary = None
    bits = Bits()
    bits.put(1 << mode, mode + 1)
    bits.put(p, pb)
    bits.put(rotation, rb)
    bits.put(selection, sb)
    for c in range(4):
        width = cb if c < 3 else ab
        if width == 0:
            continue
        for s in range(subsets):
            for k in range(2):
                bits.put(codes[s][k][c], width)
    if epb:
        for s in range(subsets):
            for k in range(2):
                bits.put(pbits[s][k], 1)
    if spb:
        for s in range(subsets):
            bits.put(pbits[s][0], 1)
    put_indices(bits, subsets, p, primary, ib)
    if secondary is not None:
        put_indices(bits, 1, 0, secondary, ib2)
    return bits.bytes()


# ---- BC6H, a block in a mode it is given

# code, code bits, regions, transformed, endpoint bits, delta bits, fields: source/bcnref.c's BC6_MODES
BC6_MODES = (
    (0, 2, 2, 1, 10, (5, 5, 5), "g2[4] b2[4] b3[4] r0[9:0] g0[9:0] b0[9:0] r1[4:0] g3[4] g2[3:0] g1[4:0] b3[0] "
     "g3[3:0] b1[4:0] b3[1] b2[3:0] r2[4:0] b3[2] r3[4:0] b3[3]"),
    (1, 2, 2, 1, 7, (6, 6, 6), "g2[5] g3[4] g3[5] r0[6:0] b3[0] b3[1] b2[4] g0[6:0] b2[5] b3[2] g2[4] b0[6:0] "
     "b3[3] b3[5] b3[4] r1[5:0] g2[3:0] g1[5:0] g3[3:0] b1[5:0] b2[3:0] r2[5:0] r3[5:0]"),
    (2, 5, 2, 1, 11, (5, 4, 4), "r0[9:0] g0[9:0] b0[9:0] r1[4:0] r0[10] g2[3:0] g1[3:0] g0[10] b3[0] g3[3:0] "
     "b1[3:0] b0[10] b3[1] b2[3:0] r2[4:0] b3[2] r3[4:0] b3[3]"),
    (6, 5, 2, 1, 11, (4, 5, 4), "r0[9:0] g0[9:0] b0[9:0] r1[3:0] r0[10] g3[4] g2[3:0] g1[4:0] g0[10] g3[3:0] "
     "b1[3:0] b0[10] b3[1] b2[3:0] r2[3:0] b3[0] b3[2] r3[3:0] g2[4] b3[3]"),
    (10, 5, 2, 1, 11, (4, 4, 5), "r0[9:0] g0[9:0] b0[9:0] r1[3:0] r0[10] b2[4] g2[3:0] g1[3:0] g0[10] b3[0] "
     "g3[3:0] b1[4:0] b0[10] b2[3:0] r2[3:0] b3[1] b3[2] r3[3:0] b3[4] b3[3]"),
    (14, 5, 2, 1, 9, (5, 5, 5), "r0[8:0] b2[4] g0[8:0] g2[4] b0[8:0] b3[4] r1[4:0] g3[4] g2[3:0] g1[4:0] b3[0] "
     "g3[3:0] b1[4:0] b3[1] b2[3:0] r2[4:0] b3[2] r3[4:0] b3[3]"),
    (18, 5, 2, 1, 8, (6, 5, 5), "r0[7:0] g3[4] b2[4] g0[7:0] b3[2] g2[4] b0[7:0] b3[3] b3[4] r1[5:0] g2[3:0] "
     "g1[4:0] b3[0] g3[3:0] b1[4:0] b3[1] b2[3:0] r2[5:0] r3[5:0]"),
    (22, 5, 2, 1, 8, (5, 6, 5), "r0[7:0] b3[0] b2[4] g0[7:0] g2[5] g2[4] b0[7:0] g3[5] b3[4] r1[4:0] g3[4] "
     "g2[3:0] g1[5:0] g3[3:0] b1[4:0] b3[1] b2[3:0] r2[4:0] b3[2] r3[4:0] b3[3]"),
    (26, 5, 2, 1, 8, (5, 5, 6), "r0[7:0] b3[1] b2[4] g0[7:0] b2[5] g2[4] b0[7:0] b3[5] b3[4] r1[4:0] g3[4] "
     "g2[3:0] g1[4:0] b3[0] g3[3:0] b1[5:0] b2[3:0] r2[4:0] b3[2] r3[4:0] b3[3]"),
    (30, 5, 2, 0, 6, (6, 6, 6), "r0[5:0] g3[4] b3[0] b3[1] b2[4] g0[5:0] g2[5] b2[5] b3[2] g2[4] b0[5:0] g3[5] "
     "b3[3] b3[5] b3[4] r1[5:0] g2[3:0] g1[5:0] g3[3:0] b1[5:0] b2[3:0] r2[5:0] r3[5:0]"),
    (3, 5, 1, 0, 10, (10, 10, 10), "r0[9:0] g0[9:0] b0[9:0] r1[9:0] g1[9:0] b1[9:0]"),
    (7, 5, 1, 1, 11, (9, 9, 9), "r0[9:0] g0[9:0] b0[9:0] r1[8:0] r0[10] g1[8:0] g0[10] b1[8:0] b0[10]"),
    (11, 5, 1, 1, 12, (8, 8, 8), "r0[9:0] g0[9:0] b0[9:0] r1[7:0] r0[10:11] g1[7:0] g0[10:11] b1[7:0] b0[10:11]"),
    (15, 5, 1, 1, 16, (4, 4, 4), "r0[9:0] g0[9:0] b0[9:0] r1[3:0] r0[10:15] g1[3:0] g0[10:15] b1[3:0] b0[10:15]"),
)
BC6_RESERVED = (19, 23, 27, 31)


def fields(text):
    """A mode's fields as (channel, endpoint, bit) in the order the block holds them."""
    out = []
    for field in text.split():
        channel = "rgb".index(field[0])
        endpoint = int(field[1])
        inside = field[3:-1]
        hi, lo = (int(x) for x in inside.split(":")) if ":" in inside else (int(inside),) * 2
        step = 1 if hi >= lo else -1
        out.extend((channel, endpoint, bit) for bit in range(lo, hi + step, step))
    return out


def half_bits(value):
    return struct.unpack("<H", struct.pack("<e", value))[0]


def half_value(bits):
    return struct.unpack("<e", struct.pack("<H", bits))[0]


def unquantize(v, bits):
    """source/bcnref.c's Bc6Unquantize, unsigned."""
    if bits >= 15 or v == 0:
        return v
    if v == (1 << bits) - 1:
        return 0xFFFF
    return ((v << 16) + 0x8000) >> bits


def bc6_block(texels, number, p):
    """One block of @p texels (bytes, read as value / 255) in BC6H mode @p number (an index into BC6_MODES),
    partition @p p, unsigned."""
    code, code_bits, regions, transformed, epb, delta, text = BC6_MODES[number]
    target = [[half_value(half_bits(t[c] / 255)) for c in range(3)] for t in texels]
    top = (1 << epb) - 1

    def code_for(value):
        # The code whose finished half is nearest the value: a finished half is the unquantized code's
        # 31/64, so the code is near the half's bits times 64/31, scaled to the code's bits.
        bits = half_bits(value)
        guess = round(bits * 64 / 31 * (1 << epb) / 65536)
        best = None
        for q in range(max(0, guess - 3), min(top, guess + 3) + 1):
            got = half_value(unquantize(q, epb) * 31 >> 6)
            if best is None or abs(got - value) < abs(best[1] - value):
                best = (q, got)
        return best[0]

    # A bounding box a region, quantized; then the endpoints after the transform, deltas cut to their bits.
    ends = []
    for r in range(regions):
        members = [target[i] for i in range(16) if subset_of(regions, p, i) == r]
        ends.append([[code_for(min(t[c] for t in members)) for c in range(3)],
                     [code_for(max(t[c] for t in members)) for c in range(3)]])

    def stored():
        """The four endpoints' stored values and the absolute codes they decode to."""
        flat = [ends[r][k] for r in range(regions) for k in range(2)]
        values, absolute = [flat[0][:]], [flat[0][:]]
        for e in flat[1:]:
            if not transformed:
                values.append(e[:])
                absolute.append(e[:])
                continue
            v, a = [], []
            for c in range(3):
                lo, hi = -(1 << (delta[c] - 1)), (1 << (delta[c] - 1)) - 1
                d = min(hi, max(lo, e[c] - flat[0][c]))
                v.append(d & ((1 << delta[c]) - 1))
                a.append((flat[0][c] + d) & top)
            values.append(v)
            absolute.append(a)
        return values, absolute

    index_bits = 3 if regions == 2 else 4
    marks = anchors(regions, p)
    for attempt in range(5):
        values, absolute = stored()
        index = []
        for i in range(16):
            r = subset_of(regions, p, i)
            e0, e1 = absolute[2 * r], absolute[2 * r + 1]
            palette = [tuple(half_value(lerp64(unquantize(e0[c], epb), unquantize(e1[c], epb), w) * 31 >> 6)
                             for c in range(3)) for w in WEIGHTS[index_bits]]
            index.append(nearest(palette, target[i]))
        # An anchor's index keeps its top bit nought: its region's ends swap, which moves a transformed
        # block's deltas, so the indices are chosen again.
        wrong = [r for r, a in enumerate(marks) if index[a] >> (index_bits - 1)]
        if not wrong or attempt == 4:
            break
        for r in wrong:
            ends[r][0], ends[r][1] = ends[r][1], ends[r][0]
    # Where the deltas' cut still leaves one set, the anchor takes the index without its top bit.
    for a in marks:
        index[a] &= (1 << (index_bits - 1)) - 1
    bits = Bits()
    bits.put(code, code_bits)
    for channel, endpoint, bit in fields(text):
        bits.put(values[endpoint][channel] >> bit & 1, 1)
    if regions == 2:
        bits.put(p, 5)
    put_indices(bits, regions, p, index, index_bits)
    return bits.bytes()


def random_block(prefix, prefix_bits, salt):
    """16 bytes whose low @p prefix_bits bits are @p prefix and the rest from a fixed hash."""
    value = 0
    for word in range(4):
        value |= lowbias32(salt * 4 + word + 0x5BC6) << (32 * word)
    value = (value & ~((1 << prefix_bits) - 1)) | prefix
    return value.to_bytes(16, "little")


# ---- the encoders, the outside decoders, the file
#
# Pillow, etcpak, imagecodecs and numpy are imported where they are used, so that the tests can load this
# script with none of them: what they check is the writer and this script's own encoders.

# name, kind, blocks, sRGB, alpha kept, the outside decoder's kind, the channels the format keeps (for the PSNR)
IMAGES = (
    ("BC1", "BCN_BC1", "BC1", False, True, "BC1", (0, 1, 2, 3)),
    ("BC1 RGB", "BCN_BC1", "BC1", False, False, "BC1", (0, 1, 2)),
    ("BC2", "BCN_BC2", "BC2", False, True, "BC2", (0, 1, 2, 3)),
    ("BC3", "BCN_BC3", "BC3", False, True, "BC3", (0, 1, 2, 3)),
    ("BC4", "BCN_BC4", "BC4", False, True, "BC4", (0,)),
    ("BC5", "BCN_BC5", "BC5", False, True, "BC5", (0, 1)),
    ("BC6H", "BCN_BC6H_UF", "BC6H", False, True, "BC6H_UF", (0, 1, 2)),
    ("BC7", "BCN_BC7", "BC7", False, True, "BC7", (0, 1, 2, 3)),
    ("BC7 in every mode", "BCN_BC7", "BC7_MODES", False, True, "BC7", (0, 1, 2, 3)),
    ("BC1 sRGB", "BCN_BC1", "BC1", True, True, "BC1", (0, 1, 2, 3)),
    ("BC2 sRGB", "BCN_BC2", "BC2", True, True, "BC2", (0, 1, 2, 3)),
    ("BC3 sRGB", "BCN_BC3", "BC3", True, True, "BC3", (0, 1, 2, 3)),
    ("BC7 sRGB", "BCN_BC7", "BC7_SRGB", True, True, "BC7", (0, 1, 2, 3)),
)
# The block arrays in the order the file holds them; the ones this script encodes itself.
ARRAYS = ("BC1", "BC2", "BC3", "BC4", "BC5", "BC6H", "BC7", "BC7_MODES", "BC7_SRGB")


def bc7_in_every_mode(texels):
    """Block n in mode n % 8, its partition, rotation and index selection in turn."""
    return b"".join(bc7_block(texels[n], n % 8, (n // 8) % (16 if n % 8 == 0 else 64),
                              (n // 8) % 4 if n % 8 in (4, 5) else 0, (n // 32) % 2 if n % 8 == 4 else 0)
                    for n in range(BLOCKS))


def bc6h_in_every_mode(texels):
    """Block n in the table's mode n % 14, its partition in turn."""
    return b"".join(bc6_block(texels[n], n % 14, (n // 14) % 32) for n in range(BLOCKS))


def mode_blocks():
    """(name, kind, block, outside kind, its signed kind) of every mode block."""
    modes = []
    for m in range(8):
        modes.append(("BC7 mode %u" % m, "BCN_BC7", random_block(1 << m, m + 1, m), "BC7", None))
    for number, mode in enumerate(BC6_MODES):
        modes.append(("BC6H mode %u" % (number + 1), "BCN_BC6H_UF", random_block(mode[0], mode[1], 8 + number),
                      "BC6H_UF", "BC6H_SF"))
    for code in BC6_RESERVED:
        modes.append(("BC6H reserved mode code %u" % code, "BCN_BC6H_UF", random_block(code, 5, 40 + code),
                      "BC6H_UF", "BC6H_SF"))
    return modes


def pillow_encode(rgba, pixel_format):
    """The blocks of the picture as Pillow writes them into a DDS file."""
    from PIL import Image
    out = io.BytesIO()
    Image.frombytes("RGBA", (SIZE, SIZE), rgba).save(out, "DDS", pixel_format=pixel_format)
    data = out.getvalue()
    if data[:4] != b"DDS ":
        raise SystemExit("Pillow wrote no DDS file")
    blocks = data[128:]
    if len(blocks) != 16 * BLOCKS:
        raise SystemExit("Pillow wrote %u bytes of %s blocks, %u wanted" % (len(blocks), pixel_format, 16 * BLOCKS))
    return blocks


def bc7_params(perceptual):
    import etcpak
    p = etcpak.BC7CompressBlockParams()
    if perceptual:
        p.init_perceptual_weights()
    else:
        p.init_linear_weights()
    return p


def encode(rgba):
    """Every block array, by its name."""
    import etcpak
    texels = [block_texels(rgba, n) for n in range(BLOCKS)]
    arrays = {
        "BC1": etcpak.compress_bc1(rgba, SIZE, SIZE),
        "BC2": pillow_encode(rgba, "DXT3"),
        "BC3": etcpak.compress_bc3(rgba, SIZE, SIZE),
        "BC4": etcpak.compress_bc4(rgba, SIZE, SIZE),
        "BC5": etcpak.compress_bc5(rgba, SIZE, SIZE),
        "BC6H": bc6h_in_every_mode(texels),
        "BC7": etcpak.compress_bc7(rgba, SIZE, SIZE, bc7_params(False)),
        "BC7_MODES": bc7_in_every_mode(texels),
        "BC7_SRGB": etcpak.compress_bc7(rgba, SIZE, SIZE, bc7_params(True)),
    }
    return {name: arrays[name] for name in ARRAYS}


# Pillow's decoder: its mode and arguments for each kind.
PILLOW = {"BC1": ("RGBA", (1,)), "BC2": ("RGBA", (2,)), "BC3": ("RGBA", (3,)), "BC4": ("L", (4,)),
          "BC5": ("RGB", (5, "BC5")), "BC7": ("RGBA", (7,))}


def outside_decode(blocks, kind, width, height):
    """What the reference suite holds the decoder to: Pillow's bytes for BC1 to BC5 and BC7, and for BC6H,
    whose halves Pillow takes to bytes by interpolating without the specification's rounding, the halves
    bcdec (in imagecodecs) decodes to, little endian, red, green and blue a texel."""
    if kind.startswith("BC6H"):
        import imagecodecs
        import numpy
        fmt = imagecodecs.BCN.FORMAT.BC6HU if kind == "BC6H_UF" else imagecodecs.BCN.FORMAT.BC6HS
        halves = imagecodecs.bcn_decode(blocks, fmt, shape=(height, width, 3))
        return halves.astype("<f2").view(numpy.uint16).astype("<u2").tobytes()
    from PIL import Image
    mode, args = PILLOW[kind]
    return Image.frombytes(mode, (width, height), blocks, "bcn", args).tobytes()


def fnv1a(data):
    h = 0x811C9DC5
    for b in data:
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return h


def shown(decoded, kind):
    """The outside decode as bytes a texel's channel: BC6H's halves clamped and rounded."""
    if not kind.startswith("BC6H"):
        return decoded
    out = []
    for (h,) in struct.iter_unpack("<H", decoded):
        out.append(min(255, max(0, round(half_value(h) * 255))))
    return bytes(out)


def psnr(source, decoded, channels):
    """PSNR of a decode against the picture, over the channels the format keeps."""
    width = len(decoded) // (SIZE * SIZE)
    total, count = 0, 0
    for i in range(SIZE * SIZE):
        for c in channels:
            d = source[4 * i + c] - decoded[width * i + c]
            total += d * d
            count += 1
    return float("inf") if total == 0 else 10 * math.log10(255 * 255 * count / total)


def c_bytes(name, data):
    lines = ["static const uint8_t %s[%u] = {" % (name, len(data))]
    for i in range(0, len(data), 16):
        lines.append("    " + ", ".join("0x%02x" % b for b in data[i:i + 16]) + ",")
    lines.append("};")
    return "\n".join(lines)


def c_file(source_hash, arrays, image_hashes, mode_hashes):
    """The C file: @p arrays by name, @p image_hashes in IMAGES' order, @p mode_hashes (unsigned, signed)
    in mode_blocks()' order."""
    parts = [
        "// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors",
        "// SPDX-License-Identifier: GPL-3.0-or-later",
        "",
        "// Written by tools/bcn-images.py; run it again rather than editing this file.",
        "",
        '#include "bcnimages.h"',
        "",
        "const uint32_t BCN_IMAGE_SOURCE_HASH = 0x%08xu;" % source_hash,
        "",
    ]
    for name in ARRAYS:
        parts.append(c_bytes("BLOCKS_%s" % name, arrays[name]))
        parts.append("")
    parts.append("const BcnImage BCN_IMAGE[BCN_IMAGES] = {")
    for (name, kind, array, srgb, alpha, _, _), h in zip(IMAGES, image_hashes):
        parts.append('    { "%s", %s, %s, %s, BLOCKS_%s, %u, 0x%08xu },' %
                     (name, kind, "true" if srgb else "false", "true" if alpha else "false", array,
                      len(arrays[array]) // (8 if kind in ("BCN_BC1", "BCN_BC4") else 16), h))
    parts.append("};")
    parts.append("")
    parts.append("const BcnModeBlock BCN_MODE_BLOCK[BCN_MODE_BLOCKS] = {")
    for (name, kind, block, _, _), (h, hs) in zip(mode_blocks(), mode_hashes):
        parts.append('    { "%s", %s, { %s }, 0x%08xu, 0x%08xu },' %
                     (name, kind, ", ".join("0x%02x" % b for b in block), h, hs))
    parts.append("};")
    return "\n".join(parts) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--out", default=os.path.join(HERE, "..", "source", "bcnimages.c"))
    args = parser.parse_args()
    import etcpak
    import imagecodecs
    import PIL
    for package, version, why in ((PIL, "12.3.0", "pillow"), (etcpak, "0.9.15", "etcpak"),
                                  (imagecodecs, "2026.11.11", "imagecodecs")):
        found = getattr(package, "__version__", None)
        if why == "etcpak":
            from importlib.metadata import version as installed
            found = installed("etcpak")
        if found != version:
            raise SystemExit("%s %s: the blocks and hashes are %s %s's (pip install %s==%s)" %
                             (why, found, why, version, why, version))
    rgba = astc_images().picture()
    arrays = encode(rgba)
    image_hashes = []
    for name, kind, array, _, _, outside, channels in IMAGES:
        decoded = outside_decode(arrays[array], outside, SIZE, SIZE)
        image_hashes.append(fnv1a(decoded))
        print("%-18s %5u bytes, the outside decode %08x, %.1f dB from the picture" %
              (name, len(arrays[array]), fnv1a(decoded), psnr(rgba, shown(decoded, outside), channels)))
    mode_hashes = []
    for name, kind, block, outside, signed in mode_blocks():
        mode_hashes.append((fnv1a(outside_decode(block, outside, 4, 4)),
                            fnv1a(outside_decode(block, signed, 4, 4)) if signed else 0))
    with open(args.out, "w") as f:
        f.write(c_file(fnv1a(rgba), arrays, image_hashes, mode_hashes))
    used = {}
    for n in range(0, len(arrays["BC7"]), 16):
        first = arrays["BC7"][n]
        mode = next(m for m in range(8) if first >> m & 1)
        used[mode] = used.get(mode, 0) + 1
    print("etcpak's BC7 blocks by mode: %s; %u pictures, %u mode blocks" %
          (", ".join("%u: %u" % kv for kv in sorted(used.items())), len(IMAGES), len(mode_blocks())))
    return 0


if __name__ == "__main__":
    sys.exit(main())
