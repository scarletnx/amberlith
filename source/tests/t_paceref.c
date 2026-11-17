// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../check.h"
#include "../paceref.h"
#include "../report.h"
#include "tests.h"

// The pace family's arithmetic against numbers worked out apart: what a case's samples come to
// and the sentence its line carries, by hand; what the processor's loops and the memory walk
// leave, by tools/pace-expect.py, which restates them in another language and shares no code
// with paceref.c.

static const char *const SUITE = "reference";

// BEGIN tools/pace-expect.py --c
static const struct {
    uint64_t iterations;
    uint64_t sums[2];
} INTEGER[] = {
    { 1ull, { 0xad84ff6e5d4c88a6ull, 0xfedcba99114d5b09ull } },
    { 2ull, { 0x59e6b96f7c244620ull, 0xac61ba011ed7bd7aull } },
    { 1000ull, { 0x3ea33c7a6b2e389dull, 0xa24cea4dca001397ull } },
    { 262144ull, { 0x8a8d1cbc8cf72bddull, 0x70bb900e20a6f0a6ull } },
}, FLOATING[] = {
    { 1ull, { 13034428ull, 3742788ull } },
    { 2ull, { 25848041ull, 3963603ull } },
    { 1000ull, { 12554678068ull, 15234529ull } },
    { 65536ull, { 823657919430ull, 4796473ull } },
};
static const struct {
    uint64_t iterations;
    uint32_t sums[12];
} VECTOR[] = {
    { 1ull,
      { 0x0001e468u, 0x00001dfdu, 0x00062012u, 0x000101e9u,
        0x3c88596cu, 0x02b49c6bu, 0xc5531b80u, 0x20221015u,
        42u, 2137u, 2698u, 249u } },
    { 2ull,
      { 0x0004d54bu, 0x000812fcu, 0x0006fa8eu, 0x0003c1f0u,
        0x5e8885dbu, 0xffaafc28u, 0x1bc3e5b9u, 0x599d246au,
        1191u, 2472u, 1123u, 3310u } },
    { 1000ull,
      { 0x0ff86b97u, 0x0f61fc87u, 0x0f3b110du, 0x0f593187u,
        0x26799aa9u, 0x73901b9au, 0xc2d9809bu, 0xc7f8d31cu,
        3481u, 734u, 3685u, 3096u } },
    { 65536ull,
      { 0xfe9eeb88u, 0x013ee318u, 0x01dc3490u, 0xfeaf82e8u,
        0x38690001u, 0xc19f0002u, 0xc01f0003u, 0xe18f0004u,
        1u, 22u, 333u, 4000u } },
};
static const uint8_t WALK_OF_16[16] = { 15, 2, 9, 4, 3, 6, 13, 8, 7, 10, 1, 12, 11, 14, 5, 0 };
static const uint64_t WALK_END = 1310720ull;
// END tools/pace-expect.py --c

static void CheckStats(void) {
    uint64_t odd[5] = { 5, 1, 9, 3, 7 };
    PaceStats five;
    PaceStatsOf(odd, 5, &five);
    uint64_t even[4] = { 4, 1, 3, 2 };
    PaceStats four;
    PaceStatsOf(even, 4, &four);
    // 1 to 200 in an order no sort was given: the hundredths are the second and the 198th.
    static uint64_t many[200];
    for (uint32_t i = 0; i < 200u; ++i) {
        many[i] = (uint64_t)(i * 73u % 200u) + 1u;
    }
    PaceStats two_hundred;
    PaceStatsOf(many, 200, &two_hundred);
    // Samples at the ends of 64 bits, whose distances would not add up in one word.
    uint64_t wide[3] = { UINT64_MAX, 0, UINT64_MAX };
    PaceStats top;
    PaceStatsOf(wide, 3, &top);
    PaceStats none;
    memset(&none, 0xff, sizeof(none));
    PaceStatsOf(NULL, 0, &none);
    const bool small = five.count == 5u && five.median == 5u && five.least == 1u &&
                       five.most == 9u && five.low == 1u && five.high == 9u &&
                       five.scatter == 2u && four.median == 2u && four.least == 1u &&
                       four.most == 4u && four.scatter == 1u;
    const bool large = two_hundred.median == 100u && two_hundred.low == 2u &&
                       two_hundred.high == 198u && two_hundred.scatter == 50u &&
                       top.median == UINT64_MAX && top.scatter == UINT64_MAX / 3u &&
                       none.count == 0u && none.median == 0u && none.most == 0u;
    CHECK_TRUE(SUITE,
               "pace: the middle, the ends, the hundredths and the scatter of a case's samples",
               CHECK_NONE, small && large,
               "of 5: median %llu, %llu to %llu, scatter %llu; of 4: median %llu, scatter %llu; "
               "of 200: median %llu, hundredths %llu and %llu, scatter %llu; wide scatter %llu",
               (unsigned long long)five.median, (unsigned long long)five.least,
               (unsigned long long)five.most, (unsigned long long)five.scatter,
               (unsigned long long)four.median, (unsigned long long)four.scatter,
               (unsigned long long)two_hundred.median, (unsigned long long)two_hundred.low,
               (unsigned long long)two_hundred.high, (unsigned long long)two_hundred.scatter,
               (unsigned long long)top.scatter);
}

