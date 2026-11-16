// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

/// The layer's entry points, with their NVN prototypes: X(result, name, parameters). One list
/// for every build of the layer and for NvnLoadProcs, which gets each function through the
/// lookup, so a build's bodies and a program's calls cannot disagree on a type. The prototypes
/// follow ScarletNX's gpu/nvn/nvn.hook, in the types of nvn/nvn.h.
///
/// Two functions are in none of them, and NvnProcs carries them by hand: nvnDeviceGetInteger,
/// written in assembly (docs/NvnLayer.md, "The API version"), and nvnDeviceGetProcAddress, the
/// lookup itself, which every build has whole.
///
/// Three lists, by what each build of the layer does with them:
/// - NVN_LAYER_PROCS_BASE (with NVN_LAYER_PROCS_TEXTURE, NVN_LAYER_PROCS_POOL,
///   NVN_LAYER_PROCS_SYNC, NVN_LAYER_PROCS_SAMPLER, NVN_LAYER_PROCS_BUFFER,
///   NVN_LAYER_PROCS_WINDOW, NVN_LAYER_PROCS_COUNTER, NVN_LAYER_PROCS_RECORDING,
///   NVN_LAYER_PROCS_CLEAR, NVN_LAYER_PROCS_STATE, NVN_LAYER_PROCS_DEBUG, NVN_LAYER_PROCS_OBJECT
///   and NVN_LAYER_PROCS_STATE_OBJECTS, parts of it written apart) and NVN_LAYER_PROCS_DRAW: both API
///   versions have them. The API 55
///   build gives each a body on deko3d; the API 231 build gives each its stopping body.
/// - NVN_LAYER_PROCS_231 (with NVN_LAYER_PROCS_WINDOW_231, NVN_LAYER_PROCS_DEBUG_231 and
///   NVN_LAYER_PROCS_PROGRAM_231, parts of it written apart): the
///   functions only the API 231 build has, compute's among them, which
///   NVN 55 has too and the deko3d build does not implement yet.

// What a texture, its builder and a view answer of themselves, and the levels of a texture.
#define NVN_LAYER_PROCS_TEXTURE(X)                                                                \
    X(void, nvnTextureBuilderSetWidth, (NVNtextureBuilder * builder, int width))                  \
    X(void, nvnTextureBuilderSetHeight, (NVNtextureBuilder * builder, int height))                \
    X(void, nvnTextureBuilderSetDepth, (NVNtextureBuilder * builder, int depth))                  \
    X(void, nvnTextureBuilderSetLevels, (NVNtextureBuilder * builder, int levels))                \
    X(void, nvnTextureBuilderSetSamples, (NVNtextureBuilder * builder, int samples))              \
    X(NVNdevice *, nvnTextureBuilderGetDevice, (const NVNtextureBuilder *builder))                \
    X(NVNtextureFlags, nvnTextureBuilderGetFlags, (const NVNtextureBuilder *builder))             \
    X(NVNtextureTarget, nvnTextureBuilderGetTarget, (const NVNtextureBuilder *builder))           \
    X(NVNformat, nvnTextureBuilderGetFormat, (const NVNtextureBuilder *builder))                  \
    X(int, nvnTextureBuilderGetWidth, (const NVNtextureBuilder *builder))                         \
    X(int, nvnTextureBuilderGetHeight, (const NVNtextureBuilder *builder))                        \
    X(int, nvnTextureBuilderGetDepth, (const NVNtextureBuilder *builder))                         \
    X(int, nvnTextureBuilderGetLevels, (const NVNtextureBuilder *builder))                        \
    X(int, nvnTextureBuilderGetSamples, (const NVNtextureBuilder *builder))                       \
    X(NVNmemoryPool *, nvnTextureBuilderGetMemoryPool, (const NVNtextureBuilder *builder))        \
    X(ptrdiff_t, nvnTextureBuilderGetMemoryOffset, (const NVNtextureBuilder *builder))            \
    X(NVNtextureFlags, nvnTextureGetFlags, (const NVNtexture *texture))                           \
    X(NVNtextureTarget, nvnTextureGetTarget, (const NVNtexture *texture))                         \
    X(NVNformat, nvnTextureGetFormat, (const NVNtexture *texture))                                \
    X(int, nvnTextureGetWidth, (const NVNtexture *texture))                                       \
    X(int, nvnTextureGetHeight, (const NVNtexture *texture))                                      \
    X(int, nvnTextureGetDepth, (const NVNtexture *texture))                                       \
    X(int, nvnTextureGetLevels, (const NVNtexture *texture))                                      \
    X(int, nvnTextureGetSamples, (const NVNtexture *texture))                                     \
    X(NVNmemoryPool *, nvnTextureGetMemoryPool, (const NVNtexture *texture))                      \
    X(ptrdiff_t, nvnTextureGetMemoryOffset, (const NVNtexture *texture))                          \
    X(void, nvnTextureViewSetDefaults, (NVNtextureView * view))                                   \
    X(void, nvnTextureViewSetLevels, (NVNtextureView * view, int base, int count))                \
    X(void, nvnTextureViewSetLayers, (NVNtextureView * view, int first, int count))               \
    X(void, nvnTextureViewSetFormat, (NVNtextureView * view, NVNformat format))                   \
    X(void, nvnTextureViewSetTarget, (NVNtextureView * view, NVNtextureTarget target))            \
    X(NVNboolean, nvnTextureViewGetLevels, (const NVNtextureView *view, int *base, int *count))   \
    X(NVNboolean, nvnTextureViewGetLayers, (const NVNtextureView *view, int *first, int *count))  \
    X(NVNboolean, nvnTextureViewGetFormat, (const NVNtextureView *view, NVNformat *format))       \
    X(NVNboolean, nvnTextureViewGetTarget,                                                        \
      (const NVNtextureView *view, NVNtextureTarget *target))                                     \
    X(NVNboolean, nvnTextureViewCompare, (const NVNtextureView *a, const NVNtextureView *b))

// What a memory pool and its builder answer of themselves, and the processor's cache of a pool.
#define NVN_LAYER_PROCS_POOL(X)                                                                   \
    X(NVNdevice *, nvnMemoryPoolBuilderGetDevice, (const NVNmemoryPoolBuilder *builder))          \
    X(void *, nvnMemoryPoolBuilderGetMemory, (const NVNmemoryPoolBuilder *builder))               \
    X(size_t, nvnMemoryPoolBuilderGetSize, (const NVNmemoryPoolBuilder *builder))                 \
    X(NVNmemoryPoolFlags, nvnMemoryPoolBuilderGetFlags, (const NVNmemoryPoolBuilder *builder))    \
    X(size_t, nvnMemoryPoolGetSize, (const NVNmemoryPool *pool))                                  \
    X(NVNmemoryPoolFlags, nvnMemoryPoolGetFlags, (const NVNmemoryPool *pool))                     \
    X(void, nvnMemoryPoolInvalidateMappedRange,                                                   \
      (const NVNmemoryPool *pool, ptrdiff_t offset, size_t size))

// Syncs and what fences and waits for them, events as the processor signals them, a queue's
// error and what a queue builder answers of itself.
#define NVN_LAYER_PROCS_SYNC(X)                                                                   \
    X(NVNboolean, nvnSyncInitialize, (NVNsync * sync, NVNdevice *device))                         \
    X(void, nvnSyncFinalize, (NVNsync * sync))                                                    \
    X(NVNsyncWaitResult, nvnSyncWait, (const NVNsync *sync, uint64_t timeout))                    \
    X(void, nvnQueueFenceSync,                                                                    \
      (NVNqueue * queue, NVNsync *sync, NVNsyncCondition condition, int flags))                   \
    X(NVNboolean, nvnQueueWaitSync, (NVNqueue * queue, const NVNsync *sync))                      \
    X(void, nvnCommandBufferFenceSync,                                                            \
      (NVNcommandBuffer * cmd, NVNsync *sync, NVNsyncCondition condition, int flags))             \
    X(void, nvnCommandBufferWaitSync, (NVNcommandBuffer * cmd, const NVNsync *sync))              \
    X(void, nvnEventBuilderSetDefaults, (NVNeventBuilder * builder))                              \
    X(void, nvnEventBuilderSetStorage,                                                            \
      (NVNeventBuilder * builder, const NVNmemoryPool *pool, ptrdiff_t offset))                   \
    X(const NVNmemoryPool *, nvnEventBuilderGetStorage,                                           \
      (const NVNeventBuilder *builder, ptrdiff_t *offset))                                        \
    X(const NVNmemoryPool *, nvnEventBuilderGetMemoryPool, (const NVNeventBuilder *builder))      \
    X(ptrdiff_t, nvnEventBuilderGetMemoryOffset, (const NVNeventBuilder *builder))                \
    X(NVNboolean, nvnEventInitialize, (NVNevent * event, const NVNeventBuilder *builder))         \
    X(void, nvnEventFinalize, (NVNevent * event))                                                 \
    X(uint32_t, nvnEventGetValue, (const NVNevent *event))                                        \
    X(void, nvnEventSignal, (NVNevent * event, NVNeventSignalMode mode, uint32_t value))          \
    X(const NVNmemoryPool *, nvnEventGetMemoryPool, (const NVNevent *event))                      \
    X(ptrdiff_t, nvnEventGetMemoryOffset, (const NVNevent *event))                                \
    X(NVNqueueGetErrorResult, nvnQueueGetError, (NVNqueue * queue, NVNqueueErrorInfo *info))      \
    X(void, nvnQueueBuilderSetFlags, (NVNqueueBuilder * builder, int flags))                      \
    X(void, nvnQueueBuilderSetCommandMemorySize, (NVNqueueBuilder * builder, size_t size))        \
    X(void, nvnQueueBuilderSetComputeMemorySize, (NVNqueueBuilder * builder, size_t size))        \
    X(void, nvnQueueBuilderSetControlMemorySize, (NVNqueueBuilder * builder, size_t size))        \
    X(void, nvnQueueBuilderSetCommandFlushThreshold, (NVNqueueBuilder * builder, size_t size))    \
    X(NVNdevice *, nvnQueueBuilderGetDevice, (const NVNqueueBuilder *builder))                    \
    X(int, nvnQueueBuilderGetFlags, (const NVNqueueBuilder *builder))                             \
    X(NVNboolean, nvnQueueBuilderGetCommandMemorySize,                                            \
      (const NVNqueueBuilder *builder, int *size))                                                \
    X(NVNboolean, nvnQueueBuilderGetComputeMemorySize,                                            \
      (const NVNqueueBuilder *builder, int *size))                                                \
    X(NVNboolean, nvnQueueBuilderGetControlMemorySize,                                            \
      (const NVNqueueBuilder *builder, int *size))                                                \
    X(NVNboolean, nvnQueueBuilderGetCommandFlushThreshold,                                        \
      (const NVNqueueBuilder *builder, int *size))

