// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#include "../check.h"
#include "../fsref.h"
#include "../gpu.h"
#include "../hooks.h"
#include "../paceref.h"
#include "../pacework.h"
#include "../report.h"
#include "../shaders.h"
#include "../volume.h"
#include "tests.h"

// The pace family (docs/families/Pace.md): how fast the machine goes, a suite a part of it. A
// case does a fixed piece of work several times over, reads the system counter around each, and
// reports the median as its line's time with the spread beside it. Nothing judges a speed yet:
// a case fails only when the work's own answer is wrong, so a time is never a wrong answer's.
// The console's run is the reference to come, held both ways by a baseline (baseline.h).

static const char *const CPU = "pace-cpu";
static const char *const MEM = "pace-mem";
static const char *const THREAD = "pace-thread";
static const char *const TIMER = "pace-timer";
static const char *const FS = "pace-fs";
static const char *const GPU = "pace-gpu";

static const char *const NOTIME =
    "the mode word notime asks for no times, and a time is all this case has";

static bool timing = true;

void PaceSetTiming(bool on) {
    timing = on;
}

/// Whether a case runs: under notime it is a skip that was asked for, and ReportStart has its
/// own reasons.
static bool Begin(const char *suite, const char *name) {
    if (!timing) {
        ReportSkip(suite, name, REPORT_REASON_ASKED, NOTIME);
        return false;
    }
    return ReportStart(suite, name);
}

/// The line of a case whose samples came to @p stats: @p scale medians as its time, and beside
/// it @p wrong, what the work got wrong ("" when it was right, which is what passes), @p said,
/// and the samples.
static void End(const char *suite, const char *name, const char *wrong, const char *said,
                const PaceStats *stats, const char *count_of, const char *measure,
                uint64_t scale) {
    if (stats->count == 0u) {
        ReportCase(suite, name, CHECK_NONE, false, wrong[0] != '\0' ? wrong : "nothing was timed");
        return;
    }
    char before[320];
    snprintf(before, sizeof(before), "%s%s%s", wrong,
             wrong[0] != '\0' && said[0] != '\0' ? "; " : "", said);
    char detail[REPORT_KEPT_DETAIL];
    PaceDescribe(detail, sizeof(detail), before, stats, count_of, measure);
    ReportCaseTime(suite, name, CHECK_NONE, wrong[0] == '\0', detail,
                   PaceMicroseconds(PaceMulDiv(stats->median, scale, 1u)));
}

static uint64_t Since(uint64_t start) {
    return armTicksToNs(armGetSystemTick() - start);
}

// ---- threads

#define STACK_SIZE 0x8000u
#define WORKERS 2u

static uint8_t stacks[WORKERS][STACK_SIZE] __attribute__((aligned(0x1000)));
static Thread workers[WORKERS];
// The main thread's own priority, which every thread here runs at: one the kernel lets this
// program have, whatever its limits are.
static int priority = 44;
// A thread of an earlier case never exited: its core may still be taken, and its stack too.
static bool stuck;

static bool Up(const uint32_t *flag) {
    return __atomic_load_n(flag, __ATOMIC_ACQUIRE) != 0u;
}

static void Raise(uint32_t *flag) {
    __atomic_store_n(flag, 1u, __ATOMIC_RELEASE);
}

static void Lower(uint32_t *flag) {
    __atomic_store_n(flag, 0u, __ATOMIC_RELEASE);
}

static bool Launch(uint32_t i, ThreadFunc entry, void *arg, int core, char *why,
                   size_t why_size) {
    Result rc = threadCreate(&workers[i], entry, arg, stacks[i], STACK_SIZE, priority, core);
    if (R_SUCCEEDED(rc)) {
        rc = threadStart(&workers[i]);
        if (R_FAILED(rc)) {
            threadClose(&workers[i]);
        }
    }
    if (R_FAILED(rc)) {
        snprintf(why, why_size, "a thread of %d on core %d did not start: 0x%x", priority, core,
                 (unsigned)rc);
    }
    return R_SUCCEEDED(rc);
}

/// Waits for worker @p i to exit and closes it. One still there after @p limit_ns is told to
/// stop through @p stop and given JOIN_GRACE_NS more, past the longest wait a thread here makes
/// before it looks at that flag; one that never exits keeps its stack and its handle, and the
/// rest of the suite is skipped.
#define JOIN_GRACE_NS 3000000000ull
static bool Join(uint32_t i, uint64_t limit_ns, uint32_t *stop, char *why, size_t why_size) {
    Result rc = svcWaitSynchronizationSingle(workers[i].handle, limit_ns);
    bool in_time = R_SUCCEEDED(rc);
    if (!in_time) {
        Raise(stop);
        rc = svcWaitSynchronizationSingle(workers[i].handle, JOIN_GRACE_NS);
    }
    if (R_FAILED(rc)) {
        stuck = true;
        snprintf(why, why_size,
                 "a thread never exited (0x%x), %llu ms after it began and %llu more after it "
                 "was told to stop: the rest of the suite is skipped",
                 (unsigned)rc, (unsigned long long)(limit_ns / 1000000u),
                 (unsigned long long)(JOIN_GRACE_NS / 1000000u));
        return false;
    }
    threadClose(&workers[i]);
    if (!in_time) {
        snprintf(why, why_size, "a thread was still at work %llu ms after it began, and was told "
                                "to stop",
                 (unsigned long long)(limit_ns / 1000000u));
    }
    return in_time;
}

/// The main thread on core 0 while a suite's threads run, as the kernel family has it, and at
/// the priority they are made with.
typedef struct {
    bool pinned;
    s32 ideal;
    u64 mask;
} MainCore;

static MainCore PinMain(void) {
    MainCore was = { false, 0, 0 };
    s32 own = 44;
    if (R_SUCCEEDED(svcGetThreadPriority(&own, CUR_THREAD_HANDLE))) {
        priority = own;
    }
    was.pinned = R_SUCCEEDED(svcGetThreadCoreMask(&was.ideal, &was.mask, CUR_THREAD_HANDLE)) &&
                 R_SUCCEEDED(svcSetThreadCoreMask(CUR_THREAD_HANDLE, 0, 1u));
    return was;
}

static void UnpinMain(const MainCore *was) {
    if (was->pinned) {
        svcSetThreadCoreMask(CUR_THREAD_HANDLE, was->ideal, (u32)was->mask);
    }
}

static int32_t seen_core[3];

static void ReadCore(void *arg) {
    seen_core[(uintptr_t)arg] = (int32_t)svcGetCurrentProcessorNumber();
}

static const char *const STARTS = "a thread starts on each of the three cores";

/// The bring-up of a suite that runs threads: one on each core, each reading where it is.
static bool ThreadsStart(const char *suite) {
    if (!Begin(suite, STARTS)) {
        return false;
    }
    char why[200] = "";
    bool ok = true;
    for (uint32_t core = 0; core < 3u && !stuck; ++core) {
        seen_core[core] = -1;
        uint32_t unused = 0;
        ok &= Launch(0, ReadCore, (void *)(uintptr_t)core, (int)core, why, sizeof(why)) &&
              Join(0, 1000000000ull, &unused, why, sizeof(why)) &&
              seen_core[core] == (int32_t)core;
    }
    char detail[320];
    snprintf(detail, sizeof(detail),
             "threads of %d made on cores 0, 1 and 2 read cores %d, %d and %d%s%s", priority,
             (int)seen_core[0], (int)seen_core[1], (int)seen_core[2], why[0] != '\0' ? "; " : "",
             why);
    ReportCase(suite, STARTS, CHECK_NONE, ok, detail);
    return ok;
}

/// What a case says in place of running when the suite's bring-up or an earlier case's thread
/// leaves it nothing to run on.
static void SkipDepends(const char *suite, const char *name) {
    if (!timing) {
        ReportSkip(suite, name, REPORT_REASON_ASKED, NOTIME);
        return;
    }
    ReportSkip(suite, name, REPORT_REASON_DEPENDS,
               stuck ? "a thread of an earlier case never exited"
                     : "threads could not be started where the cases need them");
}

