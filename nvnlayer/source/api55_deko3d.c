// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdlib.h>

#include "api55.h"

// The API 55 build: NVN on deko3d. Each NVN object is storage the program owns, of the size
// nvn/nvn.h gives it; the layer keeps its own record at the start of it, as a driver keeps its
// fields there, and nothing else reads those bytes: a program asks every question through an NVN
// function, since under ScarletNX the same storage holds ScarletNX's record instead.
//
// deko3d stands for the driver in every choice NVN leaves to it, and where the two disagree the
// layer takes NVN's word: the device starts with NVN's lower left origin and near plane at -w, a
// texture can be a render target without saying so, and a pool can hold textures whatever its
// flags. A choice NVN documents and this layer has not made yet is refused with its value in
// NvnLayerError, never filled in with a guess.


// ---- deko3d's constructors, which abort on a failure: run under the program's guard, their
// arguments passed through here because a guarded call takes none. One thread calls the layer.

static struct {
    DkDeviceMaker device_maker;
    DkDevice device;
    DkQueueMaker queue_maker;
    DkQueue queue;
    DkMemBlockMaker block_maker;
    DkMemBlock block;
    DkSwapchainMaker swapchain_maker;
    DkSwapchain swapchain;
    DkQueue acquire_queue;
    DkSwapchain acquire_swapchain;
    DkFence *acquire_fence;
    int acquired;
    int32_t crop[4];
    uint32_t interval;
} pending;

static void MakeDevice(void) {
    pending.device = dkDeviceCreate(&pending.device_maker);
}

static void MakeQueue(void) {
    pending.queue = dkQueueCreate(&pending.queue_maker);
}

static void MakeBlock(void) {
    pending.block = dkMemBlockCreate(&pending.block_maker);
}

static void MakeSwapchain(void) {
    pending.swapchain = dkSwapchainCreate(&pending.swapchain_maker);
}

static void AcquireImage(void) {
    pending.acquired = dkQueueAcquireImage(pending.acquire_queue, pending.acquire_swapchain);
}

static void AcquireFenced(void) {
    dkSwapchainAcquireImage(pending.acquire_swapchain, &pending.acquired, pending.acquire_fence);
}

static void SetCrop(void) {
    dkSwapchainSetCrop(pending.acquire_swapchain, pending.crop[0], pending.crop[1],
                       pending.crop[0] + pending.crop[2], pending.crop[1] + pending.crop[3]);
}

static void SetInterval(void) {
    dkSwapchainSetSwapInterval(pending.acquire_swapchain, pending.interval);
}

// ---- the device

void Nvnl_nvnDeviceBuilderSetDefaults(NVNdeviceBuilder *builder) {
    memset(builder, 0, sizeof(*builder));
}

void Nvnl_nvnDeviceBuilderSetFlags(NVNdeviceBuilder *builder, int flags) {
    LayerDeviceBuilder *const record = Record(builder, "nvnDeviceBuilderSetFlags");
    if (record != NULL) {
        record->flags = flags;
    }
}

/// deko3d's device for @p record's two modes; false with a complaint naming @p what.
static bool MakeLayerDevice(LayerDevice *record, const char *what) {
    dkDeviceMakerDefaults(&pending.device_maker);
    pending.device_maker.flags =
        (record->origin == NVN_WINDOW_ORIGIN_MODE_LOWER_LEFT ? DkDeviceFlags_OriginLowerLeft
                                                              : DkDeviceFlags_OriginUpperLeft) |
        (record->depth == NVN_DEPTH_MODE_NEAR_IS_MINUS_W ? DkDeviceFlags_DepthMinusOneToOne
                                                          : DkDeviceFlags_DepthZeroToOne);
    pending.device = NULL;
    if (!LayerGuarded(MakeDevice, what)) {
        return false;
    }
    if (pending.device == NULL) {
        LayerComplain("%s: dkDeviceCreate gave no device", what);
        return false;
    }
    record->device = pending.device;
    return true;
}

NVNboolean Nvnl_nvnDeviceInitialize(NVNdevice *device, const NVNdeviceBuilder *builder) {
    LayerDevice *const record = Record(device, "nvnDeviceInitialize");
    const LayerDeviceBuilder *const from = Record(builder, "nvnDeviceInitialize");
    if (record == NULL || from == NULL) {
        return 0;
    }
    // The flags all ask for the debug layer or for finalizes deferred by age, which this layer
    // does not have; a case that sets one is told, rather than run without it.
    if (from->flags != 0) {
        LayerComplain("nvnDeviceInitialize: device flags 0x%x, and the layer takes 0 only",
                      (unsigned)from->flags);
        return 0;
    }
    // NVN's defaults: the window's origin in its lower left corner and the near plane at -w,
    // OpenGL's conventions.
    memset(device, 0, sizeof(*device));
    record->origin = NVN_WINDOW_ORIGIN_MODE_LOWER_LEFT;
    record->depth = NVN_DEPTH_MODE_NEAR_IS_MINUS_W;
    if (!MakeLayerDevice(record, "nvnDeviceInitialize")) {
        return 0;
    }
    record->magic = MAGIC_DEVICE;
    return 1;
}

/// The device's two modes, which NVN sets on a device it has made and deko3d takes only as it
/// makes one: deko3d's device is made again with the mode, which is possible until something was
/// made from it. A mode set later is refused, saying so.
static void SetMode(NVNdevice *device, uint8_t origin, uint8_t depth, const char *what) {
    LayerDevice *const record = DeviceOf(device, what);
    if (record == NULL || (record->origin == origin && record->depth == depth)) {
        return;
    }
    if (record->used) {
        LayerComplain("%s after the device's first object: deko3d takes the origin and the "
                      "depth mode only as it makes a device, so the layer takes them before "
                      "any queue, pool, texture, command buffer or window",
                      what);
        return;
    }
    dkDeviceDestroy(record->device);
    record->device = NULL;
    record->origin = origin;
    record->depth = depth;
    if (!MakeLayerDevice(record, what)) {
        record->magic = 0;
    }
}

void Nvnl_nvnDeviceSetWindowOriginMode(NVNdevice *device, NVNwindowOriginMode mode) {
    const LayerDevice *const record = DeviceOf(device, "nvnDeviceSetWindowOriginMode");
    if (record == NULL) {
        return;
    }
    if (mode != NVN_WINDOW_ORIGIN_MODE_LOWER_LEFT && mode != NVN_WINDOW_ORIGIN_MODE_UPPER_LEFT) {
        LayerComplain("nvnDeviceSetWindowOriginMode: mode %d is neither lower left (%d) nor "
                      "upper left (%d)",
                      (int)mode, (int)NVN_WINDOW_ORIGIN_MODE_LOWER_LEFT,
                      (int)NVN_WINDOW_ORIGIN_MODE_UPPER_LEFT);
        return;
    }
    SetMode(device, (uint8_t)mode, record->depth, "nvnDeviceSetWindowOriginMode");
}

void Nvnl_nvnDeviceSetDepthMode(NVNdevice *device, NVNdepthMode mode) {
    const LayerDevice *const record = DeviceOf(device, "nvnDeviceSetDepthMode");
    if (record == NULL) {
        return;
    }
    if (mode != NVN_DEPTH_MODE_NEAR_IS_MINUS_W && mode != NVN_DEPTH_MODE_NEAR_IS_ZERO) {
        LayerComplain("nvnDeviceSetDepthMode: mode %d is neither near at -w (%d) nor near at "
                      "zero (%d)",
                      (int)mode, (int)NVN_DEPTH_MODE_NEAR_IS_MINUS_W,
                      (int)NVN_DEPTH_MODE_NEAR_IS_ZERO);
        return;
    }
    SetMode(device, record->origin, (uint8_t)mode, "nvnDeviceSetDepthMode");
}

NVNtextureHandle Nvnl_nvnDeviceGetTextureHandle(const NVNdevice *device, int texture,
                                                int sampler) {
    // The graphics unit's own handle, which is NVN's and deko3d's alike: the texture's place in
    // the texture pool in bits 0 to 19, the sampler's in the sampler pool above them.
    (void)device;
    return dkMakeTextureHandle((uint32_t)texture, (uint32_t)sampler);
}

void Nvnl_nvnDeviceFinalize(NVNdevice *device) {
    LayerDevice *const record = DeviceOf(device, "nvnDeviceFinalize");
    if (record == NULL) {
        return;
    }
    dkDeviceDestroy(record->device);
    record->magic = 0;
}

/// The items nvnDeviceGetInteger does not answer with a constant in its switch: the ones whose
/// deko3d value the layer knows, from deko3d.h. Anything else is refused with its number, as a
/// value nobody looked up yet; it reads back as 0.
void NvnLayerDeviceInfo(const NVNdevice *device, int item, int *value) {
    (void)device;
    int answer;
    switch (item) {
    case NVN_DEVICE_INFO_UNIFORM_BUFFER_ALIGNMENT:
        answer = DK_UNIFORM_BUF_ALIGNMENT;
        break;
    case NVN_DEVICE_INFO_MAX_UNIFORM_BUFFER_SIZE:
        answer = DK_UNIFORM_BUF_MAX_SIZE;
        break;
    case NVN_DEVICE_INFO_COLOR_BUFFER_BINDINGS:
        answer = DK_MAX_RENDER_TARGETS;
        break;
    case NVN_DEVICE_INFO_VERTEX_BUFFER_BINDINGS:
        answer = DK_MAX_VERTEX_BUFFERS;
        break;
    case NVN_DEVICE_INFO_TEXTURE_DESCRIPTOR_SIZE:
    case NVN_DEVICE_INFO_SAMPLER_DESCRIPTOR_SIZE:
        answer = 32;
        break;
    case NVN_DEVICE_INFO_COMMAND_BUFFER_COMMAND_ALIGNMENT:
        answer = DK_CMDMEM_ALIGNMENT;
        break;
    case NVN_DEVICE_INFO_LINEAR_TEXTURE_STRIDE_ALIGNMENT:
        answer = DK_IMAGE_LINEAR_STRIDE_ALIGNMENT;
        break;
    case NVN_DEVICE_INFO_MEMORY_POOL_PAGE_SIZE:
        answer = DK_MEMBLOCK_ALIGNMENT;
        break;
    case NVN_DEVICE_INFO_MIN_TEXTURES_PER_WINDOW:
        answer = 2;
        break;
    case NVN_DEVICE_INFO_MAX_TEXTURES_PER_WINDOW:
        answer = WINDOW_TEXTURES_MAX;
        break;
    case NVN_DEVICE_INFO_UNIFORM_BUFFER_BINDINGS_PER_STAGE:
        // deko3d's sixteen less the one the layer binds the texture handles as, and no more
        // than NVN's own fourteen.
        answer = 14;
        break;
    case NVN_DEVICE_INFO_TEXTURE_BINDINGS_PER_STAGE:
        answer = LAYER_HANDLE_SLOTS;
        break;
    case NVN_DEVICE_INFO_VERTEX_ATTRIBUTES:
        answer = DK_MAX_VERTEX_ATTRIBS;
        break;
    case NVN_DEVICE_INFO_RESERVED_TEXTURE_DESCRIPTORS:
    case NVN_DEVICE_INFO_RESERVED_SAMPLER_DESCRIPTORS:
        // NVN keeps the first descriptors of each pool to itself. deko3d keeps none; the layer
        // answers NVN's count so a program numbers its textures the same on every machine.
        answer = 256;
        break;
    case NVN_DEVICE_INFO_SHADER_CODE_MEMORY_POOL_PADDING_SIZE:
        answer = DK_SHADER_CODE_UNUSABLE_SIZE;
        break;
    default:
        LayerComplain("nvnDeviceGetInteger: item 0x%x has no answer in the layer yet: it reads 0",
                      (unsigned)item);
        answer = 0;
        break;
    }
    *value = answer;
}

// ---- the queue

void Nvnl_nvnQueueBuilderSetDefaults(NVNqueueBuilder *builder) {
    // The device stays, as NVN's own builder keeps it.
    LayerQueueBuilder *const record = Record(builder, "nvnQueueBuilderSetDefaults");
    if (record == NULL) {
        return;
    }
    NVNdevice *const device = record->device;
    memset(builder, 0, sizeof(*builder));
    record->device = device;
}

