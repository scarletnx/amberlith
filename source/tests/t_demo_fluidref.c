// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../check.h"
#include "../demo_fluidref.h"
#include "../report.h"
#include "tests.h"

// The fluid demo's script, passes and judge against answers worked out by hand, so a frame the
// demo calls wrong is the machine's and not the reference's. Small grids, a few frames: the
// reference suite runs in every run.

static const char *const SUITE = "reference";

/// The first reading that disagreed, for the line.
typedef struct {
    uint32_t wrong;
    const char *first;
} Tally;

static void Expect(Tally *tally, bool condition, const char *what) {
    if (!condition && tally->wrong++ == 0u) {
        tally->first = what;
    }
}

static void Report(const char *name, const Tally *tally) {
    CHECK_TRUE(SUITE, name, CHECK_NONE, tally->wrong == 0u, "%u readings wrong, the first: %s",
               (unsigned)tally->wrong, tally->first != NULL ? tally->first : "none");
}

/// Within a millionth of a value worked out by hand to more digits than a float holds.
static bool Near(float got, double want) {
    return fabs((double)got - want) <= 1e-6 * (fabs(want) > 1.0 ? fabs(want) : 1.0);
}

static bool Splat(const FluidSplat *s, double x, double y, double inv_r2, double v0, double v1,
                  double v2) {
    return Near(s->x, x) && Near(s->y, y) && Near(s->inv_r2, inv_r2) && Near(s->value[0], v0) &&
           Near(s->value[1], v1) && Near(s->value[2], v2) && s->value[3] == 0.0f;
}

static void CheckScript(void) {
    Tally t = { 0, NULL };
    const FluidGrid grid = { FLUID_W, FLUID_H };
    FluidSplat force[FLUID_SPLATS_MAX];
    FluidSplat dye[FLUID_SPLATS_MAX];
    // Frame 0. The first jet at 1/8 across and 6/8 down, (127 / 8, 426 / 8), pointing up and to
    // the right, turned by the half tangent -1/2 its sweep starts at: cosine 0.6, sine -0.8, so
    // (0.6 r + 0.8 r, ...) with r = 1/sqrt(2) gives (-0.141421356, -0.989949494), times 0.35.
    Expect(&t, FluidScript(&grid, 0, force, dye) == 3u, "frame 0 has its three jets");
    Expect(&t, Splat(&force[0], 15.875, 53.25, 0.0625, -0.0494974747, -0.346482323, 0.0),
           "frame 0's first jet");
    // Its dye: texel 2x + 1/2 of the same place, a radius twice the texels, the first colour.
    Expect(&t, Splat(&dye[0], 32.25, 107.0, 0.015625, 0.12, 0.0456, 0.012),
           "frame 0's first jet's dye");
    // The second at 7/8 across, half way through its sweep, so not turned at all.
    Expect(&t, Splat(&force[1], 111.125, 53.25, 0.0625, -0.247487373, -0.247487373, 0.0),
           "frame 0's second jet");
    Expect(&t, Splat(&dye[1], 222.75, 107.0, 0.015625, 0.114, 0.0192, 0.0744),
           "frame 0's second jet's dye, the third colour");
    // The third at the middle across and 1/8 down, pointing down, 30 frames into a sweep of 110:
    // the half tangent (30 - 55) / 110 = -5/22, cosine 0.901768173 and sine -0.432219854.
    Expect(&t, Splat(&force[2], 63.5, 8.875, 0.0625, 0.151276949, 0.315618860, 0.0),
           "frame 0's third jet");
    Expect(&t, Splat(&dye[2], 127.5, 18.25, 0.015625, 0.12, 0.0984, 0.0192),
           "frame 0's third jet's dye, the fifth colour");
    // Frame 60: the first burst, at (3/8, 3/8), pointing a right angle left of up.
    Expect(&t, FluidScript(&grid, 60, force, dye) == 4u, "frame 60 adds a burst");
    Expect(&t, Splat(&force[3], 47.625, 26.625, 1.0 / 36.0, -0.9, 0.0, 0.0), "frame 60's burst");
    Expect(&t, Splat(&dye[3], 95.75, 53.75, 1.0 / 144.0, 0.0264, 0.12, 0.0576),
           "frame 60's burst's dye, the fourth colour");
    Expect(&t, FluidScript(&grid, 71, force, dye) == 4u, "frame 71 still bursts");
    Expect(&t, FluidScript(&grid, 72, force, dye) == 3u, "frame 72 does not");
    // Frame 180, the second burst: half tangent 3/4, cosine 0.28 and sine 0.96, off up.
    Expect(&t, FluidScript(&grid, 180, force, dye) == 4u, "frame 180 bursts again");
    Expect(&t, Splat(&force[3], 79.375, 44.375, 1.0 / 36.0, 0.864, -0.252, 0.0),
           "frame 180's burst");
    // Frame 240: the first jet takes the second colour.
    (void)FluidScript(&grid, 240, force, dye);
    Expect(&t, Near(dye[0].value[0], 0.0144) && Near(dye[0].value[1], 0.066) &&
                   Near(dye[0].value[2], 0.12),
           "frame 240's first jet's colour");
    Report("fluid: the script's jets and bursts at frames 0, 60, 71, 72, 180 and 240, worked out "
           "by hand",
           &t);
}

