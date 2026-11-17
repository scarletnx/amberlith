// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <reent.h>
#include <stdio.h>
#include <string.h>
#include <sys/iosupport.h>
#include <sys/lock.h>
#include <switch.h>

#include "game.h"
#include "sdk_card.h"

// The mod's start (docs/Mod.md): the module's DT_INIT, which the game's dynamic linker calls on
// the game's main thread once the SDK has started and before the game's constructors and its
// main. Nothing of libnx's start runs or is linked: the heap is a block of the module's own, the
// memory card is the game's SDK's, and the run ends the process, so the game never begins.

// The C library's heap: the cases' pools and objects, and what the driver allocates through the
// graphics allocator. A block of the module's zeroed data, so nothing is taken from the heap the
// game's own start sized.
#define HEAP_BYTES 0x4000000u
static char heap[HEAP_BYTES] __attribute__((aligned(0x1000)));
extern char *fake_heap_start;
extern char *fake_heap_end;

// ---- what the C library asks of the program it is linked into, which libnx's start answers
// for a homebrew program. One thread runs the engine here, the game's main thread, so its
// record of the C library's state is the library's one and a lock has nobody to keep out.

struct _reent *__syscall_getreent(void);
struct _reent *__syscall_getreent(void) {
    return _impure_ptr;
}

void __syscall_lock_acquire(_LOCK_T *lock);
void __syscall_lock_acquire(_LOCK_T *lock) {
    (void)lock;
}

void __syscall_lock_release(_LOCK_T *lock);
void __syscall_lock_release(_LOCK_T *lock) {
    (void)lock;
}

void __syscall_lock_acquire_recursive(_LOCK_RECURSIVE_T *lock);
void __syscall_lock_acquire_recursive(_LOCK_RECURSIVE_T *lock) {
    (void)lock;
}

void __syscall_lock_release_recursive(_LOCK_RECURSIVE_T *lock);
void __syscall_lock_release_recursive(_LOCK_RECURSIVE_T *lock) {
    (void)lock;
}

void __syscall_exit(int status) __attribute__((noreturn));
void __syscall_exit(int status) {
    (void)status;
    svcExitProcess();
}

static void Say(const char *text) {
    svcOutputDebugString(text, strlen(text));
}

/// The card mounted as the C library's sdmc: device, through whichever of the SDK's two mounts
/// the game has and answers.
static bool MountCard(ModStart *start) {
    __typeof__(GameMountSdCardForDebug) *const volatile for_debug = GameMountSdCardForDebug;
    __typeof__(NnFsMountSdCard) *const volatile plain = NnFsMountSdCard;
    start->for_debug_rc = UINT32_MAX;
    start->plain_rc = UINT32_MAX;
    start->mounted_by = NULL;
    if (for_debug != NULL) {
        start->for_debug_rc = for_debug(SDK_CARD);
        if (R_SUCCEEDED(start->for_debug_rc)) {
            start->mounted_by = "nn::fs::MountSdCardForDebug";
        }
    }
    if (start->mounted_by == NULL && plain != NULL) {
        start->plain_rc = plain(SDK_CARD);
        if (R_SUCCEEDED(start->plain_rc)) {
            start->mounted_by = "nn::fs::MountSdCard";
        }
    }
    return start->mounted_by != NULL;
}

void EdenprobeModInit(void);
void EdenprobeModInit(void) {
    fake_heap_start = heap;
    fake_heap_end = heap + sizeof(heap);

    ModStart start;
    if (!MountCard(&start)) {
        // No card, no report: said where a debugger or a runtime's log has it, and the process
        // ends here as it does after a run, so a game that starts is never taken for a run.
        char said[320];
        snprintf(said, sizeof(said),
                 "edenprobe mod: no memory card: nn::fs::MountSdCardForDebug(\"" SDK_CARD "\") "
                 "returned 0x%x and nn::fs::MountSdCard 0x%x (0xffffffff: the game's SDK has no "
                 "such function); a game's own main.npdm gives no access to the card, the mod's "
                 "does. The process ends with no report",
                 (unsigned)start.for_debug_rc, (unsigned)start.plain_rc);
        Say(said);
        svcExitProcess();
    }
    const int device = AddDevice(SdkCardDevice());
    if (device < 0) {
        Say("edenprobe mod: the C library has no room for the sdmc device: the process ends "
            "with no report");
        svcExitProcess();
    }
    setDefaultDevice(device);

    (void)ModRun(&start);

    NnFsUnmount(SDK_CARD);
    svcExitProcess();
}
