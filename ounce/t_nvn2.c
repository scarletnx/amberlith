// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The nvn2 suite (docs/families/Nvn2.md): what a Switch 2 program asks of NVN that a Switch's
// does not, with shader programs of the Switch 2's own code, assembled by hand
// (docs/Sm86.md). Only the Switch 2's frontend runs it, and only ScarletNX runs that.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nvnlayer.h>

#include "check.h"
#include "mandelref.h"
#include "picture.h"
#include "report.h"
#include "tests/tests.h"

// Written by bin2s from what tools/sm86asm.py assembled (ounce.mk).
#include "ounce_shaders.h"

static const char *const SUITE = "nvn2";

void RunNvn2(void);

#define POOL_PAGE 0x1000u
#define POOL_ALIGNMENT 0x10000u
#define COMMAND_BYTES 0x10000u
#define CONTROL_BYTES 0x4000u
#define CODE_BYTES 0x10000u
#define DATA_BYTES 0x10000u
// The Mandelbrot's target and its depth, with room for each one's alignment.
#define IMAGE_BYTES (2u * MANDEL_SIZE * MANDEL_SIZE * 8u + 4u * POOL_ALIGNMENT)
#define READ_BYTES (MANDEL_SIZE * MANDEL_SIZE * 4u)
#define READ_PRESET 0xa5u

// The compute case's dispatch: blocks of 64 invocations, as scale.comp.sm86 says, a word each.
#define SCALE_BLOCKS 2u
#define SCALE_WORDS (SCALE_BLOCKS * 64u)
#define SCALE_OUT_BYTE 0x200u

// The Mandelbrot's counts, a word a cell, in the data pool past the scale case's buffer.
#define MANDEL_BYTE 0x1000u
#define MANDEL_WORDS (MANDEL_COLUMNS * MANDEL_ROWS)
// What an Ounce game sets before its mesh draws (ScarletNX's docs/platforms/nx/ounce.md, "Draws").
#define GEOMETRY_PROCESSING_MESH 2

// The matrix case's buffer, in the data pool past the Mandelbrot's: 32 bytes a lane in, and each
// lane's four floats 0x400 bytes on, as hmma.comp.sm86 says.
#define HMMA_BYTE 0x2000u
#define HMMA_LANES 32u
#define HMMA_OUT_BYTE 0x400u
#define HMMA_BYTES (HMMA_OUT_BYTE + HMMA_LANES * 32u)

typedef enum {
    PROGRAM_SCALE,
    PROGRAM_MANDEL_COUNT,
    PROGRAM_MANDEL_DRAW,
    PROGRAM_HMMA,
    PROGRAM_COUNT,
} ProgramId;

typedef struct {
    NVNmemoryPool pool;
    void *storage;
    size_t size;
    bool up;
} Pool;

static struct {
    NvnProcs nvn;
    NVNdevice device;
    NVNqueue queue;
    NVNcommandBuffer command;
    NVNprogram programs[PROGRAM_COUNT];
    NVNtexture target;
    NVNtexture depth;
    Pool command_pool;
    Pool code_pool;
    Pool data_pool;
    Pool image_pool;
    Pool read_pool;
    void *control;
    size_t code_used;
    size_t image_used;
    bool device_up;
    bool queue_up;
    bool command_up;
    bool program_up[PROGRAM_COUNT];
    bool target_up;
    bool depth_up;
    bool counted;   ///< the Mandelbrot's dispatch ran, and its buffer holds the counts
} n2;

static const char *Why(const char *fallback) {
    return NvnLayerError()[0] != '\0' ? NvnLayerError() : fallback;
}

static size_t RoundUp(size_t value, size_t to) {
    return (value + to - 1u) / to * to;
}

static bool MakePool(Pool *pool, size_t size, int flags) {
    pool->size = RoundUp(size, POOL_PAGE);
    pool->storage = aligned_alloc(POOL_ALIGNMENT, RoundUp(pool->size, POOL_ALIGNMENT));
    if (pool->storage == NULL) {
        return false;
    }
    memset(pool->storage, 0, pool->size);
    _Alignas(8) NVNmemoryPoolBuilder builder;
    n2.nvn.nvnMemoryPoolBuilderSetDefaults(&builder);
    n2.nvn.nvnMemoryPoolBuilderSetDevice(&builder, &n2.device);
    n2.nvn.nvnMemoryPoolBuilderSetStorage(&builder, pool->storage, pool->size);
    n2.nvn.nvnMemoryPoolBuilderSetFlags(&builder, flags);
    pool->up = n2.nvn.nvnMemoryPoolInitialize(&pool->pool, &builder);
    return pool->up;
}

static void DropPool(Pool *pool) {
    if (pool->up) {
        n2.nvn.nvnMemoryPoolFinalize(&pool->pool);
        pool->up = false;
    }
    free(pool->storage);
    pool->storage = NULL;
}

