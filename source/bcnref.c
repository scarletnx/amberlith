// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "bcnref.h"

#include <math.h>
#include <string.h>

#include "coherref.h"

// deko3d's pickTileSize: the largest tile of 1 to 16 gobs not taller than the rows asked for.
static uint32_t PickTileShift(uint32_t gobs) {
    return gobs >= 16u ? 4u : gobs >= 8u ? 3u : gobs >= 4u ? 2u : gobs >= 2u ? 1u : 0u;
}

// deko3d's adjustTileSize for heights: shrink the tile while half of it still holds the level.
static uint32_t AdjustTileShift(uint32_t shift, uint32_t rows) {
    while (shift != 0u && (8u << (shift - 1u)) >= rows) {
        --shift;
    }
    return shift;
}

static uint32_t Blocks(uint32_t texels, uint32_t level) {
    const uint32_t size = texels >> level;
    return ((size != 0u ? size : 1u) + 3u) / 4u;
}

uint32_t BcnLevelBlocksX(const BcnArray *array, uint32_t level) {
    return Blocks(array->width, level);
}

uint32_t BcnLevelBlocksY(const BcnArray *array, uint32_t level) {
    return Blocks(array->height, level);
}

static uint32_t LevelSize(const BcnArray *array, uint32_t level) {
    const uint32_t shift = AdjustTileShift(array->tile_shift, BcnLevelBlocksY(array, level));
    const uint32_t width_gobs = (BcnLevelBlocksX(array, level) * array->bytes_per_block + 63u) / 64u;
    const uint32_t height_gobs = (BcnLevelBlocksY(array, level) + 7u) / 8u;
    const uint32_t height_tiles = (height_gobs + (1u << shift) - 1u) >> shift;
    return (width_gobs * height_tiles) << (9u + shift);
}

uint32_t BcnLevelOffset(const BcnArray *array, uint32_t level) {
    uint32_t offset = 0;
    for (uint32_t i = 0; i < level; ++i) {
        offset += LevelSize(array, i);
    }
    return offset;
}

void BcnArrayLayout(BcnArray *array, uint32_t width, uint32_t height, uint32_t layers,
                    uint32_t levels, uint32_t bytes_per_block) {
    memset(array, 0, sizeof(*array));
    array->width = width;
    array->height = height;
    array->layers = layers;
    array->levels = levels;
    array->bytes_per_block = bytes_per_block;
    // The block rows times one and a half, in gobs of eight rows.
    const uint32_t rows = (height + 3u) / 4u;
    array->tile_shift = PickTileShift((rows + rows / 2u + 7u) / 8u);
    const uint32_t size = BcnLevelOffset(array, levels);
    // alignLayerSize: the tile of level 0's rows, shrunk the same way.
    const uint32_t gobs_shift = AdjustTileShift(array->tile_shift, rows) + 9u;
    const uint32_t unit = 1u << gobs_shift;
    array->layer_size = (size + unit - 1u) / unit * unit;
}

uint32_t BcnBlockOffset(const BcnArray *array, uint32_t layer, uint32_t level, uint32_t bx,
                        uint32_t by) {
    const uint32_t shift = AdjustTileShift(array->tile_shift, BcnLevelBlocksY(array, level));
    return layer * array->layer_size + BcnLevelOffset(array, level) +
           CohBlockLinearOffset(bx * array->bytes_per_block, by,
                                BcnLevelBlocksX(array, level) * array->bytes_per_block,
                                1u << shift);
}

void BcnSolidBc4(uint8_t block[8], uint8_t value) {
    memset(block, 0, 8);
    block[0] = value;
    block[1] = value;
}

// ---- the decoder: the Khronos Data Format Specification's S3TC, RGTC and BPTC chapters, after
// ScarletNX's tests/support/bcn.c, whose NVIDIA interpolants were measured on an NVIDIA card

/// A 128 bit block as two words, and the next bit a read takes.
typedef struct {
    uint64_t word[2];
    uint32_t at;
} BcnBits;

static uint64_t Le64(const uint8_t *p) {
    uint64_t v = 0;
    for (int i = 7; i >= 0; --i) {
        v = v << 8 | p[i];
    }
    return v;
}

/// @p count bits (up to 32) from bit @p at on.
static uint32_t Peek(const BcnBits *b, uint32_t at, uint32_t count) {
    uint64_t v;
    if (at >= 64u) {
        v = b->word[1] >> (at - 64u);
    } else if (at == 0u) {
        v = b->word[0];
    } else {
        v = b->word[0] >> at | b->word[1] << (64u - at);
    }
    return (uint32_t)(v & ((1ull << count) - 1u));
}

static uint32_t Take(BcnBits *b, uint32_t count) {
    const uint32_t v = Peek(b, b->at, count);
    b->at += count;
    return v;
}

/// An endpoint of @p bits bits (5 to 8) to eight, its top bits repeated below it.
static int32_t Expand(uint32_t v, uint32_t bits) {
    return (int32_t)(v << (8u - bits) | v >> (2u * bits - 8u));
}

