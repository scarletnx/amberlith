// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "nvnlayer.h"

// What the layer's shared part (layer.c) and the one version file of a build (api55_deko3d.c or
// api231_surface.c) give each other. Nothing here is a program's to call.

/// One name the lookup answers and its function.
typedef struct {
    const char *name;
    NvnGenericProc proc;
} LayerEntry;

/// The build's functions of NVN_LAYER_PROCS, in the list's order: the version file defines the
/// table, with the bodies. The two functions the list leaves out are layer.c's.
extern const LayerEntry LAYER_ENTRIES[];
extern const size_t LAYER_ENTRY_COUNT;

/// What nvnDeviceGetInteger answers for an item past the two its switch holds (the API version):
/// the version file's.
void NvnLayerDeviceInfo(const NVNdevice *device, int item, int *value);

/// Keeps the first failure since the last clear in NvnLayerError: whatever fails after it is
/// usually its consequence, and the first one is the one worth a line in the report.
void LayerComplain(const char *format, ...) __attribute__((format(printf, 1, 2)));

/// LayerComplain for a call the build has no way to do at all, whatever its arguments: the
/// program can tell that from a call it got wrong (NvnLayerLacks).
void LayerLack(const char *format, ...) __attribute__((format(printf, 1, 2)));

/// Runs @p call under the program's guard (NvnLayerSetGuard), bare without one. On an abort,
/// false, with a complaint that names @p what and the result.
bool LayerGuarded(void (*call)(void), const char *what);

/// Ends the program for a body of the API 231 build that was reached: the program's hook
/// (NvnLayerSetFatal), then LayerStop with the sentence.
void LayerFatal(const char *name) __attribute__((noreturn));

/// The last step of LayerFatal, the version file's: @p text to the machine's debug output, then
/// a break that does not come back.
void LayerStop(const char *text) __attribute__((noreturn));
