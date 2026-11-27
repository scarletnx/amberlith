// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// The pace-code suite's sixteen kernels in C (docs/families/Pace.md#pace-code): what each must
/// leave after a number of iterations from its start, one 64-bit word. pacework.c runs them in
/// the processor's own instructions and tools/pace-code-expect.py restates them in Python; the
/// reference suite holds this file to that script's table, on every frontend. No libnx here,
/// and no 128-bit integers, so the Vita's 32-bit build gives the same answers.
typedef enum {
    PACE_CODE_INT4,
    PACE_CODE_MUL,
    PACE_CODE_DIV,
    PACE_CODE_CALLS,
    PACE_CODE_INDIRECT,
    PACE_CODE_SWITCH,
    PACE_CODE_BRANCHES,
    PACE_CODE_TABLE,
    PACE_CODE_FDIV,
    PACE_CODE_BYTES,
    PACE_CODE_CRC,
    PACE_CODE_AES,
    PACE_CODE_SHA,
    PACE_CODE_EXCLUSIVE,
    PACE_CODE_CORE,
    PACE_CODE_VECTOR,
    PACE_CODE_KINDS
} PaceCodeKind;

/// The table the table kernel reads and writes, and the stream the crc, aes and sha kernels go
/// round: both filled by PaceCodeFill.
#define PACE_CODE_TABLE_BYTES (16u << 10)
#define PACE_CODE_STREAM_BYTES (64u << 10)

/// Word i of @p to, from 0, is (i + 1) * PACE_INTEGER_A + PACE_INTEGER_C, little endian;
/// @p bytes a multiple of 8.
void PaceCodeFill(uint8_t *to, size_t bytes);

/// What @p kind leaves after @p iterations, one or more. The table kernel works in @p table,
/// PACE_CODE_TABLE_BYTES filled by PaceCodeFill, and leaves it changed; the crc, aes and sha
/// kernels read @p stream, PACE_CODE_STREAM_BYTES filled the same way.
uint64_t PaceCodeAnswer(PaceCodeKind kind, uint64_t iterations, uint8_t *table,
                        const uint8_t *stream);

/// The core kernel, the one that is the compiler's code and not the processor's instructions:
/// the suite times this function itself, and the reference suite holds it to the script.
uint64_t PaceCodeCore(uint64_t iterations);

#ifdef __cplusplus
}
#endif
