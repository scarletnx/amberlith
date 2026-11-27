// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The GBA's frontend (docs/Frontends.md, docs/GBA.md): no files for the runner's journal and crash
// list, so the run is the report alone, with its mode compiled in: the judge's selfcheck, the
// references of the suites it runs, then the libc suite and the GBA's video suite (video.c), each
// line on mGBA's debug registers and in the cartridge's SRAM (platform_gba.c). The screen names each step as it starts; the program stays on
// the last screen once the report is closed, as a GBA program has nowhere to return to.

#include <gba_console.h>
#include <gba_interrupt.h>
#include <gba_systemcalls.h>
#include <stdio.h>
#include <string.h>

#include "mode.h"
#include "platform.h"
#include "report.h"
#include "tests/tests.h"
#include "video.h"

// The words of the mode file it has none of: a quick run of the libc and graphics families
static const char GBA_MODE[] = "quick libc graphics";

typedef struct {
    const char *name;
    void (*run)(void);
} GbaStep;

// The references of the suites the GBA runs, the libc family's and its video's: the others keep
// more than its 256 KiB of work RAM.
static void RunReferenceSteps(void) {
    RunReferenceLibc();
    RunReferenceGbaVideo();
}

// The judge's selfcheck first, then those references, then the suites. The judge's support step
// keeps more than the GBA's work RAM too, and the system and socket suites ask for threads, a clock
// and a network it has not.
static const GbaStep GBA_STEPS[] = {
    { "selfcheck", RunSelfcheck },
    { "reference", RunReferenceSteps },
    { "libc", RunLibc },
    { "gbavideo", RunGbaVideo },
};

int main(void) {
    irqInit();
    irqEnable(IRQ_VBLANK);
    consoleDemoInit();
    printf("Amberlith %s (%s)\n", AMBERLITH_VERSION, AMBERLITH_HASH);
    // The GBA's C library has no file system: the report says so in a note and goes on, every
    // line on the log
    ReportOpen("/amberlith/report.jsonl");
    ReportNote("Amberlith %s (%s) starting", AMBERLITH_VERSION, AMBERLITH_BUILD);
    const RunMode mode = ModeFromText(GBA_MODE);
    char described[REPORT_NOTE_MAX + 1];
    ModeDescribe(described, sizeof(described), &mode,
                 (uint32_t)(MODE_FAMILY_LIBC | MODE_FAMILY_GRAPHICS));
    ReportNote("%s", described);
    for (size_t i = 0; i < sizeof(GBA_STEPS) / sizeof(GBA_STEPS[0]); ++i) {
        printf("%2u %s\n", (unsigned)(i + 1u), GBA_STEPS[i].name);
        GBA_STEPS[i].run();
    }
    ReportSummary();
    ReportClose();
    char done[160];
    const int length =
        snprintf(done, sizeof(done), "AMBERLITH DONE passed=%zu failed=%zu skipped=%zu crashed=%zu",
                 ReportPassed(), ReportFailed(), ReportSkipped(), ReportCrashed());
    PlatformLog(done, (size_t)length);
    printf("%zu passed, %zu failed\n", ReportPassed(), ReportFailed());
    while (true) {
        VBlankIntrWait();
    }
}