void Nvnl_nvnQueueBuilderSetDevice(NVNqueueBuilder *builder, NVNdevice *device) {
    LayerQueueBuilder *const record = Record(builder, "nvnQueueBuilderSetDevice");
    if (record != NULL) {
        record->device = device;
    }
}

NVNboolean Nvnl_nvnQueueInitialize(NVNqueue *queue, const NVNqueueBuilder *builder) {
    LayerQueue *const record = Record(queue, "nvnQueueInitialize");
    const LayerQueueBuilder *const from = Record(builder, "nvnQueueInitialize");
    if (record == NULL || from == NULL) {
        return 0;
    }
    LayerDevice *const device = DeviceOf(from->device, "nvnQueueInitialize");
    if (device == NULL) {
        return 0;
    }
    if (from->flags != NVN_QUEUE_BUILDER_FLAGS_NONE) {
        LayerComplain("nvnQueueInitialize: queue flags 0x%x, and the layer makes a queue with "
                      "none (0x%x)",
                      (unsigned)from->flags, (unsigned)NVN_QUEUE_BUILDER_FLAGS_NONE);
        return 0;
    }
    // deko3d's defaults: a graphics and compute queue, as every NVN queue is. The sizes a
    // builder was set are NVN's to use; deko3d's queue keeps its own.
    dkQueueMakerDefaults(&pending.queue_maker, DeviceUse(device));
    pending.queue = NULL;
    if (!LayerGuarded(MakeQueue, "nvnQueueInitialize: dkQueueCreate")) {
        return 0;
    }
    memset(queue, 0, sizeof(*queue));
    record->magic = MAGIC_QUEUE;
    record->queue = pending.queue;
    return 1;
}

void Nvnl_nvnQueueFinalize(NVNqueue *queue) {
    LayerQueue *const record = QueueOf(queue, "nvnQueueFinalize");
    if (record == NULL) {
        return;
    }
    dkQueueDestroy(record->queue);
    record->magic = 0;
}

void Nvnl_nvnQueueSubmitCommands(NVNqueue *queue, int count,
                                        const NVNcommandHandle *handles) {
    LayerQueue *const record = QueueOf(queue, "nvnQueueSubmitCommands");
    if (record == NULL) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        dkQueueSubmitCommands(record->queue, (DkCmdList)handles[i]);
    }
}

void Nvnl_nvnQueueFlush(NVNqueue *queue) {
    LayerQueue *const record = QueueOf(queue, "nvnQueueFlush");
    if (record != NULL) {
        dkQueueFlush(record->queue);
    }
}

void Nvnl_nvnQueueFinish(NVNqueue *queue) {
    LayerQueue *const record = QueueOf(queue, "nvnQueueFinish");
    if (record == NULL) {
        return;
    }
    dkQueueWaitIdle(record->queue);
    // NVN has nvnQueueGetError for this; the layer says it where a case reads it.
    if (dkQueueIsInErrorState(record->queue)) {
        LayerComplain("nvnQueueFinish: the queue faulted on the graphics unit");
    }
}

NVNqueueGetErrorResult Nvnl_nvnQueueGetError(NVNqueue *queue, NVNqueueErrorInfo *info) {
    // deko3d says that the queue faulted and not how, so a fault is NVN's unknown error and
    // the info, which NVN fills for an error it can name, stays as it was.
    (void)info;
    LayerQueue *const record = QueueOf(queue, "nvnQueueGetError");
    if (record == NULL) {
        return NVN_QUEUE_GET_ERROR_RESULT_GPU_ERROR_UNKNOWN;
    }
    return dkQueueIsInErrorState(record->queue) ? NVN_QUEUE_GET_ERROR_RESULT_GPU_ERROR_UNKNOWN
                                                : NVN_QUEUE_GET_ERROR_RESULT_GPU_NO_ERROR;
}

void Nvnl_nvnQueueBuilderSetFlags(NVNqueueBuilder *builder, int flags) {
    LayerQueueBuilder *const record = Record(builder, "nvnQueueBuilderSetFlags");
    if (record != NULL) {
        record->flags = flags;
    }
}

NVNdevice *Nvnl_nvnQueueBuilderGetDevice(const NVNqueueBuilder *builder) {
    const LayerQueueBuilder *const record = Record(builder, "nvnQueueBuilderGetDevice");
    return record != NULL ? record->device : NULL;
}

int Nvnl_nvnQueueBuilderGetFlags(const NVNqueueBuilder *builder) {
    const LayerQueueBuilder *const record = Record(builder, "nvnQueueBuilderGetFlags");
    return record != NULL ? record->flags : 0;
}

// A size of a queue builder: set, it is kept and given back with a true; left alone, the getter
// answers false and a size of nought, the driver choosing.
#define QUEUE_SIZE(name, which)                                                                   \
    void Nvnl_nvnQueueBuilderSet##name(NVNqueueBuilder *builder, size_t size) {                   \
        LayerQueueBuilder *const record = Record(builder, "nvnQueueBuilderSet" #name);            \
        if (record != NULL) {                                                                     \
            record->set |= 1u << (which);                                                         \
            record->sizes[which] = size;                                                          \
        }                                                                                         \
    }                                                                                             \
    NVNboolean Nvnl_nvnQueueBuilderGet##name(const NVNqueueBuilder *builder, int *size) {         \
        const LayerQueueBuilder *const record = Record(builder, "nvnQueueBuilderGet" #name);      \
        const bool set = record != NULL && (record->set & 1u << (which)) != 0;                    \
        *size = set ? (int)record->sizes[which] : 0;                                              \
        return set;                                                                               \
    }
QUEUE_SIZE(CommandMemorySize, QUEUE_SIZE_COMMAND)
QUEUE_SIZE(ComputeMemorySize, QUEUE_SIZE_COMPUTE)
QUEUE_SIZE(ControlMemorySize, QUEUE_SIZE_CONTROL)
QUEUE_SIZE(CommandFlushThreshold, QUEUE_SIZE_FLUSH)
#undef QUEUE_SIZE

// ---- syncs

// The fences behind the program's syncs. A sync is fenced when a queue or a list was told to
// signal it; one never fenced has nothing to wait for.
#define SYNCS_MAX 16
static struct {
    bool used;
    bool fenced;
    DkFence fence;
} syncs[SYNCS_MAX];

/// The layer's fence of @p sync, or -1 with a complaint.
static int SyncOf(const NVNsync *sync, const char *what) {
    const LayerSync *const record = Record(sync, what);
    if (record == NULL) {
        return -1;
    }
    if (record->magic != MAGIC_SYNC || record->slot >= SYNCS_MAX || !syncs[record->slot].used) {
        LayerComplain("%s: the sync was not initialized", what);
        return -1;
    }
    return (int)record->slot;
}

NVNboolean Nvnl_nvnSyncInitialize(NVNsync *sync, NVNdevice *device) {
    LayerSync *const record = Record(sync, "nvnSyncInitialize");
    if (record == NULL || DeviceOf(device, "nvnSyncInitialize") == NULL) {
        return 0;
    }
    for (uint32_t slot = 0; slot < SYNCS_MAX; ++slot) {
        if (!syncs[slot].used) {
            memset(&syncs[slot], 0, sizeof(syncs[slot]));
            syncs[slot].used = true;
            memset(sync, 0, sizeof(*sync));
            record->magic = MAGIC_SYNC;
            record->slot = slot;
            return 1;
        }
    }
    LayerComplain("nvnSyncInitialize: a sync past the %d the layer keeps at a time", SYNCS_MAX);
    return 0;
}

void Nvnl_nvnSyncFinalize(NVNsync *sync) {
    const int slot = SyncOf(sync, "nvnSyncFinalize");
    if (slot >= 0) {
        syncs[slot].used = false;
        ((LayerSync *)(void *)sync)->magic = 0;
    }
}

NVNsyncWaitResult Nvnl_nvnSyncWait(const NVNsync *sync, uint64_t timeout) {
    const int slot = SyncOf(sync, "nvnSyncWait");
    if (slot < 0 || !syncs[slot].fenced) {
        // Nothing will ever signal it: NVN fails the wait whatever the timeout.
        return NVN_SYNC_WAIT_RESULT_FAILED;
    }
    DkResult result = dkFenceWait(&syncs[slot].fence, 0);
    if (result == DkResult_Success) {
        return NVN_SYNC_WAIT_RESULT_ALREADY_SIGNALED;
    }
    if (result == DkResult_Timeout && timeout != 0u) {
        // deko3d waits for good on a negative time, which is what a timeout past its range means.
        result = dkFenceWait(&syncs[slot].fence,
                             timeout > (uint64_t)INT64_MAX ? -1 : (int64_t)timeout);
        if (result == DkResult_Success) {
            return NVN_SYNC_WAIT_RESULT_CONDITION_SATISFIED;
        }
    }
    return result == DkResult_Timeout ? NVN_SYNC_WAIT_RESULT_TIMEOUT_EXPIRED
                                      : NVN_SYNC_WAIT_RESULT_FAILED;
}

/// What NVN's fence arguments come to in deko3d, which has the one condition, everything the
/// queue was given done; false with a complaint for another, or for a flag.
static bool FenceTaken(NVNsyncCondition condition, int flags, const char *what) {
    if (condition != NVN_SYNC_CONDITION_ALL_GPU_COMMANDS_COMPLETE || flags != 0) {
        LayerComplain("%s: sync condition %d with flags 0x%x, and the layer fences for all "
                      "commands complete (%d) with no flag",
                      what, (int)condition, (unsigned)flags,
                      (int)NVN_SYNC_CONDITION_ALL_GPU_COMMANDS_COMPLETE);
        return false;
    }
    return true;
}

void Nvnl_nvnQueueFenceSync(NVNqueue *queue, NVNsync *sync, NVNsyncCondition condition,
                            int flags) {
    LayerQueue *const record = QueueOf(queue, "nvnQueueFenceSync");
    const int slot = SyncOf(sync, "nvnQueueFenceSync");
    if (record == NULL || slot < 0 || !FenceTaken(condition, flags, "nvnQueueFenceSync")) {
        return;
    }
    // NVN leaves the flush to the program (nvnQueueFlush), and so does this.
    dkQueueSignalFence(record->queue, &syncs[slot].fence, false);
    syncs[slot].fenced = true;
}

NVNboolean Nvnl_nvnQueueWaitSync(NVNqueue *queue, const NVNsync *sync) {
    LayerQueue *const record = QueueOf(queue, "nvnQueueWaitSync");
    const int slot = SyncOf(sync, "nvnQueueWaitSync");
    if (record == NULL || slot < 0) {
        return 0;
    }
    if (!syncs[slot].fenced) {
        LayerComplain("nvnQueueWaitSync: a sync nothing fenced, which the queue would wait on "
                      "for good");
        return 0;
    }
    dkQueueWaitFence(record->queue, &syncs[slot].fence);
    return 1;
}

void Nvnl_nvnCommandBufferFenceSync(NVNcommandBuffer *cmd, NVNsync *sync,
                                    NVNsyncCondition condition, int flags) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferFenceSync");
    const int slot = SyncOf(sync, "nvnCommandBufferFenceSync");
    if (record == NULL || slot < 0 ||
        !FenceTaken(condition, flags, "nvnCommandBufferFenceSync")) {
        return;
    }
    dkCmdBufSignalFence(record->cmdbuf, &syncs[slot].fence, false);
    syncs[slot].fenced = true;
}

void Nvnl_nvnCommandBufferWaitSync(NVNcommandBuffer *cmd, const NVNsync *sync) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferWaitSync");
    const int slot = SyncOf(sync, "nvnCommandBufferWaitSync");
    if (record == NULL || slot < 0) {
        return;
    }
    if (!syncs[slot].fenced) {
        LayerComplain("nvnCommandBufferWaitSync: a sync nothing fenced, which the list would "
                      "wait on for good");
        return;
    }
    dkCmdBufWaitFence(record->cmdbuf, &syncs[slot].fence);
}

/// The textures @p record hands in turn: the ones named, all of them when none or too many were.
static int WindowActive(const LayerWindow *record) {
    return record->active > 0 && record->active < record->count ? record->active : record->count;
}

