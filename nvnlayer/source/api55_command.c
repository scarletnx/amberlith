// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "api55.h"

// The API 55 build's command buffer past the binds and the draws: what a list reports to memory
// (counters, values) and whether what follows is drawn at all, each as the deko3d commands that
// do what NVN says.

long LayerUnit(LayerCommandBuffer *record, const char *what) {
    LayerDraw *const draw = record->draw;
    if (draw->handle_used + LAYER_HANDLE_BYTES > draw->handle_size) {
        LayerComplain("%s: the quarter of the command memory the layer keeps, 0x%x bytes, is "
                      "full after %u units of 0x%x: the list needs more command memory",
                      what, (unsigned)draw->handle_size,
                      (unsigned)(draw->handle_used / LAYER_HANDLE_BYTES),
                      (unsigned)LAYER_HANDLE_BYTES);
        return -1;
    }
    const long at = (long)draw->handle_used;
    draw->handle_used += LAYER_HANDLE_BYTES;
    return at;
}

// ---- counters and what a list reports

/// deko3d's counter for NVN's, or -1.
static int CounterOf(NVNcounterType type) {
    switch (type) {
    case NVN_COUNTER_TYPE_TIMESTAMP:
        return DkCounter_Timestamp;
    case NVN_COUNTER_TYPE_SAMPLES_PASSED:
        return DkCounter_SamplesPassed;
    case NVN_COUNTER_TYPE_INPUT_VERTICES:
        return DkCounter_InputVertices;
    case NVN_COUNTER_TYPE_INPUT_PRIMITIVES:
        return DkCounter_InputPrimitives;
    case NVN_COUNTER_TYPE_VERTEX_SHADER_INVOCATIONS:
        return DkCounter_VertexShaderInvocations;
    case NVN_COUNTER_TYPE_TESS_CONTROL_SHADER_INVOCATIONS:
        return DkCounter_TessControlShaderInvocations;
    case NVN_COUNTER_TYPE_TESS_EVALUATION_SHADER_INVOCATIONS:
        return DkCounter_TessEvaluationShaderInvocations;
    case NVN_COUNTER_TYPE_GEOMETRY_SHADER_INVOCATIONS:
        return DkCounter_GeometryShaderInvocations;
    case NVN_COUNTER_TYPE_FRAGMENT_SHADER_INVOCATIONS:
        return DkCounter_FragmentShaderInvocations;
    case NVN_COUNTER_TYPE_TESS_EVALUATION_SHADER_PRIMITIVES:
        return DkCounter_TessEvaluationShaderPrimitives;
    case NVN_COUNTER_TYPE_GEOMETRY_SHADER_PRIMITIVES:
        return DkCounter_GeometryShaderPrimitives;
    case NVN_COUNTER_TYPE_CLIPPER_INPUT_PRIMITIVES:
        return DkCounter_ClipperInputPrimitives;
    case NVN_COUNTER_TYPE_CLIPPER_OUTPUT_PRIMITIVES:
        return DkCounter_ClipperOutputPrimitives;
    case NVN_COUNTER_TYPE_PRIMITIVES_GENERATED:
        return DkCounter_PrimitivesGenerated;
    case NVN_COUNTER_TYPE_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
        return DkCounter_TransformFeedbackPrimitivesWritten;
    case NVN_COUNTER_TYPE_ZCULL_STATS:
        return DkCounter_ZcullStats;
    case NVN_COUNTER_TYPE_TIMESTAMP_TOP:
        return DkCounter_TimestampPipelineTop;
    default:
        return -1;
    }
}

void Nvnl_nvnCommandBufferReportCounter(NVNcommandBuffer *cmd, NVNcounterType type,
                                        NVNbufferAddress address) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferReportCounter");
    if (record == NULL) {
        return;
    }
    const int counter = CounterOf(type);
    if (counter < 0) {
        LayerComplain("nvnCommandBufferReportCounter: counter type %d is none of NVN's 0 to "
                      "0x10",
                      (int)type);
        return;
    }
    dkCmdBufReportCounter(record->cmdbuf, (DkCounter)counter, address);
}

void Nvnl_nvnCommandBufferReportValue(NVNcommandBuffer *cmd, uint32_t value,
                                      NVNbufferAddress address) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferReportValue");
    if (record != NULL) {
        dkCmdBufReportValue(record->cmdbuf, value, address);
    }
}

void Nvnl_nvnCommandBufferResetCounter(NVNcommandBuffer *cmd, NVNcounterType type) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferResetCounter");
    if (record == NULL) {
        return;
    }
    const int counter = CounterOf(type);
    if (counter < 0) {
        LayerComplain("nvnCommandBufferResetCounter: counter type %d is none of NVN's 0 to 0x10",
                      (int)type);
        return;
    }
    dkCmdBufResetCounter(record->cmdbuf, (DkCounter)counter);
}

/// A counter's timestamp is in the graphics unit's ticks, 384 of them every 625 nanoseconds.
uint64_t Nvnl_nvnDeviceGetTimestampInNanoseconds(const NVNdevice *device,
                                                 const NVNcounterData *counter) {
    (void)device;
    return dkTimestampToNs(counter->timestamp);
}

uint64_t Nvnl_nvnDeviceGetCurrentTimestampInNanoseconds(const NVNdevice *device) {
    const LayerDevice *const record =
        DeviceOf(device, "nvnDeviceGetCurrentTimestampInNanoseconds");
    return record != NULL ? dkDeviceGetCurrentTimestampInNs(record->device) : 0u;
}

// ---- whether what follows is drawn