static void TearDown(void) {
    for (int i = 0; i < PROGRAM_COUNT; ++i) {
        if (n2.program_up[i]) {
            n2.nvn.nvnProgramFinalize(&n2.programs[i]);
        }
    }
    if (n2.depth_up) {
        n2.nvn.nvnTextureFinalize(&n2.depth);
    }
    if (n2.target_up) {
        n2.nvn.nvnTextureFinalize(&n2.target);
    }
    if (n2.command_up) {
        n2.nvn.nvnCommandBufferFinalize(&n2.command);
    }
    free(n2.control);
    DropPool(&n2.read_pool);
    DropPool(&n2.image_pool);
    DropPool(&n2.data_pool);
    DropPool(&n2.code_pool);
    DropPool(&n2.command_pool);
    if (n2.queue_up) {
        n2.nvn.nvnQueueFinalize(&n2.queue);
    }
    if (n2.device_up) {
        n2.nvn.nvnDeviceFinalize(&n2.device);
    }
    memset(&n2, 0, sizeof(n2));
}

/// A texture of the Mandelbrot's size, its storage carved from the image pool.
static bool MakeTexture(NVNtexture *texture, NVNformat format, char *why, size_t why_size) {
    _Alignas(8) NVNtextureBuilder builder;
    memset(&builder, 0, sizeof(builder));
    n2.nvn.nvnTextureBuilderSetDevice(&builder, &n2.device);
    n2.nvn.nvnTextureBuilderSetDefaults(&builder);
    n2.nvn.nvnTextureBuilderSetFlags(&builder, 0);
    n2.nvn.nvnTextureBuilderSetTarget(&builder, NVN_TEXTURE_TARGET_2D);
    n2.nvn.nvnTextureBuilderSetFormat(&builder, format);
    n2.nvn.nvnTextureBuilderSetSize2D(&builder, (int)MANDEL_SIZE, (int)MANDEL_SIZE);
    const size_t size = n2.nvn.nvnTextureBuilderGetStorageSize(&builder);
    const size_t alignment = n2.nvn.nvnTextureBuilderGetStorageAlignment(&builder);
    if (size == 0u || alignment == 0u || alignment > POOL_ALIGNMENT) {
        snprintf(why, why_size, "a texture's storage is %zu bytes at an alignment of %zu: %s",
                 size, alignment, Why("an answer the image pool cannot hold"));
        return false;
    }
    const size_t at = RoundUp(n2.image_used, alignment);
    if (at > n2.image_pool.size || size > n2.image_pool.size - at) {
        snprintf(why, why_size, "no %zu bytes left in the image pool's %zu for a texture", size,
                 n2.image_pool.size);
        return false;
    }
    n2.image_used = at + size;
    n2.nvn.nvnTextureBuilderSetStorage(&builder, &n2.image_pool.pool, (ptrdiff_t)at);
    if (!n2.nvn.nvnTextureInitialize(texture, &builder)) {
        snprintf(why, why_size, "nvnTextureInitialize: %s", Why("false"));
        return false;
    }
    return true;
}

/// The device, a queue, a command buffer with its memory, the pools a program's code and a
/// dispatch's buffer live in, and the Mandelbrot's target and depth.
static bool BringUp(char *why, size_t why_size) {
    char missing[64] = "";
    if (!NvnLoadProcs(&n2.nvn, missing, sizeof(missing))) {
        snprintf(why, why_size, "the program's NVN gave no %s", missing);
        return false;
    }
    _Alignas(8) NVNdeviceBuilder device_builder;
    n2.nvn.nvnDeviceBuilderSetDefaults(&device_builder);
    n2.nvn.nvnDeviceBuilderSetFlags(&device_builder, 0);
    if (!(n2.device_up = n2.nvn.nvnDeviceInitialize(&n2.device, &device_builder))) {
        snprintf(why, why_size, "nvnDeviceInitialize: %s", Why("false"));
        return false;
    }
    // The device keeps the modes it starts with: clip space's y of -1 is the first row of a
    // target's memory, which is the picture's top as the reference counts rows, and whichever
    // way depth is mapped a lesser z stays the nearer.
    int padding = 0;
    n2.nvn.nvnDeviceGetInteger(&n2.device, NVN_DEVICE_INFO_SHADER_CODE_MEMORY_POOL_PADDING_SIZE,
                               &padding);
    if (padding < 0 || padding > 0x10000) {
        snprintf(why, why_size, "the device pads a code pool by %d bytes, which the suite has "
                                "no pool for (0 to 65536)",
                 padding);
        return false;
    }
    _Alignas(8) NVNqueueBuilder queue_builder;
    n2.nvn.nvnQueueBuilderSetDevice(&queue_builder, &n2.device);
    n2.nvn.nvnQueueBuilderSetDefaults(&queue_builder);
    if (!(n2.queue_up = n2.nvn.nvnQueueInitialize(&n2.queue, &queue_builder))) {
        snprintf(why, why_size, "nvnQueueInitialize: %s", Why("false"));
        return false;
    }
    const int visible = NVN_MEMORY_POOL_FLAGS_CPU_UNCACHED | NVN_MEMORY_POOL_FLAGS_GPU_CACHED;
    if (!MakePool(&n2.command_pool, COMMAND_BYTES, visible) ||
        !MakePool(&n2.data_pool, DATA_BYTES, visible) ||
        !MakePool(&n2.read_pool, READ_BYTES, visible) ||
        !MakePool(&n2.image_pool, IMAGE_BYTES,
                  NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS | NVN_MEMORY_POOL_FLAGS_GPU_CACHED) ||
        !MakePool(&n2.code_pool, CODE_BYTES + (size_t)padding,
                  visible | NVN_MEMORY_POOL_FLAGS_SHADER_CODE)) {
        snprintf(why, why_size, "a memory pool: %s", Why("no memory, or false"));
        return false;
    }
    if (!(n2.command_up = n2.nvn.nvnCommandBufferInitialize(&n2.command, &n2.device))) {
        snprintf(why, why_size, "nvnCommandBufferInitialize: %s", Why("false"));
        return false;
    }
    n2.control = aligned_alloc(8u, CONTROL_BYTES);
    if (n2.control == NULL) {
        snprintf(why, why_size, "no memory for the command buffer's control memory");
        return false;
    }
    n2.nvn.nvnCommandBufferAddCommandMemory(&n2.command, &n2.command_pool.pool, 0,
                                            n2.command_pool.size);
    n2.nvn.nvnCommandBufferAddControlMemory(&n2.command, n2.control, CONTROL_BYTES);
    if (!(n2.target_up = MakeTexture(&n2.target, NVN_FORMAT_RGBA8, why, why_size)) ||
        !(n2.depth_up = MakeTexture(&n2.depth, NVN_FORMAT_DEPTH32F, why, why_size))) {
        return false;
    }
    return true;
}

