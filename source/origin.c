// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Where each suite's expected answers come from, for the lines that do not say it themselves: a
// case whose answer has another source says that one (ReportCaseFrom, CHECK_TRUE_FROM), and the
// strongest source wins. Every suite of a family that judges a machine has a row, which
// tools/case-counts.py checks against ui.c's families; the judge's suites have none, since they
// hold this program's own code to its references. The family pages say which program a
// reference or an emulator is, and at which commit.

#include <string.h>

#include "report.h"

static const struct {
    const char *suite;
    ReportFrom from;
} ORIGINS[] = {
    // The processor: the Arm ARM's pseudocode in integer arithmetic, and the lanes, flags and
    // blocks Unicorn's AArch64 agreed with; the cases it did not check say spec.
    { "cpu", REPORT_FROM_REFERENCE },
    // A person presses what the screen asks, and the case expects that press (source/padref.c).
    { "pad", REPORT_FROM_SPEC },
    { "touch", REPORT_FROM_SPEC },
    // The graphics unit against this program's references in source/*ref.c, worked out from the
    // specifications the family page names.
    { "clear", REPORT_FROM_SPEC },
    { "format", REPORT_FROM_SPEC },
    { "raster", REPORT_FROM_SPEC },
    { "intmath", REPORT_FROM_SPEC },
    { "blend", REPORT_FROM_SPEC },
    { "depth", REPORT_FROM_SPEC },
    { "fixed", REPORT_FROM_SPEC },
    { "stencil", REPORT_FROM_SPEC },
    { "lod", REPORT_FROM_SPEC },
    { "sampling", REPORT_FROM_SPEC },
    { "coverage", REPORT_FROM_SPEC },
    { "msaa", REPORT_FROM_SPEC },
    { "texture", REPORT_FROM_SPEC },
    { "coherence", REPORT_FROM_SPEC },
    { "fmath", REPORT_FROM_SPEC },
    { "order", REPORT_FROM_SPEC },
    { "queue", REPORT_FROM_SPEC },
    { "storage", REPORT_FROM_SPEC },
    { "index", REPORT_FROM_SPEC },
    { "topology", REPORT_FROM_SPEC },
    { "vertex", REPORT_FROM_SPEC },
    { "instance", REPORT_FROM_SPEC },
    { "bindless", REPORT_FROM_SPEC },
    // Each block says astcenc or spec (AstcBlockExpectedFrom, AstcImageExpectedFrom).
    { "astc", REPORT_FROM_SPEC },
    { "astcimage", REPORT_FROM_SPEC },
    // Decoders the reference suite holds to Mesa's (ETC2, EAC), to Pillow's and bcdec's and to
    // what an NVIDIA card decodes (BCn); the bcn suite's arrays, which judge where the blocks lie,
    // say spec.
    { "eac", REPORT_FROM_REFERENCE },
    { "etc2", REPORT_FROM_REFERENCE },
    { "bcn", REPORT_FROM_REFERENCE },
    { "bcnimage", REPORT_FROM_REFERENCE },
    { "zeta", REPORT_FROM_SPEC },
    { "swizzle", REPORT_FROM_SPEC },
    { "texel", REPORT_FROM_SPEC },
    { "stream", REPORT_FROM_SPEC },
    { "fill", REPORT_FROM_SPEC },
    { "ring", REPORT_FROM_SPEC },
    { "occlusion", REPORT_FROM_SPEC },
    { "drawtex", REPORT_FROM_SPEC },
    { "geometry", REPORT_FROM_SPEC },
    { "xfb", REPORT_FROM_SPEC },
    { "query", REPORT_FROM_SPEC },
    { "dma", REPORT_FROM_SPEC },
    { "blit2d", REPORT_FROM_SPEC },
    { "copy", REPORT_FROM_SPEC },
    // Where the texels lie is spec; a compressed texture's fetched texels say their decoder's
    // source (End in t_tex8k.c).
    { "tex8k", REPORT_FROM_SPEC },
    { "fs", REPORT_FROM_SPEC },
    // The rules the scheduler cases are judged by.
    { "sched", REPORT_FROM_SPEC },
    // The console kernel's results as Atmosphere's mesosphere returns them, result for result.
    { "svc", REPORT_FROM_REFERENCE },
    // Timed, judging no speed yet: what they check is their own work's checksum.
    { "pace-cpu", REPORT_FROM_SPEC },
    { "pace-mem", REPORT_FROM_SPEC },
    { "pace-thread", REPORT_FROM_SPEC },
    { "pace-timer", REPORT_FROM_SPEC },
    { "pace-fs", REPORT_FROM_SPEC },
    { "pace-gpu", REPORT_FROM_SPEC },
    // Both reply layouts the judge takes are reden's.
    { "binder", REPORT_FROM_EMULATOR },
    // vicref.c's model under each answer only a console settles, on libdrm's structure.
    { "vic", REPORT_FROM_SPEC },
    // libvpx's MD5 list for each of its test vectors.
    { "nvdec", REPORT_FROM_REFERENCE },
    // The mixer's arithmetic; the resampler's table cases say emulator (reden's tables).
    { "audio", REPORT_FROM_SPEC },
    // Every service's expectation is reden's, with the commit that set it (Service.md); sdk
    // judges this program's own exports.
    { "hwopus", REPORT_FROM_EMULATOR },
    { "am", REPORT_FROM_EMULATOR },
    { "fsp", REPORT_FROM_EMULATOR },
    { "account", REPORT_FROM_EMULATOR },
    { "ldn", REPORT_FROM_EMULATOR },
    { "set", REPORT_FROM_EMULATOR },
    { "csrng", REPORT_FROM_EMULATOR },
    { "apm", REPORT_FROM_EMULATOR },
    { "nfp", REPORT_FROM_EMULATOR },
    { "aoc", REPORT_FROM_EMULATOR },
    { "time", REPORT_FROM_EMULATOR },
    { "pl", REPORT_FROM_EMULATOR },
    { "profile", REPORT_FROM_EMULATOR },
    { "am-state", REPORT_FROM_EMULATOR },
    { "am-self", REPORT_FROM_EMULATOR },
    { "am-functions", REPORT_FROM_EMULATOR },
    { "auddev", REPORT_FROM_EMULATOR },
    { "fsdir", REPORT_FROM_EMULATOR },
    { "am-audio", REPORT_FROM_EMULATOR },
    { "am-channel", REPORT_FROM_EMULATOR },
    { "am-storage", REPORT_FROM_EMULATOR },
    { "mii", REPORT_FROM_EMULATOR },
    { "am-mode", REPORT_FROM_EMULATOR },
    { "bsd-local", REPORT_FROM_EMULATOR },
    { "ipc", REPORT_FROM_EMULATOR },
    { "hid", REPORT_FROM_EMULATOR },
    { "time-snap", REPORT_FROM_EMULATOR },
    { "vi", REPORT_FROM_EMULATOR },
    { "renderer", REPORT_FROM_EMULATOR },
    { "ngc", REPORT_FROM_EMULATOR },
    { "am-lock", REPORT_FROM_EMULATOR },
    { "set-sys", REPORT_FROM_EMULATOR },
    { "time-zone", REPORT_FROM_EMULATOR },
    { "time-perm", REPORT_FROM_EMULATOR },
    { "audout", REPORT_FROM_EMULATOR },
    { "audin", REPORT_FROM_EMULATOR },
    { "pctl", REPORT_FROM_EMULATOR },
    { "fs-file", REPORT_FROM_EMULATOR },
    { "acc-app", REPORT_FROM_EMULATOR },
    { "sdk", REPORT_FROM_SPEC },
    // libnx's and switchbrew's reading of each service, and what the backend the net file names
    // publishes.
    { "nifm", REPORT_FROM_SPEC },
    { "dns", REPORT_FROM_SPEC },
    { "bsd", REPORT_FROM_SPEC },
    { "ssl", REPORT_FROM_SPEC },
    { "acc", REPORT_FROM_SPEC },
    { "friend", REPORT_FROM_SPEC },
    // What the C standard and POSIX define; libm's table is worked out at 100 digits.
    { "libc", REPORT_FROM_SPEC },
    { "system", REPORT_FROM_SPEC },
    { "socket", REPORT_FROM_SPEC },
    // The frames and samples this program works out on the processor.
    { "game", REPORT_FROM_SPEC },
    { "wimsy", REPORT_FROM_SPEC },
    { "mandelbrot", REPORT_FROM_SPEC },
    { "julia", REPORT_FROM_SPEC },
    { "fluid", REPORT_FROM_SPEC },
    { "fluid-alias", REPORT_FROM_SPEC },
    { "music", REPORT_FROM_SPEC },
    // What NVN defines; what only the driver decides says driver (t_nvn.c).
    { "nvn", REPORT_FROM_SPEC },
    { "nvn2", REPORT_FROM_SPEC },
    // The mod's own: the card through the game's SDK, the driver's API version, its memory.
    { "mod", REPORT_FROM_SPEC },
    // Android's: what the standard and the platform's documentation define.
    { "jni", REPORT_FROM_SPEC },
    { "framework", REPORT_FROM_SPEC },
    { "bionic", REPORT_FROM_SPEC },
};

ReportFrom ReportSuiteFrom(const char *suite) {
    for (size_t i = 0; suite != NULL && i < sizeof(ORIGINS) / sizeof(ORIGINS[0]); ++i) {
        if (strcmp(ORIGINS[i].suite, suite) == 0) {
            return ORIGINS[i].from;
        }
    }
    return REPORT_FROM_UNSAID;
}
