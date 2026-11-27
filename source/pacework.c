// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pacework.h"

#include "paceref.h"

// The pace family's loops as instructions. The compiler picks the registers and nothing else:
// every loop body is the lines below, in this order, in every build.

#if defined(__aarch64__)

#include <arm_neon.h>
#include <string.h>

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

// ---- pace-code: the kernels of pacecoderef.h, in the order its enum gives

#define CODE_START 0x0123456789abcdefull
#define CODE_OTHER 0xfedcba9876543210ull

static uint64_t CodeInt4(uint64_t n) {
    uint64_t a = CODE_START, b = CODE_OTHER, c = 0x0f1e2d3c4b5a6978ull, d = 0x8796a5b4c3d2e1f0ull;
    uint64_t t1, t2, t3, t4;
    __asm__ volatile("1:\n"
                     "add %[t1], %[a], #0xabc\n"
                     "eor %[a], %[t1], %[a], lsl #7\n"
                     "eor %[t2], %[b], #0xff\n"
                     "add %[b], %[t2], %[b], lsr #3\n"
                     "sub %[t3], %[c], #0x7ff\n"
                     "eor %[c], %[t3], %[c], lsr #11\n"
                     "eor %[t4], %[d], %[d], lsl #13\n"
                     "add %[d], %[t4], #0x123\n"
                     "subs %[n], %[n], #1\n"
                     "b.ne 1b\n"
                     : [a] "+&r"(a), [b] "+&r"(b), [c] "+&r"(c), [d] "+&r"(d), [t1] "=&r"(t1),
                       [t2] "=&r"(t2), [t3] "=&r"(t3), [t4] "=&r"(t4), [n] "+&r"(n)
                     :
                     : "cc");
    return (a + b) ^ (c + d);
}

static uint64_t CodeMul(uint64_t n) {
    uint64_t x = CODE_START, y = 0, h;
    const uint64_t m = PACE_INTEGER_A;
    __asm__ volatile("1:\n"
                     "umulh %[h], %[x], %[m]\n"
                     "madd %[x], %[x], %[m], %[h]\n"
                     "add %[y], %[y], %[h]\n"
                     "subs %[n], %[n], #1\n"
                     "b.ne 1b\n"
                     : [x] "+&r"(x), [y] "+&r"(y), [h] "=&r"(h), [n] "+&r"(n)
                     : [m] "r"(m)
                     : "cc");
    return x ^ y;
}

static uint64_t CodeDiv(uint64_t n) {
    uint64_t x = CODE_START, y = CODE_OTHER, t, q, s;
    const uint64_t m = PACE_INTEGER_A, k = 0xffffffffffffffc5ull, k5 = 0x9e3779b97f4a7c15ull;
    __asm__ volatile("1:\n"
                     "lsr %[t], %[x], #44\n"
                     "orr %[t], %[t], #1\n"
                     "udiv %[q], %[k], %[t]\n"
                     "madd %[x], %[x], %[m], %[q]\n"
                     "and %[t], %[x], #0xfff\n"
                     "add %[t], %[t], #2\n"
                     "sdiv %[s], %[y], %[t]\n"
                     "eor %[y], %[y], %[s]\n"
                     "sub %[y], %[y], %[k5]\n"
                     "subs %[n], %[n], #1\n"
                     "b.ne 1b\n"
                     : [x] "+&r"(x), [y] "+&r"(y), [t] "=&r"(t), [q] "=&r"(q), [s] "=&r"(s),
                       [n] "+&r"(n)
                     : [m] "r"(m), [k] "r"(k), [k5] "r"(k5)
                     : "cc");
    return x ^ y;
}

