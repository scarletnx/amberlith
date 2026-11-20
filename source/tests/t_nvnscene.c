// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <string.h>

#include <nvnlayer.h>

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

// ---- a texture past its edges

// The second scene: one 4 by 4 texture drawn in each quarter of the target over coordinates
// from -1 to 3, so four of it across and down, two pixels a texel, every pixel centre a
// quarter of a texel from a texel's edge. Each quarter samples it nearest through a sampler of
// its own wrap mode: it repeats (top left), repeats mirrored (top right), holds its edge
// (bottom left) and shows its border colour (bottom right). No filter weighs anything, so each
// pixel is one texel's bytes or the border's.

#define WRAP_TEXTURE_SIZE 4u
#define WRAP_QUADS 4u
/// Opaque green, which no texel of the scene's texture is.
static const uint8_t WRAP_BORDER[4] = { 0x00, 0xff, 0x00, 0xff };
static const DrawWrap WRAPS[WRAP_QUADS] = { DRAW_WRAP_REPEAT, DRAW_WRAP_MIRRORED_REPEAT,
                                            DRAW_WRAP_CLAMP_TO_EDGE, DRAW_WRAP_CLAMP_TO_BORDER };

/// The texel of the 4 by 4 texture a coordinate @p at texels from its first lands on under
/// @p wrap, or -1 for the border.
static int WrapTexel(DrawWrap wrap, int at) {
    const int size = (int)WRAP_TEXTURE_SIZE;
    const int tile = at >= 0 ? at / size : -((size - 1 - at) / size);
    const int within = at - tile * size;
    switch (wrap) {
    case DRAW_WRAP_REPEAT:
        return within;
    case DRAW_WRAP_MIRRORED_REPEAT:
        return (tile & 1) != 0 ? size - 1 - within : within;
    case DRAW_WRAP_CLAMP_TO_EDGE:
        return at < 0 ? 0 : at >= size ? size - 1 : at;
    default:
        return at < 0 || at >= size ? -1 : at;
    }
}

/// The four bytes pixel @p x, @p y of the wrap scene holds, the top row first.
static void WrapExpected(uint32_t x, uint32_t y, uint8_t out[4]) {
    const uint32_t quad = (y / SCENE_QUARTER) * 2u + x / SCENE_QUARTER;
    // Two pixels a texel, the quarter's first pixel four texels before the texture's first.
    const int u = WrapTexel(WRAPS[quad], (int)(x % SCENE_QUARTER) / 2 - (int)WRAP_TEXTURE_SIZE);
    const int v = WrapTexel(WRAPS[quad], (int)(y % SCENE_QUARTER) / 2 - (int)WRAP_TEXTURE_SIZE);
    if (u < 0 || v < 0) {
        memcpy(out, WRAP_BORDER, 4);
    } else {
        SceneTexel((uint32_t)u, (uint32_t)v, out);
    }
}

static const char *WrapLooksLike(const uint8_t pixel[4]) {
    if (memcmp(pixel, WRAP_BORDER, 4) == 0) {
        return "the border colour";
    }
    if (memcmp(pixel, SCENE_CLEAR, 4) == 0) {
        return "the clear colour: nothing drawn";
    }
    for (uint32_t y = 0; y < WRAP_TEXTURE_SIZE; ++y) {
        for (uint32_t x = 0; x < WRAP_TEXTURE_SIZE; ++x) {
            uint8_t texel[4];
            SceneTexel(x, y, texel);
            if (memcmp(pixel, texel, 4) == 0) {
                return "another texel of the texture: another wrap mode";
            }
        }
    }
    return "nothing the scene draws";
}

/// Builds the wrap scene on @p path and draws it; false with the step that failed in @p why.
static bool WrapDraw(DrawPath *path, char *why, size_t why_size) {
    const int program = DrawPathProgram(path, ShaderData(SHADER_SCENE_VERT),
                                        ShaderSize(SHADER_SCENE_VERT),
                                        ShaderData(SHADER_SCENE_FRAG),
                                        ShaderSize(SHADER_SCENE_FRAG));
    uint8_t texels[WRAP_TEXTURE_SIZE * WRAP_TEXTURE_SIZE * 4u];
    for (uint32_t y = 0; y < WRAP_TEXTURE_SIZE; ++y) {
        for (uint32_t x = 0; x < WRAP_TEXTURE_SIZE; ++x) {
            SceneTexel(x, y, texels + (y * WRAP_TEXTURE_SIZE + x) * 4u);
        }
    }
    int textures[WRAP_QUADS];
    bool ok = program >= 0;
    for (uint32_t i = 0; i < WRAP_QUADS; ++i) {
        const DrawSampler sampler = { false,
                                      WRAPS[i],
                                      { (float)WRAP_BORDER[0] / 255.0f,
                                        (float)WRAP_BORDER[1] / 255.0f,
                                        (float)WRAP_BORDER[2] / 255.0f,
                                        (float)WRAP_BORDER[3] / 255.0f } };
        textures[i] = DrawPathTextureSampled(path, WRAP_TEXTURE_SIZE, WRAP_TEXTURE_SIZE, texels,
                                             &sampler);
        ok = ok && textures[i] >= 0;
    }
    DrawBuffer vertices, move, paint;
    ok = ok && DrawPathAlloc(path, WRAP_QUADS * 6u * (uint32_t)sizeof(GpuVertex), &vertices) &&
         DrawPathAlloc(path, (uint32_t)sizeof(Move), &move) &&
         DrawPathAlloc(path, (uint32_t)sizeof(Paint), &paint);
    if (!ok) {
        snprintf(why, why_size, "building the scene: %s", DrawPathFailure(path));
        return false;
    }
    GpuVertex quads[WRAP_QUADS * 6u];
    static const uint8_t white[4] = { 0xff, 0xff, 0xff, 0xff };
    for (uint32_t i = 0; i < WRAP_QUADS; ++i) {
        const uint32_t x0 = (i % 2u) * SCENE_QUARTER, y0 = (i / 2u) * SCENE_QUARTER;
        Quad(quads + i * 6u, x0, y0, x0 + SCENE_QUARTER, y0 + SCENE_QUARTER, 0.5f, white);
        // Quad lays the whole texture over its pixels: these run a texture's width before it
        // and two past it instead.
        for (uint32_t k = 0; k < 6u; ++k) {
            quads[i * 6u + k].u = quads[i * 6u + k].u * 4.0f - 1.0f;
            quads[i * 6u + k].v = quads[i * 6u + k].v * 4.0f - 1.0f;
        }
    }
    memcpy(vertices.bytes, quads, sizeof(quads));
    const Move still = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    const Paint sampled = { { 1.0f, 1.0f, 1.0f, 1.0f }, { PAINT_TEXTURE, 0, 0, 0 } };
    memcpy(move.bytes, &still, sizeof(still));
    memcpy(paint.bytes, &sampled, sizeof(sampled));

    const float clear[4] = { (float)SCENE_CLEAR[0] / 255.0f, (float)SCENE_CLEAR[1] / 255.0f,
                             (float)SCENE_CLEAR[2] / 255.0f, (float)SCENE_CLEAR[3] / 255.0f };
    DrawPathBegin(path);
    DrawPathClear(path, clear);
    DrawPathUseProgram(path, program);
    DrawPathVertices(path, &vertices, sizeof(quads));
    DrawPathUniform(path, DRAW_STAGE_VERTEX, 0, &move, sizeof(Move));
    DrawPathUniform(path, DRAW_STAGE_FRAGMENT, 0, &paint, sizeof(Paint));
    for (uint32_t i = 0; i < WRAP_QUADS; ++i) {
        // The shader reads slot 1; slot 0 holds the texture under another wrap mode.
        DrawPathBindTexture(path, DRAW_STAGE_FRAGMENT, 0, textures[(i + 1u) % WRAP_QUADS]);
        DrawPathBindTexture(path, DRAW_STAGE_FRAGMENT, 1, textures[i]);
        DrawPathDraw(path, i * 6u, 6);
    }
    if (!DrawPathEnd(path)) {
        snprintf(why, why_size, "drawing the scene: %s", DrawPathFailure(path));
        return false;
    }
    return true;
}

