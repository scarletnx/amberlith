// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
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
#ifndef NVN_LAYER_GAME_DRIVER
#include "../gpu.h"
#include "../viewer.h"
#else
// A program loaded into a game (the layer's build for a game's own driver) has none of them
// either: no deko3d whose abort a guard would turn, and no screen of its own.
static bool AbortGuarded(void (*call)(void), uint32_t *result) {
    (void)result;
    call();
    return true;
}

static bool ViewerScreenUp(void) {
    return false;
}
#endif
#endif

/// The native window the window cases present to, or NULL with why there is none in @p why: the
/// program's own default one, or in a game's process the one the mod made on a layer of its own
/// through the game's nn::vi (NvnGameWindow), which the mode file's nowindow keeps closed.
static NVNnativeWindow NativeWindowOr(const char **why) {
#ifdef NVN_LAYER_GAME_DRIVER
    return (NVNnativeWindow)NvnGameWindow(why);
#else
    *why = NULL;
    return (NVNnativeWindow)nwindowGetDefault();
#endif
}

/// The native window a window builder is given. With none, an address a builder only keeps and
/// gives back: no window is made of it.
static NVNnativeWindow NativeWindow(void) {
    const char *why = NULL;
    const NVNnativeWindow window = NativeWindowOr(&why);
    static char none;
    return window != NULL ? window : (NVNnativeWindow)&none;
}

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

/// The list of the last ClearAndRead, which has run.
static NVNcommandHandle cleared_list;

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
    cleared_list = list;
    if (NvnLayerError()[0] != '\0') {
        snprintf(why, why_size, "%s", NvnLayerError());
        return NULL;
    }
    return pixels;
}

/// Every pixel of @p pixels against @p expected: how many differ.
static uint32_t ClearWrong(const uint8_t *pixels, const uint8_t expected[4]) {
    uint32_t wrong = 0;
    for (uint32_t i = 0; i < TARGET_SIZE * TARGET_SIZE; ++i) {
        wrong += memcmp(pixels + (size_t)i * 4u, expected, 4) != 0 ? 1u : 0u;
    }
    return wrong;
}

/// Three calls a device takes and shows nothing of, which a release driver returns from at
/// once: deferred finalizes applied, the intermediate shader cache switched, and the handle of
/// a list that ran given up. A clear before them is there, and so is one after.
static void DeviceQuietCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    static const float before[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
    static const uint8_t before_bytes[4] = { 0x00, 0xff, 0x00, 0xff };
    static const float after[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
    static const uint8_t after_bytes[4] = { 0x00, 0x00, 0xff, 0xff };
    char why[256] = "";
    const uint8_t *pixels = ClearAndRead(before, why, sizeof(why));
    if (pixels == NULL) {
        ReportCase(SUITE, name, CHECK_NONE, false, why);
        return;
    }
    const uint32_t wrong_before = ClearWrong(pixels, before_bytes);
    nvn.nvn.nvnDeviceFinalizeCommandHandle(&nvn.device, cleared_list);
    nvn.nvn.nvnDeviceApplyDeferredFinalizes(&nvn.device, 0);
    nvn.nvn.nvnDeviceSetIntermediateShaderCache(&nvn.device, 1);
    nvn.nvn.nvnDeviceSetIntermediateShaderCache(&nvn.device, 0);
    pixels = ClearAndRead(after, why, sizeof(why));
    if (pixels == NULL) {
        ReportCase(SUITE, name, CHECK_NONE, false, why);
        return;
    }
    const uint32_t wrong_after = ClearWrong(pixels, after_bytes);
    CHECK_TRUE(SUITE, name, CHECK_EXACT, wrong_before == 0u && wrong_after == 0u,
               "%u of %u pixels are not the green cleared before the three calls, and %u are "
               "not the blue cleared after them",
               (unsigned)wrong_before, (unsigned)(TARGET_SIZE * TARGET_SIZE),
               (unsigned)wrong_after);
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

/// NVN's two calls between its syncs and OpenGL's, in a program that has no OpenGL: both fail,
/// and neither writes what it would have made.
static void NoOpenGlCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    static _Alignas(8) NVNsync from_opengl;
    int status = -1;
    const NVNboolean made =
        nvn.nvn.nvnSyncInitializeFromFencedGLSync(&from_opengl, &nvn.device, 1u, &status);
    Gives(why, sizeof(why), "whether a sync was made from OpenGL sync 1", made, 0);
    Gives(why, sizeof(why), "the status of making it", status, 1);
    static _Alignas(8) NVNsync sync;
    if (!nvn.nvn.nvnSyncInitialize(&sync, &nvn.device)) {
        ReportCase(SUITE, name, CHECK_NONE, false, Why("nvnSyncInitialize answered false"));
        return;
    }
    uint64_t opengl = 0x5a5a5a5a5a5a5a5aull;
    const int created = nvn.nvn.nvnSyncCreateGLSync(&sync, &opengl);
    nvn.nvn.nvnSyncFinalize(&sync);
    Gives(why, sizeof(why), "the result of making an OpenGL sync from a sync", created, 4);
    Gives(why, sizeof(why), "whether the OpenGL sync's place kept what it held",
          opengl == 0x5a5a5a5a5a5a5a5aull, 1);
    CHECK_TRUE(SUITE, name, CHECK_EXACT, why[0] == '\0' && NvnLayerError()[0] == '\0', "%s",
               why[0] != '\0' ? why : NvnLayerError());
}

/// A pool is of storage or of a native handle, the one its builder was given last: a handle
/// takes the storage away, and storage is taken again after it. No pool is made of the handle,
/// which only the system's memory manager gives.
static void NativeHandleCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    static uint8_t memory[16];
    _Alignas(8) NVNmemoryPoolBuilder builder;
    memset(&builder, 0, sizeof(builder));
    nvn.nvn.nvnMemoryPoolBuilderSetDefaults(&builder);
    nvn.nvn.nvnMemoryPoolBuilderSetDevice(&builder, &nvn.device);
    nvn.nvn.nvnMemoryPoolBuilderSetStorage(&builder, memory, 0x3000);
    Gives(why, sizeof(why), "the builder's size with storage",
          (long long)nvn.nvn.nvnMemoryPoolBuilderGetSize(&builder), 0x3000);
    nvn.nvn.nvnMemoryPoolBuilderSetNativeHandleNVX(&builder, 7);
    Gives(why, sizeof(why), "the builder's memory once it has a native handle",
          (long long)(uintptr_t)nvn.nvn.nvnMemoryPoolBuilderGetMemory(&builder), 0);
    Gives(why, sizeof(why), "the builder's size once it has a native handle",
          (long long)nvn.nvn.nvnMemoryPoolBuilderGetSize(&builder), 0);
    nvn.nvn.nvnMemoryPoolBuilderSetStorage(&builder, memory, 0x5000);
    Gives(why, sizeof(why), "the builder's memory given storage again",
          (long long)(uintptr_t)nvn.nvn.nvnMemoryPoolBuilderGetMemory(&builder),
          (long long)(uintptr_t)memory);
    Gives(why, sizeof(why), "the builder's size given storage again",
          (long long)nvn.nvn.nvnMemoryPoolBuilderGetSize(&builder), 0x5000);
    CHECK_TRUE(SUITE, name, CHECK_EXACT, why[0] == '\0' && NvnLayerError()[0] == '\0', "%s",
               why[0] != '\0' ? why : NvnLayerError());
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

// ---- buffers

// A game's vertices, indices and uniforms are in buffers: a range of a pool, with the range's
// own address and mapping. The two here are in the cached pool of the pool cases, one written
// through its mapping and one a copy by the graphics unit fills.

#define BUFFER_BYTES 0x800u
#define BUFFER_FROM_AT 0x400
#define BUFFER_TO_AT 0x1400

/// A buffer of BUFFER_BYTES at @p offset of the cached pool.
static bool MakeBuffer(NVNbuffer *buffer, NVNbufferBuilder *builder, ptrdiff_t offset) {
    memset(builder, 0, sizeof(*builder));
    nvn.nvn.nvnBufferBuilderSetDevice(builder, &nvn.device);
    nvn.nvn.nvnBufferBuilderSetDefaults(builder);
    nvn.nvn.nvnBufferBuilderSetStorage(builder, &nvn.cached_pool, offset, BUFFER_BYTES);
    return nvn.nvn.nvnBufferInitialize(buffer, builder);
}

/// What a buffer and its builder say, and that its address and its mapping are the pool's,
/// moved by the buffer's offset.
static void BufferCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    _Alignas(8) NVNbufferBuilder builder;
    static _Alignas(8) NVNbuffer buffer;
    if (!MakeBuffer(&buffer, &builder, BUFFER_FROM_AT)) {
        ReportCase(SUITE, name, CHECK_NONE, false, Why("nvnBufferInitialize answered false"));
        return;
    }
    Gives(why, sizeof(why), "the builder's device",
          (long long)(uintptr_t)nvn.nvn.nvnBufferBuilderGetDevice(&builder),
          (long long)(uintptr_t)&nvn.device);
    Gives(why, sizeof(why), "the builder's memory pool",
          (long long)(uintptr_t)nvn.nvn.nvnBufferBuilderGetMemoryPool(&builder),
          (long long)(uintptr_t)&nvn.cached_pool);
    Gives(why, sizeof(why), "the builder's memory offset",
          (long long)nvn.nvn.nvnBufferBuilderGetMemoryOffset(&builder), BUFFER_FROM_AT);
    Gives(why, sizeof(why), "the builder's size",
          (long long)nvn.nvn.nvnBufferBuilderGetSize(&builder), BUFFER_BYTES);
    Gives(why, sizeof(why), "the buffer's memory pool",
          (long long)(uintptr_t)nvn.nvn.nvnBufferGetMemoryPool(&buffer),
          (long long)(uintptr_t)&nvn.cached_pool);
    Gives(why, sizeof(why), "the buffer's memory offset",
          (long long)nvn.nvn.nvnBufferGetMemoryOffset(&buffer), BUFFER_FROM_AT);
    Gives(why, sizeof(why), "the buffer's size", (long long)nvn.nvn.nvnBufferGetSize(&buffer),
          BUFFER_BYTES);
    Gives(why, sizeof(why), "the buffer's address, its pool's and the offset",
          (long long)nvn.nvn.nvnBufferGetAddress(&buffer),
          (long long)(nvn.nvn.nvnMemoryPoolGetBufferAddress(&nvn.cached_pool) + BUFFER_FROM_AT));
    Gives(why, sizeof(why), "the buffer's mapping, its pool's and the offset",
          (long long)(uintptr_t)nvn.nvn.nvnBufferMap(&buffer),
          (long long)((uintptr_t)nvn.nvn.nvnMemoryPoolMap(&nvn.cached_pool) + BUFFER_FROM_AT));
    nvn.nvn.nvnBufferFinalize(&buffer);
    CHECK_TRUE(SUITE, name, CHECK_EXACT, why[0] == '\0' && NvnLayerError()[0] == '\0', "%s",
               why[0] != '\0' ? why : NvnLayerError());
}

/// Bytes written through one buffer's mapping and flushed, copied by the graphics unit to the
/// other's address, and read through that one's mapping after an invalidate. The whole pool is
/// judged: the bytes around each buffer keep the preset.
static void BufferCopyCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    _Alignas(8) NVNbufferBuilder builder;
    static _Alignas(8) NVNbuffer from, to;
    uint8_t *const pool = nvn.nvn.nvnMemoryPoolMap(&nvn.cached_pool);
    const bool from_up = pool != NULL && MakeBuffer(&from, &builder, BUFFER_FROM_AT);
    const bool to_up = from_up && MakeBuffer(&to, &builder, BUFFER_TO_AT);
    if (!to_up) {
        if (from_up) {
            nvn.nvn.nvnBufferFinalize(&from);
        }
        ReportCase(SUITE, name, CHECK_NONE, false,
                   Why("nvnMemoryPoolMap gave no address, or nvnBufferInitialize answered false"));
        return;
    }
    memset(pool, READ_PRESET, CACHED_BYTES);
    nvn.nvn.nvnMemoryPoolFlushMappedRange(&nvn.cached_pool, 0, CACHED_BYTES);
    uint8_t *const written = nvn.nvn.nvnBufferMap(&from);
    for (uint32_t at = 0; written != NULL && at < BUFFER_BYTES; ++at) {
        written[at] = CachedByte(at);
    }
    nvn.nvn.nvnBufferFlushMappedRange(&from, 0, BUFFER_BYTES);
    nvn.nvn.nvnCommandBufferBeginRecording(&nvn.command);
    nvn.nvn.nvnCommandBufferCopyBufferToBuffer(&nvn.command, nvn.nvn.nvnBufferGetAddress(&from),
                                               nvn.nvn.nvnBufferGetAddress(&to), BUFFER_BYTES,
                                               NVN_COPY_FLAGS_NONE);
    const NVNcommandHandle list = nvn.nvn.nvnCommandBufferEndRecording(&nvn.command);
    nvn.nvn.nvnQueueSubmitCommands(&nvn.queue, 1, &list);
    nvn.nvn.nvnQueueFinish(&nvn.queue);
    nvn.nvn.nvnBufferInvalidateMappedRange(&to, 0, BUFFER_BYTES);
    const uint8_t *const read = nvn.nvn.nvnBufferMap(&to);
    uint32_t wrong = 0, first = 0;
    uint8_t want_first = 0;
    for (uint32_t at = 0; at < CACHED_BYTES; ++at) {
        uint8_t want = READ_PRESET;
        if (at >= BUFFER_FROM_AT && at < BUFFER_FROM_AT + BUFFER_BYTES) {
            want = CachedByte(at - BUFFER_FROM_AT);
        } else if (at >= BUFFER_TO_AT && at < BUFFER_TO_AT + BUFFER_BYTES) {
            want = CachedByte(at - BUFFER_TO_AT);
        }
        if (pool[at] != want && wrong++ == 0u) {
            first = at;
            want_first = want;
        }
    }
    const bool mapped = written == pool + BUFFER_FROM_AT && read == pool + BUFFER_TO_AT;
    nvn.nvn.nvnBufferFinalize(&from);
    nvn.nvn.nvnBufferFinalize(&to);
    if (NvnLayerError()[0] != '\0' || list == 0u) {
        ReportCase(SUITE, name, CHECK_NONE, false, Why("nvnCommandBufferEndRecording gave no list"));
        return;
    }
    CHECK_TRUE(SUITE, name, CHECK_EXACT, mapped && wrong == 0u,
               "the buffers' mappings are %s the pool's at their offsets; %u of %u bytes of the "
               "pool wrong, the first, byte 0x%x (the written buffer is 0x%x to 0x%x, the "
               "copied one 0x%x to 0x%x), is 0x%02x, expected 0x%02x (0x%02x is a byte nothing "
               "wrote)",
               mapped ? "" : "not ", (unsigned)wrong, (unsigned)CACHED_BYTES, (unsigned)first,
               (unsigned)BUFFER_FROM_AT, (unsigned)(BUFFER_FROM_AT + BUFFER_BYTES),
               (unsigned)BUFFER_TO_AT, (unsigned)(BUFFER_TO_AT + BUFFER_BYTES), pool[first],
               want_first, READ_PRESET);
}

// ---- what the state objects answer of themselves

// A state object is a few bytes a game fills with setters and binds. Each case here sets every
// field of one kind of state to a number that is no default and reads it back; no state here is
// bound, so the numbers need not be a state a draw could use.

static void BlendStateCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    _Alignas(8) NVNblendState state;
    nvn.nvn.nvnBlendStateSetDefaults(&state);
    nvn.nvn.nvnBlendStateSetBlendTarget(&state, 5);
    nvn.nvn.nvnBlendStateSetBlendFunc(&state, NVN_BLEND_FUNC_SRC_ALPHA,
                                      NVN_BLEND_FUNC_ONE_MINUS_SRC_ALPHA,
                                      NVN_BLEND_FUNC_ONE_MINUS_DST_ALPHA,
                                      NVN_BLEND_FUNC_CONSTANT_ALPHA);
    nvn.nvn.nvnBlendStateSetBlendEquation(&state, NVN_BLEND_EQUATION_REVERSE_SUB,
                                          NVN_BLEND_EQUATION_MAX);
    nvn.nvn.nvnBlendStateSetAdvancedMode(&state, NVN_BLEND_ADVANCED_MODE_BLUE);
    nvn.nvn.nvnBlendStateSetAdvancedOverlap(&state, NVN_BLEND_ADVANCED_OVERLAP_CONJOINT);
    nvn.nvn.nvnBlendStateSetAdvancedPremultipliedSrc(&state, 0);
    nvn.nvn.nvnBlendStateSetAdvancedNormalizedDst(&state, 0);
    NVNblendFunc src_rgb = NVN_BLEND_FUNC_ZERO, dst_rgb = NVN_BLEND_FUNC_ZERO;
    NVNblendFunc src_alpha = NVN_BLEND_FUNC_ZERO, dst_alpha = NVN_BLEND_FUNC_ZERO;
    NVNblendEquation rgb = NVN_BLEND_EQUATION_ADD, alpha = NVN_BLEND_EQUATION_ADD;
    nvn.nvn.nvnBlendStateGetBlendFunc(&state, &src_rgb, &dst_rgb, &src_alpha, &dst_alpha);
    nvn.nvn.nvnBlendStateGetBlendEquation(&state, &rgb, &alpha);
    Gives(why, sizeof(why), "the blend target", nvn.nvn.nvnBlendStateGetBlendTarget(&state), 5);
    Gives(why, sizeof(why), "the source colour function", src_rgb, NVN_BLEND_FUNC_SRC_ALPHA);
    Gives(why, sizeof(why), "the destination colour function", dst_rgb,
          NVN_BLEND_FUNC_ONE_MINUS_SRC_ALPHA);
    Gives(why, sizeof(why), "the source alpha function", src_alpha,
          NVN_BLEND_FUNC_ONE_MINUS_DST_ALPHA);
    Gives(why, sizeof(why), "the destination alpha function", dst_alpha,
          NVN_BLEND_FUNC_CONSTANT_ALPHA);
    Gives(why, sizeof(why), "the colour equation", rgb, NVN_BLEND_EQUATION_REVERSE_SUB);
    Gives(why, sizeof(why), "the alpha equation", alpha, NVN_BLEND_EQUATION_MAX);
    Gives(why, sizeof(why), "the advanced mode", nvn.nvn.nvnBlendStateGetAdvancedMode(&state),
          NVN_BLEND_ADVANCED_MODE_BLUE);
    Gives(why, sizeof(why), "the advanced overlap",
          nvn.nvn.nvnBlendStateGetAdvancedOverlap(&state), NVN_BLEND_ADVANCED_OVERLAP_CONJOINT);
    Gives(why, sizeof(why), "whether the advanced mode's source is premultiplied",
          nvn.nvn.nvnBlendStateGetAdvancedPremultipliedSrc(&state), 0);
    Gives(why, sizeof(why), "whether the advanced mode's destination is normalized",
          nvn.nvn.nvnBlendStateGetAdvancedNormalizedDst(&state), 0);
    nvn.nvn.nvnBlendStateSetAdvancedPremultipliedSrc(&state, 1);
    Gives(why, sizeof(why), "whether the source is premultiplied once it is set so",
          nvn.nvn.nvnBlendStateGetAdvancedPremultipliedSrc(&state), 1);
    CHECK_TRUE(SUITE, name, CHECK_EXACT, why[0] == '\0' && NvnLayerError()[0] == '\0', "%s",
               why[0] != '\0' ? why : NvnLayerError());
}