// The render enable. The two conditional modes compare the 64-bit words at the address and 16 bytes
// past it, the counts of two counter reports one after the other, which is how NVN uses them.
static void RenderEnable(NVNcommandBuffer *cmd, uint64_t address, DkRenderEnableMode mode,
                         const char *what) {
    LayerCommandBuffer *const record = CmdOf(cmd, what);
    if (record != NULL) {
        dkCmdBufSetRenderEnable(record->cmdbuf, mode, address);
    }
}

void Nvnl_nvnCommandBufferSetRenderEnable(NVNcommandBuffer *cmd, NVNboolean enable) {
    RenderEnable(cmd, 0u, enable ? DkRenderEnableMode_Always : DkRenderEnableMode_Never,
                 "nvnCommandBufferSetRenderEnable");
}

void Nvnl_nvnCommandBufferSetRenderEnableConditional(NVNcommandBuffer *cmd,
                                                     NVNconditionalRenderMode mode,
                                                     NVNbufferAddress address) {
    if (mode != NVN_CONDITIONAL_RENDER_MODE_RENDER_IF_EQUAL &&
        mode != NVN_CONDITIONAL_RENDER_MODE_RENDER_IF_NOT_EQUAL) {
        LayerComplain("nvnCommandBufferSetRenderEnableConditional: mode %d is neither render if "
                      "equal (%d) nor render if not equal (%d)",
                      (int)mode, (int)NVN_CONDITIONAL_RENDER_MODE_RENDER_IF_EQUAL,
                      (int)NVN_CONDITIONAL_RENDER_MODE_RENDER_IF_NOT_EQUAL);
        return;
    }
    RenderEnable(cmd, address,
                 mode == NVN_CONDITIONAL_RENDER_MODE_RENDER_IF_EQUAL
                     ? DkRenderEnableMode_IfEqual
                     : DkRenderEnableMode_IfNotEqual,
                 "nvnCommandBufferSetRenderEnableConditional");
}

// ---- the command buffer's memory and its recording

/// The bytes of the command memory a list has taken since it was added: the layer's own part,
/// kept from the start, and deko3d's part up to where the fill byte starts again. -1 with a
/// complaint for memory the processor cannot read.
static long long CommandUsed(const LayerCommandBuffer *record, const char *what) {
    const LayerCommandMemory *const memory = &record->draw->memory;
    if (memory->added == 0u) {
        return 0;
    }
    if (memory->given == NULL) {
        LayerComplain("%s: the command memory is in a pool the processor has no access to, and "
                      "the layer reads how far a list got through the processor's mapping",
                      what);
        return -1;
    }
    uint32_t end = memory->given_size;
    while (end > 0u && memory->given[end - 1u] == LAYER_UNUSED_BYTE) {
        --end;
    }
    // Whole words: a command's last byte may be the fill byte itself.
    end = (end + 3u) & ~3u;
    return (long long)(memory->added - memory->given_size) + end;
}

size_t Nvnl_nvnCommandBufferGetCommandMemorySize(const NVNcommandBuffer *cmd) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferGetCommandMemorySize");
    return record != NULL ? record->draw->memory.added : 0u;
}

size_t Nvnl_nvnCommandBufferGetCommandMemoryUsed(const NVNcommandBuffer *cmd) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferGetCommandMemoryUsed");
    const long long used =
        record != NULL ? CommandUsed(record, "nvnCommandBufferGetCommandMemoryUsed") : -1;
    return used < 0 ? 0u : (size_t)used;
}

size_t Nvnl_nvnCommandBufferGetCommandMemoryFree(const NVNcommandBuffer *cmd) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferGetCommandMemoryFree");
    const long long used =
        record != NULL ? CommandUsed(record, "nvnCommandBufferGetCommandMemoryFree") : -1;
    return used < 0 ? 0u : record->draw->memory.added - (size_t)used;
}

// deko3d keeps a list's control words in memory of its own, so none of what the program gave
// is ever used.
size_t Nvnl_nvnCommandBufferGetControlMemorySize(const NVNcommandBuffer *cmd) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferGetControlMemorySize");
    return record != NULL ? record->draw->memory.control : 0u;
}

size_t Nvnl_nvnCommandBufferGetControlMemoryUsed(const NVNcommandBuffer *cmd) {
    (void)CmdOf(cmd, "nvnCommandBufferGetControlMemoryUsed");
    return 0u;
}

size_t Nvnl_nvnCommandBufferGetControlMemoryFree(const NVNcommandBuffer *cmd) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferGetControlMemoryFree");
    return record != NULL ? record->draw->memory.control : 0u;
}

NVNboolean Nvnl_nvnCommandBufferIsRecording(const NVNcommandBuffer *cmd) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferIsRecording");
    return record != NULL && record->draw->memory.recording;
}

void Nvnl_nvnCommandBufferSetMemoryCallback(NVNcommandBuffer *cmd,
                                            PFNNVNCOMMANDBUFFERMEMORYCALLBACKPROC callback) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferSetMemoryCallback");
    if (record != NULL) {
        record->draw->memory.callback = callback;
    }
}

void Nvnl_nvnCommandBufferSetMemoryCallbackData(NVNcommandBuffer *cmd, void *data) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferSetMemoryCallbackData");
    if (record != NULL) {
        record->draw->memory.callback_data = data;
    }
}

PFNNVNCOMMANDBUFFERMEMORYCALLBACKPROC
Nvnl_nvnCommandBufferGetMemoryCallback(const NVNcommandBuffer *cmd) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferGetMemoryCallback");
    return record != NULL ? record->draw->memory.callback : NULL;
}

