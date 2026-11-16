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
#define MAGIC_SYNC 0x4e4c4e53u    // "SNLN"
#define MAGIC_EVENT 0x4e4c5645u   // "EVLN"
#define MAGIC_BUFFER 0x4e4c4642u  // "BFLN"

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

// The sizes a queue builder keeps, each set or left to the driver.
#define QUEUE_SIZE_COMMAND 0
#define QUEUE_SIZE_COMPUTE 1
#define QUEUE_SIZE_CONTROL 2
#define QUEUE_SIZE_FLUSH 3
#define QUEUE_SIZES 4

typedef struct {
    NVNdevice *device;
    int flags;
    uint32_t set;
    size_t sizes[QUEUE_SIZES];
} LayerQueueBuilder;

/// A sync is as large as deko3d's fence, with no room for the layer's own words beside it: the
/// object holds which of the layer's fences is its own (api55_deko3d.c keeps them).
typedef struct {
    uint32_t magic;
    uint32_t slot;
} LayerSync;

typedef struct {
    const NVNmemoryPool *pool;
    ptrdiff_t offset;
} LayerEventBuilder;

/// An event is a word in a pool, which the processor signals through the pool's mapping.
typedef struct {
    uint32_t magic;
    const NVNmemoryPool *pool;
    ptrdiff_t offset;
} LayerEvent;

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
    size_t size;
} LayerPool;

typedef struct {
    NVNdevice *device;
    NVNmemoryPool *pool;
    ptrdiff_t offset;
    size_t size;
} LayerBufferBuilder;

/// A buffer is a range of its pool, and keeps where.
typedef struct {
    uint32_t magic;
    NVNmemoryPool *pool;
    ptrdiff_t offset;
    size_t size;
} LayerBuffer;

typedef struct {
    NVNdevice *device;
    int flags;
    NVNtextureTarget target;
    NVNformat format;
    int width;
    int height;
    int depth;
    int levels;
    int samples;
    NVNmemoryPool *pool;
    ptrdiff_t offset;
} LayerTextureBuilder;

typedef struct {
    DkImage image; // first: deko3d wants it at its own alignment, which the object's start has
    uint32_t magic;
    /// What the builder had when the texture was made: a texture answers for itself from then on.
    int flags;
    NVNtextureTarget target;
    NVNformat format;
    int width;
    int height;
    int depth;
    int levels;
    int samples;
    NVNmemoryPool *pool;
    ptrdiff_t offset;
} LayerTexture;

// The parts of a view a program set: a view says of each whether it has one.
#define VIEW_LEVELS 0x1u
#define VIEW_LAYERS 0x2u
#define VIEW_FORMAT 0x4u
#define VIEW_TARGET 0x8u

typedef struct {
    uint32_t set;
    int base_level;
    int levels;
    int first_layer;
    int layers;
    NVNformat format;
    NVNtextureTarget target;
} LayerView;

// The graphics stages, NVN's numbers: vertex, fragment, geometry and the two of tessellation.
#define LAYER_STAGES 5
// A stage's texture handles as NVN's programs read them: one constant buffer, the handle of
// texture slot r in the word at 0x20 + 8 * r (ScarletNX's docs/layers/gpu/nvn.md, "Constant
// buffer banks"), 32 slots, which with its first 0x20 bytes rounds up to two of deko3d's
// uniform buffer units.
#define LAYER_HANDLE_SLOTS 32
#define LAYER_HANDLE_FIRST 0x20u
#define LAYER_HANDLE_BYTES 0x200u

/// The memory a command buffer was given, as the program may ask of it, and the callback that
/// adds more.
typedef struct {
    /// The part of the command memory deko3d records into, as the processor reads it (NULL for
    /// a pool it cannot read), which the layer fills with LAYER_UNUSED_BYTE as it hands it over:
    /// how far a list got is where that byte starts again.
    const uint8_t *given;
    uint32_t given_size;
    /// All the bytes nvnCommandBufferAddCommandMemory gave, the layer's own part included.
    uint32_t added;
    size_t control;
    bool recording;
    bool command_callback;
    bool control_callback;
    PFNNVNCOMMANDBUFFERMEMORYCALLBACKPROC callback;
    void *callback_data;
} LayerCommandMemory;
#define LAYER_UNUSED_BYTE 0xa5u

typedef struct {
    int count;
    const NVNtexture *colors[COLOR_TARGETS_MAX];
    const NVNtexture *depth;
    bool scissor_set;
    DkScissor scissor;
} LayerTargets;