static int32_t Lerp64(int32_t a, int32_t b, int32_t w) {
    return (a * (64 - w) + b * w + 32) >> 6;
}

/// @p a and the difference to @p b times @p w of 256, cut toward minus infinity.
static int32_t Lerp256Down(int32_t a, int32_t b, int32_t w) {
    const int32_t scaled = (b - a) * w;
    return a + (scaled >= 0 ? scaled / 256 : -((-scaled + 255) / 256));
}

/// The weights BPTC's indices take, in 64ths, and the fifths BCN_RULE_SIXTYFOURTHS gives BC3's
/// and BC4's six value blocks.
static const uint8_t WEIGHTS2[4] = { 0, 21, 43, 64 };
static const uint8_t WEIGHTS3[8] = { 0, 9, 18, 27, 37, 46, 55, 64 };
static const uint8_t WEIGHTS4[16] = { 0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64 };
static const uint8_t FIFTHS64[6] = { 0, 13, 26, 38, 51, 64 };

static const uint8_t *Weights(uint32_t bits) {
    return bits == 2u ? WEIGHTS2 : bits == 3u ? WEIGHTS3 : WEIGHTS4;
}

/// A BC1 colour block into @p out (RGBA bytes). @p three: a block whose first endpoint is not the
/// larger has three colours and transparent black (BC1); BC2 and BC3 take every block as four.
static void Bc1Block(uint64_t bits, bool three, BcnRule rule, uint16_t out[16][4]) {
    const uint32_t e0 = (uint32_t)bits & 0xFFFFu;
    const uint32_t e1 = (uint32_t)(bits >> 16) & 0xFFFFu;
    const uint32_t indices = (uint32_t)(bits >> 32);
    const int32_t q[2][3] = {
        { (int32_t)(e0 >> 11), (int32_t)(e0 >> 5 & 63u), (int32_t)(e0 & 31u) },
        { (int32_t)(e1 >> 11), (int32_t)(e1 >> 5 & 63u), (int32_t)(e1 & 31u) },
    };
    const int32_t ends[2][3] = {
        { Expand(e0 >> 11, 5), Expand(e0 >> 5 & 63u, 6), Expand(e0 & 31u, 5) },
        { Expand(e1 >> 11, 5), Expand(e1 >> 5 & 63u, 6), Expand(e1 & 31u, 5) },
    };
    const int32_t d = ends[1][1] - ends[0][1];
    const bool four = e0 > e1 || !three;
    int32_t c[4][4];
    for (uint32_t k = 0; k < 3u; ++k) {
        const int32_t a = ends[0][k];
        const int32_t b = ends[1][k];
        c[0][k] = a;
        c[1][k] = b;
        c[3][k] = 0;
        switch (rule) {
        case BCN_RULE_ROUNDED:
            c[2][k] = four ? (2 * a + b + 1) / 3 : (a + b + 1) / 2;
            c[3][k] = four ? (a + 2 * b + 1) / 3 : 0;
            break;
        case BCN_RULE_TRUNCATED:
            c[2][k] = four ? (2 * a + b) / 3 : (a + b) / 2;
            c[3][k] = four ? (a + 2 * b) / 3 : 0;
            break;
        case BCN_RULE_SIXTYFOURTHS:
            c[2][k] = four ? Lerp64(a, b, 21) : Lerp64(a, b, 32);
            c[3][k] = four ? Lerp64(a, b, 43) : 0;
            break;
        case BCN_RULE_LLVMPIPE:
            c[2][k] = four ? Lerp256Down(a, b, 85) : (a + b + 1) / 2;
            c[3][k] = four ? Lerp256Down(a, b, 170) : 0;
            break;
        default:
            // NVIDIA's: green by 80 of 256 from its eight bit ends (d / 4 rounds toward nought,
            // and the sums are never negative), red and blue from the five bit codes.
            if (four && k == 1u) {
                c[2][k] = (256 * a + d / 4 + 128 + d * 80) / 256;
                c[3][k] = (256 * b - d / 4 + 128 - d * 80) / 256;
            } else if (four) {
                c[2][k] = (2 * q[0][k] + q[1][k]) * 22 / 8;
                c[3][k] = (q[0][k] + 2 * q[1][k]) * 22 / 8;
            } else {
                c[2][k] = k == 1u ? (256 * a + d / 4 + 128 + d * 128) / 256
                                  : (q[0][k] + q[1][k]) * 33 / 8;
            }
            break;
        }
    }
    c[0][3] = c[1][3] = c[2][3] = 255;
    c[3][3] = four ? 255 : 0;
    for (uint32_t i = 0; i < 16u; ++i) {
        for (uint32_t k = 0; k < 4u; ++k) {
            out[i][k] = (uint16_t)c[indices >> (2u * i) & 3u][k];
        }
    }
}

/// NVIDIA's BC4 weights of the second endpoint for indices 2 to 7: eight values (the first
/// endpoint the larger), then six. Unsigned they are 257ths, signed 258ths.
static const int32_t BC4_WEIGHT[2][2][6] = {
    { { 36, 72, 113, 144, 185, 221 }, { 48, 96, 161, 209 } },
    { { 36, 72, 114, 144, 186, 222 }, { 48, 96, 162, 210 } },
};