void *Nvnl_nvnCommandBufferGetMemoryCallbackData(const NVNcommandBuffer *cmd) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferGetMemoryCallbackData");
    return record != NULL ? record->draw->memory.callback_data : NULL;
}

void Nvnl_nvnCommandBufferSetCommandMemoryCallbackEnabled(NVNcommandBuffer *cmd,
                                                          NVNboolean enabled) {
    const LayerCommandBuffer *const record =
        CmdOf(cmd, "nvnCommandBufferSetCommandMemoryCallbackEnabled");
    if (record != NULL) {
        record->draw->memory.command_callback = enabled != 0;
    }
}

/// Kept for nothing: the layer never runs out of control memory, having none of the program's
/// in use.
void Nvnl_nvnCommandBufferSetControlMemoryCallbackEnabled(NVNcommandBuffer *cmd,
                                                          NVNboolean enabled) {
    const LayerCommandBuffer *const record =
        CmdOf(cmd, "nvnCommandBufferSetControlMemoryCallbackEnabled");
    if (record != NULL) {
        record->draw->memory.control_callback = enabled != 0;
    }
}

void LayerCommandGrow(void *user, DkCmdBuf cmdbuf, size_t least) {
    (void)cmdbuf;
    NVNcommandBuffer *const cmd = user;
    const LayerCommandBuffer *const record = CmdOf(cmd, "a list out of command memory");
    if (record == NULL) {
        return;
    }
    const LayerCommandMemory *const memory = &record->draw->memory;
    if (memory->callback == NULL || !memory->command_callback) {
        LayerComplain("a list ran out of the 0x%x bytes of command memory it was given and "
                      "needs 0x%zx more, with %s to add them",
                      (unsigned)memory->added, least,
                      memory->callback == NULL ? "no memory callback set"
                                               : "the command memory callback turned off");
        return;
    }
    memory->callback(cmd, NVN_COMMAND_BUFFER_MEMORY_EVENT_OUT_OF_COMMAND_MEMORY, least,
                     memory->callback_data);
}

// A list inside another. deko3d calls a list where it lies, and has no copy of one: a copied
// list is called too, so the layer asks of nvnCommandBufferCopyCommands what NVN asks only of
// nvnCommandBufferCallCommands, that the list's memory outlives the one that names it.
static void CallLists(NVNcommandBuffer *cmd, int count, const NVNcommandHandle *handles,
                      const char *what) {
    const LayerCommandBuffer *const record = CmdOf(cmd, what);
    if (record == NULL) {
        return;
    }
    if (count < 0 || (count > 0 && handles == NULL)) {
        LayerComplain("%s: %d lists%s", what, count, handles == NULL ? " and no handles" : "");
        return;
    }
    for (int i = 0; i < count; ++i) {
        if (handles[i] == 0u) {
            LayerComplain("%s: list %d of %d is no list (0)", what, i, count);
            return;
        }
        dkCmdBufCallList(record->cmdbuf, (DkCmdList)handles[i]);
    }
}

void Nvnl_nvnCommandBufferCallCommands(NVNcommandBuffer *cmd, int count,
                                       const NVNcommandHandle *handles) {
    CallLists(cmd, count, handles, "nvnCommandBufferCallCommands");
}

void Nvnl_nvnCommandBufferCopyCommands(NVNcommandBuffer *cmd, int count,
                                       const NVNcommandHandle *handles) {
    CallLists(cmd, count, handles, "nvnCommandBufferCopyCommands");
}

// ---- clears past the float clear of a target

void Nvnl_nvnCommandBufferClearColori(NVNcommandBuffer *cmd, int index, const int *rgba,
                                      int mask) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferClearColori");
    if (record != NULL) {
        dkCmdBufClearColorSint(record->cmdbuf, (uint32_t)index,
                               (uint32_t)mask & DkColorMask_RGBA, rgba[0], rgba[1], rgba[2],
                               rgba[3]);
    }
}

void Nvnl_nvnCommandBufferClearColorui(NVNcommandBuffer *cmd, int index, const uint32_t *rgba,
                                       int mask) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferClearColorui");
    if (record != NULL) {
        dkCmdBufClearColorUint(record->cmdbuf, (uint32_t)index,
                               (uint32_t)mask & DkColorMask_RGBA, rgba[0], rgba[1], rgba[2],
                               rgba[3]);
    }
}

/// How a clear's four words are read.
typedef enum {
    WORDS_FLOAT,
    WORDS_SIGNED,
    WORDS_UNSIGNED,
} ClearWords;

