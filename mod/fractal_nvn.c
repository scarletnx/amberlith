// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <switch.h>

#include <nvnlayer.h>

#include "fractal_nvn.h"
#include "fractalzoom.h"
#include "mod_shaders.h"
#include "screen.h"
#include "tests/tests.h"

// The fractal zooms' way through the game's NVN (fractalzoom.h): the plain program's two passes,
// the same GLSL compiled for NVN's layout of the driver's banks (uam --nvn, mod.mk), on a device
// of the game's driver. A frame is the compute pass that counts every pixel's iterations into a
// storage buffer, the fragment pass that colours the target from them, and the frame shown:
// through the mod's screens while they are up, the target read back, as the plain program shows
// it through its screen; otherwise drawn at twice its size on a window of the mod's own, a third
// pass in place of the plain program's textured quad, which needs no texture.

#define TARGET_WIDTH FRACTAL_WIDTH
#define TARGET_HEIGHT FRACTAL_HEIGHT
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define WINDOW_TEXTURES 2
#define COUNTS_BYTES ((size_t)FRACTAL_PIXELS * 4u)
#define UNIFORM_SIZE 1280u
#define POOL_PAGE 0x1000u
#define POOL_ALIGNMENT 0x10000u
#define COMMAND_BYTES 0x10000u
#define CONTROL_BYTES 0x2000u
#define CODE_BYTES 0x10000u
/// The uniform slots, the counts, each judged frame's counts and image, and the readback.
#define DATA_BYTES (2u * 0x100u * 8u + COUNTS_BYTES * (2u + 2u * FRACTAL_JUDGED) + 0x10000u)
/// How long a window's image is waited for before the frame is given up.
#define ACQUIRE_WAIT_NANOSECONDS 10000000000ull

_Static_assert(FRACTAL_FRAME_BYTES <= UNIFORM_SIZE, "a frame's block fits its slot");
_Static_assert(FRACTAL_WIDTH * 2 == WINDOW_WIDTH && FRACTAL_HEIGHT * 2 == WINDOW_HEIGHT,
               "the target shown at twice its size fills the window");

typedef struct {
    _Alignas(8) NVNmemoryPool pool;
    void *storage;
    size_t size;
    size_t used;
    bool up;
} Pool;

enum { PROGRAM_COUNT, PROGRAM_COLOUR, PROGRAM_SHOW, PROGRAMS };

static struct {
    NvnProcs nvn;
    bool device_up;
    bool queue_up;
    bool command_up;
    bool target_up;
    bool window_up;
    bool sync_up;
    bool on_screen;
    int textures_made;
    int programs_made;
    _Alignas(8) NVNdevice device;
    _Alignas(8) NVNqueue queue;
    _Alignas(8) NVNcommandBuffer command;
    _Alignas(8) NVNtexture target;
    _Alignas(8) NVNtexture textures[WINDOW_TEXTURES];
    _Alignas(8) NVNwindow window;
    _Alignas(8) NVNsync sync;
    _Alignas(8) NVNprogram programs[PROGRAMS];
    Pool command_pool;
    Pool code_pool;
    Pool data_pool;
    Pool image_pool;
    void *control;
    void *sections[PROGRAMS][2];
    NVNbufferAddress uniform_gpu[FRACTAL_SLOTS];
    FractalFrame *uniform_cpu[FRACTAL_SLOTS];
    NVNbufferAddress counts;
    NVNbufferAddress kept_counts_gpu[FRACTAL_JUDGED];
    const uint8_t *kept_counts[FRACTAL_JUDGED];
    NVNbufferAddress kept_image_gpu[FRACTAL_JUDGED];
    const uint8_t *kept_image[FRACTAL_JUDGED];
    NVNbufferAddress readback_gpu;
    const uint8_t *readback;
    /// The last frame's image on the window and its uniform slot, shown again while paused.
    int last_image;
    uint32_t last_slot;
    char failure[240];
} g_zoom;

static size_t RoundUp(size_t value, size_t to) {
    return (value + to - 1u) / to * to;
}

/// Keeps the first failure; a way with one is no longer ready.
static void Fail(const char *what) {
    if (g_zoom.failure[0] == '\0') {
        snprintf(g_zoom.failure, sizeof(g_zoom.failure), "%s", what);
    }
}

