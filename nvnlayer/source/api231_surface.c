// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string.h>

#ifdef __SWITCH__
#include <switch.h>
#endif

#include "layer.h"

// The API 231 build: the names of NVN_LAYER_PROCS and nothing under them. A program for the
// Switch 2 has no machine that runs its graphics unit from homebrew, so there is nothing for a
// body to record: ScarletNX takes every function the program resolves, as it takes a game's, and
// the storage the program hands each call (nvn/nvn.h's sizes for this API) holds ScarletNX's
// record. A body that runs means ScarletNX did not take that function. It stops the program with
// the function's name, so the run ends on a line that says what is missing instead of going on
// with nothing drawn.
//
// A build for a game's own driver (layer.c) compiles this file too, at either API version: there
// the bodies stand under the names that game's driver has no function for.
//
// The bodies take no arguments: none is ever read, and one shape lets a test call every one of
// them the same way. A program calls them through NvnProcs, which casts each to its NVN
// prototype, as it would a driver's.

#define NVN_LAYER_BODY(result, name, parameters)                                                  \
    static void Nvnl_##name(void) {                                                               \
        LayerFatal(#name);                                                                        \
    }
NVN_LAYER_PROCS(NVN_LAYER_BODY)
#undef NVN_LAYER_BODY

const LayerEntry LAYER_ENTRIES[] = {
#define NVN_LAYER_ENTRY(result, name, parameters) { #name, (NvnGenericProc)Nvnl_##name },
    NVN_LAYER_PROCS(NVN_LAYER_ENTRY)
#undef NVN_LAYER_ENTRY
};
const size_t LAYER_ENTRY_COUNT = sizeof(LAYER_ENTRIES) / sizeof(LAYER_ENTRIES[0]);

/// nvnDeviceGetInteger's items past the API version, which the switch in layer.c holds and
/// ScarletNX reads from its code: ScarletNX answers these from its own table, so reaching this
/// is a function it did not take, like any other body here.
void NvnLayerDeviceInfo(const NVNdevice *device, int item, int *value) {
    (void)device;
    (void)item;
    (void)value;
    LayerFatal("nvnDeviceGetInteger");
}

#ifdef __SWITCH__
void LayerStop(const char *text) {
    svcOutputDebugString(text, strlen(text));
    for (;;) {
        svcBreak(BreakReason_Panic, 0, 0);
    }
}
#endif