typedef struct {
    const uint8_t *code;
    size_t code_bytes;
    const uint8_t *control;
} Stage;

/// A program of @p count stages, through the builder a Switch 2 program makes its programs
/// with: each stage's code copied into the code pool on a 0x100 boundary, its control section
/// as the assembler wrote it.
static bool BuildProgram(ProgramId id, const Stage *stages, int count, char *why,
                         size_t why_size) {
    uint8_t *const memory = n2.nvn.nvnMemoryPoolMap(&n2.code_pool.pool);
    if (memory == NULL) {
        snprintf(why, why_size, "nvnMemoryPoolMap gave no address for the code pool: %s",
                 Why("NULL"));
        return false;
    }
    NVNshaderData data[2];
    for (int i = 0; i < count; ++i) {
        const size_t at = RoundUp(n2.code_used, 0x100u);
        if (at > CODE_BYTES || stages[i].code_bytes > CODE_BYTES - at) {
            snprintf(why, why_size, "no 0x%zx bytes left in the code pool's 0x%x for a stage",
                     stages[i].code_bytes, (unsigned)CODE_BYTES);
            return false;
        }
        memcpy(memory + at, stages[i].code, stages[i].code_bytes);
        n2.code_used = at + stages[i].code_bytes;
        data[i].data = n2.nvn.nvnMemoryPoolGetBufferAddress(&n2.code_pool.pool) + at;
        data[i].control = stages[i].control;
    }
    _Alignas(8) NVNprogramBuilder builder;
    n2.nvn.nvnProgramBuilderSetDefaults(&builder);
    n2.nvn.nvnProgramBuilderSetDevice(&builder, &n2.device);
    n2.nvn.nvnProgramBuilderSetShaders(&builder, count, data);
    n2.program_up[id] = n2.nvn.nvnProgramInitializeFromBuilder(&n2.programs[id], &builder);
    if (!n2.program_up[id]) {
        snprintf(why, why_size, "nvnProgramInitializeFromBuilder: %s", Why("false"));
    }
    return n2.program_up[id];
}

/// The open recording submitted, and the graphics unit waited for.
static bool Finish(const char *what, char *why, size_t why_size) {
    const NVNcommandHandle list = n2.nvn.nvnCommandBufferEndRecording(&n2.command);
    if (list == 0u) {
        snprintf(why, why_size, "%s: nvnCommandBufferEndRecording gave no list: %s", what,
                 Why("0"));
        return false;
    }
    n2.nvn.nvnQueueSubmitCommands(&n2.queue, 1, &list);
    n2.nvn.nvnQueueFinish(&n2.queue);
    if (NvnLayerError()[0] != '\0') {
        snprintf(why, why_size, "%s: %s", what, NvnLayerError());
        return false;
    }
    return true;
}

/// What the scale program reads at word @p index: every byte of the word different, and no two
/// words alike.
static uint32_t ScaleInput(uint32_t index) {
    return 0x01020304u + index * 0x00010203u;
}

/// The dispatch: the buffer filled, the program bound with the buffer as its storage buffer 0,
/// two blocks dispatched, the queue finished. The buffer's memory, for the case to read.
static const uint8_t *Dispatch(char *why, size_t why_size) {
    uint8_t *const memory = n2.nvn.nvnMemoryPoolMap(&n2.data_pool.pool);
    if (memory == NULL) {
        snprintf(why, why_size, "nvnMemoryPoolMap gave no address for the buffer's pool: %s",
                 Why("NULL"));
        return NULL;
    }
    // The output's words start as a value no word comes out as, so a dispatch that wrote
    // nothing is told from one that wrote wrong.
    for (uint32_t i = 0; i < SCALE_WORDS; ++i) {
        const uint32_t in = ScaleInput(i);
        const uint32_t preset = 0xdeadbeefu;
        memcpy(memory + 4u * i, &in, sizeof(in));
        memcpy(memory + SCALE_OUT_BYTE + 4u * i, &preset, sizeof(preset));
    }
    n2.nvn.nvnCommandBufferBeginRecording(&n2.command);
    n2.nvn.nvnCommandBufferBarrier(&n2.command, NVN_BARRIER_INVALIDATE_SHADER_BIT);
    n2.nvn.nvnCommandBufferBindProgram(&n2.command, &n2.programs[PROGRAM_SCALE],
                                       NVN_SHADER_STAGE_COMPUTE_BIT);
    const NVNbufferAddress buffer = n2.nvn.nvnMemoryPoolGetBufferAddress(&n2.data_pool.pool);
    n2.nvn.nvnCommandBufferBindStorageBuffer(&n2.command, NVN_SHADER_STAGE_COMPUTE, 0, buffer,
                                             SCALE_OUT_BYTE + 4u * SCALE_WORDS);
    n2.nvn.nvnCommandBufferDispatchCompute(&n2.command, (int)SCALE_BLOCKS, 1, 1);
    return Finish("the dispatch", why, why_size) ? memory : NULL;
}

