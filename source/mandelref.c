// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mandelref.h"

#include <stdbool.h>
#include <string.h>

const uint8_t MANDEL_CLEAR[4] = { 0x00, 0x00, 0xff, 0xff };

/// @p value shifted right by @p by with its sign kept, which C leaves to the compiler for a
/// negative value and the graphics unit's shift does not.
static int32_t ShiftRight(int32_t value, unsigned by) {
    const uint32_t bits = (uint32_t)value >> by;
    return (int32_t)(value < 0 ? bits | ~(UINT32_MAX >> by) : bits);
}

uint32_t MandelCount(uint32_t column, uint32_t row) {
    const int32_t real = (int32_t)column * 384 - 9216;
    const int32_t imaginary = (int32_t)row * 439 - 6144;
    int32_t x = 0;
    int32_t y = 0;
    uint32_t count = 0;
    bool within = true;
    for (uint32_t step = 0; step < MANDEL_STEPS; ++step) {
        // The program's order: the test, the count, then the step, taken only while within.
        // A cell that left keeps its last z, so no product here passes 31 bits.
        const int32_t xx = x * x;
        const int32_t yy = y * y;
        within = within && xx + yy <= 0x4000000;
        if (!within) {
            continue;
        }
        ++count;
        const int32_t next_x = ShiftRight(xx - yy, 12) + real;
        y = ShiftRight(x * y, 11) + imaginary;
        x = next_x;
    }
    return count;
}

void MandelBar(uint32_t count, uint8_t out[4]) {
    out[0] = (uint8_t)(15u * count);
    out[1] = 0x40;
    out[2] = 0x00;
    out[3] = 0xff;
}

void MandelExpected(uint32_t x, uint32_t y, uint8_t out[4]) {
    const uint32_t column = x / 8u;
    for (uint32_t back = 0; back < MANDEL_ROWS && column < MANDEL_COLUMNS; ++back) {
        const uint32_t row = MANDEL_ROWS - 1u - back;
        const uint32_t count = MandelCount(column, row);
        const uint32_t line = 32u + 8u * row;
        if (y >= line - 2u * count && y < line + 8u) {
            MandelBar(count, out);
            return;
        }
    }
    memcpy(out, MANDEL_CLEAR, 4);
}
