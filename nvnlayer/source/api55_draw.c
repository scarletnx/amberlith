// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stddef.h>

#include "api55.h"

// The API 55 build's drawing: programs, samplers, the texture and sampler pools, the state
// objects, the binds and the draw, each as the deko3d commands that do what NVN says.
//
// One thing here is the layer's own design and not a translation: where a program finds its
// uniform buffers and its textures. A program compiled by uam reads the constant buffers deko3d
// binds, and one compiled for NVN reads the ones a console's NVN binds; the layer binds NVN's.
// Uniform buffer slot r of a stage is constant buffer 3 + r, deko3d's uniform buffer r + 1; and
// the stage's texture handles are constant buffer 2, deko3d's uniform buffer 0, the handle of
// texture slot r in the word at 0x20 + 8 * r. A shader written to those two rules
// (docs/NvnLayer.md, "What a shader may use") is then one binary that draws the same through
// this layer, through deko3d alone, and through ScarletNX's NVN.

// ---- programs

/// deko3d's stage for an NVN one, or -1 for a stage the layer does not draw with.
static int StageOf(NVNshaderStage stage) {
    switch (stage) {
    case NVN_SHADER_STAGE_VERTEX:
        return DkStage_Vertex;
    case NVN_SHADER_STAGE_FRAGMENT:
        return DkStage_Fragment;
    case NVN_SHADER_STAGE_GEOMETRY:
        return DkStage_Geometry;
    case NVN_SHADER_STAGE_TESS_CONTROL:
        return DkStage_TessCtrl;
    case NVN_SHADER_STAGE_TESS_EVALUATION:
        return DkStage_TessEval;
    default:
        return -1;
    }
}

static LayerProgram *ProgramOf(const NVNprogram *program, const char *what) {
    LayerProgram *const record = Record(program, what);
    if (record != NULL && record->magic != MAGIC_PROGRAM) {
        LayerComplain("%s: the program was not initialized", what);
        return NULL;
    }
    return record;
}

NVNboolean Nvnl_nvnProgramInitialize(NVNprogram *program, NVNdevice *device) {
    LayerProgram *const record = Record(program, "nvnProgramInitialize");
    if (record == NULL || DeviceOf(device, "nvnProgramInitialize") == NULL) {
        return 0;
    }
    memset(program, 0, sizeof(*program));
    record->magic = MAGIC_PROGRAM;
    return 1;
}

void Nvnl_nvnProgramFinalize(NVNprogram *program) {
    LayerProgram *const record = ProgramOf(program, "nvnProgramFinalize");
    if (record != NULL) {
        record->magic = 0;
    }
}

void Nvnl_nvnProgramSetSampleShading(NVNprogram *program, int mode) {
    if (ProgramOf(program, "nvnProgramSetSampleShading") == NULL) {
        return;
    }
    // Mode 0 is as the program's own code says, which is how deko3d runs every program.
    if (mode != 0) {
        LayerLack("nvnProgramSetSampleShading: mode %d, a fragment stage run for every sample "
                  "whatever its code says, and deko3d has no call for that (0 is as the code "
                  "says)",
                  mode);
    }
}

/// The subtile of the tiled cache a program asks for: none, which is what a driver's
/// nvnProgramSetShaders leaves and nothing in NVN changes.
int Nvnl_nvnProgramGetSubtileSize(const NVNprogram *program) {
    (void)ProgramOf(program, "nvnProgramGetSubtileSize");
    return 0;
}

/// A program takes a linkage map for each of its stages that declares subroutines, and a driver
/// answers false to anything else: no map, or a map where no stage declares any. A stage
/// declares them in its control section (byte 0x7c0 of it, as ScarletNX reads one), and the
/// section the layer writes of a DKSH module has nought there. So no program here has such a
/// stage, and every map is refused as a driver refuses it.
NVNboolean Nvnl_nvnProgramSetSubroutineLinkage(NVNprogram *program, int count,
                                               const NVNsubroutineLinkageMapPtr *maps) {
    (void)count;
    (void)maps;
    (void)ProgramOf(program, "nvnProgramSetSubroutineLinkage");
    return 0;
}

NVNboolean Nvnl_nvnProgramSetShaders(NVNprogram *program, int count,
                                     const NVNshaderData *shaders) {
    return LayerProgramSetShaders(program, count, shaders, "nvnProgramSetShaders");
}

NVNboolean LayerProgramSetShaders(NVNprogram *program, int count, const NVNshaderData *shaders,
                                  const char *WHAT) {
    LayerProgram *const record = ProgramOf(program, WHAT);
    if (record == NULL) {
        return 0;
    }
    if (count < 1 || count > LAYER_STAGES || shaders == NULL) {
        LayerComplain("%s: %d stages, and a program has 1 to %d", WHAT, count, LAYER_STAGES);
        return 0;
    }
    const DkShader *made[LAYER_STAGES] = { NULL };
    for (int i = 0; i < count; ++i) {
        // The section is the program's memory, which the layer also keeps the DkShader in: an
        // NVN program object is too small for five of them.
        uint8_t *const section = (uint8_t *)(uintptr_t)shaders[i].control;
        LayerControlTail *const tail =
            section != NULL ? (LayerControlTail *)(void *)(section + LAYER_CONTROL_TAIL) : NULL;
        if (section == NULL || ((uintptr_t)section & 7u) != 0u ||
            tail->tag != LAYER_CONTROL_TAG) {
            LayerComplain("%s: stage %d's control section at %p is not one "
                          "NvnLayerShaderControl made, which is the kind the layer takes",
                          WHAT, i, (void *)section);
            return 0;
        }
        uint32_t offset = 0;
        const DkMemBlock block = LayerCodePool(shaders[i].data, &offset);
        if (block == NULL || (offset & (DK_SHADER_CODE_ALIGNMENT - 1u)) != 0u ||
            (uint64_t)offset + tail->dksh[3] >
                dkMemBlockGetSize(block) - DK_SHADER_CODE_UNUSABLE_SIZE) {
            LayerComplain("%s: stage %d's code at 0x%llx is not 0x%x bytes on a 0x%x boundary of "
                          "a SHADER_CODE pool, clear of the pool's last 0x%x bytes",
                          WHAT, i, (unsigned long long)shaders[i].data, (unsigned)tail->dksh[3],
                          (unsigned)DK_SHADER_CODE_ALIGNMENT,
                          (unsigned)DK_SHADER_CODE_UNUSABLE_SIZE);
            return 0;
        }
        const uint32_t stage = tail->program[0];
        if (stage >= LAYER_STAGES || made[stage] != NULL) {
            LayerComplain("%s: stage %d is a second program of NVN stage %u", WHAT, i,
                          (unsigned)stage);
            return 0;
        }
        DkShaderMaker maker;
        dkShaderMakerDefaults(&maker, block, offset);
        maker.control = tail->dksh;
        maker.programId = 0;
        DkShader *const shader = (DkShader *)(void *)tail->shader;
        dkShaderInitialize(shader, &maker);
        if (!dkShaderIsValid(shader)) {
            LayerComplain("%s: deko3d did not take stage %d's program", WHAT, i);
            return 0;
        }
        made[stage] = shader;
    }
    memcpy(record->shaders, made, sizeof(made));
    return 1;
}

// ---- transform feedback, bound with the program

_Static_assert(LAYER_SECTION_CAPTURE_BUFFERS == DK_NUM_TRANSFORM_FEEDBACK_BUFFERS &&
                   LAYER_CAPTURE_WORDS == DK_MAX_TRANSFORM_FEEDBACK_VARYINGS,
               "a section's capture buffers and their words are deko3d's");
_Static_assert(LAYER_CAPTURE_SLOT_POSITION(3) == DK_TFB_SLOT_POSITION(3) &&
                   LAYER_CAPTURE_SLOT_GENERIC(31, 3) == DK_TFB_SLOT_GENERIC(31, 3),
               "a captured word's output slot is deko3d's");

/// The section the layer keeps @p shader in (LayerProgramSetShaders), where the stage's capture
/// is said.
static const uint8_t *SectionOf(const DkShader *shader) {
    return (const uint8_t *)(const void *)shader - offsetof(LayerControlTail, shader) -
           LAYER_CONTROL_TAIL;
}

/// The capture of @p from's last stage before the rasterizer among @p stages, as NVN's program
/// carries it: its layouts bound with the program, or a capture bound before taken off when the
/// stage captures nothing. A bind of none of those stages leaves the capture as it was.
static void BindCapture(LayerCommandBuffer *record, const LayerProgram *from, int stages) {
    static const int LAST[3] = { NVN_SHADER_STAGE_GEOMETRY, NVN_SHADER_STAGE_TESS_EVALUATION,
                                 NVN_SHADER_STAGE_VERTEX };
    LayerDraw *const draw = record->draw;
    const DkShader *last = NULL;
    bool named = false;
    for (int i = 0; i < 3 && last == NULL; ++i) {
        if ((stages >> LAST[i] & 1) != 0) {
            named = true;
            last = from->shaders[LAST[i]];
        }
    }
    if (!named) {
        return;
    }
    LayerCapture buffers[LAYER_SECTION_CAPTURE_BUFFERS];
    bool captures = false;
    if (last != NULL &&
        !LayerSectionCapture(SectionOf(last), buffers, &captures, "nvnCommandBufferBindProgram")) {
        return;
    }
    if (!captures) {
        if (draw->capturing) {
            dkCmdBufBindTransformFeedbackLayouts(record->cmdbuf, NULL, 0);
            draw->capturing = false;
            draw->capture_stride = 0;
        }
        return;
    }
    DkTransformFeedbackLayout layouts[DK_NUM_TRANSFORM_FEEDBACK_BUFFERS];
    const DkTransformFeedbackLayout *bound[DK_NUM_TRANSFORM_FEEDBACK_BUFFERS];
    for (uint32_t i = 0; i < DK_NUM_TRANSFORM_FEEDBACK_BUFFERS; ++i) {
        memset(&layouts[i], 0, sizeof(layouts[i]));
        layouts[i].stride = buffers[i].stride;
        layouts[i].stream = buffers[i].stream;
        layouts[i].numVaryings = buffers[i].words;
        memcpy(layouts[i].varyings, buffers[i].slots, buffers[i].words);
        bound[i] = &layouts[i];
    }
    dkCmdBufBindTransformFeedbackLayouts(record->cmdbuf, bound, DK_NUM_TRANSFORM_FEEDBACK_BUFFERS);
    draw->capturing = true;
    draw->capture_stride = buffers[0].stride;
}

void Nvnl_nvnCommandBufferBindProgram(NVNcommandBuffer *cmd, const NVNprogram *program,
                                      int stages) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferBindProgram");
    const LayerProgram *const from = ProgramOf(program, "nvnCommandBufferBindProgram");
    if (record == NULL || from == NULL) {
        return;
    }
    if ((stages & ~NVN_SHADER_STAGE_ALL_GRAPHICS_BITS) != 0) {
        LayerComplain("nvnCommandBufferBindProgram: stages 0x%x, and the layer binds the "
                      "graphics stages, 0x%x",
                      (unsigned)stages, (unsigned)NVN_SHADER_STAGE_ALL_GRAPHICS_BITS);
        return;
    }
    // A stage named and not in the program is unbound, as deko3d does for a stage of the mask
    // it is given no shader for.
    const DkShader *bound[LAYER_STAGES];
    uint32_t count = 0;
    uint32_t mask = 0;
    for (int stage = 0; stage < LAYER_STAGES; ++stage) {
        if ((stages >> stage & 1) == 0) {
            continue;
        }
        mask |= 1u << StageOf((NVNshaderStage)stage);
        if (from->shaders[stage] != NULL) {
            bound[count++] = from->shaders[stage];
        }
    }
    dkCmdBufBindShaders(record->cmdbuf, mask, bound, count);
    BindCapture(record, from, stages);
}

// ---- samplers

void Nvnl_nvnSamplerBuilderSetDefaults(NVNsamplerBuilder *builder) {
    // The device stays, as NVN's own builder keeps it. The filters and the wrap modes start at
    // nearest and clamp to edge here; a program sets the ones it relies on, as it cannot ask
    // what a driver's defaults are. The levels are all of them and nothing is compared.
    LayerSamplerBuilder *const record = Record(builder, "nvnSamplerBuilderSetDefaults");
    if (record == NULL) {
        return;
    }
    NVNdevice *const device = record->device;
    memset(builder, 0, sizeof(*builder));
    record->device = device;
    record->state.min = NVN_MIN_FILTER_NEAREST;
    record->state.mag = NVN_MAG_FILTER_NEAREST;
    record->state.wrap[0] = record->state.wrap[1] = record->state.wrap[2] =
        NVN_WRAP_MODE_CLAMP_TO_EDGE;
    record->state.compare_mode = NVN_COMPARE_MODE_NONE;
    record->state.compare_func = NVN_COMPARE_FUNC_LESS;
    record->state.reduction = NVN_SAMPLER_REDUCTION_AVERAGE;
    record->state.lod_min = 0.0f;
    record->state.lod_max = 1000.0f;
    record->state.anisotropy = 1.0f;
}

void Nvnl_nvnSamplerBuilderSetDevice(NVNsamplerBuilder *builder, NVNdevice *device) {
    LayerSamplerBuilder *const record = Record(builder, "nvnSamplerBuilderSetDevice");
    if (record != NULL) {
        record->device = device;
    }
}

NVNdevice *Nvnl_nvnSamplerBuilderGetDevice(const NVNsamplerBuilder *builder) {
    const LayerSamplerBuilder *const record = Record(builder, "nvnSamplerBuilderGetDevice");
    return record != NULL ? record->device : NULL;
}

/// A builder's state, to set, or NULL with a complaint.
static LayerSamplerState *BuilderState(NVNsamplerBuilder *builder, const char *what) {
    LayerSamplerBuilder *const record = Record(builder, what);
    return record != NULL ? &record->state : NULL;
}

/// The state a builder holds, or a sampler was made with: NULL with a complaint.
static const LayerSamplerState *StateOfBuilder(const NVNsamplerBuilder *builder,
                                               const char *what) {
    const LayerSamplerBuilder *const record = Record(builder, what);
    return record != NULL ? &record->state : NULL;
}

static const LayerSamplerState *StateOfSampler(const NVNsampler *sampler, const char *what) {
    const LayerSampler *const record = Record(sampler, what);
    if (record != NULL && record->magic != MAGIC_SAMPLER) {
        LayerComplain("%s: the sampler was not initialized", what);
        return NULL;
    }
    return record != NULL ? &record->state : NULL;
}

void Nvnl_nvnSamplerBuilderSetMinMagFilter(NVNsamplerBuilder *builder, NVNminFilter min,
                                           NVNmagFilter mag) {
    LayerSamplerState *const state = BuilderState(builder, "nvnSamplerBuilderSetMinMagFilter");
    if (state != NULL) {
        state->min = (uint8_t)min;
        state->mag = (uint8_t)mag;
    }
}

