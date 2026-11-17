// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// What the mod says of the game it is loaded in (docs/Mod.md): the SDK and the build of the
/// module that holds the NVN driver, read from that module's own read-only data. They name the
/// driver a report was made on, and nothing here names the game.

/// What an SDK module's label starts with: "SDK MW+Nintendo+NintendoSdk_nnSdk-20_5_0-Release".
#define HOST_SDK_LABEL "SDK MW+Nintendo+NintendoSdk_nnSdk-"
/// The longest version a label gives, with its end.
#define HOST_SDK_VERSION_MAX 24u
/// The longest build id a module's note holds, and its text, two digits a byte.
#define HOST_BUILD_ID_MAX 32u
#define HOST_BUILD_ID_TEXT (2u * HOST_BUILD_ID_MAX + 1u)

/// The SDK version of the first label in @p bytes, as the label writes it less its build kind:
/// "20_5_0" as "20.5.0". False when no label is there, or its version is empty or longer than
/// HOST_SDK_VERSION_MAX; @p out is then "".
bool HostSdkVersion(const uint8_t *bytes, size_t size, char out[HOST_SDK_VERSION_MAX + 1u]);

/// The build id of the last GNU build id note in @p bytes, as hexadecimal text: the linker puts
/// the note at the end of a module's read-only data. False when none is there, or its id is
/// empty or longer than HOST_BUILD_ID_MAX; @p out is then "".
bool HostBuildId(const uint8_t *bytes, size_t size, char out[HOST_BUILD_ID_TEXT]);

/// The read-only data of the module whose code holds @p function: the mapping after its code,
/// as the kernel's memory query gives the two. False when the address is in no code.
bool HostModuleData(uintptr_t function, const uint8_t **bytes, size_t *size);

#ifdef __cplusplus
}
#endif
