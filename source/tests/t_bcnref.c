// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../astcimages.h"
#include "../bcnimages.h"
#include "../bcnref.h"
#include "../check.h"
#include "../formatref.h"
#include "../report.h"
#include "tests.h"

// The bcn suite writes its blocks where source/bcnref.c says deko3d laid the array out. Here
// that layout is held to offsets worked out by hand from deko3d 0.5.0's rules
// (source/dk_image.cpp: pickTileSize, adjustTileSize, calcLevelOffset, alignLayerSize), and the
// solid BC4 block to its bytes.
//
// Then the decoder the bcn and bcnimage suites judge by: every picture and every mode block of
// source/bcnimages.c against what Pillow 12.3.0 decodes it to (bcdec 0.985 for BC6H), the
// interpolants of each rule against values worked out by hand and against what RADV read in the
// bcn suite, the bytes a value reaches a target as, and the bcnimage suite's judge on machines
// made to order.

static const char *const SUITE = "reference";

static uint32_t Fnv1a(const uint8_t *data, size_t size) {
    uint32_t h = 0x811C9DC5u;
    for (size_t i = 0; i < size; ++i) {
        h = (h ^ data[i]) * 0x01000193u;
    }
    return h;
}

/// The pictures and the mode blocks against the hashes of Pillow's and bcdec's decodes.
static void OutsideDecoders(void) {
    CHECK_TRUE(SUITE, "the bcnimage suite's picture is the astcimage suite's", CHECK_NONE,
               BCN_IMAGE_SOURCE_HASH == ASTC_IMAGE_SOURCE_HASH,
               "the BCn encoders were given %08x, astcenc %08x", (unsigned)BCN_IMAGE_SOURCE_HASH,
               (unsigned)ASTC_IMAGE_SOURCE_HASH);
    CHECK_TRUE(SUITE, "the bcnimage suite's large textures repeat its BC1 and BC7 pictures",
               CHECK_NONE,
               BCN_IMAGE[BCN_IMAGE_BC1].kind == BCN_BC1 && BCN_IMAGE[BCN_IMAGE_BC1].alpha &&
                   !BCN_IMAGE[BCN_IMAGE_BC1].srgb && BCN_IMAGE[BCN_IMAGE_BC7].kind == BCN_BC7 &&
                   !BCN_IMAGE[BCN_IMAGE_BC7].srgb &&
                   strcmp(BCN_IMAGE[BCN_IMAGE_BC7].name, "BC7") == 0,
               "picture %u is %s, picture %u is %s", (unsigned)BCN_IMAGE_BC1,
               BCN_IMAGE[BCN_IMAGE_BC1].name, (unsigned)BCN_IMAGE_BC7,
               BCN_IMAGE[BCN_IMAGE_BC7].name);
    static uint8_t bytes[BCN_IMAGE_SIZE * BCN_IMAGE_SIZE * 6u];
    for (uint32_t i = 0; i < BCN_IMAGES; ++i) {
        const BcnImage *const image = &BCN_IMAGE[i];
        const uint32_t blocks = BCN_IMAGE_SIZE / 4u;
        const size_t size = BcnReferenceDecode(image->kind, image->blocks, blocks, blocks, bytes);
        const uint32_t got = Fnv1a(bytes, size);
        char name[128];
        snprintf(name, sizeof(name), "the %s picture's blocks decode as %s decodes them",
                 image->name, image->kind == BCN_BC6H_UF ? "bcdec 0.985" : "Pillow 12.3.0");
        CHECK_TRUE(SUITE, name, CHECK_NONE,
                   got == image->pillow_hash && image->block_count == blocks * blocks,
                   "%u blocks, the decode hashes to %08x, the outside decoder's to %08x",
                   (unsigned)image->block_count, (unsigned)got, (unsigned)image->pillow_hash);
    }
    // The mode blocks, BC7's to Pillow's and BC6H's both ways to bcdec's.
    for (uint32_t family = 0; family < 2u; ++family) {
        uint32_t wrong = 0;
        uint32_t count = 0;
        const char *first = "";
        for (uint32_t i = 0; i < BCN_MODE_BLOCKS; ++i) {
            const BcnModeBlock *const b = &BCN_MODE_BLOCK[i];
            if ((b->kind == BCN_BC7) != (family == 0u)) {
                continue;
            }
            for (uint32_t way = 0; way < (family == 0u ? 1u : 2u); ++way) {
                const size_t size =
                    BcnReferenceDecode(way == 0u ? b->kind : BCN_BC6H_SF, b->bytes, 1, 1, bytes);
                const uint32_t want = way == 0u ? b->pillow_hash : b->pillow_hash_signed;
                ++count;
                if (Fnv1a(bytes, size) != want && wrong++ == 0u) {
                    first = b->name;
                }
            }
        }
        CHECK_TRUE(SUITE,
                   family == 0u ? "a block of every BC7 mode decodes as Pillow 12.3.0 decodes it"
                                : "a block of every BC6H mode and reserved code decodes unsigned "
                                  "and signed as bcdec 0.985 decodes it",
                   CHECK_NONE, wrong == 0u && count == (family == 0u ? 8u : 36u),
                   "%u of %u decodes differ, the first %s", (unsigned)wrong, (unsigned)count,
                   first);
    }
}

