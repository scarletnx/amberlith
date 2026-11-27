// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pacecoderef.h"

#include <math.h>
#include <string.h>

#include "paceref.h"

// Each kernel as docs/families/Pace.md#pace-code writes it, in integers wherever it can be.

#define START 0x0123456789abcdefull
#define OTHER 0xfedcba9876543210ull

void PaceCodeFill(uint8_t *to, size_t bytes) {
    for (size_t i = 0; i < bytes / 8u; ++i) {
        const uint64_t word = (uint64_t)(i + 1u) * PACE_INTEGER_A + PACE_INTEGER_C;
        for (unsigned b = 0; b < 8u; ++b) {
            to[i * 8u + b] = (uint8_t)(word >> (8u * b));
        }
    }
}

static uint64_t Rotr(uint64_t v, unsigned k) {
    return (v >> k) | (v << (64u - k));
}

static uint64_t Load(const uint8_t *at, unsigned bytes) {
    uint64_t v = 0;
    for (unsigned b = 0; b < bytes; ++b) {
        v |= (uint64_t)at[b] << (8u * b);
    }
    return v;
}

static void Store(uint8_t *at, uint64_t v, unsigned bytes) {
    for (unsigned b = 0; b < bytes; ++b) {
        at[b] = (uint8_t)(v >> (8u * b));
    }
}

/// The high 64 bits of a * b, from 32-bit halves: umulh.
static uint64_t MulHigh(uint64_t a, uint64_t b) {
    const uint64_t al = a & 0xffffffffu, ah = a >> 32, bl = b & 0xffffffffu, bh = b >> 32;
    const uint64_t ll = al * bl, lh = al * bh, hl = ah * bl, hh = ah * bh;
    const uint64_t middle = (ll >> 32) + (lh & 0xffffffffu) + (hl & 0xffffffffu);
    return hh + (lh >> 32) + (hl >> 32) + (middle >> 32);
}

static uint64_t Int4(uint64_t n) {
    uint64_t a = START, b = OTHER, c = 0x0f1e2d3c4b5a6978ull, d = 0x8796a5b4c3d2e1f0ull;
    while (n-- > 0u) {
        a = (a + 0xabcu) ^ (a << 7);
        b = (b ^ 0xffu) + (b >> 3);
        c = (c - 0x7ffu) ^ (c >> 11);
        d = (d ^ (d << 13)) + 0x123u;
    }
    return (a + b) ^ (c + d);
}

static uint64_t Mul(uint64_t n) {
    uint64_t x = START, y = 0;
    while (n-- > 0u) {
        const uint64_t h = MulHigh(x, PACE_INTEGER_A);
        x = x * PACE_INTEGER_A + h;
        y += h;
    }
    return x ^ y;
}

static uint64_t Div(uint64_t n) {
    uint64_t x = START, y = OTHER;
    while (n-- > 0u) {
        const uint64_t q = 0xffffffffffffffc5ull / ((x >> 44) | 1u);
        x = x * PACE_INTEGER_A + q;
        const uint64_t d2 = (x & 0xfffu) + 2u;
        // sdiv: the quotient toward zero, worked on the magnitude so no signed value overflows
        const int negative = (y >> 63) != 0u;
        const uint64_t magnitude = negative ? 0u - y : y;
        const uint64_t quotient = magnitude / d2;
        const uint64_t s = negative ? 0u - quotient : quotient;
        y = (y ^ s) - 0x9e3779b97f4a7c15ull;
    }
    return x ^ y;
}

static uint64_t Calls(uint64_t n) {
    uint64_t x = START;
    while (n-- > 0u) {
        uint64_t v = (x + 7u) * 5u;
        v = (v ^ (v >> 7)) + 123u;
        v ^= v >> 11;
        x = v * 3u;
    }
    return x;
}

static uint64_t Indirect(uint64_t n) {
    uint64_t x = START;
    while (n-- > 0u) {
        x = x * PACE_INTEGER_A + PACE_INTEGER_C;
        const unsigned k = (unsigned)(x >> 61);
        x = (x ^ (x >> (k + 3u))) + 16u * k + 1u;
    }
    return x;
}

