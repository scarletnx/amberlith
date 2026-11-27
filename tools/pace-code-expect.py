#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""What the pace-code suite's sixteen kernels must leave, worked out apart from the program.

source/pacework.c runs each kernel in the processor's own instructions and source/paceref.c restates it in C; this
restates it once more, sharing no code with either, and source/tests/t_paceref.c holds paceref.c to the table printed
here (tools/test_pace_code_expect.py holds that file to this script). Each kernel is a fixed piece of work run a
number of times from fixed start values, and leaves one 64-bit word; docs/families/Pace.md says what each does.

  tools/pace-code-expect.py --c    the table, as t_paceref.c holds it
"""

import math
import struct
import sys

MASK64 = (1 << 64) - 1
MASK32 = (1 << 32) - 1
M = 6364136223846793005
C = 1442695040888963407
START = 0x0123456789ABCDEF
OTHER = 0xFEDCBA9876543210

TABLE_BYTES = 16 << 10
STREAM_BYTES = 64 << 10


def fill(nbytes):
    """The buffers every kernel reads: word i, from 0, is (i + 1) * M + C, little endian"""
    return bytearray(b"".join(struct.pack("<Q", ((i + 1) * M + C) & MASK64) for i in range(nbytes // 8)))


def rotr(v, k):
    return ((v >> k) | (v << (64 - k))) & MASK64


def int4(n):
    a, b, c, d = START, OTHER, 0x0F1E2D3C4B5A6978, 0x8796A5B4C3D2E1F0
    for _ in range(n):
        a = ((a + 0xABC) ^ (a << 7)) & MASK64
        b = ((b ^ 0xFF) + (b >> 3)) & MASK64
        c = ((c - 0x7FF) ^ (c >> 11)) & MASK64
        d = ((d ^ (d << 13)) + 0x123) & MASK64
    return ((a + b) ^ (c + d)) & MASK64


def mul(n):
    x, y = START, 0
    for _ in range(n):
        h = (x * M) >> 64
        x = (x * M + h) & MASK64
        y = (y + h) & MASK64
    return x ^ y


def signed(v):
    return v - (1 << 64) if v >> 63 else v


def div(n):
    x, y = START, OTHER
    k, k5 = 0xFFFFFFFFFFFFFFC5, 0x9E3779B97F4A7C15
    for _ in range(n):
        q = k // ((x >> 44) | 1)
        x = (x * M + q) & MASK64
        d2 = (x & 0xFFF) + 2
        num = signed(y)
        s = abs(num) // d2  # toward zero
        s = -s if num < 0 else s
        y = ((y ^ (s & MASK64)) - k5) & MASK64
    return x ^ y


def calls(n):
    def f3(v):
        return ((v ^ (v >> 7)) + 123) & MASK64

    def f2(v):
        u = f3((5 * v) & MASK64)
        return u ^ (u >> 11)

    def f1(v):
        return (3 * f2((v + 7) & MASK64)) & MASK64

    x = START
    for _ in range(n):
        x = f1(x)
    return x


def indirect(n):
    x = START
    for _ in range(n):
        x = (x * M + C) & MASK64
        k = x >> 61
        x = ((x ^ (x >> (k + 3))) + 16 * k + 1) & MASK64
    return x


SWITCH_OPS = (
    lambda x: x + 1,
    lambda x: x ^ (x >> 3),
    lambda x: x - 7,
    lambda x: rotr(x, 5),
    lambda x: x + (x << 4),
    lambda x: x ^ 0xFF,
    lambda x: ~x,
    lambda x: x + (x >> 9),
    lambda x: x - (x << 2),
    lambda x: x ^ (x << 13),
    lambda x: rotr(x, 31),
    lambda x: x + 0x55,
    lambda x: x + (x >> 1),
    lambda x: x ^ (x >> 17),
    lambda x: x - 0x123,
    lambda x: rotr(x, 47),
)


def switch(n):
    x, s = START, 0
    for _ in range(n):
        x = (x * M + C) & MASK64
        x = SWITCH_OPS[x >> 60](x) & MASK64
        s = (s + x) & MASK64
    return x ^ s


def branches(n):
    x, a, b, c = START, 0, 0, 0
    for _ in range(n):
        x ^= (x << 13) & MASK64
        x ^= x >> 7
        x ^= (x << 17) & MASK64
        if x >> 3 & 1:
            a += 1
        if not x >> 5 & 1:
            b = (b + x) & MASK64
        if x >> 11 & 1:
            c ^= x >> 8
    return (a + (b ^ c) + x) & MASK64


def table(n):
    t = fill(TABLE_BYTES)
    x, total = START, 0
    for _ in range(n):
        i = x & 0x3FFF
        j = (x >> 20) & 0x3FFE
        k = (x >> 40) & 0x3FFC
        b = t[i]
        h = t[j] | t[j + 1] << 8
        w = struct.unpack_from("<I", t, k)[0]
        t[i] = (b + 1) & 0xFF
        struct.pack_into("<H", t, j, (h ^ b) & 0xFFFF)
        struct.pack_into("<I", t, k, (w + h) & MASK32)
        total = (total + b + h + w) & MASK64
        x = (x * M + C + w) & MASK64
    return total ^ x


def bits(d):
    return struct.unpack("<Q", struct.pack("<d", d))[0]


def fdiv(n):
    x, acc = 2.0, 0
    for _ in range(n):
        s = math.sqrt(x)
        q = 1000.0 / (s + 1.0)
        acc = (acc + int(q * 1024.0)) & MASK64  # int() truncates toward zero, as fcvtzs
        x = float(acc & 0xFFFFF) + q + 1.0
    return acc ^ bits(x)


def bytes16(n):
    v = [(i * 37 + 11) & 0xFF for i in range(16)]
    t = [(i * 91 + 7) & 0xFF for i in range(16)]
    acc = 0
    for _ in range(n):
        m = [b & 0x0F for b in v]
        a = [t[i] for i in m]
        acc = (acc + sum(0xFF for i in range(16) if a[i] == m[i])) & MASK64
        v = v[3:] + a[:3]
        v = [(b + 0x3B) & 0xFF for b in v]
    lo = int.from_bytes(bytes(v[:8]), "little")
    hi = int.from_bytes(bytes(v[8:]), "little")
    return acc ^ lo ^ hi


def crc_step(crc, word, poly):
    for byte in struct.pack("<Q", word):
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (poly if crc & 1 else 0)
    return crc


def crc_table(poly):
    rows = []
    for byte in range(256):
        crc = byte
        for _ in range(8):
            crc = (crc >> 1) ^ (poly if crc & 1 else 0)
        rows.append(crc)
    return rows


def crc(n):
    data = fill(STREAM_BYTES)
    words = len(data) // 8
    t1, t2 = crc_table(0xEDB88320), crc_table(0x82F63B78)
    c1 = c2 = 0xFFFFFFFF
    for i in range(n):
        at = (i % words) * 8
        for byte in data[at:at + 8]:
            c1 = (c1 >> 8) ^ t1[(c1 ^ byte) & 0xFF]
            c2 = (c2 >> 8) ^ t2[(c2 ^ byte) & 0xFF]
    return c1 << 32 | c2


def sbox():
    """AES's S-box from its definition: the inverse in GF(2^8) mod x^8 + x^4 + x^3 + x + 1, then the affine map"""
    def gmul(a, b):
        p = 0
        while b:
            if b & 1:
                p ^= a
            a = (a << 1) ^ (0x11B if a & 0x80 else 0)
            b >>= 1
        return p

    box = []
    for x in range(256):
        inv = next((y for y in range(1, 256) if gmul(x, y) == 1), 0)
        s = inv
        for r in (1, 2, 3, 4):
            s ^= ((inv << r) | (inv >> (8 - r))) & 0xFF
        box.append(s ^ 0x63)
    return box


