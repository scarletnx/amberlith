// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>
#include <string.h>

#include <deko3d.h>
#include <switch.h>

#include "api55_control.h"
#include "layer.h"

// What the API 55 build's files share: the record the layer keeps in each NVN object, and the
// bodies of the list, which api55_deko3d.c (the objects, the copies, the window) and
// api55_draw.c (programs, state and draws) define between them.

#define NVN_LAYER_DECLARE(result, name, parameters) result Nvnl_##name parameters;
NVN_LAYER_PROCS(NVN_LAYER_DECLARE)
#undef NVN_LAYER_DECLARE

// ---- what the layer keeps in each NVN object

#define MAGIC_DEVICE 0x4e4c4456u  // "VDLN"
#define MAGIC_QUEUE 0x4e4c5155u   // "UQLN"
#define MAGIC_POOL 0x4e4c504fu    // "OPLN"
#define MAGIC_TEXTURE 0x4e4c5458u // "XTLN"
#define MAGIC_CMDBUF 0x4e4c4342u  // "BCLN"
#define MAGIC_WINDOW 0x4e4c5749u  // "IWLN"
#define MAGIC_PROGRAM 0x4e4c5250u // "PRLN"
#define MAGIC_SAMPLER 0x4e4c4d53u // "SMLN"
#define MAGIC_TEXPOOL 0x4e4c5054u // "TPLN"
#define MAGIC_SMPPOOL 0x4e4c5053u // "SPLN"

// NVN's windows take two to four textures (nvnDeviceGetInteger's MIN and MAX_TEXTURES_PER_WINDOW).
#define WINDOW_TEXTURES_MAX 4
// deko3d's limit on colour targets, which is NVN's too.
#define COLOR_TARGETS_MAX 8

typedef struct {
    uint32_t magic;
    DkDevice device;
    /// NVN's two device modes (NVNwindowOriginMode, NVNdepthMode), which deko3d takes only as it
    /// makes a device: a setter makes deko3d's device again, until the first object made from it.
    uint8_t origin;
    uint8_t depth;
    bool used;
} LayerDevice;

typedef struct {
    int flags;
} LayerDeviceBuilder;

typedef struct {
    NVNdevice *device;
} LayerQueueBuilder;

typedef struct {
    uint32_t magic;
    DkQueue queue;
} LayerQueue;

typedef struct {
    NVNdevice *device;
    void *storage;
    size_t size;
    int flags;
} LayerPoolBuilder;

typedef struct {
    uint32_t magic;
    DkMemBlock block;
    int flags;
} LayerPool;

typedef struct {
    NVNdevice *device;
    int flags;
    NVNtextureTarget target;
    NVNformat format;
    int width;
    int height;
    NVNmemoryPool *pool;
    ptrdiff_t offset;
} LayerTextureBuilder;

typedef struct {
    DkImage image; // first: deko3d wants it at its own alignment, which the object's start has
    uint32_t magic;
} LayerTexture;

// The graphics stages, NVN's numbers: vertex, fragment, geometry and the two of tessellation.
#define LAYER_STAGES 5
// A stage's texture handles as NVN's programs read them: one constant buffer, the handle of
// texture slot r in the word at 0x20 + 8 * r (ScarletNX's docs/layers/gpu/nvn.md, "Constant
// buffer banks"), 32 slots, which with its first 0x20 bytes rounds up to two of deko3d's
// uniform buffer units.
#define LAYER_HANDLE_SLOTS 32
#define LAYER_HANDLE_FIRST 0x20u
#define LAYER_HANDLE_BYTES 0x200u

/// What a command buffer keeps for its draws, too large for the NVN object: allocated with it.
typedef struct {
    /// The part of the command memory the layer keeps to itself, where each draw's copy of the
    /// texture handles goes: memory the program gave for the list and keeps alive for as long as
    /// the list can run, as a driver's own words in command memory are.
    DkGpuAddr handle_memory;
    uint8_t *handle_cpu;
    uint32_t handle_size;
    uint32_t handle_used;
    /// The handles nvnCommandBufferBindTexture set, by stage, and the stages changed since the
    /// last draw took a copy.
    uint8_t handles[LAYER_STAGES][LAYER_HANDLE_BYTES];
    uint8_t dirty;
} LayerDraw;

