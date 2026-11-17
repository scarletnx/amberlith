// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The mod's frontend (docs/Frontends.md, docs/Mod.md): the engine as one more module of a game,
// run before the game's own main. Its NVN is the game's driver, looked up by name as the game
// looks it up, so on a console the cases are answered by the console's own NVN, and under
// ScarletNX by ScarletNX's at the game's API version. A window of its own on a layer the game's
// nn::vi gives, for the window cases, unless the mode file says nowindow; no screens and no pad:
// the run leaves when its report is closed, and the process ends with it.

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
    const int length = snprintf(said, sizeof(said), "amberlith mod: calling %s", function);
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

// ---- a function the suite calls that the game's driver has not
//
// A driver has the names of its own API version, and the suite looks up every version's. A call
// of a name the driver lacks does nothing and returns nought (nvnlayer.h, NvnLayerMissedName), so
// the case goes on to its end and finalizes what it made; its verdict then judged no answer of
// the driver's, and the case ends unavailable, naming the functions.

/// The cases that ended so and the functions each called, for the notes the step ends on.
#define TAKEN_CASES 96u
#define TAKEN_CALLS 512u
static char taken_case[TAKEN_CASES][JOURNAL_NAME_MAX];
static size_t taken_cases;
static struct {
    const char *function;
    size_t at;
} taken[TAKEN_CALLS];
static size_t taken_count;
static bool taken_more;
/// The lacked functions called with no case open, which no case is unavailable for.
#define OUTSIDE_CALLS 32u
static const char *outside[OUTSIDE_CALLS];
static size_t outside_count;
static bool case_open;

/// The report's watcher while the suite runs: a case starts with nothing called yet.
static void CaseWatch(const char *suite, const char *name, bool started) {
    (void)suite;
    (void)name;
    if (started) {
        NvnLayerMissedClear();
    }
    case_open = started;
}

/// The layer's hook: a lacked function was called for the first time since a case started. Said
/// on the machine's log, where the report's own line comes only when the case ends.
static void LackedCalled(const char *name) {
    char said[160];
    const int length = snprintf(said, sizeof(said),
                                "amberlith mod: %s was called, and the game's driver has no "
                                "function of that name: it did nothing",
                                name);
    PlatformLog(said, (size_t)length);
    if (!case_open && outside_count < OUTSIDE_CALLS) {
        outside[outside_count++] = name;
    }
}

/// The report's question as a case ends with a verdict: whether the case called a function the
/// game's driver has not. It is then unavailable, with the functions' names.
static bool CaseUnasked(const char *suite, const char *name, bool passed, char *why,
                        size_t why_size) {
    (void)suite;
    const size_t count = NvnLayerMissedCount();
    if (count == 0u) {
        return false;
    }
    // The names first, as many as leave room for the sentence's end; the notes have them all.
    static const char END[] = ", which the case called: nothing was done and nought came back, "
                              "so the verdict the case came to judged no answer";
    const size_t room = why_size > sizeof(END) + 64u ? why_size - sizeof(END) - 64u : 0u;
    size_t length = (size_t)snprintf(why, why_size, "the game's driver has no ");
    size_t named = 0;
    for (; named < count; ++named) {
        const char *const function = NvnLayerMissedName(named);
        if (length + strlen(function) + 2u > room) {
            break;
        }
        length += (size_t)snprintf(why + length, why_size - length, "%s%s",
                                   named != 0u ? ", " : "", function);
    }
    if (named < count) {
        length += (size_t)snprintf(why + length, why_size - length, ", and %zu more",
                                   count - named);
    }
    snprintf(why + length, why_size - length, "%s (%s)", END, passed ? "pass" : "fail");

    if (taken_cases == TAKEN_CASES) {
        taken_more = true;
        return true;
    }
    snprintf(taken_case[taken_cases], sizeof(taken_case[0]), "%s", name);
    for (size_t i = 0; i < count; ++i) {
        if (taken_count == TAKEN_CALLS) {
            taken_more = true;
            break;
        }
        taken[taken_count].function = NvnLayerMissedName(i);
        taken[taken_count].at = taken_cases;
        ++taken_count;
    }
    ++taken_cases;
    return true;
}

/// A note's room for a list, after the words before it.
#define LIST_ROOM ((size_t)REPORT_NOTE_MAX - 200u)