/// The channel mask of one target and the colour state: a target's blending, the logic
/// operation and the alpha test.
static void ColorStateCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    _Alignas(8) NVNchannelMaskState mask;
    _Alignas(8) NVNcolorState color;
    nvn.nvn.nvnChannelMaskStateSetDefaults(&mask);
    nvn.nvn.nvnChannelMaskStateSetChannelMask(&mask, 3, 1, 0, 1, 0);
    NVNboolean r = 0, g = 1, b = 0, a = 1;
    nvn.nvn.nvnChannelMaskStateGetChannelMask(&mask, 3, &r, &g, &b, &a);
    Gives(why, sizeof(why), "target 3's channels written, as red 1, green 2, blue 4, alpha 8",
          (r ? 1 : 0) | (g ? 2 : 0) | (b ? 4 : 0) | (a ? 8 : 0), 5);
    nvn.nvn.nvnChannelMaskStateGetChannelMask(&mask, 2, &r, &g, &b, &a);
    Gives(why, sizeof(why), "target 2's channels written, which nothing changed",
          (r ? 1 : 0) | (g ? 2 : 0) | (b ? 4 : 0) | (a ? 8 : 0), 15);

    nvn.nvn.nvnColorStateSetDefaults(&color);
    nvn.nvn.nvnColorStateSetBlendEnable(&color, 5, 1);
    nvn.nvn.nvnColorStateSetLogicOp(&color, NVN_LOGIC_OP_XOR);
    nvn.nvn.nvnColorStateSetAlphaTest(&color, NVN_ALPHA_FUNC_GREATER);
    Gives(why, sizeof(why), "whether target 5 blends", nvn.nvn.nvnColorStateGetBlendEnable(&color, 5),
          1);
    Gives(why, sizeof(why), "whether target 4 blends, which nothing enabled",
          nvn.nvn.nvnColorStateGetBlendEnable(&color, 4), 0);
    Gives(why, sizeof(why), "the logic operation", nvn.nvn.nvnColorStateGetLogicOp(&color),
          NVN_LOGIC_OP_XOR);
    Gives(why, sizeof(why), "the alpha test's function", nvn.nvn.nvnColorStateGetAlphaTest(&color),
          NVN_ALPHA_FUNC_GREATER);
    nvn.nvn.nvnColorStateSetBlendEnable(&color, 5, 0);
    Gives(why, sizeof(why), "whether target 5 blends once it is disabled",
          nvn.nvn.nvnColorStateGetBlendEnable(&color, 5), 0);
    CHECK_TRUE(SUITE, name, CHECK_EXACT, why[0] == '\0' && NvnLayerError()[0] == '\0', "%s",
               why[0] != '\0' ? why : NvnLayerError());
}

static void DepthStencilStateCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    _Alignas(8) NVNdepthStencilState state;
    nvn.nvn.nvnDepthStencilStateSetDefaults(&state);
    nvn.nvn.nvnDepthStencilStateSetDepthTestEnable(&state, 1);
    nvn.nvn.nvnDepthStencilStateSetDepthWriteEnable(&state, 0);
    nvn.nvn.nvnDepthStencilStateSetDepthFunc(&state, NVN_DEPTH_FUNC_GEQUAL);
    nvn.nvn.nvnDepthStencilStateSetStencilTestEnable(&state, 1);
    nvn.nvn.nvnDepthStencilStateSetStencilFunc(&state, NVN_FACE_FRONT, NVN_STENCIL_FUNC_EQUAL);
    nvn.nvn.nvnDepthStencilStateSetStencilFunc(&state, NVN_FACE_BACK, NVN_STENCIL_FUNC_NOTEQUAL);
    nvn.nvn.nvnDepthStencilStateSetStencilOp(&state, NVN_FACE_FRONT, NVN_STENCIL_OP_REPLACE,
                                             NVN_STENCIL_OP_INCR, NVN_STENCIL_OP_DECR_WRAP);
    nvn.nvn.nvnDepthStencilStateSetStencilOp(&state, NVN_FACE_BACK, NVN_STENCIL_OP_INVERT,
                                             NVN_STENCIL_OP_ZERO, NVN_STENCIL_OP_INCR_WRAP);
    Gives(why, sizeof(why), "whether depth is tested",
          nvn.nvn.nvnDepthStencilStateGetDepthTestEnable(&state), 1);
    Gives(why, sizeof(why), "whether depth is written",
          nvn.nvn.nvnDepthStencilStateGetDepthWriteEnable(&state), 0);
    Gives(why, sizeof(why), "the depth function", nvn.nvn.nvnDepthStencilStateGetDepthFunc(&state),
          NVN_DEPTH_FUNC_GEQUAL);
    Gives(why, sizeof(why), "whether stencil is tested",
          nvn.nvn.nvnDepthStencilStateGetStencilTestEnable(&state), 1);
    Gives(why, sizeof(why), "the front stencil function",
          nvn.nvn.nvnDepthStencilStateGetStencilFunc(&state, NVN_FACE_FRONT),
          NVN_STENCIL_FUNC_EQUAL);
    Gives(why, sizeof(why), "the back stencil function",
          nvn.nvn.nvnDepthStencilStateGetStencilFunc(&state, NVN_FACE_BACK),
          NVN_STENCIL_FUNC_NOTEQUAL);
    NVNstencilOp fail = NVN_STENCIL_OP_KEEP, depth_fail = NVN_STENCIL_OP_KEEP;
    NVNstencilOp pass = NVN_STENCIL_OP_KEEP;
    nvn.nvn.nvnDepthStencilStateGetStencilOp(&state, NVN_FACE_FRONT, &fail, &depth_fail, &pass);
    Gives(why, sizeof(why), "the front operation for a failed stencil test", fail,
          NVN_STENCIL_OP_REPLACE);
    Gives(why, sizeof(why), "the front operation for a failed depth test", depth_fail,
          NVN_STENCIL_OP_INCR);
    Gives(why, sizeof(why), "the front operation for a pass", pass, NVN_STENCIL_OP_DECR_WRAP);
    nvn.nvn.nvnDepthStencilStateGetStencilOp(&state, NVN_FACE_BACK, &fail, &depth_fail, &pass);
    Gives(why, sizeof(why), "the back operation for a failed stencil test", fail,
          NVN_STENCIL_OP_INVERT);
    Gives(why, sizeof(why), "the back operation for a failed depth test", depth_fail,
          NVN_STENCIL_OP_ZERO);
    Gives(why, sizeof(why), "the back operation for a pass", pass, NVN_STENCIL_OP_INCR_WRAP);
    // Both faces in one call.
    nvn.nvn.nvnDepthStencilStateSetStencilFunc(&state, NVN_FACE_FRONT_AND_BACK,
                                               NVN_STENCIL_FUNC_GREATER);
    Gives(why, sizeof(why), "the front stencil function set for both faces",
          nvn.nvn.nvnDepthStencilStateGetStencilFunc(&state, NVN_FACE_FRONT),
          NVN_STENCIL_FUNC_GREATER);
    Gives(why, sizeof(why), "the back stencil function set for both faces",
          nvn.nvn.nvnDepthStencilStateGetStencilFunc(&state, NVN_FACE_BACK),
          NVN_STENCIL_FUNC_GREATER);
    CHECK_TRUE(SUITE, name, CHECK_EXACT, why[0] == '\0' && NvnLayerError()[0] == '\0', "%s",
               why[0] != '\0' ? why : NvnLayerError());
}

static void MultisampleStateCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    _Alignas(8) NVNmultisampleState state;
    nvn.nvn.nvnMultisampleStateSetDefaults(&state);
    nvn.nvn.nvnMultisampleStateSetMultisampleEnable(&state, 0);
    Gives(why, sizeof(why), "whether a state set not to multisample does",
          nvn.nvn.nvnMultisampleStateGetMultisampleEnable(&state), 0);
    nvn.nvn.nvnMultisampleStateSetMultisampleEnable(&state, 1);
    nvn.nvn.nvnMultisampleStateSetSamples(&state, 4);
    nvn.nvn.nvnMultisampleStateSetRasterSamples(&state, 8);
    nvn.nvn.nvnMultisampleStateSetAlphaToCoverageEnable(&state, 1);
    nvn.nvn.nvnMultisampleStateSetAlphaToCoverageDither(&state, 1);
    nvn.nvn.nvnMultisampleStateSetCoverageToColorEnable(&state, 1);
    nvn.nvn.nvnMultisampleStateSetCoverageToColorOutput(&state, 3);
    nvn.nvn.nvnMultisampleStateSetCoverageModulationMode(&state,
                                                         NVN_COVERAGE_MODULATION_MODE_ALPHA);
    nvn.nvn.nvnMultisampleStateSetCoverageTruncationEnable(&state, 1);
    nvn.nvn.nvnMultisampleStateSetSampleLocationsEnable(&state, 1);
    nvn.nvn.nvnMultisampleStateSetSampleLocationsGridEnable(&state, 1);
    static const float locations[4] = { 0.25f, 0.75f, 0.5f, 0.125f };
    nvn.nvn.nvnMultisampleStateSetSampleLocations(&state, 2, 2, locations);
    Gives(why, sizeof(why), "whether it multisamples",
          nvn.nvn.nvnMultisampleStateGetMultisampleEnable(&state), 1);
    Gives(why, sizeof(why), "the samples", nvn.nvn.nvnMultisampleStateGetSamples(&state), 4);
    Gives(why, sizeof(why), "the raster samples",
          nvn.nvn.nvnMultisampleStateGetRasterSamples(&state), 8);
    Gives(why, sizeof(why), "whether alpha goes to coverage",
          nvn.nvn.nvnMultisampleStateGetAlphaToCoverageEnable(&state), 1);
    Gives(why, sizeof(why), "whether alpha to coverage dithers",
          nvn.nvn.nvnMultisampleStateGetAlphaToCoverageDither(&state), 1);
    Gives(why, sizeof(why), "whether coverage goes to a colour",
          nvn.nvn.nvnMultisampleStateGetCoverageToColorEnable(&state), 1);
    Gives(why, sizeof(why), "the output coverage goes to",
          nvn.nvn.nvnMultisampleStateGetCoverageToColorOutput(&state), 3);
    Gives(why, sizeof(why), "the coverage modulation mode",
          nvn.nvn.nvnMultisampleStateGetCoverageModulationMode(&state),
          NVN_COVERAGE_MODULATION_MODE_ALPHA);
    Gives(why, sizeof(why), "whether coverage is truncated",
          nvn.nvn.nvnMultisampleStateGetCoverageTruncationEnable(&state), 1);
    Gives(why, sizeof(why), "whether the sample locations are the program's",
          nvn.nvn.nvnMultisampleStateGetSampleLocationsEnable(&state), 1);
    Gives(why, sizeof(why), "whether the sample locations are a grid's",
          nvn.nvn.nvnMultisampleStateGetSampleLocationsGridEnable(&state), 1);
    // Sixteen locations in all: with eight raster samples they cover one pixel by two.
    int width = -1, height = -1;
    nvn.nvn.nvnMultisampleStateGetSampleLocationsGrid(&state, &width, &height);
    Gives(why, sizeof(why), "the width of the sample locations' grid", width, 1);
    Gives(why, sizeof(why), "the height of the sample locations' grid", height, 2);
    nvn.nvn.nvnMultisampleStateSetAlphaToCoverageDither(&state, 0);
    Gives(why, sizeof(why), "whether alpha to coverage dithers once it is set not to",
          nvn.nvn.nvnMultisampleStateGetAlphaToCoverageDither(&state), 0);
    Gives(why, sizeof(why), "whether alpha goes to coverage, the flag beside the one cleared",
          nvn.nvn.nvnMultisampleStateGetAlphaToCoverageEnable(&state), 1);
    CHECK_TRUE(SUITE, name, CHECK_EXACT, why[0] == '\0' && NvnLayerError()[0] == '\0', "%s",
               why[0] != '\0' ? why : NvnLayerError());
}

/// The polygon state, a vertex attribute and a vertex stream.
static void PolygonVertexStateCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    _Alignas(8) NVNpolygonState polygon;
    _Alignas(8) NVNvertexAttribState attrib;
    _Alignas(8) NVNvertexStreamState stream;
    nvn.nvn.nvnPolygonStateSetDefaults(&polygon);
    nvn.nvn.nvnPolygonStateSetCullFace(&polygon, NVN_FACE_BACK);
    nvn.nvn.nvnPolygonStateSetFrontFace(&polygon, NVN_FRONT_FACE_CW);
    nvn.nvn.nvnPolygonStateSetPolygonMode(&polygon, NVN_POLYGON_MODE_LINE);
    nvn.nvn.nvnPolygonStateSetPolygonOffsetEnables(
        &polygon, NVN_POLYGON_OFFSET_ENABLE_LINE | NVN_POLYGON_OFFSET_ENABLE_FILL);
    Gives(why, sizeof(why), "the culled face", nvn.nvn.nvnPolygonStateGetCullFace(&polygon),
          NVN_FACE_BACK);
    Gives(why, sizeof(why), "the front face", nvn.nvn.nvnPolygonStateGetFrontFace(&polygon),
          NVN_FRONT_FACE_CW);
    Gives(why, sizeof(why), "the polygon mode", nvn.nvn.nvnPolygonStateGetPolygonMode(&polygon),
          NVN_POLYGON_MODE_LINE);
    Gives(why, sizeof(why), "the polygon offset enables",
          nvn.nvn.nvnPolygonStateGetPolygonOffsetEnables(&polygon),
          NVN_POLYGON_OFFSET_ENABLE_LINE | NVN_POLYGON_OFFSET_ENABLE_FILL);

    nvn.nvn.nvnVertexAttribStateSetDefaults(&attrib);
    nvn.nvn.nvnVertexAttribStateSetFormat(&attrib, NVN_FORMAT_RG32F, 24);
    nvn.nvn.nvnVertexAttribStateSetStreamIndex(&attrib, 3);
    NVNformat format = NVN_FORMAT_NONE;
    ptrdiff_t offset = -1;
    nvn.nvn.nvnVertexAttribStateGetFormat(&attrib, &format, &offset);
    Gives(why, sizeof(why), "the attribute's format", format, NVN_FORMAT_RG32F);
    Gives(why, sizeof(why), "the attribute's offset", (long long)offset, 24);
    Gives(why, sizeof(why), "the attribute's stream",
          nvn.nvn.nvnVertexAttribStateGetStreamIndex(&attrib), 3);

    nvn.nvn.nvnVertexStreamStateSetDefaults(&stream);
    nvn.nvn.nvnVertexStreamStateSetStride(&stream, 36);
    nvn.nvn.nvnVertexStreamStateSetDivisor(&stream, 2);
    Gives(why, sizeof(why), "the stream's stride",
          (long long)nvn.nvn.nvnVertexStreamStateGetStride(&stream), 36);
    Gives(why, sizeof(why), "the stream's divisor",
          nvn.nvn.nvnVertexStreamStateGetDivisor(&stream), 2);
    CHECK_TRUE(SUITE, name, CHECK_EXACT, why[0] == '\0' && NvnLayerError()[0] == '\0', "%s",
               why[0] != '\0' ? why : NvnLayerError());
}

// ---- the rest of what a texture says, and texels moved by the processor

/// The channel sources of a swizzle as one number, three bits each from red.
static long long Swizzle(NVNtextureSwizzle r, NVNtextureSwizzle g, NVNtextureSwizzle b,
                         NVNtextureSwizzle a) {
    return (long long)r | (long long)g << 3 | (long long)b << 6 | (long long)a << 9;
}

