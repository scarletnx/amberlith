// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "layer.h"

// The build for a game's own driver (layer.mk's NVNLAYER_DRIVER := game), at either API version:
// the bodies the loader puts under the names that game's driver has no function for. A driver
// has the names of its own API version, so a name it lacks is nothing to end the program for.
// The body keeps the name for the program to read (LayerMissed) and returns nought, as a call
// that did nothing would; the program goes on, and finalizes itself whatever it had made.
//
// Two bodies a name, by what the function returns, since nought comes back in another register
// for a float: the table takes the one of the function's own result. They take no arguments:
// none is ever read, and a program calls them through NvnProcs, which casts each to its NVN
// prototype, as it would a driver's.

#define NVN_LAYER_BODY(result, name, parameters)                                                  \
    __attribute__((unused)) static uint64_t NvnlWord_##name(void) {                               \
        LayerMissed(#name);                                                                       \
        return 0u;                                                                                \
    }                                                                                             \
    __attribute__((unused)) static float NvnlFloat_##name(void) {                                 \
        LayerMissed(#name);                                                                       \
        return 0.0f;                                                                              \
    }
NVN_LAYER_PROCS(NVN_LAYER_BODY)
#undef NVN_LAYER_BODY

const LayerEntry LAYER_ENTRIES[] = {
#define NVN_LAYER_ENTRY(result, name, parameters)                                                 \
    { #name, (NvnGenericProc)_Generic((result(*)(void))0,                                         \
                                      float (*)(void): NvnlFloat_##name,                          \
                                      default: NvnlWord_##name) },
    NVN_LAYER_PROCS(NVN_LAYER_ENTRY)
#undef NVN_LAYER_ENTRY
};
const size_t LAYER_ENTRY_COUNT = sizeof(LAYER_ENTRIES) / sizeof(LAYER_ENTRIES[0]);
