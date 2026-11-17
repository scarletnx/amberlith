// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <string.h>

#include "../host.h"

// What the mod reads of the game's SDK module (mod/host.c), on bytes made here: host.mk builds
// and runs it.

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

static uint8_t image[512];
static size_t image_size;

static void Reset(void) {
    memset(image, 0xcc, sizeof(image));
    image_size = 0;
}

static void Put(const void *bytes, size_t size) {
    memcpy(image + image_size, bytes, size);
    image_size += size;
}

static void PutNote(uint32_t name_bytes, uint32_t type, const uint8_t *id, uint32_t id_bytes) {
    Put(&name_bytes, 4);
    Put(&id_bytes, 4);
    Put(&type, 4);
    Put("GNU", 4);
    Put(id, id_bytes);
}

static void TestSdkVersion(void) {
    static const struct {
        const char *text;
        bool found;
        const char *version;
    } CASES[] = {
        { "xx" HOST_SDK_LABEL "20_5_0-Release", true, "20.5.0" },
        { HOST_SDK_LABEL "3_5_1", true, "3.5.1" },
        { "SDK MW+Nintendo+NintendoSdk_nnSdk-0_12_16-Develop\0" HOST_SDK_LABEL "9_9_9", true,
          "0.12.16" },
        // another library's label, a label cut at its version, and none
        { "SDK MW+Nintendo+NintendoSdk_nvn-20_5_0-Release", false, "" },
        { HOST_SDK_LABEL "-Release", false, "" },
        { HOST_SDK_LABEL "1_2_3_4_5_6_7_8_9_10_11_12_13", false, "" },
        { "no label here", false, "" },
    };
    for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); ++i) {
        char version[HOST_SDK_VERSION_MAX + 1u] = "untouched";
        // The text with no end after it: a label that ends the data ends the version.
        const size_t size = i == 2u ? 80u : strlen(CASES[i].text);
        const bool found = HostSdkVersion((const uint8_t *)CASES[i].text, size, version);
        EXPECT(found == CASES[i].found && strcmp(version, CASES[i].version) == 0,
               "\"%s\": %s \"%s\", where it is %s \"%s\"", CASES[i].text,
               found ? "found" : "none", version, CASES[i].found ? "found" : "none",
               CASES[i].version);
    }
    char version[HOST_SDK_VERSION_MAX + 1u] = "untouched";
    EXPECT(!HostSdkVersion((const uint8_t *)"", 0, version) && version[0] == '\0',
           "no bytes gave \"%s\"", version);
}

static void TestBuildId(void) {
    static const uint8_t ID[20] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x10, 0x32,
                                    0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe, 0x00, 0xff, 0x0f, 0xf0 };
    static const uint8_t OLDER[4] = { 0xde, 0xad, 0xbe, 0xef };
    char text[HOST_BUILD_ID_TEXT] = "untouched";

    // The note at the end of the data, after bytes that are no note.
    Reset();
    Put("GNU", 4);
    Put("some read-only data", 19);
    PutNote(4, 3, ID, sizeof(ID));
    EXPECT(HostBuildId(image, image_size, text) &&
               strcmp(text, "0123456789abcdef1032547698badcfe00ff0ff0") == 0,
           "a 20 byte id read as \"%s\"", text);

    // Two notes: the last is the module's.
    Reset();
    PutNote(4, 3, OLDER, sizeof(OLDER));
    PutNote(4, 3, ID, 8);
    EXPECT(HostBuildId(image, image_size, text) && strcmp(text, "0123456789abcdef") == 0,
           "the last of two notes read as \"%s\"", text);

    // No note: another type, a name of another size, an id of nought, of too many bytes, and
    // one the data ends inside.
    Reset();
    PutNote(4, 1, ID, sizeof(ID));
    EXPECT(!HostBuildId(image, image_size, text) && text[0] == '\0', "a type 1 note gave \"%s\"",
           text);
    Reset();
    PutNote(5, 3, ID, sizeof(ID));
    EXPECT(!HostBuildId(image, image_size, text), "a name of 5 bytes gave \"%s\"", text);
    Reset();
    PutNote(4, 3, ID, 0);
    EXPECT(!HostBuildId(image, image_size, text), "an id of no bytes gave \"%s\"", text);
    Reset();
    PutNote(4, 3, ID, sizeof(ID));
    const uint32_t too_many = HOST_BUILD_ID_MAX + 1u;
    memcpy(image + 4, &too_many, 4);
    EXPECT(!HostBuildId(image, image_size, text), "an id of %u bytes gave \"%s\"",
           (unsigned)too_many, text);
    Reset();
    PutNote(4, 3, ID, sizeof(ID));
    EXPECT(!HostBuildId(image, image_size - 1u, text), "an id cut by the data's end gave \"%s\"",
           text);
    EXPECT(!HostBuildId(image, 0, text) && !HostBuildId(image, 7, text), "no data gave \"%s\"",
           text);
}

static void TestLacks(void) {
    const HostName NAMES[] = {
        { "nn::vi::Initialize", true },
        { "nn::vi::OpenDefaultDisplay", false },
        { "nn::vi::CreateLayer", true },
        { "nn::vi::GetNativeWindow", false },
    };
    char out[64] = "untouched";
    EXPECT(HostLacks(out, sizeof(out), NAMES, 1) == 0u && out[0] == '\0',
           "a game with every name lacked \"%s\"", out);
    EXPECT(HostLacks(out, sizeof(out), NAMES, 4) == 2u &&
               strcmp(out, "nn::vi::OpenDefaultDisplay, nn::vi::GetNativeWindow") == 0,
           "two lacked read as \"%s\"", out);
    char small[36] = "untouched";
    EXPECT(HostLacks(small, sizeof(small), NAMES, 4) == 2u &&
               strcmp(small, "nn::vi::OpenDefaultDisplay, ...") == 0,
           "two lacked in 36 bytes read as \"%s\"", small);
    char tiny[8] = "touched";
    EXPECT(HostLacks(tiny, sizeof(tiny), NAMES, 4) == 2u && strcmp(tiny, "...") == 0,
           "two lacked in 8 bytes read as \"%s\"", tiny);
    char none[1] = "";
    EXPECT(HostLacks(none, 0, NAMES, 4) == 2u, "no room still counted the lacked");
}

int main(void) {
    TestSdkVersion();
    TestBuildId();
    TestLacks();
    if (failures != 0) {
        printf("mod host test: %d failed\n", failures);
        return 1;
    }
    printf("mod host test: ok\n");
    return 0;
}