/// What a builder and a view keep past the size and the format: the channel swizzle, the depth
/// and stencil mode, the row stride, the names a builder carries for other libraries. Neither
/// makes a texture here.
static void TextureMoreCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    static const uint8_t packaged[16] = { 1, 2, 3, 4 };
    _Alignas(8) NVNtextureBuilder builder;
    memset(&builder, 0, sizeof(builder));
    nvn.nvn.nvnTextureBuilderSetDevice(&builder, &nvn.device);
    nvn.nvn.nvnTextureBuilderSetDefaults(&builder);
    nvn.nvn.nvnTextureBuilderSetSwizzle(&builder, NVN_TEXTURE_SWIZZLE_B, NVN_TEXTURE_SWIZZLE_ZERO,
                                        NVN_TEXTURE_SWIZZLE_R, NVN_TEXTURE_SWIZZLE_ONE);
    nvn.nvn.nvnTextureBuilderSetDepthStencilMode(&builder, NVN_TEXTURE_DEPTH_STENCIL_MODE_STENCIL);
    nvn.nvn.nvnTextureBuilderSetStride(&builder, 0x240);
    nvn.nvn.nvnTextureBuilderSetGLTextureName(&builder, 0x1234abcdu);
    nvn.nvn.nvnTextureBuilderSetPackagedTextureData(&builder, packaged);
    // The twelve bits a driver keeps of a packaged texture's layout, in its first two bytes.
    const NVNpackagedTextureLayout layout = { { 0x23, 0x01, 0, 0, 0, 0, 0, 0 } };
    nvn.nvn.nvnTextureBuilderSetPackagedTextureLayout(&builder, &layout);
    NVNtextureSwizzle r = NVN_TEXTURE_SWIZZLE_A, g = NVN_TEXTURE_SWIZZLE_A;
    NVNtextureSwizzle b = NVN_TEXTURE_SWIZZLE_A, a = NVN_TEXTURE_SWIZZLE_A;
    nvn.nvn.nvnTextureBuilderGetSwizzle(&builder, &r, &g, &b, &a);
    Gives(why, sizeof(why), "the builder's swizzle, three bits a channel from red",
          Swizzle(r, g, b, a),
          Swizzle(NVN_TEXTURE_SWIZZLE_B, NVN_TEXTURE_SWIZZLE_ZERO, NVN_TEXTURE_SWIZZLE_R,
                  NVN_TEXTURE_SWIZZLE_ONE));
    Gives(why, sizeof(why), "the builder's depth and stencil mode",
          nvn.nvn.nvnTextureBuilderGetDepthStencilMode(&builder),
          NVN_TEXTURE_DEPTH_STENCIL_MODE_STENCIL);
    Gives(why, sizeof(why), "the builder's stride",
          (long long)nvn.nvn.nvnTextureBuilderGetStride(&builder), 0x240);
    Gives(why, sizeof(why), "the builder's GL texture name",
          nvn.nvn.nvnTextureBuilderGetGLTextureName(&builder), 0x1234abcdll);
    Gives(why, sizeof(why), "the builder's packaged texture data",
          (long long)(uintptr_t)nvn.nvn.nvnTextureBuilderGetPackagedTextureData(&builder),
          (long long)(uintptr_t)packaged);
    NVNpackagedTextureLayout back;
    memset(&back, 0xee, sizeof(back));
    Gives(why, sizeof(why), "whether the builder has a packaged texture layout",
          nvn.nvn.nvnTextureBuilderGetPackagedTextureLayout(&builder, &back), 1);
    Gives(why, sizeof(why), "the layout's first twelve bits",
          (back.layout[0] | back.layout[1] << 8) & 0xfff, 0x123);

    _Alignas(8) NVNtextureView view;
    nvn.nvn.nvnTextureViewSetDefaults(&view);
    NVNtextureDepthStencilMode mode = NVN_TEXTURE_DEPTH_STENCIL_MODE_DEPTH;
    Gives(why, sizeof(why), "whether a new view has a swizzle set",
          nvn.nvn.nvnTextureViewGetSwizzle(&view, &r, &g, &b, &a), 0);
    Gives(why, sizeof(why), "whether a new view has a depth and stencil mode set",
          nvn.nvn.nvnTextureViewGetDepthStencilMode(&view, &mode), 0);
    nvn.nvn.nvnTextureViewSetSwizzle(&view, NVN_TEXTURE_SWIZZLE_G, NVN_TEXTURE_SWIZZLE_A,
                                     NVN_TEXTURE_SWIZZLE_ONE, NVN_TEXTURE_SWIZZLE_B);
    nvn.nvn.nvnTextureViewSetDepthStencilMode(&view, NVN_TEXTURE_DEPTH_STENCIL_MODE_STENCIL);
    Gives(why, sizeof(why), "whether a view with a swizzle set says so",
          nvn.nvn.nvnTextureViewGetSwizzle(&view, &r, &g, &b, &a), 1);
    Gives(why, sizeof(why), "the view's swizzle, three bits a channel from red",
          Swizzle(r, g, b, a),
          Swizzle(NVN_TEXTURE_SWIZZLE_G, NVN_TEXTURE_SWIZZLE_A, NVN_TEXTURE_SWIZZLE_ONE,
                  NVN_TEXTURE_SWIZZLE_B));
    Gives(why, sizeof(why), "whether a view with a depth and stencil mode set says so",
          nvn.nvn.nvnTextureViewGetDepthStencilMode(&view, &mode), 1);
    Gives(why, sizeof(why), "the view's depth and stencil mode", mode,
          NVN_TEXTURE_DEPTH_STENCIL_MODE_STENCIL);
    CHECK_TRUE(SUITE, name, CHECK_EXACT, why[0] == '\0' && NvnLayerError()[0] == '\0', "%s",
               why[0] != '\0' ? why : NvnLayerError());
}

/// What two textures made from one builder, in the first page of the levels pool, say of
/// themselves past their size: the swizzle the builder had, the storage's size and address,
/// that they are the same texture and the levels texture is another, and a driver's own account
/// of their storage where ScarletNX's reading of a driver gives one to hold them to.
static void TextureSelfCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    static _Alignas(8) NVNtexture small, twin;
    _Alignas(8) NVNtextureBuilder builder;
    memset(&builder, 0, sizeof(builder));
    TextureBuilder(&builder, 16, 16, 0);
    nvn.nvn.nvnTextureBuilderSetSwizzle(&builder, NVN_TEXTURE_SWIZZLE_B, NVN_TEXTURE_SWIZZLE_G,
                                        NVN_TEXTURE_SWIZZLE_R, NVN_TEXTURE_SWIZZLE_ONE);
    nvn.nvn.nvnTextureBuilderSetStorage(&builder, &nvn.levels_pool, 0);
    const size_t size = nvn.nvn.nvnTextureBuilderGetStorageSize(&builder);
    if (size == 0u || size > POOL_PAGE || !nvn.nvn.nvnTextureInitialize(&small, &builder)) {
        char detail[200];
        snprintf(detail, sizeof(detail), "a 16x16 texture of %zu bytes of storage in a page of "
                 "0x%x: %s",
                 size, (unsigned)POOL_PAGE, Why("it did not fit, or nvnTextureInitialize said false"));
        ReportCase(SUITE, name, CHECK_NONE, false, detail);
        return;
    }
    if (!nvn.nvn.nvnTextureInitialize(&twin, &builder)) {
        nvn.nvn.nvnTextureFinalize(&small);
        ReportCase(SUITE, name, CHECK_NONE, false, Why("nvnTextureInitialize answered false"));
        return;
    }
    NVNtextureSwizzle r = NVN_TEXTURE_SWIZZLE_A, g = NVN_TEXTURE_SWIZZLE_A;
    NVNtextureSwizzle b = NVN_TEXTURE_SWIZZLE_A, a = NVN_TEXTURE_SWIZZLE_A;
    nvn.nvn.nvnTextureGetSwizzle(&small, &r, &g, &b, &a);
    Gives(why, sizeof(why), "the texture's swizzle, three bits a channel from red",
          Swizzle(r, g, b, a),
          Swizzle(NVN_TEXTURE_SWIZZLE_B, NVN_TEXTURE_SWIZZLE_G, NVN_TEXTURE_SWIZZLE_R,
                  NVN_TEXTURE_SWIZZLE_ONE));
    Gives(why, sizeof(why), "the depth and stencil mode of a texture whose builder set none",
          nvn.nvn.nvnTextureGetDepthStencilMode(&small), NVN_TEXTURE_DEPTH_STENCIL_MODE_DEPTH);
    Gives(why, sizeof(why), "the texture's storage size, its builder's",
          nvn.nvn.nvnTextureGetStorageSize(&small), (long long)size);
    Gives(why, sizeof(why), "the levels texture's address, its pool's and its offset",
          (long long)nvn.nvn.nvnTextureGetTextureAddress(&nvn.levels),
          (long long)(nvn.nvn.nvnMemoryPoolGetBufferAddress(&nvn.levels_pool) +
                      (uint64_t)nvn.levels_offset));
    Gives(why, sizeof(why), "whether two textures of one builder at one place are the same",
          nvn.nvn.nvnTextureCompare(&small, &twin), 1);
    Gives(why, sizeof(why), "whether the levels texture is the same as the small one",
          nvn.nvn.nvnTextureCompare(&nvn.levels, &small), 0);
    Gives(why, sizeof(why), "where no view starts in the texture's storage",
          (long long)nvn.nvn.nvnTextureGetViewOffset(&nvn.levels, NULL), 0);
    Gives(why, sizeof(why), "where no view starts in the builder's storage",
          (long long)nvn.nvn.nvnTextureBuilderGetViewOffset(&builder, NULL), 0);
    // A driver's own account, by ScarletNX's reading of one: colour is raw storage class 2, a
    // texture that is not sparse has a tile of noughts, and NVIDIA's surface starts with the
    // first level's width and height.
    Gives(why, sizeof(why), "the builder's raw storage class",
          nvn.nvn.nvnTextureBuilderGetRawStorageClass(&builder), 2);
    Gives(why, sizeof(why), "the texture's raw storage class",
          nvn.nvn.nvnTextureGetRawStorageClass(&small), 2);
    uint32_t tile[2][4];
    memset(tile, 0xee, sizeof(tile));
    nvn.nvn.nvnTextureBuilderGetSparseTileLayout(&builder, (NVNtextureSparseTileLayout *)tile[0]);
    nvn.nvn.nvnTextureGetSparseTileLayout(&small, (NVNtextureSparseTileLayout *)tile[1]);
    for (int i = 0; i < 4; ++i) {
        Gives(why, sizeof(why), "a word of the builder's sparse tile layout", tile[0][i], 0);
        Gives(why, sizeof(why), "a word of the texture's sparse tile layout", tile[1][i], 0);
    }
    uint32_t surface[0x58 / 4];
    memset(surface, 0xee, sizeof(surface));
    nvn.nvn.nvnTextureGetNvRmSurfaceNVX(&small, NULL, surface);
    Gives(why, sizeof(why), "the width NVIDIA's surface of the texture starts with", surface[0],
          16);
    Gives(why, sizeof(why), "the height that follows it", surface[1], 16);
    // Answers nothing public defines: asked, and written into the line of a failing case only.
    const long long classes[2] = { nvn.nvn.nvnTextureBuilderGetStorageClass(&builder),
                                   nvn.nvn.nvnTextureGetStorageClass(&small) };
    const long long zcull[2] = { (long long)nvn.nvn.nvnTextureBuilderGetZCullStorageSize(&builder),
                                 (long long)nvn.nvn.nvnTextureGetZCullStorageSize(&small) };
    const long long stride = (long long)nvn.nvn.nvnTextureGetStride(&small);
    nvn.nvn.nvnTextureFinalize(&twin);
    nvn.nvn.nvnTextureFinalize(&small);
    CHECK_TRUE(SUITE, name, CHECK_EXACT, why[0] == '\0' && NvnLayerError()[0] == '\0',
               "%s (the driver's own: storage classes 0x%llx and 0x%llx, ZCull sizes %lld and "
               "%lld, stride %lld)",
               why[0] != '\0' ? why : NvnLayerError(), (unsigned long long)classes[0],
               (unsigned long long)classes[1], zcull[0], zcull[1], stride);
}

/// The copy cases' picture written into the first copy texture by the processor, read back by
/// the processor, and read back again by the graphics unit's copy, which says the texels are
/// in the texture and not only where the processor's calls keep them.
static void TexelsCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    static uint8_t picture[TARGET_BYTES], back[TARGET_BYTES];
    for (uint32_t y = 0; y < TARGET_SIZE; ++y) {
        for (uint32_t x = 0; x < TARGET_SIZE; ++x) {
            // The copy cases' picture upside down, so that what they left there is not it.
            Pattern(x, TARGET_SIZE - 1u - y, picture + ((size_t)y * TARGET_SIZE + x) * 4u);
        }
    }
    memset(back, READ_PRESET, sizeof(back));
    uint8_t *const read = nvn.nvn.nvnMemoryPoolMap(&nvn.read_pool);
    if (read == NULL) {
        ReportCase(SUITE, name, CHECK_NONE, false, Why("nvnMemoryPoolMap gave no address"));
        return;
    }
    memset(read, READ_PRESET, TARGET_BYTES);
    const NVNcopyRegion whole = { 0, 0, 0, TARGET_SIZE, TARGET_SIZE, 1 };
    nvn.nvn.nvnQueueFinish(&nvn.queue);
    nvn.nvn.nvnTextureWriteTexels(&nvn.copy_a, NULL, &whole, picture);
    nvn.nvn.nvnTextureFlushTexels(&nvn.copy_a, NULL, &whole);
    nvn.nvn.nvnCommandBufferBeginRecording(&nvn.command);
    nvn.nvn.nvnCommandBufferSetCopyRowStride(&nvn.command, 0);
    nvn.nvn.nvnCommandBufferBarrier(&nvn.command, NVN_BARRIER_ORDER_PRIMITIVES_BIT |
                                                      NVN_BARRIER_INVALIDATE_TEXTURE_BIT);
    nvn.nvn.nvnCommandBufferCopyTextureToBuffer(
        &nvn.command, &nvn.copy_a, NULL, &whole,
        nvn.nvn.nvnMemoryPoolGetBufferAddress(&nvn.read_pool), NVN_COPY_FLAGS_NONE);
    const NVNcommandHandle list = nvn.nvn.nvnCommandBufferEndRecording(&nvn.command);
    nvn.nvn.nvnQueueSubmitCommands(&nvn.queue, 1, &list);
    nvn.nvn.nvnQueueFinish(&nvn.queue);
    nvn.nvn.nvnTextureInvalidateTexels(&nvn.copy_a, NULL, &whole);
    nvn.nvn.nvnTextureReadTexels(&nvn.copy_a, NULL, &whole, back);
    if (NvnLayerError()[0] != '\0') {
        ReportCase(SUITE, name, CHECK_NONE, false, NvnLayerError());
        return;
    }
    uint32_t wrong_read = 0, wrong_copy = 0, first = 0;
    for (uint32_t at = 0; at < TARGET_BYTES; ++at) {
        if (back[at] != picture[at] && wrong_read++ == 0u && wrong_copy == 0u) {
            first = at;
        }
        if (read[at] != picture[at] && wrong_copy++ == 0u && wrong_read == 0u) {
            first = at;
        }
    }
    CHECK_TRUE(SUITE, name, CHECK_EXACT, wrong_read == 0u && wrong_copy == 0u,
               "of %u bytes written, %u differ as nvnTextureReadTexels gives them back and %u as "
               "the graphics unit's copy does; byte %u was written 0x%02x and is 0x%02x by the "
               "read and 0x%02x by the copy (0x%02x is a byte nothing wrote)",
               (unsigned)TARGET_BYTES, (unsigned)wrong_read, (unsigned)wrong_copy, (unsigned)first,
               picture[first], back[first], read[first], READ_PRESET);
}

/// A 16 by 8 part of the first copy texture written from rows 80 bytes apart and read back
/// into rows 96 apart: the texels, and every byte between the rows of the destination, which a
/// read leaves as it was.
static void TexelsStridedCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    enum { WIDTH = 16, HEIGHT = 8, FROM = 80, TO = 96 };
    static uint8_t source[FROM * HEIGHT], back[TO * HEIGHT];
    memset(source, 0x5a, sizeof(source));
    for (uint32_t y = 0; y < HEIGHT; ++y) {
        for (uint32_t x = 0; x < WIDTH; ++x) {
            Pattern(x + 40u, y + 20u, source + (size_t)y * FROM + x * 4u);
        }
    }
    memset(back, READ_PRESET, sizeof(back));
    const NVNcopyRegion part = { 8, 4, 0, WIDTH, HEIGHT, 1 };
    nvn.nvn.nvnQueueFinish(&nvn.queue);
    nvn.nvn.nvnTextureWriteTexelsStrided(&nvn.copy_a, NULL, &part, source, FROM, 0);
    nvn.nvn.nvnTextureReadTexelsStrided(&nvn.copy_a, NULL, &part, back, TO, 0);
    if (NvnLayerError()[0] != '\0') {
        ReportCase(SUITE, name, CHECK_NONE, false, NvnLayerError());
        return;
    }
    uint32_t wrong = 0, first = 0;
    uint8_t want_first = 0;
    for (uint32_t at = 0; at < sizeof(back); ++at) {
        const uint32_t y = at / TO, in_row = at % TO;
        const uint8_t want = in_row < WIDTH * 4u ? source[(size_t)y * FROM + in_row] : READ_PRESET;
        if (back[at] != want && wrong++ == 0u) {
            first = at;
            want_first = want;
        }
    }
    CHECK_TRUE(SUITE, name, CHECK_EXACT, wrong == 0u,
               "%u of %u bytes wrong; the first, byte %u of row %u, is 0x%02x, expected 0x%02x "
               "(0x%02x is a byte between rows, which a read leaves)",
               (unsigned)wrong, (unsigned)sizeof(back), (unsigned)(first % TO),
               (unsigned)(first / TO), back[first], want_first, READ_PRESET);
}

