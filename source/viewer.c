// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <switch.h>

#include "gpu.h"
#include "mode.h"
#include "musicplay.h"
#include "perf.h"
#include "picture.h"
#include "report.h"
#include "ui.h"
#include "viewer.h"

// The pad and the framebuffer are driven the way switchbrew's switch-examples drive them:
// hid/read-controls for padConfigureInput, padInitializeDefault, padUpdate, padGetButtonsDown
// and padGetButtons, graphics/simplegfx for a linear double buffered framebuffer and the loop
// around it. https://github.com/switchbrew/switch-examples, marked public domain (Creative
// Commons Public Domain Mark 1.0, in its README). The library calls are libnx's, ISC licence.

// The only place in this suite where anything reaches a screen. The render targets stay
// offscreen; what is drawn here is a picture of one of them, after the fact.
#define SCREEN_WIDTH 1280u
#define SCREEN_HEIGHT 720u

#define FRAME_NANOSECONDS 16000000ull

// Static, because the index is eight kilobytes of case numbers and built once.
static UiRun run;

// One framebuffer for the program, made by the first screen that needs it and closed by the last:
// under Eden, a graphics device made after a framebuffer was closed never finished its first
// submit, so the start screen's is kept open through the run and the viewer draws into it.
static Framebuffer screen;
static bool screen_open;

static bool OpenScreen(void) {
    if (screen_open) {
        return true;
    }
    if (R_FAILED(framebufferCreate(&screen, nwindowGetDefault(), SCREEN_WIDTH, SCREEN_HEIGHT,
                                   PIXEL_FORMAT_RGBA_8888, 2))) {
        return false;
    }
    // Linear, because every frame here is composed by the processor and this is what has the
    // library do the swizzle on the way out rather than the program doing it by hand.
    if (R_FAILED(framebufferMakeLinear(&screen))) {
        framebufferClose(&screen);
        return false;
    }
    screen_open = true;
    return true;
}

static void CloseScreen(void) {
    if (screen_open) {
        framebufferClose(&screen);
        screen_open = false;
    }
}

// Held around every frame drawn while a run is on and around what the running screen is drawn
// from: the run's thread, a demo's frames' thread and the running screen's keeper all draw into
// the one framebuffer.
static Mutex screen_lock;

// Whether the running screen is what the window shows: the last frame drawn into it was that
// screen's own. A demo's or a step's frames, the pad's screen and the listening screen take the
// window from it, and B is not the running screen's to read until it has drawn again.
static bool progress_shown;

/// Another screen's frame went to the window. The lock is held.
static void ProgressLostWindow(void) {
    progress_shown = false;
}

static void ReadPad(PadState *pad) {
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(pad);
    padUpdate(pad);
}

// The touch screen, read the way libnx's hid.h describes: activated once, then the latest state
// each frame. Only the count and the first finger's place go to ui.c, which decides the rest.
static bool touch_tried;
static bool touch_ready;
static uint32_t touch_result;

bool ViewerTouchInit(uint32_t *result) {
    // Once for the program, and guarded: libnx ends the program on a failed activation.
    if (!touch_tried) {
        touch_tried = true;
        touch_ready = AbortGuarded(hidInitializeTouchScreen, &touch_result);
    }
    if (result != NULL) {
        *result = touch_result;
    }
    return touch_ready;
}

static UiTouchEvent ReadTouch(UiTouch *touch, uint32_t frame_index, UiTap *tap) {
    HidTouchScreenState state;
    memset(&state, 0, sizeof(state));
    const bool read = ViewerTouchInit(NULL) && hidGetTouchScreenStates(&state, 1) > 0u &&
                      state.count > 0;
    return UiTouchStep(touch, read ? (uint32_t)state.count : 0u, read ? state.touches[0].x : 0u,
                       read ? state.touches[0].y : 0u, frame_index, tap);
}

RunMode ViewerModeWithPad(RunMode from_file) {
    if (!ModeAsksPad(&from_file)) {
        // Already detailed, and the button could only have asked for that again.
        return from_file;
    }

    // The other way in, for a person with a card that says quick. Held, not pressed: the
    // program is already running by the time anything can be pressed.
    PadState pad;
    ReadPad(&pad);
    for (uint32_t frame = 0; UiWaitsForPad(frame, padIsConnected(&pad)); ++frame) {
        svcSleepThread(FRAME_NANOSECONDS);
        padUpdate(&pad);
    }
    return ModeWithButton(from_file, (padGetButtons(&pad) & HidNpadButton_X) != 0u);
}

/// The start screen's buttons: up and down move the cursor, A takes the row under it, plus or
/// minus begins from any row, X turns the crash list's choice from any row. Y chooses nothing,
/// and is read only so an edge that holds it and X is known for a pad's whole mask.
static uint32_t StartButtonsFrom(uint64_t pressed) {
    static const struct {
        uint64_t console;
        uint32_t screen;
    } MAP[] = {
        { HidNpadButton_Plus | HidNpadButton_Minus, UI_START_BEGIN },
        { HidNpadButton_A, UI_START_SELECT },
        { HidNpadButton_Y, UI_START_Y },
        { HidNpadButton_X, UI_START_TOGGLE_RETRY },
        { HidNpadButton_AnyUp, UI_START_UP },
        { HidNpadButton_AnyDown, UI_START_DOWN },
        { HidNpadButton_L, UI_START_TAB_PREVIOUS },
        { HidNpadButton_R, UI_START_TAB_NEXT },
    };
    uint32_t buttons = 0;
    for (size_t i = 0; i < sizeof(MAP) / sizeof(MAP[0]); ++i) {
        if ((pressed & MAP[i].console) != 0u) {
            buttons |= MAP[i].screen;
        }
    }
    return buttons;
}