static void CheckPasses(void) {
    Tally t = { 0, NULL };
    // The divergence on 3 by 2, a wall's neighbour the cell's own velocity reversed.
    const FluidGrid small = { 3u, 2u };
    const float velocity[12] = { 1, 0, 2, 0, 4, 0, 1, 1, 2, 3, 4, 5 };
    float divergence[6];
    FluidDivergencePass(&small, velocity, divergence);
    const float want_divergence[6] = { 2.0f, 3.0f, -0.5f, 1.0f, 0.0f, -5.5f };
    Expect(&t, memcmp(divergence, want_divergence, sizeof(divergence)) == 0,
           "the divergence on 3 by 2");
    // One Jacobi pass, its neighbours' sum halved, a wall's the cell's own.
    const float pressure[6] = { 1, 2, 3, 4, 5, 6 };
    const float given[6] = { 0.5f, 0.0f, -1.0f, 2.0f, 1.0f, 0.0f };
    float solved[6];
    for (uint32_t a = 0; a < 2u; ++a) {
        FluidJacobiPass(a == 0u ? FLUID_SEPARATE : FLUID_FUSED, &small, pressure, given, 0.5f,
                        solved);
        const float want_solved[6] = { 0.875f, 1.375f, 2.0f, 1.25f, 1.875f, 2.5f };
        Expect(&t, memcmp(solved, want_solved, sizeof(solved)) == 0,
               a == 0u ? "a Jacobi pass" : "a Jacobi pass, fused");
    }
    // The gradient of the same pressure off a still velocity of (1, -1).
    const float still[12] = { 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1 };
    float projected[12];
    FluidGradientPass(FLUID_SEPARATE, &small, still, pressure, projected);
    const float want_projected[12] = { 0.5f, -2.5f, 0.0f, -2.5f, 0.5f, -2.5f,
                                       0.5f, -2.5f, 0.0f, -2.5f, 0.5f, -2.5f };
    Expect(&t, memcmp(projected, want_projected, sizeof(projected)) == 0, "the gradient");
    // Two splats on 5 by 1: (1, 2) at texel 2, radius 2, is (3/4)^3 of itself a texel away and
    // nothing two away; (1, 0) at texel 0, radius 1, reaches no other texel.
    const FluidSplat splats[2] = { { 2.0f, 0.0f, 0.25f, 0.0f, { 1.0f, 2.0f, 0.0f, 0.0f } },
                                   { 0.0f, 0.0f, 1.0f, 0.0f, { 1.0f, 0.0f, 0.0f, 0.0f } } };
    const float zeros[10] = { 0 };
    float splatted[10];
    FluidSplatPass(FLUID_SEPARATE, 5u, 1u, 2u, splats, 2u, zeros, splatted);
    const float want_splatted[10] = { 1.0f, 0.0f, 0.421875f, 0.84375f, 1.0f,
                                      2.0f, 0.421875f, 0.84375f, 0.0f, 0.0f };
    Expect(&t, memcmp(splatted, want_splatted, sizeof(splatted)) == 0, "two splats");
    // Advection on 4 by 1 along a still (1/2, 0): each texel takes the field half a texel back,
    // the first clamped to its own; the field (i, 10 i) makes (i - 1/2, 10 i - 5).
    const FluidGrid row = { 4u, 1u };
    const float half[8] = { 0.5f, 0, 0.5f, 0, 0.5f, 0, 0.5f, 0 };
    const float field[8] = { 0, 0, 1, 10, 2, 20, 3, 30 };
    float advected[8];
    FluidAdvectPass(FLUID_SEPARATE, &row, half, 1u, 2u, field, 1.0f, advected);
    const float want_advected[8] = { 0, 0, 0.5f, 5, 1.5f, 15, 2.5f, 25 };
    Expect(&t, memcmp(advected, want_advected, sizeof(advected)) == 0, "an advection");
    // The dye's, at twice the grid along a still (1/4, 0) on 2 by 1: dye texel I goes back
    // 2 * 1/4 of a texel, the velocity sampled at I / 2 - 1/4 and kept; the dye (I, I, I, 0),
    // halved by the keep, gives (I - 1/2) / 2, the first clamped.
    const FluidGrid pair = { 2u, 1u };
    const float quarter[4] = { 0.25f, 0, 0.25f, 0 };
    float dye[32];
    for (uint32_t j = 0; j < 2u; ++j) {
        for (uint32_t i = 0; i < 4u; ++i) {
            float *const texel = dye + (j * 4u + i) * 4u;
            texel[0] = texel[1] = texel[2] = (float)i;
            texel[3] = 0.0f;
        }
    }
    float moved[32];
    FluidAdvectPass(FLUID_SEPARATE, &pair, quarter, 2u, 4u, dye, 0.5f, moved);
    bool dye_right = true;
    for (uint32_t j = 0; j < 2u; ++j) {
        for (uint32_t i = 0; i < 4u; ++i) {
            const float want = i == 0u ? 0.0f : ((float)i - 0.5f) * 0.5f;
            const float *const texel = moved + (j * 4u + i) * 4u;
            dye_right = dye_right && texel[0] == want && texel[1] == want && texel[2] == want &&
                        texel[3] == 0.0f;
        }
    }
    Expect(&t, dye_right, "the dye's advection at twice the grid");
    Report("fluid: the splat, divergence, Jacobi, gradient and advection passes on fields made to "
           "order, worked out by hand",
           &t);
}