// ---- the other texture targets, and the rest of a queue

/// A texture of one row, an array of three layers and a 3D texture of four slices, each layer
/// and slice given a picture of its own through a copy region's depth and read back from it.
/// Every byte of the readback is judged, so a layer that took another's picture shows.
static void TargetsCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    enum { SIDE = 16, LAYERS = 3, SLICES = 4, ROW = 64, KINDS = 3 };
    static _Alignas(8) NVNtexture textures[KINDS];
    static _Alignas(8) NVNmemoryPool pool;
    static const struct {
        NVNtextureTarget target;
        int width, height, depth;
        const char *what;
    } KIND[KINDS] = {
        { NVN_TEXTURE_TARGET_1D, ROW, 1, 1, "the 1D texture" },
        { NVN_TEXTURE_TARGET_2D_ARRAY, SIDE, SIDE, LAYERS, "the 2D array" },
        { NVN_TEXTURE_TARGET_3D, SIDE, SIDE, SLICES, "the 3D texture" },
    };
    char why[256] = "";
    _Alignas(8) NVNtextureBuilder builders[KINDS];
    size_t offsets[KINDS], total = 0;
    for (int k = 0; k < KINDS; ++k) {
        memset(&builders[k], 0, sizeof(builders[k]));
        TextureBuilder(&builders[k], KIND[k].width, KIND[k].height, 0);
        nvn.nvn.nvnTextureBuilderSetTarget(&builders[k], KIND[k].target);
        if (KIND[k].target == NVN_TEXTURE_TARGET_1D) {
            nvn.nvn.nvnTextureBuilderSetSize1D(&builders[k], KIND[k].width);
        } else if (KIND[k].target == NVN_TEXTURE_TARGET_3D) {
            nvn.nvn.nvnTextureBuilderSetSize3D(&builders[k], KIND[k].width, KIND[k].height,
                                               KIND[k].depth);
        } else {
            nvn.nvn.nvnTextureBuilderSetDepth(&builders[k], KIND[k].depth);
        }
        const size_t size = nvn.nvn.nvnTextureBuilderGetStorageSize(&builders[k]);
        const size_t alignment = nvn.nvn.nvnTextureBuilderGetStorageAlignment(&builders[k]);
        if (size == 0u || alignment == 0u || alignment > POOL_PAGE) {
            snprintf(why, sizeof(why), "%s has a storage of %zu bytes at an alignment of %zu: %s",
                     KIND[k].what, size, alignment, Why("an answer no pool here can hold"));
            ReportCase(SUITE, name, CHECK_NONE, false, why);
            return;
        }
        offsets[k] = total;
        total += RoundUp(size, POOL_PAGE);
    }
    void *storage = NULL;
    bool pool_up = MakePool(&pool, &storage, total,
                            NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS | NVN_MEMORY_POOL_FLAGS_GPU_CACHED);
    int made = 0;
    for (; pool_up && made < KINDS; ++made) {
        nvn.nvn.nvnTextureBuilderSetStorage(&builders[made], &pool, (ptrdiff_t)offsets[made]);
        if (!nvn.nvn.nvnTextureInitialize(&textures[made], &builders[made])) {
            snprintf(why, sizeof(why), "nvnTextureInitialize of %s: %s", KIND[made].what,
                     Why("false"));
            break;
        }
    }
    uint8_t *const stage = nvn.nvn.nvnMemoryPoolMap(&nvn.stage_pool);
    uint8_t *const read = nvn.nvn.nvnMemoryPoolMap(&nvn.read_pool);
    uint32_t wrong = 0, first = 0, bytes = 0;
    uint8_t want_first = 0;
    if (!pool_up || stage == NULL || read == NULL) {
        snprintf(why, sizeof(why), "the targets' pool or a mapping: %s", Why("no memory, or false"));
    } else if (made == KINDS) {
        // Every layer's picture one after another in the staging pool, and read back the same.
        uint32_t at = 0;
        memset(read, READ_PRESET, READ_POOL_BYTES);
        const NVNbufferAddress picture = nvn.nvn.nvnMemoryPoolGetBufferAddress(&nvn.stage_pool);
        const NVNbufferAddress readback = nvn.nvn.nvnMemoryPoolGetBufferAddress(&nvn.read_pool);
        nvn.nvn.nvnCommandBufferBeginRecording(&nvn.command);
        nvn.nvn.nvnCommandBufferSetCopyRowStride(&nvn.command, 0);
        for (int pass = 0; pass < 2; ++pass) {
            at = 0;
            for (int k = 0; k < KINDS; ++k) {
                for (int z = 0; z < KIND[k].depth; ++z) {
                    const NVNcopyRegion region = { 0, 0, z, KIND[k].width, KIND[k].height, 1 };
                    const uint32_t size = (uint32_t)(KIND[k].width * KIND[k].height * 4);
                    if (pass == 0) {
                        for (int y = 0; y < KIND[k].height; ++y) {
                            for (int x = 0; x < KIND[k].width; ++x) {
                                LevelTexel((uint32_t)(k * 4 + z), (uint32_t)x, (uint32_t)y,
                                           stage + at + (size_t)(y * KIND[k].width + x) * 4u);
                            }
                        }
                        nvn.nvn.nvnCommandBufferCopyBufferToTexture(&nvn.command, picture + at,
                                                                    &textures[k], NULL, &region,
                                                                    NVN_COPY_FLAGS_NONE);
                    } else {
                        nvn.nvn.nvnCommandBufferCopyTextureToBuffer(&nvn.command, &textures[k],
                                                                    NULL, &region, readback + at,
                                                                    NVN_COPY_FLAGS_NONE);
                    }
                    at += size;
                }
            }
            if (pass == 0) {
                nvn.nvn.nvnCommandBufferBarrier(&nvn.command,
                                                NVN_BARRIER_ORDER_PRIMITIVES_BIT |
                                                    NVN_BARRIER_INVALIDATE_TEXTURE_BIT);
            }
        }
        bytes = at;
        const NVNcommandHandle list = nvn.nvn.nvnCommandBufferEndRecording(&nvn.command);
        nvn.nvn.nvnQueueSubmitCommands(&nvn.queue, 1, &list);
        nvn.nvn.nvnQueueFinish(&nvn.queue);
        for (uint32_t i = 0; i < READ_POOL_BYTES; ++i) {
            const uint8_t want = i < bytes ? stage[i] : READ_PRESET;
            if (read[i] != want && wrong++ == 0u) {
                first = i;
                want_first = want;
            }
        }
        if (NvnLayerError()[0] != '\0') {
            snprintf(why, sizeof(why), "%s", NvnLayerError());
        }
    }
    nvn.nvn.nvnQueueFinish(&nvn.queue);
    for (int k = 0; k < made; ++k) {
        nvn.nvn.nvnTextureFinalize(&textures[k]);
    }
    DropPool(&pool, &storage, &pool_up);
    if (why[0] != '\0') {
        ReportCase(SUITE, name, CHECK_NONE, false, why);
        return;
    }
    CHECK_TRUE(SUITE, name, CHECK_EXACT, wrong == 0u,
               "%u of %u bytes wrong; the first, byte %u of the %u that hold the 1D texture's "
               "256, the array's three layers of 1024 and the 3D texture's four slices of 1024, "
               "is 0x%02x, expected 0x%02x (0x%02x is a byte nothing copied)",
               (unsigned)wrong, (unsigned)READ_POOL_BYTES, (unsigned)first, (unsigned)bytes,
               read != NULL ? read[first] : 0u, want_first, READ_PRESET);
}

/// What a queue builder keeps of the memory, the priority and the primary queue a program
/// gives it, and the memory a queue says it has used once its counts are reset.
static void QueueRestCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    static uint8_t memory[64];
    _Alignas(8) NVNqueueBuilder builder;
    memset(&builder, 0, sizeof(builder));
    nvn.nvn.nvnQueueBuilderSetDevice(&builder, &nvn.device);
    nvn.nvn.nvnQueueBuilderSetDefaults(&builder);
    nvn.nvn.nvnQueueBuilderSetQueueMemory(&builder, memory, 0x7000);
    nvn.nvn.nvnQueueBuilderSetQueuePriority(&builder, 37);
    nvn.nvn.nvnQueueBuilderSetPrimaryQueue(&builder, &nvn.queue);
    Gives(why, sizeof(why), "the builder's queue memory",
          (long long)(uintptr_t)nvn.nvn.nvnQueueBuilderGetMemory(&builder),
          (long long)(uintptr_t)memory);
    Gives(why, sizeof(why), "the builder's queue memory size",
          (long long)nvn.nvn.nvnQueueBuilderGetMemorySize(&builder), 0x7000);
    Gives(why, sizeof(why), "the builder's queue priority",
          nvn.nvn.nvnQueueBuilderGetQueuePriority(&builder), 37);
    // What a driver would need of the program for such a queue: its own number.
    const long long needed = (long long)nvn.nvn.nvnQueueBuilderGetQueueMemorySize(&builder);
    nvn.nvn.nvnQueueFinish(&nvn.queue);
    nvn.nvn.nvnQueueResetMemoryUsageCounts(&nvn.queue);
    Gives(why, sizeof(why), "the command memory the queue used since its counts were reset",
          (long long)nvn.nvn.nvnQueueGetTotalCommandMemoryUsed(&nvn.queue), 0);
    Gives(why, sizeof(why), "the control memory the queue used since its counts were reset",
          (long long)nvn.nvn.nvnQueueGetTotalControlMemoryUsed(&nvn.queue), 0);
    Gives(why, sizeof(why), "the compute memory the queue used since its counts were reset",
          (long long)nvn.nvn.nvnQueueGetTotalComputeMemoryUsed(&nvn.queue), 0);
    CHECK_TRUE(SUITE, name, CHECK_EXACT, why[0] == '\0' && NvnLayerError()[0] == '\0',
               "%s (the queue memory a queue of the builder needs, a driver's own: %lld)",
               why[0] != '\0' ? why : NvnLayerError(), needed);
}

// ---- an event in a list, and a virtual pool

// What the layer cannot do under deko3d at all (NvnLayerLacks) is a skip there, with the
// layer's reason: a runtime that runs NVN itself is asked it.

/// Ends the case as a skip when the layer said it has no way to do what was asked; false when
/// it did not, and the case goes on.
static bool Lacking(const char *name) {
    if (!NvnLayerLacks()) {
        return false;
    }
    ReportSkip(SUITE, name, REPORT_REASON_UNAVAILABLE, NvnLayerError());
    return true;
}

/// An event in the last word of the readback pool's first page; false with the layer's reason.
static bool ListEvent(NVNevent *event) {
    _Alignas(8) NVNeventBuilder builder;
    nvn.nvn.nvnEventBuilderSetDefaults(&builder);
    nvn.nvn.nvnEventBuilderSetStorage(&builder, &nvn.read_pool, (ptrdiff_t)(POOL_PAGE - 4u));
    return nvn.nvn.nvnEventInitialize(event, &builder);
}

/// A list that signals an event: the value the processor reads once the queue has finished. A
/// write (@p add false) of one number over another the processor signalled, or a sum with it.
static void EventListCase(const char *name, bool add) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    static _Alignas(8) NVNevent event;
    if (!ListEvent(&event)) {
        ReportCase(SUITE, name, CHECK_NONE, false, Why("nvnEventInitialize answered false"));
        return;
    }
    nvn.nvn.nvnEventSignal(&event, NVN_EVENT_SIGNAL_MODE_WRITE, 0x00a50000u);
    nvn.nvn.nvnCommandBufferBeginRecording(&nvn.command);
    nvn.nvn.nvnCommandBufferSignalEvent(&nvn.command, &event,
                                        add ? NVN_EVENT_SIGNAL_MODE_ADD
                                            : NVN_EVENT_SIGNAL_MODE_WRITE,
                                        NVN_EVENT_SIGNAL_LOCATION_BOTTOM, 0, 0x1234u);
    const NVNcommandHandle list = nvn.nvn.nvnCommandBufferEndRecording(&nvn.command);
    if (Lacking(name)) {
        nvn.nvn.nvnEventFinalize(&event);
        return;
    }
    nvn.nvn.nvnQueueSubmitCommands(&nvn.queue, 1, &list);
    nvn.nvn.nvnQueueFinish(&nvn.queue);
    const uint32_t value = nvn.nvn.nvnEventGetValue(&event);
    const uint32_t want = add ? 0x00a51234u : 0x1234u;
    nvn.nvn.nvnEventFinalize(&event);
    CHECK_TRUE(SUITE, name, CHECK_EXACT, value == want && NvnLayerError()[0] == '\0',
               "the event's value is 0x%x after a list that %s 0x1234, where 0x%x was there and "
               "0x%x is right: %s",
               (unsigned)value, add ? "adds" : "writes", 0x00a50000u, (unsigned)want,
               NvnLayerError());
}

/// A list that waits for an event to be 9, then clears the target and copies it out, with a
/// sync fenced behind it. Flushed while the event is 7, the sync must not signal within a
/// twentieth of a second; once the processor signals 9, it must, and the clear is there.
static void EventWaitCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    static _Alignas(8) NVNevent event;
    static _Alignas(8) NVNsync sync;
    static const float cyan[4] = { 0.0f, 1.0f, 1.0f, 1.0f };
    static const uint8_t cyan_bytes[4] = { 0x00, 0xff, 0xff, 0xff };
    uint8_t *const pixels = nvn.nvn.nvnMemoryPoolMap(&nvn.read_pool);
    if (pixels == NULL || !ListEvent(&event)) {
        ReportCase(SUITE, name, CHECK_NONE, false,
                   Why("nvnMemoryPoolMap gave no address, or nvnEventInitialize answered false"));
        return;
    }
    if (!nvn.nvn.nvnSyncInitialize(&sync, &nvn.device)) {
        nvn.nvn.nvnEventFinalize(&event);
        ReportCase(SUITE, name, CHECK_NONE, false, Why("nvnSyncInitialize answered false"));
        return;
    }
    // The readback's first page but its last word, which is the event's.
    memset(pixels, READ_PRESET, POOL_PAGE - 4u);
    nvn.nvn.nvnEventSignal(&event, NVN_EVENT_SIGNAL_MODE_WRITE, 7);
    nvn.nvn.nvnCommandBufferBeginRecording(&nvn.command);
    nvn.nvn.nvnCommandBufferWaitEvent(&nvn.command, &event, NVN_EVENT_WAIT_MODE_EQUAL, 9);
    RecordClear(&nvn.target, TARGET_SIZE, TARGET_SIZE, cyan);
    nvn.nvn.nvnCommandBufferBarrier(&nvn.command, NVN_BARRIER_ORDER_FRAGMENTS_BIT |
                                                      NVN_BARRIER_INVALIDATE_TEXTURE_BIT);
    // A corner of the target, which fits before the event's word.
    const NVNcopyRegion region = { 0, 0, 0, 16, 16, 1 };
    nvn.nvn.nvnCommandBufferCopyTextureToBuffer(
        &nvn.command, &nvn.target, NULL, &region,
        nvn.nvn.nvnMemoryPoolGetBufferAddress(&nvn.read_pool), NVN_COPY_FLAGS_NONE);
    const NVNcommandHandle list = nvn.nvn.nvnCommandBufferEndRecording(&nvn.command);
    if (Lacking(name)) {
        nvn.nvn.nvnSyncFinalize(&sync);
        nvn.nvn.nvnEventFinalize(&event);
        return;
    }
    nvn.nvn.nvnQueueSubmitCommands(&nvn.queue, 1, &list);
    nvn.nvn.nvnQueueFenceSync(&nvn.queue, &sync, NVN_SYNC_CONDITION_ALL_GPU_COMMANDS_COMPLETE, 0);
    nvn.nvn.nvnQueueFlush(&nvn.queue);
    const NVNsyncWaitResult held = nvn.nvn.nvnSyncWait(&sync, 50000000ull);
    nvn.nvn.nvnEventSignal(&event, NVN_EVENT_SIGNAL_MODE_WRITE, 9);
    const NVNsyncWaitResult let_go = nvn.nvn.nvnSyncWait(&sync, SYNC_WAIT_NS);
    uint32_t wrong = 0;
    for (uint32_t at = 0; at < 16u * 16u; ++at) {
        wrong += memcmp(pixels + (size_t)at * 4u, cyan_bytes, 4) != 0;
    }
    nvn.nvn.nvnQueueFinish(&nvn.queue);
    nvn.nvn.nvnSyncFinalize(&sync);
    nvn.nvn.nvnEventFinalize(&event);
    const bool signalled = let_go == NVN_SYNC_WAIT_RESULT_ALREADY_SIGNALED ||
                           let_go == NVN_SYNC_WAIT_RESULT_CONDITION_SATISFIED;
    CHECK_TRUE(SUITE, name, CHECK_EXACT,
               held == NVN_SYNC_WAIT_RESULT_TIMEOUT_EXPIRED && signalled && wrong == 0u,
               "the sync behind the list answered %d while the event was 7 (%d is a wait that "
               "ran out, which a list still waiting gives) and %d once it was 9 (0 or 1 is "
               "signalled); %u of 256 pixels of the clear were then wrong: %s",
               (int)held, (int)NVN_SYNC_WAIT_RESULT_TIMEOUT_EXPIRED, (int)let_go, (unsigned)wrong,
               NvnLayerError());
}

