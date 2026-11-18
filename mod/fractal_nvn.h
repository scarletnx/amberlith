// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/// The demo family's Mandelbrot and Julia zooms through the game's NVN, as the plain program's
/// mandelbrot and julia steps run them through deko3d: the same cases, frames, times and judge
/// (fractalzoom.h), the same GLSL compiled for NVN's layout.
void RunMandelbrotNvn(void);
void RunJuliaNvn(void);

/// The game's driver asked its API version and given its memory, once, as before any device of
/// the mod's (frontend.c): false with why in @p why when the driver is of another API or the
/// game's SDK lacks what a game calls first.
bool ModGraphicsReady(char *why, size_t why_size);

#ifdef __cplusplus
}
#endif
