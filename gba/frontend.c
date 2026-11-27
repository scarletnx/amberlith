// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The GBA's frontend (docs/Frontends.md, docs/GBA.md): no files for the runner's journal and crash
// list, so the run is the report alone, with its mode compiled in: the judge's selfcheck, the libc
// family's references, then the libc suite, each line on mGBA's debug registers and in the
// cartridge's SRAM (platform_gba.c). The screen names each step as it starts; the program stays on
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

// The words of the mode file it has none of: a quick run of the libc family
static const char GBA_MODE[] = "quick libc";

typedef struct {
    const char *name;
    void (*run)(void);
} GbaStep;

// The judge's selfcheck first, then the references of the suite the GBA runs, then that suite. The
// judge's support step and the other references keep more than its 256 KiB of work RAM, and the
// system and socket suites ask for threads, a clock and a network it has not.
static const GbaStep GBA_STEPS[] = {
    { "selfcheck", RunSelfcheck },
    { "reference", RunReferenceLibc },
    { "libc", RunLibc },
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
    ModeDescribe(described, sizeof(described), &mode, (uint32_t)MODE_FAMILY_LIBC);
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