/// The names this build looks up that the game's driver has no function for, every one, over as
/// many notes as they take.
static void NoteLacked(void) {
    static NvnProcs procs;
    char missing[64] = "";
    if (!NvnLoadProcs(&procs, missing, sizeof(missing))) {
        return;    // the nvn suite's first case says which name
    }
    const size_t count = NvnLayerLackedCount();
    const size_t all = sizeof(procs) / sizeof(NvnGenericProc) - 2u;
    if (count == 0u) {
        ReportNote("host: the game's driver has a function for each of the %zu names this build "
                   "looks up",
                   all);
        return;
    }
    // Twice over the names: how many notes they take, then the notes.
    size_t notes = 0;
    for (int writing = 0; writing < 2; ++writing) {
        char names[LIST_ROOM];
        size_t length = 0;
        size_t note = 0;
        for (size_t i = 0; i <= count; ++i) {
            const char *const name = i < count ? NvnLayerLackedName(i) : NULL;
            if (name == NULL || length + strlen(name) + 2u >= sizeof(names)) {
                ++note;
                if (writing != 0) {
                    ReportNote("host: the game's driver has no function for %zu of the %zu names "
                               "this build looks up (%zu of %zu): %s",
                               count, all, note, notes, names);
                }
                length = 0;
            }
            if (name != NULL) {
                length += (size_t)snprintf(names + length, sizeof(names) - length, "%s%s",
                                           length != 0u ? ", " : "", name);
            }
        }
        notes = note;
    }
}

/// What the lacked functions took: for each one a case called, the cases it made unavailable.
static void NoteTaken(void) {
    for (size_t first = 0; first < taken_count; ++first) {
        const char *const function = taken[first].function;
        bool seen = false;
        size_t cases = 0;
        for (size_t i = 0; i < taken_count; ++i) {
            if (strcmp(taken[i].function, function) == 0) {
                seen = seen || i < first;
                ++cases;
            }
        }
        if (seen) {
            continue;
        }
        char list[LIST_ROOM];
        size_t length = 0;
        bool began = false;
        for (size_t i = first; i <= taken_count; ++i) {
            const char *const name = i < taken_count && strcmp(taken[i].function, function) == 0
                                         ? taken_case[taken[i].at]
                                         : NULL;
            if (i < taken_count && name == NULL) {
                continue;
            }
            if (name == NULL || length + strlen(name) + 4u >= sizeof(list)) {
                if (!began) {
                    ReportNote("host: %s, which the game's driver has not, made %zu case%s "
                               "unavailable: %s",
                               function, cases, cases == 1u ? "" : "s", list);
                } else if (length != 0u) {
                    ReportNote("host: %s, more of the cases it made unavailable: %s", function,
                               list);
                }
                began = true;
                length = 0;
            }
            if (name != NULL) {
                length += (size_t)snprintf(list + length, sizeof(list) - length, "%s\"%s\"",
                                           length != 0u ? "; " : "", name);
            }
        }
    }
    if (taken_more) {
        ReportNote("host: more cases were unavailable than these notes keep (%u cases, %u calls): "
                   "each case's own line names its functions",
                   (unsigned)TAKEN_CASES, (unsigned)TAKEN_CALLS);
    }
    if (outside_count != 0u) {
        char list[LIST_ROOM];
        size_t length = 0;
        for (size_t i = 0; i < outside_count && length + strlen(outside[i]) + 2u < sizeof(list);
             ++i) {
            length += (size_t)snprintf(list + length, sizeof(list) - length, "%s%s",
                                       i != 0u ? ", " : "", outside[i]);
        }
        ReportNote("host: called with no case open, and the game's driver has no function of the "
                   "name: %s",
                   list);
    }
}

// ---- the mod's window

/// The window the mod opens for the nvn suite's window cases, as the plain program has its own: a
/// layer of the mod's on the game's default display and the layer's native window, through the
/// game's nn::vi. Opened on the first ask and kept to the run's end; the mode file's nowindow
/// keeps it closed, and the game then keeps the screen.
static struct {
    bool wanted;
    bool asked;
    bool initialized;
    GameViDisplay *display;
    GameViLayer *layer;
    void *native;
    char why[320];
} window;