static void CheckMulDiv(void) {
    static const struct {
        uint64_t a, b, c, want;
    } ROWS[] = {
        { 6u, 7u, 2u, 21u },
        { 7u, 7u, 2u, 24u },
        // 10^18 * 10^9 / 10^10: a product of 90 bits.
        { 1000000000000000000ull, 1000000000ull, 10000000000ull, 100000000000000000ull },
        // (2^64 - 1)^2 over 2^64 - 1, and over 2^63, which does not fit.
        { UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX },
        { UINT64_MAX, UINT64_MAX, 1ull << 63, UINT64_MAX },
        // 2^63 * 6 / 4 is 3 * 2^62, which fits; over 3 it is 2^64, which does not.
        { 1ull << 63, 6u, 4u, 3ull << 62 },
        { 1ull << 63, 6u, 3u, UINT64_MAX },
        // A divisor past 2^63, where the remainder's doubling carries: 3 (2^64 - 1) over
        // 2^64 - 2.
        { UINT64_MAX, 3u, UINT64_MAX - 1u, 3u },
        { 5u, 5u, 0u, UINT64_MAX },
        { 0u, 5u, 5u, 0u },
    };
    uint32_t wrong = 0;
    uint32_t first = 0;
    for (uint32_t i = 0; i < sizeof(ROWS) / sizeof(ROWS[0]); ++i) {
        if (PaceMulDiv(ROWS[i].a, ROWS[i].b, ROWS[i].c) != ROWS[i].want) {
            first = wrong == 0u ? i : first;
            ++wrong;
        }
    }
    // 200 million iterations in a second: half a second for a hundred million, and the rate
    // that time gives back.
    const bool paces = PaceTimeFor(1000000000u, 200000000u, 100000000u) == 500000000u &&
                       PaceTimeFor(1000000000u, 0u, 100000000u) == UINT64_MAX &&
                       PaceRate(100000000u, 500000000u) == 200000000u &&
                       PaceRate(100000000u, 0u) == UINT64_MAX;
    CHECK_TRUE(SUITE,
               "pace: a product past 64 bits divides whole, and a window's work gives its time "
               "and its rate",
               CHECK_NONE, wrong == 0u && paces,
               "%u of the rows wrong, the first row %u gave %llu; time and rate %d",
               (unsigned)wrong, (unsigned)first,
               (unsigned long long)PaceMulDiv(ROWS[first].a, ROWS[first].b, ROWS[first].c),
               (int)paces);
}