// ---- indexed draws

// The third scene: a quad of one flat colour in each quarter, every quad four vertices and six
// indices. The quarters' vertices follow one another in one vertex buffer, and each quarter is
// drawn another way: indices of two bytes (top left), of four (top right), of one (bottom left),
// and the top left quarter's own indices with a base vertex of twelve (bottom right), which
// without the base draw the top left quad again and leave the quarter the clear colour.

#define INDEXED_QUADS 4u
static const uint8_t *const INDEXED_COLOURS[INDEXED_QUADS] = { SCENE_NEAR, SCENE_FAR, SCENE_PAINT,
                                                               SCENE_SQUARE };

static void IndexedExpected(uint32_t x, uint32_t y, uint8_t out[4]) {
    memcpy(out, INDEXED_COLOURS[(y / SCENE_QUARTER) * 2u + x / SCENE_QUARTER], 4);
}

static const char *IndexedLooksLike(const uint8_t pixel[4]) {
    static const char *const NAMES[INDEXED_QUADS] = {
        "the top left quad's colour (indices of two bytes)",
        "the top right quad's colour (indices of four bytes)",
        "the bottom left quad's colour (indices of one byte)",
        "the bottom right quad's colour (a base vertex)",
    };
    for (uint32_t i = 0; i < INDEXED_QUADS; ++i) {
        if (memcmp(pixel, INDEXED_COLOURS[i], 4) == 0) {
            return NAMES[i];
        }
    }
    return memcmp(pixel, SCENE_CLEAR, 4) == 0 ? "the clear colour: nothing drawn"
                                              : "nothing the scene draws";
}

static bool IndexedDraw(DrawPath *path, char *why, size_t why_size) {
    const int program = DrawPathProgram(path, ShaderData(SHADER_SCENE_VERT),
                                        ShaderSize(SHADER_SCENE_VERT),
                                        ShaderData(SHADER_SCENE_FRAG),
                                        ShaderSize(SHADER_SCENE_FRAG));
    DrawBuffer vertices, move, paint, shorts, words, bytes;
    const bool ok =
        program >= 0 &&
        DrawPathAlloc(path, INDEXED_QUADS * 4u * (uint32_t)sizeof(GpuVertex), &vertices) &&
        DrawPathAlloc(path, (uint32_t)sizeof(Move), &move) &&
        DrawPathAlloc(path, (uint32_t)sizeof(Paint), &paint) &&
        DrawPathAlloc(path, 6u * 2u, &shorts) && DrawPathAlloc(path, 6u * 4u, &words) &&
        DrawPathAlloc(path, 6u, &bytes);
    if (!ok) {
        snprintf(why, why_size, "building the scene: %s", DrawPathFailure(path));
        return false;
    }
    // A quad's corners, as Quad lays them, by where each first comes among its six vertices:
    // top left, top right, bottom right, bottom left.
    static const uint8_t CORNERS[4] = { 0, 1, 2, 5 };
    static const uint8_t ORDER[6] = { 0, 1, 2, 0, 2, 3 };
    GpuVertex corners[INDEXED_QUADS * 4u];
    for (uint32_t i = 0; i < INDEXED_QUADS; ++i) {
        const uint32_t x0 = (i % 2u) * SCENE_QUARTER, y0 = (i / 2u) * SCENE_QUARTER;
        GpuVertex quad[6];
        Quad(quad, x0, y0, x0 + SCENE_QUARTER, y0 + SCENE_QUARTER, 0.5f, INDEXED_COLOURS[i]);
        for (uint32_t k = 0; k < 4u; ++k) {
            corners[i * 4u + k] = quad[CORNERS[k]];
        }
    }
    memcpy(vertices.bytes, corners, sizeof(corners));
    uint16_t two[6];
    uint32_t four[6];
    uint8_t one[6];
    for (uint32_t k = 0; k < 6u; ++k) {
        two[k] = ORDER[k];                   // the top left quad's, and with a base the fourth's
        four[k] = 4u + ORDER[k];             // the top right quad's
        one[k] = (uint8_t)(8u + ORDER[k]);   // the bottom left quad's
    }
    memcpy(shorts.bytes, two, sizeof(two));
    memcpy(words.bytes, four, sizeof(four));
    memcpy(bytes.bytes, one, sizeof(one));
    const Move still = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    const Paint flat = { { 1.0f, 1.0f, 1.0f, 1.0f }, { PAINT_VERTEX, 0, 0, 0 } };
    memcpy(move.bytes, &still, sizeof(still));
    memcpy(paint.bytes, &flat, sizeof(flat));

    const float clear[4] = { (float)SCENE_CLEAR[0] / 255.0f, (float)SCENE_CLEAR[1] / 255.0f,
                             (float)SCENE_CLEAR[2] / 255.0f, (float)SCENE_CLEAR[3] / 255.0f };
    DrawPathBegin(path);
    DrawPathClear(path, clear);
    DrawPathUseProgram(path, program);
    DrawPathVertices(path, &vertices, sizeof(corners));
    DrawPathUniform(path, DRAW_STAGE_VERTEX, 0, &move, sizeof(Move));
    DrawPathUniform(path, DRAW_STAGE_FRAGMENT, 0, &paint, sizeof(Paint));
    DrawPathDrawIndexed(path, DRAW_INDEX_U16, &shorts, 6, 0);
    DrawPathDrawIndexed(path, DRAW_INDEX_U32, &words, 6, 0);
    DrawPathDrawIndexed(path, DRAW_INDEX_U8, &bytes, 6, 0);
    DrawPathDrawIndexed(path, DRAW_INDEX_U16, &shorts, 6, 12);
    if (!DrawPathEnd(path)) {
        snprintf(why, why_size, "drawing the scene: %s", DrawPathFailure(path));
        return false;
    }
    return true;
}

// ---- a uniform buffer updated inside the list

// The fourth scene: a quad in each quarter, all four coloured by one uniform buffer of the
// fragment stage, which the list updates before each draw (DrawPathUpdate). The processor
// leaves the clear colour in the buffer: a list whose updates do not land draws nothing a
// quarter expects, and one whose updates all land first draws the last colour four times.

static bool UpdatedDraw(DrawPath *path, char *why, size_t why_size) {
    const int program = DrawPathProgram(path, ShaderData(SHADER_SCENE_VERT),
                                        ShaderSize(SHADER_SCENE_VERT),
                                        ShaderData(SHADER_SCENE_FRAG),
                                        ShaderSize(SHADER_SCENE_FRAG));
    DrawBuffer vertices, move, paint;
    const bool ok = program >= 0 &&
                    DrawPathAlloc(path, INDEXED_QUADS * 6u * (uint32_t)sizeof(GpuVertex),
                                  &vertices) &&
                    DrawPathAlloc(path, (uint32_t)sizeof(Move), &move) &&
                    DrawPathAlloc(path, (uint32_t)sizeof(Paint), &paint);
    if (!ok) {
        snprintf(why, why_size, "building the scene: %s", DrawPathFailure(path));
        return false;
    }
    GpuVertex quads[INDEXED_QUADS * 6u];
    static const uint8_t white[4] = { 0xff, 0xff, 0xff, 0xff };
    for (uint32_t i = 0; i < INDEXED_QUADS; ++i) {
        const uint32_t x0 = (i % 2u) * SCENE_QUARTER, y0 = (i / 2u) * SCENE_QUARTER;
        Quad(quads + i * 6u, x0, y0, x0 + SCENE_QUARTER, y0 + SCENE_QUARTER, 0.5f, white);
    }
    memcpy(vertices.bytes, quads, sizeof(quads));
    const Move still = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    memcpy(move.bytes, &still, sizeof(still));
    const float clear[4] = { (float)SCENE_CLEAR[0] / 255.0f, (float)SCENE_CLEAR[1] / 255.0f,
                             (float)SCENE_CLEAR[2] / 255.0f, (float)SCENE_CLEAR[3] / 255.0f };
    const Paint before = { { clear[0], clear[1], clear[2], clear[3] }, { PAINT_BUFFER, 0, 0, 0 } };
    memcpy(paint.bytes, &before, sizeof(before));

    DrawPathBegin(path);
    DrawPathClear(path, clear);
    DrawPathUseProgram(path, program);
    DrawPathVertices(path, &vertices, sizeof(quads));
    DrawPathUniform(path, DRAW_STAGE_VERTEX, 0, &move, sizeof(Move));
    DrawPathUniform(path, DRAW_STAGE_FRAGMENT, 0, &paint, sizeof(Paint));
    for (uint32_t i = 0; i < INDEXED_QUADS; ++i) {
        const uint8_t *const colour = INDEXED_COLOURS[i];
        const Paint now = { { (float)colour[0] / 255.0f, (float)colour[1] / 255.0f,
                              (float)colour[2] / 255.0f, (float)colour[3] / 255.0f },
                            { PAINT_BUFFER, 0, 0, 0 } };
        DrawPathUpdate(path, &paint, sizeof(Paint), 0, &now, sizeof(now));
        DrawPathDraw(path, i * 6u, 6);
    }
    if (!DrawPathEnd(path)) {
        snprintf(why, why_size, "drawing the scene: %s", DrawPathFailure(path));
        return false;
    }
    return true;
}