// Three functions, each calling the next, and eight leaves an index picks: they take and give
// x0 alone, so a caller's asm says x0 and x30 are what a call takes from it.
__asm__(".text\n"
        ".p2align 4\n"
        "pace_code_f3:\n"
        "eor x0, x0, x0, lsr #7\n"
        "add x0, x0, #123\n"
        "ret\n"
        "pace_code_f2:\n"
        "stp x29, x30, [sp, #-16]!\n"
        "add x0, x0, x0, lsl #2\n"
        "bl pace_code_f3\n"
        "eor x0, x0, x0, lsr #11\n"
        "ldp x29, x30, [sp], #16\n"
        "ret\n"
        "pace_code_f1:\n"
        "stp x29, x30, [sp, #-16]!\n"
        "add x0, x0, #7\n"
        "bl pace_code_f2\n"
        "add x0, x0, x0, lsl #1\n"
        "ldp x29, x30, [sp], #16\n"
        "ret\n"
        "pace_code_leaf0: eor x0, x0, x0, lsr #3\n add x0, x0, #1\n ret\n"
        "pace_code_leaf1: eor x0, x0, x0, lsr #4\n add x0, x0, #17\n ret\n"
        "pace_code_leaf2: eor x0, x0, x0, lsr #5\n add x0, x0, #33\n ret\n"
        "pace_code_leaf3: eor x0, x0, x0, lsr #6\n add x0, x0, #49\n ret\n"
        "pace_code_leaf4: eor x0, x0, x0, lsr #7\n add x0, x0, #65\n ret\n"
        "pace_code_leaf5: eor x0, x0, x0, lsr #8\n add x0, x0, #81\n ret\n"
        "pace_code_leaf6: eor x0, x0, x0, lsr #9\n add x0, x0, #97\n ret\n"
        "pace_code_leaf7: eor x0, x0, x0, lsr #10\n add x0, x0, #113\n ret\n"
        ".p2align 2\n"
        "pace_code_leaves:\n"
        ".word pace_code_leaf0 - pace_code_leaves, pace_code_leaf1 - pace_code_leaves\n"
        ".word pace_code_leaf2 - pace_code_leaves, pace_code_leaf3 - pace_code_leaves\n"
        ".word pace_code_leaf4 - pace_code_leaves, pace_code_leaf5 - pace_code_leaves\n"
        ".word pace_code_leaf6 - pace_code_leaves, pace_code_leaf7 - pace_code_leaves\n");

static uint64_t CodeCalls(uint64_t n) {
    uint64_t x = CODE_START;
    __asm__ volatile("1:\n"
                     "mov x0, %[x]\n"
                     "bl pace_code_f1\n"
                     "mov %[x], x0\n"
                     "subs %[n], %[n], #1\n"
                     "b.ne 1b\n"
                     : [x] "+&r"(x), [n] "+&r"(n)
                     :
                     : "x0", "x30", "cc", "memory");
    return x;
}

static uint64_t CodeIndirect(uint64_t n) {
    uint64_t x = CODE_START, table, k, at;
    const uint64_t m = PACE_INTEGER_A, c = PACE_INTEGER_C;
    __asm__ volatile("adrp %[table], pace_code_leaves\n"
                     "add %[table], %[table], :lo12:pace_code_leaves\n"
                     "1:\n"
                     "madd %[x], %[x], %[m], %[c]\n"
                     "lsr %[k], %[x], #61\n"
                     "ldrsw %[at], [%[table], %[k], lsl #2]\n"
                     "add %[at], %[table], %[at]\n"
                     "mov x0, %[x]\n"
                     "blr %[at]\n"
                     "mov %[x], x0\n"
                     "subs %[n], %[n], #1\n"
                     "b.ne 1b\n"
                     : [x] "+&r"(x), [table] "=&r"(table), [k] "=&r"(k), [at] "=&r"(at),
                       [n] "+&r"(n)
                     : [m] "r"(m), [c] "r"(c)
                     : "x0", "x30", "cc", "memory");
    return x;
}

