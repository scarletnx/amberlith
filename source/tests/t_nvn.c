// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <switch.h>

#include <nvnlayer.h>

#include "../check.h"
#include "../picture.h"
#include "../report.h"
#include "tests.h"

// What the suite takes from the console's frontend, which the Switch 2's has none of: deko3d's
// abort turned into a return, the program's screen that may hold the window, and the scene's
// deko3d way.
#if NVN_LAYER_API < 231
#include "../gpu.h"
#include "../viewer.h"
#endif

// The nvn family: the clear suite's question asked through NVN, as a game asks it, with the
// layer of nvnlayer/ as the driver. On the console and in Eden the layer records deko3d
// commands and the graphics unit answers; under ScarletNX, ScarletNX's own NVN takes every
// function the case looks up and answers instead. Everything goes through the functions the
// lookup handed back, never through the layer's code by name, so the case is the same program on
// every machine and only the NVN under it changes.
//
// Exact tier for the pixels, as in the clear suite: a normalised target, constant clear values,
// no shader. The bring-up and the window are the none tier: they compare nothing, a failure says
// which step stopped and why.

static const char *const SUITE = "nvn";

#define TARGET_SIZE 64u
#define TARGET_BYTES (TARGET_SIZE * TARGET_SIZE * 4u)
// Command memory for every list the suite records; NVN, like deko3d, takes it in a pool that is
// uncached to the processor and cached to the graphics unit.
#define COMMAND_BYTES 0x10000u
// What the processor-side control memory NVN asks of a command buffer comes to here: a page.
#define CONTROL_BYTES 0x1000u
// The window's textures: the console's own screen size, two of them, NVN's least.
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define WINDOW_TEXTURES 2
// A pool's storage is a whole number of these, on a boundary of one: NVN's page and deko3d's.
#define POOL_PAGE 0x1000u
// What the readback holds until the copy writes it: no clear colour of this suite is it.
#define READ_PRESET 0xa5u
/// The row stride the strided copy case sets: a 64 texel row's 256 bytes and 32 more.
#define STRIDE 288u
/// The readback pool holds the strided copy's rows.
#define READ_POOL_BYTES ((STRIDE * TARGET_SIZE + POOL_PAGE - 1u) / POOL_PAGE * POOL_PAGE)

// Everything NVN's objects need, in one place that outlives a case: NVN objects are large (the
// device alone is 12 KiB) and must sit on an eight byte boundary, which nvn/nvn.h does not ask
// the compiler for.
static struct {
    NvnProcs nvn;
    bool device_up;
    bool queue_up;
    bool command_up;
    bool target_up;
    _Alignas(8) NVNdevice device;
    _Alignas(8) NVNqueue queue;
    _Alignas(8) NVNmemoryPool command_pool;
    _Alignas(8) NVNmemoryPool target_pool;
    _Alignas(8) NVNmemoryPool read_pool;
    _Alignas(8) NVNcommandBuffer command;
    _Alignas(8) NVNtexture target;
    _Alignas(8) NVNmemoryPool copy_pool;
    _Alignas(8) NVNmemoryPool stage_pool;
    _Alignas(8) NVNtexture copy_a;
    _Alignas(8) NVNtexture copy_b;
    bool copy_pool_up;
    bool stage_pool_up;
    bool copy_a_up;
    bool copy_b_up;
    void *copy_storage;
    void *stage_storage;
    bool command_pool_up;
    bool target_pool_up;
    bool read_pool_up;
    void *command_storage;
    void *target_storage;
    void *read_storage;
    void *control;
    _Alignas(8) NVNmemoryPool levels_pool;
    _Alignas(8) NVNmemoryPool levels_read_pool;
    _Alignas(8) NVNtexture levels;
    bool levels_pool_up;
    bool levels_read_pool_up;
    bool levels_up;
    void *levels_storage;
    void *levels_read_storage;
    ptrdiff_t levels_offset;
    _Alignas(8) NVNmemoryPool cached_pool;
    bool cached_pool_up;
    void *cached_storage;
} nvn;

/// The reason a step failed: the layer's, when it gave one, else @p fallback. Under ScarletNX the
/// layer never runs, so the fallback is all a failure there carries.
static const char *Why(const char *fallback) {
    return NvnLayerError()[0] != '\0' ? NvnLayerError() : fallback;
}

static size_t RoundUp(size_t value, size_t to) {
    return (value + to - 1u) / to * to;
}

/// A pool of @p size bytes (a whole number of pages) with @p flags, over storage of its own.
static bool MakePool(NVNmemoryPool *pool, void **storage, size_t size, int flags) {
    *storage = aligned_alloc(POOL_PAGE, size);
    if (*storage == NULL) {
        return false;
    }
    memset(*storage, 0, size);
    _Alignas(8) NVNmemoryPoolBuilder builder;
    nvn.nvn.nvnMemoryPoolBuilderSetDefaults(&builder);
    nvn.nvn.nvnMemoryPoolBuilderSetDevice(&builder, &nvn.device);
    nvn.nvn.nvnMemoryPoolBuilderSetStorage(&builder, *storage, size);
    nvn.nvn.nvnMemoryPoolBuilderSetFlags(&builder, flags);
    if (!nvn.nvn.nvnMemoryPoolInitialize(pool, &builder)) {
        free(*storage);
        *storage = NULL;
        return false;
    }
    return true;
}

static void DropPool(NVNmemoryPool *pool, void **storage, bool *up) {
    if (*up) {
        nvn.nvn.nvnMemoryPoolFinalize(pool);
        *up = false;
    }
    free(*storage);
    *storage = NULL;
}

/// A builder for a 2D RGBA8 texture of @p width by @p height with @p flags, on the device.
static void TextureBuilder(NVNtextureBuilder *builder, int width, int height, int flags) {
    nvn.nvn.nvnTextureBuilderSetDevice(builder, &nvn.device);
    nvn.nvn.nvnTextureBuilderSetDefaults(builder);
    nvn.nvn.nvnTextureBuilderSetFlags(builder, flags);
    nvn.nvn.nvnTextureBuilderSetTarget(builder, NVN_TEXTURE_TARGET_2D);
    nvn.nvn.nvnTextureBuilderSetFormat(builder, NVN_FORMAT_RGBA8);
    nvn.nvn.nvnTextureBuilderSetSize2D(builder, width, height);
}

/// The queue, the pools, the command buffer and the 64 by 64 target, in that order; false with
/// the step in @p why.
static bool BringUp(char *why, size_t why_size) {
    _Alignas(8) NVNqueueBuilder queue_builder;
    nvn.nvn.nvnQueueBuilderSetDevice(&queue_builder, &nvn.device);
    nvn.nvn.nvnQueueBuilderSetDefaults(&queue_builder);
    if (!(nvn.queue_up = nvn.nvn.nvnQueueInitialize(&nvn.queue, &queue_builder))) {
        snprintf(why, why_size, "nvnQueueInitialize: %s", Why("false"));
        return false;
    }
    nvn.command_pool_up =
        MakePool(&nvn.command_pool, &nvn.command_storage, COMMAND_BYTES,
                 NVN_MEMORY_POOL_FLAGS_CPU_UNCACHED | NVN_MEMORY_POOL_FLAGS_GPU_CACHED);
    if (!nvn.command_pool_up) {
        snprintf(why, why_size, "the command pool: %s", Why("no memory, or false"));
        return false;
    }
    nvn.read_pool_up =
        MakePool(&nvn.read_pool, &nvn.read_storage, READ_POOL_BYTES,
                 NVN_MEMORY_POOL_FLAGS_CPU_UNCACHED | NVN_MEMORY_POOL_FLAGS_GPU_CACHED);
    if (!nvn.read_pool_up) {
        snprintf(why, why_size, "the readback pool: %s", Why("no memory, or false"));
        return false;
    }

    _Alignas(8) NVNtextureBuilder texture_builder;
    memset(&texture_builder, 0, sizeof(texture_builder));
    TextureBuilder(&texture_builder, TARGET_SIZE, TARGET_SIZE, 0);
    const size_t size = nvn.nvn.nvnTextureBuilderGetStorageSize(&texture_builder);
    const size_t alignment = nvn.nvn.nvnTextureBuilderGetStorageAlignment(&texture_builder);
    if (size == 0u || alignment == 0u || alignment > POOL_PAGE) {
        snprintf(why, why_size, "the target's storage is %zu bytes at an alignment of %zu: %s",
                 size, alignment, Why("an answer no pool here can hold"));
        return false;
    }
    nvn.target_pool_up =
        MakePool(&nvn.target_pool, &nvn.target_storage, RoundUp(size, POOL_PAGE),
                 NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS | NVN_MEMORY_POOL_FLAGS_GPU_CACHED);
    if (!nvn.target_pool_up) {
        snprintf(why, why_size, "the target's pool: %s", Why("no memory, or false"));
        return false;
    }
    nvn.nvn.nvnTextureBuilderSetStorage(&texture_builder, &nvn.target_pool, 0);
    if (!(nvn.target_up = nvn.nvn.nvnTextureInitialize(&nvn.target, &texture_builder))) {
        snprintf(why, why_size, "nvnTextureInitialize: %s", Why("false"));
        return false;
    }

    if (!(nvn.command_up = nvn.nvn.nvnCommandBufferInitialize(&nvn.command, &nvn.device))) {
        snprintf(why, why_size, "nvnCommandBufferInitialize: %s", Why("false"));
        return false;
    }
    nvn.control = aligned_alloc(8u, CONTROL_BYTES);
    if (nvn.control == NULL) {
        snprintf(why, why_size, "no memory for the command buffer's control memory");
        return false;
    }
    nvn.nvn.nvnCommandBufferAddCommandMemory(&nvn.command, &nvn.command_pool, 0, COMMAND_BYTES);
    nvn.nvn.nvnCommandBufferAddControlMemory(&nvn.command, nvn.control, CONTROL_BYTES);
    return true;
}

static void TearDown(void) {
    if (nvn.queue_up) {
        nvn.nvn.nvnQueueFinish(&nvn.queue);
    }
    if (nvn.copy_a_up) {
        nvn.nvn.nvnTextureFinalize(&nvn.copy_a);
        nvn.copy_a_up = false;
    }
    if (nvn.copy_b_up) {
        nvn.nvn.nvnTextureFinalize(&nvn.copy_b);
        nvn.copy_b_up = false;
    }
    DropPool(&nvn.copy_pool, &nvn.copy_storage, &nvn.copy_pool_up);
    DropPool(&nvn.stage_pool, &nvn.stage_storage, &nvn.stage_pool_up);
    if (nvn.levels_up) {
        nvn.nvn.nvnTextureFinalize(&nvn.levels);
        nvn.levels_up = false;
    }
    DropPool(&nvn.levels_pool, &nvn.levels_storage, &nvn.levels_pool_up);
    DropPool(&nvn.levels_read_pool, &nvn.levels_read_storage, &nvn.levels_read_pool_up);
    DropPool(&nvn.cached_pool, &nvn.cached_storage, &nvn.cached_pool_up);
    if (nvn.command_up) {
        nvn.nvn.nvnCommandBufferFinalize(&nvn.command);
        nvn.command_up = false;
    }
    free(nvn.control);
    nvn.control = NULL;
    if (nvn.target_up) {
        nvn.nvn.nvnTextureFinalize(&nvn.target);
        nvn.target_up = false;
    }
    DropPool(&nvn.target_pool, &nvn.target_storage, &nvn.target_pool_up);
    DropPool(&nvn.read_pool, &nvn.read_storage, &nvn.read_pool_up);
    DropPool(&nvn.command_pool, &nvn.command_storage, &nvn.command_pool_up);
    if (nvn.queue_up) {
        nvn.nvn.nvnQueueFinalize(&nvn.queue);
        nvn.queue_up = false;
    }
    if (nvn.device_up) {
        nvn.nvn.nvnDeviceFinalize(&nvn.device);
        nvn.device_up = false;
    }
}

/// Records into the open command buffer: @p texture as the only colour target, with a viewport
/// and a scissor over all @p width by @p height of it, cleared to @p rgba.
static void RecordClear(const NVNtexture *texture, int width, int height, const float rgba[4]) {
    const NVNtexture *const colors[1] = { texture };
    nvn.nvn.nvnCommandBufferSetRenderTargets(&nvn.command, 1, colors, NULL, NULL, NULL);
    nvn.nvn.nvnCommandBufferSetViewport(&nvn.command, 0, 0, width, height);
    nvn.nvn.nvnCommandBufferSetScissor(&nvn.command, 0, 0, width, height);
    nvn.nvn.nvnCommandBufferClearColor(&nvn.command, 0, rgba, NVN_CLEAR_COLOR_MASK_RGBA);
}