/// A BC4 block's sixteen values as 16 bit UNORM, or with @p snorm SNORM. Each rule's value is x / d
/// of the endpoints' range, taken to 16 bits rounded to nearest (BCN_RULE_TRUNCATED's is its
/// truncated byte, widened).
static void Bc4Block(uint64_t bits, bool snorm, BcnRule rule, uint16_t out[16]) {
    int32_t a0 = (int32_t)(bits & 0xFFu);
    int32_t a1 = (int32_t)(bits >> 8 & 0xFFu);
    if (snorm) {
        a0 = a0 >= 128 ? a0 - 256 : a0;
        a1 = a1 >= 128 ? a1 - 256 : a1;
        a0 = a0 == -128 ? -127 : a0;
        a1 = a1 == -128 ? -127 : a1;
    }
    const bool eight = a0 > a1;
    for (uint32_t i = 0; i < 16u; ++i) {
        const int32_t k = (int32_t)(bits >> (16u + 3u * i) & 7u);
        int32_t x;
        int32_t d;
        if (k == 0) {
            x = a0, d = 1;
        } else if (k == 1) {
            x = a1, d = 1;
        } else if (!eight && k >= 6) {
            x = k == 6 ? (snorm ? -127 : 0) : (snorm ? 127 : 255), d = 1;
        } else if (rule == BCN_RULE_NVIDIA) {
            const int32_t w = BC4_WEIGHT[snorm][!eight][k - 2];
            x = ((snorm ? 258 : 257) - w) * a0 + w * a1, d = snorm ? 258 : 257;
        } else if (rule == BCN_RULE_SIXTYFOURTHS) {
            const int32_t w = eight ? WEIGHTS3[k - 1] : FIFTHS64[k - 1];
            x = (64 - w) * a0 + w * a1, d = 64;
        } else if (eight) {
            x = (8 - k) * a0 + (k - 1) * a1, d = 7;
        } else {
            x = (6 - k) * a0 + (k - 1) * a1, d = 5;
        }
        if (rule == BCN_RULE_TRUNCATED || rule == BCN_RULE_LLVMPIPE) {
            // Cut to a whole code, toward nought, before it is widened.
            x /= d;
            d = 1;
        }
        if (!snorm) {
            out[i] = (uint16_t)((x * 257 + d / 2) / d);
        } else {
            const int32_t m = x < 0 ? -x : x;
            const int32_t v = (m * 32767 + 127 * d / 2) / (127 * d);
            out[i] = (uint16_t)(x < 0 ? -v : v);
        }
    }
}

/// NVIDIA's BC3 alpha weights in 2048ths, by the steps from the nearer endpoint: eight values,
/// then six.
static const int32_t BC3_WEIGHT[2][4] = { { 0, 289, 578, 892 }, { 0, 385, 770, 0 } };

/// BC3's alpha, a BC4 block to bytes, into channel 3 of @p out.
static void Bc3Alpha(uint64_t bits, BcnRule rule, uint16_t out[16][4]) {
    const int32_t a0 = (int32_t)(bits & 0xFFu);
    const int32_t a1 = (int32_t)(bits >> 8 & 0xFFu);
    const int32_t n = a0 > a1 ? 7 : 5;
    for (uint32_t i = 0; i < 16u; ++i) {
        const int32_t k = (int32_t)(bits >> (16u + 3u * i) & 7u);
        int32_t v;
        if (k < 2) {
            v = k != 0 ? a1 : a0;
        } else if (n == 5 && k >= 6) {
            v = k == 6 ? 0 : 255;
        } else if (rule == BCN_RULE_NVIDIA && 2 * (k - 1) < n) {
            // The nearer end is a0, k - 1 steps away.
            v = (2048 * a0 + 1024 + BC3_WEIGHT[n == 5][k - 1] * (a1 - a0)) / 2048;
        } else if (rule == BCN_RULE_NVIDIA) {
            v = (2048 * a1 + 1024 + BC3_WEIGHT[n == 5][n + 1 - k] * (a0 - a1)) / 2048;
        } else if (rule == BCN_RULE_SIXTYFOURTHS) {
            v = Lerp64(a0, a1, n == 7 ? WEIGHTS3[k - 1] : FIFTHS64[k - 1]);
        } else {
            const int32_t sum = n == 7 ? (8 - k) * a0 + (k - 1) * a1 : (6 - k) * a0 + (k - 1) * a1;
            // BCN_RULE_LLVMPIPE's alpha as BCN_RULE_TRUNCATED's.
            v = rule == BCN_RULE_ROUNDED ? (sum + n / 2) / n : sum / n;
        }
        out[i][3] = (uint16_t)v;
    }
}

// ---- BPTC: the partitions and weights BC6H and BC7 share

