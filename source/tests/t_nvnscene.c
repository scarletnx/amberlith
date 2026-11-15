// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <string.h>

#include "../check.h"
#include "../drawpath.h"
#include "../picture.h"
#include "../report.h"
#include "../sceneref.h"
#include "../shaders.h"
#include "tests.h"

// The nvn suite's scene: one list of five draws, recorded once (Draw, below) and run down each
// of the program's ways to the graphics unit: deko3d's calls, the layer's own NVN functions, and
// the NVN the program looks up, each judged by the same function against the same pixels
// (sceneref.c). What a way has to get right for
// them: vertex attributes, a program of two stages, a uniform buffer in each stage that changes
// between draws, a texture found through its slot's handle with another texture in the slot
// before it, and a depth test.
//
// Exact tier: the colours are bytes over 255 that no arithmetic touches, the texture is sampled
// nearest at pixel centres far from a texel's edge, and every edge of every quad is on a pixel
// boundary, so no pixel's coverage is in doubt.

static const char *const SUITE = "nvn";

#define QUADS 5u
#define VERTICES (QUADS * 6u)
// The fragment stage's uniform buffer: a colour, then where the colour comes from.
#define PAINT_TEXTURE 0
#define PAINT_VERTEX 1
#define PAINT_BUFFER 2

typedef struct {
    float color[4];
    int32_t mode[4];
} Paint;

typedef struct {
    float offset[4];
} Move;

/// Clip space for a pixel boundary: plus y is the top of the target, as in every drawing suite.
static float ClipX(uint32_t pixel) {
    return (float)pixel * 2.0f / (float)SCENE_SIZE - 1.0f;
}

static float ClipY(uint32_t pixel) {
    return 1.0f - (float)pixel * 2.0f / (float)SCENE_SIZE;
}

/// Two triangles over the pixels from (@p x0, @p y0) to (@p x1, @p y1), at depth @p z, carrying
/// the whole texture and @p colour.
static void Quad(GpuVertex out[6], uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, float z,
                 const uint8_t colour[4]) {
    const float r = (float)colour[0] / 255.0f;
    const float g = (float)colour[1] / 255.0f;
    const float b = (float)colour[2] / 255.0f;
    const float a = (float)colour[3] / 255.0f;
    const GpuVertex top_left = { ClipX(x0), ClipY(y0), z, 0.0f, 0.0f, r, g, b, a };
    const GpuVertex top_right = { ClipX(x1), ClipY(y0), z, 1.0f, 0.0f, r, g, b, a };
    const GpuVertex bottom_right = { ClipX(x1), ClipY(y1), z, 1.0f, 1.0f, r, g, b, a };
    const GpuVertex bottom_left = { ClipX(x0), ClipY(y1), z, 0.0f, 1.0f, r, g, b, a };
    out[0] = top_left;
    out[1] = top_right;
    out[2] = bottom_right;
    out[3] = top_left;
    out[4] = bottom_right;
    out[5] = bottom_left;
}