bool ViewerStartScreen(const UiStartInfo *info, const RunMode *from_file, UiStartState *chosen) {
    if (info == NULL || from_file == NULL || chosen == NULL) {
        return false;
    }
    UiStartState state = UiStartOpen(from_file, info->families_present);
    *chosen = state;
    if (!OpenScreen()) {
        return false;
    }
    PadState pad;
    ReadPad(&pad);
    UiTouch touch;
    UiTouchReset(&touch);
    const uint64_t opened = armGetSystemTick();
    bool pressed = false;
    uint32_t shown_left = UINT32_MAX;
    uint32_t frames_seen = 0;
    while (appletMainLoop() && !state.begun && state.tab_asked == 0u) {
        padUpdate(&pad);
        const uint64_t down = padGetButtonsDown(&pad);
        if (frames_seen >= UI_SETTLING_FRAMES && down != 0u) {
            pressed = true;
        }
        state = UiStartStep(state, StartButtonsFrom(down), frames_seen, info);
        UiTap tap;
        const UiTouchEvent touched = ReadTouch(&touch, frames_seen, &tap);
        pressed = pressed || touched != UI_TOUCH_NONE;
        if (touched == UI_TOUCH_TAP) {
            state = UiStartTap(state, &tap, SCREEN_WIDTH, SCREEN_HEIGHT, info);
        }
        if (frames_seen < UI_SETTLING_FRAMES) {
            ++frames_seen;
        }
        const uint64_t elapsed = armTicksToNs(armGetSystemTick() - opened);
        if (UiStartTimesOut(elapsed, pressed)) {
            break;
        }
        const uint32_t left = UiStartSecondsLeft(elapsed, pressed);
        if (state.changed || left != shown_left) {
            uint32_t stride = 0;
            UiFrame frame = { (uint8_t *)framebufferBegin(&screen, &stride), SCREEN_WIDTH,
                              SCREEN_HEIGHT, stride };
            UiComposeStart(&frame, info, state, left);
            framebufferEnd(&screen);
            state.changed = false;
            shown_left = left;
        }
        svcSleepThread(FRAME_NANOSECONDS);
    }
    // Left open through the run, for the viewer to draw the results into: the suites draw
    // offscreen, and the window keeps the last frame shown here until then.
    *chosen = state;
    return true;
}

/// The resume screen's buttons: up and down move, A confirms.
static uint32_t ResumeButtonsFrom(uint64_t pressed) {
    uint32_t buttons = 0;
    if ((pressed & HidNpadButton_AnyUp) != 0u) {
        buttons |= UI_RESUME_UP;
    }
    if ((pressed & HidNpadButton_AnyDown) != 0u) {
        buttons |= UI_RESUME_DOWN;
    }
    if ((pressed & HidNpadButton_A) != 0u) {
        buttons |= UI_RESUME_CHOOSE;
    }
    return buttons;
}

bool ViewerResumeScreen(const UiResumeInfo *info, bool *start_over) {
    if (start_over != NULL) {
        *start_over = false;
    }
    if (info == NULL || start_over == NULL || !OpenScreen()) {
        return false;
    }
    PadState pad;
    ReadPad(&pad);
    UiTouch touch;
    UiTouchReset(&touch);
    UiResumeState state = UiResumeOpen();
    const uint64_t opened = armGetSystemTick();
    bool pressed = false;
    uint32_t shown_left = UINT32_MAX;
    uint32_t frames_seen = 0;
    while (appletMainLoop() && !state.decided) {
        padUpdate(&pad);
        const uint64_t down = padGetButtonsDown(&pad);
        if (frames_seen >= UI_SETTLING_FRAMES && down != 0u) {
            pressed = true;
        }
        state = UiResumeStep(state, ResumeButtonsFrom(down), frames_seen);
        UiTap tap;
        const UiTouchEvent touched = ReadTouch(&touch, frames_seen, &tap);
        pressed = pressed || touched != UI_TOUCH_NONE;
        if (touched == UI_TOUCH_TAP) {
            state = UiResumeTap(state, &tap, SCREEN_WIDTH, SCREEN_HEIGHT);
        }
        if (frames_seen < UI_SETTLING_FRAMES) {
            ++frames_seen;
        }
        const uint64_t elapsed = armTicksToNs(armGetSystemTick() - opened);
        if (UiResumeTimesOut(elapsed, pressed)) {
            break;
        }
        const uint32_t left = UiResumeSecondsLeft(elapsed, pressed);
        if (state.changed || left != shown_left) {
            uint32_t stride = 0;
            UiFrame frame = { (uint8_t *)framebufferBegin(&screen, &stride), SCREEN_WIDTH,
                              SCREEN_HEIGHT, stride };
            UiComposeResume(&frame, info, state, left);
            framebufferEnd(&screen);
            state.changed = false;
            shown_left = left;
        }
        svcSleepThread(FRAME_NANOSECONDS);
    }
    // Left open, as the start screen leaves it, for the screens after it.
    *start_over = state.decided && state.start_over;
    return true;
}