/// Clears the target to @p rgba, copies it to the readback pool and waits: the pool's bytes, or
/// NULL with the reason in @p why.
static const uint8_t *ClearAndRead(const float rgba[4], char *why, size_t why_size) {
    uint8_t *const pixels = nvn.nvn.nvnMemoryPoolMap(&nvn.read_pool);
    if (pixels == NULL) {
        snprintf(why, why_size, "nvnMemoryPoolMap gave no address for the readback pool: %s",
                 Why("NULL"));
        return NULL;
    }
    // A copy that never lands leaves these, which no clear colour here produces.
    memset(pixels, READ_PRESET, TARGET_BYTES);

    nvn.nvn.nvnCommandBufferBeginRecording(&nvn.command);
    RecordClear(&nvn.target, TARGET_SIZE, TARGET_SIZE, rgba);
    // The clear ran on the graphics engine and the copy reads its result: an NVN program orders
    // the two, and asks for the texture cache to be dropped too.
    nvn.nvn.nvnCommandBufferBarrier(&nvn.command, NVN_BARRIER_ORDER_FRAGMENTS_BIT |
                                                      NVN_BARRIER_INVALIDATE_TEXTURE_BIT);
    const NVNcopyRegion region = { 0, 0, 0, TARGET_SIZE, TARGET_SIZE, 1 };
    nvn.nvn.nvnCommandBufferCopyTextureToBuffer(&nvn.command, &nvn.target, NULL, &region,
                                                nvn.nvn.nvnMemoryPoolGetBufferAddress(
                                                    &nvn.read_pool),
                                                NVN_COPY_FLAGS_NONE);
    const NVNcommandHandle list = nvn.nvn.nvnCommandBufferEndRecording(&nvn.command);
    if (list == 0u) {
        snprintf(why, why_size, "nvnCommandBufferEndRecording gave no list: %s", Why("0"));
        return NULL;
    }
    nvn.nvn.nvnQueueSubmitCommands(&nvn.queue, 1, &list);
    nvn.nvn.nvnQueueFinish(&nvn.queue);
    if (NvnLayerError()[0] != '\0') {
        snprintf(why, why_size, "%s", NvnLayerError());
        return NULL;
    }
    return pixels;
}

/// One case: the target cleared to @p rgba must read back as @p expected in every pixel.
static void ClearCase(const char *name, const float rgba[4], const uint8_t expected[4]) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256];
    const uint8_t *const pixels = ClearAndRead(rgba, why, sizeof(why));
    if (pixels == NULL) {
        ReportCase(SUITE, name, CHECK_NONE, false, why);
        return;
    }
    Picture *const picture = PictureBegin(TARGET_SIZE, TARGET_SIZE, PICTURE_COLOUR, pixels);
    uint32_t wrong = 0;
    uint32_t first_x = 0;
    uint32_t first_y = 0;
    uint8_t first[4] = { 0, 0, 0, 0 };
    for (uint32_t y = 0; y < TARGET_SIZE; ++y) {
        for (uint32_t x = 0; x < TARGET_SIZE; ++x) {
            const uint8_t *const pixel = pixels + ((size_t)y * TARGET_SIZE + x) * 4u;
            const bool right = memcmp(pixel, expected, 4) == 0;
            PictureSet(picture, x, y, expected, right ? PICTURE_MATCHED : PICTURE_WRONG);
            if (!right && wrong++ == 0u) {
                first_x = x;
                first_y = y;
                memcpy(first, pixel, sizeof(first));
            }
        }
    }
    CHECK_TRUE(SUITE, name, CHECK_EXACT, wrong == 0u,
               "%u of %u pixels wrong, first at (%u,%u) is %02x%02x%02x%02x, expected "
               "%02x%02x%02x%02x",
               (unsigned)wrong, (unsigned)(TARGET_SIZE * TARGET_SIZE), (unsigned)first_x,
               (unsigned)first_y, first[0], first[1], first[2], first[3], expected[0],
               expected[1], expected[2], expected[3]);
}

/// Two textures on the default window, each acquired, cleared and presented once. Nothing is read
/// back: what reaches the screen is the compositor's, and the case says only whether every step
/// came back as NVN promises. False with the step in @p why.
static bool Present(char *why, size_t why_size) {
    _Alignas(8) NVNtextureBuilder builder;
    memset(&builder, 0, sizeof(builder));
    TextureBuilder(&builder, WINDOW_WIDTH, WINDOW_HEIGHT, NVN_TEXTURE_FLAGS_DISPLAY);
    const size_t size = nvn.nvn.nvnTextureBuilderGetStorageSize(&builder);
    const size_t alignment = nvn.nvn.nvnTextureBuilderGetStorageAlignment(&builder);
    if (size == 0u || alignment == 0u || alignment > POOL_PAGE) {
        snprintf(why, why_size, "a window texture's storage is %zu bytes at an alignment of %zu: "
                 "%s",
                 size, alignment, Why("an answer no pool here can hold"));
        return false;
    }
    const size_t stride = RoundUp(size, alignment);
    static _Alignas(8) NVNmemoryPool pool;
    static _Alignas(8) NVNtexture textures[WINDOW_TEXTURES];
    static _Alignas(8) NVNwindow window;
    void *storage = NULL;
    bool pool_up = MakePool(&pool, &storage, RoundUp(stride * WINDOW_TEXTURES, POOL_PAGE),
                            NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS |
                                NVN_MEMORY_POOL_FLAGS_GPU_CACHED);
    if (!pool_up) {
        snprintf(why, why_size, "the window's pool: %s", Why("no memory, or false"));
        return false;
    }
    bool ok = true;
    int made = 0;
    for (; made < WINDOW_TEXTURES; ++made) {
        nvn.nvn.nvnTextureBuilderSetStorage(&builder, &pool, (ptrdiff_t)(stride * (size_t)made));
        if (!nvn.nvn.nvnTextureInitialize(&textures[made], &builder)) {
            snprintf(why, why_size, "window texture %d: nvnTextureInitialize: %s", made,
                     Why("false"));
            ok = false;
            break;
        }
    }
    bool window_up = false;
    if (ok) {
        NVNtexture *const list[WINDOW_TEXTURES] = { &textures[0], &textures[1] };
        _Alignas(8) NVNwindowBuilder window_builder;
        nvn.nvn.nvnWindowBuilderSetDefaults(&window_builder);
        nvn.nvn.nvnWindowBuilderSetDevice(&window_builder, &nvn.device);
        nvn.nvn.nvnWindowBuilderSetNativeWindow(&window_builder, nwindowGetDefault());
        nvn.nvn.nvnWindowBuilderSetTextures(&window_builder, WINDOW_TEXTURES, list);
        window_up = ok = nvn.nvn.nvnWindowInitialize(&window, &window_builder);
        if (!ok) {
            snprintf(why, why_size, "nvnWindowInitialize: %s", Why("false"));
        }
    }
    // Two frames, so that both textures go round once: a cleared green, then a cleared blue.
    static const float colours[WINDOW_TEXTURES][4] = { { 0.0f, 1.0f, 0.0f, 1.0f },
                                                       { 0.0f, 0.0f, 1.0f, 1.0f } };
    for (int frame = 0; ok && frame < WINDOW_TEXTURES; ++frame) {
        int index = -1;
        const NVNqueueAcquireTextureResult got =
            nvn.nvn.nvnQueueAcquireTexture(&nvn.queue, &window, &index);
        if (got != NVN_QUEUE_ACQUIRE_TEXTURE_RESULT_SUCCESS || index < 0 ||
            index >= WINDOW_TEXTURES) {
            snprintf(why, why_size, "frame %d: nvnQueueAcquireTexture answered %d with texture "
                     "%d of %d: %s",
                     frame, (int)got, index, WINDOW_TEXTURES, Why("no reason given"));
            ok = false;
            break;
        }
        nvn.nvn.nvnCommandBufferBeginRecording(&nvn.command);
        RecordClear(&textures[index], WINDOW_WIDTH, WINDOW_HEIGHT, colours[frame]);
        const NVNcommandHandle list = nvn.nvn.nvnCommandBufferEndRecording(&nvn.command);
        nvn.nvn.nvnQueueSubmitCommands(&nvn.queue, 1, &list);
        nvn.nvn.nvnQueuePresentTexture(&nvn.queue, &window, index);
    }
    nvn.nvn.nvnQueueFinish(&nvn.queue);
    if (ok && NvnLayerError()[0] != '\0') {
        snprintf(why, why_size, "%s", NvnLayerError());
        ok = false;
    }
    if (window_up) {
        nvn.nvn.nvnWindowFinalize(&window);
    }
    for (int i = 0; i < made; ++i) {
        nvn.nvn.nvnTextureFinalize(&textures[i]);
    }
    DropPool(&pool, &storage, &pool_up);
    return ok;
}

// ---- copies

// The copies a game's NVN makes between textures and buffers, each through the layer's deko3d
// copies on the console and in Eden, and through ScarletNX's own NVN copies there: a picture of
// 64x64 texels, every texel four bytes of its own, staged in a processor-visible pool, put in a
// texture and read back through the readback pool, which is first filled with a byte no texel
// holds. Every byte is judged, the strided copy's padding included.

/// Texel (x, y)'s four bytes: red and green name the place, blue mixes them, alpha is never zero.
static void Pattern(uint32_t x, uint32_t y, uint8_t out[4]) {
    out[0] = (uint8_t)(x * 4u + 1u);
    out[1] = (uint8_t)(y * 4u + 3u);
    out[2] = (uint8_t)(x ^ (y * 7u));
    out[3] = (uint8_t)(0x80u | ((x + y) & 0x7fu));
}

/// The two copy textures in a pool of their own and the staging pool, the picture staged in its
/// first half; false with the step in @p why.
static bool BringCopies(char *why, size_t why_size) {
    _Alignas(8) NVNtextureBuilder builder;
    memset(&builder, 0, sizeof(builder));
    TextureBuilder(&builder, TARGET_SIZE, TARGET_SIZE, 0);
    const size_t size = RoundUp(nvn.nvn.nvnTextureBuilderGetStorageSize(&builder), POOL_PAGE);
    nvn.copy_pool_up =
        size != 0u && MakePool(&nvn.copy_pool, &nvn.copy_storage, 2u * size,
                               NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS | NVN_MEMORY_POOL_FLAGS_GPU_CACHED);
    nvn.stage_pool_up =
        MakePool(&nvn.stage_pool, &nvn.stage_storage, 2u * TARGET_BYTES,
                 NVN_MEMORY_POOL_FLAGS_CPU_UNCACHED | NVN_MEMORY_POOL_FLAGS_GPU_CACHED);
    if (!nvn.copy_pool_up || !nvn.stage_pool_up) {
        snprintf(why, why_size, "the copy pools: %s", Why("no memory, or false"));
        return false;
    }
    nvn.nvn.nvnTextureBuilderSetStorage(&builder, &nvn.copy_pool, 0);
    nvn.copy_a_up = nvn.nvn.nvnTextureInitialize(&nvn.copy_a, &builder);
    nvn.nvn.nvnTextureBuilderSetStorage(&builder, &nvn.copy_pool, (ptrdiff_t)size);
    nvn.copy_b_up = nvn.nvn.nvnTextureInitialize(&nvn.copy_b, &builder);
    if (!nvn.copy_a_up || !nvn.copy_b_up) {
        snprintf(why, why_size, "nvnTextureInitialize of the copy textures: %s", Why("false"));
        return false;
    }
    uint8_t *const stage = nvn.nvn.nvnMemoryPoolMap(&nvn.stage_pool);
    if (stage == NULL) {
        snprintf(why, why_size, "nvnMemoryPoolMap gave no address for the staging pool: %s",
                 Why("NULL"));
        return false;
    }
    for (uint32_t y = 0; y < TARGET_SIZE; ++y) {
        for (uint32_t x = 0; x < TARGET_SIZE; ++x) {
            Pattern(x, y, stage + ((size_t)y * TARGET_SIZE + x) * 4u);
        }
    }
    memset(stage + TARGET_BYTES, READ_PRESET, TARGET_BYTES);
    return true;
}

