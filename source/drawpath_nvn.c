// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nvnlayer.h>

#include "drawpath_internal.h"
#include "drawpath_nvn.h"

// The NVN ways: the calls of drawpath.h made through NVN's functions. DRAW_PATH_NVN has every
// function looked up by name as a game looks its driver's up (NvnLoadProcs), and none of the
// layer's code called by name: under ScarletNX each call is then ScarletNX's own NVN, and on the
// console and in an emulator that runs the graphics unit, the layer's deko3d commands.
// DRAW_PATH_LAYER has the layer's own functions (NvnLayerOwnProcs), which record deko3d commands
// on every machine.
//
// The device is set to the upper left origin and a near plane at zero, the conventions the
// deko3d way's device has, so a scene's vertices mean the same pixels down both ways.

// A pool's storage is a whole number of pages; texture storage wants up to 64 KiB boundaries
// under a driver's rules (ScarletNX answers a depth target's so), which the storage's own
// alignment has to allow.
#define POOL_PAGE 0x1000u
#define POOL_ALIGNMENT 0x10000u
#define CONTROL_BYTES 0x4000u
#define CODE_BYTES 0x10000u
// What a copy leaves when it never lands: no pixel a scene judges is four of these.
#define READ_PRESET 0xa5u

/// How many stages a scene may ask for by themselves.
#define DRAW_PATH_MORE_STAGES 4u

typedef struct {
    NVNmemoryPool pool;
    void *storage;
    size_t size;
    size_t used;
    bool up;
} Pool;

#define DEFERRED_RANGES 64u
#define SCRATCH_BYTES 0x1000u

typedef struct {
    DrawPath base;
    NvnProcs nvn;
    bool device_up;
    bool queue_up;
    bool command_up;
    bool target_up;
    bool depth_up;
    bool texture_pool_up;
    bool sampler_pool_up;
    _Alignas(8) NVNdevice device;
    _Alignas(8) NVNqueue queue;
    _Alignas(8) NVNcommandBuffer command;
    _Alignas(8) NVNtexture target;
    _Alignas(8) NVNtexture depth;
    _Alignas(8) NVNtexturePool texture_pool;
    _Alignas(8) NVNsamplerPool sampler_pool;
    _Alignas(8) Pool command_pool;
    _Alignas(8) Pool data_pool;
    _Alignas(8) Pool read_pool;
    _Alignas(8) Pool image_pool;
    _Alignas(8) Pool code_pool;
    _Alignas(8) Pool descriptor_pool;
    void *control;
    /// The first texture and sampler descriptors a program may use, as the device says.
    int texture_base;
    int sampler_base;
    _Alignas(8) NVNprogram programs[DRAW_PATH_PROGRAMS];
    NVNshaderData stages[DRAW_PATH_PROGRAMS][2];
    /// The control sections of the stages a scene asked for by themselves (DrawPathNvnStage).
    void *more_sections[DRAW_PATH_MORE_STAGES];
    uint32_t more_count;
    void *sections[DRAW_PATH_PROGRAMS][2];
    uint32_t program_count;
    _Alignas(8) NVNtexture textures[DRAW_PATH_TEXTURES];
    _Alignas(8) NVNsampler samplers[DRAW_PATH_TEXTURES];
    NVNtextureHandle handles[DRAW_PATH_TEXTURES];
    uint32_t texture_count;
    bool recording;
    /// The ranges the deferred binds of the list being recorded point at: each bind its own,
    /// since a driver reads them when the list runs.
    NVNbufferRange ranges[DEFERRED_RANGES];
    uint32_t range_count;
    /// The page of the data pool the plural way lends as shader scratch memory.
    size_t scratch_at;
} NvnPath;

static NvnPath nvn_path;

static NvnPath *Nv(DrawPath *path) {
    return (NvnPath *)(void *)path;
}

/// The reason a step failed: the layer's, when it gave one, else @p fallback. Under ScarletNX the
/// layer never runs, so the fallback is all a failure there carries.
static const char *Why(const char *fallback) {
    return NvnLayerError()[0] != '\0' ? NvnLayerError() : fallback;
}

/// Carries a complaint of the layer's, if it made one, into the way's failure.
static bool LayerFailed(NvnPath *nv, const char *what) {
    if (NvnLayerError()[0] != '\0') {
        DrawPathFail(&nv->base, "%s: %s", what, NvnLayerError());
        return true;
    }
    return false;
}

static size_t RoundUp(size_t value, size_t to) {
    return (value + to - 1u) / to * to;
}

static bool MakePool(NvnPath *nv, Pool *pool, size_t size, int flags) {
    pool->size = RoundUp(size, POOL_PAGE);
    pool->storage = aligned_alloc(POOL_ALIGNMENT, RoundUp(pool->size, POOL_ALIGNMENT));
    if (pool->storage == NULL) {
        return false;
    }
    memset(pool->storage, 0, pool->size);
    _Alignas(8) NVNmemoryPoolBuilder builder;
    nv->nvn.nvnMemoryPoolBuilderSetDefaults(&builder);
    nv->nvn.nvnMemoryPoolBuilderSetDevice(&builder, &nv->device);
    nv->nvn.nvnMemoryPoolBuilderSetStorage(&builder, pool->storage, pool->size);
    nv->nvn.nvnMemoryPoolBuilderSetFlags(&builder, flags);
    pool->up = nv->nvn.nvnMemoryPoolInitialize(&pool->pool, &builder);
    pool->used = 0;
    return pool->up;
}

static void DropPool(NvnPath *nv, Pool *pool) {
    if (pool->up) {
        nv->nvn.nvnMemoryPoolFinalize(&pool->pool);
        pool->up = false;
    }
    free(pool->storage);
    pool->storage = NULL;
}

