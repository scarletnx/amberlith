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

/// A unit of the part of the command memory the layer keeps, LAYER_HANDLE_BYTES of it: its
/// offset in that part, or -1 with a complaint naming @p what when the part is used up. What
/// goes there lives exactly as long as the list.
static long Unit(LayerCommandBuffer *record, const char *what) {
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

/// Records @p count engine words into @p record's list. The words and the control list that
/// names them go in a unit of the layer's memory, which the queue reads them from when the
/// list is submitted.
static bool RawMethods(LayerCommandBuffer *record, const uint32_t *words, uint32_t count,
                       const char *what) {
    LayerDraw *const draw = record->draw;
    if (count == 0u || count > RAW_WORDS_MAX) {
        LayerComplain("%s: %u engine words, where a unit of the layer's memory holds 1 to %u",
                      what, (unsigned)count, (unsigned)RAW_WORDS_MAX);
        return false;
    }
    const long unit = Unit(record, what);
    if (unit < 0) {
        return false;
    }
    uint8_t *const at = draw->handle_cpu + unit;
    const RawList list = { 3u | (uint64_t)1u << 32,
                           draw->handle_memory + (uint64_t)unit + sizeof(RawList), count, 0u,
                           0u };
    memcpy(at, &list, sizeof(list));
    memcpy(at + sizeof(list), words, (size_t)count * 4u);
    dkCmdBufCallList(record->cmdbuf, (DkCmdList)(uintptr_t)at);
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