void Nvnl_nvnSamplerBuilderSetWrapMode(NVNsamplerBuilder *builder, NVNwrapMode s, NVNwrapMode t,
                                       NVNwrapMode r) {
    LayerSamplerState *const state = BuilderState(builder, "nvnSamplerBuilderSetWrapMode");
    if (state != NULL) {
        state->wrap[0] = (uint8_t)s;
        state->wrap[1] = (uint8_t)t;
        state->wrap[2] = (uint8_t)r;
    }
}

void Nvnl_nvnSamplerBuilderSetCompare(NVNsamplerBuilder *builder, NVNcompareMode mode,
                                      NVNcompareFunc func) {
    LayerSamplerState *const state = BuilderState(builder, "nvnSamplerBuilderSetCompare");
    if (state != NULL) {
        state->compare_mode = (uint8_t)mode;
        state->compare_func = (uint8_t)func;
    }
}

void Nvnl_nvnSamplerBuilderSetReductionFilter(NVNsamplerBuilder *builder,
                                              NVNsamplerReduction filter) {
    LayerSamplerState *const state = BuilderState(builder, "nvnSamplerBuilderSetReductionFilter");
    if (state != NULL) {
        state->reduction = (uint8_t)filter;
    }
}

void Nvnl_nvnSamplerBuilderSetLodClamp(NVNsamplerBuilder *builder, float min, float max) {
    LayerSamplerState *const state = BuilderState(builder, "nvnSamplerBuilderSetLodClamp");
    if (state != NULL) {
        state->lod_min = min;
        state->lod_max = max;
    }
}

#define SAMPLER_FLOAT(name, field)                                                                \
    void Nvnl_nvnSamplerBuilderSet##name(NVNsamplerBuilder *builder, float value) {               \
        LayerSamplerState *const state = BuilderState(builder, "nvnSamplerBuilderSet" #name);     \
        if (state != NULL) {                                                                      \
            state->field = value;                                                                 \
        }                                                                                         \
    }                                                                                             \
    float Nvnl_nvnSamplerBuilderGet##name(const NVNsamplerBuilder *builder) {                     \
        const LayerSamplerState *const state =                                                    \
            StateOfBuilder(builder, "nvnSamplerBuilderGet" #name);                                \
        return state != NULL ? state->field : 0.0f;                                               \
    }
SAMPLER_FLOAT(LodBias, lod_bias)
SAMPLER_FLOAT(LodSnap, lod_snap)
SAMPLER_FLOAT(MaxAnisotropy, anisotropy)
#undef SAMPLER_FLOAT

