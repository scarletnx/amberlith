// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <switch.h>

#include "browse.h"
#include "demo.h"
#include "hooks.h"
#include "journal.h"
#include "mode.h"
#include "musicplay.h"
#include "picture.h"
#include "platform.h"
#include "report.h"
#include "runner.h"
#include "tests/tests.h"
#include "viewer.h"
#include "version.h"
#include "wimsyref.h"

// The console's frontend (docs/Frontends.md): the run itself is runner.c's, the same on every
// frontend; what is the console's own is here: its steps, the screens a person sees around the
// run, and the settings its suites take from the mode.

// Where the run leaves its report on the machine it ran on (hooks.h, PROBE_DIRECTORY): on an
// emulator a plain directory on the host; on the console the memory card.

static void RunSupportSteps(void) {
    RunSupportMode();
    RunSupport();
    RunSupportPerf();
    RunSupportSettle();
}

// The order they run in, which is the roadmap's: the judge first and always, since the
// selfcheck suite proves the comparisons every family then leans on; then the processor, which
// needs no device and runs in every mode; then the families.
static const RunnerStep STEPS[] = {
    { "selfcheck", RunSelfcheck, 0, false, false },
    { "support", RunSupportSteps, 0, false, false },
    { "reference", RunReference, 0, false, false },
    { "cpu", RunCpu, 0, false, true },
    // The C library over the processor, the same cases every frontend runs (docs/families/Libc.md):
    // only when named here, so a run that names no family reports what it did before them.
    { "libc", RunLibc, MODE_FAMILY_LIBC, false, true },
    { "system", RunSystem, MODE_FAMILY_LIBC, false, true },
    { "socket", RunSocket, MODE_FAMILY_LIBC, false, true },
    // Right after the processor, which takes seconds: a person sits in front of this one, and
    // should not wait for the families that need nobody. Untimed: a person's press is the point.
    { "pad", RunPad, MODE_FAMILY_PAD, false, true },
    // The touch screen after the controller: the same person, still holding the console.
    { "touch", RunTouch, MODE_FAMILY_PAD, false, true },
    { "clear", RunClear, MODE_FAMILY_GRAPHICS, true, true },
    { "format", RunFormat, MODE_FAMILY_GRAPHICS, true, true },
    { "raster", RunRaster, MODE_FAMILY_GRAPHICS, true, true },
    { "intmath", RunIntmath, MODE_FAMILY_GRAPHICS, true, true },
    { "blend", RunBlend, MODE_FAMILY_GRAPHICS, true, true },
    { "depth", RunDepth, MODE_FAMILY_GRAPHICS, true, true },
    // After depth, whose test the depth bounds and the depth mode cases lean on.
    { "fixed", RunFixed, MODE_FAMILY_GRAPHICS, true, true },
    { "stencil", RunStencil, MODE_FAMILY_GRAPHICS, true, true },
    { "texture", RunTexture, MODE_FAMILY_GRAPHICS, true, true },
    { "lod", RunLod, MODE_FAMILY_GRAPHICS, true, true },
    // After lod: its last cases fetch a four sample texture, which leans on the texture suites.
    { "coverage", RunCoverage, MODE_FAMILY_GRAPHICS, true, true },
    // After coverage, whose four sample targets these lay out in memory.
    { "msaa", RunMsaa, MODE_FAMILY_GRAPHICS, true, true },
    // After the texture suites: the same sampler, asked about formats, reductions and coordinates.
    { "sampling", RunSampling, MODE_FAMILY_GRAPHICS, true, true },
    { "coherence", RunCoherence, MODE_FAMILY_GRAPHICS, true, true },
    { "fmath", RunFmath, MODE_FAMILY_GRAPHICS, true, true },
    { "order", RunOrder, MODE_FAMILY_GRAPHICS, true, true },
    // After order: the same question between two queues, by a word in memory.
    { "queue", RunQueue, MODE_FAMILY_GRAPHICS, true, true },
    // After order and queue, whose compute dispatches these store into images from.
    { "storage", RunStorage, MODE_FAMILY_GRAPHICS, true, true },
    { "index", RunIndex, MODE_FAMILY_GRAPHICS, true, true },
    // After index, whose indexed draws the restart and indexed quad cases lean on.
    { "topology", RunTopology, MODE_FAMILY_GRAPHICS, true, true },
    { "vertex", RunVertex, MODE_FAMILY_GRAPHICS, true, true },
    { "instance", RunInstance, MODE_FAMILY_GRAPHICS, true, true },
    { "geometry", RunGeometry, MODE_FAMILY_GRAPHICS, true, true },
    // After geometry: the vertex stage's outputs again, captured into a buffer instead of drawn.
    { "xfb", RunXfb, MODE_FAMILY_GRAPHICS, true, true },
    { "bindless", RunBindless, MODE_FAMILY_GRAPHICS, true, true },
    { "astc", RunAstc, MODE_FAMILY_GRAPHICS, true, true },
    { "astcimage", RunAstcImage, MODE_FAMILY_GRAPHICS, true, true },
    { "eac", RunEac, MODE_FAMILY_GRAPHICS, true, true },
    { "etc2", RunEtc2, MODE_FAMILY_GRAPHICS, true, true },
    { "bcn", RunBcn, MODE_FAMILY_GRAPHICS, true, true },
    // After bcn, whose blocks prove each format and mode one at a time: the same formats as whole
    // textures, as the astcimage suite after astc.
    { "bcnimage", RunBcnImage, MODE_FAMILY_GRAPHICS, true, true },
    { "zeta", RunZeta, MODE_FAMILY_GRAPHICS, true, true },
    { "swizzle", RunSwizzle, MODE_FAMILY_GRAPHICS, true, true },
    { "texel", RunTexel, MODE_FAMILY_GRAPHICS, true, true },
    { "stream", RunStream, MODE_FAMILY_GRAPHICS, true, true },
    { "fill", RunFill, MODE_FAMILY_GRAPHICS, true, true },
    { "ring", RunRing, MODE_FAMILY_GRAPHICS, true, true },
    { "occlusion", RunOcclusion, MODE_FAMILY_GRAPHICS, true, true },
    { "drawtex", RunDrawtex, MODE_FAMILY_GRAPHICS, true, true },
    { "query", RunQuery, MODE_FAMILY_GRAPHICS, true, true },
    { "dma", RunDma, MODE_FAMILY_GRAPHICS, true, true },
    { "blit2d", RunBlit2d, MODE_FAMILY_GRAPHICS, true, true },
    // After dma and blit2d, whose engines its copies go through by the library's own calls.
    { "copy", RunCopy, MODE_FAMILY_GRAPHICS, true, true },
    // Last of the family, since a machine that runs out of memory on a texture this large may
    // take the launch down with it. Untimed by the passes: one pass makes the uploads a game
    // makes once, and each case times its own work.
    { "tex8k", RunTex8k, MODE_FAMILY_GRAPHICS, false, true },
    // The graphics questions asked through NVN, its own family since it runs only when named.
    // Untimed: what it compares is a machine's NVN, not how fast it runs.
    { "nvn", RunNvn, MODE_FAMILY_NVN, false, true },
    { "fs", RunFs, MODE_FAMILY_FS, true, true },
    // Untimed: what a case judges is the order its threads ran in, and each waits on purpose.
    { "sched", RunSched, MODE_FAMILY_KERNEL, false, true },
    // Untimed too: what a case judges is the kernel's answer, and one sleeps out a timeout.
    { "svc", RunSvc, MODE_FAMILY_KERNEL, false, true },
    // After every drawing suite: under Eden a graphics device made after a framebuffer was
    // closed never finished its first submit, and each case here closes one of its own.
    { "binder", RunBinder, MODE_FAMILY_DISPLAY, false, true },
    // Untimed: one job per case is the point, and the time a job takes is not.
    { "vic", RunVic, MODE_FAMILY_VIDEO, false, true },
    // Untimed too: a case runs a job per frame, and what the line judges is every image.
    { "nvdec", RunNvdec, MODE_FAMILY_VIDEO, false, true },
    // Before the net family, whose cases can wait out timeouts. Untimed: its cases wait for
    // sound to play, and a rehearsal would only play it again.
    { "audio", RunAudio, MODE_FAMILY_AUDIO, false, true },
    // Untimed: what a case judges is a service's answer, and some open two dozen objects.
    { "service", RunService, MODE_FAMILY_SERVICE, false, true },
    { "net", RunNet, MODE_FAMILY_NET, false, true },
    // Last: it asks everything the families above ask one at a time, all at once. Untimed by
    // the passes: it runs its frames once and times each of them itself.
    { "game", RunGame, MODE_FAMILY_GAME, false, true },
    // The game family's levels, each named by a `level=` word: none runs without one. Untimed by
    // the passes, as the scene before it.
    { "wimsy", RunWimsy, MODE_FAMILY_GAME, false, true },
    // The demos, after every family: each makes a device and a swapchain of its own, as the game
    // family's scene does. Untimed by the passes: each times its own frames.
    { "mandelbrot", RunMandelbrot, MODE_FAMILY_DEMO, false, true },
    { "julia", RunJulia, MODE_FAMILY_DEMO, false, true },
    // A 2D fluid in ping-pong passes, after the zooms.
    { "fluid", RunFluid, MODE_FAMILY_DEMO, false, true },
    // Its aliasing level: targets drawn over their old selves and a storage image over them.
    { "fluid-alias", RunFluidAlias, MODE_FAMILY_DEMO, false, true },
    // After every family that times its frames: what it asks for is the music those must not
    // hear. Untimed: its cases wait for sound to play.
    { "music", RunMusic, MODE_FAMILY_MUSIC, false, true },
};

