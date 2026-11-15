// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "drawpath.h"

// What a way gives the calls of drawpath.h: one function for each, on its own state.
typedef struct {
    void (*close)(DrawPath *path);
    int (*program)(DrawPath *path, const void *vertex_dksh, uint32_t vertex_bytes,
                   const void *fragment_dksh, uint32_t fragment_bytes);
    int (*texture)(DrawPath *path, uint32_t width, uint32_t height, const uint8_t *texels,
                   bool linear);
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
} DrawPathWay;

/// What every way starts with: its functions, what it was asked for and its failure.
struct DrawPath {
    const DrawPathWay *way;
    DrawPathSpec spec;
    char failure[256];
};

/// Keeps the first failure since the way came up, as the report wants the cause and not what
/// followed from it.
void DrawPathFail(DrawPath *path, const char *format, ...) __attribute__((format(printf, 2, 3)));

/// The two ways: each brings itself up for @p spec, or says why not.
DrawPath *DrawPathOpenDeko3d(const DrawPathSpec *spec, char *why, size_t why_size);
/// @p own: the layer's own functions, not the ones the program looks up.
DrawPath *DrawPathOpenNvn(const DrawPathSpec *spec, bool own, char *why, size_t why_size);

/// Where the texture handle of slot @p slot sits in a stage's handles, for both ways.
#define DRAW_PATH_HANDLE_FIRST 0x20u
#define DRAW_PATH_HANDLE_BYTES 0x200u