// The rest of a sampler: its compare, its levels, its border colour, what it and its builder
// answer of themselves, and what the texture and sampler pools do.
#define NVN_LAYER_PROCS_SAMPLER(X)                                                                \
    X(void, nvnSamplerBuilderSetCompare,                                                          \
      (NVNsamplerBuilder * builder, NVNcompareMode mode, NVNcompareFunc func))                    \
    X(void, nvnSamplerBuilderSetReductionFilter,                                                  \
      (NVNsamplerBuilder * builder, NVNsamplerReduction filter))                                  \
    X(void, nvnSamplerBuilderSetLodClamp, (NVNsamplerBuilder * builder, float min, float max))    \
    X(void, nvnSamplerBuilderSetLodBias, (NVNsamplerBuilder * builder, float bias))               \
    X(void, nvnSamplerBuilderSetLodSnap, (NVNsamplerBuilder * builder, float snap))               \
    X(void, nvnSamplerBuilderSetMaxAnisotropy, (NVNsamplerBuilder * builder, float anisotropy))   \
    X(void, nvnSamplerBuilderSetBorderColor, (NVNsamplerBuilder * builder, const float *color))   \
    X(void, nvnSamplerBuilderSetBorderColori, (NVNsamplerBuilder * builder, const int *color))    \
    X(void, nvnSamplerBuilderSetBorderColorui,                                                    \
      (NVNsamplerBuilder * builder, const uint32_t *color))                                       \
    X(NVNdevice *, nvnSamplerBuilderGetDevice, (const NVNsamplerBuilder *builder))                \
    X(void, nvnSamplerBuilderGetMinMagFilter,                                                     \
      (const NVNsamplerBuilder *builder, NVNminFilter *min, NVNmagFilter *mag))                   \
    X(void, nvnSamplerBuilderGetWrapMode,                                                         \
      (const NVNsamplerBuilder *builder, NVNwrapMode *s, NVNwrapMode *t, NVNwrapMode *r))         \
    X(void, nvnSamplerBuilderGetCompare,                                                          \
      (const NVNsamplerBuilder *builder, NVNcompareMode *mode, NVNcompareFunc *func))             \
    X(NVNsamplerReduction, nvnSamplerBuilderGetReductionFilter,                                   \
      (const NVNsamplerBuilder *builder))                                                         \
    X(void, nvnSamplerBuilderGetLodClamp,                                                         \
      (const NVNsamplerBuilder *builder, float *min, float *max))                                 \
    X(float, nvnSamplerBuilderGetLodBias, (const NVNsamplerBuilder *builder))                     \
    X(float, nvnSamplerBuilderGetLodSnap, (const NVNsamplerBuilder *builder))                     \
    X(float, nvnSamplerBuilderGetMaxAnisotropy, (const NVNsamplerBuilder *builder))               \
    X(void, nvnSamplerBuilderGetBorderColor, (const NVNsamplerBuilder *builder, float *color))    \
    X(void, nvnSamplerBuilderGetBorderColori, (const NVNsamplerBuilder *builder, int *color))     \
    X(void, nvnSamplerBuilderGetBorderColorui,                                                    \
      (const NVNsamplerBuilder *builder, uint32_t *color))                                        \
    X(void, nvnSamplerGetMinMagFilter,                                                            \
      (const NVNsampler *sampler, NVNminFilter *min, NVNmagFilter *mag))                          \
    X(void, nvnSamplerGetWrapMode,                                                                \
      (const NVNsampler *sampler, NVNwrapMode *s, NVNwrapMode *t, NVNwrapMode *r))                \
    X(void, nvnSamplerGetCompare,                                                                 \
      (const NVNsampler *sampler, NVNcompareMode *mode, NVNcompareFunc *func))                    \
    X(NVNsamplerReduction, nvnSamplerGetReductionFilter, (const NVNsampler *sampler))             \
    X(void, nvnSamplerGetLodClamp, (const NVNsampler *sampler, float *min, float *max))           \
    X(float, nvnSamplerGetLodBias, (const NVNsampler *sampler))                                   \
    X(float, nvnSamplerGetMaxAnisotropy, (const NVNsampler *sampler))                             \
    X(void, nvnSamplerGetBorderColor, (const NVNsampler *sampler, float *color))                  \
    X(void, nvnSamplerGetBorderColori, (const NVNsampler *sampler, int *color))                   \
    X(void, nvnSamplerGetBorderColorui, (const NVNsampler *sampler, uint32_t *color))             \
    X(NVNboolean, nvnSamplerCompare, (const NVNsampler *a, const NVNsampler *b))                  \
    X(void, nvnSamplerPoolRegisterSamplerBuilder,                                                 \
      (const NVNsamplerPool *pool, int id, const NVNsamplerBuilder *builder))                     \
    X(int, nvnSamplerPoolGetSize, (const NVNsamplerPool *pool))                                   \
    X(const NVNmemoryPool *, nvnSamplerPoolGetMemoryPool, (const NVNsamplerPool *pool))           \
    X(ptrdiff_t, nvnSamplerPoolGetMemoryOffset, (const NVNsamplerPool *pool))                     \
    X(int, nvnTexturePoolGetSize, (const NVNtexturePool *pool))                                   \
    X(const NVNmemoryPool *, nvnTexturePoolGetMemoryPool, (const NVNtexturePool *pool))           \
    X(ptrdiff_t, nvnTexturePoolGetMemoryOffset, (const NVNtexturePool *pool))

// Buffers: a range of a memory pool with an address and a mapping of its own.
#define NVN_LAYER_PROCS_BUFFER(X)                                                                 \
    X(void, nvnBufferBuilderSetDefaults, (NVNbufferBuilder * builder))                            \
    X(void, nvnBufferBuilderSetDevice, (NVNbufferBuilder * builder, NVNdevice *device))           \
    X(void, nvnBufferBuilderSetStorage,                                                           \
      (NVNbufferBuilder * builder, NVNmemoryPool *pool, ptrdiff_t offset, size_t size))           \
    X(NVNdevice *, nvnBufferBuilderGetDevice, (const NVNbufferBuilder *builder))                  \
    X(NVNmemoryPool *, nvnBufferBuilderGetMemoryPool, (const NVNbufferBuilder *builder))          \
    X(ptrdiff_t, nvnBufferBuilderGetMemoryOffset, (const NVNbufferBuilder *builder))              \
    X(size_t, nvnBufferBuilderGetSize, (const NVNbufferBuilder *builder))                         \
    X(NVNboolean, nvnBufferInitialize, (NVNbuffer * buffer, const NVNbufferBuilder *builder))     \
    X(void, nvnBufferFinalize, (NVNbuffer * buffer))                                              \
    X(void *, nvnBufferMap, (const NVNbuffer *buffer))                                            \
    X(NVNbufferAddress, nvnBufferGetAddress, (const NVNbuffer *buffer))                           \
    X(NVNmemoryPool *, nvnBufferGetMemoryPool, (const NVNbuffer *buffer))                         \
    X(ptrdiff_t, nvnBufferGetMemoryOffset, (const NVNbuffer *buffer))                             \
    X(size_t, nvnBufferGetSize, (const NVNbuffer *buffer))                                        \
    X(void, nvnBufferFlushMappedRange,                                                            \
      (const NVNbuffer *buffer, ptrdiff_t offset, size_t size))                                   \
    X(void, nvnBufferInvalidateMappedRange,                                                       \
      (const NVNbuffer *buffer, ptrdiff_t offset, size_t size))

// The rest of a window: what it and its builder answer of themselves, its crop, its present
// interval, the textures it hands in turn, and a texture acquired with a sync of the program's.
#define NVN_LAYER_PROCS_WINDOW(X)                                                                 \
    X(void, nvnWindowBuilderSetPresentInterval, (NVNwindowBuilder * builder, int interval))       \
    X(void, nvnWindowBuilderSetNumActiveTextures, (NVNwindowBuilder * builder, int count))        \
    X(NVNdevice *, nvnWindowBuilderGetDevice, (const NVNwindowBuilder *builder))                  \
    X(NVNnativeWindow, nvnWindowBuilderGetNativeWindow, (const NVNwindowBuilder *builder))        \
    X(int, nvnWindowBuilderGetNumTextures, (const NVNwindowBuilder *builder))                     \
    X(NVNtexture *, nvnWindowBuilderGetTexture, (const NVNwindowBuilder *builder, int index))     \
    X(int, nvnWindowBuilderGetPresentInterval, (const NVNwindowBuilder *builder))                 \
    X(int, nvnWindowBuilderGetNumActiveTextures, (const NVNwindowBuilder *builder))               \
    X(NVNnativeWindow, nvnWindowGetNativeWindow, (const NVNwindow *window))                       \
    X(int, nvnWindowGetNumTextures, (const NVNwindow *window))                                    \
    X(int, nvnWindowGetNumActiveTextures, (const NVNwindow *window))                              \
    X(void, nvnWindowSetNumActiveTextures, (NVNwindow * window, int count))                       \
    X(int, nvnWindowGetPresentInterval, (const NVNwindow *window))                                \
    X(void, nvnWindowSetPresentInterval, (NVNwindow * window, int interval))                      \
    X(void, nvnWindowGetCrop, (const NVNwindow *window, NVNrectangle *crop))                      \
    X(void, nvnWindowSetCrop, (NVNwindow * window, int x, int y, int width, int height))          \
    X(NVNwindowAcquireTextureResult, nvnWindowAcquireTexture,                                     \
      (NVNwindow * window, NVNsync *sync, int *index))

