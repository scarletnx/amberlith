// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nvnlayer.h>

#include "../source/layer.h"

// The layer's parts that touch no console, on the machine that builds: the shared part in both
// API versions, the API 231 build's bodies, and the API 55 build's control sections. host.mk
// builds this file once for each version and runs both.

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

// ---- what stands in for the console

// Where a stopped program lands: LayerStop does not come back, so the test jumps out of it.
static jmp_buf stopped;
static char stop_text[256];

void LayerStop(const char *text) {
    snprintf(stop_text, sizeof(stop_text), "%s", text);
    longjmp(stopped, 1);
}

#if NVN_LAYER_API < 231
// The API 55 build's bodies record deko3d commands, which no host has: the table here gives
// every name of the list a body of its own, so the shared part is tested against the list.
#define NVN_LAYER_BODY(result, name, parameters)                                                  \
    static void Body_##name(void) {}
NVN_LAYER_PROCS(NVN_LAYER_BODY)
#undef NVN_LAYER_BODY

const LayerEntry LAYER_ENTRIES[] = {
#define NVN_LAYER_ENTRY(result, name, parameters) { #name, (NvnGenericProc)Body_##name },
    NVN_LAYER_PROCS(NVN_LAYER_ENTRY)
#undef NVN_LAYER_ENTRY
};
const size_t LAYER_ENTRY_COUNT = sizeof(LAYER_ENTRIES) / sizeof(LAYER_ENTRIES[0]);

void NvnLayerDeviceInfo(const NVNdevice *device, int item, int *value) {
    (void)device;
    *value = 1000 + item;
}
#endif

// ---- the shared part

static const char *const NAMES[] = {
#define NVN_LAYER_NAME(result, name, parameters) #name,
    NVN_LAYER_PROCS(NVN_LAYER_NAME)
#undef NVN_LAYER_NAME
};
#define NAME_COUNT (sizeof(NAMES) / sizeof(NAMES[0]))

static void TestLookup(void) {
    EXPECT(LAYER_ENTRY_COUNT == NAME_COUNT, "the table has %zu entries for a list of %zu",
           LAYER_ENTRY_COUNT, NAME_COUNT);
    EXPECT(nvnBootstrapLoader(NULL) == NULL, "a NULL name resolved");
    EXPECT(nvnBootstrapLoader("") == NULL, "an empty name resolved");
    EXPECT(nvnBootstrapLoader("nvnDeviceGetProcAddres") == NULL, "a name cut short resolved");
    EXPECT(nvnBootstrapLoader("nvnDeviceGetProcAddresss") == NULL, "a name too long resolved");
    EXPECT(nvnBootstrapLoader("nvnNoSuchFunction") == NULL, "a name NVN does not have resolved");

    const NvnDeviceGetProcAddressProc get =
        (NvnDeviceGetProcAddressProc)nvnBootstrapLoader("nvnDeviceGetProcAddress");
    EXPECT(get != NULL, "the bootstrap gave no nvnDeviceGetProcAddress");
    if (get == NULL) {
        return;
    }
    EXPECT(get(NULL, "nvnDeviceGetProcAddress") == (NvnGenericProc)get,
           "nvnDeviceGetProcAddress does not resolve to itself");
    EXPECT(get(NULL, "nvnDeviceGetInteger") != NULL, "no nvnDeviceGetInteger");
    EXPECT(get(NULL, "nvnDeviceGetInteger") == nvnBootstrapLoader("nvnDeviceGetInteger"),
           "the two lookups disagree on nvnDeviceGetInteger");
    // Every name of the list, through both lookups, each to a function no other name has: a
    // runtime that takes functions by address cannot tell two names at one address apart.
    for (size_t i = 0; i < NAME_COUNT; ++i) {
        const NvnGenericProc proc = get(NULL, NAMES[i]);
        EXPECT(proc != NULL, "%s does not resolve", NAMES[i]);
        EXPECT(proc == nvnBootstrapLoader(NAMES[i]), "the two lookups disagree on %s", NAMES[i]);
        for (size_t j = 0; j < i; ++j) {
            EXPECT(proc != get(NULL, NAMES[j]), "%s and %s share an address", NAMES[i], NAMES[j]);
        }
    }
}