typedef enum {
    COPY_ROUND_TRIP,
    COPY_TEXTURE,
    COPY_BUFFER,
    COPY_MIRROR_Y,
    COPY_MIRROR_X,
    COPY_STRIDE,
} CopyKind;

/// Records @p kind's copies, runs them and hands back the bytes it reads back, or NULL with the
/// reason in @p why.
static const uint8_t *RunCopies(CopyKind kind, char *why, size_t why_size) {
    uint8_t *const read = nvn.nvn.nvnMemoryPoolMap(&nvn.read_pool);
    uint8_t *const stage = nvn.nvn.nvnMemoryPoolMap(&nvn.stage_pool);
    if (read == NULL || stage == NULL) {
        snprintf(why, why_size, "nvnMemoryPoolMap gave no address: %s", Why("NULL"));
        return NULL;
    }
    memset(read, READ_PRESET, READ_POOL_BYTES);
    memset(stage + TARGET_BYTES, READ_PRESET, TARGET_BYTES);
    const NVNbufferAddress picture = nvn.nvn.nvnMemoryPoolGetBufferAddress(&nvn.stage_pool);
    const NVNbufferAddress readback = nvn.nvn.nvnMemoryPoolGetBufferAddress(&nvn.read_pool);
    const NVNcopyRegion whole = { 0, 0, 0, TARGET_SIZE, TARGET_SIZE, 1 };
    const int order = NVN_BARRIER_ORDER_PRIMITIVES_BIT | NVN_BARRIER_INVALIDATE_TEXTURE_BIT;
    NVNcommandBuffer *const cmd = &nvn.command;
    nvn.nvn.nvnCommandBufferBeginRecording(cmd);
    nvn.nvn.nvnCommandBufferSetCopyRowStride(cmd, 0);
    if (kind == COPY_BUFFER) {
        nvn.nvn.nvnCommandBufferCopyBufferToBuffer(cmd, picture, readback, TARGET_BYTES,
                                                   NVN_COPY_FLAGS_NONE);
    } else {
        nvn.nvn.nvnCommandBufferCopyBufferToTexture(cmd, picture, &nvn.copy_a, NULL, &whole,
                                                    NVN_COPY_FLAGS_NONE);
        nvn.nvn.nvnCommandBufferBarrier(cmd, order);
        const NVNtexture *read_from = &nvn.copy_a;
        int read_flags = NVN_COPY_FLAGS_NONE;
        if (kind == COPY_TEXTURE || kind == COPY_MIRROR_X) {
            nvn.nvn.nvnCommandBufferCopyTextureToTexture(
                cmd, &nvn.copy_a, NULL, &whole, &nvn.copy_b, NULL, &whole,
                kind == COPY_MIRROR_X ? NVN_COPY_FLAGS_MIRROR_X_BIT : NVN_COPY_FLAGS_NONE);
            nvn.nvn.nvnCommandBufferBarrier(cmd, order);
            read_from = &nvn.copy_b;
        }
        if (kind == COPY_MIRROR_Y) {
            read_flags = NVN_COPY_FLAGS_MIRROR_Y_BIT;
        }
        if (kind == COPY_STRIDE) {
            nvn.nvn.nvnCommandBufferSetCopyRowStride(cmd, (ptrdiff_t)STRIDE);
        }
        nvn.nvn.nvnCommandBufferCopyTextureToBuffer(cmd, read_from, NULL, &whole, readback,
                                                    read_flags);
        nvn.nvn.nvnCommandBufferSetCopyRowStride(cmd, 0);
    }
    const NVNcommandHandle list = nvn.nvn.nvnCommandBufferEndRecording(cmd);
    if (list == 0u) {
        snprintf(why, why_size, "nvnCommandBufferEndRecording gave no list: %s", Why("0"));
        return NULL;
    }
    nvn.nvn.nvnQueueSubmitCommands(&nvn.queue, 1, &list);
    nvn.nvn.nvnQueueFinish(&nvn.queue);
    if (NvnLayerError()[0] != '\0') {
        snprintf(why, why_size, "%s", NvnLayerError());
        return NULL;
    }
    return read;
}

/// One copy case: what @p kind reads back must be the picture as the copy moves it, every byte
/// past it the preset.
static void CopyCase(const char *name, CopyKind kind) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256];
    const uint8_t *const got = RunCopies(kind, why, sizeof(why));
    if (got == NULL) {
        ReportCase(SUITE, name, CHECK_NONE, false, why);
        return;
    }
    const uint32_t row = kind == COPY_STRIDE ? STRIDE : TARGET_SIZE * 4u;
    uint32_t wrong = 0, first = 0;
    uint8_t want_first = 0;
    for (uint32_t at = 0; at < READ_POOL_BYTES; ++at) {
        const uint32_t y = at / row, in_row = at % row;
        uint8_t want = READ_PRESET;
        if (y < TARGET_SIZE && in_row < TARGET_SIZE * 4u) {
            uint32_t x = in_row / 4u, ty = y;
            if (kind == COPY_MIRROR_X) {
                x = TARGET_SIZE - 1u - x;
            }
            if (kind == COPY_MIRROR_Y) {
                ty = TARGET_SIZE - 1u - y;
            }
            uint8_t texel[4];
            Pattern(x, ty, texel);
            want = texel[in_row % 4u];
        }
        if (got[at] != want && wrong++ == 0u) {
            first = at;
            want_first = want;
        }
    }
    CHECK_TRUE(SUITE, name, CHECK_EXACT, wrong == 0u,
               "%u of %u bytes wrong; the first, byte %u (row %u, byte %u of it), is 0x%02x, "
               "expected 0x%02x (0x%02x is a byte nothing copied)",
               (unsigned)wrong, (unsigned)READ_POOL_BYTES, (unsigned)first, (unsigned)(first / row),
               (unsigned)(first % row), got[first], want_first, READ_PRESET);
}

// ---- what a texture, its builder and a view answer of themselves, and a texture's levels

// A game asks its textures what they are (a size to make a view or a copy region from, the pool
// to free) and addresses the levels of one through views. The getters are held to what the
// setters took, which is all NVN says of them; the levels to a picture of its own in each, every
// byte read back through the level's view.

/// The levels texture: 64x64 with every level down to 16x16.
#define LEVELS 3
/// The bytes of all three levels' texels, one after another: where the case stages them and
/// where it reads them back.
#define LEVELS_BYTES (TARGET_BYTES + TARGET_BYTES / 4u + TARGET_BYTES / 16u)
#define LEVELS_READ_BYTES ((LEVELS_BYTES + POOL_PAGE - 1u) / POOL_PAGE * POOL_PAGE)

/// Keeps in @p why the first answer that is not the number given: true when @p got is @p want.
static bool Gives(char *why, size_t why_size, const char *what, long long got, long long want) {
    if (got != want && why[0] == '\0') {
        snprintf(why, why_size, "%s is %lld (0x%llx), where %lld (0x%llx) was set", what, got,
                 (unsigned long long)got, want, (unsigned long long)want);
    }
    return got == want;
}

/// Every setter of a builder that keeps a number, each with a number that is not the default,
/// and what the getters then say. The builder makes no texture, so the numbers need not be a
/// texture any machine has.
static void TextureBuilderCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    _Alignas(8) NVNtextureBuilder builder;
    memset(&builder, 0, sizeof(builder));
    nvn.nvn.nvnTextureBuilderSetDevice(&builder, &nvn.device);
    nvn.nvn.nvnTextureBuilderSetDefaults(&builder);
    nvn.nvn.nvnTextureBuilderSetFlags(&builder, NVN_TEXTURE_FLAGS_COMPRESSIBLE);
    nvn.nvn.nvnTextureBuilderSetTarget(&builder, NVN_TEXTURE_TARGET_2D_ARRAY);
    nvn.nvn.nvnTextureBuilderSetFormat(&builder, NVN_FORMAT_BGRA8);
    nvn.nvn.nvnTextureBuilderSetWidth(&builder, 48);
    nvn.nvn.nvnTextureBuilderSetHeight(&builder, 24);
    nvn.nvn.nvnTextureBuilderSetDepth(&builder, 5);
    nvn.nvn.nvnTextureBuilderSetLevels(&builder, 3);
    nvn.nvn.nvnTextureBuilderSetSamples(&builder, 4);
    nvn.nvn.nvnTextureBuilderSetStorage(&builder, &nvn.target_pool, 0x3000);
    Gives(why, sizeof(why), "the builder's device",
          (long long)(uintptr_t)nvn.nvn.nvnTextureBuilderGetDevice(&builder),
          (long long)(uintptr_t)&nvn.device);
    Gives(why, sizeof(why), "the builder's flags", nvn.nvn.nvnTextureBuilderGetFlags(&builder),
          NVN_TEXTURE_FLAGS_COMPRESSIBLE);
    Gives(why, sizeof(why), "the builder's target", nvn.nvn.nvnTextureBuilderGetTarget(&builder),
          NVN_TEXTURE_TARGET_2D_ARRAY);
    Gives(why, sizeof(why), "the builder's format", nvn.nvn.nvnTextureBuilderGetFormat(&builder),
          NVN_FORMAT_BGRA8);
    Gives(why, sizeof(why), "the builder's width", nvn.nvn.nvnTextureBuilderGetWidth(&builder), 48);
    Gives(why, sizeof(why), "the builder's height", nvn.nvn.nvnTextureBuilderGetHeight(&builder),
          24);
    Gives(why, sizeof(why), "the builder's depth", nvn.nvn.nvnTextureBuilderGetDepth(&builder), 5);
    Gives(why, sizeof(why), "the builder's levels", nvn.nvn.nvnTextureBuilderGetLevels(&builder),
          3);
    Gives(why, sizeof(why), "the builder's samples", nvn.nvn.nvnTextureBuilderGetSamples(&builder),
          4);
    Gives(why, sizeof(why), "the builder's memory pool",
          (long long)(uintptr_t)nvn.nvn.nvnTextureBuilderGetMemoryPool(&builder),
          (long long)(uintptr_t)&nvn.target_pool);
    Gives(why, sizeof(why), "the builder's memory offset",
          (long long)nvn.nvn.nvnTextureBuilderGetMemoryOffset(&builder), 0x3000);
    // The size set in one call is the width and the height set apart.
    nvn.nvn.nvnTextureBuilderSetSize2D(&builder, 40, 20);
    Gives(why, sizeof(why), "the builder's width after nvnTextureBuilderSetSize2D",
          nvn.nvn.nvnTextureBuilderGetWidth(&builder), 40);
    Gives(why, sizeof(why), "the builder's height after nvnTextureBuilderSetSize2D",
          nvn.nvn.nvnTextureBuilderGetHeight(&builder), 20);
    CHECK_TRUE(SUITE, name, CHECK_EXACT, why[0] == '\0' && NvnLayerError()[0] == '\0', "%s",
               why[0] != '\0' ? why : NvnLayerError());
}