/// @p bytes of @p pool on a boundary of @p alignment: their offset, or SIZE_MAX when it is full.
static size_t Carve(Pool *pool, size_t bytes, size_t alignment) {
    const size_t at = RoundUp(pool->used, alignment);
    if (at > pool->size || bytes > pool->size - at) {
        return SIZE_MAX;
    }
    pool->used = at + bytes;
    return at;
}

/// A 2D texture of @p format in the image pool, wherever the builder says its storage goes.
static bool MakeTexture(NvnPath *nv, NVNtexture *texture, NVNformat format, int width,
                        int height, char *why, size_t why_size) {
    _Alignas(8) NVNtextureBuilder builder;
    memset(&builder, 0, sizeof(builder));
    nv->nvn.nvnTextureBuilderSetDevice(&builder, &nv->device);
    nv->nvn.nvnTextureBuilderSetDefaults(&builder);
    nv->nvn.nvnTextureBuilderSetFlags(&builder, 0);
    nv->nvn.nvnTextureBuilderSetTarget(&builder, NVN_TEXTURE_TARGET_2D);
    nv->nvn.nvnTextureBuilderSetFormat(&builder, format);
    nv->nvn.nvnTextureBuilderSetSize2D(&builder, width, height);
    const size_t size = nv->nvn.nvnTextureBuilderGetStorageSize(&builder);
    const size_t alignment = nv->nvn.nvnTextureBuilderGetStorageAlignment(&builder);
    if (size == 0u || alignment == 0u || alignment > POOL_ALIGNMENT) {
        snprintf(why, why_size, "a %dx%d texture's storage is %zu bytes at an alignment of %zu: %s",
                 width, height, size, alignment, Why("an answer the image pool cannot hold"));
        return false;
    }
    const size_t at = Carve(&nv->image_pool, size, alignment);
    if (at == SIZE_MAX) {
        snprintf(why, why_size, "no %zu bytes left in the image pool's %zu for a %dx%d texture",
                 size, nv->image_pool.size, width, height);
        return false;
    }
    nv->nvn.nvnTextureBuilderSetStorage(&builder, &nv->image_pool.pool, (ptrdiff_t)at);
    if (!nv->nvn.nvnTextureInitialize(texture, &builder)) {
        snprintf(why, why_size, "nvnTextureInitialize: %s", Why("false"));
        return false;
    }
    return true;
}

/// Submits the open recording and waits for the graphics unit.
static bool Finish(NvnPath *nv, const char *what) {
    const NVNcommandHandle list = nv->nvn.nvnCommandBufferEndRecording(&nv->command);
    nv->recording = false;
    if (list == 0u) {
        DrawPathFail(&nv->base, "%s: nvnCommandBufferEndRecording gave no list: %s", what,
                     Why("0"));
        return false;
    }
    nv->nvn.nvnQueueSubmitCommands(&nv->queue, 1, &list);
    nv->nvn.nvnQueueFinish(&nv->queue);
    return !LayerFailed(nv, what);
}

static void NvClose(DrawPath *path) {
    NvnPath *const nv = Nv(path);
    if (nv->queue_up) {
        nv->nvn.nvnQueueFinish(&nv->queue);
    }
    for (uint32_t i = 0; i < nv->program_count; ++i) {
        nv->nvn.nvnProgramFinalize(&nv->programs[i]);
    }
    for (uint32_t i = 0; i < DRAW_PATH_PROGRAMS; ++i) {
        free(nv->sections[i][0]);
        free(nv->sections[i][1]);
        nv->sections[i][0] = nv->sections[i][1] = NULL;
    }
    nv->program_count = 0;
    for (uint32_t i = 0; i < nv->texture_count; ++i) {
        nv->nvn.nvnSamplerFinalize(&nv->samplers[i]);
        nv->nvn.nvnTextureFinalize(&nv->textures[i]);
    }
    nv->texture_count = 0;
    if (nv->sampler_pool_up) {
        nv->nvn.nvnSamplerPoolFinalize(&nv->sampler_pool);
        nv->sampler_pool_up = false;
    }
    if (nv->texture_pool_up) {
        nv->nvn.nvnTexturePoolFinalize(&nv->texture_pool);
        nv->texture_pool_up = false;
    }
    if (nv->command_up) {
        nv->nvn.nvnCommandBufferFinalize(&nv->command);
        nv->command_up = false;
    }
    for (uint32_t i = 0; i < nv->more_count; ++i) {
        free(nv->more_sections[i]);
        nv->more_sections[i] = NULL;
    }
    nv->more_count = 0;
    free(nv->control);
    nv->control = NULL;
    if (nv->depth_up) {
        nv->nvn.nvnTextureFinalize(&nv->depth);
        nv->depth_up = false;
    }
    if (nv->target_up) {
        nv->nvn.nvnTextureFinalize(&nv->target);
        nv->target_up = false;
    }
    DropPool(nv, &nv->descriptor_pool);
    DropPool(nv, &nv->code_pool);
    DropPool(nv, &nv->image_pool);
    DropPool(nv, &nv->read_pool);
    DropPool(nv, &nv->data_pool);
    DropPool(nv, &nv->command_pool);
    if (nv->queue_up) {
        nv->nvn.nvnQueueFinalize(&nv->queue);
        nv->queue_up = false;
    }
    if (nv->device_up) {
        nv->nvn.nvnDeviceFinalize(&nv->device);
        nv->device_up = false;
    }
}

