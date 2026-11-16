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
    return Nvnl_nvnProgramSetShaders(program, count, plain);
}
