// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Wimsy as Amberlith, the picture the start screen draws (source/wimsypicture.c, written by
// tools/wimsy-picture.py from static/wimsy-amberlith.svg).

#pragma once

#include <stdint.h>

#define WIMSY_PICTURE_SIDE 128u

/// How many colours the palette has.
extern const uint32_t WIMSY_PICTURE_COLOURS;
/// Red, green, blue and alpha, by a pixel's place in the palette; a pixel with no alpha is the
/// palette's own colour of none.
extern const uint8_t WIMSY_PICTURE_PALETTE[][4];
/// Each pixel's place in the palette, row by row from the top.
extern const uint8_t WIMSY_PICTURE[WIMSY_PICTURE_SIDE * WIMSY_PICTURE_SIDE];