/// A view starts with no part set and says so; a part set is given back with what was set, the
/// others still unset; two views are the same when the same parts are set the same.
static void TextureViewCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    _Alignas(8) NVNtextureView view, other;
    memset(&view, 0xee, sizeof(view));
    memset(&other, 0xee, sizeof(other));
    nvn.nvn.nvnTextureViewSetDefaults(&view);
    nvn.nvn.nvnTextureViewSetDefaults(&other);
    int first = -1, count = -1;
    NVNformat format = NVN_FORMAT_NONE;
    NVNtextureTarget target = NVN_TEXTURE_TARGET_1D;
    Gives(why, sizeof(why), "whether a new view has levels set",
          nvn.nvn.nvnTextureViewGetLevels(&view, &first, &count), 0);
    Gives(why, sizeof(why), "whether a new view has layers set",
          nvn.nvn.nvnTextureViewGetLayers(&view, &first, &count), 0);
    Gives(why, sizeof(why), "whether a new view has a format set",
          nvn.nvn.nvnTextureViewGetFormat(&view, &format), 0);
    Gives(why, sizeof(why), "whether a new view has a target set",
          nvn.nvn.nvnTextureViewGetTarget(&view, &target), 0);
    Gives(why, sizeof(why), "whether two new views are the same",
          nvn.nvn.nvnTextureViewCompare(&view, &other), 1);

    nvn.nvn.nvnTextureViewSetLevels(&view, 1, 2);
    Gives(why, sizeof(why), "whether a view with levels set says so",
          nvn.nvn.nvnTextureViewGetLevels(&view, &first, &count), 1);
    Gives(why, sizeof(why), "the view's first level", first, 1);
    Gives(why, sizeof(why), "the view's number of levels", count, 2);
    Gives(why, sizeof(why), "whether a view with only levels set has layers set",
          nvn.nvn.nvnTextureViewGetLayers(&view, &first, &count), 0);
    Gives(why, sizeof(why), "whether a view with levels is the same as a new one",
          nvn.nvn.nvnTextureViewCompare(&view, &other), 0);

    nvn.nvn.nvnTextureViewSetLayers(&view, 3, 4);
    nvn.nvn.nvnTextureViewSetFormat(&view, NVN_FORMAT_BGRA8);
    nvn.nvn.nvnTextureViewSetTarget(&view, NVN_TEXTURE_TARGET_2D_ARRAY);
    Gives(why, sizeof(why), "whether a view with layers set says so",
          nvn.nvn.nvnTextureViewGetLayers(&view, &first, &count), 1);
    Gives(why, sizeof(why), "the view's first layer", first, 3);
    Gives(why, sizeof(why), "the view's number of layers", count, 4);
    Gives(why, sizeof(why), "whether a view with a format set says so",
          nvn.nvn.nvnTextureViewGetFormat(&view, &format), 1);
    Gives(why, sizeof(why), "the view's format", format, NVN_FORMAT_BGRA8);
    Gives(why, sizeof(why), "whether a view with a target set says so",
          nvn.nvn.nvnTextureViewGetTarget(&view, &target), 1);
    Gives(why, sizeof(why), "the view's target", target, NVN_TEXTURE_TARGET_2D_ARRAY);

    nvn.nvn.nvnTextureViewSetLevels(&other, 1, 2);
    nvn.nvn.nvnTextureViewSetLayers(&other, 3, 4);
    nvn.nvn.nvnTextureViewSetFormat(&other, NVN_FORMAT_BGRA8);
    nvn.nvn.nvnTextureViewSetTarget(&other, NVN_TEXTURE_TARGET_2D_ARRAY);
    Gives(why, sizeof(why), "whether two views set the same are the same",
          nvn.nvn.nvnTextureViewCompare(&view, &other), 1);
    nvn.nvn.nvnTextureViewSetLevels(&other, 1, 1);
    Gives(why, sizeof(why), "whether two views of 2 levels and of 1 are the same",
          nvn.nvn.nvnTextureViewCompare(&view, &other), 0);
    CHECK_TRUE(SUITE, name, CHECK_EXACT, why[0] == '\0' && NvnLayerError()[0] == '\0', "%s",
               why[0] != '\0' ? why : NvnLayerError());
}

/// The levels texture, at an offset of its own in its pool, and the pool its levels are read
/// back into; then what the texture says of itself. False with the first thing wrong in @p why.
static bool BringLevels(char *why, size_t why_size) {
    _Alignas(8) NVNtextureBuilder builder;
    memset(&builder, 0, sizeof(builder));
    TextureBuilder(&builder, TARGET_SIZE, TARGET_SIZE, 0);
    nvn.nvn.nvnTextureBuilderSetDepth(&builder, 1);
    nvn.nvn.nvnTextureBuilderSetLevels(&builder, LEVELS);
    nvn.nvn.nvnTextureBuilderSetSamples(&builder, 0);
    const size_t size = nvn.nvn.nvnTextureBuilderGetStorageSize(&builder);
    const size_t alignment = nvn.nvn.nvnTextureBuilderGetStorageAlignment(&builder);
    if (size == 0u || alignment == 0u || alignment > POOL_PAGE) {
        snprintf(why, why_size, "a texture of %d levels has a storage of %zu bytes at an "
                 "alignment of %zu: %s",
                 LEVELS, size, alignment, Why("an answer no pool here can hold"));
        return false;
    }
    // Past the pool's start, so that the offset the texture gives back is a number of its own.
    nvn.levels_offset = (ptrdiff_t)POOL_PAGE;
    nvn.levels_pool_up =
        MakePool(&nvn.levels_pool, &nvn.levels_storage, POOL_PAGE + RoundUp(size, POOL_PAGE),
                 NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS | NVN_MEMORY_POOL_FLAGS_GPU_CACHED);
    nvn.levels_read_pool_up =
        MakePool(&nvn.levels_read_pool, &nvn.levels_read_storage, LEVELS_READ_BYTES,
                 NVN_MEMORY_POOL_FLAGS_CPU_UNCACHED | NVN_MEMORY_POOL_FLAGS_GPU_CACHED);
    if (!nvn.levels_pool_up || !nvn.levels_read_pool_up) {
        snprintf(why, why_size, "the levels texture's pools: %s", Why("no memory, or false"));
        return false;
    }
    nvn.nvn.nvnTextureBuilderSetStorage(&builder, &nvn.levels_pool, nvn.levels_offset);
    if (!(nvn.levels_up = nvn.nvn.nvnTextureInitialize(&nvn.levels, &builder))) {
        snprintf(why, why_size, "nvnTextureInitialize of a texture of %d levels: %s", LEVELS,
                 Why("false"));
        return false;
    }
    why[0] = '\0';
    Gives(why, why_size, "the texture's width", nvn.nvn.nvnTextureGetWidth(&nvn.levels),
          TARGET_SIZE);
    Gives(why, why_size, "the texture's height", nvn.nvn.nvnTextureGetHeight(&nvn.levels),
          TARGET_SIZE);
    Gives(why, why_size, "the texture's depth", nvn.nvn.nvnTextureGetDepth(&nvn.levels), 1);
    Gives(why, why_size, "the texture's levels", nvn.nvn.nvnTextureGetLevels(&nvn.levels), LEVELS);
    Gives(why, why_size, "the texture's samples", nvn.nvn.nvnTextureGetSamples(&nvn.levels), 0);
    Gives(why, why_size, "the texture's format", nvn.nvn.nvnTextureGetFormat(&nvn.levels),
          NVN_FORMAT_RGBA8);
    Gives(why, why_size, "the texture's target", nvn.nvn.nvnTextureGetTarget(&nvn.levels),
          NVN_TEXTURE_TARGET_2D);
    Gives(why, why_size, "the texture's flags", nvn.nvn.nvnTextureGetFlags(&nvn.levels), 0);
    Gives(why, why_size, "the texture's memory pool",
          (long long)(uintptr_t)nvn.nvn.nvnTextureGetMemoryPool(&nvn.levels),
          (long long)(uintptr_t)&nvn.levels_pool);
    Gives(why, why_size, "the texture's memory offset",
          (long long)nvn.nvn.nvnTextureGetMemoryOffset(&nvn.levels), (long long)nvn.levels_offset);
    return why[0] == '\0';
}

/// Level @p level's texel (x, y): the copy cases' picture, its blue moved by the level, so no
/// level's picture is a part of another's.
static void LevelTexel(uint32_t level, uint32_t x, uint32_t y, uint8_t out[4]) {
    Pattern(x, y, out);
    out[2] = (uint8_t)(out[2] + 0x51u * level);
}

/// Where level @p level's texels start among LEVELS_BYTES, and its side in texels.
static uint32_t LevelStart(uint32_t level, uint32_t *side) {
    uint32_t start = 0;
    for (uint32_t i = 0; i < level; ++i) {
        start += (TARGET_SIZE >> i) * (TARGET_SIZE >> i) * 4u;
    }
    *side = TARGET_SIZE >> level;
    return start;
}

/// A picture of its own copied into each level of the levels texture, the first with no view and
/// the others through a view of that level alone, then each level read back through its view.
/// Every byte of the readback is judged: a level that took another's picture, or kept nothing,
/// shows in the first byte wrong.
static void LevelsCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    uint8_t *const stage = nvn.nvn.nvnMemoryPoolMap(&nvn.stage_pool);
    uint8_t *const read = nvn.nvn.nvnMemoryPoolMap(&nvn.levels_read_pool);
    if (stage == NULL || read == NULL) {
        ReportCase(SUITE, name, CHECK_NONE, false, Why("nvnMemoryPoolMap gave no address"));
        return;
    }
    for (uint32_t level = 0; level < LEVELS; ++level) {
        uint32_t side;
        const uint32_t start = LevelStart(level, &side);
        for (uint32_t y = 0; y < side; ++y) {
            for (uint32_t x = 0; x < side; ++x) {
                LevelTexel(level, x, y, stage + start + ((size_t)y * side + x) * 4u);
            }
        }
    }
    memset(read, READ_PRESET, LEVELS_READ_BYTES);
    const NVNbufferAddress picture = nvn.nvn.nvnMemoryPoolGetBufferAddress(&nvn.stage_pool);
    const NVNbufferAddress readback = nvn.nvn.nvnMemoryPoolGetBufferAddress(&nvn.levels_read_pool);
    _Alignas(8) NVNtextureView views[LEVELS];
    NVNcommandBuffer *const cmd = &nvn.command;
    nvn.nvn.nvnCommandBufferBeginRecording(cmd);
    nvn.nvn.nvnCommandBufferSetCopyRowStride(cmd, 0);
    for (uint32_t level = 0; level < LEVELS; ++level) {
        uint32_t side;
        const uint32_t start = LevelStart(level, &side);
        nvn.nvn.nvnTextureViewSetDefaults(&views[level]);
        nvn.nvn.nvnTextureViewSetLevels(&views[level], (int)level, 1);
        const NVNcopyRegion region = { 0, 0, 0, (int)side, (int)side, 1 };
        // No view is the first level, as every copy of the cases above has it.
        nvn.nvn.nvnCommandBufferCopyBufferToTexture(cmd, picture + start, &nvn.levels,
                                                    level == 0u ? NULL : &views[level], &region,
                                                    NVN_COPY_FLAGS_NONE);
    }
    nvn.nvn.nvnCommandBufferBarrier(cmd, NVN_BARRIER_ORDER_PRIMITIVES_BIT |
                                             NVN_BARRIER_INVALIDATE_TEXTURE_BIT);
    for (uint32_t level = 0; level < LEVELS; ++level) {
        uint32_t side;
        const uint32_t start = LevelStart(level, &side);
        const NVNcopyRegion region = { 0, 0, 0, (int)side, (int)side, 1 };
        nvn.nvn.nvnCommandBufferCopyTextureToBuffer(cmd, &nvn.levels, &views[level], &region,
                                                    readback + start, NVN_COPY_FLAGS_NONE);
    }
    const NVNcommandHandle list = nvn.nvn.nvnCommandBufferEndRecording(cmd);
    if (list == 0u) {
        ReportCase(SUITE, name, CHECK_NONE, false, Why("nvnCommandBufferEndRecording gave no list"));
        return;
    }
    nvn.nvn.nvnQueueSubmitCommands(&nvn.queue, 1, &list);
    nvn.nvn.nvnQueueFinish(&nvn.queue);
    if (NvnLayerError()[0] != '\0') {
        ReportCase(SUITE, name, CHECK_NONE, false, NvnLayerError());
        return;
    }
    uint32_t wrong = 0, first = 0, first_level = 0;
    uint8_t want_first = 0;
    for (uint32_t at = 0; at < LEVELS_READ_BYTES; ++at) {
        uint8_t want = READ_PRESET;
        uint32_t in_level = LEVELS;
        for (uint32_t level = 0; level < LEVELS; ++level) {
            uint32_t side;
            const uint32_t start = LevelStart(level, &side);
            if (at >= start && at < start + side * side * 4u) {
                const uint32_t texel = (at - start) / 4u;
                uint8_t bytes[4];
                LevelTexel(level, texel % side, texel / side, bytes);
                want = bytes[(at - start) % 4u];
                in_level = level;
            }
        }
        if (read[at] != want && wrong++ == 0u) {
            first = at;
            first_level = in_level;
            want_first = want;
        }
    }
    CHECK_TRUE(SUITE, name, CHECK_EXACT, wrong == 0u,
               "%u of %u bytes wrong; the first, byte %u (level %u of %u; %u is past the last "
               "level), is 0x%02x, expected 0x%02x (0x%02x is a byte nothing copied)",
               (unsigned)wrong, (unsigned)LEVELS_READ_BYTES, (unsigned)first,
               (unsigned)first_level, (unsigned)LEVELS, (unsigned)LEVELS, read[first], want_first,
               READ_PRESET);
}

