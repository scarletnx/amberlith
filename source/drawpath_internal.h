// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "drawpath.h"

// What a way gives the calls of drawpath.h: one function for each, on its own state.
typedef struct {
    void (*close)(DrawPath *path);
    int (*program)(DrawPath *path, const void *vertex_dksh, uint32_t vertex_bytes,
                   const void *fragment_dksh, uint32_t fragment_bytes);
    int (*texture)(DrawPath *path, uint32_t width, uint32_t height, const uint8_t *texels,
                   const DrawSampler *sampler);
    bool (*alloc)(DrawPath *path, uint32_t bytes, DrawBuffer *out);
    void (*begin)(DrawPath *path);
    void (*clear)(DrawPath *path, const float rgba[4]);
    void (*clear_depth)(DrawPath *path, float depth);
    void (*depth)(DrawPath *path, bool test, bool write);
    void (*use_program)(DrawPath *path, int program);
    void (*vertices)(DrawPath *path, const DrawBuffer *vertices, uint32_t bytes);
    void (*uniform)(DrawPath *path, DrawStage stage, uint32_t slot, const DrawBuffer *buffer,
                    uint32_t bytes);
    void (*bind_texture)(DrawPath *path, DrawStage stage, uint32_t slot, int texture);
    void (*draw)(DrawPath *path, uint32_t first, uint32_t count);
    bool (*end)(DrawPath *path);
    const uint8_t *(*read)(DrawPath *path);
    void (*draw_indexed)(DrawPath *path, DrawIndex type, const DrawBuffer *indices,
                         uint32_t count, int32_t base_vertex);
    void (*update)(DrawPath *path, const DrawBuffer *buffer, uint32_t buffer_bytes,
                   uint32_t offset, const void *data, uint32_t bytes);
    void (*instances)(DrawPath *path, const DrawBuffer *instances, uint32_t bytes);
    void (*draw_instanced)(DrawPath *path, const DrawElements *elements, uint32_t first,
                           uint32_t count, uint32_t base_instance, uint32_t instances);
    void (*draw_indirect)(DrawPath *path, const DrawElements *elements,
                          const DrawBuffer *numbers);
    void (*draw_indirect_count)(DrawPath *path, const DrawElements *elements,
                                const DrawBuffer *numbers, const DrawBuffer *count,
                                uint32_t most, uint32_t stride);
    void (*draw_texture)(DrawPath *path, int texture, const float destination[4],
                         const float source[4]);
    int (*program_capturing)(DrawPath *path, const void *vertex_dksh, uint32_t vertex_bytes,
                             const void *fragment_dksh, uint32_t fragment_bytes);
    void (*capture_into)(DrawPath *path, const DrawBuffer *into, uint32_t bytes, bool several);
    void (*capture)(DrawPath *path, DrawCaptureStep step, const DrawBuffer *control);
    void (*draw_captured)(DrawPath *path, const DrawBuffer *control);
    void (*restart)(DrawPath *path, bool on, uint32_t index);
} DrawPathWay;

/// What every way starts with: its functions, what it was asked for and its failure.
struct DrawPath {
    const DrawPathWay *way;
    DrawPathSpec spec;
    char failure[256];
    /// What the draws that read their numbers draw (DrawPathShape), triangles until a scene says.
    DrawShape shape;
};

/// Keeps the first failure since the way came up, as the report wants the cause and not what
/// followed from it.
void DrawPathFail(DrawPath *path, const char *format, ...) __attribute__((format(printf, 2, 3)));

/// The two ways: each brings itself up for @p spec, or says why not.
DrawPath *DrawPathOpenDeko3d(const DrawPathSpec *spec, char *why, size_t why_size);
/// @p own: the layer's own functions, not the ones the program looks up.
DrawPath *DrawPathOpenNvn(const DrawPathSpec *spec, bool own, char *why, size_t why_size);

/// What a capturing program's vertex stage gives each vertex, for both ways: the words of three
/// outputs, each { output, first word, count }, output 0 the position and 0x20 + n generic output
/// n (NVN_LAYER_CAPTURE_POSITION, NVN_LAYER_CAPTURE_GENERIC): the position's first three words,
/// two of generic output 0 and four of generic output 1, which is a GpuVertex.
#define DRAW_PATH_CAPTURE_GROUPS { { 0x00, 0, 3 }, { 0x20, 0, 2 }, { 0x21, 0, 4 } }
#define DRAW_PATH_CAPTURE_GROUP_COUNT 3

/// Where the texture handle of slot @p slot sits in a stage's handles, for both ways.
#define DRAW_PATH_HANDLE_FIRST 0x20u
#define DRAW_PATH_HANDLE_BYTES 0x200u
