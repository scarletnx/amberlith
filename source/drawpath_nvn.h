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
    /// The shader data each program the scene made was set from, a vertex stage and a fragment
    /// stage, in the order DrawPathProgram numbered them.
    const NVNshaderData (*stages)[2];
} DrawPathNvnParts;

/// Fills @p parts for @p path. False for the deko3d way, which has none of them.
bool DrawPathNvn(DrawPath *path, DrawPathNvnParts *parts);

/// The shader data of one more stage, for a program a scene makes itself (a stage past the
/// two DrawPathProgram takes): @p dksh, a whole compiled module, with its code in the way's
/// code pool and a control section the way keeps until it is taken down. False with
/// DrawPathFailure saying why, and on the deko3d way.
bool DrawPathNvnStage(DrawPath *path, const void *dksh, uint32_t dksh_bytes,
                      NVNshaderData *stage);

#ifdef __cplusplus
}
#endif
