// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The DS's frontend (docs/Frontends.md, docs/DS.md): the engine as the other frontends run it, from
// the card's /amberlith, with the DS's steps: the judge, then the libc family's suites its ARM9 can
// run, the C library (newlib, as the consoles') and calico's threads and clock. No screens to choose
// from: the top screen says which step runs, and a run leaves when its report is closed, whatever
// the mode file says. Without a card the C library can write, the report is on the log alone.

// setenv, which ISO C leaves out
#define _POSIX_C_SOURCE 200809L

#include <fat.h>
#include <nds.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/iosupport.h>
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

// The references of the suites the DS runs, the libc family's: the others judge suites it has not,
// and their tables alone hold more than its 4 MiB.
static void RunReferenceSteps(void) {
    RunReferenceLibc();
}

// The judge first and always, as on every frontend; then the C library's cases.
static const RunnerStep STEPS[] = {
    { "selfcheck", RunSelfcheck, 0, false, false },
    { "support", RunSupportSteps, 0, false, false },
    { "reference", RunReferenceSteps, 0, false, false },
    { "libc", RunLibc, MODE_FAMILY_LIBC, false, true },
    { "system", RunSystem, MODE_FAMILY_LIBC, false, true },
};

static struct {
    RunnerFrontend frontend;
    Runner runner;
    // What the run's thread hands main to exit with: the failed cases, or 1 when it could not start
    int failed;
} g_frontend_nds = {
    .frontend = {
        .root = "/amberlith",
        .version = AMBERLITH_VERSION,
        .build = AMBERLITH_BUILD,
        .hash = AMBERLITH_HASH,
        .steps = STEPS,
        .step_count = sizeof(STEPS) / sizeof(STEPS[0]),
        .present = (uint32_t)MODE_FAMILY_LIBC,
        .asked = 0u,
    },
};

// The default device when no card mounts: no operation, so every file call fails with an error.
// With no device at all a path names none, and devkitARM's mkdir indexes its device table with that
// -1 (its open checks for it), which ended the program on its first mkdir.
static const devoptab_t NO_CARD = { .name = "nocard", .structSize = 0 };

static void Progress(uint32_t number, const char *name, bool timed) {
    printf("%2lu %s\n", (unsigned long)number, name);
}

// The run, on a thread of its own: the main thread's stack is the 16 KiB of data TCM, and the judge's
// journal checks keep up to 296 KiB in one frame (t_journal.c's CheckReading).
#define NDS_ENGINE_STACK (512u * 1024u)

static void *Run(void *unused) {
    Runner *const runner = &g_frontend_nds.runner;
    if (!RunnerInit(runner, &g_frontend_nds.frontend)) {
        static const char said[] = "amberlith: the runner refused /amberlith";
        PlatformLog(said, sizeof(said) - 1u);
        g_frontend_nds.failed = 1;
        return nullptr;
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
        ReportNote("detailed: the DS's frontend has no screens, so the run leaves once its "
                   "report is closed, as a quick one does");
    }
    const bool timing = !mode.no_timing;
    RunnerSteps(runner, families, timing, Progress);
    RunnerFinish(runner, &mode, timing);
    g_frontend_nds.failed = (int)ReportFailed();
    printf("%d failed; the report is in %s\n", g_frontend_nds.failed, runner->report_path);
    return nullptr;
}

int main(void) {
    consoleDemoInit();
    printf("Amberlith %s (%s)\n", AMBERLITH_VERSION, AMBERLITH_HASH);
    // The card's FAT through its DLDI driver, or a DSi's SD card, as the C library's default
    // device: the root below is on it
    if (!fatInitDefault()) {
        static const char said[] =
            "amberlith: no card the C library can write; the report is on the log alone";
        PlatformLog(said, sizeof(said) - 1u);
        printf("no card: the report is on the log alone\n");
        const int device = AddDevice(&NO_CARD);
        if (device < 0) {
            static const char full[] = "amberlith: the device table took no device for the run "
                                       "with no card; a file call may end it";
            PlatformLog(full, sizeof(full) - 1u);
        } else {
            setDefaultDevice(device);
        }
    }
    // The root and its scratch folder: the C library's place for temporary files, which the
    // support suite writes its checks in, since the DS has no /tmp
    mkdir(g_frontend_nds.frontend.root, 0777);
    mkdir("/amberlith/tmp", 0777);
    setenv("TMPDIR", "/amberlith/tmp", 1);
    pthread_attr_t attributes;
    pthread_t thread;
    int made = pthread_attr_init(&attributes);
    if (made == 0) {
        made = pthread_attr_setstacksize(&attributes, NDS_ENGINE_STACK);
    }
    if (made == 0) {
        made = pthread_create(&thread, &attributes, Run, nullptr);
    }
    if (made != 0) {
        char said[96];
        const int length = snprintf(said, sizeof(said),
                                    "amberlith: no thread with a %u-byte stack for the run (%d)",
                                    NDS_ENGINE_STACK, made);
        PlatformLog(said, (size_t)length);
        return 1;
    }
    pthread_join(thread, nullptr);
    return g_frontend_nds.failed;
}