static void TestLoad(void) {
    NvnProcs procs;
    char missing[64] = "untouched";
    EXPECT(NvnLoadProcs(&procs, missing, sizeof(missing)), "NvnLoadProcs missed %s", missing);
    EXPECT(strcmp(missing, "untouched") == 0, "a load that worked wrote \"%s\"", missing);
    // The structure is the two functions the list leaves out and then the list's, a pointer
    // each, in order: every one filled, each with what the lookup gives its name.
    const NvnGenericProc *const fields = (const NvnGenericProc *)(const void *)&procs;
    EXPECT(sizeof(procs) == (NAME_COUNT + 2u) * sizeof(NvnGenericProc),
           "NvnProcs is %zu bytes for %zu functions", sizeof(procs), NAME_COUNT + 2u);
    EXPECT(fields[0] == nvnBootstrapLoader("nvnDeviceGetInteger"), "nvnDeviceGetInteger's field");
    EXPECT(fields[1] == nvnBootstrapLoader("nvnDeviceGetProcAddress"),
           "nvnDeviceGetProcAddress's field");
    for (size_t i = 0; i < NAME_COUNT; ++i) {
        EXPECT(fields[i + 2u] == nvnBootstrapLoader(NAMES[i]), "the field of %s holds another",
               NAMES[i]);
    }
}

static void TestOwn(void) {
    // The layer's own functions are the ones the lookup gives where nothing takes them over:
    // asked for before the first lookup, and refused after it, the structure left alone.
    NvnProcs looked_up;
    NvnProcs own;
    char missing[64];
    memset(&own, 0xee, sizeof(own));
    NvnLayerClearError();
    EXPECT(NvnLayerOwnProcs(&own) && NvnLayerError()[0] == '\0',
           "the layer's own functions before any lookup: \"%s\"", NvnLayerError());
    EXPECT(NvnLoadProcs(&looked_up, missing, sizeof(missing)) &&
               memcmp(&looked_up, &own, sizeof(own)) == 0,
           "NvnLayerOwnProcs and NvnLoadProcs fill the structure differently");
    NvnProcs late;
    memset(&late, 0xee, sizeof(late));
    NvnProcs untouched = late;
    EXPECT(!NvnLayerOwnProcs(&late) && memcmp(&late, &untouched, sizeof(late)) == 0 &&
               strstr(NvnLayerError(), "after NvnLoadProcs") != NULL,
           "the layer's own functions after a lookup: \"%s\"", NvnLayerError());
    NvnLayerClearError();
}

static void TestVersion(void) {
    // Off the console the switch is C: the same two answers the assembly holds.
    int major = -1;
    int minor = -1;
    NvnProcs procs;
    char missing[64];
    if (!NvnLoadProcs(&procs, missing, sizeof(missing))) {
        return;
    }
    procs.nvnDeviceGetInteger(NULL, NVN_DEVICE_INFO_API_MAJOR_VERSION, &major);
    procs.nvnDeviceGetInteger(NULL, NVN_DEVICE_INFO_API_MINOR_VERSION, &minor);
    EXPECT(major == NVN_LAYER_API_MAJOR && minor == NVN_LAYER_API_MINOR,
           "API %d.%d, where the build is %d.%d", major, minor, NVN_LAYER_API_MAJOR,
           NVN_LAYER_API_MINOR);
#if NVN_LAYER_API >= 231
    EXPECT(major == 231 && minor == 16, "the API 231 build answers %d.%d", major, minor);
#else
    EXPECT(major == 55 && minor == 8, "the API 55 build answers %d.%d", major, minor);
    int other = -1;
    procs.nvnDeviceGetInteger(NULL, NVN_DEVICE_INFO_UNIFORM_BUFFER_ALIGNMENT, &other);
    EXPECT(other == 1000 + (int)NVN_DEVICE_INFO_UNIFORM_BUFFER_ALIGNMENT,
           "an item past the version did not reach the version file's answer: %d", other);
#endif
}