/// One stage's shader data from @p module, a whole DKSH: its control section made and kept in
/// @p section, which stays for as long as the program does, the layer keeping its own record of
/// the stage in it, and its code copied into the code pool. False with the way's failure set.
static bool Stage(NvnPath *nv, const void *module, uint32_t bytes, const char *which,
                  void **section, NVNshaderData *stage) {
    uint8_t *const code_memory = nv->nvn.nvnMemoryPoolMap(&nv->code_pool.pool);
    const NVNbufferAddress code_address =
        nv->nvn.nvnMemoryPoolGetBufferAddress(&nv->code_pool.pool);
    *section = aligned_alloc(8u, RoundUp(NVN_LAYER_CONTROL_BYTES, 8u));
    NvnLayerShaderCode code;
    if (*section == NULL || code_memory == NULL ||
        !NvnLayerShaderControl(module, bytes, *section, &code)) {
        DrawPathFail(&nv->base, "a program's %s stage: %s", which,
                     Why("no memory for its control section, or no code pool mapping"));
        return false;
    }
    const size_t at = Carve(&nv->code_pool, code.code_bytes, 0x100u);
    if (at == SIZE_MAX) {
        DrawPathFail(&nv->base, "no 0x%x bytes left in the code pool for a program's stage",
                     (unsigned)code.code_bytes);
        return false;
    }
    memcpy(code_memory + at, code.code, code.code_bytes);
    stage->data = code_address + at;
    stage->control = *section;
    return true;
}

static int NvProgram(DrawPath *path, const void *vertex_dksh, uint32_t vertex_bytes,
                     const void *fragment_dksh, uint32_t fragment_bytes) {
    NvnPath *const nv = Nv(path);
    const uint32_t index = nv->program_count;
    if (index >= path->spec.programs) {
        DrawPathFail(path, "program %u of the %u the scene asked for", (unsigned)index,
                     (unsigned)path->spec.programs);
        return -1;
    }
    const void *const modules[2] = { vertex_dksh, fragment_dksh };
    const uint32_t sizes[2] = { vertex_bytes, fragment_bytes };
    NVNshaderData stages[2];
    for (int i = 0; i < 2; ++i) {
        if (!Stage(nv, modules[i], sizes[i], i == 0 ? "vertex" : "fragment",
                   &nv->sections[index][i], &stages[i])) {
            return -1;
        }
    }
    if (!nv->nvn.nvnProgramInitialize(&nv->programs[index], &nv->device)) {
        DrawPathFail(path, "nvnProgramInitialize: %s", Why("false"));
        return -1;
    }
    nv->program_count = index + 1u;
    memcpy(nv->stages[index], stages, sizeof(stages));
    if (!nv->nvn.nvnProgramSetShaders(&nv->programs[index], 2, stages)) {
        DrawPathFail(path, "nvnProgramSetShaders: %s", Why("false"));
        return -1;
    }
    return (int)index;
}

static int NvTexture(DrawPath *path, uint32_t width, uint32_t height, const uint8_t *texels,
                     const DrawSampler *how) {
    static const NVNwrapMode WRAPS[] = { NVN_WRAP_MODE_CLAMP_TO_EDGE, NVN_WRAP_MODE_REPEAT,
                                         NVN_WRAP_MODE_MIRRORED_REPEAT,
                                         NVN_WRAP_MODE_CLAMP_TO_BORDER };
    NvnPath *const nv = Nv(path);
    const uint32_t index = nv->texture_count;
    if (index >= path->spec.textures) {
        DrawPathFail(path, "texture %u of the %u the scene asked for", (unsigned)index,
                     (unsigned)path->spec.textures);
        return -1;
    }
    char why[200];
    if (!MakeTexture(nv, &nv->textures[index], NVN_FORMAT_RGBA8, (int)width, (int)height, why,
                     sizeof(why))) {
        DrawPathFail(path, "%s", why);
        return -1;
    }
    _Alignas(8) NVNsamplerBuilder builder;
    memset(&builder, 0, sizeof(builder));
    nv->nvn.nvnSamplerBuilderSetDevice(&builder, &nv->device);
    nv->nvn.nvnSamplerBuilderSetDefaults(&builder);
    nv->nvn.nvnSamplerBuilderSetMinMagFilter(
        &builder, how->linear ? NVN_MIN_FILTER_LINEAR : NVN_MIN_FILTER_NEAREST,
        how->linear ? NVN_MAG_FILTER_LINEAR : NVN_MAG_FILTER_NEAREST);
    nv->nvn.nvnSamplerBuilderSetWrapMode(&builder, WRAPS[how->wrap], WRAPS[how->wrap],
                                        WRAPS[how->wrap]);
    nv->nvn.nvnSamplerBuilderSetBorderColor(&builder, how->border);
    if (!nv->nvn.nvnSamplerInitialize(&nv->samplers[index], &builder)) {
        nv->nvn.nvnTextureFinalize(&nv->textures[index]);
        DrawPathFail(path, "nvnSamplerInitialize: %s", Why("false"));
        return -1;
    }
    nv->texture_count = index + 1u;

    // The texels go through the data pool and a copy, as a game uploads a texture.
    const size_t bytes = (size_t)width * height * 4u;
    const size_t at = Carve(&nv->data_pool, bytes, 0x100u);
    uint8_t *const data = nv->nvn.nvnMemoryPoolMap(&nv->data_pool.pool);
    if (at == SIZE_MAX || data == NULL) {
        DrawPathFail(path, "no %zu bytes left in the data pool to stage a texture's texels: %s",
                     bytes, Why("the scene asked for too few texture bytes"));
        return -1;
    }
    memcpy(data + at, texels, bytes);
    nv->nvn.nvnCommandBufferBeginRecording(&nv->command);
    const NVNcopyRegion region = { 0, 0, 0, (int)width, (int)height, 1 };
    nv->nvn.nvnCommandBufferCopyBufferToTexture(
        &nv->command, nv->nvn.nvnMemoryPoolGetBufferAddress(&nv->data_pool.pool) + at,
        &nv->textures[index], NULL, &region, NVN_COPY_FLAGS_NONE);
    if (!Finish(nv, "a texture's upload")) {
        return -1;
    }
    const int texture_id = nv->texture_base + (int)index;
    const int sampler_id = nv->sampler_base + (int)index;
    nv->nvn.nvnTexturePoolRegisterTexture(&nv->texture_pool, texture_id, &nv->textures[index],
                                         NULL);
    nv->nvn.nvnSamplerPoolRegisterSampler(&nv->sampler_pool, sampler_id, &nv->samplers[index]);
    nv->handles[index] = nv->nvn.nvnDeviceGetTextureHandle(&nv->device, texture_id, sampler_id);
    if (LayerFailed(nv, "a texture's descriptors")) {
        return -1;
    }
    return (int)index;
}