/// @p bytes at @p source copied by the graphics unit into the readback pool, whose memory this
/// answers: what a buffer holds as the graphics unit sees it.
static const uint8_t *CopyOut(NVNbufferAddress source, size_t bytes, char *why, size_t why_size) {
    uint8_t *const copy = n2.nvn.nvnMemoryPoolMap(&n2.read_pool.pool);
    if (copy == NULL) {
        snprintf(why, why_size, "nvnMemoryPoolMap gave no address for the readback pool: %s",
                 Why("NULL"));
        return NULL;
    }
    memset(copy, READ_PRESET, bytes);
    n2.nvn.nvnCommandBufferBeginRecording(&n2.command);
    n2.nvn.nvnCommandBufferCopyBufferToBuffer(
        &n2.command, source, n2.nvn.nvnMemoryPoolGetBufferAddress(&n2.read_pool.pool), bytes,
        NVN_COPY_FLAGS_NONE);
    return Finish("the copy", why, why_size) ? copy : NULL;
}

/// The scale program's words at @p out against the processor's arithmetic, three times the word
/// plus its index; @p why is what kept them from being read, when @p out is NULL.
static void ScaleJudge(const char *name, const uint8_t *out, const char *why) {
    if (out == NULL) {
        ReportCase(SUITE, name, CHECK_NONE, false, why);
        return;
    }
    uint32_t wrong = 0;
    uint32_t first = 0;
    uint32_t got_first = 0;
    for (uint32_t i = 0; i < SCALE_WORDS; ++i) {
        uint32_t got;
        memcpy(&got, out + 4u * i, sizeof(got));
        if (got != ScaleInput(i) * 3u + i && wrong++ == 0u) {
            first = i;
            got_first = got;
        }
    }
    CHECK_TRUE(SUITE, name, CHECK_EXACT, wrong == 0u,
               "%u of %u words differ; the first, word %u, is 0x%08x where three times 0x%08x "
               "plus %u is 0x%08x (0xdeadbeef is a word nothing wrote, 0xa5a5a5a5 one nothing "
               "copied)",
               (unsigned)wrong, (unsigned)SCALE_WORDS, (unsigned)first, (unsigned)got_first,
               (unsigned)ScaleInput(first), (unsigned)first,
               (unsigned)(ScaleInput(first) * 3u + first));
}

/// The Mandelbrot's counts: the buffer preset, the program bound with it as its storage buffer
/// 0, a block a row dispatched. The counts as the graphics unit copies them out: what the
/// processor reads of a dispatch's buffer is the scale case's question, asked once.
static const uint8_t *MandelDispatch(char *why, size_t why_size) {
    uint8_t *const memory = n2.nvn.nvnMemoryPoolMap(&n2.data_pool.pool);
    if (memory == NULL) {
        snprintf(why, why_size, "nvnMemoryPoolMap gave no address for the buffer's pool: %s",
                 Why("NULL"));
        return NULL;
    }
    for (uint32_t i = 0; i < MANDEL_WORDS; ++i) {
        const uint32_t preset = 0xdeadbeefu;
        memcpy(memory + MANDEL_BYTE + 4u * i, &preset, sizeof(preset));
    }
    const NVNbufferAddress buffer =
        n2.nvn.nvnMemoryPoolGetBufferAddress(&n2.data_pool.pool) + MANDEL_BYTE;
    n2.nvn.nvnCommandBufferBeginRecording(&n2.command);
    n2.nvn.nvnCommandBufferBarrier(&n2.command, NVN_BARRIER_INVALIDATE_SHADER_BIT);
    n2.nvn.nvnCommandBufferBindProgram(&n2.command, &n2.programs[PROGRAM_MANDEL_COUNT],
                                       NVN_SHADER_STAGE_COMPUTE_BIT);
    n2.nvn.nvnCommandBufferBindStorageBuffer(&n2.command, NVN_SHADER_STAGE_COMPUTE, 0, buffer,
                                             4u * MANDEL_WORDS);
    n2.nvn.nvnCommandBufferDispatchCompute(&n2.command, 1, (int)MANDEL_ROWS, 1);
    n2.counted = Finish("the dispatch", why, why_size);
    return n2.counted ? CopyOut(buffer, 4u * MANDEL_WORDS, why, why_size) : NULL;
}