/// The subset (1) of each texel of the 64 two subset partitions, a bit a texel.
static const uint16_t PARTITION2[64] = {
    0xCCCC, 0x8888, 0xEEEE, 0xECC8, 0xC880, 0xFEEC, 0xFEC8, 0xEC80, 0xC800, 0xFFEC, 0xFE80,
    0xE800, 0xFFE8, 0xFF00, 0xFFF0, 0xF000, 0xF710, 0x008E, 0x7100, 0x08CE, 0x008C, 0x7310,
    0x3100, 0x8CCE, 0x088C, 0x3110, 0x6666, 0x366C, 0x17E8, 0x0FF0, 0x718E, 0x399C, 0xAAAA,
    0xF0F0, 0x5A5A, 0x33CC, 0x3C3C, 0x55AA, 0x9696, 0xA55A, 0x73CE, 0x13C8, 0x324C, 0x3BDC,
    0x6996, 0xC33C, 0x9966, 0x0660, 0x0272, 0x04E4, 0x4E40, 0x2720, 0xC936, 0x936C, 0x39C6,
    0x639C, 0x9336, 0x9CC6, 0x817E, 0xE718, 0xCCF0, 0x0FCC, 0x7744, 0xEE22,
};

/// The subset of each texel of the 64 three subset partitions, two bits a texel, texel 0 lowest.
static const uint32_t PARTITION3[64] = {
    0xaa685050u, 0x6a5a5040u, 0x5a5a4200u, 0x5450a0a8u, 0xa5a50000u, 0xa0a05050u, 0x5555a0a0u,
    0x5a5a5050u, 0xaa550000u, 0xaa555500u, 0xaaaa5500u, 0x90909090u, 0x94949494u, 0xa4a4a4a4u,
    0xa9a59450u, 0x2a0a4250u, 0xa5945040u, 0x0a425054u, 0xa5a5a500u, 0x55a0a0a0u, 0xa8a85454u,
    0x6a6a4040u, 0xa4a45000u, 0x1a1a0500u, 0x0050a4a4u, 0xaaa59090u, 0x14696914u, 0x69691400u,
    0xa08585a0u, 0xaa821414u, 0x50a4a450u, 0x6a5a0200u, 0xa9a58000u, 0x5090a0a8u, 0xa8a09050u,
    0x24242424u, 0x00aa5500u, 0x24924924u, 0x24499224u, 0x50a50a50u, 0x500aa550u, 0xaaaa4444u,
    0x66660000u, 0xa5a0a5a0u, 0x50a050a0u, 0x69286928u, 0x44aaaa44u, 0x66666600u, 0xaa444444u,
    0x54a854a8u, 0x95809580u, 0x96969600u, 0xa85454a8u, 0x80959580u, 0xaa141414u, 0x96960000u,
    0xaaaa1414u, 0xa05050a0u, 0xa0a5a5a0u, 0x96000000u, 0x40804080u, 0xa9a8a9a8u, 0xaaaaaa44u,
    0x2a4a5254u,
};

/// The anchor texel of subset 1 of each two subset partition, and of subsets 1 and 2 of each
/// three subset one (subset 0's is texel 0). An anchor's index leaves out its top bit, nought.
static const uint8_t ANCHOR2[64] = {
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 2, 8, 2, 2, 8,
    8,  15, 2,  8,  2,  2,  8,  8,  2,  2,  15, 15, 6,  8,  2,  8,  15, 15, 2, 8, 2, 2,
    2,  15, 15, 6,  6,  2,  6,  8,  15, 15, 2,  2,  15, 15, 15, 15, 15, 2,  2, 15,
};
static const uint8_t ANCHOR3[2][64] = {
    { 3, 3,  15, 15, 8, 3,  15, 15, 8, 8,  6,  6,  6,  5,  3,  3, 3,  3,  8, 15, 3,  3,
      6, 10, 5,  8,  8, 6,  8,  5,  15, 15, 8, 15, 3,  5,  6,  10, 8, 15, 15, 3,  15, 5,
      15, 15, 15, 15, 3, 15, 5, 5,  5,  8,  5, 10, 5,  10, 8,  13, 15, 12, 3,  3 },
    { 15, 8,  8,  3,  15, 15, 3,  8,  15, 15, 15, 15, 15, 15, 15, 8,  15, 8,  15, 3,  15, 8,
      15, 8,  3,  15, 6,  10, 15, 15, 10, 8,  15, 3,  15, 10, 10, 8,  9,  10, 6,  15, 8,  15,
      3,  6,  6,  8,  15, 3,  15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 3,  15, 15, 8 },
};

/// The subset of texel @p i of partition @p p of a block of @p subsets subsets.
static uint32_t Subset(uint32_t subsets, uint32_t p, uint32_t i) {
    if (subsets == 1u) {
        return 0u;
    }
    if (subsets == 2u) {
        return PARTITION2[p] >> i & 1u;
    }
    return PARTITION3[p] >> (2u * i) & 3u;
}

static bool IsAnchor(uint32_t subsets, uint32_t p, uint32_t i) {
    return i == 0u || (subsets == 2u && i == ANCHOR2[p]) ||
           (subsets == 3u && (i == ANCHOR3[0][p] || i == ANCHOR3[1][p]));
}