// ---- pace-cpu: the three loops on each core

#define CPU_WINDOWS 10u
#define CPU_WINDOW_NS 1000000000ull
/// Runs of a loop after which a counter that has not moved is taken for one that never will.
#define CPU_STALL_RUNS 64u

typedef union {
    uint64_t pair[2];
    uint32_t lanes[12];
} Sums;

static const struct {
    const char *word;
    uint32_t chunk;
    uint64_t unit;
    const char *measure;
    uint32_t instructions;
} LOOPS[3] = {
    { "integer", PACE_INTEGER_CHUNK, 100000000u, "the time of 100 million iterations",
      PACE_INTEGER_INSTRUCTIONS },
    { "floating point", PACE_FLOAT_CHUNK, 10000000u, "the time of 10 million iterations",
      PACE_FLOAT_INSTRUCTIONS },
    { "vector", PACE_VECTOR_CHUNK, 10000000u, "the time of 10 million iterations",
      PACE_VECTOR_INSTRUCTIONS },
};

static const char *const CPU_NAMES[3][3] = {
    { "core 0 runs the integer loop", "core 0 runs the floating point loop",
      "core 0 runs the vector loop" },
    { "core 1 runs the integer loop", "core 1 runs the floating point loop",
      "core 1 runs the vector loop" },
    { "core 2 runs the integer loop", "core 2 runs the floating point loop",
      "core 2 runs the vector loop" },
};

static struct {
    uint32_t loop;
    uint32_t stop;
    int32_t core;
    uint32_t windows;
    uint64_t runs;
    uint64_t wrong;
    bool stalled;
    Sums expected;
    uint64_t samples[CPU_WINDOWS];
} cpu;

static void Work(uint32_t loop, Sums *out) {
    memset(out, 0, sizeof(*out));
    switch (loop) {
    case 0:
        PaceWorkInteger(PACE_INTEGER_CHUNK, out->pair);
        break;
    case 1:
        PaceWorkFloat(PACE_FLOAT_CHUNK, out->pair);
        break;
    default:
        PaceWorkVector(PACE_VECTOR_CHUNK, out->lanes);
        break;
    }
}

static void Expect(uint32_t loop, Sums *out) {
    memset(out, 0, sizeof(*out));
    switch (loop) {
    case 0:
        PaceIntegerLoop(PACE_INTEGER_CHUNK, out->pair);
        break;
    case 1:
        PaceFloatLoop(PACE_FLOAT_CHUNK, out->pair);
        break;
    default:
        PaceVectorLoop(PACE_VECTOR_CHUNK, out->lanes);
        break;
    }
}

/// Windows of a second, each as many runs of the loop as fit, the counter read after every run:
/// a window's sample is the time its pace gives the loop's unit of work.
static void CpuWorker(void *arg) {
    cpu.core = (int32_t)svcGetCurrentProcessorNumber();
    const uint64_t window = armNsToTicks(CPU_WINDOW_NS);
    for (uint32_t w = 0; w < CPU_WINDOWS && !Up(&cpu.stop); ++w) {
        Sums got;
        uint64_t runs = 0;
        const uint64_t start = armGetSystemTick();
        uint64_t now = start;
        do {
            Work(cpu.loop, &got);
            cpu.wrong += memcmp(&got, &cpu.expected, sizeof(got)) != 0 ? 1u : 0u;
            ++runs;
            now = armGetSystemTick();
            if (runs == CPU_STALL_RUNS && now == start) {
                cpu.stalled = true;
                return;
            }
        } while (now - start < window && !Up(&cpu.stop));
        cpu.runs += runs;
        cpu.samples[w] = PaceTimeFor(armTicksToNs(now - start), runs * LOOPS[cpu.loop].chunk,
                                     LOOPS[cpu.loop].unit);
        cpu.windows = w + 1u;
    }
}

static void CpuCase(uint32_t core, uint32_t loop) {
    const char *const name = CPU_NAMES[core][loop];
    if (!Begin(CPU, name)) {
        return;
    }
    memset(&cpu, 0, sizeof(cpu));
    cpu.loop = loop;
    cpu.core = -1;
    Expect(loop, &cpu.expected);
    char wrong[240] = "";
    if (Launch(0, CpuWorker, NULL, (int)core, wrong, sizeof(wrong))) {
        Join(0, CPU_WINDOWS * CPU_WINDOW_NS * 3u, &cpu.stop, wrong, sizeof(wrong));
    }
    if (wrong[0] == '\0' && cpu.stalled) {
        snprintf(wrong, sizeof(wrong), "the system counter did not move over %u runs of the loop",
                 (unsigned)CPU_STALL_RUNS);
    } else if (wrong[0] == '\0' && cpu.wrong != 0u) {
        snprintf(wrong, sizeof(wrong),
                 "%llu of %llu runs of %u iterations left other sums than the loop's",
                 (unsigned long long)cpu.wrong, (unsigned long long)cpu.runs,
                 (unsigned)LOOPS[loop].chunk);
    } else if (wrong[0] == '\0' && cpu.windows < CPU_WINDOWS) {
        snprintf(wrong, sizeof(wrong), "%u of %u windows ran", (unsigned)cpu.windows,
                 (unsigned)CPU_WINDOWS);
    }
    PaceStats stats;
    PaceStatsOf(cpu.samples, stuck ? 0u : cpu.windows, &stats);
    // Tenths of a million iterations a second.
    const uint64_t rate = PaceRate(LOOPS[loop].unit, stats.median) / 100000u;
    char said[200];
    snprintf(said, sizeof(said),
             "%llu.%u million iterations of %u instructions a second on core %d, in %llu runs of "
             "%u",
             (unsigned long long)(rate / 10u), (unsigned)(rate % 10u),
             (unsigned)LOOPS[loop].instructions, (int)cpu.core, (unsigned long long)cpu.runs,
             (unsigned)LOOPS[loop].chunk);
    End(CPU, name, wrong, said, &stats, "windows of 1 s", LOOPS[loop].measure, 1u);
}

void RunPaceCpu(void) {
    stuck = false;
    const MainCore was = PinMain();
    const bool open = ThreadsStart(CPU);
    for (uint32_t core = 0; core < 3u; ++core) {
        for (uint32_t loop = 0; loop < 3u; ++loop) {
            if (!open || stuck) {
                SkipDepends(CPU, CPU_NAMES[core][loop]);
            } else {
                CpuCase(core, loop);
            }
        }
    }
    UnpinMain(&was);
}

// ---- pace-mem: a walk, a fill, a sum and a copy, on the main thread

#define MEM_BYTES (64u * 1024u * 1024u)
#define MEM_HALF (MEM_BYTES / 2u)
#define MEM_WORDS (MEM_HALF / 8u)
#define MEM_WALKS 16u
/// Passes of 32 MiB in a sample, and samples in a case.
#define MEM_PASSES 8u
#define MEM_SAMPLES 16u

_Static_assert(PACE_WALK_SLOTS * 8u == MEM_BYTES, "the walk's table is the whole block");
_Static_assert(MEM_WALKS <= MEM_SAMPLES, "the walk's samples fit the suite's");

static const char *const MEM_WALK = "a walk through 64 MiB reads a slot at a time";
static const char *const MEM_FILL = "32 MiB are filled with one word, eight times";
static const char *const MEM_SUM = "32 MiB are read and added up, eight times";
static const char *const MEM_COPY = "32 MiB are copied, eight times";

static uint64_t mem_samples[MEM_SAMPLES];

/// The first half as the sum and the copy read it, every word its own; hands back their sum.
static uint64_t MemPattern(uint64_t *memory) {
    uint64_t sum = 0;
    for (uint64_t i = 0; i < MEM_WORDS; ++i) {
        memory[i] = i * PACE_INTEGER_A + PACE_INTEGER_C;
        sum += memory[i];
    }
    return sum;
}