/// Whether @p record may acquire: deko3d takes whichever texture the display hands back, so a
/// window told to hand only its first few is one the layer cannot keep its word for.
static bool WindowHandsAll(const LayerWindow *record, const char *what) {
    if (WindowActive(record) != record->count) {
        LayerComplain("%s: %d of the window's %d textures are active, and the layer acquires "
                      "from all of them",
                      what, WindowActive(record), record->count);
        return false;
    }
    return true;
}

NVNqueueAcquireTextureResult Nvnl_nvnQueueAcquireTexture(NVNqueue *queue,
                                                                NVNwindow *window, int *index) {
    LayerQueue *const record = QueueOf(queue, "nvnQueueAcquireTexture");
    LayerWindow *const target = WindowOf(window, "nvnQueueAcquireTexture");
    if (record == NULL || target == NULL || !WindowHandsAll(target, "nvnQueueAcquireTexture")) {
        return NVN_QUEUE_ACQUIRE_TEXTURE_RESULT_NATIVE_ERROR;
    }
    pending.acquire_queue = record->queue;
    pending.acquire_swapchain = target->swapchain;
    pending.acquired = -1;
    if (!LayerGuarded(AcquireImage, "nvnQueueAcquireTexture: dkQueueAcquireImage") ||
        pending.acquired < 0) {
        LayerComplain("nvnQueueAcquireTexture: dkQueueAcquireImage gave slot %d", pending.acquired);
        return NVN_QUEUE_ACQUIRE_TEXTURE_RESULT_NATIVE_ERROR;
    }
    *index = pending.acquired;
    return NVN_QUEUE_ACQUIRE_TEXTURE_RESULT_SUCCESS;
}

/// The window's own acquire: the texture comes at once, and @p sync signals when the display has
/// let it go, which is deko3d's fence of the dequeued buffer.
NVNwindowAcquireTextureResult Nvnl_nvnWindowAcquireTexture(NVNwindow *window, NVNsync *sync,
                                                           int *index) {
    LayerWindow *const target = WindowOf(window, "nvnWindowAcquireTexture");
    const int slot = SyncOf(sync, "nvnWindowAcquireTexture");
    if (target == NULL || slot < 0 || !WindowHandsAll(target, "nvnWindowAcquireTexture")) {
        return NVN_WINDOW_ACQUIRE_TEXTURE_RESULT_NATIVE_ERROR;
    }
    pending.acquire_swapchain = target->swapchain;
    pending.acquire_fence = &syncs[slot].fence;
    pending.acquired = -1;
    if (!LayerGuarded(AcquireFenced, "nvnWindowAcquireTexture: dkSwapchainAcquireImage") ||
        pending.acquired < 0) {
        LayerComplain("nvnWindowAcquireTexture: dkSwapchainAcquireImage gave slot %d",
                      pending.acquired);
        return NVN_WINDOW_ACQUIRE_TEXTURE_RESULT_NATIVE_ERROR;
    }
    syncs[slot].fenced = true;
    *index = pending.acquired;
    return NVN_WINDOW_ACQUIRE_TEXTURE_RESULT_SUCCESS;
}

void Nvnl_nvnQueuePresentTexture(NVNqueue *queue, NVNwindow *window, int index) {
    LayerQueue *const record = QueueOf(queue, "nvnQueuePresentTexture");
    LayerWindow *const target = WindowOf(window, "nvnQueuePresentTexture");
    if (record != NULL && target != NULL) {
        dkQueuePresentImage(record->queue, target->swapchain, index);
    }
}

// ---- memory pools

// The pools that hold shader code. NVN names a program's code by its GPU address alone
// (NVNshaderData.data), and deko3d takes it as a block and an offset, so the layer has to find
// the block again from the address.
#define CODE_POOLS_MAX 8
static DkMemBlock code_pools[CODE_POOLS_MAX];

static bool CodePoolAdd(DkMemBlock block) {
    for (int i = 0; i < CODE_POOLS_MAX; ++i) {
        if (code_pools[i] == NULL) {
            code_pools[i] = block;
            return true;
        }
    }
    return false;
}

static void CodePoolDrop(DkMemBlock block) {
    for (int i = 0; i < CODE_POOLS_MAX; ++i) {
        if (code_pools[i] == block) {
            code_pools[i] = NULL;
        }
    }
}

DkMemBlock LayerCodePool(NVNbufferAddress address, uint32_t *offset) {
    for (int i = 0; i < CODE_POOLS_MAX; ++i) {
        if (code_pools[i] == NULL) {
            continue;
        }
        const DkGpuAddr base = dkMemBlockGetGpuAddr(code_pools[i]);
        if (address >= base && address - base < dkMemBlockGetSize(code_pools[i])) {
            *offset = (uint32_t)(address - base);
            return code_pools[i];
        }
    }
    return NULL;
}

void Nvnl_nvnMemoryPoolBuilderSetDefaults(NVNmemoryPoolBuilder *builder) {
    memset(builder, 0, sizeof(*builder));
}

void Nvnl_nvnMemoryPoolBuilderSetDevice(NVNmemoryPoolBuilder *builder, NVNdevice *device) {
    LayerPoolBuilder *const record = Record(builder, "nvnMemoryPoolBuilderSetDevice");
    if (record != NULL) {
        record->device = device;
    }
}

void Nvnl_nvnMemoryPoolBuilderSetStorage(NVNmemoryPoolBuilder *builder, void *memory,
                                                size_t size) {
    LayerPoolBuilder *const record = Record(builder, "nvnMemoryPoolBuilderSetStorage");
    if (record != NULL) {
        record->storage = memory;
        record->size = size;
    }
}

void Nvnl_nvnMemoryPoolBuilderSetFlags(NVNmemoryPoolBuilder *builder, int flags) {
    LayerPoolBuilder *const record = Record(builder, "nvnMemoryPoolBuilderSetFlags");
    if (record != NULL) {
        record->flags = flags;
    }
}

/// deko3d's access bits for one of NVN's three-way choices (no access, uncached, cached), or
/// false when the flags name more than one of them.
static bool Access(int flags, int none, int uncached, int cached, uint32_t *access) {
    const int named = flags & (none | uncached | cached);
    if (named == none) {
        *access = DkMemAccess_None;
    } else if (named == uncached) {
        *access = DkMemAccess_Uncached;
    } else if (named == cached) {
        *access = DkMemAccess_Cached;
    } else {
        return false;
    }
    return true;
}

NVNboolean Nvnl_nvnMemoryPoolInitialize(NVNmemoryPool *pool,
                                               const NVNmemoryPoolBuilder *builder) {
    LayerPool *const record = Record(pool, "nvnMemoryPoolInitialize");
    const LayerPoolBuilder *const from = Record(builder, "nvnMemoryPoolInitialize");
    if (record == NULL || from == NULL) {
        return 0;
    }
    LayerDevice *const device = DeviceOf(from->device, "nvnMemoryPoolInitialize");
    if (device == NULL) {
        return 0;
    }
    const int known = NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS | NVN_MEMORY_POOL_FLAGS_CPU_UNCACHED |
                      NVN_MEMORY_POOL_FLAGS_CPU_CACHED | NVN_MEMORY_POOL_FLAGS_GPU_NO_ACCESS |
                      NVN_MEMORY_POOL_FLAGS_GPU_UNCACHED | NVN_MEMORY_POOL_FLAGS_GPU_CACHED |
                      NVN_MEMORY_POOL_FLAGS_SHADER_CODE | NVN_MEMORY_POOL_FLAGS_COMPRESSIBLE;
    uint32_t cpu = 0;
    uint32_t gpu = 0;
    if ((from->flags & ~known) != 0 ||
        !Access(from->flags, NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS,
                NVN_MEMORY_POOL_FLAGS_CPU_UNCACHED, NVN_MEMORY_POOL_FLAGS_CPU_CACHED, &cpu) ||
        !Access(from->flags, NVN_MEMORY_POOL_FLAGS_GPU_NO_ACCESS,
                NVN_MEMORY_POOL_FLAGS_GPU_UNCACHED, NVN_MEMORY_POOL_FLAGS_GPU_CACHED, &gpu)) {
        LayerComplain("nvnMemoryPoolInitialize: pool flags 0x%x, which the layer does not take: "
                      "one processor and one graphics access each, shader code and compressible",
                      (unsigned)from->flags);
        return 0;
    }
    if (from->storage == NULL || ((uintptr_t)from->storage & (DK_MEMBLOCK_ALIGNMENT - 1)) != 0 ||
        from->size == 0 || (from->size & (DK_MEMBLOCK_ALIGNMENT - 1)) != 0 ||
        from->size > UINT32_MAX) {
        LayerComplain("nvnMemoryPoolInitialize: storage %p of 0x%zx bytes, where NVN and deko3d "
                      "take a non-empty multiple of 0x%x bytes at that alignment",
                      from->storage, from->size, (unsigned)DK_MEMBLOCK_ALIGNMENT);
        return 0;
    }
    uint32_t flags = cpu << DkMemBlockFlags_CpuAccessShift | gpu << DkMemBlockFlags_GpuAccessShift;
    if ((from->flags & NVN_MEMORY_POOL_FLAGS_SHADER_CODE) != 0) {
        flags |= DkMemBlockFlags_Code;
    } else if (gpu != DkMemAccess_None) {
        // Any pool the graphics unit reaches can hold a texture in NVN; in deko3d that takes the
        // image mappings, the compressed one among them, which is what COMPRESSIBLE asks for.
        flags |= DkMemBlockFlags_Image;
    }
    dkMemBlockMakerDefaults(&pending.block_maker, DeviceUse(device), (uint32_t)from->size);
    pending.block_maker.flags = flags;
    pending.block_maker.storage = from->storage;
    pending.block = NULL;
    if (!LayerGuarded(MakeBlock, "nvnMemoryPoolInitialize: dkMemBlockCreate")) {
        return 0;
    }
    memset(pool, 0, sizeof(*pool));
    record->magic = MAGIC_POOL;
    record->block = pending.block;
    record->flags = from->flags;
    record->size = from->size;
    if ((from->flags & NVN_MEMORY_POOL_FLAGS_SHADER_CODE) != 0 && !CodePoolAdd(pending.block)) {
        LayerComplain("nvnMemoryPoolInitialize: a shader code pool past the %d the layer keeps "
                      "track of at a time",
                      CODE_POOLS_MAX);
        dkMemBlockDestroy(pending.block);
        record->magic = 0;
        return 0;
    }
    return 1;
}

void Nvnl_nvnMemoryPoolFinalize(NVNmemoryPool *pool) {
    LayerPool *const record = PoolOf(pool, "nvnMemoryPoolFinalize");
    if (record == NULL) {
        return;
    }
    CodePoolDrop(record->block);
    dkMemBlockDestroy(record->block);
    record->magic = 0;
}

void *Nvnl_nvnMemoryPoolMap(const NVNmemoryPool *pool) {
    LayerPool *const record = PoolOf(pool, "nvnMemoryPoolMap");
    return record != NULL ? dkMemBlockGetCpuAddr(record->block) : NULL;
}

NVNbufferAddress Nvnl_nvnMemoryPoolGetBufferAddress(const NVNmemoryPool *pool) {
    LayerPool *const record = PoolOf(pool, "nvnMemoryPoolGetBufferAddress");
    return record != NULL ? dkMemBlockGetGpuAddr(record->block) : 0;
}

/// Whether @p size bytes at @p offset are inside @p record's pool; a complaint with the numbers
/// when they are not.
static bool InPool(const LayerPool *record, ptrdiff_t offset, size_t size, const char *what) {
    if (offset < 0 || (size_t)offset > record->size || size > record->size - (size_t)offset) {
        LayerComplain("%s: 0x%zx bytes at offset %lld, of a pool of 0x%zx", what, size,
                      (long long)offset, record->size);
        return false;
    }
    return true;
}

