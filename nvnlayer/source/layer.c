// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "layer.h"

// The layer's shared part, the same code in every build: the failure a program reads, the two
// hooks it sets, the lookup over the build's table, and the loader a program fills its function
// pointers with. It names no deko3d and no machine function, so it also builds where the
// program's own tests run.

// ---- the failure a program reports

static char layer_error[256];

const char *NvnLayerError(void) {
    return layer_error;
}

void NvnLayerClearError(void) {
    layer_error[0] = '\0';
}

void LayerComplain(const char *format, ...) {
    if (layer_error[0] != '\0') {
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(layer_error, sizeof(layer_error), format, args);
    va_end(args);
}

// ---- the program's hooks

static NvnLayerGuard layer_guard;
static NvnLayerFatalHook layer_fatal;

void NvnLayerSetGuard(NvnLayerGuard guard) {
    layer_guard = guard;
}

void NvnLayerSetFatal(NvnLayerFatalHook hook) {
    layer_fatal = hook;
}

bool LayerGuarded(void (*call)(void), const char *what) {
    if (layer_guard == NULL) {
        call();
        return true;
    }
    uint32_t result = 0;
    if (layer_guard(call, &result)) {
        return true;
    }
    LayerComplain("%s ended in deko3d's abort, result 0x%x", what, (unsigned)result);
    return false;
}

void LayerFatal(const char *name) {
    // The hook is taken down first: a hook that itself reaches a body must not come back here.
    const NvnLayerFatalHook hook = layer_fatal;
    layer_fatal = NULL;
    if (hook != NULL) {
        hook(name);
    }
    char text[192];
    snprintf(text, sizeof(text),
             "nvnlayer: FATAL: %s was reached: no NVN implementation took it", name);
    LayerStop(text);
}

// ---- nvnDeviceGetInteger

// In assembly, in the shape the console's NVN drivers give it: a bounded switch on the item
// through a byte table scaled by four from an ADR, its cases setting w0. That is the shape
// ScarletNX reads a game's API version out of, from the code, before the game asks (its
// nvn_driver.c); a C switch compiles to another shape, and ScarletNX would then warn and answer
// from its own table. Items 0 and 1, the API's major and minor version, are the two constants
// here; every other item goes to the version file's NvnLayerDeviceInfo, which ScarletNX's reader
// sees as an item it cannot read from code, and answers from its own table.
//
// The table holds the item's case as a word offset from label 2: 0 for the major version, 2 for
// the minor, 4 for the rest. The item is zero-extended first, since the callee owns the upper
// half of x1 and the table load indexes with all of it.
void NvnLayerDeviceGetInteger(const NVNdevice *device, NVNdeviceInfo item, int *value);
#ifdef __aarch64__
#define LAYER_STRING(x) #x
#define LAYER_EXPAND(x) LAYER_STRING(x)
__asm__(".section .text.NvnLayerDeviceGetInteger,\"ax\",%progbits\n"
        ".global NvnLayerDeviceGetInteger\n"
        ".hidden NvnLayerDeviceGetInteger\n"
        ".type NvnLayerDeviceGetInteger, %function\n"
        ".p2align 2\n"
        "NvnLayerDeviceGetInteger:\n"
        ".cfi_startproc\n"
        "    mov w1, w1\n"
        "    cmp w1, #0x68\n"
        "    b.hi 1f\n"
        "    adr x8, 3f\n"
        "    ldrb w9, [x8, x1]\n"
        "    adr x10, 2f\n"
        "    add x10, x10, x9, lsl #2\n"
        "    br x10\n"
        "2:  mov w0, #" LAYER_EXPAND(NVN_LAYER_API_MAJOR) "\n"
        "    b 4f\n"
        "    mov w0, #" LAYER_EXPAND(NVN_LAYER_API_MINOR) "\n"
        "    b 4f\n"
        "1:  b NvnLayerDeviceInfo\n"
        "4:  str w0, [x2]\n"
        "    ret\n"
        ".cfi_endproc\n"
        "3:  .byte 0, 2\n"
        "    .fill 0x67, 1, 4\n"
        ".size NvnLayerDeviceGetInteger, . - NvnLayerDeviceGetInteger\n"
        ".text\n");
#else
// Any other processor, where only the layer's own tests run it: the same answers from C.
void NvnLayerDeviceGetInteger(const NVNdevice *device, NVNdeviceInfo item, int *value) {
    if ((unsigned)item == 0u) {
        *value = NVN_LAYER_API_MAJOR;
    } else if ((unsigned)item == 1u) {
        *value = NVN_LAYER_API_MINOR;
    } else {
        NvnLayerDeviceInfo(device, (int)item, value);
    }
}
#endif

// ---- the lookup

static NvnGenericProc Nvnl_nvnDeviceGetProcAddress(const NVNdevice *device, const char *name);

/// The function named @p name, by a search of the names and nothing else: ScarletNX runs this
/// code itself, from inside the program's first nvnBootstrapLoader call, and expects of it what
/// a driver's lookup does, a name in and an address out.
static NvnGenericProc Lookup(const char *name) {
    if (name == NULL) {
        return NULL;
    }
    if (strcmp(name, "nvnDeviceGetInteger") == 0) {
        return (NvnGenericProc)NvnLayerDeviceGetInteger;
    }
    if (strcmp(name, "nvnDeviceGetProcAddress") == 0) {
        return (NvnGenericProc)Nvnl_nvnDeviceGetProcAddress;
    }
    for (size_t i = 0; i < LAYER_ENTRY_COUNT; ++i) {
        if (strcmp(LAYER_ENTRIES[i].name, name) == 0) {
            return LAYER_ENTRIES[i].proc;
        }
    }
    return NULL;
}

static NvnGenericProc Nvnl_nvnDeviceGetProcAddress(const NVNdevice *device, const char *name) {
    // A game's driver answers with no device too, and ScarletNX's loader calls it so.
    (void)device;
    return Lookup(name);
}

NvnGenericProc nvnBootstrapLoader(const char *name) {
    return Lookup(name);
}

// ---- what a program does with the lookup

/// Whether NvnLoadProcs has looked the functions up: from then on a runtime may hold them.
static bool looked_up;

bool NvnLayerOwnProcs(NvnProcs *procs) {
    if (looked_up) {
        LayerComplain("NvnLayerOwnProcs after NvnLoadProcs: a runtime that runs NVN itself has "
                      "taken the functions at their addresses by now, so they are the layer's "
                      "own only before the first lookup");
        return false;
    }
    procs->nvnDeviceGetInteger = NvnLayerDeviceGetInteger;
    procs->nvnDeviceGetProcAddress = Nvnl_nvnDeviceGetProcAddress;
    // The table is the list in the list's order, as NvnProcs's fields after the first two are.
    size_t next = 0;
#define NVN_LAYER_OWN(result, name, parameters)                                                   \
    procs->name = (result(*) parameters)LAYER_ENTRIES[next++].proc;
    NVN_LAYER_PROCS(NVN_LAYER_OWN)
#undef NVN_LAYER_OWN
    return true;
}

bool NvnLoadProcs(NvnProcs *procs, char *missing, size_t missing_size) {
    // Through pointers the compiler cannot see through, as a game calls its imports: a runtime
    // that runs NVN itself takes a function when a call of the lookup returns its address, and
    // a call it never sees takes nothing. The second pointer matters as much as the first. This
    // file names the function nvnDeviceGetProcAddress resolves to (NvnLayerOwnProcs), and a
    // compiler that can tell which function a field holds calls it directly, guarded by a
    // compare: GCC 15 did, and the names looked up that way kept the layer's bodies under
    // ScarletNX. tools/nvn-lookups.py checks the program for it.
    NvnGenericProc (*volatile bootstrap)(const char *) = nvnBootstrapLoader;
    looked_up = true;
    memset(procs, 0, sizeof(*procs));
    const NvnDeviceGetProcAddressProc volatile get =
        (NvnDeviceGetProcAddressProc)bootstrap("nvnDeviceGetProcAddress");
    procs->nvnDeviceGetProcAddress = get;
    if (procs->nvnDeviceGetProcAddress == NULL) {
        snprintf(missing, missing_size, "nvnDeviceGetProcAddress");
        return false;
    }
    procs->nvnDeviceGetInteger = (NvnDeviceGetIntegerProc)get(NULL, "nvnDeviceGetInteger");
    if (procs->nvnDeviceGetInteger == NULL) {
        snprintf(missing, missing_size, "nvnDeviceGetInteger");
        return false;
    }
#define NVN_LAYER_LOAD(result, name, parameters)                                                  \
    procs->name = (result(*) parameters)get(NULL, #name);                                         \
    if (procs->name == NULL) {                                                                    \
        snprintf(missing, missing_size, "%s", #name);                                             \
        return false;                                                                             \
    }
    NVN_LAYER_PROCS(NVN_LAYER_LOAD)
#undef NVN_LAYER_LOAD
    return true;
}
