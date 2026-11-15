// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../check.h"
#include "../demo.h"
#include "../report.h"
#include "tests.h"

// The times every demo of the family reports, and how a step runs one, against answers worked
// out by hand.

static const char *const SUITE = "reference";

typedef struct {
    uint32_t wrong;
    const char *first;
} Tally;

static void Expect(Tally *tally, bool condition, const char *what) {
    if (!condition && tally->wrong++ == 0u) {
        tally->first = what;
    }
}

static void Report(const char *name, const Tally *tally) {
    CHECK_TRUE(SUITE, name, CHECK_NONE, tally->wrong == 0u, "%u readings wrong, the first: %s",
               (unsigned)tally->wrong, tally->first != NULL ? tally->first : "none");
}

static uint32_t present[4];
static uint32_t work[4];
static uint32_t scratch[4];

static void CheckDemoTimes(void) {
    Tally t = { 0, NULL };
    // Frame 0 the warm-up; past it 16000, 17000 and 50000 us: a mean of 27667 rounded, the 99th
    // percentile and the longest the last, a hitch at frame 3.
    static const uint32_t PRESENT[4] = { 100000u, 16000u, 17000u, 50000u };
    static const uint32_t WORK[4] = { 90000u, 1000u, 3000u, 2000u };
    memcpy(present, PRESENT, sizeof(present));
    memcpy(work, WORK, sizeof(work));
    DemoTimes times;
    DemoTimesOf(&times, 4u, present, work, scratch);
    DemoTimeCase cases[DEMO_TIME_CASES];
    const uint32_t timed = DemoTimeCases(&times, 120u, cases);
    Expect(&t, timed == 3u && times.warmup_present == 100000u && times.warmup_work == 90000u,
           "three frames past the warm-up");
    Expect(&t,
           cases[0].us == 27667u && cases[1].us == 50000u && cases[2].us == 50000u &&
               cases[3].us == 2000u && cases[4].us == 3000u,
           "means, 99th percentiles and the longest");
    Expect(&t,
           strcmp(cases[0].name, "120 frames, present to present: the mean") == 0 &&
               strcmp(cases[4].name,
                      "120 frames, the frame's work, the graphics unit's in it: the 99th "
                      "percentile") == 0,
           "the names carry the frames asked for");
    char note[512];
    DemoDescribeTimes(note, sizeof(note), "a demo", &times);
    Expect(&t,
           strcmp(note,
                  "a demo: 4 frames in 0.18 s; frame 0, the warm-up, left out of the times "
                  "(100000 us present to present, 90000 us the frame's work); the 3 past it; "
                  "the frame's work, the graphics unit's in it: mean 2000 us, 99th percentile "
                  "3000 us, longest 3000 us at frame 2; present to present: mean 27667 us, 99th "
                  "percentile 50000 us, longest 50000 us at frame 3, 1 over 33333 us, two "
                  "frames at 60 Hz: frame 3") == 0,
           "the note");
    DemoTimesOf(&times, 1u, present, work, scratch);
    Expect(&t, DemoTimeCases(&times, 1u, cases) == 0u, "a run of one frame times none");
    // A step's run takes the mode's frames, a refused count keeps 600, and nobody stops it.
    const DemoRun was = DemoScriptRun();
    DemoSetFrames(120u);
    const DemoRun asked = DemoScriptRun();
    DemoSetFrames(0u);
    const DemoRun refused = DemoScriptRun();
    DemoSetFrames(was.frames);
    Expect(&t,
           asked.frames == 120u && refused.frames == 600u && !asked.interactive &&
               asked.stopped == NULL && asked.paused == NULL,
           "a step's run");
    Report("a demo's times leave the warm-up out and carry the frames asked for", &t);
}

void RunReferenceDemo(void) {
    CheckDemoTimes();
}
