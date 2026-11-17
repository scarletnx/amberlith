// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sys/iosupport.h>

#ifdef __cplusplus
extern "C" {
#endif

/// The nn::fs mount name of the memory card when the C library's files are nn::fs's: the C
/// library's device name, so a path the program writes is the SDK's path unchanged.
#define SDK_CARD "sdmc"

/// The C library's sdmc: device over the SDK's nn::fs functions (sdk.h), for AddDevice once the
/// card is mounted as SDK_CARD: every file the program opens is then nn::fs's, each write
/// flushed, as a journal line must be on the card before the next case may end the program.
const devoptab_t *SdkCardDevice(void);

#ifdef __cplusplus
}
#endif