static uint64_t CodeSwitch(uint64_t n) {
    uint64_t x = CODE_START, s = 0, table, k, at;
    const uint64_t m = PACE_INTEGER_A, c = PACE_INTEGER_C;
    __asm__ volatile("adr %[table], 5f\n"
                     "1:\n"
                     "madd %[x], %[x], %[m], %[c]\n"
                     "lsr %[k], %[x], #60\n"
                     "ldrsw %[at], [%[table], %[k], lsl #2]\n"
                     "add %[at], %[table], %[at]\n"
                     "br %[at]\n"
                     "10: add %[x], %[x], #1\n b 2f\n"
                     "11: eor %[x], %[x], %[x], lsr #3\n b 2f\n"
                     "12: sub %[x], %[x], #7\n b 2f\n"
                     "13: ror %[x], %[x], #5\n b 2f\n"
                     "14: add %[x], %[x], %[x], lsl #4\n b 2f\n"
                     "15: eor %[x], %[x], #0xff\n b 2f\n"
                     "16: mvn %[x], %[x]\n b 2f\n"
                     "17: add %[x], %[x], %[x], lsr #9\n b 2f\n"
                     "18: sub %[x], %[x], %[x], lsl #2\n b 2f\n"
                     "19: eor %[x], %[x], %[x], lsl #13\n b 2f\n"
                     "20: ror %[x], %[x], #31\n b 2f\n"
                     "21: add %[x], %[x], #0x55\n b 2f\n"
                     "22: add %[x], %[x], %[x], lsr #1\n b 2f\n"
                     "23: eor %[x], %[x], %[x], lsr #17\n b 2f\n"
                     "24: sub %[x], %[x], #0x123\n b 2f\n"
                     "25: ror %[x], %[x], #47\n"
                     "2:\n"
                     "add %[s], %[s], %[x]\n"
                     "subs %[n], %[n], #1\n"
                     "b.ne 1b\n"
                     "b 3f\n"
                     ".p2align 2\n"
                     "5:\n"
                     ".word 10b - 5b, 11b - 5b, 12b - 5b, 13b - 5b\n"
                     ".word 14b - 5b, 15b - 5b, 16b - 5b, 17b - 5b\n"
                     ".word 18b - 5b, 19b - 5b, 20b - 5b, 21b - 5b\n"
                     ".word 22b - 5b, 23b - 5b, 24b - 5b, 25b - 5b\n"
                     "3:\n"
                     : [x] "+&r"(x), [s] "+&r"(s), [table] "=&r"(table), [k] "=&r"(k),
                       [at] "=&r"(at), [n] "+&r"(n)
                     : [m] "r"(m), [c] "r"(c)
                     : "cc");
    return x ^ s;
}

static uint64_t CodeBranches(uint64_t n) {
    uint64_t x = CODE_START, a = 0, b = 0, c = 0;
    __asm__ volatile("1:\n"
                     "eor %[x], %[x], %[x], lsl #13\n"
                     "eor %[x], %[x], %[x], lsr #7\n"
                     "eor %[x], %[x], %[x], lsl #17\n"
                     "tbz %[x], #3, 2f\n"
                     "add %[a], %[a], #1\n"
                     "2:\n"
                     "tbnz %[x], #5, 3f\n"
                     "add %[b], %[b], %[x]\n"
                     "3:\n"
                     "tbz %[x], #11, 4f\n"
                     "eor %[c], %[c], %[x], lsr #8\n"
                     "4:\n"
                     "subs %[n], %[n], #1\n"
                     "b.ne 1b\n"
                     : [x] "+&r"(x), [a] "+&r"(a), [b] "+&r"(b), [c] "+&r"(c), [n] "+&r"(n)
                     :
                     : "cc");
    return a + (b ^ c) + x;
}

