// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <switch.h>

#include "platform.h"

// The mod's side of platform.h: the engine inside a game's process, where the kernel's calls are
// the only thing of libnx's that is linked. No sockets, no network question and no drawing
// surface: the mod's steps run none of the cases that ask for them.

void PlatformLog(const char *line, size_t length) {
    svcOutputDebugString(line, length);
}

bool PlatformLogIsStandardOutput(void) {
    return false;
}

const char *PlatformError(void) {
    return "the mod opens no sockets: a game's process has its own";
}

bool PlatformSocketsOpen(void) {
    return false;
}

void PlatformSocketsClose(void) {
}

size_t PlatformNetworkOff(char *out, size_t out_size) {
    if (out != NULL && out_size != 0u) {
        out[0] = '\0';
    }
    return 0;
}