// ---- instanced draws, and draws whose numbers the graphics unit reads

// The fifth scene: eight cells, four across and two down, each one instance of the same quad.
// The quad's vertices are the first cell's; an instance's entry of a second vertex stream, which
// steps once an instance, moves it to its cell and colours it (scene_instanced.vert.glsl). Two
// instances a draw, four draws: vertices instanced, indices instanced, and each again with its
// numbers in a buffer the graphics unit reads. Every draw but the first starts at a base
// instance, so one that starts at nought draws the first two cells again and leaves its own
// the clear colour.

#define INSTANCE_CELLS 8u
#define INSTANCE_CELL_WIDTH (SCENE_SIZE / 4u)
#define INSTANCE_CELL_HEIGHT (SCENE_SIZE / 2u)

static void InstanceColour(uint32_t cell, uint8_t out[4]) {
    out[0] = (uint8_t)(0x20u + 0x18u * cell);
    out[1] = (uint8_t)(0xf0u - 0x18u * cell);
    out[2] = (uint8_t)(0x40u + 0x11u * cell);
    out[3] = 0xff;
}

static void InstancedExpected(uint32_t x, uint32_t y, uint8_t out[4]) {
    InstanceColour((y / INSTANCE_CELL_HEIGHT) * 4u + x / INSTANCE_CELL_WIDTH, out);
}

static const char *InstancedLooksLike(const uint8_t pixel[4]) {
    static const char *const NAMES[INSTANCE_CELLS] = {
        "instance 0's colour (vertices instanced)", "instance 1's colour (vertices instanced)",
        "instance 2's colour (indices instanced)",  "instance 3's colour (indices instanced)",
        "instance 4's colour (vertices, numbers in a buffer)",
        "instance 5's colour (vertices, numbers in a buffer)",
        "instance 6's colour (indices, numbers in a buffer)",
        "instance 7's colour (indices, numbers in a buffer)",
    };
    for (uint32_t i = 0; i < INSTANCE_CELLS; ++i) {
        uint8_t colour[4];
        InstanceColour(i, colour);
        if (memcmp(pixel, colour, 4) == 0) {
            return NAMES[i];
        }
    }
    return memcmp(pixel, SCENE_CLEAR, 4) == 0 ? "the clear colour: nothing drawn"
                                              : "nothing the scene draws";
}

static bool InstancedDraw(DrawPath *path, char *why, size_t why_size) {
    const int program = DrawPathProgram(path, ShaderData(SHADER_SCENE_INSTANCED_VERT),
                                        ShaderSize(SHADER_SCENE_INSTANCED_VERT),
                                        ShaderData(SHADER_SCENE_FRAG),
                                        ShaderSize(SHADER_SCENE_FRAG));
    DrawBuffer vertices, entries, paint, indices, of_vertices, of_indices;
    const bool ok = program >= 0 &&
                    DrawPathAlloc(path, 6u * (uint32_t)sizeof(GpuVertex), &vertices) &&
                    DrawPathAlloc(path, INSTANCE_CELLS * (uint32_t)sizeof(DrawInstance),
                                  &entries) &&
                    DrawPathAlloc(path, (uint32_t)sizeof(Paint), &paint) &&
                    DrawPathAlloc(path, 6u * 2u, &indices) &&
                    DrawPathAlloc(path, 4u * 4u, &of_vertices) &&
                    DrawPathAlloc(path, 5u * 4u, &of_indices);
    if (!ok) {
        snprintf(why, why_size, "building the scene: %s", DrawPathFailure(path));
        return false;
    }
    // The first cell's quad, in a colour no cell has: the instance's is the one drawn.
    GpuVertex quad[6];
    Quad(quad, 0, 0, INSTANCE_CELL_WIDTH, INSTANCE_CELL_HEIGHT, 0.5f, SCENE_FAR);
    memcpy(vertices.bytes, quad, sizeof(quad));
    DrawInstance instances[INSTANCE_CELLS];
    for (uint32_t i = 0; i < INSTANCE_CELLS; ++i) {
        uint8_t colour[4];
        InstanceColour(i, colour);
        // A cell across is half the clip space's width a quarter of the target is, a cell down
        // its whole height a half is.
        const DrawInstance instance = { { (float)(i % 4u) * 0.5f, -(float)(i / 4u), 0.0f, 0.0f },
                                        { (float)colour[0] / 255.0f, (float)colour[1] / 255.0f,
                                          (float)colour[2] / 255.0f, (float)colour[3] / 255.0f } };
        instances[i] = instance;
    }
    memcpy(entries.bytes, instances, sizeof(instances));
    static const uint16_t order[6] = { 0, 1, 2, 3, 4, 5 };
    // The count, the instances, the first vertex and the base instance; then the same for
    // indices, with the base vertex before the base instance.
    static const uint32_t vertex_numbers[4] = { 6, 2, 0, 4 };
    static const uint32_t index_numbers[5] = { 6, 2, 0, 0, 6 };
    memcpy(indices.bytes, order, sizeof(order));
    memcpy(of_vertices.bytes, vertex_numbers, sizeof(vertex_numbers));
    memcpy(of_indices.bytes, index_numbers, sizeof(index_numbers));
    const Paint flat = { { 1.0f, 1.0f, 1.0f, 1.0f }, { PAINT_VERTEX, 0, 0, 0 } };
    memcpy(paint.bytes, &flat, sizeof(flat));

    const float clear[4] = { (float)SCENE_CLEAR[0] / 255.0f, (float)SCENE_CLEAR[1] / 255.0f,
                             (float)SCENE_CLEAR[2] / 255.0f, (float)SCENE_CLEAR[3] / 255.0f };
    const DrawElements by_index = { true, DRAW_INDEX_U16, &indices };
    DrawPathBegin(path);
    DrawPathClear(path, clear);
    DrawPathUseProgram(path, program);
    DrawPathVertices(path, &vertices, sizeof(quad));
    DrawPathInstances(path, &entries, sizeof(instances));
    DrawPathUniform(path, DRAW_STAGE_FRAGMENT, 0, &paint, sizeof(Paint));
    DrawPathDrawInstanced(path, NULL, 0, 6, 0, 2);
    DrawPathDrawInstanced(path, &by_index, 0, 6, 2, 2);
    DrawPathDrawIndirect(path, NULL, &of_vertices);
    DrawPathDrawIndirect(path, &by_index, &of_indices);
    if (!DrawPathEnd(path)) {
        snprintf(why, why_size, "drawing the scene: %s", DrawPathFailure(path));
        return false;
    }
    return true;
}

// ---- the draws that read their numbers, in every form