// A border colour is four words, whichever of the three kinds set them, as the graphics unit's
// sampler has it: a getter of another kind gives the same words.
#define SAMPLER_BORDER(suffix, type)                                                              \
    void Nvnl_nvnSamplerBuilderSetBorderColor##suffix(NVNsamplerBuilder *builder,                 \
                                                      const type *color) {                        \
        LayerSamplerState *const state =                                                          \
            BuilderState(builder, "nvnSamplerBuilderSetBorderColor" #suffix);                     \
        if (state != NULL) {                                                                      \
            memcpy(state->border, color, sizeof(state->border));                                  \
        }                                                                                         \
    }                                                                                             \
    void Nvnl_nvnSamplerBuilderGetBorderColor##suffix(const NVNsamplerBuilder *builder,           \
                                                      type *color) {                              \
        const LayerSamplerState *const state =                                                    \
            StateOfBuilder(builder, "nvnSamplerBuilderGetBorderColor" #suffix);                   \
        if (state != NULL) {                                                                      \
            memcpy(color, state->border, sizeof(state->border));                                  \
        }                                                                                         \
    }                                                                                             \
    void Nvnl_nvnSamplerGetBorderColor##suffix(const NVNsampler *sampler, type *color) {          \
        const LayerSamplerState *const state =                                                    \
            StateOfSampler(sampler, "nvnSamplerGetBorderColor" #suffix);                          \
        if (state != NULL) {                                                                      \
            memcpy(color, state->border, sizeof(state->border));                                  \
        }                                                                                         \
    }
SAMPLER_BORDER(, float)
SAMPLER_BORDER(i, int)
SAMPLER_BORDER(ui, uint32_t)
#undef SAMPLER_BORDER

// What a builder holds and what a sampler was made with, by the same getters.
#define SAMPLER_GETTERS(object, type, state_of)                                                   \
    void Nvnl_nvn##object##GetMinMagFilter(const type *from, NVNminFilter *min,                   \
                                           NVNmagFilter *mag) {                                   \
        const LayerSamplerState *const state = state_of(from, "nvn" #object "GetMinMagFilter");   \
        if (state != NULL) {                                                                      \
            *min = (NVNminFilter)state->min;                                                      \
            *mag = (NVNmagFilter)state->mag;                                                      \
        }                                                                                         \
    }                                                                                             \
    void Nvnl_nvn##object##GetWrapMode(const type *from, NVNwrapMode *s, NVNwrapMode *t,          \
                                       NVNwrapMode *r) {                                          \
        const LayerSamplerState *const state = state_of(from, "nvn" #object "GetWrapMode");       \
        if (state != NULL) {                                                                      \
            *s = (NVNwrapMode)state->wrap[0];                                                     \
            *t = (NVNwrapMode)state->wrap[1];                                                     \
            *r = (NVNwrapMode)state->wrap[2];                                                     \
        }                                                                                         \
    }                                                                                             \
    void Nvnl_nvn##object##GetCompare(const type *from, NVNcompareMode *mode,                     \
                                      NVNcompareFunc *func) {                                     \
        const LayerSamplerState *const state = state_of(from, "nvn" #object "GetCompare");        \
        if (state != NULL) {                                                                      \
            *mode = (NVNcompareMode)state->compare_mode;                                          \
            *func = (NVNcompareFunc)state->compare_func;                                          \
        }                                                                                         \
    }                                                                                             \
    void Nvnl_nvn##object##GetLodClamp(const type *from, float *min, float *max) {                \
        const LayerSamplerState *const state = state_of(from, "nvn" #object "GetLodClamp");       \
        if (state != NULL) {                                                                      \
            *min = state->lod_min;                                                                \
            *max = state->lod_max;                                                                \
        }                                                                                         \
    }                                                                                             \
    NVNsamplerReduction Nvnl_nvn##object##GetReductionFilter(const type *from) {                  \
        const LayerSamplerState *const state =                                                    \
            state_of(from, "nvn" #object "GetReductionFilter");                                   \
        return state != NULL ? (NVNsamplerReduction)state->reduction                              \
                             : NVN_SAMPLER_REDUCTION_AVERAGE;                                     \
    }
SAMPLER_GETTERS(SamplerBuilder, NVNsamplerBuilder, StateOfBuilder)
SAMPLER_GETTERS(Sampler, NVNsampler, StateOfSampler)
#undef SAMPLER_GETTERS

float Nvnl_nvnSamplerGetLodBias(const NVNsampler *sampler) {
    const LayerSamplerState *const state = StateOfSampler(sampler, "nvnSamplerGetLodBias");
    return state != NULL ? state->lod_bias : 0.0f;
}

float Nvnl_nvnSamplerGetMaxAnisotropy(const NVNsampler *sampler) {
    const LayerSamplerState *const state = StateOfSampler(sampler, "nvnSamplerGetMaxAnisotropy");
    return state != NULL ? state->anisotropy : 0.0f;
}

NVNboolean Nvnl_nvnSamplerCompare(const NVNsampler *a, const NVNsampler *b) {
    const LayerSamplerState *const x = StateOfSampler(a, "nvnSamplerCompare");
    const LayerSamplerState *const y = StateOfSampler(b, "nvnSamplerCompare");
    // The state's bytes are all set by name (the builder's defaults clear the rest), so two
    // samplers of one state hold the same bytes.
    return x != NULL && y != NULL && memcmp(x, y, sizeof(*x)) == 0;
}

/// deko3d's wrap mode for NVN's, or false for a number that is none.
static bool WrapOf(uint8_t mode, DkWrapMode *wrap) {
    switch (mode) {
    case NVN_WRAP_MODE_CLAMP:
        *wrap = DkWrapMode_Clamp;
        return true;
    case NVN_WRAP_MODE_REPEAT:
        *wrap = DkWrapMode_Repeat;
        return true;
    case NVN_WRAP_MODE_MIRROR_CLAMP:
        *wrap = DkWrapMode_MirrorClamp;
        return true;
    case NVN_WRAP_MODE_MIRROR_CLAMP_TO_EDGE:
        *wrap = DkWrapMode_MirrorClampToEdge;
        return true;
    case NVN_WRAP_MODE_MIRROR_CLAMP_TO_BORDER:
        *wrap = DkWrapMode_MirrorClampToBorder;
        return true;
    case NVN_WRAP_MODE_CLAMP_TO_BORDER:
        *wrap = DkWrapMode_ClampToBorder;
        return true;
    case NVN_WRAP_MODE_MIRRORED_REPEAT:
        *wrap = DkWrapMode_MirroredRepeat;
        return true;
    case NVN_WRAP_MODE_CLAMP_TO_EDGE:
        *wrap = DkWrapMode_ClampToEdge;
        return true;
    default:
        return false;
    }
}

/// deko3d's sampler for @p state; false with a complaint naming the number that is none of
/// NVN's.
static bool SamplerOf(const LayerSamplerState *state, DkSampler *sampler, const char *what) {
    dkSamplerDefaults(sampler);
    switch (state->min) {
    case NVN_MIN_FILTER_NEAREST:
    case NVN_MIN_FILTER_NEAREST_MIPMAP_NEAREST:
    case NVN_MIN_FILTER_NEAREST_MIPMAP_LINEAR:
        sampler->minFilter = DkFilter_Nearest;
        break;
    case NVN_MIN_FILTER_LINEAR:
    case NVN_MIN_FILTER_LINEAR_MIPMAP_NEAREST:
    case NVN_MIN_FILTER_LINEAR_MIPMAP_LINEAR:
        sampler->minFilter = DkFilter_Linear;
        break;
    default:
        LayerComplain("%s: min filter %u is none of NVN's", what, (unsigned)state->min);
        return false;
    }
    switch (state->min) {
    case NVN_MIN_FILTER_NEAREST_MIPMAP_NEAREST:
    case NVN_MIN_FILTER_LINEAR_MIPMAP_NEAREST:
        sampler->mipFilter = DkMipFilter_Nearest;
        break;
    case NVN_MIN_FILTER_NEAREST_MIPMAP_LINEAR:
    case NVN_MIN_FILTER_LINEAR_MIPMAP_LINEAR:
        sampler->mipFilter = DkMipFilter_Linear;
        break;
    default:
        sampler->mipFilter = DkMipFilter_None;
        break;
    }
    if (state->mag != NVN_MAG_FILTER_NEAREST && state->mag != NVN_MAG_FILTER_LINEAR) {
        LayerComplain("%s: mag filter %u is none of NVN's", what, (unsigned)state->mag);
        return false;
    }
    sampler->magFilter = state->mag == NVN_MAG_FILTER_LINEAR ? DkFilter_Linear : DkFilter_Nearest;
    for (int i = 0; i < 3; ++i) {
        if (!WrapOf(state->wrap[i], &sampler->wrapMode[i])) {
            LayerComplain("%s: wrap mode %u is none of NVN's", what, (unsigned)state->wrap[i]);
            return false;
        }
    }
    if (state->compare_mode > NVN_COMPARE_MODE_COMPARE_R_TO_TEXTURE ||
        state->compare_func < NVN_COMPARE_FUNC_NEVER ||
        state->compare_func > NVN_COMPARE_FUNC_ALWAYS ||
        state->reduction > NVN_SAMPLER_REDUCTION_MAX) {
        LayerComplain("%s: compare mode %u with function %u and reduction filter %u, of which "
                      "one is none of NVN's",
                      what, (unsigned)state->compare_mode, (unsigned)state->compare_func,
                      (unsigned)state->reduction);
        return false;
    }
    // NVN's compare functions and reduction filters have deko3d's numbers.
    sampler->compareEnable = state->compare_mode == NVN_COMPARE_MODE_COMPARE_R_TO_TEXTURE;
    sampler->compareOp = (DkCompareOp)state->compare_func;
    sampler->reductionMode = (DkSamplerReduction)state->reduction;
    sampler->lodClampMin = state->lod_min;
    sampler->lodClampMax = state->lod_max;
    sampler->lodBias = state->lod_bias;
    sampler->lodSnap = state->lod_snap;
    sampler->maxAnisotropy = state->anisotropy;
    for (int i = 0; i < 4; ++i) {
        sampler->borderColor[i].value_ui = state->border[i];
    }
    return true;
}

NVNboolean Nvnl_nvnSamplerInitialize(NVNsampler *sampler, const NVNsamplerBuilder *builder) {
    LayerSampler *const record = Record(sampler, "nvnSamplerInitialize");
    const LayerSamplerBuilder *const from = Record(builder, "nvnSamplerInitialize");
    if (record == NULL || from == NULL ||
        DeviceOf(from->device, "nvnSamplerInitialize") == NULL) {
        return 0;
    }
    DkSampler probe;
    if (!SamplerOf(&from->state, &probe, "nvnSamplerInitialize")) {
        return 0;
    }
    memset(sampler, 0, sizeof(*sampler));
    record->state = from->state;
    record->magic = MAGIC_SAMPLER;
    return 1;
}

void Nvnl_nvnSamplerFinalize(NVNsampler *sampler) {
    LayerSampler *const record = Record(sampler, "nvnSamplerFinalize");
    if (record != NULL && record->magic == MAGIC_SAMPLER) {
        record->magic = 0;
    }
}

// ---- the texture pool and the sampler pool

/// A pool of @p count descriptors at @p offset of @p memory, under @p magic.
static NVNboolean DescriptorPool(void *pool, uint32_t magic, const NVNmemoryPool *memory,
                                 ptrdiff_t offset, int count, const char *what) {
    LayerDescriptorPool *const record = Record(pool, what);
    const LayerPool *const from = PoolOf(memory, what);
    if (record == NULL || from == NULL) {
        return 0;
    }
    // A descriptor is 32 bytes in NVN and in deko3d, the graphics unit's own.
    if (count <= 0 || offset < 0 || (offset & (DK_IMAGE_DESCRIPTOR_ALIGNMENT - 1)) != 0 ||
        (uint64_t)offset + (uint64_t)count * 32u > dkMemBlockGetSize(from->block) ||
        (from->flags & NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS) != 0) {
        LayerComplain("%s: %d descriptors at offset 0x%llx of a pool of 0x%x bytes with flags "
                      "0x%x: they have to fit it, on a 0x%x boundary, where the processor "
                      "writes",
                      what, count, (unsigned long long)offset,
                      (unsigned)dkMemBlockGetSize(from->block), (unsigned)from->flags,
                      (unsigned)DK_IMAGE_DESCRIPTOR_ALIGNMENT);
        return 0;
    }
    memset(record, 0, sizeof(*record));
    record->magic = magic;
    record->count = count;
    record->block = from->block;
    record->offset = (uint32_t)offset;
    record->memory = memory;
    return 1;
}

static LayerDescriptorPool *DescriptorPoolOf(const void *pool, uint32_t magic, const char *what) {
    LayerDescriptorPool *const record = Record(pool, what);
    if (record != NULL && record->magic != magic) {
        LayerComplain("%s: the pool was not initialized", what);
        return NULL;
    }
    return record;
}

/// Where descriptor @p id of @p record is, or NULL with a complaint for one outside the pool.
static void *Descriptor(const LayerDescriptorPool *record, int id, const char *what) {
    if (id < 0 || id >= record->count) {
        LayerComplain("%s: descriptor %d of a pool of %d", what, id, record->count);
        return NULL;
    }
    return (uint8_t *)dkMemBlockGetCpuAddr(record->block) + record->offset + (uint32_t)id * 32u;
}

NVNboolean Nvnl_nvnTexturePoolInitialize(NVNtexturePool *pool, const NVNmemoryPool *memory,
                                         ptrdiff_t offset, int count) {
    return DescriptorPool(pool, MAGIC_TEXPOOL, memory, offset, count,
                          "nvnTexturePoolInitialize");
}

void Nvnl_nvnTexturePoolFinalize(NVNtexturePool *pool) {
    LayerDescriptorPool *const record =
        DescriptorPoolOf(pool, MAGIC_TEXPOOL, "nvnTexturePoolFinalize");
    if (record != NULL) {
        record->magic = 0;
    }
}

void Nvnl_nvnTexturePoolRegisterTexture(const NVNtexturePool *pool, int id,
                                        const NVNtexture *texture, const NVNtextureView *view) {
    static const char *const WHAT = "nvnTexturePoolRegisterTexture";
    const LayerDescriptorPool *const record = DescriptorPoolOf(pool, MAGIC_TEXPOOL, WHAT);
    const LayerTexture *const from = TextureOf(texture, WHAT);
    if (record == NULL || from == NULL) {
        return;
    }
    if (view != NULL) {
        LayerComplain("%s: a texture view, which the layer does not take yet", WHAT);
        return;
    }
    DkImageDescriptor *const descriptor = Descriptor(record, id, WHAT);
    if (descriptor == NULL) {
        return;
    }
    DkImageView whole;
    dkImageViewDefaults(&whole, &from->image);
    dkImageDescriptorInitialize(descriptor, &whole, false, false);
}

NVNboolean Nvnl_nvnSamplerPoolInitialize(NVNsamplerPool *pool, const NVNmemoryPool *memory,
                                         ptrdiff_t offset, int count) {
    return DescriptorPool(pool, MAGIC_SMPPOOL, memory, offset, count,
                          "nvnSamplerPoolInitialize");
}

void Nvnl_nvnSamplerPoolFinalize(NVNsamplerPool *pool) {
    LayerDescriptorPool *const record =
        DescriptorPoolOf(pool, MAGIC_SMPPOOL, "nvnSamplerPoolFinalize");
    if (record != NULL) {
        record->magic = 0;
    }
}

void Nvnl_nvnSamplerPoolRegisterSampler(const NVNsamplerPool *pool, int id,
                                        const NVNsampler *sampler) {
    static const char *const WHAT = "nvnSamplerPoolRegisterSampler";
    const LayerDescriptorPool *const record = DescriptorPoolOf(pool, MAGIC_SMPPOOL, WHAT);
    const LayerSampler *const from = Record(sampler, WHAT);
    if (record == NULL || from == NULL) {
        return;
    }
    if (from->magic != MAGIC_SAMPLER) {
        LayerComplain("%s: the sampler was not initialized", WHAT);
        return;
    }
    DkSamplerDescriptor *const descriptor = Descriptor(record, id, WHAT);
    DkSampler made;
    if (descriptor != NULL && SamplerOf(&from->state, &made, WHAT)) {
        dkSamplerDescriptorInitialize(descriptor, &made);
    }
}

void Nvnl_nvnSamplerPoolRegisterSamplerBuilder(const NVNsamplerPool *pool, int id,
                                               const NVNsamplerBuilder *builder) {
    // The descriptor a sampler made from the builder would get, with no sampler made.
    static const char *const WHAT = "nvnSamplerPoolRegisterSamplerBuilder";
    const LayerDescriptorPool *const record = DescriptorPoolOf(pool, MAGIC_SMPPOOL, WHAT);
    const LayerSamplerBuilder *const from = Record(builder, WHAT);
    if (record == NULL || from == NULL) {
        return;
    }
    DkSamplerDescriptor *const descriptor = Descriptor(record, id, WHAT);
    DkSampler made;
    if (descriptor != NULL && SamplerOf(&from->state, &made, WHAT)) {
        dkSamplerDescriptorInitialize(descriptor, &made);
    }
}

// Where a descriptor pool is and how many descriptors it holds, as it was made.
#define POOL_GETTERS(object, type, magic)                                                         \
    int Nvnl_nvn##object##GetSize(const type *pool) {                                             \
        const LayerDescriptorPool *const record =                                                 \
            DescriptorPoolOf(pool, magic, "nvn" #object "GetSize");                               \
        return record != NULL ? record->count : 0;                                                \
    }                                                                                             \
    const NVNmemoryPool *Nvnl_nvn##object##GetMemoryPool(const type *pool) {                      \
        const LayerDescriptorPool *const record =                                                 \
            DescriptorPoolOf(pool, magic, "nvn" #object "GetMemoryPool");                         \
        return record != NULL ? record->memory : NULL;                                            \
    }                                                                                             \
    ptrdiff_t Nvnl_nvn##object##GetMemoryOffset(const type *pool) {                               \
        const LayerDescriptorPool *const record =                                                 \
            DescriptorPoolOf(pool, magic, "nvn" #object "GetMemoryOffset");                       \
        return record != NULL ? (ptrdiff_t)record->offset : 0;                                    \
    }
POOL_GETTERS(TexturePool, NVNtexturePool, MAGIC_TEXPOOL)
POOL_GETTERS(SamplerPool, NVNsamplerPool, MAGIC_SMPPOOL)
#undef POOL_GETTERS

void Nvnl_nvnCommandBufferSetTexturePool(NVNcommandBuffer *cmd, const NVNtexturePool *pool) {
    static const char *const WHAT = "nvnCommandBufferSetTexturePool";
    LayerCommandBuffer *const record = CmdOf(cmd, WHAT);
    const LayerDescriptorPool *const from = DescriptorPoolOf(pool, MAGIC_TEXPOOL, WHAT);
    if (record != NULL && from != NULL) {
        dkCmdBufBindImageDescriptorSet(record->cmdbuf,
                                       dkMemBlockGetGpuAddr(from->block) + from->offset,
                                       (uint32_t)from->count);
    }
}

void Nvnl_nvnCommandBufferSetSamplerPool(NVNcommandBuffer *cmd, const NVNsamplerPool *pool) {
    static const char *const WHAT = "nvnCommandBufferSetSamplerPool";
    LayerCommandBuffer *const record = CmdOf(cmd, WHAT);
    const LayerDescriptorPool *const from = DescriptorPoolOf(pool, MAGIC_SMPPOOL, WHAT);
    if (record != NULL && from != NULL) {
        dkCmdBufBindSamplerDescriptorSet(record->cmdbuf,
                                         dkMemBlockGetGpuAddr(from->block) + from->offset,
                                         (uint32_t)from->count);
    }
}

// ---- the state objects

// A state object is a few bytes the program owns. A setter keeps what it is given when that is
// one of NVN's numbers, and complains of any other, since the fields are as wide as NVN's
// numbers are; a getter gives back what the setter took. What a state does to a draw is said
// when it is bound.

// NVN's numbers for these are the graphics unit's own, as deko3d's are: a bind hands them on.
_Static_assert((int)NVN_BLEND_FUNC_ZERO == (int)DkBlendFactor_Zero &&
                   (int)NVN_BLEND_FUNC_ONE_MINUS_SRC1_ALPHA == (int)DkBlendFactor_InvSrc1Alpha &&
                   (int)NVN_BLEND_EQUATION_ADD == (int)DkBlendOp_Add &&
                   (int)NVN_BLEND_EQUATION_MAX == (int)DkBlendOp_Max,
               "NVN's blend functions and equations are deko3d's numbers");
_Static_assert((int)NVN_LOGIC_OP_CLEAR == (int)DkLogicOp_Clear &&
                   (int)NVN_LOGIC_OP_COPY == (int)DkLogicOp_Copy &&
                   (int)NVN_LOGIC_OP_SET == (int)DkLogicOp_Set &&
                   (int)NVN_ALPHA_FUNC_NEVER == (int)DkCompareOp_Never &&
                   (int)NVN_ALPHA_FUNC_ALWAYS == (int)DkCompareOp_Always,
               "NVN's logic operations and alpha functions are deko3d's numbers");
_Static_assert((int)NVN_STENCIL_FUNC_NEVER == (int)DkCompareOp_Never &&
                   (int)NVN_STENCIL_FUNC_ALWAYS == (int)DkCompareOp_Always &&
                   (int)NVN_STENCIL_OP_KEEP == (int)DkStencilOp_Keep &&
                   (int)NVN_STENCIL_OP_DECR_WRAP == (int)DkStencilOp_DecrWrap,
               "NVN's stencil functions and operations are deko3d's numbers");
_Static_assert((int)NVN_POLYGON_MODE_POINT == (int)DkPolygonMode_Point &&
                   (int)NVN_POLYGON_MODE_FILL == (int)DkPolygonMode_Fill &&
                   (int)NVN_POLYGON_OFFSET_ENABLE_POINT == (int)DkPolygonFlag_Point &&
                   (int)NVN_POLYGON_OFFSET_ENABLE_FILL == (int)DkPolygonFlag_Fill &&
                   (int)NVN_COVERAGE_MODULATION_MODE_RGBA == (int)DkCoverageModulation_Rgba,
               "NVN's polygon modes, offset enables and coverage modulations are deko3d's numbers");

/// Whether @p value is one of NVN's blend functions.
static bool BlendFuncIs(int value) {
    return (value >= NVN_BLEND_FUNC_ZERO && value <= NVN_BLEND_FUNC_SRC_ALPHA_SATURATE) ||
           (value >= NVN_BLEND_FUNC_SRC1_COLOR && value <= NVN_BLEND_FUNC_ONE_MINUS_SRC1_ALPHA) ||
           (value >= NVN_BLEND_FUNC_CONSTANT_COLOR &&
            value <= NVN_BLEND_FUNC_ONE_MINUS_CONSTANT_ALPHA);
}

void Nvnl_nvnBlendStateSetDefaults(NVNblendState *state) {
    // Target 0, the source replacing the target; the advanced mode off, with the premultiplied
    // source and normalized destination ScarletNX has as a driver's.
    LayerBlend *const record = StateRecord(state, "nvnBlendStateSetDefaults");
    if (record != NULL) {
        memset(state, 0, sizeof(*state));
        record->src_rgb = record->src_alpha = NVN_BLEND_FUNC_ONE;
        record->dst_rgb = record->dst_alpha = NVN_BLEND_FUNC_ZERO;
        record->equation_rgb = record->equation_alpha = NVN_BLEND_EQUATION_ADD;
        record->premultiplied = 1;
        record->normalized = 1;
    }
}

void Nvnl_nvnBlendStateSetBlendTarget(NVNblendState *state, int target) {
    LayerBlend *const record = StateRecord(state, "nvnBlendStateSetBlendTarget");
    if (record == NULL) {
        return;
    }
    if (target < 0 || target >= COLOR_TARGETS_MAX) {
        LayerComplain("nvnBlendStateSetBlendTarget: target %d of %d", target, COLOR_TARGETS_MAX);
        return;
    }
    record->target = (uint32_t)target;
}

void Nvnl_nvnBlendStateSetBlendFunc(NVNblendState *state, NVNblendFunc src_rgb,
                                    NVNblendFunc dst_rgb, NVNblendFunc src_alpha,
                                    NVNblendFunc dst_alpha) {
    LayerBlend *const record = StateRecord(state, "nvnBlendStateSetBlendFunc");
    if (record == NULL) {
        return;
    }
    if (!BlendFuncIs(src_rgb) || !BlendFuncIs(dst_rgb) || !BlendFuncIs(src_alpha) ||
        !BlendFuncIs(dst_alpha)) {
        LayerComplain("nvnBlendStateSetBlendFunc: functions 0x%x, 0x%x, 0x%x and 0x%x, of which "
                      "one is none of NVN's",
                      (unsigned)src_rgb, (unsigned)dst_rgb, (unsigned)src_alpha,
                      (unsigned)dst_alpha);
        return;
    }
    record->src_rgb = (uint32_t)src_rgb;
    record->dst_rgb = (uint32_t)dst_rgb;
    record->src_alpha = (uint32_t)src_alpha;
    record->dst_alpha = (uint32_t)dst_alpha;
}

void Nvnl_nvnBlendStateSetBlendEquation(NVNblendState *state, NVNblendEquation rgb,
                                        NVNblendEquation alpha) {
    LayerBlend *const record = StateRecord(state, "nvnBlendStateSetBlendEquation");
    if (record == NULL) {
        return;
    }
    if (rgb < NVN_BLEND_EQUATION_ADD || rgb > NVN_BLEND_EQUATION_MAX ||
        alpha < NVN_BLEND_EQUATION_ADD || alpha > NVN_BLEND_EQUATION_MAX) {
        LayerComplain("nvnBlendStateSetBlendEquation: equations %d and %d, of NVN's %d to %d",
                      (int)rgb, (int)alpha, (int)NVN_BLEND_EQUATION_ADD,
                      (int)NVN_BLEND_EQUATION_MAX);
        return;
    }
    record->equation_rgb = (uint32_t)rgb;
    record->equation_alpha = (uint32_t)alpha;
}

void Nvnl_nvnBlendStateSetAdvancedMode(NVNblendState *state, NVNblendAdvancedMode mode) {
    LayerBlend *const record = StateRecord(state, "nvnBlendStateSetAdvancedMode");
    if (record == NULL) {
        return;
    }
    if (mode < 0 || mode > 0xff) {
        LayerComplain("nvnBlendStateSetAdvancedMode: mode 0x%x, past the byte NVN's modes fit",
                      (unsigned)mode);
        return;
    }
    record->advanced = (uint32_t)mode;
}

void Nvnl_nvnBlendStateSetAdvancedOverlap(NVNblendState *state,
                                          NVNblendAdvancedOverlap overlap) {
    LayerBlend *const record = StateRecord(state, "nvnBlendStateSetAdvancedOverlap");
    if (record == NULL) {
        return;
    }
    if (overlap < NVN_BLEND_ADVANCED_OVERLAP_UNCORRELATED ||
        overlap > NVN_BLEND_ADVANCED_OVERLAP_CONJOINT) {
        LayerComplain("nvnBlendStateSetAdvancedOverlap: overlap %d, of NVN's %d to %d",
                      (int)overlap, (int)NVN_BLEND_ADVANCED_OVERLAP_UNCORRELATED,
                      (int)NVN_BLEND_ADVANCED_OVERLAP_CONJOINT);
        return;
    }
    record->overlap = (uint32_t)overlap;
}

void Nvnl_nvnBlendStateSetAdvancedPremultipliedSrc(NVNblendState *state, NVNboolean on) {
    LayerBlend *const record = StateRecord(state, "nvnBlendStateSetAdvancedPremultipliedSrc");
    if (record != NULL) {
        record->premultiplied = on != 0;
    }
}

void Nvnl_nvnBlendStateSetAdvancedNormalizedDst(NVNblendState *state, NVNboolean on) {
    LayerBlend *const record = StateRecord(state, "nvnBlendStateSetAdvancedNormalizedDst");
    if (record != NULL) {
        record->normalized = on != 0;
    }
}

int Nvnl_nvnBlendStateGetBlendTarget(const NVNblendState *state) {
    const LayerBlend *const record = StateRecord(state, "nvnBlendStateGetBlendTarget");
    return record != NULL ? (int)record->target : 0;
}

void Nvnl_nvnBlendStateGetBlendFunc(const NVNblendState *state, NVNblendFunc *src_rgb,
                                    NVNblendFunc *dst_rgb, NVNblendFunc *src_alpha,
                                    NVNblendFunc *dst_alpha) {
    const LayerBlend *const record = StateRecord(state, "nvnBlendStateGetBlendFunc");
    if (record != NULL) {
        *src_rgb = (NVNblendFunc)record->src_rgb;
        *dst_rgb = (NVNblendFunc)record->dst_rgb;
        *src_alpha = (NVNblendFunc)record->src_alpha;
        *dst_alpha = (NVNblendFunc)record->dst_alpha;
    }
}

void Nvnl_nvnBlendStateGetBlendEquation(const NVNblendState *state, NVNblendEquation *rgb,
                                        NVNblendEquation *alpha) {
    const LayerBlend *const record = StateRecord(state, "nvnBlendStateGetBlendEquation");
    if (record != NULL) {
        *rgb = (NVNblendEquation)record->equation_rgb;
        *alpha = (NVNblendEquation)record->equation_alpha;
    }
}

NVNblendAdvancedMode Nvnl_nvnBlendStateGetAdvancedMode(const NVNblendState *state) {
    const LayerBlend *const record = StateRecord(state, "nvnBlendStateGetAdvancedMode");
    return record != NULL ? (NVNblendAdvancedMode)record->advanced : NVN_BLEND_ADVANCED_MODE_NONE;
}

NVNblendAdvancedOverlap Nvnl_nvnBlendStateGetAdvancedOverlap(const NVNblendState *state) {
    const LayerBlend *const record = StateRecord(state, "nvnBlendStateGetAdvancedOverlap");
    return record != NULL ? (NVNblendAdvancedOverlap)record->overlap
                          : NVN_BLEND_ADVANCED_OVERLAP_UNCORRELATED;
}

NVNboolean Nvnl_nvnBlendStateGetAdvancedPremultipliedSrc(const NVNblendState *state) {
    const LayerBlend *const record =
        StateRecord(state, "nvnBlendStateGetAdvancedPremultipliedSrc");
    return record != NULL && record->premultiplied != 0;
}

NVNboolean Nvnl_nvnBlendStateGetAdvancedNormalizedDst(const NVNblendState *state) {
    const LayerBlend *const record = StateRecord(state, "nvnBlendStateGetAdvancedNormalizedDst");
    return record != NULL && record->normalized != 0;
}

void Nvnl_nvnChannelMaskStateSetDefaults(NVNchannelMaskState *state) {
    LayerChannelMask *const record = StateRecord(state, "nvnChannelMaskStateSetDefaults");
    if (record != NULL) {
        record->masks = 0xffffffffu; // every channel of every target written
    }
}

void Nvnl_nvnChannelMaskStateSetChannelMask(NVNchannelMaskState *state, int target,
                                            NVNboolean r, NVNboolean g, NVNboolean b,
                                            NVNboolean a) {
    LayerChannelMask *const record = StateRecord(state, "nvnChannelMaskStateSetChannelMask");
    if (record == NULL) {
        return;
    }
    if (target < 0 || target >= COLOR_TARGETS_MAX) {
        LayerComplain("nvnChannelMaskStateSetChannelMask: target %d of %d", target,
                      COLOR_TARGETS_MAX);
        return;
    }
    // Four bits a target, red the lowest: deko3d's mask, and the graphics unit's.
    const uint32_t mask = (r ? 1u : 0u) | (g ? 2u : 0u) | (b ? 4u : 0u) | (a ? 8u : 0u);
    record->masks = (record->masks & ~(0xfu << (4 * target))) | mask << (4 * target);
}

void Nvnl_nvnChannelMaskStateGetChannelMask(const NVNchannelMaskState *state, int target,
                                            NVNboolean *r, NVNboolean *g, NVNboolean *b,
                                            NVNboolean *a) {
    const LayerChannelMask *const record =
        StateRecord(state, "nvnChannelMaskStateGetChannelMask");
    if (record == NULL) {
        return;
    }
    const uint32_t mask =
        target >= 0 && target < COLOR_TARGETS_MAX ? record->masks >> (4 * target) : 0u;
    *r = (mask & 1u) != 0u;
    *g = (mask & 2u) != 0u;
    *b = (mask & 4u) != 0u;
    *a = (mask & 8u) != 0u;
}

void Nvnl_nvnColorStateSetDefaults(NVNcolorState *state) {
    // No target blends, the colour written as it is, every alpha passes.
    LayerColor *const record = StateRecord(state, "nvnColorStateSetDefaults");
    if (record != NULL) {
        memset(state, 0, sizeof(*state));
        record->logic_op = NVN_LOGIC_OP_COPY;
        record->alpha_func = NVN_ALPHA_FUNC_ALWAYS;
    }
}

void Nvnl_nvnColorStateSetBlendEnable(NVNcolorState *state, int target, NVNboolean enable) {
    LayerColor *const record = StateRecord(state, "nvnColorStateSetBlendEnable");
    if (record == NULL) {
        return;
    }
    if (target < 0 || target >= COLOR_TARGETS_MAX) {
        LayerComplain("nvnColorStateSetBlendEnable: target %d of %d", target, COLOR_TARGETS_MAX);
        return;
    }
    const uint32_t bit = 1u << target;
    record->blend_enable = enable ? record->blend_enable | bit : record->blend_enable & ~bit;
}

void Nvnl_nvnColorStateSetLogicOp(NVNcolorState *state, NVNlogicOp op) {
    LayerColor *const record = StateRecord(state, "nvnColorStateSetLogicOp");
    if (record == NULL) {
        return;
    }
    if (op < NVN_LOGIC_OP_CLEAR || op > NVN_LOGIC_OP_SET) {
        LayerComplain("nvnColorStateSetLogicOp: operation %d, of NVN's %d to %d", (int)op,
                      (int)NVN_LOGIC_OP_CLEAR, (int)NVN_LOGIC_OP_SET);
        return;
    }
    record->logic_op = (uint32_t)op;
}

void Nvnl_nvnColorStateSetAlphaTest(NVNcolorState *state, NVNalphaFunc func) {
    LayerColor *const record = StateRecord(state, "nvnColorStateSetAlphaTest");
    if (record == NULL) {
        return;
    }
    if (func < NVN_ALPHA_FUNC_NEVER || func > NVN_ALPHA_FUNC_ALWAYS) {
        LayerComplain("nvnColorStateSetAlphaTest: function %d, of NVN's %d to %d", (int)func,
                      (int)NVN_ALPHA_FUNC_NEVER, (int)NVN_ALPHA_FUNC_ALWAYS);
        return;
    }
    record->alpha_func = (uint32_t)func;
}

NVNboolean Nvnl_nvnColorStateGetBlendEnable(const NVNcolorState *state, int target) {
    const LayerColor *const record = StateRecord(state, "nvnColorStateGetBlendEnable");
    return record != NULL && target >= 0 && target < COLOR_TARGETS_MAX &&
           (record->blend_enable >> target & 1u) != 0u;
}

NVNlogicOp Nvnl_nvnColorStateGetLogicOp(const NVNcolorState *state) {
    const LayerColor *const record = StateRecord(state, "nvnColorStateGetLogicOp");
    return record != NULL ? (NVNlogicOp)record->logic_op : NVN_LOGIC_OP_COPY;
}

NVNalphaFunc Nvnl_nvnColorStateGetAlphaTest(const NVNcolorState *state) {
    const LayerColor *const record = StateRecord(state, "nvnColorStateGetAlphaTest");
    return record != NULL ? (NVNalphaFunc)record->alpha_func : NVN_ALPHA_FUNC_ALWAYS;
}

void Nvnl_nvnDepthStencilStateSetDefaults(NVNdepthStencilState *state) {
    // No depth test, depth written, less; no stencil test, every stencil function passing and
    // every operation keeping. A program sets the ones it relies on, as it cannot ask what a
    // driver's defaults are.
    LayerDepthStencil *const record = StateRecord(state, "nvnDepthStencilStateSetDefaults");
    if (record != NULL) {
        memset(state, 0, sizeof(*state));
        record->depth_write = 1;
        record->depth_func = NVN_DEPTH_FUNC_LESS;
        record->front_func = record->back_func = NVN_STENCIL_FUNC_ALWAYS;
        record->front_fail = record->front_depth_fail = record->front_pass = NVN_STENCIL_OP_KEEP;
        record->back_fail = record->back_depth_fail = record->back_pass = NVN_STENCIL_OP_KEEP;
    }
}

void Nvnl_nvnDepthStencilStateSetDepthTestEnable(NVNdepthStencilState *state,
                                                 NVNboolean enable) {
    LayerDepthStencil *const record =
        StateRecord(state, "nvnDepthStencilStateSetDepthTestEnable");
    if (record != NULL) {
        record->depth_test = enable != 0;
    }
}

void Nvnl_nvnDepthStencilStateSetDepthWriteEnable(NVNdepthStencilState *state,
                                                  NVNboolean enable) {
    LayerDepthStencil *const record =
        StateRecord(state, "nvnDepthStencilStateSetDepthWriteEnable");
    if (record != NULL) {
        record->depth_write = enable != 0;
    }
}

void Nvnl_nvnDepthStencilStateSetDepthFunc(NVNdepthStencilState *state, NVNdepthFunc func) {
    LayerDepthStencil *const record = StateRecord(state, "nvnDepthStencilStateSetDepthFunc");
    if (record == NULL) {
        return;
    }
    if (func < NVN_DEPTH_FUNC_NEVER || func > NVN_DEPTH_FUNC_ALWAYS) {
        LayerComplain("nvnDepthStencilStateSetDepthFunc: function %d is none of NVN's, %d to %d",
                      (int)func, (int)NVN_DEPTH_FUNC_NEVER, (int)NVN_DEPTH_FUNC_ALWAYS);
        return;
    }
    record->depth_func = (uint32_t)func;
}

void Nvnl_nvnDepthStencilStateSetStencilTestEnable(NVNdepthStencilState *state,
                                                   NVNboolean enable) {
    LayerDepthStencil *const record =
        StateRecord(state, "nvnDepthStencilStateSetStencilTestEnable");
    if (record != NULL) {
        record->stencil_test = enable != 0;
    }
}

void Nvnl_nvnDepthStencilStateSetStencilFunc(NVNdepthStencilState *state, NVNface faces,
                                             NVNstencilFunc func) {
    LayerDepthStencil *const record = StateRecord(state, "nvnDepthStencilStateSetStencilFunc");
    if (record == NULL) {
        return;
    }
    if (faces < NVN_FACE_NONE || faces > NVN_FACE_FRONT_AND_BACK || func < NVN_STENCIL_FUNC_NEVER ||
        func > NVN_STENCIL_FUNC_ALWAYS) {
        LayerComplain("nvnDepthStencilStateSetStencilFunc: function %d for faces %d, of NVN's "
                      "functions %d to %d and faces %d to %d",
                      (int)func, (int)faces, (int)NVN_STENCIL_FUNC_NEVER,
                      (int)NVN_STENCIL_FUNC_ALWAYS, (int)NVN_FACE_NONE,
                      (int)NVN_FACE_FRONT_AND_BACK);
        return;
    }
    if ((faces & NVN_FACE_FRONT) != 0) {
        record->front_func = (uint32_t)func;
    }
    if ((faces & NVN_FACE_BACK) != 0) {
        record->back_func = (uint32_t)func;
    }
}

static bool StencilOpIs(NVNstencilOp op) {
    return op >= NVN_STENCIL_OP_KEEP && op <= NVN_STENCIL_OP_DECR_WRAP;
}

void Nvnl_nvnDepthStencilStateSetStencilOp(NVNdepthStencilState *state, NVNface faces,
                                           NVNstencilOp fail, NVNstencilOp depth_fail,
                                           NVNstencilOp pass) {
    LayerDepthStencil *const record = StateRecord(state, "nvnDepthStencilStateSetStencilOp");
    if (record == NULL) {
        return;
    }
    if (faces < NVN_FACE_NONE || faces > NVN_FACE_FRONT_AND_BACK || !StencilOpIs(fail) ||
        !StencilOpIs(depth_fail) || !StencilOpIs(pass)) {
        LayerComplain("nvnDepthStencilStateSetStencilOp: operations %d, %d and %d for faces %d, "
                      "of NVN's operations %d to %d and faces %d to %d",
                      (int)fail, (int)depth_fail, (int)pass, (int)faces, (int)NVN_STENCIL_OP_KEEP,
                      (int)NVN_STENCIL_OP_DECR_WRAP, (int)NVN_FACE_NONE,
                      (int)NVN_FACE_FRONT_AND_BACK);
        return;
    }
    if ((faces & NVN_FACE_FRONT) != 0) {
        record->front_fail = (uint32_t)fail;
        record->front_depth_fail = (uint32_t)depth_fail;
        record->front_pass = (uint32_t)pass;
    }
    if ((faces & NVN_FACE_BACK) != 0) {
        record->back_fail = (uint32_t)fail;
        record->back_depth_fail = (uint32_t)depth_fail;
        record->back_pass = (uint32_t)pass;
    }
}

NVNboolean Nvnl_nvnDepthStencilStateGetDepthTestEnable(const NVNdepthStencilState *state) {
    const LayerDepthStencil *const record =
        StateRecord(state, "nvnDepthStencilStateGetDepthTestEnable");
    return record != NULL && record->depth_test != 0;
}

NVNboolean Nvnl_nvnDepthStencilStateGetDepthWriteEnable(const NVNdepthStencilState *state) {
    const LayerDepthStencil *const record =
        StateRecord(state, "nvnDepthStencilStateGetDepthWriteEnable");
    return record != NULL && record->depth_write != 0;
}

NVNdepthFunc Nvnl_nvnDepthStencilStateGetDepthFunc(const NVNdepthStencilState *state) {
    const LayerDepthStencil *const record =
        StateRecord(state, "nvnDepthStencilStateGetDepthFunc");
    return record != NULL ? (NVNdepthFunc)record->depth_func : NVN_DEPTH_FUNC_LESS;
}

NVNboolean Nvnl_nvnDepthStencilStateGetStencilTestEnable(const NVNdepthStencilState *state) {
    const LayerDepthStencil *const record =
        StateRecord(state, "nvnDepthStencilStateGetStencilTestEnable");
    return record != NULL && record->stencil_test != 0;
}

// A face's stencil state is the back's for NVN_FACE_BACK and the front's for any other face.

NVNstencilFunc Nvnl_nvnDepthStencilStateGetStencilFunc(const NVNdepthStencilState *state,
                                                       NVNface face) {
    const LayerDepthStencil *const record =
        StateRecord(state, "nvnDepthStencilStateGetStencilFunc");
    if (record == NULL) {
        return NVN_STENCIL_FUNC_ALWAYS;
    }
    return (NVNstencilFunc)(face == NVN_FACE_BACK ? record->back_func : record->front_func);
}

void Nvnl_nvnDepthStencilStateGetStencilOp(const NVNdepthStencilState *state, NVNface face,
                                           NVNstencilOp *fail, NVNstencilOp *depth_fail,
                                           NVNstencilOp *pass) {
    const LayerDepthStencil *const record =
        StateRecord(state, "nvnDepthStencilStateGetStencilOp");
    if (record == NULL) {
        return;
    }
    const bool back = face == NVN_FACE_BACK;
    *fail = (NVNstencilOp)(back ? record->back_fail : record->front_fail);
    *depth_fail = (NVNstencilOp)(back ? record->back_depth_fail : record->front_depth_fail);
    *pass = (NVNstencilOp)(back ? record->back_pass : record->front_pass);
}

void Nvnl_nvnMultisampleStateSetDefaults(NVNmultisampleState *state) {
    LayerMultisample *const record = StateRecord(state, "nvnMultisampleStateSetDefaults");
    if (record != NULL) {
        // Multisampling on, as ScarletNX has a driver's new state, with no samples set: one
        // sample, and nothing of the rest on.
        memset(state, 0, sizeof(*state));
        record->flags = MULTISAMPLE_ENABLE;
    }
}

/// Whether @p count is a number of samples a target can have, nought being none set.
static bool SamplesIs(int count) {
    return count == 0 || count == 1 || count == 2 || count == 4 || count == 8 || count == 16;
}

void Nvnl_nvnMultisampleStateSetSamples(NVNmultisampleState *state, int samples) {
    LayerMultisample *const record = StateRecord(state, "nvnMultisampleStateSetSamples");
    if (record == NULL) {
        return;
    }
    if (!SamplesIs(samples)) {
        LayerComplain("nvnMultisampleStateSetSamples: %d samples, of 0, 1, 2, 4, 8 and 16",
                      samples);
        return;
    }
    record->samples = (uint8_t)samples;
}

void Nvnl_nvnMultisampleStateSetRasterSamples(NVNmultisampleState *state, int samples) {
    LayerMultisample *const record = StateRecord(state, "nvnMultisampleStateSetRasterSamples");
    if (record == NULL) {
        return;
    }
    if (!SamplesIs(samples) || samples == 1) {
        LayerComplain("nvnMultisampleStateSetRasterSamples: %d raster samples, of 0, 2, 4, 8 and "
                      "16",
                      samples);
        return;
    }
    record->raster_samples = (uint8_t)samples;
}

void Nvnl_nvnMultisampleStateSetCoverageToColorOutput(NVNmultisampleState *state, int output) {
    LayerMultisample *const record =
        StateRecord(state, "nvnMultisampleStateSetCoverageToColorOutput");
    if (record == NULL) {
        return;
    }
    if (output < 0 || output >= COLOR_TARGETS_MAX) {
        LayerComplain("nvnMultisampleStateSetCoverageToColorOutput: output %d of %d", output,
                      COLOR_TARGETS_MAX);
        return;
    }
    record->coverage_to_color_output = (uint8_t)output;
}

void Nvnl_nvnMultisampleStateSetCoverageModulationMode(NVNmultisampleState *state,
                                                       NVNcoverageModulationMode mode) {
    LayerMultisample *const record =
        StateRecord(state, "nvnMultisampleStateSetCoverageModulationMode");
    if (record == NULL) {
        return;
    }
    if (mode < NVN_COVERAGE_MODULATION_MODE_NONE || mode > NVN_COVERAGE_MODULATION_MODE_RGBA) {
        LayerComplain("nvnMultisampleStateSetCoverageModulationMode: mode %d, of NVN's %d to %d",
                      (int)mode, (int)NVN_COVERAGE_MODULATION_MODE_NONE,
                      (int)NVN_COVERAGE_MODULATION_MODE_RGBA);
        return;
    }
    record->coverage_modulation = (uint8_t)mode;
}

int Nvnl_nvnMultisampleStateGetSamples(const NVNmultisampleState *state) {
    const LayerMultisample *const record = StateRecord(state, "nvnMultisampleStateGetSamples");
    return record != NULL ? record->samples : 0;
}

int Nvnl_nvnMultisampleStateGetRasterSamples(const NVNmultisampleState *state) {
    const LayerMultisample *const record =
        StateRecord(state, "nvnMultisampleStateGetRasterSamples");
    return record != NULL ? record->raster_samples : 0;
}

int Nvnl_nvnMultisampleStateGetCoverageToColorOutput(const NVNmultisampleState *state) {
    const LayerMultisample *const record =
        StateRecord(state, "nvnMultisampleStateGetCoverageToColorOutput");
    return record != NULL ? record->coverage_to_color_output : 0;
}

NVNcoverageModulationMode
Nvnl_nvnMultisampleStateGetCoverageModulationMode(const NVNmultisampleState *state) {
    const LayerMultisample *const record =
        StateRecord(state, "nvnMultisampleStateGetCoverageModulationMode");
    return record != NULL ? (NVNcoverageModulationMode)record->coverage_modulation
                          : NVN_COVERAGE_MODULATION_MODE_NONE;
}

// The switches of a multisample state, each a flag.
#define MULTISAMPLE_FLAG(name, parameter, flag)                                                   \
    void Nvnl_nvnMultisampleStateSet##name(NVNmultisampleState *state, NVNboolean parameter) {    \
        LayerMultisample *const record = StateRecord(state, "nvnMultisampleStateSet" #name);      \
        if (record != NULL) {                                                                     \
            record->flags = (uint8_t)(parameter ? record->flags | (flag)                          \
                                                : record->flags & ~(flag));                       \
        }                                                                                         \
    }                                                                                             \
    NVNboolean Nvnl_nvnMultisampleStateGet##name(const NVNmultisampleState *state) {              \
        const LayerMultisample *const record =                                                    \
            StateRecord(state, "nvnMultisampleStateGet" #name);                                   \
        return record != NULL && (record->flags & (flag)) != 0u;                                  \
    }
MULTISAMPLE_FLAG(MultisampleEnable, enable, MULTISAMPLE_ENABLE)
MULTISAMPLE_FLAG(AlphaToCoverageEnable, enable, MULTISAMPLE_ALPHA_TO_COVERAGE)
MULTISAMPLE_FLAG(AlphaToCoverageDither, on, MULTISAMPLE_DITHER)
MULTISAMPLE_FLAG(CoverageToColorEnable, on, MULTISAMPLE_COVERAGE_TO_COLOR)
MULTISAMPLE_FLAG(CoverageTruncationEnable, on, MULTISAMPLE_TRUNCATION)
MULTISAMPLE_FLAG(SampleLocationsEnable, on, MULTISAMPLE_LOCATIONS)
MULTISAMPLE_FLAG(SampleLocationsGridEnable, on, MULTISAMPLE_GRID)
#undef MULTISAMPLE_FLAG

void Nvnl_nvnMultisampleStateSetSampleLocations(NVNmultisampleState *state, int first, int count,
                                                const float *xy) {
    LayerMultisample *const record = StateRecord(state, "nvnMultisampleStateSetSampleLocations");
    if (record == NULL) {
        return;
    }
    if (first < 0 || count < 0 || first > MULTISAMPLE_LOCATIONS_MAX ||
        count > MULTISAMPLE_LOCATIONS_MAX - first || (count != 0 && xy == NULL)) {
        LayerComplain("nvnMultisampleStateSetSampleLocations: %d locations from %d, of %d", count,
                      first, MULTISAMPLE_LOCATIONS_MAX);
        return;
    }
    for (int i = 0; i < count; ++i) {
        // Sixteenths of a pixel, the nearest, kept inside the pixel.
        uint8_t packed = 0;
        for (int axis = 0; axis < 2; ++axis) {
            const float at = xy[2 * i + axis] >= 0.0f ? xy[2 * i + axis] : 0.0f;
            const uint8_t step = at < 0.9375f ? (uint8_t)(at * 16.0f + 0.5f) : 15u;
            packed |= (uint8_t)(step << (4 * axis));
        }
        record->locations[first + i] = packed;
    }
}

void Nvnl_nvnMultisampleStateGetSampleLocationsGrid(NVNmultisampleState *state, int *width,
                                                    int *height) {
    // The pixels sixteen sample locations cover, by the greater of the samples and the raster
    // samples: ScarletNX's answer for a driver, kept the same here.
    const LayerMultisample *const record =
        StateRecord(state, "nvnMultisampleStateGetSampleLocationsGrid");
    if (record == NULL) {
        return;
    }
    const int samples =
        record->samples > record->raster_samples ? record->samples : record->raster_samples;
    switch (samples) {
    case 0:
    case 1:
        *width = 4;
        *height = 4;
        break;
    case 2:
        *width = 2;
        *height = 4;
        break;
    case 4:
        *width = 2;
        *height = 2;
        break;
    case 8:
        *width = 1;
        *height = 2;
        break;
    default:
        *width = 1;
        *height = 1;
        break;
    }
}

void Nvnl_nvnPolygonStateSetDefaults(NVNpolygonState *state) {
    // Nothing culled, counterclockwise in front, filled, no offset; a program sets the ones it
    // relies on.
    LayerPolygon *const record = StateRecord(state, "nvnPolygonStateSetDefaults");
    if (record != NULL) {
        memset(state, 0, sizeof(*state));
        record->cull = NVN_FACE_NONE;
        record->front = NVN_FRONT_FACE_CCW;
        record->mode = NVN_POLYGON_MODE_FILL;
    }
}

void Nvnl_nvnPolygonStateSetCullFace(NVNpolygonState *state, NVNface face) {
    LayerPolygon *const record = StateRecord(state, "nvnPolygonStateSetCullFace");
    if (record == NULL) {
        return;
    }
    if (face < NVN_FACE_NONE || face > NVN_FACE_FRONT_AND_BACK) {
        LayerComplain("nvnPolygonStateSetCullFace: face %d is none of NVN's, %d to %d", (int)face,
                      (int)NVN_FACE_NONE, (int)NVN_FACE_FRONT_AND_BACK);
        return;
    }
    record->cull = (uint32_t)face;
}

void Nvnl_nvnPolygonStateSetFrontFace(NVNpolygonState *state, NVNfrontFace face) {
    LayerPolygon *const record = StateRecord(state, "nvnPolygonStateSetFrontFace");
    if (record == NULL) {
        return;
    }
    if (face != NVN_FRONT_FACE_CW && face != NVN_FRONT_FACE_CCW) {
        LayerComplain("nvnPolygonStateSetFrontFace: face %d is neither clockwise (%d) nor "
                      "counterclockwise (%d)",
                      (int)face, (int)NVN_FRONT_FACE_CW, (int)NVN_FRONT_FACE_CCW);
        return;
    }
    record->front = (uint32_t)face;
}

void Nvnl_nvnPolygonStateSetPolygonMode(NVNpolygonState *state, NVNpolygonMode mode) {
    LayerPolygon *const record = StateRecord(state, "nvnPolygonStateSetPolygonMode");
    if (record == NULL) {
        return;
    }
    if (mode < NVN_POLYGON_MODE_POINT || mode > NVN_POLYGON_MODE_FILL) {
        LayerComplain("nvnPolygonStateSetPolygonMode: mode %d, of NVN's %d to %d", (int)mode,
                      (int)NVN_POLYGON_MODE_POINT, (int)NVN_POLYGON_MODE_FILL);
        return;
    }
    record->mode = (uint32_t)mode;
}

void Nvnl_nvnPolygonStateSetPolygonOffsetEnables(NVNpolygonState *state, int enables) {
    LayerPolygon *const record = StateRecord(state, "nvnPolygonStateSetPolygonOffsetEnables");
    if (record == NULL) {
        return;
    }
    const int all = NVN_POLYGON_OFFSET_ENABLE_POINT | NVN_POLYGON_OFFSET_ENABLE_LINE |
                    NVN_POLYGON_OFFSET_ENABLE_FILL;
    if ((enables & ~all) != 0) {
        LayerComplain("nvnPolygonStateSetPolygonOffsetEnables: enables 0x%x, of the bits 0x%x",
                      (unsigned)enables, (unsigned)all);
        return;
    }
    record->offset = (uint32_t)enables;
}

NVNface Nvnl_nvnPolygonStateGetCullFace(const NVNpolygonState *state) {
    const LayerPolygon *const record = StateRecord(state, "nvnPolygonStateGetCullFace");
    return record != NULL ? (NVNface)record->cull : NVN_FACE_NONE;
}

NVNfrontFace Nvnl_nvnPolygonStateGetFrontFace(const NVNpolygonState *state) {
    const LayerPolygon *const record = StateRecord(state, "nvnPolygonStateGetFrontFace");
    return record != NULL ? (NVNfrontFace)record->front : NVN_FRONT_FACE_CCW;
}

NVNpolygonMode Nvnl_nvnPolygonStateGetPolygonMode(const NVNpolygonState *state) {
    const LayerPolygon *const record = StateRecord(state, "nvnPolygonStateGetPolygonMode");
    return record != NULL ? (NVNpolygonMode)record->mode : NVN_POLYGON_MODE_FILL;
}

NVNpolygonOffsetEnable Nvnl_nvnPolygonStateGetPolygonOffsetEnables(const NVNpolygonState *state) {
    const LayerPolygon *const record =
        StateRecord(state, "nvnPolygonStateGetPolygonOffsetEnables");
    return record != NULL ? (NVNpolygonOffsetEnable)record->offset
                          : NVN_POLYGON_OFFSET_ENABLE_NONE;
}

void Nvnl_nvnVertexAttribStateSetDefaults(NVNvertexAttribState *state) {
    LayerVertexAttrib *const record = StateRecord(state, "nvnVertexAttribStateSetDefaults");
    if (record != NULL) {
        memset(state, 0, sizeof(*state)); // no format: the attribute is off
    }
}

void Nvnl_nvnVertexAttribStateSetFormat(NVNvertexAttribState *state, NVNformat format,
                                        ptrdiff_t offset) {
    LayerVertexAttrib *const record = StateRecord(state, "nvnVertexAttribStateSetFormat");
    if (record == NULL) {
        return;
    }
    if (offset < 0 || offset >= 1 << 14 || (unsigned)format >= 1u << 10) {
        LayerComplain("nvnVertexAttribStateSetFormat: format 0x%x at offset %lld, where an "
                      "attribute's offset is 0 to %d",
                      (unsigned)format, (long long)offset, (1 << 14) - 1);
        return;
    }
    record->set = format != NVN_FORMAT_NONE;
    record->format = (uint32_t)format;
    record->offset = (uint32_t)offset;
}

void Nvnl_nvnVertexAttribStateSetStreamIndex(NVNvertexAttribState *state, int index) {
    LayerVertexAttrib *const record = StateRecord(state, "nvnVertexAttribStateSetStreamIndex");
    if (record == NULL) {
        return;
    }
    if (index < 0 || index >= DK_MAX_VERTEX_BUFFERS) {
        LayerComplain("nvnVertexAttribStateSetStreamIndex: stream %d of %d", index,
                      DK_MAX_VERTEX_BUFFERS);
        return;
    }
    record->stream = (uint32_t)index;
}

void Nvnl_nvnVertexStreamStateSetDefaults(NVNvertexStreamState *state) {
    LayerVertexStream *const record = StateRecord(state, "nvnVertexStreamStateSetDefaults");
    if (record != NULL) {
        memset(state, 0, sizeof(*state)); // no stride, a vertex's own data
    }
}

void Nvnl_nvnVertexStreamStateSetStride(NVNvertexStreamState *state, ptrdiff_t stride) {
    LayerVertexStream *const record = StateRecord(state, "nvnVertexStreamStateSetStride");
    if (record == NULL) {
        return;
    }
    if (stride < 0 || stride > (ptrdiff_t)UINT32_MAX) {
        LayerComplain("nvnVertexStreamStateSetStride: a stride of %lld", (long long)stride);
        return;
    }
    record->stride = (uint32_t)stride;
}

void Nvnl_nvnVertexAttribStateGetFormat(const NVNvertexAttribState *state, NVNformat *format,
                                        ptrdiff_t *offset) {
    const LayerVertexAttrib *const record = StateRecord(state, "nvnVertexAttribStateGetFormat");
    if (record != NULL) {
        *format = (NVNformat)record->format;
        *offset = (ptrdiff_t)record->offset;
    }
}

int Nvnl_nvnVertexAttribStateGetStreamIndex(const NVNvertexAttribState *state) {
    const LayerVertexAttrib *const record =
        StateRecord(state, "nvnVertexAttribStateGetStreamIndex");
    return record != NULL ? (int)record->stream : 0;
}

void Nvnl_nvnVertexStreamStateSetDivisor(NVNvertexStreamState *state, int divisor) {
    LayerVertexStream *const record = StateRecord(state, "nvnVertexStreamStateSetDivisor");
    if (record == NULL) {
        return;
    }
    if (divisor < 0) {
        LayerComplain("nvnVertexStreamStateSetDivisor: a divisor of %d", divisor);
        return;
    }
    record->divisor = (uint32_t)divisor;
}

ptrdiff_t Nvnl_nvnVertexStreamStateGetStride(const NVNvertexStreamState *state) {
    const LayerVertexStream *const record = StateRecord(state, "nvnVertexStreamStateGetStride");
    return record != NULL ? (ptrdiff_t)record->stride : 0;
}

int Nvnl_nvnVertexStreamStateGetDivisor(const NVNvertexStreamState *state) {
    const LayerVertexStream *const record = StateRecord(state, "nvnVertexStreamStateGetDivisor");
    return record != NULL ? (int)record->divisor : 0;
}

// ---- the binds

/// deko3d's blend factor for one of NVN's functions: the same number, but for the four of the
/// constant colour, which deko3d numbers apart.
static DkBlendFactor FactorOf(uint32_t func) {
    switch (func) {
    case NVN_BLEND_FUNC_CONSTANT_COLOR:
        return DkBlendFactor_ConstColor;
    case NVN_BLEND_FUNC_ONE_MINUS_CONSTANT_COLOR:
        return DkBlendFactor_InvConstColor;
    case NVN_BLEND_FUNC_CONSTANT_ALPHA:
        return DkBlendFactor_ConstAlpha;
    case NVN_BLEND_FUNC_ONE_MINUS_CONSTANT_ALPHA:
        return DkBlendFactor_InvConstAlpha;
    default:
        return (DkBlendFactor)func;
    }
}

void Nvnl_nvnCommandBufferBindBlendState(NVNcommandBuffer *cmd, const NVNblendState *state) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferBindBlendState");
    const LayerBlend *const from = StateRecord(state, "nvnCommandBufferBindBlendState");
    if (record == NULL || from == NULL) {
        return;
    }
    if (from->advanced != NVN_BLEND_ADVANCED_MODE_NONE) {
        LayerComplain("nvnCommandBufferBindBlendState: advanced blend mode 0x%x, and deko3d "
                      "blends with functions and equations alone (mode 0x%x)",
                      (unsigned)from->advanced, (unsigned)NVN_BLEND_ADVANCED_MODE_NONE);
        return;
    }
    DkBlendState blend;
    dkBlendStateDefaults(&blend);
    blend.colorBlendOp = (DkBlendOp)from->equation_rgb;
    blend.alphaBlendOp = (DkBlendOp)from->equation_alpha;
    blend.srcColorBlendFactor = FactorOf(from->src_rgb);
    blend.dstColorBlendFactor = FactorOf(from->dst_rgb);
    blend.srcAlphaBlendFactor = FactorOf(from->src_alpha);
    blend.dstAlphaBlendFactor = FactorOf(from->dst_alpha);
    dkCmdBufBindBlendStates(record->cmdbuf, from->target, &blend, 1);
}