/// A simulation's fields, for the cases that run one.
typedef struct {
    float *velocity;
    float *pressure;
    float *dye;
    FluidWork work;
} Sim;

static bool MakeSim(Sim *sim, const FluidGrid *grid) {
    memset(sim, 0, sizeof(*sim));
    const size_t v = FluidVelocityFloats(grid);
    const size_t p = FluidPressureFloats(grid);
    const size_t d = FluidDyeFloats(grid);
    float **const slots[] = { &sim->velocity,          &sim->pressure,
                              &sim->dye,               &sim->work.f.splatted,
                              &sim->work.f.divergence, &sim->work.f.pressure,
                              &sim->work.f.projected,  &sim->work.f.velocity,
                              &sim->work.f.dyed,       &sim->work.f.dye,
                              &sim->work.scratch };
    const size_t sizes[] = { v, p, d, v, p, p, v, v, d, d, p };
    bool made = true;
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        *slots[i] = (float *)calloc(sizes[i], sizeof(float));
        made = made && *slots[i] != NULL;
    }
    return made;
}

static void FreeSim(Sim *sim) {
    float *const all[] = { sim->velocity,          sim->pressure,
                           sim->dye,               sim->work.f.splatted,
                           sim->work.f.divergence, sim->work.f.pressure,
                           sim->work.f.projected,  sim->work.f.velocity,
                           sim->work.f.dyed,       sim->work.f.dye,
                           sim->work.scratch };
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); ++i) {
        free(all[i]);
    }
}

/// Runs @p sim from where it is through frames @p from to @p to - 1.
static void Simulate(Sim *sim, const FluidGrid *grid, FluidArith arith, uint32_t from,
                     uint32_t to) {
    for (uint32_t k = from; k < to; ++k) {
        FluidStep(arith, grid, k, sim->velocity, sim->pressure, sim->dye, &sim->work.f,
                  sim->work.scratch);
        memcpy(sim->velocity, sim->work.f.velocity, FluidVelocityFloats(grid) * sizeof(float));
        memcpy(sim->pressure, sim->work.f.pressure, FluidPressureFloats(grid) * sizeof(float));
        memcpy(sim->dye, sim->work.f.dye, FluidDyeFloats(grid) * sizeof(float));
    }
}

