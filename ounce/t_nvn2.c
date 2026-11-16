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
#define IMAGE_BYTES (2u * MANDEL_SIZE * MANDEL_SIZE * 8u + 8u * POOL_ALIGNMENT)
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

// The numbers the graphics unit reads for the indirect dispatch and the indirect mesh draws, in
// the data pool past the matrix case's buffer.
#define INDIRECT_BYTE 0x3000u

// The image cases' two images, 16 by 16 texels of a word, and the words of both in the data pool
// past the indirect numbers: what image 1 is filled with, then what image 0 starts as.
#define IMAGE_SIDE 16u
#define IMAGE_WORDS (IMAGE_SIDE * IMAGE_SIDE)
#define IMAGE_SOURCE_BYTE 0x4000u
#define IMAGE_PRESET_BYTE 0x4400u
#define DESCRIPTOR_BYTES 32u

// The case of a texture and a sampler bound apart: 4 by 4 textures, sixteen words written, the
// output and the textures' texels in the data pool past the images' words.
#define APART_SIDE 4u
#define APART_WORDS 16u
#define APART_OUT_BYTE 0x4800u
#define APART_TEXELS_BYTE 0x4900u

/// Which calls bind the image program's two images, or the two textures and samplers bound apart.
typedef enum {
    IMAGES_ONE_BY_ONE, ///< the calls that take one, twice
    IMAGES_SEVERAL,    ///< the calls that take several
    IMAGES_DEFERRED,   ///< the deferred calls
} ImagesKind;

/// How the scale case's dispatch binds its storage buffer and gives its numbers.
typedef enum {
    DISPATCH_PLAIN,
    DISPATCH_BUFFERS,          ///< nvnCommandBufferBindStorageBuffers
    DISPATCH_BUFFERS_DEFERRED, ///< nvnCommandBufferBindStorageBuffersDeferred
    DISPATCH_INDIRECT,         ///< nvnCommandBufferDispatchComputeIndirect
    DISPATCH_PUSHED,           ///< the pushed program, its numbers by PushComputeConstants
} DispatchKind;

// What the pushed program multiplies by and adds: words 2 and 3 of root table 1.
#define PUSHED_TABLE 1
#define PUSHED_BYTE 8
#define PUSHED_TIMES 5u
#define PUSHED_PLUS 77u
// The pushed fragment program's green and blue, floats at byte 0x10 of root table 2, and the bytes
// a target of eight bits a channel holds for them: 153 and 51 of 255.
#define PUSHED_COLOUR_TABLE 2
#define PUSHED_COLOUR_BYTE 0x10
#define PUSHED_GREEN 0.6f
#define PUSHED_BLUE 0.2f
#define PUSHED_GREEN_BYTE 0x99u
#define PUSHED_BLUE_BYTE 0x33u

/// Which of the mesh draws draws the Mandelbrot's rows.
typedef enum {
    MESH_TASKS,
    MESH_TASKS_3D,
    MESH_INDIRECT,
    MESH_3D_INDIRECT,
    MESH_MULTI,
    MESH_MULTI_3D,
    MESH_MULTI_COUNT,
    MESH_MULTI_3D_COUNT,
    MESH_PUSHED,   ///< the pushed fragment program, its colour by PushGraphicsConstants
    MESH_NO_RATE,  ///< nvnCommandBufferBindShadingRateImage with no texture
} MeshKind;

