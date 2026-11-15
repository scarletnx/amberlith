// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "demo.h"
#include "mode.h"

static uint32_t script_frames = MODE_FRAMES_DEFAULT;
static bool script_timing = true;

void DemoSetFrames(uint32_t frames) {
    script_frames = frames >= 1u && frames <= MODE_FRAMES_MAX ? frames : MODE_FRAMES_DEFAULT;
}

void DemoSetTiming(bool on) {
    script_timing = on;
}

DemoRun DemoScriptRun(void) {
    const DemoRun run = { false, script_frames, script_timing, NULL, NULL, NULL };
    return run;
}

void DemoTimesOf(DemoTimes *out, uint32_t frames, const uint32_t *present_us,
                 const uint32_t *work_us, uint32_t *scratch) {
    memset(out, 0, sizeof(*out));
    out->frames = frames;
    if (frames == 0u) {
        return;
    }
    out->warmup_present = present_us[0];
    out->warmup_work = work_us[0];
    GameFrameStatsOf(present_us, GAME_WARMUP_FRAMES, frames, scratch, &out->present);
    GameFrameStatsOf(work_us, GAME_WARMUP_FRAMES, frames, scratch, &out->work);
}

uint32_t DemoTimeCases(const DemoTimes *times, uint32_t frames, DemoTimeCase out[DEMO_TIME_CASES]) {
    static const char *const MEASURES[DEMO_TIME_CASES] = {
        "present to present: the mean",
        "present to present: the 99th percentile",
        "present to present: the longest",
        "the frame's work, the graphics unit's in it: the mean",
        "the frame's work, the graphics unit's in it: the 99th percentile",
    };
    for (uint32_t i = 0; i < DEMO_TIME_CASES; ++i) {
        snprintf(out[i].name, sizeof(out[i].name), "%u frames, %s", (unsigned)frames, MEASURES[i]);
        out[i].us = 0;
    }
    if (times == NULL) {
        return 0;
    }
    const uint32_t us[DEMO_TIME_CASES] = {
        times->present.mean, times->present.p99, times->present.longest,
        times->work.mean,    times->work.p99,
    };
    for (uint32_t i = 0; i < DEMO_TIME_CASES; ++i) {
        out[i].us = us[i];
    }
    return times->present.frames;
}

/// Appends to @p out, never past @p out_size.
static void Append(char *out, size_t out_size, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

static void Append(char *out, size_t out_size, const char *fmt, ...) {
    const size_t used = strlen(out);
    if (used + 1u >= out_size) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    (void)vsnprintf(out + used, out_size - used, fmt, args);
    va_end(args);
}

static void AppendMeasure(char *out, size_t out_size, const char *what,
                          const GameFrameStats *stats) {
    Append(out, out_size, "; %s: mean %u us, 99th percentile %u us, longest %u us at frame %u",
           what, (unsigned)stats->mean, (unsigned)stats->p99, (unsigned)stats->longest,
           (unsigned)stats->longest_at);
}

size_t DemoDescribeTimes(char *out, size_t out_size, const char *suite, const DemoTimes *times) {
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';
    if (times == NULL) {
        return 0;
    }
    const GameFrameStats *const present = &times->present;
    const uint64_t total = present->total + times->warmup_present;
    Append(out, out_size,
           "%s: %u frame%s in %.2f s; frame 0, the warm-up, left out of the times (%u us present "
           "to present, %u us the frame's work)",
           suite, (unsigned)times->frames, times->frames == 1u ? "" : "s", (double)total / 1e6,
           (unsigned)times->warmup_present, (unsigned)times->warmup_work);
    if (present->frames == 0u) {
        Append(out, out_size, "; no frame past it to time");
        return strlen(out);
    }
    Append(out, out_size, "; the %u past it", (unsigned)present->frames);
    AppendMeasure(out, out_size, "the frame's work, the graphics unit's in it", &times->work);
    AppendMeasure(out, out_size, "present to present", present);
    if (present->hitches == 0u) {
        Append(out, out_size, ", none over %u us, two frames at 60 Hz", (unsigned)GAME_HITCH_US);
        return strlen(out);
    }
    Append(out, out_size, ", %u over %u us, two frames at 60 Hz: frame%s",
           (unsigned)present->hitches, (unsigned)GAME_HITCH_US, present->hitches == 1u ? "" : "s");
    const uint32_t listed =
        present->hitches < GAME_HITCHES_LISTED ? present->hitches : GAME_HITCHES_LISTED;
    for (uint32_t i = 0; i < listed; ++i) {
        Append(out, out_size, "%s %u", i == 0u ? "" : ",", (unsigned)present->hitch_at[i]);
    }
    if (present->hitches > listed) {
        Append(out, out_size, " and %u more", (unsigned)(present->hitches - listed));
    }
    return strlen(out);
}
