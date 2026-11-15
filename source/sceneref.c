// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string.h>

#include "sceneref.h"

const uint8_t SCENE_CLEAR[4] = { 0x20, 0x40, 0x60, 0xff };
const uint8_t SCENE_NEAR[4] = { 0xc0, 0x30, 0x90, 0xff };
const uint8_t SCENE_FAR[4] = { 0x05, 0xfa, 0x05, 0xff };
const uint8_t SCENE_PAINT[4] = { 0x11, 0x77, 0xdd, 0xff };
const uint8_t SCENE_SQUARE[4] = { 0xff, 0xee, 0x02, 0xff };

void SceneTexel(uint32_t x, uint32_t y, uint8_t out[4]) {
    // Red steps with the column and green with the row, so a transposed or mirrored texture
    // reads as one; blue numbers the texel, so no two share three bytes.
    out[0] = (uint8_t)(0x10u + x * 0x1cu);
    out[1] = (uint8_t)(0x18u + y * 0x1cu);
    out[2] = (uint8_t)(0x81u + y * SCENE_TEXTURE_SIZE + x);
    out[3] = 0xff;
}

void SceneDecoyTexel(uint32_t x, uint32_t y, uint8_t out[4]) {
    out[0] = (uint8_t)(0x0fu + y * 0x1cu);
    out[1] = (uint8_t)(0x17u + x * 0x1cu);
    out[2] = (uint8_t)(0x01u + y * SCENE_TEXTURE_SIZE + x);
    out[3] = 0xff;
}

void SceneExpected(uint32_t x, uint32_t y, uint8_t out[4]) {
    const bool left = x < SCENE_QUARTER;
    const bool top = y < SCENE_QUARTER;
    if (top && left) {
        const uint32_t pixels_a_texel = SCENE_QUARTER / SCENE_TEXTURE_SIZE;
        SceneTexel(x / pixels_a_texel, y / pixels_a_texel, out);
    } else if (top) {
        memcpy(out, SCENE_NEAR, 4);
    } else if (left) {
        memcpy(out, SCENE_PAINT, 4);
    } else if (x >= SCENE_SQUARE_AT && x < SCENE_SQUARE_AT + SCENE_SQUARE_SIDE &&
               y >= SCENE_SQUARE_AT && y < SCENE_SQUARE_AT + SCENE_SQUARE_SIDE) {
        memcpy(out, SCENE_SQUARE, 4);
    } else {
        memcpy(out, SCENE_CLEAR, 4);
    }
}

const char *SceneLooksLike(const uint8_t pixel[4]) {
    static const struct {
        const uint8_t *colour;
        const char *what;
    } FLAT[] = {
        { SCENE_CLEAR, "the clear colour" },
        { SCENE_NEAR, "the near quad of the top right quarter" },
        { SCENE_FAR, "the far quad the depth test hides" },
        { SCENE_PAINT, "the fragment uniform buffer's colour" },
        { SCENE_SQUARE, "the moved square" },
    };
    for (size_t i = 0; i < sizeof(FLAT) / sizeof(FLAT[0]); ++i) {
        if (memcmp(pixel, FLAT[i].colour, 4) == 0) {
            return FLAT[i].what;
        }
    }
    for (uint32_t y = 0; y < SCENE_TEXTURE_SIZE; ++y) {
        for (uint32_t x = 0; x < SCENE_TEXTURE_SIZE; ++x) {
            uint8_t texel[4];
            SceneTexel(x, y, texel);
            if (memcmp(pixel, texel, 4) == 0) {
                return "a texel of the texture";
            }
            SceneDecoyTexel(x, y, texel);
            if (memcmp(pixel, texel, 4) == 0) {
                return "a texel of the decoy in texture slot 0";
            }
        }
    }
    return "nothing the scene draws";
}