/// The Mandelbrot's picture: the target and its depth cleared, the mesh and fragment program
/// bound with the counts as the mesh stage's storage buffer 0, a workgroup a row drawn with
/// the depth test on, and the target copied to memory the processor reads.
static const uint8_t *MandelDraw(char *why, size_t why_size) {
    const NvnProcs *const n = &n2.nvn;
    static const float clear[4] = { 0.0f, 0.0f, 1.0f, 1.0f };   // MANDEL_CLEAR
    n->nvnCommandBufferBeginRecording(&n2.command);
    n->nvnCommandBufferBarrier(&n2.command, NVN_BARRIER_ORDER_FRAGMENTS_BIT |
                                                NVN_BARRIER_INVALIDATE_SHADER_BIT);
    const NVNtexture *const colors[1] = { &n2.target };
    n->nvnCommandBufferSetRenderTargets(&n2.command, 1, colors, NULL, &n2.depth, NULL);
    n->nvnCommandBufferSetViewport(&n2.command, 0, 0, (int)MANDEL_SIZE, (int)MANDEL_SIZE);
    n->nvnCommandBufferSetScissor(&n2.command, 0, 0, (int)MANDEL_SIZE, (int)MANDEL_SIZE);
    // Every state said: an NVN command buffer starts with none.
    _Alignas(8) NVNblendState blend;
    n->nvnBlendStateSetDefaults(&blend);
    n->nvnCommandBufferBindBlendState(&n2.command, &blend);
    _Alignas(8) NVNchannelMaskState mask;
    n->nvnChannelMaskStateSetDefaults(&mask);
    n->nvnCommandBufferBindChannelMaskState(&n2.command, &mask);
    _Alignas(8) NVNcolorState color;
    n->nvnColorStateSetDefaults(&color);
    n->nvnCommandBufferBindColorState(&n2.command, &color);
    _Alignas(8) NVNmultisampleState multisample;
    n->nvnMultisampleStateSetDefaults(&multisample);
    n->nvnCommandBufferBindMultisampleState(&n2.command, &multisample);
    _Alignas(8) NVNpolygonState polygon;
    n->nvnPolygonStateSetDefaults(&polygon);
    n->nvnPolygonStateSetCullFace(&polygon, NVN_FACE_NONE);
    n->nvnCommandBufferBindPolygonState(&n2.command, &polygon);
    _Alignas(8) NVNdepthStencilState depth;
    n->nvnDepthStencilStateSetDefaults(&depth);
    n->nvnDepthStencilStateSetDepthTestEnable(&depth, true);
    n->nvnDepthStencilStateSetDepthWriteEnable(&depth, true);
    n->nvnDepthStencilStateSetDepthFunc(&depth, NVN_DEPTH_FUNC_LESS);
    n->nvnCommandBufferBindDepthStencilState(&n2.command, &depth);
    n->nvnCommandBufferClearColor(&n2.command, 0, clear, NVN_CLEAR_COLOR_MASK_RGBA);
    n->nvnCommandBufferClearDepthStencil(&n2.command, 1.0f, 1, 0, 0);

    n->nvnCommandBufferBindProgram(&n2.command, &n2.programs[PROGRAM_MANDEL_DRAW],
                                   NVN_SHADER_STAGE_ALL_BITS_231);
    n->nvnCommandBufferSetGeometryProcessingMode(&n2.command, GEOMETRY_PROCESSING_MESH);
    n->nvnCommandBufferBindStorageBuffer(
        &n2.command, NVN_SHADER_STAGE_MESH, 0,
        n->nvnMemoryPoolGetBufferAddress(&n2.data_pool.pool) + MANDEL_BYTE, 4u * MANDEL_WORDS);
    n->nvnCommandBufferDrawMeshTasks(&n2.command, 0, (int)MANDEL_ROWS);
    if (!Finish("the draw", why, why_size)) {
        return NULL;
    }

    uint8_t *const pixels = n->nvnMemoryPoolMap(&n2.read_pool.pool);
    if (pixels == NULL) {
        snprintf(why, why_size, "nvnMemoryPoolMap gave no address for the readback pool: %s",
                 Why("NULL"));
        return NULL;
    }
    memset(pixels, READ_PRESET, READ_BYTES);
    n->nvnCommandBufferBeginRecording(&n2.command);
    n->nvnCommandBufferBarrier(&n2.command, NVN_BARRIER_ORDER_FRAGMENTS_BIT |
                                                NVN_BARRIER_INVALIDATE_TEXTURE_BIT);
    const NVNcopyRegion region = { 0, 0, 0, (int)MANDEL_SIZE, (int)MANDEL_SIZE, 1 };
    n->nvnCommandBufferCopyTextureToBuffer(&n2.command, &n2.target, NULL, &region,
                                           n->nvnMemoryPoolGetBufferAddress(&n2.read_pool.pool),
                                           NVN_COPY_FLAGS_NONE);
    return Finish("the readback", why, why_size) ? pixels : NULL;
}

/// The next element of the matrix case's matrices, in quarters: -6 to 6 and never 0, so every
/// product and every sum of sixteen is exact in a float, and a sum of nought is +0 whichever
/// order the products are added in. From a generator's high bits, so no row or column repeats
/// another and a matrix laid out wrong gives another product.
static int32_t HmmaQuarters(uint32_t *seed) {
    *seed = *seed * 1664525u + 1013904223u;
    const int32_t step = (int32_t)((*seed >> 24) % 12u);
    return step < 6 ? step - 6 : step - 5;
}