/// Sixteen indices of @p bits bits each, anchors one less.
static void Indices(BcnBits *b, uint32_t subsets, uint32_t p, uint32_t bits, uint8_t out[16]) {
    for (uint32_t i = 0; i < 16u; ++i) {
        out[i] = (uint8_t)Take(b, bits - (IsAnchor(subsets, p, i) ? 1u : 0u));
    }
}

// ---- BC6H

/// A mode after its mode bits: the endpoints' fields in the order the block holds them, each
/// `<channel><endpoint>[hi:lo]` (r0 to b3) with bit lo first and hi last, so [10:15] is read from
/// its high bit down, or one bit as [n].
typedef struct {
    uint8_t code;
    uint8_t code_bits;
    uint8_t regions;
    uint8_t transformed;
    uint8_t endpoint_bits;
    uint8_t delta_bits[3];
    const char *fields;
} Bc6Mode;

static const Bc6Mode BC6_MODES[14] = {
    { 0, 2, 2, 1, 10, { 5, 5, 5 },
      "g2[4] b2[4] b3[4] r0[9:0] g0[9:0] b0[9:0] r1[4:0] g3[4] g2[3:0] g1[4:0] b3[0] g3[3:0] "
      "b1[4:0] b3[1] b2[3:0] r2[4:0] b3[2] r3[4:0] b3[3]" },
    { 1, 2, 2, 1, 7, { 6, 6, 6 },
      "g2[5] g3[4] g3[5] r0[6:0] b3[0] b3[1] b2[4] g0[6:0] b2[5] b3[2] g2[4] b0[6:0] b3[3] b3[5] "
      "b3[4] r1[5:0] g2[3:0] g1[5:0] g3[3:0] b1[5:0] b2[3:0] r2[5:0] r3[5:0]" },
    { 2, 5, 2, 1, 11, { 5, 4, 4 },
      "r0[9:0] g0[9:0] b0[9:0] r1[4:0] r0[10] g2[3:0] g1[3:0] g0[10] b3[0] g3[3:0] b1[3:0] b0[10] "
      "b3[1] b2[3:0] r2[4:0] b3[2] r3[4:0] b3[3]" },
    { 6, 5, 2, 1, 11, { 4, 5, 4 },
      "r0[9:0] g0[9:0] b0[9:0] r1[3:0] r0[10] g3[4] g2[3:0] g1[4:0] g0[10] g3[3:0] b1[3:0] b0[10] "
      "b3[1] b2[3:0] r2[3:0] b3[0] b3[2] r3[3:0] g2[4] b3[3]" },
    { 10, 5, 2, 1, 11, { 4, 4, 5 },
      "r0[9:0] g0[9:0] b0[9:0] r1[3:0] r0[10] b2[4] g2[3:0] g1[3:0] g0[10] b3[0] g3[3:0] b1[4:0] "
      "b0[10] b2[3:0] r2[3:0] b3[1] b3[2] r3[3:0] b3[4] b3[3]" },
    { 14, 5, 2, 1, 9, { 5, 5, 5 },
      "r0[8:0] b2[4] g0[8:0] g2[4] b0[8:0] b3[4] r1[4:0] g3[4] g2[3:0] g1[4:0] b3[0] g3[3:0] "
      "b1[4:0] b3[1] b2[3:0] r2[4:0] b3[2] r3[4:0] b3[3]" },
    { 18, 5, 2, 1, 8, { 6, 5, 5 },
      "r0[7:0] g3[4] b2[4] g0[7:0] b3[2] g2[4] b0[7:0] b3[3] b3[4] r1[5:0] g2[3:0] g1[4:0] b3[0] "
      "g3[3:0] b1[4:0] b3[1] b2[3:0] r2[5:0] r3[5:0]" },
    { 22, 5, 2, 1, 8, { 5, 6, 5 },
      "r0[7:0] b3[0] b2[4] g0[7:0] g2[5] g2[4] b0[7:0] g3[5] b3[4] r1[4:0] g3[4] g2[3:0] g1[5:0] "
      "g3[3:0] b1[4:0] b3[1] b2[3:0] r2[4:0] b3[2] r3[4:0] b3[3]" },
    { 26, 5, 2, 1, 8, { 5, 5, 6 },
      "r0[7:0] b3[1] b2[4] g0[7:0] b2[5] g2[4] b0[7:0] b3[5] b3[4] r1[4:0] g3[4] g2[3:0] g1[4:0] "
      "b3[0] g3[3:0] b1[5:0] b2[3:0] r2[4:0] b3[2] r3[4:0] b3[3]" },
    { 30, 5, 2, 0, 6, { 6, 6, 6 },
      "r0[5:0] g3[4] b3[0] b3[1] b2[4] g0[5:0] g2[5] b2[5] b3[2] g2[4] b0[5:0] g3[5] b3[3] b3[5] "
      "b3[4] r1[5:0] g2[3:0] g1[5:0] g3[3:0] b1[5:0] b2[3:0] r2[5:0] r3[5:0]" },
    { 3, 5, 1, 0, 10, { 10, 10, 10 }, "r0[9:0] g0[9:0] b0[9:0] r1[9:0] g1[9:0] b1[9:0]" },
    { 7, 5, 1, 1, 11, { 9, 9, 9 },
      "r0[9:0] g0[9:0] b0[9:0] r1[8:0] r0[10] g1[8:0] g0[10] b1[8:0] b0[10]" },
    { 11, 5, 1, 1, 12, { 8, 8, 8 },
      "r0[9:0] g0[9:0] b0[9:0] r1[7:0] r0[10:11] g1[7:0] g0[10:11] b1[7:0] b0[10:11]" },
    { 15, 5, 1, 1, 16, { 4, 4, 4 },
      "r0[9:0] g0[9:0] b0[9:0] r1[3:0] r0[10:15] g1[3:0] g0[10:15] b1[3:0] b0[10:15]" },
};