/// A clear of a texture that is no target. deko3d clears targets only: the texture is bound as
/// the one target, the scissor put over the region, the clear recorded, and the targets and the
/// scissor the program last set put back.
static void ClearTexture(NVNcommandBuffer *cmd, const NVNtexture *texture,
                         const NVNtextureView *view, const NVNcopyRegion *region,
                         const void *rgba, int mask, ClearWords words, const char *what) {
    const LayerCommandBuffer *const record = CmdOf(cmd, what);
    const LayerTexture *const to = TextureOf(texture, what);
    if (record == NULL || to == NULL) {
        return;
    }
    if (view != NULL || region->zoffset != 0 || region->depth != 1 ||
        to->format == NVN_FORMAT_DEPTH32F || to->format == NVN_FORMAT_DEPTH24_STENCIL8) {
        LayerComplain("%s: %s view, layers %d to %d and NVN format 0x%x, and the layer clears "
                      "the one layer of a colour texture with no view",
                      what, view != NULL ? "a" : "no", region->zoffset,
                      region->zoffset + region->depth, (unsigned)to->format);
        return;
    }
    if (region->xoffset < 0 || region->yoffset < 0 || region->width <= 0 || region->height <= 0 ||
        region->xoffset + region->width > to->width ||
        region->yoffset + region->height > to->height) {
        LayerComplain("%s: the region %d,%d %dx%d is not inside the %dx%d texture", what,
                      region->xoffset, region->yoffset, region->width, region->height, to->width,
                      to->height);
        return;
    }
    DkImageView target;
    dkImageViewDefaults(&target, &to->image);
    const DkImageView *const one = &target;
    dkCmdBufBindRenderTargets(record->cmdbuf, &one, 1, NULL);
    const DkScissor over = { (uint32_t)region->xoffset, (uint32_t)region->yoffset,
                             (uint32_t)region->width, (uint32_t)region->height };
    dkCmdBufSetScissors(record->cmdbuf, 0, &over, 1);
    const uint32_t channels = (uint32_t)mask & DkColorMask_RGBA;
    if (words == WORDS_FLOAT) {
        const float *const value = rgba;
        dkCmdBufClearColorFloat(record->cmdbuf, 0, channels, value[0], value[1], value[2],
                                value[3]);
    } else if (words == WORDS_SIGNED) {
        const int32_t *const value = rgba;
        dkCmdBufClearColorSint(record->cmdbuf, 0, channels, value[0], value[1], value[2],
                               value[3]);
    } else {
        const uint32_t *const value = rgba;
        dkCmdBufClearColorUint(record->cmdbuf, 0, channels, value[0], value[1], value[2],
                               value[3]);
    }
    // Back to what the program set.
    const LayerTargets *const kept = &record->draw->targets;
    DkImageView colors[COLOR_TARGETS_MAX];
    DkImageView const *bound[COLOR_TARGETS_MAX];
    for (int i = 0; i < kept->count; ++i) {
        dkImageViewDefaults(&colors[i], &((const LayerTexture *)(const void *)kept->colors[i])->image);
        bound[i] = &colors[i];
    }
    DkImageView depth;
    if (kept->depth != NULL) {
        dkImageViewDefaults(&depth, &((const LayerTexture *)(const void *)kept->depth)->image);
    }
    dkCmdBufBindRenderTargets(record->cmdbuf, bound, (uint32_t)kept->count,
                              kept->depth != NULL ? &depth : NULL);
    if (kept->scissor_set) {
        dkCmdBufSetScissors(record->cmdbuf, 0, &kept->scissor, 1);
    }
}

void Nvnl_nvnCommandBufferClearTexture(NVNcommandBuffer *cmd, const NVNtexture *texture,
                                       const NVNtextureView *view, const NVNcopyRegion *region,
                                       const float *rgba, int mask) {
    ClearTexture(cmd, texture, view, region, rgba, mask, WORDS_FLOAT,
                 "nvnCommandBufferClearTexture");
}

void Nvnl_nvnCommandBufferClearTexturei(NVNcommandBuffer *cmd, const NVNtexture *texture,
                                        const NVNtextureView *view, const NVNcopyRegion *region,
                                        const int *rgba, int mask) {
    ClearTexture(cmd, texture, view, region, rgba, mask, WORDS_SIGNED,
                 "nvnCommandBufferClearTexturei");
}

void Nvnl_nvnCommandBufferClearTextureui(NVNcommandBuffer *cmd, const NVNtexture *texture,
                                         const NVNtextureView *view, const NVNcopyRegion *region,
                                         const uint32_t *rgba, int mask) {
    ClearTexture(cmd, texture, view, region, rgba, mask, WORDS_UNSIGNED,
                 "nvnCommandBufferClearTextureui");
}

/// A range of a buffer filled with one word. deko3d has no fill: the word fills a unit of the
/// layer's memory, and the graphics unit copies from it, a unit's bytes at a time.
void Nvnl_nvnCommandBufferClearBuffer(NVNcommandBuffer *cmd, NVNbufferAddress address,
                                      size_t size, uint32_t value) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferClearBuffer");
    if (record == NULL || size == 0u) {
        return;
    }
    if ((address & 3u) != 0u || (size & 3u) != 0u) {
        LayerComplain("nvnCommandBufferClearBuffer: 0x%zx bytes at 0x%llx, and the layer fills "
                      "whole words on a four byte boundary",
                      size, (unsigned long long)address);
        return;
    }
    const long unit = LayerUnit(record, "nvnCommandBufferClearBuffer");
    if (unit < 0) {
        return;
    }
    const LayerDraw *const draw = record->draw;
    for (uint32_t at = 0; at < LAYER_HANDLE_BYTES; at += 4u) {
        memcpy(draw->handle_cpu + unit + at, &value, 4);
    }
    for (size_t done = 0; done < size; done += LAYER_HANDLE_BYTES) {
        const size_t part = size - done < LAYER_HANDLE_BYTES ? size - done : LAYER_HANDLE_BYTES;
        dkCmdBufCopyBuffer(record->cmdbuf, draw->handle_memory + (uint64_t)unit, address + done,
                           (uint32_t)part);
    }
}

// ---- what a copy and a target keep

/// Kept for its getter: the layer copies the one layer of a 2D texture, which has no image
/// after it for the stride to reach.
void Nvnl_nvnCommandBufferSetCopyImageStride(NVNcommandBuffer *cmd, ptrdiff_t stride) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferSetCopyImageStride");
    if (record != NULL) {
        record->draw->copy_image_stride = stride;
    }
}