/// Texel @p t's first three channels of @p kind's @p block under @p rule, against @p want.
static bool Texel3(BcnKind kind, BcnRule rule, const uint8_t *block, uint32_t t,
                   uint16_t a, uint16_t b, uint16_t c, char *seen, size_t seen_size) {
    uint16_t texels[16][4];
    BcnDecodeBlock(kind, rule, block, texels);
    const bool right = texels[t][0] == a && texels[t][1] == b && texels[t][2] == c;
    if (!right && seen[0] == '\0') {
        snprintf(seen, seen_size, "texel %u is %u %u %u, %u %u %u wanted", (unsigned)t,
                 texels[t][0], texels[t][1], texels[t][2], a, b, c);
    }
    return right;
}

static uint16_t Channel(BcnKind kind, BcnRule rule, const uint8_t *block, uint32_t t,
                        uint32_t c) {
    uint16_t texels[16][4];
    BcnDecodeBlock(kind, rule, block, texels);
    return texels[t][c];
}

static void PutWord64(uint8_t *out, uint64_t value) {
    for (uint32_t i = 0; i < 8u; ++i) {
        out[i] = (uint8_t)(value >> (8u * i));
    }
}

/// Each rule's interpolants against numbers worked out by hand: NVIDIA's are the bcn suite's (an
/// NVIDIA card's, as ScarletNX's BCn decoder test measured them), the sixty-fourths' what RADV on
/// an RX 6700 XT read in the bcn suite (edenprobe 23f4c7a under ScarletNX), the rounded and cut
/// ones the specification's fractions.
static void Rules(void) {
    uint8_t bc1[8];
    uint8_t bc3[16];
    uint8_t bc4[8];
    char seen[96] = "";
    // BC1 between 31 9 2 and 2 0 31: thirds 176 25 96 and 96 11 176 at NVIDIA's, 177 24 94 at
    // sixty-fourths, 175 24 96 rounded (526 / 3, 72 / 3, 287 / 3) and 175 24 95 cut.
    PutWord64(bc1, 0xE4ull << 32 | 0x101Full << 16 | 0xF922u);
    bool right = Texel3(BCN_BC1, BCN_RULE_NVIDIA, bc1, 2, 176, 25, 96, seen, sizeof(seen)) &&
                 Texel3(BCN_BC1, BCN_RULE_NVIDIA, bc1, 3, 96, 11, 176, seen, sizeof(seen)) &&
                 Texel3(BCN_BC1, BCN_RULE_SIXTYFOURTHS, bc1, 2, 177, 24, 94, seen, sizeof(seen)) &&
                 Texel3(BCN_BC1, BCN_RULE_ROUNDED, bc1, 2, 175, 24, 96, seen, sizeof(seen)) &&
                 Texel3(BCN_BC1, BCN_RULE_TRUNCATED, bc1, 2, 175, 24, 95, seen, sizeof(seen));
    // Red above blue: NVIDIA 170 0 85, sixty-fourths 171 0 84. Swapped, three colours: the
    // midpoint NVIDIA 127, sixty-fourths 128, and index 3 transparent black under every rule.
    PutWord64(bc1, 0xE4ull << 32 | 0x001Full << 16 | 0xF800u);
    right = right && Texel3(BCN_BC1, BCN_RULE_NVIDIA, bc1, 2, 170, 0, 85, seen, sizeof(seen)) &&
            Texel3(BCN_BC1, BCN_RULE_SIXTYFOURTHS, bc1, 2, 171, 0, 84, seen, sizeof(seen));
    PutWord64(bc1, 0xE4ull << 32 | 0xF800ull << 16 | 0x001Fu);
    right = right && Texel3(BCN_BC1, BCN_RULE_NVIDIA, bc1, 2, 127, 0, 127, seen, sizeof(seen)) &&
            Texel3(BCN_BC1, BCN_RULE_SIXTYFOURTHS, bc1, 2, 128, 0, 128, seen, sizeof(seen)) &&
            Channel(BCN_BC1, BCN_RULE_ROUNDED, bc1, 3, 3) == 0u;
    CHECK_TRUE(SUITE, "each rule's BC1 thirds and midpoints are its own numbers", CHECK_NONE, right,
               "%s", seen);
    // BC3's alpha: six values from 0 to 255, index 2 is NVIDIA's 48, sixty-fourths' 52, a fifth
    // rounded 51; eight from 255 to 4, index 4 is NVIDIA's 146, sixty-fourths' 149, 1032 / 7
    // rounded 147 and cut 147.
    memset(bc3, 0, sizeof(bc3));
    PutWord64(bc3, 2ull << 16 | 0xFF00u);
    PutWord64(bc3 + 8, 0xF800u);
    const uint16_t six[3] = { Channel(BCN_BC3, BCN_RULE_NVIDIA, bc3, 0, 3),
                              Channel(BCN_BC3, BCN_RULE_SIXTYFOURTHS, bc3, 0, 3),
                              Channel(BCN_BC3, BCN_RULE_ROUNDED, bc3, 0, 3) };
    PutWord64(bc3, 4ull << 16 | 0x04FFu);
    const uint16_t eight[3] = { Channel(BCN_BC3, BCN_RULE_NVIDIA, bc3, 0, 3),
                                Channel(BCN_BC3, BCN_RULE_SIXTYFOURTHS, bc3, 0, 3),
                                Channel(BCN_BC3, BCN_RULE_ROUNDED, bc3, 0, 3) };
    CHECK_TRUE(SUITE, "each rule's BC3 alphas are its own numbers", CHECK_NONE,
               six[0] == 48u && six[1] == 52u && six[2] == 51u && eight[0] == 146u &&
                   eight[1] == 149u && eight[2] == 147u,
               "six values: %u %u %u, 48 52 51 wanted; eight: %u %u %u, 146 149 147 wanted", six[0],
               six[1], six[2], eight[0], eight[1], eight[2]);
    // BC4, 255 above 0, index 2: NVIDIA's 56355 of 65535 (219 257ths), sixty-fourths' 55 / 64 of
    // the range, which RADV read as 0.859375 and is 56319 rounded, six sevenths 56173 rounded;
    // signed, 127 above -128 (-127): NVIDIA's 23623, sixty-fourths' 46 / 64 of 127, 0.71875, 23551.
    PutWord64(bc4, 2ull << 16 | 0x00FFu);
    const uint16_t unorm[3] = { Channel(BCN_BC4, BCN_RULE_NVIDIA, bc4, 0, 0),
                                Channel(BCN_BC4, BCN_RULE_SIXTYFOURTHS, bc4, 0, 0),
                                Channel(BCN_BC4, BCN_RULE_ROUNDED, bc4, 0, 0) };
    PutWord64(bc4, 2ull << 16 | 0x807Fu);
    const uint16_t snorm[2] = { Channel(BCN_BC4_SNORM, BCN_RULE_NVIDIA, bc4, 0, 0),
                                Channel(BCN_BC4_SNORM, BCN_RULE_SIXTYFOURTHS, bc4, 0, 0) };
    CHECK_TRUE(SUITE, "each rule's BC4 weights are its own numbers", CHECK_NONE,
               unorm[0] == 56355u && unorm[1] == 56319u && unorm[2] == 56173u &&
                   snorm[0] == 23623u && snorm[1] == 23551u,
               "unsigned %u %u %u, 56355 56319 56173 wanted; signed %u %u, 23623 23551 wanted",
               unorm[0], unorm[1], unorm[2], snorm[0], snorm[1]);
    // The bytes a target gets, the nearest and the integer on the value's other side: BC4's 56355
    // of 65535 is 219.29, 219 then 220, and 65535 itself 255 alone; a half clamped and times 255,
    // 0.5 is 127.5, 128 then 127, and 0x3555 (0.33325) 84.98, 85 then 84; -1 is 0 and 65504 255
    // alone; BC6H's alpha the target's 255, BC7's its own byte.
    uint8_t other[6] = { 0, 0, 0, 0, 0, 0 };
    const uint8_t near[6] = { BcnTarget8(BCN_BC4, 0, 56355u, &other[0]),
                              BcnTarget8(BCN_BC4, 0, 65535u, &other[1]),
                              BcnTarget8(BCN_BC6H_UF, 0, 0x3800u, &other[2]),
                              BcnTarget8(BCN_BC6H_UF, 1, 0x3555u, &other[3]),
                              BcnTarget8(BCN_BC6H_SF, 0, 0xBC00u, &other[4]),
                              BcnTarget8(BCN_BC6H_UF, 0, 0x7BFFu, &other[5]) };
    CHECK_TRUE(SUITE,
               "a decoded value reaches a UNORM8 target as the nearest byte or the other one "
               "beside it",
               CHECK_NONE,
               near[0] == 219u && other[0] == 220u && near[1] == 255u && other[1] == 255u &&
                   near[2] == 128u && other[2] == 127u && near[3] == 85u && other[3] == 84u &&
                   near[4] == 0u && other[4] == 0u && near[5] == 255u && other[5] == 255u &&
                   BcnTarget8(BCN_BC4, 3, 0u, NULL) == 255u &&
                   BcnTarget8(BCN_BC6H_UF, 3, 0x3C00u, NULL) == 255u &&
                   BcnTarget8(BCN_BC7, 3, 17u, NULL) == 17u,
               "56355 is %u or %u, 65535 %u or %u, 0.5 %u or %u, 0x3555 %u or %u, -1 %u or %u, "
               "65504 %u or %u",
               near[0], other[0], near[1], other[1], near[2], other[2], near[3], other[3], near[4],
               other[4], near[5], other[5]);
}