static bool MakePool(Pool *pool, size_t size, int flags) {
    pool->size = RoundUp(size, POOL_PAGE);
    pool->storage = aligned_alloc(POOL_ALIGNMENT, RoundUp(pool->size, POOL_ALIGNMENT));
    if (pool->storage == NULL) {
        return false;
    }
    memset(pool->storage, 0, pool->size);
    _Alignas(8) NVNmemoryPoolBuilder builder;
    g_zoom.nvn.nvnMemoryPoolBuilderSetDefaults(&builder);
    g_zoom.nvn.nvnMemoryPoolBuilderSetDevice(&builder, &g_zoom.device);
    g_zoom.nvn.nvnMemoryPoolBuilderSetStorage(&builder, pool->storage, pool->size);
    g_zoom.nvn.nvnMemoryPoolBuilderSetFlags(&builder, flags);
    pool->up = g_zoom.nvn.nvnMemoryPoolInitialize(&pool->pool, &builder);
    pool->used = 0;
    return pool->up;
}

static void DropPool(Pool *pool) {
    if (pool->up) {
        g_zoom.nvn.nvnMemoryPoolFinalize(&pool->pool);
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

/// @p bytes of the data pool: their address for the graphics unit and for the processor, or
/// false with @p what in @p why.
static bool Data(size_t bytes, size_t alignment, NVNbufferAddress *gpu, uint8_t **cpu,
                 const char *what, char *why, size_t why_size) {
    const size_t at = Carve(&g_zoom.data_pool, bytes, alignment);
    uint8_t *const mapped = (uint8_t *)g_zoom.nvn.nvnMemoryPoolMap(&g_zoom.data_pool.pool);
    if (at == SIZE_MAX || mapped == NULL) {
        snprintf(why, why_size, "%s: %s", what,
                 mapped == NULL ? "nvnMemoryPoolMap gave no address for the data pool"
                                : "no room left in the data pool");
        return false;
    }
    *gpu = g_zoom.nvn.nvnMemoryPoolGetBufferAddress(&g_zoom.data_pool.pool) + at;
    *cpu = mapped + at;
    return true;
}

/// A builder of a 2D RGBA8 texture of @p flags.
static void Builder(NVNtextureBuilder *builder, int width, int height, int flags) {
    memset(builder, 0, sizeof(*builder));
    g_zoom.nvn.nvnTextureBuilderSetDevice(builder, &g_zoom.device);
    g_zoom.nvn.nvnTextureBuilderSetDefaults(builder);
    g_zoom.nvn.nvnTextureBuilderSetFlags(builder, flags);
    g_zoom.nvn.nvnTextureBuilderSetTarget(builder, NVN_TEXTURE_TARGET_2D);
    g_zoom.nvn.nvnTextureBuilderSetFormat(builder, NVN_FORMAT_RGBA8);
    g_zoom.nvn.nvnTextureBuilderSetSize2D(builder, width, height);
}

/// What the textures need of the image pool, as the driver answers each one's storage: the
/// target, and the window's images when there is no screen to show the frames through.
static bool ImagePoolUp(bool window, char *why, size_t why_size) {
    _Alignas(8) NVNtextureBuilder builder;
    Builder(&builder, TARGET_WIDTH, TARGET_HEIGHT, 0);
    size_t bytes = RoundUp(g_zoom.nvn.nvnTextureBuilderGetStorageSize(&builder), POOL_ALIGNMENT);
    if (window) {
        Builder(&builder, WINDOW_WIDTH, WINDOW_HEIGHT, NVN_TEXTURE_FLAGS_DISPLAY);
        bytes += WINDOW_TEXTURES *
                 RoundUp(g_zoom.nvn.nvnTextureBuilderGetStorageSize(&builder), POOL_ALIGNMENT);
    }
    if (!MakePool(&g_zoom.image_pool, bytes,
                  NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS | NVN_MEMORY_POOL_FLAGS_GPU_CACHED)) {
        snprintf(why, why_size, "the image pool of %zu bytes: no memory, or "
                 "nvnMemoryPoolInitialize answered false",
                 bytes);
        return false;
    }
    return true;
}

/// A 2D RGBA8 texture of @p flags in the image pool.
static bool MakeTexture(NVNtexture *texture, int width, int height, int flags, const char *what,
                        char *why, size_t why_size) {
    _Alignas(8) NVNtextureBuilder builder;
    Builder(&builder, width, height, flags);
    const size_t size = g_zoom.nvn.nvnTextureBuilderGetStorageSize(&builder);
    const size_t alignment = g_zoom.nvn.nvnTextureBuilderGetStorageAlignment(&builder);
    const size_t at = size == 0u || alignment == 0u || alignment > POOL_ALIGNMENT
                          ? SIZE_MAX
                          : Carve(&g_zoom.image_pool, size, alignment);
    if (at == SIZE_MAX) {
        snprintf(why, why_size, "%s, %dx%d: its storage is %zu bytes at an alignment of %zu, "
                 "which the image pool's %zu bytes, %zu used, cannot hold",
                 what, width, height, size, alignment, g_zoom.image_pool.size,
                 g_zoom.image_pool.used);
        return false;
    }
    g_zoom.nvn.nvnTextureBuilderSetStorage(&builder, &g_zoom.image_pool.pool, (ptrdiff_t)at);
    if (!g_zoom.nvn.nvnTextureInitialize(texture, &builder)) {
        snprintf(why, why_size, "%s: nvnTextureInitialize answered false", what);
        return false;
    }
    return true;
}

/// One stage from @p module, a whole DKSH compiled for NVN's layout: its control section, kept
/// in @p section for as long as the program lives, and its code copied into the code pool.
static bool Stage(const void *module, size_t bytes, void **section, NVNshaderData *stage,
                  const char *what, char *why, size_t why_size) {
    uint8_t *const code_memory = (uint8_t *)g_zoom.nvn.nvnMemoryPoolMap(&g_zoom.code_pool.pool);
    *section = aligned_alloc(8u, RoundUp(NVN_LAYER_CONTROL_BYTES, 8u));
    NvnLayerShaderCode code;
    NvnLayerClearError();
    if (*section == NULL || code_memory == NULL ||
        !NvnLayerShaderControl(module, bytes, *section, &code)) {
        snprintf(why, why_size, "%s: %s", what,
                 NvnLayerError()[0] != '\0' ? NvnLayerError()
                                            : "no memory for its control section, or no code "
                                              "pool mapping");
        return false;
    }
    const size_t at = Carve(&g_zoom.code_pool, code.code_bytes, 0x100u);
    if (at == SIZE_MAX) {
        snprintf(why, why_size, "%s: no 0x%x bytes left in the code pool", what,
                 (unsigned)code.code_bytes);
        return false;
    }
    memcpy(code_memory + at, code.code, code.code_bytes);
    stage->data = g_zoom.nvn.nvnMemoryPoolGetBufferAddress(&g_zoom.code_pool.pool) + at;
    stage->control = *section;
    return true;
}

/// Program @p index of @p count stages, from @p modules.
static bool Program(int index, int count, const uint8_t *const *modules, const size_t *sizes,
                    const char *what, char *why, size_t why_size) {
    NVNshaderData stages[2];
    for (int i = 0; i < count; ++i) {
        if (!Stage(modules[i], sizes[i], &g_zoom.sections[index][i], &stages[i], what, why,
                   why_size)) {
            return false;
        }
    }
    if (!g_zoom.nvn.nvnProgramInitialize(&g_zoom.programs[index], &g_zoom.device)) {
        snprintf(why, why_size, "%s: nvnProgramInitialize answered false", what);
        return false;
    }
    g_zoom.programs_made = index + 1;
    if (!g_zoom.nvn.nvnProgramSetShaders(&g_zoom.programs[index], count, stages)) {
        snprintf(why, why_size, "%s: nvnProgramSetShaders answered false", what);
        return false;
    }
    return true;
}

/// The device, its queue and command buffer, and the pools.
static bool DeviceUp(char *why, size_t why_size) {
    char missing[64] = "";
    if (!NvnLoadProcs(&g_zoom.nvn, missing, sizeof(missing))) {
        snprintf(why, why_size, "the game's driver gave no %s", missing);
        return false;
    }
    _Alignas(8) NVNdeviceBuilder device_builder;
    g_zoom.nvn.nvnDeviceBuilderSetDefaults(&device_builder);
    g_zoom.nvn.nvnDeviceBuilderSetFlags(&device_builder, 0);
    if (!(g_zoom.device_up = g_zoom.nvn.nvnDeviceInitialize(&g_zoom.device, &device_builder))) {
        snprintf(why, why_size, "nvnDeviceInitialize answered false");
        return false;
    }
    _Alignas(8) NVNqueueBuilder queue_builder;
    g_zoom.nvn.nvnQueueBuilderSetDevice(&queue_builder, &g_zoom.device);
    g_zoom.nvn.nvnQueueBuilderSetDefaults(&queue_builder);
    if (!(g_zoom.queue_up = g_zoom.nvn.nvnQueueInitialize(&g_zoom.queue, &queue_builder))) {
        snprintf(why, why_size, "nvnQueueInitialize answered false");
        return false;
    }
    int padding = 0;
    g_zoom.nvn.nvnDeviceGetInteger(&g_zoom.device,
                                   NVN_DEVICE_INFO_SHADER_CODE_MEMORY_POOL_PADDING_SIZE,
                                   &padding);
    const int cpu = NVN_MEMORY_POOL_FLAGS_CPU_UNCACHED | NVN_MEMORY_POOL_FLAGS_GPU_CACHED;
    if (!MakePool(&g_zoom.command_pool, COMMAND_BYTES, cpu) ||
        !MakePool(&g_zoom.code_pool, CODE_BYTES + (size_t)(padding > 0 ? padding : 0),
                  cpu | NVN_MEMORY_POOL_FLAGS_SHADER_CODE) ||
        !MakePool(&g_zoom.data_pool, DATA_BYTES, cpu)) {
        snprintf(why, why_size, "a memory pool: no memory, or nvnMemoryPoolInitialize answered "
                 "false (code padding %d)",
                 padding);
        return false;
    }
    if (!(g_zoom.command_up =
              g_zoom.nvn.nvnCommandBufferInitialize(&g_zoom.command, &g_zoom.device))) {
        snprintf(why, why_size, "nvnCommandBufferInitialize answered false");
        return false;
    }
    g_zoom.control = aligned_alloc(8u, CONTROL_BYTES);
    if (g_zoom.control == NULL) {
        snprintf(why, why_size, "no memory for %u bytes of control memory",
                 (unsigned)CONTROL_BYTES);
        return false;
    }
    g_zoom.nvn.nvnCommandBufferAddCommandMemory(&g_zoom.command, &g_zoom.command_pool.pool, 0,
                                                COMMAND_BYTES);
    g_zoom.nvn.nvnCommandBufferAddControlMemory(&g_zoom.command, g_zoom.control, CONTROL_BYTES);
    return true;
}

/// The window of the mod's own, with two images, when its screens are not up.
static bool WindowUp(char *why, size_t why_size) {
    const char *no_window = NULL;
    void *const native = NvnGameWindow(&no_window);
    if (native == NULL) {
        snprintf(why, why_size, "no way to the display: %s", no_window);
        return false;
    }
    for (g_zoom.textures_made = 0; g_zoom.textures_made < WINDOW_TEXTURES;
         ++g_zoom.textures_made) {
        if (!MakeTexture(&g_zoom.textures[g_zoom.textures_made], WINDOW_WIDTH, WINDOW_HEIGHT,
                         NVN_TEXTURE_FLAGS_DISPLAY, "a window image", why, why_size)) {
            return false;
        }
    }
    NVNtexture *const list[WINDOW_TEXTURES] = { &g_zoom.textures[0], &g_zoom.textures[1] };
    _Alignas(8) NVNwindowBuilder builder;
    g_zoom.nvn.nvnWindowBuilderSetDefaults(&builder);
    g_zoom.nvn.nvnWindowBuilderSetDevice(&builder, &g_zoom.device);
    g_zoom.nvn.nvnWindowBuilderSetNativeWindow(&builder, (NVNnativeWindow)native);
    g_zoom.nvn.nvnWindowBuilderSetTextures(&builder, WINDOW_TEXTURES, list);
    if (!(g_zoom.window_up = g_zoom.nvn.nvnWindowInitialize(&g_zoom.window, &builder))) {
        snprintf(why, why_size, "nvnWindowInitialize on the mod's window answered false");
        return false;
    }
    if (!(g_zoom.sync_up = g_zoom.nvn.nvnSyncInitialize(&g_zoom.sync, &g_zoom.device))) {
        snprintf(why, why_size, "nvnSyncInitialize answered false");
        return false;
    }
    return true;
}

static bool Up(char *why, size_t why_size) {
    memset(&g_zoom, 0, sizeof(g_zoom));
    g_zoom.last_image = -1;
    if (!ModGraphicsReady(why, why_size) || !DeviceUp(why, why_size)) {
        return false;
    }
    const uint8_t *const count_modules[1] = { fractal_comp_dksh };
    const size_t count_sizes[1] = { (size_t)fractal_comp_dksh_size };
    const uint8_t *const colour_modules[2] = { fullscreen_vert_dksh, fractal_frag_dksh };
    const size_t colour_sizes[2] = { (size_t)fullscreen_vert_dksh_size,
                                     (size_t)fractal_frag_dksh_size };
    const uint8_t *const show_modules[2] = { fullscreen_vert_dksh, fractal_show_frag_dksh };
    const size_t show_sizes[2] = { (size_t)fullscreen_vert_dksh_size,
                                   (size_t)fractal_show_frag_dksh_size };
    if (!Program(PROGRAM_COUNT, 1, count_modules, count_sizes, "the counting program", why,
                 why_size) ||
        !Program(PROGRAM_COLOUR, 2, colour_modules, colour_sizes, "the colouring program", why,
                 why_size) ||
        !Program(PROGRAM_SHOW, 2, show_modules, show_sizes, "the showing program", why,
                 why_size)) {
        return false;
    }
    int alignment = 0;
    g_zoom.nvn.nvnDeviceGetInteger(&g_zoom.device, NVN_DEVICE_INFO_UNIFORM_BUFFER_ALIGNMENT,
                                   &alignment);
    const size_t uniform_alignment = alignment > 0 ? (size_t)alignment : 0x100u;
    uint8_t *cpu = NULL;
    for (uint32_t s = 0; s < FRACTAL_SLOTS; ++s) {
        if (!Data(UNIFORM_SIZE, uniform_alignment, &g_zoom.uniform_gpu[s], &cpu,
                  "a frame's uniform block", why, why_size)) {
            return false;
        }
        g_zoom.uniform_cpu[s] = (FractalFrame *)(void *)cpu;
    }
    if (!Data(COUNTS_BYTES, 0x100u, &g_zoom.counts, &cpu, "the counts", why, why_size)) {
        return false;
    }
    for (uint32_t j = 0; j < FRACTAL_JUDGED; ++j) {
        if (!Data(COUNTS_BYTES, 0x100u, &g_zoom.kept_counts_gpu[j], &cpu,
                  "a judged frame's counts", why, why_size)) {
            return false;
        }
        g_zoom.kept_counts[j] = cpu;
        if (!Data(FRACTAL_IMAGE_BYTES, 0x100u, &g_zoom.kept_image_gpu[j], &cpu,
                  "a judged frame's image", why, why_size)) {
            return false;
        }
        g_zoom.kept_image[j] = cpu;
    }
    if (!Data(FRACTAL_IMAGE_BYTES, 0x100u, &g_zoom.readback_gpu, &cpu, "the frame read back",
              why, why_size)) {
        return false;
    }
    g_zoom.readback = cpu;
    // The display is the mod's screens' while they are up, and stays so, as on the plain
    // program.
    g_zoom.on_screen = NvnGameScreenUp();
    if (!ImagePoolUp(!g_zoom.on_screen, why, why_size)) {
        return false;
    }
    if (!(g_zoom.target_up = MakeTexture(&g_zoom.target, TARGET_WIDTH, TARGET_HEIGHT, 0,
                                         "the target", why, why_size))) {
        return false;
    }
    return g_zoom.on_screen || WindowUp(why, why_size);
}

static int Acquire(void) {
    if (g_zoom.on_screen) {
        return 0;
    }
    int index = -1;
    const NVNwindowAcquireTextureResult got =
        g_zoom.nvn.nvnWindowAcquireTexture(&g_zoom.window, &g_zoom.sync, &index);
    if (got != NVN_WINDOW_ACQUIRE_TEXTURE_RESULT_SUCCESS || index < 0 ||
        index >= WINDOW_TEXTURES) {
        char what[120];
        snprintf(what, sizeof(what), "nvnWindowAcquireTexture answered %d with image %d of %d",
                 (int)got, index, WINDOW_TEXTURES);
        Fail(what);
        return -1;
    }
    const NVNsyncWaitResult wait =
        g_zoom.nvn.nvnSyncWait(&g_zoom.sync, ACQUIRE_WAIT_NANOSECONDS);
    if (wait != NVN_SYNC_WAIT_RESULT_ALREADY_SIGNALED &&
        wait != NVN_SYNC_WAIT_RESULT_CONDITION_SATISFIED) {
        char what[120];
        snprintf(what, sizeof(what), "the sync of window image %d answered a wait of up to 10 s "
                 "with %d",
                 index, (int)wait);
        Fail(what);
    }
    return index;
}

static FractalFrame *Block(uint32_t slot) {
    return g_zoom.uniform_cpu[slot];
}

/// The states a graphics pass draws with, every one said: an NVN command buffer starts with
/// none.
static void States(NVNtexture *target, int width, int height) {
    const NvnProcs *const n = &g_zoom.nvn;
    NVNcommandBuffer *const list = &g_zoom.command;
    const NVNtexture *const colors[1] = { target };
    n->nvnCommandBufferSetRenderTargets(list, 1, colors, NULL, NULL, NULL);
    n->nvnCommandBufferSetViewport(list, 0, 0, width, height);
    n->nvnCommandBufferSetScissor(list, 0, 0, width, height);
    _Alignas(8) NVNblendState blend;
    n->nvnBlendStateSetDefaults(&blend);
    n->nvnCommandBufferBindBlendState(list, &blend);
    _Alignas(8) NVNchannelMaskState mask;
    n->nvnChannelMaskStateSetDefaults(&mask);
    n->nvnCommandBufferBindChannelMaskState(list, &mask);
    _Alignas(8) NVNcolorState color;
    n->nvnColorStateSetDefaults(&color);
    n->nvnCommandBufferBindColorState(list, &color);
    _Alignas(8) NVNmultisampleState multisample;
    n->nvnMultisampleStateSetDefaults(&multisample);
    n->nvnCommandBufferBindMultisampleState(list, &multisample);
    _Alignas(8) NVNpolygonState polygon;
    n->nvnPolygonStateSetDefaults(&polygon);
    n->nvnPolygonStateSetCullFace(&polygon, NVN_FACE_NONE);
    n->nvnCommandBufferBindPolygonState(list, &polygon);
    _Alignas(8) NVNdepthStencilState depth;
    n->nvnDepthStencilStateSetDefaults(&depth);
    n->nvnDepthStencilStateSetDepthTestEnable(&depth, 0);
    n->nvnDepthStencilStateSetDepthWriteEnable(&depth, 0);
    n->nvnCommandBufferBindDepthStencilState(list, &depth);
    n->nvnCommandBufferBindVertexAttribState(list, 0, NULL);
    n->nvnCommandBufferBindVertexStreamState(list, 0, NULL);
}

/// A pass of program @p program over the whole of @p target, reading slot @p slot's block and
/// the counts.
static void Colour(int program, uint32_t slot, NVNtexture *target, int width, int height) {
    const NvnProcs *const n = &g_zoom.nvn;
    NVNcommandBuffer *const list = &g_zoom.command;
    States(target, width, height);
    n->nvnCommandBufferBindProgram(list, &g_zoom.programs[program],
                                   NVN_SHADER_STAGE_VERTEX_BIT | NVN_SHADER_STAGE_FRAGMENT_BIT);
    n->nvnCommandBufferBindUniformBuffer(list, NVN_SHADER_STAGE_FRAGMENT, 0,
                                         g_zoom.uniform_gpu[slot], UNIFORM_SIZE);
    n->nvnCommandBufferBindStorageBuffer(list, NVN_SHADER_STAGE_FRAGMENT, 0, g_zoom.counts,
                                         COUNTS_BYTES);
    n->nvnCommandBufferDrawArrays(list, NVN_DRAW_PRIMITIVE_TRIANGLES, 0, 3);
}

/// Begins a recording on the whole of the command buffer's memory again. A game's driver hands
/// out command and control memory from what it was given and never takes it back by itself: given
/// it once, a driver of API 55 ran out after some hundred frames and aborted (a frame's list,
/// nvnCommandBufferDispatchCompute's reserve). Each frame here waits for the graphics unit, so
/// the same memory is free to be given again.
static void BeginRecording(void) {
    const NvnProcs *const n = &g_zoom.nvn;
    n->nvnCommandBufferAddCommandMemory(&g_zoom.command, &g_zoom.command_pool.pool, 0,
                                        COMMAND_BYTES);
    n->nvnCommandBufferAddControlMemory(&g_zoom.command, g_zoom.control, CONTROL_BYTES);
    n->nvnCommandBufferBeginRecording(&g_zoom.command);
}

/// Ends the recording, submits it and waits for the graphics unit.
static void Submit(const char *what) {
    const NVNcommandHandle handle = g_zoom.nvn.nvnCommandBufferEndRecording(&g_zoom.command);
    if (handle == 0u) {
        char said[160];
        snprintf(said, sizeof(said), "%s: nvnCommandBufferEndRecording gave no list", what);
        Fail(said);
        return;
    }
    g_zoom.nvn.nvnQueueSubmitCommands(&g_zoom.queue, 1, &handle);
    g_zoom.nvn.nvnQueueFinish(&g_zoom.queue);
}

static void Draw(uint32_t slot, uint32_t judged, int image) {
    const NvnProcs *const n = &g_zoom.nvn;
    NVNcommandBuffer *const list = &g_zoom.command;
    g_zoom.last_slot = slot;
    BeginRecording();
    // The processor rewrote the slot after the graphics unit last read it.
    n->nvnCommandBufferBarrier(list, NVN_BARRIER_INVALIDATE_SHADER_BIT);

    // The counts: one invocation a pixel.
    n->nvnCommandBufferBindProgram(list, &g_zoom.programs[PROGRAM_COUNT],
                                   NVN_SHADER_STAGE_COMPUTE_BIT);
    n->nvnCommandBufferBindUniformBuffer(list, NVN_SHADER_STAGE_COMPUTE, 0,
                                         g_zoom.uniform_gpu[slot], UNIFORM_SIZE);
    n->nvnCommandBufferBindStorageBuffer(list, NVN_SHADER_STAGE_COMPUTE, 0, g_zoom.counts,
                                         COUNTS_BYTES);
    n->nvnCommandBufferDispatchCompute(list, FRACTAL_GROUPS_X, FRACTAL_GROUPS_Y, 1);
    // Every store done before a fragment loads.
    n->nvnCommandBufferBarrier(list, NVN_BARRIER_ORDER_PRIMITIVES_BIT |
                                         NVN_BARRIER_INVALIDATE_SHADER_BIT);

    // The colours: three vertices, no attributes, over the whole target.
    Colour(PROGRAM_COLOUR, slot, &g_zoom.target, TARGET_WIDTH, TARGET_HEIGHT);
    n->nvnCommandBufferBarrier(list, NVN_BARRIER_ORDER_FRAGMENTS_BIT |
                                         NVN_BARRIER_INVALIDATE_TEXTURE_BIT);

    const NVNcopyRegion whole = { 0, 0, 0, TARGET_WIDTH, TARGET_HEIGHT, 1 };
    n->nvnCommandBufferSetCopyRowStride(list, 0);
    if (judged < FRACTAL_JUDGED) {
        n->nvnCommandBufferCopyTextureToBuffer(list, &g_zoom.target, NULL, &whole,
                                               g_zoom.kept_image_gpu[judged],
                                               NVN_COPY_FLAGS_NONE);
        n->nvnCommandBufferCopyBufferToBuffer(list, g_zoom.counts, g_zoom.kept_counts_gpu[judged],
                                              COUNTS_BYTES, NVN_COPY_FLAGS_NONE);
    }
    if (g_zoom.on_screen) {
        n->nvnCommandBufferCopyTextureToBuffer(list, &g_zoom.target, NULL, &whole,
                                               g_zoom.readback_gpu, NVN_COPY_FLAGS_NONE);
    } else if (image >= 0) {
        // The frame shown: the target at twice its size, the window's way up.
        Colour(PROGRAM_SHOW, slot, &g_zoom.textures[image], WINDOW_WIDTH, WINDOW_HEIGHT);
    }
    Submit("a frame");
}

static bool Present(int image) {
    if (g_zoom.on_screen) {
        return ScreenShowImage(g_zoom.readback, TARGET_WIDTH, TARGET_HEIGHT);
    }
    if (image < 0) {
        return false;
    }
    g_zoom.nvn.nvnQueuePresentTexture(&g_zoom.queue, &g_zoom.window, image);
    g_zoom.last_image = image;
    return true;
}

static void ShowAgain(void) {
    if (g_zoom.on_screen) {
        (void)ScreenShowImage(g_zoom.readback, TARGET_WIDTH, TARGET_HEIGHT);
        return;
    }
    // The counts and the block of the last frame are still there: the showing pass alone again.
    const int image = Acquire();
    if (image < 0) {
        return;
    }
    BeginRecording();
    Colour(PROGRAM_SHOW, g_zoom.last_slot, &g_zoom.textures[image], WINDOW_WIDTH, WINDOW_HEIGHT);
    Submit("a frame shown again");
    (void)Present(image);
}

static bool Ready(void) {
    return g_zoom.failure[0] == '\0';
}

static const char *Failure(void) {
    return g_zoom.failure;
}

static const uint8_t *KeptCounts(uint32_t j) {
    return g_zoom.kept_counts[j];
}

static const uint8_t *KeptImage(uint32_t j) {
    return g_zoom.kept_image[j];
}

static void Close(void) {
    const NvnProcs *const n = &g_zoom.nvn;
    if (g_zoom.queue_up) {
        n->nvnQueueFinish(&g_zoom.queue);
    }
    if (g_zoom.sync_up) {
        n->nvnSyncFinalize(&g_zoom.sync);
        g_zoom.sync_up = false;
    }
    if (g_zoom.window_up) {
        n->nvnWindowFinalize(&g_zoom.window);
        g_zoom.window_up = false;
    }
    for (int i = 0; i < g_zoom.textures_made; ++i) {
        n->nvnTextureFinalize(&g_zoom.textures[i]);
    }
    g_zoom.textures_made = 0;
    if (g_zoom.target_up) {
        n->nvnTextureFinalize(&g_zoom.target);
        g_zoom.target_up = false;
    }
    for (int i = 0; i < g_zoom.programs_made; ++i) {
        n->nvnProgramFinalize(&g_zoom.programs[i]);
    }
    g_zoom.programs_made = 0;
    for (int i = 0; i < PROGRAMS; ++i) {
        free(g_zoom.sections[i][0]);
        free(g_zoom.sections[i][1]);
        g_zoom.sections[i][0] = g_zoom.sections[i][1] = NULL;
    }
    if (g_zoom.command_up) {
        n->nvnCommandBufferFinalize(&g_zoom.command);
        g_zoom.command_up = false;
    }
    free(g_zoom.control);
    g_zoom.control = NULL;
    DropPool(&g_zoom.image_pool);
    DropPool(&g_zoom.data_pool);
    DropPool(&g_zoom.code_pool);
    DropPool(&g_zoom.command_pool);
    if (g_zoom.queue_up) {
        n->nvnQueueFinalize(&g_zoom.queue);
        g_zoom.queue_up = false;
    }
    if (g_zoom.device_up) {
        n->nvnDeviceFinalize(&g_zoom.device);
        g_zoom.device_up = false;
    }
}

static const FractalWay NVN = {
    .up = Up,
    .acquire = Acquire,
    .block = Block,
    .draw = Draw,
    .present = Present,
    .show_again = ShowAgain,
    .ready = Ready,
    .failure = Failure,
    .kept_counts = KeptCounts,
    .kept_image = KeptImage,
    .close = Close,
};

void RunMandelbrotNvn(void) {
    const MusicMade before = MusicMadeSoFar();
    const DemoRun run = DemoScriptRun();
    FractalZoom(&NVN, FRACTAL_MANDELBROT, &run);
    MusicJudgeOff(FractalSuite(FRACTAL_MANDELBROT), "while the mandelbrot zoom ran", before);
}

void RunJuliaNvn(void) {
    const MusicMade before = MusicMadeSoFar();
    const DemoRun run = DemoScriptRun();
    FractalZoom(&NVN, FRACTAL_JULIA, &run);
    MusicJudgeOff(FractalSuite(FRACTAL_JULIA), "while the julia zoom ran", before);
}
