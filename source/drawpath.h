// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
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

/// How the NVN ways bind vertex buffers, uniform buffers and textures: one at a time, as the
/// first scene does; through the calls that take several, with the two hints a game records at
/// the start of a list (a coverage modulation table and shader scratch memory); or through the
/// deferred calls, which read their ranges and handles when the list runs.
typedef enum {
    DRAW_BINDS_SINGLE = 0,
    DRAW_BINDS_PLURAL = 1,
    DRAW_BINDS_DEFERRED = 2,
} DrawBinds;

/// What a scene asks a way to bring up. Nothing it did not ask for is made.
typedef struct {
    uint32_t width;
    uint32_t height;
    /// A 32-bit float depth target beside the colour one.
    bool depth;
    /// The depth target is 24 bits of depth with 8 of stencil instead.
    bool stencil;
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
    /// The calls the NVN ways bind with; the deko3d way has the one kind.
    DrawBinds binds;
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
/// Writes @p bytes of @p data at @p offset of @p buffer, a uniform buffer of @p buffer_bytes,
/// when the list reaches this point: the draws before it read what was there, the ones after
/// it read @p data.
void DrawPathUpdate(DrawPath *path, const DrawBuffer *buffer, uint32_t buffer_bytes,
                    uint32_t offset, const void *data, uint32_t bytes);
/// A triangle list of @p count vertices from @p first.
void DrawPathDraw(DrawPath *path, uint32_t first, uint32_t count);
/// The width of an index: its bytes are 1 << type.
typedef enum {
    DRAW_INDEX_U8 = 0,
    DRAW_INDEX_U16 = 1,
    DRAW_INDEX_U32 = 2,
} DrawIndex;
/// A triangle list of the vertices the @p count indices of @p type at @p indices name, each
/// with @p base_vertex added.
void DrawPathDrawIndexed(DrawPath *path, DrawIndex type, const DrawBuffer *indices,
                         uint32_t count, int32_t base_vertex);
/// One instance's entry of the second vertex stream, which steps once an instance: where the
/// instance goes, added to each vertex's position, and its colour (scene_instanced.vert.glsl).
typedef struct {
    float place[4];
    float tint[4];
} DrawInstance;
/// @p bytes of DrawInstance at @p instances, for the instanced draws after it. After
/// DrawPathVertices, which it adds a stream to.
void DrawPathInstances(DrawPath *path, const DrawBuffer *instances, uint32_t bytes);
/// What an instanced draw draws with: nothing (DrawPathDraw's vertices), or indices.
typedef struct {
    bool indexed;
    DrawIndex type;
    const DrawBuffer *indices;
} DrawElements;
/// A triangle list of @p count vertices from @p first, or of @p count indices of @p elements,
/// @p instances times, the instances' entries from @p base_instance on.
void DrawPathDrawInstanced(DrawPath *path, const DrawElements *elements, uint32_t first,
                           uint32_t count, uint32_t base_instance, uint32_t instances);
/// The same draw with its numbers read by the graphics unit from @p numbers: four words for a
/// draw of vertices (the count, the instances, the first vertex, the base instance), five for
/// an indexed one (the count, the instances, the first index, the base vertex, the base
/// instance).
void DrawPathDrawIndirect(DrawPath *path, const DrawElements *elements,
                          const DrawBuffer *numbers);
/// What the draws that read their numbers make of their vertices or indices, from this call on:
/// triangles as the list starts, quads of four (around the quad, as the graphics unit's own
/// quads go), or one strip of triangles.
typedef enum {
    DRAW_SHAPE_TRIANGLES = 0,
    DRAW_SHAPE_QUADS = 1,
    DRAW_SHAPE_TRIANGLE_STRIP = 2,
} DrawShape;
void DrawPathShape(DrawPath *path, DrawShape shape);
/// With @p on, an index equal to @p index is no vertex: it ends the strip, and the next index
/// starts another. Off as the list starts.
void DrawPathRestart(DrawPath *path, bool on, uint32_t index);
/// As many of the draws whose numbers are @p stride bytes apart at @p numbers as the word at
/// @p count says, @p most at the most, the graphics unit reading the word. NVN's alone: the
/// deko3d way fails it, deko3d having no such draw.
void DrawPathDrawIndirectCount(DrawPath *path, const DrawElements *elements,
                               const DrawBuffer *numbers, const DrawBuffer *count, uint32_t most,
                               uint32_t stride);
/// The texels from (@p source 0, 1) to (2, 3) of @p texture drawn over the target's pixels from
/// (@p destination 0, 1) to (2, 3) with no program, through the texture's own sampler. NVN's
/// alone, as the draw above is.
void DrawPathDrawTexture(DrawPath *path, int texture, const float destination[4],
                         const float source[4]);
/// As DrawPathProgram, with the vertex stage's outputs captured into capture buffer 0 while a
/// capture runs (transform feedback): a vertex's position without its fourth number, then the
/// two numbers of generic output 0 and the four of generic output 1, which for the instanced
/// scene's vertex stage is a GpuVertex. NVN's alone, as every call of a capture below is: the
/// deko3d way fails them, deko3d having no call for a capture.
int DrawPathProgramCapturing(DrawPath *path, const void *vertex_dksh, uint32_t vertex_bytes,
                             const void *fragment_dksh, uint32_t fragment_bytes);
/// @p bytes at @p into as capture buffer 0, by NVN's call for one buffer or, with @p several,
/// by its call for several.
void DrawPathCaptureInto(DrawPath *path, const DrawBuffer *into, uint32_t bytes, bool several);
typedef enum {
    DRAW_CAPTURE_BEGIN,
    DRAW_CAPTURE_PAUSE,
    DRAW_CAPTURE_RESUME,
    DRAW_CAPTURE_END,
} DrawCaptureStep;
/// A step of the capture whose state the graphics unit keeps at @p control, 32 bytes of nought
/// before the capture begins: the draws between a begin or a resume and the next pause or end
/// are captured.
void DrawPathCapture(DrawPath *path, DrawCaptureStep step, const DrawBuffer *control);
/// Triangles of as many vertices as the capture of @p control wrote, from the vertices bound.
void DrawPathDrawCaptured(DrawPath *path, const DrawBuffer *control);
/// Ends the list, submits it and waits for the graphics unit. False when anything since
/// DrawPathBegin failed, with DrawPathFailure saying what.
bool DrawPathEnd(DrawPath *path);

/// The colour target's pixels, four bytes each, the top row first: valid until the next call on
/// @p path. NULL when the copy failed.
const uint8_t *DrawPathRead(DrawPath *path);

#ifdef __cplusplus
}
#endif