// NVN's and deko3d's number of viewports.
#define LAYER_VIEWPORTS 16

/// What NVN sets by itself and deko3d takes with something else: a viewport's depth range,
/// which deko3d takes with its rectangle, and three switches deko3d keeps in the rasterizer
/// state it takes for NVN's polygon state. Each call records the whole of what it is part of
/// (LayerBindRasterizer, LayerBindViewport).
typedef struct {
    /// The rectangle and the range of each viewport, the range 0 to 1 until ranged says it was
    /// set (a bit a viewport).
    DkViewport viewports[LAYER_VIEWPORTS];
    uint16_t ranged;
    /// The polygon state last bound, as nvnCommandBufferBindPolygonState took it.
    uint8_t cull;
    uint8_t front;
    /// Its polygon mode and its offset enables, once polygon says one was bound: until then
    /// deko3d's own, filled polygons and no offset.
    uint8_t mode;
    uint8_t offset;
    bool polygon;
    bool discard;
    bool depth_clamp;
    bool provoking_first;
} LayerDynamic;

/// Records @p record's rasterizer state from its polygon state and its switches, and viewport
/// @p index from its rectangle and its range.
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
    /// What the command buffer answers of its memory and of its recording (api55_command.c).
    LayerCommandMemory memory;
    /// The targets and the scissor the program last set, which a clear of a texture that is no
    /// target puts back when it is done (api55_command.c).
    LayerTargets targets;
    /// nvnCommandBufferSetCopyImageStride's, kept for its getter.
    ptrdiff_t copy_image_stride;
    /// The state NVN sets by itself and deko3d takes together with another's (api55_command.c).
    LayerDynamic dynamic;
} LayerDraw;

typedef struct {
    uint32_t magic;
    DkCmdBuf cmdbuf;
    /// nvnCommandBufferSetCopyRowStride's, nought for rows packed tight, NVN's default.
    uint32_t copy_row_stride;
    LayerDraw *draw;
} LayerCommandBuffer;

/// Records @p record's rasterizer state from its polygon state and its switches, and viewport
/// @p index from its rectangle and its range (api55_command.c).
void LayerBindRasterizer(const LayerCommandBuffer *record);
void LayerBindViewport(const LayerCommandBuffer *record, int index);

typedef struct {
    uint32_t magic;
    /// The stages' shaders, by NVN stage: each inside the control section the program keeps
    /// (NvnLayerShaderControl), NULL for a stage the program has none of.
    const DkShader *shaders[LAYER_STAGES];
} LayerProgram;

/// What a sampler is, in a builder and in the sampler made from it. The border colour is its
/// four words, whichever kind (float, signed, unsigned) set them.
typedef struct {
    uint8_t min;
    uint8_t mag;
    uint8_t wrap[3];
    uint8_t compare_mode;
    uint8_t compare_func;
    uint8_t reduction;
    uint32_t border[4];
    float lod_min;
    float lod_max;
    float lod_bias;
    float anisotropy;
    float lod_snap;
} LayerSamplerState;

typedef struct {
    NVNdevice *device;
    LayerSamplerState state;
} LayerSamplerBuilder;

typedef struct {
    uint32_t magic;
    LayerSamplerState state;
} LayerSampler;