void Nvnl_nvnCommandBufferBindChannelMaskState(NVNcommandBuffer *cmd,
                                               const NVNchannelMaskState *state) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferBindChannelMaskState");
    const LayerChannelMask *const from =
        StateRecord(state, "nvnCommandBufferBindChannelMaskState");
    if (record == NULL || from == NULL) {
        return;
    }
    const DkColorWriteState write = { from->masks };
    dkCmdBufBindColorWriteState(record->cmdbuf, &write);
}

void Nvnl_nvnCommandBufferBindColorState(NVNcommandBuffer *cmd, const NVNcolorState *state) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferBindColorState");
    const LayerColor *const from = StateRecord(state, "nvnCommandBufferBindColorState");
    if (record == NULL || from == NULL) {
        return;
    }
    DkColorState color;
    dkColorStateDefaults(&color);
    color.blendEnableMask = from->blend_enable;
    color.logicOp = (DkLogicOp)from->logic_op;
    color.alphaCompareOp = (DkCompareOp)from->alpha_func;
    dkCmdBufBindColorState(record->cmdbuf, &color);
}

void Nvnl_nvnCommandBufferBindDepthStencilState(NVNcommandBuffer *cmd,
                                                const NVNdepthStencilState *state) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferBindDepthStencilState");
    const LayerDepthStencil *const from =
        StateRecord(state, "nvnCommandBufferBindDepthStencilState");
    if (record == NULL || from == NULL) {
        return;
    }
    DkDepthStencilState depth;
    dkDepthStencilStateDefaults(&depth);
    depth.depthTestEnable = from->depth_test;
    depth.depthWriteEnable = from->depth_write;
    depth.stencilTestEnable = from->stencil_test;
    // NVN's depth functions are the graphics unit's, as deko3d's are: never 1 to always 8.
    depth.depthCompareOp = (DkCompareOp)from->depth_func;
    depth.stencilFrontCompareOp = (DkCompareOp)from->front_func;
    depth.stencilFrontFailOp = (DkStencilOp)from->front_fail;
    depth.stencilFrontDepthFailOp = (DkStencilOp)from->front_depth_fail;
    depth.stencilFrontPassOp = (DkStencilOp)from->front_pass;
    depth.stencilBackCompareOp = (DkCompareOp)from->back_func;
    depth.stencilBackFailOp = (DkStencilOp)from->back_fail;
    depth.stencilBackDepthFailOp = (DkStencilOp)from->back_depth_fail;
    depth.stencilBackPassOp = (DkStencilOp)from->back_pass;
    dkCmdBufBindDepthStencilState(record->cmdbuf, &depth);
}

