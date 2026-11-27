// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>

#include "libcref.h"

uint8_t LibcTiedKey(uint8_t id) {
    return (uint8_t)((id * 3u) % LIBC_TIED_KEYS);
}

uint8_t LibcTiedMark(uint8_t id) {
    return (uint8_t)(0xA0u + id);
}

void LibcTiedRecords(LibcRecord out[LIBC_TIED_RECORDS]) {
    for (uint8_t id = 0; id < LIBC_TIED_RECORDS; ++id) {
        out[id] = (LibcRecord){ .key = LibcTiedKey(id), .id = id, .mark = LibcTiedMark(id) };
    }
}

bool LibcJudgeTiedSort(const LibcRecord sorted[LIBC_TIED_RECORDS], char *out, size_t out_size) {
    unsigned times[LIBC_TIED_RECORDS] = { 0 };
    for (unsigned i = 0; i < LIBC_TIED_RECORDS; ++i) {
        const LibcRecord r = sorted[i];
        if (r.id >= LIBC_TIED_RECORDS) {
            snprintf(out, out_size,
                     "record %u torn: id %u, key %u, mark 0x%02x, and no record of 0 to %u has "
                     "that id",
                     i, r.id, r.key, r.mark, LIBC_TIED_RECORDS - 1u);
            return false;
        }
        if (r.key != LibcTiedKey(r.id) || r.mark != LibcTiedMark(r.id)) {
            snprintf(out, out_size,
                     "record %u torn: id %u with key %u and mark 0x%02x, handed over with key %u "
                     "and mark 0x%02x",
                     i, r.id, r.key, r.mark, LibcTiedKey(r.id), LibcTiedMark(r.id));
            return false;
        }
        ++times[r.id];
        if (i > 0u && r.key < sorted[i - 1u].key) {
            snprintf(out, out_size, "record %u has key %u after key %u: the keys go down", i,
                     r.key, sorted[i - 1u].key);
            return false;
        }
    }
    // Sixteen whole records: an id missing means another is there more than once.
    unsigned missing = LIBC_TIED_RECORDS, twice = LIBC_TIED_RECORDS;
    for (unsigned id = 0; id < LIBC_TIED_RECORDS; ++id) {
        if (times[id] == 0u && missing == LIBC_TIED_RECORDS) {
            missing = id;
        }
        if (times[id] > 1u && twice == LIBC_TIED_RECORDS) {
            twice = id;
        }
    }
    if (missing < LIBC_TIED_RECORDS) {
        snprintf(out, out_size, "id %u is there %u times and id %u not at all", twice,
                 twice < LIBC_TIED_RECORDS ? times[twice] : 0u, missing);
        return false;
    }
    int at = snprintf(out, out_size, "each record whole and once, the keys in order; the ties as");
    for (unsigned i = 0; i < LIBC_TIED_RECORDS && at > 0 && (size_t)at < out_size; ++i) {
        at += snprintf(out + at, out_size - (size_t)at, "%s%u",
                       i > 0u && sorted[i].key != sorted[i - 1u].key ? " | " : " ", sorted[i].id);
    }
    return true;
}
