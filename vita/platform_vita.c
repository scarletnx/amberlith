// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The Vita's side of platform.h (docs/Frontends.md): its log is the kernel's printf, which an
// emulator prints; no sockets, since its steps run no socket suite; no drawing surface, since its
// drawing is its own suite (vita/gxm.c).

#include <psp2/kernel/clib.h>
#include <stdio.h>

#include "platform.h"

void PlatformLog(const char *line, size_t length) {
    sceClibPrintf("%.*s\n", (int)length, line);
}
bool PlatformLogIsStandardOutput(void) {
    return false;
}
const char *PlatformError(void) {
    return "the Vita's frontend does not bring up its network";
}
bool PlatformSocketsOpen(void) {
    return false;
}
void PlatformSocketsClose(void) {}
size_t PlatformNetworkOff(char *out, size_t out_size) {
    if (out != nullptr && out_size > 0u) {
        out[0] = '\0';
    }
    return 0;
}