/// What a machine made to order reads for texel (@p x, @p y) of @p image.
typedef enum {
    MACHINE_CONSOLE,      ///< the console's decode
    MACHINE_ROUNDED,      ///< the exact fractions rounded
    MACHINE_SIXTYFOURTHS, ///< BPTC's weights
    MACHINE_RADV,         ///< BPTC's weights, the colour through a half cut toward nought, as RADV
    MACHINE_COLUMNS,      ///< a block read down its columns: texel (x, y) is the decode's (y, x)
    MACHINE_ASIDE,        ///< the block to the right's texel (the last column's from the first)
    MACHINES,
} Machine;

/// The float a fragment writes for channel @p c of a texel decoded to @p value, in [0, 1].
static double TargetFloat(const BcnImage *image, uint32_t c, uint16_t value) {
    if (image->srgb && c < 3u) {
        return FormatSrgbDecode((double)value / 255.0);
    }
    switch (image->kind) {
    case BCN_BC4:
    case BCN_BC5:
        return c == 3u ? 1.0 : (double)value / 65535.0;
    case BCN_BC6H_UF:
    case BCN_BC6H_SF: {
        const float f = BcnHalf(value);
        return c == 3u ? 1.0 : !(f > 0.0f) ? 0.0 : f >= 1.0f ? 1.0 : (double)f;
    }
    default:
        return (double)value / 255.0;
    }
}