// The forms scene: the draws whose numbers the graphics unit reads, each form in a cell of four
// across and two down, and in every cell a red quad and then a green one over it, so a cell the
// form drew whole is green, one whose last draw was left out is red, and one nothing drew keeps
// the clear colour. A runtime that keeps these draws' numbers on the graphics unit has to turn
// each form into what its own unit draws without reading them:
//   cell 0  quads of vertices, the two quads one draw's eight vertices;
//   cell 1  quads of two byte indices, from the second index of their buffer through a base vertex;
//   cell 2  quads of vertices, as many draws as a word says: red, green and red again, of which
//           the word says two (the counted scene alone, for the NVN the program looks up);
//   cell 3  the same by two byte indices (the counted scene alone);
//   cell 4  65537 quads of two byte indices in one draw, the red quad 65536 times and then the
//           green one;
//   cell 5  65537 quads of vertices in one draw, the same;
//   cell 6  triangles of one byte indices;
//   cell 7  one strip of two byte indices that an index of 64 restarts, which is no vertex of the
//           two quads: after the green quad's strip come two more indices with a restart around
//           each, no triangle, and a runtime that takes the 64 for a vertex draws a red triangle
//           over half the cell with them.
// Without the count the two counted cells keep the clear colour, which is the scene deko3d and
// the layer can draw.

#define FORM_CELLS 8u
#define FORM_CELL_WIDTH (SCENE_SIZE / 4u)
#define FORM_CELL_HEIGHT (SCENE_SIZE / 2u)
#define FORM_MANY 65537u
// The cells' vertices, eight each; then the restart strip's red triangle; then the 65537 quads.
#define FORM_RESTART 64u
#define FORM_SMALL (FORM_CELLS * 8u + 3u)
#define FORM_VERTICES (FORM_SMALL + FORM_MANY * 4u)
#define FORM_DATA_BYTES                                                                          \
    (FORM_VERTICES * (uint32_t)sizeof(GpuVertex) + FORM_MANY * 4u * 2u + 0x4000u)
static const uint8_t FORM_RED[4] = { 0xe0, 0x18, 0x28, 0xff };
static const uint8_t FORM_GREEN[4] = { 0x18, 0xd0, 0x48, 0xff };

static void FormsExpected(uint32_t x, uint32_t y, uint8_t out[4]) {
    (void)x;
    (void)y;
    memcpy(out, FORM_GREEN, 4);
}

static void FormsPlainExpected(uint32_t x, uint32_t y, uint8_t out[4]) {
    const uint32_t cell = (y / FORM_CELL_HEIGHT) * 4u + x / FORM_CELL_WIDTH;
    memcpy(out, cell == 2u || cell == 3u ? SCENE_CLEAR : FORM_GREEN, 4);
}

static const char *FormsLooksLike(const uint8_t pixel[4]) {
    if (memcmp(pixel, FORM_GREEN, 4) == 0) {
        return "a green quad, where no form draws";
    }
    if (memcmp(pixel, FORM_RED, 4) == 0) {
        return "red: the form's green quad was not drawn over it, or a restart index was taken "
               "for a vertex";
    }
    return memcmp(pixel, SCENE_CLEAR, 4) == 0 ? "the clear colour: the form drew nothing"
                                              : "nothing the scene draws";
}

/// The four corners of @p cell's quad in @p colour, around it: top left, top right, bottom
/// right, bottom left.
static void FormCorners(GpuVertex out[4], uint32_t cell, const uint8_t colour[4]) {
    static const uint8_t CORNERS[4] = { 0, 1, 2, 5 };
    const uint32_t x0 = (cell % 4u) * FORM_CELL_WIDTH, y0 = (cell / 4u) * FORM_CELL_HEIGHT;
    GpuVertex quad[6];
    Quad(quad, x0, y0, x0 + FORM_CELL_WIDTH, y0 + FORM_CELL_HEIGHT, 0.5f, colour);
    for (uint32_t k = 0; k < 4u; ++k) {
        out[k] = quad[CORNERS[k]];
    }
}

static bool FormsDrawBy(DrawPath *path, bool counted, char *why, size_t why_size) {
    const int program = DrawPathProgram(path, ShaderData(SHADER_SCENE_VERT),
                                        ShaderSize(SHADER_SCENE_VERT),
                                        ShaderData(SHADER_SCENE_FRAG),
                                        ShaderSize(SHADER_SCENE_FRAG));
    DrawBuffer vertices, move, paint, shorts, many, bytes, strip, count;
    // The numbers: cells 0, 1, 4, 5, 6 and 7 a draw each, cells 2 and 3 three draws each.
    DrawBuffer numbers[FORM_CELLS];
    bool ok = program >= 0 &&
              DrawPathAlloc(path, FORM_VERTICES * (uint32_t)sizeof(GpuVertex), &vertices) &&
              DrawPathAlloc(path, (uint32_t)sizeof(Move), &move) &&
              DrawPathAlloc(path, (uint32_t)sizeof(Paint), &paint) &&
              DrawPathAlloc(path, 9u * 2u, &shorts) &&
              DrawPathAlloc(path, FORM_MANY * 4u * 2u, &many) && DrawPathAlloc(path, 12u, &bytes) &&
              DrawPathAlloc(path, 15u * 2u, &strip) && DrawPathAlloc(path, 4u, &count);
    for (uint32_t i = 0; ok && i < FORM_CELLS; ++i) {
        ok = DrawPathAlloc(path, 3u * 20u, &numbers[i]);
    }
    if (!ok) {
        snprintf(why, why_size, "building the scene: %s", DrawPathFailure(path));
        return false;
    }
    // Every cell's red corners and then its green ones; the red triangle a restart index taken
    // for a vertex would draw over the top right half of cell 7; cell 5's 65536 red quads and its
    // green one.
    GpuVertex *const corners = vertices.bytes;
    for (uint32_t cell = 0; cell < FORM_CELLS; ++cell) {
        FormCorners(corners + cell * 8u, cell, FORM_RED);
        FormCorners(corners + cell * 8u + 4u, cell, FORM_GREEN);
    }
    GpuVertex red[4], green[4];
    FormCorners(red, 7, FORM_RED);
    corners[FORM_RESTART] = red[0];
    corners[FORM_RESTART + 1u] = red[1];
    corners[FORM_RESTART + 2u] = red[2];
    FormCorners(red, 5, FORM_RED);
    FormCorners(green, 5, FORM_GREEN);
    for (uint32_t i = 0; i < FORM_MANY * 4u; ++i) {
        corners[FORM_SMALL + i] = i < (FORM_MANY - 1u) * 4u ? red[i & 3u] : green[i & 3u];
    }
    // Two byte indices of a cell's eight vertices, after one index that is no draw's.
    uint16_t eight[9] = { 0xdead, 0, 1, 2, 3, 4, 5, 6, 7 };
    memcpy(shorts.bytes, eight, sizeof(eight));
    uint16_t *const repeated = many.bytes;
    for (uint32_t i = 0; i < FORM_MANY * 4u; ++i) {
        repeated[i] = (uint16_t)((i & 3u) + (i < (FORM_MANY - 1u) * 4u ? 0u : 4u));
    }
    static const uint8_t triangles[12] = { 0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7 };
    memcpy(bytes.bytes, triangles, sizeof(triangles));
    // Cell 7's vertices by their own numbers, each quad a strip (top left, top right, bottom
    // left, bottom right) and the restart index after each; then the red triangle's other two
    // corners, a restart before and after each, so restarted they are strips of one vertex, and
    // taken for a vertex the index makes the triangle with them, once each way round.
    static const uint16_t strips[15] = { 56,           57,
                                         59,           58,
                                         FORM_RESTART, 60,
                                         61,           63,
                                         62,           FORM_RESTART,
                                         FORM_RESTART + 1u, FORM_RESTART,
                                         FORM_RESTART + 2u, FORM_RESTART,
                                         FORM_RESTART + 1u };
    memcpy(strip.bytes, strips, sizeof(strips));
    // The count, the instances, the first vertex and the base instance; for indices the first
    // index, the base vertex and the base instance.
    static const uint32_t cell0[4] = { 8, 1, 0, 0 };
    static const uint32_t cell1[5] = { 8, 1, 1, 8, 0 };
    static const uint32_t cell2[3][4] = { { 4, 1, 16, 0 }, { 4, 1, 20, 0 }, { 4, 1, 16, 0 } };
    static const uint32_t cell3[3][5] = { { 4, 1, 1, 24, 0 }, { 4, 1, 5, 24, 0 }, { 4, 1, 1, 24, 0 } };
    static const uint32_t cell4[5] = { FORM_MANY * 4u, 1, 0, 32, 0 };
    static const uint32_t cell5[4] = { FORM_MANY * 4u, 1, FORM_SMALL, 0 };
    static const uint32_t cell6[5] = { 12, 1, 0, 48, 0 };
    static const uint32_t cell7[5] = { 15, 1, 0, 0, 0 };
    static const uint32_t two = 2;
    memcpy(numbers[0].bytes, cell0, sizeof(cell0));
    memcpy(numbers[1].bytes, cell1, sizeof(cell1));
    memcpy(numbers[2].bytes, cell2, sizeof(cell2));
    memcpy(numbers[3].bytes, cell3, sizeof(cell3));
    memcpy(numbers[4].bytes, cell4, sizeof(cell4));
    memcpy(numbers[5].bytes, cell5, sizeof(cell5));
    memcpy(numbers[6].bytes, cell6, sizeof(cell6));
    memcpy(numbers[7].bytes, cell7, sizeof(cell7));
    memcpy(count.bytes, &two, sizeof(two));
    const Move still = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    const Paint flat = { { 1.0f, 1.0f, 1.0f, 1.0f }, { PAINT_VERTEX, 0, 0, 0 } };
    memcpy(move.bytes, &still, sizeof(still));
    memcpy(paint.bytes, &flat, sizeof(flat));

    const float clear[4] = { (float)SCENE_CLEAR[0] / 255.0f, (float)SCENE_CLEAR[1] / 255.0f,
                             (float)SCENE_CLEAR[2] / 255.0f, (float)SCENE_CLEAR[3] / 255.0f };
    const DrawElements by_shorts = { true, DRAW_INDEX_U16, &shorts };
    const DrawElements by_many = { true, DRAW_INDEX_U16, &many };
    const DrawElements by_bytes = { true, DRAW_INDEX_U8, &bytes };
    const DrawElements by_strip = { true, DRAW_INDEX_U16, &strip };
    DrawPathBegin(path);
    DrawPathClear(path, clear);
    DrawPathUseProgram(path, program);
    DrawPathVertices(path, &vertices, FORM_VERTICES * (uint32_t)sizeof(GpuVertex));
    DrawPathUniform(path, DRAW_STAGE_VERTEX, 0, &move, sizeof(Move));
    DrawPathUniform(path, DRAW_STAGE_FRAGMENT, 0, &paint, sizeof(Paint));
    DrawPathShape(path, DRAW_SHAPE_QUADS);
    DrawPathDrawIndirect(path, NULL, &numbers[0]);
    DrawPathDrawIndirect(path, &by_shorts, &numbers[1]);
    if (counted) {
        DrawPathDrawIndirectCount(path, NULL, &numbers[2], &count, 3, 16);
        DrawPathDrawIndirectCount(path, &by_shorts, &numbers[3], &count, 3, 20);
    }
    DrawPathDrawIndirect(path, &by_many, &numbers[4]);
    DrawPathDrawIndirect(path, NULL, &numbers[5]);
    DrawPathShape(path, DRAW_SHAPE_TRIANGLES);
    DrawPathDrawIndirect(path, &by_bytes, &numbers[6]);
    DrawPathShape(path, DRAW_SHAPE_TRIANGLE_STRIP);
    DrawPathRestart(path, true, FORM_RESTART);
    DrawPathDrawIndirect(path, &by_strip, &numbers[7]);
    DrawPathRestart(path, false, 0);
    DrawPathShape(path, DRAW_SHAPE_TRIANGLES);
    if (!DrawPathEnd(path)) {
        snprintf(why, why_size, "drawing the scene: %s", DrawPathFailure(path));
        return false;
    }
    return true;
}

