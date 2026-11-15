// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gameref.h"

#ifdef __cplusplus
extern "C" {
#endif

/// The demo family (docs/families/Demo.md): what a demo is asked to do, and the times every demo
/// reports. The types and the times are portable, so the host build checks them; the console's
/// demos are in source/tests.

/// The pad's two sticks as a person holds them, each way from -32767 to 32767: right and up are
/// more. What a demo whose camera a person moves reads once a frame.
typedef struct {
    int32_t left[2];
    int32_t right[2];
} DemoSticks;

/// How a demo is run: by a step of the run, from the mode file's words (DemoScriptRun), or by a
/// person choosing it on the screen. A demo reads nothing else about the run.
typedef struct {
    /// A person is in front of the screen: only then may a demo play anything for them.
    bool interactive;
    /// How many frames to draw, from 1 to MODE_FRAMES_MAX.
    uint32_t frames;
    /// Whether the frames' times are reported.
    bool timing;
    /// True once the person asked the demo to stop; asked once a frame. NULL for a run nobody
    /// stops. The judged frames it did not reach are skipped as asked.
    bool (*stopped)(void);
    /// True while the person holds the demo paused; asked once a frame and again while paused.
    /// Paused, the demo shows its last frame again and draws no new one, and the time it stays
    /// paused is in no frame's time. NULL for a run nobody pauses.
    bool (*paused)(void);
    /// The sticks, for a demo a person steers; asked once a frame. NULL for a run nobody steers.
    void (*sticks)(DemoSticks *out);
} DemoRun;

/// What a step of the run gives a demo: the mode's `frames=` and whether the run times, set
/// before the steps run, never interactive and never stopped.
void DemoSetFrames(uint32_t frames);
void DemoSetTiming(bool on);
DemoRun DemoScriptRun(void);

/// A demo's times, as the game family's scene measures its own: frame 0 is the warm-up and left
/// out, then present to present, from one present queued to the next, and the frame's work, from
/// the image in hand to the queue standing idle after the frame's submit, so the graphics unit's
/// work is in it and no wait for the display is.
typedef struct {
    uint32_t frames;
    uint32_t warmup_present;
    uint32_t warmup_work;
    GameFrameStats present;
    GameFrameStats work;
} DemoTimes;

/// The times of @p frames frames; @p scratch holds @p frames values.
void DemoTimesOf(DemoTimes *out, uint32_t frames, const uint32_t *present_us,
                 const uint32_t *work_us, uint32_t *scratch);

/// The timed cases a demo reports, in order: present to present's mean, 99th percentile and
/// longest, then the frame's work's mean and 99th percentile.
#define DEMO_TIME_CASES 5u
#define DEMO_TIME_NAME_MAX 96u

typedef struct {
    char name[DEMO_TIME_NAME_MAX];
    uint32_t us;
} DemoTimeCase;

/// The timed cases of @p times, names carrying the @p frames the run was asked for, whatever it
/// drew before it was stopped; with @p times NULL, the names alone and every time nought. Returns
/// how many frames past the warm-up the times are over, nought when there is none to time.
uint32_t DemoTimeCases(const DemoTimes *times, uint32_t frames, DemoTimeCase out[DEMO_TIME_CASES]);

/// The note a timed demo ends on, without the braces around it: @p suite's frames, the warm-up
/// left out and what it took, present to present and the frame's work past it, and the frames
/// over two at 60 Hz by number. Returns the length; the text is always terminated, and cut short
/// rather than overrun.
size_t DemoDescribeTimes(char *out, size_t out_size, const char *suite, const DemoTimes *times);

#ifdef __cplusplus
}
#endif