static void TestError(void) {
    NvnLayerClearError();
    EXPECT(NvnLayerError()[0] == '\0', "a cleared error reads \"%s\"", NvnLayerError());
    LayerComplain("first %d", 1);
    LayerComplain("second %d", 2);
    EXPECT(strcmp(NvnLayerError(), "first 1") == 0, "the error kept \"%s\", not the first",
           NvnLayerError());
    NvnLayerClearError();
    LayerComplain("third");
    EXPECT(strcmp(NvnLayerError(), "third") == 0, "after a clear the error is \"%s\"",
           NvnLayerError());
    NvnLayerClearError();
}

static int guard_calls;
static int guarded_runs;

static void Guarded(void) {
    ++guarded_runs;
}

static bool PassingGuard(void (*call)(void), uint32_t *result) {
    ++guard_calls;
    call();
    *result = 0;
    return true;
}

static bool AbortingGuard(void (*call)(void), uint32_t *result) {
    (void)call;
    ++guard_calls;
    *result = 0x2a67u;
    return false;
}

static void TestGuard(void) {
    NvnLayerClearError();
    NvnLayerSetGuard(NULL);
    guard_calls = guarded_runs = 0;
    EXPECT(LayerGuarded(Guarded, "bare") && guarded_runs == 1 && guard_calls == 0,
           "with no guard the call ran %d times under %d guards", guarded_runs, guard_calls);
    NvnLayerSetGuard(PassingGuard);
    EXPECT(LayerGuarded(Guarded, "passing") && guarded_runs == 2 && guard_calls == 1,
           "under a guard the call ran %d times under %d guards", guarded_runs, guard_calls);
    EXPECT(NvnLayerError()[0] == '\0', "a call that came back complained: %s", NvnLayerError());
    NvnLayerSetGuard(AbortingGuard);
    EXPECT(!LayerGuarded(Guarded, "nvnThing: dkThingCreate"), "an aborted call reads as done");
    EXPECT(strstr(NvnLayerError(), "nvnThing: dkThingCreate") != NULL &&
               strstr(NvnLayerError(), "0x2a67") != NULL,
           "the abort's sentence names neither the call nor the result: \"%s\"",
           NvnLayerError());
    NvnLayerSetGuard(NULL);
    NvnLayerClearError();
}

// ---- the API 231 build's bodies

#if NVN_LAYER_API >= 231
static int hook_calls;
static char hook_name[128];

static void Hook(const char *name) {
    ++hook_calls;
    snprintf(hook_name, sizeof(hook_name), "%s", name);
}

/// Reaches @p proc as a program would by calling it, and says what it stopped with.
static bool Stops(NvnGenericProc proc) {
    stop_text[0] = '\0';
    if (setjmp(stopped) != 0) {
        return true;
    }
    proc();
    return false;
}

static void TestBodies(void) {
    for (size_t i = 0; i < NAME_COUNT; ++i) {
        char want[256];
        snprintf(want, sizeof(want),
                 "nvnlayer: FATAL: %s was reached: no NVN implementation took it", NAMES[i]);
        hook_calls = 0;
        hook_name[0] = '\0';
        NvnLayerSetFatal(Hook);
        const bool stopped_there = Stops(nvnBootstrapLoader(NAMES[i]));
        EXPECT(stopped_there, "%s came back", NAMES[i]);
        EXPECT(strcmp(stop_text, want) == 0, "%s stopped with \"%s\"", NAMES[i], stop_text);
        EXPECT(hook_calls == 1 && strcmp(hook_name, NAMES[i]) == 0,
               "%s called the program's hook %d times, with \"%s\"", NAMES[i], hook_calls,
               hook_name);
        // The hook is taken down by the stop it saw: a second body reached says its own name
        // through the stop alone.
        hook_calls = 0;
        EXPECT(Stops(nvnBootstrapLoader(NAMES[i])) && hook_calls == 0,
               "the hook outlived the stop it was told of: %d more calls", hook_calls);
    }
    // nvnDeviceGetInteger's switch answers the version; any other item is a body like the rest.
    NvnLayerSetFatal(NULL);
    stop_text[0] = '\0';
    if (setjmp(stopped) == 0) {
        int value = 0;
        NvnLayerDeviceInfo(NULL, NVN_DEVICE_INFO_UNIFORM_BUFFER_ALIGNMENT, &value);
        EXPECT(false, "an item past the version came back with %d", value);
    }
    EXPECT(strstr(stop_text, "FATAL: nvnDeviceGetInteger was reached") != NULL,
           "an item past the version stopped with \"%s\"", stop_text);
}
#endif

