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
/// - NVN_LAYER_PROCS_BASE and NVN_LAYER_PROCS_DRAW: both API versions have them. The API 55
///   build gives each a body on deko3d; the API 231 build gives each its stopping body.
/// - NVN_LAYER_PROCS_231: the functions only the API 231 build has, compute's among them, which
///   NVN 55 has too and the deko3d build does not implement yet.

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
    X(void, nvnWindowFinalize, (NVNwindow * window))

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
    X(void, nvnCommandBufferDrawMeshTasks, (NVNcommandBuffer *cmd, int first, int count))

#if NVN_LAYER_API >= 231
#define NVN_LAYER_PROCS(X) NVN_LAYER_PROCS_BASE(X) NVN_LAYER_PROCS_DRAW(X) NVN_LAYER_PROCS_231(X)
#else
#define NVN_LAYER_PROCS(X) NVN_LAYER_PROCS_BASE(X) NVN_LAYER_PROCS_DRAW(X)
#endif