/// A texture pool or a sampler pool: @p count descriptors of 32 bytes in a memory pool.
typedef struct {
    uint32_t magic;
    int count;
    DkMemBlock block;
    uint32_t offset;
    const NVNmemoryPool *memory;
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

// A blend state's two words: the target and the four functions, then the equations and the
// advanced mode's four fields.
typedef struct {
    uint32_t target : 3;
    uint32_t src_rgb : 7;
    uint32_t dst_rgb : 7;
    uint32_t src_alpha : 7;
    uint32_t dst_alpha : 7;
    uint32_t equation_rgb : 3;
    uint32_t equation_alpha : 3;
    uint32_t advanced : 8;
    uint32_t overlap : 2;
    uint32_t premultiplied : 1;
    uint32_t normalized : 1;
} LayerBlend;

typedef struct {
    uint32_t masks;
} LayerChannelMask;

typedef struct {
    uint32_t blend_enable : 8;
    uint32_t logic_op : 4;
    uint32_t alpha_func : 4;
} LayerColor;

// A face's stencil state: its function and what a failed stencil test, a failed depth test and
// a pass do.
typedef struct {
    uint32_t depth_test : 1;
    uint32_t depth_write : 1;
    uint32_t depth_func : 4;
    uint32_t stencil_test : 1;
    uint32_t front_func : 4;
    uint32_t front_fail : 4;
    uint32_t front_depth_fail : 4;
    uint32_t front_pass : 4;
    uint32_t back_func : 4;
    uint32_t back_fail : 4;
    uint32_t back_depth_fail : 4;
    uint32_t back_pass : 4;
} LayerDepthStencil;

// The flags of a multisample state.
#define MULTISAMPLE_ENABLE 0x01u
#define MULTISAMPLE_ALPHA_TO_COVERAGE 0x02u
#define MULTISAMPLE_DITHER 0x04u
#define MULTISAMPLE_COVERAGE_TO_COLOR 0x08u
#define MULTISAMPLE_LOCATIONS 0x10u
#define MULTISAMPLE_GRID 0x20u
#define MULTISAMPLE_TRUNCATION 0x40u
#define MULTISAMPLE_LOCATIONS_MAX 16

typedef struct {
    uint8_t samples;
    uint8_t raster_samples;
    uint8_t coverage_modulation;
    uint8_t coverage_to_color_output;
    uint8_t flags;
    /// The sample locations, x in the low four bits and y in the high, sixteenths of a pixel.
    uint8_t locations[MULTISAMPLE_LOCATIONS_MAX];
} LayerMultisample;

typedef struct {
    uint32_t cull : 2;
    uint32_t front : 1;
    uint32_t mode : 2;
    uint32_t offset : 3;
} LayerPolygon;

/// A builder keeps its textures themselves, as NVN's does: the program's list need not outlive
/// the call that gave it. active is the textures the window hands in turn, 0 for all of them.
typedef struct {
    NVNdevice *device;
    NVNnativeWindow native;
    NVNtexture *textures[WINDOW_TEXTURES_MAX];
    int16_t count;
    int16_t active;
    int32_t interval;
} LayerWindowBuilder;

typedef struct {
    uint32_t magic;
    DkSwapchain swapchain;
    NVNnativeWindow native;
    int16_t count;
    int16_t active;
    int32_t interval;
    /// x, y, width and height, as nvnWindowSetCrop took them.
    int32_t crop[4];
} LayerWindow;

_Static_assert(sizeof(LayerDevice) <= sizeof(NVNdevice), "the device record fits NVNdevice");
_Static_assert(sizeof(LayerDeviceBuilder) <= sizeof(NVNdeviceBuilder), "fits NVNdeviceBuilder");
_Static_assert(sizeof(LayerQueueBuilder) <= sizeof(NVNqueueBuilder), "fits NVNqueueBuilder");
_Static_assert(sizeof(LayerQueue) <= sizeof(NVNqueue), "fits NVNqueue");
_Static_assert(sizeof(LayerSync) <= sizeof(NVNsync), "fits NVNsync");
_Static_assert(sizeof(LayerEventBuilder) <= sizeof(NVNeventBuilder), "fits the builder");
_Static_assert(sizeof(LayerEvent) <= sizeof(NVNevent), "fits NVNevent");
_Static_assert(sizeof(LayerPoolBuilder) <= sizeof(NVNmemoryPoolBuilder), "fits the builder");
_Static_assert(sizeof(LayerPool) <= sizeof(NVNmemoryPool), "fits NVNmemoryPool");
_Static_assert(sizeof(LayerBufferBuilder) <= sizeof(NVNbufferBuilder), "fits the builder");
_Static_assert(sizeof(LayerBuffer) <= sizeof(NVNbuffer), "fits NVNbuffer");
_Static_assert(sizeof(LayerTextureBuilder) <= sizeof(NVNtextureBuilder), "fits the builder");
_Static_assert(sizeof(LayerTexture) <= sizeof(NVNtexture), "fits NVNtexture");
_Static_assert(sizeof(LayerView) <= sizeof(NVNtextureView), "fits NVNtextureView");
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

/// A program's stages set from @p count shader data, for the two NVN calls that set them, the
/// one named @p what: neither calls the other, since a runtime that runs NVN itself takes each
/// at its own address (tools/nvn-lookups.py holds the program to that).
NVNboolean LayerProgramSetShaders(NVNprogram *program, int count, const NVNshaderData *shaders,
                                  const char *what);

/// deko3d's call for a command buffer out of command memory, @p user the NVN command buffer:
/// the program's callback, if it set one and left it on (api55_command.c).
void LayerCommandGrow(void *user, DkCmdBuf cmdbuf, size_t least);

_Static_assert(sizeof(DkShader) <= sizeof(((LayerControlTail *)0)->shader),
               "deko3d's shader fits the room the control section keeps for it");