static uint64_t Switch(uint64_t n) {
    uint64_t x = START, s = 0;
    while (n-- > 0u) {
        x = x * PACE_INTEGER_A + PACE_INTEGER_C;
        switch (x >> 60) {
        case 0: x += 1u; break;
        case 1: x ^= x >> 3; break;
        case 2: x -= 7u; break;
        case 3: x = Rotr(x, 5); break;
        case 4: x += x << 4; break;
        case 5: x ^= 0xffu; break;
        case 6: x = ~x; break;
        case 7: x += x >> 9; break;
        case 8: x -= x << 2; break;
        case 9: x ^= x << 13; break;
        case 10: x = Rotr(x, 31); break;
        case 11: x += 0x55u; break;
        case 12: x += x >> 1; break;
        case 13: x ^= x >> 17; break;
        case 14: x -= 0x123u; break;
        default: x = Rotr(x, 47); break;
        }
        s += x;
    }
    return x ^ s;
}

static uint64_t Branches(uint64_t n) {
    uint64_t x = START, a = 0, b = 0, c = 0;
    while (n-- > 0u) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        if ((x >> 3) & 1u) {
            a += 1u;
        }
        if (!((x >> 5) & 1u)) {
            b += x;
        }
        if ((x >> 11) & 1u) {
            c ^= x >> 8;
        }
    }
    return a + (b ^ c) + x;
}

static uint64_t Table(uint64_t n, uint8_t *t) {
    uint64_t x = START, sum = 0;
    while (n-- > 0u) {
        const uint64_t i = x & 0x3fffu, j = (x >> 20) & 0x3ffeu, k = (x >> 40) & 0x3ffcu;
        const uint64_t b = t[i];
        const uint64_t h = Load(t + j, 2);
        const uint64_t w = Load(t + k, 4);
        t[i] = (uint8_t)(b + 1u);
        Store(t + j, h ^ b, 2);
        Store(t + k, w + h, 4);
        sum += b + h + w;
        x = x * PACE_INTEGER_A + PACE_INTEGER_C + w;
    }
    return sum ^ x;
}

static uint64_t Fdiv(uint64_t n) {
    double x = 2.0;
    uint64_t acc = 0;
    while (n-- > 0u) {
        const double q = 1000.0 / (sqrt(x) + 1.0);
        acc += (uint64_t)(int64_t)(q * 1024.0);
        x = (double)(acc & 0xfffffu) + q + 1.0;
    }
    uint64_t pattern;
    memcpy(&pattern, &x, sizeof(pattern));
    return acc ^ pattern;
}

static uint64_t Bytes(uint64_t n) {
    uint8_t v[16], t[16];
    for (unsigned i = 0; i < 16u; ++i) {
        v[i] = (uint8_t)(i * 37u + 11u);
        t[i] = (uint8_t)(i * 91u + 7u);
    }
    uint64_t acc = 0;
    while (n-- > 0u) {
        uint8_t a[16];
        for (unsigned i = 0; i < 16u; ++i) {
            const uint8_t m = v[i] & 0x0fu;
            a[i] = t[m];
            acc += a[i] == m ? 0xffu : 0u;
        }
        uint8_t next[16];
        for (unsigned i = 0; i < 16u; ++i) {
            next[i] = (uint8_t)((i < 13u ? v[i + 3u] : a[i - 13u]) + 0x3bu);
        }
        memcpy(v, next, sizeof(v));
    }
    return acc ^ Load(v, 8) ^ Load(v + 8, 8);
}

static uint32_t Crc(uint32_t crc, uint64_t word, uint32_t poly) {
    for (unsigned bit = 0; bit < 64u; ++bit) {
        const uint32_t in = (uint32_t)((word >> bit) & 1u);
        crc = (crc >> 1) ^ (((crc ^ in) & 1u) != 0u ? poly : 0u);
    }
    return crc;
}