void Nvnl_nvnMemoryPoolFlushMappedRange(const NVNmemoryPool *pool, ptrdiff_t offset,
                                               size_t size) {
    LayerPool *const record = PoolOf(pool, "nvnMemoryPoolFlushMappedRange");
    if (record != NULL && InPool(record, offset, size, "nvnMemoryPoolFlushMappedRange")) {
        dkMemBlockFlushCpuCache(record->block, (uint32_t)offset, (uint32_t)size);
    }
}

void Nvnl_nvnMemoryPoolInvalidateMappedRange(const NVNmemoryPool *pool, ptrdiff_t offset,
                                             size_t size) {
    // deko3d has the one call for the processor's cache of a block, which writes the range back
    // and drops it: after it a read takes what the graphics unit wrote, which is what NVN's
    // invalidate is for, and nothing of the program's is lost by the write back.
    LayerPool *const record = PoolOf(pool, "nvnMemoryPoolInvalidateMappedRange");
    if (record != NULL && InPool(record, offset, size, "nvnMemoryPoolInvalidateMappedRange")) {
        dkMemBlockFlushCpuCache(record->block, (uint32_t)offset, (uint32_t)size);
    }
}

NVNdevice *Nvnl_nvnMemoryPoolBuilderGetDevice(const NVNmemoryPoolBuilder *builder) {
    const LayerPoolBuilder *const record = Record(builder, "nvnMemoryPoolBuilderGetDevice");
    return record != NULL ? record->device : NULL;
}

void *Nvnl_nvnMemoryPoolBuilderGetMemory(const NVNmemoryPoolBuilder *builder) {
    const LayerPoolBuilder *const record = Record(builder, "nvnMemoryPoolBuilderGetMemory");
    return record != NULL ? record->storage : NULL;
}

size_t Nvnl_nvnMemoryPoolBuilderGetSize(const NVNmemoryPoolBuilder *builder) {
    const LayerPoolBuilder *const record = Record(builder, "nvnMemoryPoolBuilderGetSize");
    return record != NULL ? record->size : 0;
}

NVNmemoryPoolFlags Nvnl_nvnMemoryPoolBuilderGetFlags(const NVNmemoryPoolBuilder *builder) {
    const LayerPoolBuilder *const record = Record(builder, "nvnMemoryPoolBuilderGetFlags");
    return record != NULL ? (NVNmemoryPoolFlags)record->flags : 0;
}

size_t Nvnl_nvnMemoryPoolGetSize(const NVNmemoryPool *pool) {
    const LayerPool *const record = PoolOf(pool, "nvnMemoryPoolGetSize");
    return record != NULL ? record->size : 0;
}

NVNmemoryPoolFlags Nvnl_nvnMemoryPoolGetFlags(const NVNmemoryPool *pool) {
    const LayerPool *const record = PoolOf(pool, "nvnMemoryPoolGetFlags");
    return record != NULL ? (NVNmemoryPoolFlags)record->flags : 0;
}

// ---- buffers

void Nvnl_nvnBufferBuilderSetDefaults(NVNbufferBuilder *builder) {
    // The device stays, as NVN's own builder keeps it.
    LayerBufferBuilder *const record = Record(builder, "nvnBufferBuilderSetDefaults");
    if (record == NULL) {
        return;
    }
    NVNdevice *const device = record->device;
    memset(builder, 0, sizeof(*builder));
    record->device = device;
}

void Nvnl_nvnBufferBuilderSetDevice(NVNbufferBuilder *builder, NVNdevice *device) {
    LayerBufferBuilder *const record = Record(builder, "nvnBufferBuilderSetDevice");
    if (record != NULL) {
        record->device = device;
    }
}

void Nvnl_nvnBufferBuilderSetStorage(NVNbufferBuilder *builder, NVNmemoryPool *pool,
                                     ptrdiff_t offset, size_t size) {
    LayerBufferBuilder *const record = Record(builder, "nvnBufferBuilderSetStorage");
    if (record != NULL) {
        record->pool = pool;
        record->offset = offset;
        record->size = size;
    }
}

NVNdevice *Nvnl_nvnBufferBuilderGetDevice(const NVNbufferBuilder *builder) {
    const LayerBufferBuilder *const record = Record(builder, "nvnBufferBuilderGetDevice");
    return record != NULL ? record->device : NULL;
}

NVNmemoryPool *Nvnl_nvnBufferBuilderGetMemoryPool(const NVNbufferBuilder *builder) {
    const LayerBufferBuilder *const record = Record(builder, "nvnBufferBuilderGetMemoryPool");
    return record != NULL ? record->pool : NULL;
}

ptrdiff_t Nvnl_nvnBufferBuilderGetMemoryOffset(const NVNbufferBuilder *builder) {
    const LayerBufferBuilder *const record = Record(builder, "nvnBufferBuilderGetMemoryOffset");
    return record != NULL ? record->offset : 0;
}

size_t Nvnl_nvnBufferBuilderGetSize(const NVNbufferBuilder *builder) {
    const LayerBufferBuilder *const record = Record(builder, "nvnBufferBuilderGetSize");
    return record != NULL ? record->size : 0;
}

NVNboolean Nvnl_nvnBufferInitialize(NVNbuffer *buffer, const NVNbufferBuilder *builder) {
    LayerBuffer *const record = Record(buffer, "nvnBufferInitialize");
    const LayerBufferBuilder *const from = Record(builder, "nvnBufferInitialize");
    if (record == NULL || from == NULL) {
        return 0;
    }
    const LayerPool *const pool = PoolOf(from->pool, "nvnBufferInitialize");
    if (pool == NULL || !InPool(pool, from->offset, from->size, "nvnBufferInitialize")) {
        return 0;
    }
    memset(buffer, 0, sizeof(*buffer));
    record->magic = MAGIC_BUFFER;
    record->pool = from->pool;
    record->offset = from->offset;
    record->size = from->size;
    return 1;
}

/// The record of @p buffer with its pool's, or NULL with a complaint.
static const LayerBuffer *BufferOf(const NVNbuffer *buffer, const LayerPool **pool,
                                   const char *what) {
    const LayerBuffer *const record = Record(buffer, what);
    if (record == NULL) {
        return NULL;
    }
    if (record->magic != MAGIC_BUFFER) {
        LayerComplain("%s: the buffer was not initialized", what);
        return NULL;
    }
    *pool = PoolOf(record->pool, what);
    return *pool != NULL ? record : NULL;
}

void Nvnl_nvnBufferFinalize(NVNbuffer *buffer) {
    LayerBuffer *const record = Record(buffer, "nvnBufferFinalize");
    if (record != NULL && record->magic == MAGIC_BUFFER) {
        record->magic = 0;
    }
}

void *Nvnl_nvnBufferMap(const NVNbuffer *buffer) {
    const LayerPool *pool = NULL;
    const LayerBuffer *const record = BufferOf(buffer, &pool, "nvnBufferMap");
    if (record == NULL) {
        return NULL;
    }
    uint8_t *const base = dkMemBlockGetCpuAddr(pool->block);
    return base != NULL ? base + record->offset : NULL;
}

NVNbufferAddress Nvnl_nvnBufferGetAddress(const NVNbuffer *buffer) {
    const LayerPool *pool = NULL;
    const LayerBuffer *const record = BufferOf(buffer, &pool, "nvnBufferGetAddress");
    return record != NULL ? dkMemBlockGetGpuAddr(pool->block) + (uint64_t)record->offset : 0;
}

NVNmemoryPool *Nvnl_nvnBufferGetMemoryPool(const NVNbuffer *buffer) {
    const LayerPool *pool = NULL;
    const LayerBuffer *const record = BufferOf(buffer, &pool, "nvnBufferGetMemoryPool");
    return record != NULL ? record->pool : NULL;
}

ptrdiff_t Nvnl_nvnBufferGetMemoryOffset(const NVNbuffer *buffer) {
    const LayerPool *pool = NULL;
    const LayerBuffer *const record = BufferOf(buffer, &pool, "nvnBufferGetMemoryOffset");
    return record != NULL ? record->offset : 0;
}

size_t Nvnl_nvnBufferGetSize(const NVNbuffer *buffer) {
    const LayerPool *pool = NULL;
    const LayerBuffer *const record = BufferOf(buffer, &pool, "nvnBufferGetSize");
    return record != NULL ? record->size : 0;
}

/// deko3d's one call for the processor's cache, over @p size bytes at @p offset of @p buffer:
/// what both of a buffer's cache calls come to, as both of a pool's do.
static void BufferCache(const NVNbuffer *buffer, ptrdiff_t offset, size_t size,
                        const char *what) {
    const LayerPool *pool = NULL;
    const LayerBuffer *const record = BufferOf(buffer, &pool, what);
    if (record == NULL) {
        return;
    }
    if (offset < 0 || (size_t)offset > record->size || size > record->size - (size_t)offset) {
        LayerComplain("%s: 0x%zx bytes at offset %lld, of a buffer of 0x%zx", what, size,
                      (long long)offset, record->size);
        return;
    }
    dkMemBlockFlushCpuCache(pool->block, (uint32_t)(record->offset + offset), (uint32_t)size);
}

void Nvnl_nvnBufferFlushMappedRange(const NVNbuffer *buffer, ptrdiff_t offset, size_t size) {
    BufferCache(buffer, offset, size, "nvnBufferFlushMappedRange");
}

void Nvnl_nvnBufferInvalidateMappedRange(const NVNbuffer *buffer, ptrdiff_t offset,
                                         size_t size) {
    BufferCache(buffer, offset, size, "nvnBufferInvalidateMappedRange");
}

// ---- events

void Nvnl_nvnEventBuilderSetDefaults(NVNeventBuilder *builder) {
    if (Record(builder, "nvnEventBuilderSetDefaults") != NULL) {
        memset(builder, 0, sizeof(*builder));
    }
}

void Nvnl_nvnEventBuilderSetStorage(NVNeventBuilder *builder, const NVNmemoryPool *pool,
                                    ptrdiff_t offset) {
    LayerEventBuilder *const record = Record(builder, "nvnEventBuilderSetStorage");
    if (record != NULL) {
        record->pool = pool;
        record->offset = offset;
    }
}

const NVNmemoryPool *Nvnl_nvnEventBuilderGetStorage(const NVNeventBuilder *builder,
                                                    ptrdiff_t *offset) {
    const LayerEventBuilder *const record = Record(builder, "nvnEventBuilderGetStorage");
    if (record == NULL) {
        return NULL;
    }
    *offset = record->offset;
    return record->pool;
}

const NVNmemoryPool *Nvnl_nvnEventBuilderGetMemoryPool(const NVNeventBuilder *builder) {
    const LayerEventBuilder *const record = Record(builder, "nvnEventBuilderGetMemoryPool");
    return record != NULL ? record->pool : NULL;
}

ptrdiff_t Nvnl_nvnEventBuilderGetMemoryOffset(const NVNeventBuilder *builder) {
    const LayerEventBuilder *const record = Record(builder, "nvnEventBuilderGetMemoryOffset");
    return record != NULL ? record->offset : 0;
}

/// The word of @p event in its pool's mapping, with the pool's record; NULL with a complaint.
static uint32_t *EventWord(const NVNevent *event, const LayerPool **pool, const char *what) {
    const LayerEvent *const record = Record(event, what);
    if (record == NULL) {
        return NULL;
    }
    if (record->magic != MAGIC_EVENT) {
        LayerComplain("%s: the event was not initialized", what);
        return NULL;
    }
    *pool = PoolOf(record->pool, what);
    if (*pool == NULL) {
        return NULL;
    }
    return (uint32_t *)(void *)((uint8_t *)dkMemBlockGetCpuAddr((*pool)->block) + record->offset);
}

NVNboolean Nvnl_nvnEventInitialize(NVNevent *event, const NVNeventBuilder *builder) {
    LayerEvent *const record = Record(event, "nvnEventInitialize");
    const LayerEventBuilder *const from = Record(builder, "nvnEventInitialize");
    if (record == NULL || from == NULL) {
        return 0;
    }
    const LayerPool *const pool = PoolOf(from->pool, "nvnEventInitialize");
    if (pool == NULL) {
        return 0;
    }
    if ((pool->flags & NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS) != 0 || (from->offset & 3) != 0 ||
        !InPool(pool, from->offset, sizeof(uint32_t), "nvnEventInitialize")) {
        LayerComplain("nvnEventInitialize: an event at offset %lld of a pool with flags 0x%x, "
                      "where the layer takes a word on a four byte boundary inside a pool the "
                      "processor reaches",
                      (long long)from->offset, (unsigned)pool->flags);
        return 0;
    }
    memset(event, 0, sizeof(*event));
    record->magic = MAGIC_EVENT;
    record->pool = from->pool;
    record->offset = from->offset;
    return 1;
}