/// Opens the window, or keeps in window.why what stopped it, naming the result or the names the
/// game's SDK has not.
static void WindowOpen(void) {
    void (*const volatile initialize)(void) = GameViInitialize;
    __typeof__(GameViOpenDefaultDisplay) *const volatile open_display = GameViOpenDefaultDisplay;
    __typeof__(GameViCreateLayer) *const volatile create_layer = GameViCreateLayer;
    __typeof__(GameViGetNativeWindow) *const volatile get_native = GameViGetNativeWindow;
    const HostName NAMES[] = {
        { "nn::vi::Initialize", initialize != NULL },
        { "nn::vi::OpenDefaultDisplay", open_display != NULL },
        { "nn::vi::CreateLayer", create_layer != NULL },
        { "nn::vi::GetNativeWindow", get_native != NULL },
    };
    char lacked[200];
    if (HostLacks(lacked, sizeof(lacked), NAMES, sizeof(NAMES) / sizeof(NAMES[0])) != 0u) {
        snprintf(window.why, sizeof(window.why),
                 "the game's SDK has no %s, so the mod has no window of its own", lacked);
        return;
    }
    initialize();
    window.initialized = true;
    uint32_t rc = open_display(&window.display);
    if (R_FAILED(rc)) {
        window.display = NULL;
        snprintf(window.why, sizeof(window.why),
                 "nn::vi::OpenDefaultDisplay returned 0x%x, so the mod has no window of its own",
                 (unsigned)rc);
        return;
    }
    rc = create_layer(&window.layer, window.display);
    if (R_FAILED(rc)) {
        window.layer = NULL;
        snprintf(window.why, sizeof(window.why),
                 "nn::vi::CreateLayer on the default display returned 0x%x, so the mod has no "
                 "window of its own",
                 (unsigned)rc);
        return;
    }
    rc = get_native(&window.native, window.layer);
    if (R_FAILED(rc) || window.native == NULL) {
        snprintf(window.why, sizeof(window.why),
                 "nn::vi::GetNativeWindow of the mod's layer returned 0x%x and %s, so the mod has "
                 "no window of its own",
                 (unsigned)rc, window.native == NULL ? "no window" : "a window");
        window.native = NULL;
        return;
    }
    ReportNote("host: the mod's window, a layer of its own on the default display through the "
               "game's nn::vi");
}

void *NvnGameWindow(const char **why) {
    if (!window.wanted) {
        *why = "the mode file says nowindow: the mod opens no window of its own, and the game keeps "
               "the screen";
        return NULL;
    }
    if (!window.asked) {
        window.asked = true;
        WindowOpen();
    }
    *why = window.native != NULL ? NULL : window.why;
    return window.native;
}

/// Closes what WindowOpen opened, the layer, the display and nn::vi, each when the game has it.
static void WindowClose(void) {
    void (*const volatile destroy_layer)(GameViLayer *) = GameViDestroyLayer;
    void (*const volatile close_display)(GameViDisplay *) = GameViCloseDisplay;
    void (*const volatile finalize)(void) = GameViFinalize;
    if (window.layer != NULL && destroy_layer != NULL) {
        destroy_layer(window.layer);
    }
    if (window.display != NULL && close_display != NULL) {
        close_display(window.display);
    }
    if (window.initialized && finalize != NULL) {
        finalize();
    }
    window.layer = NULL;
    window.display = NULL;
    window.native = NULL;
    window.initialized = false;
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
    NoteLacked();
    taken_cases = taken_count = outside_count = 0u;
    taken_more = false;
    NvnLayerSetFatal(LackedCalled);
    const ReportWatcher watching = ReportSetWatcher(CaseWatch);
    const ReportUnasked asking = ReportSetUnasked(CaseUnasked);
    RunNvnInGame();
    (void)ReportSetUnasked(asking);
    (void)ReportSetWatcher(watching);
    NvnLayerSetFatal(NULL);
    NoteTaken();
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
    .old_root = PROBE_OLD_DIRECTORY,
    .version = AMBERLITH_VERSION,
    .build = AMBERLITH_BUILD,
    .hash = AMBERLITH_HASH,
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
    window.wanted = !mode.no_window;
    RunnerSteps(&runner, families, timing, NULL);
    WindowClose();
    RunnerFinish(&runner, &mode, timing);
    return ReportFailed() == 0 ? 0 : 1;
}