// ---- the API 55 build's control sections

#if NVN_LAYER_API < 231
static uint32_t Read(const uint8_t *bytes, uint32_t at) {
    return (uint32_t)bytes[at] | (uint32_t)bytes[at + 1u] << 8 | (uint32_t)bytes[at + 2u] << 16 |
           (uint32_t)bytes[at + 3u] << 24;
}

static void Write(uint8_t *bytes, uint32_t at, uint32_t value) {
    bytes[at] = (uint8_t)value;
    bytes[at + 1u] = (uint8_t)(value >> 8);
    bytes[at + 2u] = (uint8_t)(value >> 16);
    bytes[at + 3u] = (uint8_t)(value >> 24);
}

#define MODULE_CONTROL 0x100u
#define MODULE_CODE 0x300u
#define MODULE_BYTES (MODULE_CONTROL + MODULE_CODE)

/// A DKSH as uam writes one: the header, one program of @p type at 0x18 whose code starts 0x30
/// bytes into a code section of 0x300, with @p constants bytes of constants at 0x200 of it.
static void Module(uint8_t module[MODULE_BYTES], uint32_t type, uint32_t constants) {
    memset(module, 0, MODULE_BYTES);
    Write(module, 0, 0x48534b44u);
    Write(module, 4, 0x18u);
    Write(module, 8, MODULE_CONTROL);
    Write(module, 12, MODULE_CODE);
    Write(module, 16, 0x18u);
    Write(module, 20, 1u);
    Write(module, 0x18 + 0, type);
    Write(module, 0x18 + 4, 0x30u);
    Write(module, 0x18 + 8, 7u);
    Write(module, 0x18 + 12, constants != 0u ? 0x200u : 0u);
    Write(module, 0x18 + 16, constants);
    for (uint32_t i = 0; i < MODULE_CODE; ++i) {
        module[MODULE_CONTROL + i] = (uint8_t)(i * 7u + 3u);
    }
}

/// What ScarletNX's nvnProgramSetShaders asks of a section before it takes the stage (its
/// nvn_object.c, stage_of, as of edee39302), on the bytes alone: the offsets and the conditions
/// are written out here, apart from the writer's own constants.
static bool ScarletNxTakes(const uint8_t *section) {
    const uint32_t magic = Read(section, 0x000);
    const uint32_t one = Read(section, 0x004);
    const uint32_t format = Read(section, 0x008);
    const uint32_t constants = Read(section, 0x700);
    const uint32_t size = Read(section, 0x704);
    const uint32_t program = Read(section, 0x708);
    const uint32_t stage = Read(section, 0x714);
    return magic == 0x98761234u && one == 1u && format >= 5u && format <= 16u && stage <= 5u &&
           (program >= 0x30u || stage == 5u) && constants >= program && size >= constants &&
           (constants & 0xffu) == 0u && (size & 0xffu) == 0u && size >> 24 == 0u;
}

