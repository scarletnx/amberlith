// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <switch.h>

#include <nvnlayer.h>

#include "perf.h"
#include "report.h"
#include "screen.h"

// The buffer a frame is copied from: the processor composes into memory of its own and copies
// the frame there whole, so ui.c never reads the pool's uncached memory back.
#define FRAME_BYTES ((size_t)SCREEN_WIDTH * SCREEN_HEIGHT * 4u)
#define POOL_PAGE 0x1000u
#define COMMAND_BYTES 0x10000u
#define CONTROL_BYTES 0x1000u
#define TEXTURES 2

#define SECOND_NANOSECONDS 1000000000ull
/// How long a texture the window hands is waited for before the frame is given up.
#define ACQUIRE_WAIT_NANOSECONDS 10000000000ull

// NVN's objects are large and must sit on an eight byte boundary, which nvn/nvn.h does not ask
// the compiler for.
static struct {
    NvnProcs nvn;
    bool device_up;
    bool queue_up;
    bool command_up;
    bool command_pool_up;
    bool frame_pool_up;
    bool texture_pool_up;
    bool window_up;
    bool sync_up;
    int textures_made;
    _Alignas(8) NVNdevice device;
    _Alignas(8) NVNqueue queue;
    _Alignas(8) NVNcommandBuffer command;
    _Alignas(8) NVNmemoryPool command_pool;
    _Alignas(8) NVNmemoryPool frame_pool;
    _Alignas(8) NVNmemoryPool texture_pool;
    _Alignas(8) NVNtexture textures[TEXTURES];
    _Alignas(8) NVNwindow window;
    _Alignas(8) NVNsync sync;
    void *command_storage;
    void *frame_storage;
    void *texture_storage;
    void *control;
    uint8_t *composed;
    uint8_t *mapped;
    char error[200];
    /// The running screen.
    bool progress_on;
    bool progress_drawn_once;
    uint64_t progress_began;
    uint64_t progress_drawn;
    UiProgress progress;
    UiProgressCount count;
} g_screen;

static size_t RoundUp(size_t value, size_t to) {
    return (value + to - 1u) / to * to;
}

static bool MakePool(NVNmemoryPool *pool, void **storage, size_t size, int flags) {
    *storage = aligned_alloc(POOL_PAGE, size);
    if (*storage == NULL) {
        return false;
    }
    memset(*storage, 0, size);
    _Alignas(8) NVNmemoryPoolBuilder builder;
    g_screen.nvn.nvnMemoryPoolBuilderSetDefaults(&builder);
    g_screen.nvn.nvnMemoryPoolBuilderSetDevice(&builder, &g_screen.device);
    g_screen.nvn.nvnMemoryPoolBuilderSetStorage(&builder, *storage, size);
    g_screen.nvn.nvnMemoryPoolBuilderSetFlags(&builder, flags);
    if (!g_screen.nvn.nvnMemoryPoolInitialize(pool, &builder)) {
        free(*storage);
        *storage = NULL;
        return false;
    }
    return true;
}

static void DropPool(NVNmemoryPool *pool, void **storage, bool *up) {
    if (*up) {
        g_screen.nvn.nvnMemoryPoolFinalize(pool);
        *up = false;
    }
    free(*storage);
    *storage = NULL;
}