typedef enum {
    PROGRAM_SCALE,
    PROGRAM_MANDEL_COUNT,
    PROGRAM_MANDEL_DRAW,
    PROGRAM_HMMA,
    PROGRAM_PUSHED,
    PROGRAM_MANDEL_PUSHED,
    PROGRAM_IMAGE,
    PROGRAM_APART,
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
    Pool descriptor_pool;
    NVNtexturePool texture_pool;
    NVNsamplerPool sampler_pool;
    NVNtexture images[2];
    NVNimageHandle image_handles[2];
    NVNtexture apart_textures[2];
    NVNsampler apart_samplers[2];
    NVNseparateTextureHandle apart_texture_handles[2];
    NVNseparateSamplerHandle apart_sampler_handles[2];
    int texture_base;
    int sampler_base;
    void *control;
    size_t code_used;
    size_t image_used;
    bool device_up;
    bool queue_up;
    bool command_up;
    bool program_up[PROGRAM_COUNT];
    bool target_up;
    bool depth_up;
    bool texture_pool_up;
    bool sampler_pool_up;
    int images_up;   ///< how many of the two images were made
    bool images_ready;
    int apart_textures_up;
    int apart_samplers_up;
    bool apart_ready;
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
    if (n2.sampler_pool_up) {
        n2.nvn.nvnSamplerPoolFinalize(&n2.sampler_pool);
    }
    if (n2.texture_pool_up) {
        n2.nvn.nvnTexturePoolFinalize(&n2.texture_pool);
    }
    for (int i = 0; i < n2.apart_samplers_up; ++i) {
        n2.nvn.nvnSamplerFinalize(&n2.apart_samplers[i]);
    }
    for (int i = 0; i < n2.apart_textures_up; ++i) {
        n2.nvn.nvnTextureFinalize(&n2.apart_textures[i]);
    }
    for (int i = 0; i < n2.images_up; ++i) {
        n2.nvn.nvnTextureFinalize(&n2.images[i]);
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
    DropPool(&n2.descriptor_pool);
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

/// A square 2D texture of @p side texels with @p flags, its storage carved from the image pool.
static bool MakeTextureOf(NVNtexture *texture, NVNformat format, uint32_t side, int flags,
                          char *why, size_t why_size) {
    _Alignas(8) NVNtextureBuilder builder;
    memset(&builder, 0, sizeof(builder));
    n2.nvn.nvnTextureBuilderSetDevice(&builder, &n2.device);
    n2.nvn.nvnTextureBuilderSetDefaults(&builder);
    n2.nvn.nvnTextureBuilderSetFlags(&builder, flags);
    n2.nvn.nvnTextureBuilderSetTarget(&builder, NVN_TEXTURE_TARGET_2D);
    n2.nvn.nvnTextureBuilderSetFormat(&builder, format);
    n2.nvn.nvnTextureBuilderSetSize2D(&builder, (int)side, (int)side);
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

/// A texture of the Mandelbrot's size.
static bool MakeTexture(NVNtexture *texture, NVNformat format, char *why, size_t why_size) {
    return MakeTextureOf(texture, format, MANDEL_SIZE, 0, why, why_size);
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
static const uint8_t *Dispatch(DispatchKind kind, char *why, size_t why_size) {
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
    n2.nvn.nvnCommandBufferBindProgram(
        &n2.command, &n2.programs[kind == DISPATCH_PUSHED ? PROGRAM_PUSHED : PROGRAM_SCALE],
        NVN_SHADER_STAGE_COMPUTE_BIT);
    if (kind == DISPATCH_PUSHED) {
        // Two words at the table's byte 8, then the second word alone over the one just pushed:
        // a push starts at its offset and leaves the words before it.
        const uint32_t both[2] = { PUSHED_TIMES, 1000u };
        const uint32_t plus = PUSHED_PLUS;
        n2.nvn.nvnCommandBufferPushComputeConstants(&n2.command, PUSHED_TABLE, PUSHED_BYTE,
                                                    (int)sizeof(both), both);
        n2.nvn.nvnCommandBufferPushComputeConstants(&n2.command, PUSHED_TABLE, PUSHED_BYTE + 4,
                                                    (int)sizeof(plus), &plus);
    }
    const NVNbufferAddress buffer = n2.nvn.nvnMemoryPoolGetBufferAddress(&n2.data_pool.pool);
    // A deferred bind's range is read when the list runs: it stays where it is.
    static NVNbufferRange range;
    range.address = buffer;
    range.size = SCALE_OUT_BYTE + 4u * SCALE_WORDS;
    if (kind == DISPATCH_BUFFERS) {
        n2.nvn.nvnCommandBufferBindStorageBuffers(&n2.command, NVN_SHADER_STAGE_COMPUTE, 0, 1,
                                                  &range);
    } else if (kind == DISPATCH_BUFFERS_DEFERRED) {
        n2.nvn.nvnCommandBufferBindStorageBuffersDeferred(&n2.command, NVN_SHADER_STAGE_COMPUTE,
                                                          0, 1, &range);
    } else {
        n2.nvn.nvnCommandBufferBindStorageBuffer(&n2.command, NVN_SHADER_STAGE_COMPUTE, 0, buffer,
                                                 range.size);
    }
    if (kind == DISPATCH_INDIRECT) {
        // The three counts of blocks, where the graphics unit reads them.
        const uint32_t blocks[3] = { SCALE_BLOCKS, 1, 1 };
        memcpy(memory + INDIRECT_BYTE, blocks, sizeof(blocks));
        n2.nvn.nvnCommandBufferDispatchComputeIndirect(&n2.command, buffer + INDIRECT_BYTE);
    } else {
        n2.nvn.nvnCommandBufferDispatchCompute(&n2.command, (int)SCALE_BLOCKS, 1, 1);
    }
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

/// The words at @p out against the processor's arithmetic: the scale program's three times the
/// word plus its index, or with @p pushed the pushed program's five times the word plus 77;
/// @p why is what kept them from being read, when @p out is NULL.
static void ScaleJudgeAs(const char *name, const uint8_t *out, const char *why, bool pushed) {
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
        const uint32_t want = pushed ? ScaleInput(i) * PUSHED_TIMES + PUSHED_PLUS
                                     : ScaleInput(i) * 3u + i;
        if (got != want && wrong++ == 0u) {
            first = i;
            got_first = got;
        }
    }
    const uint32_t times = pushed ? PUSHED_TIMES : 3u;
    const uint32_t plus = pushed ? PUSHED_PLUS : first;
    CHECK_TRUE(SUITE, name, CHECK_EXACT, wrong == 0u,
               "%u of %u words differ; the first, word %u, is 0x%08x where %u times 0x%08x "
               "plus %u is 0x%08x (0xdeadbeef is a word nothing wrote, 0xa5a5a5a5 one nothing "
               "copied)",
               (unsigned)wrong, (unsigned)SCALE_WORDS, (unsigned)first, (unsigned)got_first,
               (unsigned)times, (unsigned)ScaleInput(first), (unsigned)plus,
               (unsigned)(ScaleInput(first) * times + plus));
}

static void ScaleJudge(const char *name, const uint8_t *out, const char *why) {
    ScaleJudgeAs(name, out, why, false);
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
static const uint8_t *MandelDraw(MeshKind kind, char *why, size_t why_size) {
    const NvnProcs *const n = &n2.nvn;
    // The numbers of the indirect draws, written before the list is recorded: a row's workgroups
    // as a count and a first workgroup, or as three counts for the draws of a grid; the rows
    // whole, and in two halves for the draws of several; and how many of those the graphics
    // unit is to draw.
    uint8_t *const numbers = n->nvnMemoryPoolMap(&n2.data_pool.pool);
    if (numbers == NULL) {
        snprintf(why, why_size, "nvnMemoryPoolMap gave no address for the buffer's pool: %s",
                 Why("NULL"));
        return NULL;
    }
    const NVNbufferAddress indirect =
        n->nvnMemoryPoolGetBufferAddress(&n2.data_pool.pool) + INDIRECT_BYTE;
    const uint32_t whole[2] = { MANDEL_ROWS, 0 };
    const uint32_t grid[3] = { MANDEL_ROWS, 1, 1 };
    const uint32_t halves[4] = { MANDEL_ROWS / 2u, 0, MANDEL_ROWS - MANDEL_ROWS / 2u,
                                 MANDEL_ROWS / 2u };
    const uint32_t two = 2, one = 1;
    memcpy(numbers + INDIRECT_BYTE + 0x100, whole, sizeof(whole));
    memcpy(numbers + INDIRECT_BYTE + 0x110, grid, sizeof(grid));
    memcpy(numbers + INDIRECT_BYTE + 0x120, halves, sizeof(halves));
    memcpy(numbers + INDIRECT_BYTE + 0x140, &two, sizeof(two));
    memcpy(numbers + INDIRECT_BYTE + 0x144, &one, sizeof(one));
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

    n->nvnCommandBufferBindProgram(
        &n2.command,
        &n2.programs[kind == MESH_PUSHED ? PROGRAM_MANDEL_PUSHED : PROGRAM_MANDEL_DRAW],
        NVN_SHADER_STAGE_ALL_BITS_231);
    if (kind == MESH_PUSHED) {
        const float colour[2] = { PUSHED_GREEN, PUSHED_BLUE };
        n->nvnCommandBufferPushGraphicsConstants(&n2.command, PUSHED_COLOUR_TABLE,
                                                 PUSHED_COLOUR_BYTE, (int)sizeof(colour), colour);
    }
    if (kind == MESH_NO_RATE) {
        // No texture is no shading rate image: every pixel is shaded, as with none ever bound.
        n->nvnCommandBufferBindShadingRateImage(&n2.command, NULL, NULL);
    }
    n->nvnCommandBufferSetGeometryProcessingMode(&n2.command, GEOMETRY_PROCESSING_MESH);
    n->nvnCommandBufferBindStorageBuffer(
        &n2.command, NVN_SHADER_STAGE_MESH, 0,
        n->nvnMemoryPoolGetBufferAddress(&n2.data_pool.pool) + MANDEL_BYTE, 4u * MANDEL_WORDS);
    switch (kind) {
    case MESH_TASKS_3D:
        n->nvnCommandBufferDrawMeshTasks3D(&n2.command, (int)MANDEL_ROWS, 1, 1);
        break;
    case MESH_INDIRECT:
        n->nvnCommandBufferDrawMeshTasksIndirect(&n2.command, indirect + 0x100);
        break;
    case MESH_3D_INDIRECT:
        n->nvnCommandBufferDrawMeshTasks3DIndirect(&n2.command, indirect + 0x110);
        break;
    case MESH_MULTI:
        n->nvnCommandBufferMultiDrawMeshTasksIndirect(&n2.command, indirect + 0x120, 2, 8);
        break;
    case MESH_MULTI_3D:
        n->nvnCommandBufferMultiDrawMeshTasks3DIndirect(&n2.command, indirect + 0x110, 1, 12);
        break;
    case MESH_MULTI_COUNT:
        n->nvnCommandBufferMultiDrawMeshTasksIndirectCount(&n2.command, indirect + 0x120,
                                                           indirect + 0x140, 2, 8);
        break;
    case MESH_MULTI_3D_COUNT:
        n->nvnCommandBufferMultiDrawMeshTasks3DIndirectCount(&n2.command, indirect + 0x110,
                                                             indirect + 0x144, 1, 12);
        break;
    default:
        n->nvnCommandBufferDrawMeshTasks(&n2.command, 0, (int)MANDEL_ROWS);
        break;
    }
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

/// What image 1 holds at (@p x, @p y): every byte of the word different, and no two texels alike.
static uint32_t ImageInput(uint32_t x, uint32_t y) {
    return 0x10203040u + (y * IMAGE_SIDE + x) * 0x00010305u;
}

/// The texture pool and the sampler pool, made once, with room past what the device reserves
/// for the suite's four textures and two samplers.
static bool DescriptorsUp(char *why, size_t why_size) {
    if (n2.texture_pool_up && n2.sampler_pool_up) {
        return true;
    }
    n2.nvn.nvnDeviceGetInteger(&n2.device, NVN_DEVICE_INFO_RESERVED_TEXTURE_DESCRIPTORS,
                               &n2.texture_base);
    n2.nvn.nvnDeviceGetInteger(&n2.device, NVN_DEVICE_INFO_RESERVED_SAMPLER_DESCRIPTORS,
                               &n2.sampler_base);
    if (n2.texture_base < 0 || n2.texture_base > 4096 || n2.sampler_base < 0 ||
        n2.sampler_base > 4096) {
        snprintf(why, why_size, "the device reserves %d texture and %d sampler descriptors, which "
                                "the suite has no pool for (0 to 4096 of each)",
                 n2.texture_base, n2.sampler_base);
        return false;
    }
    const size_t textures =
        RoundUp((size_t)(n2.texture_base + 4) * DESCRIPTOR_BYTES, 0x100u);
    if (!n2.descriptor_pool.up &&
        !MakePool(&n2.descriptor_pool,
                  textures + (size_t)(n2.sampler_base + 2) * DESCRIPTOR_BYTES,
                  NVN_MEMORY_POOL_FLAGS_CPU_UNCACHED | NVN_MEMORY_POOL_FLAGS_GPU_CACHED)) {
        snprintf(why, why_size, "the descriptors' pool: %s", Why("no memory, or false"));
        return false;
    }
    if (!n2.texture_pool_up) {
        n2.texture_pool_up = n2.nvn.nvnTexturePoolInitialize(
            &n2.texture_pool, &n2.descriptor_pool.pool, 0, n2.texture_base + 4);
    }
    if (!n2.sampler_pool_up) {
        n2.sampler_pool_up = n2.nvn.nvnSamplerPoolInitialize(
            &n2.sampler_pool, &n2.descriptor_pool.pool, (ptrdiff_t)textures, n2.sampler_base + 2);
    }
    if (!n2.texture_pool_up || !n2.sampler_pool_up) {
        snprintf(why, why_size, "the texture and sampler pools: %s", Why("false"));
        return false;
    }
    return true;
}

/// The image cases' own objects, made once: two textures a program may store into, each
/// registered in the texture pool as an image, and each one's handle.
static bool ImagesUp(char *why, size_t why_size) {
    if (n2.images_ready) {
        return true;
    }
    if (!DescriptorsUp(why, why_size)) {
        return false;
    }
    while (n2.images_up < 2) {
        if (!MakeTextureOf(&n2.images[n2.images_up], NVN_FORMAT_R32UI, IMAGE_SIDE,
                           NVN_TEXTURE_FLAGS_IMAGE, why, why_size)) {
            return false;
        }
        ++n2.images_up;
    }
    for (int i = 0; i < 2; ++i) {
        n2.nvn.nvnTexturePoolRegisterImage(&n2.texture_pool, n2.texture_base + i, &n2.images[i],
                                           NULL);
        n2.image_handles[i] = n2.nvn.nvnDeviceGetImageHandle(&n2.device, n2.texture_base + i);
    }
    if (NvnLayerError()[0] != '\0') {
        snprintf(why, why_size, "the images' descriptors: %s", NvnLayerError());
        return false;
    }
    n2.images_ready = true;
    return true;
}

/// What the texture sampled apart holds at (@p x, @p y), and what the decoy beside it does: floats
/// a quarter apart, so each is exact and no two texels of the two are alike.
static float ApartTexel(uint32_t x, uint32_t y, bool decoy) {
    return (decoy ? -1.0f : 1.0f) * (1.0f + 0.25f * (float)(y * APART_SIDE + x));
}

/// The cases of a texture and a sampler bound apart, their own objects made once: two 4 by 4
/// textures of a float a texel, the decoy first, and two samplers that take the nearest texel,
/// one that holds the edge and one that repeats; each registered and its separate handle asked.
static bool ApartUp(char *why, size_t why_size) {
    if (n2.apart_ready) {
        return true;
    }
    if (!DescriptorsUp(why, why_size)) {
        return false;
    }
    uint8_t *const memory = n2.nvn.nvnMemoryPoolMap(&n2.data_pool.pool);
    if (memory == NULL) {
        snprintf(why, why_size, "nvnMemoryPoolMap gave no address for the buffer's pool: %s",
                 Why("NULL"));
        return false;
    }
    while (n2.apart_textures_up < 2) {
        if (!MakeTextureOf(&n2.apart_textures[n2.apart_textures_up], NVN_FORMAT_R32F, APART_SIDE,
                           0, why, why_size)) {
            return false;
        }
        ++n2.apart_textures_up;
    }
    while (n2.apart_samplers_up < 2) {
        const NVNwrapMode wrap = n2.apart_samplers_up == 0 ? NVN_WRAP_MODE_CLAMP_TO_EDGE
                                                           : NVN_WRAP_MODE_REPEAT;
        _Alignas(8) NVNsamplerBuilder builder;
        memset(&builder, 0, sizeof(builder));
        n2.nvn.nvnSamplerBuilderSetDevice(&builder, &n2.device);
        n2.nvn.nvnSamplerBuilderSetDefaults(&builder);
        n2.nvn.nvnSamplerBuilderSetMinMagFilter(&builder, NVN_MIN_FILTER_NEAREST,
                                                NVN_MAG_FILTER_NEAREST);
        n2.nvn.nvnSamplerBuilderSetWrapMode(&builder, wrap, wrap, wrap);
        if (!n2.nvn.nvnSamplerInitialize(&n2.apart_samplers[n2.apart_samplers_up], &builder)) {
            snprintf(why, why_size, "nvnSamplerInitialize: %s", Why("false"));
            return false;
        }
        ++n2.apart_samplers_up;
    }
    const NVNbufferAddress data = n2.nvn.nvnMemoryPoolGetBufferAddress(&n2.data_pool.pool);
    const NVNcopyRegion region = { 0, 0, 0, (int)APART_SIDE, (int)APART_SIDE, 1 };
    n2.nvn.nvnCommandBufferBeginRecording(&n2.command);
    for (uint32_t t = 0; t < 2u; ++t) {
        // Texture 0 is the decoy, texture 1 the one sampled.
        for (uint32_t i = 0; i < APART_SIDE * APART_SIDE; ++i) {
            const float texel = ApartTexel(i % APART_SIDE, i / APART_SIDE, t == 0u);
            memcpy(memory + APART_TEXELS_BYTE + 0x40u * t + 4u * i, &texel, sizeof(texel));
        }
        n2.nvn.nvnCommandBufferCopyBufferToTexture(&n2.command,
                                                   data + APART_TEXELS_BYTE + 0x40u * t,
                                                   &n2.apart_textures[t], NULL, &region,
                                                   NVN_COPY_FLAGS_NONE);
    }
    if (!Finish("the textures' upload", why, why_size)) {
        return false;
    }
    for (int i = 0; i < 2; ++i) {
        n2.nvn.nvnTexturePoolRegisterTexture(&n2.texture_pool, n2.texture_base + 2 + i,
                                             &n2.apart_textures[i], NULL);
        n2.nvn.nvnSamplerPoolRegisterSampler(&n2.sampler_pool, n2.sampler_base + i,
                                             &n2.apart_samplers[i]);
        n2.apart_texture_handles[i].value =
            n2.nvn.nvnDeviceGetSeparateTextureHandle(&n2.device, n2.texture_base + 2 + i);
        n2.apart_sampler_handles[i].value =
            n2.nvn.nvnDeviceGetSeparateSamplerHandle(&n2.device, n2.sampler_base + i);
    }
    if (NvnLayerError()[0] != '\0') {
        snprintf(why, why_size, "the textures' and samplers' descriptors: %s", NvnLayerError());
        return false;
    }
    n2.apart_ready = true;
    return true;
}

/// The case of a texture and a sampler bound apart: the decoy and the sampler that holds the
/// edge in slot 0 of each, the texture and the sampler that repeats in slot 1, bound by
/// @p kind's calls; sixteen invocations sample slot 1 of both past the texture's right edge, and
/// each float they write is held to the texel a repeat lands on.
static void ApartCase(const char *name, ImagesKind kind) {
    char why[256] = "";
    NvnLayerClearError();
    const NvnProcs *const n = &n2.nvn;
    uint8_t *const memory = n->nvnMemoryPoolMap(&n2.data_pool.pool);
    if (memory == NULL || !ApartUp(why, sizeof(why))) {
        ReportCase(SUITE, name, CHECK_NONE, false,
                   why[0] != '\0' ? why : Why("nvnMemoryPoolMap gave no address"));
        return;
    }
    for (uint32_t i = 0; i < APART_WORDS; ++i) {
        const uint32_t preset = 0xdeadbeefu;
        memcpy(memory + APART_OUT_BYTE + 4u * i, &preset, sizeof(preset));
    }
    const NVNbufferAddress out =
        n->nvnMemoryPoolGetBufferAddress(&n2.data_pool.pool) + APART_OUT_BYTE;
    n->nvnCommandBufferBeginRecording(&n2.command);
    n->nvnCommandBufferBarrier(&n2.command, NVN_BARRIER_INVALIDATE_TEXTURE_BIT |
                                                NVN_BARRIER_INVALIDATE_SHADER_BIT);
    n->nvnCommandBufferSetTexturePool(&n2.command, &n2.texture_pool);
    n->nvnCommandBufferSetSamplerPool(&n2.command, &n2.sampler_pool);
    n->nvnCommandBufferBindProgram(&n2.command, &n2.programs[PROGRAM_APART],
                                   NVN_SHADER_STAGE_COMPUTE_BIT);
    n->nvnCommandBufferBindStorageBuffer(&n2.command, NVN_SHADER_STAGE_COMPUTE, 0, out,
                                         4u * APART_WORDS);
    // A deferred bind's handles are read when the list runs: they stay where they are.
    static NVNseparateTextureHandle textures[2];
    static NVNseparateSamplerHandle samplers[2];
    for (int i = 0; i < 2; ++i) {
        textures[i] = n2.apart_texture_handles[i];
        samplers[i] = n2.apart_sampler_handles[i];
    }
    if (kind == IMAGES_SEVERAL) {
        n->nvnCommandBufferBindSeparateTextures(&n2.command, NVN_SHADER_STAGE_COMPUTE, 0, 2,
                                                textures);
        n->nvnCommandBufferBindSeparateSamplers(&n2.command, NVN_SHADER_STAGE_COMPUTE, 0, 2,
                                                samplers);
    } else if (kind == IMAGES_DEFERRED) {
        n->nvnCommandBufferBindSeparateTexturesDeferred(&n2.command, NVN_SHADER_STAGE_COMPUTE, 0,
                                                        2, textures);
        n->nvnCommandBufferBindSeparateSamplersDeferred(&n2.command, NVN_SHADER_STAGE_COMPUTE, 0,
                                                        2, samplers);
    } else {
        for (int i = 0; i < 2; ++i) {
            n->nvnCommandBufferBindSeparateTexture(&n2.command, NVN_SHADER_STAGE_COMPUTE, i,
                                                   textures[i]);
            n->nvnCommandBufferBindSeparateSampler(&n2.command, NVN_SHADER_STAGE_COMPUTE, i,
                                                   samplers[i]);
        }
    }
    n->nvnCommandBufferDispatchCompute(&n2.command, 1, 1, 1);
    const uint8_t *const copy = Finish("the dispatch that samples", why, sizeof(why))
                                    ? CopyOut(out, 4u * APART_WORDS, why, sizeof(why))
                                    : NULL;
    if (copy == NULL) {
        ReportCase(SUITE, name, CHECK_NONE, false, why);
        return;
    }
    uint32_t wrong = 0;
    uint32_t first = 0;
    uint32_t got_first = 0;
    for (uint32_t i = 0; i < APART_WORDS; ++i) {
        uint32_t got;
        uint32_t want;
        const float texel = ApartTexel(i % APART_SIDE, 1, false);
        memcpy(&got, copy + 4u * i, sizeof(got));
        memcpy(&want, &texel, sizeof(want));
        if (got != want && wrong++ == 0u) {
            first = i;
            got_first = got;
        }
    }
    const float want_first = ApartTexel(first % APART_SIDE, 1, false);
    float got_float;
    memcpy(&got_float, &got_first, sizeof(got_float));
    CHECK_TRUE(SUITE, name, CHECK_EXACT, wrong == 0u,
               "%u of %u words differ; the first, word %u, is 0x%08x (%g) where the texture's "
               "texel (%u,1) is %g (%g is what the sampler that holds the edge gives, a number "
               "below nought the decoy, 0xdeadbeef a word nothing wrote)",
               (unsigned)wrong, (unsigned)APART_WORDS, (unsigned)first, (unsigned)got_first,
               (double)got_float, (unsigned)(first % APART_SIDE), (double)want_first,
               (double)ApartTexel(APART_SIDE - 1u, 1, false));
}

/// The image case: image 1 filled and image 0 preset from a buffer, the program bound with both
/// as its images by @p kind's call, one block dispatched, and image 0 copied to memory the
/// processor reads, every word against three times image 1's plus its column.
static void ImageCase(const char *name, ImagesKind kind) {
    char why[256] = "";
    NvnLayerClearError();
    const NvnProcs *const n = &n2.nvn;
    uint8_t *const memory = n->nvnMemoryPoolMap(&n2.data_pool.pool);
    uint8_t *const read = n->nvnMemoryPoolMap(&n2.read_pool.pool);
    if (memory == NULL || read == NULL || !ImagesUp(why, sizeof(why))) {
        ReportCase(SUITE, name, CHECK_NONE, false,
                   why[0] != '\0' ? why : Why("nvnMemoryPoolMap gave no address"));
        return;
    }
    for (uint32_t i = 0; i < IMAGE_WORDS; ++i) {
        const uint32_t in = ImageInput(i % IMAGE_SIDE, i / IMAGE_SIDE);
        const uint32_t preset = 0xdeadbeefu;
        memcpy(memory + IMAGE_SOURCE_BYTE + 4u * i, &in, sizeof(in));
        memcpy(memory + IMAGE_PRESET_BYTE + 4u * i, &preset, sizeof(preset));
    }
    memset(read, READ_PRESET, 4u * IMAGE_WORDS);
    const NVNbufferAddress data = n->nvnMemoryPoolGetBufferAddress(&n2.data_pool.pool);
    const NVNcopyRegion region = { 0, 0, 0, (int)IMAGE_SIDE, (int)IMAGE_SIDE, 1 };
    n->nvnCommandBufferBeginRecording(&n2.command);
    n->nvnCommandBufferCopyBufferToTexture(&n2.command, data + IMAGE_PRESET_BYTE, &n2.images[0],
                                           NULL, &region, NVN_COPY_FLAGS_NONE);
    n->nvnCommandBufferCopyBufferToTexture(&n2.command, data + IMAGE_SOURCE_BYTE, &n2.images[1],
                                           NULL, &region, NVN_COPY_FLAGS_NONE);
    n->nvnCommandBufferBarrier(&n2.command, NVN_BARRIER_INVALIDATE_TEXTURE_BIT |
                                                NVN_BARRIER_INVALIDATE_SHADER_BIT);
    n->nvnCommandBufferSetTexturePool(&n2.command, &n2.texture_pool);
    n->nvnCommandBufferBindProgram(&n2.command, &n2.programs[PROGRAM_IMAGE],
                                   NVN_SHADER_STAGE_COMPUTE_BIT);
    // A deferred bind's handles are read when the list runs: they stay where they are.
    static NVNimageHandle handles[2];
    handles[0] = n2.image_handles[0];
    handles[1] = n2.image_handles[1];
    if (kind == IMAGES_SEVERAL) {
        n->nvnCommandBufferBindImages(&n2.command, NVN_SHADER_STAGE_COMPUTE, 0, 2, handles);
    } else if (kind == IMAGES_DEFERRED) {
        n->nvnCommandBufferBindImagesDeferred(&n2.command, NVN_SHADER_STAGE_COMPUTE, 0, 2,
                                              handles);
    } else {
        n->nvnCommandBufferBindImage(&n2.command, NVN_SHADER_STAGE_COMPUTE, 0, handles[0]);
        n->nvnCommandBufferBindImage(&n2.command, NVN_SHADER_STAGE_COMPUTE, 1, handles[1]);
    }
    n->nvnCommandBufferDispatchCompute(&n2.command, 1, 1, 1);
    n->nvnCommandBufferBarrier(&n2.command, NVN_BARRIER_INVALIDATE_TEXTURE_BIT);
    n->nvnCommandBufferCopyTextureToBuffer(&n2.command, &n2.images[0], NULL, &region,
                                           n->nvnMemoryPoolGetBufferAddress(&n2.read_pool.pool),
                                           NVN_COPY_FLAGS_NONE);
    if (!Finish("the dispatch into an image", why, sizeof(why))) {
        ReportCase(SUITE, name, CHECK_NONE, false, why);
        return;
    }
    uint32_t wrong = 0;
    uint32_t first = 0;
    uint32_t got_first = 0;
    for (uint32_t i = 0; i < IMAGE_WORDS; ++i) {
        uint32_t got;
        memcpy(&got, read + 4u * i, sizeof(got));
        const uint32_t x = i % IMAGE_SIDE;
        if (got != ImageInput(x, i / IMAGE_SIDE) * 3u + x && wrong++ == 0u) {
            first = i;
            got_first = got;
        }
    }
    const uint32_t first_x = first % IMAGE_SIDE;
    const uint32_t first_y = first / IMAGE_SIDE;
    CHECK_TRUE(SUITE, name, CHECK_EXACT, wrong == 0u,
               "%u of %u texels differ; the first, (%u,%u), is 0x%08x where three times 0x%08x "
               "plus %u is 0x%08x (0xdeadbeef is a texel nothing stored, 0xa5a5a5a5 one nothing "
               "copied)",
               (unsigned)wrong, (unsigned)IMAGE_WORDS, (unsigned)first_x, (unsigned)first_y,
               (unsigned)got_first, (unsigned)ImageInput(first_x, first_y), (unsigned)first_x,
               (unsigned)(ImageInput(first_x, first_y) * 3u + first_x));
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

/// The scale case again with its dispatch made another way, its words read as the graphics
/// unit copies them out.
static void ScaleAgain(const char *name, DispatchKind kind) {
    char why[256] = "";
    NvnLayerClearError();
    const uint8_t *const memory = Dispatch(kind, why, sizeof(why));
    const NVNbufferAddress out =
        n2.nvn.nvnMemoryPoolGetBufferAddress(&n2.data_pool.pool) + SCALE_OUT_BYTE;
    ScaleJudgeAs(name, memory != NULL ? CopyOut(out, 4u * SCALE_WORDS, why, sizeof(why)) : NULL,
                 why, kind == DISPATCH_PUSHED);
}

/// The picture case: every pixel against the bars the processor works out.
static void MandelPictureCase(const char *name, MeshKind kind) {
    char why[256] = "";
    NvnLayerClearError();
    // A launch that goes on after the count case dispatches for this one.
    if (!n2.counted) {
        (void)MandelDispatch(why, sizeof(why));
    }
    const uint8_t *const pixels = n2.counted ? MandelDraw(kind, why, sizeof(why)) : NULL;
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
            if (kind == MESH_PUSHED && memcmp(want, MANDEL_CLEAR, 4) != 0) {
                want[1] = PUSHED_GREEN_BYTE;
                want[2] = PUSHED_BLUE_BYTE;
            }
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
    static const char *const SCALE_BUFFERS = "the same dispatch, its storage buffer bound by "
                                             "the call that takes several";
    static const char *const SCALE_DEFERRED = "the same dispatch, its storage buffer bound by "
                                              "the deferred call";
    static const char *const SCALE_INDIRECT = "the same dispatch, its counts of blocks read "
                                              "by the graphics unit from a buffer";
    static const char *const PICTURE_3D = "the same picture by a mesh draw of a grid of "
                                          "workgroups";
    static const char *const PICTURE_INDIRECT = "the same picture by a mesh draw that reads "
                                                "its numbers";
    static const char *const PICTURE_3D_INDIRECT = "the same picture by a mesh draw of a grid "
                                                   "that reads its numbers";
    static const char *const PICTURE_MULTI = "the same picture by two mesh draws read from "
                                             "one buffer, a half of the rows each";
    static const char *const PICTURE_MULTI_3D = "the same picture by the mesh draws of a grid "
                                                "read from one buffer";
    static const char *const PICTURE_MULTI_COUNT = "the same picture by as many mesh draws as "
                                                   "a count in a buffer says, a half each";
    static const char *const PICTURE_MULTI_3D_COUNT = "the same picture by as many mesh draws "
                                                      "of a grid as a count in a buffer says";
    static const char *const PUSHED = "a dispatch multiplies and adds by the two words pushed "
                                      "into a root table, the second pushed again alone";
    static const char *const PICTURE_PUSHED = "the same bars with the green and blue a fragment "
                                              "program reads from a root table pushed";
    static const char *const PICTURE_NO_RATE = "the same picture with no texture bound as the "
                                               "shading rate image";
    static const char *const IMAGE = "a dispatch loads each texel of one image and stores three "
                                     "times it plus its column into another";
    static const char *const IMAGE_SEVERAL = "the same images bound by the call that takes "
                                             "several";
    static const char *const IMAGE_DEFERRED = "the same images bound by the deferred call";
    static const char *const APART = "a dispatch samples a texture bound apart from its "
                                     "sampler, slot 1 of each, past the texture's edge";
    static const char *const APART_SEVERAL = "the same texture and sampler bound by the calls "
                                             "that take several";
    static const char *const APART_DEFERRED = "the same texture and sampler bound by the "
                                              "deferred calls";
    const char *const AFTER[] = { PROGRAM,      SCALE,          SCALE_COPIED, SCALE_BUFFERS,
                                  SCALE_DEFERRED, SCALE_INDIRECT, MANDEL_PROGRAMS,
                                  MANDEL_COUNT, MANDEL_PICTURE, PICTURE_3D, PICTURE_INDIRECT,
                                  PICTURE_3D_INDIRECT, PICTURE_MULTI, PICTURE_MULTI_3D,
                                  PICTURE_MULTI_COUNT, PICTURE_MULTI_3D_COUNT, HMMA, PUSHED,
                                  PICTURE_PUSHED, PICTURE_NO_RATE, IMAGE, IMAGE_SEVERAL,
                                  IMAGE_DEFERRED, APART, APART_SEVERAL, APART_DEFERRED };

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
        ReportSkip(SUITE, SCALE_BUFFERS, REPORT_REASON_DEPENDS,
                   "the compute program made no program");
        ReportSkip(SUITE, SCALE_DEFERRED, REPORT_REASON_DEPENDS,
                   "the compute program made no program");
        ReportSkip(SUITE, SCALE_INDIRECT, REPORT_REASON_DEPENDS,
                   "the compute program made no program");
    } else {
        const uint8_t *memory = NULL;
        if (ReportStart(SUITE, SCALE)) {
            NvnLayerClearError();
            memory = Dispatch(DISPATCH_PLAIN, why, sizeof(why));
            ScaleJudge(SCALE, memory != NULL ? memory + SCALE_OUT_BYTE : NULL, why);
        }
        if (ReportStart(SUITE, SCALE_COPIED)) {
            NvnLayerClearError();
            // A launch that goes on after the case above dispatches for this one.
            if (memory == NULL) {
                memory = Dispatch(DISPATCH_PLAIN, why, sizeof(why));
            }
            const NVNbufferAddress out =
                n2.nvn.nvnMemoryPoolGetBufferAddress(&n2.data_pool.pool) + SCALE_OUT_BYTE;
            ScaleJudge(SCALE_COPIED,
                       memory != NULL ? CopyOut(out, 4u * SCALE_WORDS, why, sizeof(why)) : NULL,
                       why);
        }
        // The other ways to the same dispatch, each read as the graphics unit copies it.
        if (ReportStart(SUITE, SCALE_BUFFERS)) {
            ScaleAgain(SCALE_BUFFERS, DISPATCH_BUFFERS);
        }
        if (ReportStart(SUITE, SCALE_DEFERRED)) {
            ScaleAgain(SCALE_DEFERRED, DISPATCH_BUFFERS_DEFERRED);
        }
        if (ReportStart(SUITE, SCALE_INDIRECT)) {
            ScaleAgain(SCALE_INDIRECT, DISPATCH_INDIRECT);
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
        const char *const pictures[] = { PICTURE_3D,          PICTURE_INDIRECT,
                                         PICTURE_3D_INDIRECT, PICTURE_MULTI,
                                         PICTURE_MULTI_3D,    PICTURE_MULTI_COUNT,
                                         PICTURE_MULTI_3D_COUNT };
        for (size_t i = 0; i < sizeof(pictures) / sizeof(pictures[0]); ++i) {
            ReportSkip(SUITE, pictures[i], REPORT_REASON_DEPENDS,
                       "the Mandelbrot's programs made no programs");
        }
    } else {
        // The picture is drawn from the counts the dispatch left, whatever the processor read
        // of them: the two cases fail apart.
        if (ReportStart(SUITE, MANDEL_COUNT)) {
            MandelCountCase(MANDEL_COUNT);
        }
        if (ReportStart(SUITE, MANDEL_PICTURE)) {
            MandelPictureCase(MANDEL_PICTURE, MESH_TASKS);
        }
        if (ReportStart(SUITE, PICTURE_3D)) {
            MandelPictureCase(PICTURE_3D, MESH_TASKS_3D);
        }
        if (ReportStart(SUITE, PICTURE_INDIRECT)) {
            MandelPictureCase(PICTURE_INDIRECT, MESH_INDIRECT);
        }
        if (ReportStart(SUITE, PICTURE_3D_INDIRECT)) {
            MandelPictureCase(PICTURE_3D_INDIRECT, MESH_3D_INDIRECT);
        }
        if (ReportStart(SUITE, PICTURE_MULTI)) {
            MandelPictureCase(PICTURE_MULTI, MESH_MULTI);
        }
        if (ReportStart(SUITE, PICTURE_MULTI_3D)) {
            MandelPictureCase(PICTURE_MULTI_3D, MESH_MULTI_3D);
        }
        if (ReportStart(SUITE, PICTURE_MULTI_COUNT)) {
            MandelPictureCase(PICTURE_MULTI_COUNT, MESH_MULTI_COUNT);
        }
        if (ReportStart(SUITE, PICTURE_MULTI_3D_COUNT)) {
            MandelPictureCase(PICTURE_MULTI_3D_COUNT, MESH_MULTI_3D_COUNT);
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

    // The root tables: what a program reads of them once the list pushed it, in a dispatch and
    // in a draw.
    if (ReportStart(SUITE, PUSHED)) {
        NvnLayerClearError();
        const Stage pushed = { pushed_comp_bin, pushed_comp_bin_size, pushed_comp_ctl };
        if (BuildProgram(PROGRAM_PUSHED, &pushed, 1, why, sizeof(why))) {
            ScaleAgain(PUSHED, DISPATCH_PUSHED);
        } else {
            ReportCase(SUITE, PUSHED, CHECK_NONE, false, why);
        }
    }
    if (!mandel) {
        ReportSkip(SUITE, PICTURE_PUSHED, REPORT_REASON_DEPENDS,
                   "the Mandelbrot's programs made no programs");
        ReportSkip(SUITE, PICTURE_NO_RATE, REPORT_REASON_DEPENDS,
                   "the Mandelbrot's programs made no programs");
    } else {
        if (ReportStart(SUITE, PICTURE_PUSHED)) {
            NvnLayerClearError();
            const Stage draw[2] = {
                { mandel_mesh_bin, mandel_mesh_bin_size, mandel_mesh_ctl },
                { pushed_frag_bin, pushed_frag_bin_size, pushed_frag_ctl },
            };
            if (BuildProgram(PROGRAM_MANDEL_PUSHED, draw, 2, why, sizeof(why))) {
                MandelPictureCase(PICTURE_PUSHED, MESH_PUSHED);
            } else {
                ReportCase(SUITE, PICTURE_PUSHED, CHECK_NONE, false, why);
            }
        }
        if (ReportStart(SUITE, PICTURE_NO_RATE)) {
            MandelPictureCase(PICTURE_NO_RATE, MESH_NO_RATE);
        }
    }

    // Images: textures a program loads from and stores into, each by its handle.
    bool image = false;
    if (ReportStart(SUITE, IMAGE)) {
        NvnLayerClearError();
        const Stage stage = { image_comp_bin, image_comp_bin_size, image_comp_ctl };
        image = BuildProgram(PROGRAM_IMAGE, &stage, 1, why, sizeof(why));
        if (image) {
            ImageCase(IMAGE, IMAGES_ONE_BY_ONE);
        } else {
            ReportCase(SUITE, IMAGE, CHECK_NONE, false, why);
        }
    }
    if (!image) {
        ReportSkip(SUITE, IMAGE_SEVERAL, REPORT_REASON_DEPENDS,
                   "the image program made no program");
        ReportSkip(SUITE, IMAGE_DEFERRED, REPORT_REASON_DEPENDS,
                   "the image program made no program");
    } else {
        if (ReportStart(SUITE, IMAGE_SEVERAL)) {
            ImageCase(IMAGE_SEVERAL, IMAGES_SEVERAL);
        }
        if (ReportStart(SUITE, IMAGE_DEFERRED)) {
            ImageCase(IMAGE_DEFERRED, IMAGES_DEFERRED);
        }
    }

    // A texture and a sampler bound each by itself, which a program puts together.
    bool apart = false;
    if (ReportStart(SUITE, APART)) {
        NvnLayerClearError();
        const Stage stage = { apart_comp_bin, apart_comp_bin_size, apart_comp_ctl };
        apart = BuildProgram(PROGRAM_APART, &stage, 1, why, sizeof(why));
        if (apart) {
            ApartCase(APART, IMAGES_ONE_BY_ONE);
        } else {
            ReportCase(SUITE, APART, CHECK_NONE, false, why);
        }
    }
    if (!apart) {
        ReportSkip(SUITE, APART_SEVERAL, REPORT_REASON_DEPENDS,
                   "the sampling program made no program");
        ReportSkip(SUITE, APART_DEFERRED, REPORT_REASON_DEPENDS,
                   "the sampling program made no program");
    } else {
        if (ReportStart(SUITE, APART_SEVERAL)) {
            ApartCase(APART_SEVERAL, IMAGES_SEVERAL);
        }
        if (ReportStart(SUITE, APART_DEFERRED)) {
            ApartCase(APART_DEFERRED, IMAGES_DEFERRED);
        }
    }
    TearDown();
}
