// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string.h>

#include "hang.h"

// Ryujinx 1.3.351 accepts SO_RCVTIMEO and then never returns from the receive. The rescue
// thread is queued on the core the receive holds, so it does not run either, and the launch
// stays there until something else kills it. ScarletNX 2f46296 returns -1 with EAGAIN after
// 300 ms (`quick hooks cpu net unban hang`, 2026-11-17, Amberlith 1.0.0).
static const Hang TABLE[] = {
    { "bsd", HANG_BSD_TIMEOUT,
      "Ryujinx 1.3.351 accepts SO_RCVTIMEO and never returns from the receive, and the rescue "
      "thread is queued on the core that receive holds",
      "scarletnx" },
};

size_t HangCount(void) {
    return sizeof(TABLE) / sizeof(TABLE[0]);
}

const Hang *HangAt(size_t index) {
    return index < HangCount() ? &TABLE[index] : NULL;
}

const Hang *HangFind(const char *suite, const char *name) {
    for (size_t i = 0; suite != NULL && name != NULL && i < HangCount(); ++i) {
        if (strcmp(TABLE[i].suite, suite) == 0 && strcmp(TABLE[i].name, name) == 0) {
            return &TABLE[i];
        }
    }
    return NULL;
}

bool HangReturnsOn(const Hang *hang, const char *machine) {
    const size_t length = machine != NULL ? strlen(machine) : 0u;
    for (const char *at = hang != NULL ? hang->returns_on : NULL; at != NULL && length > 0u;) {
        at = strstr(at, machine);
        if (at == NULL) {
            break;
        }
        const bool starts = at == hang->returns_on || at[-1] == ' ';
        if (starts && (at[length] == '\0' || at[length] == ' ')) {
            return true;
        }
        at += length;
    }
    return false;
}