/// @p quarters / 4 as a half float's sixteen bits.
static uint32_t HmmaHalf(int32_t quarters) {
    const uint32_t magnitude = (uint32_t)(quarters < 0 ? -quarters : quarters);
    const uint32_t exponent = magnitude >= 4u ? 2u : magnitude >= 2u ? 1u : 0u;
    return (quarters < 0 ? 0x8000u : 0u) | (13u + exponent) << 10 |
           ((magnitude << (10u - exponent)) & 0x3ffu);
}

/// The matrix case: A, 16 by 16, times B, 16 by 8, by one warp's HMMA, every float of the
/// product against the integers' sum.
///
/// The lanes hold the matrices as the instruction's public description lays them out (PTX's
/// mma.sync of m16n8k16, which ScarletNX's translator follows). Lane l is thread t = l % 4 of
/// group g = l / 4. Its four words of A are rows g and g + 8 at columns 2t and 2t + 1, then
/// the same rows at columns 2t + 8 and 2t + 9, two halves a word; its two words of B are rows
/// 2t and 2t + 1, then 2t + 8 and 2t + 9, of column g; its four floats of the product are rows
/// g and g + 8 at columns 2t and 2t + 1.
static void HmmaCase(const char *name) {
    char why[256] = "";
    NvnLayerClearError();
    static int32_t a[16][16];
    static int32_t b[16][8];
    uint32_t seed = 0x2545f491u;
    for (uint32_t row = 0; row < 16u; ++row) {
        for (uint32_t column = 0; column < 16u; ++column) {
            a[row][column] = HmmaQuarters(&seed);
        }
    }
    for (uint32_t row = 0; row < 16u; ++row) {
        for (uint32_t column = 0; column < 8u; ++column) {
            b[row][column] = HmmaQuarters(&seed);
        }
    }
    uint8_t *const pool = n2.nvn.nvnMemoryPoolMap(&n2.data_pool.pool);
    if (pool == NULL) {
        ReportCase(SUITE, name, CHECK_NONE, false,
                   "nvnMemoryPoolMap gave no address for the buffer's pool");
        return;
    }
    uint8_t *const memory = pool + HMMA_BYTE;
    memset(memory, 0, HMMA_BYTES);
    for (uint32_t lane = 0; lane < HMMA_LANES; ++lane) {
        const uint32_t g = lane / 4u;
        const uint32_t t = lane % 4u;
        const uint32_t words[6] = {
            HmmaHalf(a[g][2u * t]) | HmmaHalf(a[g][2u * t + 1u]) << 16,
            HmmaHalf(a[g + 8u][2u * t]) | HmmaHalf(a[g + 8u][2u * t + 1u]) << 16,
            HmmaHalf(a[g][2u * t + 8u]) | HmmaHalf(a[g][2u * t + 9u]) << 16,
            HmmaHalf(a[g + 8u][2u * t + 8u]) | HmmaHalf(a[g + 8u][2u * t + 9u]) << 16,
            HmmaHalf(b[2u * t][g]) | HmmaHalf(b[2u * t + 1u][g]) << 16,
            HmmaHalf(b[2u * t + 8u][g]) | HmmaHalf(b[2u * t + 9u][g]) << 16,
        };
        memcpy(memory + 32u * lane, words, sizeof(words));
        for (uint32_t i = 0; i < 4u; ++i) {
            const uint32_t preset = 0xdeadbeefu;
            memcpy(memory + HMMA_OUT_BYTE + 32u * lane + 4u * i, &preset, sizeof(preset));
        }
    }
    const NVNbufferAddress buffer =
        n2.nvn.nvnMemoryPoolGetBufferAddress(&n2.data_pool.pool) + HMMA_BYTE;
    n2.nvn.nvnCommandBufferBeginRecording(&n2.command);
    n2.nvn.nvnCommandBufferBarrier(&n2.command, NVN_BARRIER_INVALIDATE_SHADER_BIT);
    n2.nvn.nvnCommandBufferBindProgram(&n2.command, &n2.programs[PROGRAM_HMMA],
                                       NVN_SHADER_STAGE_COMPUTE_BIT);
    n2.nvn.nvnCommandBufferBindStorageBuffer(&n2.command, NVN_SHADER_STAGE_COMPUTE, 0, buffer,
                                             HMMA_BYTES);
    n2.nvn.nvnCommandBufferDispatchCompute(&n2.command, 1, 1, 1);
    const uint8_t *const out =
        Finish("the dispatch", why, sizeof(why))
            ? CopyOut(buffer + HMMA_OUT_BYTE, HMMA_LANES * 32u, why, sizeof(why))
            : NULL;
    if (out == NULL) {
        ReportCase(SUITE, name, CHECK_NONE, false, why);
        return;
    }
    uint32_t wrong = 0;
    uint32_t first_row = 0;
    uint32_t first_column = 0;
    uint32_t first_got = 0;
    uint32_t first_want = 0;
    int32_t total = 0;
    for (uint32_t lane = 0; lane < HMMA_LANES; ++lane) {
        for (uint32_t i = 0; i < 4u; ++i) {
            const uint32_t row = lane / 4u + (i >= 2u ? 8u : 0u);
            const uint32_t column = 2u * (lane % 4u) + (i & 1u);
            int32_t sixteenths = 0;
            for (uint32_t k = 0; k < 16u; ++k) {
                sixteenths += a[row][k] * b[k][column];
            }
            total += sixteenths;
            const float product = (float)sixteenths / 16.0f;
            uint32_t want;
            uint32_t got;
            memcpy(&want, &product, sizeof(want));
            memcpy(&got, out + 32u * lane + 4u * i, sizeof(got));
            if (got != want && wrong++ == 0u) {
                first_row = row;
                first_column = column;
                first_got = got;
                first_want = want;
            }
        }
    }
    // The product's 128 elements come to -176 sixteenths, reckoned apart with unbounded
    // integers: the case's own matrices and sums are held to that before the machine's are.
    CHECK_TRUE(SUITE, name, CHECK_EXACT, wrong == 0u && total == -176,
               "%u of 128 elements of the product differ; the first, row %u and column %u, holds "
               "the bits 0x%08x where the integers' sum is 0x%08x (0xdeadbeef is a word nothing "
               "wrote, 0xa5a5a5a5 one nothing copied); the processor's own product sums to %d "
               "sixteenths, where it has -176",
               (unsigned)wrong, (unsigned)first_row, (unsigned)first_column, (unsigned)first_got,
               (unsigned)first_want, (int)total);
}