static void MemEnd(const char *name, const char *wrong) {
    PaceStats stats;
    PaceStatsOf(mem_samples, MEM_SAMPLES, &stats);
    char said[96];
    snprintf(said, sizeof(said), "%llu MiB a second",
             (unsigned long long)PaceRate((uint64_t)(MEM_HALF >> 20) * MEM_PASSES, stats.median));
    End(MEM, name, wrong, said, &stats, "rounds of 8 passes", "the time of 256 MiB", 1u);
}

static void MemWalk(uint64_t *memory) {
    if (!Begin(MEM, MEM_WALK)) {
        return;
    }
    for (uint64_t i = 0; i < PACE_WALK_SLOTS; ++i) {
        memory[i] = PaceWalkNext(i, PACE_WALK_SLOTS);
    }
    // Each sample goes on from where the one before it ended, so none reads a slot again.
    uint64_t slot = 0;
    uint32_t astray = 0;
    for (uint32_t s = 0; s < MEM_WALKS; ++s) {
        const uint64_t want = PaceWalkEnd(slot, PACE_WALK_STEPS, PACE_WALK_SLOTS);
        const uint64_t start = armGetSystemTick();
        const uint64_t got = PaceWorkWalk(memory, slot, PACE_WALK_STEPS);
        mem_samples[s] = Since(start);
        astray += got != want ? 1u : 0u;
        slot = want;
    }
    char wrong[120] = "";
    if (astray != 0u) {
        snprintf(wrong, sizeof(wrong), "%u of %u walks ended on another slot than the rule gives",
                 (unsigned)astray, (unsigned)MEM_WALKS);
    }
    PaceStats stats;
    PaceStatsOf(mem_samples, MEM_WALKS, &stats);
    const uint64_t tenths = PaceMulDiv(stats.median, 10u, PACE_WALK_STEPS);
    char said[96];
    snprintf(said, sizeof(said), "%llu.%u ns a read", (unsigned long long)(tenths / 10u),
             (unsigned)(tenths % 10u));
    End(MEM, MEM_WALK, wrong, said, &stats, "walks", "the time of 262144 reads", 1u);
}

static void MemFill(uint64_t *memory) {
    if (!Begin(MEM, MEM_FILL)) {
        return;
    }
    uint64_t *const half = memory + MEM_WORDS;
    uint64_t value = 0;
    for (uint32_t s = 0; s < MEM_SAMPLES; ++s) {
        const uint64_t start = armGetSystemTick();
        for (uint32_t p = 0; p < MEM_PASSES; ++p) {
            value = PACE_INTEGER_A * (s * MEM_PASSES + p + 1u);
            PaceWorkFill(half, value, MEM_HALF);
        }
        mem_samples[s] = Since(start);
    }
    uint64_t others = 0;
    for (uint64_t i = 0; i < MEM_WORDS; ++i) {
        others += half[i] != value ? 1u : 0u;
    }
    char wrong[120] = "";
    if (others != 0u) {
        snprintf(wrong, sizeof(wrong), "%llu of %u words do not hold the last word written",
                 (unsigned long long)others, (unsigned)MEM_WORDS);
    }
    MemEnd(MEM_FILL, wrong);
}

static void MemSum(uint64_t *memory) {
    if (!Begin(MEM, MEM_SUM)) {
        return;
    }
    const uint64_t want = MemPattern(memory);
    uint32_t off = 0;
    for (uint32_t s = 0; s < MEM_SAMPLES; ++s) {
        uint64_t got[MEM_PASSES];
        const uint64_t start = armGetSystemTick();
        for (uint32_t p = 0; p < MEM_PASSES; ++p) {
            got[p] = PaceWorkSum(memory, MEM_HALF);
        }
        mem_samples[s] = Since(start);
        for (uint32_t p = 0; p < MEM_PASSES; ++p) {
            off += got[p] != want ? 1u : 0u;
        }
    }
    char wrong[120] = "";
    if (off != 0u) {
        snprintf(wrong, sizeof(wrong), "%u of %u passes added the words up to another sum",
                 (unsigned)off, (unsigned)(MEM_SAMPLES * MEM_PASSES));
    }
    MemEnd(MEM_SUM, wrong);
}

static void MemCopy(uint64_t *memory) {
    if (!Begin(MEM, MEM_COPY)) {
        return;
    }
    (void)MemPattern(memory);
    uint64_t *const half = memory + MEM_WORDS;
    for (uint32_t s = 0; s < MEM_SAMPLES; ++s) {
        const uint64_t start = armGetSystemTick();
        for (uint32_t p = 0; p < MEM_PASSES; ++p) {
            PaceWorkCopy(half, memory, MEM_HALF);
        }
        mem_samples[s] = Since(start);
    }
    MemEnd(MEM_COPY,
           memcmp(half, memory, MEM_HALF) != 0 ? "the copy differs from its source" : "");
}

void RunPaceMem(void) {
    static const char *const NAMES[4] = { MEM_WALK, MEM_FILL, MEM_SUM, MEM_COPY };
    uint64_t *const memory = timing ? memalign(0x1000, MEM_BYTES) : NULL;
    if (memory == NULL) {
        for (uint32_t i = 0; i < 4u; ++i) {
            ReportSkip(MEM, NAMES[i], timing ? REPORT_REASON_UNAVAILABLE : REPORT_REASON_ASKED,
                       timing ? "the heap has no 64 MiB for the suite to work in" : NOTIME);
        }
        return;
    }
    MemWalk(memory);
    MemFill(memory);
    MemSum(memory);
    MemCopy(memory);
    free(memory);
}

// ---- pace-thread: a wake and a mutex passed, on one core and across two

#define HANDOFFS 2000u
/// How long a thread waits for the other before the case gives up.
#define HANDOFF_LIMIT_NS 2000000000ull
/// How long after the other thread said it was about to sleep it is taken to be asleep.
#define SETTLE_NS 50000u
/// The mark the kernel's mutex word carries while a thread waits for it.
#define MUTEX_WAITERS 0x40000000u

static const char *const WAKE_NAMES[2] = {
    "a thread asleep on an event wakes at a signal from its own core",
    "a thread asleep on an event wakes at a signal from another core",
};
static const char *const MUTEX_NAMES[2] = {
    "a mutex passes to a thread waiting for it on the same core",
    "a mutex passes to a thread waiting for it on another core",
};

static struct {
    Handle readable[2];
    Handle writable[2];
    Mutex baton;
    uint32_t waiting[2];
    uint32_t done;
    // Written by the thread that has the turn, read by the other only once it has it.
    uint32_t count;
    bool armed;
    uint64_t sent;
    uint64_t samples[HANDOFFS];
    char failure[160];
} pass;

/// Waits for the other thread to be asleep in the kernel: until @p word carries @p mask, which
/// the other sets just before the call it sleeps in, and SETTLE_NS more. A thread on this one's
/// core cannot get there while this one spins, so the wait yields.
static bool Asleep(const uint32_t *word, uint32_t mask) {
    const uint64_t start = armGetSystemTick();
    uint32_t spins = 0;
    while ((__atomic_load_n(word, __ATOMIC_ACQUIRE) & mask) == 0u) {
        if (++spins % 64u == 0u) {
            if (Since(start) > HANDOFF_LIMIT_NS) {
                return false;
            }
            svcSleepThread(YieldType_WithoutCoreMigration);
        }
    }
    const uint64_t seen = armGetSystemTick();
    while (Since(seen) < SETTLE_NS) {
    }
    return true;
}

static void PassFailed(const char *what, uint32_t me, Result rc) {
    if (pass.failure[0] == '\0') {
        snprintf(pass.failure, sizeof(pass.failure), "thread %u: %s (0x%x) after %u handoffs",
                 (unsigned)me, what, (unsigned)rc, (unsigned)pass.count);
    }
    Raise(&pass.done);
}