// The Ounce's windows (API 231): the colour space a builder keeps, and the HDR metadata of the
// display and of the content, on a builder and on a window.
#define NVN_LAYER_PROCS_WINDOW_231(X)                                                             \
    X(void, nvnWindowBuilderSetColorSpace, (NVNwindowBuilder * builder, int space))               \
    X(int, nvnWindowBuilderGetColorSpace, (const NVNwindowBuilder *builder))                      \
    X(void, nvnWindowBuilderSetHdrDisplayMetadata,                                                \
      (NVNwindowBuilder * builder, float red_x, float red_y, float green_x, float green_y,        \
       float blue_x, float blue_y, float white_x, float white_y, float least, float most))        \
    X(void, nvnWindowBuilderGetHdrDisplayMetadata,                                                \
      (const NVNwindowBuilder *builder, float *red_x, float *red_y, float *green_x,               \
       float *green_y, float *blue_x, float *blue_y, float *white_x, float *white_y,              \
       float *least, float *most))                                                                \
    X(void, nvnWindowBuilderSetHdrContentMetadata,                                                \
      (NVNwindowBuilder * builder, float most, float frame_average))                              \
    X(void, nvnWindowBuilderGetHdrContentMetadata,                                                \
      (const NVNwindowBuilder *builder, float *most, float *frame_average))                       \
    X(void, nvnWindowSetHdrDisplayMetadata,                                                       \
      (NVNwindow * window, float red_x, float red_y, float green_x, float green_y, float blue_x,  \
       float blue_y, float white_x, float white_y, float least, float most))                      \
    X(void, nvnWindowGetHdrDisplayMetadata,                                                       \
      (const NVNwindow *window, float *red_x, float *red_y, float *green_x, float *green_y,       \
       float *blue_x, float *blue_y, float *white_x, float *white_y, float *least, float *most))  \
    X(void, nvnWindowSetHdrContentMetadata,                                                       \
      (NVNwindow * window, float most, float frame_average))                                      \
    X(void, nvnWindowGetHdrContentMetadata,                                                       \
      (const NVNwindow *window, float *most, float *frame_average))

// What a list reports to memory (a counter with its time, a value), the counter's reset, the
// render enable, and the device's two readings of the graphics unit's clock.
#define NVN_LAYER_PROCS_COUNTER(X)                                                                \
    X(void, nvnCommandBufferReportCounter,                                                        \
      (NVNcommandBuffer * cmd, NVNcounterType type, NVNbufferAddress address))                    \
    X(void, nvnCommandBufferReportValue,                                                          \
      (NVNcommandBuffer * cmd, uint32_t value, NVNbufferAddress address))                         \
    X(void, nvnCommandBufferResetCounter, (NVNcommandBuffer * cmd, NVNcounterType type))          \
    X(void, nvnCommandBufferSetRenderEnable, (NVNcommandBuffer * cmd, NVNboolean enable))         \
    X(void, nvnCommandBufferSetRenderEnableConditional,                                           \
      (NVNcommandBuffer * cmd, NVNconditionalRenderMode mode, NVNbufferAddress address))          \
    X(uint64_t, nvnDeviceGetTimestampInNanoseconds,                                               \
      (const NVNdevice *device, const NVNcounterData *counter))                                   \
    X(uint64_t, nvnDeviceGetCurrentTimestampInNanoseconds, (const NVNdevice *device))

// What a command buffer says of its memory and of its recording, the callback that adds memory
// to a list that ran out, and lists called from a list or copied into one.
#define NVN_LAYER_PROCS_RECORDING(X)                                                              \
    X(size_t, nvnCommandBufferGetCommandMemorySize, (const NVNcommandBuffer *cmd))                \
    X(size_t, nvnCommandBufferGetCommandMemoryUsed, (const NVNcommandBuffer *cmd))                \
    X(size_t, nvnCommandBufferGetCommandMemoryFree, (const NVNcommandBuffer *cmd))                \
    X(size_t, nvnCommandBufferGetControlMemorySize, (const NVNcommandBuffer *cmd))                \
    X(size_t, nvnCommandBufferGetControlMemoryUsed, (const NVNcommandBuffer *cmd))                \
    X(size_t, nvnCommandBufferGetControlMemoryFree, (const NVNcommandBuffer *cmd))                \
    X(NVNboolean, nvnCommandBufferIsRecording, (const NVNcommandBuffer *cmd))                     \
    X(void, nvnCommandBufferSetMemoryCallback,                                                    \
      (NVNcommandBuffer * cmd, PFNNVNCOMMANDBUFFERMEMORYCALLBACKPROC callback))                   \
    X(void, nvnCommandBufferSetMemoryCallbackData, (NVNcommandBuffer * cmd, void *data))          \
    X(PFNNVNCOMMANDBUFFERMEMORYCALLBACKPROC, nvnCommandBufferGetMemoryCallback,                   \
      (const NVNcommandBuffer *cmd))                                                              \
    X(void *, nvnCommandBufferGetMemoryCallbackData, (const NVNcommandBuffer *cmd))               \
    X(void, nvnCommandBufferSetCommandMemoryCallbackEnabled,                                      \
      (NVNcommandBuffer * cmd, NVNboolean enabled))                                               \
    X(void, nvnCommandBufferSetControlMemoryCallbackEnabled,                                      \
      (NVNcommandBuffer * cmd, NVNboolean enabled))                                               \
    X(void, nvnCommandBufferCallCommands,                                                         \
      (NVNcommandBuffer * cmd, int count, const NVNcommandHandle *handles))                       \
    X(void, nvnCommandBufferCopyCommands,                                                         \
      (NVNcommandBuffer * cmd, int count, const NVNcommandHandle *handles))

// Clears past the float clear of a target (integer targets, a region of any texture, a range of
// a buffer), what a copy keeps of its strides, and targets whose contents are given up.
#define NVN_LAYER_PROCS_CLEAR(X)                                                                  \
    X(void, nvnCommandBufferClearColori,                                                          \
      (NVNcommandBuffer * cmd, int index, const int *rgba, int mask))                             \
    X(void, nvnCommandBufferClearColorui,                                                         \
      (NVNcommandBuffer * cmd, int index, const uint32_t *rgba, int mask))                        \
    X(void, nvnCommandBufferClearTexture,                                                         \
      (NVNcommandBuffer * cmd, const NVNtexture *texture, const NVNtextureView *view,             \
       const NVNcopyRegion *region, const float *rgba, int mask))                                 \
    X(void, nvnCommandBufferClearTexturei,                                                        \
      (NVNcommandBuffer * cmd, const NVNtexture *texture, const NVNtextureView *view,             \
       const NVNcopyRegion *region, const int *rgba, int mask))                                   \
    X(void, nvnCommandBufferClearTextureui,                                                       \
      (NVNcommandBuffer * cmd, const NVNtexture *texture, const NVNtextureView *view,             \
       const NVNcopyRegion *region, const uint32_t *rgba, int mask))                              \
    X(void, nvnCommandBufferClearBuffer,                                                          \
      (NVNcommandBuffer * cmd, NVNbufferAddress address, size_t size, uint32_t value))            \
    X(void, nvnCommandBufferSetCopyImageStride, (NVNcommandBuffer * cmd, ptrdiff_t stride))       \
    X(ptrdiff_t, nvnCommandBufferGetCopyImageStride, (const NVNcommandBuffer *cmd))               \
    X(ptrdiff_t, nvnCommandBufferGetCopyRowStride, (const NVNcommandBuffer *cmd))                 \
    X(void, nvnCommandBufferDiscardColor, (NVNcommandBuffer * cmd, int index))                    \
    X(void, nvnCommandBufferDiscardDepthStencil, (NVNcommandBuffer * cmd))

// The render state a list sets by itself: viewports and scissors by number, depth ranges and
// bounds, the rasterizer's switches, the point size, the restart index, the tiled cache, the
// line width, conservative rasterization, and a patch's size and tessellation levels.
#define NVN_LAYER_PROCS_STATE(X)                                                                  \
    X(void, nvnCommandBufferSetViewports,                                                         \
      (NVNcommandBuffer * cmd, int first, int count, const float *rectangles))                    \
    X(void, nvnCommandBufferSetScissors,                                                          \
      (NVNcommandBuffer * cmd, int first, int count, const int *rectangles))                      \
    X(void, nvnCommandBufferSetDepthRange, (NVNcommandBuffer * cmd, float near, float far))       \
    X(void, nvnCommandBufferSetDepthRanges,                                                       \
      (NVNcommandBuffer * cmd, int first, int count, const float *ranges))                        \
    X(void, nvnCommandBufferSetViewportSwizzles,                                                  \
      (NVNcommandBuffer * cmd, int first, int count, const NVNviewportSwizzle *swizzles))         \
    X(void, nvnCommandBufferSetDepthBounds,                                                       \
      (NVNcommandBuffer * cmd, NVNboolean enable, float least, float most))                       \
    X(void, nvnCommandBufferSetRasterizerDiscard, (NVNcommandBuffer * cmd, NVNboolean discard))   \
    X(void, nvnCommandBufferSetDepthClamp, (NVNcommandBuffer * cmd, NVNboolean clamp))            \
    X(void, nvnCommandBufferSetProvokingVertexMode, (NVNcommandBuffer * cmd, int mode))           \
    X(void, nvnCommandBufferSetPointSize, (NVNcommandBuffer * cmd, float size))                   \
    X(void, nvnCommandBufferSetPrimitiveRestart,                                                  \
      (NVNcommandBuffer * cmd, NVNboolean enable, int index))                                     \
    X(void, nvnCommandBufferSetTiledCacheAction,                                                  \
      (NVNcommandBuffer * cmd, NVNtiledCacheAction action))                                       \
    X(void, nvnCommandBufferSetTiledCacheTileSize,                                                \
      (NVNcommandBuffer * cmd, int width, int height))                                            \
    X(void, nvnCommandBufferSetLineWidth, (NVNcommandBuffer * cmd, float width))                  \
    X(void, nvnCommandBufferSetConservativeRasterEnable,                                          \
      (NVNcommandBuffer * cmd, NVNboolean enable))                                                \
    X(void, nvnCommandBufferSetConservativeRasterDilate, (NVNcommandBuffer * cmd, float dilate))  \
    X(void, nvnCommandBufferSetSubpixelPrecisionBias, (NVNcommandBuffer * cmd, int x, int y))     \
    X(void, nvnCommandBufferSetPatchSize, (NVNcommandBuffer * cmd, int size))                     \
    X(void, nvnCommandBufferSetOuterTessellationLevels,                                           \
      (NVNcommandBuffer * cmd, const float *levels))                                              \
    X(void, nvnCommandBufferSetInnerTessellationLevels,                                           \
      (NVNcommandBuffer * cmd, const float *levels))

