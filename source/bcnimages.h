// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bcnref.h"

#ifdef __cplusplus
extern "C" {
#endif

/// The astcimage suite's picture in every BCn format, and one block of every BC6H and BC7 mode.
///
/// tools/bcn-images.py takes the picture from tools/astc-images.py, the same 128x128 bytes, and
/// encodes it as a game's tools would: etcpak 0.9.15 for BC1, BC3, BC4 (red), BC5 (red and
/// green) and BC7 (its bc7enc, which keeps to modes 1, 5 and 6; perceptual weights for the sRGB
/// picture), Pillow 12.3.0 for BC2. No encoder here writes BC7's other modes or BC6H, so the
/// script encodes two more itself, a block's mode chosen in turn: BC7 block n in mode n % 8, its
/// partition, rotation and index selection in turn too, and BC6H block n in mode n % 14 of the
/// table's order, its partition in turn. BC1's, BC2's and BC3's sRGB pictures are their linear
/// blocks, BC1's RGB one too. The hashes are FNV-1a over what Pillow 12.3.0 decodes the blocks to
/// (BcnReferenceDecode), and for BC6H over the halves bcdec 0.985 (imagecodecs 2026.11.11)
/// decodes them to, since Pillow takes BC6H to bytes interpolating without the specification's
/// rounding; the reference suite holds source/bcnref.c to them.
typedef struct {
    const char *name;
    BcnKind kind;
    bool srgb;
    /// False for BC1's RGB format, whose alpha is one whatever the block says.
    bool alpha;
    /// Row major, a row of blocks after another, as a DDS file holds them.
    const uint8_t *blocks;
    uint32_t block_count;
    uint32_t pillow_hash;
} BcnImage;

#define BCN_IMAGE_SIZE 128u
#define BCN_IMAGES 13u
/// The pictures the bcnimage suite's large textures repeat, BC1's and BC7's, which the reference
/// suite checks are there.
#define BCN_IMAGE_BC1 0u
#define BCN_IMAGE_BC7 7u

extern const BcnImage BCN_IMAGE[BCN_IMAGES];

/// FNV-1a over the picture the encoders were given: ASTC_IMAGE_SOURCE_HASH, the same picture.
extern const uint32_t BCN_IMAGE_SOURCE_HASH;

/// One block of a mode: BC7's eight and BC6H's fourteen (the table's order, which is the
/// specification's mode 1 to 14), then BC6H's four reserved mode codes, each block's bits after
/// its mode from a fixed hash. A BC6H block is decoded both ways, unsigned and signed: its two
/// hashes are bcdec's decode as BC6HU and as BC6HS; a BC7 block's second hash is nought.
typedef struct {
    const char *name;
    BcnKind kind;
    uint8_t bytes[16];
    uint32_t pillow_hash;
    uint32_t pillow_hash_signed;
} BcnModeBlock;

#define BCN_MODE_BLOCKS 26u

extern const BcnModeBlock BCN_MODE_BLOCK[BCN_MODE_BLOCKS];

/// The block of @p image that holds texel (@p x, @p y).
const uint8_t *BcnImageBlock(const BcnImage *image, uint32_t x, uint32_t y);

/// The bytes the reference suite's outside decoder gives for @p width_blocks by @p height_blocks
/// blocks of @p kind, rows of texels after each other: Pillow's RGBA (BC1, BC2, BC3, BC7), L (BC4)
/// and RGB with blue nought (BC5), BC1 to BC5 under BCN_RULE_TRUNCATED; bcdec's halves for BC6H,
/// signed or not, red, green and blue, each little endian. Returns the bytes written to @p out,
/// which holds width * height * 6.
size_t BcnReferenceDecode(BcnKind kind, const uint8_t *blocks, uint32_t width_blocks,
                       uint32_t height_blocks, uint8_t *out);

/// What each channel of a picture may be read back as on a UNORM8 target: the console's bytes
/// (lowest and highest: a value between two integers, a BC4 or BC5 one, a half or an sRGB code
/// turned linear, may be written as either, BcnTarget8),
/// and the lowest and highest any known GPU's interpolants give (every BcnRule for BC1 to BC5;
/// BC6H and BC7 decode one way).
/// RGBA a texel, rows of the picture after each other.
#define BCN_IMAGE_CHANNELS (BCN_IMAGE_SIZE * BCN_IMAGE_SIZE * 4u)
typedef struct {
    uint8_t exact[2][BCN_IMAGE_CHANNELS];
    uint8_t any[2][BCN_IMAGE_CHANNELS];
} BcnImageAnswers;

/// Whether @p kind leaves its interpolants to the implementation: BC1 to BC5.
bool BcnHasRules(BcnKind kind);

/// The answers for every texel of @p image.
void BcnImageAnswer(const BcnImage *image, BcnImageAnswers *out);

typedef enum {
    /// Every channel one of the console's bytes.
    BCN_TEXEL_EXACT,
    /// A channel off the console's bytes but within another GPU's interpolants.
    BCN_TEXEL_OFF,
    /// A channel outside every known decode.
    BCN_TEXEL_WRONG,
} BcnTexelVerdict;

/// The verdict on @p got read for texel (@p x, @p y); @p away gets how far its farthest channel is
/// from the console's bytes.
BcnTexelVerdict BcnImageVerdict(const BcnImageAnswers *answers, uint32_t x, uint32_t y,
                                const uint8_t got[4], uint32_t *away);

#ifdef __cplusplus
}
#endif