// ---- what a memory pool answers of itself, and the processor's cache of one

// Every other pool of the suite is uncached to the processor, which needs neither call: a game
// keeps its vertex and uniform data in cached pools, and flushes what it wrote before the
// graphics unit reads it. The pool here is two pages, the first the bytes and the second where a
// copy by the graphics unit puts them.

#define CACHED_BYTES (2u * POOL_PAGE)
#define CACHED_FLAGS (NVN_MEMORY_POOL_FLAGS_CPU_CACHED | NVN_MEMORY_POOL_FLAGS_GPU_CACHED)

/// Byte @p at of the page the cache case copies; no run of it is the preset's.
static uint8_t CachedByte(uint32_t at) {
    return (uint8_t)(at * 7u + (at >> 8) + 3u);
}

/// A cached pool made through a builder, with what the builder and the pool then say: the
/// device, the storage, the size and the flags, and a mapping that is the storage itself, since
/// a pool is the memory the program gave it.
static void PoolCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    nvn.cached_storage = aligned_alloc(POOL_PAGE, CACHED_BYTES);
    if (nvn.cached_storage == NULL) {
        ReportCase(SUITE, name, CHECK_NONE, false, "no memory for a pool of two pages");
        return;
    }
    memset(nvn.cached_storage, 0, CACHED_BYTES);
    _Alignas(8) NVNmemoryPoolBuilder builder;
    nvn.nvn.nvnMemoryPoolBuilderSetDefaults(&builder);
    nvn.nvn.nvnMemoryPoolBuilderSetDevice(&builder, &nvn.device);
    nvn.nvn.nvnMemoryPoolBuilderSetStorage(&builder, nvn.cached_storage, CACHED_BYTES);
    nvn.nvn.nvnMemoryPoolBuilderSetFlags(&builder, CACHED_FLAGS);
    Gives(why, sizeof(why), "the builder's device",
          (long long)(uintptr_t)nvn.nvn.nvnMemoryPoolBuilderGetDevice(&builder),
          (long long)(uintptr_t)&nvn.device);
    Gives(why, sizeof(why), "the builder's memory",
          (long long)(uintptr_t)nvn.nvn.nvnMemoryPoolBuilderGetMemory(&builder),
          (long long)(uintptr_t)nvn.cached_storage);
    Gives(why, sizeof(why), "the builder's size",
          (long long)nvn.nvn.nvnMemoryPoolBuilderGetSize(&builder), CACHED_BYTES);
    Gives(why, sizeof(why), "the builder's flags",
          nvn.nvn.nvnMemoryPoolBuilderGetFlags(&builder), CACHED_FLAGS);
    nvn.cached_pool_up = nvn.nvn.nvnMemoryPoolInitialize(&nvn.cached_pool, &builder);
    if (!nvn.cached_pool_up) {
        free(nvn.cached_storage);
        nvn.cached_storage = NULL;
        ReportCase(SUITE, name, CHECK_NONE, false,
                   Why("nvnMemoryPoolInitialize of a pool cached to the processor: false"));
        return;
    }
    Gives(why, sizeof(why), "the pool's size", (long long)nvn.nvn.nvnMemoryPoolGetSize(
                                                   &nvn.cached_pool),
          CACHED_BYTES);
    Gives(why, sizeof(why), "the pool's flags", nvn.nvn.nvnMemoryPoolGetFlags(&nvn.cached_pool),
          CACHED_FLAGS);
    Gives(why, sizeof(why), "the pool's mapping",
          (long long)(uintptr_t)nvn.nvn.nvnMemoryPoolMap(&nvn.cached_pool),
          (long long)(uintptr_t)nvn.cached_storage);
    CHECK_TRUE(SUITE, name, CHECK_EXACT, why[0] == '\0' && NvnLayerError()[0] == '\0', "%s",
               why[0] != '\0' ? why : NvnLayerError());
}

/// A page written through the cached pool's mapping and flushed, copied by the graphics unit
/// into the pool's second page, which is read after an invalidate: both pages, every byte.
static void PoolCacheCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    uint8_t *const bytes = nvn.nvn.nvnMemoryPoolMap(&nvn.cached_pool);
    if (bytes == NULL) {
        ReportCase(SUITE, name, CHECK_NONE, false, Why("nvnMemoryPoolMap gave no address"));
        return;
    }
    for (uint32_t at = 0; at < POOL_PAGE; ++at) {
        bytes[at] = CachedByte(at);
    }
    memset(bytes + POOL_PAGE, READ_PRESET, POOL_PAGE);
    nvn.nvn.nvnMemoryPoolFlushMappedRange(&nvn.cached_pool, 0, CACHED_BYTES);
    const NVNbufferAddress address = nvn.nvn.nvnMemoryPoolGetBufferAddress(&nvn.cached_pool);
    nvn.nvn.nvnCommandBufferBeginRecording(&nvn.command);
    nvn.nvn.nvnCommandBufferCopyBufferToBuffer(&nvn.command, address, address + POOL_PAGE,
                                               POOL_PAGE, NVN_COPY_FLAGS_NONE);
    const NVNcommandHandle list = nvn.nvn.nvnCommandBufferEndRecording(&nvn.command);
    if (list == 0u) {
        ReportCase(SUITE, name, CHECK_NONE, false, Why("nvnCommandBufferEndRecording gave no list"));
        return;
    }
    nvn.nvn.nvnQueueSubmitCommands(&nvn.queue, 1, &list);
    nvn.nvn.nvnQueueFinish(&nvn.queue);
    nvn.nvn.nvnMemoryPoolInvalidateMappedRange(&nvn.cached_pool, (ptrdiff_t)POOL_PAGE, POOL_PAGE);
    if (NvnLayerError()[0] != '\0') {
        ReportCase(SUITE, name, CHECK_NONE, false, NvnLayerError());
        return;
    }
    uint32_t wrong = 0, first = 0;
    for (uint32_t at = 0; at < CACHED_BYTES; ++at) {
        if (bytes[at] != CachedByte(at % POOL_PAGE) && wrong++ == 0u) {
            first = at;
        }
    }
    CHECK_TRUE(SUITE, name, CHECK_EXACT, wrong == 0u,
               "%u of %u bytes wrong; the first, byte %u of the %s page, is 0x%02x, expected "
               "0x%02x (0x%02x is a byte nothing copied)",
               (unsigned)wrong, (unsigned)CACHED_BYTES, (unsigned)(first % POOL_PAGE),
               first < POOL_PAGE ? "written" : "copied", bytes[first],
               CachedByte(first % POOL_PAGE), READ_PRESET);
}

// ---- syncs, a queue's error and its builder, events

// A game paces itself on syncs: it fences one behind a frame's work, flushes, and waits on it,
// never finishing the queue. The clear cases' question is asked that way here, the readback
// read once the sync signals, with the fence put on the queue and inside the list.

/// Ten seconds, in a sync wait's nanoseconds: far past any clear, and an end to a wait on a sync
/// that never signals.
#define SYNC_WAIT_NS 10000000000ull

typedef enum {
    FENCE_QUEUE,
    FENCE_LIST,
    FENCE_WAITED,
} FenceKind;

/// The clear cases' list (a clear of the target to @p rgba, a barrier, a copy to the readback
/// pool) submitted, flushed and never finished: the answer is read when @p sync signals. The
/// fence is the queue's after the submit, or the list's last command (FENCE_LIST). For
/// FENCE_WAITED, @p sync has signalled already and the queue and the list are both told to wait
/// for it first, with a second sync fenced behind the work.
static void FenceCase(const char *name, FenceKind kind, const float rgba[4],
                      const uint8_t expected[4]) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    static _Alignas(8) NVNsync sync, before;
    uint8_t *const pixels = nvn.nvn.nvnMemoryPoolMap(&nvn.read_pool);
    if (pixels == NULL || !nvn.nvn.nvnSyncInitialize(&sync, &nvn.device)) {
        ReportCase(SUITE, name, CHECK_NONE, false,
                   Why("nvnMemoryPoolMap gave no address, or nvnSyncInitialize answered false"));
        return;
    }
    bool before_up = false;
    char why[256] = "";
    if (kind == FENCE_WAITED) {
        // A sync that has signalled: fenced behind nothing, flushed, waited for.
        before_up = nvn.nvn.nvnSyncInitialize(&before, &nvn.device);
        if (before_up) {
            nvn.nvn.nvnQueueFenceSync(&nvn.queue, &before,
                                      NVN_SYNC_CONDITION_ALL_GPU_COMMANDS_COMPLETE, 0);
            nvn.nvn.nvnQueueFlush(&nvn.queue);
            const NVNsyncWaitResult first = nvn.nvn.nvnSyncWait(&before, SYNC_WAIT_NS);
            if (first != NVN_SYNC_WAIT_RESULT_ALREADY_SIGNALED &&
                first != NVN_SYNC_WAIT_RESULT_CONDITION_SATISFIED) {
                snprintf(why, sizeof(why), "the sync fenced behind no work answered its wait "
                         "with %d: %s",
                         (int)first, Why("neither signalled (0) nor satisfied (1)"));
            }
        } else {
            snprintf(why, sizeof(why), "nvnSyncInitialize of the first sync: %s", Why("false"));
        }
    }
    memset(pixels, READ_PRESET, TARGET_BYTES);
    NVNsyncWaitResult result = NVN_SYNC_WAIT_RESULT_FAILED;
    if (why[0] == '\0') {
        nvn.nvn.nvnCommandBufferBeginRecording(&nvn.command);
        if (kind == FENCE_WAITED) {
            nvn.nvn.nvnCommandBufferWaitSync(&nvn.command, &before);
        }
        RecordClear(&nvn.target, TARGET_SIZE, TARGET_SIZE, rgba);
        nvn.nvn.nvnCommandBufferBarrier(&nvn.command, NVN_BARRIER_ORDER_FRAGMENTS_BIT |
                                                          NVN_BARRIER_INVALIDATE_TEXTURE_BIT);
        const NVNcopyRegion region = { 0, 0, 0, TARGET_SIZE, TARGET_SIZE, 1 };
        nvn.nvn.nvnCommandBufferCopyTextureToBuffer(
            &nvn.command, &nvn.target, NULL, &region,
            nvn.nvn.nvnMemoryPoolGetBufferAddress(&nvn.read_pool), NVN_COPY_FLAGS_NONE);
        if (kind == FENCE_LIST) {
            nvn.nvn.nvnCommandBufferFenceSync(&nvn.command, &sync,
                                              NVN_SYNC_CONDITION_ALL_GPU_COMMANDS_COMPLETE, 0);
        }
        const NVNcommandHandle list = nvn.nvn.nvnCommandBufferEndRecording(&nvn.command);
        if (kind == FENCE_WAITED && !nvn.nvn.nvnQueueWaitSync(&nvn.queue, &before)) {
            snprintf(why, sizeof(why), "nvnQueueWaitSync on a sync that had signalled: %s",
                     Why("false"));
        }
        nvn.nvn.nvnQueueSubmitCommands(&nvn.queue, 1, &list);
        if (kind != FENCE_LIST) {
            nvn.nvn.nvnQueueFenceSync(&nvn.queue, &sync,
                                      NVN_SYNC_CONDITION_ALL_GPU_COMMANDS_COMPLETE, 0);
        }
        nvn.nvn.nvnQueueFlush(&nvn.queue);
        result = nvn.nvn.nvnSyncWait(&sync, SYNC_WAIT_NS);
    }
    uint32_t wrong = 0;
    uint8_t first[4] = { 0, 0, 0, 0 };
    const bool signalled = result == NVN_SYNC_WAIT_RESULT_ALREADY_SIGNALED ||
                           result == NVN_SYNC_WAIT_RESULT_CONDITION_SATISFIED;
    if (signalled) {
        for (uint32_t at = 0; at < TARGET_SIZE * TARGET_SIZE; ++at) {
            if (memcmp(pixels + (size_t)at * 4u, expected, 4) != 0 && wrong++ == 0u) {
                memcpy(first, pixels + (size_t)at * 4u, 4);
            }
        }
    }
    // The queue is left with nothing running, whatever the wait said, before the syncs go.
    nvn.nvn.nvnQueueFinish(&nvn.queue);
    nvn.nvn.nvnSyncFinalize(&sync);
    if (before_up) {
        nvn.nvn.nvnSyncFinalize(&before);
    }
    if (why[0] == '\0' && NvnLayerError()[0] != '\0') {
        snprintf(why, sizeof(why), "%s", NvnLayerError());
    }
    if (why[0] != '\0') {
        ReportCase(SUITE, name, CHECK_NONE, false, why);
        return;
    }
    CHECK_TRUE(SUITE, name, CHECK_EXACT, signalled && wrong == 0u,
               "nvnSyncWait answered %d after up to 10 s (0 is signalled, 1 satisfied, 2 timed "
               "out, 3 failed); once it signalled, %u of %u pixels were wrong, the first "
               "%02x%02x%02x%02x, expected %02x%02x%02x%02x (0x%02x is a byte nothing copied)",
               (int)result, (unsigned)wrong, (unsigned)(TARGET_SIZE * TARGET_SIZE), first[0],
               first[1], first[2], first[3], expected[0], expected[1], expected[2], expected[3],
               READ_PRESET);
}

