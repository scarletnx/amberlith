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
    int acquired;
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
    // deko3d's defaults: a graphics and compute queue, as every NVN queue is.
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

NVNqueueAcquireTextureResult Nvnl_nvnQueueAcquireTexture(NVNqueue *queue,
                                                                NVNwindow *window, int *index) {
    LayerQueue *const record = QueueOf(queue, "nvnQueueAcquireTexture");
    LayerWindow *const target = WindowOf(window, "nvnQueueAcquireTexture");
    if (record == NULL || target == NULL) {
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

void Nvnl_nvnMemoryPoolFlushMappedRange(const NVNmemoryPool *pool, ptrdiff_t offset,
                                               size_t size) {
    LayerPool *const record = PoolOf(pool, "nvnMemoryPoolFlushMappedRange");
    if (record != NULL) {
        dkMemBlockFlushCpuCache(record->block, (uint32_t)offset, (uint32_t)size);
    }
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

/// The deko3d format of an NVN one, or DkImageFormat_None for one the layer has not mapped.
static DkImageFormat FormatOf(NVNformat format) {
    switch (format) {
    case NVN_FORMAT_RGBA8:
        return DkImageFormat_RGBA8_Unorm;
    case NVN_FORMAT_BGRA8:
        return DkImageFormat_BGRA8_Unorm;
    case NVN_FORMAT_DEPTH32F:
        return DkImageFormat_ZF32;
    case NVN_FORMAT_DEPTH24_STENCIL8:
        return DkImageFormat_Z24S8;
    default:
        return DkImageFormat_None;
    }
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
    DkImageLayoutMaker maker;
    dkImageLayoutMakerDefaults(&maker, DeviceUse(device));
    maker.type = DkImageType_2D;
    maker.format = format;
    // Any NVN texture can be drawn into, and one of a colour format copied by either engine,
    // so every one here says so: the 2D engine is where a mirrored copy goes.
    maker.flags = DkImageFlags_UsageRender;
    if (format != DkImageFormat_ZF32 && format != DkImageFormat_Z24S8) {
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
    return 1;
}

void Nvnl_nvnTextureFinalize(NVNtexture *texture) {
    LayerTexture *const record = TextureOf(texture, "nvnTextureFinalize");
    if (record != NULL) {
        record->magic = 0;
    }
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
    // No memory callback: a case gives a command buffer the memory it needs up front, and
    // deko3d ends the program when a list runs out, which the case's journal then names.
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
    dkCmdBufAddMemory(record->cmdbuf, from->block, (uint32_t)offset,
                      kept != 0 ? start - (uint32_t)offset : (uint32_t)size);
}

void Nvnl_nvnCommandBufferAddControlMemory(NVNcommandBuffer *cmd, void *memory,
                                                  size_t size) {
    // deko3d keeps a list's control words in its command memory, so the processor-side memory
    // NVN asks for has no use here; taken and left alone.
    (void)memory;
    (void)size;
    (void)CmdOf(cmd, "nvnCommandBufferAddControlMemory");
}

void Nvnl_nvnCommandBufferBeginRecording(NVNcommandBuffer *cmd) {
    // A deko3d list is whatever was recorded since the last one finished, so a recording starts
    // by itself.
    (void)CmdOf(cmd, "nvnCommandBufferBeginRecording");
}

NVNcommandHandle Nvnl_nvnCommandBufferEndRecording(NVNcommandBuffer *cmd) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferEndRecording");
    return record != NULL ? (NVNcommandHandle)dkCmdBufFinishList(record->cmdbuf) : 0;
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

/// deko3d's flags for NVN's copy @p flags, of which @p mirrors are the mirror bits the copy can
/// take: false with a complaint for a view, or a flag the layer does not take there yet.
static bool CopyFlags(const char *what, const NVNtextureView *view, int flags, int mirrors,
                      uint32_t *blit) {
    *blit = 0;
    if (view != NULL || (flags & ~mirrors) != 0) {
        LayerComplain("%s: %s view and copy flags 0x%x, and the layer takes no view and of the "
                      "flags only 0x%x here",
                      what, view != NULL ? "a" : "no", (unsigned)flags, (unsigned)mirrors);
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
    if (!CopyFlags("nvnCommandBufferCopyTextureToBuffer", view, flags, NVN_COPY_FLAGS_MIRROR_Y_BIT,
                   &blit)) {
        return;
    }
    DkImageView from;
    dkImageViewDefaults(&from, &source->image);
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
    if (record == NULL || target == NULL ||
        !CopyFlags("nvnCommandBufferCopyBufferToTexture", view, flags,
                   NVN_COPY_FLAGS_MIRROR_X_BIT | NVN_COPY_FLAGS_MIRROR_Y_BIT, &blit)) {
        return;
    }
    DkImageView to;
    dkImageViewDefaults(&to, &target->image);
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
    if (record == NULL || from == NULL || to == NULL ||
        !CopyFlags(WHAT, source_view != NULL ? source_view : destination_view, flags,
                   NVN_COPY_FLAGS_MIRROR_X_BIT | NVN_COPY_FLAGS_MIRROR_Y_BIT, &blit)) {
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
    DkImageView src, dst;
    dkImageViewDefaults(&src, &from->image);
    dkImageViewDefaults(&dst, &to->image);
    const DkImageRect src_rect = RectOf(source_region);
    const DkImageRect dst_rect = RectOf(destination_region);
    dkCmdBufCopyImage(record->cmdbuf, &src, &src_rect, &dst, &dst_rect, blit);
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
    if (record != NULL) {
        record->count = count;
        record->textures = textures;
    }
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
    if (from->count < 2 || from->count > WINDOW_TEXTURES_MAX || from->textures == NULL ||
        from->native == NULL) {
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
    return 1;
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
