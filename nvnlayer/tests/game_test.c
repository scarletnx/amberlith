// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <nvnlayer.h>

#include "../source/layer.h"

// The build for a game's own driver (layer.mk's NVNLAYER_DRIVER := game), on the machine that
// builds: the layer defines no nvnBootstrapLoader there, so the one below stands for the game's,
// and the loader has to fill every field from it, or for a name it lacks with the body that
// stops the program.

static int failures;

#define EXPECT(condition, ...)                                                                    \
    do {                                                                                          \
        if (!(condition)) {                                                                       \
            ++failures;                                                                           \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                           \
            printf(__VA_ARGS__);                                                                  \
            printf("\n");                                                                         \
        }                                                                                         \
    } while (0)

static const char *const NAMES[] = {
#define NVN_LAYER_NAME(result, name, parameters) #name,
    NVN_LAYER_PROCS(NVN_LAYER_NAME)
#undef NVN_LAYER_NAME
};
#define NAME_COUNT (sizeof(NAMES) / sizeof(NAMES[0]))

// ---- what stands in for the console

// Where a stopped program lands: LayerStop does not come back, so the test jumps out of it.
static jmp_buf stopped;
static char stop_text[256];

void LayerStop(const char *text) {
    snprintf(stop_text, sizeof(stop_text), "%s", text);
    longjmp(stopped, 1);
}

static int hook_calls;
static char hook_name[64];

static void Hook(const char *name) {
    ++hook_calls;
    snprintf(hook_name, sizeof(hook_name), "%s", name);
}

// ---- what stands in for the game's driver

/// The names the driver answers NULL for, NULL for none.
static const char *driver_lacks;
static const char *driver_lacks_too;
static size_t bootstrap_calls;
static size_t lookup_calls;
static const NVNdevice *lookup_device;

/// The address the driver gives @p name: one of its own for each name, none of them code.
static NvnGenericProc DriverProc(const char *name) {
    if (strcmp(name, "nvnDeviceGetInteger") == 0) {
        return (NvnGenericProc)(uintptr_t)0x7000u;
    }
    for (size_t i = 0; i < NAME_COUNT; ++i) {
        if (strcmp(NAMES[i], name) == 0) {
            return (NvnGenericProc)(uintptr_t)(0x8000u + 8u * i);
        }
    }
    return NULL;
}

static NvnGenericProc DriverGetProcAddress(const NVNdevice *device, const char *name) {
    ++lookup_calls;
    lookup_device = device;
    if ((driver_lacks != NULL && strcmp(driver_lacks, name) == 0) ||
        (driver_lacks_too != NULL && strcmp(driver_lacks_too, name) == 0)) {
        return NULL;
    }
    return DriverProc(name);
}

NvnGenericProc nvnBootstrapLoader(const char *name) {
    ++bootstrap_calls;
    if (driver_lacks != NULL && strcmp(driver_lacks, name) == 0) {
        return NULL;
    }
    return strcmp(name, "nvnDeviceGetProcAddress") == 0 ? (NvnGenericProc)DriverGetProcAddress
                                                        : NULL;
}

// ---- the cases

static void TestLoad(void) {
    NvnProcs procs;
    char missing[64] = "untouched";
    driver_lacks = NULL;
    bootstrap_calls = 0;
    lookup_calls = 0;
    lookup_device = (const NVNdevice *)&procs;
    EXPECT(NvnLoadProcs(&procs, missing, sizeof(missing)), "NvnLoadProcs missed %s", missing);
    EXPECT(strcmp(missing, "untouched") == 0, "a load that worked wrote \"%s\"", missing);
    // As a game asks: the bootstrap once, for nvnDeviceGetProcAddress, then every other name
    // from that function with no device.
    EXPECT(bootstrap_calls == 1u, "the game's nvnBootstrapLoader was called %zu times, not once",
           bootstrap_calls);
    EXPECT(lookup_calls == NAME_COUNT + 1u, "%zu lookups for %zu names", lookup_calls,
           NAME_COUNT + 1u);
    EXPECT(lookup_device == NULL, "a lookup was given a device");
    const NvnGenericProc *const fields = (const NvnGenericProc *)(const void *)&procs;
    EXPECT(fields[0] == DriverProc("nvnDeviceGetInteger"), "nvnDeviceGetInteger's field");
    EXPECT(fields[1] == (NvnGenericProc)DriverGetProcAddress, "nvnDeviceGetProcAddress's field");
    for (size_t i = 0; i < NAME_COUNT; ++i) {
        EXPECT(fields[i + 2u] == DriverProc(NAMES[i]),
               "the field of %s holds another function than the driver's", NAMES[i]);
    }
    EXPECT(NvnLayerLackedCount() == 0u && NvnLayerLackedName(0) == NULL,
           "a driver with every name lacked %zu, the first %s", NvnLayerLackedCount(),
           NvnLayerLackedName(0) != NULL ? NvnLayerLackedName(0) : "none");
}