/// The hashes of a 16 by 9 run of 64 frames, the first burst in it, as the host build worked them
/// out: a processor or compiler that fused a multiply into its add, or rounded otherwise, gives
/// others, and its judge would blame the graphics unit for its own rounding.
#define PINNED_VELOCITY 0x8ea5c8dfu
#define PINNED_PRESSURE 0x68226d1eu
#define PINNED_DYE 0x8b0c0642u

static void CheckSameEverywhere(void) {
    Tally t = { 0, NULL };
    const FluidGrid grid = { 16u, 9u };
    Sim sim;
    Sim fused;
    if (!MakeSim(&sim, &grid) || !MakeSim(&fused, &grid)) {
        Expect(&t, false, "no memory for two 16 by 9 simulations");
    } else {
        Simulate(&sim, &grid, FLUID_SEPARATE, 0u, 64u);
        Simulate(&fused, &grid, FLUID_FUSED, 0u, 64u);
        const uint32_t velocity = FluidHashFloats(sim.velocity, FluidVelocityFloats(&grid));
        const uint32_t pressure = FluidHashFloats(sim.pressure, FluidPressureFloats(&grid));
        const uint32_t dye = FluidHashFloats(sim.dye, FluidDyeFloats(&grid));
        Expect(&t, velocity == PINNED_VELOCITY, "the velocity's hash");
        Expect(&t, pressure == PINNED_PRESSURE, "the pressure's hash");
        Expect(&t, dye == PINNED_DYE, "the dye's hash");
        // The fused rounding is another, and within the early frames' budget of it.
        Expect(&t, FluidHashFloats(fused.dye, FluidDyeFloats(&grid)) != dye,
               "the fused rounding answers otherwise");
        FluidDiff diff;
        FluidCompare(fused.dye, sim.dye, FluidDyeFloats(&grid), FLUID_EARLY_ABSOLUTE, 0.0f,
                     &diff);
        Expect(&t, diff.off == 0u && diff.farthest > 0.0f, "the fused rounding's dye is near");
        if (t.wrong > 0u) {
            ReportNote("fluid: the 16 by 9 run's hashes here: velocity 0x%08x, pressure 0x%08x, "
                       "dye 0x%08x",
                       (unsigned)velocity, (unsigned)pressure, (unsigned)dye);
        }
    }
    FreeSim(&sim);
    FreeSim(&fused);
    Report("fluid: a 16 by 9 run of 64 frames gives the pinned bits, and the fused rounding others "
           "within the early budget",
           &t);
}

static bool Frames(uint32_t frames, const uint32_t *want, uint32_t count) {
    uint32_t got[FLUID_JUDGED_MAX];
    const uint32_t n = FluidJudgedFrames(frames, got);
    return n == count && memcmp(got, want, count * sizeof(uint32_t)) == 0;
}

static void CheckJudgedFrames(void) {
    Tally t = { 0, NULL };
    static const uint32_t ONE[] = { 0 };
    static const uint32_t TWO[] = { 0, 1 };
    static const uint32_t SIXTY_FOUR[] = { 0, 1, 3, 7, 15, 31, 63 };
    static const uint32_t HUNDRED[] = { 0, 1, 3, 7, 15, 31, 63, 74, 99 };
    static const uint32_t DEFAULT[] = { 0, 1, 3, 7, 15, 31, 63, 149, 299, 449, 599 };
    static const uint32_t MOST[] = { 0, 1, 3, 7, 15, 31, 63, 8999, 17999, 26999, 35999 };
    Expect(&t, Frames(1u, ONE, 1u), "a run of one frame");
    Expect(&t, Frames(2u, TWO, 2u), "a run of two");
    Expect(&t, Frames(64u, SIXTY_FOUR, 7u), "a run of 64, its quarters among the early ones");
    Expect(&t, Frames(100u, HUNDRED, 9u), "a run of 100");
    Expect(&t, Frames(600u, DEFAULT, 11u), "the default run of 600");
    Expect(&t, Frames(36000u, MOST, 11u), "the longest run");
    Report("fluid: the frames a run of 1, 2, 64, 100, 600 and 36000 frames judges", &t);
}