// The console's frontend for the runner: its folder, its build and its steps.
static const RunnerFrontend FRONTEND = {
    .root = PROBE_DIRECTORY,
    .version = EDENPROBE_VERSION,
    .build = EDENPROBE_BUILD,
    .hash = EDENPROBE_HASH,
    .steps = STEPS,
    .step_count = sizeof(STEPS) / sizeof(STEPS[0]),
    .present = MODE_FAMILIES_PRESENT,
    .asked = MODE_FAMILIES_ASKED,
    .step_done = HooksReportOwnCode,
};

// The run, static: the journal and the crash list it holds are tens of kilobytes.
static Runner runner;

/// Writes the words the start screen chose into the mode file, when it was asked to; a file that
/// cannot be written leaves the choice this run's alone, and says so.
static void SaveModeFile(RunMode *mode) {
    if (!mode->saved_by_screen) {
        return;
    }
    static char text[MODE_FILE_MAX];
    const size_t length = ModeFormat(text, sizeof(text) - 1u, mode);
    text[length] = '\n';
    text[length + 1u] = '\0';
    if (!JournalWriteFile(runner.mode_path, text)) {
        mode->saved_by_screen = false;
        ReportNote("mode file: %s could not be written, so what the start screen chose is this "
                   "run's only",
                   runner.mode_path);
    }
}