static uint64_t CodeTable(uint64_t n, uint8_t *t) {
    uint64_t x = CODE_START, sum = 0, i, j, k, b, h, w, tmp;
    const uint64_t m = PACE_INTEGER_A, c = PACE_INTEGER_C;
    __asm__ volatile("1:\n"
                     "and %[i], %[x], #0x3fff\n"
                     "lsr %[j], %[x], #20\n"
                     "and %[j], %[j], #0x3ffe\n"
                     "lsr %[k], %[x], #40\n"
                     "and %[k], %[k], #0x3ffc\n"
                     "ldrb %w[b], [%[t], %[i]]\n"
                     "ldrh %w[h], [%[t], %[j]]\n"
                     "ldr %w[w], [%[t], %[k]]\n"
                     "add %w[tmp], %w[b], #1\n"
                     "strb %w[tmp], [%[t], %[i]]\n"
                     "eor %w[tmp], %w[h], %w[b]\n"
                     "strh %w[tmp], [%[t], %[j]]\n"
                     "add %w[tmp], %w[w], %w[h]\n"
                     "str %w[tmp], [%[t], %[k]]\n"
                     "add %[sum], %[sum], %[b]\n"
                     "add %[sum], %[sum], %[h]\n"
                     "add %[sum], %[sum], %[w]\n"
                     "madd %[x], %[x], %[m], %[c]\n"
                     "add %[x], %[x], %[w]\n"
                     "subs %[n], %[n], #1\n"
                     "b.ne 1b\n"
                     : [x] "+&r"(x), [sum] "+&r"(sum), [i] "=&r"(i), [j] "=&r"(j), [k] "=&r"(k),
                       [b] "=&r"(b), [h] "=&r"(h), [w] "=&r"(w), [tmp] "=&r"(tmp), [n] "+&r"(n)
                     : [t] "r"(t), [m] "r"(m), [c] "r"(c)
                     : "cc", "memory");
    return sum ^ x;
}

static uint64_t CodeFdiv(uint64_t n) {
    double x = 2.0, s, q;
    const double one = 1.0, thousand = 1000.0, scale = 1024.0;
    uint64_t acc = 0, i;
    __asm__ volatile("1:\n"
                     "fsqrt %d[s], %d[x]\n"
                     "fadd %d[s], %d[s], %d[one]\n"
                     "fdiv %d[q], %d[thousand], %d[s]\n"
                     "fmul %d[s], %d[q], %d[scale]\n"
                     "fcvtzs %[i], %d[s]\n"
                     "add %[acc], %[acc], %[i]\n"
                     "and %[i], %[acc], #0xfffff\n"
                     "scvtf %d[x], %[i]\n"
                     "fadd %d[x], %d[x], %d[q]\n"
                     "fadd %d[x], %d[x], %d[one]\n"
                     "subs %[n], %[n], #1\n"
                     "b.ne 1b\n"
                     : [x] "+&w"(x), [s] "=&w"(s), [q] "=&w"(q), [acc] "+&r"(acc), [i] "=&r"(i),
                       [n] "+&r"(n)
                     : [one] "w"(one), [thousand] "w"(thousand), [scale] "w"(scale)
                     : "cc");
    uint64_t pattern;
    memcpy(&pattern, &x, sizeof(pattern));
    return acc ^ pattern;
}

static uint64_t CodeBytes(uint64_t n) {
    uint8_t start[16], picks[16];
    for (unsigned i = 0; i < 16u; ++i) {
        start[i] = (uint8_t)(i * 37u + 11u);
        picks[i] = (uint8_t)(i * 91u + 7u);
    }
    uint8x16_t v = vld1q_u8(start), m, a, e;
    uint16x8_t sum;
    const uint8x16_t t = vld1q_u8(picks), low = vdupq_n_u8(0x0f), step = vdupq_n_u8(0x3b);
    uint64_t acc = 0, tmp;
    __asm__ volatile("1:\n"
                     "and %[m].16b, %[v].16b, %[low].16b\n"
                     "tbl %[a].16b, {%[t].16b}, %[m].16b\n"
                     "cmeq %[e].16b, %[a].16b, %[m].16b\n"
                     "uaddlv %h[sum], %[e].16b\n"
                     "umov %w[tmp], %[sum].h[0]\n"
                     "add %[acc], %[acc], %[tmp]\n"
                     "ext %[v].16b, %[v].16b, %[a].16b, #3\n"
                     "add %[v].16b, %[v].16b, %[step].16b\n"
                     "subs %[n], %[n], #1\n"
                     "b.ne 1b\n"
                     : [v] "+&w"(v), [m] "=&w"(m), [a] "=&w"(a), [e] "=&w"(e), [sum] "=&w"(sum),
                       [acc] "+&r"(acc), [tmp] "=&r"(tmp), [n] "+&r"(n)
                     : [t] "w"(t), [low] "w"(low), [step] "w"(step)
                     : "cc");
    const uint64x2_t halves = vreinterpretq_u64_u8(v);
    return acc ^ vgetq_lane_u64(halves, 0) ^ vgetq_lane_u64(halves, 1);
}