/// A page of a physical pool mapped into a virtual one: bytes the processor wrote through the
/// physical pool, copied by the graphics unit from the virtual pool's address.
static void VirtualPoolCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    enum { PAGE = 0x10000, BYTES = 0x400 };
    // A mapping request as a driver reads it: the physical pool, where in it, where in the
    // virtual pool, how much, and the storage class of what will be kept there.
    struct {
        NVNmemoryPool *physical;
        int64_t physical_offset;
        int64_t virtual_offset;
        uint64_t size;
        int32_t storage_class;
        int32_t pad;
    } request;
    static _Alignas(8) NVNmemoryPool physical, virtual_pool;
    void *storage = NULL;
    bool physical_up = MakePool(&physical, &storage, 2u * PAGE,
                                NVN_MEMORY_POOL_FLAGS_CPU_UNCACHED |
                                    NVN_MEMORY_POOL_FLAGS_GPU_NO_ACCESS |
                                    NVN_MEMORY_POOL_FLAGS_PHYSICAL);
    _Alignas(8) NVNmemoryPoolBuilder builder;
    nvn.nvn.nvnMemoryPoolBuilderSetDefaults(&builder);
    nvn.nvn.nvnMemoryPoolBuilderSetDevice(&builder, &nvn.device);
    nvn.nvn.nvnMemoryPoolBuilderSetStorage(&builder, NULL, 2u * PAGE);
    nvn.nvn.nvnMemoryPoolBuilderSetFlags(&builder, NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS |
                                                       NVN_MEMORY_POOL_FLAGS_GPU_CACHED |
                                                       NVN_MEMORY_POOL_FLAGS_VIRTUAL);
    const bool virtual_up =
        physical_up && nvn.nvn.nvnMemoryPoolInitialize(&virtual_pool, &builder);
    if (Lacking(name)) {
        DropPool(&physical, &storage, &physical_up);
        return;
    }
    uint8_t *const bytes = physical_up ? nvn.nvn.nvnMemoryPoolMap(&physical) : NULL;
    uint8_t *const read = nvn.nvn.nvnMemoryPoolMap(&nvn.read_pool);
    if (!virtual_up || bytes == NULL || read == NULL) {
        if (virtual_up) {
            nvn.nvn.nvnMemoryPoolFinalize(&virtual_pool);
        }
        DropPool(&physical, &storage, &physical_up);
        ReportCase(SUITE, name, CHECK_NONE, false,
                   Why("a physical pool, a virtual pool or a mapping did not come up"));
        return;
    }
    for (uint32_t at = 0; at < BYTES; ++at) {
        bytes[PAGE + 0x100 + at] = CachedByte(at);
    }
    memset(read, READ_PRESET, BYTES);
    // The physical pool's second page as the virtual pool's first.
    memset(&request, 0, sizeof(request));
    request.physical = &physical;
    request.physical_offset = PAGE;
    request.virtual_offset = 0;
    request.size = PAGE;
    const bool mapped =
        nvn.nvn.nvnMemoryPoolMapVirtual(&virtual_pool, 1, (const NVNmappingRequest *)&request);
    nvn.nvn.nvnCommandBufferBeginRecording(&nvn.command);
    nvn.nvn.nvnCommandBufferCopyBufferToBuffer(
        &nvn.command, nvn.nvn.nvnMemoryPoolGetBufferAddress(&virtual_pool) + 0x100,
        nvn.nvn.nvnMemoryPoolGetBufferAddress(&nvn.read_pool), BYTES, NVN_COPY_FLAGS_NONE);
    const NVNcommandHandle list = nvn.nvn.nvnCommandBufferEndRecording(&nvn.command);
    nvn.nvn.nvnQueueSubmitCommands(&nvn.queue, 1, &list);
    nvn.nvn.nvnQueueFinish(&nvn.queue);
    uint32_t wrong = 0, first = 0;
    for (uint32_t at = 0; at < BYTES; ++at) {
        if (read[at] != CachedByte(at) && wrong++ == 0u) {
            first = at;
        }
    }
    const uint8_t got = read[first];
    nvn.nvn.nvnMemoryPoolFinalize(&virtual_pool);
    DropPool(&physical, &storage, &physical_up);
    CHECK_TRUE(SUITE, name, CHECK_EXACT, mapped && wrong == 0u,
               "nvnMemoryPoolMapVirtual answered %d; %u of %u bytes read through the virtual "
               "pool differ from the physical pool's, the first, byte %u, 0x%02x where 0x%02x "
               "was written (0x%02x is a byte nothing copied)",
               (int)mapped, (unsigned)wrong, (unsigned)BYTES, (unsigned)first, got,
               CachedByte(first), READ_PRESET);
}

// ---- the debug layer's calls

/// How often the debug callback and the database walk were called.
static uint32_t debug_calls;

static void DebugCallback(NVNdebugCallbackSource source, NVNdebugCallbackType type, int id,
                          NVNdebugCallbackSeverity severity, const unsigned char *message,
                          void *user) {
    (void)source;
    (void)type;
    (void)id;
    (void)severity;
    (void)message;
    (void)user;
    ++debug_calls;
}

static void DebugWalk(void *object, void *user) {
    (void)object;
    (void)user;
    ++debug_calls;
}

/// The debug layer's calls as a release driver answers them, the driver a game ships with: no
/// debugger attached; a callback installed and a walk of the object database, neither of which
/// calls back; each checkpoint switch taken; two debug domains numbered one after the other;
/// no object numbered; and every object given a label, after which each says of itself what it
/// said before.
static void DebugCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    static _Alignas(8) NVNtexturePool textures;
    static _Alignas(8) NVNsamplerPool samplers;
    static _Alignas(8) NVNsampler sampler;
    static _Alignas(8) NVNbuffer buffer;
    static _Alignas(8) NVNsync sync;
    static _Alignas(8) NVNprogram program;
    const int texture_count = 96, sampler_count = 64;
    const ptrdiff_t buffer_offset = 0x800;
    const size_t buffer_bytes = 0x400;
    _Alignas(8) NVNsamplerBuilder sampler_builder;
    SamplerBuilder(&sampler_builder, 0.5f);
    _Alignas(8) NVNbufferBuilder buffer_builder;
    memset(&buffer_builder, 0, sizeof(buffer_builder));
    nvn.nvn.nvnBufferBuilderSetDevice(&buffer_builder, &nvn.device);
    nvn.nvn.nvnBufferBuilderSetDefaults(&buffer_builder);
    nvn.nvn.nvnBufferBuilderSetStorage(&buffer_builder, &nvn.read_pool, buffer_offset,
                                       buffer_bytes);
    const bool textures_up = nvn.nvn.nvnTexturePoolInitialize(&textures, &nvn.read_pool,
                                                              (ptrdiff_t)POOL_PAGE, texture_count);
    const bool samplers_up =
        textures_up && nvn.nvn.nvnSamplerPoolInitialize(&samplers, &nvn.read_pool,
                                                        3 * (ptrdiff_t)POOL_PAGE, sampler_count);
    const bool sampler_up = samplers_up && nvn.nvn.nvnSamplerInitialize(&sampler, &sampler_builder);
    const bool buffer_up = sampler_up && nvn.nvn.nvnBufferInitialize(&buffer, &buffer_builder);
    const bool sync_up = buffer_up && nvn.nvn.nvnSyncInitialize(&sync, &nvn.device);
    const bool program_up = sync_up && nvn.nvn.nvnProgramInitialize(&program, &nvn.device);
    if (program_up) {
        debug_calls = 0;
        nvn.nvn.nvnSetExternalDebuggerEnabledNVX(0);
        Gives(why, sizeof(why), "whether an external debugger is attached",
              nvn.nvn.nvnDeviceIsExternalDebuggerAttached(&nvn.device), 0);
        nvn.nvn.nvnDeviceInstallDebugCallback(&nvn.device, DebugCallback, &debug_calls, 1);
        nvn.nvn.nvnDeviceWalkDebugDatabase(&nvn.device, NVN_DEBUG_OBJECT_TYPE_MEMORY_POOL,
                                           DebugWalk, &debug_calls);
        Gives(why, sizeof(why), "the checkpoints by debug group, as taken",
              nvn.nvn.nvnDeviceSetAutomaticCheckpointsByDebugGroup(&nvn.device, 1), 1);
        Gives(why, sizeof(why), "the automatic checkpoints, as taken",
              nvn.nvn.nvnDeviceSetAutomaticCheckpointsEnable(&nvn.device, 1), 1);
        Gives(why, sizeof(why), "the checkpoints' mask, as taken",
              nvn.nvn.nvnDeviceSetAutomaticCheckpointsMask(&nvn.device, 0xffu), 1);
        Gives(why, sizeof(why), "the checkpoints' sampling interval, as taken",
              nvn.nvn.nvnDeviceSetAutomaticCheckpointsSamplingInterval(&nvn.device, 4u), 1);
        Gives(why, sizeof(why), "the checkpoints' call stacks, as taken",
              nvn.nvn.nvnDeviceSetCheckpointCallStacksEnable(&nvn.device, 1), 1);
        Gives(why, sizeof(why), "the call stacks' depth, as taken",
              nvn.nvn.nvnDeviceSetCheckpointMaxCallStacksDepth(&nvn.device, 8), 1);
        // Put back off: a later case's list is no checkpoint's business.
        nvn.nvn.nvnDeviceSetAutomaticCheckpointsEnable(&nvn.device, 0);
        nvn.nvn.nvnDeviceSetAutomaticCheckpointsByDebugGroup(&nvn.device, 0);
        nvn.nvn.nvnDeviceSetCheckpointCallStacksEnable(&nvn.device, 0);
        const NVNdebugDomainId first = nvn.nvn.nvnDeviceGenerateDebugDomainId(&nvn.device, "one");
        Gives(why, sizeof(why), "the debug domain made after another",
              nvn.nvn.nvnDeviceGenerateDebugDomainId(&nvn.device, "two"), (long long)first + 1);
        nvn.nvn.nvnDeviceSetDebugLabel(&nvn.device, "Amberlith's device");
        nvn.nvn.nvnQueueSetDebugLabel(&nvn.queue, "Amberlith's queue");
        nvn.nvn.nvnMemoryPoolSetDebugLabel(&nvn.read_pool, "Amberlith's readback pool");
        nvn.nvn.nvnTextureSetDebugLabel(&nvn.target, "Amberlith's target");
        nvn.nvn.nvnCommandBufferSetDebugLabel(&nvn.command, "Amberlith's command buffer");
        nvn.nvn.nvnBufferSetDebugLabel(&buffer, "Amberlith's buffer");
        nvn.nvn.nvnSamplerSetDebugLabel(&sampler, "Amberlith's sampler");
        nvn.nvn.nvnProgramSetDebugLabel(&program, "Amberlith's program");
        nvn.nvn.nvnSyncSetDebugLabel(&sync, "Amberlith's sync");
        nvn.nvn.nvnTexturePoolSetDebugLabel(&textures, "Amberlith's texture pool");
        nvn.nvn.nvnSamplerPoolSetDebugLabel(&samplers, "Amberlith's sampler pool");
        Gives(why, sizeof(why), "the labelled buffer's debug number",
              (long long)nvn.nvn.nvnBufferGetDebugID(&buffer), 0);
        Gives(why, sizeof(why), "the labelled texture's debug number",
              (long long)nvn.nvn.nvnTextureGetDebugID(&nvn.target), 0);
        Gives(why, sizeof(why), "the labelled sampler's debug number",
              (long long)nvn.nvn.nvnSamplerGetDebugID(&sampler), 0);
        Gives(why, sizeof(why), "the labelled pool's size",
              (long long)nvn.nvn.nvnMemoryPoolGetSize(&nvn.read_pool), READ_POOL_BYTES);
        Gives(why, sizeof(why), "the labelled texture's width",
              nvn.nvn.nvnTextureGetWidth(&nvn.target), TARGET_SIZE);
        Gives(why, sizeof(why), "the labelled buffer's size",
              (long long)nvn.nvn.nvnBufferGetSize(&buffer), (long long)buffer_bytes);
        Gives(why, sizeof(why), "the labelled texture pool's size",
              nvn.nvn.nvnTexturePoolGetSize(&textures), texture_count);
        Gives(why, sizeof(why), "the labelled sampler pool's size",
              nvn.nvn.nvnSamplerPoolGetSize(&samplers), sampler_count);
        Gives(why, sizeof(why), "the labelled queue's error",
              nvn.nvn.nvnQueueGetError(&nvn.queue, NULL), NVN_QUEUE_GET_ERROR_RESULT_GPU_NO_ERROR);
        nvn.nvn.nvnDeviceInstallDebugCallback(&nvn.device, NULL, NULL, 0);
        Gives(why, sizeof(why), "the times the debug callback and the database walk called back",
              debug_calls, 0);
    }
    if (program_up) {
        nvn.nvn.nvnProgramFinalize(&program);
    }
    if (sync_up) {
        nvn.nvn.nvnSyncFinalize(&sync);
    }
    if (buffer_up) {
        nvn.nvn.nvnBufferFinalize(&buffer);
    }
    if (sampler_up) {
        nvn.nvn.nvnSamplerFinalize(&sampler);
    }
    if (samplers_up) {
        nvn.nvn.nvnSamplerPoolFinalize(&samplers);
    }
    if (textures_up) {
        nvn.nvn.nvnTexturePoolFinalize(&textures);
    }
    if (!program_up) {
        ReportCase(SUITE, name, CHECK_NONE, false,
                   Why("a texture pool, a sampler pool, a sampler, a buffer, a sync or a program "
                       "did not initialize"));
        return;
    }
    CHECK_TRUE(SUITE, name, CHECK_EXACT, why[0] == '\0' && NvnLayerError()[0] == '\0', "%s",
               why[0] != '\0' ? why : NvnLayerError());
}

/// The first word of a linkage map, and the bytes of its header, as ScarletNX reads the map a
/// console's shader compiler writes of a stage with subroutines.
#define LINKAGE_MAGIC 0xfabffabcu
#define LINKAGE_HEADER_BYTES 80u

/// A program takes a linkage map for each of its stages that declares subroutines and for no
/// other, and says whether the maps it was given are those stages': a driver answers false to a
/// map for a stage that declares none, to no map and to no array of maps. A program no shader
/// was set for has no such stage. The map offered is a header alone, with the magic, the
/// fragment stage and its own length where a compiler's map has them.
static void SubroutineLinkageCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    static _Alignas(8) NVNprogram program;
    if (!nvn.nvn.nvnProgramInitialize(&program, &nvn.device)) {
        ReportCase(SUITE, name, CHECK_NONE, false, Why("nvnProgramInitialize answered false"));
        return;
    }
    static _Alignas(8) uint32_t header[LINKAGE_HEADER_BYTES / 4u];
    memset(header, 0, sizeof(header));
    header[0] = LINKAGE_MAGIC;
    header[1] = NVN_SHADER_STAGE_FRAGMENT;
    header[5] = LINKAGE_HEADER_BYTES;
    const NVNsubroutineLinkageMapPtr maps[1] = { header };
    Gives(why, sizeof(why), "whether a program with no subroutine took a linkage map",
          nvn.nvn.nvnProgramSetSubroutineLinkage(&program, 1, maps), 0);
    Gives(why, sizeof(why), "whether it took none of an array of linkage maps",
          nvn.nvn.nvnProgramSetSubroutineLinkage(&program, 0, maps), 0);
    Gives(why, sizeof(why), "whether it took one linkage map of no array",
          nvn.nvn.nvnProgramSetSubroutineLinkage(&program, 1, NULL), 0);
    nvn.nvn.nvnProgramFinalize(&program);
    CHECK_TRUE_FROM(SUITE, name, CHECK_EXACT, REPORT_FROM_DRIVER,
                    why[0] == '\0' && NvnLayerError()[0] == '\0', "%s",
                    why[0] != '\0' ? why : NvnLayerError());
}

#if NVN_LAYER_API >= 231
/// Records into the open command buffer the Ounce's markers and groups, which say whether they
/// name graphics or compute work: a compute group inside a graphics one, a marker in each.
static void RecordOunceMarkers(NVNdebugDomainId domain) {
    nvn.nvn.nvnCommandBufferPushDebugGroupGraphics(&nvn.command, domain, "Amberlith's clear");
    nvn.nvn.nvnCommandBufferInsertDebugMarkerGraphics(&nvn.command, domain, "before the clear");
    nvn.nvn.nvnCommandBufferPushDebugGroupCompute(&nvn.command, domain, "no compute work");
    nvn.nvn.nvnCommandBufferInsertDebugMarkerCompute(&nvn.command, domain, "nothing here");
    nvn.nvn.nvnCommandBufferPopDebugGroupCompute(&nvn.command, domain);
}