// The start screen's words, which the browser's start tab shows too.
static UiStartInfo start_info;

/// The start screen, when a new detailed run begins: the menu that chooses the families, quick,
/// notime, the crash list's cases, and whether to keep the choice in the mode file; and L, R or
/// a tab for the browser instead. Hands back the mode the run begins with, as it came when there
/// was no screen, and in @p chosen the screen as it was left.
static void FillStartInfo(const RunMode *from_file) {
    UiStartInfo *const info = &start_info;
    memset(info, 0, sizeof(*info));
    static char heading[96];
    snprintf(heading, sizeof(heading), "edenprobe %s (%s)", EDENPROBE_VERSION, EDENPROBE_BUILD);
    info->heading = heading;
    info->families_present = MODE_FAMILIES_PRESENT;
    info->nvn_named = (ModeFamiliesNamed(from_file, MODE_FAMILIES_PRESENT) & MODE_FAMILY_NVN) != 0u;
    const CrashList *const listed = ReportCrashList();
    for (uint32_t i = 0; i < listed->count && info->listed_count < UI_START_LIST_MAX; ++i) {
        info->listed_suite[info->listed_count] = listed->entries[i].suite;
        info->listed_name[info->listed_count] = listed->entries[i].name;
        ++info->listed_count;
    }
}

static RunMode StartScreen(const RunMode *from_file, UiStartState *chosen) {
    FillStartInfo(from_file);
    const UiStartInfo *const info = &start_info;
    *chosen = UiStartOpen(from_file, MODE_FAMILIES_PRESENT);
    if (!UiStartShown(info) || !ViewerStartScreen(info, from_file, chosen)) {
        return *from_file;
    }
    return chosen->tab_asked != 0u ? *from_file
                                   : UiStartMode(chosen, *from_file, MODE_FAMILIES_PRESENT);
}

/// The screen that asks whether to go on with the run the journal left in the middle, before
/// anything is opened. Starting over makes the plan a new run's; the case the program ended
/// inside stays in it, for the crash list.
static void AskToGoOn(const RunMode *mode) {
    static UiResumeInfo info;
    memset(&info, 0, sizeof(info));
    static char heading[96];
    snprintf(heading, sizeof(heading), "edenprobe %s (%s)", EDENPROBE_VERSION, EDENPROBE_BUILD);
    static char where[JOURNAL_SUITE_MAX + JOURNAL_NAME_MAX + 96u];
    JournalDescribeStop(where, sizeof(where), &runner.plan, &runner.journal);
    info.heading = heading;
    info.where = where;
    info.folder = runner.run_directory;
    info.launch = runner.plan.launch;
    const uint32_t families = RunnerFamilies(&runner, mode);
    for (size_t i = 0; i < sizeof(STEPS) / sizeof(STEPS[0]); ++i) {
        if (STEPS[i].family == 0u || (families & STEPS[i].family) != 0u) {
            ++info.steps;
            info.steps_done += JournalStepDone(&runner.journal, STEPS[i].name) ? 1u : 0u;
        }
    }
    info.passed = runner.plan.counts.counts[JOURNAL_PASSED];
    info.failed = runner.plan.counts.counts[JOURNAL_FAILED];
    info.skipped = runner.plan.counts.counts[JOURNAL_SKIPPED];
    info.crashed = runner.plan.counts.counts[JOURNAL_CRASHED];
    bool start_over = false;
    if (ViewerResumeScreen(&info, &start_over) && start_over) {
        JournalPlanRestart(&runner.plan);
    }
}