// Every function the screens call. A game's driver has the names of its own API version, and a
// name it lacks does nothing (nvnlayer.h): the screens come up only on a driver that has them
// all, so no case's tally of lacked calls ever holds one of theirs.
static const char *const CALLED[] = {
    "nvnDeviceBuilderSetDefaults", "nvnDeviceBuilderSetFlags", "nvnDeviceInitialize",
    "nvnDeviceFinalize", "nvnQueueBuilderSetDevice", "nvnQueueBuilderSetDefaults",
    "nvnQueueInitialize", "nvnQueueFinalize", "nvnQueueFinish", "nvnQueueSubmitCommands",
    "nvnQueuePresentTexture", "nvnSyncInitialize", "nvnSyncFinalize", "nvnSyncWait",
    "nvnMemoryPoolBuilderSetDefaults",
    "nvnMemoryPoolBuilderSetDevice", "nvnMemoryPoolBuilderSetStorage",
    "nvnMemoryPoolBuilderSetFlags", "nvnMemoryPoolInitialize", "nvnMemoryPoolFinalize",
    "nvnMemoryPoolMap", "nvnMemoryPoolGetBufferAddress", "nvnCommandBufferInitialize",
    "nvnCommandBufferFinalize", "nvnCommandBufferAddCommandMemory",
    "nvnCommandBufferAddControlMemory", "nvnCommandBufferBeginRecording",
    "nvnCommandBufferEndRecording", "nvnCommandBufferSetCopyRowStride",
    "nvnCommandBufferCopyBufferToTexture", "nvnTextureBuilderSetDevice",
    "nvnTextureBuilderSetDefaults", "nvnTextureBuilderSetFlags", "nvnTextureBuilderSetTarget",
    "nvnTextureBuilderSetFormat", "nvnTextureBuilderSetSize2D",
    "nvnTextureBuilderGetStorageSize", "nvnTextureBuilderGetStorageAlignment",
    "nvnTextureBuilderSetStorage", "nvnTextureInitialize", "nvnTextureFinalize",
    "nvnWindowBuilderSetDefaults", "nvnWindowBuilderSetDevice", "nvnWindowBuilderSetNativeWindow",
    "nvnWindowBuilderSetTextures", "nvnWindowInitialize", "nvnWindowFinalize",
    "nvnWindowAcquireTexture",
};

/// The first of CALLED the game's driver has no function for, or NULL.
static const char *Lacked(void) {
    for (size_t i = 0; i < NvnLayerLackedCount(); ++i) {
        const char *const name = NvnLayerLackedName(i);
        for (size_t j = 0; j < sizeof(CALLED) / sizeof(CALLED[0]); ++j) {
            if (strcmp(name, CALLED[j]) == 0) {
                return name;
            }
        }
    }
    return NULL;
}

/// The device, its queue and command buffer: false with the step in @p why.
static bool DeviceUp(char *why, size_t why_size) {
    char missing[64] = "";
    if (!NvnLoadProcs(&g_screen.nvn, missing, sizeof(missing))) {
        snprintf(why, why_size, "the game's driver gave no %s", missing);
        return false;
    }
    const char *const lacked = Lacked();
    if (lacked != NULL) {
        snprintf(why, why_size, "the game's driver has no %s, which the screens call", lacked);
        return false;
    }
    _Alignas(8) NVNdeviceBuilder device_builder;
    g_screen.nvn.nvnDeviceBuilderSetDefaults(&device_builder);
    g_screen.nvn.nvnDeviceBuilderSetFlags(&device_builder, 0);
    if (!(g_screen.device_up = g_screen.nvn.nvnDeviceInitialize(&g_screen.device,
                                                                &device_builder))) {
        snprintf(why, why_size, "nvnDeviceInitialize answered false");
        return false;
    }
    _Alignas(8) NVNqueueBuilder queue_builder;
    g_screen.nvn.nvnQueueBuilderSetDevice(&queue_builder, &g_screen.device);
    g_screen.nvn.nvnQueueBuilderSetDefaults(&queue_builder);
    if (!(g_screen.queue_up = g_screen.nvn.nvnQueueInitialize(&g_screen.queue, &queue_builder))) {
        snprintf(why, why_size, "nvnQueueInitialize answered false");
        return false;
    }
    g_screen.command_pool_up =
        MakePool(&g_screen.command_pool, &g_screen.command_storage, COMMAND_BYTES,
                 NVN_MEMORY_POOL_FLAGS_CPU_UNCACHED | NVN_MEMORY_POOL_FLAGS_GPU_CACHED);
    if (!g_screen.command_pool_up) {
        snprintf(why, why_size, "the command pool of %u bytes: no memory, or "
                 "nvnMemoryPoolInitialize answered false",
                 (unsigned)COMMAND_BYTES);
        return false;
    }
    if (!(g_screen.command_up =
              g_screen.nvn.nvnCommandBufferInitialize(&g_screen.command, &g_screen.device))) {
        snprintf(why, why_size, "nvnCommandBufferInitialize answered false");
        return false;
    }
    g_screen.control = aligned_alloc(8u, CONTROL_BYTES);
    if (g_screen.control == NULL) {
        snprintf(why, why_size, "no memory for %u bytes of control memory",
                 (unsigned)CONTROL_BYTES);
        return false;
    }
    g_screen.nvn.nvnCommandBufferAddCommandMemory(&g_screen.command, &g_screen.command_pool, 0,
                                                  COMMAND_BYTES);
    g_screen.nvn.nvnCommandBufferAddControlMemory(&g_screen.command, g_screen.control,
                                                  CONTROL_BYTES);
    return true;
}