static void CheckLine(void) {
    const bool numbers = PaceMicroseconds(0u) == 1u && PaceMicroseconds(499u) == 1u &&
                         PaceMicroseconds(1499u) == 1u && PaceMicroseconds(1500u) == 2u &&
                         PaceMicroseconds(489912345u) == 489912u &&
                         PaceMicroseconds(4294967295000ull) == UINT32_MAX &&
                         PaceMicroseconds(UINT64_MAX) == UINT32_MAX;
    // Ten windows, the times in milliseconds since the median is past ten of them.
    const PaceStats windows = { 10u,        489912345u, 489100000u, 491000000u,
                                489100000u, 491000000u, 400000u };
    char said[512];
    const size_t length =
        PaceDescribe(said, sizeof(said), "204.1 million iterations a second", &windows,
                     "windows of 1 s", "the time of 100 million iterations");
    static const char WINDOWS[] =
        "204.1 million iterations a second; 10 windows of 1 s: the time of 100 million "
        "iterations, the median 489.9 ms, 489.1 to 491.0 (0.3% apart); " PACE_NO_REFERENCE;
    // A thousand wakes in microseconds, with the hundredths and the scatter a hundred samples
    // bring; and nothing said before them.
    const PaceStats wakes = { 1000u, 12345u, 9800u, 250000u, 10100u, 31000u, 1260u };
    char many[512];
    PaceDescribe(many, sizeof(many), "", &wakes, "wakes", "the time of one");
    static const char WAKES[] =
        "1000 wakes: the time of one, the median 12.3 us, 9.8 to 250.0 (2451.0% apart), 98 in "
        "100 from 10.1 to 31.0, 1.2 from the median on average; " PACE_NO_REFERENCE;
    // Under ten microseconds the times are whole nanoseconds; a fastest sample of nought has
    // no percent.
    const PaceStats short_ones = { 3u, 52u, 0u, 104u, 0u, 104u, 34u };
    char brief[512];
    PaceDescribe(brief, sizeof(brief), NULL, &short_ones, "reads", "the time of one");
    static const char READS[] = "3 reads: the time of one, the median 52 ns, 0 to 104 (the "
                                "fastest took no time); " PACE_NO_REFERENCE;
    // Where the unit changes: a median just under ten microseconds and at them, just under ten
    // milliseconds and at them.
    static const struct {
        uint64_t median;
        const char *want;
    } UNITS[] = {
        { 9999u, "the median 9999 ns," },
        { 10000u, "the median 10.0 us," },
        { 9999999u, "the median 9999.9 us," },
        { 10000000u, "the median 10.0 ms," },
    };
    bool units = true;
    for (uint32_t i = 0; i < sizeof(UNITS) / sizeof(UNITS[0]); ++i) {
        const PaceStats one = { 1u, UNITS[i].median, UNITS[i].median, UNITS[i].median,
                                UNITS[i].median, UNITS[i].median, 0u };
        char unit[256];
        PaceDescribe(unit, sizeof(unit), "", &one, "runs", "one");
        units = units && strstr(unit, UNITS[i].want) != NULL &&
                strstr(unit, "(0.0% apart)") != NULL;
    }
    // A buffer too small ends the sentence where it must, terminated.
    char cut[16];
    const size_t kept = PaceDescribe(cut, sizeof(cut), "", &wakes, "wakes", "the time of one");
    const bool sentences = strcmp(said, WINDOWS) == 0 && length == sizeof(WINDOWS) - 1u &&
                           strcmp(many, WAKES) == 0 && strcmp(brief, READS) == 0 && units &&
                           kept == sizeof(cut) - 1u && strcmp(cut, "1000 wakes: the") == 0 &&
                           PaceDescribe(NULL, 0, "", &wakes, "wakes", "one") == 0u;
    CHECK_TRUE(SUITE, "pace: the number a line carries and the sentence beside it", CHECK_NONE,
               numbers && sentences,
               "numbers %d, units %d; said \"%.150s\"; \"%.150s\"; \"%.100s\"; cut \"%s\" (%u)",
               (int)numbers, (int)units, said, many, brief, cut, (unsigned)kept);
}