// ---- The browser (docs/UI.md) ---------------------------------------------------------------

// The demos the browser lists, one line a demo, as STEPS is one a step: what it is called, the
// suite its lines report under, what it shows, its music, its family, and its run.
static void DemoGame(const DemoRun *run) {
    GameSetFrames(run->frames);
    GameSetTiming(run->timing);
    GameSetDemo(run);
    // Its track on its own renderer, as long as the browser's music option says so.
    GameSetMusic(MusicTrackNamed(BrowseMusicOf("game").track), ViewerDemoMusic);
    RunGame();
    GameSetMusic(MUSIC_TRACKS, NULL);
    GameSetDemo(NULL);
}

static void DemoWimsy(const DemoRun *run, uint32_t level) {
    WimsySetFrames(run->frames);
    WimsySetTiming(run->timing);
    WimsySetLevels(1u << level);
    WimsySetDemo(run);
    RunWimsy();
    WimsySetDemo(NULL);
}

static void DemoWimsy0(const DemoRun *run) {
    DemoWimsy(run, 0u);
}
static void DemoWimsy1(const DemoRun *run) {
    DemoWimsy(run, 1u);
}
static void DemoWimsy2(const DemoRun *run) {
    DemoWimsy(run, 2u);
}
static void DemoWimsy3(const DemoRun *run) {
    DemoWimsy(run, 3u);
}
static void DemoWimsy4(const DemoRun *run) {
    DemoWimsy(run, 4u);
}

static const BrowseDemo DEMOS[] = {
    { "Game scene", "game",
      "Everything a game asks at once: four threads over three cores, a silent tone, the card and "
      "the services, each frame's image held to what the processor works out.",
      MODE_FAMILY_GAME, DemoGame },
    { "Wimsy level 0", "wimsy",
      "Sprites: Wimsy flat, a sheet of her frames, as a game draws a character in two "
      "dimensions. Five frames are held to the ranges the processor works out.",
      MODE_FAMILY_GAME, DemoWimsy0 },
    { "Wimsy level 1", "wimsy",
      "Her lit model, turning: depth, culling and light, five frames held to the processor's "
      "ranges.",
      MODE_FAMILY_GAME, DemoWimsy1 },
    { "Wimsy level 2", "wimsy",
      "A scene near and far: Wimsy on a mipmapped ground with plants around her.",
      MODE_FAMILY_GAME, DemoWimsy2 },
    { "Wimsy level 3", "wimsy",
      "A game's heavy frame: Wimsy among some fifteen hundred draws, each with its own uniform "
      "buffer, over a 4K ASTC ground.",
      MODE_FAMILY_GAME, DemoWimsy3 },
    { "Wimsy level 4", "wimsy", "Level 3 over an 8K ground.", MODE_FAMILY_GAME, DemoWimsy4 },
    { "Mandelbrot", "mandelbrot",
      "A zoom into the Mandelbrot set: each frame a compute pass counts every pixel's "
      "iterations and a fragment pass colours them; six frames held to the processor's answer.",
      MODE_FAMILY_DEMO, RunMandelbrotDemo },
    { "Julia", "julia", "The same zoom over a Julia set.", MODE_FAMILY_DEMO, RunJuliaDemo },
    { "Fluid", "fluid",
      "A 2D fluid, Stam's stable fluids, in fragment passes in ping-pong between two targets a "
      "field; its early frames held to the processor's simulation, every judged one to the "
      "invariants.",
      MODE_FAMILY_DEMO, FluidDemo },
    { "Fluid, aliased", "fluid-alias",
      "The fluid with its targets drawn over their old selves and a compute pass's storage "
      "image over them, as a game's dynamic resolution leaves its pool.",
      MODE_FAMILY_DEMO, FluidAliasDemo },
};

#define DEMO_COUNT (sizeof(DEMOS) / sizeof(DEMOS[0]))
_Static_assert(DEMO_COUNT <= UI_BROWSE_DEMOS && DEMO_COUNT <= 32u,
               "every demo has a row and a mark in the browser");

// What the browser shows and what its runs pick, for the report's filter, which takes a plain
// function.
static BrowseCatalog catalog;
static UiDemoRow demo_rows[DEMO_COUNT];
static UiBrowseInfo browse_info;
static UiPick picked;

static bool Picked(const char *suite, const char *name) {
    return UiPickHas(&picked, &browse_info, suite, name);
}

static bool StepPicked(const RunnerStep *step) {
    return BrowseStepRuns(step->name, &picked, &browse_info);
}

static const PictureRecord *PictureOf(int32_t index) {
    return index >= 0 ? PictureAt((size_t)index) : NULL;
}

static bool DemoStopped(void) {
    return ViewerDemoStopped();
}

static bool DemoPaused(void) {
    return ViewerDemoPaused();
}