/// The window's two textures in a pool of their own, the window on @p native, and the buffer a
/// frame is copied from: false with the step in @p why.
static bool WindowUp(void *native, char *why, size_t why_size) {
    _Alignas(8) NVNtextureBuilder builder;
    memset(&builder, 0, sizeof(builder));
    g_screen.nvn.nvnTextureBuilderSetDevice(&builder, &g_screen.device);
    g_screen.nvn.nvnTextureBuilderSetDefaults(&builder);
    g_screen.nvn.nvnTextureBuilderSetFlags(&builder, NVN_TEXTURE_FLAGS_DISPLAY);
    g_screen.nvn.nvnTextureBuilderSetTarget(&builder, NVN_TEXTURE_TARGET_2D);
    g_screen.nvn.nvnTextureBuilderSetFormat(&builder, NVN_FORMAT_RGBA8);
    g_screen.nvn.nvnTextureBuilderSetSize2D(&builder, (int)SCREEN_WIDTH, (int)SCREEN_HEIGHT);
    const size_t size = g_screen.nvn.nvnTextureBuilderGetStorageSize(&builder);
    const size_t alignment = g_screen.nvn.nvnTextureBuilderGetStorageAlignment(&builder);
    if (size == 0u || alignment == 0u || alignment > POOL_PAGE) {
        snprintf(why, why_size, "a window texture's storage is %zu bytes at an alignment of %zu, "
                 "which no pool here can hold",
                 size, alignment);
        return false;
    }
    const size_t stride = RoundUp(size, alignment);
    g_screen.texture_pool_up =
        MakePool(&g_screen.texture_pool, &g_screen.texture_storage,
                 RoundUp(stride * TEXTURES, POOL_PAGE),
                 NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS | NVN_MEMORY_POOL_FLAGS_GPU_CACHED);
    if (!g_screen.texture_pool_up) {
        snprintf(why, why_size, "the window's pool of %zu bytes: no memory, or "
                 "nvnMemoryPoolInitialize answered false",
                 RoundUp(stride * TEXTURES, POOL_PAGE));
        return false;
    }
    for (g_screen.textures_made = 0; g_screen.textures_made < TEXTURES;
         ++g_screen.textures_made) {
        const ptrdiff_t offset = (ptrdiff_t)(stride * (size_t)g_screen.textures_made);
        g_screen.nvn.nvnTextureBuilderSetStorage(&builder, &g_screen.texture_pool, offset);
        if (!g_screen.nvn.nvnTextureInitialize(&g_screen.textures[g_screen.textures_made],
                                               &builder)) {
            snprintf(why, why_size, "window texture %d: nvnTextureInitialize answered false",
                     g_screen.textures_made);
            return false;
        }
    }
    NVNtexture *const list[TEXTURES] = { &g_screen.textures[0], &g_screen.textures[1] };
    _Alignas(8) NVNwindowBuilder window_builder;
    g_screen.nvn.nvnWindowBuilderSetDefaults(&window_builder);
    g_screen.nvn.nvnWindowBuilderSetDevice(&window_builder, &g_screen.device);
    g_screen.nvn.nvnWindowBuilderSetNativeWindow(&window_builder, (NVNnativeWindow)native);
    g_screen.nvn.nvnWindowBuilderSetTextures(&window_builder, TEXTURES, list);
    if (!(g_screen.window_up =
              g_screen.nvn.nvnWindowInitialize(&g_screen.window, &window_builder))) {
        snprintf(why, why_size, "nvnWindowInitialize answered false");
        return false;
    }
    if (!(g_screen.sync_up = g_screen.nvn.nvnSyncInitialize(&g_screen.sync, &g_screen.device))) {
        snprintf(why, why_size, "nvnSyncInitialize answered false");
        return false;
    }
    g_screen.frame_pool_up =
        MakePool(&g_screen.frame_pool, &g_screen.frame_storage, RoundUp(FRAME_BYTES, POOL_PAGE),
                 NVN_MEMORY_POOL_FLAGS_CPU_UNCACHED | NVN_MEMORY_POOL_FLAGS_GPU_CACHED);
    g_screen.mapped = g_screen.frame_pool_up
                          ? (uint8_t *)g_screen.nvn.nvnMemoryPoolMap(&g_screen.frame_pool)
                          : NULL;
    g_screen.composed = (uint8_t *)malloc(FRAME_BYTES);
    if (g_screen.mapped == NULL || g_screen.composed == NULL) {
        snprintf(why, why_size, "the frame's buffer of %zu bytes: %s", FRAME_BYTES,
                 !g_screen.frame_pool_up ? "no memory, or nvnMemoryPoolInitialize answered false"
                 : g_screen.mapped == NULL ? "nvnMemoryPoolMap gave no address"
                                           : "no memory to compose it in");
        return false;
    }
    return true;
}