// The debug layer's calls, as a release driver answers them: labels, markers and groups, object
// numbers, the callback, the object database and the checkpoints.
#define NVN_LAYER_PROCS_DEBUG(X)                                                                  \
    X(void, nvnSetExternalDebuggerEnabledNVX, (NVNboolean enabled))                               \
    X(NVNdebugDomainId, nvnDeviceGenerateDebugDomainId,                                           \
      (const NVNdevice *device, const char *name))                                                \
    X(void, nvnDeviceInstallDebugCallback,                                                        \
      (NVNdevice * device, PFNNVNDEBUGCALLBACKPROC callback, void *data, NVNboolean enable))      \
    X(NVNboolean, nvnDeviceIsExternalDebuggerAttached, (const NVNdevice *device))                 \
    X(void, nvnDeviceWalkDebugDatabase,                                                           \
      (const NVNdevice *device, NVNdebugObjectType type,                                          \
       PFNNVNWALKDEBUGDATABASECALLBACKPROC callback, void *data))                                 \
    X(NVNboolean, nvnDeviceSetAutomaticCheckpointsByDebugGroup,                                   \
      (NVNdevice * device, NVNboolean value))                                                     \
    X(NVNboolean, nvnDeviceSetAutomaticCheckpointsEnable, (NVNdevice * device, NVNboolean value)) \
    X(NVNboolean, nvnDeviceSetAutomaticCheckpointsMask, (NVNdevice * device, uint32_t value))     \
    X(NVNboolean, nvnDeviceSetAutomaticCheckpointsSamplingInterval,                               \
      (NVNdevice * device, uint32_t value))                                                       \
    X(NVNboolean, nvnDeviceSetCheckpointCallStacksEnable, (NVNdevice * device, NVNboolean value)) \
    X(NVNboolean, nvnDeviceSetCheckpointMaxCallStacksDepth, (NVNdevice * device, int value))      \
    X(void, nvnDeviceSetDebugLabel, (NVNdevice * object, const char *label))                      \
    X(void, nvnQueueSetDebugLabel, (NVNqueue * object, const char *label))                        \
    X(void, nvnMemoryPoolSetDebugLabel, (NVNmemoryPool * object, const char *label))              \
    X(void, nvnTextureSetDebugLabel, (NVNtexture * object, const char *label))                    \
    X(void, nvnCommandBufferSetDebugLabel, (NVNcommandBuffer * object, const char *label))        \
    X(void, nvnWindowSetDebugLabel, (NVNwindow * object, const char *label))                      \
    X(void, nvnBufferSetDebugLabel, (NVNbuffer * object, const char *label))                      \
    X(void, nvnSamplerSetDebugLabel, (NVNsampler * object, const char *label))                    \
    X(void, nvnProgramSetDebugLabel, (NVNprogram * object, const char *label))                    \
    X(void, nvnSyncSetDebugLabel, (NVNsync * object, const char *label))                          \
    X(void, nvnTexturePoolSetDebugLabel, (NVNtexturePool * object, const char *label))            \
    X(void, nvnSamplerPoolSetDebugLabel, (NVNsamplerPool * object, const char *label))            \
    X(uint64_t, nvnBufferGetDebugID, (const NVNbuffer *object))                                   \
    X(uint64_t, nvnTextureGetDebugID, (const NVNtexture *object))                                 \
    X(uint64_t, nvnSamplerGetDebugID, (const NVNsampler *object))                                 \
    X(void, nvnCommandBufferInsertDebugMarker, (NVNcommandBuffer * cmd, const char *text))        \
    X(void, nvnCommandBufferInsertDebugMarkerDynamic,                                             \
      (NVNcommandBuffer * cmd, NVNdebugDomainId domain, const char *text))                        \
    X(void, nvnCommandBufferInsertDebugMarkerStatic,                                              \
      (NVNcommandBuffer * cmd, NVNdebugDomainId domain, const char *text))                        \
    X(void, nvnCommandBufferPushDebugGroup,                                                       \
      (NVNcommandBuffer * cmd, NVNdebugDomainId domain, const char *text))                        \
    X(void, nvnCommandBufferPushDebugGroupDynamic,                                                \
      (NVNcommandBuffer * cmd, NVNdebugDomainId domain, const char *text))                        \
    X(void, nvnCommandBufferPushDebugGroupStatic,                                                 \
      (NVNcommandBuffer * cmd, NVNdebugDomainId domain, const char *text))                        \
    X(void, nvnCommandBufferPopDebugGroup, (NVNcommandBuffer * cmd))                              \
    X(void, nvnCommandBufferPopDebugGroupId, (NVNcommandBuffer * cmd, NVNdebugDomainId domain))   \
    X(NVNboolean, nvnCommandBufferInsertCheckpointDynamic,                                        \
      (NVNcommandBuffer * cmd, const char *label))                                                \
    X(NVNboolean, nvnCommandBufferInsertCheckpointStatic,                                         \
      (NVNcommandBuffer * cmd, const char *label))

// The Ounce's markers and groups (API 231), which say whether they name graphics or compute work.
#define NVN_LAYER_PROCS_DEBUG_231(X)                                                              \
    X(void, nvnCommandBufferInsertDebugMarkerGraphics,                                            \
      (NVNcommandBuffer * cmd, NVNdebugDomainId domain, const char *text))                        \
    X(void, nvnCommandBufferInsertDebugMarkerCompute,                                             \
      (NVNcommandBuffer * cmd, NVNdebugDomainId domain, const char *text))                        \
    X(void, nvnCommandBufferPushDebugGroupGraphics,                                               \
      (NVNcommandBuffer * cmd, NVNdebugDomainId domain, const char *text))                        \
    X(void, nvnCommandBufferPushDebugGroupCompute,                                                \
      (NVNcommandBuffer * cmd, NVNdebugDomainId domain, const char *text))                        \
    X(void, nvnCommandBufferPopDebugGroupGraphics,                                                \
      (NVNcommandBuffer * cmd, NVNdebugDomainId domain))                                          \
    X(void, nvnCommandBufferPopDebugGroupCompute,                                                 \
      (NVNcommandBuffer * cmd, NVNdebugDomainId domain))

// What a device and its builder answer of themselves, the clear values registered with a
// device, a program set from NVN 55's longer shader data, and the handles of a texture and of
// a sampler apart and of a texel fetch.
#define NVN_LAYER_PROCS_OBJECT(X)                                                                 \
    X(int, nvnDeviceBuilderGetFlags, (const NVNdeviceBuilder *builder))                           \
    X(NVNwindowOriginMode, nvnDeviceGetWindowOriginMode, (const NVNdevice *device))               \
    X(NVNdepthMode, nvnDeviceGetDepthMode, (const NVNdevice *device))                             \
    X(NVNboolean, nvnDeviceRegisterFastClearColor,                                                \
      (NVNdevice * device, const float *color, NVNformat format))                                 \
    X(NVNboolean, nvnDeviceRegisterFastClearColori,                                               \
      (NVNdevice * device, const int *color, NVNformat format))                                   \
    X(NVNboolean, nvnDeviceRegisterFastClearColorui,                                              \
      (NVNdevice * device, const uint32_t *color, NVNformat format))                              \
    X(NVNboolean, nvnDeviceRegisterFastClearDepth, (NVNdevice * device, float depth))             \
    X(NVNboolean, nvnDeviceRegisterFastClearStencil, (NVNdevice * device, int stencil))           \
    X(NVNboolean, nvnProgramSetShadersExt,                                                        \
      (NVNprogram * program, int count, const void *stages))                                      \
    X(uint64_t, nvnDeviceGetSeparateTextureHandle, (const NVNdevice *device, int id))             \
    X(uint64_t, nvnDeviceGetSeparateSamplerHandle, (const NVNdevice *device, int id))             \
    X(uint64_t, nvnDeviceGetTexelFetchHandle, (const NVNdevice *device, int id))

// What the Ounce's program builder (API 231) answers of itself: its device, its shader data,
// its sample shading, and the three numbers it keeps for a ray tracing program.
#define NVN_LAYER_PROCS_PROGRAM_231(X)                                                            \
    X(NVNdevice *, nvnProgramBuilderGetDevice, (const NVNprogramBuilder *builder))                \
    X(const NVNshaderData *, nvnProgramBuilderGetShaders, (const NVNprogramBuilder *builder))     \
    X(int, nvnProgramBuilderGetShaderCount, (const NVNprogramBuilder *builder))                   \
    X(void, nvnProgramBuilderSetSampleShading, (NVNprogramBuilder * builder, int mode))           \
    X(int, nvnProgramBuilderGetSampleShading, (const NVNprogramBuilder *builder))                 \
    X(void, nvnProgramBuilderSetMaxCallableRecursionDepth,                                        \
      (NVNprogramBuilder * builder, int depth))                                                   \
    X(int, nvnProgramBuilderGetMaxCallableRecursionDepth, (const NVNprogramBuilder *builder))     \
    X(void, nvnProgramBuilderSetMaxTraceRecursionDepth, (NVNprogramBuilder * builder, int depth)) \
    X(int, nvnProgramBuilderGetMaxTraceRecursionDepth, (const NVNprogramBuilder *builder))        \
    X(void, nvnProgramBuilderSetProgramRayTracingFlags, (NVNprogramBuilder * builder, int flags)) \
    X(int, nvnProgramBuilderGetProgramRayTracingFlags, (const NVNprogramBuilder *builder))