static bool FormsDraw(DrawPath *path, char *why, size_t why_size) {
    return FormsDrawBy(path, false, why, why_size);
}

static bool FormsCountedDraw(DrawPath *path, char *why, size_t why_size) {
    return FormsDrawBy(path, true, why, why_size);
}

// ---- the draw deko3d has none of

// The sixth scene, for the NVN the program looks up alone: deko3d has no draw of as many draws
// as a word says, so the deko3d way cannot draw it and the layer lacks it, which is a skip where
// the layer answers (the texture draw alone is the next scene, for those two ways). The top left
// quarter is the first scene's 8 by 8 texture drawn with no program, four pixels a texel
// (nvnCommandBufferDrawTexture); the top right keeps the clear colour; the bottom row is the
// instanced scene's cells 4 to 7, two by as many draws of vertices as a word in a buffer says
// and two by as many draws of indices, each draw one instance from its own base instance. The
// word says two and the buffers hold a third draw's numbers, cell 0's, so a runtime that draws
// as many as the most it was given paints over the texture.

static void RareExpected(uint32_t x, uint32_t y, uint8_t out[4]) {
    if (y >= INSTANCE_CELL_HEIGHT) {
        InstanceColour(4u + x / INSTANCE_CELL_WIDTH, out);
    } else if (x < SCENE_QUARTER) {
        SceneTexel(x / 4u, y / 4u, out);
    } else {
        memcpy(out, SCENE_CLEAR, 4);
    }
}

static const char *RareLooksLike(const uint8_t pixel[4]) {
    for (uint32_t ty = 0; ty < SCENE_TEXTURE_SIZE; ++ty) {
        for (uint32_t tx = 0; tx < SCENE_TEXTURE_SIZE; ++tx) {
            uint8_t texel[4];
            SceneTexel(tx, ty, texel);
            if (memcmp(pixel, texel, 4) == 0) {
                return "a texel of the texture";
            }
        }
    }
    return InstancedLooksLike(pixel);
}