/// What the Ounce's program builder keeps comes back from it: the device, the shader data by its
/// address and its count, the sample shading mode, and the three numbers of a ray tracing
/// program, the two recursion depths and the flags. The builder makes no program, so the shader
/// data is only named and the numbers need not be a program any machine has.
static void ProgramBuilderCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    char why[256] = "";
    static const NVNshaderData named[2];
    _Alignas(8) NVNprogramBuilder builder;
    memset(&builder, 0, sizeof(builder));
    nvn.nvn.nvnProgramBuilderSetDefaults(&builder);
    nvn.nvn.nvnProgramBuilderSetDevice(&builder, &nvn.device);
    nvn.nvn.nvnProgramBuilderSetShaders(&builder, 2, named);
    nvn.nvn.nvnProgramBuilderSetSampleShading(&builder, 1);
    nvn.nvn.nvnProgramBuilderSetMaxCallableRecursionDepth(&builder, 3);
    nvn.nvn.nvnProgramBuilderSetMaxTraceRecursionDepth(&builder, 5);
    nvn.nvn.nvnProgramBuilderSetProgramRayTracingFlags(&builder, 6);
    Gives(why, sizeof(why), "the builder's device",
          (long long)(uintptr_t)nvn.nvn.nvnProgramBuilderGetDevice(&builder),
          (long long)(uintptr_t)&nvn.device);
    Gives(why, sizeof(why), "the builder's shader data",
          (long long)(uintptr_t)nvn.nvn.nvnProgramBuilderGetShaders(&builder),
          (long long)(uintptr_t)named);
    Gives(why, sizeof(why), "the builder's number of shaders",
          nvn.nvn.nvnProgramBuilderGetShaderCount(&builder), 2);
    Gives(why, sizeof(why), "the builder's sample shading",
          nvn.nvn.nvnProgramBuilderGetSampleShading(&builder), 1);
    Gives(why, sizeof(why), "the builder's greatest callable recursion depth",
          nvn.nvn.nvnProgramBuilderGetMaxCallableRecursionDepth(&builder), 3);
    Gives(why, sizeof(why), "the builder's greatest trace recursion depth",
          nvn.nvn.nvnProgramBuilderGetMaxTraceRecursionDepth(&builder), 5);
    Gives(why, sizeof(why), "the builder's ray tracing flags",
          nvn.nvn.nvnProgramBuilderGetProgramRayTracingFlags(&builder), 6);
    CHECK_TRUE(SUITE, name, CHECK_EXACT, why[0] == '\0', "%s", why);
}

/// The Ounce's program builder keeps the subroutine linkage it is given, an array of maps by
/// its address and their number, and has none until then: an Ounce game's driver writes and
/// reads the two in the builder, and its defaults put nought in both. The builder is filled with
/// a byte first, so the defaults are the driver's and not the memory's. It makes no program, so
/// the maps are only named.
static void ProgramBuilderLinkageCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    char why[256] = "";
    static const NVNsubroutineLinkageMapPtr named[3];
    _Alignas(8) NVNprogramBuilder builder;
    memset(&builder, 0xa5, sizeof(builder));
    nvn.nvn.nvnProgramBuilderSetDefaults(&builder);
    Gives(why, sizeof(why), "a new builder's linkage maps",
          (long long)(uintptr_t)nvn.nvn.nvnProgramBuilderGetSubroutineLinkage(&builder), 0);
    Gives(why, sizeof(why), "a new builder's number of linkage maps",
          nvn.nvn.nvnProgramBuilderGetSubroutineLinkageCount(&builder), 0);
    nvn.nvn.nvnProgramBuilderSetSubroutineLinkage(&builder, 3, named);
    Gives(why, sizeof(why), "the builder's linkage maps",
          (long long)(uintptr_t)nvn.nvn.nvnProgramBuilderGetSubroutineLinkage(&builder),
          (long long)(uintptr_t)named);
    Gives(why, sizeof(why), "the builder's number of linkage maps",
          nvn.nvn.nvnProgramBuilderGetSubroutineLinkageCount(&builder), 3);
    CHECK_TRUE_FROM(SUITE, name, CHECK_EXACT, REPORT_FROM_DRIVER, why[0] == '\0', "%s", why);
}

/// The Ounce's program builder keeps the hashes of its shaders' debug data it is given, by
/// their address, and has none until then: an Ounce game's driver writes their number and their
/// address in the builder, reads the address back, and its defaults put nought in both. The
/// builder is filled with a byte first, and makes no program, so the hashes are only named.
static void ProgramBuilderHashesCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    char why[256] = "";
    static const uint8_t named[2 * 32];
    _Alignas(8) NVNprogramBuilder builder;
    memset(&builder, 0xa5, sizeof(builder));
    nvn.nvn.nvnProgramBuilderSetDefaults(&builder);
    Gives(why, sizeof(why), "a new builder's debug data hashes",
          (long long)(uintptr_t)nvn.nvn.nvnProgramBuilderGetDebugDataHashes(&builder), 0);
    nvn.nvn.nvnProgramBuilderSetDebugDataHashes(&builder, 2, named);
    Gives(why, sizeof(why), "the builder's debug data hashes",
          (long long)(uintptr_t)nvn.nvn.nvnProgramBuilderGetDebugDataHashes(&builder),
          (long long)(uintptr_t)named);
    CHECK_TRUE_FROM(SUITE, name, CHECK_EXACT, REPORT_FROM_DRIVER, why[0] == '\0', "%s", why);
}

/// The scratch memory a warp of a program's ray tracing needs is a number of its ray tracing
/// part, which a program has once ray tracing modules made it one: an Ounce game's driver
/// answers nought for a program without that part, and a program no module was given has none.
static void ProgramScratchCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    static _Alignas(8) NVNprogram program;
    if (!nvn.nvn.nvnProgramInitialize(&program, &nvn.device)) {
        ReportCase(SUITE, name, CHECK_NONE, false, Why("nvnProgramInitialize answered false"));
        return;
    }
    Gives(why, sizeof(why), "the scratch bytes per warp of a program with no ray tracing",
          (long long)nvn.nvn.nvnProgramGetScratchBytesPerWarp(&program), 0);
    nvn.nvn.nvnProgramFinalize(&program);
    CHECK_TRUE_FROM(SUITE, name, CHECK_EXACT, REPORT_FROM_DRIVER, why[0] == '\0', "%s", why);
}

/// A shader group's handle is 32 bytes a ray tracing program writes for one of its groups. An
/// Ounce game's driver writes none for a program with no ray tracing part, and a program no
/// module was given has none: the bytes handed, and a guard of eight on each side, stay as they
/// were.
#define GROUP_HANDLE_BYTES 32u
#define GROUP_GUARD_BYTES 8u
static void ProgramGroupCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    static _Alignas(8) NVNprogram program;
    if (!nvn.nvn.nvnProgramInitialize(&program, &nvn.device)) {
        ReportCase(SUITE, name, CHECK_NONE, false, Why("nvnProgramInitialize answered false"));
        return;
    }
    uint8_t handle[GROUP_GUARD_BYTES + GROUP_HANDLE_BYTES + GROUP_GUARD_BYTES];
    memset(handle, 0x5a, sizeof(handle));
    const NVNshaderGroup group = { { 0, -1, -1 } };
    nvn.nvn.nvnProgramGetShaderGroupHandle(&program, group, handle + GROUP_GUARD_BYTES);
    int changed = 0;
    for (size_t i = 0; i < sizeof(handle); ++i) {
        changed += handle[i] != 0x5a ? 1 : 0;
    }
    Gives(why, sizeof(why), "the bytes written of a shader group's handle and its guards, for a "
                            "program with no ray tracing,",
          changed, 0);
    nvn.nvn.nvnProgramFinalize(&program);
    CHECK_TRUE_FROM(SUITE, name, CHECK_EXACT, REPORT_FROM_DRIVER, why[0] == '\0', "%s", why);
}

/// The ray tracing modules a builder is given, an array by its address and their number, have
/// a place of their own in it: an Ounce game's driver writes the two where no getter reads, and
/// every number the builder answers is the one it was set before the modules were.
static void ProgramBuilderModulesCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    char why[256] = "";
    static const NVNshaderData named[2];
    static const uint8_t modules[64];
    _Alignas(8) NVNprogramBuilder builder;
    memset(&builder, 0, sizeof(builder));
    nvn.nvn.nvnProgramBuilderSetDefaults(&builder);
    nvn.nvn.nvnProgramBuilderSetDevice(&builder, &nvn.device);
    nvn.nvn.nvnProgramBuilderSetShaders(&builder, 2, named);
    nvn.nvn.nvnProgramBuilderSetSampleShading(&builder, 1);
    nvn.nvn.nvnProgramBuilderSetMaxCallableRecursionDepth(&builder, 3);
    nvn.nvn.nvnProgramBuilderSetMaxTraceRecursionDepth(&builder, 5);
    nvn.nvn.nvnProgramBuilderSetProgramRayTracingFlags(&builder, 6);
    nvn.nvn.nvnProgramBuilderSetRayTracingModules(&builder, 4, modules);
    Gives(why, sizeof(why), "the builder's device, after the modules,",
          (long long)(uintptr_t)nvn.nvn.nvnProgramBuilderGetDevice(&builder),
          (long long)(uintptr_t)&nvn.device);
    Gives(why, sizeof(why), "the builder's shader data, after the modules,",
          (long long)(uintptr_t)nvn.nvn.nvnProgramBuilderGetShaders(&builder),
          (long long)(uintptr_t)named);
    Gives(why, sizeof(why), "the builder's number of shaders, after the modules,",
          nvn.nvn.nvnProgramBuilderGetShaderCount(&builder), 2);
    Gives(why, sizeof(why), "the builder's sample shading, after the modules,",
          nvn.nvn.nvnProgramBuilderGetSampleShading(&builder), 1);
    Gives(why, sizeof(why), "the builder's greatest callable recursion depth, after the modules,",
          nvn.nvn.nvnProgramBuilderGetMaxCallableRecursionDepth(&builder), 3);
    Gives(why, sizeof(why), "the builder's greatest trace recursion depth, after the modules,",
          nvn.nvn.nvnProgramBuilderGetMaxTraceRecursionDepth(&builder), 5);
    Gives(why, sizeof(why), "the builder's ray tracing flags, after the modules,",
          nvn.nvn.nvnProgramBuilderGetProgramRayTracingFlags(&builder), 6);
    CHECK_TRUE_FROM(SUITE, name, CHECK_EXACT, REPORT_FROM_DRIVER, why[0] == '\0', "%s", why);
}

/// A queue builder's ray tracing memory size is one more of its sizes: unset on a new builder,
/// and once set said to be, with the size, as an Ounce game's driver keeps it beside the others.
/// The builder makes no queue.
static void QueueRayBuilderCase(const char *name) {
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
    Gives(why, sizeof(why), "whether a new builder has a ray tracing memory size set",
          nvn.nvn.nvnQueueBuilderGetRayTracingMemorySize(&builder, &size), 0);
    nvn.nvn.nvnQueueBuilderSetRayTracingMemorySize(&builder, 0x300000);
    Gives(why, sizeof(why), "whether a ray tracing memory size that was set is said to be",
          nvn.nvn.nvnQueueBuilderGetRayTracingMemorySize(&builder, &size), 1);
    Gives(why, sizeof(why), "the builder's ray tracing memory size", size, 0x300000);
    CHECK_TRUE_FROM(SUITE, name, CHECK_EXACT, REPORT_FROM_DRIVER,
                    why[0] == '\0' && NvnLayerError()[0] == '\0', "%s",
                    why[0] != '\0' ? why : NvnLayerError());
}

/// The ray tracing memory a queue has used is one of the counts nvnQueueResetMemoryUsageCounts
/// puts back to nought, in an Ounce game's driver as the other three: nought right after it.
static void QueueRayUsedCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    nvn.nvn.nvnQueueFinish(&nvn.queue);
    nvn.nvn.nvnQueueResetMemoryUsageCounts(&nvn.queue);
    Gives(why, sizeof(why), "the ray tracing memory the queue used since its counts were reset",
          (long long)nvn.nvn.nvnQueueGetTotalRayTracingMemoryUsed(&nvn.queue), 0);
    CHECK_TRUE_FROM(SUITE, name, CHECK_EXACT, REPORT_FROM_DRIVER,
                    why[0] == '\0' && NvnLayerError()[0] == '\0', "%s",
                    why[0] != '\0' ? why : NvnLayerError());
}

/// A clear recorded inside the Ounce's debug groups, between its markers, is the clear: the
/// markers name work for the debug layer and record nothing a picture shows.
static void OunceMarkersCase(const char *name, const float rgba[4], const uint8_t expected[4]) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    uint8_t *const pixels = nvn.nvn.nvnMemoryPoolMap(&nvn.read_pool);
    if (pixels == NULL) {
        ReportCase(SUITE, name, CHECK_NONE, false, Why("nvnMemoryPoolMap gave no address"));
        return;
    }
    memset(pixels, READ_PRESET, TARGET_BYTES);
    const NVNdebugDomainId domain = nvn.nvn.nvnDeviceGenerateDebugDomainId(&nvn.device, "ounce");
    nvn.nvn.nvnCommandBufferBeginRecording(&nvn.command);
    RecordOunceMarkers(domain);
    RecordClear(&nvn.target, TARGET_SIZE, TARGET_SIZE, rgba);
    nvn.nvn.nvnCommandBufferPopDebugGroupGraphics(&nvn.command, domain);
    nvn.nvn.nvnCommandBufferBarrier(&nvn.command, NVN_BARRIER_ORDER_FRAGMENTS_BIT |
                                                      NVN_BARRIER_INVALIDATE_TEXTURE_BIT);
    const NVNcopyRegion region = { 0, 0, 0, TARGET_SIZE, TARGET_SIZE, 1 };
    nvn.nvn.nvnCommandBufferCopyTextureToBuffer(
        &nvn.command, &nvn.target, NULL, &region,
        nvn.nvn.nvnMemoryPoolGetBufferAddress(&nvn.read_pool), NVN_COPY_FLAGS_NONE);
    const NVNcommandHandle list = nvn.nvn.nvnCommandBufferEndRecording(&nvn.command);
    nvn.nvn.nvnQueueSubmitCommands(&nvn.queue, 1, &list);
    nvn.nvn.nvnQueueFinish(&nvn.queue);
    uint32_t wrong = 0;
    uint8_t first[4] = { 0, 0, 0, 0 };
    for (uint32_t at = 0; at < TARGET_SIZE * TARGET_SIZE; ++at) {
        if (memcmp(pixels + (size_t)at * 4u, expected, 4) != 0 && wrong++ == 0u) {
            memcpy(first, pixels + (size_t)at * 4u, 4);
        }
    }
    CHECK_TRUE(SUITE, name, CHECK_EXACT, list != 0u && wrong == 0u,
               "%u of %u pixels wrong after the clear between the markers, the first "
               "%02x%02x%02x%02x, expected %02x%02x%02x%02x (0x%02x is a byte nothing copied)",
               (unsigned)wrong, (unsigned)(TARGET_SIZE * TARGET_SIZE), first[0], first[1],
               first[2], first[3], expected[0], expected[1], expected[2], expected[3],
               READ_PRESET);
}
#endif

// ---- the window

// The suite's one window, which every window case shares: a second swapchain on the default
// window after a first is what Eden never came back from (the game suite says the same).
static struct {
    _Alignas(8) NVNmemoryPool pool;
    _Alignas(8) NVNtexture textures[WINDOW_TEXTURES];
    _Alignas(8) NVNwindow window;
    void *storage;
    bool pool_up;
    bool window_up;
    int made;
} win;

static void WindowDown(void) {
    if (win.window_up) {
        nvn.nvn.nvnWindowFinalize(&win.window);
        win.window_up = false;
    }
    for (int i = 0; i < win.made; ++i) {
        nvn.nvn.nvnTextureFinalize(&win.textures[i]);
    }
    win.made = 0;
    if (win.pool_up) {
        DropPool(&win.pool, &win.storage, &win.pool_up);
    }
}

/// Two 1280x720 textures in a pool of their own, and a window of them on the default native
/// window. False with the step in @p why, and nothing left up.
static bool WindowUp(char *why, size_t why_size) {
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
    win.pool_up = MakePool(&win.pool, &win.storage, RoundUp(stride * WINDOW_TEXTURES, POOL_PAGE),
                           NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS |
                               NVN_MEMORY_POOL_FLAGS_GPU_CACHED);
    if (!win.pool_up) {
        snprintf(why, why_size, "the window's pool: %s", Why("no memory, or false"));
        return false;
    }
    for (win.made = 0; win.made < WINDOW_TEXTURES; ++win.made) {
        nvn.nvn.nvnTextureBuilderSetStorage(&builder, &win.pool,
                                            (ptrdiff_t)(stride * (size_t)win.made));
        if (!nvn.nvn.nvnTextureInitialize(&win.textures[win.made], &builder)) {
            snprintf(why, why_size, "window texture %d: nvnTextureInitialize: %s", win.made,
                     Why("false"));
            WindowDown();
            return false;
        }
    }
    NVNtexture *const list[WINDOW_TEXTURES] = { &win.textures[0], &win.textures[1] };
    _Alignas(8) NVNwindowBuilder window_builder;
    nvn.nvn.nvnWindowBuilderSetDefaults(&window_builder);
    nvn.nvn.nvnWindowBuilderSetDevice(&window_builder, &nvn.device);
    nvn.nvn.nvnWindowBuilderSetNativeWindow(&window_builder, NativeWindow());
    nvn.nvn.nvnWindowBuilderSetTextures(&window_builder, WINDOW_TEXTURES, list);
    win.window_up = nvn.nvn.nvnWindowInitialize(&win.window, &window_builder);
    if (!win.window_up) {
        snprintf(why, why_size, "nvnWindowInitialize: %s", Why("false"));
        WindowDown();
        return false;
    }
    return true;
}

/// Clears window texture @p index to @p rgba and presents it.
static void ClearAndPresent(int index, const float rgba[4]) {
    nvn.nvn.nvnCommandBufferBeginRecording(&nvn.command);
    RecordClear(&win.textures[index], WINDOW_WIDTH, WINDOW_HEIGHT, rgba);
    const NVNcommandHandle list = nvn.nvn.nvnCommandBufferEndRecording(&nvn.command);
    nvn.nvn.nvnQueueSubmitCommands(&nvn.queue, 1, &list);
    nvn.nvn.nvnQueuePresentTexture(&nvn.queue, &win.window, index);
}