static void CheckLoops(void) {
    uint32_t wrong = 0;
    uint64_t got[2] = { 0, 0 };
    uint64_t first[2] = { 0, 0 };
    for (uint32_t i = 0; i < sizeof(INTEGER) / sizeof(INTEGER[0]); ++i) {
        PaceIntegerLoop(INTEGER[i].iterations, got);
        if (memcmp(got, INTEGER[i].sums, sizeof(got)) != 0) {
            if (wrong == 0u) {
                memcpy(first, got, sizeof(first));
            }
            ++wrong;
        }
    }
    CHECK_TRUE(SUITE, "pace: the integer loop leaves the sums worked out apart", CHECK_NONE,
               wrong == 0u && INTEGER[3].iterations == PACE_INTEGER_CHUNK,
               "%u of the rows wrong, the first gave 0x%016llx and 0x%016llx; the last row is "
               "%llu iterations and the chunk %u",
               (unsigned)wrong, (unsigned long long)first[0], (unsigned long long)first[1],
               (unsigned long long)INTEGER[3].iterations, (unsigned)PACE_INTEGER_CHUNK);

    wrong = 0;
    for (uint32_t i = 0; i < sizeof(FLOATING) / sizeof(FLOATING[0]); ++i) {
        PaceFloatLoop(FLOATING[i].iterations, got);
        if (memcmp(got, FLOATING[i].sums, sizeof(got)) != 0) {
            if (wrong == 0u) {
                memcpy(first, got, sizeof(first));
            }
            ++wrong;
        }
    }
    // The most iterations the sum stays whole for: each adds 2^24 at most, and a double keeps
    // every whole number to 2^53.
    const bool whole = (PACE_FLOAT_ITERATIONS_MAX << 24) <= (1ull << 53) &&
                       PACE_FLOAT_CHUNK <= PACE_FLOAT_ITERATIONS_MAX;
    CHECK_TRUE(SUITE, "pace: the floating point loop leaves the sums worked out apart",
               CHECK_NONE, wrong == 0u && whole && FLOATING[3].iterations == PACE_FLOAT_CHUNK,
               "%u of the rows wrong, the first gave %llu and %llu; the sum whole in a double %d; "
               "the last row is %llu iterations and the chunk %u",
               (unsigned)wrong, (unsigned long long)first[0], (unsigned long long)first[1],
               (int)whole, (unsigned long long)FLOATING[3].iterations,
               (unsigned)PACE_FLOAT_CHUNK);

    wrong = 0;
    uint32_t lanes[12];
    uint32_t first_lanes[12] = { 0 };
    for (uint32_t i = 0; i < sizeof(VECTOR) / sizeof(VECTOR[0]); ++i) {
        PaceVectorLoop(VECTOR[i].iterations, lanes);
        if (memcmp(lanes, VECTOR[i].sums, sizeof(lanes)) != 0) {
            if (wrong == 0u) {
                memcpy(first_lanes, lanes, sizeof(first_lanes));
            }
            ++wrong;
        }
    }
    // What keeps every single whole: the largest x times the largest A, plus the largest C.
    uint32_t widest = 0;
    for (uint32_t lane = 0; lane < 4u; ++lane) {
        const uint32_t product = 4095u * PACE_VECTOR_A[lane] + PACE_VECTOR_C[lane];
        widest = product > widest ? product : widest;
    }
    CHECK_TRUE(SUITE, "pace: the vector loop leaves the sums worked out apart", CHECK_NONE,
               wrong == 0u && widest < (1u << 24) && VECTOR[3].iterations == PACE_VECTOR_CHUNK,
               "%u of the rows wrong, the first gave sums 0x%08x 0x%08x 0x%08x 0x%08x; the widest "
               "single is %u, and 2^24 is %u; the last row is %llu iterations and the chunk %u",
               (unsigned)wrong, (unsigned)first_lanes[0], (unsigned)first_lanes[1],
               (unsigned)first_lanes[2], (unsigned)first_lanes[3], (unsigned)widest,
               (unsigned)(1u << 24), (unsigned long long)VECTOR[3].iterations,
               (unsigned)PACE_VECTOR_CHUNK);
}

static void CheckWalk(void) {
    uint32_t wrong = 0;
    uint64_t slot = 0;
    for (uint32_t i = 0; i < 16u; ++i) {
        slot = PaceWalkNext(slot, 16u);
        wrong += slot != WALK_OF_16[i] ? 1u : 0u;
    }
    // Every slot before the walk is back where it began: counted over a table small enough to
    // walk here, the same rule at any power of two. As many steps as there are slots, and no
    // more, so a rule that never comes back fails here instead of walking for ever.
    static uint8_t seen[1u << 12];
    memset(seen, 0, sizeof(seen));
    uint32_t visited = 0;
    slot = 0;
    for (uint32_t i = 0; i < sizeof(seen); ++i) {
        slot = PaceWalkNext(slot, sizeof(seen));
        visited += seen[slot] == 0u ? 1u : 0u;
        seen[slot] = 1u;
    }
    const uint64_t end = PaceWalkEnd(0u, PACE_WALK_STEPS, PACE_WALK_SLOTS);
    CHECK_TRUE(SUITE,
               "pace: the memory walk reads every slot once before it is back at its start",
               CHECK_NONE,
               wrong == 0u && visited == sizeof(seen) && slot == 0u && end == WALK_END &&
                   PaceWalkEnd(7u, 0u, 16u) == 7u,
               "%u of 16 slots out of order; %u of %u slots read before the start came back; %u "
               "steps over %u slots end at %llu, and the table says %llu",
               (unsigned)wrong, (unsigned)visited, (unsigned)sizeof(seen),
               (unsigned)PACE_WALK_STEPS, (unsigned)PACE_WALK_SLOTS, (unsigned long long)end,
               (unsigned long long)WALK_END);
}

void RunReferencePace(void) {
    CheckStats();
    CheckMulDiv();
    CheckLine();
    CheckLoops();
    CheckWalk();
}