static bool RareDraw(DrawPath *path, char *why, size_t why_size) {
    const int program = DrawPathProgram(path, ShaderData(SHADER_SCENE_INSTANCED_VERT),
                                        ShaderSize(SHADER_SCENE_INSTANCED_VERT),
                                        ShaderData(SHADER_SCENE_FRAG),
                                        ShaderSize(SHADER_SCENE_FRAG));
    uint8_t texels[SCENE_TEXTURE_SIZE * SCENE_TEXTURE_SIZE * 4u];
    for (uint32_t y = 0; y < SCENE_TEXTURE_SIZE; ++y) {
        for (uint32_t x = 0; x < SCENE_TEXTURE_SIZE; ++x) {
            SceneTexel(x, y, texels + (y * SCENE_TEXTURE_SIZE + x) * 4u);
        }
    }
    const int texture =
        DrawPathTexture(path, SCENE_TEXTURE_SIZE, SCENE_TEXTURE_SIZE, texels, false);
    DrawBuffer vertices, entries, paint, indices, of_vertices, of_indices, count;
    const bool ok = program >= 0 && texture >= 0 &&
                    DrawPathAlloc(path, 6u * (uint32_t)sizeof(GpuVertex), &vertices) &&
                    DrawPathAlloc(path, INSTANCE_CELLS * (uint32_t)sizeof(DrawInstance),
                                  &entries) &&
                    DrawPathAlloc(path, (uint32_t)sizeof(Paint), &paint) &&
                    DrawPathAlloc(path, 6u * 2u, &indices) &&
                    DrawPathAlloc(path, 3u * 16u, &of_vertices) &&
                    DrawPathAlloc(path, 3u * 20u, &of_indices) && DrawPathAlloc(path, 4u, &count);
    if (!ok) {
        snprintf(why, why_size, "building the scene: %s", DrawPathFailure(path));
        return false;
    }
    GpuVertex quad[6];
    Quad(quad, 0, 0, INSTANCE_CELL_WIDTH, INSTANCE_CELL_HEIGHT, 0.5f, SCENE_FAR);
    memcpy(vertices.bytes, quad, sizeof(quad));
    DrawInstance instances[INSTANCE_CELLS];
    for (uint32_t i = 0; i < INSTANCE_CELLS; ++i) {
        uint8_t colour[4];
        InstanceColour(i, colour);
        const DrawInstance instance = { { (float)(i % 4u) * 0.5f, -(float)(i / 4u), 0.0f, 0.0f },
                                        { (float)colour[0] / 255.0f, (float)colour[1] / 255.0f,
                                          (float)colour[2] / 255.0f, (float)colour[3] / 255.0f } };
        instances[i] = instance;
    }
    memcpy(entries.bytes, instances, sizeof(instances));
    static const uint16_t order[6] = { 0, 1, 2, 3, 4, 5 };
    // Three draws' numbers each, of which the word says two: cells 4 and 5, then cell 0; and
    // cells 6 and 7, then cell 0.
    static const uint32_t vertex_numbers[3][4] = { { 6, 1, 0, 4 }, { 6, 1, 0, 5 }, { 6, 1, 0, 0 } };
    static const uint32_t index_numbers[3][5] = { { 6, 1, 0, 0, 6 },
                                                  { 6, 1, 0, 0, 7 },
                                                  { 6, 1, 0, 0, 0 } };
    static const uint32_t two = 2;
    memcpy(indices.bytes, order, sizeof(order));
    memcpy(of_vertices.bytes, vertex_numbers, sizeof(vertex_numbers));
    memcpy(of_indices.bytes, index_numbers, sizeof(index_numbers));
    memcpy(count.bytes, &two, sizeof(two));
    const Paint flat = { { 1.0f, 1.0f, 1.0f, 1.0f }, { PAINT_VERTEX, 0, 0, 0 } };
    memcpy(paint.bytes, &flat, sizeof(flat));

    const float clear[4] = { (float)SCENE_CLEAR[0] / 255.0f, (float)SCENE_CLEAR[1] / 255.0f,
                             (float)SCENE_CLEAR[2] / 255.0f, (float)SCENE_CLEAR[3] / 255.0f };
    const DrawElements by_index = { true, DRAW_INDEX_U16, &indices };
    static const float onto[4] = { 0.0f, 0.0f, (float)SCENE_QUARTER, (float)SCENE_QUARTER };
    static const float from[4] = { 0.0f, 0.0f, (float)SCENE_TEXTURE_SIZE,
                                   (float)SCENE_TEXTURE_SIZE };
    DrawPathBegin(path);
    DrawPathClear(path, clear);
    DrawPathDrawTexture(path, texture, onto, from);
    DrawPathUseProgram(path, program);
    DrawPathVertices(path, &vertices, sizeof(quad));
    DrawPathInstances(path, &entries, sizeof(instances));
    DrawPathUniform(path, DRAW_STAGE_FRAGMENT, 0, &paint, sizeof(Paint));
    DrawPathDrawIndirectCount(path, NULL, &of_vertices, &count, 3, 16);
    DrawPathDrawIndirectCount(path, &by_index, &of_indices, &count, 3, 20);
    if (!DrawPathEnd(path)) {
        snprintf(why, why_size, "drawing the scene: %s", DrawPathFailure(path));
        return false;
    }
    return true;
}

// ---- a texture drawn with no program

// The sixth scene's texture draw alone, for deko3d's call and the layer's, both
// dkCmdBufDrawTexture of the deko3d the NRO links: the top left quarter is the first scene's 8 by
// 8 texture drawn four pixels a texel, and the rest keeps the clear colour.

static void DrawnExpected(uint32_t x, uint32_t y, uint8_t out[4]) {
    if (x < SCENE_QUARTER && y < SCENE_QUARTER) {
        SceneTexel(x / 4u, y / 4u, out);
    } else {
        memcpy(out, SCENE_CLEAR, 4);
    }
}

static bool DrawnDraw(DrawPath *path, char *why, size_t why_size) {
    uint8_t texels[SCENE_TEXTURE_SIZE * SCENE_TEXTURE_SIZE * 4u];
    for (uint32_t y = 0; y < SCENE_TEXTURE_SIZE; ++y) {
        for (uint32_t x = 0; x < SCENE_TEXTURE_SIZE; ++x) {
            SceneTexel(x, y, texels + (y * SCENE_TEXTURE_SIZE + x) * 4u);
        }
    }
    const int texture =
        DrawPathTexture(path, SCENE_TEXTURE_SIZE, SCENE_TEXTURE_SIZE, texels, false);
    if (texture < 0) {
        snprintf(why, why_size, "building the scene: %s", DrawPathFailure(path));
        return false;
    }
    const float clear[4] = { (float)SCENE_CLEAR[0] / 255.0f, (float)SCENE_CLEAR[1] / 255.0f,
                             (float)SCENE_CLEAR[2] / 255.0f, (float)SCENE_CLEAR[3] / 255.0f };
    static const float onto[4] = { 0.0f, 0.0f, (float)SCENE_QUARTER, (float)SCENE_QUARTER };
    static const float from[4] = { 0.0f, 0.0f, (float)SCENE_TEXTURE_SIZE,
                                   (float)SCENE_TEXTURE_SIZE };
    DrawPathBegin(path);
    DrawPathClear(path, clear);
    DrawPathDrawTexture(path, texture, onto, from);
    if (!DrawPathEnd(path)) {
        snprintf(why, why_size, "drawing the scene: %s", DrawPathFailure(path));
        return false;
    }
    return true;
}

// ---- a capture of what a vertex stage writes

// The seventh scene, the three ways: transform feedback. The instanced scene's program draws
// cells 0, 1 and 2 of the top row, one instance a draw, with its vertex stage's outputs captured:
// the capture begins before cell 0, is paused for cell 1, resumed for cell 2 and ended. A
// captured vertex is a GpuVertex (the position the stage wrote, its texture coordinate, its
// colour), so the capture buffer is then bound as the vertices and drawn with as many vertices
// as the capture wrote, through one instance that moves it down a row and gives it cell 7's
// colour: the bottom row holds cells 0 and 2 again, and nothing under cell 1, which no capture
// took. A capture a pause does not stop paints under cell 1, and one a resume does not start
// again leaves the place under cell 2 clear. The eighth, for deko3d's way and the layer's, is the
// same row asked of the capture alone: cells 0 and 2 drawn under one capture, cell 1 after it
// ended, and the twelve vertices it wrote drawn by a draw of twelve, so no saved byte counts are
// read back.

#define CAPTURE_BYTES 0x400u
// NVN's control takes 20 bytes (a stride and four buffers' bytes, as ScarletNX's NVN reads it),
// deko3d's saved byte counts 64 (DK_TRANSFORM_FEEDBACK_OFFSETS_SIZE).
#define CAPTURE_CONTROL_BYTES 64u

static void CaptureExpected(uint32_t x, uint32_t y, uint8_t out[4]) {
    const uint32_t cell = x / INSTANCE_CELL_WIDTH;
    if (y < INSTANCE_CELL_HEIGHT && cell < 3u) {
        InstanceColour(cell, out);
    } else if (y >= INSTANCE_CELL_HEIGHT && (cell == 0u || cell == 2u)) {
        InstanceColour(7, out);
    } else {
        memcpy(out, SCENE_CLEAR, 4);
    }
}

static const char *CaptureLooksLike(const uint8_t pixel[4]) {
    static const char *const NAMES[4] = {
        "cell 0's colour (drawn while the capture ran)",
        "cell 1's colour (drawn while the capture was paused)",
        "cell 2's colour (drawn once the capture was resumed)",
        "the colour of what the capture wrote, drawn again",
    };
    static const uint32_t CELLS[4] = { 0, 1, 2, 7 };
    for (uint32_t i = 0; i < 4u; ++i) {
        uint8_t colour[4];
        InstanceColour(CELLS[i], colour);
        if (memcmp(pixel, colour, 4) == 0) {
            return NAMES[i];
        }
    }
    return memcmp(pixel, SCENE_CLEAR, 4) == 0 ? "the clear colour: nothing drawn"
                                              : "nothing the scene draws";
}