/// A texture of the window through the queue's acquire: its index, or -1 with the answer in
/// @p why.
static int Acquire(const char *when, char *why, size_t why_size) {
    int index = -1;
    const NVNqueueAcquireTextureResult got =
        nvn.nvn.nvnQueueAcquireTexture(&nvn.queue, &win.window, &index);
    if (got != NVN_QUEUE_ACQUIRE_TEXTURE_RESULT_SUCCESS || index < 0 ||
        index >= WINDOW_TEXTURES) {
        snprintf(why, why_size, "%s: nvnQueueAcquireTexture answered %d with texture %d of %d: "
                 "%s",
                 when, (int)got, index, WINDOW_TEXTURES, Why("no reason given"));
        return -1;
    }
    return index;
}

/// The window's two textures, each acquired, cleared and presented once. Nothing is read back:
/// what reaches the screen is the compositor's, and the case says only whether every step came
/// back as NVN promises. False with the step in @p why.
static bool Present(char *why, size_t why_size) {
    // Two frames, so that both textures go round once: a cleared green, then a cleared blue.
    static const float colours[WINDOW_TEXTURES][4] = { { 0.0f, 1.0f, 0.0f, 1.0f },
                                                       { 0.0f, 0.0f, 1.0f, 1.0f } };
    static const char *const frames[WINDOW_TEXTURES] = { "frame 0", "frame 1" };
    bool ok = true;
    for (int frame = 0; ok && frame < WINDOW_TEXTURES; ++frame) {
        const int index = Acquire(frames[frame], why, why_size);
        ok = index >= 0;
        if (ok) {
            ClearAndPresent(index, colours[frame]);
        }
    }
    nvn.nvn.nvnQueueFinish(&nvn.queue);
    if (ok && NvnLayerError()[0] != '\0') {
        snprintf(why, why_size, "%s", NvnLayerError());
        ok = false;
    }
    return ok;
}

/// What a window builder keeps comes back from it: the device, the native window, each texture
/// and how many, the present interval and the number of textures the window is to hand in
/// turn. The builder makes no window, and its three textures are only named.
static void WindowBuilderCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    static _Alignas(8) NVNtexture named[3];
    NVNtexture *const list[3] = { &named[0], &named[1], &named[2] };
    _Alignas(8) NVNwindowBuilder builder;
    nvn.nvn.nvnWindowBuilderSetDefaults(&builder);
    nvn.nvn.nvnWindowBuilderSetDevice(&builder, &nvn.device);
    nvn.nvn.nvnWindowBuilderSetNativeWindow(&builder, NativeWindow());
    nvn.nvn.nvnWindowBuilderSetTextures(&builder, 3, list);
    nvn.nvn.nvnWindowBuilderSetPresentInterval(&builder, 2);
    nvn.nvn.nvnWindowBuilderSetNumActiveTextures(&builder, 2);
    Gives(why, sizeof(why), "the builder's device",
          (long long)(uintptr_t)nvn.nvn.nvnWindowBuilderGetDevice(&builder),
          (long long)(uintptr_t)&nvn.device);
    Gives(why, sizeof(why), "the builder's native window",
          (long long)(uintptr_t)nvn.nvn.nvnWindowBuilderGetNativeWindow(&builder),
          (long long)(uintptr_t)NativeWindow());
    Gives(why, sizeof(why), "the builder's number of textures",
          nvn.nvn.nvnWindowBuilderGetNumTextures(&builder), 3);
    static const char *const which[3] = { "the builder's texture 0", "the builder's texture 1",
                                          "the builder's texture 2" };
    for (int i = 0; i < 3; ++i) {
        Gives(why, sizeof(why), which[i],
              (long long)(uintptr_t)nvn.nvn.nvnWindowBuilderGetTexture(&builder, i),
              (long long)(uintptr_t)list[i]);
    }
    Gives(why, sizeof(why), "the builder's present interval",
          nvn.nvn.nvnWindowBuilderGetPresentInterval(&builder), 2);
    Gives(why, sizeof(why), "the builder's number of active textures",
          nvn.nvn.nvnWindowBuilderGetNumActiveTextures(&builder), 2);
    CHECK_TRUE(SUITE, name, CHECK_EXACT, why[0] == '\0' && NvnLayerError()[0] == '\0', "%s",
               why[0] != '\0' ? why : NvnLayerError());
}

/// The window up: what it says of itself, and that a number of active textures, a present
/// interval and a crop set on it come back. A frame is presented under the crop and the
/// interval, and the window is left as it was made.
static void WindowSaysCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    // A label is the debug layer's: the window says the same with one.
    nvn.nvn.nvnWindowSetDebugLabel(&win.window, "Amberlith's window");
    Gives(why, sizeof(why), "the window's native window",
          (long long)(uintptr_t)nvn.nvn.nvnWindowGetNativeWindow(&win.window),
          (long long)(uintptr_t)NativeWindow());
    Gives(why, sizeof(why), "the window's number of textures",
          nvn.nvn.nvnWindowGetNumTextures(&win.window), WINDOW_TEXTURES);
    nvn.nvn.nvnWindowSetNumActiveTextures(&win.window, 1);
    Gives(why, sizeof(why), "the window's number of active textures after 1 was set",
          nvn.nvn.nvnWindowGetNumActiveTextures(&win.window), 1);
    nvn.nvn.nvnWindowSetNumActiveTextures(&win.window, WINDOW_TEXTURES);
    Gives(why, sizeof(why), "the window's number of active textures after all were set",
          nvn.nvn.nvnWindowGetNumActiveTextures(&win.window), WINDOW_TEXTURES);
    nvn.nvn.nvnWindowSetPresentInterval(&win.window, 2);
    Gives(why, sizeof(why), "the window's present interval",
          nvn.nvn.nvnWindowGetPresentInterval(&win.window), 2);
    nvn.nvn.nvnWindowSetCrop(&win.window, 16, 8, 640, 360);
    NVNrectangle crop = { -1, -1, -1, -1 };
    nvn.nvn.nvnWindowGetCrop(&win.window, &crop);
    Gives(why, sizeof(why), "the crop's x", crop.x, 16);
    Gives(why, sizeof(why), "the crop's y", crop.y, 8);
    Gives(why, sizeof(why), "the crop's width", crop.width, 640);
    Gives(why, sizeof(why), "the crop's height", crop.height, 360);
    // One frame as the window now is: a quarter of a cleared texture, shown for two refreshes.
    static const float cyan[4] = { 0.0f, 1.0f, 1.0f, 1.0f };
    char step[160] = "";
    const int index = Acquire("under the crop", step, sizeof(step));
    if (index >= 0) {
        ClearAndPresent(index, cyan);
    } else if (why[0] == '\0') {
        snprintf(why, sizeof(why), "%s", step);
    }
    nvn.nvn.nvnQueueFinish(&nvn.queue);
    nvn.nvn.nvnWindowSetPresentInterval(&win.window, 1);
    Gives(why, sizeof(why), "the window's present interval set back",
          nvn.nvn.nvnWindowGetPresentInterval(&win.window), 1);
    nvn.nvn.nvnWindowSetCrop(&win.window, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    nvn.nvn.nvnWindowGetCrop(&win.window, &crop);
    Gives(why, sizeof(why), "the whole texture's crop width", crop.width, WINDOW_WIDTH);
    Gives(why, sizeof(why), "the whole texture's crop height", crop.height, WINDOW_HEIGHT);
    CHECK_TRUE(SUITE, name, CHECK_EXACT, why[0] == '\0' && NvnLayerError()[0] == '\0', "%s",
               why[0] != '\0' ? why : NvnLayerError());
}

/// The window's own acquire, which hands a texture at once with a sync that signals when the
/// display has let it go: the program waits for the sync itself, where nvnQueueAcquireTexture
/// has the queue wait. The texture is then cleared and presented.
static void WindowSyncCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    static _Alignas(8) NVNsync sync;
    if (!nvn.nvn.nvnSyncInitialize(&sync, &nvn.device)) {
        ReportCase(SUITE, name, CHECK_NONE, false, Why("nvnSyncInitialize answered false"));
        return;
    }
    int index = -1;
    const NVNwindowAcquireTextureResult got =
        nvn.nvn.nvnWindowAcquireTexture(&win.window, &sync, &index);
    const bool acquired = got == NVN_WINDOW_ACQUIRE_TEXTURE_RESULT_SUCCESS && index >= 0 &&
                          index < WINDOW_TEXTURES;
    NVNsyncWaitResult wait = NVN_SYNC_WAIT_RESULT_FAILED;
    if (acquired) {
        wait = nvn.nvn.nvnSyncWait(&sync, SYNC_WAIT_NS);
        static const float magenta[4] = { 1.0f, 0.0f, 1.0f, 1.0f };
        ClearAndPresent(index, magenta);
    }
    nvn.nvn.nvnQueueFinish(&nvn.queue);
    nvn.nvn.nvnSyncFinalize(&sync);
    const bool free_now = wait == NVN_SYNC_WAIT_RESULT_ALREADY_SIGNALED ||
                          wait == NVN_SYNC_WAIT_RESULT_CONDITION_SATISFIED;
    CHECK_TRUE(SUITE, name, CHECK_EXACT, acquired && free_now && NvnLayerError()[0] == '\0',
               "nvnWindowAcquireTexture answered %d with texture %d of %d; its sync then "
               "answered a wait of up to 10 s with %d (0 is signalled, 1 satisfied, 2 timed out, "
               "3 failed, which is also what stands when nothing was acquired): %s",
               (int)got, index, WINDOW_TEXTURES, (int)wait, Why("no reason given"));
}

#if NVN_LAYER_API >= 231
// The Ounce's windows: a colour space and the HDR metadata of the display and of the content.

/// The ten numbers of a display's metadata as the setters take them: the three primaries and
/// the white point, each x then y, then the least and the greatest luminance.
#define HDR_DISPLAY 10

/// Keeps in @p why the first of @p got that is not @p want's bits.
static void HdrGives(char *why, size_t why_size, const char *whose, const float *got,
                     const float *want, int count) {
    static const char *const part[HDR_DISPLAY + 2] = {
        "red x", "red y", "green x", "green y", "blue x", "blue y", "white x", "white y",
        "least luminance", "greatest luminance", "greatest content light level",
        "greatest frame average light level" };
    for (int i = 0; i < count; ++i) {
        char what[96];
        snprintf(what, sizeof(what), "%s %s, as bits,", whose, part[i]);
        Gives(why, why_size, what, Bits(got[i]), Bits(want[i]));
    }
}

/// What a new Ounce builder holds, as ScarletNX read it from an Ounce game's driver: colour
/// space 0, Rec. 709's primaries with a D65 white point, 0.03 to 1000 nits, and content light
/// levels of 200 and 60. Then a colour space and metadata set on it, none a default and every
/// float exact, read back.
static void WindowHdrBuilderCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    static const float initial[HDR_DISPLAY + 2] = { 0.64f, 0.33f, 0.3f, 0.6f, 0.15f, 0.06f,
                                                    0.3127f, 0.329f, 0.03f, 1000.0f, 200.0f,
                                                    60.0f };
    static const float set[HDR_DISPLAY + 2] = { 0.75f, 0.25f, 0.125f, 0.875f, 0.0625f, 0.03125f,
                                                0.3125f, 0.328125f, 0.5f, 4000.0f, 1500.0f,
                                                400.0f };
    float got[HDR_DISPLAY + 2];
    _Alignas(8) NVNwindowBuilder builder;
    nvn.nvn.nvnWindowBuilderSetDefaults(&builder);
    Gives(why, sizeof(why), "a new builder's colour space",
          nvn.nvn.nvnWindowBuilderGetColorSpace(&builder), 0);
    memset(got, 0, sizeof(got));
    nvn.nvn.nvnWindowBuilderGetHdrDisplayMetadata(&builder, &got[0], &got[1], &got[2], &got[3],
                                                  &got[4], &got[5], &got[6], &got[7], &got[8],
                                                  &got[9]);
    nvn.nvn.nvnWindowBuilderGetHdrContentMetadata(&builder, &got[10], &got[11]);
    HdrGives(why, sizeof(why), "a new builder's", got, initial, HDR_DISPLAY + 2);
    nvn.nvn.nvnWindowBuilderSetColorSpace(&builder, 1);
    nvn.nvn.nvnWindowBuilderSetHdrDisplayMetadata(&builder, set[0], set[1], set[2], set[3],
                                                  set[4], set[5], set[6], set[7], set[8],
                                                  set[9]);
    nvn.nvn.nvnWindowBuilderSetHdrContentMetadata(&builder, set[10], set[11]);
    Gives(why, sizeof(why), "the builder's colour space",
          nvn.nvn.nvnWindowBuilderGetColorSpace(&builder), 1);
    memset(got, 0, sizeof(got));
    nvn.nvn.nvnWindowBuilderGetHdrDisplayMetadata(&builder, &got[0], &got[1], &got[2], &got[3],
                                                  &got[4], &got[5], &got[6], &got[7], &got[8],
                                                  &got[9]);
    nvn.nvn.nvnWindowBuilderGetHdrContentMetadata(&builder, &got[10], &got[11]);
    HdrGives(why, sizeof(why), "the builder's", got, set, HDR_DISPLAY + 2);
    CHECK_TRUE_FROM(SUITE, name, CHECK_EXACT, REPORT_FROM_DRIVER, why[0] == '\0', "%s", why);
}

/// The window's own metadata, set on it and read back, and put back to what a new builder has.
static void WindowHdrCase(const char *name) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    NvnLayerClearError();
    char why[256] = "";
    static const float initial[HDR_DISPLAY + 2] = { 0.64f, 0.33f, 0.3f, 0.6f, 0.15f, 0.06f,
                                                    0.3127f, 0.329f, 0.03f, 1000.0f, 200.0f,
                                                    60.0f };
    static const float set[HDR_DISPLAY + 2] = { 0.6875f, 0.3125f, 0.25f, 0.6875f, 0.125f,
                                                0.0625f, 0.3125f, 0.34375f, 0.25f, 2000.0f,
                                                800.0f, 300.0f };
    float got[HDR_DISPLAY + 2];
    memset(got, 0, sizeof(got));
    nvn.nvn.nvnWindowGetHdrDisplayMetadata(&win.window, &got[0], &got[1], &got[2], &got[3],
                                           &got[4], &got[5], &got[6], &got[7], &got[8], &got[9]);
    nvn.nvn.nvnWindowGetHdrContentMetadata(&win.window, &got[10], &got[11]);
    HdrGives(why, sizeof(why), "the window's, from a builder left as new,", got, initial,
             HDR_DISPLAY + 2);
    nvn.nvn.nvnWindowSetHdrDisplayMetadata(&win.window, set[0], set[1], set[2], set[3], set[4],
                                           set[5], set[6], set[7], set[8], set[9]);
    nvn.nvn.nvnWindowSetHdrContentMetadata(&win.window, set[10], set[11]);
    memset(got, 0, sizeof(got));
    nvn.nvn.nvnWindowGetHdrDisplayMetadata(&win.window, &got[0], &got[1], &got[2], &got[3],
                                           &got[4], &got[5], &got[6], &got[7], &got[8], &got[9]);
    nvn.nvn.nvnWindowGetHdrContentMetadata(&win.window, &got[10], &got[11]);
    HdrGives(why, sizeof(why), "the window's", got, set, HDR_DISPLAY + 2);
    nvn.nvn.nvnWindowSetHdrDisplayMetadata(&win.window, initial[0], initial[1], initial[2],
                                           initial[3], initial[4], initial[5], initial[6],
                                           initial[7], initial[8], initial[9]);
    nvn.nvn.nvnWindowSetHdrContentMetadata(&win.window, initial[10], initial[11]);
    CHECK_TRUE_FROM(SUITE, name, CHECK_EXACT, REPORT_FROM_DRIVER, why[0] == '\0', "%s", why);
}
#endif

/// Which NVN the suite's cases ask: the layer's own functions, which are what a console runs of
/// the layer and what no runtime that runs NVN itself reaches once a function is looked up; or
/// the ones the program looks up, which such a runtime answers.
typedef enum {
    LAYER_OWN,
    LAYER_LOOKED_UP,
} LayerWay;

// A case of the layer's own way goes by its name after these words, so the two ways' cases are
// two lines of a report; the names are kept here for as long as the run lasts.
#define WAY_OWN "the layer's own: "
#define WAY_NAMES 128u
#define WAY_NAME_MOST 127u   // what a report's line keeps of a name
static LayerWay way_now;
static char way_names[WAY_NAMES][WAY_NAME_MOST + 1u];
static uint32_t way_count;

/// @p name as the way under way has it.
static const char *Way(const char *name) {
    if (way_now != LAYER_OWN) {
        return name;
    }
    if (way_count >= WAY_NAMES) {
        return WAY_OWN "a case past the 128 names the suite keeps for this way";
    }
    snprintf(way_names[way_count], sizeof(way_names[0]), WAY_OWN "%s", name);
    return way_names[way_count++];
}

