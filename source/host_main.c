// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <string.h>

#include "report.h"
#include "tests/tests.h"

// The entry point for the host build. It runs only the suites that need no console, which are
// the ones that check this program's own judgement and what it writes around a verdict, so a
// change to any of them can be proven wrong in a second with no emulator in the loop. "reference"
// runs the reference suite alone, which the build with the processor reference's 128-bit integers
// in two words runs (host.mk).
int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "reference") == 0) {
        ReportOpen("amberlith-host-reference.jsonl");
        RunReference();
        ReportSummary();
        ReportClose();
        printf("reference: passed=%zu failed=%zu\n", ReportPassed(), ReportFailed());
        return ReportFailed() == 0 && ReportPassed() > 0 ? 0 : 1;
    }
    ReportOpen("amberlith-host-report.jsonl");
    // Kept as a detailed run keeps them, so the support suite can check the store the screens
    // read against the cases this very run reported.
    ReportRetain(true);
    ReportNote("Amberlith host build");

    RunSelfcheck();
    RunSupport();
    RunSupportPerf();
    RunSupportSettle();
    RunReference();
    // The C library's cases, as every frontend runs them: glibc here, so a case that holds a
    // library to more than the standard says fails on the machine every pull request builds on.
    RunLibc();
    RunSystem();
    RunSocket();
    // The music family's tracks as this machine renders them: the hashes the console's run is
    // held to are these.
    RunMusicRender();

    ReportSummary();
    ReportClose();

    printf("passed=%zu failed=%zu skipped=%zu crashed=%zu\n", ReportPassed(), ReportFailed(),
           ReportSkipped(), ReportCrashed());
    return ReportFailed() == 0 ? 0 : 1;
}