bool ScreenUp(void *native, char *why, size_t why_size) {
    ScreenDown();
    g_screen.error[0] = '\0';
    if (native == NULL) {
        snprintf(why, why_size, "the mod has no window");
        return false;
    }
    if (!DeviceUp(why, why_size) || !WindowUp(native, why, why_size)) {
        ScreenDown();
        return false;
    }
    return true;
}

bool ScreenIsUp(void) {
    return g_screen.window_up && g_screen.composed != NULL;
}

void ScreenDown(void) {
    if (g_screen.queue_up) {
        g_screen.nvn.nvnQueueFinish(&g_screen.queue);
    }
    free(g_screen.composed);
    g_screen.composed = NULL;
    g_screen.mapped = NULL;
    DropPool(&g_screen.frame_pool, &g_screen.frame_storage, &g_screen.frame_pool_up);
    if (g_screen.sync_up) {
        g_screen.nvn.nvnSyncFinalize(&g_screen.sync);
        g_screen.sync_up = false;
    }
    if (g_screen.window_up) {
        g_screen.nvn.nvnWindowFinalize(&g_screen.window);
        g_screen.window_up = false;
    }
    for (int i = 0; i < g_screen.textures_made; ++i) {
        g_screen.nvn.nvnTextureFinalize(&g_screen.textures[i]);
    }
    g_screen.textures_made = 0;
    DropPool(&g_screen.texture_pool, &g_screen.texture_storage, &g_screen.texture_pool_up);
    if (g_screen.command_up) {
        g_screen.nvn.nvnCommandBufferFinalize(&g_screen.command);
        g_screen.command_up = false;
    }
    free(g_screen.control);
    g_screen.control = NULL;
    DropPool(&g_screen.command_pool, &g_screen.command_storage, &g_screen.command_pool_up);
    if (g_screen.queue_up) {
        g_screen.nvn.nvnQueueFinalize(&g_screen.queue);
        g_screen.queue_up = false;
    }
    if (g_screen.device_up) {
        g_screen.nvn.nvnDeviceFinalize(&g_screen.device);
        g_screen.device_up = false;
    }
}

const char *ScreenError(void) {
    return g_screen.error;
}

/// The frame to compose into.
static UiFrame Frame(void) {
    const UiFrame frame = { g_screen.composed, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_WIDTH * 4u };
    return frame;
}