// The rest of the state objects: every setter, and what each state says of itself.
#define NVN_LAYER_PROCS_STATE_OBJECTS(X)                                                          \
    X(void, nvnBlendStateSetBlendTarget, (NVNblendState * state, int target))                     \
    X(void, nvnBlendStateSetBlendFunc,                                                            \
      (NVNblendState * state, NVNblendFunc src_rgb, NVNblendFunc dst_rgb, NVNblendFunc src_alpha, \
       NVNblendFunc dst_alpha))                                                                   \
    X(void, nvnBlendStateSetBlendEquation,                                                        \
      (NVNblendState * state, NVNblendEquation rgb, NVNblendEquation alpha))                      \
    X(void, nvnBlendStateSetAdvancedMode, (NVNblendState * state, NVNblendAdvancedMode mode))     \
    X(void, nvnBlendStateSetAdvancedOverlap,                                                      \
      (NVNblendState * state, NVNblendAdvancedOverlap overlap))                                   \
    X(void, nvnBlendStateSetAdvancedPremultipliedSrc, (NVNblendState * state, NVNboolean on))     \
    X(void, nvnBlendStateSetAdvancedNormalizedDst, (NVNblendState * state, NVNboolean on))        \
    X(int, nvnBlendStateGetBlendTarget, (const NVNblendState *state))                             \
    X(void, nvnBlendStateGetBlendFunc,                                                            \
      (const NVNblendState *state, NVNblendFunc *src_rgb, NVNblendFunc *dst_rgb,                  \
       NVNblendFunc *src_alpha, NVNblendFunc *dst_alpha))                                         \
    X(void, nvnBlendStateGetBlendEquation,                                                        \
      (const NVNblendState *state, NVNblendEquation *rgb, NVNblendEquation *alpha))               \
    X(NVNblendAdvancedMode, nvnBlendStateGetAdvancedMode, (const NVNblendState *state))           \
    X(NVNblendAdvancedOverlap, nvnBlendStateGetAdvancedOverlap, (const NVNblendState *state))     \
    X(NVNboolean, nvnBlendStateGetAdvancedPremultipliedSrc, (const NVNblendState *state))         \
    X(NVNboolean, nvnBlendStateGetAdvancedNormalizedDst, (const NVNblendState *state))            \
    X(void, nvnChannelMaskStateSetChannelMask,                                                    \
      (NVNchannelMaskState * state, int target, NVNboolean r, NVNboolean g, NVNboolean b,         \
       NVNboolean a))                                                                             \
    X(void, nvnChannelMaskStateGetChannelMask,                                                    \
      (const NVNchannelMaskState *state, int target, NVNboolean *r, NVNboolean *g, NVNboolean *b, \
       NVNboolean *a))                                                                            \
    X(void, nvnColorStateSetBlendEnable, (NVNcolorState * state, int target, NVNboolean enable))  \
    X(void, nvnColorStateSetLogicOp, (NVNcolorState * state, NVNlogicOp op))                      \
    X(void, nvnColorStateSetAlphaTest, (NVNcolorState * state, NVNalphaFunc func))                \
    X(NVNboolean, nvnColorStateGetBlendEnable, (const NVNcolorState *state, int target))          \
    X(NVNlogicOp, nvnColorStateGetLogicOp, (const NVNcolorState *state))                          \
    X(NVNalphaFunc, nvnColorStateGetAlphaTest, (const NVNcolorState *state))                      \
    X(void, nvnDepthStencilStateSetStencilTestEnable,                                             \
      (NVNdepthStencilState * state, NVNboolean enable))                                          \
    X(void, nvnDepthStencilStateSetStencilFunc,                                                   \
      (NVNdepthStencilState * state, NVNface faces, NVNstencilFunc func))                         \
    X(void, nvnDepthStencilStateSetStencilOp,                                                     \
      (NVNdepthStencilState * state, NVNface faces, NVNstencilOp fail, NVNstencilOp depth_fail,   \
       NVNstencilOp pass))                                                                        \
    X(NVNboolean, nvnDepthStencilStateGetDepthTestEnable, (const NVNdepthStencilState *state))    \
    X(NVNboolean, nvnDepthStencilStateGetDepthWriteEnable, (const NVNdepthStencilState *state))   \
    X(NVNdepthFunc, nvnDepthStencilStateGetDepthFunc, (const NVNdepthStencilState *state))        \
    X(NVNboolean, nvnDepthStencilStateGetStencilTestEnable, (const NVNdepthStencilState *state))  \
    X(NVNstencilFunc, nvnDepthStencilStateGetStencilFunc,                                         \
      (const NVNdepthStencilState *state, NVNface face))                                          \
    X(void, nvnDepthStencilStateGetStencilOp,                                                     \
      (const NVNdepthStencilState *state, NVNface face, NVNstencilOp *fail,                       \
       NVNstencilOp *depth_fail, NVNstencilOp *pass))                                             \
    X(void, nvnMultisampleStateSetMultisampleEnable,                                              \
      (NVNmultisampleState * state, NVNboolean enable))                                           \
    X(void, nvnMultisampleStateSetSamples, (NVNmultisampleState * state, int samples))            \
    X(void, nvnMultisampleStateSetRasterSamples, (NVNmultisampleState * state, int samples))      \
    X(void, nvnMultisampleStateSetAlphaToCoverageEnable,                                          \
      (NVNmultisampleState * state, NVNboolean enable))                                           \
    X(void, nvnMultisampleStateSetAlphaToCoverageDither,                                          \
      (NVNmultisampleState * state, NVNboolean on))                                               \
    X(void, nvnMultisampleStateSetCoverageToColorEnable,                                          \
      (NVNmultisampleState * state, NVNboolean on))                                               \
    X(void, nvnMultisampleStateSetCoverageToColorOutput,                                          \
      (NVNmultisampleState * state, int output))                                                  \
    X(void, nvnMultisampleStateSetCoverageModulationMode,                                         \
      (NVNmultisampleState * state, NVNcoverageModulationMode mode))                              \
    X(void, nvnMultisampleStateSetCoverageTruncationEnable,                                       \
      (NVNmultisampleState * state, NVNboolean on))                                               \
    X(void, nvnMultisampleStateSetSampleLocationsEnable,                                          \
      (NVNmultisampleState * state, NVNboolean on))                                               \
    X(void, nvnMultisampleStateSetSampleLocationsGridEnable,                                      \
      (NVNmultisampleState * state, NVNboolean on))                                               \
    X(void, nvnMultisampleStateSetSampleLocations,                                                \
      (NVNmultisampleState * state, int first, int count, const float *xy))                       \
    X(NVNboolean, nvnMultisampleStateGetMultisampleEnable, (const NVNmultisampleState *state))    \
    X(int, nvnMultisampleStateGetSamples, (const NVNmultisampleState *state))                     \
    X(int, nvnMultisampleStateGetRasterSamples, (const NVNmultisampleState *state))               \
    X(NVNboolean, nvnMultisampleStateGetAlphaToCoverageEnable,                                    \
      (const NVNmultisampleState *state))                                                         \
    X(NVNboolean, nvnMultisampleStateGetAlphaToCoverageDither,                                    \
      (const NVNmultisampleState *state))                                                         \
    X(NVNboolean, nvnMultisampleStateGetCoverageToColorEnable,                                    \
      (const NVNmultisampleState *state))                                                         \
    X(int, nvnMultisampleStateGetCoverageToColorOutput, (const NVNmultisampleState *state))       \
    X(NVNcoverageModulationMode, nvnMultisampleStateGetCoverageModulationMode,                    \
      (const NVNmultisampleState *state))                                                         \
    X(NVNboolean, nvnMultisampleStateGetCoverageTruncationEnable,                                 \
      (const NVNmultisampleState *state))                                                         \
    X(NVNboolean, nvnMultisampleStateGetSampleLocationsEnable,                                    \
      (const NVNmultisampleState *state))                                                         \
    X(NVNboolean, nvnMultisampleStateGetSampleLocationsGridEnable,                                \
      (const NVNmultisampleState *state))                                                         \
    X(void, nvnMultisampleStateGetSampleLocationsGrid,                                            \
      (NVNmultisampleState * state, int *width, int *height))                                     \
    X(void, nvnPolygonStateSetPolygonMode, (NVNpolygonState * state, NVNpolygonMode mode))        \
    X(void, nvnPolygonStateSetPolygonOffsetEnables, (NVNpolygonState * state, int enables))       \
    X(NVNface, nvnPolygonStateGetCullFace, (const NVNpolygonState *state))                        \
    X(NVNfrontFace, nvnPolygonStateGetFrontFace, (const NVNpolygonState *state))                  \
    X(NVNpolygonMode, nvnPolygonStateGetPolygonMode, (const NVNpolygonState *state))              \
    X(NVNpolygonOffsetEnable, nvnPolygonStateGetPolygonOffsetEnables,                             \
      (const NVNpolygonState *state))                                                             \
    X(void, nvnVertexAttribStateGetFormat,                                                        \
      (const NVNvertexAttribState *state, NVNformat *format, ptrdiff_t *offset))                  \
    X(int, nvnVertexAttribStateGetStreamIndex, (const NVNvertexAttribState *state))               \
    X(void, nvnVertexStreamStateSetDivisor, (NVNvertexStreamState * state, int divisor))          \
    X(ptrdiff_t, nvnVertexStreamStateGetStride, (const NVNvertexStreamState *state))              \
    X(int, nvnVertexStreamStateGetDivisor, (const NVNvertexStreamState *state))

