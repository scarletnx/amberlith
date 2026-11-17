// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pacework.h"

#include "paceref.h"

// The pace family's loops as instructions. The compiler picks the registers and nothing else:
// every loop body is the lines below, in this order, in every build.

#if defined(__aarch64__)

#include <arm_neon.h>

void PaceWorkInteger(uint64_t iterations, uint64_t out[2]) {
    uint64_t n = iterations;
    uint64_t x = PACE_INTEGER_X;
    uint64_t y = PACE_INTEGER_Y;
    uint64_t z = 0;
    uint64_t sum = 0;
    uint64_t picked = 0;
    const uint64_t a = PACE_INTEGER_A;
    const uint64_t c = PACE_INTEGER_C;
    __asm__ volatile("1:\n"
                     "madd %[x], %[x], %[a], %[c]\n"
                     "eor %[y], %[y], %[x], lsr #29\n"
                     "add %[z], %[z], %[y]\n"
                     "ror %[y], %[y], #17\n"
                     "cmp %[z], %[x]\n"
                     "csel %[picked], %[y], %[z], hi\n"
                     "add %[sum], %[sum], %[picked]\n"
                     "subs %[n], %[n], #1\n"
                     "b.ne 1b\n"
                     : [x] "+r"(x), [y] "+r"(y), [z] "+r"(z), [sum] "+r"(sum),
                       [picked] "+r"(picked), [n] "+r"(n)
                     : [a] "r"(a), [c] "r"(c)
                     : "cc");
    out[0] = sum;
    out[1] = z;
}

void PaceWorkFloat(uint64_t iterations, uint64_t out[2]) {
    uint64_t n = iterations;
    double x = (double)PACE_FLOAT_X;
    double sum = 0.0;
    double quotient = 0.0;
    double other = 0.0;
    const double a = (double)PACE_FLOAT_A;
    const double c = (double)PACE_FLOAT_C;
    const double down = 0x1p-24;
    const double up = 0x1p24;
    __asm__ volatile("1:\n"
                     "fmadd %d[x], %d[x], %d[a], %d[c]\n"
                     "fmul %d[quotient], %d[x], %d[down]\n"
                     "frintz %d[quotient], %d[quotient]\n"
                     "fmsub %d[x], %d[quotient], %d[up], %d[x]\n"
                     "fsub %d[other], %d[up], %d[x]\n"
                     "fmax %d[other], %d[other], %d[x]\n"
                     "fadd %d[sum], %d[sum], %d[other]\n"
                     "subs %[n], %[n], #1\n"
                     "b.ne 1b\n"
                     : [x] "+w"(x), [sum] "+w"(sum), [quotient] "+w"(quotient),
                       [other] "+w"(other), [n] "+r"(n)
                     : [a] "w"(a), [c] "w"(c), [down] "w"(down), [up] "w"(up)
                     : "cc");
    // Whole numbers under 2^53, so the conversions lose nothing.
    out[0] = (uint64_t)sum;
    out[1] = (uint64_t)x;
}

void PaceWorkVector(uint64_t iterations, uint32_t out[12]) {
    uint64_t n = iterations;
    float32x4_t x = vcvtq_f32_u32(vld1q_u32(PACE_VECTOR_X));
    float32x4_t t = vdupq_n_f32(0.0f);
    uint32x4_t quotient = vdupq_n_u32(0);
    uint32x4_t whole = vld1q_u32(PACE_VECTOR_I);
    uint32x4_t sum = vdupq_n_u32(0);
    uint32x4_t shifted = vdupq_n_u32(0);
    const float32x4_t a = vcvtq_f32_u32(vld1q_u32(PACE_VECTOR_A));
    const float32x4_t c = vcvtq_f32_u32(vld1q_u32(PACE_VECTOR_C));
    const float32x4_t down = vdupq_n_f32(0x1p-12f);
    const float32x4_t up = vdupq_n_f32(0x1p12f);
    const uint32x4_t m = vld1q_u32(PACE_VECTOR_M);
    const uint32x4_t d = vld1q_u32(PACE_VECTOR_D);
    __asm__ volatile("1:\n"
                     "mov %[t].16b, %[c].16b\n"
                     "fmla %[t].4s, %[x].4s, %[a].4s\n"
                     "fmul %[quotient].4s, %[t].4s, %[down].4s\n"
                     "frintz %[quotient].4s, %[quotient].4s\n"
                     "fmls %[t].4s, %[quotient].4s, %[up].4s\n"
                     "mov %[x].16b, %[t].16b\n"
                     "fcvtzu %[quotient].4s, %[x].4s\n"
                     "mul %[whole].4s, %[whole].4s, %[m].4s\n"
                     "add %[whole].4s, %[whole].4s, %[d].4s\n"
                     "ushr %[shifted].4s, %[whole].4s, #13\n"
                     "eor %[shifted].16b, %[shifted].16b, %[quotient].16b\n"
                     "add %[sum].4s, %[sum].4s, %[shifted].4s\n"
                     "subs %[n], %[n], #1\n"
                     "b.ne 1b\n"
                     : [x] "+w"(x), [t] "+w"(t), [quotient] "+w"(quotient), [whole] "+w"(whole),
                       [sum] "+w"(sum), [shifted] "+w"(shifted), [n] "+r"(n)
                     : [a] "w"(a), [c] "w"(c), [down] "w"(down), [up] "w"(up), [m] "w"(m),
                       [d] "w"(d)
                     : "cc");
    vst1q_u32(out, sum);
    vst1q_u32(out + 4, whole);
    vst1q_u32(out + 8, vcvtq_u32_f32(x));
}

