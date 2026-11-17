// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// The pace family's arithmetic (docs/families/Pace.md): what a case's samples come to, the
/// number its line carries and the sentence beside it, and what each of the processor's loops
/// and the memory walk must leave, worked out in integers. No libnx here, so the reference suite
/// holds all of it to numbers worked out apart, on every frontend.
///
/// A pace case measures how long a fixed amount of work takes, several times over. Its line
/// carries the median as `us`, the place every timed case's number goes, so a baseline holds it
/// both ways (baseline.h): a machine faster than the console is as far from it as a slower one.
/// The case itself judges no speed: it passes when the work's own answer was right.

/// What every pace line ends with until a console's run gives the family its numbers.
#define PACE_NO_REFERENCE "no reference yet: a console's run gives it"

/// The most samples one case keeps.
#define PACE_SAMPLES_MAX 4096u

/// What a case's samples come to, each a time in nanoseconds.
typedef struct {
    uint32_t count;
    /// The middle sample, the lower of the two middle ones.
    uint64_t median;
    uint64_t least;
    uint64_t most;
    /// The samples a hundredth of them are at or under, and at or over: the nearest ranks, so
    /// with fewer than a hundred samples they are the least and the most.
    uint64_t low;
    uint64_t high;
    /// How far a sample lies from the median, on average, rounded down.
    uint64_t scatter;
} PaceStats;

/// The summary of @p count samples, which it sorts in place. All zeros for none.
void PaceStatsOf(uint64_t *samples, size_t count, PaceStats *out);

/// @p a times @p b over @p c, rounded down, the product kept whole past 64 bits. UINT64_MAX
/// when @p c is nought or the quotient does not fit.
uint64_t PaceMulDiv(uint64_t a, uint64_t b, uint64_t c);

/// The time @p unit of work takes at the pace @p work was done in @p nanoseconds, rounded down:
/// what a window that counted its work gives as a sample. UINT64_MAX when no work was done.
uint64_t PaceTimeFor(uint64_t nanoseconds, uint64_t work, uint64_t unit);

/// How much work a second is done at @p nanoseconds for @p unit of it, rounded down.
/// UINT64_MAX for a time of nought.
uint64_t PaceRate(uint64_t unit, uint64_t nanoseconds);

/// The number a line carries for a time of @p nanoseconds: microseconds, rounded to the nearest,
/// never nought, since a line's nought would read as a case that did nothing, and never past 32
/// bits.
uint32_t PaceMicroseconds(uint64_t nanoseconds);

/// How far apart the fastest and the slowest sample are, in tenths of a percent of the fastest.
/// UINT32_MAX when the fastest is nought and the slowest is not, which has no percent.
uint32_t PaceSpreadTenths(const PaceStats *stats);

/// The sentence a case's line carries beside its number: @p said, what the case says of its own
/// (a rate, a size), then the samples (@p count_of names them, "windows of 1 s"; @p measure
/// what each is the time of, "the time of 100 million iterations"), then PACE_NO_REFERENCE:
///
///   204.1 million iterations a second; 10 windows of 1 s: the time of 100 million iterations,
///   the median 489.9 ms, 489.1 to 491.0 (0.3% apart); no reference yet: a console's run gives
///   it
///
/// With a hundred samples or more it says where 98 in 100 lie and the scatter too. The times are
/// all in the unit the median reads best in: ms from 10 ms, us from 10 us, ns under it. Returns
/// the length; the text is always terminated, and cut short when @p out is small.
size_t PaceDescribe(char *out, size_t out_size, const char *said, const PaceStats *stats,
                    const char *count_of, const char *measure);

/// The three loops the pace-cpu suite times, as integer arithmetic: what pacework.c's
/// instructions must leave after @p iterations, whatever machine runs them. Every value the
/// floating point and vector loops hold is a whole number their format keeps exactly, so no
/// rounding rule decides an answer, and a fused multiply and a separate one agree.

/// How many instructions one iteration of each loop executes, its count and branch included.
#define PACE_INTEGER_INSTRUCTIONS 9u
#define PACE_FLOAT_INSTRUCTIONS 9u
#define PACE_VECTOR_INSTRUCTIONS 14u

/// How many iterations the suite runs between two reads of the clock, each such run from the
/// loop's start, so every one must leave the same sums: about a millisecond on the console.
#define PACE_INTEGER_CHUNK (1u << 18)
#define PACE_FLOAT_CHUNK (1u << 16)
#define PACE_VECTOR_CHUNK (1u << 16)

/// The integer loop: x = x * A + C; y ^= x >> 29; z += y; y = y rotated right by 17; then the
/// sum takes y where z is above x, unsigned, and z otherwise. @p out gets the sum and z.
#define PACE_INTEGER_A 6364136223846793005ull
#define PACE_INTEGER_C 1442695040888963407ull
#define PACE_INTEGER_X 0x0123456789abcdefull
#define PACE_INTEGER_Y 0xfedcba9876543210ull
void PaceIntegerLoop(uint64_t iterations, uint64_t out[2]);

/// The floating point loop, on doubles: x = (x * A + C) mod 2^24, by a multiply by 2^-24, a round
/// toward nought and a multiply and subtract; then the sum takes the larger of x and 2^24 - x.
/// @p out gets the sum and x. Exact up to PACE_FLOAT_ITERATIONS_MAX iterations, where the sum
/// still fits a double's 53 bits.
#define PACE_FLOAT_A 1664525u
#define PACE_FLOAT_C 1013904223u
#define PACE_FLOAT_X 12345u
#define PACE_FLOAT_ITERATIONS_MAX (1ull << 28)
void PaceFloatLoop(uint64_t iterations, uint64_t out[2]);

/// The vector loop, four lanes of singles beside four of 32-bit integers: each single lane
/// x = (x * A + C) mod 2^12, the way the floating point loop takes its remainder; each integer
/// lane i = i * M + D; then the lane's sum takes (i >> 13) ^ x. @p out gets the four sums, the
/// four i and the four x.
extern const uint32_t PACE_VECTOR_A[4];
extern const uint32_t PACE_VECTOR_C[4];
extern const uint32_t PACE_VECTOR_X[4];
extern const uint32_t PACE_VECTOR_M[4];
extern const uint32_t PACE_VECTOR_D[4];
extern const uint32_t PACE_VECTOR_I[4];
void PaceVectorLoop(uint64_t iterations, uint32_t out[12]);

/// The memory walk the pace-mem suite times: slot i of @p slots, a power of two, holds the slot
/// to read next, (i * PACE_INTEGER_A + PACE_INTEGER_C) mod @p slots, which goes through every
/// slot once before it comes back to its start, far from where it was each time.
uint64_t PaceWalkNext(uint64_t slot, uint64_t slots);

/// Where @p steps reads from @p slot end.
uint64_t PaceWalkEnd(uint64_t slot, uint64_t steps, uint64_t slots);

/// The walk the suite times: PACE_WALK_SLOTS slots of eight bytes, 64 MiB, far past the
/// console's caches and what its address translation keeps at hand, PACE_WALK_STEPS reads a
/// sample.
#define PACE_WALK_SLOTS (1u << 23)
#define PACE_WALK_STEPS (1u << 18)

#ifdef __cplusplus
}
#endif