static bool NvAlloc(DrawPath *path, uint32_t bytes, DrawBuffer *out) {
    NvnPath *const nv = Nv(path);
    const size_t at = Carve(&nv->data_pool, bytes, 0x100u);
    uint8_t *const data = nv->nvn.nvnMemoryPoolMap(&nv->data_pool.pool);
    if (at == SIZE_MAX || data == NULL) {
        DrawPathFail(path, "no %u bytes left of the data memory the scene asked for",
                     (unsigned)bytes);
        return false;
    }
    out->bytes = data + at;
    out->address = nv->nvn.nvnMemoryPoolGetBufferAddress(&nv->data_pool.pool) + at;
    return true;
}

static void NvDepth(DrawPath *path, bool test, bool write) {
    NvnPath *const nv = Nv(path);
    _Alignas(8) NVNdepthStencilState state;
    nv->nvn.nvnDepthStencilStateSetDefaults(&state);
    nv->nvn.nvnDepthStencilStateSetDepthTestEnable(&state, test);
    nv->nvn.nvnDepthStencilStateSetDepthWriteEnable(&state, write);
    nv->nvn.nvnDepthStencilStateSetDepthFunc(&state, NVN_DEPTH_FUNC_LESS);
    nv->nvn.nvnCommandBufferBindDepthStencilState(&nv->command, &state);
}

static void NvBegin(DrawPath *path) {
    NvnPath *const nv = Nv(path);
    const NvnProcs *const n = &nv->nvn;
    n->nvnCommandBufferBeginRecording(&nv->command);
    nv->recording = true;
    nv->range_count = 0;
    if (path->spec.binds == DRAW_BINDS_PLURAL) {
        // The hints: a table no draw here reads, since nothing modulates coverage, and a page of
        // the data pool that nothing else is in.
        static const float table[16] = { 0.0625f, 0.125f, 0.1875f, 0.25f, 0.3125f, 0.375f,
                                         0.4375f, 0.5f,   0.5625f, 0.625f, 0.6875f, 0.75f,
                                         0.8125f, 0.875f, 0.9375f, 1.0f };
        n->nvnCommandBufferBindCoverageModulationTable(&nv->command, table);
        if (nv->scratch_at == SIZE_MAX) {
            nv->scratch_at = Carve(&nv->data_pool, SCRATCH_BYTES, SCRATCH_BYTES);
        }
        if (nv->scratch_at == SIZE_MAX) {
            DrawPathFail(path, "no 0x%x bytes left in the data pool for shader scratch memory",
                         (unsigned)SCRATCH_BYTES);
        } else {
            n->nvnCommandBufferSetShaderScratchMemory(&nv->command, &nv->data_pool.pool,
                                                     (ptrdiff_t)nv->scratch_at, SCRATCH_BYTES);
        }
    }
    // What the deko3d way's lists start with: the work before this list done, and the caches of
    // what the processor wrote since (texels, programs, descriptors) dropped.
    n->nvnCommandBufferBarrier(&nv->command, NVN_BARRIER_ORDER_FRAGMENTS_BIT |
                                                 NVN_BARRIER_INVALIDATE_TEXTURE_BIT |
                                                 NVN_BARRIER_INVALIDATE_SHADER_BIT |
                                                 NVN_BARRIER_INVALIDATE_TEXTURE_DESCRIPTOR_BIT);
    const NVNtexture *const colors[1] = { &nv->target };
    n->nvnCommandBufferSetRenderTargets(&nv->command, 1, colors, NULL,
                                        path->spec.depth ? &nv->depth : NULL, NULL);
    n->nvnCommandBufferSetViewport(&nv->command, 0, 0, (int)path->spec.width,
                                   (int)path->spec.height);
    n->nvnCommandBufferSetScissor(&nv->command, 0, 0, (int)path->spec.width,
                                  (int)path->spec.height);

    // Every state said: an NVN command buffer starts with none, and the two ways have to agree.
    _Alignas(8) NVNblendState blend;
    n->nvnBlendStateSetDefaults(&blend);
    n->nvnCommandBufferBindBlendState(&nv->command, &blend);
    _Alignas(8) NVNchannelMaskState mask;
    n->nvnChannelMaskStateSetDefaults(&mask);
    n->nvnCommandBufferBindChannelMaskState(&nv->command, &mask);
    _Alignas(8) NVNcolorState color;
    n->nvnColorStateSetDefaults(&color);
    n->nvnCommandBufferBindColorState(&nv->command, &color);
    _Alignas(8) NVNmultisampleState multisample;
    n->nvnMultisampleStateSetDefaults(&multisample);
    n->nvnCommandBufferBindMultisampleState(&nv->command, &multisample);
    _Alignas(8) NVNpolygonState polygon;
    n->nvnPolygonStateSetDefaults(&polygon);
    n->nvnPolygonStateSetCullFace(&polygon, NVN_FACE_NONE);
    n->nvnPolygonStateSetFrontFace(&polygon, NVN_FRONT_FACE_CCW);
    n->nvnCommandBufferBindPolygonState(&nv->command, &polygon);
    NvDepth(path, false, false);
    n->nvnCommandBufferSetTexturePool(&nv->command, &nv->texture_pool);
    n->nvnCommandBufferSetSamplerPool(&nv->command, &nv->sampler_pool);
}

