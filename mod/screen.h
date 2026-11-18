// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ui.h"

#ifdef __cplusplus
extern "C" {
#endif

/// The mod's screens on its own window (docs/Mod.md): the start screen, the running screen and
/// the results, the plain program's own (source/ui.c), composed on the processor and copied into
/// the window's texture through the game's NVN. A game's process gives the mod no pad, so every
/// screen is padless: the start screen begins the run when its countdown ends, and the results
/// leave when theirs does.

/// The screens' frame, the window's size.
#define SCREEN_WIDTH 1280u
#define SCREEN_HEIGHT 720u

/// Brings the screens up on @p native, the mod's window: a device of the game's driver, its
/// queue, a command buffer, the window's two textures and the buffer each frame is copied from.
/// The driver must have been given its memory first. False with the step and its answer in
/// @p why, and nothing left up.
bool ScreenUp(void *native, char *why, size_t why_size);

/// Whether the screens are up: from ScreenUp to ScreenDown, the window is theirs.
bool ScreenIsUp(void);

/// Everything ScreenUp made, last first, once the queue has finished. Safe when nothing is up.
void ScreenDown(void);

/// The start screen for UI_START_NANOSECONDS, a frame a second as its countdown goes.
void ScreenStart(const UiStartInfo *info, UiStartState state);

/// The running screen, from the run's first step to its last: drawn at a step's start and at a
/// case's start or end, at most every UI_PROGRESS_NANOSECONDS, by the run's one thread.
void ScreenProgressBegin(const char *heading, const char *folder, uint32_t steps);
void ScreenProgressStep(uint32_t step, const char *name, bool timed);
void ScreenProgressEnd(void);

/// What the running screen hears of a case starting or ending (a ReportWatcher), for a watcher
/// that stands in its place to pass on.
void ScreenProgressWatch(const char *suite, const char *name, bool started);

/// The results of the run just closed, for UI_UNATTENDED_NANOSECONDS, a frame a second.
void ScreenResults(const char *heading);

/// What a frame's copy and present answered, for the report: "" while every one went through.
const char *ScreenError(void);

#ifdef __cplusplus
}
#endif