/// A sync nothing was told to signal: a wait on it has nothing to wait for and fails, at once
/// and with a timeout alike, where a wait that only ran out says so.
static void UnfencedCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    static _Alignas(8) NVNsync sync;
    if (!nvn.nvn.nvnSyncInitialize(&sync, &nvn.device)) {
        ReportCase(SUITE, name, CHECK_NONE, false, Why("nvnSyncInitialize answered false"));
        return;
    }
    const NVNsyncWaitResult poll = nvn.nvn.nvnSyncWait(&sync, 0);
    // A hundredth of a second: enough to tell a wait that ran out from one that failed.
    const NVNsyncWaitResult timed = nvn.nvn.nvnSyncWait(&sync, 10000000ull);
    nvn.nvn.nvnSyncFinalize(&sync);
    CHECK_TRUE(SUITE, name, CHECK_EXACT,
               poll == NVN_SYNC_WAIT_RESULT_FAILED && timed == NVN_SYNC_WAIT_RESULT_FAILED,
               "nvnSyncWait answered %d with no timeout and %d with one of 10 ms, where a sync "
               "nothing fenced fails both (%d; %d is a wait that ran out)",
               (int)poll, (int)timed, (int)NVN_SYNC_WAIT_RESULT_FAILED,
               (int)NVN_SYNC_WAIT_RESULT_TIMEOUT_EXPIRED);
}

/// The queue's own word on the work the cases above gave it, all of which was valid.
static void QueueErrorCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    NVNqueueErrorInfo info;
    memset(&info, 0, sizeof(info));
    nvn.nvn.nvnQueueFinish(&nvn.queue);
    const NVNqueueGetErrorResult error = nvn.nvn.nvnQueueGetError(&nvn.queue, &info);
    CHECK_TRUE(SUITE, name, CHECK_EXACT, error == NVN_QUEUE_GET_ERROR_RESULT_GPU_NO_ERROR,
               "nvnQueueGetError answered %d, where a queue whose work was valid has no error "
               "(%d)",
               (int)error, (int)NVN_QUEUE_GET_ERROR_RESULT_GPU_NO_ERROR);
}

/// A queue builder's flags and device, and its four sizes, each of which a getter says was set
/// or not, with the size. The builder makes no queue, so the sizes need not be ones a machine
/// takes.
static void QueueBuilderCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    _Alignas(8) NVNqueueBuilder builder;
    memset(&builder, 0, sizeof(builder));
    nvn.nvn.nvnQueueBuilderSetDevice(&builder, &nvn.device);
    nvn.nvn.nvnQueueBuilderSetDefaults(&builder);
    int size = -1;
    Gives(why, sizeof(why), "whether a new builder has a command memory size set",
          nvn.nvn.nvnQueueBuilderGetCommandMemorySize(&builder, &size), 0);
    Gives(why, sizeof(why), "whether a new builder has a compute memory size set",
          nvn.nvn.nvnQueueBuilderGetComputeMemorySize(&builder, &size), 0);
    Gives(why, sizeof(why), "whether a new builder has a control memory size set",
          nvn.nvn.nvnQueueBuilderGetControlMemorySize(&builder, &size), 0);
    Gives(why, sizeof(why), "whether a new builder has a command flush threshold set",
          nvn.nvn.nvnQueueBuilderGetCommandFlushThreshold(&builder, &size), 0);
    nvn.nvn.nvnQueueBuilderSetFlags(&builder, NVN_QUEUE_BUILDER_FLAGS_NO_ZCULL);
    nvn.nvn.nvnQueueBuilderSetCommandMemorySize(&builder, 0x30000);
    nvn.nvn.nvnQueueBuilderSetComputeMemorySize(&builder, 0x50000);
    nvn.nvn.nvnQueueBuilderSetControlMemorySize(&builder, 0x7000);
    nvn.nvn.nvnQueueBuilderSetCommandFlushThreshold(&builder, 0x9000);
    Gives(why, sizeof(why), "the builder's device",
          (long long)(uintptr_t)nvn.nvn.nvnQueueBuilderGetDevice(&builder),
          (long long)(uintptr_t)&nvn.device);
    Gives(why, sizeof(why), "the builder's flags", nvn.nvn.nvnQueueBuilderGetFlags(&builder),
          NVN_QUEUE_BUILDER_FLAGS_NO_ZCULL);
    Gives(why, sizeof(why), "whether a command memory size that was set is said to be",
          nvn.nvn.nvnQueueBuilderGetCommandMemorySize(&builder, &size), 1);
    Gives(why, sizeof(why), "the builder's command memory size", size, 0x30000);
    Gives(why, sizeof(why), "whether a compute memory size that was set is said to be",
          nvn.nvn.nvnQueueBuilderGetComputeMemorySize(&builder, &size), 1);
    Gives(why, sizeof(why), "the builder's compute memory size", size, 0x50000);
    Gives(why, sizeof(why), "whether a control memory size that was set is said to be",
          nvn.nvn.nvnQueueBuilderGetControlMemorySize(&builder, &size), 1);
    Gives(why, sizeof(why), "the builder's control memory size", size, 0x7000);
    Gives(why, sizeof(why), "whether a command flush threshold that was set is said to be",
          nvn.nvn.nvnQueueBuilderGetCommandFlushThreshold(&builder, &size), 1);
    Gives(why, sizeof(why), "the builder's command flush threshold", size, 0x9000);
    CHECK_TRUE(SUITE, name, CHECK_EXACT, why[0] == '\0' && NvnLayerError()[0] == '\0', "%s",
               why[0] != '\0' ? why : NvnLayerError());
}

/// An event is a word in a pool the processor reaches: what its builder and the event say of
/// where it is, and the word after a write, an add and an add that wraps.
static void EventCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    // The last word of the readback pool's first page, which this case alone uses here.
    const ptrdiff_t offset = (ptrdiff_t)(POOL_PAGE - 4u);
    _Alignas(8) NVNeventBuilder builder;
    static _Alignas(8) NVNevent event;
    nvn.nvn.nvnEventBuilderSetDefaults(&builder);
    nvn.nvn.nvnEventBuilderSetStorage(&builder, &nvn.read_pool, offset);
    ptrdiff_t given = -1;
    Gives(why, sizeof(why), "the builder's storage",
          (long long)(uintptr_t)nvn.nvn.nvnEventBuilderGetStorage(&builder, &given),
          (long long)(uintptr_t)&nvn.read_pool);
    Gives(why, sizeof(why), "the offset the builder's storage comes with", (long long)given,
          (long long)offset);
    Gives(why, sizeof(why), "the builder's memory pool",
          (long long)(uintptr_t)nvn.nvn.nvnEventBuilderGetMemoryPool(&builder),
          (long long)(uintptr_t)&nvn.read_pool);
    Gives(why, sizeof(why), "the builder's memory offset",
          (long long)nvn.nvn.nvnEventBuilderGetMemoryOffset(&builder), (long long)offset);
    if (!nvn.nvn.nvnEventInitialize(&event, &builder)) {
        ReportCase(SUITE, name, CHECK_NONE, false, Why("nvnEventInitialize answered false"));
        return;
    }
    Gives(why, sizeof(why), "the event's memory pool",
          (long long)(uintptr_t)nvn.nvn.nvnEventGetMemoryPool(&event),
          (long long)(uintptr_t)&nvn.read_pool);
    Gives(why, sizeof(why), "the event's memory offset",
          (long long)nvn.nvn.nvnEventGetMemoryOffset(&event), (long long)offset);
    nvn.nvn.nvnEventSignal(&event, NVN_EVENT_SIGNAL_MODE_WRITE, 0x12345678u);
    Gives(why, sizeof(why), "the event's value after a write of 0x12345678",
          nvn.nvn.nvnEventGetValue(&event), 0x12345678ll);
    nvn.nvn.nvnEventSignal(&event, NVN_EVENT_SIGNAL_MODE_ADD, 0x11u);
    Gives(why, sizeof(why), "the event's value after an add of 0x11",
          nvn.nvn.nvnEventGetValue(&event), 0x12345689ll);
    nvn.nvn.nvnEventSignal(&event, NVN_EVENT_SIGNAL_MODE_ADD, 0xffffffffu);
    Gives(why, sizeof(why), "the event's value after an add of 0xffffffff, which wraps",
          nvn.nvn.nvnEventGetValue(&event), 0x12345688ll);
    // The event is its word: the pool's mapping shows it where the builder put it.
    const uint8_t *const bytes = nvn.nvn.nvnMemoryPoolMap(&nvn.read_pool);
    uint32_t word = 0;
    if (bytes != NULL) {
        memcpy(&word, bytes + offset, sizeof(word));
    }
    Gives(why, sizeof(why), "the word of the pool the event is in", word, 0x12345688ll);
    nvn.nvn.nvnEventFinalize(&event);
    CHECK_TRUE(SUITE, name, CHECK_EXACT, why[0] == '\0' && NvnLayerError()[0] == '\0', "%s",
               why[0] != '\0' ? why : NvnLayerError());
}

// ---- what a sampler and its builder answer of themselves, and the two descriptor pools

// A game keeps one sampler for each state it draws with and finds it again by asking: the
// getters and nvnSamplerCompare are how. The numbers set here are all exact in a float, so what
// comes back is compared bit for bit, and none is the default of anything.

/// A sampler builder on the suite's device with every setter called, @p bias its level bias.
static void SamplerBuilder(NVNsamplerBuilder *builder, float bias) {
    static const float border[4] = { 0.25f, 0.5f, 0.75f, 1.0f };
    memset(builder, 0, sizeof(*builder));
    nvn.nvn.nvnSamplerBuilderSetDevice(builder, &nvn.device);
    nvn.nvn.nvnSamplerBuilderSetDefaults(builder);
    nvn.nvn.nvnSamplerBuilderSetMinMagFilter(builder, NVN_MIN_FILTER_LINEAR_MIPMAP_NEAREST,
                                             NVN_MAG_FILTER_LINEAR);
    nvn.nvn.nvnSamplerBuilderSetWrapMode(builder, NVN_WRAP_MODE_REPEAT,
                                         NVN_WRAP_MODE_MIRRORED_REPEAT,
                                         NVN_WRAP_MODE_CLAMP_TO_BORDER);
    nvn.nvn.nvnSamplerBuilderSetCompare(builder, NVN_COMPARE_MODE_COMPARE_R_TO_TEXTURE,
                                        NVN_COMPARE_FUNC_GEQUAL);
    nvn.nvn.nvnSamplerBuilderSetReductionFilter(builder, NVN_SAMPLER_REDUCTION_MAX);
    nvn.nvn.nvnSamplerBuilderSetLodClamp(builder, 1.5f, 6.25f);
    nvn.nvn.nvnSamplerBuilderSetLodBias(builder, bias);
    nvn.nvn.nvnSamplerBuilderSetLodSnap(builder, 0.25f);
    nvn.nvn.nvnSamplerBuilderSetMaxAnisotropy(builder, 4.0f);
    nvn.nvn.nvnSamplerBuilderSetBorderColor(builder, border);
}

/// A float's bits, for Gives: the numbers here are exact, so the bits are the comparison.
static long long Bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