static void CheckCompare(void) {
    Tally t = { 0, NULL };
    // At the budget passes, past it fails; the relative part scales with the value expected.
    const float want[6] = { 1.0f, -2.0f, 0.0f, 3.0f, 0.0f, 4.0f };
    const float got[6] = { 1.25f, -2.75f, 0.5f, 3.0f, NAN, INFINITY };
    FluidDiff diff;
    FluidCompare(got, want, 4u, 0.5f, 0.125f, &diff);
    Expect(&t, diff.off == 0u, "within 1/2 and 1/8 of the value");
    FluidCompare(got, want, 4u, 0.25f, 0.0f, &diff);
    Expect(&t, diff.off == 2u && diff.first == 1u && diff.first_got == -2.75f &&
                   diff.first_want == -2.0f && diff.farthest == 0.75f && diff.farthest_at == 1u,
           "past 1/4, two off, the first and the farthest the second");
    // A NaN or an infinity is off whatever the budget, and the farthest a float can be.
    FluidCompare(got + 4, want + 4, 2u, 1e30f, 1.0f, &diff);
    Expect(&t, diff.off == 2u && diff.first == 0u && isinf(diff.farthest),
           "a NaN and an infinity");
    // The sanity: a NaN, an infinity, a negative dye; minus nought is not below nought.
    const float values[5] = { 0.5f, -0.0f, -1e-30f, NAN, -INFINITY };
    FluidSane sane;
    FluidSanity(values, 5u, true, &sane);
    Expect(&t, sane.not_finite == 2u && sane.not_finite_at == 3u && sane.negative == 1u &&
                   sane.negative_at == 2u,
           "the dye's sanity");
    FluidSanity(values, 5u, false, &sane);
    Expect(&t, sane.not_finite == 2u && sane.negative == 0u, "a velocity may be negative");
    Report("fluid: the comparison's budgets, a NaN and an infinity, and the sanity of fields made "
           "to order",
           &t);
}

// Machines made to order, each wrong in one way: the judge fails exactly the checks the way
// reaches, every pass after the wrong one worked out from what the wrong one left, as a machine's
// would be.
typedef enum {
    MACHINE_RIGHT,
    MACHINE_FUSED,
    MACHINE_NO_FORCE,
    MACHINE_NO_DYE_SPLAT,
    MACHINE_HALF_SOLVE,
    MACHINE_NO_GRADIENT,
    MACHINE_STALE_VELOCITY,
    MACHINE_DYE_KEPT_WHOLE,
    MACHINE_NAN_PRESSURE,
    MACHINE_NEGATIVE_DYE,
    MACHINE_NUDGED_VELOCITY,
    MACHINES,
} Machine;

static const struct {
    const char *what;
    uint32_t fails;
} MACHINE_CASES[MACHINES] = {
    { "the processor's own rounding", 0u },
    { "every multiply-add fused", 0u },
    { "no force splats", 1u << FLUID_CHECK_FORCE },
    { "no dye splats", 1u << FLUID_CHECK_DYE_SPLAT },
    { "half the Jacobi passes", 1u << FLUID_CHECK_SOLVE },
    { "no gradient taken off", (1u << FLUID_CHECK_GRADIENT) | (1u << FLUID_CHECK_PROJECTION) },
    { "the velocity advected from the target before the projection",
      1u << FLUID_CHECK_ADVECT_VELOCITY },
    { "the dye kept whole", 1u << FLUID_CHECK_ADVECT_DYE },
    { "a pressure texel NaN",
      (1u << FLUID_CHECK_SOLVE) | (1u << FLUID_CHECK_GRADIENT) |
          (1u << FLUID_CHECK_ADVECT_VELOCITY) | (1u << FLUID_CHECK_PROJECTION) |
          (1u << FLUID_CHECK_SANE) },
    { "a dye texel below nought", (1u << FLUID_CHECK_ADVECT_DYE) | (1u << FLUID_CHECK_SANE) },
    { "a velocity texel 2^-11 off", 1u << FLUID_CHECK_ADVECT_VELOCITY },
};

