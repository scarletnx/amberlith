// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// The libc family's judges whose rule is more than one comparison: the records a case hands the
/// library and the verdict on what it hands back. Touches no library function but snprintf, so
/// the reference suite pins all of it on the host with answers worked out by hand.

/// A record of three bytes, so the library moves it with a swap of no word's size: a key, the
/// record's own number and a mark that only that number has, which together show whether the
/// three bytes still belong to one record.
typedef struct {
    uint8_t key;
    uint8_t id;
    uint8_t mark;
} LibcRecord;

_Static_assert(sizeof(LibcRecord) == 3, "a record is three bytes on every machine");

/// The records of the tied sort case, and how many keys they share: each key four times.
#define LIBC_TIED_RECORDS 16u
#define LIBC_TIED_KEYS 4u

/// The key and the mark record @p id carries: the key (id * 3) % 4, so the records arrive with
/// their keys mixed, and the mark 0xA0 plus the id.
uint8_t LibcTiedKey(uint8_t id);
uint8_t LibcTiedMark(uint8_t id);

/// The records in the order the case hands them to qsort: record i is id i.
void LibcTiedRecords(LibcRecord out[LIBC_TIED_RECORDS]);

/// What qsort left of LibcTiedRecords' records, sorted by key alone. Passes when every record is
/// whole (its key and mark are its id's), every id is there once, and the keys never go down.
/// The order of records with one key is the library's to choose (C11 7.22.5.2: equal elements are
/// left in an unspecified order), so a stable sort and one that reverses the ties both pass.
/// The sentence names the first thing wrong, or the order the ties came in, into @p out.
bool LibcJudgeTiedSort(const LibcRecord sorted[LIBC_TIED_RECORDS], char *out, size_t out_size);

#ifdef __cplusplus
}
#endif