/// The composed frame to the display: copied into the pool, from there into the texture the
/// window gives, which is then presented. The queue is finished before the next frame is copied
/// over this one. The first answer that stops a frame is kept for the report (ScreenError).
///
/// The texture comes from the window with a sync, waited for here: the queue's own acquire is a
/// later API's, which a game's driver of API 55 has not.
static void Present(void) {
    if (!ScreenIsUp()) {
        return;
    }
    int index = -1;
    const NVNwindowAcquireTextureResult got =
        g_screen.nvn.nvnWindowAcquireTexture(&g_screen.window, &g_screen.sync, &index);
    if (got != NVN_WINDOW_ACQUIRE_TEXTURE_RESULT_SUCCESS || index < 0 || index >= TEXTURES) {
        if (g_screen.error[0] == '\0') {
            snprintf(g_screen.error, sizeof(g_screen.error),
                     "nvnWindowAcquireTexture answered %d with texture %d of %d", (int)got, index,
                     TEXTURES);
        }
        return;
    }
    const NVNsyncWaitResult wait =
        g_screen.nvn.nvnSyncWait(&g_screen.sync, ACQUIRE_WAIT_NANOSECONDS);
    if (wait != NVN_SYNC_WAIT_RESULT_ALREADY_SIGNALED &&
        wait != NVN_SYNC_WAIT_RESULT_CONDITION_SATISFIED && g_screen.error[0] == '\0') {
        snprintf(g_screen.error, sizeof(g_screen.error),
                 "the sync of texture %d answered a wait of up to 10 s with %d (2 timed out, 3 "
                 "failed); the frame went to it all the same",
                 index, (int)wait);
    }
    // The window shows a texture's first row at the bottom, as NVN's default lower left origin
    // has it, where ui.c composes the top row first: the rows go in last first.
    const size_t row = (size_t)SCREEN_WIDTH * 4u;
    for (size_t y = 0; y < SCREEN_HEIGHT; ++y) {
        memcpy(g_screen.mapped + (SCREEN_HEIGHT - 1u - y) * row, g_screen.composed + y * row, row);
    }
    const NVNcopyRegion whole = { 0, 0, 0, (int)SCREEN_WIDTH, (int)SCREEN_HEIGHT, 1 };
    NVNcommandBuffer *const command = &g_screen.command;
    // The whole of the command buffer's memory again: a game's driver never takes back what it
    // handed out, and runs out (fractal_nvn.c says how it ended). The last frame was waited for.
    g_screen.nvn.nvnCommandBufferAddCommandMemory(command, &g_screen.command_pool, 0,
                                                  COMMAND_BYTES);
    g_screen.nvn.nvnCommandBufferAddControlMemory(command, g_screen.control, CONTROL_BYTES);
    g_screen.nvn.nvnCommandBufferBeginRecording(command);
    g_screen.nvn.nvnCommandBufferSetCopyRowStride(command, 0);
    g_screen.nvn.nvnCommandBufferCopyBufferToTexture(
        command, g_screen.nvn.nvnMemoryPoolGetBufferAddress(&g_screen.frame_pool),
        &g_screen.textures[index], NULL, &whole, NVN_COPY_FLAGS_NONE);
    const NVNcommandHandle list = g_screen.nvn.nvnCommandBufferEndRecording(command);
    if (list == 0u) {
        if (g_screen.error[0] == '\0') {
            snprintf(g_screen.error, sizeof(g_screen.error),
                     "nvnCommandBufferEndRecording gave no list");
        }
    } else {
        g_screen.nvn.nvnQueueSubmitCommands(&g_screen.queue, 1, &list);
    }
    // Presented whatever the copy did: a texture acquired goes back to the window.
    g_screen.nvn.nvnQueuePresentTexture(&g_screen.queue, &g_screen.window, index);
    g_screen.nvn.nvnQueueFinish(&g_screen.queue);
}

static uint64_t Now(void) {
    return armTicksToNs(armGetSystemTick());
}

bool ScreenShowImage(const uint8_t *pixels, uint32_t width, uint32_t height) {
    if (!ScreenIsUp() || pixels == NULL || width == 0u || height == 0u || width > SCREEN_WIDTH ||
        height > SCREEN_HEIGHT) {
        return false;
    }
    const uint32_t across = SCREEN_WIDTH / width;
    const uint32_t down = SCREEN_HEIGHT / height;
    const uint32_t scale = across < down ? across : down;
    const uint32_t left = (SCREEN_WIDTH - width * scale) / 2u;
    const uint32_t top = (SCREEN_HEIGHT - height * scale) / 2u;
    static const uint8_t BACKGROUND[4] = { 0x0F, 0x0F, 0x14, 0xFF };
    for (uint32_t y = 0; y < SCREEN_HEIGHT; ++y) {
        uint8_t *const row = g_screen.composed + (size_t)y * SCREEN_WIDTH * 4u;
        const bool inside_y = y >= top && y < top + height * scale;
        for (uint32_t x = 0; x < SCREEN_WIDTH; ++x) {
            const bool inside = inside_y && x >= left && x < left + width * scale;
            const size_t at = (size_t)((y - top) / scale) * width + (x - left) / scale;
            memcpy(row + (size_t)x * 4u, inside ? pixels + at * 4u : BACKGROUND, 4u);
        }
    }
    Present();
    return g_screen.error[0] == '\0';
}

