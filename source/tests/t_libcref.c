// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../check.h"
#include "../libcref.h"
#include "../report.h"
#include "tests.h"

// The libc family's judges against answers worked out by hand: the records the tied sort case
// hands qsort, and its verdict on what a library may hand back and on what a broken one does.

static const char *const SUITE = "reference";

/// The ids in key order with each key's records in the order they were handed over, which a
/// stable sort leaves: key 0 is ids 0 4 8 12, key 1 ids 3 7 11 15, key 2 ids 2 6 10 14 and key
/// 3 ids 1 5 9 13, from (id * 3) % 4.
static const uint8_t STABLE[LIBC_TIED_RECORDS] = { 0, 4, 8, 12, 3, 7, 11, 15,
                                                   2, 6, 10, 14, 1, 5, 9, 13 };

static void Lay(LibcRecord out[LIBC_TIED_RECORDS], const uint8_t ids[LIBC_TIED_RECORDS]) {
    for (unsigned i = 0; i < LIBC_TIED_RECORDS; ++i) {
        out[i] = (LibcRecord){ .key = LibcTiedKey(ids[i]), .id = ids[i],
                               .mark = LibcTiedMark(ids[i]) };
    }
}

static void CheckTiedRecords(void) {
    static const uint8_t KEYS[LIBC_TIED_RECORDS] = { 0, 3, 2, 1, 0, 3, 2, 1,
                                                     0, 3, 2, 1, 0, 3, 2, 1 };
    LibcRecord handed[LIBC_TIED_RECORDS];
    LibcTiedRecords(handed);
    unsigned wrong = LIBC_TIED_RECORDS;
    for (unsigned i = 0; i < LIBC_TIED_RECORDS && wrong == LIBC_TIED_RECORDS; ++i) {
        if (handed[i].key != KEYS[i] || handed[i].id != i || handed[i].mark != 0xA0u + i) {
            wrong = i;
        }
    }
    const LibcRecord seen = handed[wrong < LIBC_TIED_RECORDS ? wrong : 0u];
    CHECK_TRUE(SUITE, "libc: the tied sort hands over ids 0 to 15, four to a key, marks from 0xa0",
               CHECK_NONE, wrong == LIBC_TIED_RECORDS,
               "record %u is key %u, id %u, mark 0x%02x", wrong, seen.key, seen.id, seen.mark);
}

static void CheckTiedJudge(void) {
    LibcRecord sorted[LIBC_TIED_RECORDS];
    char said[160];

    // A stable sort keeps the order the ties were handed over in: glibc 2.43's qsort does here.
    Lay(sorted, STABLE);
    const bool stable = LibcJudgeTiedSort(sorted, said, sizeof(said)) &&
                        strcmp(said, "each record whole and once, the keys in order; the ties as "
                                     "0 4 8 12 | 3 7 11 15 | 2 6 10 14 | 1 5 9 13") == 0;
    // A quicksort may leave them in any order: here each key's reversed.
    static const uint8_t REVERSED[LIBC_TIED_RECORDS] = { 12, 8, 4, 0, 15, 11, 7, 3,
                                                         14, 10, 6, 2, 13, 9, 5, 1 };
    Lay(sorted, REVERSED);
    const bool reversed = LibcJudgeTiedSort(sorted, said, sizeof(said)) &&
                          strstr(said, "the ties as 12 8 4 0 | 15 11 7 3 |") != NULL;

    // A swap that moved two of a record's three bytes: records 3 and 4 trade key and id, and
    // each keeps the other's mark.
    Lay(sorted, STABLE);
    const LibcRecord third = sorted[3];
    sorted[3].key = sorted[4].key;
    sorted[3].id = sorted[4].id;
    sorted[4].key = third.key;
    sorted[4].id = third.id;
    const bool torn = !LibcJudgeTiedSort(sorted, said, sizeof(said)) &&
                      strcmp(said, "record 3 torn: id 3 with key 1 and mark 0xac, handed over "
                                   "with key 1 and mark 0xa3") == 0;
    // A byte from outside the array where an id goes.
    Lay(sorted, STABLE);
    sorted[9].id = 200u;
    const bool stranger = !LibcJudgeTiedSort(sorted, said, sizeof(said)) &&
                          strstr(said, "record 9 torn: id 200,") == said;
    // A swap that copied one side over the other without keeping it: id 4 lost, id 0 twice.
    Lay(sorted, STABLE);
    sorted[1] = sorted[0];
    const bool lost = !LibcJudgeTiedSort(sorted, said, sizeof(said)) &&
                      strcmp(said, "id 0 is there 2 times and id 4 not at all") == 0;
    // A qsort that did nothing hands the records back as they came.
    LibcTiedRecords(sorted);
    const bool unsorted = !LibcJudgeTiedSort(sorted, said, sizeof(said)) &&
                          strcmp(said, "record 2 has key 2 after key 3: the keys go down") == 0;

    CHECK_TRUE(SUITE,
               "libc: the tied sort judge takes the ties in any order and refuses a torn, lost or "
               "unsorted record",
               CHECK_NONE, stable && reversed && torn && stranger && lost && unsorted,
               "stable %d, reversed %d, torn %d, stranger %d, lost %d, unsorted %d; last said "
               "\"%s\"",
               stable, reversed, torn, stranger, lost, unsorted, said);
}

void RunReferenceLibc(void) {
    CheckTiedRecords();
    CheckTiedJudge();
}
