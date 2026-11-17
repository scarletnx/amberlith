// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Where the processor writes the blocks of a block linear 2D array of a 4x4 block format, as
/// deko3d lays it out (source/dk_image.cpp, 0.5.0): each level's tile height shrinks to what
/// its height needs, the levels follow each other inside a layer, and a layer is aligned to
/// the tile height of level 0's size.
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t layers;
    uint32_t levels;
    uint32_t bytes_per_block;
    uint32_t tile_shift;
    uint32_t layer_size;
} BcnArray;

/// Lays out a @p width by @p height array of @p layers layers and @p levels levels.
void BcnArrayLayout(BcnArray *array, uint32_t width, uint32_t height, uint32_t layers,
                    uint32_t levels, uint32_t bytes_per_block);

/// The byte offset of level @p level inside a layer.
uint32_t BcnLevelOffset(const BcnArray *array, uint32_t level);

/// The byte offset of block (@p bx, @p by) of @p level in @p layer.
uint32_t BcnBlockOffset(const BcnArray *array, uint32_t layer, uint32_t level, uint32_t bx,
                        uint32_t by);

/// The blocks of @p level in each direction.
uint32_t BcnLevelBlocksX(const BcnArray *array, uint32_t level);
uint32_t BcnLevelBlocksY(const BcnArray *array, uint32_t level);

/// The BCn formats, as the decoder below takes them. Every block is 4x4 texels, little endian,
/// texel (x, y) number y * 4 + x.
typedef enum {
    BCN_BC1,       ///< 8 bytes: two RGB565 endpoints and two bit indices; a first endpoint not
                   ///< above the second has three colours and transparent black
    BCN_BC2,       ///< 16 bytes: a four bit alpha a texel, then a BC1 colour block of four colours
    BCN_BC3,       ///< 16 bytes: a BC4 alpha block, then a BC1 colour block of four colours
    BCN_BC4,       ///< 8 bytes: one channel, unsigned
    BCN_BC4_SNORM, ///< the same, signed
    BCN_BC5,       ///< 16 bytes: two BC4 blocks, red then green, unsigned
    BCN_BC5_SNORM, ///< the same, signed
    BCN_BC6H_UF,   ///< 16 bytes: HDR RGB as unsigned halves, alpha one
    BCN_BC6H_SF,   ///< the same bits, signed
    BCN_BC7,       ///< 16 bytes: RGBA, eight modes
    BCN_KINDS,
} BcnKind;

/// How BC1 to BC5's interpolated values are rounded, which the specification leaves to the
/// implementation; BC6H and BC7 decode one way under every rule.
typedef enum {
    /// NVIDIA's, the console's texture unit: BC1's red and blue from the five bit codes, its green
    /// from the eight bit ends by 80 of 256, BC3's alpha in 2048ths from the nearer end, BC4 in
    /// 257ths (258ths signed). What ScarletNX's BCn decoder test measured on an NVIDIA card.
    BCN_RULE_NVIDIA,
    /// The exact thirds, halves, sevenths and fifths of the eight bit ends, rounded to nearest.
    BCN_RULE_ROUNDED,
    /// The same, cut toward nought: Pillow's decoder, which the reference suite holds blocks to, and
    /// Mesa's; lavapipe lands between this and the rounded one on every texel of the pictures.
    BCN_RULE_TRUNCATED,
    /// The weights BPTC's two and three bit indices take (21 and 43 of 64 for the thirds, 9 to 55
    /// for the sevenths, 13 to 51 for the fifths), rounded: what RADV on an RX 6700 XT gives.
    BCN_RULE_SIXTYFOURTHS,
    /// Mesa's llvmpipe, which lavapipe samples with (26.2.6): BC1 to BC3's thirds as the first end
    /// and the difference times 85 or 170 of 256, cut toward minus infinity, and the midpoint
    /// rounded, which every colour channel of the bcnimage suite's BC1, BC2 and BC3 pictures and
    /// the bcn suite's blocks read; alphas and BC4 as BCN_RULE_TRUNCATED, since lavapipe's land
    /// between the cut and the rounded fractions on every texel of the pictures.
    BCN_RULE_LLVMPIPE,
    BCN_RULES,
} BcnRule;

/// The bytes of one block of @p kind.
uint32_t BcnBlockBytes(BcnKind kind);

/// Decodes one block. A texel's channels: bytes for BC1, BC2, BC3 and BC7; 16 bit UNORM, or SNORM
/// as an int16_t's bits, for BC4 (red, then nought, nought, all ones) and BC5 (red, green, nought,
/// all ones); half floats for BC6H, alpha 1.0. A BC6H block of a reserved mode is opaque black,
/// a BC7 block with no mode transparent black.
void BcnDecodeBlock(BcnKind kind, BcnRule rule, const uint8_t *block, uint16_t texels[16][4]);

/// What a channel @p value of @p kind (as BcnDecodeBlock gives it) reaches a UNORM8 target as: the
/// nearest byte, and in @p other (NULL for none) the integer on the other side of the value when
/// it lies between two, else the same byte: the conversion lets a machine write either (Vulkan
/// 1.3, 3.9.2; OpenGL's is the same), and RADV, which hands a fragment's colour to an eight bit
/// target through a half cut toward nought, writes the lower one for some. Bytes as they are,
/// 16 bit UNORM times 255 over 65535, a half clamped to [0, 1] times 255; the alpha of BC4, BC5
/// and BC6H is the target's 255.
uint8_t BcnTarget8(BcnKind kind, uint32_t channel, uint16_t value, uint8_t *other);

/// The float a half's bits stand for.
float BcnHalf(uint16_t half);

/// A BC4 block whose sixteen texels are all @p value: both endpoints @p value, every index 0.
void BcnSolidBc4(uint8_t block[8], uint8_t value);

#ifdef __cplusplus
}
#endif