/// What SamplerBuilder set, as the getters of a builder (@p sampler NULL) or of a sampler give
/// it.
static void SamplerGives(char *why, size_t why_size, const NVNsamplerBuilder *builder,
                         const NVNsampler *sampler, float bias) {
    NVNminFilter min = NVN_MIN_FILTER_NEAREST;
    NVNmagFilter mag = NVN_MAG_FILTER_NEAREST;
    NVNwrapMode s = NVN_WRAP_MODE_CLAMP, t = NVN_WRAP_MODE_CLAMP, r = NVN_WRAP_MODE_CLAMP;
    NVNcompareMode mode = NVN_COMPARE_MODE_NONE;
    NVNcompareFunc func = NVN_COMPARE_FUNC_NEVER;
    float low = -1.0f, high = -1.0f, got_bias, anisotropy;
    float border[4] = { -1.0f, -1.0f, -1.0f, -1.0f };
    NVNsamplerReduction reduction;
    if (sampler == NULL) {
        nvn.nvn.nvnSamplerBuilderGetMinMagFilter(builder, &min, &mag);
        nvn.nvn.nvnSamplerBuilderGetWrapMode(builder, &s, &t, &r);
        nvn.nvn.nvnSamplerBuilderGetCompare(builder, &mode, &func);
        nvn.nvn.nvnSamplerBuilderGetLodClamp(builder, &low, &high);
        nvn.nvn.nvnSamplerBuilderGetBorderColor(builder, border);
        reduction = nvn.nvn.nvnSamplerBuilderGetReductionFilter(builder);
        got_bias = nvn.nvn.nvnSamplerBuilderGetLodBias(builder);
        anisotropy = nvn.nvn.nvnSamplerBuilderGetMaxAnisotropy(builder);
    } else {
        nvn.nvn.nvnSamplerGetMinMagFilter(sampler, &min, &mag);
        nvn.nvn.nvnSamplerGetWrapMode(sampler, &s, &t, &r);
        nvn.nvn.nvnSamplerGetCompare(sampler, &mode, &func);
        nvn.nvn.nvnSamplerGetLodClamp(sampler, &low, &high);
        nvn.nvn.nvnSamplerGetBorderColor(sampler, border);
        reduction = nvn.nvn.nvnSamplerGetReductionFilter(sampler);
        got_bias = nvn.nvn.nvnSamplerGetLodBias(sampler);
        anisotropy = nvn.nvn.nvnSamplerGetMaxAnisotropy(sampler);
    }
    Gives(why, why_size, "the min filter", min, NVN_MIN_FILTER_LINEAR_MIPMAP_NEAREST);
    Gives(why, why_size, "the mag filter", mag, NVN_MAG_FILTER_LINEAR);
    Gives(why, why_size, "the wrap mode of s", s, NVN_WRAP_MODE_REPEAT);
    Gives(why, why_size, "the wrap mode of t", t, NVN_WRAP_MODE_MIRRORED_REPEAT);
    Gives(why, why_size, "the wrap mode of r", r, NVN_WRAP_MODE_CLAMP_TO_BORDER);
    Gives(why, why_size, "the compare mode", mode, NVN_COMPARE_MODE_COMPARE_R_TO_TEXTURE);
    Gives(why, why_size, "the compare function", func, NVN_COMPARE_FUNC_GEQUAL);
    Gives(why, why_size, "the reduction filter", reduction, NVN_SAMPLER_REDUCTION_MAX);
    Gives(why, why_size, "the bits of the least level (1.5)", Bits(low), Bits(1.5f));
    Gives(why, why_size, "the bits of the greatest level (6.25)", Bits(high), Bits(6.25f));
    Gives(why, why_size, "the bits of the level bias", Bits(got_bias), Bits(bias));
    Gives(why, why_size, "the bits of the greatest anisotropy (4)", Bits(anisotropy), Bits(4.0f));
    Gives(why, why_size, "the bits of the border colour's red (0.25)", Bits(border[0]),
          Bits(0.25f));
    Gives(why, why_size, "the bits of the border colour's green (0.5)", Bits(border[1]),
          Bits(0.5f));
    Gives(why, why_size, "the bits of the border colour's blue (0.75)", Bits(border[2]),
          Bits(0.75f));
    Gives(why, why_size, "the bits of the border colour's alpha (1)", Bits(border[3]), Bits(1.0f));
}

/// Every setter of a sampler builder and what its getters then say, the level snap and the
/// integer border colours among them, which only a builder gives back.
static void SamplerBuilderCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    _Alignas(8) NVNsamplerBuilder builder;
    SamplerBuilder(&builder, -0.75f);
    Gives(why, sizeof(why), "the builder's device",
          (long long)(uintptr_t)nvn.nvn.nvnSamplerBuilderGetDevice(&builder),
          (long long)(uintptr_t)&nvn.device);
    SamplerGives(why, sizeof(why), &builder, NULL, -0.75f);
    Gives(why, sizeof(why), "the bits of the builder's level snap (0.25)",
          Bits(nvn.nvn.nvnSamplerBuilderGetLodSnap(&builder)), Bits(0.25f));
    static const int signed_border[4] = { -1, 2, -3, 0x7fffffff };
    int signed_back[4] = { 0, 0, 0, 0 };
    nvn.nvn.nvnSamplerBuilderSetBorderColori(&builder, signed_border);
    nvn.nvn.nvnSamplerBuilderGetBorderColori(&builder, signed_back);
    static const uint32_t unsigned_border[4] = { 0xfffffffeu, 5u, 0x80000000u, 7u };
    uint32_t unsigned_back[4] = { 0, 0, 0, 0 };
    nvn.nvn.nvnSamplerBuilderSetBorderColorui(&builder, unsigned_border);
    nvn.nvn.nvnSamplerBuilderGetBorderColorui(&builder, unsigned_back);
    for (int i = 0; i < 4; ++i) {
        Gives(why, sizeof(why), "a channel of the signed border colour", signed_back[i],
              signed_border[i]);
        Gives(why, sizeof(why), "a channel of the unsigned border colour", unsigned_back[i],
              unsigned_border[i]);
    }
    CHECK_TRUE(SUITE, name, CHECK_EXACT, why[0] == '\0' && NvnLayerError()[0] == '\0', "%s",
               why[0] != '\0' ? why : NvnLayerError());
}

/// A sampler says what its builder had, the integer border colours as the words of the float
/// one; two samplers of one state compare equal, and one with another level bias does not.
static void SamplerCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    _Alignas(8) NVNsamplerBuilder builder;
    static _Alignas(8) NVNsampler sampler, same, other;
    SamplerBuilder(&builder, -0.75f);
    const bool made = nvn.nvn.nvnSamplerInitialize(&sampler, &builder) &&
                      nvn.nvn.nvnSamplerInitialize(&same, &builder);
    SamplerBuilder(&builder, 0.5f);
    if (!made || !nvn.nvn.nvnSamplerInitialize(&other, &builder)) {
        ReportCase(SUITE, name, CHECK_NONE, false, Why("nvnSamplerInitialize answered false"));
        return;
    }
    SamplerGives(why, sizeof(why), NULL, &sampler, -0.75f);
    int signed_back[4] = { 0, 0, 0, 0 };
    uint32_t unsigned_back[4] = { 0, 0, 0, 0 };
    nvn.nvn.nvnSamplerGetBorderColori(&sampler, signed_back);
    nvn.nvn.nvnSamplerGetBorderColorui(&sampler, unsigned_back);
    Gives(why, sizeof(why), "the border colour's red (0.25) read as a signed word", signed_back[0],
          Bits(0.25f));
    Gives(why, sizeof(why), "the border colour's alpha (1) read as an unsigned word",
          unsigned_back[3], Bits(1.0f));
    Gives(why, sizeof(why), "whether two samplers of one state are the same",
          nvn.nvn.nvnSamplerCompare(&sampler, &same), 1);
    Gives(why, sizeof(why), "whether samplers with level biases of -0.75 and 0.5 are the same",
          nvn.nvn.nvnSamplerCompare(&sampler, &other), 0);
    nvn.nvn.nvnSamplerFinalize(&sampler);
    nvn.nvn.nvnSamplerFinalize(&same);
    nvn.nvn.nvnSamplerFinalize(&other);
    CHECK_TRUE(SUITE, name, CHECK_EXACT, why[0] == '\0' && NvnLayerError()[0] == '\0', "%s",
               why[0] != '\0' ? why : NvnLayerError());
}

/// A texture pool and a sampler pool in the readback pool's memory: where each says it is and
/// how many descriptors it holds, and a sampler registered straight from its builder, whose
/// descriptor has to be the bytes a sampler made from that builder gets.
static void DescriptorPoolsCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    static _Alignas(8) NVNtexturePool textures;
    static _Alignas(8) NVNsamplerPool samplers;
    static _Alignas(8) NVNsampler sampler;
    const ptrdiff_t texture_offset = (ptrdiff_t)POOL_PAGE, sampler_offset = 3 * (ptrdiff_t)POOL_PAGE;
    const int texture_count = 96, sampler_count = 64;
    uint8_t *const bytes = nvn.nvn.nvnMemoryPoolMap(&nvn.read_pool);
    if (bytes == NULL) {
        ReportCase(SUITE, name, CHECK_NONE, false, Why("nvnMemoryPoolMap gave no address"));
        return;
    }
    memset(bytes, 0, READ_POOL_BYTES);
    const bool textures_up =
        nvn.nvn.nvnTexturePoolInitialize(&textures, &nvn.read_pool, texture_offset, texture_count);
    const bool samplers_up = textures_up && nvn.nvn.nvnSamplerPoolInitialize(
                                                &samplers, &nvn.read_pool, sampler_offset,
                                                sampler_count);
    _Alignas(8) NVNsamplerBuilder builder;
    SamplerBuilder(&builder, -0.75f);
    const bool sampler_up = samplers_up && nvn.nvn.nvnSamplerInitialize(&sampler, &builder);
    if (!sampler_up) {
        if (samplers_up) {
            nvn.nvn.nvnSamplerPoolFinalize(&samplers);
        }
        if (textures_up) {
            nvn.nvn.nvnTexturePoolFinalize(&textures);
        }
        ReportCase(SUITE, name, CHECK_NONE, false,
                   Why("a texture pool, a sampler pool or a sampler did not initialize"));
        return;
    }
    Gives(why, sizeof(why), "the texture pool's size", nvn.nvn.nvnTexturePoolGetSize(&textures),
          texture_count);
    Gives(why, sizeof(why), "the texture pool's memory pool",
          (long long)(uintptr_t)nvn.nvn.nvnTexturePoolGetMemoryPool(&textures),
          (long long)(uintptr_t)&nvn.read_pool);
    Gives(why, sizeof(why), "the texture pool's memory offset",
          (long long)nvn.nvn.nvnTexturePoolGetMemoryOffset(&textures), (long long)texture_offset);
    Gives(why, sizeof(why), "the sampler pool's size", nvn.nvn.nvnSamplerPoolGetSize(&samplers),
          sampler_count);
    Gives(why, sizeof(why), "the sampler pool's memory pool",
          (long long)(uintptr_t)nvn.nvn.nvnSamplerPoolGetMemoryPool(&samplers),
          (long long)(uintptr_t)&nvn.read_pool);
    Gives(why, sizeof(why), "the sampler pool's memory offset",
          (long long)nvn.nvn.nvnSamplerPoolGetMemoryOffset(&samplers), (long long)sampler_offset);
    // Descriptors 40 and 41, 32 bytes each: the sampler's, and the builder's with no sampler.
    nvn.nvn.nvnSamplerPoolRegisterSampler(&samplers, 40, &sampler);
    nvn.nvn.nvnSamplerPoolRegisterSamplerBuilder(&samplers, 41, &builder);
    const uint8_t *const from_sampler = bytes + sampler_offset + 40 * 32;
    const uint8_t *const from_builder = bytes + sampler_offset + 41 * 32;
    uint32_t differ = 32, set = 0;
    for (uint32_t i = 32; i-- > 0u;) {
        if (from_sampler[i] != from_builder[i]) {
            differ = i;
        }
        set += from_sampler[i] != 0u;
    }
    if (why[0] == '\0' && (differ != 32u || set == 0u)) {
        snprintf(why, sizeof(why), "the descriptor registered from the builder differs from the "
                 "sampler's at byte %u of 32 (32 is none): 0x%02x against 0x%02x; %u bytes of "
                 "the sampler's are not nought, where a registered sampler leaves some",
                 (unsigned)differ, differ < 32u ? from_builder[differ] : 0u,
                 differ < 32u ? from_sampler[differ] : 0u, (unsigned)set);
    }
    nvn.nvn.nvnSamplerFinalize(&sampler);
    nvn.nvn.nvnSamplerPoolFinalize(&samplers);
    nvn.nvn.nvnTexturePoolFinalize(&textures);
    CHECK_TRUE(SUITE, name, CHECK_EXACT, why[0] == '\0' && NvnLayerError()[0] == '\0', "%s",
               why[0] != '\0' ? why : NvnLayerError());
}

