// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The mod's frontend (docs/Frontends.md, docs/Mod.md): the engine as one more module of a game,
// run before the game's own main. Its NVN is the game's driver, looked up by name as the game
// looks it up, so on a console the cases are answered by the console's own NVN, and under
// ScarletNX by ScarletNX's at the game's API version. No screen and no pad: the run leaves when
// its report is closed, and the process ends with it.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nvnlayer.h>

#include "check.h"
#include "game.h"
#include "hooks.h"
#include "host.h"
#include "mode.h"
#include "platform.h"
#include "report.h"
#include "runner.h"
#include "tests/tests.h"
#include "version.h"

static const char *const SUITE = "mod";

/// What the driver keeps for itself (nv::InitializeGraphics): a block of the module's zeroed
/// data, which the kernel shares with the graphics services as it does a game's heap.
#define GRAPHICS_BYTES 0x2000000u
static char graphics_memory[GRAPHICS_BYTES] __attribute__((aligned(0x1000)));

static const ModStart *started;
/// The driver's API version, asked with no device; -1 until it answered.
static int api_major = -1;
static int api_minor = -1;

/// Whether the driver's objects are the ones this build was made with (nvn/nvn.h's sizes).
static bool ApiIsThisBuilds(void) {
    return api_major >= NVN_LAYER_GAME_API_MAJOR_OLDEST && api_major <= NVN_LAYER_API_MAJOR;
}

// ---- the driver's allocator: the C library's heap, which is the module's own block

static void *Allocate(size_t size, size_t alignment, void *user) {
    (void)user;
    const size_t boundary = alignment < sizeof(void *) ? sizeof(void *) : alignment;
    return aligned_alloc(boundary, (size + boundary - 1u) / boundary * boundary);
}

static void Free(void *address, void *user) {
    (void)user;
    free(address);
}

static void *Reallocate(void *address, size_t size, void *user) {
    (void)user;
    return realloc(address, size);
}

// ---- the mod's own cases: what it needed of the game before the first NVN object

static void CardCase(void) {
    static const char *const NAME = "the memory card is mounted through the game's SDK";
    if (!ReportStart(SUITE, NAME)) {
        return;
    }
    // The report is on the card, so this case only ever passes; its line says which mount.
    CHECK_TRUE(SUITE, NAME, CHECK_NONE, started->mounted_by != NULL,
               "nn::fs::MountSdCardForDebug returned 0x%x and nn::fs::MountSdCard 0x%x",
               (unsigned)started->for_debug_rc, (unsigned)started->plain_rc);
}

static void VersionCase(void) {
    static const char *const NAME =
        "the game's driver answers its API version with no device, and it is one of this build's";
    if (!ReportStart(SUITE, NAME)) {
        return;
    }
    NvnGenericProc (*const volatile bootstrap)(const char *) = nvnBootstrapLoader;
    const NvnDeviceGetProcAddressProc volatile get =
        (NvnDeviceGetProcAddressProc)bootstrap("nvnDeviceGetProcAddress");
    const NvnDeviceGetIntegerProc volatile integer =
        get != NULL ? (NvnDeviceGetIntegerProc)get(NULL, "nvnDeviceGetInteger") : NULL;
    if (integer != NULL) {
        integer(NULL, NVN_DEVICE_INFO_API_MAJOR_VERSION, &api_major);
        integer(NULL, NVN_DEVICE_INFO_API_MINOR_VERSION, &api_minor);
    }
    CHECK_TRUE(SUITE, NAME, CHECK_EXACT, ApiIsThisBuilds(),
               "%s: API %d.%d, where this build has the objects of API %d to %d, so the nvn "
               "family is not run",
               get == NULL       ? "the driver's nvnBootstrapLoader gave no nvnDeviceGetProcAddress"
               : integer == NULL ? "the driver's lookup gave no nvnDeviceGetInteger"
                                 : "nvnDeviceGetInteger(NULL)",
               api_major, api_minor, NVN_LAYER_GAME_API_MAJOR_OLDEST, NVN_LAYER_API_MAJOR);
}

/// Says on the machine's log which of the game's functions is called next: one of them that
/// ends the process, or never returns, is then the last line a debugger or a runtime's log has.
static void Calling(const char *function) {
    char said[96];
    const int length = snprintf(said, sizeof(said), "edenprobe mod: calling %s", function);
    PlatformLog(said, (size_t)length);
}

// No nv::SetGraphicsDevtoolsAllocator: it is the debug layer's allocator, the suite's device asks
// for no debug layer, and the one SDK it was called in here (12.3.9, under ScarletNX) aborted
// inside it at this point of the start.
static void GraphicsCase(void) {
    static const char *const NAME =
        "the driver is given its allocator and its memory before the first device";
    if (!ReportStart(SUITE, NAME)) {
        return;
    }
    __typeof__(GameOeInitialize) *const volatile oe = GameOeInitialize;
    __typeof__(GameSetGraphicsAllocator) *const volatile allocator = GameSetGraphicsAllocator;
    __typeof__(GameInitializeGraphics) *const volatile initialize = GameInitializeGraphics;
    if (oe != NULL) {
        Calling("nn::oe::Initialize");
        oe();
    }
    if (allocator != NULL) {
        Calling("nv::SetGraphicsAllocator");
        allocator(Allocate, Free, Reallocate, NULL);
    }
    if (initialize != NULL) {
        Calling("nv::InitializeGraphics");
        initialize(graphics_memory, sizeof(graphics_memory));
    }
    CHECK_TRUE(SUITE, NAME, CHECK_NONE, allocator != NULL && initialize != NULL,
               "the game's SDK has no %s%s%s, which a game calls before nvnDeviceInitialize",
               allocator == NULL ? "nv::SetGraphicsAllocator" : "",
               allocator == NULL && initialize == NULL ? " and no " : "",
               initialize == NULL ? "nv::InitializeGraphics" : "");
    if (oe == NULL) {
        ReportNote("mod: the game's SDK has no nn::oe::Initialize, and the mod went on without");
    }
}

