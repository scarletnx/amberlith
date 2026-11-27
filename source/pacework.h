// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

#include "pacecoderef.h"

#ifdef __cplusplus
extern "C" {
#endif

/// The pace family's work, written in the processor's own instructions, so that what is timed is
/// the same machine code whatever compiler built the program: the loops paceref.h describes, and
/// the memory and counter loops below. Only numbers and pointers come in and go out, so each can
/// be run on its own on a model of the processor against paceref.c. Console build only.
#if defined(__aarch64__)

/// The three loops of paceref.h, @p iterations of each from its start, one or more: what
/// PaceIntegerLoop, PaceFloatLoop and PaceVectorLoop say they leave.
void PaceWorkInteger(uint64_t iterations, uint64_t out[2]);
void PaceWorkFloat(uint64_t iterations, uint64_t out[2]);
void PaceWorkVector(uint64_t iterations, uint32_t out[12]);

/// Reads @p steps slots of @p table, one or more, each slot holding the slot to read next, from
/// @p slot on: one load an iteration, its address known only when the load before it ends. Hands
/// back the slot it ended on.
uint64_t PaceWorkWalk(const uint64_t *table, uint64_t slot, uint64_t steps);

/// Memory in turns of 64 bytes, @p bytes a multiple of 64 and not nought, both ends aligned to
/// 16: stores of @p value in every 64-bit word; loads added up as 64-bit words, the sum handed
/// back; and loads stored again somewhere else.
void PaceWorkFill(void *to, uint64_t value, uint64_t bytes);
uint64_t PaceWorkSum(const void *from, uint64_t bytes);
void PaceWorkCopy(void *to, const void *from, uint64_t bytes);

/// Reads the system counter @p reads times, one or more, one after another. @p out gets the
/// smallest step between two reads that was not nought (UINT64_MAX when every step was), the
/// largest step, and how many reads were under the read before them.
void PaceWorkCounter(uint64_t reads, uint64_t out[3]);

/// The pace-code suite's kernels (pacecoderef.h), @p iterations of @p kind, one or more: what
/// PaceCodeAnswer says it leaves. The table kernel works in @p table and the crc, aes and sha
/// kernels read @p stream, as PaceCodeAnswer's; the core kernel is PaceCodeCore, the compiler's
/// code, and every other kind is these instructions.
uint64_t PaceCodeWork(PaceCodeKind kind, uint64_t iterations, uint8_t *table,
                      const uint8_t *stream);

#endif

#ifdef __cplusplus
}
#endif