ptrdiff_t Nvnl_nvnCommandBufferGetCopyImageStride(const NVNcommandBuffer *cmd) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferGetCopyImageStride");
    return record != NULL ? record->draw->copy_image_stride : 0;
}

ptrdiff_t Nvnl_nvnCommandBufferGetCopyRowStride(const NVNcommandBuffer *cmd) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferGetCopyRowStride");
    return record != NULL ? (ptrdiff_t)record->copy_row_stride : 0;
}

void Nvnl_nvnCommandBufferDiscardColor(NVNcommandBuffer *cmd, int index) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferDiscardColor");
    if (record == NULL) {
        return;
    }
    if (index < 0 || index >= COLOR_TARGETS_MAX) {
        LayerComplain("nvnCommandBufferDiscardColor: colour target %d, where there are 0 to %d",
                      index, COLOR_TARGETS_MAX - 1);
        return;
    }
    dkCmdBufDiscardColor(record->cmdbuf, (uint32_t)index);
}

void Nvnl_nvnCommandBufferDiscardDepthStencil(NVNcommandBuffer *cmd) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferDiscardDepthStencil");
    if (record != NULL) {
        dkCmdBufDiscardDepthStencil(record->cmdbuf);
    }
}

// ---- the render state NVN sets by itself

void LayerBindRasterizer(const LayerCommandBuffer *record) {
    const LayerDynamic *const dynamic = &record->draw->dynamic;
    DkRasterizerState raster;
    dkRasterizerStateDefaults(&raster);
    // NVN's faces are deko3d's numbers, none 0 to both 3, and its front faces too.
    raster.cullMode = (DkFace)dynamic->cull;
    raster.frontFace = (DkFrontFace)dynamic->front;
    if (dynamic->polygon) {
        // NVN's polygon modes and offset enables are deko3d's numbers too.
        raster.polygonModeFront = (DkPolygonMode)dynamic->mode;
        raster.polygonModeBack = (DkPolygonMode)dynamic->mode;
        raster.depthBiasEnableMask = dynamic->offset;
    }
    raster.rasterizerEnable = !dynamic->discard;
    raster.depthClampEnable = dynamic->depth_clamp;
    raster.provokingVertex =
        dynamic->provoking_first ? DkProvokingVertex_First : DkProvokingVertex_Last;
    dkCmdBufBindRasterizerState(record->cmdbuf, &raster);
}

void LayerBindViewport(const LayerCommandBuffer *record, int index) {
    const LayerDynamic *const dynamic = &record->draw->dynamic;
    DkViewport viewport = dynamic->viewports[index];
    if ((dynamic->ranged >> index & 1u) == 0u) {
        viewport.near = 0.0f;
        viewport.far = 1.0f;
    }
    dkCmdBufSetViewports(record->cmdbuf, (uint32_t)index, &viewport, 1);
}

/// Whether @p count from @p first are viewports; a complaint naming @p what when they are not.
static bool Viewports(int first, int count, const void *values, const char *what) {
    if (first < 0 || count <= 0 || count > LAYER_VIEWPORTS - first || values == NULL) {
        LayerComplain("%s: %d from %d%s, where there are %d viewports", what, count, first,
                      values == NULL ? " and no values" : "", LAYER_VIEWPORTS);
        return false;
    }
    return true;
}

void Nvnl_nvnCommandBufferSetViewports(NVNcommandBuffer *cmd, int first, int count,
                                       const float *rectangles) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferSetViewports");
    if (record == NULL ||
        !Viewports(first, count, rectangles, "nvnCommandBufferSetViewports")) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        DkViewport *const viewport = &record->draw->dynamic.viewports[first + i];
        viewport->x = rectangles[4 * i];
        viewport->y = rectangles[4 * i + 1];
        viewport->width = rectangles[4 * i + 2];
        viewport->height = rectangles[4 * i + 3];
        LayerBindViewport(record, first + i);
    }
}

void Nvnl_nvnCommandBufferSetScissors(NVNcommandBuffer *cmd, int first, int count,
                                      const int *rectangles) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferSetScissors");
    if (record == NULL || !Viewports(first, count, rectangles, "nvnCommandBufferSetScissors")) {
        return;
    }
    DkScissor scissors[LAYER_VIEWPORTS];
    for (int i = 0; i < count; ++i) {
        scissors[i] = (DkScissor){ (uint32_t)rectangles[4 * i], (uint32_t)rectangles[4 * i + 1],
                                   (uint32_t)rectangles[4 * i + 2],
                                   (uint32_t)rectangles[4 * i + 3] };
    }
    dkCmdBufSetScissors(record->cmdbuf, (uint32_t)first, scissors, (uint32_t)count);
    if (first == 0) {
        record->draw->targets.scissor = scissors[0];
        record->draw->targets.scissor_set = true;
    }
}

/// The depth range of every viewport, as NVN's call with no viewport sets it.
void Nvnl_nvnCommandBufferSetDepthRange(NVNcommandBuffer *cmd, float near, float far) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferSetDepthRange");
    if (record == NULL) {
        return;
    }
    LayerDynamic *const dynamic = &record->draw->dynamic;
    for (int i = 0; i < LAYER_VIEWPORTS; ++i) {
        dynamic->viewports[i].near = near;
        dynamic->viewports[i].far = far;
    }
    dynamic->ranged = 0xffffu;
    // The first viewport is the one a list of the layer's draws through; the others take their
    // range when their rectangle is set.
    LayerBindViewport(record, 0);
}

