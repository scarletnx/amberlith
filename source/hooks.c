// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <string.h>
#include <sys/iosupport.h>
#include <switch.h>

#include "check.h"
#include "hooks.h"
#include "mode.h"
#include "platform.h"
#include "report.h"
#include "runner.h"
#include "sdk.h"
#include "sdk_card.h"

// The program's start in either mode (hooks.h); in hooks mode the C library's sdmc: device is
// nn::fs's (sdk_card.h).

// libnx's start, which the program's __appInit below takes the place of: its own functions for the
// time zone and the working directory, the window's and the application's hooks when linked, and
// the applet type its start reads.
void __libnx_init_time(void);
void __libnx_init_cwd(void);
void __nx_win_init(void) __attribute__((weak));
void __nx_win_exit(void) __attribute__((weak));
void userAppInit(void) __attribute__((weak));
void userAppExit(void) __attribute__((weak));
extern AppletType __nx_applet_type;

/// The nn::fs mount name the memory card has in hooks mode.
#define CARD SDK_CARD

static bool hooks_on;
/// What the start saw: whether the program's own code made the card's mount when the mode file
/// was read, and in hooks mode whether it answered nn::time::Initialize.
static bool card_own;
static bool time_own;
static uint32_t card_rc;
static uint32_t time_rc;

bool HooksOn(void) {
    return hooks_on;
}

// ------------------------------------------------------------------------- the mode file first

/// Whether @p folder "/" @p name is on the card, through nn::fs.
static bool CardHolds(const char *folder, const char *name) {
    char path[64];
    int32_t type = -1;
    snprintf(path, sizeof(path), "%s/%s", folder, name);
    return R_SUCCEEDED(NnFsGetEntryType(&type, path));
}

/// Whether the mode file, read through nn::fs from the card mounted as CARD, has the word `hooks`,
/// as RunnerModeFromFile reads it: a folder is no file, and past MODE_FILE_MAX nothing is read. The
/// mode file is the one of the folder the runner will run from (RunnerOldRoot).
static const char *mode_path = PROBE_DIRECTORY "/mode";

static bool ModeAsksHooks(void) {
    static char text[MODE_FILE_MAX + 2u];
    const RunnerRootChoice choice =
        RunnerOldRoot(CardHolds(PROBE_DIRECTORY, "mode"), CardHolds(PROBE_DIRECTORY, "runs"),
                      CardHolds(PROBE_OLD_DIRECTORY, "mode"), CardHolds(PROBE_OLD_DIRECTORY, "runs"));
    mode_path = choice == RUNNER_ROOT_OLD ? PROBE_OLD_DIRECTORY "/mode" : PROBE_DIRECTORY "/mode";
    int32_t type = -1;
    if (R_SUCCEEDED(NnFsGetEntryType(&type, mode_path)) && type == NN_FS_ENTRY_DIRECTORY) {
        return false;
    }
    NnFsFileHandle file = { 0 };
    if (R_FAILED(NnFsOpenFile(&file, mode_path, NN_FS_OPEN_READ))) {
        return false;
    }
    int64_t size = 0;
    size_t read = 0;
    if (R_SUCCEEDED(NnFsGetFileSize(&size, file)) && size > 0) {
        const size_t want = (size_t)size < MODE_FILE_MAX + 1u ? (size_t)size : MODE_FILE_MAX + 1u;
        if (R_FAILED(NnFsReadFile(&read, file, 0, text, want))) {
            read = 0;
        }
    }
    NnFsCloseFile(file);
    text[read] = '\0';
    const bool longer = read > MODE_FILE_MAX;
    return ModeFromFile(MODE_SOURCE_FILE, text, longer ? MODE_FILE_MAX : read, longer).hooks;
}

// ---------------------------------------------------------------------------------- the start

/// Says on the machine's log why the start is going the way it is, for a machine that ends the
/// program before the report opens.
static void Say(const char *text) {
    PlatformLog(text, strlen(text));
}

/// libnx's own start for the card and the clocks, what the IPC mode keeps: time:u, the C library's
/// time zone, fsp-srv and its sdmc: device, the working directory. Each failure ends the program
/// with libnx's result for it, as libnx's start does.
static void StartIpc(void) {
    Result rc = timeInitialize();
    if (R_FAILED(rc)) {
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_Time));
    }
    __libnx_init_time();
    rc = fsInitialize();
    if (R_FAILED(rc)) {
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_FS));
    }
    fsdevMountSdmc();
    __libnx_init_cwd();
}

/// The card and the clocks through the SDK's functions: the card stays mounted as CARD and is the
/// C library's sdmc: device, and nn::time is initialized as a game's start initializes it.
static void StartHooks(void) {
    const int device = AddDevice(SdkCardDevice());
    if (device < 0) {
        Say("amberlith: hooks mode: the C library has no room for the sdmc device");
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_TooManyDevOpTabs));
    }
    setDefaultDevice(device);
    time_rc = NnTimeInitialize();
    time_own = SdkOwnCalls(SDK_TIME_INITIALIZE) != 0u;
}