void Nvnl_nvnEventFinalize(NVNevent *event) {
    LayerEvent *const record = Record(event, "nvnEventFinalize");
    if (record != NULL) {
        record->magic = 0;
    }
}

uint32_t Nvnl_nvnEventGetValue(const NVNevent *event) {
    const LayerPool *pool = NULL;
    const uint32_t *const word = EventWord(event, &pool, "nvnEventGetValue");
    return word != NULL ? *word : 0u;
}

void Nvnl_nvnEventSignal(NVNevent *event, NVNeventSignalMode mode, uint32_t value) {
    const LayerPool *pool = NULL;
    uint32_t *const word = EventWord(event, &pool, "nvnEventSignal");
    if (word == NULL) {
        return;
    }
    if (mode != NVN_EVENT_SIGNAL_MODE_WRITE && mode != NVN_EVENT_SIGNAL_MODE_ADD) {
        LayerComplain("nvnEventSignal: signal mode %d, and the layer takes write (%d) and add "
                      "(%d)",
                      (int)mode, (int)NVN_EVENT_SIGNAL_MODE_WRITE, (int)NVN_EVENT_SIGNAL_MODE_ADD);
        return;
    }
    *word = mode == NVN_EVENT_SIGNAL_MODE_ADD ? *word + value : value;
    if ((pool->flags & NVN_MEMORY_POOL_FLAGS_CPU_CACHED) != 0) {
        // A list that waits on the event reads the word from the graphics unit's side.
        const LayerEvent *const record = (const LayerEvent *)(const void *)event;
        dkMemBlockFlushCpuCache(pool->block, (uint32_t)record->offset, sizeof(uint32_t));
    }
}

const NVNmemoryPool *Nvnl_nvnEventGetMemoryPool(const NVNevent *event) {
    const LayerEvent *const record = Record(event, "nvnEventGetMemoryPool");
    return record != NULL && record->magic == MAGIC_EVENT ? record->pool : NULL;
}

ptrdiff_t Nvnl_nvnEventGetMemoryOffset(const NVNevent *event) {
    const LayerEvent *const record = Record(event, "nvnEventGetMemoryOffset");
    return record != NULL && record->magic == MAGIC_EVENT ? record->offset : 0;
}

// ---- textures

void Nvnl_nvnTextureBuilderSetDefaults(NVNtextureBuilder *builder) {
    // NVN's defaults: a 2D texture of one texel, one level; the device stays, as NVN keeps it.
    LayerTextureBuilder *const record = Record(builder, "nvnTextureBuilderSetDefaults");
    if (record == NULL) {
        return;
    }
    NVNdevice *const device = record->device;
    memset(builder, 0, sizeof(*builder));
    record->device = device;
    record->target = NVN_TEXTURE_TARGET_2D;
    record->format = NVN_FORMAT_NONE;
    record->width = 1;
    record->height = 1;
    record->depth = 1;
    record->levels = 1;
}

void Nvnl_nvnTextureBuilderSetDevice(NVNtextureBuilder *builder, NVNdevice *device) {
    LayerTextureBuilder *const record = Record(builder, "nvnTextureBuilderSetDevice");
    if (record != NULL) {
        record->device = device;
    }
}

void Nvnl_nvnTextureBuilderSetFlags(NVNtextureBuilder *builder, int flags) {
    LayerTextureBuilder *const record = Record(builder, "nvnTextureBuilderSetFlags");
    if (record != NULL) {
        record->flags = flags;
    }
}

void Nvnl_nvnTextureBuilderSetTarget(NVNtextureBuilder *builder, NVNtextureTarget target) {
    LayerTextureBuilder *const record = Record(builder, "nvnTextureBuilderSetTarget");
    if (record != NULL) {
        record->target = target;
    }
}

void Nvnl_nvnTextureBuilderSetFormat(NVNtextureBuilder *builder, NVNformat format) {
    LayerTextureBuilder *const record = Record(builder, "nvnTextureBuilderSetFormat");
    if (record != NULL) {
        record->format = format;
    }
}

void Nvnl_nvnTextureBuilderSetSize2D(NVNtextureBuilder *builder, int width, int height) {
    LayerTextureBuilder *const record = Record(builder, "nvnTextureBuilderSetSize2D");
    if (record != NULL) {
        record->width = width;
        record->height = height;
    }
}

void Nvnl_nvnTextureBuilderSetStorage(NVNtextureBuilder *builder, NVNmemoryPool *pool,
                                             ptrdiff_t offset) {
    LayerTextureBuilder *const record = Record(builder, "nvnTextureBuilderSetStorage");
    if (record != NULL) {
        record->pool = pool;
        record->offset = offset;
    }
}

// A builder keeps whatever number it is given, as NVN's does: what the layer cannot make of it
// is said when a size is asked or the texture is made (LayoutOf), with the number.
#define BUILDER_SET(name, type, field)                                                            \
    void Nvnl_nvnTextureBuilderSet##name(NVNtextureBuilder *builder, type value) {                \
        LayerTextureBuilder *const record = Record(builder, "nvnTextureBuilderSet" #name);        \
        if (record != NULL) {                                                                     \
            record->field = value;                                                                \
        }                                                                                         \
    }
BUILDER_SET(Width, int, width)
BUILDER_SET(Height, int, height)
BUILDER_SET(Depth, int, depth)
BUILDER_SET(Levels, int, levels)
BUILDER_SET(Samples, int, samples)
#undef BUILDER_SET

#define BUILDER_GET(name, type, field, none)                                                      \
    type Nvnl_nvnTextureBuilderGet##name(const NVNtextureBuilder *builder) {                      \
        const LayerTextureBuilder *const record = Record(builder, "nvnTextureBuilderGet" #name);  \
        return record != NULL ? (type)record->field : (none);                                     \
    }
BUILDER_GET(Device, NVNdevice *, device, NULL)
BUILDER_GET(Flags, NVNtextureFlags, flags, 0)
BUILDER_GET(Target, NVNtextureTarget, target, 0)
BUILDER_GET(Format, NVNformat, format, NVN_FORMAT_NONE)
BUILDER_GET(Width, int, width, 0)
BUILDER_GET(Height, int, height, 0)
BUILDER_GET(Depth, int, depth, 0)
BUILDER_GET(Levels, int, levels, 0)
BUILDER_GET(Samples, int, samples, 0)
BUILDER_GET(MemoryPool, NVNmemoryPool *, pool, NULL)
BUILDER_GET(MemoryOffset, ptrdiff_t, offset, 0)
#undef BUILDER_GET

/// The deko3d format of an NVN one, or DkImageFormat_None for one the layer has not mapped.
static DkImageFormat FormatOf(NVNformat format) {
    switch (format) {
    case NVN_FORMAT_RGBA8:
        return DkImageFormat_RGBA8_Unorm;
    case NVN_FORMAT_BGRA8:
        return DkImageFormat_BGRA8_Unorm;
    case NVN_FORMAT_RGBA8UI:
        return DkImageFormat_RGBA8_Uint;
    case NVN_FORMAT_RGBA8I:
        return DkImageFormat_RGBA8_Sint;
    case NVN_FORMAT_DEPTH32F:
        return DkImageFormat_ZF32;
    case NVN_FORMAT_DEPTH24_STENCIL8:
        return DkImageFormat_Z24S8;
    default:
        return DkImageFormat_None;
    }
}

static bool FormatIsDepth(NVNformat format) {
    return format == NVN_FORMAT_DEPTH32F || format == NVN_FORMAT_DEPTH24_STENCIL8;
}

/// The layout @p builder describes, or false with a complaint naming what the layer does not
/// take yet: a target, a format or a flag, by its number.
static bool LayoutOf(const LayerTextureBuilder *builder, DkImageLayout *layout, const char *what) {
    LayerDevice *const device = DeviceOf(builder->device, what);
    if (device == NULL) {
        return false;
    }
    if (builder->target != NVN_TEXTURE_TARGET_2D) {
        LayerComplain("%s: texture target %d, and the layer takes 2D (%d) only", what,
                      (int)builder->target, (int)NVN_TEXTURE_TARGET_2D);
        return false;
    }
    const DkImageFormat format = FormatOf(builder->format);
    if (format == DkImageFormat_None) {
        LayerComplain("%s: NVN format 0x%x has no deko3d format in the layer yet", what,
                      (unsigned)builder->format);
        return false;
    }
    const int known = NVN_TEXTURE_FLAGS_DISPLAY | NVN_TEXTURE_FLAGS_COMPRESSIBLE;
    if ((builder->flags & ~known) != 0) {
        LayerComplain("%s: texture flags 0x%x, and the layer takes display and compressible only",
                      what, (unsigned)builder->flags);
        return false;
    }
    if (builder->width <= 0 || builder->height <= 0) {
        LayerComplain("%s: a %dx%d texture", what, builder->width, builder->height);
        return false;
    }
    // The levels a texture of this size can have: one, and one more for each halving of its
    // longer side down to a texel.
    int most = 1;
    for (int side = builder->width > builder->height ? builder->width : builder->height; side > 1;
         side >>= 1) {
        ++most;
    }
    if (builder->levels < 1 || builder->levels > most) {
        LayerComplain("%s: %d levels for a %dx%d texture, which has 1 to %d", what,
                      builder->levels, builder->width, builder->height, most);
        return false;
    }
    if (builder->depth != 1 || builder->samples > 1 || builder->samples < 0) {
        LayerComplain("%s: a depth of %d and %d samples, and the layer takes a depth of 1 and no "
                      "multisampling (0 or 1 samples) yet",
                      what, builder->depth, builder->samples);
        return false;
    }
    DkImageLayoutMaker maker;
    dkImageLayoutMakerDefaults(&maker, DeviceUse(device));
    maker.type = DkImageType_2D;
    maker.format = format;
    // Any NVN texture can be drawn into, and one of a colour format copied by either engine,
    // so every one here says so: the 2D engine is where a mirrored copy goes.
    maker.flags = DkImageFlags_UsageRender;
    if (format == DkImageFormat_RGBA8_Unorm || format == DkImageFormat_BGRA8_Unorm) {
        maker.flags |= DkImageFlags_Usage2DEngine;
    }
    if ((builder->flags & NVN_TEXTURE_FLAGS_DISPLAY) != 0) {
        maker.flags |= DkImageFlags_UsagePresent;
    }
    if ((builder->flags & NVN_TEXTURE_FLAGS_COMPRESSIBLE) != 0) {
        maker.flags |= DkImageFlags_HwCompression;
    }
    maker.dimensions[0] = (uint32_t)builder->width;
    maker.dimensions[1] = (uint32_t)builder->height;
    maker.mipLevels = (uint32_t)builder->levels;
    dkImageLayoutInitialize(layout, &maker);
    return true;
}

size_t Nvnl_nvnTextureBuilderGetStorageSize(const NVNtextureBuilder *builder) {
    const LayerTextureBuilder *const record = Record(builder, "nvnTextureBuilderGetStorageSize");
    DkImageLayout layout;
    if (record == NULL || !LayoutOf(record, &layout, "nvnTextureBuilderGetStorageSize")) {
        return 0;
    }
    return (size_t)dkImageLayoutGetSize(&layout);
}

size_t Nvnl_nvnTextureBuilderGetStorageAlignment(const NVNtextureBuilder *builder) {
    const LayerTextureBuilder *const record =
        Record(builder, "nvnTextureBuilderGetStorageAlignment");
    DkImageLayout layout;
    if (record == NULL || !LayoutOf(record, &layout, "nvnTextureBuilderGetStorageAlignment")) {
        return 0;
    }
    return (size_t)dkImageLayoutGetAlignment(&layout);
}