/// The browser's buttons from the console's: the directions, A open, B back, X mark (and the
/// results' view), Y again, plus run, minus quit, L and R the tabs, ZL and ZR the failures.
static uint32_t BrowseButtonsFrom(uint64_t pressed) {
    static const struct {
        uint64_t console;
        uint32_t screen;
    } MAP[] = {
        { HidNpadButton_Minus, UI_BUTTON_QUIT },
        { HidNpadButton_Plus, UI_BUTTON_RUN },
        { HidNpadButton_B, UI_BUTTON_BACK },
        { HidNpadButton_A, UI_BUTTON_OPEN },
        { HidNpadButton_X, UI_BUTTON_CYCLE_VIEW },
        { HidNpadButton_Y, UI_BUTTON_AGAIN },
        { HidNpadButton_L, UI_BUTTON_TAB_PREVIOUS },
        { HidNpadButton_R, UI_BUTTON_TAB_NEXT },
        { HidNpadButton_ZR, UI_BUTTON_NEXT_FAILURE },
        { HidNpadButton_ZL, UI_BUTTON_PREVIOUS_FAILURE },
        { HidNpadButton_AnyUp, UI_BUTTON_UP },
        { HidNpadButton_AnyDown, UI_BUTTON_DOWN },
        { HidNpadButton_AnyLeft, UI_BUTTON_LEFT },
        { HidNpadButton_AnyRight, UI_BUTTON_RIGHT },
    };
    uint32_t buttons = 0;
    for (size_t i = 0; i < sizeof(MAP) / sizeof(MAP[0]); ++i) {
        if ((pressed & MAP[i].console) != 0u) {
            buttons |= MAP[i].screen;
        }
    }
    return buttons;
}

// The motion option's two frames: the screen before and the one after, made when the browser
// opens with the option on and freed when it leaves. Without them a screen comes in at once.
static uint8_t *motion_before;
static uint8_t *motion_after;

#define FRAME_BYTES ((size_t)SCREEN_WIDTH * SCREEN_HEIGHT * 4u)

/// Copies a composed frame of @p from into the screen's buffer of @p stride.
static void CopyFrame(uint8_t *out, uint32_t stride, const uint8_t *from, uint32_t rows) {
    for (uint32_t y = 0; y < rows && y < SCREEN_HEIGHT; ++y) {
        memcpy(out + (size_t)y * stride, from + (size_t)y * SCREEN_WIDTH * 4u,
               SCREEN_WIDTH * 4u);
    }
}

/// Draws the browser: into the screen when there is no motion, else the new screen over the old
/// one from the top over UI_MOTION_STEPS frames, when the tab or the level changed.
static void DrawBrowse(const UiBrowseInfo *info, const UiBrowseState *state, bool moved) {
    if (motion_after != NULL && motion_before != NULL) {
        const UiFrame frame = { motion_after, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_WIDTH * 4u };
        UiComposeBrowse(&frame, info, state);
        const uint32_t steps = moved && state->options.motion ? UI_MOTION_STEPS : 1u;
        for (uint32_t step = steps == 1u ? UI_MOTION_STEPS : 0u; step <= UI_MOTION_STEPS;
             ++step) {
            uint32_t stride = 0;
            uint8_t *const out = (uint8_t *)framebufferBegin(&screen, &stride);
            if (out != NULL && stride >= SCREEN_WIDTH * 4u) {
                const uint32_t rows = UiMotionRows(step, SCREEN_HEIGHT);
                CopyFrame(out, stride, motion_after, rows);
                for (uint32_t y = rows; y < SCREEN_HEIGHT; ++y) {
                    memcpy(out + (size_t)y * stride,
                           motion_before + (size_t)y * SCREEN_WIDTH * 4u, SCREEN_WIDTH * 4u);
                }
            }
            framebufferEnd(&screen);
            if (step < UI_MOTION_STEPS) {
                svcSleepThread(FRAME_NANOSECONDS);
            }
        }
        uint8_t *const swap = motion_before;
        motion_before = motion_after;
        motion_after = swap;
        return;
    }
    uint32_t stride = 0;
    uint8_t *const out = (uint8_t *)framebufferBegin(&screen, &stride);
    if (out != NULL && stride >= SCREEN_WIDTH * 4u) {
        const UiFrame frame = { out, SCREEN_WIDTH, SCREEN_HEIGHT, stride };
        UiComposeBrowse(&frame, info, state);
    }
    framebufferEnd(&screen);
}

ViewerEnd ViewerBrowse(UiBrowseInfo *info, UiBrowseState *state, uint64_t unattended_ns) {
    if (info == NULL || state == NULL || !OpenScreen()) {
        return VIEWER_NO_SCREEN;
    }
    if (state->options.motion && motion_before == NULL) {
        motion_before = (uint8_t *)calloc(1, FRAME_BYTES);
        motion_after = (uint8_t *)calloc(1, FRAME_BYTES);
    }
    PadState pad;
    ReadPad(&pad);
    UiTouch touch;
    UiTouchReset(&touch);
    UiRepeat repeat = { 0, 0 };
    const uint64_t opened = armGetSystemTick();
    uint32_t frames_seen = 0;
    uint32_t shown_left = UINT32_MAX;
    UiTab shown_tab = UI_TABS;
    uint32_t shown_level = UINT32_MAX;
    state->changed = true;
    while (appletMainLoop()) {
        padUpdate(&pad);
        const uint32_t buttons =
            UiRepeatButtons(&repeat, BrowseButtonsFrom(padGetButtons(&pad)),
                            BrowseButtonsFrom(padGetButtonsDown(&pad)), frames_seen);
        *state = UiBrowseStep(*state, buttons, frames_seen, info);
        UiTap tap;
        const UiTouchEvent touched = ReadTouch(&touch, frames_seen, &tap);
        if (touched == UI_TOUCH_BEGAN) {
            state->pressed = true;
        } else if (touched == UI_TOUCH_TAP && state->ask == UI_ASK_NONE) {
            *state = UiBrowseTap(*state, &tap, SCREEN_WIDTH, SCREEN_HEIGHT, info);
        }
        if (frames_seen < UI_SETTLING_FRAMES) {
            ++frames_seen;
        }
        info->seconds_left = 0u;
        if (unattended_ns > 0u && !state->pressed) {
            const uint64_t elapsed = armTicksToNs(armGetSystemTick() - opened);
            if (elapsed >= unattended_ns) {
                return VIEWER_UNATTENDED;
            }
            info->seconds_left =
                (uint32_t)((unattended_ns - elapsed + 999999999ull) / 1000000000ull);
        }
        if (state->ask == UI_ASK_QUIT) {
            return VIEWER_CLOSED;
        }
        if (state->ask != UI_ASK_NONE) {
            return VIEWER_ASKED;
        }
        if (state->changed || info->seconds_left != shown_left) {
            const bool moved = state->tab != shown_tab || state->level != shown_level;
            DrawBrowse(info, state, moved && shown_tab != UI_TABS);
            state->changed = false;
            shown_left = info->seconds_left;
            shown_tab = state->tab;
            shown_level = state->level;
        }
        svcSleepThread(FRAME_NANOSECONDS);
    }
    return VIEWER_CLOSED;
}