/// Sleeps on its event, and once woken signals the other's: a sample is the time from just
/// before the signal to just after the wait returned.
static void EventPlayer(void *arg) {
    const uint32_t me = (uint32_t)(uintptr_t)arg;
    const uint32_t other = me ^ 1u;
    for (;;) {
        Raise(&pass.waiting[me]);
        s32 index = -1;
        const Result rc =
            svcWaitSynchronization(&index, &pass.readable[me], 1, HANDOFF_LIMIT_NS);
        const uint64_t woke = armGetSystemTick();
        Lower(&pass.waiting[me]);
        if (R_FAILED(rc)) {
            PassFailed("the wait for its event ended", me, rc);
        } else {
            svcResetSignal(pass.readable[me]);
        }
        if (Up(&pass.done)) {
            break;
        }
        if (pass.armed) {
            pass.samples[pass.count++] = armTicksToNs(woke - pass.sent);
        }
        if (pass.count >= HANDOFFS) {
            Raise(&pass.done);
        } else if (!Asleep(&pass.waiting[other], 1u)) {
            PassFailed("the other thread never came to wait", me, 0);
        }
        if (Up(&pass.done)) {
            break;
        }
        pass.armed = true;
        pass.sent = armGetSystemTick();
        svcSignalEvent(pass.writable[other]);
    }
    // Whatever ended it, the other is not left asleep.
    svcSignalEvent(pass.writable[other]);
}

/// Takes the mutex, waits for the other to be asleep on it, and unlocks, which hands it over:
/// a sample is the time from just before the unlock to just after the other's lock returned.
static void MutexPlayer(void *arg) {
    const uint32_t me = (uint32_t)(uintptr_t)arg;
    for (;;) {
        mutexLock(&pass.baton);
        const uint64_t got = armGetSystemTick();
        if (!Up(&pass.done) && pass.armed) {
            pass.samples[pass.count++] = armTicksToNs(got - pass.sent);
        }
        if (!Up(&pass.done) && pass.count >= HANDOFFS) {
            Raise(&pass.done);
        }
        if (!Up(&pass.done) && !Asleep((const uint32_t *)&pass.baton, MUTEX_WAITERS)) {
            PassFailed("the other thread never came to wait for the mutex", me, 0);
        }
        if (Up(&pass.done)) {
            mutexUnlock(&pass.baton);
            break;
        }
        pass.armed = true;
        pass.sent = armGetSystemTick();
        mutexUnlock(&pass.baton);
    }
}

static void PassEnd(const char *name, bool joined, const char *why, const char *measure) {
    char wrong[240] = "";
    if (!joined) {
        snprintf(wrong, sizeof(wrong), "%s", why);
    } else if (pass.failure[0] != '\0') {
        snprintf(wrong, sizeof(wrong), "%s", pass.failure);
    }
    PaceStats stats;
    PaceStatsOf(pass.samples, stuck ? 0u : pass.count, &stats);
    End(THREAD, name, wrong, "the line's time is 1000 of the middle one", &stats, "handoffs",
        measure, 1000u);
}

static void WakeCase(uint32_t which) {
    const char *const name = WAKE_NAMES[which];
    if (!Begin(THREAD, name)) {
        return;
    }
    memset(&pass, 0, sizeof(pass));
    char why[200] = "";
    bool made = R_SUCCEEDED(svcCreateEvent(&pass.writable[0], &pass.readable[0]));
    if (made && R_FAILED(svcCreateEvent(&pass.writable[1], &pass.readable[1]))) {
        svcCloseHandle(pass.writable[0]);
        svcCloseHandle(pass.readable[0]);
        made = false;
    }
    if (!made) {
        ReportCase(THREAD, name, CHECK_NONE, false, "the kernel made no pair of events");
        return;
    }
    // Thread 0 on core 1; thread 1 beside it, or on core 2.
    bool joined = false;
    if (Launch(0, EventPlayer, (void *)(uintptr_t)0u, 1, why, sizeof(why))) {
        if (Launch(1, EventPlayer, (void *)(uintptr_t)1u, which == 0u ? 1 : 2, why,
                   sizeof(why))) {
            // Both asleep before the first signal, which is this thread's and no sample.
            for (uint32_t i = 0; i < 10000u && !(Up(&pass.waiting[0]) && Up(&pass.waiting[1]));
                 ++i) {
                svcSleepThread(100000);
            }
            svcSleepThread(1000000);
            svcSignalEvent(pass.writable[0]);
            joined = Join(1, 30000000000ull, &pass.done, why, sizeof(why));
        } else {
            Raise(&pass.done);
        }
        svcSignalEvent(pass.writable[0]);
        joined = Join(0, joined ? 30000000000ull : 1000000000ull, &pass.done, why, sizeof(why)) &&
                 joined;
    }
    for (uint32_t i = 0; i < 2u && !stuck; ++i) {
        svcCloseHandle(pass.writable[i]);
        svcCloseHandle(pass.readable[i]);
    }
    PassEnd(name, joined, why, "the time from a signal to the woken thread running");
}

static void MutexCase(uint32_t which) {
    const char *const name = MUTEX_NAMES[which];
    if (!Begin(THREAD, name)) {
        return;
    }
    memset(&pass, 0, sizeof(pass));
    mutexInit(&pass.baton);
    char why[200] = "";
    bool joined = false;
    if (Launch(0, MutexPlayer, (void *)(uintptr_t)0u, 1, why, sizeof(why))) {
        if (Launch(1, MutexPlayer, (void *)(uintptr_t)1u, which == 0u ? 1 : 2, why,
                   sizeof(why))) {
            joined = Join(1, 30000000000ull, &pass.done, why, sizeof(why));
        } else {
            Raise(&pass.done);
        }
        joined = Join(0, joined ? 30000000000ull : 3000000000ull, &pass.done, why, sizeof(why)) &&
                 joined;
    }
    PassEnd(name, joined, why, "the time from an unlock to the waiting thread holding the mutex");
}

void RunPaceThread(void) {
    stuck = false;
    const MainCore was = PinMain();
    const bool open = ThreadsStart(THREAD);
    for (uint32_t which = 0; which < 4u; ++which) {
        const char *const name = which < 2u ? WAKE_NAMES[which] : MUTEX_NAMES[which - 2u];
        if (!open || stuck) {
            SkipDepends(THREAD, name);
        } else if (which < 2u) {
            WakeCase(which);
        } else {
            MutexCase(which - 2u);
        }
    }
    UnpinMain(&was);
}

// ---- pace-timer: the counter read, and sleeps held to the time they asked for

#define COUNTER_READS 1000000u
#define COUNTER_SAMPLES 16u
#define SLEEPS_MAX 2000u

static const char *const COUNTER = "the system counter is read a million times";

/// Groups a length's sleeps are cut into for its second case: a sample is how far apart the
/// longest and the shortest sleep of a group are.
#define SLEEP_GROUPS 10u

static const struct {
    uint64_t ns;
    uint32_t count;
    const char *over;
    const char *apart;
    const char *over_measure;
    const char *apart_measure;
} SLEEPS[3] = {
    { 100000u, 2000u, "sleeps of 100 us, by how much each runs over",
      "sleeps of 100 us, how far apart the longest and the shortest of 200 are",
      "the time a sleep lasts past the 100 us asked",
      "the time between the longest and the shortest of 200 sleeps in a row" },
    { 1000000u, 1000u, "sleeps of 1 ms, by how much each runs over",
      "sleeps of 1 ms, how far apart the longest and the shortest of 100 are",
      "the time a sleep lasts past the 1 ms asked",
      "the time between the longest and the shortest of 100 sleeps in a row" },
    { 16000000u, 100u, "sleeps of 16 ms, by how much each runs over",
      "sleeps of 16 ms, how far apart the longest and the shortest of 10 are",
      "the time a sleep lasts past the 16 ms asked",
      "the time between the longest and the shortest of 10 sleeps in a row" },
};

static uint64_t slept[SLEEPS_MAX];
static uint64_t timer_samples[SLEEPS_MAX];