NVNboolean Nvnl_nvnTextureInitialize(NVNtexture *texture,
                                            const NVNtextureBuilder *builder) {
    LayerTexture *const record = Record(texture, "nvnTextureInitialize");
    const LayerTextureBuilder *const from = Record(builder, "nvnTextureInitialize");
    if (record == NULL || from == NULL) {
        return 0;
    }
    const LayerPool *const pool = PoolOf(from->pool, "nvnTextureInitialize");
    DkImageLayout layout;
    if (pool == NULL || !LayoutOf(from, &layout, "nvnTextureInitialize")) {
        return 0;
    }
    if (from->offset < 0 || (uint64_t)from->offset + dkImageLayoutGetSize(&layout) >
                                dkMemBlockGetSize(pool->block)) {
        LayerComplain("nvnTextureInitialize: 0x%llx bytes at offset 0x%llx, past the pool's 0x%x",
                      (unsigned long long)dkImageLayoutGetSize(&layout),
                      (unsigned long long)from->offset, (unsigned)dkMemBlockGetSize(pool->block));
        return 0;
    }
    memset(texture, 0, sizeof(*texture));
    dkImageInitialize(&record->image, &layout, pool->block, (uint32_t)from->offset);
    record->magic = MAGIC_TEXTURE;
    record->flags = from->flags;
    record->target = from->target;
    record->format = from->format;
    record->width = from->width;
    record->height = from->height;
    record->depth = from->depth;
    record->levels = from->levels;
    record->samples = from->samples;
    record->pool = from->pool;
    record->offset = from->offset;
    return 1;
}

#define TEXTURE_GET(name, type, field, none)                                                      \
    type Nvnl_nvnTextureGet##name(const NVNtexture *texture) {                                    \
        const LayerTexture *const record = TextureOf(texture, "nvnTextureGet" #name);             \
        return record != NULL ? (type)record->field : (none);                                     \
    }
TEXTURE_GET(Flags, NVNtextureFlags, flags, 0)
TEXTURE_GET(Target, NVNtextureTarget, target, 0)
TEXTURE_GET(Format, NVNformat, format, NVN_FORMAT_NONE)
TEXTURE_GET(Width, int, width, 0)
TEXTURE_GET(Height, int, height, 0)
TEXTURE_GET(Depth, int, depth, 0)
TEXTURE_GET(Levels, int, levels, 0)
TEXTURE_GET(Samples, int, samples, 0)
TEXTURE_GET(MemoryPool, NVNmemoryPool *, pool, NULL)
TEXTURE_GET(MemoryOffset, ptrdiff_t, offset, 0)
#undef TEXTURE_GET

void Nvnl_nvnTextureFinalize(NVNtexture *texture) {
    LayerTexture *const record = TextureOf(texture, "nvnTextureFinalize");
    if (record != NULL) {
        record->magic = 0;
    }
}

// ---- texture views

// A view is a few numbers the program owns, with no device behind it: each part is either set
// or left to the texture, and a getter says which with its result.

void Nvnl_nvnTextureViewSetDefaults(NVNtextureView *view) {
    if (Record(view, "nvnTextureViewSetDefaults") != NULL) {
        memset(view, 0, sizeof(*view));
    }
}

void Nvnl_nvnTextureViewSetLevels(NVNtextureView *view, int base, int count) {
    LayerView *const record = Record(view, "nvnTextureViewSetLevels");
    if (record != NULL) {
        record->set |= VIEW_LEVELS;
        record->base_level = base;
        record->levels = count;
    }
}

void Nvnl_nvnTextureViewSetLayers(NVNtextureView *view, int first, int count) {
    LayerView *const record = Record(view, "nvnTextureViewSetLayers");
    if (record != NULL) {
        record->set |= VIEW_LAYERS;
        record->first_layer = first;
        record->layers = count;
    }
}

void Nvnl_nvnTextureViewSetFormat(NVNtextureView *view, NVNformat format) {
    LayerView *const record = Record(view, "nvnTextureViewSetFormat");
    if (record != NULL) {
        record->set |= VIEW_FORMAT;
        record->format = format;
    }
}

void Nvnl_nvnTextureViewSetTarget(NVNtextureView *view, NVNtextureTarget target) {
    LayerView *const record = Record(view, "nvnTextureViewSetTarget");
    if (record != NULL) {
        record->set |= VIEW_TARGET;
        record->target = target;
    }
}

NVNboolean Nvnl_nvnTextureViewGetLevels(const NVNtextureView *view, int *base, int *count) {
    const LayerView *const record = Record(view, "nvnTextureViewGetLevels");
    if (record == NULL) {
        return 0;
    }
    *base = record->base_level;
    *count = record->levels;
    return (record->set & VIEW_LEVELS) != 0;
}

NVNboolean Nvnl_nvnTextureViewGetLayers(const NVNtextureView *view, int *first, int *count) {
    const LayerView *const record = Record(view, "nvnTextureViewGetLayers");
    if (record == NULL) {
        return 0;
    }
    *first = record->first_layer;
    *count = record->layers;
    return (record->set & VIEW_LAYERS) != 0;
}

NVNboolean Nvnl_nvnTextureViewGetFormat(const NVNtextureView *view, NVNformat *format) {
    const LayerView *const record = Record(view, "nvnTextureViewGetFormat");
    if (record == NULL) {
        return 0;
    }
    *format = record->format;
    return (record->set & VIEW_FORMAT) != 0;
}

NVNboolean Nvnl_nvnTextureViewGetTarget(const NVNtextureView *view, NVNtextureTarget *target) {
    const LayerView *const record = Record(view, "nvnTextureViewGetTarget");
    if (record == NULL) {
        return 0;
    }
    *target = record->target;
    return (record->set & VIEW_TARGET) != 0;
}

NVNboolean Nvnl_nvnTextureViewCompare(const NVNtextureView *a, const NVNtextureView *b) {
    const LayerView *const x = Record(a, "nvnTextureViewCompare");
    const LayerView *const y = Record(b, "nvnTextureViewCompare");
    if (x == NULL || y == NULL || x->set != y->set) {
        return 0;
    }
    // The same view: the same parts set, and each part set the same.
    return ((x->set & VIEW_LEVELS) == 0 ||
            (x->base_level == y->base_level && x->levels == y->levels)) &&
           ((x->set & VIEW_LAYERS) == 0 ||
            (x->first_layer == y->first_layer && x->layers == y->layers)) &&
           ((x->set & VIEW_FORMAT) == 0 || x->format == y->format) &&
           ((x->set & VIEW_TARGET) == 0 || x->target == y->target);
}

// ---- command buffers

NVNboolean Nvnl_nvnCommandBufferInitialize(NVNcommandBuffer *cmd, NVNdevice *device) {
    LayerCommandBuffer *const record = Record(cmd, "nvnCommandBufferInitialize");
    LayerDevice *const owner = DeviceOf(device, "nvnCommandBufferInitialize");
    if (record == NULL || owner == NULL) {
        return 0;
    }
    DkCmdBufMaker maker;
    dkCmdBufMakerDefaults(&maker, DeviceUse(owner));
    // Out of command memory, deko3d asks here, and the program's callback adds more; with none
    // set, deko3d ends the program, which the case's journal then names.
    maker.userData = cmd;
    maker.cbAddMem = LayerCommandGrow;
    const DkCmdBuf made = dkCmdBufCreate(&maker);
    if (made == NULL) {
        LayerComplain("nvnCommandBufferInitialize: dkCmdBufCreate gave no command buffer");
        return 0;
    }
    LayerDraw *const draw = calloc(1, sizeof(*draw));
    if (draw == NULL) {
        LayerComplain("nvnCommandBufferInitialize: no %u bytes for the layer's draw state",
                      (unsigned)sizeof(*draw));
        dkCmdBufDestroy(made);
        return 0;
    }
    // A stage's handles start with the words 0 to 7, as a driver's do (ScarletNX's nvn_draw.c
    // has it from two of them): nothing here reads them, and a program that does reads the same.
    for (int stage = 0; stage < LAYER_STAGES; ++stage) {
        for (uint32_t word = 0; word < 8u; ++word) {
            memcpy(draw->handles[stage] + 4u * word, &word, 4);
        }
    }
    memset(cmd, 0, sizeof(*cmd));
    record->magic = MAGIC_CMDBUF;
    record->cmdbuf = made;
    record->draw = draw;
    // NVN starts a command buffer with both of its memory callbacks on.
    draw->memory.command_callback = true;
    draw->memory.control_callback = true;
    return 1;
}

void Nvnl_nvnCommandBufferFinalize(NVNcommandBuffer *cmd) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferFinalize");
    if (record == NULL) {
        return;
    }
    dkCmdBufDestroy(record->cmdbuf);
    free(record->draw);
    record->draw = NULL;
    record->magic = 0;
}

void Nvnl_nvnCommandBufferAddCommandMemory(NVNcommandBuffer *cmd,
                                                  const NVNmemoryPool *pool, ptrdiff_t offset,
                                                  size_t size) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferAddCommandMemory");
    const LayerPool *const from = PoolOf(pool, "nvnCommandBufferAddCommandMemory");
    if (record == NULL || from == NULL) {
        return;
    }
    // The last quarter is the layer's, for the texture handles each draw reads (api55_draw.c): a
    // driver keeps such words in the command memory too, which is what makes them live exactly
    // as long as the list. Whole uniform buffer units, and none of a memory too small for one.
    uint32_t kept = ((uint32_t)size / 4u) & ~(LAYER_HANDLE_BYTES - 1u);
    const uint32_t start =
        ((uint32_t)offset + (uint32_t)size - kept) & ~(DK_UNIFORM_BUF_ALIGNMENT - 1u);
    if (kept < LAYER_HANDLE_BYTES || start < (uint32_t)offset ||
        (from->flags & NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS) != 0) {
        kept = 0;
    }
    LayerDraw *const draw = record->draw;
    draw->handle_size = kept != 0 ? (uint32_t)offset + (uint32_t)size - start : 0;
    draw->handle_used = 0;
    draw->handle_memory = dkMemBlockGetGpuAddr(from->block) + start;
    draw->handle_cpu = (uint8_t *)dkMemBlockGetCpuAddr(from->block) + start;
    const uint32_t given = kept != 0 ? start - (uint32_t)offset : (uint32_t)size;
    draw->memory.added = (uint32_t)size;
    draw->memory.given_size = given;
    draw->memory.given = NULL;
    if ((from->flags & NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS) == 0) {
        uint8_t *const bytes = (uint8_t *)dkMemBlockGetCpuAddr(from->block) + (uint32_t)offset;
        memset(bytes, LAYER_UNUSED_BYTE, given);
        draw->memory.given = bytes;
    }
    dkCmdBufAddMemory(record->cmdbuf, from->block, (uint32_t)offset, given);
}

void Nvnl_nvnCommandBufferAddControlMemory(NVNcommandBuffer *cmd, void *memory,
                                                  size_t size) {
    // deko3d keeps a list's control words in its command memory, so the processor-side memory
    // NVN asks for has no use here; taken and left alone.
    (void)memory;
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferAddControlMemory");
    if (record != NULL) {
        record->draw->memory.control = size;
    }
}

void Nvnl_nvnCommandBufferBeginRecording(NVNcommandBuffer *cmd) {
    // A deko3d list is whatever was recorded since the last one finished, so a recording starts
    // by itself.
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferBeginRecording");
    if (record != NULL) {
        record->draw->memory.recording = true;
    }
}

NVNcommandHandle Nvnl_nvnCommandBufferEndRecording(NVNcommandBuffer *cmd) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferEndRecording");
    if (record == NULL) {
        return 0;
    }
    record->draw->memory.recording = false;
    return (NVNcommandHandle)dkCmdBufFinishList(record->cmdbuf);
}