/// deko3d's mode for a number of samples, or false for sixteen, which it has none for.
static bool MsModeOf(uint8_t samples, DkMsMode *mode) {
    switch (samples) {
    case 0:
    case 1:
        *mode = DkMsMode_1x;
        return true;
    case 2:
        *mode = DkMsMode_2x;
        return true;
    case 4:
        *mode = DkMsMode_4x;
        return true;
    case 8:
        *mode = DkMsMode_8x;
        return true;
    default:
        return false;
    }
}

void Nvnl_nvnCommandBufferBindMultisampleState(NVNcommandBuffer *cmd,
                                               const NVNmultisampleState *state) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferBindMultisampleState");
    const LayerMultisample *const from =
        StateRecord(state, "nvnCommandBufferBindMultisampleState");
    if (record == NULL || from == NULL) {
        return;
    }
    DkMultisampleState multisample;
    dkMultisampleStateDefaults(&multisample);
    DkMsMode mode = DkMsMode_1x, raster = DkMsMode_1x;
    const uint8_t undrawn = MULTISAMPLE_LOCATIONS | MULTISAMPLE_GRID | MULTISAMPLE_TRUNCATION;
    if (!MsModeOf(from->samples, &mode) ||
        !MsModeOf(from->raster_samples != 0 ? from->raster_samples : from->samples, &raster) ||
        (from->flags & undrawn) != 0u) {
        LayerComplain("nvnCommandBufferBindMultisampleState: %u samples, %u raster samples and "
                      "flags 0x%x, where the layer draws with 1, 2, 4 or 8 samples and none of "
                      "the sample locations and coverage truncation flags (0x%x)",
                      (unsigned)from->samples, (unsigned)from->raster_samples,
                      (unsigned)from->flags, (unsigned)undrawn);
        return;
    }
    // A state that does not enable multisampling draws one sample, whatever its count.
    if ((from->flags & MULTISAMPLE_ENABLE) != 0u) {
        multisample.mode = mode;
        multisample.rasterizerMode = raster;
    }
    multisample.alphaToCoverageEnable = (from->flags & MULTISAMPLE_ALPHA_TO_COVERAGE) != 0u;
    multisample.alphaToCoverageDither = (from->flags & MULTISAMPLE_DITHER) != 0u;
    multisample.coverageToColorEnable = (from->flags & MULTISAMPLE_COVERAGE_TO_COLOR) != 0u;
    multisample.coverageToColorOutput = from->coverage_to_color_output;
    multisample.coverageModulation = (DkCoverageModulation)from->coverage_modulation;
    dkCmdBufBindMultisampleState(record->cmdbuf, &multisample);
}