/// Builds the scene on @p path and draws it; false with the step that failed in @p why.
static bool Draw(DrawPath *path, char *why, size_t why_size) {
    const int program = DrawPathProgram(path, ShaderData(SHADER_SCENE_VERT),
                                        ShaderSize(SHADER_SCENE_VERT),
                                        ShaderData(SHADER_SCENE_FRAG),
                                        ShaderSize(SHADER_SCENE_FRAG));
    uint8_t texels[SCENE_TEXTURE_SIZE * SCENE_TEXTURE_SIZE * 4u];
    uint8_t decoy[SCENE_TEXTURE_SIZE * SCENE_TEXTURE_SIZE * 4u];
    for (uint32_t y = 0; y < SCENE_TEXTURE_SIZE; ++y) {
        for (uint32_t x = 0; x < SCENE_TEXTURE_SIZE; ++x) {
            SceneTexel(x, y, texels + (y * SCENE_TEXTURE_SIZE + x) * 4u);
            SceneDecoyTexel(x, y, decoy + (y * SCENE_TEXTURE_SIZE + x) * 4u);
        }
    }
    const int decoy_texture =
        DrawPathTexture(path, SCENE_TEXTURE_SIZE, SCENE_TEXTURE_SIZE, decoy, false);
    const int texture =
        DrawPathTexture(path, SCENE_TEXTURE_SIZE, SCENE_TEXTURE_SIZE, texels, false);
    DrawBuffer vertices;
    DrawBuffer moves[2];
    DrawBuffer paints[3];
    bool ok = program >= 0 && texture >= 0 && decoy_texture >= 0 &&
              DrawPathAlloc(path, VERTICES * (uint32_t)sizeof(GpuVertex), &vertices);
    for (int i = 0; ok && i < 2; ++i) {
        ok = DrawPathAlloc(path, (uint32_t)sizeof(Move), &moves[i]);
    }
    for (int i = 0; ok && i < 3; ++i) {
        ok = DrawPathAlloc(path, (uint32_t)sizeof(Paint), &paints[i]);
    }
    if (!ok) {
        snprintf(why, why_size, "building the scene: %s", DrawPathFailure(path));
        return false;
    }

    GpuVertex quads[VERTICES];
    static const uint8_t white[4] = { 0xff, 0xff, 0xff, 0xff };
    const uint32_t q = SCENE_QUARTER;
    Quad(quads + 0, 0, 0, q, q, 0.5f, white);
    Quad(quads + 6, q, 0, SCENE_SIZE, q, 0.25f, SCENE_NEAR);
    Quad(quads + 12, q, 0, SCENE_SIZE, q, 0.75f, SCENE_FAR);
    Quad(quads + 18, 0, q, q, SCENE_SIZE, 0.5f, white);
    // The square, at the target's top left corner until the vertex stage's buffer moves it.
    Quad(quads + 24, 0, 0, SCENE_SQUARE_SIDE, SCENE_SQUARE_SIDE, 0.5f, SCENE_SQUARE);
    memcpy(vertices.bytes, quads, sizeof(quads));

    const Move still = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    const float step = (float)SCENE_SQUARE_AT * 2.0f / (float)SCENE_SIZE;
    const Move moved = { { step, -step, 0.0f, 0.0f } };
    memcpy(moves[0].bytes, &still, sizeof(still));
    memcpy(moves[1].bytes, &moved, sizeof(moved));
    for (int mode = 0; mode < 3; ++mode) {
        // The colour is the buffer's in every mode, so a stage that ignores the mode shows it.
        const Paint paint = { { (float)SCENE_PAINT[0] / 255.0f, (float)SCENE_PAINT[1] / 255.0f,
                                (float)SCENE_PAINT[2] / 255.0f, (float)SCENE_PAINT[3] / 255.0f },
                              { mode, 0, 0, 0 } };
        memcpy(paints[mode].bytes, &paint, sizeof(paint));
    }

    const float clear[4] = { (float)SCENE_CLEAR[0] / 255.0f, (float)SCENE_CLEAR[1] / 255.0f,
                             (float)SCENE_CLEAR[2] / 255.0f, (float)SCENE_CLEAR[3] / 255.0f };
    DrawPathBegin(path);
    DrawPathClear(path, clear);
    DrawPathClearDepth(path, 1.0f);
    DrawPathUseProgram(path, program);
    DrawPathVertices(path, &vertices, sizeof(quads));
    DrawPathBindTexture(path, DRAW_STAGE_FRAGMENT, 0, decoy_texture);
    DrawPathBindTexture(path, DRAW_STAGE_FRAGMENT, 1, texture);

    DrawPathUniform(path, DRAW_STAGE_VERTEX, 0, &moves[0], sizeof(Move));
    DrawPathUniform(path, DRAW_STAGE_FRAGMENT, 0, &paints[PAINT_TEXTURE], sizeof(Paint));
    DrawPathDraw(path, 0, 6);

    DrawPathDepth(path, true, true);
    DrawPathUniform(path, DRAW_STAGE_FRAGMENT, 0, &paints[PAINT_VERTEX], sizeof(Paint));
    DrawPathDraw(path, 6, 6);
    DrawPathDraw(path, 12, 6);
    DrawPathDepth(path, false, false);

    DrawPathUniform(path, DRAW_STAGE_FRAGMENT, 0, &paints[PAINT_BUFFER], sizeof(Paint));
    DrawPathDraw(path, 18, 6);

    DrawPathUniform(path, DRAW_STAGE_VERTEX, 0, &moves[1], sizeof(Move));
    DrawPathUniform(path, DRAW_STAGE_FRAGMENT, 0, &paints[PAINT_VERTEX], sizeof(Paint));
    DrawPathDraw(path, 24, 6);
    if (!DrawPathEnd(path)) {
        snprintf(why, why_size, "drawing the scene: %s", DrawPathFailure(path));
        return false;
    }
    return true;
}

