// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "drawpath_internal.h"

// The calls of drawpath.h, handed to the way that is up, and the deko3d way itself, which is
// gpu.h's context with the two things a scene of both ways adds: uniform buffers numbered as
// NVN numbers them, and each stage's texture handles in a uniform buffer of their own.

const char *DrawPathName(DrawPathKind kind) {
    switch (kind) {
    case DRAW_PATH_LAYER:
        return "the layer's own NVN";
    case DRAW_PATH_NVN:
        return "the program's NVN";
    default:
        return "deko3d";
    }
}

void DrawPathFail(DrawPath *path, const char *format, ...) {
    if (path->failure[0] != '\0') {
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(path->failure, sizeof(path->failure), format, args);
    va_end(args);
}

DrawPath *DrawPathOpen(DrawPathKind kind, const DrawPathSpec *spec, char *why, size_t why_size) {
    if (spec->programs > DRAW_PATH_PROGRAMS || spec->textures > DRAW_PATH_TEXTURES) {
        snprintf(why, why_size,
                 "a scene of %u programs and %u textures, past the %u and %u a way has",
                 (unsigned)spec->programs, (unsigned)spec->textures, (unsigned)DRAW_PATH_PROGRAMS,
                 (unsigned)DRAW_PATH_TEXTURES);
        return NULL;
    }
    return kind == DRAW_PATH_DEKO3D ? DrawPathOpenDeko3d(spec, why, why_size)
                                    : DrawPathOpenNvn(spec, kind == DRAW_PATH_LAYER, why, why_size);
}

void DrawPathClose(DrawPath *path) {
    if (path != NULL) {
        path->way->close(path);
    }
}

const char *DrawPathFailure(const DrawPath *path) {
    return path->failure;
}

int DrawPathProgram(DrawPath *path, const void *vertex_dksh, uint32_t vertex_bytes,
                    const void *fragment_dksh, uint32_t fragment_bytes) {
    return path->way->program(path, vertex_dksh, vertex_bytes, fragment_dksh, fragment_bytes);
}

int DrawPathTextureSampled(DrawPath *path, uint32_t width, uint32_t height,
                           const uint8_t *texels, const DrawSampler *sampler) {
    return path->way->texture(path, width, height, texels, sampler);
}

int DrawPathTexture(DrawPath *path, uint32_t width, uint32_t height, const uint8_t *texels,
                    bool linear) {
    const DrawSampler sampler = { linear, DRAW_WRAP_CLAMP_TO_EDGE, { 0.0f, 0.0f, 0.0f, 0.0f } };
    return path->way->texture(path, width, height, texels, &sampler);
}

bool DrawPathAlloc(DrawPath *path, uint32_t bytes, DrawBuffer *out) {
    return path->way->alloc(path, bytes, out);
}

void DrawPathBegin(DrawPath *path) {
    path->way->begin(path);
}

void DrawPathClear(DrawPath *path, const float rgba[4]) {
    path->way->clear(path, rgba);
}

void DrawPathClearDepth(DrawPath *path, float depth) {
    path->way->clear_depth(path, depth);
}

void DrawPathDepth(DrawPath *path, bool test, bool write) {
    path->way->depth(path, test, write);
}

void DrawPathUseProgram(DrawPath *path, int program) {
    path->way->use_program(path, program);
}

void DrawPathVertices(DrawPath *path, const DrawBuffer *vertices, uint32_t bytes) {
    path->way->vertices(path, vertices, bytes);
}

void DrawPathUniform(DrawPath *path, DrawStage stage, uint32_t slot, const DrawBuffer *buffer,
                     uint32_t bytes) {
    path->way->uniform(path, stage, slot, buffer, bytes);
}

void DrawPathBindTexture(DrawPath *path, DrawStage stage, uint32_t slot, int texture) {
    path->way->bind_texture(path, stage, slot, texture);
}

void DrawPathDraw(DrawPath *path, uint32_t first, uint32_t count) {
    path->way->draw(path, first, count);
}

void DrawPathUpdate(DrawPath *path, const DrawBuffer *buffer, uint32_t buffer_bytes,
                    uint32_t offset, const void *data, uint32_t bytes) {
    path->way->update(path, buffer, buffer_bytes, offset, data, bytes);
}

void DrawPathDrawIndexed(DrawPath *path, DrawIndex type, const DrawBuffer *indices,
                         uint32_t count, int32_t base_vertex) {
    path->way->draw_indexed(path, type, indices, count, base_vertex);
}

bool DrawPathEnd(DrawPath *path) {
    return path->way->end(path);
}

const uint8_t *DrawPathRead(DrawPath *path) {
    return path->way->read(path);
}

// ---- the deko3d way

typedef struct {
    DrawPath base;
    GpuContext gpu;
    bool up;
    DkShader vertex[DRAW_PATH_PROGRAMS];
    DkShader fragment[DRAW_PATH_PROGRAMS];
    uint32_t programs;
    DkImage images[DRAW_PATH_TEXTURES];
    DkImageLayout layouts[DRAW_PATH_TEXTURES];
    uint32_t textures;
    /// The textures' descriptors and one sampler each, where deko3d's descriptor sets point.
    DkImageDescriptor *image_set;
    DkSamplerDescriptor *sampler_set;
    DkGpuAddr image_set_address;
    DkGpuAddr sampler_set_address;
    /// Each stage's handles, as a program of both ways reads them: uniform buffer 0, the handle
    /// of slot r at 0x20 + 8 * r. Changed since the last draw copied them.
    uint8_t handles[2][DRAW_PATH_HANDLE_BYTES];
    uint8_t dirty;
    int program;
} Deko3dPath;

static Deko3dPath deko3d_path;

static Deko3dPath *Dk(DrawPath *path) {
    return (Deko3dPath *)(void *)path;
}

/// Carries the context's failure, a sentence gpu.c wrote, into the way's.
static bool DkFailed(Deko3dPath *dk, const char *what) {
    if (dk->gpu.failure[0] != '\0') {
        DrawPathFail(&dk->base, "%s: %s", what, dk->gpu.failure);
        return true;
    }
    return false;
}

static void DkClose(DrawPath *path) {
    Deko3dPath *const dk = Dk(path);
    if (dk->up) {
        GpuClose(&dk->gpu);
        dk->up = false;
    }
}

static int DkProgram(DrawPath *path, const void *vertex_dksh, uint32_t vertex_bytes,
                     const void *fragment_dksh, uint32_t fragment_bytes) {
    Deko3dPath *const dk = Dk(path);
    if (dk->programs >= path->spec.programs) {
        DrawPathFail(path, "program %u of the %u the scene asked for", (unsigned)dk->programs,
                     (unsigned)path->spec.programs);
        return -1;
    }
    if (!GpuLoadShader(&dk->gpu, &dk->vertex[dk->programs], vertex_dksh, vertex_bytes) ||
        !GpuLoadShader(&dk->gpu, &dk->fragment[dk->programs], fragment_dksh, fragment_bytes)) {
        DkFailed(dk, "a program's shaders");
        return -1;
    }
    return (int)dk->programs++;
}

static int DkTexture(DrawPath *path, uint32_t width, uint32_t height, const uint8_t *texels,
                     const DrawSampler *how) {
    Deko3dPath *const dk = Dk(path);
    const uint32_t index = dk->textures;
    if (index >= path->spec.textures) {
        DrawPathFail(path, "texture %u of the %u the scene asked for", (unsigned)index,
                     (unsigned)path->spec.textures);
        return -1;
    }
    if (!GpuCreateTexture(&dk->gpu, &dk->images[index], &dk->layouts[index],
                          DkImageFormat_RGBA8_Unorm, width, height) ||
        !GpuUploadTexture(&dk->gpu, &dk->images[index], DkImageFormat_RGBA8_Unorm, width, height,
                          texels)) {
        DkFailed(dk, "a texture");
        return -1;
    }
    DkImageView view;
    dkImageViewDefaults(&view, &dk->images[index]);
    dkImageDescriptorInitialize(&dk->image_set[index], &view, false, false);
    DkSampler sampler;
    dkSamplerDefaults(&sampler);
    static const DkWrapMode WRAPS[] = { DkWrapMode_ClampToEdge, DkWrapMode_Repeat,
                                        DkWrapMode_MirroredRepeat, DkWrapMode_ClampToBorder };
    sampler.minFilter = how->linear ? DkFilter_Linear : DkFilter_Nearest;
    sampler.magFilter = sampler.minFilter;
    sampler.mipFilter = DkMipFilter_None;
    sampler.wrapMode[0] = sampler.wrapMode[1] = sampler.wrapMode[2] = WRAPS[how->wrap];
    for (int i = 0; i < 4; ++i) {
        sampler.borderColor[i].value_f = how->border[i];
    }
    dkSamplerDescriptorInitialize(&dk->sampler_set[index], &sampler);
    return (int)dk->textures++;
}

static bool DkAlloc(DrawPath *path, uint32_t bytes, DrawBuffer *out) {
    Deko3dPath *const dk = Dk(path);
    DkGpuAddr address = 0;
    out->bytes = GpuAllocData(&dk->gpu, bytes, DK_UNIFORM_BUF_ALIGNMENT, &address);
    out->address = address;
    if (out->bytes == NULL) {
        DrawPathFail(path, "no %u bytes left of the data memory the scene asked for",
                     (unsigned)bytes);
        return false;
    }
    return true;
}

static void DkBegin(DrawPath *path) {
    Deko3dPath *const dk = Dk(path);
    GpuBeginPass(&dk->gpu);
    dkCmdBufBindImageDescriptorSet(dk->gpu.cmdbuf, dk->image_set_address, DRAW_PATH_TEXTURES);
    dkCmdBufBindSamplerDescriptorSet(dk->gpu.cmdbuf, dk->sampler_set_address, DRAW_PATH_TEXTURES);
    // GpuBeginPass leaves the depth test and write as deko3d's defaults when the context has a
    // depth target; a scene of both ways starts with them off and says when it wants them.
    path->way->depth(path, false, false);
    dk->dirty = 3;
    dk->program = -1;
}

static void DkClear(DrawPath *path, const float rgba[4]) {
    dkCmdBufClearColorFloat(Dk(path)->gpu.cmdbuf, 0, DkColorMask_RGBA, rgba[0], rgba[1], rgba[2],
                            rgba[3]);
}

static void DkClearDepth(DrawPath *path, float depth) {
    if (path->spec.depth) {
        dkCmdBufClearDepthStencil(Dk(path)->gpu.cmdbuf, true, depth, 0, 0);
    }
}

static void DkDepth(DrawPath *path, bool test, bool write) {
    DkDepthStencilState state;
    dkDepthStencilStateDefaults(&state);
    state.depthTestEnable = test;
    state.depthWriteEnable = write;
    state.stencilTestEnable = false;
    state.depthCompareOp = DkCompareOp_Less;
    dkCmdBufBindDepthStencilState(Dk(path)->gpu.cmdbuf, &state);
}

static void DkUseProgram(DrawPath *path, int program) {
    Deko3dPath *const dk = Dk(path);
    if (program < 0 || (uint32_t)program >= dk->programs) {
        DrawPathFail(path, "program %d, where the scene made %u", program, (unsigned)dk->programs);
        return;
    }
    dk->program = program;
}

static void DkVertices(DrawPath *path, const DrawBuffer *vertices, uint32_t bytes) {
    Deko3dPath *const dk = Dk(path);
    if (dk->program < 0) {
        DrawPathFail(path, "vertices bound before a program");
        return;
    }
    GpuBindVertices(&dk->gpu, &dk->vertex[dk->program], &dk->fragment[dk->program],
                    vertices->address, bytes);
}

static DkStage DkStageOf(DrawStage stage) {
    return stage == DRAW_STAGE_VERTEX ? DkStage_Vertex : DkStage_Fragment;
}

static void DkUniform(DrawPath *path, DrawStage stage, uint32_t slot, const DrawBuffer *buffer,
                      uint32_t bytes) {
    // NVN's slot r is the constant buffer deko3d binds as uniform buffer r + 1.
    dkCmdBufBindUniformBuffer(Dk(path)->gpu.cmdbuf, DkStageOf(stage), slot + 1u, buffer->address,
                              bytes);
}

static void DkBindTexture(DrawPath *path, DrawStage stage, uint32_t slot, int texture) {
    Deko3dPath *const dk = Dk(path);
    if (slot >= DRAW_PATH_SLOTS || texture < 0 || (uint32_t)texture >= dk->textures) {
        DrawPathFail(path, "texture %d in slot %u, where the scene made %u textures and a stage "
                           "has %u slots",
                     texture, (unsigned)slot, (unsigned)dk->textures, (unsigned)DRAW_PATH_SLOTS);
        return;
    }
    const uint64_t handle = dkMakeTextureHandle((uint32_t)texture, (uint32_t)texture);
    memcpy(dk->handles[stage] + DRAW_PATH_HANDLE_FIRST + 8u * slot, &handle, 8);
    dk->dirty |= (uint8_t)(1u << stage);
}

/// A copy of each changed stage's handles for the draw about to be recorded: the list runs
/// later, and each draw has to find the handles it was recorded with. False when the data
/// memory has no room for one.
static bool DkHandles(DrawPath *path) {
    Deko3dPath *const dk = Dk(path);
    for (uint32_t stage = 0; stage < 2u; ++stage) {
        if ((dk->dirty >> stage & 1u) == 0u) {
            continue;
        }
        DrawBuffer copy;
        if (!DkAlloc(path, DRAW_PATH_HANDLE_BYTES, &copy)) {
            return false;
        }
        memcpy(copy.bytes, dk->handles[stage], DRAW_PATH_HANDLE_BYTES);
        dkCmdBufBindUniformBuffer(dk->gpu.cmdbuf, DkStageOf((DrawStage)stage), 0, copy.address,
                                  DRAW_PATH_HANDLE_BYTES);
        dk->dirty &= (uint8_t)~(1u << stage);
    }
    return true;
}

static void DkDraw(DrawPath *path, uint32_t first, uint32_t count) {
    if (DkHandles(path)) {
        dkCmdBufDraw(Dk(path)->gpu.cmdbuf, DkPrimitive_Triangles, count, 1, first, 0);
    }
}

static void DkDrawIndexed(DrawPath *path, DrawIndex type, const DrawBuffer *indices,
                          uint32_t count, int32_t base_vertex) {
    static const DkIdxFormat FORMATS[] = { DkIdxFormat_Uint8, DkIdxFormat_Uint16,
                                           DkIdxFormat_Uint32 };
    if (DkHandles(path)) {
        dkCmdBufBindIdxBuffer(Dk(path)->gpu.cmdbuf, FORMATS[type], indices->address);
        dkCmdBufDrawIndexed(Dk(path)->gpu.cmdbuf, DkPrimitive_Triangles, count, 1, 0, base_vertex,
                            0);
    }
}

static void DkUpdate(DrawPath *path, const DrawBuffer *buffer, uint32_t buffer_bytes,
                     uint32_t offset, const void *data, uint32_t bytes) {
    if (offset > buffer_bytes || bytes > buffer_bytes - offset) {
        DrawPathFail(path, "an update of %u bytes at %u of a buffer of %u", (unsigned)bytes,
                     (unsigned)offset, (unsigned)buffer_bytes);
        return;
    }
    dkCmdBufPushData(Dk(path)->gpu.cmdbuf, buffer->address + offset, data, bytes);
}

static bool DkEnd(DrawPath *path) {
    Deko3dPath *const dk = Dk(path);
    GpuSubmit(&dk->gpu);
    DkFailed(dk, "the list");
    return path->failure[0] == '\0';
}

static const uint8_t *DkRead(DrawPath *path) {
    Deko3dPath *const dk = Dk(path);
    const uint8_t *const pixels = GpuReadBack(&dk->gpu);
    if (pixels == NULL) {
        DkFailed(dk, "the readback");
    }
    return pixels;
}

static const DrawPathWay DEKO3D_WAY = {
    DkClose, DkProgram, DkTexture, DkAlloc,       DkBegin, DkClear, DkClearDepth, DkDepth,
    DkUseProgram, DkVertices, DkUniform, DkBindTexture, DkDraw,  DkEnd,   DkRead,
    DkDrawIndexed, DkUpdate,
};

DrawPath *DrawPathOpenDeko3d(const DrawPathSpec *spec, char *why, size_t why_size) {
    Deko3dPath *const dk = &deko3d_path;
    if (dk->up) {
        snprintf(why, why_size, "the deko3d way is already up");
        return NULL;
    }
    memset(dk, 0, sizeof(*dk));
    dk->base.way = &DEKO3D_WAY;
    dk->base.spec = *spec;
    GpuSpec gpu;
    GpuSpecDefaults(&gpu, spec->width, spec->height);
    gpu.depth_format = spec->depth ? DkImageFormat_ZF32 : DkImageFormat_None;
    // A program's two modules, each a multiple of 0x100 bytes of code after deko3d's own skip.
    gpu.code_size = 0x10000u;
    // The scene's own data, the two descriptor sets, and room to stage the textures' texels.
    gpu.data_size = spec->data_bytes + 0x1000u + 2u * spec->texture_bytes + 0x10000u;
    gpu.image_size = 2u * spec->texture_bytes + DRAW_PATH_TEXTURES * 0x20000u;
    gpu.command_size = spec->command_bytes;
    if (!GpuOpenSpec(&dk->gpu, &gpu)) {
        snprintf(why, why_size, "%s", dk->gpu.failure);
        return NULL;
    }
    dk->up = true;
    dk->image_set = GpuAllocData(&dk->gpu, DRAW_PATH_TEXTURES * (uint32_t)sizeof(DkImageDescriptor),
                                 DK_IMAGE_DESCRIPTOR_ALIGNMENT, &dk->image_set_address);
    dk->sampler_set =
        GpuAllocData(&dk->gpu, DRAW_PATH_TEXTURES * (uint32_t)sizeof(DkSamplerDescriptor),
                     DK_SAMPLER_DESCRIPTOR_ALIGNMENT, &dk->sampler_set_address);
    if (dk->image_set == NULL || dk->sampler_set == NULL) {
        snprintf(why, why_size, "no room for the descriptor sets");
        DkClose(&dk->base);
        return NULL;
    }
    memset(dk->image_set, 0, DRAW_PATH_TEXTURES * sizeof(DkImageDescriptor));
    memset(dk->sampler_set, 0, DRAW_PATH_TEXTURES * sizeof(DkSamplerDescriptor));
    dk->program = -1;
    return &dk->base;
}