void ViewerBrowseClose(void) {
    free(motion_before);
    free(motion_after);
    motion_before = NULL;
    motion_after = NULL;
}

void ViewerEndScreen(void) {
    CloseScreen();
}

bool ViewerScreenUp(void) {
    return screen_open;
}

const UiRun *ViewerIndexRun(const char *heading) {
    // What the run recorded, and nothing else: the cases as their lines went out, and the count
    // the report's own summary carries, so a store that ran out is said to have.
    size_t count = 0;
    const ReportRecord *const records = ReportRecords(&count);
    UiRunIndex(&run, records, (uint32_t)count,
               (uint32_t)(ReportPassed() + ReportFailed() + ReportSkipped() + ReportCrashed()));
    run.heading = heading;
    return &run;
}

/// @p pixels, @p width by @p height, scaled up by whole steps to fit and centred on the screen's
/// buffer @p out.
static void DrawImage(uint8_t *out, uint32_t stride, const uint8_t *pixels, uint32_t width,
                      uint32_t height) {
    const uint32_t across = SCREEN_WIDTH / width;
    const uint32_t down = SCREEN_HEIGHT / height;
    const uint32_t scale = across < down ? across : down;
    const uint32_t left = (SCREEN_WIDTH - width * scale) / 2u;
    const uint32_t top = (SCREEN_HEIGHT - height * scale) / 2u;
    static const uint8_t BACKGROUND[4] = { 0x0F, 0x0F, 0x14, 0xFF };
    for (uint32_t y = 0; y < SCREEN_HEIGHT; ++y) {
        uint8_t *const row = out + (size_t)y * stride;
        const bool inside_y = y >= top && y < top + height * scale;
        if (inside_y && scale == 1u && width == SCREEN_WIDTH) {
            memcpy(row, pixels + (size_t)(y - top) * width * 4u, (size_t)width * 4u);
            continue;
        }
        for (uint32_t x = 0; x < SCREEN_WIDTH; ++x) {
            const bool inside = inside_y && x >= left && x < left + width * scale;
            const size_t at = (size_t)((y - top) / scale) * width + (x - left) / scale;
            const uint8_t *const from = inside ? pixels + at * 4u : BACKGROUND;
            memcpy(row + (size_t)x * 4u, from, 4u);
        }
    }
}

// The demo running, when the browser runs one: its HUD and the pause menu over its frames, both
// drawn and read on the frames' own thread as each frame is shown. Paused, the demo asks
// paused() and shows its last frame again until it says no (demo.h), and the menu is drawn over
// that frame and read here.
static struct {
    bool on;
    ViewerDemo asked;
    UiHud hud;
    bool hud_on;
    bool music;
    PadState pad;
    uint32_t frames_seen;
    uint64_t last;
    bool paused;
    bool stopped;
    UiPauseInfo pause_info;
    UiPauseState pause;
    ViewerDemoEnd end;
    /// The sticks as the last frame shown found them, read by the demo's own thread.
    DemoSticks sticks;
} demo;

void ViewerDemoBegin(const ViewerDemo *asked) {
    memset(&demo, 0, sizeof(demo));
    if (asked == NULL) {
        return;
    }
    demo.on = true;
    demo.asked = *asked;
    demo.hud.name = asked->name;
    demo.hud.track = asked->music ? asked->track : NULL;
    demo.hud.frames = asked->frames;
    demo.hud_on = asked->hud;
    demo.music = asked->music;
    ReadPad(&demo.pad);
    demo.end.next = UINT32_MAX;
    demo.end.frames = asked->frames;
    demo.end.music = asked->music;
    demo.end.hud = asked->hud;
    memcpy(demo.end.option, asked->option_now, sizeof(demo.end.option));
}

ViewerDemoEnd ViewerDemoFinish(void) {
    const ViewerDemoEnd end = demo.end;
    demo.on = false;
    return end;
}

bool ViewerDemoStopped(void) {
    return __atomic_load_n(&demo.stopped, __ATOMIC_ACQUIRE);
}

bool ViewerDemoPaused(void) {
    return __atomic_load_n(&demo.paused, __ATOMIC_ACQUIRE);
}