// The music (musicplay.h), which only the browser plays: started, switched and stopped here, a
// refusal said in the report's next line rather than kept back.
static void MusicTo(const char *track) {
    char error[160];
    const bool played = track == NULL ? (MusicPlayStop(), true)
                        : MusicPlayOn()  ? MusicPlaySwitch(track, error, sizeof(error))
                                         : MusicPlayStart(track, error, sizeof(error));
    if (!played) {
        ReportNote("browser: the music's track %s did not play: %s", track, error);
    }
}

/// The demos of the same suite as @p index, the ones its pause menu's level row walks: the first
/// of them and how many.
static uint32_t DemoLevels(uint32_t index, uint32_t *count) {
    uint32_t first = index;
    while (first > 0u && strcmp(DEMOS[first - 1u].suite, DEMOS[index].suite) == 0) {
        --first;
    }
    uint32_t last = index;
    while (last + 1u < DEMO_COUNT && strcmp(DEMOS[last + 1u].suite, DEMOS[index].suite) == 0) {
        ++last;
    }
    *count = last - first + 1u;
    return first;
}

typedef struct {
    const BrowseDemo *demo;
    DemoRun run;
} DemoStep;

static void RunDemoStep(const void *argument) {
    const DemoStep *const step = (const DemoStep *)argument;
    step->demo->run(&step->run);
}

/// The suites and the demos @p pick holds, a run of its own in a folder of its own: the report
/// opened, the judge, the steps picked in their order, the demos, the summary. The screen stays
/// up for the results.
static void BrowserRun(const RunMode *from_file, UiOptions *options, const UiStartState *start) {
    RunMode mode = *from_file;
    mode.no_timing = !options->timing;
    mode.every_case = options->every_picture;
    mode.retry = options->retry;
    mode.frames = options->frames;
    mode.quick = false;
    mode.detailed = true;
    mode.listen = start != NULL && start->listen && UiPickHasFamily(&picked, UI_FAMILY_AUDIO);
    // The cases the last run kept, and their pictures, go: this run's are what its results show.
    ReportForgetRecords();
    PictureForgetKept();
    const uint32_t number = RunnerPlanBrowser(&runner, &mode);
    if (number == 0u) {
        char said[RUNNER_ROOT_MAX + 160u];
        snprintf(said, sizeof(said),
                 "EDENPROBE BROWSER: no folder is left under %s for a run, the %u there are taken",
                 runner.runs_directory, (unsigned)RUNNER_BROWSER_RUNS_MAX);
        PlatformLog(said, strlen(said));
        return;
    }
    RunnerOpen(&runner, &mode);
    RunnerBeginJournal(&runner, &mode, &mode);
    RunnerApplyMode(&runner, &mode);
    {
        char described[REPORT_NOTE_MAX - 64];
        BrowseDescribePick(described, sizeof(described), &picked, &browse_info);
        ReportNote("browser: run %u, picked in the browser: the judge, then %s; the cases of a "
                   "step not picked neither run nor leave a line",
                   (unsigned)number, described);
    }
    HooksReportOwnCode(NULL);
    PictureWriteEveryCase(mode.every_case);
    PictureRetain(true);
    ReportRetain(true);
    StreamSetStress(mode.stress);
    AudioSetListen(mode.listen);
    const bool timing = !mode.no_timing;
    GameSetFrames(mode.frames);
    GameSetTiming(timing);
    WimsySetFrames(mode.frames);
    WimsySetTiming(timing);
    WimsySetLevels(BrowseLevels(&picked, &browse_info, "wimsy", WIMSY_LEVELS));
    Tex8kSetTiming(timing);
    Tex8kSetVram(mode.vram);

    uint32_t steps = 0;
    for (size_t i = 0; i < sizeof(STEPS) / sizeof(STEPS[0]); ++i) {
        steps += StepPicked(&STEPS[i]) ? 1u : 0u;
    }
    uint32_t demos = 0;
    for (uint32_t i = 0; i < DEMO_COUNT; ++i) {
        demos += (picked.demos >> i & 1u) != 0u ? 1u : 0u;
    }
    // No music while cases run: what they time and the audio family's own renderer stay as a run
    // from the mode file has them.
    MusicTo(NULL);
    static char running[96];
    snprintf(running, sizeof(running), "edenprobe %s (%s), browser run %u", EDENPROBE_VERSION,
             EDENPROBE_BUILD, (unsigned)number);
    ViewerProgressBegin(running, runner.run_directory, steps + demos, false, false);
    ReportSetPicked(Picked);
    RunnerStepsWhere(&runner, StepPicked, timing, ViewerProgressStep);
    ReportSetPicked(NULL);
    uint32_t step_number = steps;
    for (uint32_t i = 0; i < DEMO_COUNT; ++i) {
        if ((picked.demos >> i & 1u) == 0u) {
            continue;
        }
        // A demo, and the levels the person asks for from its pause menu after it.
        for (uint32_t next = i; next != UINT32_MAX;) {
            const BrowseDemo *const demo = &DEMOS[next];
            uint32_t level_count = 0;
            const uint32_t first = DemoLevels(next, &level_count);
            // The player plays the demo's track; a demo that streams it on its own renderer (the
            // game scene) wants the player's closed first, or its own would be the second.
            const BrowseMusic music = BrowseMusicOf(demo->suite);
            if (options->music || music.streams) {
                MusicTo(music.streams ? NULL : music.track);
            }
            const ViewerDemo shown = {
                demo->name,     music.track,       options->frames, options->hud,
                options->music, &demo_rows[first], level_count,     next - first,
                music.streams,
            };
            ViewerDemoBegin(&shown);
            DemoStep step = { demo, { true, options->frames, timing, DemoStopped, DemoPaused } };
            RunnerRunExtra(&runner, ++step_number, demo->suite, RunDemoStep, &step,
                           ViewerProgressStep);
            const ViewerDemoEnd end = ViewerDemoFinish();
            options->frames = end.frames;
            options->music = end.music;
            options->hud = end.hud;
            if (!options->music) {
                MusicTo(NULL);
            }
            next = end.back || end.next == UINT32_MAX ? UINT32_MAX : first + end.next;
            if (end.back) {
                i = DEMO_COUNT;
            }
        }
    }
    ViewerProgressEnd();
    RunnerFinish(&runner, &mode, timing);
    if (options->music) {
        MusicTo(BROWSE_MENU_TRACK);
    }
}