static void NvClear(DrawPath *path, const float rgba[4]) {
    NvnPath *const nv = Nv(path);
    nv->nvn.nvnCommandBufferClearColor(&nv->command, 0, rgba, NVN_CLEAR_COLOR_MASK_RGBA);
}

static void NvClearDepth(DrawPath *path, float depth) {
    NvnPath *const nv = Nv(path);
    if (path->spec.depth) {
        nv->nvn.nvnCommandBufferClearDepthStencil(&nv->command, depth, 1, 0, 0);
    }
}

static void NvUseProgram(DrawPath *path, int program) {
    NvnPath *const nv = Nv(path);
    if (program < 0 || (uint32_t)program >= nv->program_count) {
        DrawPathFail(path, "program %d, where the scene made %u", program,
                     (unsigned)nv->program_count);
        return;
    }
    nv->nvn.nvnCommandBufferBindProgram(&nv->command, &nv->programs[program],
                                       NVN_SHADER_STAGE_VERTEX_BIT | NVN_SHADER_STAGE_FRAGMENT_BIT);
}

/// The attribute and stream states: GpuVertex's three attributes from stream 0, and with
/// @p instanced DrawInstance's two from stream 1, which steps once an instance.
static void NvLayout(NvnPath *nv, bool instanced) {
    const NvnProcs *const n = &nv->nvn;
    // Field for field, the offsets from the structures so they cannot drift.
    _Alignas(8) NVNvertexAttribState attributes[5];
    static const struct {
        NVNformat format;
        size_t offset;
        int stream;
    } FIELDS[5] = {
        { NVN_FORMAT_RGB32F, offsetof(GpuVertex, x), 0 },
        { NVN_FORMAT_RG32F, offsetof(GpuVertex, u), 0 },
        { NVN_FORMAT_RGBA32F, offsetof(GpuVertex, r), 0 },
        { NVN_FORMAT_RGBA32F, offsetof(DrawInstance, place), 1 },
        { NVN_FORMAT_RGBA32F, offsetof(DrawInstance, tint), 1 },
    };
    const int count = instanced ? 5 : 3;
    for (int i = 0; i < count; ++i) {
        n->nvnVertexAttribStateSetDefaults(&attributes[i]);
        n->nvnVertexAttribStateSetFormat(&attributes[i], FIELDS[i].format,
                                         (ptrdiff_t)FIELDS[i].offset);
        n->nvnVertexAttribStateSetStreamIndex(&attributes[i], FIELDS[i].stream);
    }
    _Alignas(8) NVNvertexStreamState streams[2];
    n->nvnVertexStreamStateSetDefaults(&streams[0]);
    n->nvnVertexStreamStateSetStride(&streams[0], (ptrdiff_t)sizeof(GpuVertex));
    n->nvnVertexStreamStateSetDefaults(&streams[1]);
    n->nvnVertexStreamStateSetStride(&streams[1], (ptrdiff_t)sizeof(DrawInstance));
    n->nvnVertexStreamStateSetDivisor(&streams[1], 1);
    n->nvnCommandBufferBindVertexAttribState(&nv->command, count, attributes);
    n->nvnCommandBufferBindVertexStreamState(&nv->command, instanced ? 2 : 1, streams);
}

static void NvInstances(DrawPath *path, const DrawBuffer *instances, uint32_t bytes) {
    NvnPath *const nv = Nv(path);
    NvLayout(nv, true);
    nv->nvn.nvnCommandBufferBindVertexBuffer(&nv->command, 1, instances->address, bytes);
}

static const NVNindexType NV_INDEX_TYPES[] = { NVN_INDEX_TYPE_UNSIGNED_BYTE,
                                               NVN_INDEX_TYPE_UNSIGNED_SHORT,
                                               NVN_INDEX_TYPE_UNSIGNED_INT };

static void NvDrawInstanced(DrawPath *path, const DrawElements *elements, uint32_t first,
                            uint32_t count, uint32_t base_instance, uint32_t instances) {
    NvnPath *const nv = Nv(path);
    if (elements != NULL && elements->indexed) {
        // NVN's indexed draws start at the first index of the buffer they are given.
        nv->nvn.nvnCommandBufferDrawElementsInstanced(
            &nv->command, NVN_DRAW_PRIMITIVE_TRIANGLES, NV_INDEX_TYPES[elements->type], (int)count,
            elements->indices->address + ((uint64_t)first << elements->type), 0,
            (int)base_instance, (int)instances);
    } else {
        nv->nvn.nvnCommandBufferDrawArraysInstanced(&nv->command, NVN_DRAW_PRIMITIVE_TRIANGLES,
                                                   (int)first, (int)count, (int)base_instance,
                                                   (int)instances);
    }
}