bool ViewerDemoMusic(void) {
    return __atomic_load_n(&demo.music, __ATOMIC_ACQUIRE);
}

void ViewerDemoSticks(DemoSticks *out) {
    for (uint32_t k = 0; k < 2u; ++k) {
        out->left[k] = __atomic_load_n(&demo.sticks.left[k], __ATOMIC_RELAXED);
        out->right[k] = __atomic_load_n(&demo.sticks.right[k], __ATOMIC_RELAXED);
    }
}

/// The music as the pause menu left it: the demo's track, or none, through the browser's player;
/// a demo that streams its own (the game scene) switches it on its own thread. The choice holds
/// for the demos after it.
static void DemoMusicTo(bool on) {
    if (on == demo.music) {
        return;
    }
    char error[160];
    if (demo.asked.streams) {
        // The demo's own thread switches its stream when it sees the change.
    } else if (!on) {
        MusicPlayStop();
    } else if (demo.asked.track != NULL &&
               !(MusicPlayOn() ? MusicPlaySwitch(demo.asked.track, error, sizeof(error))
                               : MusicPlayStart(demo.asked.track, error, sizeof(error)))) {
        ReportNote("browser: the music's track %s did not play for %s: %s", demo.asked.track,
                   demo.asked.name, error);
    }
    __atomic_store_n(&demo.music, on, __ATOMIC_RELEASE);
    demo.hud.track = on ? demo.asked.track : NULL;
}

/// One look at the pad while the menu is up, the frame shown again under it: its rows moved and
/// chosen. Leaving it, by plus, resume, back or a level, ends the pause.
static void DemoPauseStep(uint32_t buttons) {
    demo.pause = UiPauseStep(demo.pause, buttons, &demo.pause_info);
    if (!demo.pause.resume && !demo.pause.back && !demo.pause.start_level) {
        return;
    }
    demo.hud_on = demo.pause.hud;
    DemoMusicTo(demo.pause.music);
    demo.end.hud = demo.pause.hud;
    demo.end.music = demo.pause.music;
    demo.end.frames = demo.pause.frames;
    memcpy(demo.end.option, demo.pause.option, sizeof(demo.end.option));
    if (demo.pause.back) {
        demo.end.back = true;
        __atomic_store_n(&demo.stopped, true, __ATOMIC_RELEASE);
    } else if (demo.pause.start_level) {
        demo.end.next = demo.pause.level;
        __atomic_store_n(&demo.stopped, true, __ATOMIC_RELEASE);
    }
    __atomic_store_n(&demo.paused, false, __ATOMIC_RELEASE);
}

bool ViewerShowImage(const uint8_t *pixels, uint32_t width, uint32_t height) {
    if (!screen_open || pixels == NULL || width == 0u || height == 0u || width > SCREEN_WIDTH ||
        height > SCREEN_HEIGHT) {
        return false;
    }
    // The pad first, so the frame drawn says what it asked: the menu up, or gone.
    uint32_t buttons = 0;
    if (demo.on) {
        padUpdate(&demo.pad);
        buttons = demo.frames_seen >= UI_SETTLING_FRAMES
                      ? BrowseButtonsFrom(padGetButtonsDown(&demo.pad))
                      : 0u;
        demo.frames_seen += demo.frames_seen < UI_SETTLING_FRAMES ? 1u : 0u;
        const HidAnalogStickState left = padGetStickPos(&demo.pad, 0);
        const HidAnalogStickState right = padGetStickPos(&demo.pad, 1);
        __atomic_store_n(&demo.sticks.left[0], demo.paused ? 0 : left.x, __ATOMIC_RELAXED);
        __atomic_store_n(&demo.sticks.left[1], demo.paused ? 0 : left.y, __ATOMIC_RELAXED);
        __atomic_store_n(&demo.sticks.right[0], demo.paused ? 0 : right.x, __ATOMIC_RELAXED);
        __atomic_store_n(&demo.sticks.right[1], demo.paused ? 0 : right.y, __ATOMIC_RELAXED);
        if (demo.paused) {
            DemoPauseStep(buttons);
        } else if ((buttons & UI_BUTTON_BACK) != 0u) {
            __atomic_store_n(&demo.stopped, true, __ATOMIC_RELEASE);
        } else if ((buttons & UI_BUTTON_AGAIN) != 0u) {
            demo.hud_on = !demo.hud_on;
            demo.end.hud = demo.hud_on;
        } else if ((buttons & UI_BUTTON_RUN) != 0u) {
            memset(&demo.pause_info, 0, sizeof(demo.pause_info));
            demo.pause_info.name = demo.asked.name;
            demo.pause_info.levels = demo.asked.levels;
            demo.pause_info.level_count = demo.asked.level_count;
            demo.pause_info.running = demo.asked.running;
            demo.pause_info.frame = demo.hud.frame;
            demo.pause_info.frames = demo.asked.frames;
            demo.pause_info.options = demo.asked.options;
            demo.pause_info.option_count = demo.asked.option_count;
            memcpy(demo.pause_info.option_now, demo.end.option, sizeof(demo.end.option));
            demo.pause = UiPauseOpen(&demo.pause_info, demo.asked.frames, demo.music, demo.hud_on);
            __atomic_store_n(&demo.paused, true, __ATOMIC_RELEASE);
        }
    }
    mutexLock(&screen_lock);
    uint32_t stride = 0;
    uint8_t *const out = (uint8_t *)framebufferBegin(&screen, &stride);
    if (out == NULL) {
        mutexUnlock(&screen_lock);
        return false;
    }
    if (stride >= SCREEN_WIDTH * 4u) {
        DrawImage(out, stride, pixels, width, height);
        const UiFrame frame = { out, SCREEN_WIDTH, SCREEN_HEIGHT, stride };
        if (demo.on && demo.paused) {
            UiComposePause(&frame, &demo.pause_info, &demo.pause);
        } else if (demo.on && demo.hud_on) {
            UiComposeHud(&frame, &demo.hud);
        }
    }
    framebufferEnd(&screen);
    ProgressLostWindow();
    mutexUnlock(&screen_lock);
    if (!demo.on || demo.paused) {
        // A frame shown again under the menu is no new frame, and its time no frame's.
        demo.last = 0u;
        return true;
    }
    // The HUD's numbers for the next frame: this one's time from the one before, its share of a
    // second, and the times the graph draws, oldest first.
    const uint64_t now = armGetSystemTick();
    ++demo.hud.frame;
    if (demo.last != 0u) {
        const uint32_t us = (uint32_t)(armTicksToNs(now - demo.last) / 1000u);
        demo.hud.frame_us = us;
        demo.hud.fps_tenths = us > 0u ? (uint32_t)(10000000u / us) : 0u;
        if (demo.hud.time_count == UI_HUD_TIMES) {
            memmove(demo.hud.times, demo.hud.times + 1, (UI_HUD_TIMES - 1u) * sizeof(uint32_t));
            --demo.hud.time_count;
        }
        demo.hud.times[demo.hud.time_count++] = us;
    }
    demo.last = now;
    return true;
}


