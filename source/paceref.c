// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <string.h>

#include "paceref.h"

/// Shell sort: a case has up to PACE_SAMPLES_MAX samples, too many for an insertion sort on the
/// console between two cases.
static void Sort(uint64_t *values, size_t count) {
    for (size_t gap = count / 2u; gap > 0u; gap /= 2u) {
        for (size_t i = gap; i < count; ++i) {
            const uint64_t value = values[i];
            size_t j = i;
            while (j >= gap && values[j - gap] > value) {
                values[j] = values[j - gap];
                j -= gap;
            }
            values[j] = value;
        }
    }
}

void PaceStatsOf(uint64_t *samples, size_t count, PaceStats *out) {
    memset(out, 0, sizeof(*out));
    if (samples == NULL || count == 0u) {
        return;
    }
    Sort(samples, count);
    out->count = (uint32_t)count;
    out->median = samples[(count - 1u) / 2u];
    out->least = samples[0];
    out->most = samples[count - 1u];
    // The nearest rank: the smallest sample with a hundredth of them at or under it, and the one
    // with ninety nine hundredths.
    out->low = samples[(count + 99u) / 100u - 1u];
    out->high = samples[(count * 99u + 99u) / 100u - 1u];
    // Averaged as it adds, a whole part and a remainder, so 4096 samples of any size fit.
    uint64_t whole = 0;
    uint64_t rest = 0;
    for (size_t i = 0; i < count; ++i) {
        const uint64_t away =
            samples[i] > out->median ? samples[i] - out->median : out->median - samples[i];
        whole += away / count;
        rest += away % count;
    }
    out->scatter = whole + rest / count;
}

uint64_t PaceMulDiv(uint64_t a, uint64_t b, uint64_t c) {
    if (c == 0u) {
        return UINT64_MAX;
    }
    // The product in two halves, from 32-bit limbs.
    const uint64_t a_low = a & 0xffffffffu, a_high = a >> 32;
    const uint64_t b_low = b & 0xffffffffu, b_high = b >> 32;
    const uint64_t low_low = a_low * b_low;
    const uint64_t low_high = a_low * b_high;
    const uint64_t high_low = a_high * b_low;
    const uint64_t middle = (low_low >> 32) + (low_high & 0xffffffffu) + (high_low & 0xffffffffu);
    const uint64_t high = a_high * b_high + (low_high >> 32) + (high_low >> 32) + (middle >> 32);
    const uint64_t low = (middle << 32) | (low_low & 0xffffffffu);
    if (high >= c) {
        return UINT64_MAX;
    }
    // Long division, a bit at a time: the remainder stays under c, so its doubling overflows 64
    // bits only by the one bit kept in carry.
    uint64_t quotient = 0;
    uint64_t remainder = high;
    for (int bit = 63; bit >= 0; --bit) {
        const bool carry = (remainder >> 63) != 0u;
        remainder = (remainder << 1) | ((low >> bit) & 1u);
        if (carry || remainder >= c) {
            remainder -= c;
            quotient |= 1ull << bit;
        }
    }
    return quotient;
}

uint64_t PaceTimeFor(uint64_t nanoseconds, uint64_t work, uint64_t unit) {
    return PaceMulDiv(nanoseconds, unit, work);
}

uint64_t PaceRate(uint64_t unit, uint64_t nanoseconds) {
    return PaceMulDiv(unit, 1000000000u, nanoseconds);
}

uint32_t PaceMicroseconds(uint64_t nanoseconds) {
    if (nanoseconds > (uint64_t)UINT32_MAX * 1000u) {
        return UINT32_MAX;
    }
    const uint64_t us = (nanoseconds + 500u) / 1000u;
    return us == 0u ? 1u : (uint32_t)us;
}

uint32_t PaceSpreadTenths(const PaceStats *stats) {
    if (stats->most == stats->least) {
        return 0;
    }
    const uint64_t tenths = PaceMulDiv(stats->most - stats->least, 1000u, stats->least);
    return tenths > UINT32_MAX ? UINT32_MAX : (uint32_t)tenths;
}

/// The unit a time of @p nanoseconds reads best in, as its size in nanoseconds.
static uint64_t UnitOf(uint64_t nanoseconds) {
    return nanoseconds >= 10000000u ? 1000000u : nanoseconds >= 10000u ? 1000u : 1u;
}

/// @p nanoseconds in @p unit, a tenth kept unless the unit is the nanosecond.
static void SayTime(char *out, size_t out_size, uint64_t nanoseconds, uint64_t unit) {
    if (unit == 1u) {
        snprintf(out, out_size, "%llu", (unsigned long long)nanoseconds);
        return;
    }
    const uint64_t tenths = nanoseconds / (unit / 10u);
    snprintf(out, out_size, "%llu.%u", (unsigned long long)(tenths / 10u),
             (unsigned)(tenths % 10u));
}