typedef struct {
    uint32_t magic;
    DkCmdBuf cmdbuf;
    /// nvnCommandBufferSetCopyRowStride's, nought for rows packed tight, NVN's default.
    uint32_t copy_row_stride;
    LayerDraw *draw;
} LayerCommandBuffer;

typedef struct {
    uint32_t magic;
    /// The stages' shaders, by NVN stage: each inside the control section the program keeps
    /// (NvnLayerShaderControl), NULL for a stage the program has none of.
    const DkShader *shaders[LAYER_STAGES];
} LayerProgram;

typedef struct {
    NVNdevice *device;
    uint8_t min;
    uint8_t mag;
    uint8_t wrap[3];
} LayerSamplerBuilder;

typedef struct {
    uint32_t magic;
    uint8_t min;
    uint8_t mag;
    uint8_t wrap[3];
} LayerSampler;

/// A texture pool or a sampler pool: @p count descriptors of 32 bytes in a memory pool.
typedef struct {
    uint32_t magic;
    int count;
    DkMemBlock block;
    uint32_t offset;
} LayerDescriptorPool;

// The state objects are a few bytes each in NVN, so their records are bit fields with no magic:
// a state a program never gave defaults reads as whatever its storage held, as in a driver.
typedef struct {
    uint32_t set : 1;
    uint32_t stream : 5;
    uint32_t offset : 14;
    uint32_t format : 10;
} LayerVertexAttrib;

typedef struct {
    uint32_t stride;
    uint32_t divisor;
} LayerVertexStream;

typedef struct {
    uint32_t target;
} LayerBlend;

typedef struct {
    uint32_t masks;
} LayerChannelMask;

typedef struct {
    uint32_t blend_enable : 8;
} LayerColor;

typedef struct {
    uint32_t depth_test : 1;
    uint32_t depth_write : 1;
    uint32_t depth_func : 4;
} LayerDepthStencil;

typedef struct {
    uint32_t samples;
} LayerMultisample;

typedef struct {
    uint32_t cull : 2;
    uint32_t front : 1;
} LayerPolygon;

typedef struct {
    NVNdevice *device;
    NVNnativeWindow native;
    int count;
    NVNtexture *const *textures;
} LayerWindowBuilder;

typedef struct {
    uint32_t magic;
    DkSwapchain swapchain;
} LayerWindow;

_Static_assert(sizeof(LayerDevice) <= sizeof(NVNdevice), "the device record fits NVNdevice");
_Static_assert(sizeof(LayerDeviceBuilder) <= sizeof(NVNdeviceBuilder), "fits NVNdeviceBuilder");
_Static_assert(sizeof(LayerQueueBuilder) <= sizeof(NVNqueueBuilder), "fits NVNqueueBuilder");
_Static_assert(sizeof(LayerQueue) <= sizeof(NVNqueue), "fits NVNqueue");
_Static_assert(sizeof(LayerPoolBuilder) <= sizeof(NVNmemoryPoolBuilder), "fits the builder");
_Static_assert(sizeof(LayerPool) <= sizeof(NVNmemoryPool), "fits NVNmemoryPool");
_Static_assert(sizeof(LayerTextureBuilder) <= sizeof(NVNtextureBuilder), "fits the builder");
_Static_assert(sizeof(LayerTexture) <= sizeof(NVNtexture), "fits NVNtexture");
_Static_assert(sizeof(LayerCommandBuffer) <= sizeof(NVNcommandBuffer), "fits NVNcommandBuffer");
_Static_assert(sizeof(LayerWindowBuilder) <= sizeof(NVNwindowBuilder), "fits the builder");
_Static_assert(sizeof(LayerWindow) <= sizeof(NVNwindow), "fits NVNwindow");
_Static_assert(sizeof(LayerProgram) <= sizeof(NVNprogram), "fits NVNprogram");
_Static_assert(sizeof(LayerSamplerBuilder) <= sizeof(NVNsamplerBuilder), "fits the builder");
_Static_assert(sizeof(LayerSampler) <= sizeof(NVNsampler), "fits NVNsampler");
_Static_assert(sizeof(LayerDescriptorPool) <= sizeof(NVNtexturePool), "fits NVNtexturePool");
_Static_assert(sizeof(LayerDescriptorPool) <= sizeof(NVNsamplerPool), "fits NVNsamplerPool");
_Static_assert(sizeof(LayerVertexAttrib) <= sizeof(NVNvertexAttribState), "fits the state");
_Static_assert(sizeof(LayerVertexStream) <= sizeof(NVNvertexStreamState), "fits the state");
_Static_assert(sizeof(LayerBlend) <= sizeof(NVNblendState), "fits NVNblendState");
_Static_assert(sizeof(LayerChannelMask) <= sizeof(NVNchannelMaskState), "fits the state");
_Static_assert(sizeof(LayerColor) <= sizeof(NVNcolorState), "fits NVNcolorState");
_Static_assert(sizeof(LayerDepthStencil) <= sizeof(NVNdepthStencilState), "fits the state");
_Static_assert(sizeof(LayerMultisample) <= sizeof(NVNmultisampleState), "fits the state");
_Static_assert(sizeof(LayerPolygon) <= sizeof(NVNpolygonState), "fits NVNpolygonState");