static uint64_t CodeCrc(uint64_t n, const uint8_t *stream) {
    uint64_t c1 = 0xffffffffu, c2 = 0xffffffffu, w;
    const uint8_t *at = stream;
    const uint8_t *const end = stream + PACE_CODE_STREAM_BYTES;
    __asm__ volatile("1:\n"
                     "ldr %[w], [%[at]], #8\n"
                     "crc32x %w[c1], %w[c1], %[w]\n"
                     "crc32cx %w[c2], %w[c2], %[w]\n"
                     "cmp %[at], %[end]\n"
                     "csel %[at], %[stream], %[at], eq\n"
                     "subs %[n], %[n], #1\n"
                     "b.ne 1b\n"
                     : [c1] "+&r"(c1), [c2] "+&r"(c2), [w] "=&r"(w), [at] "+&r"(at), [n] "+&r"(n)
                     : [end] "r"(end), [stream] "r"(stream)
                     : "cc", "memory");
    return (c1 & 0xffffffffu) << 32 | (c2 & 0xffffffffu);
}

static uint64_t CodeAes(uint64_t n, const uint8_t *stream) {
    uint8x16_t v = vld1q_u8(stream + 16), key;
    const uint8_t *at = stream;
    const uint8_t *const end = stream + PACE_CODE_STREAM_BYTES;
    __asm__ volatile("1:\n"
                     "ldr %q[key], [%[at]], #16\n"
                     "aese %[v].16b, %[key].16b\n"
                     "aesmc %[v].16b, %[v].16b\n"
                     "cmp %[at], %[end]\n"
                     "csel %[at], %[stream], %[at], eq\n"
                     "subs %[n], %[n], #1\n"
                     "b.ne 1b\n"
                     : [v] "+&w"(v), [key] "=&w"(key), [at] "+&r"(at), [n] "+&r"(n)
                     : [end] "r"(end), [stream] "r"(stream)
                     : "cc", "memory");
    const uint64x2_t halves = vreinterpretq_u64_u8(v);
    return vgetq_lane_u64(halves, 0) ^ vgetq_lane_u64(halves, 1);
}

static const uint32_t CODE_SHA_K[64] __attribute__((aligned(16))) = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u,
    0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu,
    0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu,
    0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u,
    0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u,
    0xc67178f2u,
};

// Four rounds of SHA-256: W + K into v23, then the two halves of the state; the first twelve
// groups also make the message words four groups on (sha256su0, then sha256su1).
#define SHA_ROUNDS(j, w0)                                                                         \
    "ldr q24, [%[k], #" #j " * 16]\n"                                                             \
    "add v23.4s, " w0 ".4s, v24.4s\n"                                                             \
    "mov v22.16b, v20.16b\n"                                                                      \
    "sha256h q20, q21, v23.4s\n"                                                                  \
    "sha256h2 q21, q22, v23.4s\n"
#define SHA_SCHEDULE(w0, w1, w2, w3)                                                              \
    "sha256su0 " w0 ".4s, " w1 ".4s\n"                                                            \
    "sha256su1 " w0 ".4s, " w2 ".4s, " w3 ".4s\n"