/// A function the suite called that the game's driver has not: said in the report before the
/// layer ends the program, so the launch's last line names it.
static void LackedCalled(const char *name) {
    ReportNote("nvnlayer: %s was called, and the game's driver has no function of that name: "
               "the launch ends here, and the next goes on after this case",
               name);
}

/// The names this build looks up that the game's driver has no function for: a driver has the
/// names of its own API version.
static void NoteLacked(void) {
    static NvnProcs procs;
    char missing[64] = "";
    if (!NvnLoadProcs(&procs, missing, sizeof(missing))) {
        return;    // the nvn suite's first case says which name
    }
    const size_t count = NvnLayerLackedCount();
    if (count == 0u) {
        ReportNote("host: the game's driver has a function for every name this build looks up");
        return;
    }
    char names[REPORT_NOTE_MAX - 256];
    size_t length = 0;
    size_t named = 0;
    for (; named < count; ++named) {
        const char *const name = NvnLayerLackedName(named);
        if (length + strlen(name) + 2u >= sizeof(names)) {
            break;
        }
        length += (size_t)snprintf(names + length, sizeof(names) - length, "%s%s",
                                   named != 0u ? ", " : "", name);
    }
    ReportNote("host: the game's driver has no function for %zu of the names this build looks "
               "up, and a case that calls one ends its launch there: %s%s",
               count, names, named < count ? ", and more" : "");
}

/// The mod's own suite, then the nvn family when the driver is of the API this build has the
/// objects of: a device of another API would be made in storage of another size.
static void RunInGame(void) {
    CardCase();
    VersionCase();
    if (!ApiIsThisBuilds()) {
        return;
    }
    GraphicsCase();
    NvnLayerSetFatal(LackedCalled);
    NoteLacked();
    RunNvnInGame();
}

/// The driver a report was made on: its API version, and the SDK and the build of the module
/// that holds it. Nothing of the game's own.
static void NoteHost(void) {
    char sdk[HOST_SDK_VERSION_MAX + 1u] = "";
    char build[HOST_BUILD_ID_TEXT] = "";
    const uint8_t *data = NULL;
    size_t size = 0;
    NvnGenericProc (*const volatile bootstrap)(const char *) = nvnBootstrapLoader;
    const bool module = HostModuleData((uintptr_t)bootstrap, &data, &size);
    const bool has_sdk = module && HostSdkVersion(data, size, sdk);
    const bool has_build = module && HostBuildId(data, size, build);
    ReportNote("host: the NVN driver is the game's own, in a module of SDK %s and build id %s%s",
               has_sdk ? sdk : "unknown", has_build ? build : "unknown",
               !module       ? " (the kernel gave no read-only data after the code of "
                               "nvnBootstrapLoader)"
               : !has_sdk    ? " (that module's read-only data has no SDK label)"
               : !has_build  ? " (that module's read-only data has no build id note)"
                             : "");
}

static void RunSupportSteps(void) {
    RunSupport();
    RunSupportPerf();
    RunSupportSettle();
}

static void StepDone(const char *name) {
    if (strcmp(name, "nvn") == 0 && api_major >= 0) {
        ReportNote("host: the game's NVN driver answers API %d.%d", api_major, api_minor);
    }
}

// The judge first and always, as on every frontend, less its reference step: that one holds the
// drawing suites' references, which no case here is judged by, and under a recompiler that made
// the game unoptimized it alone took over ten minutes. Then the nvn family, which runs when the
// file names no family: the mod's own cases and the nvn suite's looked up way.
static const RunnerStep STEPS[] = {
    { "selfcheck", RunSelfcheck, 0, false, false },
    { "support", RunSupportSteps, 0, false, false },
    { "nvn", RunInGame, MODE_FAMILY_NVN, false, true },
};

static const RunnerFrontend FRONTEND = {
    .root = PROBE_DIRECTORY,
    .version = EDENPROBE_VERSION,
    .build = EDENPROBE_BUILD,
    .hash = EDENPROBE_HASH,
    .steps = STEPS,
    .step_count = sizeof(STEPS) / sizeof(STEPS[0]),
    .present = MODE_FAMILY_NVN,
    .asked = 0u,
    .step_done = StepDone,
};

static Runner runner;

int ModRun(const ModStart *start) {
    started = start;
    (void)RunnerInit(&runner, &FRONTEND);

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
    if (runner.plan.start == JOURNAL_FRESH) {
        if (mode.detailed) {
            ReportNote("detailed: the mod has no screens, so the run leaves once its report is "
                       "closed, as a quick one does");
        }
        ReportNote("frontend: a module of a game, run before the game's main; the card by %s",
                   start->mounted_by);
        NoteHost();
    }
    const bool timing = !mode.no_timing;
    RunnerSteps(&runner, families, timing, NULL);
    RunnerFinish(&runner, &mode, timing);
    return ReportFailed() == 0 ? 0 : 1;
}