/// The capture scene; @p plain draws it without a pause, a resume or a draw of what the capture
/// wrote, the eighth scene.
static bool CaptureDrawBy(DrawPath *path, bool several, char *why, size_t why_size, bool plain) {
    const int program = DrawPathProgramCapturing(path, ShaderData(SHADER_SCENE_INSTANCED_VERT),
                                                 ShaderSize(SHADER_SCENE_INSTANCED_VERT),
                                                 ShaderData(SHADER_SCENE_FRAG),
                                                 ShaderSize(SHADER_SCENE_FRAG));
    DrawBuffer vertices, entries, again, paint, captured, control;
    const bool ok = program >= 0 &&
                    DrawPathAlloc(path, 6u * (uint32_t)sizeof(GpuVertex), &vertices) &&
                    DrawPathAlloc(path, 3u * (uint32_t)sizeof(DrawInstance), &entries) &&
                    DrawPathAlloc(path, (uint32_t)sizeof(DrawInstance), &again) &&
                    DrawPathAlloc(path, (uint32_t)sizeof(Paint), &paint) &&
                    DrawPathAlloc(path, CAPTURE_BYTES, &captured) &&
                    DrawPathAlloc(path, CAPTURE_CONTROL_BYTES, &control);
    if (!ok) {
        snprintf(why, why_size, "building the scene: %s", DrawPathFailure(path));
        return false;
    }
    GpuVertex quad[6];
    Quad(quad, 0, 0, INSTANCE_CELL_WIDTH, INSTANCE_CELL_HEIGHT, 0.5f, SCENE_FAR);
    memcpy(vertices.bytes, quad, sizeof(quad));
    DrawInstance instances[3];
    for (uint32_t i = 0; i < 3u; ++i) {
        uint8_t colour[4];
        InstanceColour(i, colour);
        const DrawInstance instance = { { (float)i * 0.5f, 0.0f, 0.0f, 0.0f },
                                        { (float)colour[0] / 255.0f, (float)colour[1] / 255.0f,
                                          (float)colour[2] / 255.0f, (float)colour[3] / 255.0f } };
        instances[i] = instance;
    }
    memcpy(entries.bytes, instances, sizeof(instances));
    // What draws the captured vertices: down a row, in cell 7's colour.
    uint8_t last[4];
    InstanceColour(7, last);
    const DrawInstance down = { { 0.0f, -1.0f, 0.0f, 0.0f },
                                { (float)last[0] / 255.0f, (float)last[1] / 255.0f,
                                  (float)last[2] / 255.0f, (float)last[3] / 255.0f } };
    memcpy(again.bytes, &down, sizeof(down));
    const Paint flat = { { 1.0f, 1.0f, 1.0f, 1.0f }, { PAINT_VERTEX, 0, 0, 0 } };
    memcpy(paint.bytes, &flat, sizeof(flat));
    memset(captured.bytes, 0, CAPTURE_BYTES);
    memset(control.bytes, 0, CAPTURE_CONTROL_BYTES);

    const float clear[4] = { (float)SCENE_CLEAR[0] / 255.0f, (float)SCENE_CLEAR[1] / 255.0f,
                             (float)SCENE_CLEAR[2] / 255.0f, (float)SCENE_CLEAR[3] / 255.0f };
    DrawPathBegin(path);
    DrawPathClear(path, clear);
    DrawPathUseProgram(path, program);
    DrawPathVertices(path, &vertices, sizeof(quad));
    DrawPathInstances(path, &entries, sizeof(instances));
    DrawPathUniform(path, DRAW_STAGE_FRAGMENT, 0, &paint, sizeof(Paint));
    DrawPathCaptureInto(path, &captured, CAPTURE_BYTES, several);
    DrawPathCapture(path, DRAW_CAPTURE_BEGIN, &control);
    DrawPathDrawInstanced(path, NULL, 0, 6, 0, 1);
    if (plain) {
        DrawPathDrawInstanced(path, NULL, 0, 6, 2, 1);
        DrawPathCapture(path, DRAW_CAPTURE_END, &control);
        DrawPathDrawInstanced(path, NULL, 0, 6, 1, 1);
    } else {
        DrawPathCapture(path, DRAW_CAPTURE_PAUSE, &control);
        DrawPathDrawInstanced(path, NULL, 0, 6, 1, 1);
        DrawPathCapture(path, DRAW_CAPTURE_RESUME, &control);
        DrawPathDrawInstanced(path, NULL, 0, 6, 2, 1);
        DrawPathCapture(path, DRAW_CAPTURE_END, &control);
    }
    DrawPathVertices(path, &captured, 12u * (uint32_t)sizeof(GpuVertex));
    DrawPathInstances(path, &again, sizeof(down));
    if (plain) {
        DrawPathDrawInstanced(path, NULL, 0, 12, 0, 1);
    } else {
        DrawPathDrawCaptured(path, &control);
    }
    if (!DrawPathEnd(path)) {
        snprintf(why, why_size, "drawing the scene: %s", DrawPathFailure(path));
        return false;
    }
    return true;
}

static bool CaptureDraw(DrawPath *path, char *why, size_t why_size) {
    return CaptureDrawBy(path, false, why, why_size, false);
}

static bool CaptureSeveralDraw(DrawPath *path, char *why, size_t why_size) {
    return CaptureDrawBy(path, true, why, why_size, false);
}

static bool CapturePlainDraw(DrawPath *path, char *why, size_t why_size) {
    return CaptureDrawBy(path, false, why, why_size, true);
}

/// A scene the suite draws down each way: what it asks of a way, how it draws, and what each of
/// its pixels has to hold.
typedef struct {
    bool depth;
    uint32_t textures;
    uint32_t texture_bytes;
    bool (*draw)(DrawPath *path, char *why, size_t why_size);
    void (*expected)(uint32_t x, uint32_t y, uint8_t out[4]);
    const char *(*looks_like)(const uint8_t pixel[4]);
    DrawBinds binds;
    /// Bytes of vertices and buffers past what every scene gets, for the scene that needs them.
    uint32_t data_bytes;
} Scene;

static const Scene FIVE_DRAWS = { true, 2, 2u * SCENE_TEXTURE_SIZE * SCENE_TEXTURE_SIZE * 4u,
                                  Draw, SceneExpected, SceneLooksLike, DRAW_BINDS_SINGLE, 0 };
static const Scene WRAPPED = { false, WRAP_QUADS,
                               WRAP_QUADS * WRAP_TEXTURE_SIZE * WRAP_TEXTURE_SIZE * 4u, WrapDraw,
                               WrapExpected, WrapLooksLike, DRAW_BINDS_SINGLE, 0 };
static const Scene INDEXED = { false, 0, 0, IndexedDraw, IndexedExpected, IndexedLooksLike,
                               DRAW_BINDS_SINGLE, 0 };
// The first scene again, the NVN ways binding its buffers and textures through other calls.
static const Scene FIVE_PLURAL = { true, 2, 2u * SCENE_TEXTURE_SIZE * SCENE_TEXTURE_SIZE * 4u,
                                   Draw, SceneExpected, SceneLooksLike, DRAW_BINDS_PLURAL, 0 };
static const Scene FIVE_DEFERRED = { true, 2, 2u * SCENE_TEXTURE_SIZE * SCENE_TEXTURE_SIZE * 4u,
                                     Draw, SceneExpected, SceneLooksLike, DRAW_BINDS_DEFERRED, 0 };
static const Scene UPDATED = { false, 0, 0, UpdatedDraw, IndexedExpected, IndexedLooksLike,
                               DRAW_BINDS_SINGLE, 0 };
static const Scene RARE = { false, 1,           SCENE_TEXTURE_SIZE * SCENE_TEXTURE_SIZE * 4u,
                            RareDraw, RareExpected, RareLooksLike,
                            DRAW_BINDS_SINGLE, 0 };
static const Scene DRAWN = { false, 1, SCENE_TEXTURE_SIZE * SCENE_TEXTURE_SIZE * 4u, DrawnDraw,
                             DrawnExpected, RareLooksLike, DRAW_BINDS_SINGLE, 0 };
