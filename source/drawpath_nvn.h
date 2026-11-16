// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <nvnlayer.h>

#include "drawpath.h"

#ifdef __cplusplus
extern "C" {
#endif

/// What an NVN way is made of, for a scene that records NVN calls drawpath.h has no word for
/// (a counter report, a render state, a clear of its own): the functions the way draws with,
/// the layer's own or the looked-up ones as the way's kind says, and the objects they take.
/// Such a scene goes down the two NVN ways only.
typedef struct {
    const NvnProcs *nvn;
    NVNdevice *device;
    NVNqueue *queue;
    /// Recording between DrawPathBegin and DrawPathEnd.
    NVNcommandBuffer *command;
    NVNtexture *target;
    /// NULL when the scene asked for no depth target.
    NVNtexture *depth;
    /// The memory the command buffer was given as the way came up: all of this pool, and this
    /// control memory.
    NVNmemoryPool *command_pool;
    size_t command_bytes;
    void *control;
    size_t control_bytes;
} DrawPathNvnParts;

/// Fills @p parts for @p path. False for the deko3d way, which has none of them.
bool DrawPathNvn(DrawPath *path, DrawPathNvnParts *parts);

#ifdef __cplusplus
}
#endif
