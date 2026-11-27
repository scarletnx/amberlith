// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

/// The GBA's video suite, gbavideo, the GBA frontend's own (docs/families/Graphics.md#gbavideo): the
/// line counter, the status flags and their interrupts, a frame's length, and stores to VRAM,
/// palette RAM and OAM in each display mode, judged by source/gbavideoref.c.
void RunGbaVideo(void);