size_t PaceDescribe(char *out, size_t out_size, const char *said, const PaceStats *stats,
                    const char *count_of, const char *measure) {
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    const uint64_t in = UnitOf(stats->median);
    const char *const word = in == 1000000u ? "ms" : in == 1000u ? "us" : "ns";
    char median[24], least[24], most[24], low[24], high[24], scatter[24];
    SayTime(median, sizeof(median), stats->median, in);
    SayTime(least, sizeof(least), stats->least, in);
    SayTime(most, sizeof(most), stats->most, in);
    SayTime(low, sizeof(low), stats->low, in);
    SayTime(high, sizeof(high), stats->high, in);
    SayTime(scatter, sizeof(scatter), stats->scatter, in);
    char apart[40];
    const uint32_t spread = PaceSpreadTenths(stats);
    if (spread == UINT32_MAX) {
        snprintf(apart, sizeof(apart), "the fastest took no time");
    } else {
        snprintf(apart, sizeof(apart), "%u.%u%% apart", (unsigned)(spread / 10u),
                 (unsigned)(spread % 10u));
    }
    char spread_out[160] = "";
    if (stats->count >= 100u) {
        snprintf(spread_out, sizeof(spread_out),
                 ", 98 in 100 from %s to %s, %s from the median on average", low, high, scatter);
    }
    const int length =
        snprintf(out, out_size, "%s%s%u %s: %s, the median %s %s, %s to %s (%s)%s; %s",
                 said != NULL ? said : "", said != NULL && said[0] != '\0' ? "; " : "",
                 (unsigned)stats->count, count_of, measure, median, word, least, most, apart,
                 spread_out, PACE_NO_REFERENCE);
    if (length < 0) {
        out[0] = '\0';
        return 0;
    }
    return (size_t)length < out_size ? (size_t)length : out_size - 1u;
}

void PaceIntegerLoop(uint64_t iterations, uint64_t out[2]) {
    uint64_t x = PACE_INTEGER_X;
    uint64_t y = PACE_INTEGER_Y;
    uint64_t z = 0;
    uint64_t sum = 0;
    for (uint64_t i = 0; i < iterations; ++i) {
        x = x * PACE_INTEGER_A + PACE_INTEGER_C;
        y ^= x >> 29;
        z += y;
        y = (y >> 17) | (y << 47);
        sum += z > x ? y : z;
    }
    out[0] = sum;
    out[1] = z;
}

void PaceFloatLoop(uint64_t iterations, uint64_t out[2]) {
    uint64_t x = PACE_FLOAT_X;
    uint64_t sum = 0;
    for (uint64_t i = 0; i < iterations; ++i) {
        x = (x * PACE_FLOAT_A + PACE_FLOAT_C) & 0xffffffu;
        const uint64_t other = 0x1000000u - x;
        sum += other > x ? other : x;
    }
    out[0] = sum;
    out[1] = x;
}

const uint32_t PACE_VECTOR_A[4] = { 3125u, 2053u, 1229u, 4093u };
const uint32_t PACE_VECTOR_C[4] = { 1013u, 2027u, 3041u, 4057u };
const uint32_t PACE_VECTOR_X[4] = { 1u, 22u, 333u, 4000u };
const uint32_t PACE_VECTOR_M[4] = { 1664525u, 22695477u, 1103515245u, 134775813u };
const uint32_t PACE_VECTOR_D[4] = { 1013904223u, 1u, 12345u, 1u };
const uint32_t PACE_VECTOR_I[4] = { 1u, 2u, 3u, 4u };

void PaceVectorLoop(uint64_t iterations, uint32_t out[12]) {
    uint32_t sum[4] = { 0, 0, 0, 0 };
    uint32_t whole[4];
    uint32_t x[4];
    memcpy(whole, PACE_VECTOR_I, sizeof(whole));
    memcpy(x, PACE_VECTOR_X, sizeof(x));
    for (uint64_t i = 0; i < iterations; ++i) {
        for (uint32_t lane = 0; lane < 4u; ++lane) {
            x[lane] = (x[lane] * PACE_VECTOR_A[lane] + PACE_VECTOR_C[lane]) & 0xfffu;
            whole[lane] = whole[lane] * PACE_VECTOR_M[lane] + PACE_VECTOR_D[lane];
            sum[lane] += (whole[lane] >> 13) ^ x[lane];
        }
    }
    memcpy(out, sum, sizeof(sum));
    memcpy(out + 4, whole, sizeof(whole));
    memcpy(out + 8, x, sizeof(x));
}

uint64_t PaceWalkNext(uint64_t slot, uint64_t slots) {
    return (slot * PACE_INTEGER_A + PACE_INTEGER_C) & (slots - 1u);
}

uint64_t PaceWalkEnd(uint64_t slot, uint64_t steps, uint64_t slots) {
    for (uint64_t i = 0; i < steps; ++i) {
        slot = PaceWalkNext(slot, slots);
    }
    return slot;
}