static void NvDrawIndirect(DrawPath *path, const DrawElements *elements,
                           const DrawBuffer *numbers) {
    NvnPath *const nv = Nv(path);
    if (elements != NULL && elements->indexed) {
        nv->nvn.nvnCommandBufferDrawElementsIndirect(&nv->command, NVN_DRAW_PRIMITIVE_TRIANGLES,
                                                    NV_INDEX_TYPES[elements->type],
                                                    elements->indices->address,
                                                    numbers->address);
    } else {
        nv->nvn.nvnCommandBufferDrawArraysIndirect(&nv->command, NVN_DRAW_PRIMITIVE_TRIANGLES,
                                                  numbers->address);
    }
}

static void NvVertices(DrawPath *path, const DrawBuffer *vertices, uint32_t bytes) {
    NvnPath *const nv = Nv(path);
    const NvnProcs *const n = &nv->nvn;
    NvLayout(nv, false);
    if (path->spec.binds == DRAW_BINDS_SINGLE) {
        n->nvnCommandBufferBindVertexBuffer(&nv->command, 0, vertices->address, bytes);
    } else {
        const NVNbufferRange range = { vertices->address, bytes };
        n->nvnCommandBufferBindVertexBuffers(&nv->command, 0, 1, &range);
    }
}

static NVNshaderStage NvStageOf(DrawStage stage) {
    return stage == DRAW_STAGE_VERTEX ? NVN_SHADER_STAGE_VERTEX : NVN_SHADER_STAGE_FRAGMENT;
}

static void NvUniform(DrawPath *path, DrawStage stage, uint32_t slot, const DrawBuffer *buffer,
                      uint32_t bytes) {
    NvnPath *const nv = Nv(path);
    const NVNbufferRange range = { buffer->address, bytes };
    switch (path->spec.binds) {
    case DRAW_BINDS_PLURAL:
        nv->nvn.nvnCommandBufferBindUniformBuffers(&nv->command, NvStageOf(stage), (int)slot, 1,
                                                  &range);
        break;
    case DRAW_BINDS_DEFERRED:
        if (nv->range_count >= DEFERRED_RANGES) {
            DrawPathFail(path, "deferred bind %u of a list, past the %u ranges the way keeps",
                         (unsigned)nv->range_count, (unsigned)DEFERRED_RANGES);
            return;
        }
        nv->ranges[nv->range_count] = range;
        nv->nvn.nvnCommandBufferBindUniformBuffersDeferred(&nv->command, NvStageOf(stage),
                                                          (int)slot, 1,
                                                          &nv->ranges[nv->range_count]);
        ++nv->range_count;
        break;
    default:
        nv->nvn.nvnCommandBufferBindUniformBuffer(&nv->command, NvStageOf(stage), (int)slot,
                                                 buffer->address, bytes);
        break;
    }
}

static void NvBindTexture(DrawPath *path, DrawStage stage, uint32_t slot, int texture) {
    NvnPath *const nv = Nv(path);
    if (slot >= DRAW_PATH_SLOTS || texture < 0 || (uint32_t)texture >= nv->texture_count) {
        DrawPathFail(path, "texture %d in slot %u, where the scene made %u textures and a stage "
                           "has %u slots",
                     texture, (unsigned)slot, (unsigned)nv->texture_count,
                     (unsigned)DRAW_PATH_SLOTS);
        return;
    }
    switch (path->spec.binds) {
    case DRAW_BINDS_PLURAL:
        nv->nvn.nvnCommandBufferBindTextures(&nv->command, NvStageOf(stage), (int)slot, 1,
                                            &nv->handles[texture]);
        break;
    case DRAW_BINDS_DEFERRED:
        // The handles stay where they are for as long as the way is up.
        nv->nvn.nvnCommandBufferBindTexturesDeferred(&nv->command, NvStageOf(stage), (int)slot,
                                                    1, &nv->handles[texture]);
        break;
    default:
        nv->nvn.nvnCommandBufferBindTexture(&nv->command, NvStageOf(stage), (int)slot,
                                           nv->handles[texture]);
        break;
    }
}

static void NvUpdate(DrawPath *path, const DrawBuffer *buffer, uint32_t buffer_bytes,
                     uint32_t offset, const void *data, uint32_t bytes) {
    NvnPath *const nv = Nv(path);
    nv->nvn.nvnCommandBufferUpdateUniformBuffer(&nv->command, buffer->address, buffer_bytes,
                                               (ptrdiff_t)offset, bytes, data);
}

static void NvDraw(DrawPath *path, uint32_t first, uint32_t count) {
    NvnPath *const nv = Nv(path);
    nv->nvn.nvnCommandBufferDrawArrays(&nv->command, NVN_DRAW_PRIMITIVE_TRIANGLES, (int)first,
                                      (int)count);
}

static void NvDrawIndexed(DrawPath *path, DrawIndex type, const DrawBuffer *indices,
                          uint32_t count, int32_t base_vertex) {
    static const NVNindexType TYPES[] = { NVN_INDEX_TYPE_UNSIGNED_BYTE,
                                          NVN_INDEX_TYPE_UNSIGNED_SHORT,
                                          NVN_INDEX_TYPE_UNSIGNED_INT };
    NvnPath *const nv = Nv(path);
    // A game draws with no base through the call that has none.
    if (base_vertex == 0) {
        nv->nvn.nvnCommandBufferDrawElements(&nv->command, NVN_DRAW_PRIMITIVE_TRIANGLES,
                                            TYPES[type], (int)count, indices->address);
    } else {
        nv->nvn.nvnCommandBufferDrawElementsBaseVertex(&nv->command, NVN_DRAW_PRIMITIVE_TRIANGLES,
                                                      TYPES[type], (int)count, indices->address,
                                                      base_vertex);
    }
}

