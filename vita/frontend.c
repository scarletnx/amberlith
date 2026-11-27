// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The Vita's frontend (docs/Frontends.md): the engine as the console's and Android's run it, from
// ux0:data/amberlith, with the Vita's steps: the judge, the libc family's cases its C library can
// run (newlib, as the consoles'), then its own, the gxm suite (vita/gxm.c). No screens: a run
// leaves when its report is closed, whatever the mode file says.

// setenv, which ISO C leaves out
#define _POSIX_C_SOURCE 200809L

#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gxm.h"
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

// The judge first and always, as on every frontend; then the C library's cases; then the GPU's.
static const RunnerStep STEPS[] = {
    { "selfcheck", RunSelfcheck, 0, false, false },
    { "support", RunSupportSteps, 0, false, false },
    { "reference", RunReference, 0, false, false },
    { "libc", RunLibc, MODE_FAMILY_LIBC, false, true },
    { "gxm", RunGxm, MODE_FAMILY_GRAPHICS, false, true },
};

static RunnerFrontend g_vita = {
    .root = "ux0:data/amberlith",
    .version = AMBERLITH_VERSION,
    .build = AMBERLITH_BUILD,
    .hash = AMBERLITH_HASH,
    .steps = STEPS,
    .step_count = sizeof(STEPS) / sizeof(STEPS[0]),
    .present = (uint32_t)(MODE_FAMILY_LIBC | MODE_FAMILY_GRAPHICS),
    .asked = 0u,
};

static Runner g_runner;

// The main thread's stack, which the module's process parameters give the kernel (vita-elf-create
// reads this name): the judge's support cases keep large records on it, past the kernel's default
// of 256 KiB, as the other frontends' 1 MiB and more hold them.
unsigned int sceUserMainThreadStackSize = 4u * 1024u * 1024u;

int main(void) {
    // The root and its parents: the data folder's own may not be there on a new memory card.
    sceIoMkdir("ux0:data", 0777);
    sceIoMkdir(g_vita.root, 0777);
    // The C library's place for temporary files, which the support suite writes its checks in: the
    // Vita has no /tmp
    sceIoMkdir("ux0:data/amberlith/tmp", 0777);
    setenv("TMPDIR", "ux0:data/amberlith/tmp", 1);
    if (!RunnerInit(&g_runner, &g_vita)) {
        static const char said[] = "amberlith: the runner refused ux0:data/amberlith";
        PlatformLog(said, sizeof(said) - 1u);
        sceKernelExitProcess(1);
        return 1;
    }
    const RunMode from_file = RunnerModeFromFile(g_runner.mode_path);
    RunnerPlan(&g_runner, &from_file);
    // No screen to ask whether to go on with a run left in the middle: it goes on, as a quick run
    // does on the console.
    RunMode mode = RunnerModeAtStart(&g_runner, &from_file);
    RunnerOpen(&g_runner, &mode);
    if (g_runner.plan.start == JOURNAL_FRESH) {
        RunnerBeginJournal(&g_runner, &from_file, &mode);
    }
    const uint32_t families = RunnerFamilies(&g_runner, &mode);
    RunnerApplyMode(&g_runner, &mode);
    if (g_runner.plan.start == JOURNAL_FRESH && mode.detailed) {
        ReportNote("detailed: the Vita's frontend has no screens, so the run leaves once its "
                   "report is closed, as a quick one does");
    }
    const bool timing = !mode.no_timing;
    RunnerSteps(&g_runner, families, timing, nullptr);
    RunnerFinish(&g_runner, &mode, timing);
    const int failed = (int)ReportFailed();
    sceKernelExitProcess(failed);
    return failed;
}