static void TestControl(void) {
    _Alignas(8) uint8_t section[NVN_LAYER_CONTROL_BYTES];
    uint8_t module[MODULE_BYTES];
    NvnLayerShaderCode code;

    // A vertex program with no constants of its own: they start where the code area ends.
    Module(module, 0, 0);
    memset(section, 0xee, sizeof(section));
    NvnLayerClearError();
    EXPECT(NvnLayerShaderControl(module, sizeof(module), section, &code), "a vertex module: %s",
           NvnLayerError());
    EXPECT(ScarletNxTakes(section), "ScarletNX's conditions refuse the vertex section");
    EXPECT(Read(section, 0x714) == 0u && Read(section, 0x708) == 0x30u &&
               Read(section, 0x700) == MODULE_CODE && Read(section, 0x704) == MODULE_CODE,
           "stage %u, program at 0x%x, constants at 0x%x of 0x%x", (unsigned)Read(section, 0x714),
           (unsigned)Read(section, 0x708), (unsigned)Read(section, 0x700),
           (unsigned)Read(section, 0x704));
    EXPECT(code.code == module + MODULE_CONTROL && code.code_bytes == MODULE_CODE,
           "the code is said to be %u bytes at offset %td", (unsigned)code.code_bytes,
           (const uint8_t *)code.code - module);
    // The two words a game's driver checks before it takes the program (its nvnProgramSetShaders,
    // API 55.8): 0x120 and 0xb, which a section of the console's compiler has there.
    EXPECT(Read(section, 0x00c) == 0x120u && Read(section, 0x010) == 0xbu,
           "the words a game's driver checks are 0x%x and 0x%x, not 0x120 and 0xb",
           (unsigned)Read(section, 0x00c), (unsigned)Read(section, 0x010));
    // Everything ScarletNX reads past those fields is nought: the early tests' byte, the capture
    // layout from 0x6dc, the tessellation words at 0x738, the subroutines' byte at 0x7c0.
    for (uint32_t at = 0x014; at < 0x800u; ++at) {
        if (at >= 0x700u && at < 0x70cu) {
            continue;
        }
        if (at >= 0x714u && at < 0x718u) {
            continue;
        }
        EXPECT(section[at] == 0u, "byte 0x%x of the vertex section is 0x%02x", (unsigned)at,
               section[at]);
    }
    // The layer's own part: the DKSH's header with its program's right after it.
    EXPECT(Read(section, 0x800) == 0x53434c4eu, "no tag on the layer's part");
    EXPECT(Read(section, 0x808) == 0x48534b44u && Read(section, 0x808 + 16) == 0x18u &&
               Read(section, 0x808 + 20) == 1u && Read(section, 0x808 + 12) == MODULE_CODE,
           "the DKSH header in the section is not the module's with its program after it");
    EXPECT(Read(section, 0x820) == 0u && Read(section, 0x824) == 0x30u &&
               Read(section, 0x828) == 7u,
           "the program's header in the section is not the module's");

    // A fragment program with constants and the early tests asked for.
    Module(module, 1, 0x40u);
    module[0x18 + 25] = 1;
    EXPECT(NvnLayerShaderControl(module, sizeof(module), section, &code), "a fragment module: %s",
           NvnLayerError());
    EXPECT(ScarletNxTakes(section), "ScarletNX's conditions refuse the fragment section");
    EXPECT(Read(section, 0x714) == 1u && Read(section, 0x700) == 0x200u &&
               Read(section, 0x704) == MODULE_CODE && section[0x718] == 1u,
           "stage %u, constants at 0x%x of 0x%x, early tests %u", (unsigned)Read(section, 0x714),
           (unsigned)Read(section, 0x700), (unsigned)Read(section, 0x704), section[0x718]);
    module[0x18 + 25] = 0;
    EXPECT(NvnLayerShaderControl(module, sizeof(module), section, &code) && section[0x718] == 0u,
           "a fragment module without the early tests says %u", section[0x718]);

    // A tessellation evaluation program of quads, fractional even spacing and the second
    // winding, then one of triangles that makes points: its declarations as GLSL's words.
    Module(module, 4, 0);
    Write(module, 0x18 + 24, 2u | 2u << 4 | 3u << 8);
    EXPECT(NvnLayerShaderControl(module, sizeof(module), section, &code),
           "a tessellation evaluation module: %s", NvnLayerError());
    EXPECT(Read(section, 0x714) == 4u && Read(section, 0x738) == 7u &&
               Read(section, 0x73c) == 0x8e7cu && Read(section, 0x740) == 0u &&
               Read(section, 0x744) == 0x900u,
           "stage %u declares domain 0x%x, spacing 0x%x, points %u and winding 0x%x",
           (unsigned)Read(section, 0x714), (unsigned)Read(section, 0x738),
           (unsigned)Read(section, 0x73c), (unsigned)Read(section, 0x740),
           (unsigned)Read(section, 0x744));
    Write(module, 0x18 + 24, 1u);
    EXPECT(NvnLayerShaderControl(module, sizeof(module), section, &code) &&
               Read(section, 0x738) == 4u && Read(section, 0x73c) == 0x202u &&
               Read(section, 0x740) == 1u && Read(section, 0x744) == 0x901u,
           "a module of triangles that makes points declares domain 0x%x, spacing 0x%x, points "
           "%u and winding 0x%x",
           (unsigned)Read(section, 0x738), (unsigned)Read(section, 0x73c),
           (unsigned)Read(section, 0x740), (unsigned)Read(section, 0x744));
    // A compute program of 8 by 8 by 1 with 0x100 bytes of shared memory and 0x40 of constants,
    // starting where the code area does: its workgroup, its shared memory and its constants'
    // bytes where ScarletNX reads a compute section's.
    Module(module, 5, 0x40u);
    Write(module, 0x18 + 4, 0u);
    Write(module, 0x18 + 24, 8u);
    Write(module, 0x18 + 28, 8u);
    Write(module, 0x18 + 32, 1u);
    Write(module, 0x18 + 36, 0x100u);
    NvnLayerClearError();
    EXPECT(NvnLayerShaderControl(module, sizeof(module), section, &code), "a compute module: %s",
           NvnLayerError());
    EXPECT(ScarletNxTakes(section), "ScarletNX's conditions refuse the compute section");
    EXPECT(Read(section, 0x714) == 5u && Read(section, 0x708) == 0u &&
               Read(section, 0x700) == 0x200u && Read(section, 0x748) == 8u &&
               Read(section, 0x74c) == 8u && Read(section, 0x750) == 1u &&
               Read(section, 0x754) == 0x100u && Read(section, 0x6fc) == 0x40u,
           "stage %u at 0x%x, constants at 0x%x of 0x%x bytes, a workgroup of %u by %u by %u and "
           "0x%x bytes shared",
           (unsigned)Read(section, 0x714), (unsigned)Read(section, 0x708),
           (unsigned)Read(section, 0x700), (unsigned)Read(section, 0x6fc),
           (unsigned)Read(section, 0x748), (unsigned)Read(section, 0x74c),
           (unsigned)Read(section, 0x750), (unsigned)Read(section, 0x754));
    // A graphics module writes none of those words.
    Module(module, 1, 0x40u);
    EXPECT(NvnLayerShaderControl(module, sizeof(module), section, &code) &&
               Read(section, 0x748) == 0u && Read(section, 0x754) == 0u &&
               Read(section, 0x6fc) == 0u,
           "a fragment module's section has a workgroup of %u and 0x%x bytes shared and 0x%x of "
           "constants",
           (unsigned)Read(section, 0x748), (unsigned)Read(section, 0x754),
           (unsigned)Read(section, 0x6fc));

    // A capture of a vertex stage's outputs, where ScarletNX reads it: the position's first
    // three words, two of generic output 0 and four of generic output 1 into buffer 0.
    Module(module, 0, 0);
    EXPECT(NvnLayerShaderControl(module, sizeof(module), section, &code), "a vertex module: %s",
           NvnLayerError());
    static const NvnLayerCaptureGroup vertex[3] = { { NVN_LAYER_CAPTURE_POSITION, 0, 3 },
                                                    { NVN_LAYER_CAPTURE_GENERIC(0), 0, 2 },
                                                    { NVN_LAYER_CAPTURE_GENERIC(1), 0, 4 } };
    NvnLayerClearError();
    EXPECT(NvnLayerShaderCapture(section, 0, vertex, 3), "a capture of three groups: %s",
           NvnLayerError());
    EXPECT(section[0x6ec] == 1u && section[0x6e8] == 3u && section[0x6e4] == 0u &&
               section[0x6dc] == 36u && section[0x6dd] == 0u,
           "the capture says on %u, %u groups, stream %u, a stride of %u", section[0x6ec],
           section[0x6e8], section[0x6e4], (unsigned)(section[0x6dc] | section[0x6dd] << 8));
    EXPECT(section[0xdc] == 0u && section[0xdd] == 0x20u && section[0xde] == 0x21u &&
               section[0x2dc] == 0u && section[0x4dc] == 3u && section[0x4dd] == 2u &&
               section[0x4de] == 4u,
           "the groups' outputs are %u, %u, %u and their counts %u, %u, %u", section[0xdc],
           section[0xdd], section[0xde], section[0x4dc], section[0x4dd], section[0x4de]);
    EXPECT(ScarletNxTakes(section), "ScarletNX's conditions refuse the capturing section");
    static const NvnLayerCaptureGroup past[1] = { { NVN_LAYER_CAPTURE_POSITION, 2, 3 } };
    NvnLayerClearError();
    EXPECT(!NvnLayerShaderCapture(section, 0, past, 1) &&
               strstr(NvnLayerError(), "3 words from word 2") != NULL,
           "three words from word 2 of an output: %s", NvnLayerError());
    NvnLayerClearError();
    EXPECT(!NvnLayerShaderCapture(section, 4, vertex, 3) &&
               strstr(NvnLayerError(), "buffer 4") != NULL,
           "a capture into buffer 4: %s", NvnLayerError());

    // What is refused, each with the value given in its sentence.
    static const struct {
        const char *what;
        uint32_t at;
        uint32_t value;
        const char *names;
    } BAD[] = {
        { "another magic", 0, 0x12345678u, "do not start a DKSH" },
        { "two programs", 20, 2u, "2 programs" },
        { "a code section off a 0x100 boundary", 12, 0x2f0u, "0x2f0" },
        { "a code section past the module", 12, 0x400u, "0x400" },
        { "a program table past the control section", 16, 0xf0u, "0xf0" },
        { "a program of a stage NVN has not", 0x18, 6u, "type 6" },
        { "a program that starts before 0x30", 0x18 + 4, 0x20u, "0x20" },
        { "a program that starts past its code", 0x18 + 4, 0x300u, "0x300" },
        { "constants off a 0x100 boundary", 0x18 + 12, 0x180u, "0x180" },
        { "constants past the code", 0x18 + 16, 0x200u, "0x200" },
    };
    for (size_t i = 0; i < sizeof(BAD) / sizeof(BAD[0]); ++i) {
        Module(module, 1, 0x40u);
        Write(module, BAD[i].at, BAD[i].value);
        NvnLayerClearError();
        EXPECT(!NvnLayerShaderControl(module, sizeof(module), section, &code), "%s was taken",
               BAD[i].what);
        EXPECT(strstr(NvnLayerError(), BAD[i].names) != NULL, "%s: \"%s\" does not say %s",
               BAD[i].what, NvnLayerError(), BAD[i].names);
    }
    Module(module, 0, 0);
    Write(module, 0x18 + 24, 0x180u);
    NvnLayerClearError();
    EXPECT(!NvnLayerShaderControl(module, sizeof(module), section, &code) &&
               strstr(NvnLayerError(), "0x180") != NULL,
           "a vertex program with a second entry point: \"%s\"", NvnLayerError());
    NvnLayerClearError();
    EXPECT(!NvnLayerShaderControl(module, 0x10, section, &code) &&
               strstr(NvnLayerError(), "16 bytes") != NULL,
           "a module cut to 16 bytes: \"%s\"", NvnLayerError());
    NvnLayerClearError();
    EXPECT(!NvnLayerShaderControl(module, sizeof(module), section + 4, &code),
           "a section off its 8-byte boundary was taken");
    NvnLayerClearError();
}
#endif

// ---- the objects' sizes

/// A window builder holds what the build's NVN writes into it: the Ounce's keeps a colour space
/// and twelve floats of HDR metadata from 0x2c on, which its nvnWindowBuilderSetDefaults fills.
static void TestSizes(void) {
#if NVN_LAYER_API >= 231
    const size_t builder = 0x2c + sizeof(int32_t) + 12 * sizeof(float);
#else
    const size_t builder = 0x40;
#endif
    EXPECT(sizeof(NVNwindowBuilder) == builder,
           "NVNwindowBuilder is 0x%zx bytes, where API %d's NVN writes 0x%zx",
           sizeof(NVNwindowBuilder), NVN_LAYER_API, builder);
}

int main(void) {
    TestSizes();
    TestLookup();
    TestOwn();
    TestLoad();
    TestVersion();
    TestError();
    TestGuard();
#if NVN_LAYER_API >= 231
    TestBodies();
#else
    TestControl();
#endif
    if (failures != 0) {
        printf("nvnlayer (API %d): %d failed\n", NVN_LAYER_API, failures);
        return EXIT_FAILURE;
    }
    printf("nvnlayer (API %d): all passed, %zu names\n", NVN_LAYER_API, NAME_COUNT);
    return EXIT_SUCCESS;
}
