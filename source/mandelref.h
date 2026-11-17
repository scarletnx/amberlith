// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef AMBERLITH_MANDELREF_H
#define AMBERLITH_MANDELREF_H

#include <stdint.h>

// What the nvn2 suite's Mandelbrot holds, worked out on the processor (docs/families/Nvn2.md):
// the counts a compute program leaves for a grid of the plane, and the picture a mesh program
// and a fragment program make of them. Integers alone, so every machine gives one answer.

#define MANDEL_COLUMNS 32u
#define MANDEL_ROWS 28u
#define MANDEL_STEPS 16u
/// The picture's side: a cell is 8 pixels wide, a row's line 8 pixels under the one before it,
/// the first 32 pixels down, and a bar rises 2 pixels a count above its line.
#define MANDEL_SIZE 256u

/// How many of MANDEL_STEPS steps of z = z * z + c keep z within the circle of radius 2, for
/// the cell's c: its real part -2.25 + column * 3 / 32, its imaginary part -1.5 + row * 439 /
/// 4096, in fixed point with twelve fraction bits. 1 to MANDEL_STEPS.
uint32_t MandelCount(uint32_t column, uint32_t row);

extern const uint8_t MANDEL_CLEAR[4];   ///< where no bar is

/// A bar's four bytes for its count: red 15 a count, green 64, no blue.
void MandelBar(uint32_t count, uint8_t out[4]);

/// The four bytes pixel @p x, @p y holds, the top row first: the bar of the nearest row, the
/// one furthest down the picture, that reaches the pixel, or MANDEL_CLEAR.
void MandelExpected(uint32_t x, uint32_t y, uint8_t out[4]);

#endif