/// Where the results of the run just ended open: on the one case or the one suite it ran alone,
/// else on the first failure.
static UiState ResultsOn(const UiRun *results, const UiPick *pick) {
    UiState state = UiStart(results);
    state.browser = true;
    uint32_t whole = 0;
    uint32_t one_case = UINT32_MAX;
    uint32_t cases = 0;
    for (uint32_t i = 0; i < UiBrowseSuiteCount(); ++i) {
        whole += (pick->suites[i / 8u] >> (i % 8u) & 1u) != 0u ? 1u : 0u;
    }
    for (uint32_t i = 0; i < browse_info.catalogue_count && i < UI_MAX_CASES; ++i) {
        if ((pick->cases[i / 8u] >> (i % 8u) & 1u) != 0u) {
            one_case = i;
            ++cases;
        }
    }
    if (whole != 0u || cases != 1u || pick->demos != 0u) {
        return state;
    }
    const ReportRecord *const wanted = &browse_info.catalogue[one_case];
    for (uint32_t g = 0; g < results->group_count; ++g) {
        for (uint32_t n = 0; n < results->groups[g].count; ++n) {
            const ReportRecord *const record = UiCaseAt(results, g, n);
            if (strcmp(record->suite, wanted->suite) == 0 &&
                strcmp(record->name, wanted->name) == 0) {
                state.level = UI_LEVEL_CASE;
                state.group = g;
                state.index = n;
                return state;
            }
        }
    }
    return state;
}

/// The browser until it is left: @p tab first, the results of the run just ended among the tabs
/// when there was one. @p unattended_ns leaves when nothing is pressed that long (0: never).
/// Hands back how it ended.
static ViewerEnd BrowserSession(const RunMode *from_file, UiTab tab, const UiStartState *start,
                                const UiStartInfo *start_info, uint64_t unattended_ns,
                                bool after_run) {
    for (uint32_t i = 0; i < DEMO_COUNT; ++i) {
        const UiDemoRow row = { DEMOS[i].name, DEMOS[i].suite, DEMOS[i].description,
                                BrowseMusicOf(DEMOS[i].suite).track };
        demo_rows[i] = row;
    }
    static char heading[96];
    snprintf(heading, sizeof(heading), "%s%s%s", EDENPROBE_VERSION,
             from_file->machine[0] != '\0' ? " on " : "", from_file->machine);
    browse_info.heading = heading;
    browse_info.machine = heading;
    browse_info.catalogue = catalog.records;
    browse_info.catalogue_count = catalog.count;
    browse_info.demos = demo_rows;
    browse_info.demo_count = (uint32_t)DEMO_COUNT;
    browse_info.picture = PictureOf;
    browse_info.start = start_info;
    browse_info.results = after_run ? ViewerIndexRun(heading) : NULL;
    if (after_run) {
        BrowseCatalogMerge(&catalog, browse_info.results->records,
                           browse_info.results->count);
        browse_info.catalogue = catalog.records;
        browse_info.catalogue_count = catalog.count;
    }
    UiBrowseState state = UiBrowseOpen(tab, start, UiOptionsFrom(from_file), &browse_info);
    // The interactive mode: the music may play now, and only now (music.h).
    MusicSetInteractive(true);
    if (state.options.music) {
        MusicTo(BROWSE_MENU_TRACK);
    }
    UiPick last;
    memset(&last, 0, sizeof(last));
    bool have_last = false;
    ViewerEnd end = VIEWER_CLOSED;
    for (;;) {
        end = ViewerBrowse(&browse_info, &state, unattended_ns);
        unattended_ns = state.pressed ? 0u : unattended_ns;
        if (end != VIEWER_ASKED) {
            break;
        }
        UiPick pick;
        if (state.ask == UI_ASK_AGAIN) {
            if (!have_last) {
                continue;
            }
            pick = last;
        } else if (!UiBrowsePick(&state, &browse_info, &pick)) {
            continue;
        }
        picked = pick;
        last = pick;
        have_last = true;
        BrowserRun(from_file, &state.options, &state.start);
        browse_info.results = ViewerIndexRun(heading);
        BrowseCatalogMerge(&catalog, browse_info.results->records, browse_info.results->count);
        browse_info.catalogue = catalog.records;
        browse_info.catalogue_count = catalog.count;
        state.results = ResultsOn(browse_info.results, &pick);
        state.results.repeatable = true;
        state.tab = UI_TAB_RESULTS;
        state.changed = true;
    }
    MusicTo(NULL);
    MusicSetInteractive(false);
    ViewerBrowseClose();
    return end;
}