static bool NvEnd(DrawPath *path) {
    NvnPath *const nv = Nv(path);
    if (!nv->recording) {
        DrawPathFail(path, "a list ended that was not begun");
        return false;
    }
    Finish(nv, "the list");
    return path->failure[0] == '\0';
}

static const uint8_t *NvRead(DrawPath *path) {
    NvnPath *const nv = Nv(path);
    uint8_t *const pixels = nv->nvn.nvnMemoryPoolMap(&nv->read_pool.pool);
    if (pixels == NULL) {
        DrawPathFail(path, "nvnMemoryPoolMap gave no address for the readback pool: %s",
                     Why("NULL"));
        return NULL;
    }
    const size_t bytes = (size_t)path->spec.width * path->spec.height * 4u;
    memset(pixels, READ_PRESET, bytes);
    nv->nvn.nvnCommandBufferBeginRecording(&nv->command);
    // The draws ran on the graphics engine and the copy reads their result: an NVN program
    // orders the two, and asks for the texture cache to be dropped too.
    nv->nvn.nvnCommandBufferBarrier(&nv->command, NVN_BARRIER_ORDER_FRAGMENTS_BIT |
                                                     NVN_BARRIER_INVALIDATE_TEXTURE_BIT);
    const NVNcopyRegion region = { 0, 0, 0, (int)path->spec.width, (int)path->spec.height, 1 };
    nv->nvn.nvnCommandBufferCopyTextureToBuffer(
        &nv->command, &nv->target, NULL, &region,
        nv->nvn.nvnMemoryPoolGetBufferAddress(&nv->read_pool.pool), NVN_COPY_FLAGS_NONE);
    return Finish(nv, "the readback") ? pixels : NULL;
}

static const DrawPathWay NVN_WAY = {
    NvClose, NvProgram, NvTexture, NvAlloc,       NvBegin, NvClear, NvClearDepth, NvDepth,
    NvUseProgram, NvVertices, NvUniform, NvBindTexture, NvDraw,  NvEnd,   NvRead,
    NvDrawIndexed, NvUpdate, NvInstances, NvDrawInstanced, NvDrawIndirect,
};

/// The steps of the bring-up, in order; false with the step in @p why.
static bool BringUp(NvnPath *nv, const DrawPathSpec *spec, bool own, char *why,
                    size_t why_size) {
    const NvnProcs *const n = &nv->nvn;
    char missing[64] = "";
    if (own) {
        if (!NvnLayerOwnProcs(&nv->nvn)) {
            snprintf(why, why_size, "%s", NvnLayerError());
            return false;
        }
    } else if (!NvnLoadProcs(&nv->nvn, missing, sizeof(missing))) {
        snprintf(why, why_size, "the program's NVN gave no %s", missing);
        return false;
    }
    _Alignas(8) NVNdeviceBuilder device_builder;
    n->nvnDeviceBuilderSetDefaults(&device_builder);
    n->nvnDeviceBuilderSetFlags(&device_builder, 0);
    if (!(nv->device_up = n->nvnDeviceInitialize(&nv->device, &device_builder))) {
        snprintf(why, why_size, "nvnDeviceInitialize: %s", Why("false"));
        return false;
    }
    // The deko3d way's conventions, before anything is made from the device.
    n->nvnDeviceSetWindowOriginMode(&nv->device, NVN_WINDOW_ORIGIN_MODE_UPPER_LEFT);
    n->nvnDeviceSetDepthMode(&nv->device, NVN_DEPTH_MODE_NEAR_IS_ZERO);
    if (NvnLayerError()[0] != '\0') {
        snprintf(why, why_size, "the device's origin and depth mode: %s", NvnLayerError());
        return false;
    }
    int padding = 0;
    n->nvnDeviceGetInteger(&nv->device, NVN_DEVICE_INFO_RESERVED_TEXTURE_DESCRIPTORS,
                           &nv->texture_base);
    n->nvnDeviceGetInteger(&nv->device, NVN_DEVICE_INFO_RESERVED_SAMPLER_DESCRIPTORS,
                           &nv->sampler_base);
    n->nvnDeviceGetInteger(&nv->device, NVN_DEVICE_INFO_SHADER_CODE_MEMORY_POOL_PADDING_SIZE,
                           &padding);
    if (nv->texture_base < 0 || nv->texture_base > 4096 || nv->sampler_base < 0 ||
        nv->sampler_base > 4096 || padding < 0 || padding > 0x10000) {
        snprintf(why, why_size, "the device reserves %d texture and %d sampler descriptors and "
                                "pads a code pool by %d bytes: %s",
                 nv->texture_base, nv->sampler_base, padding, Why("answers the way cannot use"));
        return false;
    }

    _Alignas(8) NVNqueueBuilder queue_builder;
    n->nvnQueueBuilderSetDevice(&queue_builder, &nv->device);
    n->nvnQueueBuilderSetDefaults(&queue_builder);
    if (!(nv->queue_up = n->nvnQueueInitialize(&nv->queue, &queue_builder))) {
        snprintf(why, why_size, "nvnQueueInitialize: %s", Why("false"));
        return false;
    }
    const int visible = NVN_MEMORY_POOL_FLAGS_CPU_UNCACHED | NVN_MEMORY_POOL_FLAGS_GPU_CACHED;
    const size_t texture_descriptors = (size_t)(nv->texture_base + (int)DRAW_PATH_TEXTURES) * 32u;
    const size_t sampler_descriptors = (size_t)(nv->sampler_base + (int)DRAW_PATH_TEXTURES) * 32u;
    // The layer keeps a quarter of the command memory for the texture handles a draw reads, so
    // the scene's own count is a part of what the list gets.
    const size_t command_bytes = 2u * (spec->command_bytes != 0u ? spec->command_bytes : 0x10000u);
    if (!MakePool(nv, &nv->command_pool, command_bytes, visible) ||
        !MakePool(nv, &nv->data_pool, (size_t)spec->data_bytes + spec->texture_bytes + 0x1000u,
                  visible) ||
        !MakePool(nv, &nv->read_pool, (size_t)spec->width * spec->height * 4u, visible) ||
        !MakePool(nv, &nv->image_pool,
                  2u * ((size_t)spec->width * spec->height * 8u + spec->texture_bytes) +
                      (DRAW_PATH_TEXTURES + 2u) * (size_t)POOL_ALIGNMENT,
                  NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS | NVN_MEMORY_POOL_FLAGS_GPU_CACHED) ||
        !MakePool(nv, &nv->code_pool, CODE_BYTES + (size_t)padding,
                  visible | NVN_MEMORY_POOL_FLAGS_SHADER_CODE) ||
        !MakePool(nv, &nv->descriptor_pool,
                  RoundUp(texture_descriptors, 0x100u) + sampler_descriptors, visible)) {
        snprintf(why, why_size, "a memory pool: %s", Why("no memory, or false"));
        return false;
    }
    // The code pool's last bytes are not for programs, as the device said.
    nv->code_pool.size -= (size_t)padding;

    if (!(nv->target_up = MakeTexture(nv, &nv->target, NVN_FORMAT_RGBA8, (int)spec->width,
                                      (int)spec->height, why, why_size))) {
        return false;
    }
    if (spec->depth && !(nv->depth_up = MakeTexture(nv, &nv->depth, NVN_FORMAT_DEPTH32F,
                                                    (int)spec->width, (int)spec->height, why,
                                                    why_size))) {
        return false;
    }
    nv->texture_pool_up =
        n->nvnTexturePoolInitialize(&nv->texture_pool, &nv->descriptor_pool.pool, 0,
                                    nv->texture_base + (int)DRAW_PATH_TEXTURES);
    nv->sampler_pool_up = n->nvnSamplerPoolInitialize(
        &nv->sampler_pool, &nv->descriptor_pool.pool,
        (ptrdiff_t)RoundUp(texture_descriptors, 0x100u),
        nv->sampler_base + (int)DRAW_PATH_TEXTURES);
    if (!nv->texture_pool_up || !nv->sampler_pool_up) {
        snprintf(why, why_size, "the texture and sampler pools: %s", Why("false"));
        return false;
    }

    if (!(nv->command_up = n->nvnCommandBufferInitialize(&nv->command, &nv->device))) {
        snprintf(why, why_size, "nvnCommandBufferInitialize: %s", Why("false"));
        return false;
    }
    nv->control = aligned_alloc(8u, CONTROL_BYTES);
    if (nv->control == NULL) {
        snprintf(why, why_size, "no memory for the command buffer's control memory");
        return false;
    }
    n->nvnCommandBufferAddCommandMemory(&nv->command, &nv->command_pool.pool, 0,
                                        nv->command_pool.size);
    n->nvnCommandBufferAddControlMemory(&nv->command, nv->control, CONTROL_BYTES);
    if (NvnLayerError()[0] != '\0') {
        snprintf(why, why_size, "the command buffer's memory: %s", NvnLayerError());
        return false;
    }
    return true;
}