/// @p f taken to a half toward nought, then to the nearest byte: RADV's colour export to an
/// eight bit target.
static uint8_t HalfTowardNought8(double f) {
    if (!(f > 0.0)) {
        return 0u;
    }
    int exponent = 0;
    (void)frexp(f, &exponent);
    // A normal half keeps eleven bits from its leading one; below 2^-14 the step is 2^-24.
    const double step = exponent - 1 >= -14 ? ldexp(1.0, exponent - 11) : ldexp(1.0, -24);
    const double cut = floor(f / step) * step;
    return (uint8_t)floor(cut * 255.0 + 0.5);
}

static void MachineTexel(Machine machine, const BcnImage *image, uint32_t x, uint32_t y,
                         uint8_t out[4]) {
    static const BcnRule RULE[MACHINES] = { BCN_RULE_NVIDIA,       BCN_RULE_ROUNDED,
                                            BCN_RULE_SIXTYFOURTHS, BCN_RULE_SIXTYFOURTHS,
                                            BCN_RULE_NVIDIA,       BCN_RULE_NVIDIA };
    const uint32_t bx = machine == MACHINE_ASIDE ? (x + 4u) % BCN_IMAGE_SIZE : x;
    uint16_t texels[16][4];
    BcnDecodeBlock(image->kind, RULE[machine], BcnImageBlock(image, bx, y), texels);
    const uint16_t *const texel = machine == MACHINE_COLUMNS ? texels[(x % 4u) * 4u + y % 4u]
                                                             : texels[(y % 4u) * 4u + x % 4u];
    for (uint32_t c = 0; c < 4u; ++c) {
        // The target's byte: the nearest, the lower one beside an sRGB code's linear value.
        if (c == 3u && !image->alpha) {
            out[c] = 255u;
        } else if (machine == MACHINE_RADV) {
            out[c] = HalfTowardNought8(TargetFloat(image, c, texel[c]));
        } else if (image->srgb && c < 3u) {
            out[c] = (uint8_t)(FormatSrgbDecode((double)texel[c] / 255.0) * 255.0);
        } else {
            out[c] = BcnTarget8(image->kind, c, texel[c], NULL);
        }
    }
}