void Nvnl_nvnCommandBufferSetDepthRanges(NVNcommandBuffer *cmd, int first, int count,
                                         const float *ranges) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferSetDepthRanges");
    if (record == NULL || !Viewports(first, count, ranges, "nvnCommandBufferSetDepthRanges")) {
        return;
    }
    LayerDynamic *const dynamic = &record->draw->dynamic;
    for (int i = 0; i < count; ++i) {
        dynamic->viewports[first + i].near = ranges[2 * i];
        dynamic->viewports[first + i].far = ranges[2 * i + 1];
        dynamic->ranged |= (uint16_t)(1u << (first + i));
        LayerBindViewport(record, first + i);
    }
}

void Nvnl_nvnCommandBufferSetViewportSwizzles(NVNcommandBuffer *cmd, int first, int count,
                                              const NVNviewportSwizzle *swizzles) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferSetViewportSwizzles");
    if (record == NULL ||
        !Viewports(first, count, swizzles, "nvnCommandBufferSetViewportSwizzles")) {
        return;
    }
    // NVN's swizzles are deko3d's numbers, plus x 0 to minus w 7, four a viewport.
    DkViewportSwizzle to[LAYER_VIEWPORTS];
    for (int i = 0; i < count; ++i) {
        to[i] = (DkViewportSwizzle){ (DkSwizzle)swizzles[4 * i], (DkSwizzle)swizzles[4 * i + 1],
                                     (DkSwizzle)swizzles[4 * i + 2],
                                     (DkSwizzle)swizzles[4 * i + 3] };
    }
    dkCmdBufSetViewportSwizzles(record->cmdbuf, (uint32_t)first, to, (uint32_t)count);
}

void Nvnl_nvnCommandBufferSetDepthBounds(NVNcommandBuffer *cmd, NVNboolean enable, float least,
                                         float most) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferSetDepthBounds");
    if (record != NULL) {
        dkCmdBufSetDepthBounds(record->cmdbuf, enable != 0, least, most);
    }
}

void Nvnl_nvnCommandBufferSetRasterizerDiscard(NVNcommandBuffer *cmd, NVNboolean discard) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferSetRasterizerDiscard");
    if (record != NULL) {
        record->draw->dynamic.discard = discard != 0;
        LayerBindRasterizer(record);
    }
}

void Nvnl_nvnCommandBufferSetDepthClamp(NVNcommandBuffer *cmd, NVNboolean clamp) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferSetDepthClamp");
    if (record != NULL) {
        record->draw->dynamic.depth_clamp = clamp != 0;
        LayerBindRasterizer(record);
    }
}

/// NVN's mode is the graphics unit's word: 0 the first vertex, 1 the last.
void Nvnl_nvnCommandBufferSetProvokingVertexMode(NVNcommandBuffer *cmd, int mode) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferSetProvokingVertexMode");
    if (record == NULL) {
        return;
    }
    if (mode != 0 && mode != 1) {
        LayerComplain("nvnCommandBufferSetProvokingVertexMode: mode %d is neither the first "
                      "vertex (0) nor the last (1)",
                      mode);
        return;
    }
    record->draw->dynamic.provoking_first = mode == 0;
    LayerBindRasterizer(record);
}

void Nvnl_nvnCommandBufferSetPointSize(NVNcommandBuffer *cmd, float size) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferSetPointSize");
    if (record != NULL) {
        dkCmdBufSetPointSize(record->cmdbuf, size);
    }
}

void Nvnl_nvnCommandBufferSetPrimitiveRestart(NVNcommandBuffer *cmd, NVNboolean enable,
                                              int index) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferSetPrimitiveRestart");
    if (record != NULL) {
        dkCmdBufSetPrimitiveRestart(record->cmdbuf, enable != 0, (uint32_t)index);
    }
}

void Nvnl_nvnCommandBufferSetTiledCacheAction(NVNcommandBuffer *cmd,
                                              NVNtiledCacheAction action) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferSetTiledCacheAction");
    if (record == NULL) {
        return;
    }
    static const DkTiledCacheOp OPS[] = { [NVN_TILED_CACHE_ACTION_ENABLE] = DkTiledCacheOp_Enable,
                                          [NVN_TILED_CACHE_ACTION_DISABLE] =
                                              DkTiledCacheOp_Disable,
                                          [NVN_TILED_CACHE_ACTION_FLUSH] = DkTiledCacheOp_Flush,
                                          [NVN_TILED_CACHE_ACTION_FLUSH_NO_TILING] =
                                              DkTiledCacheOp_FlushAlt,
                                          [NVN_TILED_CACHE_ACTION_ENABLE_RENDER_TARGET_BINNING] =
                                              DkTiledCacheOp_UnkEnable,
                                          [NVN_TILED_CACHE_ACTION_DISABLE_RENDER_TARGET_BINNING] =
                                              DkTiledCacheOp_UnkDisable };
    if (action < NVN_TILED_CACHE_ACTION_ENABLE ||
        action > NVN_TILED_CACHE_ACTION_DISABLE_RENDER_TARGET_BINNING) {
        LayerComplain("nvnCommandBufferSetTiledCacheAction: action %d is none of NVN's 1 to 6",
                      (int)action);
        return;
    }
    dkCmdBufTiledCacheOp(record->cmdbuf, OPS[action]);
}

