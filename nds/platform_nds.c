// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The DS's side of platform.h (docs/Frontends.md): its log is no$gba's debug port, which no$gba
// and melonDS print; no sockets, since its steps run no socket suite; no drawing surface, since its
// steps run no draw suite.

#include <nds.h>

#include "platform.h"

void PlatformLog(const char *line, size_t length) {
    // A console drops what goes there, which is why every line also goes to the report file when
    // the card has one.
    nocashWrite(line, length);
    nocashWrite("\n", 1u);
}

bool PlatformLogIsStandardOutput(void) {
    return false;
}

const char *PlatformError(void) {
    return "the DS's frontend does not bring up its network";
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