/// The catalogue the browser lists, read before the report a launch opens is written over the
/// last one: that report, then the newest of the browser's runs over it.
static void ReadCatalogue(void) {
    (void)BrowseCatalogRead(&catalog, runner.report_path);
    char browse[RUNNER_PATH_MAX + 16u];
    snprintf(browse, sizeof(browse), "%s/browse", runner.run_directory);
    uint32_t newest = 0;
    for (uint32_t n = 1; n <= RUNNER_BROWSER_RUNS_MAX; ++n) {
        char path[RUNNER_PATH_MAX + 32u];
        snprintf(path, sizeof(path), "%s/%u", browse, (unsigned)n);
        struct stat about;
        if (stat(path, &about) != 0) {
            break;
        }
        newest = n;
    }
    if (newest > 0u) {
        char path[RUNNER_PATH_MAX + 48u];
        snprintf(path, sizeof(path), "%s/%u/report.jsonl", browse, (unsigned)newest);
        (void)BrowseCatalogRead(&catalog, path);
    }
}

/// The browser instead of a run, asked for by the mode file's `browse` or from the start
/// screen: the launch's report says so and closes, and every run begun in the browser writes a
/// report of its own. Nothing pressed on its first screen for UI_START_NANOSECONDS when the word
/// opened it, the program leaves and the launch's report says why.
static int Browse(const RunMode *mode, UiTab tab, const UiStartState *start, bool by_word) {
    static char launch_report[RUNNER_PATH_MAX + 16u];
    snprintf(launch_report, sizeof(launch_report), "%s", runner.report_path);
    ReportNote("browser: %s; each run begun there writes its own report under %s/browse",
               by_word ? "the mode file's word browse opened it" : "the start screen opened it",
               runner.run_directory);
    ReportSummary();
    ReportClose();
    const ViewerEnd end = BrowserSession(mode, tab, start, &start_info,
                                         by_word ? UI_START_NANOSECONDS : 0u, false);
    if (end == VIEWER_UNATTENDED) {
        ReportAppend(launch_report);
        ReportNote("browser: nothing pressed for %u seconds on its first screen, left by itself",
                   (unsigned)(UI_START_NANOSECONDS / 1000000000ull));
        ReportClose();
    }
    ViewerEndScreen();
    return 0;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    // No console. The report channels stay the interface a verdict is read from, so a bring-up
    // problem on a new machine is never "did the screen work". The screens up before the report
    // closes, the one that asks whether to go on, the start screen, the running screen and the
    // listening sequence, only choose what runs and say what is happening.
    (void)RunnerInit(&runner, &FRONTEND);

    // What the card says before anything is opened: the mode file's words, then, in the folder
    // they and this build name, the journal of the run before, and the crash list. Whether this
    // launch goes on with that run or starts a new one depends on all three, and on this build.
    const RunMode from_file = RunnerModeFromFile(runner.mode_path);
    RunnerPlan(&runner, &from_file);
    RunMode mode = RunnerModeAtStart(&runner, &from_file);
    // A person in front of a detailed run chooses whether to go on with the run left in the
    // middle or start over; a quick run is a script's, and goes on.
    if (JournalPlanAsks(&runner.plan, mode.detailed)) {
        AskToGoOn(&mode);
        if (runner.plan.start == JOURNAL_FRESH) {
            mode = from_file;
        }
    }

    // The cases the browser lists, from the report the launch is about to open over: only for a
    // launch a person attends, so a run from the mode file reads nothing more.
    if (runner.plan.start == JOURNAL_FRESH && mode.detailed) {
        ReadCatalogue();
    }

    // Opened first, because reading the pad touches a service this program otherwise never
    // uses, and a run that dies there should still leave a report.
    RunnerOpen(&runner, &mode);
    RunnerAdoptBrowserCrash(&runner);

    // What the mode changes is which families run, whether the finished cases and pictures are
    // kept in memory and shown once everything is over, and whether every drawing case writes
    // one. It changes no verdict, and before the summary the line below is all it adds to the
    // report; a launch that goes on with a run said it when the run began, and keeps what that
    // launch's start screen chose. A new detailed run's start screen chooses it, and the
    // journal begins once it has.
    mode = ViewerModeWithPad(mode);
    if (runner.plan.start == JOURNAL_FRESH) {
        const RunMode asked = mode;
        UiStartState start = UiStartOpen(&asked, MODE_FAMILIES_PRESENT);
        uint32_t tab = 0u;
        if (ModeOpensBrowser(&mode)) {
            FillStartInfo(&asked);
            tab = UI_TAB_SUITES;
        } else if (mode.detailed) {
            mode = StartScreen(&asked, &start);
            tab = start.tab_asked;
            if (tab == 0u) {
                SaveModeFile(&mode);
            }
        }
        if (tab != 0u) {
            return Browse(&asked, (UiTab)tab, &start, ModeOpensBrowser(&asked));
        }
        RunnerBeginJournal(&runner, &asked, &mode);
    }
    const uint32_t families = RunnerFamilies(&runner, &mode);
    RunnerApplyMode(&runner, &mode);
    // What the SDK's own code answered at the start, the mode file read through it (hooks.h).
    HooksReportOwnCode(NULL);
    // A step the screens know no family for is shown under other, away from its own. The net
    // and service steps report under their suites' names, which the screens know.
    for (size_t i = 0; i < sizeof(STEPS) / sizeof(STEPS[0]); ++i) {
        if (strcmp(STEPS[i].name, "net") != 0 && strcmp(STEPS[i].name, "service") != 0 &&
            UiFamilyOf(STEPS[i].name) == UI_FAMILY_OTHER) {
            ReportNote("bookkeeping: the screens have no family for suite %s and show it under "
                       "other",
                       STEPS[i].name);
        }
    }
    PictureWriteEveryCase(mode.every_case);
    PictureRetain(mode.detailed);
    ReportRetain(mode.detailed);

    StreamSetStress(mode.stress);
    AudioSetListen(mode.listen);
    const bool timing = !mode.no_timing;
    GameSetFrames(mode.frames);
    GameSetTiming(timing);
    WimsySetFrames(mode.frames);
    WimsySetTiming(timing);
    WimsySetLevels(mode.levels);
    DemoSetFrames(mode.frames);
    DemoSetTiming(timing);
    Tex8kSetTiming(timing);
    Tex8kSetVram(mode.vram);
    // The running screen, for a run with a screen: a detailed one, or one the start screen made
    // quick. Counted over every step the run has, done ones included, so a launch that goes on
    // says how far the whole run is.
    uint32_t steps = 0;
    for (size_t i = 0; i < sizeof(STEPS) / sizeof(STEPS[0]); ++i) {
        steps += STEPS[i].family == 0u || (families & STEPS[i].family) != 0u ? 1u : 0u;
    }
    static char running[96];
    snprintf(running, sizeof(running), "edenprobe %s (%s)", EDENPROBE_VERSION, EDENPROBE_BUILD);
    PadSetHeading(running);
    TouchSetHeading(running);
    if (!mode.no_progress) {
        ViewerProgressBegin(running, runner.run_directory, steps, !mode.detailed, mode.detailed);
    }
    RunnerSteps(&runner, families, timing, ViewerProgressStep);
    ViewerProgressEnd();
    RunnerFinish(&runner, &mode, timing);

    // Last, and only now: the report is written, counted and closed, so nothing a person does
    // in front of the screen can reach it. The results are the browser's results tab, and its
    // other tabs are a press away.
    UiStartState start = UiStartOpen(&mode, MODE_FAMILIES_PRESENT);
    if (start_info.heading == NULL) {
        FillStartInfo(&mode);
    }
    if (mode.detailed && BrowserSession(&mode, UI_TAB_RESULTS, &start, &start_info,
                                        UI_UNATTENDED_NANOSECONDS, true) == VIEWER_UNATTENDED) {
        // The one line that comes after the summary, and only on this path: a detailed run
        // nobody attended says it left by itself, on both channels, so a script that forgot the
        // mode file finds out why its emulator took ten seconds longer to stop.
        ReportAppend(runner.report_path);
        ReportNote("detailed mode: nothing pressed for %u seconds after the report closed, "
                   "left by itself",
                   (unsigned)(UI_UNATTENDED_NANOSECONDS / 1000000000ull));
        ReportClose();
    }
    // A quick run the start screen chose has its screen still open.
    ViewerEndScreen();

    return ReportFailed() == 0 ? 0 : 1;
}