static int32_t FieldNumber(const char **s) {
    int32_t v = 0;
    while (**s >= '0' && **s <= '9') {
        v = v * 10 + (*(*s)++ - '0');
    }
    return v;
}

/// The mode's endpoints, e[endpoint][channel], from the block's fields.
static void Bc6Fields(BcnBits *b, const char *s, int32_t e[4][3]) {
    memset(e, 0, sizeof(int32_t[4][3]));
    while (*s != '\0') {
        const int32_t channel = *s == 'r' ? 0 : *s == 'g' ? 1 : 2;
        ++s;
        const int32_t endpoint = FieldNumber(&s);
        ++s; // [
        const int32_t hi = FieldNumber(&s);
        int32_t lo = hi;
        if (*s == ':') {
            ++s;
            lo = FieldNumber(&s);
        }
        s += *s == ']' ? 1 : 0;
        while (*s == ' ') {
            ++s;
        }
        for (int32_t bit = lo;; bit += hi >= lo ? 1 : -1) {
            e[endpoint][channel] |= (int32_t)Take(b, 1) << bit;
            if (bit == hi) {
                break;
            }
        }
    }
}

static int32_t SignExtend(int32_t v, uint32_t bits) {
    const uint32_t sign = 1u << (bits - 1u);
    const uint32_t masked = (uint32_t)v & ((sign << 1) - 1u);
    return (int32_t)(masked ^ sign) - (int32_t)sign;
}

static int32_t Bc6Unquantize(int32_t v, int32_t bits, bool sf) {
    if (!sf) {
        if (bits >= 15 || v == 0) {
            return v;
        }
        if (v == (1 << bits) - 1) {
            return 0xFFFF;
        }
        return ((v << 16) + 0x8000) >> bits;
    }
    if (bits >= 16) {
        return v;
    }
    const bool negative = v < 0;
    const int32_t m = negative ? -v : v;
    int32_t u;
    if (m == 0) {
        u = 0;
    } else if (m >= (1 << (bits - 1)) - 1) {
        u = 0x7FFF;
    } else {
        u = ((m << 15) + 0x4000) >> (bits - 1);
    }
    return negative ? -u : u;
}

/// An interpolated value to a half's bits: 31/64 of the unsigned range, 31/32 of the signed
/// one's magnitude.
static uint16_t Bc6Finish(int32_t v, bool sf) {
    if (!sf) {
        return (uint16_t)(v * 31 >> 6);
    }
    return (uint16_t)(v < 0 ? 0x8000 | (-v * 31 >> 5) : v * 31 >> 5);
}

static void Bc6Block(const uint8_t *block, bool sf, uint16_t out[16][4]) {
    BcnBits b = { { Le64(block), Le64(block + 8) }, 0 };
    const uint32_t code = Peek(&b, 0, 2) < 2u ? Peek(&b, 0, 2) : Peek(&b, 0, 5);
    const Bc6Mode *mode = NULL;
    for (uint32_t i = 0; i < 14u; ++i) {
        if (BC6_MODES[i].code == code && (BC6_MODES[i].code_bits == 2u) == (code < 2u)) {
            mode = &BC6_MODES[i];
        }
    }
    for (uint32_t i = 0; i < 16u; ++i) {
        out[i][0] = out[i][1] = out[i][2] = 0;
        out[i][3] = 0x3C00u;
    }
    if (mode == NULL) {
        return;
    }
    b.at = mode->code_bits;
    int32_t e[4][3];
    Bc6Fields(&b, mode->fields, e);
    const uint32_t regions = mode->regions;
    const uint32_t p = regions == 2u ? Take(&b, 5) : 0u;
    const uint32_t ends = regions * 2u;
    const int32_t bits = mode->endpoint_bits;
    for (uint32_t c = 0; c < 3u; ++c) {
        if (sf) {
            e[0][c] = SignExtend(e[0][c], (uint32_t)bits);
        }
        for (uint32_t k = 1; k < ends; ++k) {
            if (sf || mode->transformed) {
                const uint32_t width = mode->transformed ? mode->delta_bits[c] : (uint32_t)bits;
                e[k][c] = SignExtend(e[k][c], width);
            }
            if (mode->transformed) {
                e[k][c] = (e[0][c] + e[k][c]) & ((1 << bits) - 1);
                if (sf) {
                    e[k][c] = SignExtend(e[k][c], (uint32_t)bits);
                }
            }
        }
        for (uint32_t k = 0; k < ends; ++k) {
            e[k][c] = Bc6Unquantize(e[k][c], bits, sf);
        }
    }
    const uint32_t index_bits = regions == 2u ? 3u : 4u;
    uint8_t index[16];
    Indices(&b, regions, p, index_bits, index);
    for (uint32_t i = 0; i < 16u; ++i) {
        const uint32_t s = Subset(regions, p, i);
        for (uint32_t c = 0; c < 3u; ++c) {
            out[i][c] = Bc6Finish(
                Lerp64(e[2u * s][c], e[2u * s + 1u][c], Weights(index_bits)[index[i]]), sf);
        }
    }
}