static uint64_t Crcs(uint64_t n, const uint8_t *stream) {
    uint32_t c1 = 0xffffffffu, c2 = 0xffffffffu;
    for (uint64_t i = 0; i < n; ++i) {
        const uint64_t word = Load(stream + (i % (PACE_CODE_STREAM_BYTES / 8u)) * 8u, 8);
        c1 = Crc(c1, word, 0xedb88320u);
        c2 = Crc(c2, word, 0x82f63b78u);
    }
    return (uint64_t)c1 << 32 | c2;
}

/// Multiplication in GF(2^8) modulo x^8 + x^4 + x^3 + x + 1.
static uint8_t GfMul(uint8_t a, uint8_t b) {
    uint8_t p = 0;
    while (b != 0u) {
        if (b & 1u) {
            p ^= a;
        }
        a = (uint8_t)((a << 1) ^ ((a & 0x80u) != 0u ? 0x1bu : 0u));
        b >>= 1;
    }
    return p;
}

/// The S-box worked out: the inverse in GF(2^8), then the affine map.
static uint8_t Sub(uint8_t x) {
    uint8_t inverse = 0;
    for (unsigned y = 1; y < 256u && x != 0u; ++y) {
        if (GfMul(x, (uint8_t)y) == 1u) {
            inverse = (uint8_t)y;
            break;
        }
    }
    uint8_t s = inverse;
    for (unsigned r = 1; r <= 4u; ++r) {
        s ^= (uint8_t)((inverse << r) | (inverse >> (8u - r)));
    }
    return (uint8_t)(s ^ 0x63u);
}

static uint64_t Aes(uint64_t n, const uint8_t *stream) {
    uint8_t box[256];
    for (unsigned x = 0; x < 256u; ++x) {
        box[x] = Sub((uint8_t)x);
    }
    uint8_t v[16];
    memcpy(v, stream + 16, 16);
    for (uint64_t i = 0; i < n; ++i) {
        const uint8_t *key = stream + (i % (PACE_CODE_STREAM_BYTES / 16u)) * 16u;
        uint8_t s[16];
        // AddRoundKey and SubBytes, then ShiftRows: byte i is row i % 4, column i / 4
        for (unsigned b = 0; b < 16u; ++b) {
            const unsigned from = (b + 4u * (b % 4u)) % 16u;
            s[b] = box[v[from] ^ key[from]];
        }
        for (unsigned col = 0; col < 4u; ++col) {
            const uint8_t *a = s + 4u * col;
            for (unsigned r = 0; r < 4u; ++r) {
                // MixColumns' row r: 2 a[r] + 3 a[r + 1] + a[r + 2] + a[r + 3]
                v[4u * col + r] = (uint8_t)(GfMul(2u, a[r]) ^ GfMul(3u, a[(r + 1u) % 4u]) ^
                                            a[(r + 2u) % 4u] ^ a[(r + 3u) % 4u]);
            }
        }
    }
    return Load(v, 8) ^ Load(v + 8, 8);
}

static const uint32_t SHA_K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u,
    0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu,
    0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu,
    0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u,
    0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u,
    0xc67178f2u,
};

static uint32_t Ror32(uint32_t v, unsigned k) {
    return (v >> k) | (v << (32u - k));
}