// The rest of what a texture, its builder and a view say and take, and texels moved by the
// processor.
#define NVN_LAYER_PROCS_TEXTURE_MORE(X)                                                           \
    X(void, nvnTextureBuilderSetSwizzle,                                                          \
      (NVNtextureBuilder * builder, NVNtextureSwizzle r, NVNtextureSwizzle g,                     \
       NVNtextureSwizzle b, NVNtextureSwizzle a))                                                 \
    X(void, nvnTextureBuilderSetDepthStencilMode,                                                 \
      (NVNtextureBuilder * builder, NVNtextureDepthStencilMode mode))                             \
    X(void, nvnTextureBuilderSetStride, (NVNtextureBuilder * builder, ptrdiff_t stride))          \
    X(void, nvnTextureBuilderSetGLTextureName, (NVNtextureBuilder * builder, uint32_t name))      \
    X(void, nvnTextureBuilderSetPackagedTextureData,                                              \
      (NVNtextureBuilder * builder, const void *data))                                            \
    X(void, nvnTextureBuilderSetPackagedTextureLayout,                                            \
      (NVNtextureBuilder * builder, const NVNpackagedTextureLayout *layout))                      \
    X(void, nvnTextureBuilderGetSwizzle,                                                          \
      (const NVNtextureBuilder *builder, NVNtextureSwizzle *r, NVNtextureSwizzle *g,              \
       NVNtextureSwizzle *b, NVNtextureSwizzle *a))                                               \
    X(NVNtextureDepthStencilMode, nvnTextureBuilderGetDepthStencilMode,                           \
      (const NVNtextureBuilder *builder))                                                         \
    X(ptrdiff_t, nvnTextureBuilderGetStride, (const NVNtextureBuilder *builder))                  \
    X(uint32_t, nvnTextureBuilderGetGLTextureName, (const NVNtextureBuilder *builder))            \
    X(const void *, nvnTextureBuilderGetPackagedTextureData, (const NVNtextureBuilder *builder))  \
    X(NVNboolean, nvnTextureBuilderGetPackagedTextureLayout,                                      \
      (const NVNtextureBuilder *builder, NVNpackagedTextureLayout *layout))                       \
    X(int, nvnTextureBuilderGetRawStorageClass, (const NVNtextureBuilder *builder))               \
    X(int, nvnTextureBuilderGetStorageClass, (const NVNtextureBuilder *builder))                  \
    X(size_t, nvnTextureBuilderGetZCullStorageSize, (const NVNtextureBuilder *builder))           \
    X(void, nvnTextureBuilderGetSparseTileLayout,                                                 \
      (const NVNtextureBuilder *builder, NVNtextureSparseTileLayout *layout))                     \
    X(ptrdiff_t, nvnTextureBuilderGetViewOffset,                                                  \
      (const NVNtextureBuilder *builder, const NVNtextureView *view))                             \
    X(int, nvnTextureGetStorageSize, (const NVNtexture *texture))                                 \
    X(NVNtextureDepthStencilMode, nvnTextureGetDepthStencilMode, (const NVNtexture *texture))     \
    X(void, nvnTextureGetSwizzle,                                                                 \
      (const NVNtexture *texture, NVNtextureSwizzle *r, NVNtextureSwizzle *g,                     \
       NVNtextureSwizzle *b, NVNtextureSwizzle *a))                                               \
    X(NVNbufferAddress, nvnTextureGetTextureAddress, (const NVNtexture *texture))                 \
    X(NVNboolean, nvnTextureCompare, (const NVNtexture *a, const NVNtexture *b))                  \
    X(ptrdiff_t, nvnTextureGetStride, (const NVNtexture *texture))                                \
    X(int, nvnTextureGetRawStorageClass, (const NVNtexture *texture))                             \
    X(int, nvnTextureGetStorageClass, (const NVNtexture *texture))                                \
    X(size_t, nvnTextureGetZCullStorageSize, (const NVNtexture *texture))                         \
    X(void, nvnTextureGetSparseTileLayout,                                                        \
      (const NVNtexture *texture, NVNtextureSparseTileLayout *layout))                            \
    X(ptrdiff_t, nvnTextureGetViewOffset,                                                         \
      (const NVNtexture *texture, const NVNtextureView *view))                                    \
    X(void, nvnTextureGetNvRmSurfaceNVX,                                                          \
      (const NVNtexture *texture, void *unused, void *surface))                                   \
    X(void, nvnTextureViewSetSwizzle,                                                             \
      (NVNtextureView * view, NVNtextureSwizzle r, NVNtextureSwizzle g, NVNtextureSwizzle b,      \
       NVNtextureSwizzle a))                                                                      \
    X(NVNboolean, nvnTextureViewGetSwizzle,                                                       \
      (const NVNtextureView *view, NVNtextureSwizzle *r, NVNtextureSwizzle *g,                    \
       NVNtextureSwizzle *b, NVNtextureSwizzle *a))                                               \
    X(void, nvnTextureViewSetDepthStencilMode,                                                    \
      (NVNtextureView * view, NVNtextureDepthStencilMode mode))                                   \
    X(NVNboolean, nvnTextureViewGetDepthStencilMode,                                              \
      (const NVNtextureView *view, NVNtextureDepthStencilMode *mode))                             \
    X(void, nvnTextureWriteTexels,                                                                \
      (const NVNtexture *texture, const NVNtextureView *view, const NVNcopyRegion *region,        \
       const void *source))                                                                       \
    X(void, nvnTextureWriteTexelsStrided,                                                         \
      (const NVNtexture *texture, const NVNtextureView *view, const NVNcopyRegion *region,        \
       const void *source, ptrdiff_t row, ptrdiff_t image))                                       \
    X(void, nvnTextureReadTexels,                                                                 \
      (const NVNtexture *texture, const NVNtextureView *view, const NVNcopyRegion *region,        \
       void *destination))                                                                        \
    X(void, nvnTextureReadTexelsStrided,                                                          \
      (const NVNtexture *texture, const NVNtextureView *view, const NVNcopyRegion *region,        \
       void *destination, ptrdiff_t row, ptrdiff_t image))                                        \
    X(void, nvnTextureFlushTexels,                                                                \
      (const NVNtexture *texture, const NVNtextureView *view, const NVNcopyRegion *region))       \
    X(void, nvnTextureInvalidateTexels,                                                           \
      (const NVNtexture *texture, const NVNtextureView *view, const NVNcopyRegion *region))

// The other texture sizes, and the rest of a queue and its builder.
#define NVN_LAYER_PROCS_REST(X)                                                                   \
    X(void, nvnTextureBuilderSetSize1D, (NVNtextureBuilder * builder, int width))                 \
    X(void, nvnTextureBuilderSetSize3D,                                                           \
      (NVNtextureBuilder * builder, int width, int height, int depth))                            \
    X(void, nvnQueueBuilderSetQueueMemory,                                                        \
      (NVNqueueBuilder * builder, void *memory, size_t size))                                     \
    X(void *, nvnQueueBuilderGetMemory, (const NVNqueueBuilder *builder))                         \
    X(size_t, nvnQueueBuilderGetMemorySize, (const NVNqueueBuilder *builder))                     \
    X(size_t, nvnQueueBuilderGetQueueMemorySize, (const NVNqueueBuilder *builder))                \
    X(void, nvnQueueBuilderSetQueuePriority, (NVNqueueBuilder * builder, int priority))           \
    X(int, nvnQueueBuilderGetQueuePriority, (const NVNqueueBuilder *builder))                     \
    X(void, nvnQueueBuilderSetPrimaryQueue, (NVNqueueBuilder * builder, NVNqueue *primary))       \
    X(size_t, nvnQueueGetTotalCommandMemoryUsed, (NVNqueue * queue))                              \
    X(size_t, nvnQueueGetTotalControlMemoryUsed, (NVNqueue * queue))                              \
    X(size_t, nvnQueueGetTotalComputeMemoryUsed, (NVNqueue * queue))                              \
    X(void, nvnQueueResetMemoryUsageCounts, (NVNqueue * queue))