/// Machine @p m's frame @p frame from the fields @p in, its passes into @p out.
static void RunMachine(Machine m, const FluidGrid *grid, uint32_t frame, const Sim *in,
                       FluidFields *out, float *scratch) {
    const FluidArith arith = m == MACHINE_FUSED ? FLUID_FUSED : FLUID_SEPARATE;
    const size_t v = FluidVelocityFloats(grid);
    const size_t d = FluidDyeFloats(grid);
    FluidSplat force[FLUID_SPLATS_MAX];
    FluidSplat colour[FLUID_SPLATS_MAX];
    const uint32_t count = FluidScript(grid, frame, force, colour);
    if (m == MACHINE_NO_FORCE) {
        memcpy(out->splatted, in->velocity, v * sizeof(float));
    } else {
        FluidSplatPass(arith, grid->w, grid->h, FLUID_VELOCITY_CHANNELS, force, count,
                       in->velocity, out->splatted);
    }
    if (m == MACHINE_NO_DYE_SPLAT) {
        memcpy(out->dyed, in->dye, d * sizeof(float));
    } else {
        FluidSplatPass(arith, FluidDyeW(grid), FluidDyeH(grid), FLUID_DYE_CHANNELS, colour, count,
                       in->dye, out->dyed);
    }
    FluidDivergencePass(grid, out->splatted, out->divergence);
    FluidSolve(arith, grid, in->pressure, out->divergence,
               m == MACHINE_HALF_SOLVE ? FLUID_JACOBI / 2u : FLUID_JACOBI, out->pressure, scratch);
    if (m == MACHINE_NAN_PRESSURE) {
        out->pressure[grid->w + 2u] = NAN;
    }
    if (m == MACHINE_NO_GRADIENT) {
        memcpy(out->projected, out->splatted, v * sizeof(float));
    } else {
        FluidGradientPass(arith, grid, out->splatted, out->pressure, out->projected);
    }
    const float *const along = m == MACHINE_STALE_VELOCITY ? out->splatted : out->projected;
    FluidAdvectPass(arith, grid, out->projected, 1u, FLUID_VELOCITY_CHANNELS, along,
                    FLUID_VELOCITY_KEEP, out->velocity);
    FluidAdvectPass(arith, grid, out->projected, FLUID_DYE_SCALE, FLUID_DYE_CHANNELS, out->dyed,
                    m == MACHINE_DYE_KEPT_WHOLE ? 1.0f : FLUID_DYE_KEEP, out->dye);
    if (m == MACHINE_NEGATIVE_DYE) {
        out->dye[FLUID_DYE_CHANNELS * 7u] = -0.001f;
    }
    if (m == MACHINE_NUDGED_VELOCITY) {
        out->velocity[2u * (grid->w + 3u)] += 0x1p-11f;
    }
}

