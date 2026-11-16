// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "api55.h"

// The API 55 build's objects past what a draw takes: what a device and its builder answer of
// themselves, the clear values a program registers with it, and a program set from shader data
// of the longer kind.

// ---- the device

int Nvnl_nvnDeviceBuilderGetFlags(const NVNdeviceBuilder *builder) {
    const LayerDeviceBuilder *const record = Record(builder, "nvnDeviceBuilderGetFlags");
    return record != NULL ? record->flags : 0;
}

NVNwindowOriginMode Nvnl_nvnDeviceGetWindowOriginMode(const NVNdevice *device) {
    const LayerDevice *const record = DeviceOf(device, "nvnDeviceGetWindowOriginMode");
    return record != NULL ? (NVNwindowOriginMode)record->origin : NVN_WINDOW_ORIGIN_MODE_LOWER_LEFT;
}

NVNdepthMode Nvnl_nvnDeviceGetDepthMode(const NVNdevice *device) {
    const LayerDevice *const record = DeviceOf(device, "nvnDeviceGetDepthMode");
    return record != NULL ? (NVNdepthMode)record->depth : NVN_DEPTH_MODE_NEAR_IS_MINUS_W;
}

// A value a program says it will clear to, so the graphics unit can keep a target of it
// compressed: a hint, which deko3d has no word for. Taken, and every clear is the same clear.
NVNboolean Nvnl_nvnDeviceRegisterFastClearColor(NVNdevice *device, const float *color,
                                                NVNformat format) {
    (void)color;
    (void)format;
    return DeviceOf(device, "nvnDeviceRegisterFastClearColor") != NULL;
}

NVNboolean Nvnl_nvnDeviceRegisterFastClearColori(NVNdevice *device, const int *color,
                                                 NVNformat format) {
    (void)color;
    (void)format;
    return DeviceOf(device, "nvnDeviceRegisterFastClearColori") != NULL;
}

NVNboolean Nvnl_nvnDeviceRegisterFastClearColorui(NVNdevice *device, const uint32_t *color,
                                                  NVNformat format) {
    (void)color;
    (void)format;
    return DeviceOf(device, "nvnDeviceRegisterFastClearColorui") != NULL;
}

NVNboolean Nvnl_nvnDeviceRegisterFastClearDepth(NVNdevice *device, float depth) {
    (void)depth;
    return DeviceOf(device, "nvnDeviceRegisterFastClearDepth") != NULL;
}

NVNboolean Nvnl_nvnDeviceRegisterFastClearStencil(NVNdevice *device, int stencil) {
    (void)stencil;
    return DeviceOf(device, "nvnDeviceRegisterFastClearStencil") != NULL;
}

// The handles of a texture and of a sampler apart, which a shader puts together itself, and of a
// texture read with no sampler. Each is the descriptor's number where deko3d's one handle has
// it, the texture's in the low twenty bits and the sampler's above them, so the two low words
// one over the other are deko3d's handle; a texture's alone also has bit 32 set, as a console's
// driver sets it (ScarletNX has that from two of them), which no shader reads.
#define HANDLE_ALONE ((uint64_t)1u << 32)

/// Whether @p id is a descriptor of the kind the handle is for; a complaint naming @p what when
/// it is not.
static bool Descriptor(int id, int count, const char *what) {
    if (id < 0 || id >= count) {
        LayerComplain("%s: descriptor %d, where a pool holds 0 to %d", what, id, count - 1);
        return false;
    }
    return true;
}

uint64_t Nvnl_nvnDeviceGetSeparateTextureHandle(const NVNdevice *device, int id) {
    (void)device;
    return Descriptor(id, 1 << 20, "nvnDeviceGetSeparateTextureHandle")
               ? (uint64_t)(uint32_t)id | HANDLE_ALONE
               : 0u;
}

uint64_t Nvnl_nvnDeviceGetSeparateSamplerHandle(const NVNdevice *device, int id) {
    (void)device;
    return Descriptor(id, 1 << 12, "nvnDeviceGetSeparateSamplerHandle")
               ? (uint64_t)(uint32_t)id << 20
               : 0u;
}

uint64_t Nvnl_nvnDeviceGetTexelFetchHandle(const NVNdevice *device, int id) {
    (void)device;
    return Descriptor(id, 1 << 20, "nvnDeviceGetTexelFetchHandle")
               ? (uint64_t)(uint32_t)id | HANDLE_ALONE
               : 0u;
}

// ---- programs

/// NVN 55's longer shader data: NVNshaderData and eight bytes more, which name nothing the
/// layer draws with.
typedef struct {
    NVNshaderData data;
    uint64_t more;
} ShaderDataExt;
#define PROGRAM_STAGES_MAX 6

NVNboolean Nvnl_nvnProgramSetShadersExt(NVNprogram *program, int count, const void *stages) {
    if (count < 1 || count > PROGRAM_STAGES_MAX || stages == NULL) {
        LayerComplain("nvnProgramSetShadersExt: %d stages%s, where a program has 1 to %d", count,
                      stages == NULL ? " and no shader data" : "", PROGRAM_STAGES_MAX);
        return 0;
    }
    NVNshaderData plain[PROGRAM_STAGES_MAX];
    for (int i = 0; i < count; ++i) {
        plain[i] = ((const ShaderDataExt *)stages)[i].data;
    }
    return LayerProgramSetShaders(program, count, plain, "nvnProgramSetShadersExt");
}