SBOX = sbox()


def xtime(a):
    return ((a << 1) ^ (0x1B if a & 0x80 else 0)) & 0xFF


def aes_round(v, key):
    """aese then aesmc: AddRoundKey, ShiftRows and SubBytes, then MixColumns; byte i of the state is row i % 4,
    column i // 4, as FIPS-197 lays out the bytes in memory order"""
    s = [v[i] ^ key[i] for i in range(16)]
    s = [SBOX[s[(i + 4 * (i % 4)) % 16]] for i in range(16)]
    out = []
    for col in range(4):
        a = s[4 * col:4 * col + 4]
        t = a[0] ^ a[1] ^ a[2] ^ a[3]
        out += [a[r] ^ t ^ xtime(a[r] ^ a[(r + 1) % 4]) for r in range(4)]
    return out


def aes(n):
    data = fill(STREAM_BYTES)
    blocks = len(data) // 16
    v = list(data[16:32])
    for i in range(n):
        at = (i % blocks) * 16
        v = aes_round(v, data[at:at + 16])
    return int.from_bytes(bytes(v[:8]), "little") ^ int.from_bytes(bytes(v[8:]), "little")


SHA_K = (
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
)
SHA_IV = (0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19)


def ror32(v, k):
    return ((v >> k) | (v << (32 - k))) & MASK32


