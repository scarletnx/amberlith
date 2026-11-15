// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

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

NVNboolean Nvnl_nvnProgramSetShaders(NVNprogram *program, int count,
                                     const NVNshaderData *shaders) {
    static const char *const WHAT = "nvnProgramSetShaders";
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
}

// ---- samplers

void Nvnl_nvnSamplerBuilderSetDefaults(NVNsamplerBuilder *builder) {
    // The device stays, as NVN's own builder keeps it. The filters and the wrap modes start at
    // nearest and clamp to edge here; a program sets the ones it relies on, as it cannot ask
    // what a driver's defaults are.
    LayerSamplerBuilder *const record = Record(builder, "nvnSamplerBuilderSetDefaults");
    if (record == NULL) {
        return;
    }
    NVNdevice *const device = record->device;
    memset(builder, 0, sizeof(*builder));
    record->device = device;
    record->min = NVN_MIN_FILTER_NEAREST;
    record->mag = NVN_MAG_FILTER_NEAREST;
    record->wrap[0] = record->wrap[1] = record->wrap[2] = NVN_WRAP_MODE_CLAMP_TO_EDGE;
}

void Nvnl_nvnSamplerBuilderSetDevice(NVNsamplerBuilder *builder, NVNdevice *device) {
    LayerSamplerBuilder *const record = Record(builder, "nvnSamplerBuilderSetDevice");
    if (record != NULL) {
        record->device = device;
    }
}

void Nvnl_nvnSamplerBuilderSetMinMagFilter(NVNsamplerBuilder *builder, NVNminFilter min,
                                           NVNmagFilter mag) {
    LayerSamplerBuilder *const record = Record(builder, "nvnSamplerBuilderSetMinMagFilter");
    if (record != NULL) {
        record->min = (uint8_t)min;
        record->mag = (uint8_t)mag;
    }
}