void ScreenStart(const UiStartInfo *info, UiStartState state) {
    if (!ScreenIsUp() || info == NULL) {
        return;
    }
    const uint64_t opened = Now();
    for (;;) {
        const uint64_t elapsed = Now() - opened;
        if (UiStartTimesOut(elapsed, false)) {
            return;
        }
        UiFrame frame = Frame();
        UiComposeStart(&frame, info, state, UiStartSecondsLeft(elapsed, false));
        Present();
        // To the next whole second of the countdown, when the number on the screen changes.
        const uint64_t into = (Now() - opened) % SECOND_NANOSECONDS;
        svcSleepThread((s64)(SECOND_NANOSECONDS - into));
    }
}

/// The running screen as it is now, when one is due: at a step's start, otherwise at most every
/// UI_PROGRESS_NANOSECONDS.
static void ProgressDraw(bool step_began) {
    if (!g_screen.progress_on || !ScreenIsUp()) {
        return;
    }
    UiProgress *const progress = &g_screen.progress;
    progress->passed = (uint32_t)ReportPassed();
    progress->failed = (uint32_t)ReportFailed();
    progress->skipped = (uint32_t)ReportSkipped();
    progress->crashed = (uint32_t)ReportCrashed();
    progress->pass = progress->passes > 0u ? PerfPassNumber() : 0u;
    progress->case_number = g_screen.count.count;
    // How many the pass before met is how many this one will, from the second pass on.
    progress->case_total = progress->pass > 1u ? g_screen.count.previous : 0u;
    const uint64_t now = Now();
    if (!UiProgressDue(g_screen.progress_drawn_once, g_screen.progress_drawn, now, step_began)) {
        return;
    }
    progress->elapsed_ns = now - g_screen.progress_began;
    UiFrame frame = Frame();
    UiComposeProgress(&frame, progress);
    Present();
    g_screen.progress_drawn = now;
    g_screen.progress_drawn_once = true;
}

void ScreenProgressBegin(const char *heading, const char *folder, uint32_t steps) {
    memset(&g_screen.progress, 0, sizeof(g_screen.progress));
    g_screen.progress.heading = heading;
    g_screen.progress.folder = folder;
    g_screen.progress.steps = steps;
    g_screen.progress.padless = true;
    UiProgressCountReset(&g_screen.count);
    g_screen.progress_began = Now();
    g_screen.progress_drawn_once = false;
    g_screen.progress_on = ScreenIsUp();
    if (g_screen.progress_on) {
        ReportNote("running screen: the mod runs on the game's one thread, so the screen is drawn "
                   "between cases only, and a long case leaves it still");
    }
}

void ScreenProgressStep(uint32_t step, const char *name, bool timed) {
    if (!g_screen.progress_on) {
        return;
    }
    g_screen.progress.step = step;
    g_screen.progress.step_name = name;
    g_screen.progress.passes = timed ? PERF_WARMUPS + PERF_REHEARSALS + 1u : 0u;
    g_screen.progress.suite = NULL;
    g_screen.progress.name = NULL;
    UiProgressCountReset(&g_screen.count);
    ProgressDraw(true);
}

void ScreenProgressWatch(const char *suite, const char *name, bool started) {
    if (!g_screen.progress_on) {
        return;
    }
    UiProgressCountCase(&g_screen.count, PerfPassNumber(), started);
    g_screen.progress.suite = suite;
    g_screen.progress.name = name;
    ProgressDraw(false);
}

void ScreenProgressEnd(void) {
    g_screen.progress_on = false;
}

void ScreenResults(const char *heading) {
    if (!ScreenIsUp()) {
        return;
    }
    // What the run recorded, and the count the report's own summary carries, so a store that ran
    // out is said to have: as the plain program indexes its run (ViewerIndexRun).
    static UiRun run;
    size_t count = 0;
    const ReportRecord *const records = ReportRecords(&count);
    UiRunIndex(&run, records, (uint32_t)count,
               (uint32_t)(ReportPassed() + ReportFailed() + ReportSkipped() + ReportCrashed()));
    run.heading = heading;
    UiState state = UiStart(&run);
    state.padless = true;
    const uint64_t opened = Now();
    for (;;) {
        const uint64_t elapsed = Now() - opened;
        if (UiLeavesUnattended(elapsed, false)) {
            return;
        }
        UiFrame frame = Frame();
        UiCompose(&frame, &run, state, NULL);
        UiComposeCountdown(&frame, UiUnattendedSecondsLeft(elapsed, false), true);
        Present();
        const uint64_t into = (Now() - opened) % SECOND_NANOSECONDS;
        svcSleepThread((s64)(SECOND_NANOSECONDS - into));
    }
}