/// The bcnimage suite's judge on machines made to order, every picture: the console's decode is
/// exact; the rounded fractions and BPTC's sixty-fourths are off but within the bound, on BC1 to
/// BC5 only, and RADV's, the sixty-fourths through a half cut toward nought, wrong nowhere; a
/// block read down its columns and a block a place aside are wrong, over an eighth of the picture
/// at least.
static void Machines(void) {
    static BcnImageAnswers answers;
    static const char *const NAME[MACHINES] = { "the console's",      "the rounded",
                                                "the sixty-fourths",  "RADV's",
                                                "a block read down its columns", "a block aside" };
    uint32_t mistakes = 0;
    char first[192] = "";
    for (uint32_t i = 0; i < BCN_IMAGES; ++i) {
        const BcnImage *const image = &BCN_IMAGE[i];
        BcnImageAnswer(image, &answers);
        for (uint32_t m = 0; m < MACHINES; ++m) {
            uint32_t count[3] = { 0, 0, 0 };
            for (uint32_t y = 0; y < BCN_IMAGE_SIZE; ++y) {
                for (uint32_t x = 0; x < BCN_IMAGE_SIZE; ++x) {
                    uint8_t got[4];
                    MachineTexel((Machine)m, image, x, y, got);
                    ++count[BcnImageVerdict(&answers, x, y, got, NULL)];
                }
            }
            bool right;
            if (m == MACHINE_CONSOLE) {
                right = count[BCN_TEXEL_OFF] == 0u && count[BCN_TEXEL_WRONG] == 0u;
            } else if (m == MACHINE_RADV) {
                right = count[BCN_TEXEL_WRONG] == 0u;
            } else if (m == MACHINE_ROUNDED || m == MACHINE_SIXTYFOURTHS) {
                right = count[BCN_TEXEL_WRONG] == 0u &&
                        (BcnHasRules(image->kind) ? count[BCN_TEXEL_OFF] != 0u
                                                  : count[BCN_TEXEL_OFF] == 0u);
            } else {
                right = count[BCN_TEXEL_WRONG] > BCN_IMAGE_SIZE * BCN_IMAGE_SIZE / 8u;
            }
            if (!right && mistakes++ == 0u) {
                snprintf(first, sizeof(first),
                         "%s machine on the %s picture: %u exact, %u off, %u wrong", NAME[m],
                         image->name, (unsigned)count[0], (unsigned)count[1], (unsigned)count[2]);
            }
        }
    }
    CHECK_TRUE(SUITE,
               "the bcnimage judge: the console's decode exact, other GPUs' and RADV's bound, a "
               "block read down its columns or aside wrong",
               CHECK_NONE, mistakes == 0u, "%u verdicts not as they should be; the first, %s",
               (unsigned)mistakes, first);
}