void Nvnl_nvnSamplerBuilderSetWrapMode(NVNsamplerBuilder *builder, NVNwrapMode s, NVNwrapMode t,
                                       NVNwrapMode r) {
    LayerSamplerBuilder *const record = Record(builder, "nvnSamplerBuilderSetWrapMode");
    if (record != NULL) {
        record->wrap[0] = (uint8_t)s;
        record->wrap[1] = (uint8_t)t;
        record->wrap[2] = (uint8_t)r;
    }
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

/// deko3d's sampler for the filters and wrap modes a sampler record holds; false with a
/// complaint naming the number that is none of NVN's.
static bool SamplerOf(const LayerSampler *record, DkSampler *sampler, const char *what) {
    dkSamplerDefaults(sampler);
    switch (record->min) {
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
        LayerComplain("%s: min filter %u is none of NVN's", what, (unsigned)record->min);
        return false;
    }
    switch (record->min) {
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
    if (record->mag != NVN_MAG_FILTER_NEAREST && record->mag != NVN_MAG_FILTER_LINEAR) {
        LayerComplain("%s: mag filter %u is none of NVN's", what, (unsigned)record->mag);
        return false;
    }
    sampler->magFilter = record->mag == NVN_MAG_FILTER_LINEAR ? DkFilter_Linear : DkFilter_Nearest;
    for (int i = 0; i < 3; ++i) {
        if (!WrapOf(record->wrap[i], &sampler->wrapMode[i])) {
            LayerComplain("%s: wrap mode %u is none of NVN's", what, (unsigned)record->wrap[i]);
            return false;
        }
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
    memset(sampler, 0, sizeof(*sampler));
    record->min = from->min;
    record->mag = from->mag;
    memcpy(record->wrap, from->wrap, sizeof(record->wrap));
    DkSampler probe;
    if (!SamplerOf(record, &probe, "nvnSamplerInitialize")) {
        return 0;
    }
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
    if (descriptor != NULL && SamplerOf(from, &made, WHAT)) {
        dkSamplerDescriptorInitialize(descriptor, &made);
    }
}

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

void Nvnl_nvnBlendStateSetDefaults(NVNblendState *state) {
    // Target 0, the source's colour replacing the target's: all the layer has setters for yet.
    LayerBlend *const record = StateRecord(state, "nvnBlendStateSetDefaults");
    if (record != NULL) {
        memset(state, 0, sizeof(*state));
    }
}

void Nvnl_nvnChannelMaskStateSetDefaults(NVNchannelMaskState *state) {
    LayerChannelMask *const record = StateRecord(state, "nvnChannelMaskStateSetDefaults");
    if (record != NULL) {
        record->masks = 0xffffffffu; // every channel of every target written
    }
}

void Nvnl_nvnColorStateSetDefaults(NVNcolorState *state) {
    LayerColor *const record = StateRecord(state, "nvnColorStateSetDefaults");
    if (record != NULL) {
        memset(state, 0, sizeof(*state)); // no target blends
    }
}

void Nvnl_nvnDepthStencilStateSetDefaults(NVNdepthStencilState *state) {
    // No depth test, depth written, less: a program sets the ones it relies on, as it cannot
    // ask what a driver's defaults are.
    LayerDepthStencil *const record = StateRecord(state, "nvnDepthStencilStateSetDefaults");
    if (record != NULL) {
        memset(state, 0, sizeof(*state));
        record->depth_write = 1;
        record->depth_func = NVN_DEPTH_FUNC_LESS;
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

void Nvnl_nvnMultisampleStateSetDefaults(NVNmultisampleState *state) {
    LayerMultisample *const record = StateRecord(state, "nvnMultisampleStateSetDefaults");
    if (record != NULL) {
        memset(state, 0, sizeof(*state)); // one sample
    }
}

void Nvnl_nvnPolygonStateSetDefaults(NVNpolygonState *state) {
    // Nothing culled, counterclockwise in front, filled; a program sets the ones it relies on.
    LayerPolygon *const record = StateRecord(state, "nvnPolygonStateSetDefaults");
    if (record != NULL) {
        memset(state, 0, sizeof(*state));
        record->cull = NVN_FACE_NONE;
        record->front = NVN_FRONT_FACE_CCW;
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

// ---- the binds

void Nvnl_nvnCommandBufferBindBlendState(NVNcommandBuffer *cmd, const NVNblendState *state) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferBindBlendState");
    const LayerBlend *const from = StateRecord(state, "nvnCommandBufferBindBlendState");
    if (record == NULL || from == NULL) {
        return;
    }
    DkBlendState blend;
    dkBlendStateDefaults(&blend);
    blend.srcColorBlendFactor = DkBlendFactor_One;
    blend.dstColorBlendFactor = DkBlendFactor_Zero;
    blend.srcAlphaBlendFactor = DkBlendFactor_One;
    blend.dstAlphaBlendFactor = DkBlendFactor_Zero;
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
    depth.stencilTestEnable = false;
    // NVN's depth functions are the graphics unit's, as deko3d's are: never 1 to always 8.
    depth.depthCompareOp = (DkCompareOp)from->depth_func;
    dkCmdBufBindDepthStencilState(record->cmdbuf, &depth);
}

void Nvnl_nvnCommandBufferBindMultisampleState(NVNcommandBuffer *cmd,
                                               const NVNmultisampleState *state) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferBindMultisampleState");
    if (record == NULL || StateRecord(state, "nvnCommandBufferBindMultisampleState") == NULL) {
        return;
    }
    DkMultisampleState multisample;
    dkMultisampleStateDefaults(&multisample);
    dkCmdBufBindMultisampleState(record->cmdbuf, &multisample);
}

void Nvnl_nvnCommandBufferBindPolygonState(NVNcommandBuffer *cmd, const NVNpolygonState *state) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferBindPolygonState");
    const LayerPolygon *const from = StateRecord(state, "nvnCommandBufferBindPolygonState");
    if (record == NULL || from == NULL) {
        return;
    }
    DkRasterizerState raster;
    dkRasterizerStateDefaults(&raster);
    // NVN's faces are deko3d's numbers, none 0 to both 3, and its front faces too.
    raster.cullMode = (DkFace)from->cull;
    raster.frontFace = (DkFrontFace)from->front;
    dkCmdBufBindRasterizerState(record->cmdbuf, &raster);
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

void Nvnl_nvnCommandBufferBindVertexBuffer(NVNcommandBuffer *cmd, int index,
                                           NVNbufferAddress address, size_t size) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferBindVertexBuffer");
    if (record == NULL) {
        return;
    }
    if (index < 0 || index >= DK_MAX_VERTEX_BUFFERS || size > UINT32_MAX) {
        LayerComplain("nvnCommandBufferBindVertexBuffer: stream %d of %d, 0x%llx bytes", index,
                      DK_MAX_VERTEX_BUFFERS, (unsigned long long)size);
        return;
    }
    dkCmdBufBindVtxBuffer(record->cmdbuf, (uint32_t)index, address, (uint32_t)size);
}

void Nvnl_nvnCommandBufferBindUniformBuffer(NVNcommandBuffer *cmd, NVNshaderStage stage,
                                            int index, NVNbufferAddress address, size_t size) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferBindUniformBuffer");
    if (record == NULL) {
        return;
    }
    const int to = StageOf(stage);
    // Slot r is deko3d's uniform buffer r + 1, the constant buffer an NVN program reads it from.
    if (to < 0 || index < 0 || index + 1 >= DK_NUM_UNIFORM_BUFS || size > DK_UNIFORM_BUF_MAX_SIZE) {
        LayerComplain("nvnCommandBufferBindUniformBuffer: stage %d, slot %d, 0x%llx bytes, where "
                      "the layer takes the graphics stages, slots 0 to %d and 0x%x bytes",
                      (int)stage, index, (unsigned long long)size, DK_NUM_UNIFORM_BUFS - 2,
                      (unsigned)DK_UNIFORM_BUF_MAX_SIZE);
        return;
    }
    dkCmdBufBindUniformBuffer(record->cmdbuf, (DkStage)to, (uint32_t)index + 1u, address,
                              (uint32_t)size);
}

void Nvnl_nvnCommandBufferBindTexture(NVNcommandBuffer *cmd, NVNshaderStage stage, int index,
                                      NVNtextureHandle handle) {
    LayerCommandBuffer *const record = CmdOf(cmd, "nvnCommandBufferBindTexture");
    if (record == NULL) {
        return;
    }
    if (StageOf(stage) < 0 || index < 0 || index >= LAYER_HANDLE_SLOTS) {
        LayerComplain("nvnCommandBufferBindTexture: stage %d, slot %d, where the layer takes "
                      "the graphics stages and slots 0 to %d",
                      (int)stage, index, LAYER_HANDLE_SLOTS - 1);
        return;
    }
    // Kept until the next draw, which is when the graphics unit has to find it.
    LayerDraw *const draw = record->draw;
    memcpy(draw->handles[stage] + LAYER_HANDLE_FIRST + 8u * (uint32_t)index, &handle, 8);
    draw->dirty |= (uint8_t)(1u << stage);
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
