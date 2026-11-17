// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <nvnlayer.h>

// The build for a game's own driver (layer.mk's NVNLAYER_DRIVER := game), on the machine that
// builds: the layer defines no nvnBootstrapLoader there, so the one below stands for the game's,
// and the loader has to fill every field from it, or for a name it lacks with the body that
// keeps the name and returns nought.

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

/// Whether each name's function returns a float, where nought comes back in another register.
static const bool RETURNS_FLOAT[] = {
#define NVN_LAYER_FLOAT(result, name, parameters)                                                 \
    _Generic((result(*)(void))0, float (*)(void): true, default: false),
    NVN_LAYER_PROCS(NVN_LAYER_FLOAT)
#undef NVN_LAYER_FLOAT
};

// ---- the program's hook

static int hook_calls;
static char hook_name[64];

static void Hook(const char *name) {
    ++hook_calls;
    snprintf(hook_name, sizeof(hook_name), "%s", name);
}

// ---- what stands in for the game's driver

/// The names the driver answers NULL for, NULL for none; or every name but the two the lookup
/// is made of.
static const char *driver_lacks;
static const char *driver_lacks_too;
static bool driver_lacks_all;
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
        (driver_lacks_too != NULL && strcmp(driver_lacks_too, name) == 0) ||
        (driver_lacks_all && strcmp(name, "nvnDeviceGetInteger") != 0)) {
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
    // on and says which, in the list's order, and every other field is the driver's.
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
        } else {
            EXPECT(fields[i + 2u] != NULL && fields[i + 2u] != DriverProc(NAMES[i]),
                   "the field of the lacked %s is not a body of the layer's", NAMES[i]);
        }
    }
    driver_lacks = NULL;
    driver_lacks_too = NULL;

    // The next load starts the count again.
    EXPECT(NvnLoadProcs(&procs, missing, sizeof(missing)) && NvnLayerLackedCount() == 0u,
           "a load from a driver with every name still says %zu lacked", NvnLayerLackedCount());
}

static void TestMissed(void) {
    // A driver with none of the list's names: every field is then a body of the layer's, and a
    // call of one comes back with nought, in the register its function's result has, the name
    // kept and the hook told, each once however often it is called.
    NvnProcs procs;
    char missing[64] = "untouched";
    driver_lacks_all = true;
    EXPECT(NvnLoadProcs(&procs, missing, sizeof(missing)), "a driver with no name of the list: "
                                                           "the load missed %s", missing);
    driver_lacks_all = false;
    EXPECT(NvnLayerLackedCount() == NAME_COUNT, "%zu names lacked of %zu", NvnLayerLackedCount(),
           NAME_COUNT);
    const NvnGenericProc *const fields = (const NvnGenericProc *)(const void *)&procs;
    NvnLayerMissedClear();
    NvnLayerSetFatal(Hook);
    hook_calls = 0;
    size_t floats = 0;
    for (size_t i = 0; i < NAME_COUNT; ++i) {
        for (int call = 0; call < 2; ++call) {
            if (RETURNS_FLOAT[i]) {
                const float result = ((float (*)(void))fields[i + 2u])();
                EXPECT(result == 0.0f, "the lacked %s returned %f", NAMES[i], (double)result);
            } else {
                const uint64_t result = ((uint64_t (*)(void))fields[i + 2u])();
                EXPECT(result == 0u, "the lacked %s returned 0x%llx", NAMES[i],
                       (unsigned long long)result);
            }
        }
        floats += RETURNS_FLOAT[i] ? 1u : 0u;
        EXPECT(NvnLayerMissedCount() == i + 1u, "%zu names kept after %zu were called",
               NvnLayerMissedCount(), i + 1u);
        EXPECT(NvnLayerMissedName(i) != NULL && strcmp(NvnLayerMissedName(i), NAMES[i]) == 0,
               "the name kept for the call of %s is %s", NAMES[i],
               NvnLayerMissedName(i) != NULL ? NvnLayerMissedName(i) : "none");
        EXPECT(hook_calls == (int)i + 1 && strcmp(hook_name, NAMES[i]) == 0,
               "the hook was told %d times after %zu names, the last \"%s\"", hook_calls, i + 1u,
               hook_name);
    }
    EXPECT(NvnLayerMissedName(NAME_COUNT) == NULL, "a name past the count");
    EXPECT(floats != 0u && floats != NAME_COUNT, "%zu of the %zu functions return a float", floats,
           NAME_COUNT);
    EXPECT(NvnLayerError()[0] == '\0' && !NvnLayerLacks(),
           "a lacked call left the layer's own error: \"%s\"", NvnLayerError());

    // A clear forgets them, and a name called again is kept and told again.
    NvnLayerMissedClear();
    EXPECT(NvnLayerMissedCount() == 0u && NvnLayerMissedName(0) == NULL,
           "%zu names kept after a clear", NvnLayerMissedCount());
    (void)((uint64_t (*)(void))fields[2])();
    EXPECT(NvnLayerMissedCount() == 1u && strcmp(NvnLayerMissedName(0), NAMES[0]) == 0 &&
               hook_calls == (int)NAME_COUNT + 1,
           "after a clear, a call of %s kept %zu names and told the hook %d times in all",
           NAMES[0], NvnLayerMissedCount(), hook_calls);

    // With no hook the name is kept all the same.
    NvnLayerSetFatal(NULL);
    NvnLayerMissedClear();
    (void)((uint64_t (*)(void))fields[2])();
    EXPECT(NvnLayerMissedCount() == 1u && hook_calls == (int)NAME_COUNT + 1,
           "with no hook, %zu names kept and %d hook calls", NvnLayerMissedCount(), hook_calls);
    NvnLayerMissedClear();
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
    // The two hooks are taken down again: no call here reaches either.
    NvnLayerSetGuard(NULL);
    NvnLayerSetFatal(NULL);
}

int main(void) {
    TestLoad();
    TestLacking();
    TestMissed();
    TestOwn();
    if (failures != 0) {
        printf("nvnlayer game driver test (API %d): %d failed\n", NVN_LAYER_API, failures);
        return 1;
    }
    printf("nvnlayer game driver test (API %d): ok, %zu names\n", NVN_LAYER_API, NAME_COUNT);
    return 0;
}