void Nvnl_nvnCommandBufferSetTiledCacheTileSize(NVNcommandBuffer *cmd, int width, int height) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferSetTiledCacheTileSize");
    if (record == NULL) {
        return;
    }
    if (width < 16 || width > 16384 || (width & (width - 1)) != 0 || height < 16 ||
        height > 16384 || (height & (height - 1)) != 0) {
        LayerComplain("nvnCommandBufferSetTiledCacheTileSize: %dx%d, where a tile's sides are "
                      "powers of two from 16 to 16384",
                      width, height);
        return;
    }
    dkCmdBufSetTileSize(record->cmdbuf, (uint32_t)width, (uint32_t)height);
}

// ---- what a device and a list take and show nothing of

// A release driver returns from each of these at once: objects whose finalize was deferred are
// deko3d's to free, the shader cache is a compiler's, and a list's handle is tracked by the
// debug layer alone. The layer checks its object and does nothing more.

void Nvnl_nvnDeviceApplyDeferredFinalizes(NVNdevice *device, int age) {
    (void)age;
    (void)DeviceOf(device, "nvnDeviceApplyDeferredFinalizes");
}

void Nvnl_nvnDeviceSetIntermediateShaderCache(NVNdevice *device, int count) {
    (void)count;
    (void)DeviceOf(device, "nvnDeviceSetIntermediateShaderCache");
}

void Nvnl_nvnDeviceFinalizeCommandHandle(NVNdevice *device, NVNcommandHandle handle) {
    (void)handle;
    (void)DeviceOf(device, "nvnDeviceFinalizeCommandHandle");
}

// Three more of the graphics unit's own business with a depth target: writing a compressed one
// out as values, and saving and restoring its coarse culling's data. deko3d keeps a depth target
// as values and has no call for the culling's data, so nothing is recorded.

void Nvnl_nvnCommandBufferResolveDepthBuffer(NVNcommandBuffer *cmd) {
    (void)CmdOf(cmd, "nvnCommandBufferResolveDepthBuffer");
}

void Nvnl_nvnCommandBufferSaveZCullData(NVNcommandBuffer *cmd, NVNbufferAddress address,
                                        size_t size) {
    (void)address;
    (void)size;
    (void)CmdOf(cmd, "nvnCommandBufferSaveZCullData");
}

void Nvnl_nvnCommandBufferRestoreZCullData(NVNcommandBuffer *cmd, NVNbufferAddress address,
                                           size_t size) {
    (void)address;
    (void)size;
    (void)CmdOf(cmd, "nvnCommandBufferRestoreZCullData");
}

// ---- the rest of the render state NVN sets by itself

void Nvnl_nvnCommandBufferSetAlphaRef(NVNcommandBuffer *cmd, float ref) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferSetAlphaRef");
    if (record != NULL) {
        dkCmdBufSetAlphaRef(record->cmdbuf, ref);
    }
}

void Nvnl_nvnCommandBufferSetBlendColor(NVNcommandBuffer *cmd, const float *rgba) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferSetBlendColor");
    if (record == NULL) {
        return;
    }
    if (rgba == NULL) {
        LayerComplain("nvnCommandBufferSetBlendColor: no colour, where it takes four floats");
        return;
    }
    dkCmdBufSetBlendConst(record->cmdbuf, rgba[0], rgba[1], rgba[2], rgba[3]);
}

/// NVN sets one of a face's three stencil numbers at a time and deko3d all three: @p which
/// (0 the reference, 1 the write mask, 2 the compare mask) of @p faces takes @p value, and each
/// face named is recorded whole.
static void Stencil(NVNcommandBuffer *cmd, NVNface faces, int which, int value,
                    const char *what) {
    const LayerCommandBuffer *const record = CmdOf(cmd, what);
    if (record == NULL) {
        return;
    }
    if (faces != NVN_FACE_FRONT && faces != NVN_FACE_BACK && faces != NVN_FACE_FRONT_AND_BACK) {
        LayerComplain("%s: face %d, where the stencil is the front's (%d), the back's (%d) or "
                      "both's (%d)",
                      what, (int)faces, (int)NVN_FACE_FRONT, (int)NVN_FACE_BACK,
                      (int)NVN_FACE_FRONT_AND_BACK);
        return;
    }
    LayerDynamic *const dynamic = &record->draw->dynamic;
    for (int face = 0; face < 2; ++face) {
        if (((int)faces >> face & 1) == 0) {
            continue;
        }
        if (which == 0) {
            dynamic->stencil_ref[face] = (uint8_t)value;
        } else if (which == 1) {
            dynamic->stencil_unwritten[face] = (uint8_t)~value;
        } else {
            dynamic->stencil_uncompared[face] = (uint8_t)~value;
        }
        dkCmdBufSetStencil(record->cmdbuf, face == 0 ? DkFace_Front : DkFace_Back,
                           (uint8_t)~dynamic->stencil_unwritten[face], dynamic->stencil_ref[face],
                           (uint8_t)~dynamic->stencil_uncompared[face]);
    }
}

void Nvnl_nvnCommandBufferSetStencilRef(NVNcommandBuffer *cmd, NVNface faces, int ref) {
    Stencil(cmd, faces, 0, ref, "nvnCommandBufferSetStencilRef");
}

void Nvnl_nvnCommandBufferSetStencilMask(NVNcommandBuffer *cmd, NVNface faces, int mask) {
    Stencil(cmd, faces, 1, mask, "nvnCommandBufferSetStencilMask");
}

void Nvnl_nvnCommandBufferSetStencilValueMask(NVNcommandBuffer *cmd, NVNface faces, int mask) {
    Stencil(cmd, faces, 2, mask, "nvnCommandBufferSetStencilValueMask");
}

void Nvnl_nvnCommandBufferSetSampleMask(NVNcommandBuffer *cmd, int mask) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferSetSampleMask");
    if (record != NULL) {
        dkCmdBufSetSampleMask(record->cmdbuf, (uint32_t)mask);
    }
}