static void CheckMachines(void) {
    // Frame 20 of a 32 by 18 run: every jet has blown for twenty frames, and the projection and
    // the mass have something to hold.
    const FluidGrid grid = { 32u, 18u };
    const uint32_t frame = 20u;
    Sim start;
    Sim machine;
    Sim judge;
    uint32_t wrong = 0;
    char first[200] = "none";
    if (!MakeSim(&start, &grid) || !MakeSim(&machine, &grid) || !MakeSim(&judge, &grid)) {
        wrong = 1u;
        snprintf(first, sizeof(first), "no memory for three 32 by 18 simulations");
    } else {
        Simulate(&start, &grid, FLUID_SEPARATE, 0u, frame);
        double budget = 0.0;
        FluidDyeBudgets(&grid, &frame, 1u, &budget);
        for (uint32_t m = 0; m < MACHINES; ++m) {
            RunMachine((Machine)m, &grid, frame, &start, &machine.work.f, machine.work.scratch);
            FluidFrame seen;
            seen.frame = frame;
            seen.read[FLUID_READ_VELOCITY_IN] = start.velocity;
            seen.read[FLUID_READ_PRESSURE_IN] = start.pressure;
            seen.read[FLUID_READ_DYE_IN] = start.dye;
            seen.read[FLUID_READ_SPLATTED] = machine.work.f.splatted;
            seen.read[FLUID_READ_DYED] = machine.work.f.dyed;
            seen.read[FLUID_READ_DIVERGENCE] = machine.work.f.divergence;
            seen.read[FLUID_READ_PRESSURE] = machine.work.f.pressure;
            seen.read[FLUID_READ_PROJECTED] = machine.work.f.projected;
            seen.read[FLUID_READ_VELOCITY] = machine.work.f.velocity;
            seen.read[FLUID_READ_DYE] = machine.work.f.dye;
            FluidVerdict verdicts[FLUID_CHECKS];
            FluidJudgeFrame(&grid, &seen, budget, &judge.work, verdicts);
            uint32_t failed = 0;
            for (uint32_t c = 0; c < FLUID_CHECKS; ++c) {
                failed |= verdicts[c].passed ? 0u : 1u << c;
            }
            if (failed != MACHINE_CASES[m].fails && wrong++ == 0u) {
                snprintf(first, sizeof(first),
                         "%s: the checks failed are 0x%03x where 0x%03x; the projection left "
                         "%.4g, the mass %.4g off",
                         MACHINE_CASES[m].what, (unsigned)failed,
                         (unsigned)MACHINE_CASES[m].fails,
                         verdicts[FLUID_CHECK_PROJECTION].measure,
                         verdicts[FLUID_CHECK_MASS].measure);
            }
        }
    }
    FreeSim(&start);
    FreeSim(&machine);
    FreeSim(&judge);
    CHECK_TRUE(SUITE,
               "fluid: the judge on eleven machines made to order, each wrong in one way, fails "
               "the checks that way reaches and no other",
               CHECK_NONE, wrong == 0u, "%u machines judged otherwise, the first: %s",
               (unsigned)wrong, first);
}

static void CheckNotes(void) {
    Tally t = { 0, NULL };
    static GameTimes times;
    memset(&times, 0, sizeof(times));
    times.frames = 3u;
    times.warmup_present = 20000u;
    times.warmup_main = 300u;
    times.warmup_render = 5000u;
    times.present.frames = 2u;
    times.present.total = 50000u;
    times.present.mean = 25000u;
    times.present.p99 = 40000u;
    times.present.longest = 40000u;
    times.present.longest_at = 2u;
    times.present.hitches = 1u;
    times.present.hitch_at[0] = 2u;
    times.main_part.mean = 10u;
    times.main_part.p99 = 12u;
    times.main_part.longest = 12u;
    times.main_part.longest_at = 1u;
    times.render_part.mean = 2000u;
    times.render_part.p99 = 2500u;
    times.render_part.longest = 2500u;
    times.render_part.longest_at = 2u;
    char note[512];
    FluidDescribeFrames(note, sizeof(note), &times);
    Expect(&t, strcmp(note, "fluid: 3 frames in 0.07 s; frame 0, the warm-up, left out of the "
                            "times (20000 us present to present, 300 us the main thread's part, "
                            "5000 us the render thread's); the 2 past it; present to present: "
                            "mean 25000 us, 99th percentile 40000 us, longest 40000 us at frame "
                            "2, 1 over 33333 us, two frames at 60 Hz: frame 2") == 0,
           "the frames' note");
    FluidDescribeParts(note, sizeof(note), &times);
    Expect(&t, strcmp(note, "fluid: past the warm-up; the main thread's part: mean 10 us, 99th "
                            "percentile 12 us, longest 12 us at frame 1; the render thread's "
                            "part, the graphics unit's in it: mean 2000 us, 99th percentile 2500 "
                            "us, longest 2500 us at frame 2") == 0,
           "the parts' note");
    times.present.frames = 0u;
    FluidDescribeFrames(note, sizeof(note), &times);
    Expect(&t, strstr(note, "; no frame past it to time") != NULL, "a run of one frame");
    char cut[24];
    Expect(&t, FluidDescribeFrames(cut, sizeof(cut), &times) == strlen(cut) && strlen(cut) < 24u,
           "a note cut short, terminated");
    Report("fluid: the frames' and the parts' notes on times made to order", &t);
}

void RunReferenceFluid(void) {
    CheckScript();
    CheckPasses();
    CheckSameEverywhere();
    CheckJudgedFrames();
    CheckCompare();
    CheckMachines();
    CheckNotes();
}
