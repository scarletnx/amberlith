// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The 3DS's frontend (docs/Frontends.md, docs/3DS.md): the engine as the console's and Android's
// run it, from sdmc:/amberlith, with the 3DS's steps: the judge, then the libc family whole, the C
// library (newlib, as the consoles'), libctru's threads and clock, and soc:U on loopback. No screens
// to choose from: the top screen says which step runs, and a run leaves when its report is closed,
// whatever the mode file says.

// setenv, which ISO C leaves out
#define _POSIX_C_SOURCE 200809L

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "mode.h"
#include "platform.h"
#include "report.h"
#include "runner.h"
#include "tests/tests.h"

static void RunSupportSteps(void) {
    RunSupport();
    RunSupportPerf();
    RunSupportSettle();
}

// The judge first and always, as on every frontend; then the C library's cases.
static const RunnerStep STEPS[] = {
    { "selfcheck", RunSelfcheck, 0, false, false },
    { "support", RunSupportSteps, 0, false, false },
    { "reference", RunReference, 0, false, false },
    { "libc", RunLibc, MODE_FAMILY_LIBC, false, true },
    { "system", RunSystem, MODE_FAMILY_LIBC, false, true },
    { "socket", RunSocket, MODE_FAMILY_LIBC, false, true },
};

static struct {
    RunnerFrontend frontend;
    Runner runner;
} g_frontend_3ds = {
    .frontend = {
        .root = "sdmc:/amberlith",
        .version = AMBERLITH_VERSION,
        .build = AMBERLITH_BUILD,
        .hash = AMBERLITH_HASH,
        .steps = STEPS,
        .step_count = sizeof(STEPS) / sizeof(STEPS[0]),
        .present = (uint32_t)MODE_FAMILY_LIBC,
        .asked = 0u,
    },
};

// The main thread's stack, which libctru's start takes from this name: the judge's support cases
// keep large records on it, past the 32 KiB a 3DSX gets, as the other frontends' 1 MiB and more
// hold them.
u32 __stacksize__ = 4u * 1024u * 1024u;

static void Progress(uint32_t number, const char *name, bool timed) {
    printf("%2lu %s\n", (unsigned long)number, name);
}

int main(void) {
    gfxInitDefault();
    consoleInit(GFX_TOP, nullptr);
    printf("Amberlith %s (%s)\n", AMBERLITH_VERSION, AMBERLITH_HASH);
    Runner *const runner = &g_frontend_3ds.runner;
    // The root and its scratch folder: the C library's place for temporary files, which the
    // support suite writes its checks in, since the 3DS has no /tmp
    mkdir(g_frontend_3ds.frontend.root, 0777);
    mkdir("sdmc:/amberlith/tmp", 0777);
    setenv("TMPDIR", "sdmc:/amberlith/tmp", 1);
    if (!RunnerInit(runner, &g_frontend_3ds.frontend)) {
        static const char said[] = "amberlith: the runner refused sdmc:/amberlith";
        PlatformLog(said, sizeof(said) - 1u);
        gfxExit();
        return 1;
    }
    const RunMode from_file = RunnerModeFromFile(runner->mode_path);
    RunnerPlan(runner, &from_file);
    // No screen to ask whether to go on with a run left in the middle: it goes on, as a quick run
    // does on the console.
    RunMode mode = RunnerModeAtStart(runner, &from_file);
    RunnerOpen(runner, &mode);
    if (runner->plan.start == JOURNAL_FRESH) {
        RunnerBeginJournal(runner, &from_file, &mode);
    }
    const uint32_t families = RunnerFamilies(runner, &mode);
    RunnerApplyMode(runner, &mode);
    if (runner->plan.start == JOURNAL_FRESH && mode.detailed) {
        ReportNote("detailed: the 3DS's frontend has no screens, so the run leaves once its "
                   "report is closed, as a quick one does");
    }
    const bool timing = !mode.no_timing;
    RunnerSteps(runner, families, timing, Progress);
    RunnerFinish(runner, &mode, timing);
    const int failed = (int)ReportFailed();
    printf("%d failed; the report is in %s\n", failed, runner->report_path);
    gfxExit();
    return failed;
}