// ---- BC7

/// Subsets, partition bits, rotation bits, index selection bits, colour and alpha bits, a p-bit an
/// endpoint or one a subset, index bits and the second index's.
typedef struct {
    uint8_t subsets, partition_bits, rotation_bits, selection_bits, colour_bits, alpha_bits;
    uint8_t endpoint_pbits, shared_pbits, index_bits, index2_bits;
} Bc7Mode;

static const Bc7Mode BC7_MODES[8] = {
    { 3, 4, 0, 0, 4, 0, 1, 0, 3, 0 }, { 2, 6, 0, 0, 6, 0, 0, 1, 3, 0 },
    { 3, 6, 0, 0, 5, 0, 0, 0, 2, 0 }, { 2, 6, 0, 0, 7, 0, 1, 0, 2, 0 },
    { 1, 0, 2, 1, 5, 6, 0, 0, 2, 3 }, { 1, 0, 2, 0, 7, 8, 0, 0, 2, 2 },
    { 1, 0, 0, 0, 7, 7, 1, 0, 4, 0 }, { 2, 6, 0, 0, 5, 5, 1, 0, 2, 0 },
};

static void Bc7Block(const uint8_t *block, uint16_t out[16][4]) {
    BcnBits b = { { Le64(block), Le64(block + 8) }, 0 };
    uint32_t number = 0;
    while (number < 8u && Take(&b, 1) == 0u) {
        ++number;
    }
    if (number == 8u) {
        memset(out, 0, sizeof(uint16_t[16][4]));
        return;
    }
    const Bc7Mode *m = &BC7_MODES[number];
    const uint32_t p = Take(&b, m->partition_bits);
    const uint32_t rotation = Take(&b, m->rotation_bits);
    const uint32_t selection = Take(&b, m->selection_bits);
    const uint32_t ends = m->subsets * 2u;
    int32_t e[6][4];
    for (uint32_t c = 0; c < 4u; ++c) {
        for (uint32_t k = 0; k < ends; ++k) {
            const uint32_t bits = c < 3u ? m->colour_bits : m->alpha_bits;
            e[k][c] = bits != 0u ? (int32_t)Take(&b, bits) : 255;
        }
    }
    // The p-bits, one an endpoint or one a subset its two share, below each channel's bits.
    uint32_t precision[2] = { m->colour_bits, m->alpha_bits };
    if (m->endpoint_pbits != 0u || m->shared_pbits != 0u) {
        uint32_t pbit[6] = { 0, 0, 0, 0, 0, 0 };
        for (uint32_t k = 0; k < ends; ++k) {
            pbit[k] = m->endpoint_pbits != 0u ? Take(&b, 1) : 0u;
        }
        if (m->shared_pbits != 0u) {
            for (uint32_t s = 0; s < m->subsets; ++s) {
                pbit[2u * s] = pbit[2u * s + 1u] = Take(&b, 1);
            }
        }
        for (uint32_t k = 0; k < ends; ++k) {
            for (uint32_t c = 0; c < 4u; ++c) {
                if (c < 3u || m->alpha_bits != 0u) {
                    e[k][c] = e[k][c] << 1 | (int32_t)pbit[k];
                }
            }
        }
        ++precision[0];
        if (m->alpha_bits != 0u) {
            ++precision[1];
        }
    }
    for (uint32_t k = 0; k < ends; ++k) {
        for (uint32_t c = 0; c < 4u; ++c) {
            if (c < 3u || m->alpha_bits != 0u) {
                e[k][c] = Expand((uint32_t)e[k][c], precision[c < 3u ? 0 : 1]);
            }
        }
    }
    uint8_t index[16];
    uint8_t index2[16] = { 0 };
    Indices(&b, m->subsets, p, m->index_bits, index);
    if (m->index2_bits != 0u) {
        Indices(&b, 1, 0, m->index2_bits, index2);
    }
    for (uint32_t i = 0; i < 16u; ++i) {
        const uint32_t s = Subset(m->subsets, p, i);
        // Modes 4 and 5: colour from the first indices and alpha from the second, swapped by the
        // index selection bit.
        const uint32_t colour_index = selection != 0u ? index2[i] : index[i];
        const uint32_t colour_bits = selection != 0u ? m->index2_bits : m->index_bits;
        const uint32_t alpha_index =
            m->index2_bits != 0u ? (selection != 0u ? index[i] : index2[i]) : index[i];
        const uint32_t alpha_bits = m->index2_bits == 0u ? m->index_bits
                                    : selection != 0u    ? m->index_bits
                                                         : m->index2_bits;
        int32_t texel[4];
        for (uint32_t c = 0; c < 4u; ++c) {
            const uint32_t k = c < 3u ? colour_index : alpha_index;
            const uint32_t bits = c < 3u ? colour_bits : alpha_bits;
            texel[c] = Lerp64(e[2u * s][c], e[2u * s + 1u][c], Weights(bits)[k]);
        }
        if (rotation != 0u) {
            // Alpha's values swapped with red's, green's or blue's.
            const int32_t t = texel[3];
            texel[3] = texel[rotation - 1u];
            texel[rotation - 1u] = t;
        }
        for (uint32_t c = 0; c < 4u; ++c) {
            out[i][c] = (uint16_t)texel[c];
        }
    }
}

