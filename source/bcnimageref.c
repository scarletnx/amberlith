// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "bcnimages.h"

#include <math.h>

#include "formatref.h"

const uint8_t *BcnImageBlock(const BcnImage *image, uint32_t x, uint32_t y) {
    const uint32_t across = BCN_IMAGE_SIZE / 4u;
    return image->blocks + BcnBlockBytes(image->kind) * ((y / 4u) * across + x / 4u);
}

size_t BcnReferenceDecode(BcnKind kind, const uint8_t *blocks, uint32_t width_blocks,
                       uint32_t height_blocks, uint8_t *out) {
    const uint32_t width = width_blocks * 4u;
    const bool halves = kind == BCN_BC6H_UF || kind == BCN_BC6H_SF;
    const uint32_t channels = kind == BCN_BC4 || kind == BCN_BC4_SNORM ? 1u
                              : kind == BCN_BC5 || kind == BCN_BC5_SNORM || halves ? 3u
                                                                                    : 4u;
    const uint32_t bytes = halves ? 2u : 1u;
    uint16_t texels[16][4];
    for (uint32_t by = 0; by < height_blocks; ++by) {
        for (uint32_t bx = 0; bx < width_blocks; ++bx) {
            BcnDecodeBlock(kind, BCN_RULE_TRUNCATED,
                           blocks + BcnBlockBytes(kind) * (by * width_blocks + bx), texels);
            for (uint32_t i = 0; i < 16u; ++i) {
                uint8_t *const texel =
                    out + bytes * channels * ((by * 4u + i / 4u) * width + bx * 4u + i % 4u);
                for (uint32_t c = 0; c < channels; ++c) {
                    const uint16_t v = texels[i][c];
                    switch (kind) {
                    case BCN_BC4:
                    case BCN_BC5:
                        // BCN_RULE_TRUNCATED's value is a byte widened, and blue is nought.
                        texel[c] = (uint8_t)(v / 257u);
                        break;
                    case BCN_BC6H_UF:
                    case BCN_BC6H_SF:
                        texel[2u * c] = (uint8_t)v;
                        texel[2u * c + 1u] = (uint8_t)(v >> 8);
                        break;
                    default:
                        texel[c] = (uint8_t)v;
                        break;
                    }
                }
            }
        }
    }
    return (size_t)bytes * channels * width * height_blocks * 4u;
}

bool BcnHasRules(BcnKind kind) {
    return kind != BCN_BC6H_UF && kind != BCN_BC6H_SF && kind != BCN_BC7;
}

/// The bytes channel @p c of a texel decoded to @p value reaches the target as, lowest and highest.
static void TargetBytes(const BcnImage *image, uint32_t c, uint16_t value, uint8_t *lo,
                        uint8_t *hi) {
    if (c == 3u && !image->alpha) {
        *lo = *hi = 255u;
        return;
    }
    if (image->srgb && c < 3u) {
        const double linear = FormatSrgbDecode((double)value / 255.0) * 255.0;
        *lo = (uint8_t)floor(linear);
        *hi = (uint8_t)ceil(linear);
        return;
    }
    uint8_t other = 0;
    const uint8_t near = BcnTarget8(image->kind, c, value, &other);
    *lo = near < other ? near : other;
    *hi = near > other ? near : other;
}

void BcnImageAnswer(const BcnImage *image, BcnImageAnswers *out) {
    static const BcnRule RULES[] = { BCN_RULE_NVIDIA, BCN_RULE_ROUNDED, BCN_RULE_TRUNCATED,
                                     BCN_RULE_SIXTYFOURTHS, BCN_RULE_LLVMPIPE };
    const uint32_t rules = BcnHasRules(image->kind) ? 5u : 1u;
    uint16_t texels[16][4];
    for (uint32_t by = 0; by < BCN_IMAGE_SIZE; by += 4u) {
        for (uint32_t bx = 0; bx < BCN_IMAGE_SIZE; bx += 4u) {
            const uint8_t *const block = BcnImageBlock(image, bx, by);
            for (uint32_t r = 0; r < rules; ++r) {
                BcnDecodeBlock(image->kind, RULES[r], block, texels);
                for (uint32_t t = 0; t < 16u; ++t) {
                    for (uint32_t c = 0; c < 4u; ++c) {
                        const uint32_t i = ((by + t / 4u) * BCN_IMAGE_SIZE + bx + t % 4u) * 4u + c;
                        uint8_t lo;
                        uint8_t hi;
                        TargetBytes(image, c, texels[t][c], &lo, &hi);
                        if (r == 0u) {
                            out->exact[0][i] = out->any[0][i] = lo;
                            out->exact[1][i] = out->any[1][i] = hi;
                        } else {
                            out->any[0][i] = lo < out->any[0][i] ? lo : out->any[0][i];
                            out->any[1][i] = hi > out->any[1][i] ? hi : out->any[1][i];
                        }
                    }
                }
            }
        }
    }
}

/// How far @p got is from [@p lo, @p hi].
static uint32_t Away(uint8_t got, uint8_t lo, uint8_t hi) {
    return got < lo ? (uint32_t)(lo - got) : got > hi ? (uint32_t)(got - hi) : 0u;
}

BcnTexelVerdict BcnImageVerdict(const BcnImageAnswers *answers, uint32_t x, uint32_t y,
                                const uint8_t got[4], uint32_t *away) {
    const uint32_t base = (y * BCN_IMAGE_SIZE + x) * 4u;
    BcnTexelVerdict verdict = BCN_TEXEL_EXACT;
    uint32_t farthest = 0;
    for (uint32_t c = 0; c < 4u; ++c) {
        const uint32_t from_exact = Away(got[c], answers->exact[0][base + c],
                                         answers->exact[1][base + c]);
        if (from_exact == 0u) {
            continue;
        }
        farthest = from_exact > farthest ? from_exact : farthest;
        if (Away(got[c], answers->any[0][base + c], answers->any[1][base + c]) != 0u) {
            verdict = BCN_TEXEL_WRONG;
        } else if (verdict == BCN_TEXEL_EXACT) {
            verdict = BCN_TEXEL_OFF;
        }
    }
    if (away != NULL) {
        *away = farthest;
    }
    return verdict;
}