/// The cases on the suite's own device, from the functions' coming to the window, down @p way.
static void LayerCases(LayerWay way) {
    way_now = way;
    way_count = 0;
    const char *const RESOLVE = Way("the program's NVN functions resolve");
    const char *const DEVICE = Way("the device initializes");
    const char *const VERSION = Way("the device answers the layer's API version");
    const char *const TARGET = Way("a queue, its pools and a 64x64 target come up");
    const char *const RED = Way("clear to opaque red");
    const char *const MID = Way("clear to a mid channel value");
    const char *const DEVICE_QUIET = Way("clears before and after deferred finalizes, the shader "
                                         "cache switch and a list's handle given up are there");
    const char *const WINDOW = Way("a window acquires and presents both its textures");
    const char *const COPIES = Way("two copy textures and a staging pool come up");
    const char *const ROUND_TRIP = Way("a picture copied into a texture and back keeps every texel");
    const char *const TEXTURE = Way("a texture copied into another keeps every texel");
    const char *const BUFFER = Way("a buffer copied into another keeps every byte");
    const char *const MIRROR_Y = Way("a texture copied to a buffer with MIRROR_Y lands upside down");
    const char *const MIRROR_X = Way("a texture copied into another with MIRROR_X lands mirrored");
    const char *const STRIDED = Way("a copy to a buffer with a row stride of 288 keeps the bytes "
                                       "between rows");
    const char *const BUILDER = Way("a texture builder gives back each number it was set");
    const char *const VIEW = Way("a texture view says which of its parts were set, and with "
                                    "what");
    const char *const LEVELS_UP = Way("a texture of three levels comes up and says what its "
                                         "builder had");
    const char *const LEVELS_COPY = Way("a picture copied into each level of a texture "
                                           "through a view comes back from that level");
    const char *const POOL = Way("a memory pool and its builder give back what they were "
                                    "set, and the pool maps its storage");
    const char *const POOL_CACHE = Way("a page flushed from a cached pool is what a copy "
                                          "reads, and the copy is there after an invalidate");
    const char *const FENCED = Way("a sync fenced on the queue behind a clear signals, and "
                                      "the clear is there with no finish");
    const char *const FENCED_LIST = Way("a sync fenced inside the list behind a clear "
                                           "signals, and the clear is there with no finish");
    const char *const WAITED = Way("a queue and a list told to wait for a sync that "
                                      "signalled run their clear");
    const char *const UNFENCED = Way("a wait on a sync nothing fenced fails, with a timeout "
                                        "or without");
    const char *const NO_OPENGL = Way("with no OpenGL, a sync from an OpenGL sync and an "
                                         "OpenGL sync from a sync both fail");
    const char *const NATIVE_HANDLE = Way("a pool builder given a native handle forgets its "
                                             "storage, and takes storage again");
    const char *const QUEUE_ERROR = Way("the queue has no error after work that was valid");
    const char *const QUEUE_BUILDER = Way("a queue builder gives back its flags and says "
                                             "which of its sizes were set");
    const char *const EVENT = Way("an event keeps what the processor signals: a write, an "
                                     "add and an add that wraps");
    const char *const EVENT_WRITTEN = Way("a list writes an event's value over the one the "
                                             "processor signalled");
    const char *const EVENT_ADDED = Way("a list adds to an event's value");
    const char *const EVENT_WAITED = Way("a list that waits for an event's value runs when "
                                            "the processor signals it, and not before");
    const char *const VIRTUAL_POOL = Way("a virtual pool's page is the physical pool's page "
                                            "mapped into it");
    const char *const TARGETS = Way("a 1D texture, each layer of an array and each slice "
                                       "of a 3D texture keep a picture of their own");
    const char *const QUEUE_REST = Way("a queue builder gives back its memory and priority, "
                                          "and a queue's memory counts are nought once reset");
    const char *const TEXTURE_MORE = Way("a texture builder and a view give back their "
                                            "swizzle, depth and stencil mode and the rest");
    const char *const TEXTURE_SELF = Way("a texture says its swizzle, storage and address, "
                                            "and is the same as its twin alone");
    const char *const TEXELS = Way("texels the processor writes into a texture are what it "
                                      "and the graphics unit read back");
    const char *const TEXELS_STRIDED = Way("a part of a texture written and read with row "
                                              "strides keeps the bytes between rows");
    const char *const BLEND_STATE = Way("a blend state gives back its target, functions, "
                                           "equations and advanced mode");
    const char *const COLOR_STATE = Way("a channel mask and a colour state give back "
                                        "channels, blending, the logic operation and the "
                                        "alpha test");
    const char *const DEPTH_STENCIL_STATE = Way("a depth and stencil state gives back its "
                                                   "depth test and each face's stencil");
    const char *const MULTISAMPLE_STATE = Way("a multisample state gives back its samples, "
                                                 "its switches and its sample locations' grid");
    const char *const POLYGON_VERTEX_STATE = Way("a polygon state, a vertex attribute and a "
                                                    "vertex stream give back what they were set");
    const char *const BUFFER_RANGE = Way("a buffer is a range of its pool: its address, its "
                                            "mapping, and what it and its builder say");
    const char *const BUFFER_COPY = Way("bytes flushed from one buffer and copied to "
                                           "another are there after an invalidate");
    const char *const SAMPLER_BUILDER = Way("a sampler builder gives back each number it "
                                               "was set");
    const char *const SAMPLER = Way("a sampler says what its builder had, and is the same "
                                       "as another of that state alone");
    const char *const DESCRIPTOR_POOLS = Way("the texture and sampler pools say where they "
                                                "are, and a builder registers as its sampler");
    const char *const DEBUG = Way("the debug layer's calls are answered as a release "
                                     "driver answers them, and a label changes no object");
    const char *const SUBROUTINE_LINKAGE = Way("a program with no subroutine takes no linkage "
                                                  "map, one offered or none");
#if NVN_LAYER_API >= 231
    const char *const OUNCE_MARKERS = Way("a clear between the Ounce's graphics and compute "
                                             "markers and groups is the clear");
    const char *const PROGRAM_BUILDER = Way("an Ounce program builder gives back what it "
                                               "was set, a ray tracing program's numbers too");
    const char *const BUILDER_LINKAGE = Way("an Ounce program builder gives back the "
                                               "subroutine linkage it was set, and none before");
    const char *const PROGRAM_SCRATCH = Way("an Ounce program with no ray tracing asks for no "
                                               "scratch memory per warp");
    const char *const BUILDER_HASHES = Way("an Ounce program builder gives back the debug "
                                              "data hashes it was set, and none before");
    const char *const PROGRAM_GROUP = Way("an Ounce program with no ray tracing leaves a "
                                             "shader group's handle as it was");
    const char *const BUILDER_MODULES = Way("ray tracing modules given to an Ounce program "
                                               "builder leave what it answers as set");
    const char *const QUEUE_RAY_BUILDER = Way("an Ounce queue builder says whether its ray "
                                                 "tracing memory size was set, and gives it back");
    const char *const QUEUE_RAY_USED = Way("an Ounce queue's ray tracing memory count is "
                                              "nought once its counts are reset");
#endif
    const char *const WINDOW_BUILDER = Way("a window builder gives back what it was set");
    const char *const WINDOW_SAYS = Way("a window says what it is of, and keeps the active "
                                           "textures, the interval and the crop it is set");
    const char *const WINDOW_SYNC = Way("a texture the window hands with a sync is free "
                                           "once the sync signals, and presents");
#if NVN_LAYER_API >= 231
    const char *const WINDOW_HDR_BUILDER = Way("an Ounce window builder starts with the "
                                                  "driver's colour space and HDR metadata, "
                                                  "and gives back what it is set");
    const char *const WINDOW_HDR = Way("an Ounce window has its builder's HDR metadata, and "
                                          "gives back what it is set");
#endif
    const char *const AFTER_DEVICE[] = { VERSION,  TARGET,   RED,      MID,     DEVICE_QUIET,
                                         COPIES,
                                         ROUND_TRIP, TEXTURE, BUFFER, MIRROR_Y, MIRROR_X,
                                         STRIDED,  BUILDER, VIEW, LEVELS_UP, LEVELS_COPY,
                                         POOL, POOL_CACHE, FENCED, FENCED_LIST, WAITED,
                                         UNFENCED, NO_OPENGL, NATIVE_HANDLE,
                                         QUEUE_ERROR, QUEUE_BUILDER, EVENT,
                                         SAMPLER_BUILDER, SAMPLER, DESCRIPTOR_POOLS,
                                         BUFFER_RANGE, BUFFER_COPY, DEBUG, SUBROUTINE_LINKAGE,
                                         BLEND_STATE, COLOR_STATE, DEPTH_STENCIL_STATE,
                                         MULTISAMPLE_STATE, POLYGON_VERTEX_STATE,
                                         TEXTURE_MORE, TEXTURE_SELF, TEXELS, TEXELS_STRIDED,
                                         TARGETS, QUEUE_REST,
                                         EVENT_WRITTEN, EVENT_ADDED, EVENT_WAITED,
                                         VIRTUAL_POOL,
                                         WINDOW_BUILDER,
#if NVN_LAYER_API >= 231
                                         OUNCE_MARKERS, PROGRAM_BUILDER, WINDOW_HDR_BUILDER,
                                         WINDOW_HDR, BUILDER_LINKAGE, PROGRAM_SCRATCH,
                                         PROGRAM_GROUP, BUILDER_MODULES, QUEUE_RAY_BUILDER,
                                         QUEUE_RAY_USED, BUILDER_HASHES,
#endif
                                         WINDOW, WINDOW_SAYS, WINDOW_SYNC };
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
    // The layer's own functions come without a lookup, and only before the first one.
    const bool resolved = way == LAYER_OWN ? NvnLayerOwnProcs(&nvn.nvn)
                                           : NvnLoadProcs(&nvn.nvn, missing, sizeof(missing));
    CHECK_TRUE(SUITE, RESOLVE, CHECK_NONE, resolved, "%s%s",
               way == LAYER_OWN ? "the layer gave none of its own functions: " : "the lookup gave no ",
               way == LAYER_OWN ? NvnLayerError() : missing);
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
#ifdef NVN_LAYER_GAME_DRIVER
        // The game's driver has its own version; its major is one this build has the objects
        // of, or the device above was made in storage of another size.
        CHECK_TRUE(SUITE, VERSION, CHECK_EXACT,
                   major >= NVN_LAYER_GAME_API_MAJOR_OLDEST && major <= NVN_LAYER_API_MAJOR &&
                       minor >= 0,
                   "API %d.%d, the game's driver's, where this build has the objects of API %d "
                   "to %d",
                   major, minor, NVN_LAYER_GAME_API_MAJOR_OLDEST, NVN_LAYER_API_MAJOR);
#else
        CHECK_TRUE(SUITE, VERSION, CHECK_EXACT,
                   major == NVN_LAYER_API_MAJOR && minor == NVN_LAYER_API_MINOR,
                   "API %d.%d, where the layer answers %d.%d", major, minor, NVN_LAYER_API_MAJOR,
                   NVN_LAYER_API_MINOR);
#endif
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
    DeviceQuietCase(DEVICE_QUIET);

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
    NoOpenGlCase(NO_OPENGL);
    NativeHandleCase(NATIVE_HANDLE);
    QueueErrorCase(QUEUE_ERROR);
    QueueBuilderCase(QUEUE_BUILDER);
    EventCase(EVENT);
    SamplerBuilderCase(SAMPLER_BUILDER);
    SamplerCase(SAMPLER);
    DescriptorPoolsCase(DESCRIPTOR_POOLS);
    if (!nvn.cached_pool_up) {
        ReportSkip(SUITE, BUFFER_RANGE, REPORT_REASON_DEPENDS,
                   "the pool cached to the processor did not come up");
        ReportSkip(SUITE, BUFFER_COPY, REPORT_REASON_DEPENDS,
                   "the pool cached to the processor did not come up");
    } else {
        BufferCase(BUFFER_RANGE);
        BufferCopyCase(BUFFER_COPY);
    }
    BlendStateCase(BLEND_STATE);
    ColorStateCase(COLOR_STATE);
    DepthStencilStateCase(DEPTH_STENCIL_STATE);
    MultisampleStateCase(MULTISAMPLE_STATE);
    PolygonVertexStateCase(POLYGON_VERTEX_STATE);
    TextureMoreCase(TEXTURE_MORE);
    if (!nvn.levels_up) {
        ReportSkip(SUITE, TEXTURE_SELF, REPORT_REASON_DEPENDS,
                   "the texture of three levels did not come up");
    } else {
        TextureSelfCase(TEXTURE_SELF);
    }
    if (!nvn.copy_a_up) {
        ReportSkip(SUITE, TEXELS, REPORT_REASON_DEPENDS, "the copy textures did not come up");
        ReportSkip(SUITE, TEXELS_STRIDED, REPORT_REASON_DEPENDS,
                   "the copy textures did not come up");
    } else {
        TexelsCase(TEXELS);
        TexelsStridedCase(TEXELS_STRIDED);
    }
    if (!nvn.stage_pool_up) {
        ReportSkip(SUITE, TARGETS, REPORT_REASON_DEPENDS, "the staging pool did not come up");
    } else {
        TargetsCase(TARGETS);
    }
    QueueRestCase(QUEUE_REST);
    EventListCase(EVENT_WRITTEN, false);
    EventListCase(EVENT_ADDED, true);
    EventWaitCase(EVENT_WAITED);
    VirtualPoolCase(VIRTUAL_POOL);

    DebugCase(DEBUG);
    SubroutineLinkageCase(SUBROUTINE_LINKAGE);
    WindowBuilderCase(WINDOW_BUILDER);
#if NVN_LAYER_API >= 231
    // A colour no case before left in the target or in the readback.
    static const float teal[4] = { 0.0f, 128.0f / 255.0f, 128.0f / 255.0f, 1.0f };
    static const uint8_t teal_bytes[4] = { 0x00, 0x80, 0x80, 0xff };
    OunceMarkersCase(OUNCE_MARKERS, teal, teal_bytes);
    ProgramBuilderCase(PROGRAM_BUILDER);
    ProgramBuilderLinkageCase(BUILDER_LINKAGE);
    ProgramBuilderHashesCase(BUILDER_HASHES);
    ProgramScratchCase(PROGRAM_SCRATCH);
    ProgramGroupCase(PROGRAM_GROUP);
    ProgramBuilderModulesCase(BUILDER_MODULES);
    QueueRayBuilderCase(QUEUE_RAY_BUILDER);
    QueueRayUsedCase(QUEUE_RAY_USED);
    WindowHdrBuilderCase(WINDOW_HDR_BUILDER);
#endif

    // The cases that need the window up, after the one that brings it up.
    const char *const ON_WINDOW[] = { WINDOW_SAYS, WINDOW_SYNC,
#if NVN_LAYER_API >= 231
                                      WINDOW_HDR,
#endif
    };
    const size_t on_window = sizeof(ON_WINDOW) / sizeof(ON_WINDOW[0]);
    const char *no_window = NULL;
#if NVN_LAYER_API < 231
    const bool screen_up = ViewerScreenUp();
#else
    const bool screen_up = false;    // the Switch 2's frontend has no screen of its own
#endif
    if (way == LAYER_OWN) {
        // The layer's own window is a deko3d swapchain on the default window, and the looked up
        // way's case makes a second there afterwards, which is what Eden never came back from.
        static const char *const SECOND =
            "the layer's own window would be a first swapchain on the default window, and the "
            "case of the looked up way makes the one this run has";
        ReportSkip(SUITE, WINDOW, REPORT_REASON_UNAVAILABLE, SECOND);
        for (size_t i = 0; i < on_window; ++i) {
            ReportSkip(SUITE, ON_WINDOW[i], REPORT_REASON_UNAVAILABLE, SECOND);
        }
    } else if (screen_up) {
        // The program's own screen holds the default window, and a swapchain made on it after
        // the screen is what Eden never came back from (the game suite says the same). The start
        // screen leaves it up to the run's end, noprogress or not; a quick run from the mode file
        // never opens it.
        static const char *const HELD =
            "the program's screen holds the window: a run begun on the start screen keeps it up "
            "to its end, noprogress or not; quick in the mode file runs this case";
        ReportSkip(SUITE, WINDOW, REPORT_REASON_UNAVAILABLE, HELD);
        for (size_t i = 0; i < on_window; ++i) {
            ReportSkip(SUITE, ON_WINDOW[i], REPORT_REASON_UNAVAILABLE, HELD);
        }
    } else if (NativeWindowOr(&no_window) == NULL) {
        ReportSkip(SUITE, WINDOW, REPORT_REASON_UNAVAILABLE, no_window);
        for (size_t i = 0; i < on_window; ++i) {
            ReportSkip(SUITE, ON_WINDOW[i], REPORT_REASON_UNAVAILABLE, no_window);
        }
    } else if (ReportStart(SUITE, WINDOW)) {
        NvnLayerClearError();
        const bool window_up = WindowUp(why, sizeof(why));
        const bool presented = window_up && Present(why, sizeof(why));
        CHECK_TRUE(SUITE, WINDOW, CHECK_NONE, presented, "%s", why);
        if (!window_up) {
            for (size_t i = 0; i < on_window; ++i) {
                ReportSkip(SUITE, ON_WINDOW[i], REPORT_REASON_DEPENDS,
                           "the window did not come up");
            }
        } else {
            WindowSaysCase(WINDOW_SAYS);
            WindowSyncCase(WINDOW_SYNC);
#if NVN_LAYER_API >= 231
            WindowHdrCase(WINDOW_HDR);
#endif
        }
        WindowDown();
    }
    TearDown();
}

#ifndef NVN_LAYER_GAME_DRIVER
void RunNvn(void) {
#if NVN_LAYER_API < 231
    // Before the first lookup: a runtime that runs a game's NVN itself takes each function at
    // the address its name resolves to, the layer's own from then on with it.
    NvnSceneFirst();
    NvnCommandFirst();
    LayerCases(LAYER_OWN);
    LayerCases(LAYER_LOOKED_UP);
    // With the suite's own device down, whatever the cases above came to: the scene's way
    // makes its own.
    NvnSceneLast();
    NvnCommandLast();
#else
    // The scene is a Switch program's: its shaders are the Switch's code and two of its three
    // ways are deko3d's.
    LayerCases(LAYER_LOOKED_UP);
#endif
}
#else
// In a game the family is the looked up way alone: the layer has no functions of its own there,
// and the scene's and the commands' programs are uam's code under a control section the layer
// writes, which a game's own driver does not read.
void RunNvnInGame(void) {
    LayerCases(LAYER_LOOKED_UP);
}
#endif
