// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/// The NVN API version this build of the layer is: 55 or 231, set by the build (layer.mk's
/// NVNLAYER_API, which gives the layer and the program that links it the same value).
#ifndef NVN_LAYER_API
#define NVN_LAYER_API 55
#endif

#include "nvn/nvn.h"
#include "nvnlayer_procs.h"

#ifdef __cplusplus
extern "C" {
#endif

/// NVN, the console's graphics API, for a homebrew program: the functions a game gets by name
/// from nvnBootstrapLoader and the nvnDeviceGetProcAddress it hands back.
///
/// A homebrew program carries no NVN: each game brings its own driver. This layer stands where
/// that driver does, so a program written against NVN runs. One layer, built for one API version
/// (docs/NvnLayer.md says which parts are shared and which are a version's):
///
/// - API 55 (NVN_LAYER_API 55, the Switch's): NVN on deko3d. On the console and in an emulator
///   that runs the graphics unit, its functions record deko3d commands, and what the graphics
///   unit does with them is the answer. ScarletNX runs a game's NVN calls itself: it finds
///   nvnBootstrapLoader among the program's exported functions, puts its own implementation in
///   place of every function the program resolves through it, and none of the layer's bodies
///   runs past the name lookup.
/// - API 231 (NVN_LAYER_API 231, the Switch 2's): the names and the object sizes ScarletNX's
///   API 231 implementation takes, and nothing under them: no machine but ScarletNX runs such a
///   program, so a body that is reached means ScarletNX did not take that function, and it
///   stops the program saying which (NvnLayerSetFatal).

/// The type every NVN lookup answers with, cast to the function's own type by its caller.
typedef void (*NvnGenericProc)(void);

/// The NVN API version the layer answers through nvnDeviceGetInteger, which ScarletNX reads out
/// of that function's code before the program asks. The API 55 build answers 55.8, one
/// ScarletNX has measured a game's driver for, and not the 55.15 ScarletNX answers when it
/// cannot read a program's version from its code, so a program can tell the two apart. The API
/// 231 build answers 231.16, the version ScarletNX has an Ounce game's layouts for.
#if NVN_LAYER_API >= 231
#define NVN_LAYER_API_MAJOR 231
#define NVN_LAYER_API_MINOR 16
#else
#define NVN_LAYER_API_MAJOR 55
#define NVN_LAYER_API_MINOR 8
#endif

/// The prototypes of the two functions NVN_LAYER_PROCS leaves out.
typedef void (*NvnDeviceGetIntegerProc)(const NVNdevice *device, NVNdeviceInfo item, int *value);
typedef NvnGenericProc (*NvnDeviceGetProcAddressProc)(const NVNdevice *device, const char *name);

/// Every function of the layer, as a program holds them once it has looked them up.
typedef struct {
    NvnDeviceGetIntegerProc nvnDeviceGetInteger;
    NvnDeviceGetProcAddressProc nvnDeviceGetProcAddress;
#define NVN_LAYER_FIELD(result, name, parameters) result(*name) parameters;
    NVN_LAYER_PROCS(NVN_LAYER_FIELD)
#undef NVN_LAYER_FIELD
} NvnProcs;

/// The program's NVN entry point, as a game's driver exports it: the function named @p name, or
/// NULL for a name the layer does not have. The program's link exports it (layer.mk's
/// NVNLAYER_LDFLAGS), because an exported nvnBootstrapLoader is what ScarletNX looks for.
NvnGenericProc nvnBootstrapLoader(const char *name);

/// Fills @p procs the way a game does: nvnDeviceGetProcAddress from nvnBootstrapLoader, then
/// every other function from it, with no device. The bootstrap is called through a pointer, as
/// a game's code calls it through its import, so ScarletNX's implementation takes the call even
/// where a direct branch would be compiled as one. False with the first name that came back
/// NULL in @p missing.
bool NvnLoadProcs(NvnProcs *procs, char *missing, size_t missing_size);

/// Fills @p procs with the layer's own functions, through no lookup: what NvnLoadProcs gives on
/// a machine that takes nothing over. For a program that wants the layer's bodies themselves
/// wherever it runs, as a test of the layer does: under ScarletNX they then record deko3d
/// commands like any deko3d program's, where the functions NvnLoadProcs gives are ScarletNX's.
///
/// ScarletNX takes a function by its address, at the moment the program first resolves its name
/// through nvnBootstrapLoader or nvnDeviceGetProcAddress, and from then on every call to that
/// address is ScarletNX's, these included. So a program that uses both asks for these, and is
/// done with them, before its first NvnLoadProcs. After one, on every machine alike, this
/// returns false with the reason in NvnLayerError and leaves @p procs as it was: the functions
/// would be the layer's on one machine and not on another.
bool NvnLayerOwnProcs(NvnProcs *procs);

/// The last failure the layer met under deko3d, or "" when none: a sentence for the program's
/// own report. The layer keeps NVN's signatures, which have no room for a reason; this is where
/// it goes. Always "" under ScarletNX, whose implementation replaces the layer's functions, and
/// in the API 231 build, whose bodies stop the program instead.
const char *NvnLayerError(void);

/// Clears NvnLayerError, before a step whose failure should name only its own reason.
void NvnLayerClearError(void);

/// Whether a call since the last clear was one the layer has no way to do at all under deko3d,
/// whatever its arguments (NvnLayerError says which and why): a program that can do without
/// tells that from a call it got wrong. Never under ScarletNX, as NvnLayerError is never set.
bool NvnLayerLacks(void);

/// What runs one of deko3d's constructors, which end the program on a failure: @p call, with
/// false and the abort's result in @p result when it aborted. A program that can turn deko3d's
/// abort into a return (a linker wrap of diagAbortWithResult and a longjmp, as edenprobe's
/// gpu.c has) gives it here, and a failed bring-up then reads as a false NVN result with its
/// reason in NvnLayerError. Without one, NULL, the constructors run bare and a failure ends the
/// program, as it does for any deko3d program.
typedef bool (*NvnLayerGuard)(void (*call)(void), uint32_t *result);
void NvnLayerSetGuard(NvnLayerGuard guard);

/// What the API 231 build calls first when one of its bodies is reached, with the function's
/// name: the program's chance to put the line in its own log or report. The layer then writes
/// "nvnlayer: FATAL: <name> was reached: no NVN implementation took it" to the debug output and
/// breaks (svcBreak), so the run ends there instead of drawing nothing. NULL, the default, for
/// no call. The API 55 build has every body and never calls it.
typedef void (*NvnLayerFatalHook)(const char *name);
void NvnLayerSetFatal(NvnLayerFatalHook hook);

// ---- programs (the API 55 build)

#if NVN_LAYER_API < 231
/// The bytes NvnLayerShaderControl writes: an NVN shader control section as ScarletNX reads one,
/// then the layer's own record of the DKSH it came from.
#define NVN_LAYER_CONTROL_BYTES 0x900u

/// Where a stage's code is, for a program compiled by uam into a DKSH, the only compiler a
/// homebrew program has.
typedef struct {
    /// The DKSH's code section, which the program copies to a multiple of 0x100 bytes into a
    /// SHADER_CODE pool: NVNshaderData.data is then the pool's buffer address plus that offset.
    /// It starts with the 0x30 bytes a console's NVN programs keep before the program header,
    /// as uam writes them.
    const void *code;
    uint32_t code_bytes;
} NvnLayerShaderCode;

/// Makes the control section of one stage from @p dksh, a whole module of @p dksh_bytes as uam
/// wrote it, into @p control, NVN_LAYER_CONTROL_BYTES of memory the program keeps for as long as
/// the NVN program made from it lives, 8-byte aligned; and says in @p code where the stage's
/// code is. False, with the reason in NvnLayerError, for a module that is not a DKSH of one
/// program.
///
/// The section holds what ScarletNX's nvnProgramSetShaders reads of a game's (the magic, the
/// format, the stage, the program's offset past the 0x30 bytes, the constants' offset and the
/// code area's bytes), written from the DKSH's own header, and after it the DKSH's control
/// section, which is what the API 55 build's nvnProgramSetShaders gives deko3d. The code is the
/// same bytes on both: the graphics unit runs what uam compiled, and ScarletNX translates it.
///
/// Such a program reads its constant buffers where deko3d and NVN both put them, which leaves
/// two rules for its GLSL (docs/NvnLayer.md, "What a shader may use"): uniform block binding N
/// is NVN's uniform buffer slot N - 1, binding 0 being the stage's texture handles; and a
/// sampler is made from the handle at 0x20 + 8 * slot of that block (GL_ARB_bindless_texture),
/// never declared as a uniform of its own.
bool NvnLayerShaderControl(const void *dksh, size_t dksh_bytes, void *control,
                           NvnLayerShaderCode *code);

/// The outputs a stage's capture takes words of (transform feedback): the position, or generic
/// output @p n, which is `layout (location = n) out` in the stage's GLSL.
#define NVN_LAYER_CAPTURE_POSITION 0
#define NVN_LAYER_CAPTURE_GENERIC(n) (0x20 + (n))
/// @p count words of @p output from word @p first, of the four an output has.
typedef struct {
    uint8_t output;
    uint8_t first;
    uint8_t count;
} NvnLayerCaptureGroup;

/// Says in @p control, a section NvnLayerShaderControl wrote for the last stage before the
/// rasterizer, that the stage's outputs are captured into capture buffer @p buffer, 0 to 3: each
/// vertex the words of the @p count groups at @p groups, one group after the other, so a
/// captured vertex is four bytes a word. Before nvnProgramSetShaders, which is where a runtime
/// reads it. False, with the reason in NvnLayerError, for a section, a buffer or a group it
/// cannot write.
///
/// uam says nothing of a capture, and a console's compiler writes it where this does, which is
/// where ScarletNX reads it. The API 55 build's own bodies capture nothing (deko3d has no call
/// for it), so only the NVN a program looks up captures.
bool NvnLayerShaderCapture(void *control, int buffer, const NvnLayerCaptureGroup *groups,
                           int count);
#endif

#ifdef __cplusplus
}
#endif
