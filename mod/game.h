// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "sdk.h"

#ifdef __cplusplus
extern "C" {
#endif

/// What the mod asks of the game it is loaded in beyond nn::fs's file functions (sdk.h) and the
/// driver's nvnBootstrapLoader (nvnlayer.h), each by the SDK's own symbol and bound by the game's
/// dynamic linker (docs/Mod.md). mod/imports.txt lists every one, and tools/mod-imports.py holds
/// the built module to that list.
///
/// All of these are weak: a game whose SDK has none of a name leaves its address NULL, and the
/// mod says which it went without. A call is made only through an address compared first.
#define GAME_OPTIONAL(symbol) __asm__(symbol) __attribute__((weak))

/// nn::fs::MountSdCardForDebug, the mount every SDK has and mods' loaders call, and
/// nn::fs::MountSdCard, tried when the first is not there or fails.
uint32_t GameMountSdCardForDebug(const char *name)
    GAME_OPTIONAL("_ZN2nn2fs19MountSdCardForDebugEPKc");
extern __typeof__(NnFsMountSdCard) NnFsMountSdCard __attribute__((weak));

/// nn::oe::Initialize: the game's own start calls it before its main, which the mod runs before.
void GameOeInitialize(void) GAME_OPTIONAL("_ZN2nn2oe10InitializeEv");

/// The graphics memory a game gives its driver before it makes an NVN device:
/// nv::SetGraphicsAllocator with the three functions the driver allocates with, and
/// nv::InitializeGraphics with the block it keeps for itself.
typedef void *(*GameAllocate)(size_t size, size_t alignment, void *user);
typedef void (*GameFree)(void *address, void *user);
typedef void *(*GameReallocate)(void *address, size_t size, void *user);
void GameSetGraphicsAllocator(GameAllocate allocate, GameFree free, GameReallocate reallocate,
                              void *user)
    GAME_OPTIONAL("_ZN2nv20SetGraphicsAllocatorEPFPvmmS0_EPFvS0_S0_EPFS0_S0_mS0_ES0_");
void GameInitializeGraphics(void *memory, size_t size)
    GAME_OPTIONAL("_ZN2nv18InitializeGraphicsEPvm");

/// The mod's frontend (mod/frontend.c): the run, from the mode file to the closed report, with
/// what the start saw of the card for its first case. Returns the process's exit status.
typedef struct {
    /// Which mount answered, and each one's result: UINT32_MAX for one the game has not.
    const char *mounted_by;
    uint32_t for_debug_rc;
    uint32_t plain_rc;
} ModStart;
int ModRun(const ModStart *start);

#ifdef __cplusplus
}
#endif