uint32_t BcnBlockBytes(BcnKind kind) {
    return kind == BCN_BC1 || kind == BCN_BC4 || kind == BCN_BC4_SNORM ? 8u : 16u;
}

void BcnDecodeBlock(BcnKind kind, BcnRule rule, const uint8_t *block, uint16_t texels[16][4]) {
    uint16_t channel[2][16];
    switch (kind) {
    case BCN_BC1:
        Bc1Block(Le64(block), true, rule, texels);
        return;
    case BCN_BC2: {
        const uint64_t alpha = Le64(block);
        Bc1Block(Le64(block + 8), false, rule, texels);
        for (uint32_t i = 0; i < 16u; ++i) {
            texels[i][3] = (uint16_t)((alpha >> (4u * i) & 15u) * 17u);
        }
        return;
    }
    case BCN_BC3:
        Bc1Block(Le64(block + 8), false, rule, texels);
        Bc3Alpha(Le64(block), rule, texels);
        return;
    case BCN_BC4:
    case BCN_BC4_SNORM:
    case BCN_BC5:
    case BCN_BC5_SNORM: {
        const bool snorm = kind == BCN_BC4_SNORM || kind == BCN_BC5_SNORM;
        const bool two = kind == BCN_BC5 || kind == BCN_BC5_SNORM;
        Bc4Block(Le64(block), snorm, rule, channel[0]);
        if (two) {
            Bc4Block(Le64(block + 8), snorm, rule, channel[1]);
        }
        for (uint32_t i = 0; i < 16u; ++i) {
            texels[i][0] = channel[0][i];
            texels[i][1] = two ? channel[1][i] : 0u;
            texels[i][2] = 0u;
            texels[i][3] = snorm ? 0x7FFFu : 0xFFFFu;
        }
        return;
    }
    case BCN_BC6H_UF:
    case BCN_BC6H_SF:
        Bc6Block(block, kind == BCN_BC6H_SF, texels);
        return;
    default:
        Bc7Block(block, texels);
        return;
    }
}

float BcnHalf(uint16_t half) {
    const uint32_t exponent = half >> 10 & 0x1Fu;
    const uint32_t mantissa = half & 0x3FFu;
    float value;
    if (exponent == 0u) {
        value = (float)mantissa / 16777216.0f;
    } else if (exponent == 31u) {
        value = mantissa == 0u ? INFINITY : NAN;
    } else {
        value = (1.0f + (float)mantissa / 1024.0f) * ldexpf(1.0f, (int)exponent - 15);
    }
    return (half & 0x8000u) != 0u ? -value : value;
}

uint8_t BcnTarget8(BcnKind kind, uint32_t channel, uint16_t value, uint8_t *other) {
    uint32_t down = value;
    uint32_t up = value;
    uint32_t nearest = value;
    switch (kind) {
    case BCN_BC4:
    case BCN_BC5: {
        const uint32_t scaled = (uint32_t)value * 255u;
        down = scaled / 65535u;
        up = down + (scaled % 65535u != 0u ? 1u : 0u);
        // 65535 is odd, so no value lands on a half step.
        nearest = (scaled + 32767u) / 65535u;
        break;
    }
    case BCN_BC6H_UF:
    case BCN_BC6H_SF: {
        const float f = BcnHalf(value);
        // Exact in a double: a half has eleven bits, times 255 needs eight more.
        const double scaled = !(f > 0.0f) ? 0.0 : f >= 1.0f ? 255.0 : (double)f * 255.0;
        down = (uint32_t)floor(scaled);
        up = (uint32_t)ceil(scaled);
        nearest = (uint32_t)floor(scaled + 0.5);
        break;
    }
    default:
        break;
    }
    if (channel == 3u && kind != BCN_BC1 && kind != BCN_BC2 && kind != BCN_BC3 &&
        kind != BCN_BC7) {
        down = up = nearest = 255u;
    }
    if (other != NULL) {
        *other = (uint8_t)(nearest == down ? up : down);
    }
    return (uint8_t)nearest;
}