def compress(h, block):
    w = list(struct.unpack(">16I", block))
    for t in range(16, 64):
        s0 = ror32(w[t - 15], 7) ^ ror32(w[t - 15], 18) ^ (w[t - 15] >> 3)
        s1 = ror32(w[t - 2], 17) ^ ror32(w[t - 2], 19) ^ (w[t - 2] >> 10)
        w.append((w[t - 16] + s0 + w[t - 7] + s1) & MASK32)
    a, b, c, d, e, f, g, hh = h
    for t in range(64):
        t1 = (hh + (ror32(e, 6) ^ ror32(e, 11) ^ ror32(e, 25)) + ((e & f) ^ (~e & g)) + SHA_K[t] + w[t]) & MASK32
        t2 = ((ror32(a, 2) ^ ror32(a, 13) ^ ror32(a, 22)) + ((a & b) ^ (a & c) ^ (b & c))) & MASK32
        a, b, c, d, e, f, g, hh = (t1 + t2) & MASK32, a, b, c, (d + t1) & MASK32, e, f, g
    return tuple((x + y) & MASK32 for x, y in zip(h, (a, b, c, d, e, f, g, hh)))


def sha(n):
    data = fill(STREAM_BYTES)
    blocks = len(data) // 64
    h = SHA_IV
    for i in range(n):
        at = (i % blocks) * 64
        h = compress(h, bytes(data[at:at + 64]))
    return (h[0] << 32 | h[1]) ^ (h[2] << 32 | h[3]) ^ (h[4] << 32 | h[5]) ^ (h[6] << 32 | h[7])


def exclusive(n):
    w, step = 0, 1
    for _ in range(n):
        w = (w + step) & MASK64
        step = (step * 3 + 1) & MASK64
    return w ^ step


# The core kernel's state machine: a character's class, and the next state by state and class.
CORE_ALPHABET = b"0123456789+-.eE ,"
CORE_CLASS = {**{ord(d): 0 for d in "0123456789"}, ord("+"): 1, ord("-"): 1, ord("."): 2, ord("e"): 3, ord("E"): 3,
              ord(" "): 4, ord(","): 4}
CORE_NEXT = ((1, 1, 2, 5, 0), (1, 5, 2, 3, 0), (2, 5, 5, 3, 0), (4, 4, 5, 5, 0), (4, 5, 5, 5, 0), (5, 5, 5, 5, 0))


def crc16(crc, value, nbytes):
    for byte in value.to_bytes(nbytes, "little"):
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xA001 if crc & 1 else 0)
    return crc