// The running screen, drawn into the program's one framebuffer while the run is on: by the run's
// own thread between cases and, inside a case that lasts, by its keeper, a thread beside the run.
// Nothing here is read by a case: the counts are the report's, the pass is the timer's.
static bool progress_on;
static UiProgress progress;
// The pad the running screen reads for B, which cancels a run a person attends.
static PadState progress_pad;
static bool progress_pad_open;
static UiProgressCancel progress_cancel;
// The report was told of the cancel: only the run's thread writes to the report.
static bool progress_cancel_told;
static UiProgressCount progress_count;
static uint64_t progress_began;
static uint64_t progress_drawn;
static bool progress_drawn_once;
static char progress_suite[REPORT_KEPT_SUITE];
static char progress_name[REPORT_KEPT_NAME];

// The keeper: its thread, when the last frame it drew was, and the word that ends it.
static Thread keeper_thread;
static bool keeper_on;
static bool keeper_stop;
static uint64_t keeper_drawn;

#define KEEPER_STACK 0x8000
// The lowest a program's thread takes on the console, under every thread a case makes: the
// keeper runs on a core nothing else wants and never takes one from a case.
#define KEEPER_PRIORITY 0x3B

// The listening sequence's screen and the pad screen are each up from their first frame to their
// close, and the running screen keeps off them; declared here, set below.
static bool listen_open;
static bool pad_open;

/// One look at B while the running screen has the window, by either thread: whether it is the
/// press that cancels, which the screen then says. The lock is held.
static bool ProgressLookAtB(void) {
    if (!progress_pad_open || progress.quick || progress.cancelling) {
        return false;
    }
    // Held, not newly pressed: a look is one moment, and the press may be older than it.
    padUpdate(&progress_pad);
    if (!UiProgressCancelLook(&progress_cancel,
                              (padGetButtons(&progress_pad) & HidNpadButton_B) != 0u)) {
        return false;
    }
    progress.cancelling = true;
    return true;
}

/// The running screen as it is now, into the framebuffer. The lock is held.
static bool ProgressCompose(uint64_t now) {
    progress.elapsed_ns = armTicksToNs(now - progress_began);
    uint32_t stride = 0;
    uint8_t *const pixels = (uint8_t *)framebufferBegin(&screen, &stride);
    if (pixels == NULL) {
        return false;
    }
    if (stride >= SCREEN_WIDTH * 4u) {
        const UiFrame frame = { pixels, SCREEN_WIDTH, SCREEN_HEIGHT, stride };
        UiComposeProgress(&frame, &progress);
    }
    framebufferEnd(&screen);
    progress_shown = true;
    return true;
}

/// The run's own thread at a case's start or end, or a step's: B looked at, the counts taken
/// from the report and the timer, which only this thread asks, and the frame drawn when one is
/// due.
static void ProgressDraw(bool step_began) {
    mutexLock(&screen_lock);
    if (!progress_on || !screen_open || listen_open || pad_open) {
        mutexUnlock(&screen_lock);
        return;
    }
    if (!progress_shown) {
        UiProgressCancelReset(&progress_cancel);
    } else {
        (void)ProgressLookAtB();
    }
    // The keeper's look may have seen the press: the report hears of it here, from this thread.
    const bool tell = progress.cancelling && !progress_cancel_told;
    progress_cancel_told = progress.cancelling;
    progress.passed = (uint32_t)ReportPassed();
    progress.failed = (uint32_t)ReportFailed();
    progress.skipped = (uint32_t)ReportSkipped();
    progress.crashed = (uint32_t)ReportCrashed();
    progress.pass = progress.passes > 0u ? PerfPassNumber() : 0u;
    progress.case_number = progress_count.count;
    // How many the pass before met is how many this one will, from the second pass on.
    progress.case_total = progress.pass > 1u ? progress_count.previous : 0u;
    const uint64_t now = armGetSystemTick();
    if (UiProgressDue(progress_drawn_once, armTicksToNs(progress_drawn), armTicksToNs(now),
                      step_began || tell) &&
        ProgressCompose(now)) {
        progress_drawn = now;
        progress_drawn_once = true;
    }
    mutexUnlock(&screen_lock);
    if (tell) {
        ReportCancel();
    }
}