void Nvnl_nvnCommandBufferBindPolygonState(NVNcommandBuffer *cmd, const NVNpolygonState *state) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferBindPolygonState");
    const LayerPolygon *const from = StateRecord(state, "nvnCommandBufferBindPolygonState");
    if (record == NULL || from == NULL) {
        return;
    }
    record->draw->dynamic.cull = (uint8_t)from->cull;
    record->draw->dynamic.front = (uint8_t)from->front;
    record->draw->dynamic.mode = (uint8_t)from->mode;
    record->draw->dynamic.offset = (uint8_t)from->offset;
    record->draw->dynamic.polygon = true;
    LayerBindRasterizer(record);
}

/// deko3d's attribute size and type for an NVN format, or false for one the layer has not
/// mapped.
static bool AttribOf(NVNformat format, DkVtxAttribSize *size, DkVtxAttribType *type) {
    *type = DkVtxAttribType_Float;
    switch (format) {
    case NVN_FORMAT_R32F:
        *size = DkVtxAttribSize_1x32;
        return true;
    case NVN_FORMAT_RG32F:
        *size = DkVtxAttribSize_2x32;
        return true;
    case NVN_FORMAT_RGB32F:
        *size = DkVtxAttribSize_3x32;
        return true;
    case NVN_FORMAT_RGBA32F:
        *size = DkVtxAttribSize_4x32;
        return true;
    case NVN_FORMAT_RGBA8:
        *size = DkVtxAttribSize_4x8;
        *type = DkVtxAttribType_Unorm;
        return true;
    default:
        return false;
    }
}

void Nvnl_nvnCommandBufferBindVertexAttribState(NVNcommandBuffer *cmd, int count,
                                                const NVNvertexAttribState *states) {
    static const char *const WHAT = "nvnCommandBufferBindVertexAttribState";
    LayerCommandBuffer *const record = CmdOf(cmd, WHAT);
    if (record == NULL) {
        return;
    }
    if (count < 0 || count > DK_MAX_VERTEX_ATTRIBS || (count > 0 && states == NULL)) {
        LayerComplain("%s: %d attributes of %d", WHAT, count, DK_MAX_VERTEX_ATTRIBS);
        return;
    }
    DkVtxAttribState attribs[DK_MAX_VERTEX_ATTRIBS];
    memset(attribs, 0, sizeof(attribs));
    for (int i = 0; i < count; ++i) {
        const LayerVertexAttrib *const from = StateRecord(&states[i], WHAT);
        if (from == NULL) {
            return;
        }
        if (!from->set) {
            // An attribute that is off reads the constant (0, 0, 0, 1), deko3d's fixed one.
            attribs[i].isFixed = 1;
            attribs[i].size = DkVtxAttribSize_4x32;
            attribs[i].type = DkVtxAttribType_Float;
            continue;
        }
        DkVtxAttribSize size;
        DkVtxAttribType type;
        if (!AttribOf((NVNformat)from->format, &size, &type)) {
            LayerComplain("%s: attribute %d has NVN format 0x%x, which has no deko3d attribute "
                          "in the layer yet",
                          WHAT, i, (unsigned)from->format);
            return;
        }
        attribs[i].bufferId = from->stream;
        attribs[i].offset = from->offset;
        attribs[i].size = size;
        attribs[i].type = type;
    }
    dkCmdBufBindVtxAttribState(record->cmdbuf, attribs, (uint32_t)count);
}

void Nvnl_nvnCommandBufferBindVertexStreamState(NVNcommandBuffer *cmd, int count,
                                                const NVNvertexStreamState *states) {
    static const char *const WHAT = "nvnCommandBufferBindVertexStreamState";
    LayerCommandBuffer *const record = CmdOf(cmd, WHAT);
    if (record == NULL) {
        return;
    }
    if (count < 0 || count > DK_MAX_VERTEX_BUFFERS || (count > 0 && states == NULL)) {
        LayerComplain("%s: %d streams of %d", WHAT, count, DK_MAX_VERTEX_BUFFERS);
        return;
    }
    DkVtxBufferState buffers[DK_MAX_VERTEX_BUFFERS];
    for (int i = 0; i < count; ++i) {
        const LayerVertexStream *const from = StateRecord(&states[i], WHAT);
        if (from == NULL) {
            return;
        }
        buffers[i].stride = from->stride;
        buffers[i].divisor = from->divisor;
    }
    dkCmdBufBindVtxBufferState(record->cmdbuf, buffers, (uint32_t)count);
}