static uint64_t CodeSha(uint64_t n, const uint8_t *stream) {
    uint32_t h[8] __attribute__((aligned(16))) = { 0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u,
                                                   0xa54ff53au, 0x510e527fu, 0x9b05688cu,
                                                   0x1f83d9abu, 0x5be0cd19u };
    const uint8_t *at = stream;
    const uint8_t *const end = stream + PACE_CODE_STREAM_BYTES;
    __asm__ volatile("ldp q20, q21, [%[h]]\n"
                     "1:\n"
                     "ldp q16, q17, [%[at]], #32\n"
                     "ldp q18, q19, [%[at]], #32\n"
                     "rev32 v16.16b, v16.16b\n"
                     "rev32 v17.16b, v17.16b\n"
                     "rev32 v18.16b, v18.16b\n"
                     "rev32 v19.16b, v19.16b\n"
                     "mov v25.16b, v20.16b\n"
                     "mov v26.16b, v21.16b\n"
                     SHA_ROUNDS(0, "v16") SHA_SCHEDULE("v16", "v17", "v18", "v19")
                     SHA_ROUNDS(1, "v17") SHA_SCHEDULE("v17", "v18", "v19", "v16")
                     SHA_ROUNDS(2, "v18") SHA_SCHEDULE("v18", "v19", "v16", "v17")
                     SHA_ROUNDS(3, "v19") SHA_SCHEDULE("v19", "v16", "v17", "v18")
                     SHA_ROUNDS(4, "v16") SHA_SCHEDULE("v16", "v17", "v18", "v19")
                     SHA_ROUNDS(5, "v17") SHA_SCHEDULE("v17", "v18", "v19", "v16")
                     SHA_ROUNDS(6, "v18") SHA_SCHEDULE("v18", "v19", "v16", "v17")
                     SHA_ROUNDS(7, "v19") SHA_SCHEDULE("v19", "v16", "v17", "v18")
                     SHA_ROUNDS(8, "v16") SHA_SCHEDULE("v16", "v17", "v18", "v19")
                     SHA_ROUNDS(9, "v17") SHA_SCHEDULE("v17", "v18", "v19", "v16")
                     SHA_ROUNDS(10, "v18") SHA_SCHEDULE("v18", "v19", "v16", "v17")
                     SHA_ROUNDS(11, "v19") SHA_SCHEDULE("v19", "v16", "v17", "v18")
                     SHA_ROUNDS(12, "v16")
                     SHA_ROUNDS(13, "v17")
                     SHA_ROUNDS(14, "v18")
                     SHA_ROUNDS(15, "v19")
                     "add v20.4s, v20.4s, v25.4s\n"
                     "add v21.4s, v21.4s, v26.4s\n"
                     "cmp %[at], %[end]\n"
                     "csel %[at], %[stream], %[at], eq\n"
                     "subs %[n], %[n], #1\n"
                     "b.ne 1b\n"
                     "stp q20, q21, [%[h]]\n"
                     : [at] "+&r"(at), [n] "+&r"(n)
                     : [h] "r"(h), [k] "r"(CODE_SHA_K), [end] "r"(end), [stream] "r"(stream)
                     : "cc", "memory", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                       "v24", "v25", "v26");
    uint64_t answer = 0;
    for (unsigned i = 0; i < 8u; i += 2u) {
        answer ^= (uint64_t)h[i] << 32 | h[i + 1u];
    }
    return answer;
}

#undef SHA_ROUNDS
#undef SHA_SCHEDULE

static uint64_t CodeExclusive(uint64_t n) {
    uint64_t word __attribute__((aligned(16))) = 0;
    uint64_t step = 1, v, t;
    uint32_t failed;
    __asm__ volatile("1:\n"
                     "2:\n"
                     "ldaxr %[v], [%[p]]\n"
                     "add %[v], %[v], %[step]\n"
                     "stlxr %w[failed], %[v], [%[p]]\n"
                     "cbnz %w[failed], 2b\n"
                     "add %[t], %[step], %[step], lsl #1\n"
                     "add %[step], %[t], #1\n"
                     "subs %[n], %[n], #1\n"
                     "b.ne 1b\n"
                     : [step] "+&r"(step), [v] "=&r"(v), [t] "=&r"(t), [failed] "=&r"(failed),
                       [n] "+&r"(n)
                     : [p] "r"(&word)
                     : "cc", "memory");
    return word ^ step;
}