/// The keeper's thread: a look at B every UI_PROGRESS_LOOK_NANOSECONDS and a frame when the run's
/// thread has drawn none for a while, only while the running screen has the window. It asks the
/// report and the timer nothing: what it draws is what the run's thread left at its last case.
static void KeeperMain(void *argument) {
    (void)argument;
    for (;;) {
        svcSleepThread((s64)UI_PROGRESS_LOOK_NANOSECONDS);
        mutexLock(&screen_lock);
        const bool stop = keeper_stop;
        if (!stop && progress_on && progress_shown) {
            const bool asked = ProgressLookAtB();
            const uint64_t now = armGetSystemTick();
            if (UiProgressKeeperDraws(armTicksToNs(progress_drawn), armTicksToNs(keeper_drawn),
                                      armTicksToNs(now), asked, progress.passes > 0u) &&
                ProgressCompose(now)) {
                keeper_drawn = now;
            }
        }
        mutexUnlock(&screen_lock);
        if (stop) {
            return;
        }
    }
}

/// Starts the keeper on a core the run's thread is not on: a case that holds the run's core for
/// minutes is what it is for. A machine that gives no thread keeps the screen as it was, drawn
/// between cases, and the report says so.
static void KeeperStart(void) {
    if (keeper_on) {
        return;
    }
    keeper_stop = false;
    keeper_drawn = 0u;
    const int core = svcGetCurrentProcessorNumber() == 0 ? 1 : 0;
    Result rc = threadCreate(&keeper_thread, KeeperMain, NULL, NULL, KEEPER_STACK,
                             KEEPER_PRIORITY, core);
    if (R_SUCCEEDED(rc)) {
        rc = threadStart(&keeper_thread);
        if (R_FAILED(rc)) {
            threadClose(&keeper_thread);
        }
    }
    if (R_FAILED(rc)) {
        ReportNote("running screen: the thread that draws it inside a long case did not start "
                   "on core %d at priority %d, result 0x%x: the screen is drawn between cases "
                   "only, and a long case leaves it still",
                   core, KEEPER_PRIORITY, (unsigned)rc);
        return;
    }
    keeper_on = true;
}

static void KeeperStop(void) {
    if (!keeper_on) {
        return;
    }
    mutexLock(&screen_lock);
    keeper_stop = true;
    mutexUnlock(&screen_lock);
    threadWaitForExit(&keeper_thread);
    threadClose(&keeper_thread);
    keeper_on = false;
}

static void ProgressWatch(const char *suite, const char *name, bool started) {
    mutexLock(&screen_lock);
    UiProgressCountCase(&progress_count, progress.passes > 0u ? PerfPassNumber() : 0u, started);
    snprintf(progress_suite, sizeof(progress_suite), "%s", suite != NULL ? suite : "");
    snprintf(progress_name, sizeof(progress_name), "%s", name != NULL ? name : "");
    progress.suite = progress_suite;
    progress.name = progress_name;
    mutexUnlock(&screen_lock);
    ProgressDraw(false);
}

void ViewerProgressBegin(const char *heading, const char *folder, uint32_t steps, bool quick,
                         bool open) {
    if (!screen_open && !(open && OpenScreen())) {
        return;
    }
    mutexLock(&screen_lock);
    memset(&progress, 0, sizeof(progress));
    if (!quick) {
        ReadPad(&progress_pad);
        progress_pad_open = true;
    }
    progress.heading = heading;
    progress.folder = folder;
    progress.steps = steps;
    progress.quick = quick;
    UiProgressCountReset(&progress_count);
    progress_cancel_told = false;
    progress_began = armGetSystemTick();
    progress_drawn_once = false;
    ProgressLostWindow();
    progress_on = true;
    mutexUnlock(&screen_lock);
    ReportSetWatcher(ProgressWatch);
    KeeperStart();
}

void ViewerProgressStep(uint32_t step, const char *name, bool timed) {
    mutexLock(&screen_lock);
    if (!progress_on) {
        mutexUnlock(&screen_lock);
        return;
    }
    progress.step = step;
    progress.step_name = name;
    progress.passes = timed ? PERF_WARMUPS + PERF_REHEARSALS + 1u : 0u;
    progress.suite = NULL;
    progress.name = NULL;
    UiProgressCountReset(&progress_count);
    mutexUnlock(&screen_lock);
    ProgressDraw(true);
}

void ViewerProgressEnd(void) {
    ReportSetWatcher(NULL);
    KeeperStop();
    mutexLock(&screen_lock);
    progress_on = false;
    // A press the keeper saw in the run's last case, with no case after it to tell the report:
    // the browser goes back to the start for it, as for any other cancel.
    const bool tell = progress.cancelling && !progress_cancel_told;
    progress_cancel_told = progress.cancelling;
    mutexUnlock(&screen_lock);
    if (tell) {
        ReportCancel();
    }
}

// The listening sequence's screen, up during the sequence to name what is playing; it is not
// an input to a verdict. It draws into the program's one framebuffer and leaves it as it found
// it: open when the start screen opened it, for the results, and closed otherwise.
static bool listen_tried;
static bool listen_opened_screen;
static PadState listen_pad;
static bool listen_pad_ready;
static uint32_t listen_frames;
static UiListenScreen listen_screen;
static double listen_left;
static double listen_right;
static bool listen_drawn;