/// One way's case: the scene drawn down @p kind and every pixel judged.
static void SceneCase(DrawPathKind kind, const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    DrawPathSpec spec;
    memset(&spec, 0, sizeof(spec));
    spec.width = SCENE_SIZE;
    spec.height = SCENE_SIZE;
    spec.depth = true;
    spec.programs = 1;
    spec.textures = 2;
    spec.data_bytes = 0x4000u;
    spec.texture_bytes = 2u * SCENE_TEXTURE_SIZE * SCENE_TEXTURE_SIZE * 4u;
    spec.command_bytes = 0x10000u;
    char why[320] = "";
    DrawPath *const path = DrawPathOpen(kind, &spec, why, sizeof(why));
    if (path == NULL) {
        char detail[400];
        snprintf(detail, sizeof(detail), "the way through %s did not come up: %s",
                 DrawPathName(kind), why);
        ReportCase(SUITE, name, CHECK_NONE, false, detail);
        return;
    }
    const uint8_t *const pixels = Draw(path, why, sizeof(why)) ? DrawPathRead(path) : NULL;
    if (pixels == NULL) {
        char detail[400];
        snprintf(detail, sizeof(detail), "%s",
                 why[0] != '\0' ? why : DrawPathFailure(path));
        ReportCase(SUITE, name, CHECK_NONE, false, detail);
        DrawPathClose(path);
        return;
    }

    Picture *const picture = PictureBegin(SCENE_SIZE, SCENE_SIZE, PICTURE_COLOUR, pixels);
    uint32_t wrong = 0;
    uint32_t first_x = 0;
    uint32_t first_y = 0;
    uint8_t first[4] = { 0, 0, 0, 0 };
    uint8_t first_want[4] = { 0, 0, 0, 0 };
    for (uint32_t y = 0; y < SCENE_SIZE; ++y) {
        for (uint32_t x = 0; x < SCENE_SIZE; ++x) {
            uint8_t want[4];
            SceneExpected(x, y, want);
            const uint8_t *const pixel = GpuPixel(pixels, SCENE_SIZE, x, y);
            const bool right = memcmp(pixel, want, 4) == 0;
            PictureSet(picture, x, y, want, right ? PICTURE_MATCHED : PICTURE_WRONG);
            if (right) {
                continue;
            }
            if (wrong == 0u) {
                first_x = x;
                first_y = y;
                memcpy(first, pixel, 4);
                memcpy(first_want, want, 4);
            }
            ++wrong;
        }
    }
    CHECK_TRUE(SUITE, name, CHECK_EXACT, wrong == 0u,
               "%u of %u pixels wrong; the first, (%u,%u), holds %02x%02x%02x%02x, expected "
               "%02x%02x%02x%02x: it holds %s",
               (unsigned)wrong, (unsigned)(SCENE_SIZE * SCENE_SIZE), (unsigned)first_x,
               (unsigned)first_y, first[0], first[1], first[2], first[3], first_want[0],
               first_want[1], first_want[2], first_want[3], SceneLooksLike(first));
    DrawPathClose(path);
}

void NvnSceneFirst(void) {
    SceneCase(DRAW_PATH_DEKO3D,
              "a scene of five draws through deko3d holds what the processor works out");
    SceneCase(DRAW_PATH_LAYER,
              "the same scene through the layer's own NVN functions holds what the processor "
              "works out");
}

void NvnSceneLast(void) {
    SceneCase(DRAW_PATH_NVN,
              "the same scene through the NVN the program looks up holds what the processor "
              "works out");
}
