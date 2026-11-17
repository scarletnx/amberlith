// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The Switch 2's frontend (docs/Frontends.md, docs/Ounce.md): the engine built as a Switch 2
// program, an ExeFS of an NSO and an NPDM whose descriptor is a Switch 2's, which is how a
// machine tells one. No console starts it: ScarletNX does, and what the program asks is what
// ScarletNX gives such a program, its API 231 NVN first. No deko3d (the library drives the
// Switch's graphics unit) and no screens: a run leaves when its report is closed.

#include <stdio.h>
#include <string.h>
#include <switch.h>

#include <nvnlayer.h>

#include "hooks.h"
#include "mode.h"
#include "platform.h"
#include "report.h"
#include "runner.h"
#include "tests/tests.h"
#include "version.h"

/// The Switch 2 frontend's own suite (ounce/t_nvn2.c).
void RunNvn2(void);

// The heap, 256 MiB. libnx's own start takes all the memory the kernel says the program may
// have, which for a Switch 2 program is gigabytes a run of the probe never touches.
#define HEAP_BYTES 0x10000000u

void __libnx_initheap(void);
void __libnx_initheap(void) {
    extern char *fake_heap_start;
    extern char *fake_heap_end;
    void *heap = NULL;
    const Result rc = svcSetHeapSize(&heap, HEAP_BYTES);
    if (R_FAILED(rc)) {
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_HeapAllocFailed));
    }
    fake_heap_start = heap;
    fake_heap_end = (char *)heap + HEAP_BYTES;
}

/// A body of the layer that was reached: ScarletNX did not take that function. Said in the
/// report before the layer ends the program, so the run's last line names it.
static void LayerReached(const char *name) {
    ReportNote("nvnlayer: %s was reached: no NVN implementation took it, and the run ends here",
               name);
}

static void RunSupportSteps(void) {
    RunSupportMode();
    RunSupport();
    RunSupportPerf();
    RunSupportSettle();
}

// The judge first and always, as on every frontend; then the nvn family, which here asks
// ScarletNX's API 231 NVN through the layer's names, and runs when the file names no family.
static const RunnerStep STEPS[] = {
    { "selfcheck", RunSelfcheck, 0, false, false },
    { "support", RunSupportSteps, 0, false, false },
    { "reference", RunReference, 0, false, false },
    { "nvn", RunNvn, MODE_FAMILY_NVN, false, true },
    // After nvn, whose device comes down before this suite makes its own.
    { "nvn2", RunNvn2, MODE_FAMILY_NVN, false, true },
};

static const RunnerFrontend FRONTEND = {
    .root = PROBE_DIRECTORY,
    .version = AMBERLITH_VERSION,
    .build = AMBERLITH_BUILD,
    .hash = AMBERLITH_HASH,
    .steps = STEPS,
    .step_count = sizeof(STEPS) / sizeof(STEPS[0]),
    .present = MODE_FAMILY_NVN,
    .asked = 0u,
    .step_done = HooksReportOwnCode,
};

static Runner runner;

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    (void)RunnerInit(&runner, &FRONTEND);
    NvnLayerSetFatal(LayerReached);

    const RunMode from_file = RunnerModeFromFile(runner.mode_path);
    RunnerPlan(&runner, &from_file);
    // No screen to ask whether to go on with a run left in the middle: it goes on, as a quick
    // run does on the console.
    RunMode mode = RunnerModeAtStart(&runner, &from_file);
    RunnerOpen(&runner, &mode);
    if (runner.plan.start == JOURNAL_FRESH) {
        RunnerBeginJournal(&runner, &from_file, &mode);
    }
    const uint32_t families = RunnerFamilies(&runner, &mode);
    RunnerApplyMode(&runner, &mode);
    HooksReportOwnCode(NULL);
    if (runner.plan.start == JOURNAL_FRESH) {
        if (mode.detailed) {
            ReportNote("detailed: the Switch 2 frontend has no screens, so the run leaves once "
                       "its report is closed, as a quick one does");
        }
        ReportNote("frontend: a Switch 2 program, its NVN the layer's API %d.%d names",
                   NVN_LAYER_API_MAJOR, NVN_LAYER_API_MINOR);
    }
    const bool timing = !mode.no_timing;
    RunnerSteps(&runner, families, timing, NULL);
    RunnerFinish(&runner, &mode, timing);
    return ReportFailed() == 0 ? 0 : 1;
}