static uint64_t CodeVector(uint64_t n) {
    static const float B[4] = { 1, 2, 3, 4 }, K0[4] = { 3, 5, 7, 9 }, K1[4] = { 11, 13, 15, 17 };
    static const float A[4][4] = { { 1, 2, 3, 4 }, { 5, 6, 7, 8 }, { 9, 10, 11, 12 },
                                   { 13, 14, 15, 16 } };
    float32x4_t a0 = vld1q_f32(A[0]), a1 = vld1q_f32(A[1]), a2 = vld1q_f32(A[2]),
                a3 = vld1q_f32(A[3]);
    const float32x4_t b = vld1q_f32(B), k0 = vld1q_f32(K0), k1 = vld1q_f32(K1),
                      three = vdupq_n_f32(3.0f);
    const uint32x4_t mask = vdupq_n_u32(0xffffu);
    int32x4_t t0 = vdupq_n_s32(0), t1 = t0, t2 = t0, t3 = t0;
    __asm__ volatile("1:\n"
                     "fmla %[a0].4s, %[b].4s, %[k0].4s\n"
                     "fmla %[a1].4s, %[b].4s, %[k1].4s\n"
                     "fmul %[a2].4s, %[a2].4s, %[three].4s\n"
                     "fadd %[a2].4s, %[a2].4s, %[b].4s\n"
                     "fadd %[a3].4s, %[a3].4s, %[b].4s\n"
                     "fmul %[a3].4s, %[a3].4s, %[three].4s\n"
                     "fcvtzs %[t0].4s, %[a0].4s\n"
                     "fcvtzs %[t1].4s, %[a1].4s\n"
                     "fcvtzs %[t2].4s, %[a2].4s\n"
                     "fcvtzs %[t3].4s, %[a3].4s\n"
                     "and %[t0].16b, %[t0].16b, %[mask].16b\n"
                     "and %[t1].16b, %[t1].16b, %[mask].16b\n"
                     "and %[t2].16b, %[t2].16b, %[mask].16b\n"
                     "and %[t3].16b, %[t3].16b, %[mask].16b\n"
                     "scvtf %[a0].4s, %[t0].4s\n"
                     "scvtf %[a1].4s, %[t1].4s\n"
                     "scvtf %[a2].4s, %[t2].4s\n"
                     "scvtf %[a3].4s, %[t3].4s\n"
                     "subs %[n], %[n], #1\n"
                     "b.ne 1b\n"
                     : [a0] "+&w"(a0), [a1] "+&w"(a1), [a2] "+&w"(a2), [a3] "+&w"(a3),
                       [t0] "+&w"(t0), [t1] "+&w"(t1), [t2] "+&w"(t2), [t3] "+&w"(t3), [n] "+&r"(n)
                     : [b] "w"(b), [k0] "w"(k0), [k1] "w"(k1), [three] "w"(three),
                       [mask] "w"(mask)
                     : "cc");
    int32_t lanes[4][4];
    vst1q_s32(lanes[0], t0);
    vst1q_s32(lanes[1], t1);
    vst1q_s32(lanes[2], t2);
    vst1q_s32(lanes[3], t3);
    uint64_t acc = 0;
    for (unsigned c = 0; c < 4u; ++c) {
        for (unsigned lane = 0; lane < 4u; ++lane) {
            acc = acc * 31u + (uint32_t)lanes[c][lane];
        }
    }
    return acc;
}

uint64_t PaceCodeWork(PaceCodeKind kind, uint64_t iterations, uint8_t *table,
                      const uint8_t *stream) {
    switch (kind) {
    case PACE_CODE_INT4: return CodeInt4(iterations);
    case PACE_CODE_MUL: return CodeMul(iterations);
    case PACE_CODE_DIV: return CodeDiv(iterations);
    case PACE_CODE_CALLS: return CodeCalls(iterations);
    case PACE_CODE_INDIRECT: return CodeIndirect(iterations);
    case PACE_CODE_SWITCH: return CodeSwitch(iterations);
    case PACE_CODE_BRANCHES: return CodeBranches(iterations);
    case PACE_CODE_TABLE: return CodeTable(iterations, table);
    case PACE_CODE_FDIV: return CodeFdiv(iterations);
    case PACE_CODE_BYTES: return CodeBytes(iterations);
    case PACE_CODE_CRC: return CodeCrc(iterations, stream);
    case PACE_CODE_AES: return CodeAes(iterations, stream);
    case PACE_CODE_SHA: return CodeSha(iterations, stream);
    case PACE_CODE_EXCLUSIVE: return CodeExclusive(iterations);
    case PACE_CODE_CORE: return PaceCodeCore(iterations);
    case PACE_CODE_VECTOR: return CodeVector(iterations);
    case PACE_CODE_KINDS: break;
    }
    return 0;
}

#endif
