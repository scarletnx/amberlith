// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string.h>

#include "host.h"

// The two readings are of bytes alone, so the host build tests them (mod/tests/host_test.c);
// the memory query is the console's.

/// Where @p needle of @p needle_size starts in @p bytes at or after @p from, or @p size.
static size_t Find(const uint8_t *bytes, size_t size, size_t from, const void *needle,
                   size_t needle_size) {
    for (size_t at = from; needle_size <= size && at <= size - needle_size; ++at) {
        if (bytes[at] == *(const uint8_t *)needle && memcmp(bytes + at, needle, needle_size) == 0) {
            return at;
        }
    }
    return size;
}

bool HostSdkVersion(const uint8_t *bytes, size_t size, char out[HOST_SDK_VERSION_MAX + 1u]) {
    static const char LABEL[] = HOST_SDK_LABEL;
    const size_t label = sizeof(LABEL) - 1u;
    out[0] = '\0';
    const size_t at = Find(bytes, size, 0, LABEL, label);
    if (at == size) {
        return false;
    }
    // Digits with an underscore between two numbers, up to the build kind's dash or the end.
    size_t length = 0;
    for (size_t i = at + label; i < size; ++i) {
        const char c = (char)bytes[i];
        if (!(c >= '0' && c <= '9') && c != '_') {
            break;
        }
        if (length == HOST_SDK_VERSION_MAX) {
            out[0] = '\0';
            return false;
        }
        out[length++] = c == '_' ? '.' : c;
    }
    out[length] = '\0';
    return length != 0u;
}

bool HostBuildId(const uint8_t *bytes, size_t size, char out[HOST_BUILD_ID_TEXT]) {
    // An ELF note: the name's size (4, "GNU" and its end), the id's size, the type (3, a build
    // id), the name, the id. All little endian, as the console is.
    static const uint8_t NAME[] = { 3, 0, 0, 0, 'G', 'N', 'U', 0 };
    out[0] = '\0';
    size_t found = size;
    uint32_t found_bytes = 0;
    for (size_t at = Find(bytes, size, 8u, NAME, sizeof(NAME)); at < size;
         at = Find(bytes, size, at + 1u, NAME, sizeof(NAME))) {
        uint32_t name_bytes;
        uint32_t id_bytes;
        memcpy(&name_bytes, bytes + at - 8u, sizeof(name_bytes));
        memcpy(&id_bytes, bytes + at - 4u, sizeof(id_bytes));
        if (name_bytes == 4u && id_bytes != 0u && id_bytes <= HOST_BUILD_ID_MAX &&
            id_bytes <= size - at - sizeof(NAME)) {
            found = at + sizeof(NAME);
            found_bytes = id_bytes;
        }
    }
    if (found == size) {
        return false;
    }
    static const char DIGITS[] = "0123456789abcdef";
    for (uint32_t i = 0; i < found_bytes; ++i) {
        out[2u * i] = DIGITS[bytes[found + i] >> 4];
        out[2u * i + 1u] = DIGITS[bytes[found + i] & 15u];
    }
    out[2u * found_bytes] = '\0';
    return true;
}

#ifdef __SWITCH__
#include <switch.h>

bool HostModuleData(uintptr_t function, const uint8_t **bytes, size_t *size) {
    MemoryInfo info = { 0 };
    u32 page = 0;
    if (R_FAILED(svcQueryMemory(&info, &page, function)) || info.type != MemType_CodeStatic ||
        (info.perm & Perm_X) == 0u) {
        return false;
    }
    // A module is its code, its read-only data and its data, one mapping after the other.
    const u64 after = info.addr + info.size;
    if (R_FAILED(svcQueryMemory(&info, &page, after)) || info.type != MemType_CodeStatic ||
        info.perm != Perm_R) {
        return false;
    }
    *bytes = (const uint8_t *)(uintptr_t)info.addr;
    *size = (size_t)info.size;
    return true;
}
#endif