// The objects, the queue, the copies and the window.
#define NVN_LAYER_PROCS_BASE(X)                                                                   \
    X(void, nvnDeviceBuilderSetDefaults, (NVNdeviceBuilder * builder))                            \
    X(void, nvnDeviceBuilderSetFlags, (NVNdeviceBuilder * builder, int flags))                    \
    X(NVNboolean, nvnDeviceInitialize, (NVNdevice * device, const NVNdeviceBuilder *builder))    \
    X(void, nvnDeviceFinalize, (NVNdevice * device))                                              \
    X(void, nvnQueueBuilderSetDefaults, (NVNqueueBuilder * builder))                              \
    X(void, nvnQueueBuilderSetDevice, (NVNqueueBuilder * builder, NVNdevice *device))             \
    X(NVNboolean, nvnQueueInitialize, (NVNqueue * queue, const NVNqueueBuilder *builder))         \
    X(void, nvnQueueFinalize, (NVNqueue * queue))                                                 \
    X(void, nvnQueueSubmitCommands,                                                               \
      (NVNqueue * queue, int count, const NVNcommandHandle *handles))                            \
    X(void, nvnQueueFlush, (NVNqueue * queue))                                                    \
    X(void, nvnQueueFinish, (NVNqueue * queue))                                                   \
    X(NVNqueueAcquireTextureResult, nvnQueueAcquireTexture,                                       \
      (NVNqueue * queue, NVNwindow *window, int *index))                                          \
    X(void, nvnQueuePresentTexture, (NVNqueue * queue, NVNwindow *window, int index))             \
    X(void, nvnMemoryPoolBuilderSetDefaults, (NVNmemoryPoolBuilder * builder))                    \
    X(void, nvnMemoryPoolBuilderSetDevice, (NVNmemoryPoolBuilder * builder, NVNdevice *device))   \
    X(void, nvnMemoryPoolBuilderSetStorage,                                                       \
      (NVNmemoryPoolBuilder * builder, void *memory, size_t size))                                \
    X(void, nvnMemoryPoolBuilderSetFlags, (NVNmemoryPoolBuilder * builder, int flags))            \
    X(NVNboolean, nvnMemoryPoolInitialize,                                                        \
      (NVNmemoryPool * pool, const NVNmemoryPoolBuilder *builder))                                \
    X(void, nvnMemoryPoolFinalize, (NVNmemoryPool * pool))                                        \
    X(void *, nvnMemoryPoolMap, (const NVNmemoryPool *pool))                                      \
    X(NVNbufferAddress, nvnMemoryPoolGetBufferAddress, (const NVNmemoryPool *pool))               \
    X(void, nvnMemoryPoolFlushMappedRange,                                                        \
      (const NVNmemoryPool *pool, ptrdiff_t offset, size_t size))                                 \
    X(void, nvnTextureBuilderSetDefaults, (NVNtextureBuilder * builder))                          \
    X(void, nvnTextureBuilderSetDevice, (NVNtextureBuilder * builder, NVNdevice *device))         \
    X(void, nvnTextureBuilderSetFlags, (NVNtextureBuilder * builder, int flags))                  \
    X(void, nvnTextureBuilderSetTarget, (NVNtextureBuilder * builder, NVNtextureTarget target))   \
    X(void, nvnTextureBuilderSetFormat, (NVNtextureBuilder * builder, NVNformat format))          \
    X(void, nvnTextureBuilderSetSize2D, (NVNtextureBuilder * builder, int width, int height))     \
    X(void, nvnTextureBuilderSetStorage,                                                          \
      (NVNtextureBuilder * builder, NVNmemoryPool *pool, ptrdiff_t offset))                       \
    X(size_t, nvnTextureBuilderGetStorageSize, (const NVNtextureBuilder *builder))                \
    X(size_t, nvnTextureBuilderGetStorageAlignment, (const NVNtextureBuilder *builder))           \
    X(NVNboolean, nvnTextureInitialize, (NVNtexture * texture, const NVNtextureBuilder *builder)) \
    X(void, nvnTextureFinalize, (NVNtexture * texture))                                           \
    X(NVNboolean, nvnCommandBufferInitialize, (NVNcommandBuffer * cmd, NVNdevice *device))        \
    X(void, nvnCommandBufferFinalize, (NVNcommandBuffer * cmd))                                   \
    X(void, nvnCommandBufferAddCommandMemory,                                                     \
      (NVNcommandBuffer * cmd, const NVNmemoryPool *pool, ptrdiff_t offset, size_t size))         \
    X(void, nvnCommandBufferAddControlMemory,                                                     \
      (NVNcommandBuffer * cmd, void *memory, size_t size))                                        \
    X(void, nvnCommandBufferBeginRecording, (NVNcommandBuffer * cmd))                             \
    X(NVNcommandHandle, nvnCommandBufferEndRecording, (NVNcommandBuffer * cmd))                   \
    X(void, nvnCommandBufferSetRenderTargets,                                                     \
      (NVNcommandBuffer * cmd, int count, const NVNtexture *const *colors,                       \
       const NVNtextureView *const *views, const NVNtexture *depth,                               \
       const NVNtextureView *depth_view))                                                         \
    X(void, nvnCommandBufferSetViewport,                                                          \
      (NVNcommandBuffer * cmd, int x, int y, int width, int height))                              \
    X(void, nvnCommandBufferSetScissor,                                                           \
      (NVNcommandBuffer * cmd, int x, int y, int width, int height))                              \
    X(void, nvnCommandBufferClearColor,                                                           \
      (NVNcommandBuffer * cmd, int index, const float *rgba, int mask))                           \
    X(void, nvnCommandBufferBarrier, (NVNcommandBuffer * cmd, int bits))                          \
    X(void, nvnCommandBufferCopyTextureToBuffer,                                                  \
      (NVNcommandBuffer * cmd, const NVNtexture *texture, const NVNtextureView *view,             \
       const NVNcopyRegion *region, NVNbufferAddress buffer, int flags))                          \
    X(void, nvnCommandBufferCopyBufferToTexture,                                                  \
      (NVNcommandBuffer * cmd, NVNbufferAddress buffer, const NVNtexture *texture,                \
       const NVNtextureView *view, const NVNcopyRegion *region, int flags))                       \
    X(void, nvnCommandBufferCopyTextureToTexture,                                                 \
      (NVNcommandBuffer * cmd, const NVNtexture *source, const NVNtextureView *source_view,       \
       const NVNcopyRegion *source_region, const NVNtexture *destination,                         \
       const NVNtextureView *destination_view, const NVNcopyRegion *destination_region,           \
       int flags))                                                                                \
    X(void, nvnCommandBufferCopyBufferToBuffer,                                                   \
      (NVNcommandBuffer * cmd, NVNbufferAddress source, NVNbufferAddress destination,             \
       size_t size, int flags))                                                                   \
    X(void, nvnCommandBufferSetCopyRowStride, (NVNcommandBuffer * cmd, ptrdiff_t stride))         \
    X(void, nvnWindowBuilderSetDefaults, (NVNwindowBuilder * builder))                            \
    X(void, nvnWindowBuilderSetDevice, (NVNwindowBuilder * builder, NVNdevice *device))           \
    X(void, nvnWindowBuilderSetNativeWindow,                                                      \
      (NVNwindowBuilder * builder, NVNnativeWindow native))                                       \
    X(void, nvnWindowBuilderSetTextures,                                                          \
      (NVNwindowBuilder * builder, int count, NVNtexture *const *textures))                       \
    X(NVNboolean, nvnWindowInitialize, (NVNwindow * window, const NVNwindowBuilder *builder))     \
    X(void, nvnWindowFinalize, (NVNwindow * window))                                              \
    NVN_LAYER_PROCS_TEXTURE(X)                                                                    \
    NVN_LAYER_PROCS_POOL(X)                                                                       \
    NVN_LAYER_PROCS_SYNC(X)                                                                       \
    NVN_LAYER_PROCS_SAMPLER(X)                                                                    \
    NVN_LAYER_PROCS_BUFFER(X)                                                                     \
    NVN_LAYER_PROCS_WINDOW(X)                                                                     \
    NVN_LAYER_PROCS_COUNTER(X)                                                                    \
    NVN_LAYER_PROCS_RECORDING(X)                                                                  \
    NVN_LAYER_PROCS_CLEAR(X)                                                                      \
    NVN_LAYER_PROCS_STATE(X)                                                                      \
    NVN_LAYER_PROCS_DEBUG(X)                                                                      \
    NVN_LAYER_PROCS_OBJECT(X)                                                                     \
    NVN_LAYER_PROCS_STATE_OBJECTS(X)                                                              \
    NVN_LAYER_PROCS_TEXTURE_MORE(X)                                                               \
    NVN_LAYER_PROCS_REST(X)