uint64_t PaceWorkWalk(const uint64_t *table, uint64_t slot, uint64_t steps) {
    uint64_t n = steps;
    __asm__ volatile("1:\n"
                     "ldr %[slot], [%[table], %[slot], lsl #3]\n"
                     "subs %[n], %[n], #1\n"
                     "b.ne 1b\n"
                     : [slot] "+r"(slot), [n] "+r"(n)
                     : [table] "r"(table)
                     : "cc", "memory");
    return slot;
}

void PaceWorkFill(void *to, uint64_t value, uint64_t bytes) {
    uint64_t n = bytes;
    const uint64x2_t pair = vdupq_n_u64(value);
    __asm__ volatile("1:\n"
                     "stp %q[pair], %q[pair], [%[to]]\n"
                     "stp %q[pair], %q[pair], [%[to], #32]\n"
                     "add %[to], %[to], #64\n"
                     "subs %[n], %[n], #64\n"
                     "b.ne 1b\n"
                     : [to] "+r"(to), [n] "+r"(n)
                     : [pair] "w"(pair)
                     : "cc", "memory");
}

uint64_t PaceWorkSum(const void *from, uint64_t bytes) {
    uint64_t n = bytes;
    uint64x2_t first = vdupq_n_u64(0);
    uint64x2_t second = vdupq_n_u64(0);
    // Two sums, so the adds of one turn do not all wait on one another.
    __asm__ volatile("1:\n"
                     "ldp q16, q17, [%[from]]\n"
                     "ldp q18, q19, [%[from], #32]\n"
                     "add %[from], %[from], #64\n"
                     "add %[first].2d, %[first].2d, v16.2d\n"
                     "add %[second].2d, %[second].2d, v17.2d\n"
                     "add %[first].2d, %[first].2d, v18.2d\n"
                     "add %[second].2d, %[second].2d, v19.2d\n"
                     "subs %[n], %[n], #64\n"
                     "b.ne 1b\n"
                     : [from] "+r"(from), [first] "+w"(first), [second] "+w"(second), [n] "+r"(n)
                     :
                     : "cc", "v16", "v17", "v18", "v19");
    const uint64x2_t both = vaddq_u64(first, second);
    return vgetq_lane_u64(both, 0) + vgetq_lane_u64(both, 1);
}

void PaceWorkCopy(void *to, const void *from, uint64_t bytes) {
    uint64_t n = bytes;
    __asm__ volatile("1:\n"
                     "ldp q16, q17, [%[from]]\n"
                     "ldp q18, q19, [%[from], #32]\n"
                     "add %[from], %[from], #64\n"
                     "stp q16, q17, [%[to]]\n"
                     "stp q18, q19, [%[to], #32]\n"
                     "add %[to], %[to], #64\n"
                     "subs %[n], %[n], #64\n"
                     "b.ne 1b\n"
                     : [to] "+r"(to), [from] "+r"(from), [n] "+r"(n)
                     :
                     : "cc", "memory", "v16", "v17", "v18", "v19");
}

void PaceWorkCounter(uint64_t reads, uint64_t out[3]) {
    uint64_t n = reads;
    uint64_t before = 0;
    uint64_t now = 0;
    uint64_t step = 0;
    uint64_t candidate = 0;
    uint64_t least = UINT64_MAX;
    uint64_t most = 0;
    uint64_t back = 0;
    __asm__ volatile("mrs %[before], cntpct_el0\n"
                     "1:\n"
                     "mrs %[now], cntpct_el0\n"
                     "subs %[step], %[now], %[before]\n"
                     // A read under the one before it is counted, and is no step.
                     "cinc %[back], %[back], lo\n"
                     "csel %[step], xzr, %[step], lo\n"
                     "cmp %[step], %[most]\n"
                     "csel %[most], %[step], %[most], hi\n"
                     // The smallest step that is not nought.
                     "cmp %[step], #0\n"
                     "csel %[candidate], %[least], %[step], eq\n"
                     "cmp %[candidate], %[least]\n"
                     "csel %[least], %[candidate], %[least], lo\n"
                     "mov %[before], %[now]\n"
                     "subs %[n], %[n], #1\n"
                     "b.ne 1b\n"
                     : [before] "+r"(before), [now] "+r"(now), [step] "+r"(step),
                       [candidate] "+r"(candidate), [least] "+r"(least), [most] "+r"(most),
                       [back] "+r"(back), [n] "+r"(n)
                     :
                     : "cc");
    out[0] = least;
    out[1] = most;
    out[2] = back;
}

#endif
