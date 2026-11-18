// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "demo.h"
#include "fractalref.h"

#ifdef __cplusplus
extern "C" {
#endif

/// The demo family's fractal zooms on any way to the graphics unit: the cases, the frames, their
/// times and the judge of the judged frames, the same on each, so a report reads the same case
/// names and times whichever way drew it. The plain program's way is deko3d's
/// (tests/t_fractal.c), the mod's a game's NVN (mod/fractal_nvn.c).
///
/// A frame is measured the same on each way: its work from the processor filling its block to the
/// end of its wait for the graphics unit, and present to present from one frame's present to the
/// next's, the wait for an image to draw into included.

/// The uniform slots a way keeps a frame's block in, a frame each in turn: the processor writes
/// one while the graphics unit may still read the other.
#define FRACTAL_SLOTS 2u

typedef struct {
    /// The device with compute, the programs, the counts, the judged frames' copies and a way to
    /// the display. False with the step and its answer in @p why.
    bool (*up)(char *why, size_t why_size);
    /// The image frame @p k draws into: its index on the display's ring, taken before the frame's
    /// work begins. Nought for a way with no ring.
    int (*acquire)(void);
    /// The block of uniform slot @p slot, which the processor writes a frame's numbers into.
    FractalFrame *(*block)(uint32_t slot);
    /// The frame's work, recorded from slot @p slot, with the copies the judge reads when it is
    /// judged frame @p judged (FRACTAL_JUDGED when it is none), submitted and waited for.
    void (*draw)(uint32_t slot, uint32_t judged, int image);
    /// The frame drawn shown: presented on image @p image, or through the program's screen.
    /// Whether it reached the display.
    bool (*present)(int image);
    /// The last frame shown again, while the demo is paused.
    void (*show_again)(void);
    /// Whether the graphics unit took every submit so far, and what it said when it did not.
    bool (*ready)(void);
    const char *(*failure)(void);
    /// Judged frame @p j's counts and image, as the graphics unit copied them aside.
    const uint8_t *(*kept_counts)(uint32_t j);
    const uint8_t *(*kept_image)(uint32_t j);
    /// Everything up undone, whatever came of it.
    void (*close)(void);
} FractalWay;

/// One zoom of @p kind through @p way, from the bring-up to its times: every case after one that
/// did not happen skipped as depending on it, saying why.
void FractalZoom(const FractalWay *way, FractalKind kind, const DemoRun *run);

#ifdef __cplusplus
}
#endif