static uint64_t Sha(uint64_t n, const uint8_t *stream) {
    uint32_t h[8] = { 0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                      0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u };
    for (uint64_t i = 0; i < n; ++i) {
        const uint8_t *block = stream + (i % (PACE_CODE_STREAM_BYTES / 64u)) * 64u;
        uint32_t w[64];
        for (unsigned t = 0; t < 16u; ++t) {
            w[t] = (uint32_t)block[4u * t] << 24 | (uint32_t)block[4u * t + 1u] << 16 |
                   (uint32_t)block[4u * t + 2u] << 8 | block[4u * t + 3u];
        }
        for (unsigned t = 16; t < 64u; ++t) {
            const uint32_t s0 = Ror32(w[t - 15u], 7) ^ Ror32(w[t - 15u], 18) ^ (w[t - 15u] >> 3);
            const uint32_t s1 = Ror32(w[t - 2u], 17) ^ Ror32(w[t - 2u], 19) ^ (w[t - 2u] >> 10);
            w[t] = w[t - 16u] + s0 + w[t - 7u] + s1;
        }
        uint32_t s[8];
        memcpy(s, h, sizeof(s));
        for (unsigned t = 0; t < 64u; ++t) {
            const uint32_t big1 = Ror32(s[4], 6) ^ Ror32(s[4], 11) ^ Ror32(s[4], 25);
            const uint32_t choose = (s[4] & s[5]) ^ (~s[4] & s[6]);
            const uint32_t t1 = s[7] + big1 + choose + SHA_K[t] + w[t];
            const uint32_t big0 = Ror32(s[0], 2) ^ Ror32(s[0], 13) ^ Ror32(s[0], 22);
            const uint32_t major = (s[0] & s[1]) ^ (s[0] & s[2]) ^ (s[1] & s[2]);
            memmove(s + 1, s, 7u * sizeof(s[0]));
            s[4] += t1;
            s[0] = t1 + big0 + major;
        }
        for (unsigned k = 0; k < 8u; ++k) {
            h[k] += s[k];
        }
    }
    uint64_t answer = 0;
    for (unsigned k = 0; k < 8u; k += 2u) {
        answer ^= (uint64_t)h[k] << 32 | h[k + 1u];
    }
    return answer;
}

static uint64_t Exclusive(uint64_t n) {
    uint64_t w = 0, step = 1;
    while (n-- > 0u) {
        w += step;
        step = step * 3u + 1u;
    }
    return w ^ step;
}

static uint16_t Crc16(uint16_t crc, uint64_t value, unsigned bytes) {
    for (unsigned b = 0; b < bytes; ++b) {
        crc ^= (uint8_t)(value >> (8u * b));
        for (unsigned bit = 0; bit < 8u; ++bit) {
            crc = (uint16_t)((crc >> 1) ^ ((crc & 1u) != 0u ? 0xa001u : 0u));
        }
    }
    return crc;
}

typedef struct Node {
    struct Node *next;
    uint32_t value;
} Node;

uint64_t PaceCodeCore(uint64_t iterations) {
    // The state machine's characters, a character's class, and the next state by state and class:
    // a number's start, an integer, a fraction, an exponent's mark, its digits, and not a number.
    static const char ALPHABET[] = "0123456789+-.eE ,";
    static const uint8_t NEXT[6][5] = { { 1, 1, 2, 5, 0 }, { 1, 5, 2, 3, 0 }, { 2, 5, 5, 3, 0 },
                                        { 4, 4, 5, 5, 0 }, { 4, 5, 5, 5, 0 }, { 5, 5, 5, 5, 0 } };
    uint64_t s = START, acc = 0;
    for (uint64_t i = 0; i < iterations; ++i) {
        Node nodes[32];
        for (unsigned j = 0; j < 32u; ++j) {
            nodes[j].value = (uint32_t)(((s >> (j & 31u)) ^ ((uint64_t)j * 0x9e3779b1u)) & 0xffffu);
            nodes[j].next = j + 1u < 32u ? &nodes[j + 1u] : NULL;
        }
        Node *head = NULL;
        for (Node *at = &nodes[0]; at != NULL;) {
            Node *const next = at->next;
            at->next = head;
            head = at;
            at = next;
        }
        uint32_t walked = 0, found = 32, place = 0;
        for (const Node *at = head; at != NULL; at = at->next, ++place) {
            walked += at->value * (place + 1u);
            if (found == 32u && (at->value & 15u) == (i & 15u)) {
                found = place;
            }
        }
        int16_t a[8][8], b[8][8];
        for (unsigned r = 0; r < 8u; ++r) {
            for (unsigned c = 0; c < 8u; ++c) {
                a[r][c] = (int16_t)((int)((s >> ((r * 8u + c) & 63u)) & 0xffu) - 128);
                b[r][c] = (int16_t)((int)(r * 8u + c) - 32);
            }
        }
        uint32_t matrix = 0;
        for (unsigned r = 0; r < 8u; ++r) {
            for (unsigned c = 0; c < 8u; ++c) {
                int32_t cell = 0;
                for (unsigned k = 0; k < 8u; ++k) {
                    cell += (int32_t)a[r][k] * b[k][c];
                }
                matrix += (uint32_t)cell * (r ^ c);
            }
        }
        unsigned state = 0, changes = 0, ended[6] = { 0 };
        for (unsigned k = 0; k < 64u; ++k) {
            const char ch = ALPHABET[((s >> (k % 60u)) + k * 7u) % 17u];
            const unsigned cls = ch >= '0' && ch <= '9'       ? 0u
                                 : ch == '+' || ch == '-'     ? 1u
                                 : ch == '.'                  ? 2u
                                 : ch == 'e' || ch == 'E'     ? 3u
                                                              : 4u;
            unsigned after;
            if (cls == 4u) {
                ended[state] += 1u;
                after = 0;
            } else {
                after = NEXT[state][cls];
            }
            changes += after != state ? 1u : 0u;
            state = after;
        }
        uint64_t folded = 0;
        for (unsigned k = 0; k < 6u; ++k) {
            folded = folded * 67u + ended[k];
        }
        uint16_t c16 = 0;
        c16 = Crc16(c16, walked, 4);
        c16 = Crc16(c16, found, 1);
        c16 = Crc16(c16, matrix, 4);
        c16 = Crc16(c16, changes, 1);
        c16 = Crc16(c16, state, 1);
        c16 = Crc16(c16, folded, 8);
        s = s * PACE_INTEGER_A + PACE_INTEGER_C + c16;
        acc ^= (uint64_t)c16 << ((i % 4u) * 16u);
    }
    return acc ^ s;
}

