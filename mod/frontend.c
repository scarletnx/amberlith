// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The mod's frontend (docs/Frontends.md, docs/Mod.md): the engine as one more module of a game,
// run before the game's own main. Its NVN is the game's driver, looked up by name as the game
// looks it up, so on a console the cases are answered by the console's own NVN, and under
// ScarletNX by ScarletNX's at the game's API version. A window of its own on a layer the game's
// nn::vi gives, unless the mode file says nowindow: a detailed run's screens are drawn there
// through the game's NVN (screen.c), and a quick run leaves it to the window cases. No pad: every
// screen is left by its countdown, and the process ends with the run.

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
#include "screen.h"
#include "tests/tests.h"
#include "ui.h"
#include "version.h"
#include "wimsyref.h"

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

/// Asks the driver its API version with no device, once: the screens ask before the run, the
/// case after. @p api_asked names where the answer came from, or which lookup gave none.
static const char *api_asked;

static void AskApiVersion(void) {
    if (api_asked != NULL) {
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
    api_asked = get == NULL       ? "the driver's nvnBootstrapLoader gave no nvnDeviceGetProcAddress"
                : integer == NULL ? "the driver's lookup gave no nvnDeviceGetInteger"
                                  : "nvnDeviceGetInteger(NULL)";
}

static void VersionCase(void) {
    static const char *const NAME =
        "the game's driver answers its API version with no device, and it is one of this build's";
    if (!ReportStart(SUITE, NAME)) {
        return;
    }
    AskApiVersion();
    CHECK_TRUE(SUITE, NAME, CHECK_EXACT, ApiIsThisBuilds(),
               "%s: API %d.%d, where this build has the objects of API %d to %d, so the nvn "
               "family is not run",
               api_asked, api_major, api_minor, NVN_LAYER_GAME_API_MAJOR_OLDEST,
               NVN_LAYER_API_MAJOR);
}

/// Says on the machine's log which of the game's functions is called next: one of them that
/// ends the process, or never returns, is then the last line a debugger or a runtime's log has.
static void Calling(const char *function) {
    char said[96];
    const int length = snprintf(said, sizeof(said), "amberlith mod: calling %s", function);
    PlatformLog(said, (size_t)length);
}

/// What the driver was given before the first device, once: the screens' device is made before
/// the run, the suite's in it, and the driver takes its memory one time only.
static struct {
    bool given;
    bool oe;
    bool allocator;
    bool initialize;
} graphics;

// No nv::SetGraphicsDevtoolsAllocator: it is the debug layer's allocator, no device here asks for
// the debug layer, and the one SDK it was called in here (12.3.9, under ScarletNX) aborted inside
// it at this point of the start.
static void GiveGraphics(void) {
    if (graphics.given) {
        return;
    }
    graphics.given = true;
    __typeof__(GameOeInitialize) *const volatile oe = GameOeInitialize;
    __typeof__(GameSetGraphicsAllocator) *const volatile allocator = GameSetGraphicsAllocator;
    __typeof__(GameInitializeGraphics) *const volatile initialize = GameInitializeGraphics;
    graphics.oe = oe != NULL;
    graphics.allocator = allocator != NULL;
    graphics.initialize = initialize != NULL;
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
}

static void GraphicsCase(void) {
    static const char *const NAME =
        "the driver is given its allocator and its memory before the first device";
    if (!ReportStart(SUITE, NAME)) {
        return;
    }
    GiveGraphics();
    CHECK_TRUE(SUITE, NAME, CHECK_NONE, graphics.allocator && graphics.initialize,
               "the game's SDK has no %s%s%s, which a game calls before nvnDeviceInitialize",
               !graphics.allocator ? "nv::SetGraphicsAllocator" : "",
               !graphics.allocator && !graphics.initialize ? " and no " : "",
               !graphics.initialize ? "nv::InitializeGraphics" : "");
    if (!graphics.oe) {
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
/// The watcher CaseWatch stands in for, the running screen's, which it passes each case on to.
static ReportWatcher passed_on;

/// The report's watcher while the suite runs: a case starts with nothing called yet.
static void CaseWatch(const char *suite, const char *name, bool started) {
    if (passed_on != NULL) {
        passed_on(suite, name, started);
    }
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

/// The window the mod opens for its screens, or in a run with none for the nvn suite's window
/// cases, as the plain program has its own: a layer of the mod's on the game's default display
/// and the layer's native window, through the game's nn::vi. Opened on the first ask and kept to
/// the run's end; the mode file's nowindow keeps it closed, and the game then keeps the screen.
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
    passed_on = ReportSetWatcher(CaseWatch);
    const ReportUnasked asking = ReportSetUnasked(CaseUnasked);
    RunNvnInGame();
    (void)ReportSetUnasked(asking);
    (void)ReportSetWatcher(passed_on);
    passed_on = NULL;
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

// ---- the demos, which the mod lists as skipped
//
// Each demo's first case, as the plain program names it, skipped with why: its frames are
// deko3d's and its programs uam's, and in a game's process the mod has only the game's NVN.

static const char *const NO_DEMO =
    "the demos draw through deko3d, which the mod has not: in a game's process it draws only "
    "through the game's NVN";

/// The game family's levels the mode file names (RunMode.levels).
static uint32_t levels;

static void SkipGameScene(void) {
    ReportSkip("game", "a graphics device, a way to the display, the renderer and the card's file "
               "come up",
               REPORT_REASON_UNAVAILABLE, NO_DEMO);
}

static void SkipWimsy(void) {
    for (uint32_t level = 0; level < WIMSY_LEVELS; ++level) {
        if ((levels & (1u << level)) == 0u) {
            continue;
        }
        char name[96];
        snprintf(name, sizeof(name),
                 "level %u, %s: a graphics device and a way to the display come up",
                 (unsigned)level, WimsyLevelName(level));
        ReportSkip("wimsy", name, REPORT_REASON_UNAVAILABLE, NO_DEMO);
    }
}

static void SkipZoom(const char *suite) {
    ReportSkip(suite,
               "a graphics device with compute, the programs and a way to the display come up",
               REPORT_REASON_UNAVAILABLE, NO_DEMO);
}

static void SkipMandelbrot(void) {
    SkipZoom("mandelbrot");
}

static void SkipJulia(void) {
    SkipZoom("julia");
}

static void SkipFluidScene(const char *suite) {
    ReportSkip(suite, "a graphics device, the fields' targets, the shaders and a way to the "
               "display come up",
               REPORT_REASON_UNAVAILABLE, NO_DEMO);
}

static void SkipFluid(void) {
    SkipFluidScene("fluid");
}

static void SkipFluidAlias(void) {
    SkipFluidScene("fluid-alias");
}

// The judge first and always, as on every frontend, less its reference step: that one holds the
// drawing suites' references, which no case here is judged by, and under a recompiler that made
// the game unoptimized it alone took over ten minutes. Then the nvn family, which runs when the
// file names no family: the mod's own cases and the nvn suite's looked up way. Then the game and
// demo families, each run only when named, as on the plain program: their demos listed as
// skipped, in the plain program's order.
static const RunnerStep STEPS[] = {
    { "selfcheck", RunSelfcheck, 0, false, false },
    { "support", RunSupportSteps, 0, false, false },
    { "nvn", RunInGame, MODE_FAMILY_NVN, false, true },
    { "game", SkipGameScene, MODE_FAMILY_GAME, false, false },
    { "wimsy", SkipWimsy, MODE_FAMILY_GAME, false, false },
    { "mandelbrot", SkipMandelbrot, MODE_FAMILY_DEMO, false, false },
    { "julia", SkipJulia, MODE_FAMILY_DEMO, false, false },
    { "fluid", SkipFluid, MODE_FAMILY_DEMO, false, false },
    { "fluid-alias", SkipFluidAlias, MODE_FAMILY_DEMO, false, false },
};

static const RunnerFrontend FRONTEND = {
    .root = PROBE_DIRECTORY,
    .old_root = PROBE_OLD_DIRECTORY,
    .version = AMBERLITH_VERSION,
    .build = AMBERLITH_BUILD,
    .hash = AMBERLITH_HASH,
    .steps = STEPS,
    .step_count = sizeof(STEPS) / sizeof(STEPS[0]),
    .present = MODE_FAMILY_NVN | MODE_FAMILY_GAME | MODE_FAMILY_DEMO,
    .asked = MODE_FAMILY_GAME | MODE_FAMILY_DEMO,
    .step_done = StepDone,
};

static Runner runner;

// ---- the screens

bool NvnGameScreenUp(void) {
    return ScreenIsUp();
}

/// The screens on the mod's window: the driver asked its version and given its memory, then the
/// screens' device and window. False with the reason in a note; the run then goes on without
/// them, and the nvn suite's window cases have the window.
static bool ScreensUp(void) {
    AskApiVersion();
    if (!ApiIsThisBuilds()) {
        ReportNote("screens: none, since %s gave API %d.%d, where this build has the objects of "
                   "API %d to %d",
                   api_asked, api_major, api_minor, NVN_LAYER_GAME_API_MAJOR_OLDEST,
                   NVN_LAYER_API_MAJOR);
        return false;
    }
    GiveGraphics();
    if (!graphics.allocator || !graphics.initialize) {
        ReportNote("screens: none, since the game's SDK has no %s, which a game calls before "
                   "nvnDeviceInitialize",
                   !graphics.allocator ? "nv::SetGraphicsAllocator" : "nv::InitializeGraphics");
        return false;
    }
    const char *no_window = NULL;
    void *const native = NvnGameWindow(&no_window);
    if (native == NULL) {
        ReportNote("screens: none, since %s", no_window);
        return false;
    }
    Calling("nvnDeviceInitialize, for the screens");
    char why[200];
    if (!ScreenUp(native, why, sizeof(why))) {
        ReportNote("screens: none, since %s; the nvn suite's window cases have the window", why);
        return false;
    }
    ReportNote("screens: the start screen, the running screen and the results, on the mod's "
               "window through the game's NVN; a game's process gives the mod no pad, so each "
               "is left by its countdown");
    return true;
}

/// The start screen, padless: the rows as the mode leaves them, its crash list, and the line on
/// the nvn suite's window cases whenever the family runs, which in the mod is when no family is
/// named.
static void StartScreen(const RunMode *mode, uint32_t families, const char *heading) {
    static UiStartInfo info;
    memset(&info, 0, sizeof(info));
    info.heading = heading;
    info.families_present = FRONTEND.present;
    info.nvn_named = (families & MODE_FAMILY_NVN) != 0u;
    info.padless = true;
    const CrashList *const listed = ReportCrashList();
    for (uint32_t i = 0; i < listed->count && info.listed_count < UI_START_LIST_MAX; ++i) {
        info.listed_suite[info.listed_count] = listed->entries[i].suite;
        info.listed_name[info.listed_count] = listed->entries[i].name;
        ++info.listed_count;
    }
    ScreenStart(&info, UiStartOpen(mode, FRONTEND.present));
}

/// A frame that did not reach the window, said once in the report.
static void NoteScreenError(void) {
    static bool said;
    if (!said && ScreenError()[0] != '\0') {
        said = true;
        ReportNote("screens: a frame did not reach the window: %s", ScreenError());
    }
}

int ModRun(const ModStart *start) {
    started = start;
    (void)RunnerInit(&runner, &FRONTEND);

    const RunMode from_file = RunnerModeFromFile(runner.mode_path);
    RunnerPlan(&runner, &from_file);
    // No screen to ask whether to go on with a run left in the middle: it goes on, as a quick
    // run does on the console.
    RunMode mode = RunnerModeAtStart(&runner, &from_file);
    RunnerOpen(&runner, &mode);
    window.wanted = !mode.no_window;
    static char heading[96];
    snprintf(heading, sizeof(heading), "Amberlith %s (%s)", AMBERLITH_VERSION, AMBERLITH_BUILD);
    // A detailed run has the plain program's screens; a quick one, a script's, draws nothing.
    const bool screens = mode.detailed && window.wanted && ScreensUp();
    if (runner.plan.start == JOURNAL_FRESH) {
        if (screens) {
            StartScreen(&mode, RunnerFamilies(&runner, &mode), heading);
        }
        RunnerBeginJournal(&runner, &from_file, &mode);
    }
    const uint32_t families = RunnerFamilies(&runner, &mode);
    RunnerApplyMode(&runner, &mode);
    if (runner.plan.start == JOURNAL_FRESH) {
        if (mode.detailed && !screens) {
            ReportNote("detailed: the mod has no screens in this launch, so the run leaves once "
                       "its report is closed, as a quick one does");
        }
        ReportNote("frontend: a module of a game, run before the game's main; the card by %s",
                   start->mounted_by);
        NoteHost();
    }
    const bool timing = !mode.no_timing;
    levels = mode.levels;
    uint32_t steps = 0;
    for (size_t i = 0; i < sizeof(STEPS) / sizeof(STEPS[0]); ++i) {
        steps += STEPS[i].family == 0u || (families & STEPS[i].family) != 0u ? 1u : 0u;
    }
    ReportRetain(screens);
    const bool progress = screens && !mode.no_progress;
    if (progress) {
        ScreenProgressBegin(heading, runner.run_directory, steps);
        (void)ReportSetWatcher(ScreenProgressWatch);
    }
    RunnerSteps(&runner, families, timing, progress ? ScreenProgressStep : NULL);
    if (progress) {
        (void)ReportSetWatcher(NULL);
        ScreenProgressEnd();
    }
    NoteScreenError();
    RunnerFinish(&runner, &mode, timing);
    if (screens) {
        // Last, and only now: the report is written, counted and closed. The one line that
        // comes after the summary says the results left by themselves, as the plain program's
        // do when nothing is pressed; here nothing can be.
        static char results[96];
        snprintf(results, sizeof(results), "%s%s%s", AMBERLITH_VERSION,
                 mode.machine[0] != '\0' ? " on " : "", mode.machine);
        ScreenResults(results);
        ReportAppend(runner.report_path);
        NoteScreenError();
        ReportNote("detailed mode: the results closed by themselves %u seconds after the report "
                   "closed: the mod reads no pad",
                   (unsigned)(UI_UNATTENDED_NANOSECONDS / 1000000000ull));
        ReportClose();
    }
    ScreenDown();
    WindowClose();
    return ReportFailed() == 0 ? 0 : 1;
}
