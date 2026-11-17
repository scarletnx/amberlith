// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <nvnlayer.h>

// The build for a game's own driver (layer.mk's NVNLAYER_DRIVER := game), on the machine that
// builds: the layer defines no nvnBootstrapLoader there, so the one below stands for the game's,
// and the loader has to fill every field from it and from nothing else.

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

// ---- what stands in for the game's driver

/// The name the driver answers NULL for, or NULL for none.
static const char *driver_lacks;
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
    if (driver_lacks != NULL && strcmp(driver_lacks, name) == 0) {
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
}

static void TestLacking(void) {
    // A driver without one name: the load says which, whichever of the three steps asks it.
    static const char *const LACKED[] = { "nvnDeviceGetProcAddress", "nvnDeviceGetInteger",
                                          "nvnQueueInitialize" };
    for (size_t i = 0; i < sizeof(LACKED) / sizeof(LACKED[0]); ++i) {
        NvnProcs procs;
        char missing[64] = "untouched";
        driver_lacks = LACKED[i];
        EXPECT(!NvnLoadProcs(&procs, missing, sizeof(missing)),
               "a driver without %s loaded", LACKED[i]);
        EXPECT(strcmp(missing, LACKED[i]) == 0, "a driver without %s: the load named \"%s\"",
               LACKED[i], missing);
    }
    driver_lacks = NULL;
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
