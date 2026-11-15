// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gpu.h"

#ifdef __cplusplus
extern "C" {
#endif

/// A scene's drawing, written once and run down any of the program's ways to the graphics
/// unit: deko3d's calls, as every drawing suite makes them, or NVN's (nvnlayer,
/// docs/NvnLayer.md), as a game draws.
///
/// The ways reach different code on a machine that runs a game's NVN calls itself: under
/// ScarletNX the deko3d way is the engine methods path and the NVN way is the NVN the games
/// take. On the console and in an emulator that runs the graphics unit, the NVN way is the layer
/// recording deko3d commands, so the ways end in the same engine and say whether the layer
/// translated the scene.
///
/// What a scene draws with is what every way has: one RGBA8 target the processor reads back,
/// an optional depth target, programs of a vertex and a fragment stage, vertices in GpuVertex's
/// layout, uniform buffers, and 2D RGBA8 textures. Its shaders follow the layer's two rules
/// (docs/NvnLayer.md, "What a shader may use"), which is what makes one binary right for all: a
/// uniform block of NVN slot r is `binding = r + 1`, and a texture of slot r is sampled through
/// the handle at 0x20 + 8 * r of the block at binding 0.
///
/// DRAW_PATH_LAYER is the NVN way with the layer's own functions (NvnLayerOwnProcs) where
/// DRAW_PATH_NVN has the ones the program looks up: the same code on the console, and under
/// ScarletNX the layer recording deko3d commands, which is how its translation is checked on a
/// machine that otherwise never runs it. ScarletNX takes a function over by its address once
/// the program has looked its name up, so a scene goes down DRAW_PATH_LAYER before anything in
/// the program calls NvnLoadProcs.
typedef enum {
    DRAW_PATH_DEKO3D = 0,
    DRAW_PATH_LAYER = 1,
    DRAW_PATH_NVN = 2,
    DRAW_PATH_COUNT = 3,
} DrawPathKind;

/// The way's name for a failure's text: "deko3d", "the layer's own NVN" or "the program's NVN".
const char *DrawPathName(DrawPathKind kind);

typedef enum {
    DRAW_STAGE_VERTEX = 0,
    DRAW_STAGE_FRAGMENT = 1,
} DrawStage;

/// What a scene asks a way to bring up. Nothing it did not ask for is made.
typedef struct {
    uint32_t width;
    uint32_t height;
    /// A 32-bit float depth target beside the colour one.
    bool depth;
    /// How many programs and textures the scene makes, at most DRAW_PATH_PROGRAMS and
    /// DRAW_PATH_TEXTURES.
    uint32_t programs;
    uint32_t textures;
    /// Bytes of the memory DrawPathAlloc carves: vertices and uniform buffers, each on a 256
    /// byte boundary.
    uint32_t data_bytes;
    /// Bytes of texels over all the textures, four a texel.
    uint32_t texture_bytes;
    /// Bytes of command memory for the longest list the scene records.
    uint32_t command_bytes;
} DrawPathSpec;

#define DRAW_PATH_PROGRAMS 4u
#define DRAW_PATH_TEXTURES 8u
/// The texture slots a stage has through either way.
#define DRAW_PATH_SLOTS 16u

/// Memory the graphics unit reads, as DrawPathAlloc carved it.
typedef struct {
    void *bytes;       ///< where the processor writes
    uint64_t address;  ///< the graphics unit's address of the same bytes
} DrawBuffer;

typedef struct DrawPath DrawPath;

/// Brings @p kind up for @p spec. NULL when it could not, with the reason in @p why. One way of
/// each kind is up at a time.
DrawPath *DrawPathOpen(DrawPathKind kind, const DrawPathSpec *spec, char *why, size_t why_size);

/// Takes the way down, with everything it made.
void DrawPathClose(DrawPath *path);

/// The reason the last call on @p path failed, "" when it did not: a sentence for the report.
/// A way that failed stays usable only for DrawPathClose.
const char *DrawPathFailure(const DrawPath *path);

/// A program of two compiled modules, whole DKSH files. Its number for DrawPathUseProgram, or
/// -1.
int DrawPathProgram(DrawPath *path, const void *vertex_dksh, uint32_t vertex_bytes,
                    const void *fragment_dksh, uint32_t fragment_bytes);

/// What a texture does with a coordinate past its edges.
typedef enum {
    DRAW_WRAP_CLAMP_TO_EDGE = 0,
    DRAW_WRAP_REPEAT = 1,
    DRAW_WRAP_MIRRORED_REPEAT = 2,
    DRAW_WRAP_CLAMP_TO_BORDER = 3,
} DrawWrap;

/// How a texture is sampled: one filter both ways, one wrap mode on every axis, and the colour
/// DRAW_WRAP_CLAMP_TO_BORDER shows past the edges.
typedef struct {
    bool linear;
    DrawWrap wrap;
    float border[4];
} DrawSampler;

/// A 2D RGBA8 texture of @p width by @p height holding @p texels, tightly packed, sampled as
/// @p sampler says. Its number for DrawPathBindTexture, or -1.
int DrawPathTextureSampled(DrawPath *path, uint32_t width, uint32_t height,
                           const uint8_t *texels, const DrawSampler *sampler);
/// DrawPathTextureSampled with one filter both ways, clamped to its edge.
int DrawPathTexture(DrawPath *path, uint32_t width, uint32_t height, const uint8_t *texels,
                    bool linear);

/// Carves @p bytes of the way's data memory on a 256 byte boundary. False when it is used up.
bool DrawPathAlloc(DrawPath *path, uint32_t bytes, DrawBuffer *out);

/// Starts a list: the targets bound, the viewport and the scissor over all of them, nothing
/// blended, nothing culled, counterclockwise in front, every channel written, and the depth
/// test and write off.
void DrawPathBegin(DrawPath *path);
void DrawPathClear(DrawPath *path, const float rgba[4]);
void DrawPathClearDepth(DrawPath *path, float depth);
/// The depth test (less) and the depth write, for the draws after it.
void DrawPathDepth(DrawPath *path, bool test, bool write);
void DrawPathUseProgram(DrawPath *path, int program);
/// @p bytes of GpuVertex at @p vertices, for the draws after it.
void DrawPathVertices(DrawPath *path, const DrawBuffer *vertices, uint32_t bytes);
/// Uniform buffer @p slot of @p stage, NVN's numbering: the shader's `binding = slot + 1`.
void DrawPathUniform(DrawPath *path, DrawStage stage, uint32_t slot, const DrawBuffer *buffer,
                     uint32_t bytes);
/// Texture @p slot of @p stage: the handle the shader reads at 0x20 + 8 * slot of its binding 0.
void DrawPathBindTexture(DrawPath *path, DrawStage stage, uint32_t slot, int texture);
/// A triangle list of @p count vertices from @p first.
void DrawPathDraw(DrawPath *path, uint32_t first, uint32_t count);
/// Ends the list, submits it and waits for the graphics unit. False when anything since
/// DrawPathBegin failed, with DrawPathFailure saying what.
bool DrawPathEnd(DrawPath *path);

/// The colour target's pixels, four bytes each, the top row first: valid until the next call on
/// @p path. NULL when the copy failed.
const uint8_t *DrawPathRead(DrawPath *path);

#ifdef __cplusplus
}
#endif