static void CounterCase(void) {
    if (!Begin(TIMER, COUNTER)) {
        return;
    }
    uint64_t least = UINT64_MAX;
    uint64_t most = 0;
    uint64_t back = 0;
    for (uint32_t s = 0; s < COUNTER_SAMPLES; ++s) {
        uint64_t out[3] = { 0, 0, 0 };
        const uint64_t start = armGetSystemTick();
        PaceWorkCounter(COUNTER_READS, out);
        timer_samples[s] = Since(start);
        least = out[0] < least ? out[0] : least;
        most = out[1] > most ? out[1] : most;
        back += out[2];
    }
    char wrong[120] = "";
    if (back != 0u) {
        snprintf(wrong, sizeof(wrong), "%llu reads were under the read before them",
                 (unsigned long long)back);
    } else if (least == UINT64_MAX) {
        snprintf(wrong, sizeof(wrong), "the counter never moved");
    }
    PaceStats stats;
    PaceStatsOf(timer_samples, COUNTER_SAMPLES, &stats);
    const uint64_t tenths = stats.median / (COUNTER_READS / 10u);
    char said[200];
    snprintf(said, sizeof(said),
             "%llu.%u ns a read; between two reads the counter's smallest step was %llu of its "
             "19.2 MHz ticks and its largest %llu",
             (unsigned long long)(tenths / 10u), (unsigned)(tenths % 10u),
             (unsigned long long)(least == UINT64_MAX ? 0u : least), (unsigned long long)most);
    End(TIMER, COUNTER, wrong, said, &stats, "passes", "the time of a million reads", 1u);
}

/// The sleeps of row @p row, each timed by the counter around the call.
static void Sleep(uint32_t row) {
    for (uint32_t i = 0; i < SLEEPS[row].count; ++i) {
        const uint64_t start = armGetSystemTick();
        svcSleepThread((s64)SLEEPS[row].ns);
        slept[i] = Since(start);
    }
}

static void SleepCases(uint32_t row) {
    const uint32_t count = SLEEPS[row].count;
    const uint64_t asked = SLEEPS[row].ns;
    bool measured = false;
    if (Begin(TIMER, SLEEPS[row].over)) {
        Sleep(row);
        measured = true;
        uint32_t early = 0;
        uint64_t earliest = 0;
        for (uint32_t i = 0; i < count; ++i) {
            timer_samples[i] = slept[i] > asked ? slept[i] - asked : 0u;
            if (slept[i] < asked) {
                ++early;
                earliest = asked - slept[i] > earliest ? asked - slept[i] : earliest;
            }
        }
        PaceStats stats;
        PaceStatsOf(timer_samples, count, &stats);
        char said[200];
        if (early == 0u) {
            snprintf(said, sizeof(said),
                     "the line's time is 1000 of the middle one; none ended before its time");
        } else {
            snprintf(said, sizeof(said),
                     "the line's time is 1000 of the middle one; %u ended before their time, the "
                     "earliest %llu ns before",
                     (unsigned)early, (unsigned long long)earliest);
        }
        End(TIMER, SLEEPS[row].over, "", said, &stats, "sleeps", SLEEPS[row].over_measure, 1000u);
    }
    if (Begin(TIMER, SLEEPS[row].apart)) {
        if (!measured) {
            Sleep(row);
        }
        uint64_t shortest = UINT64_MAX;
        uint64_t longest = 0;
        const uint32_t group = count / SLEEP_GROUPS;
        for (uint32_t g = 0; g < SLEEP_GROUPS; ++g) {
            uint64_t least = UINT64_MAX;
            uint64_t most = 0;
            for (uint32_t i = g * group; i < (g + 1u) * group; ++i) {
                least = slept[i] < least ? slept[i] : least;
                most = slept[i] > most ? slept[i] : most;
            }
            timer_samples[g] = most - least;
            shortest = least < shortest ? least : shortest;
            longest = most > longest ? most : longest;
        }
        PaceStats stats;
        PaceStatsOf(timer_samples, SLEEP_GROUPS, &stats);
        char said[200];
        snprintf(said, sizeof(said),
                 "the line's time is 1000 of the middle one; of all %u the shortest lasted %llu "
                 "ns and the longest %llu",
                 (unsigned)count, (unsigned long long)shortest, (unsigned long long)longest);
        End(TIMER, SLEEPS[row].apart, "", said, &stats, "groups", SLEEPS[row].apart_measure,
            1000u);
    }
}

void RunPaceTimer(void) {
    CounterCase();
    for (uint32_t row = 0; row < 3u; ++row) {
        SleepCases(row);
    }
}

// ---- pace-fs: a file written and read back, and small files made and removed

#define FS_PACE_DIRECTORY "/edenprobe/pace"
#define FS_PACE_BIG FS_PACE_DIRECTORY "/big.bin"
#define FS_PACE_SIZE (4u * 1024u * 1024u)
#define FS_PACE_BLOCK (64u * 1024u)
#define FS_PACE_SAMPLES 8u
#define FS_PACE_SMALL 64u
#define FS_PACE_SMALL_SIZE 4096u
#define FS_PACE_SMALL_SAMPLES 4u

static const char *const FS_OPENS = "the memory card opens as a file system";
static const char *const FS_WRITE = "a 4 MiB file is made and written 64 KiB at a time";
static const char *const FS_READ = "the 4 MiB file is opened and read 64 KiB at a time";
static const char *const FS_SMALL = "64 files of 4 KiB are made, written, read back and removed";

static uint8_t fs_block[FS_PACE_BLOCK];
static uint64_t fs_samples[FS_PACE_SAMPLES];
static uint64_t fs_spent;
static uint64_t fs_started;
static Result fs_failed;
static const char *fs_failed_at;

static void Tic(void) {
    fs_started = armGetSystemTick();
}

/// Ends a timed call: its time is added, and the first failing result kept with @p what it was.
static void Toc(Result rc, const char *what) {
    fs_spent += armGetSystemTick() - fs_started;
    if (R_FAILED(rc) && fs_failed == 0) {
        fs_failed = rc;
        fs_failed_at = what;
    }
}

/// Byte @p offset of the big file as sample @p salt writes it: the fs family's pattern, a
/// different stretch of it each time.
static uint8_t BigByte(uint32_t salt, uint64_t offset) {
    return FsRefPatternByte(offset + (uint64_t)salt * 7919u);
}

/// One sample of the write case: the file made at its size, opened, written, flushed, closed.
static void WriteBig(Volume *fs, uint32_t salt) {
    VolumeDeleteFile(fs, FS_PACE_BIG);
    VolumeFile file;
    Tic();
    Result rc = VolumeCreateFile(fs, FS_PACE_BIG, FS_PACE_SIZE);
    Toc(rc, "making the file");
    Tic();
    rc = VolumeOpenFile(fs, FS_PACE_BIG, FsOpenMode_Write, &file);
    Toc(rc, "opening the file to write");
    if (R_FAILED(rc)) {
        return;
    }
    for (uint64_t offset = 0; offset < FS_PACE_SIZE; offset += FS_PACE_BLOCK) {
        for (uint32_t b = 0; b < FS_PACE_BLOCK; ++b) {
            fs_block[b] = BigByte(salt, offset + b);
        }
        Tic();
        rc = VolumeFileWrite(&file, (s64)offset, fs_block, FS_PACE_BLOCK, FsWriteOption_None);
        Toc(rc, "a write");
    }
    Tic();
    rc = VolumeFileFlush(&file);
    Toc(rc, "the flush");
    Tic();
    VolumeFileClose(&file);
    Toc(0, "");
}