static uint64_t Vector(uint64_t n) {
    static const uint32_t B[4] = { 1, 2, 3, 4 }, K0[4] = { 3, 5, 7, 9 }, K1[4] = { 11, 13, 15, 17 };
    // Whole numbers under 2^24 at every step, which a single keeps exactly: kept in integers here.
    uint32_t chains[4][4] = {
        { 1, 2, 3, 4 }, { 5, 6, 7, 8 }, { 9, 10, 11, 12 }, { 13, 14, 15, 16 }
    };
    while (n-- > 0u) {
        for (unsigned lane = 0; lane < 4u; ++lane) {
            chains[0][lane] = (chains[0][lane] + B[lane] * K0[lane]) & 0xffffu;
            chains[1][lane] = (chains[1][lane] + B[lane] * K1[lane]) & 0xffffu;
            chains[2][lane] = (chains[2][lane] * 3u + B[lane]) & 0xffffu;
            chains[3][lane] = ((chains[3][lane] + B[lane]) * 3u) & 0xffffu;
        }
    }
    uint64_t acc = 0;
    for (unsigned c = 0; c < 4u; ++c) {
        for (unsigned lane = 0; lane < 4u; ++lane) {
            acc = acc * 31u + chains[c][lane];
        }
    }
    return acc;
}

uint64_t PaceCodeAnswer(PaceCodeKind kind, uint64_t iterations, uint8_t *table,
                        const uint8_t *stream) {
    switch (kind) {
    case PACE_CODE_INT4: return Int4(iterations);
    case PACE_CODE_MUL: return Mul(iterations);
    case PACE_CODE_DIV: return Div(iterations);
    case PACE_CODE_CALLS: return Calls(iterations);
    case PACE_CODE_INDIRECT: return Indirect(iterations);
    case PACE_CODE_SWITCH: return Switch(iterations);
    case PACE_CODE_BRANCHES: return Branches(iterations);
    case PACE_CODE_TABLE: return Table(iterations, table);
    case PACE_CODE_FDIV: return Fdiv(iterations);
    case PACE_CODE_BYTES: return Bytes(iterations);
    case PACE_CODE_CRC: return Crcs(iterations, stream);
    case PACE_CODE_AES: return Aes(iterations, stream);
    case PACE_CODE_SHA: return Sha(iterations, stream);
    case PACE_CODE_EXCLUSIVE: return Exclusive(iterations);
    case PACE_CODE_CORE: return PaceCodeCore(iterations);
    case PACE_CODE_VECTOR: return Vector(iterations);
    case PACE_CODE_KINDS: break;
    }
    return 0;
}