void Nvnl_nvnCommandBufferSetPolygonOffsetClamp(NVNcommandBuffer *cmd, float factor,
                                                float units, float clamp) {
    const LayerCommandBuffer *const record =
        CmdOf(cmd, "nvnCommandBufferSetPolygonOffsetClamp");
    if (record != NULL) {
        // NVN's units are deko3d's constant factor, and its factor the one of the slope.
        dkCmdBufSetDepthBias(record->cmdbuf, units, clamp, factor);
    }
}

// Five hints to the graphics unit: what its coarse depth culling may take of the stencil test
// and of 32-bit float depth, the tiled cache's subtile, and the shortcut that skips a blend the
// source alpha makes plain. deko3d has a call for none of them and no picture shows one, so the
// layer checks the list and records nothing.

void Nvnl_nvnCommandBufferSetStencilCullCriteria(NVNcommandBuffer *cmd, NVNstencilFunc func,
                                                 int ref, int mask) {
    (void)func;
    (void)ref;
    (void)mask;
    (void)CmdOf(cmd, "nvnCommandBufferSetStencilCullCriteria");
}

void Nvnl_nvnCommandBufferOverrideSubtileSize(NVNcommandBuffer *cmd, NVNboolean enable,
                                              int size) {
    (void)enable;
    (void)size;
    (void)CmdOf(cmd, "nvnCommandBufferOverrideSubtileSize");
}

void Nvnl_nvnCommandBufferSetColorReductionEnable(NVNcommandBuffer *cmd, NVNboolean enable) {
    (void)enable;
    (void)CmdOf(cmd, "nvnCommandBufferSetColorReductionEnable");
}

void Nvnl_nvnCommandBufferSetColorReductionThresholds(NVNcommandBuffer *cmd, int format_class,
                                                      int conservative, int aggressive) {
    (void)format_class;
    (void)conservative;
    (void)aggressive;
    (void)CmdOf(cmd, "nvnCommandBufferSetColorReductionThresholds");
}

void Nvnl_nvnCommandBufferSetZCullZF32CompressionEnable(NVNcommandBuffer *cmd,
                                                        NVNboolean enable) {
    (void)enable;
    (void)CmdOf(cmd, "nvnCommandBufferSetZCullZF32CompressionEnable");
}

// ---- lines and conservative rasterization, each deko3d's own command

void Nvnl_nvnCommandBufferSetLineWidth(NVNcommandBuffer *cmd, float width) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferSetLineWidth");
    if (record != NULL) {
        dkCmdBufSetLineWidth(record->cmdbuf, width);
    }
}

void Nvnl_nvnCommandBufferSetConservativeRasterEnable(NVNcommandBuffer *cmd,
                                                      NVNboolean enable) {
    const LayerCommandBuffer *const record =
        CmdOf(cmd, "nvnCommandBufferSetConservativeRasterEnable");
    if (record != NULL) {
        dkCmdBufSetConservativeRasterEnable(record->cmdbuf, enable != 0);
    }
}

void Nvnl_nvnCommandBufferSetConservativeRasterDilate(NVNcommandBuffer *cmd, float dilate) {
    const LayerCommandBuffer *const record =
        CmdOf(cmd, "nvnCommandBufferSetConservativeRasterDilate");
    if (record != NULL) {
        dkCmdBufSetConservativeRasterDilate(record->cmdbuf, dilate);
    }
}

void Nvnl_nvnCommandBufferSetSubpixelPrecisionBias(NVNcommandBuffer *cmd, int x, int y) {
    const LayerCommandBuffer *const record =
        CmdOf(cmd, "nvnCommandBufferSetSubpixelPrecisionBias");
    if (record == NULL) {
        return;
    }
    if (x < 0 || x > 8 || y < 0 || y > 8) {
        LayerComplain("nvnCommandBufferSetSubpixelPrecisionBias: %d and %d bits, where each is "
                      "0 to 8",
                      x, y);
        return;
    }
    dkCmdBufSetSubpixelPrecisionBias(record->cmdbuf, (uint32_t)x, (uint32_t)y);
}

// ---- tessellation, each deko3d's own command

void Nvnl_nvnCommandBufferSetPatchSize(NVNcommandBuffer *cmd, int size) {
    const LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferSetPatchSize");
    if (record == NULL) {
        return;
    }
    if (size < 1 || size > 32) {
        LayerComplain("nvnCommandBufferSetPatchSize: %d vertices a patch, where a patch has 1 "
                      "to 32",
                      size);
        return;
    }
    dkCmdBufSetPatchSize(record->cmdbuf, (uint32_t)size);
}

void Nvnl_nvnCommandBufferSetOuterTessellationLevels(NVNcommandBuffer *cmd,
                                                     const float *levels) {
    const LayerCommandBuffer *const record =
        CmdOf(cmd, "nvnCommandBufferSetOuterTessellationLevels");
    if (record != NULL) {
        dkCmdBufSetTessOuterLevels(record->cmdbuf, levels[0], levels[1], levels[2], levels[3]);
    }
}

void Nvnl_nvnCommandBufferSetInnerTessellationLevels(NVNcommandBuffer *cmd,
                                                     const float *levels) {
    const LayerCommandBuffer *const record =
        CmdOf(cmd, "nvnCommandBufferSetInnerTessellationLevels");
    if (record != NULL) {
        dkCmdBufSetTessInnerLevels(record->cmdbuf, levels[0], levels[1]);
    }
}