/// One sample of the read case; hands back how many bytes were not the ones @p salt wrote.
static uint64_t ReadBig(Volume *fs, uint32_t salt) {
    uint64_t wrong = 0;
    VolumeFile file;
    Tic();
    Result rc = VolumeOpenFile(fs, FS_PACE_BIG, FsOpenMode_Read, &file);
    Toc(rc, "opening the file to read");
    if (R_FAILED(rc)) {
        return FS_PACE_SIZE;
    }
    for (uint64_t offset = 0; offset < FS_PACE_SIZE; offset += FS_PACE_BLOCK) {
        u64 got = 0;
        Tic();
        rc = VolumeFileRead(&file, (s64)offset, fs_block, FS_PACE_BLOCK, &got);
        Toc(rc, "a read");
        for (uint32_t b = 0; b < FS_PACE_BLOCK; ++b) {
            wrong += b >= got || fs_block[b] != BigByte(salt, offset + b) ? 1u : 0u;
        }
    }
    Tic();
    VolumeFileClose(&file);
    Toc(0, "");
    return wrong;
}

static uint8_t SmallByte(uint32_t salt, uint32_t file, uint32_t offset) {
    return (uint8_t)(salt * 131u + file * 31u + offset * 7u + 1u);
}

/// One sample of the small files case; hands back how many files read back other bytes.
static uint32_t SmallFiles(Volume *fs, uint32_t salt) {
    uint32_t wrong = 0;
    char path[64];
    for (uint32_t i = 0; i < FS_PACE_SMALL; ++i) {
        snprintf(path, sizeof(path), FS_PACE_DIRECTORY "/%02u.bin", (unsigned)i);
        for (uint32_t b = 0; b < FS_PACE_SMALL_SIZE; ++b) {
            fs_block[b] = SmallByte(salt, i, b);
        }
        VolumeFile file;
        Tic();
        Result rc = VolumeCreateFile(fs, path, FS_PACE_SMALL_SIZE);
        Toc(rc, "making a small file");
        Tic();
        rc = VolumeOpenFile(fs, path, FsOpenMode_Write, &file);
        Toc(rc, "opening a small file to write");
        if (R_SUCCEEDED(rc)) {
            Tic();
            rc = VolumeFileWrite(&file, 0, fs_block, FS_PACE_SMALL_SIZE, FsWriteOption_None);
            Toc(rc, "writing a small file");
            Tic();
            VolumeFileClose(&file);
            Toc(0, "");
        }
    }
    for (uint32_t i = 0; i < FS_PACE_SMALL; ++i) {
        snprintf(path, sizeof(path), FS_PACE_DIRECTORY "/%02u.bin", (unsigned)i);
        VolumeFile file;
        u64 got = 0;
        memset(fs_block, 0, FS_PACE_SMALL_SIZE);
        Tic();
        Result rc = VolumeOpenFile(fs, path, FsOpenMode_Read, &file);
        Toc(rc, "opening a small file to read");
        if (R_SUCCEEDED(rc)) {
            Tic();
            rc = VolumeFileRead(&file, 0, fs_block, FS_PACE_SMALL_SIZE, &got);
            Toc(rc, "reading a small file");
            Tic();
            VolumeFileClose(&file);
            Toc(0, "");
        }
        bool same = got == FS_PACE_SMALL_SIZE;
        for (uint32_t b = 0; same && b < FS_PACE_SMALL_SIZE; ++b) {
            same = fs_block[b] == SmallByte(salt, i, b);
        }
        wrong += same ? 0u : 1u;
        Tic();
        rc = VolumeDeleteFile(fs, path);
        Toc(rc, "removing a small file");
    }
    return wrong;
}

static void FsEnd(const char *name, uint64_t wrong_count, const char *wrong_what, uint32_t count,
                  const char *said, const char *count_of, const char *measure) {
    char wrong[200] = "";
    if (fs_failed != 0) {
        snprintf(wrong, sizeof(wrong), "%s failed: 0x%x", fs_failed_at, (unsigned)fs_failed);
    } else if (wrong_count != 0u) {
        snprintf(wrong, sizeof(wrong), "%llu %s", (unsigned long long)wrong_count, wrong_what);
    }
    PaceStats stats;
    PaceStatsOf(fs_samples, count, &stats);
    char rate[120];
    if (said != NULL) {
        snprintf(rate, sizeof(rate), "%s", said);
    } else {
        // Tenths of a MiB a second.
        const uint64_t tenths = PaceRate((FS_PACE_SIZE >> 20) * 10u, stats.median);
        snprintf(rate, sizeof(rate), "%llu.%u MiB a second%s", (unsigned long long)(tenths / 10u),
                 (unsigned)(tenths % 10u), HooksOn() ? ", through nn::fs" : "");
    }
    End(FS, name, wrong, rate, &stats, count_of, measure, 1u);
}

void RunPaceFs(void) {
    static const char *const NAMES[3] = { FS_WRITE, FS_READ, FS_SMALL };
    if (!Begin(FS, FS_OPENS)) {
        for (uint32_t i = 0; i < 3u; ++i) {
            ReportSkip(FS, NAMES[i], timing ? REPORT_REASON_DEPENDS : REPORT_REASON_ASKED,
                       timing ? "the case that opens the card did not run" : NOTIME);
        }
        return;
    }
    Volume fs;
    const Result opened = VolumeOpenCard(&fs, "edenprobe-pace");
    CHECK_TRUE(FS, FS_OPENS, CHECK_NONE, R_SUCCEEDED(opened), "result 0x%x%s", (unsigned)opened,
               HooksOn() ? ", nn::fs::MountSdCard" : "");
    if (R_FAILED(opened)) {
        for (uint32_t i = 0; i < 3u; ++i) {
            ReportSkip(FS, NAMES[i], REPORT_REASON_DEPENDS, "the memory card did not open");
        }
        return;
    }
    VolumeCreateDirectory(&fs, "/edenprobe");
    VolumeCreateDirectory(&fs, FS_PACE_DIRECTORY);

    // The file the last write sample leaves is the one the read case reads.
    const uint32_t last = FS_PACE_SAMPLES - 1u;
    bool written = false;
    if (Begin(FS, FS_WRITE)) {
        fs_failed = 0;
        for (uint32_t s = 0; s < FS_PACE_SAMPLES; ++s) {
            fs_spent = 0;
            WriteBig(&fs, s);
            fs_samples[s] = armTicksToNs(fs_spent);
        }
        written = fs_failed == 0;
        // Read once more, untimed, so a write that kept other bytes is not a time.
        const uint64_t wrong = written ? ReadBig(&fs, last) : 0u;
        written = written && wrong == 0u && fs_failed == 0;
        FsEnd(FS_WRITE, wrong, "bytes read back are not the ones written", FS_PACE_SAMPLES, NULL,
              "files", "the time of 4 MiB");
    }
    if (!written) {
        ReportSkip(FS, FS_READ, timing ? REPORT_REASON_DEPENDS : REPORT_REASON_ASKED,
                   timing ? "the write case left no file to read" : NOTIME);
    } else if (Begin(FS, FS_READ)) {
        fs_failed = 0;
        uint64_t wrong = 0;
        for (uint32_t s = 0; s < FS_PACE_SAMPLES; ++s) {
            fs_spent = 0;
            wrong += ReadBig(&fs, last);
            fs_samples[s] = armTicksToNs(fs_spent);
        }
        FsEnd(FS_READ, wrong, "bytes read are not the ones written", FS_PACE_SAMPLES, NULL,
              "readings", "the time of 4 MiB");
    }
    VolumeDeleteFile(&fs, FS_PACE_BIG);

    if (Begin(FS, FS_SMALL)) {
        fs_failed = 0;
        uint64_t wrong = 0;
        for (uint32_t s = 0; s < FS_PACE_SMALL_SAMPLES; ++s) {
            fs_spent = 0;
            wrong += SmallFiles(&fs, s);
            fs_samples[s] = armTicksToNs(fs_spent);
        }
        FsEnd(FS_SMALL, wrong, "files read back other bytes than were written",
              FS_PACE_SMALL_SAMPLES, HooksOn() ? "through nn::fs" : "", "rounds",
              "the time of the 64 files");
    }
    VolumeClose(&fs);
}