static const Scene CAPTURE = { false, 0, 0, CaptureDraw, CaptureExpected, CaptureLooksLike,
                               DRAW_BINDS_SINGLE, 0 };
static const Scene CAPTURE_PLAIN = { false,           0, 0, CapturePlainDraw, CaptureExpected,
                                     CaptureLooksLike, DRAW_BINDS_SINGLE, 0 };
static const Scene CAPTURE_SEVERAL = { false,           0, 0, CaptureSeveralDraw, CaptureExpected,
                                       CaptureLooksLike, DRAW_BINDS_SINGLE, 0 };
static const Scene INSTANCED = { false,           0, 0, InstancedDraw, InstancedExpected,
                                 InstancedLooksLike, DRAW_BINDS_SINGLE, 0 };
static const Scene FORMS = { false, 0, 0, FormsDraw, FormsPlainExpected, FormsLooksLike,
                             DRAW_BINDS_SINGLE, FORM_DATA_BYTES };
static const Scene FORMS_COUNTED = { false,          0,                 0,              FormsCountedDraw,
                                     FormsExpected, FormsLooksLike, DRAW_BINDS_SINGLE,
                                     FORM_DATA_BYTES };

/// One way's case: @p scene drawn down @p kind and every pixel judged.
static void SceneCase(DrawPathKind kind, const Scene *scene, const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    DrawPathSpec spec;
    memset(&spec, 0, sizeof(spec));
    spec.width = SCENE_SIZE;
    spec.height = SCENE_SIZE;
    spec.depth = scene->depth;
    spec.programs = 1;
    spec.textures = scene->textures;
    // A page more where the way lends one as shader scratch memory.
    spec.data_bytes = (scene->binds == DRAW_BINDS_PLURAL ? 0x6000u : 0x4000u) + scene->data_bytes;
    spec.texture_bytes = scene->texture_bytes;
    spec.binds = scene->binds;
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
    const uint8_t *const pixels =
        scene->draw(path, why, sizeof(why)) ? DrawPathRead(path) : NULL;
    if (pixels == NULL && NvnLayerLacks()) {
        // The layer answered, and deko3d gives it no way to draw this at all.
        ReportSkip(SUITE, name, REPORT_REASON_UNAVAILABLE, NvnLayerError());
        DrawPathClose(path);
        return;
    }
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
            scene->expected(x, y, want);
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
               first_want[1], first_want[2], first_want[3], scene->looks_like(first));
    DrawPathClose(path);
}

void NvnSceneFirst(void) {
    SceneCase(DRAW_PATH_DEKO3D, &FIVE_DRAWS,
              "a scene of five draws through deko3d holds what the processor works out");
    SceneCase(DRAW_PATH_LAYER, &FIVE_DRAWS,
              "the same scene through the layer's own NVN functions holds what the processor "
              "works out");
    SceneCase(DRAW_PATH_DEKO3D, &WRAPPED,
              "a texture drawn past its edges through deko3d repeats, mirrors, holds its edge "
              "and shows its border");
    SceneCase(DRAW_PATH_LAYER, &WRAPPED,
              "the same texture past its edges through the layer's own NVN functions does the "
              "same");
    SceneCase(DRAW_PATH_DEKO3D, &INDEXED,
              "four quads drawn through deko3d by indices of one, two and four bytes and a "
              "base vertex are each in its quarter");
    SceneCase(DRAW_PATH_LAYER, &INDEXED,
              "the same four indexed quads through the layer's own NVN functions are each in "
              "its quarter");
    SceneCase(DRAW_PATH_LAYER, &FIVE_PLURAL,
              "the scene of five draws bound through the layer's own calls that take several "
              "holds what the processor works out");
    SceneCase(DRAW_PATH_LAYER, &FIVE_DEFERRED,
              "the scene of five draws bound through the layer's own deferred calls holds "
              "what the processor works out");
    SceneCase(DRAW_PATH_DEKO3D, &UPDATED,
              "four quads drawn through deko3d from one uniform buffer updated before each "
              "are each its own colour");
    SceneCase(DRAW_PATH_LAYER, &UPDATED,
              "the same four quads of an updated uniform buffer through the layer's own NVN "
              "functions are each its own colour");
    SceneCase(DRAW_PATH_DEKO3D, &INSTANCED,
              "eight instances of a quad drawn through deko3d, by instanced draws and by "
              "draws that read their numbers, are each in its cell");
    SceneCase(DRAW_PATH_LAYER, &INSTANCED,
              "the same eight instances through the layer's own NVN functions are each in "
              "its cell");
    SceneCase(DRAW_PATH_DEKO3D, &FORMS,
              "draws that read their numbers through deko3d, in the forms a runtime has to "
              "convert, each leave their cell green");
    SceneCase(DRAW_PATH_LAYER, &FORMS,
              "the same draws that read their numbers through the layer's own NVN functions "
              "each leave their cell green");
    SceneCase(DRAW_PATH_DEKO3D, &DRAWN,
              "a texture drawn with no program through deko3d fills its quarter, four pixels a "
              "texel");
    SceneCase(DRAW_PATH_LAYER, &DRAWN,
              "the same texture drawn with no program through the layer's own NVN functions "
              "fills its quarter");
    SceneCase(DRAW_PATH_DEKO3D, &CAPTURE_PLAIN,
              "what a capture took of two draws through deko3d, drawn again as twelve vertices, "
              "is the two a row down");
    SceneCase(DRAW_PATH_LAYER, &CAPTURE_PLAIN,
              "the same capture of two draws through the layer's own NVN functions, drawn again "
              "as twelve vertices, is the two a row down");
    SceneCase(DRAW_PATH_DEKO3D, &CAPTURE,
              "what a capture took of two draws, paused for a third between them, is drawn "
              "again a row down through deko3d");
    SceneCase(DRAW_PATH_LAYER, &CAPTURE,
              "the same paused capture is drawn again a row down through the layer's own NVN "
              "functions");
    SceneCase(DRAW_PATH_LAYER, &CAPTURE_SEVERAL,
              "the same paused capture into a buffer bound by the layer's call that takes "
              "several is drawn again a row down");
}

void NvnSceneLast(void) {
    SceneCase(DRAW_PATH_NVN, &FIVE_DRAWS,
              "the same scene through the NVN the program looks up holds what the processor "
              "works out");
    SceneCase(DRAW_PATH_NVN, &WRAPPED,
              "the same texture past its edges through the NVN the program looks up does the "
              "same");
    SceneCase(DRAW_PATH_NVN, &INDEXED,
              "the same four indexed quads through the NVN the program looks up are each in "
              "its quarter");
    SceneCase(DRAW_PATH_NVN, &FIVE_PLURAL,
              "the scene of five draws bound through the looked up calls that take several "
              "holds what the processor works out");
    SceneCase(DRAW_PATH_NVN, &FIVE_DEFERRED,
              "the scene of five draws bound through the looked up deferred calls holds what "
              "the processor works out");
    SceneCase(DRAW_PATH_NVN, &UPDATED,
              "the same four quads of an updated uniform buffer through the NVN the program "
              "looks up are each its own colour");
    SceneCase(DRAW_PATH_NVN, &INSTANCED,
              "the same eight instances through the NVN the program looks up are each in its "
              "cell");
    SceneCase(DRAW_PATH_NVN, &FORMS_COUNTED,
              "draws that read their numbers through the NVN the program looks up, counted "
              "ones too, each leave their cell green");
    SceneCase(DRAW_PATH_NVN, &RARE,
              "a texture drawn with no program and as many draws as a word says, through the "
              "NVN the program looks up, are each in its place");
    SceneCase(DRAW_PATH_NVN, &CAPTURE,
              "what a capture took of two draws, paused for a third between them, is drawn "
              "again a row down through the NVN the program looks up");
    SceneCase(DRAW_PATH_NVN, &CAPTURE_SEVERAL,
              "the same capture into a buffer bound by the call that takes several is drawn "
              "again a row down");
}