void Nvnl_nvnCommandBufferSetRenderTargets(NVNcommandBuffer *cmd, int count,
                                                  const NVNtexture *const *colors,
                                                  const NVNtextureView *const *views,
                                                  const NVNtexture *depth,
                                                  const NVNtextureView *depth_view) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferSetRenderTargets");
    if (record == NULL) {
        return;
    }
    if (count < 0 || count > COLOR_TARGETS_MAX || depth_view != NULL) {
        LayerComplain("nvnCommandBufferSetRenderTargets: %d colour targets and %s view on the "
                      "depth target, and the layer takes 0 to %d colour targets and no view",
                      count, depth_view != NULL ? "a" : "no", COLOR_TARGETS_MAX);
        return;
    }
    DkImageView targets[COLOR_TARGETS_MAX];
    DkImageView const *bound[COLOR_TARGETS_MAX];
    for (int i = 0; i < count; ++i) {
        if (views != NULL && views[i] != NULL) {
            LayerComplain("nvnCommandBufferSetRenderTargets: a texture view on target %d, which "
                          "the layer does not take yet",
                          i);
            return;
        }
        const LayerTexture *const texture =
            TextureOf(colors[i], "nvnCommandBufferSetRenderTargets");
        if (texture == NULL) {
            return;
        }
        dkImageViewDefaults(&targets[i], &texture->image);
        bound[i] = &targets[i];
    }
    DkImageView depth_target;
    if (depth != NULL) {
        const LayerTexture *const texture = TextureOf(depth, "nvnCommandBufferSetRenderTargets");
        if (texture == NULL) {
            return;
        }
        dkImageViewDefaults(&depth_target, &texture->image);
    }
    dkCmdBufBindRenderTargets(record->cmdbuf, bound, (uint32_t)count,
                              depth != NULL ? &depth_target : NULL);
    LayerTargets *const kept = &record->draw->targets;
    kept->count = count;
    for (int i = 0; i < count; ++i) {
        kept->colors[i] = colors[i];
    }
    kept->depth = depth;
}

void Nvnl_nvnCommandBufferSetViewport(NVNcommandBuffer *cmd, int x, int y, int width,
                                             int height) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferSetViewport");
    if (record == NULL) {
        return;
    }
    // NVN sets the depth range apart (nvnCommandBufferSetDepthRange), 0 to 1 by default; deko3d
    // takes it with the rectangle.
    const DkViewport viewport = { (float)x, (float)y, (float)width, (float)height, 0.0f, 1.0f };
    dkCmdBufSetViewports(record->cmdbuf, 0, &viewport, 1);
}

void Nvnl_nvnCommandBufferSetScissor(NVNcommandBuffer *cmd, int x, int y, int width,
                                            int height) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferSetScissor");
    if (record == NULL) {
        return;
    }
    const DkScissor scissor = { (uint32_t)x, (uint32_t)y, (uint32_t)width, (uint32_t)height };
    dkCmdBufSetScissors(record->cmdbuf, 0, &scissor, 1);
    record->draw->targets.scissor = scissor;
    record->draw->targets.scissor_set = true;
}

void Nvnl_nvnCommandBufferClearColor(NVNcommandBuffer *cmd, int index, const float *rgba,
                                            int mask) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferClearColor");
    if (record == NULL) {
        return;
    }
    // NVN's channel mask bits (R 1, G 2, B 4, A 8) are deko3d's.
    dkCmdBufClearColorFloat(record->cmdbuf, (uint32_t)index, (uint32_t)mask & DkColorMask_RGBA,
                            rgba[0], rgba[1], rgba[2], rgba[3]);
}

void Nvnl_nvnCommandBufferBarrier(NVNcommandBuffer *cmd, int bits) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferBarrier");
    if (record == NULL) {
        return;
    }
    // Every order NVN can ask for becomes deko3d's full barrier, a wait for all the work before
    // it: what a case reads back is then the ordered answer whichever order it asked for. The
    // invalidations map one to one.
    const int order = NVN_BARRIER_ORDER_PRIMITIVES_BIT | NVN_BARRIER_ORDER_FRAGMENTS_BIT |
                      NVN_BARRIER_ORDER_FRAGMENTS_TILED_BIT | NVN_BARRIER_ORDER_INDIRECT_DATA_BIT;
    uint32_t invalidate = 0;
    if ((bits & NVN_BARRIER_INVALIDATE_TEXTURE_BIT) != 0) {
        invalidate |= DkInvalidateFlags_Image;
    }
    if ((bits & NVN_BARRIER_INVALIDATE_SHADER_BIT) != 0) {
        invalidate |= DkInvalidateFlags_Shader;
    }
    if ((bits & NVN_BARRIER_INVALIDATE_TEXTURE_DESCRIPTOR_BIT) != 0) {
        invalidate |= DkInvalidateFlags_Descriptors;
    }
    if ((bits & NVN_BARRIER_INVALIDATE_ZCULL_BIT) != 0) {
        invalidate |= DkInvalidateFlags_Zcull;
    }
    dkCmdBufBarrier(record->cmdbuf, (bits & order) != 0 ? DkBarrier_Full : DkBarrier_None,
                    invalidate);
}

/// deko3d's rectangle for an NVN copy region.
static DkImageRect RectOf(const NVNcopyRegion *region) {
    const DkImageRect rect = { (uint32_t)region->xoffset, (uint32_t)region->yoffset,
                               (uint32_t)region->zoffset, (uint32_t)region->width,
                               (uint32_t)region->height, (uint32_t)region->depth };
    return rect;
}

/// deko3d's view of @p texture as @p view sees it in a copy, a NULL one being the whole texture:
/// false with a complaint for a part the layer does not take there yet. Of a view's parts a
/// copy here takes its levels, and copies the first of them.
static bool CopyView(const char *what, const LayerTexture *texture, const NVNtextureView *view,
                     DkImageView *out) {
    dkImageViewDefaults(out, &texture->image);
    if (view == NULL) {
        return true;
    }
    const LayerView *const record = Record(view, what);
    if (record == NULL) {
        return false;
    }
    if ((record->set & ~VIEW_LEVELS) != 0) {
        LayerComplain("%s: a view with parts 0x%x set, and of a view the layer takes its levels "
                      "(0x%x) in a copy",
                      what, (unsigned)record->set, (unsigned)VIEW_LEVELS);
        return false;
    }
    if ((record->set & VIEW_LEVELS) != 0) {
        if (record->base_level < 0 || record->levels < 1 ||
            record->base_level + record->levels > texture->levels) {
            LayerComplain("%s: a view of %d levels from level %d, of a texture that has %d", what,
                          record->levels, record->base_level, texture->levels);
            return false;
        }
        out->mipLevelOffset = (uint8_t)record->base_level;
        out->mipLevelCount = (uint8_t)record->levels;
    }
    return true;
}

/// deko3d's flags for NVN's copy @p flags, of which @p mirrors are the mirror bits the copy can
/// take: false with a complaint for a flag the layer does not take there yet.
static bool CopyFlags(const char *what, int flags, int mirrors, uint32_t *blit) {
    *blit = 0;
    if ((flags & ~mirrors) != 0) {
        LayerComplain("%s: copy flags 0x%x, and of the flags the layer takes only 0x%x here", what,
                      (unsigned)flags, (unsigned)mirrors);
        return false;
    }
    if ((flags & NVN_COPY_FLAGS_MIRROR_X_BIT) != 0) {
        *blit |= DkBlitFlag_FlipX;
    }
    if ((flags & NVN_COPY_FLAGS_MIRROR_Y_BIT) != 0) {
        *blit |= DkBlitFlag_FlipY;
    }
    return true;
}

void Nvnl_nvnCommandBufferCopyTextureToBuffer(NVNcommandBuffer *cmd,
                                                     const NVNtexture *texture,
                                                     const NVNtextureView *view,
                                                     const NVNcopyRegion *region,
                                                     NVNbufferAddress buffer, int flags) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferCopyTextureToBuffer");
    const LayerTexture *const source = TextureOf(texture, "nvnCommandBufferCopyTextureToBuffer");
    if (record == NULL || source == NULL) {
        return;
    }
    // deko3d mirrors a copy to a buffer in y alone (the copy engine stepping backwards); x is
    // the 2D engine's, between images.
    uint32_t blit = 0;
    DkImageView from;
    if (!CopyFlags("nvnCommandBufferCopyTextureToBuffer", flags, NVN_COPY_FLAGS_MIRROR_Y_BIT,
                   &blit) ||
        !CopyView("nvnCommandBufferCopyTextureToBuffer", source, view, &from)) {
        return;
    }
    const DkImageRect rect = RectOf(region);
    const DkCopyBuf to = { buffer, record->copy_row_stride, 0 };
    dkCmdBufCopyImageToBuffer(record->cmdbuf, &from, &rect, &to, blit);
}

void Nvnl_nvnCommandBufferCopyBufferToTexture(NVNcommandBuffer *cmd, NVNbufferAddress buffer,
                                                     const NVNtexture *texture,
                                                     const NVNtextureView *view,
                                                     const NVNcopyRegion *region, int flags) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferCopyBufferToTexture");
    const LayerTexture *const target = TextureOf(texture, "nvnCommandBufferCopyBufferToTexture");
    uint32_t blit = 0;
    DkImageView to;
    if (record == NULL || target == NULL ||
        !CopyFlags("nvnCommandBufferCopyBufferToTexture", flags,
                   NVN_COPY_FLAGS_MIRROR_X_BIT | NVN_COPY_FLAGS_MIRROR_Y_BIT, &blit) ||
        !CopyView("nvnCommandBufferCopyBufferToTexture", target, view, &to)) {
        return;
    }
    const DkImageRect rect = RectOf(region);
    const DkCopyBuf from = { buffer, record->copy_row_stride, 0 };
    dkCmdBufCopyBufferToImage(record->cmdbuf, &from, &to, &rect, blit);
}

void Nvnl_nvnCommandBufferCopyTextureToTexture(
    NVNcommandBuffer *cmd, const NVNtexture *source, const NVNtextureView *source_view,
    const NVNcopyRegion *source_region, const NVNtexture *destination,
    const NVNtextureView *destination_view, const NVNcopyRegion *destination_region, int flags) {
    static const char *const WHAT = "nvnCommandBufferCopyTextureToTexture";
    LayerCommandBuffer *const record = CmdOf(cmd, WHAT);
    const LayerTexture *const from = TextureOf(source, WHAT);
    const LayerTexture *const to = TextureOf(destination, WHAT);
    uint32_t blit = 0;
    DkImageView src, dst;
    if (record == NULL || from == NULL || to == NULL ||
        !CopyFlags(WHAT, flags, NVN_COPY_FLAGS_MIRROR_X_BIT | NVN_COPY_FLAGS_MIRROR_Y_BIT,
                   &blit) ||
        !CopyView(WHAT, from, source_view, &src) || !CopyView(WHAT, to, destination_view, &dst)) {
        return;
    }
    if (source_region->width != destination_region->width ||
        source_region->height != destination_region->height ||
        source_region->depth != destination_region->depth) {
        LayerComplain("%s: a %dx%dx%d region into a %dx%dx%d one, and the layer copies without "
                      "scaling only",
                      WHAT, source_region->width, source_region->height, source_region->depth,
                      destination_region->width, destination_region->height,
                      destination_region->depth);
        return;
    }
    const DkImageRect src_rect = RectOf(source_region);
    const DkImageRect dst_rect = RectOf(destination_region);
    if (blit == 0) {
        dkCmdBufCopyImage(record->cmdbuf, &src, &src_rect, &dst, &dst_rect, 0);
        return;
    }
    // deko3d's copy between images takes no mirror and drops the flag: a mirrored one is its
    // blit, the 2D engine's, which the depth formats cannot go through.
    if (FormatIsDepth(from->format) || FormatIsDepth(to->format)) {
        LayerComplain("%s: copy flags 0x%x between textures of formats 0x%x and 0x%x, and the "
                      "layer mirrors colour formats only",
                      WHAT, (unsigned)flags, (unsigned)from->format, (unsigned)to->format);
        return;
    }
    dkCmdBufBlitImage(record->cmdbuf, &src, &src_rect, &dst, &dst_rect, blit, 0);
}

void Nvnl_nvnCommandBufferCopyBufferToBuffer(NVNcommandBuffer *cmd, NVNbufferAddress source,
                                                    NVNbufferAddress destination, size_t size,
                                                    int flags) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferCopyBufferToBuffer");
    if (record == NULL) {
        return;
    }
    if (flags != NVN_COPY_FLAGS_NONE || size > UINT32_MAX) {
        LayerComplain("nvnCommandBufferCopyBufferToBuffer: copy flags 0x%x over 0x%llx bytes, and "
                      "the layer takes no flags and at most 4 GiB",
                      (unsigned)flags, (unsigned long long)size);
        return;
    }
    dkCmdBufCopyBuffer(record->cmdbuf, source, destination, (uint32_t)size);
}