void RunReferenceBcn(void) {
    BcnArray a;
    // 4x4 BC4, three levels, six layers: one block per level, rows 1 * 1.5 -> one gob, so every
    // level is one 512-byte gob and a layer is three of them.
    BcnArrayLayout(&a, 4, 4, 6, 3, 8);
    CHECK_TRUE(SUITE, "a 4x4 BC4 array's levels are one gob each and a layer is three",
               CHECK_NONE,
               BcnLevelOffset(&a, 1) == 512u && BcnLevelOffset(&a, 2) == 1024u &&
                   a.layer_size == 1536u && BcnBlockOffset(&a, 5, 2, 0, 0) == 5u * 1536u + 1024u,
               "levels at %u and %u, layer size %u, layer 5 level 2 at %u",
               (unsigned)BcnLevelOffset(&a, 1), (unsigned)BcnLevelOffset(&a, 2),
               (unsigned)a.layer_size, (unsigned)BcnBlockOffset(&a, 5, 2, 0, 0));
    // 8x6: 2x2 blocks in one gob; block (1, 1) is 8 bytes across and one row down, which the
    // gob puts at 16 + 8.
    BcnArrayLayout(&a, 8, 6, 2, 1, 8);
    CHECK_TRUE(SUITE, "a BC4 block one down and one across sits 24 bytes into its gob",
               CHECK_NONE, BcnBlockOffset(&a, 0, 0, 1, 1) == 24u && a.layer_size == 512u,
               "block (1, 1) at %u, layer size %u", (unsigned)BcnBlockOffset(&a, 0, 0, 1, 1),
               (unsigned)a.layer_size);
    // 64x64: 16 block rows * 1.5 = 24 rows, three gobs, so a tile of two gobs. Level 0 is two
    // gobs wide (128 bytes) and one tile tall: 2048. Levels 1 and 2 (8 and 4 block rows) shrink
    // to one-gob tiles: 512 each. 3072 is already a multiple of the 1024-byte tile.
    BcnArrayLayout(&a, 64, 64, 2, 3, 8);
    CHECK_TRUE(SUITE, "a 64x64 BC4 array uses two-gob tiles on level 0 and one-gob tiles below",
               CHECK_NONE,
               a.tile_shift == 1u && BcnLevelOffset(&a, 1) == 2048u &&
                   BcnLevelOffset(&a, 2) == 2560u && a.layer_size == 3072u,
               "tile shift %u, levels at %u and %u, layer size %u", (unsigned)a.tile_shift,
               (unsigned)BcnLevelOffset(&a, 1), (unsigned)BcnLevelOffset(&a, 2),
               (unsigned)a.layer_size);
    uint8_t block[8];
    BcnSolidBc4(block, 0x5A);
    CHECK_TRUE(SUITE, "a solid BC4 block is its value twice and sixteen nought indices",
               CHECK_NONE,
               block[0] == 0x5Au && block[1] == 0x5Au && block[2] == 0u && block[7] == 0u,
               "bytes %02x %02x %02x .. %02x", block[0], block[1], block[2], block[7]);
    OutsideDecoders();
    Rules();
    Machines();
}