// ---- pace-gpu: quads over a target, an upload and a readback, through deko3d

#define GPU_SIDE 1024u
#define GPU_QUADS 200u
#define GPU_SAMPLES 8u
#define GPU_BYTES (GPU_SIDE * GPU_SIDE * 4u)
/// The side of the corner a sample reads back to end its work.
#define GPU_CORNER 16u

static const char *const GPU_OPENS = "graphics device opens with a 1024 by 1024 target";
static const char *const GPU_LOADS = "the flat shaders load";
static const char *const GPU_FILL = "200 quads cover a 1024 by 1024 target, each over the last";
static const char *const GPU_BLEND = "200 quads are added to a 1024 by 1024 target by the blender";
static const char *const GPU_UPLOAD = "a 1024 by 1024 texture is uploaded through the copy engine";
static const char *const GPU_READ =
    "the 1024 by 1024 target is copied to memory the processor reads";

// A sample of the first three cases is a piece of work, a corner of what it made copied to a
// buffer, GPU_CORNER pixels a side, and that corner read by the processor: a machine may put a
// list's work off until something reads its result, and a time that ended before any read would
// then be the time of recording the list. Timed by the counter, as the rest of the family, from
// the submit to the last pixel of the corner read. The work's own part, before anything was
// read, is kept beside it. The whole result is read back after the sample, untimed, for the
// check.
static uint64_t gpu_samples[GPU_SAMPLES];
static uint64_t gpu_alone[GPU_SAMPLES];

static GpuVertex GpuCorner(float x, float y, const float colour[4]) {
    const GpuVertex v = { x, y, 0.5f, 0.0f, 0.0f, colour[0], colour[1], colour[2], colour[3] };
    return v;
}

/// Quad k's colour when each covers the last: a colour of its own a quad.
static void QuadColour(uint32_t k, uint8_t out[4]) {
    out[0] = (uint8_t)(k * 2u + 1u);
    out[1] = (uint8_t)(255u - k);
    out[2] = (uint8_t)(k * 7u);
    out[3] = 255u;
}

/// GPU_QUADS quads over the cleared target in one draw, in order, submitted and waited for, then
/// a corner of the target copied out and read: the time of all of it goes in @p ns and of the
/// list alone in @p alone, and the corner's pixels that are not @p want are added to
/// @p corner_off. Blended, every quad adds one 255th to each channel. Hands back the whole
/// target, read after them, or NULL.
static const uint8_t *Quads(GpuContext *gpu, const DkShader *vs, const DkShader *fs, bool blended,
                            const uint8_t want[4], uint64_t *ns, uint64_t *alone,
                            uint64_t *corner_off) {
    static GpuVertex quads[GPU_QUADS * 6u];
    for (uint32_t k = 0; k < GPU_QUADS; ++k) {
        float c[4] = { 1.0f / 255.0f, 1.0f / 255.0f, 1.0f / 255.0f, 1.0f / 255.0f };
        if (!blended) {
            uint8_t bytes[4];
            QuadColour(k, bytes);
            for (uint32_t i = 0; i < 4u; ++i) {
                c[i] = (float)bytes[i] / 255.0f;
            }
        }
        const GpuVertex quad[6] = { GpuCorner(-1, 1, c), GpuCorner(1, 1, c),
                                    GpuCorner(1, -1, c), GpuCorner(-1, 1, c),
                                    GpuCorner(1, -1, c), GpuCorner(-1, -1, c) };
        memcpy(&quads[k * 6u], quad, sizeof(quad));
    }
    GpuClear(gpu, 0.0f, 0.0f, 0.0f, 0.0f);
    GpuBeginPass(gpu);
    if (blended) {
        DkColorState colour_state;
        dkColorStateDefaults(&colour_state);
        dkColorStateSetBlendEnable(&colour_state, 0, true);
        dkCmdBufBindColorState(gpu->cmdbuf, &colour_state);
        DkBlendState blend;
        dkBlendStateDefaults(&blend);
        dkBlendStateSetOps(&blend, DkBlendOp_Add, DkBlendOp_Add);
        dkBlendStateSetFactors(&blend, DkBlendFactor_One, DkBlendFactor_One, DkBlendFactor_One,
                               DkBlendFactor_One);
        dkCmdBufBindBlendState(gpu->cmdbuf, 0, &blend);
    }
    const bool drawn = GpuDrawTriangles(gpu, vs, fs, quads, GPU_QUADS * 6u);
    const uint64_t start = armGetSystemTick();
    GpuSubmit(gpu);
    *alone = Since(start);
    const uint8_t *const corner =
        drawn && gpu->ready ? GpuReadBackImage(gpu, &gpu->image, GPU_CORNER, GPU_CORNER) : NULL;
    uint32_t off = 0;
    for (uint32_t i = 0; corner != NULL && i < GPU_CORNER * GPU_CORNER; ++i) {
        off += memcmp(corner + (size_t)i * 4u, want, 4) != 0 ? 1u : 0u;
    }
    *ns = Since(start);
    *corner_off += off;
    return corner != NULL ? GpuReadBack(gpu) : NULL;
}

/// The line of a graphics case: @p ran when every list and readback happened; @p wrong_count of
/// @p wrong_what when what was read back whole was not what was drawn, and @p corner_off pixels
/// of the corners read inside the samples. The rate is @p unit of work over the middle sample,
/// in @p rate_in of them a second; @p alone_said names the part of a sample gpu_alone holds, or
/// is NULL when a sample has one part.
static void GpuEnd(GpuContext *gpu, const char *name, bool ran, uint64_t wrong_count,
                   const char *wrong_what, uint64_t corner_off, uint64_t unit, uint64_t rate_in,
                   const char *rate_word, const char *alone_said, const char *count_of,
                   const char *measure) {
    char wrong[240] = "";
    if (!ran) {
        snprintf(wrong, sizeof(wrong), "%.230s",
                 gpu->failure[0] != '\0' ? gpu->failure
                                         : "a list or a readback did not happen");
    } else if (wrong_count != 0u) {
        snprintf(wrong, sizeof(wrong), "%llu %s", (unsigned long long)wrong_count, wrong_what);
    } else if (corner_off != 0u) {
        snprintf(wrong, sizeof(wrong),
                 "%llu of the %u pixels of the corners read right after the work were not what "
                 "the whole readback then held",
                 (unsigned long long)corner_off,
                 (unsigned)(GPU_SAMPLES * GPU_CORNER * GPU_CORNER));
    }
    PaceStats stats;
    PaceStatsOf(gpu_samples, ran ? GPU_SAMPLES : 0u, &stats);
    char said[200];
    const unsigned long long rate = PaceRate(unit, stats.median) / rate_in;
    if (alone_said != NULL) {
        PaceStats alone;
        PaceStatsOf(gpu_alone, ran ? GPU_SAMPLES : 0u, &alone);
        snprintf(said, sizeof(said), "%llu %s a second; %s, the median %llu us", rate, rate_word,
                 alone_said, (unsigned long long)PaceMicroseconds(alone.median));
    } else {
        snprintf(said, sizeof(said), "%llu %s a second", rate, rate_word);
    }
    End(GPU, name, wrong, said, &stats, count_of, measure, 1u);
}