bool DrawPathNvnStage(DrawPath *path, const void *dksh, uint32_t dksh_bytes,
                      NVNshaderData *stage) {
    if (path == NULL || path->way != &NVN_WAY) {
        return false;
    }
    NvnPath *const nv = Nv(path);
    if (nv->more_count >= DRAW_PATH_MORE_STAGES) {
        DrawPathFail(path, "a stage past the %u a scene may ask for by themselves",
                     (unsigned)DRAW_PATH_MORE_STAGES);
        return false;
    }
    return Stage(nv, dksh, dksh_bytes, "own", &nv->more_sections[nv->more_count++], stage);
}

bool DrawPathNvn(DrawPath *path, DrawPathNvnParts *parts) {
    if (path == NULL || path->way != &NVN_WAY) {
        return false;
    }
    NvnPath *const nv = Nv(path);
    parts->nvn = &nv->nvn;
    parts->device = &nv->device;
    parts->queue = &nv->queue;
    parts->command = &nv->command;
    parts->target = &nv->target;
    parts->depth = nv->depth_up ? &nv->depth : NULL;
    parts->command_pool = &nv->command_pool.pool;
    parts->command_bytes = nv->command_pool.size;
    parts->control = nv->control;
    parts->control_bytes = CONTROL_BYTES;
    parts->stages = nv->stages;
    parts->texture_base = nv->texture_base;
    parts->sampler_base = nv->sampler_base;
    return true;
}

DrawPath *DrawPathOpenNvn(const DrawPathSpec *spec, bool own, char *why, size_t why_size) {
    NvnPath *const nv = &nvn_path;
    if (nv->device_up) {
        snprintf(why, why_size, "the NVN way is already up");
        return NULL;
    }
    memset(nv, 0, sizeof(*nv));
    nv->base.way = &NVN_WAY;
    nv->base.spec = *spec;
    nv->scratch_at = SIZE_MAX;
    // deko3d ends the program when it cannot make an object; with gpu.c's guard a bring-up that
    // fails under the layer is a sentence in the report instead.
    NvnLayerSetGuard(AbortGuarded);
    NvnLayerClearError();
    if (!BringUp(nv, spec, own, why, why_size)) {
        NvClose(&nv->base);
        return NULL;
    }
    return &nv->base;
}
