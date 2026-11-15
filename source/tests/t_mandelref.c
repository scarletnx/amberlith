// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../check.h"
#include "../mandelref.h"
#include "../report.h"
#include "tests.h"

// The nvn2 suite's Mandelbrot, as mandelref.c expects it, held to numbers worked out apart: the
// counts by unbounded integers with floor shifts (Python's), the picture by painting every
// row's bars from the furthest to the nearest.

static const char *const SUITE = "reference";

static void CheckCounts(void) {
    uint32_t sum = 0;
    uint32_t full = 0;
    uint32_t ones = 0;
    uint32_t hash = 0;
    uint32_t outside = 0;
    for (uint32_t row = 0; row < MANDEL_ROWS; ++row) {
        for (uint32_t column = 0; column < MANDEL_COLUMNS; ++column) {
            const uint32_t count = MandelCount(column, row);
            sum += count;
            full += count == MANDEL_STEPS ? 1u : 0u;
            ones += count == 1u ? 1u : 0u;
            outside += count < 1u || count > MANDEL_STEPS ? 1u : 0u;
            hash = hash * 31u + count;
        }
    }
    // c = -2.25 - 1.5i is outside the circle after one step; c near 0 never leaves.
    const uint32_t corner = MandelCount(0, 0);
    const uint32_t origin = MandelCount(24, 14);
    CHECK_TRUE(SUITE, "mandel: the grid's counts are the ones unbounded integers give",
               CHECK_EXACT,
               sum == 5345u && full == 185u && ones == 147u && hash == 0x01f122abu &&
                   outside == 0u && corner == 1u && origin == 16u && MandelCount(20, 5) == 5u,
               "sum %u, %u cells of 16, %u of 1, hash %08x, %u out of 1 to 16, the corner %u, "
               "the origin %u, cell (20,5) %u, where 896 cells give 5345, 185, 147, 01f122ab, "
               "0, 1, 16 and 5",
               (unsigned)sum, (unsigned)full, (unsigned)ones, (unsigned)hash, (unsigned)outside,
               (unsigned)corner, (unsigned)origin, (unsigned)MandelCount(20, 5));
}

static void CheckPicture(void) {
    // Painted here from the furthest row to the nearest, each bar over what is behind it, which
    // is what the depth test leaves; MandelExpected looks from the nearest row back instead.
    static uint8_t painted[MANDEL_SIZE][MANDEL_SIZE];
    memset(painted, 0, sizeof(painted));
    for (uint32_t row = 0; row < MANDEL_ROWS; ++row) {
        for (uint32_t column = 0; column < MANDEL_COLUMNS; ++column) {
            const uint32_t count = MandelCount(column, row);
            for (uint32_t y = 32u + 8u * row - 2u * count; y < 40u + 8u * row; ++y) {
                memset(&painted[y][8u * column], (int)count, 8);
            }
        }
    }
    uint32_t clear = 0;
    uint32_t bars = 0;
    uint32_t full = 0;
    uint32_t red = 0;
    uint32_t differ = 0;
    for (uint32_t y = 0; y < MANDEL_SIZE; ++y) {
        for (uint32_t x = 0; x < MANDEL_SIZE; ++x) {
            uint8_t got[4];
            uint8_t want[4];
            MandelExpected(x, y, got);
            if (painted[y][x] == 0u) {
                memcpy(want, MANDEL_CLEAR, 4);
                ++clear;
            } else {
                MandelBar(painted[y][x], want);
                ++bars;
                full += painted[y][x] == MANDEL_STEPS ? 1u : 0u;
                red += want[0];
            }
            differ += memcmp(got, want, 4) != 0 ? 1u : 0u;
        }
    }
    CHECK_TRUE(SUITE, "mandel: the picture is the bars painted from the furthest row to the "
                      "nearest",
               CHECK_EXACT,
               differ == 0u && clear == 7328u && bars == 58208u && full == 14560u &&
                   red == 5712960u,
               "%u pixels differ from the painting; clear %u, bars %u, of a count of 16 %u, the "
               "reds' sum %u, where the picture has 0, 7328, 58208, 14560 and 5712960",
               (unsigned)differ, (unsigned)clear, (unsigned)bars, (unsigned)full, (unsigned)red);
}

void RunReferenceMandel(void) {
    CheckCounts();
    CheckPicture();
}