/// The count case: every cell's word against the processor's count.
static void MandelCountCase(const char *name) {
    char why[256] = "";
    NvnLayerClearError();
    const uint8_t *const memory = MandelDispatch(why, sizeof(why));
    if (memory == NULL) {
        ReportCase(SUITE, name, CHECK_NONE, false, why);
        return;
    }
    uint32_t wrong = 0;
    uint32_t first = 0;
    uint32_t got_first = 0;
    for (uint32_t i = 0; i < MANDEL_WORDS; ++i) {
        uint32_t got;
        memcpy(&got, memory + 4u * i, sizeof(got));
        if (got != MandelCount(i % MANDEL_COLUMNS, i / MANDEL_COLUMNS) && wrong++ == 0u) {
            first = i;
            got_first = got;
        }
    }
    CHECK_TRUE(SUITE, name, CHECK_EXACT, wrong == 0u,
               "%u of %u cells differ; the first, column %u of row %u, holds 0x%08x where the "
               "processor counts %u (0xdeadbeef is a word nothing wrote, 0xa5a5a5a5 one nothing "
               "copied)",
               (unsigned)wrong, (unsigned)MANDEL_WORDS, (unsigned)(first % MANDEL_COLUMNS),
               (unsigned)(first / MANDEL_COLUMNS), (unsigned)got_first,
               (unsigned)MandelCount(first % MANDEL_COLUMNS, first / MANDEL_COLUMNS));
}

/// The picture case: every pixel against the bars the processor works out.
static void MandelPictureCase(const char *name) {
    char why[256] = "";
    NvnLayerClearError();
    // A launch that goes on after the count case dispatches for this one.
    if (!n2.counted) {
        (void)MandelDispatch(why, sizeof(why));
    }
    const uint8_t *const pixels = n2.counted ? MandelDraw(why, sizeof(why)) : NULL;
    if (pixels == NULL) {
        ReportCase(SUITE, name, CHECK_NONE, false, why);
        return;
    }
    Picture *const picture = PictureBegin(MANDEL_SIZE, MANDEL_SIZE, PICTURE_COLOUR, pixels);
    uint32_t wrong = 0;
    uint32_t clear = 0;
    uint32_t first_x = 0;
    uint32_t first_y = 0;
    uint8_t first[4] = { 0, 0, 0, 0 };
    uint8_t first_want[4] = { 0, 0, 0, 0 };
    for (uint32_t y = 0; y < MANDEL_SIZE; ++y) {
        for (uint32_t x = 0; x < MANDEL_SIZE; ++x) {
            uint8_t want[4];
            MandelExpected(x, y, want);
            const uint8_t *const pixel = pixels + ((size_t)y * MANDEL_SIZE + x) * 4u;
            const bool right = memcmp(pixel, want, 4) == 0;
            PictureSet(picture, x, y, want, right ? PICTURE_MATCHED : PICTURE_WRONG);
            clear += memcmp(pixel, MANDEL_CLEAR, 4) == 0 ? 1u : 0u;
            if (!right && wrong++ == 0u) {
                first_x = x;
                first_y = y;
                memcpy(first, pixel, 4);
                memcpy(first_want, want, 4);
            }
        }
    }
    CHECK_TRUE(SUITE, name, CHECK_EXACT, wrong == 0u,
               "%u of %u pixels wrong; the first, (%u,%u), holds %02x%02x%02x%02x, expected "
               "%02x%02x%02x%02x; %u pixels hold the clear colour, where the picture has 7328 "
               "(all 65536 when no mesh draw ran: ScarletNX draws them with mesh_draws=1)",
               (unsigned)wrong, (unsigned)(MANDEL_SIZE * MANDEL_SIZE), (unsigned)first_x,
               (unsigned)first_y, first[0], first[1], first[2], first[3], first_want[0],
               first_want[1], first_want[2], first_want[3], (unsigned)clear);
}

