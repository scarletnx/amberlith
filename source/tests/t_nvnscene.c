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
} Scene;

static const Scene FIVE_DRAWS = { true, 2, 2u * SCENE_TEXTURE_SIZE * SCENE_TEXTURE_SIZE * 4u,
                                  Draw, SceneExpected, SceneLooksLike, DRAW_BINDS_SINGLE };
static const Scene WRAPPED = { false, WRAP_QUADS,
                               WRAP_QUADS * WRAP_TEXTURE_SIZE * WRAP_TEXTURE_SIZE * 4u, WrapDraw,
                               WrapExpected, WrapLooksLike, DRAW_BINDS_SINGLE };
static const Scene INDEXED = { false, 0, 0, IndexedDraw, IndexedExpected, IndexedLooksLike,
                               DRAW_BINDS_SINGLE };
// The first scene again, the NVN ways binding its buffers and textures through other calls.
static const Scene FIVE_PLURAL = { true, 2, 2u * SCENE_TEXTURE_SIZE * SCENE_TEXTURE_SIZE * 4u,
                                   Draw, SceneExpected, SceneLooksLike, DRAW_BINDS_PLURAL };
static const Scene FIVE_DEFERRED = { true, 2, 2u * SCENE_TEXTURE_SIZE * SCENE_TEXTURE_SIZE * 4u,
                                     Draw, SceneExpected, SceneLooksLike, DRAW_BINDS_DEFERRED };
static const Scene UPDATED = { false, 0, 0, UpdatedDraw, IndexedExpected, IndexedLooksLike,
                               DRAW_BINDS_SINGLE };

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
    spec.data_bytes = scene->binds == DRAW_BINDS_PLURAL ? 0x6000u : 0x4000u;
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
}
