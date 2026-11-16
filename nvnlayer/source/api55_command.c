// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "api55.h"

// The API 55 build's command buffer past the binds and the draws: what a list reports to memory
// (counters, values) and whether what follows is drawn at all, each as the deko3d commands that
// do what NVN says, or, where deko3d has no command for it, as the engine methods themselves.

// ---- engine methods deko3d has no command for

// A method header: the method, its subchannel, how many words follow, and the mode that puts
// each word in the next register (the 3D engine is subchannel 0).
#define METHOD_INCREASING(method, count) ((uint32_t)(method) | (uint32_t)(count) << 16 | 1u << 29)

// deko3d's control list for words of the program's own (its source/dk_ctrlcmd.h): one header
// naming one command entry, the entry, and a return.
typedef struct {
    uint64_t header;      // type 3 (a list of entries) in bits 0-7, one entry in bits 32-63
    uint64_t entry_iova;  // where the words are, for the graphics unit
    uint32_t entry_words; // how many
    uint32_t entry_flags; // none
    uint64_t end;         // type 0 (return)
} RawList;
_Static_assert(sizeof(RawList) == 32, "a raw control list is four 64-bit words");
#define RAW_WORDS_MAX ((LAYER_HANDLE_BYTES - (uint32_t)sizeof(RawList)) / 4u)

/// Records @p count engine words into @p record's list. The words and the control list that
/// names them go in the part of the command memory the layer keeps (a unit of it each), so they
/// live exactly as long as the list, which the queue reads them from when it is submitted.
static bool RawMethods(LayerCommandBuffer *record, const uint32_t *words, uint32_t count,
                       const char *what) {
    LayerDraw *const draw = record->draw;
    if (count == 0u || count > RAW_WORDS_MAX) {
        LayerComplain("%s: %u engine words, where a unit of the layer's memory holds 1 to %u",
                      what, (unsigned)count, (unsigned)RAW_WORDS_MAX);
        return false;
    }
    if (draw->handle_used + LAYER_HANDLE_BYTES > draw->handle_size) {
        LayerComplain("%s: the quarter of the command memory the layer keeps, 0x%x bytes, is "
                      "full after %u units of 0x%x: the list needs more command memory",
                      what, (unsigned)draw->handle_size,
                      (unsigned)(draw->handle_used / LAYER_HANDLE_BYTES),
                      (unsigned)LAYER_HANDLE_BYTES);
        return false;
    }
    uint8_t *const at = draw->handle_cpu + draw->handle_used;
    const RawList list = { 3u | (uint64_t)1u << 32,
                           draw->handle_memory + draw->handle_used + sizeof(RawList), count, 0u,
                           0u };
    memcpy(at, &list, sizeof(list));
    memcpy(at + sizeof(list), words, (size_t)count * 4u);
    dkCmdBufCallList(record->cmdbuf, (DkCmdList)(uintptr_t)at);
    draw->handle_used += LAYER_HANDLE_BYTES;
    return true;
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

// The 3D engine's render enable, which deko3d never writes: an address in two words and a mode.
// The two conditional modes compare the 64-bit words at the address and 16 bytes past it, the
// counts of two counter reports one after the other, which is how NVN uses them.
#define METHOD_RENDER_ENABLE 0x554u
#define RENDER_FALSE 0u
#define RENDER_TRUE 1u
#define RENDER_IF_EQUAL 3u
#define RENDER_IF_NOT_EQUAL 4u

static void RenderEnable(NVNcommandBuffer *cmd, uint64_t address, uint32_t mode,
                         const char *what) {
    LayerCommandBuffer *const record = CmdOf(cmd, what);
    if (record != NULL) {
        const uint32_t words[4] = { METHOD_INCREASING(METHOD_RENDER_ENABLE, 3),
                                    (uint32_t)(address >> 32), (uint32_t)address, mode };
        RawMethods(record, words, 4, what);
    }
}

void Nvnl_nvnCommandBufferSetRenderEnable(NVNcommandBuffer *cmd, NVNboolean enable) {
    RenderEnable(cmd, 0u, enable ? RENDER_TRUE : RENDER_FALSE, "nvnCommandBufferSetRenderEnable");
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
                 mode == NVN_CONDITIONAL_RENDER_MODE_RENDER_IF_EQUAL ? RENDER_IF_EQUAL
                                                                      : RENDER_IF_NOT_EQUAL,
                 "nvnCommandBufferSetRenderEnableConditional");
}