void RunNvn2(void) {
    static const char *const UP = "a device, a queue, a command buffer, their pools and a "
                                  "256x256 target with its depth come up";
    static const char *const PROGRAM = "a compute program assembled by hand makes a program "
                                       "through the builder";
    static const char *const SCALE = "a dispatch's writes to a storage buffer, as the "
                                     "processor reads them once the queue finishes";
    static const char *const SCALE_COPIED = "a dispatch's writes to a storage buffer, as the "
                                            "graphics unit copies them to another buffer";
    static const char *const MANDEL_PROGRAMS = "the Mandelbrot's compute, mesh and fragment "
                                               "programs make two programs";
    static const char *const MANDEL_COUNT = "a compute program counts the Mandelbrot set's "
                                            "steps for every cell of a grid";
    static const char *const MANDEL_PICTURE = "a mesh program draws the counts as bars, a "
                                              "fragment program colours them and the depth "
                                              "test keeps the nearest";
    static const char *const HMMA = "one warp's matrix instruction multiplies a 16 by 16 matrix "
                                    "of halves by a 16 by 8 one into floats";
    const char *const AFTER[] = { PROGRAM,      SCALE,          SCALE_COPIED, MANDEL_PROGRAMS,
                                  MANDEL_COUNT, MANDEL_PICTURE, HMMA };

    memset(&n2, 0, sizeof(n2));
    NvnLayerClearError();
    if (!ReportStart(SUITE, UP)) {
        return;
    }
    char why[256] = "";
    const bool up = BringUp(why, sizeof(why));
    CHECK_TRUE(SUITE, UP, CHECK_NONE, up, "%s", why);
    if (!up) {
        for (size_t i = 0; i < sizeof(AFTER) / sizeof(AFTER[0]); ++i) {
            ReportSkip(SUITE, AFTER[i], REPORT_REASON_DEPENDS,
                       "the device, the queue, the command buffer, a pool or a texture did not "
                       "come up");
        }
        TearDown();
        return;
    }

    bool built = false;
    if (ReportStart(SUITE, PROGRAM)) {
        NvnLayerClearError();
        const Stage scale = { scale_comp_bin, scale_comp_bin_size, scale_comp_ctl };
        built = BuildProgram(PROGRAM_SCALE, &scale, 1, why, sizeof(why));
        CHECK_TRUE(SUITE, PROGRAM, CHECK_NONE, built, "%s", why);
    }
    if (!built) {
        ReportSkip(SUITE, SCALE, REPORT_REASON_DEPENDS, "the compute program made no program");
        ReportSkip(SUITE, SCALE_COPIED, REPORT_REASON_DEPENDS,
                   "the compute program made no program");
    } else {
        const uint8_t *memory = NULL;
        if (ReportStart(SUITE, SCALE)) {
            NvnLayerClearError();
            memory = Dispatch(why, sizeof(why));
            ScaleJudge(SCALE, memory != NULL ? memory + SCALE_OUT_BYTE : NULL, why);
        }
        if (ReportStart(SUITE, SCALE_COPIED)) {
            NvnLayerClearError();
            // A launch that goes on after the case above dispatches for this one.
            if (memory == NULL) {
                memory = Dispatch(why, sizeof(why));
            }
            const NVNbufferAddress out =
                n2.nvn.nvnMemoryPoolGetBufferAddress(&n2.data_pool.pool) + SCALE_OUT_BYTE;
            ScaleJudge(SCALE_COPIED,
                       memory != NULL ? CopyOut(out, 4u * SCALE_WORDS, why, sizeof(why)) : NULL,
                       why);
        }
    }

    bool mandel = false;
    if (ReportStart(SUITE, MANDEL_PROGRAMS)) {
        NvnLayerClearError();
        const Stage count = { mandel_comp_bin, mandel_comp_bin_size, mandel_comp_ctl };
        const Stage draw[2] = {
            { mandel_mesh_bin, mandel_mesh_bin_size, mandel_mesh_ctl },
            { mandel_frag_bin, mandel_frag_bin_size, mandel_frag_ctl },
        };
        mandel = BuildProgram(PROGRAM_MANDEL_COUNT, &count, 1, why, sizeof(why)) &&
                 BuildProgram(PROGRAM_MANDEL_DRAW, draw, 2, why, sizeof(why));
        CHECK_TRUE(SUITE, MANDEL_PROGRAMS, CHECK_NONE, mandel, "%s", why);
    }
    if (!mandel) {
        ReportSkip(SUITE, MANDEL_COUNT, REPORT_REASON_DEPENDS,
                   "the Mandelbrot's programs made no programs");
        ReportSkip(SUITE, MANDEL_PICTURE, REPORT_REASON_DEPENDS,
                   "the Mandelbrot's programs made no programs");
    } else {
        // The picture is drawn from the counts the dispatch left, whatever the processor read
        // of them: the two cases fail apart.
        if (ReportStart(SUITE, MANDEL_COUNT)) {
            MandelCountCase(MANDEL_COUNT);
        }
        if (ReportStart(SUITE, MANDEL_PICTURE)) {
            MandelPictureCase(MANDEL_PICTURE);
        }
    }

    if (ReportStart(SUITE, HMMA)) {
        NvnLayerClearError();
        const Stage hmma = { hmma_comp_bin, hmma_comp_bin_size, hmma_comp_ctl };
        if (BuildProgram(PROGRAM_HMMA, &hmma, 1, why, sizeof(why))) {
            HmmaCase(HMMA);
        } else {
            ReportCase(SUITE, HMMA, CHECK_NONE, false, why);
        }
    }
    TearDown();
}