def core(n):
    s, acc = START, 0
    for i in range(n):
        # a list of 32 nodes, reversed, then walked: each value times its place, and the first node whose
        # low four bits are the iteration's
        values = [((s >> (j & 31)) ^ (j * 0x9E3779B1)) & 0xFFFF for j in range(32)]
        order = list(range(32))[::-1]
        walked = sum(values[node] * (place + 1) for place, node in enumerate(order)) & MASK32
        found = next((place for place, node in enumerate(order) if values[node] & 15 == i & 15), 32)
        # an 8 by 8 product of 16-bit integers into 32 bits, each cell weighed by its row xor its column
        a = [[((s >> ((r * 8 + c) & 63)) & 0xFF) - 128 for c in range(8)] for r in range(8)]
        b = [[r * 8 + c - 32 for c in range(8)] for r in range(8)]
        matrix = 0
        for r in range(8):
            for c in range(8):
                cell = sum(a[r][k] * b[k][c] for k in range(8))
                matrix = (matrix + cell * (r ^ c)) & MASK32
        # the state machine over 64 characters, counting the changes and the states a separator ends
        state, changes, ended = 0, 0, [0] * 6
        for k in range(64):
            ch = CORE_ALPHABET[(((s >> (k % 60)) + k * 7) & MASK64) % 17]
            cls = CORE_CLASS[ch]
            if cls == 4:
                ended[state] += 1
                after = 0
            else:
                after = CORE_NEXT[state][cls]
            changes += after != state
            state = after
        folded = 0
        for count in ended:
            folded = folded * 67 + count
        c16 = 0
        for value, nbytes in ((walked, 4), (found, 1), (matrix, 4), (changes, 1), (state, 1), (folded, 8)):
            c16 = crc16(c16, value & ((1 << (8 * nbytes)) - 1), nbytes)
        s = (s * M + C + c16) & MASK64
        acc ^= c16 << ((i % 4) * 16)
    return acc ^ s


def vector(n):
    import array

    def f32(x):
        return array.array("f", [x])[0]

    b = (1.0, 2.0, 3.0, 4.0)
    k0, k1, k2, k3 = (3.0, 5.0, 7.0, 9.0), (11.0, 13.0, 15.0, 17.0), (3.0,) * 4, (3.0,) * 4
    chains = [[1.0, 2.0, 3.0, 4.0], [5.0, 6.0, 7.0, 8.0], [9.0, 10.0, 11.0, 12.0], [13.0, 14.0, 15.0, 16.0]]
    ts = [[0] * 4 for _ in range(4)]
    for _ in range(n):
        for lane in range(4):
            chains[0][lane] = f32(chains[0][lane] + b[lane] * k0[lane])
            chains[1][lane] = f32(chains[1][lane] + b[lane] * k1[lane])
            chains[2][lane] = f32(chains[2][lane] * k2[lane] + b[lane])
            chains[3][lane] = f32((chains[3][lane] + b[lane]) * k3[lane])
        for c in range(4):
            for lane in range(4):
                # every value is a whole number under 2^24, which a single keeps exactly
                assert chains[c][lane] < 1 << 24 and chains[c][lane] == int(chains[c][lane])
                ts[c][lane] = int(chains[c][lane]) & 0xFFFF
                chains[c][lane] = float(ts[c][lane])
    acc = 0
    for c in range(4):
        for lane in range(4):
            acc = (acc * 31 + ts[c][lane]) & MASK64
    return acc


KERNELS = (
    ("INT4", int4), ("MUL", mul), ("DIV", div), ("CALLS", calls), ("INDIRECT", indirect), ("SWITCH", switch),
    ("BRANCHES", branches), ("TABLE", table), ("FDIV", fdiv), ("BYTES", bytes16), ("CRC", crc), ("AES", aes),
    ("SHA", sha), ("EXCLUSIVE", exclusive), ("CORE", core), ("VECTOR", vector),
)
# The counts the table holds: one, two, a thousand, and one past the last block or word the buffer kernels go
# round, so a wrap is in it; the core kernel's iterations are the costliest here, a hundred is its last.
COUNTS = {"CRC": (1, 2, 1000, 8195), "AES": (1, 2, 1000, 4099), "SHA": (1, 2, 1027), "CORE": (1, 2, 100)}


def print_c():
    print("static const struct {")
    print("    PaceCodeKind kind;")
    print("    uint64_t iterations;")
    print("    uint64_t answer;")
    print("} CODE[] = {")
    for name, kernel in KERNELS:
        for n in COUNTS.get(name, (1, 2, 1000, 9000)):
            print("    { PACE_CODE_%s, %dull, 0x%016xull }," % (name, n, kernel(n)))
    print("};")


def main(argv):
    if argv[1:] == ["--c"]:
        print_c()
        return 0
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