// The binds of one and of several share these: an entry point never calls another by name,
// which a runtime that takes the entry points over by their addresses could not follow.

static void VertexBufferBind(NVNcommandBuffer *cmd, int index, NVNbufferAddress address,
                             size_t size, const char *what) {
    LayerCommandBuffer *const record = CmdOf(cmd, what);
    if (record == NULL) {
        return;
    }
    if (index < 0 || index >= DK_MAX_VERTEX_BUFFERS || size > UINT32_MAX) {
        LayerComplain("%s: stream %d of %d, 0x%llx bytes", what, index, DK_MAX_VERTEX_BUFFERS,
                      (unsigned long long)size);
        return;
    }
    dkCmdBufBindVtxBuffer(record->cmdbuf, (uint32_t)index, address, (uint32_t)size);
}

void Nvnl_nvnCommandBufferBindVertexBuffer(NVNcommandBuffer *cmd, int index,
                                           NVNbufferAddress address, size_t size) {
    VertexBufferBind(cmd, index, address, size, "nvnCommandBufferBindVertexBuffer");
}

static void UniformBufferBind(NVNcommandBuffer *cmd, NVNshaderStage stage, int index,
                              NVNbufferAddress address, size_t size, const char *what) {
    LayerCommandBuffer *const record = CmdOf(cmd, what);
    if (record == NULL) {
        return;
    }
    const int to = StageOf(stage);
    // Slot r is deko3d's uniform buffer r + 1, the constant buffer an NVN program reads it from.
    if (to < 0 || index < 0 || index + 1 >= DK_NUM_UNIFORM_BUFS || size > DK_UNIFORM_BUF_MAX_SIZE) {
        LayerComplain("%s: stage %d, slot %d, 0x%llx bytes, where the layer takes the graphics "
                      "stages, slots 0 to %d and 0x%x bytes",
                      what, (int)stage, index, (unsigned long long)size, DK_NUM_UNIFORM_BUFS - 2,
                      (unsigned)DK_UNIFORM_BUF_MAX_SIZE);
        return;
    }
    dkCmdBufBindUniformBuffer(record->cmdbuf, (DkStage)to, (uint32_t)index + 1u, address,
                              (uint32_t)size);
}

void Nvnl_nvnCommandBufferBindUniformBuffer(NVNcommandBuffer *cmd, NVNshaderStage stage,
                                            int index, NVNbufferAddress address, size_t size) {
    UniformBufferBind(cmd, stage, index, address, size, "nvnCommandBufferBindUniformBuffer");
}

static void TextureBind(NVNcommandBuffer *cmd, NVNshaderStage stage, int index,
                        NVNtextureHandle handle, const char *what) {
    LayerCommandBuffer *const record = CmdOf(cmd, what);
    if (record == NULL) {
        return;
    }
    if (StageOf(stage) < 0 || index < 0 || index >= LAYER_HANDLE_SLOTS) {
        LayerComplain("%s: stage %d, slot %d, where the layer takes the graphics stages and "
                      "slots 0 to %d",
                      what, (int)stage, index, LAYER_HANDLE_SLOTS - 1);
        return;
    }
    // Kept until the next draw, which is when the graphics unit has to find it.
    LayerDraw *const draw = record->draw;
    memcpy(draw->handles[stage] + LAYER_HANDLE_FIRST + 8u * (uint32_t)index, &handle, 8);
    draw->dirty |= (uint8_t)(1u << stage);
}

void Nvnl_nvnCommandBufferBindTexture(NVNcommandBuffer *cmd, NVNshaderStage stage, int index,
                                      NVNtextureHandle handle) {
    TextureBind(cmd, stage, index, handle, "nvnCommandBufferBindTexture");
}

// The binds of several at once are the single binds, one after another. A deferred bind reads
// its ranges or handles when the list runs, on a driver; the layer reads them as it records,
// which is the same to a program that leaves them alone until the list has run.

void Nvnl_nvnCommandBufferBindVertexBuffers(NVNcommandBuffer *cmd, int first, int count,
                                            const NVNbufferRange *ranges) {
    for (int i = 0; i < count; ++i) {
        VertexBufferBind(cmd, first + i, ranges[i].address, ranges[i].size,
                         "nvnCommandBufferBindVertexBuffers");
    }
}

void Nvnl_nvnCommandBufferBindUniformBuffers(NVNcommandBuffer *cmd, NVNshaderStage stage,
                                             int first, int count,
                                             const NVNbufferRange *ranges) {
    for (int i = 0; i < count; ++i) {
        UniformBufferBind(cmd, stage, first + i, ranges[i].address, ranges[i].size,
                          "nvnCommandBufferBindUniformBuffers");
    }
}

void Nvnl_nvnCommandBufferBindUniformBuffersDeferred(NVNcommandBuffer *cmd, NVNshaderStage stage,
                                                     int first, int count,
                                                     const NVNbufferRange *ranges) {
    for (int i = 0; i < count; ++i) {
        UniformBufferBind(cmd, stage, first + i, ranges[i].address, ranges[i].size,
                          "nvnCommandBufferBindUniformBuffersDeferred");
    }
}

void Nvnl_nvnCommandBufferBindTextures(NVNcommandBuffer *cmd, NVNshaderStage stage, int first,
                                       int count, const NVNtextureHandle *handles) {
    for (int i = 0; i < count; ++i) {
        TextureBind(cmd, stage, first + i, handles[i], "nvnCommandBufferBindTextures");
    }
}

void Nvnl_nvnCommandBufferBindTexturesDeferred(NVNcommandBuffer *cmd, NVNshaderStage stage,
                                               int first, int count,
                                               const NVNtextureHandle *handles) {
    for (int i = 0; i < count; ++i) {
        TextureBind(cmd, stage, first + i, handles[i], "nvnCommandBufferBindTexturesDeferred");
    }
}

void Nvnl_nvnCommandBufferUpdateUniformBuffer(NVNcommandBuffer *cmd, NVNbufferAddress buffer,
                                              size_t buffer_size, ptrdiff_t offset, size_t size,
                                              const void *data) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferUpdateUniformBuffer");
    if (record == NULL) {
        return;
    }
    if (offset < 0 || (size_t)offset > buffer_size || size > buffer_size - (size_t)offset ||
        size > DK_UNIFORM_BUF_MAX_SIZE || data == NULL) {
        LayerComplain("nvnCommandBufferUpdateUniformBuffer: 0x%zx bytes at offset %lld of a "
                      "buffer of 0x%zx",
                      size, (long long)offset, buffer_size);
        return;
    }
    // The bytes go into the list, and the graphics unit writes them when it reaches them.
    dkCmdBufPushData(record->cmdbuf, buffer + (uint64_t)offset, data, (uint32_t)size);
}

void Nvnl_nvnCommandBufferBindCoverageModulationTable(NVNcommandBuffer *cmd,
                                                      const float *entries) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferBindCoverageModulationTable");
    if (record == NULL) {
        return;
    }
    if (entries == NULL) {
        LayerComplain("nvnCommandBufferBindCoverageModulationTable: no table, which takes the "
                      "program's table away in NVN, and deko3d has a call to set one only");
        return;
    }
    dkCmdBufSetCoverageModulationTable(record->cmdbuf, entries);
}

void Nvnl_nvnCommandBufferSetShaderScratchMemory(NVNcommandBuffer *cmd, const NVNmemoryPool *pool,
                                                 ptrdiff_t offset, size_t size) {
    // Memory a driver lends its shaders: deko3d asks for none, so a range that is in its pool is
    // taken and nothing recorded.
    const LayerPool *const from = PoolOf(pool, "nvnCommandBufferSetShaderScratchMemory");
    if (CmdOf(cmd, "nvnCommandBufferSetShaderScratchMemory") == NULL || from == NULL) {
        return;
    }
    if (offset < 0 || (size_t)offset > from->size || size > from->size - (size_t)offset) {
        LayerComplain("nvnCommandBufferSetShaderScratchMemory: 0x%zx bytes at offset %lld, of a "
                      "pool of 0x%zx",
                      size, (long long)offset, from->size);
    }
}

void Nvnl_nvnCommandBufferSetProgramSubroutines(NVNcommandBuffer *cmd, NVNprogram *program,
                                                NVNshaderStage stage, int first, int count,
                                                const int *values) {
    // A choice of subroutines goes through the linkage map the program kept for the stage, and
    // a driver that finds none there does nothing with the choice. No program here keeps one
    // (nvnProgramSetSubroutineLinkage), so a choice a driver could record is taken and nothing
    // is recorded: a stage of NVN's six, and a first and a count of sixteen bits each.
    static const char *const WHAT = "nvnCommandBufferSetProgramSubroutines";
    if (CmdOf(cmd, WHAT) == NULL || ProgramOf(program, WHAT) == NULL) {
        return;
    }
    if ((unsigned)stage > (unsigned)NVN_SHADER_STAGE_COMPUTE || first < 0 || first > 0xffff ||
        count < 0 || count > 0xffff || (count > 0 && values == NULL)) {
        LayerComplain("%s: %d subroutines from %d of stage %d, with%s values: a stage is 0 to 5, "
                      "and a first and a count 0 to 65535 each",
                      WHAT, count, first, (int)stage, values == NULL ? " no" : "");
    }
}

// ---- the clears and the draws

void Nvnl_nvnCommandBufferClearDepthStencil(NVNcommandBuffer *cmd, float depth,
                                            NVNboolean depth_mask, int stencil,
                                            int stencil_mask) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferClearDepthStencil");
    if (record != NULL) {
        dkCmdBufClearDepthStencil(record->cmdbuf, depth_mask != 0, depth, (uint8_t)stencil_mask,
                                  (uint8_t)stencil);
    }
}

/// Gives the graphics unit the texture handles of every stage that changed since the last draw:
/// a copy in the layer's part of the command memory, bound as the stage's uniform buffer 0. A
/// copy per change, because a list's draws run later and each has to find the handles it was
/// recorded with. False with a complaint when the copies no longer fit.
static bool FlushHandles(LayerCommandBuffer *record, const char *what) {
    LayerDraw *const draw = record->draw;
    for (int stage = 0; stage < LAYER_STAGES; ++stage) {
        if ((draw->dirty >> stage & 1u) == 0u) {
            continue;
        }
        if (draw->handle_used + LAYER_HANDLE_BYTES > draw->handle_size) {
            LayerComplain("%s: the quarter of the command memory the layer keeps for texture "
                          "handles, 0x%x bytes, is full after %u copies of 0x%x: a list that "
                          "changes its textures this often needs more command memory",
                          what, (unsigned)draw->handle_size,
                          (unsigned)(draw->handle_used / LAYER_HANDLE_BYTES),
                          (unsigned)LAYER_HANDLE_BYTES);
            return false;
        }
        memcpy(draw->handle_cpu + draw->handle_used, draw->handles[stage], LAYER_HANDLE_BYTES);
        dkCmdBufBindUniformBuffer(record->cmdbuf, (DkStage)StageOf((NVNshaderStage)stage), 0,
                                  draw->handle_memory + draw->handle_used, LAYER_HANDLE_BYTES);
        draw->handle_used += LAYER_HANDLE_BYTES;
        draw->dirty &= (uint8_t)~(1u << stage);
    }
    return true;
}

void Nvnl_nvnCommandBufferDrawArrays(NVNcommandBuffer *cmd, NVNdrawPrimitive primitive,
                                     int first, int count) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferDrawArrays");
    if (record == NULL) {
        return;
    }
    if (primitive < NVN_DRAW_PRIMITIVE_POINTS || primitive > NVN_DRAW_PRIMITIVE_PATCHES ||
        first < 0 || count < 0) {
        LayerComplain("nvnCommandBufferDrawArrays: primitive %d, %d vertices from %d",
                      (int)primitive, count, first);
        return;
    }
    if (!FlushHandles(record, "nvnCommandBufferDrawArrays")) {
        return;
    }
    // NVN's primitives are the graphics unit's numbers, as deko3d's are: points 0 to patches 14.
    dkCmdBufDraw(record->cmdbuf, (DkPrimitive)primitive, (uint32_t)count, 1, (uint32_t)first, 0);
}

void Nvnl_nvnCommandBufferDrawArraysInstanced(NVNcommandBuffer *cmd, NVNdrawPrimitive primitive,
                                              int first, int count, int base_instance,
                                              int instances) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferDrawArraysInstanced");
    if (record == NULL) {
        return;
    }
    if (primitive < NVN_DRAW_PRIMITIVE_POINTS || primitive > NVN_DRAW_PRIMITIVE_PATCHES ||
        first < 0 || count < 0 || base_instance < 0 || instances < 0) {
        LayerComplain("nvnCommandBufferDrawArraysInstanced: primitive %d, %d vertices from %d, "
                      "%d instances from %d",
                      (int)primitive, count, first, instances, base_instance);
        return;
    }
    if (!FlushHandles(record, "nvnCommandBufferDrawArraysInstanced")) {
        return;
    }
    dkCmdBufDraw(record->cmdbuf, (DkPrimitive)primitive, (uint32_t)count, (uint32_t)instances,
                 (uint32_t)first, (uint32_t)base_instance);
}

// NVN's indirect draws read the words deko3d's read, in the same order: the count, the
// instances, the first vertex or index, for an indexed draw the base vertex, the base instance.

/// What the draws that read their numbers from a buffer check; false with a complaint.
static bool IndirectTaken(LayerCommandBuffer *record, NVNdrawPrimitive primitive,
                          NVNbufferAddress indirect, const char *what) {
    if (record == NULL) {
        return false;
    }
    if (primitive < NVN_DRAW_PRIMITIVE_POINTS || primitive > NVN_DRAW_PRIMITIVE_PATCHES ||
        indirect == 0u || (indirect & 3u) != 0u) {
        LayerComplain("%s: primitive %d with its numbers at 0x%llx, where the layer takes them "
                      "on a four byte boundary",
                      what, (int)primitive, (unsigned long long)indirect);
        return false;
    }
    return FlushHandles(record, what);
}

void Nvnl_nvnCommandBufferDrawArraysIndirect(NVNcommandBuffer *cmd, NVNdrawPrimitive primitive,
                                             NVNbufferAddress indirect) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferDrawArraysIndirect");
    if (IndirectTaken(record, primitive, indirect, "nvnCommandBufferDrawArraysIndirect")) {
        dkCmdBufDrawIndirect(record->cmdbuf, (DkPrimitive)primitive, indirect);
    }
}