/// The layer's record in @p object, or NULL with a complaint when the object is not on the eight
/// byte boundary NVN asks of its objects: nvn/nvn.h declares them as bytes, so the compiler does
/// not place them there by itself, and a program that forgot gets told rather than a misaligned
/// load.
static inline void *Record(const void *object, const char *what) {
    if (object == NULL) {
        LayerComplain("%s: no object", what);
        return NULL;
    }
    if (((uintptr_t)object & 7u) != 0u) {
        LayerComplain("%s: the object at %p is not 8-byte aligned, as NVN objects must be", what,
                 object);
        return NULL;
    }
    return (void *)(uintptr_t)object;
}

static inline LayerDevice *DeviceOf(const NVNdevice *device, const char *what) {
    LayerDevice *const record = Record(device, what);
    if (record != NULL && record->magic != MAGIC_DEVICE) {
        LayerComplain("%s: the device was not initialized", what);
        return NULL;
    }
    return record;
}

static inline LayerPool *PoolOf(const NVNmemoryPool *pool, const char *what) {
    LayerPool *const record = Record(pool, what);
    if (record != NULL && record->magic != MAGIC_POOL) {
        LayerComplain("%s: the memory pool was not initialized", what);
        return NULL;
    }
    return record;
}

static inline LayerTexture *TextureOf(const NVNtexture *texture, const char *what) {
    LayerTexture *const record = Record(texture, what);
    if (record != NULL && record->magic != MAGIC_TEXTURE) {
        LayerComplain("%s: the texture was not initialized", what);
        return NULL;
    }
    return record;
}

static inline LayerQueue *QueueOf(const NVNqueue *queue, const char *what) {
    LayerQueue *const record = Record(queue, what);
    if (record != NULL && record->magic != MAGIC_QUEUE) {
        LayerComplain("%s: the queue was not initialized", what);
        return NULL;
    }
    return record;
}

static inline LayerCommandBuffer *CmdOf(const NVNcommandBuffer *cmd, const char *what) {
    LayerCommandBuffer *const record = Record(cmd, what);
    if (record != NULL && record->magic != MAGIC_CMDBUF) {
        LayerComplain("%s: the command buffer was not initialized", what);
        return NULL;
    }
    return record;
}

static inline LayerWindow *WindowOf(const NVNwindow *window, const char *what) {
    LayerWindow *const record = Record(window, what);
    if (record != NULL && record->magic != MAGIC_WINDOW) {
        LayerComplain("%s: the window was not initialized", what);
        return NULL;
    }
    return record;
}

/// The record of a state object, which NVN keeps at four byte boundaries.
static inline void *StateRecord(const void *object, const char *what) {
    if (object == NULL) {
        LayerComplain("%s: no object", what);
        return NULL;
    }
    if (((uintptr_t)object & 3u) != 0u) {
        LayerComplain("%s: the state at %p is not 4-byte aligned, as NVN's state objects must be",
                      what, object);
        return NULL;
    }
    return (void *)(uintptr_t)object;
}

/// deko3d's device of @p device for an object made from it: from here on the device's modes
/// stay as they are.
static inline DkDevice DeviceUse(LayerDevice *device) {
    device->used = true;
    return device->device;
}

/// The shader code pool that holds the GPU address @p address, with the offset of it in the
/// pool: api55_deko3d.c keeps the pools nvnMemoryPoolInitialize made with SHADER_CODE.
DkMemBlock LayerCodePool(NVNbufferAddress address, uint32_t *offset);

_Static_assert(sizeof(DkShader) <= sizeof(((LayerControlTail *)0)->shader),
               "deko3d's shader fits the room the control section keeps for it");