void Nvnl_nvnCommandBufferSetCopyRowStride(NVNcommandBuffer *cmd, ptrdiff_t stride) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferSetCopyRowStride");
    if (record == NULL) {
        return;
    }
    if (stride < 0 || stride > (ptrdiff_t)UINT32_MAX) {
        LayerComplain("nvnCommandBufferSetCopyRowStride: a stride of %lld, and the layer takes "
                      "nought to 4 GiB",
                      (long long)stride);
        return;
    }
    record->copy_row_stride = (uint32_t)stride;
}

// ---- windows

void Nvnl_nvnWindowBuilderSetDefaults(NVNwindowBuilder *builder) {
    memset(builder, 0, sizeof(*builder));
    LayerWindowBuilder *const record = Record(builder, "nvnWindowBuilderSetDefaults");
    if (record != NULL) {
        // NVN's default: a present every refresh.
        record->interval = 1;
    }
}

void Nvnl_nvnWindowBuilderSetDevice(NVNwindowBuilder *builder, NVNdevice *device) {
    LayerWindowBuilder *const record = Record(builder, "nvnWindowBuilderSetDevice");
    if (record != NULL) {
        record->device = device;
    }
}

void Nvnl_nvnWindowBuilderSetNativeWindow(NVNwindowBuilder *builder,
                                                 NVNnativeWindow native) {
    LayerWindowBuilder *const record = Record(builder, "nvnWindowBuilderSetNativeWindow");
    if (record != NULL) {
        record->native = native;
    }
}

void Nvnl_nvnWindowBuilderSetTextures(NVNwindowBuilder *builder, int count,
                                             NVNtexture *const *textures) {
    LayerWindowBuilder *const record = Record(builder, "nvnWindowBuilderSetTextures");
    if (record == NULL) {
        return;
    }
    if (count < 0 || count > WINDOW_TEXTURES_MAX || (count > 0 && textures == NULL)) {
        LayerComplain("nvnWindowBuilderSetTextures: %d textures%s, where a window takes up to %d",
                      count, textures == NULL ? " and no list" : "", WINDOW_TEXTURES_MAX);
        return;
    }
    memset(record->textures, 0, sizeof(record->textures));
    record->count = (int16_t)count;
    for (int i = 0; i < count; ++i) {
        record->textures[i] = textures[i];
    }
}

void Nvnl_nvnWindowBuilderSetPresentInterval(NVNwindowBuilder *builder, int interval) {
    LayerWindowBuilder *const record = Record(builder, "nvnWindowBuilderSetPresentInterval");
    if (record != NULL) {
        record->interval = interval;
    }
}

void Nvnl_nvnWindowBuilderSetNumActiveTextures(NVNwindowBuilder *builder, int count) {
    LayerWindowBuilder *const record = Record(builder, "nvnWindowBuilderSetNumActiveTextures");
    if (record == NULL) {
        return;
    }
    if (count < 0 || count > WINDOW_TEXTURES_MAX) {
        LayerComplain("nvnWindowBuilderSetNumActiveTextures: %d active textures, where a window "
                      "has up to %d",
                      count, WINDOW_TEXTURES_MAX);
        return;
    }
    record->active = (int16_t)count;
}

NVNdevice *Nvnl_nvnWindowBuilderGetDevice(const NVNwindowBuilder *builder) {
    const LayerWindowBuilder *const record = Record(builder, "nvnWindowBuilderGetDevice");
    return record != NULL ? record->device : NULL;
}

NVNnativeWindow Nvnl_nvnWindowBuilderGetNativeWindow(const NVNwindowBuilder *builder) {
    const LayerWindowBuilder *const record = Record(builder, "nvnWindowBuilderGetNativeWindow");
    return record != NULL ? record->native : NULL;
}

int Nvnl_nvnWindowBuilderGetNumTextures(const NVNwindowBuilder *builder) {
    const LayerWindowBuilder *const record = Record(builder, "nvnWindowBuilderGetNumTextures");
    return record != NULL ? record->count : 0;
}

NVNtexture *Nvnl_nvnWindowBuilderGetTexture(const NVNwindowBuilder *builder, int index) {
    const LayerWindowBuilder *const record = Record(builder, "nvnWindowBuilderGetTexture");
    if (record == NULL) {
        return NULL;
    }
    if (index < 0 || index >= record->count) {
        LayerComplain("nvnWindowBuilderGetTexture: texture %d of a builder that has %d", index,
                      record->count);
        return NULL;
    }
    return record->textures[index];
}

int Nvnl_nvnWindowBuilderGetPresentInterval(const NVNwindowBuilder *builder) {
    const LayerWindowBuilder *const record =
        Record(builder, "nvnWindowBuilderGetPresentInterval");
    return record != NULL ? record->interval : 0;
}

int Nvnl_nvnWindowBuilderGetNumActiveTextures(const NVNwindowBuilder *builder) {
    const LayerWindowBuilder *const record =
        Record(builder, "nvnWindowBuilderGetNumActiveTextures");
    return record != NULL ? record->active : 0;
}

/// Gives deko3d @p interval for @p record's swapchain, and keeps it; false with a complaint
/// naming @p what for one deko3d has no word for, or when deko3d aborts.
static bool WindowInterval(LayerWindow *record, int interval, const char *what) {
    if (interval < 0) {
        LayerComplain("%s: a present interval of %d, where the least is 0", what, interval);
        return false;
    }
    pending.acquire_swapchain = record->swapchain;
    pending.interval = (uint32_t)interval;
    if (!LayerGuarded(SetInterval, "dkSwapchainSetSwapInterval")) {
        return false;
    }
    record->interval = interval;
    return true;
}

NVNboolean Nvnl_nvnWindowInitialize(NVNwindow *window, const NVNwindowBuilder *builder) {
    LayerWindow *const record = Record(window, "nvnWindowInitialize");
    const LayerWindowBuilder *const from = Record(builder, "nvnWindowInitialize");
    if (record == NULL || from == NULL) {
        return 0;
    }
    LayerDevice *const device = DeviceOf(from->device, "nvnWindowInitialize");
    if (device == NULL) {
        return 0;
    }
    if (from->count < 2 || from->count > WINDOW_TEXTURES_MAX || from->native == NULL) {
        LayerComplain("nvnWindowInitialize: %d textures and %s native window, where NVN takes 2 to "
                      "%d and one",
                      from->count, from->native != NULL ? "a" : "no", WINDOW_TEXTURES_MAX);
        return 0;
    }
    DkImage const *images[WINDOW_TEXTURES_MAX];
    for (int i = 0; i < from->count; ++i) {
        const LayerTexture *const texture = TextureOf(from->textures[i], "nvnWindowInitialize");
        if (texture == NULL) {
            return 0;
        }
        images[i] = &texture->image;
    }
    dkSwapchainMakerDefaults(&pending.swapchain_maker, DeviceUse(device), from->native, images,
                             (uint32_t)from->count);
    pending.swapchain = NULL;
    if (!LayerGuarded(MakeSwapchain, "nvnWindowInitialize: dkSwapchainCreate")) {
        return 0;
    }
    memset(window, 0, sizeof(*window));
    record->magic = MAGIC_WINDOW;
    record->swapchain = pending.swapchain;
    record->native = from->native;
    record->count = from->count;
    record->active = from->active;
    // deko3d's swapchain starts at NVN's default too.
    record->interval = 1;
    // No crop set: NVN shows the whole texture.
    const LayerTexture *const first = TextureOf(from->textures[0], "nvnWindowInitialize");
    record->crop[2] = first->width;
    record->crop[3] = first->height;
    if (from->interval != 1 && !WindowInterval(record, from->interval, "nvnWindowInitialize")) {
        dkSwapchainDestroy(record->swapchain);
        record->magic = 0;
        return 0;
    }
    return 1;
}

NVNnativeWindow Nvnl_nvnWindowGetNativeWindow(const NVNwindow *window) {
    const LayerWindow *const record = WindowOf(window, "nvnWindowGetNativeWindow");
    return record != NULL ? record->native : NULL;
}

int Nvnl_nvnWindowGetNumTextures(const NVNwindow *window) {
    const LayerWindow *const record = WindowOf(window, "nvnWindowGetNumTextures");
    return record != NULL ? record->count : 0;
}

int Nvnl_nvnWindowGetNumActiveTextures(const NVNwindow *window) {
    const LayerWindow *const record = WindowOf(window, "nvnWindowGetNumActiveTextures");
    return record != NULL ? WindowActive(record) : 0;
}

void Nvnl_nvnWindowSetNumActiveTextures(NVNwindow *window, int count) {
    LayerWindow *const record = WindowOf(window, "nvnWindowSetNumActiveTextures");
    if (record == NULL) {
        return;
    }
    if (count < 0 || count > WINDOW_TEXTURES_MAX) {
        LayerComplain("nvnWindowSetNumActiveTextures: %d active textures, where a window has up "
                      "to %d",
                      count, WINDOW_TEXTURES_MAX);
        return;
    }
    record->active = (int16_t)count;
}

int Nvnl_nvnWindowGetPresentInterval(const NVNwindow *window) {
    const LayerWindow *const record = WindowOf(window, "nvnWindowGetPresentInterval");
    return record != NULL ? record->interval : 0;
}

void Nvnl_nvnWindowSetPresentInterval(NVNwindow *window, int interval) {
    LayerWindow *const record = WindowOf(window, "nvnWindowSetPresentInterval");
    if (record != NULL) {
        WindowInterval(record, interval, "nvnWindowSetPresentInterval");
    }
}

void Nvnl_nvnWindowGetCrop(const NVNwindow *window, NVNrectangle *crop) {
    const LayerWindow *const record = WindowOf(window, "nvnWindowGetCrop");
    if (record != NULL) {
        crop->x = record->crop[0];
        crop->y = record->crop[1];
        crop->width = record->crop[2];
        crop->height = record->crop[3];
    }
}

void Nvnl_nvnWindowSetCrop(NVNwindow *window, int x, int y, int width, int height) {
    LayerWindow *const record = WindowOf(window, "nvnWindowSetCrop");
    if (record == NULL) {
        return;
    }
    if (x < 0 || y < 0 || width <= 0 || height <= 0) {
        LayerComplain("nvnWindowSetCrop: %d,%d %dx%d is not a rectangle of a texture", x, y,
                      width, height);
        return;
    }
    pending.acquire_swapchain = record->swapchain;
    pending.crop[0] = x;
    pending.crop[1] = y;
    pending.crop[2] = width;
    pending.crop[3] = height;
    if (LayerGuarded(SetCrop, "nvnWindowSetCrop: dkSwapchainSetCrop")) {
        memcpy(record->crop, pending.crop, sizeof(record->crop));
    }
}

void Nvnl_nvnWindowFinalize(NVNwindow *window) {
    LayerWindow *const record = WindowOf(window, "nvnWindowFinalize");
    if (record == NULL) {
        return;
    }
    dkSwapchainDestroy(record->swapchain);
    record->magic = 0;
}

// ---- the build's table

// The names the lookup answers, built from the list in nvnlayer_procs.h, as api55.h declares
// their bodies from it: a name of the list with no body does not link.
const LayerEntry LAYER_ENTRIES[] = {
#define NVN_LAYER_ENTRY(result, name, parameters) { #name, (NvnGenericProc)Nvnl_##name },
    NVN_LAYER_PROCS(NVN_LAYER_ENTRY)
#undef NVN_LAYER_ENTRY
};
const size_t LAYER_ENTRY_COUNT = sizeof(LAYER_ENTRIES) / sizeof(LAYER_ENTRIES[0]);

void LayerStop(const char *text) {
    // No body of this build stops the program; the shared part still links against the name.
    svcOutputDebugString(text, strlen(text));
    for (;;) {
        svcBreak(BreakReason_Panic, 0, 0);
    }
}