static bool ListenReady(void) {
    if (listen_tried) {
        return listen_open;
    }
    listen_tried = true;
    const bool was_open = screen_open;
    if (!OpenScreen()) {
        return false;
    }
    listen_opened_screen = !was_open;
    listen_open = true;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&listen_pad);
    listen_pad_ready = true;
    return true;
}

void ViewerListenShow(UiListenScreen shown, double left, double right) {
    if (!ListenReady()) {
        return;
    }
    if (listen_drawn && shown == listen_screen && left == listen_left && right == listen_right) {
        return;
    }
    mutexLock(&screen_lock);
    uint32_t stride = 0;
    uint8_t *const pixels = (uint8_t *)framebufferBegin(&screen, &stride);
    if (pixels == NULL || stride < SCREEN_WIDTH * 4u) {
        if (pixels != NULL) {
            framebufferEnd(&screen);
        }
        mutexUnlock(&screen_lock);
        return;
    }
    const UiFrame frame = { pixels, SCREEN_WIDTH, SCREEN_HEIGHT, stride };
    UiComposeListen(&frame, shown, left, right);
    framebufferEnd(&screen);
    ProgressLostWindow();
    mutexUnlock(&screen_lock);
    listen_screen = shown;
    listen_left = left;
    listen_right = right;
    listen_drawn = true;
}

ViewerListenAction ViewerListenPoll(void) {
    if (!listen_pad_ready) {
        return VIEWER_LISTEN_NONE;
    }
    if (!appletMainLoop()) {
        return VIEWER_LISTEN_CLOSED;
    }
    padUpdate(&listen_pad);
    const uint64_t down = padGetButtonsDown(&listen_pad);
    // Y and X only. Plus, minus and the results screens' buttons are not in this mask, so
    // none of them can quit the sequence or move a list that is not up.
    uint32_t buttons = 0;
    if ((down & HidNpadButton_Y) != 0u) {
        buttons |= UI_BUTTON_LISTEN_REPEAT;
    }
    if ((down & HidNpadButton_X) != 0u) {
        buttons |= UI_BUTTON_LISTEN_SKIP;
    }
    const uint32_t frame = listen_frames;
    if (listen_frames < UI_SETTLING_FRAMES) {
        ++listen_frames;
    }
    const UiListenInput input = UiListenInputFromButtons(buttons, frame);
    if (input == UI_LISTEN_REPEAT) {
        return VIEWER_LISTEN_REPEAT;
    }
    if (input == UI_LISTEN_SKIP) {
        return VIEWER_LISTEN_SKIP;
    }
    return VIEWER_LISTEN_NONE;
}

void ViewerListenClose(void) {
    if (listen_open && listen_opened_screen) {
        CloseScreen();
    }
    listen_open = false;
    listen_tried = true;
    listen_pad_ready = false;
    listen_drawn = false;
}

// The pad family's screen, drawn by its suite; it leaves the framebuffer as it found it, as the
// listening screen does.
static bool pad_opened_screen;

bool ViewerPadShow(const UiPadScreen *shown) {
    if (shown == NULL) {
        return false;
    }
    if (!pad_open) {
        const bool was_open = screen_open;
        if (!OpenScreen()) {
            return false;
        }
        pad_opened_screen = !was_open;
        pad_open = true;
    }
    mutexLock(&screen_lock);
    uint32_t stride = 0;
    uint8_t *const pixels = (uint8_t *)framebufferBegin(&screen, &stride);
    if (pixels == NULL) {
        mutexUnlock(&screen_lock);
        return false;
    }
    if (stride >= SCREEN_WIDTH * 4u) {
        const UiFrame frame = { pixels, SCREEN_WIDTH, SCREEN_HEIGHT, stride };
        UiComposePad(&frame, shown);
    }
    framebufferEnd(&screen);
    ProgressLostWindow();
    mutexUnlock(&screen_lock);
    return true;
}

bool ViewerTouchShow(const UiTouchScreen *shown) {
    if (shown == NULL) {
        return false;
    }
    if (!pad_open) {
        const bool was_open = screen_open;
        if (!OpenScreen()) {
            return false;
        }
        pad_opened_screen = !was_open;
        pad_open = true;
    }
    mutexLock(&screen_lock);
    uint32_t stride = 0;
    uint8_t *const pixels = (uint8_t *)framebufferBegin(&screen, &stride);
    if (pixels == NULL) {
        mutexUnlock(&screen_lock);
        return false;
    }
    if (stride >= SCREEN_WIDTH * 4u) {
        const UiFrame frame = { pixels, SCREEN_WIDTH, SCREEN_HEIGHT, stride };
        UiComposeTouch(&frame, shown);
    }
    framebufferEnd(&screen);
    ProgressLostWindow();
    mutexUnlock(&screen_lock);
    return true;
}

void ViewerPadClose(void) {
    if (pad_open && pad_opened_screen) {
        CloseScreen();
    }
    pad_open = false;
    pad_opened_screen = false;
}

// A yes or no question on the pad screen, answered on its touch bar.
static UiTouch pad_touch;

void ViewerPadTapBegin(void) {
    UiTouchReset(&pad_touch);
}

PadAnswer ViewerPadTap(void) {
    UiTap tap;
    // The screen settled long before a question: what keeps an old finger out is the reset.
    if (ReadTouch(&pad_touch, UI_SETTLING_FRAMES, &tap) != UI_TOUCH_TAP) {
        return PAD_ANSWER_NONE;
    }
    return UiPadTap(&tap, SCREEN_WIDTH, SCREEN_HEIGHT, true);
}