static void TestLacking(void) {
    // A driver without one of the two names the lookup is made of: no load, and which is said.
    static const char *const NEEDED[] = { "nvnDeviceGetProcAddress", "nvnDeviceGetInteger" };
    for (size_t i = 0; i < sizeof(NEEDED) / sizeof(NEEDED[0]); ++i) {
        NvnProcs procs;
        char missing[64] = "untouched";
        driver_lacks = NEEDED[i];
        EXPECT(!NvnLoadProcs(&procs, missing, sizeof(missing)), "a driver without %s loaded",
               NEEDED[i]);
        EXPECT(strcmp(missing, NEEDED[i]) == 0, "a driver without %s: the load named \"%s\"",
               NEEDED[i], missing);
    }

    // A driver without two other names, as a driver of another API version is: the load goes
    // on, says which in the list's order, and each one's field stops the program with its name.
    const size_t first = NAME_COUNT / 3u;
    const size_t second = NAME_COUNT - 1u;
    NvnProcs procs;
    char missing[64] = "untouched";
    driver_lacks = NAMES[second];
    driver_lacks_too = NAMES[first];
    EXPECT(NvnLoadProcs(&procs, missing, sizeof(missing)), "a driver without %s and %s: no load",
           NAMES[first], NAMES[second]);
    EXPECT(strcmp(missing, "untouched") == 0, "such a load wrote \"%s\"", missing);
    EXPECT(NvnLayerLackedCount() == 2u, "%zu names lacked, not 2", NvnLayerLackedCount());
    EXPECT(NvnLayerLackedCount() == 2u && strcmp(NvnLayerLackedName(0), NAMES[first]) == 0 &&
               strcmp(NvnLayerLackedName(1), NAMES[second]) == 0 && NvnLayerLackedName(2) == NULL,
           "the lacked names are not %s then %s", NAMES[first], NAMES[second]);
    const NvnGenericProc *const fields = (const NvnGenericProc *)(const void *)&procs;
    for (size_t i = 0; i < NAME_COUNT; ++i) {
        if (i != first && i != second) {
            EXPECT(fields[i + 2u] == DriverProc(NAMES[i]),
                   "beside a lacked name, the field of %s holds another function than the driver's",
                   NAMES[i]);
            continue;
        }
        EXPECT(fields[i + 2u] != NULL && fields[i + 2u] != DriverProc(NAMES[i]),
               "the field of the lacked %s is not a body of the layer's", NAMES[i]);
        hook_calls = 0;
        hook_name[0] = '\0';
        stop_text[0] = '\0';
        NvnLayerSetFatal(Hook);
        if (fields[i + 2u] != NULL && setjmp(stopped) == 0) {
            fields[i + 2u]();
            EXPECT(false, "the body of the lacked %s came back", NAMES[i]);
        }
        char expected[192];
        snprintf(expected, sizeof(expected),
                 "nvnlayer: FATAL: %s was called, and the game's driver has no function of that "
                 "name",
                 NAMES[i]);
        EXPECT(strcmp(stop_text, expected) == 0, "the lacked %s stopped with \"%s\"", NAMES[i],
               stop_text);
        EXPECT(hook_calls == 1 && strcmp(hook_name, NAMES[i]) == 0,
               "the hook was called %d times, with \"%s\"", hook_calls, hook_name);
    }
    driver_lacks = NULL;
    driver_lacks_too = NULL;

    // The next load starts the count again.
    EXPECT(NvnLoadProcs(&procs, missing, sizeof(missing)) && NvnLayerLackedCount() == 0u,
           "a load from a driver with every name still says %zu lacked", NvnLayerLackedCount());
}

static void TestOwn(void) {
    // No function of the layer's own in this build, before a load and after one alike, and the
    // structure is left as it was.
    NvnProcs procs;
    memset(&procs, 0x5a, sizeof(procs));
    NvnProcs before = procs;
    NvnLayerClearError();
    EXPECT(!NvnLayerOwnProcs(&procs), "NvnLayerOwnProcs gave functions in a game's driver build");
    EXPECT(strstr(NvnLayerError(), "game's own driver") != NULL, "the reason was \"%s\"",
           NvnLayerError());
    EXPECT(memcmp(&procs, &before, sizeof(procs)) == 0, "a refused NvnLayerOwnProcs wrote");
    EXPECT(!NvnLayerLacks(), "a refusal counted as a call deko3d cannot do");
    NvnLayerClearError();
    EXPECT(NvnLayerError()[0] == '\0', "the error stayed after a clear: \"%s\"", NvnLayerError());
    // The two hooks are set and never called here: no body, no deko3d constructor.
    NvnLayerSetGuard(NULL);
    NvnLayerSetFatal(NULL);
}

int main(void) {
    TestLoad();
    TestLacking();
    TestOwn();
    if (failures != 0) {
        printf("nvnlayer game driver test (API %d): %d failed\n", NVN_LAYER_API, failures);
        return 1;
    }
    printf("nvnlayer game driver test (API %d): ok, %zu names\n", NVN_LAYER_API, NAME_COUNT);
    return 0;
}