// What a draw takes: programs, samplers, the texture and sampler pools, the state objects, the
// binds and the draw.
#define NVN_LAYER_PROCS_DRAW(X)                                                                   \
    X(void, nvnDeviceSetWindowOriginMode, (NVNdevice *device, NVNwindowOriginMode mode))          \
    X(void, nvnDeviceSetDepthMode, (NVNdevice *device, NVNdepthMode mode))                        \
    X(NVNtextureHandle, nvnDeviceGetTextureHandle,                                                \
      (const NVNdevice *device, int texture, int sampler))                                        \
    X(NVNboolean, nvnProgramInitialize, (NVNprogram *program, NVNdevice *device))                 \
    X(void, nvnProgramFinalize, (NVNprogram *program))                                            \
    X(NVNboolean, nvnProgramSetShaders,                                                           \
      (NVNprogram *program, int count, const NVNshaderData *shaders))                             \
    X(void, nvnSamplerBuilderSetDefaults, (NVNsamplerBuilder *builder))                           \
    X(void, nvnSamplerBuilderSetDevice, (NVNsamplerBuilder *builder, NVNdevice *device))          \
    X(void, nvnSamplerBuilderSetMinMagFilter,                                                     \
      (NVNsamplerBuilder *builder, NVNminFilter min, NVNmagFilter mag))                           \
    X(void, nvnSamplerBuilderSetWrapMode,                                                         \
      (NVNsamplerBuilder *builder, NVNwrapMode s, NVNwrapMode t, NVNwrapMode r))                  \
    X(NVNboolean, nvnSamplerInitialize, (NVNsampler *sampler, const NVNsamplerBuilder *builder))  \
    X(void, nvnSamplerFinalize, (NVNsampler *sampler))                                            \
    X(NVNboolean, nvnTexturePoolInitialize,                                                       \
      (NVNtexturePool *pool, const NVNmemoryPool *memory, ptrdiff_t offset, int count))           \
    X(void, nvnTexturePoolFinalize, (NVNtexturePool *pool))                                       \
    X(void, nvnTexturePoolRegisterTexture,                                                        \
      (const NVNtexturePool *pool, int id, const NVNtexture *texture,                             \
       const NVNtextureView *view))                                                               \
    X(NVNboolean, nvnSamplerPoolInitialize,                                                       \
      (NVNsamplerPool *pool, const NVNmemoryPool *memory, ptrdiff_t offset, int count))           \
    X(void, nvnSamplerPoolFinalize, (NVNsamplerPool *pool))                                       \
    X(void, nvnSamplerPoolRegisterSampler,                                                        \
      (const NVNsamplerPool *pool, int id, const NVNsampler *sampler))                            \
    X(void, nvnBlendStateSetDefaults, (NVNblendState *state))                                     \
    X(void, nvnChannelMaskStateSetDefaults, (NVNchannelMaskState *state))                         \
    X(void, nvnColorStateSetDefaults, (NVNcolorState *state))                                     \
    X(void, nvnDepthStencilStateSetDefaults, (NVNdepthStencilState *state))                       \
    X(void, nvnDepthStencilStateSetDepthTestEnable,                                               \
      (NVNdepthStencilState *state, NVNboolean enable))                                           \
    X(void, nvnDepthStencilStateSetDepthWriteEnable,                                              \
      (NVNdepthStencilState *state, NVNboolean enable))                                           \
    X(void, nvnDepthStencilStateSetDepthFunc, (NVNdepthStencilState *state, NVNdepthFunc func))   \
    X(void, nvnMultisampleStateSetDefaults, (NVNmultisampleState *state))                         \
    X(void, nvnPolygonStateSetDefaults, (NVNpolygonState *state))                                 \
    X(void, nvnPolygonStateSetCullFace, (NVNpolygonState *state, NVNface face))                   \
    X(void, nvnPolygonStateSetFrontFace, (NVNpolygonState *state, NVNfrontFace face))             \
    X(void, nvnVertexAttribStateSetDefaults, (NVNvertexAttribState *state))                       \
    X(void, nvnVertexAttribStateSetFormat,                                                        \
      (NVNvertexAttribState *state, NVNformat format, ptrdiff_t offset))                          \
    X(void, nvnVertexAttribStateSetStreamIndex, (NVNvertexAttribState *state, int index))         \
    X(void, nvnVertexStreamStateSetDefaults, (NVNvertexStreamState *state))                       \
    X(void, nvnVertexStreamStateSetStride, (NVNvertexStreamState *state, ptrdiff_t stride))       \
    X(void, nvnCommandBufferBindProgram,                                                          \
      (NVNcommandBuffer *cmd, const NVNprogram *program, int stages))                             \
    X(void, nvnCommandBufferBindVertexAttribState,                                                \
      (NVNcommandBuffer *cmd, int count, const NVNvertexAttribState *states))                     \
    X(void, nvnCommandBufferBindVertexStreamState,                                                \
      (NVNcommandBuffer *cmd, int count, const NVNvertexStreamState *states))                     \
    X(void, nvnCommandBufferBindVertexBuffer,                                                     \
      (NVNcommandBuffer *cmd, int index, NVNbufferAddress address, size_t size))                  \
    X(void, nvnCommandBufferBindUniformBuffer,                                                    \
      (NVNcommandBuffer *cmd, NVNshaderStage stage, int index, NVNbufferAddress address,          \
       size_t size))                                                                              \
    X(void, nvnCommandBufferBindTexture,                                                          \
      (NVNcommandBuffer *cmd, NVNshaderStage stage, int index, NVNtextureHandle handle))          \
    X(void, nvnCommandBufferSetTexturePool, (NVNcommandBuffer *cmd, const NVNtexturePool *pool))  \
    X(void, nvnCommandBufferSetSamplerPool, (NVNcommandBuffer *cmd, const NVNsamplerPool *pool))  \
    X(void, nvnCommandBufferBindBlendState, (NVNcommandBuffer *cmd, const NVNblendState *state))  \
    X(void, nvnCommandBufferBindChannelMaskState,                                                 \
      (NVNcommandBuffer *cmd, const NVNchannelMaskState *state))                                  \
    X(void, nvnCommandBufferBindColorState, (NVNcommandBuffer *cmd, const NVNcolorState *state))  \
    X(void, nvnCommandBufferBindDepthStencilState,                                                \
      (NVNcommandBuffer *cmd, const NVNdepthStencilState *state))                                 \
    X(void, nvnCommandBufferBindMultisampleState,                                                 \
      (NVNcommandBuffer *cmd, const NVNmultisampleState *state))                                  \
    X(void, nvnCommandBufferBindPolygonState,                                                     \
      (NVNcommandBuffer *cmd, const NVNpolygonState *state))                                      \
    X(void, nvnCommandBufferDrawArrays,                                                           \
      (NVNcommandBuffer *cmd, NVNdrawPrimitive primitive, int first, int count))                  \
    X(void, nvnCommandBufferDrawElements,                                                         \
      (NVNcommandBuffer *cmd, NVNdrawPrimitive primitive, NVNindexType type, int count,           \
       NVNbufferAddress indices))                                                                 \
    X(void, nvnCommandBufferDrawElementsBaseVertex,                                               \
      (NVNcommandBuffer *cmd, NVNdrawPrimitive primitive, NVNindexType type, int count,           \
       NVNbufferAddress indices, int base_vertex))                                                \
    X(void, nvnCommandBufferDrawArraysInstanced,                                                  \
      (NVNcommandBuffer *cmd, NVNdrawPrimitive primitive, int first, int count,                   \
       int base_instance, int instances))                                                         \
    X(void, nvnCommandBufferDrawElementsInstanced,                                                \
      (NVNcommandBuffer *cmd, NVNdrawPrimitive primitive, NVNindexType type, int count,           \
       NVNbufferAddress indices, int base_vertex, int base_instance, int instances))              \
    X(void, nvnCommandBufferDrawArraysIndirect,                                                   \
      (NVNcommandBuffer *cmd, NVNdrawPrimitive primitive, NVNbufferAddress indirect))             \
    X(void, nvnCommandBufferDrawElementsIndirect,                                                 \
      (NVNcommandBuffer *cmd, NVNdrawPrimitive primitive, NVNindexType type,                      \
       NVNbufferAddress indices, NVNbufferAddress indirect))                                      \
    X(void, nvnCommandBufferBindVertexBuffers,                                                    \
      (NVNcommandBuffer *cmd, int first, int count, const NVNbufferRange *ranges))                \
    X(void, nvnCommandBufferBindUniformBuffers,                                                   \
      (NVNcommandBuffer *cmd, NVNshaderStage stage, int first, int count,                         \
       const NVNbufferRange *ranges))                                                             \
    X(void, nvnCommandBufferBindUniformBuffersDeferred,                                           \
      (NVNcommandBuffer *cmd, NVNshaderStage stage, int first, int count,                         \
       const NVNbufferRange *ranges))                                                             \
    X(void, nvnCommandBufferBindTextures,                                                         \
      (NVNcommandBuffer *cmd, NVNshaderStage stage, int first, int count,                         \
       const NVNtextureHandle *handles))                                                          \
    X(void, nvnCommandBufferBindTexturesDeferred,                                                 \
      (NVNcommandBuffer *cmd, NVNshaderStage stage, int first, int count,                         \
       const NVNtextureHandle *handles))                                                          \
    X(void, nvnCommandBufferUpdateUniformBuffer,                                                  \
      (NVNcommandBuffer *cmd, NVNbufferAddress buffer, size_t buffer_size, ptrdiff_t offset,      \
       size_t size, const void *data))                                                            \
    X(void, nvnCommandBufferBindCoverageModulationTable,                                          \
      (NVNcommandBuffer *cmd, const float *entries))                                              \
    X(void, nvnCommandBufferSetShaderScratchMemory,                                               \
      (NVNcommandBuffer *cmd, const NVNmemoryPool *pool, ptrdiff_t offset, size_t size))          \
    X(void, nvnCommandBufferClearDepthStencil,                                                    \
      (NVNcommandBuffer *cmd, float depth, NVNboolean depth_mask, int stencil, int stencil_mask))

// The API 231 build's own: compute, storage buffers and images, the program builder, push
// constants and mesh draws.
#define NVN_LAYER_PROCS_231(X)                                                                    \
    X(NVNimageHandle, nvnDeviceGetImageHandle, (const NVNdevice *device, int id))                 \
    X(void, nvnTexturePoolRegisterImage,                                                          \
      (const NVNtexturePool *pool, int id, const NVNtexture *texture,                             \
       const NVNtextureView *view))                                                               \
    X(void, nvnProgramBuilderSetDefaults, (NVNprogramBuilder *builder))                           \
    X(void, nvnProgramBuilderSetDevice, (NVNprogramBuilder *builder, NVNdevice *device))          \
    X(void, nvnProgramBuilderSetShaders,                                                          \
      (NVNprogramBuilder *builder, int count, const NVNshaderData *shaders))                      \
    X(NVNboolean, nvnProgramInitializeFromBuilder,                                                \
      (NVNprogram *program, const NVNprogramBuilder *builder))                                    \
    X(void, nvnCommandBufferBindStorageBuffer,                                                    \
      (NVNcommandBuffer *cmd, NVNshaderStage stage, int index, NVNbufferAddress address,          \
       size_t size))                                                                              \
    X(void, nvnCommandBufferBindImage,                                                            \
      (NVNcommandBuffer *cmd, NVNshaderStage stage, int index, NVNimageHandle handle))            \
    X(void, nvnCommandBufferDispatchCompute, (NVNcommandBuffer *cmd, int x, int y, int z))        \
    X(void, nvnCommandBufferPushGraphicsConstants,                                                \
      (NVNcommandBuffer *cmd, int table, int offset, int bytes, const void *data))                \
    X(void, nvnCommandBufferPushComputeConstants,                                                 \
      (NVNcommandBuffer *cmd, int table, int offset, int bytes, const void *data))                \
    X(void, nvnCommandBufferSetGeometryProcessingMode, (NVNcommandBuffer *cmd, int mode))         \
    X(void, nvnCommandBufferDrawMeshTasks, (NVNcommandBuffer *cmd, int first, int count))         \
    NVN_LAYER_PROCS_WINDOW_231(X)                                                                 \
    NVN_LAYER_PROCS_DEBUG_231(X)                                                                  \
    NVN_LAYER_PROCS_PROGRAM_231(X)

#if NVN_LAYER_API >= 231
#define NVN_LAYER_PROCS(X) NVN_LAYER_PROCS_BASE(X) NVN_LAYER_PROCS_DRAW(X) NVN_LAYER_PROCS_231(X)
#else
#define NVN_LAYER_PROCS(X) NVN_LAYER_PROCS_BASE(X) NVN_LAYER_PROCS_DRAW(X)
#endif