/// The cases on the suite's own device, from the lookup to the window.
static void LayerCases(void) {
    static const char *const RESOLVE = "the program's NVN functions resolve";
    static const char *const DEVICE = "the device initializes";
    static const char *const VERSION = "the device answers the layer's API version";
    static const char *const TARGET = "a queue, its pools and a 64x64 target come up";
    static const char *const RED = "clear to opaque red";
    static const char *const MID = "clear to a mid channel value";
    static const char *const WINDOW = "a window acquires and presents both its textures";
    static const char *const COPIES = "two copy textures and a staging pool come up";
    static const char *const ROUND_TRIP = "a picture copied into a texture and back keeps every texel";
    static const char *const TEXTURE = "a texture copied into another keeps every texel";
    static const char *const BUFFER = "a buffer copied into another keeps every byte";
    static const char *const MIRROR_Y = "a texture copied to a buffer with MIRROR_Y lands upside down";
    static const char *const MIRROR_X = "a texture copied into another with MIRROR_X lands mirrored";
    static const char *const STRIDED = "a copy to a buffer with a row stride of 288 keeps the bytes "
                                       "between rows";
    static const char *const BUILDER = "a texture builder gives back each number it was set";
    static const char *const VIEW = "a texture view says which of its parts were set, and with "
                                    "what";
    static const char *const LEVELS_UP = "a texture of three levels comes up and says what its "
                                         "builder had";
    static const char *const LEVELS_COPY = "a picture copied into each level of a texture "
                                           "through a view comes back from that level";
    static const char *const POOL = "a memory pool and its builder give back what they were "
                                    "set, and the pool maps its storage";
    static const char *const POOL_CACHE = "a page flushed from a cached pool is what a copy "
                                          "reads, and the copy is there after an invalidate";
    static const char *const FENCED = "a sync fenced on the queue behind a clear signals, and "
                                      "the clear is there with no finish";
    static const char *const FENCED_LIST = "a sync fenced inside the list behind a clear "
                                           "signals, and the clear is there with no finish";
    static const char *const WAITED = "a queue and a list told to wait for a sync that "
                                      "signalled run their clear";
    static const char *const UNFENCED = "a wait on a sync nothing fenced fails, with a timeout "
                                        "or without";
    static const char *const QUEUE_ERROR = "the queue has no error after work that was valid";
    static const char *const QUEUE_BUILDER = "a queue builder gives back its flags and says "
                                             "which of its sizes were set";
    static const char *const EVENT = "an event keeps what the processor signals: a write, an "
                                     "add and an add that wraps";
    static const char *const SAMPLER_BUILDER = "a sampler builder gives back each number it "
                                               "was set";
    static const char *const SAMPLER = "a sampler says what its builder had, and is the same "
                                       "as another of that state alone";
    static const char *const DESCRIPTOR_POOLS = "the texture and sampler pools say where they "
                                                "are, and a builder registers as its sampler";
    const char *const AFTER_DEVICE[] = { VERSION,  TARGET,   RED,      MID,     COPIES,
                                         ROUND_TRIP, TEXTURE, BUFFER, MIRROR_Y, MIRROR_X,
                                         STRIDED,  BUILDER, VIEW, LEVELS_UP, LEVELS_COPY,
                                         POOL, POOL_CACHE, FENCED, FENCED_LIST, WAITED,
                                         UNFENCED, QUEUE_ERROR, QUEUE_BUILDER, EVENT,
                                         SAMPLER_BUILDER, SAMPLER, DESCRIPTOR_POOLS, WINDOW };
    const size_t after_count = sizeof(AFTER_DEVICE) / sizeof(AFTER_DEVICE[0]);

    memset(&nvn, 0, sizeof(nvn));
#if NVN_LAYER_API < 231
    // deko3d ends the program when it cannot make an object; under gpu.c's guard a bring-up
    // that fails in the layer is a false result with its reason, which a case reports.
    NvnLayerSetGuard(AbortGuarded);
#endif
    NvnLayerClearError();
    if (!ReportStart(SUITE, RESOLVE)) {
        return;
    }
    char missing[64] = "";
    const bool resolved = NvnLoadProcs(&nvn.nvn, missing, sizeof(missing));
    CHECK_TRUE(SUITE, RESOLVE, CHECK_NONE, resolved, "the lookup gave no %s", missing);
    if (!resolved) {
        ReportSkip(SUITE, DEVICE, REPORT_REASON_DEPENDS,
                   "the program's NVN functions did not resolve");
        for (size_t i = 0; i < after_count; ++i) {
            ReportSkip(SUITE, AFTER_DEVICE[i], REPORT_REASON_DEPENDS,
                       "the program's NVN functions did not resolve");
        }
        return;
    }

    if (!ReportStart(SUITE, DEVICE)) {
        return;
    }
    _Alignas(8) NVNdeviceBuilder device_builder;
    nvn.nvn.nvnDeviceBuilderSetDefaults(&device_builder);
    nvn.nvn.nvnDeviceBuilderSetFlags(&device_builder, 0);
    nvn.device_up = nvn.nvn.nvnDeviceInitialize(&nvn.device, &device_builder);
    CHECK_TRUE(SUITE, DEVICE, CHECK_NONE, nvn.device_up, "nvnDeviceInitialize: %s",
               Why("false"));
    if (!nvn.device_up) {
        for (size_t i = 0; i < after_count; ++i) {
            ReportSkip(SUITE, AFTER_DEVICE[i], REPORT_REASON_DEPENDS,
                       "the device did not initialize");
        }
        return;
    }

    // Read from the layer's own code under ScarletNX, as a game's driver's version is: its own
    // table answers 55.15, so the layer's 55.8 coming back says the program's code was read.
    if (ReportStart(SUITE, VERSION)) {
        int major = -1;
        int minor = -1;
        nvn.nvn.nvnDeviceGetInteger(&nvn.device, NVN_DEVICE_INFO_API_MAJOR_VERSION, &major);
        nvn.nvn.nvnDeviceGetInteger(&nvn.device, NVN_DEVICE_INFO_API_MINOR_VERSION, &minor);
        CHECK_TRUE(SUITE, VERSION, CHECK_EXACT,
                   major == NVN_LAYER_API_MAJOR && minor == NVN_LAYER_API_MINOR,
                   "API %d.%d, where the layer answers %d.%d", major, minor, NVN_LAYER_API_MAJOR,
                   NVN_LAYER_API_MINOR);
    }

    if (!ReportStart(SUITE, TARGET)) {
        TearDown();
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    const bool up = BringUp(why, sizeof(why));
    CHECK_TRUE(SUITE, TARGET, CHECK_NONE, up, "%s", why);
    if (!up) {
        for (size_t i = 2; i < after_count; ++i) {
            ReportSkip(SUITE, AFTER_DEVICE[i], REPORT_REASON_DEPENDS,
                       "the queue, its pools or the target did not come up");
        }
        TearDown();
        return;
    }

    // The clear suite's colours, whose bytes the conversion to eight bits cannot round two ways.
    static const float red[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
    static const uint8_t red_bytes[4] = { 0xff, 0x00, 0x00, 0xff };
    static const float mid[4] = { 128.0f / 255.0f, 0.0f, 0.0f, 1.0f };
    static const uint8_t mid_bytes[4] = { 0x80, 0x00, 0x00, 0xff };
    ClearCase(RED, red, red_bytes);
    ClearCase(MID, mid, mid_bytes);

    if (ReportStart(SUITE, COPIES)) {
        NvnLayerClearError();
        const bool copies = BringCopies(why, sizeof(why));
        CHECK_TRUE(SUITE, COPIES, CHECK_NONE, copies, "%s", why);
        if (copies) {
            CopyCase(ROUND_TRIP, COPY_ROUND_TRIP);
            CopyCase(TEXTURE, COPY_TEXTURE);
            CopyCase(BUFFER, COPY_BUFFER);
            CopyCase(MIRROR_Y, COPY_MIRROR_Y);
            CopyCase(MIRROR_X, COPY_MIRROR_X);
            CopyCase(STRIDED, COPY_STRIDE);
        } else {
            const char *const after[] = { ROUND_TRIP, TEXTURE, BUFFER, MIRROR_Y, MIRROR_X, STRIDED };
            for (size_t i = 0; i < sizeof(after) / sizeof(after[0]); ++i) {
                ReportSkip(SUITE, after[i], REPORT_REASON_DEPENDS,
                           "the copy textures or the staging pool did not come up");
            }
        }
    }

    TextureBuilderCase(BUILDER);
    TextureViewCase(VIEW);
    if (ReportStart(SUITE, LEVELS_UP)) {
        NvnLayerClearError();
        const bool levels = BringLevels(why, sizeof(why));
        CHECK_TRUE(SUITE, LEVELS_UP, CHECK_EXACT, levels, "%s", why);
        if (!levels || !nvn.stage_pool_up) {
            ReportSkip(SUITE, LEVELS_COPY, REPORT_REASON_DEPENDS,
                       "the texture of three levels or the staging pool did not come up");
        } else {
            LevelsCase(LEVELS_COPY);
        }
    }

    PoolCase(POOL);
    if (!nvn.cached_pool_up) {
        ReportSkip(SUITE, POOL_CACHE, REPORT_REASON_DEPENDS,
                   "the pool cached to the processor did not come up");
    } else {
        PoolCacheCase(POOL_CACHE);
    }

    // Colours no case before these left in the target or in the readback.
    static const float green[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
    static const uint8_t green_bytes[4] = { 0x00, 0xff, 0x00, 0xff };
    static const float blue[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
    static const uint8_t blue_bytes[4] = { 0x00, 0x00, 0xff, 0xff };
    static const float yellow[4] = { 1.0f, 1.0f, 0.0f, 1.0f };
    static const uint8_t yellow_bytes[4] = { 0xff, 0xff, 0x00, 0xff };
    FenceCase(FENCED, FENCE_QUEUE, green, green_bytes);
    FenceCase(FENCED_LIST, FENCE_LIST, blue, blue_bytes);
    FenceCase(WAITED, FENCE_WAITED, yellow, yellow_bytes);
    UnfencedCase(UNFENCED);
    QueueErrorCase(QUEUE_ERROR);
    QueueBuilderCase(QUEUE_BUILDER);
    EventCase(EVENT);
    SamplerBuilderCase(SAMPLER_BUILDER);
    SamplerCase(SAMPLER);
    DescriptorPoolsCase(DESCRIPTOR_POOLS);

#if NVN_LAYER_API < 231
    const bool screen_up = ViewerScreenUp();
#else
    const bool screen_up = false;    // the Switch 2's frontend has no screen of its own
#endif
    if (screen_up) {
        // The program's own screen holds the default window, and a swapchain made on it after
        // the screen is what Eden never came back from (the game suite says the same). The start
        // screen leaves it up to the run's end, noprogress or not; a quick run from the mode file
        // never opens it.
        ReportSkip(SUITE, WINDOW, REPORT_REASON_UNAVAILABLE,
                   "the program's screen holds the window: a run begun on the start screen keeps "
                   "it up to its end, noprogress or not; quick in the mode file runs this case");
    } else if (ReportStart(SUITE, WINDOW)) {
        NvnLayerClearError();
        const bool presented = Present(why, sizeof(why));
        CHECK_TRUE(SUITE, WINDOW, CHECK_NONE, presented, "%s", why);
    }
    TearDown();
}

void RunNvn(void) {
#if NVN_LAYER_API < 231
    // Before the first lookup: a runtime that runs a game's NVN itself takes each function at
    // the address its name resolves to, the layer's own from then on with it.
    NvnSceneFirst();
    LayerCases();
    // With the suite's own device down, whatever the cases above came to: the scene's way
    // makes its own.
    NvnSceneLast();
#else
    // The scene is a Switch program's: its shaders are the Switch's code and two of its three
    // ways are deko3d's.
    LayerCases();
#endif
}