void Nvnl_nvnCommandBufferDrawElementsIndirect(NVNcommandBuffer *cmd, NVNdrawPrimitive primitive,
                                               NVNindexType type, NVNbufferAddress indices,
                                               NVNbufferAddress indirect) {
    static const char *const WHAT = "nvnCommandBufferDrawElementsIndirect";
    LayerCommandBuffer *const record = CmdOf(cmd, WHAT);
    if (record == NULL) {
        return;
    }
    if (type < NVN_INDEX_TYPE_UNSIGNED_BYTE || type > NVN_INDEX_TYPE_UNSIGNED_INT ||
        indices == 0u) {
        LayerComplain("%s: indices of type %d at 0x%llx", WHAT, (int)type,
                      (unsigned long long)indices);
        return;
    }
    if (IndirectTaken(record, primitive, indirect, WHAT)) {
        dkCmdBufBindIdxBuffer(record->cmdbuf, (DkIdxFormat)type, indices);
        dkCmdBufDrawIndexedIndirect(record->cmdbuf, (DkPrimitive)primitive, indirect);
    }
}

void Nvnl_nvnCommandBufferMultiDrawArraysIndirectCount(NVNcommandBuffer *cmd,
                                                       NVNdrawPrimitive primitive,
                                                       NVNbufferAddress indirect,
                                                       NVNbufferAddress count, int most,
                                                       ptrdiff_t stride) {
    (void)cmd;
    (void)primitive;
    (void)indirect;
    (void)count;
    LayerLack("nvnCommandBufferMultiDrawArraysIndirectCount: up to %d draws %lld bytes apart, as "
              "many as a word the graphics unit reads says, and deko3d has no draw that reads "
              "how many draws it is",
              most, (long long)stride);
}

void Nvnl_nvnCommandBufferMultiDrawElementsIndirectCount(NVNcommandBuffer *cmd,
                                                         NVNdrawPrimitive primitive,
                                                         NVNindexType type,
                                                         NVNbufferAddress indices,
                                                         NVNbufferAddress indirect,
                                                         NVNbufferAddress count, int most,
                                                         ptrdiff_t stride) {
    (void)cmd;
    (void)primitive;
    (void)type;
    (void)indices;
    (void)indirect;
    (void)count;
    LayerLack("nvnCommandBufferMultiDrawElementsIndirectCount: up to %d draws %lld bytes apart, "
              "as many as a word the graphics unit reads says, and deko3d has no draw that "
              "reads how many draws it is",
              most, (long long)stride);
}

void Nvnl_nvnCommandBufferDrawTexture(NVNcommandBuffer *cmd, NVNtextureHandle handle,
                                      const NVNdrawTextureRegion *destination,
                                      const NVNdrawTextureRegion *source) {
    static const char *const WHAT = "nvnCommandBufferDrawTexture";
    LayerCommandBuffer *const record = CmdOf(cmd, WHAT);
    if (record == NULL) {
        return;
    }
    // The handle is the layer's own, deko3d's texture and sampler pair in 32 bits
    // (nvnDeviceGetTextureHandle).
    if (destination == NULL || source == NULL || handle > UINT32_MAX) {
        LayerComplain("%s: texture handle 0x%llx with a destination region at %p and a source "
                      "region at %p, where both regions are needed and a handle the layer made "
                      "is 32 bits",
                      WHAT, (unsigned long long)handle, (const void *)destination,
                      (const void *)source);
        return;
    }
    // NVN's region is deko3d's: a corner's x and y, then the other corner's, four floats.
    DkDrawTextureRegion onto;
    DkDrawTextureRegion from;
    memcpy(&onto, destination, sizeof(onto));
    memcpy(&from, source, sizeof(from));
    // An empty destination draws nothing, as ScarletNX's NVN reads NVN (its nvn_cmd.c); deko3d
    // would divide by its size.
    if (onto.x1 == onto.x0 || onto.y1 == onto.y0) {
        return;
    }
    dkCmdBufDrawTexture(record->cmdbuf, (DkResHandle)handle, &onto, &from);
}

// Transform feedback through deko3d's calls (scarletnx/deko3d's: 0.5.0 has none). NVN keeps a
// capture's state in a control the program gives, which ScarletNX's NVN reads as buffer 0's stride
// in its first word and the bytes each of the four buffers took in the four after it (its
// xfb.c): a begin writes the stride, a pause and an end the bytes, and a resume and a draw of what
// was captured read them. deko3d saves the bytes as four reports of 16 bytes, each buffer's in
// its second word, so a pause and an end save them into a unit of the layer's memory and copy the
// four words into the control, and a resume copies them back into a unit before deko3d reads it.

void Nvnl_nvnCommandBufferBindTransformFeedbackBuffer(NVNcommandBuffer *cmd, int index,
                                                      NVNbufferAddress buffer, size_t size) {
    static const char *const WHAT = "nvnCommandBufferBindTransformFeedbackBuffer";
    LayerCommandBuffer *const record = CmdOf(cmd, WHAT);
    if (record == NULL) {
        return;
    }
    if (index < 0 || index >= DK_NUM_TRANSFORM_FEEDBACK_BUFFERS || size > UINT32_MAX) {
        LayerComplain("%s: 0x%zx bytes at 0x%llx as capture buffer %d, where the buffers are 0 "
                      "to %d and a buffer is at most 0xffffffff bytes",
                      WHAT, size, (unsigned long long)buffer, index,
                      DK_NUM_TRANSFORM_FEEDBACK_BUFFERS - 1);
        return;
    }
    const DkBufExtents extents = { buffer, (uint32_t)size };
    dkCmdBufBindTransformFeedbackBuffers(record->cmdbuf, (uint32_t)index, &extents, 1);
}

void Nvnl_nvnCommandBufferBindTransformFeedbackBuffers(NVNcommandBuffer *cmd, int first,
                                                       int count,
                                                       const NVNbufferRange *ranges) {
    static const char *const WHAT = "nvnCommandBufferBindTransformFeedbackBuffers";
    LayerCommandBuffer *const record = CmdOf(cmd, WHAT);
    if (record == NULL) {
        return;
    }
    if (first < 0 || count < 0 || count > DK_NUM_TRANSFORM_FEEDBACK_BUFFERS - first ||
        (count > 0 && ranges == NULL)) {
        LayerComplain("%s: %d capture buffers from buffer %d, ranges at %p, where the buffers "
                      "are 0 to %d",
                      WHAT, count, first, (const void *)ranges,
                      DK_NUM_TRANSFORM_FEEDBACK_BUFFERS - 1);
        return;
    }
    DkBufExtents extents[DK_NUM_TRANSFORM_FEEDBACK_BUFFERS];
    for (int i = 0; i < count; ++i) {
        if (ranges[i].size > UINT32_MAX) {
            LayerComplain("%s: 0x%llx bytes as capture buffer %d, where a buffer is at most "
                          "0xffffffff bytes",
                          WHAT, (unsigned long long)ranges[i].size, first + i);
            return;
        }
        extents[i].addr = ranges[i].address;
        extents[i].size = (uint32_t)ranges[i].size;
    }
    dkCmdBufBindTransformFeedbackBuffers(record->cmdbuf, (uint32_t)first, extents,
                                         (uint32_t)count);
}

void Nvnl_nvnCommandBufferBeginTransformFeedback(NVNcommandBuffer *cmd,
                                                 NVNbufferAddress control) {
    static const char *const WHAT = "nvnCommandBufferBeginTransformFeedback";
    LayerCommandBuffer *const record = CmdOf(cmd, WHAT);
    if (record == NULL) {
        return;
    }
    if (control == 0u) {
        LayerComplain("%s: a capture begun with its control at 0x0, where the begin writes "
                      "buffer 0's stride",
                      WHAT);
        return;
    }
    LayerDraw *const draw = record->draw;
    dkCmdBufBeginTransformFeedback(record->cmdbuf);
    dkCmdBufPushData(record->cmdbuf, control, &draw->capture_stride, 4u);
    draw->begun_stride = draw->capture_stride;
}

/// A pause or an end: with a control, the bytes each buffer took saved and copied into it as
/// NVN keeps them; with none, only the capture stopped, as a pause the program resumes with none.
static void StopCapture(NVNcommandBuffer *cmd, NVNbufferAddress control, const char *what) {
    LayerCommandBuffer *const record = CmdOf(cmd, what);
    if (record == NULL) {
        return;
    }
    if (control == 0u) {
        dkCmdBufEndTransformFeedback(record->cmdbuf);
        return;
    }
    const long unit = LayerUnit(record, what);
    if (unit < 0) {
        return;
    }
    const DkGpuAddr saved = record->draw->handle_memory + (uint64_t)unit;
    dkCmdBufSaveTransformFeedbackOffsets(record->cmdbuf, saved);
    dkCmdBufEndTransformFeedback(record->cmdbuf);
    for (uint32_t buffer = 0; buffer < DK_NUM_TRANSFORM_FEEDBACK_BUFFERS; ++buffer) {
        dkCmdBufCopyBuffer(record->cmdbuf, saved + 16u * buffer + 4u, control + 4u + 4u * buffer,
                           4u);
    }
    // The copy engine runs beside the processor: a draw of what was captured and a resume read
    // the control as command data, which only a full barrier holds back until the copies land.
    dkCmdBufBarrier(record->cmdbuf, DkBarrier_Full, 0);
}

void Nvnl_nvnCommandBufferEndTransformFeedback(NVNcommandBuffer *cmd, NVNbufferAddress control) {
    StopCapture(cmd, control, "nvnCommandBufferEndTransformFeedback");
}

void Nvnl_nvnCommandBufferPauseTransformFeedback(NVNcommandBuffer *cmd,
                                                 NVNbufferAddress control) {
    StopCapture(cmd, control, "nvnCommandBufferPauseTransformFeedback");
}

void Nvnl_nvnCommandBufferResumeTransformFeedback(NVNcommandBuffer *cmd,
                                                  NVNbufferAddress control) {
    static const char *const WHAT = "nvnCommandBufferResumeTransformFeedback";
    LayerCommandBuffer *const record = CmdOf(cmd, WHAT);
    if (record == NULL) {
        return;
    }
    if (control == 0u) {
        LayerLack("%s: a capture resumed with its control at 0x0, where it goes on from the "
                  "bytes the buffers hold, and deko3d starts a capture over or from saved bytes",
                  WHAT);
        return;
    }
    const long unit = LayerUnit(record, WHAT);
    if (unit < 0) {
        return;
    }
    const DkGpuAddr saved = record->draw->handle_memory + (uint64_t)unit;
    for (uint32_t buffer = 0; buffer < DK_NUM_TRANSFORM_FEEDBACK_BUFFERS; ++buffer) {
        dkCmdBufCopyBuffer(record->cmdbuf, control + 4u + 4u * buffer, saved + 16u * buffer + 4u,
                           4u);
    }
    // The copies land before deko3d's resume has the graphics unit read the words.
    dkCmdBufBarrier(record->cmdbuf, DkBarrier_Full, 0);
    dkCmdBufResumeTransformFeedback(record->cmdbuf, saved);
}

void Nvnl_nvnCommandBufferDrawTransformFeedback(NVNcommandBuffer *cmd,
                                                NVNdrawPrimitive primitive,
                                                NVNbufferAddress control) {
    static const char *const WHAT = "nvnCommandBufferDrawTransformFeedback";
    LayerCommandBuffer *const record = CmdOf(cmd, WHAT);
    if (record == NULL) {
        return;
    }
    // The stride is the begin's, which deko3d takes from the processor: NVN reads it from the
    // control, which only the graphics unit has written by then.
    const uint32_t stride = record->draw->begun_stride;
    if (control == 0u || stride == 0u || (uint32_t)primitive > DkPrimitive_Patches) {
        LayerComplain("%s: primitive %d of what the capture with its control at 0x%llx wrote, "
                      "with a stride of %u from the last begin this command buffer recorded, "
                      "where a control, a begin under a program that captures and a primitive "
                      "of 0 to %d are needed",
                      WHAT, (int)primitive, (unsigned long long)control, (unsigned)stride,
                      (int)DkPrimitive_Patches);
        return;
    }
    // Buffer 0's bytes are the control's second word, the second word of a 16-byte report deko3d
    // reads at its start.
    if (!FlushHandles(record, WHAT)) {
        return;
    }
    dkCmdBufDrawTransformFeedback(record->cmdbuf, (DkPrimitive)primitive, stride, control, 0);
}

/// An indexed draw of @p count indices of @p type at @p indices, @p base_vertex added to each,
/// @p instances times from instance @p base_instance.
static void DrawElements(NVNcommandBuffer *cmd, NVNdrawPrimitive primitive, NVNindexType type,
                         int count, NVNbufferAddress indices, int base_vertex, int base_instance,
                         int instances, const char *what) {
    LayerCommandBuffer *const record = CmdOf(cmd, what);
    if (record == NULL) {
        return;
    }
    if (primitive < NVN_DRAW_PRIMITIVE_POINTS || primitive > NVN_DRAW_PRIMITIVE_PATCHES ||
        type < NVN_INDEX_TYPE_UNSIGNED_BYTE || type > NVN_INDEX_TYPE_UNSIGNED_INT || count < 0 ||
        indices == 0u || base_instance < 0 || instances < 0) {
        LayerComplain("%s: primitive %d, %d indices of type %d at 0x%llx, %d instances from %d",
                      what, (int)primitive, count, (int)type, (unsigned long long)indices,
                      instances, base_instance);
        return;
    }
    if (!FlushHandles(record, what)) {
        return;
    }
    // NVN's index types are deko3d's numbers: a byte 0, two 1, four 2.
    dkCmdBufBindIdxBuffer(record->cmdbuf, (DkIdxFormat)type, indices);
    dkCmdBufDrawIndexed(record->cmdbuf, (DkPrimitive)primitive, (uint32_t)count,
                        (uint32_t)instances, 0, base_vertex, (uint32_t)base_instance);
}

void Nvnl_nvnCommandBufferDrawElementsInstanced(NVNcommandBuffer *cmd,
                                                NVNdrawPrimitive primitive, NVNindexType type,
                                                int count, NVNbufferAddress indices,
                                                int base_vertex, int base_instance,
                                                int instances) {
    DrawElements(cmd, primitive, type, count, indices, base_vertex, base_instance, instances,
                 "nvnCommandBufferDrawElementsInstanced");
}

void Nvnl_nvnCommandBufferDrawElements(NVNcommandBuffer *cmd, NVNdrawPrimitive primitive,
                                       NVNindexType type, int count, NVNbufferAddress indices) {
    DrawElements(cmd, primitive, type, count, indices, 0, 0, 1, "nvnCommandBufferDrawElements");
}

void Nvnl_nvnCommandBufferDrawElementsBaseVertex(NVNcommandBuffer *cmd,
                                                 NVNdrawPrimitive primitive, NVNindexType type,
                                                 int count, NVNbufferAddress indices,
                                                 int base_vertex) {
    DrawElements(cmd, primitive, type, count, indices, base_vertex, 0, 1,
                 "nvnCommandBufferDrawElementsBaseVertex");
}
