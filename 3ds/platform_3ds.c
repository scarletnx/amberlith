// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The 3DS's side of platform.h (docs/Frontends.md): its log is the kernel's debug string, which an
// emulator logs; sockets are soc:U, brought up with a buffer of their own; no drawing surface,
// since its steps run no draw suite.

#include <3ds.h>
#include <stdio.h>
#include <string.h>

#include "platform.h"

// soc:U takes the memory its sockets work in from the program, page aligned, for as long as it is
// up: 1 MiB, the size libctru's own examples give it.
#define PLATFORM_3DS_SOC_SIZE 0x100000u

static struct {
    alignas(0x1000) uint8_t soc[PLATFORM_3DS_SOC_SIZE];
    char error[160];
} g_platform_3ds;

void PlatformLog(const char *line, size_t length) {
    // A console without a debugger attached drops it, which is why every line also goes to the
    // report file.
    svcOutputDebugString(line, (s32)length);
}

bool PlatformLogIsStandardOutput(void) {
    return false;
}

const char *PlatformError(void) {
    return g_platform_3ds.error;
}

bool PlatformSocketsOpen(void) {
    const Result opened = socInit((u32 *)(void *)g_platform_3ds.soc, PLATFORM_3DS_SOC_SIZE);
    if (R_FAILED(opened)) {
        snprintf(g_platform_3ds.error, sizeof(g_platform_3ds.error),
                 "socInit(%u bytes) returned 0x%08lx: soc:U cannot be opened from this program",
                 PLATFORM_3DS_SOC_SIZE, (unsigned long)opened);
        return false;
    }
    return true;
}

void PlatformSocketsClose(void) {
    socExit();
}

size_t PlatformNetworkOff(char *out, size_t out_size) {
    if (out == nullptr || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';
    if (R_FAILED(acInit())) {
        return 0;
    }
    u32 status = 0;
    const Result asked = ACU_GetWifiStatus(&status);
    acExit();
    if (R_SUCCEEDED(asked) && status == 0u) {
        snprintf(out, out_size, "ac:u says no wireless network is connected (status 0)");
    } else if (R_FAILED(asked)) {
        snprintf(out, out_size, "ac:u has no wireless status (0x%08lx)", (unsigned long)asked);
    }
    return strlen(out);
}