static void QuadsCase(GpuContext *gpu, const DkShader *vs, const DkShader *fs, bool blended) {
    const char *const name = blended ? GPU_BLEND : GPU_FILL;
    if (!Begin(GPU, name)) {
        return;
    }
    uint8_t want[4] = { GPU_QUADS, GPU_QUADS, GPU_QUADS, GPU_QUADS };
    if (!blended) {
        QuadColour(GPU_QUADS - 1u, want);
    }
    // A list before the samples takes the target's first use, as the fill suite's first quad.
    uint64_t first = 0;
    uint64_t first_alone = 0;
    uint64_t first_off = 0;
    bool ran = Quads(gpu, vs, fs, blended, want, &first, &first_alone, &first_off) != NULL;
    uint64_t wrong = 0;
    uint64_t corner_off = 0;
    for (uint32_t s = 0; s < GPU_SAMPLES && ran; ++s) {
        const uint8_t *const pixels =
            Quads(gpu, vs, fs, blended, want, &gpu_samples[s], &gpu_alone[s], &corner_off);
        ran = pixels != NULL;
        for (uint32_t i = 0; ran && i < GPU_SIDE * GPU_SIDE; ++i) {
            wrong += memcmp(pixels + (size_t)i * 4u, want, 4) != 0 ? 1u : 0u;
        }
    }
    GpuEnd(gpu, name, ran, wrong, "pixels are not what the last quad leaves", corner_off,
           (uint64_t)GPU_QUADS * GPU_SIDE * GPU_SIDE, 1000000u, "million pixels",
           "the list alone, before anything was read", "lists",
           "the time of the 200 quads and of a corner of them read");
}

static void UploadCase(GpuContext *gpu) {
    if (!Begin(GPU, GPU_UPLOAD)) {
        return;
    }
    uint32_t *const texels = malloc(GPU_BYTES);
    if (texels == NULL) {
        ReportCase(GPU, GPU_UPLOAD, CHECK_NONE, false, "no memory for the 4 MiB of texels");
        return;
    }
    for (uint32_t i = 0; i < GPU_SIDE * GPU_SIDE; ++i) {
        texels[i] = FsRefPatternWord(i);
    }
    DkImage texture;
    DkImageLayout layout;
    DkCopyBuf staged;
    bool ran =
        GpuCreateTexture(gpu, &texture, &layout, DkImageFormat_RGBA8_Unorm, GPU_SIDE, GPU_SIDE) &&
        GpuStagePixels(gpu, GPU_SIDE, GPU_SIDE, texels, &staged);
    // The first copy takes the texture's first use; the samples follow it.
    uint64_t wrong = 0;
    uint64_t corner_off = 0;
    for (uint32_t s = 0; s <= GPU_SAMPLES && ran; ++s) {
        dkCmdBufClear(gpu->cmdbuf);
        dkCmdBufBarrier(gpu->cmdbuf, DkBarrier_Full, 0);
        DkImageView view;
        dkImageViewDefaults(&view, &texture);
        const DkImageRect rect = { 0, 0, 0, GPU_SIDE, GPU_SIDE, 1 };
        dkCmdBufCopyBufferToImage(gpu->cmdbuf, &staged, &view, &rect, 0);
        const uint64_t start = armGetSystemTick();
        GpuSubmit(gpu);
        const uint64_t alone = Since(start);
        const uint8_t *const corner =
            gpu->ready ? GpuReadBackImage(gpu, &texture, GPU_CORNER, GPU_CORNER) : NULL;
        uint32_t off = 0;
        for (uint32_t y = 0; corner != NULL && y < GPU_CORNER; ++y) {
            off += memcmp(corner + (size_t)y * GPU_CORNER * 4u, &texels[y * GPU_SIDE],
                          GPU_CORNER * 4u) != 0
                       ? GPU_CORNER
                       : 0u;
        }
        if (s > 0u) {
            gpu_samples[s - 1u] = Since(start);
            gpu_alone[s - 1u] = alone;
            corner_off += off;
        }
        const uint8_t *const back =
            corner != NULL ? GpuReadBackImage(gpu, &texture, GPU_SIDE, GPU_SIDE) : NULL;
        ran = back != NULL;
        for (uint32_t i = 0; ran && i < GPU_SIDE * GPU_SIDE; ++i) {
            wrong += memcmp(back + (size_t)i * 4u, &texels[i], 4) != 0 ? 1u : 0u;
        }
    }
    GpuEnd(gpu, GPU_UPLOAD, ran, wrong, "texels read back are not the ones uploaded", corner_off,
           GPU_BYTES >> 20, 1u, "MiB", "the copy alone, before anything was read", "copies",
           "the time of 4 MiB and of a corner of them read");
    free(texels);
}

static void ReadCase(GpuContext *gpu) {
    if (!Begin(GPU, GPU_READ)) {
        return;
    }
    // 51, 102, 153 and 204: fifths of 255, which the clear suite holds a clear to.
    static const uint8_t WANT[4] = { 51u, 102u, 153u, 204u };
    GpuClear(gpu, 0.2f, 0.4f, 0.6f, 0.8f);
    // A copy before the samples, as the cases above have a list or a copy before theirs.
    bool ran = gpu->ready && GpuReadBack(gpu) != NULL;
    uint64_t wrong = 0;
    uint64_t touched_off = 0;
    for (uint32_t s = 0; s < GPU_SAMPLES && ran; ++s) {
        const uint64_t start = armGetSystemTick();
        const uint8_t *const pixels = GpuReadBack(gpu);
        // A byte of every page, so the copy is in memory the processor has read when the
        // sample ends: the first channel of a pixel each time, since a page is whole pixels.
        for (uint32_t at = 0; pixels != NULL && at < GPU_BYTES; at += 0x1000u) {
            touched_off += pixels[at] != WANT[0] ? 1u : 0u;
        }
        gpu_samples[s] = Since(start);
        ran = pixels != NULL;
        for (uint32_t i = 0; ran && i < GPU_SIDE * GPU_SIDE; ++i) {
            wrong += memcmp(pixels + (size_t)i * 4u, WANT, 4) != 0 ? 1u : 0u;
        }
    }
    GpuEnd(gpu, GPU_READ, ran, wrong + touched_off,
           "pixels read are not the colour the target was cleared to", 0u, GPU_BYTES >> 20, 1u,
           "MiB", NULL, "copies", "the time of 4 MiB and of a byte of every page of them read");
}

void RunPaceGpu(void) {
    static const char *const NAMES[6] = { GPU_OPENS, GPU_LOADS, GPU_FILL,
                                          GPU_BLEND, GPU_UPLOAD, GPU_READ };
    if (!timing) {
        for (uint32_t i = 0; i < 6u; ++i) {
            ReportSkip(GPU, NAMES[i], REPORT_REASON_ASKED, NOTIME);
        }
        return;
    }
    GpuSpec spec;
    GpuSpecDefaults(&spec, GPU_SIDE, GPU_SIDE);
    spec.code_size = 32u * 1024u;
    // The staged texels, and the vertices of eighteen lists of 200 quads.
    spec.data_size = 8u * 1024u * 1024u;
    spec.image_size = 8u * 1024u * 1024u;
    bool open = false;
    GpuContext gpu;
    DkShader vs;
    DkShader fs;
    if (ReportStart(GPU, GPU_OPENS)) {
        open = GpuOpenSpec(&gpu, &spec);
        CHECK_TRUE(GPU, GPU_OPENS, CHECK_NONE, open, "%s", gpu.failure);
    }
    bool loaded = false;
    if (!open) {
        ReportSkip(GPU, GPU_LOADS, REPORT_REASON_DEPENDS, "the graphics device did not open");
    } else if (ReportStart(GPU, GPU_LOADS)) {
        loaded = GpuLoadShader(&gpu, &vs, ShaderData(SHADER_COMMON_VERT),
                               ShaderSize(SHADER_COMMON_VERT)) &&
                 GpuLoadShader(&gpu, &fs, ShaderData(SHADER_FLAT_FRAG),
                               ShaderSize(SHADER_FLAT_FRAG));
        CHECK_TRUE(GPU, GPU_LOADS, CHECK_NONE, loaded, "%s", gpu.failure);
    }
    if (!loaded) {
        for (uint32_t i = 2; i < 6u; ++i) {
            ReportSkip(GPU, NAMES[i], REPORT_REASON_DEPENDS,
                       open ? "the shaders did not load" : "the graphics device did not open");
        }
    } else {
        QuadsCase(&gpu, &vs, &fs, false);
        QuadsCase(&gpu, &vs, &fs, true);
        UploadCase(&gpu);
        ReadCase(&gpu);
    }
    if (open) {
        GpuClose(&gpu);
    }
}