/// The mode file read through nn::fs, then the start that mode asks for.
static void StartFiles(void) {
    card_rc = NnFsMountSdCard(CARD);
    card_own = SdkFsOwnMount(CARD);
    if (R_FAILED(card_rc)) {
        char said[160];
        snprintf(said, sizeof(said),
                 "amberlith: nn::fs::MountSdCard(\"" CARD "\") returned 0x%x, so the mode file is "
                 "read as no file: the IPC mode",
                 (unsigned)card_rc);
        Say(said);
    }
    hooks_on = R_SUCCEEDED(card_rc) && ModeAsksHooks();
    if (hooks_on) {
        StartHooks();
        return;
    }
    if (R_SUCCEEDED(card_rc) && !card_own) {
        char said[320];
        snprintf(said, sizeof(said),
                 "amberlith: the machine answered nn::fs::MountSdCard itself and the mode file has "
                 "no hooks: the IPC mode opens time:u and fsp-srv now, which such a machine may "
                 "refuse; the word hooks in %s asks through the SDK's functions instead",
                 mode_path);
        Say(said);
    }
    if (R_SUCCEEDED(card_rc)) {
        NnFsUnmount(CARD);
    }
    StartIpc();
}

/// libnx's __appInit, the card and the clocks as the mode file asks.
void __appInit(void) {
    Result rc = smInitialize();
    if (R_FAILED(rc)) {
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM));
    }
    if (hosversionGet() == 0u && R_SUCCEEDED(setsysInitialize())) {
        SetSysFirmwareVersion firmware;
        if (R_SUCCEEDED(setsysGetFirmwareVersion(&firmware))) {
            hosversionSet(MAKEHOSVERSION(firmware.major, firmware.minor, firmware.micro));
        }
        setsysExit();
    }
    rc = appletInitialize();
    if (R_FAILED(rc)) {
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_AM));
    }
    if (__nx_applet_type != AppletType_None) {
        rc = hidInitialize();
        if (R_FAILED(rc)) {
            diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_HID));
        }
    }
    StartFiles();
    if (__nx_win_init != NULL) {
        __nx_win_init();
    }
    if (userAppInit != NULL) {
        userAppInit();
    }
}

/// libnx's __appExit, with the card and the clocks of the mode the program started in.
void __appExit(void) {
    if (userAppExit != NULL) {
        userAppExit();
    }
    if (__nx_win_exit != NULL) {
        __nx_win_exit();
    }
    if (hooks_on) {
        RemoveDevice(CARD ":");
        NnFsUnmount(CARD);
    } else {
        fsdevUnmountAll();
        fsExit();
        timeExit();
    }
    hidExit();
    appletExit();
    smExit();
}

// --------------------------------------------------------------------------- what the run says

void RunSupportMode(void) {
    static const char *const SUITE = "support";
    static const char *const NAME =
        "the services a machine may answer in a game's code are asked the way the mode file says";
    if (!ReportStart(SUITE, NAME)) {
        return;
    }
    const char *const card = card_own ? "the program's own code (fsp-srv)" : "the machine";
    if (hooks_on) {
        CHECK_TRUE(SUITE, NAME, CHECK_NONE, R_SUCCEEDED(time_rc),
                   "hooks: the mode file has the word; nn::fs::MountSdCard(\"" CARD "\") answered "
                   "by %s, nn::time::Initialize by %s with 0x%x",
                   card, time_own ? "the program's own code (time:u)" : "the machine",
                   (unsigned)time_rc);
        return;
    }
    const bool machine_took = R_SUCCEEDED(card_rc) && !card_own;
    CHECK_TRUE(SUITE, NAME, CHECK_NONE, !machine_took,
               "IPC: the mode file has no hooks, so the program opened time:u and fsp-srv at its "
               "start, as libnx does%s; nn::fs::MountSdCard(\"" CARD "\") for the mode file "
               "returned 0x%x, answered by %s",
               machine_took ? ", on a machine that answered nn::fs itself: the word hooks asks it "
                              "through the SDK's functions"
                            : "",
               (unsigned)card_rc, R_SUCCEEDED(card_rc) ? card : "nobody");
}

void HooksReportOwnCode(const char *step) {
    static uint32_t noted[SDK_FUNCTION_COUNT];
    if (!hooks_on) {
        return;
    }
    char said[REPORT_NOTE_MAX - 96];
    if (SdkOwnDescribeSince(said, sizeof(said), noted) > 0u) {
        ReportNote("hooks: %s%s, the program's own code answered %s", step != NULL ? "in step " : "",
                   step != NULL ? step : "at the start", said);
    }
}
