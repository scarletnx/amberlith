// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../check.h"
#include "../report.h"
#include "../sceneref.h"
#include "tests.h"

// The nvn suite's scene, as sceneref.c expects it: what each part of the picture holds, counted
// here pixel by pixel from the scene's description rather than from the function's own branches,
// and what a wrong pixel is said to look like.

static const char *const SUITE = "reference";

static bool Same(const uint8_t a[4], const uint8_t b[4]) {
    return memcmp(a, b, 4) == 0;
}

static void CheckPicture(void) {
    uint32_t texture = 0;
    uint32_t near = 0;
    uint32_t far = 0;
    uint32_t paint = 0;
    uint32_t square = 0;
    uint32_t clear = 0;
    uint32_t other = 0;
    uint32_t misplaced = 0;
    for (uint32_t y = 0; y < SCENE_SIZE; ++y) {
        for (uint32_t x = 0; x < SCENE_SIZE; ++x) {
            uint8_t got[4];
            SceneExpected(x, y, got);
            const uint32_t quarter =
                (y >= SCENE_QUARTER ? 2u : 0u) + (x >= SCENE_QUARTER ? 1u : 0u);
            uint8_t texel[4];
            SceneTexel(x * SCENE_TEXTURE_SIZE / SCENE_QUARTER % SCENE_TEXTURE_SIZE,
                       y * SCENE_TEXTURE_SIZE / SCENE_QUARTER % SCENE_TEXTURE_SIZE, texel);
            const bool in_square = x >= 40u && x <= 55u && y >= 40u && y <= 55u;
            if (quarter == 0u && Same(got, texel)) {
                ++texture;
            } else if (Same(got, SCENE_NEAR)) {
                ++near;
                misplaced += quarter != 1u;
            } else if (Same(got, SCENE_FAR)) {
                ++far;
            } else if (Same(got, SCENE_PAINT)) {
                ++paint;
                misplaced += quarter != 2u;
            } else if (Same(got, SCENE_SQUARE)) {
                ++square;
                misplaced += !in_square;
            } else if (Same(got, SCENE_CLEAR)) {
                ++clear;
                misplaced += quarter != 3u || in_square;
            } else {
                ++other;
            }
        }
    }
    CHECK_TRUE(SUITE, "scene: the four quarters, the hidden quad and the moved square are where "
                      "the scene's description puts them",
               CHECK_EXACT,
               texture == 1024u && near == 1024u && far == 0u && paint == 1024u &&
                   square == 256u && clear == 768u && other == 0u && misplaced == 0u,
               "texture %u, near %u, far %u, paint %u, square %u, clear %u, other %u, misplaced "
               "%u, where a 64x64 scene has 1024, 1024, 0, 1024, 256, 768, 0 and 0",
               (unsigned)texture, (unsigned)near, (unsigned)far, (unsigned)paint,
               (unsigned)square, (unsigned)clear, (unsigned)other, (unsigned)misplaced);
}

static void CheckNames(void) {
    // Every colour and texel differs from every other, or a wrong pixel could not be named.
    uint8_t all[5u + 2u * SCENE_TEXTURE_SIZE * SCENE_TEXTURE_SIZE][4];
    size_t count = 0;
    memcpy(all[count++], SCENE_CLEAR, 4);
    memcpy(all[count++], SCENE_NEAR, 4);
    memcpy(all[count++], SCENE_FAR, 4);
    memcpy(all[count++], SCENE_PAINT, 4);
    memcpy(all[count++], SCENE_SQUARE, 4);
    for (uint32_t y = 0; y < SCENE_TEXTURE_SIZE; ++y) {
        for (uint32_t x = 0; x < SCENE_TEXTURE_SIZE; ++x) {
            SceneTexel(x, y, all[count++]);
            SceneDecoyTexel(x, y, all[count++]);
        }
    }
    uint32_t twins = 0;
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = 0; j < i; ++j) {
            twins += Same(all[i], all[j]);
        }
    }
    uint8_t texel[4];
    uint8_t decoy[4];
    SceneTexel(5, 2, texel);
    SceneDecoyTexel(5, 2, decoy);
    static const uint8_t stray[4] = { 0x01, 0x02, 0x03, 0x04 };
    const bool named =
        strcmp(SceneLooksLike(SCENE_FAR), "the far quad the depth test hides") == 0 &&
        strcmp(SceneLooksLike(SCENE_CLEAR), "the clear colour") == 0 &&
        strcmp(SceneLooksLike(SCENE_SQUARE), "the moved square") == 0 &&
        strcmp(SceneLooksLike(texel), "a texel of the texture") == 0 &&
        strcmp(SceneLooksLike(decoy), "a texel of the decoy in texture slot 0") == 0 &&
        strcmp(SceneLooksLike(stray), "nothing the scene draws") == 0;
    CHECK_TRUE(SUITE, "scene: no two of its colours and texels are alike, and a wrong pixel is "
                      "named by the part it shows",
               CHECK_EXACT, twins == 0u && named,
               "%u pairs alike among %zu; the far quad reads \"%s\", a texel \"%s\", a decoy "
               "texel \"%s\", a stray pixel \"%s\"",
               (unsigned)twins, count, SceneLooksLike(SCENE_FAR), SceneLooksLike(texel),
               SceneLooksLike(decoy), SceneLooksLike(stray));
}

void RunReferenceScene(void) {
    CheckPicture();
    CheckNames();
}
