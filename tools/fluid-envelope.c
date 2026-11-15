// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Where the fluid demo's budgets come from (source/demo_fluidref.h), measured on the processor:
//
//   fluid-envelope EARLY FRAMES
//
// The whole simulation from frame 0 run twice for EARLY frames, every multiply and the add after
// it rounded apart and fused, as a graphics compiler may fuse them: the farthest the two part by,
// which the early frames' budget has to hold a hundred times over. Then one simulation for FRAMES
// frames, each frame's passes worked out again fused from its own inputs: the farthest a pass
// parts by, which the pass budget has to hold sixteen times over; the most divergence the
// projection leaves and the farthest the dye's mass gets from its budget, which their budgets have
// to hold. Prints what it measured; exits 1 when a budget does not hold it with its margin, 2 for
// arguments it refuses. host.mk's host-run runs `fluid-envelope 64 600`; the budgets' run over
// MODE_FRAMES_MAX frames is `fluid-envelope 64 36000`, minutes.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "demo_fluidref.h"

#define EARLY_MARGIN 100.0f
#define PASS_MARGIN 16.0f

typedef struct {
    float *velocity;
    float *pressure;
    float *dye;
    FluidWork work;
} Sim;

static float *Floats(size_t count) {
    float *const out = (float *)calloc(count, sizeof(float));
    if (out == NULL) {
        fprintf(stderr, "fluid-envelope: no memory for %zu floats\n", count);
        exit(2);
    }
    return out;
}

static void Make(Sim *sim, const FluidGrid *grid) {
    const size_t v = FluidVelocityFloats(grid);
    const size_t p = FluidPressureFloats(grid);
    const size_t d = FluidDyeFloats(grid);
    sim->velocity = Floats(v);
    sim->pressure = Floats(p);
    sim->dye = Floats(d);
    sim->work.f.splatted = Floats(v);
    sim->work.f.divergence = Floats(p);
    sim->work.f.pressure = Floats(p);
    sim->work.f.projected = Floats(v);
    sim->work.f.velocity = Floats(v);
    sim->work.f.dyed = Floats(d);
    sim->work.f.dye = Floats(d);
    sim->work.scratch = Floats(p);
}

static void Step(Sim *sim, const FluidGrid *grid, FluidArith arith, uint32_t frame) {
    FluidStep(arith, grid, frame, sim->velocity, sim->pressure, sim->dye, &sim->work.f,
              sim->work.scratch);
    memcpy(sim->velocity, sim->work.f.velocity, FluidVelocityFloats(grid) * sizeof(float));
    memcpy(sim->pressure, sim->work.f.pressure, FluidPressureFloats(grid) * sizeof(float));
    memcpy(sim->dye, sim->work.f.dye, FluidDyeFloats(grid) * sizeof(float));
}

static float Farthest(const float *a, const float *b, size_t count) {
    FluidDiff diff;
    FluidCompare(a, b, count, 0.0f, 0.0f, &diff);
    return diff.farthest;
}

static uint32_t Whole(const char *text, const char *what) {
    char *end = NULL;
    const unsigned long value = strtoul(text, &end, 10);
    if (end == text || *end != '\0' || value < 1u || value > 36000u) {
        fprintf(stderr, "fluid-envelope: %s %s is not a whole number of frames from 1 to 36000\n",
                what, text);
        exit(2);
    }
    return (uint32_t)value;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: fluid-envelope EARLY FRAMES (host.mk runs 64 600)\n");
        return 2;
    }
    const uint32_t early = Whole(argv[1], "EARLY");
    const uint32_t frames = Whole(argv[2], "FRAMES");
    const FluidGrid grid = { FLUID_W, FLUID_H };
    bool held = true;

    // The two roundings apart, frame by frame.
    Sim apart;
    Sim fused;
    Make(&apart, &grid);
    Make(&fused, &grid);
    float farthest = 0.0f;
    uint32_t farthest_at = 0;
    for (uint32_t k = 0; k < early; ++k) {
        Step(&apart, &grid, FLUID_SEPARATE, k);
        Step(&fused, &grid, FLUID_FUSED, k);
        const float dye = Farthest(fused.dye, apart.dye, FluidDyeFloats(&grid));
        const float velocity = Farthest(fused.velocity, apart.velocity, FluidVelocityFloats(&grid));
        const float most = dye > velocity ? dye : velocity;
        if (most > farthest) {
            farthest = most;
            farthest_at = k;
        }
        if (((k + 1u) & k) == 0u || k + 1u == early) {
            printf("frames 0 to %u: the two roundings part by %.3g at most, %.3g in frame %u\n",
                   (unsigned)k, (double)farthest, (double)most, (unsigned)k);
        }
    }
    const bool early_held = farthest * EARLY_MARGIN <= FLUID_EARLY_ABSOLUTE;
    printf("early: %.3g at most, in frame %u; the budget %.3g is %.0f times it, %s %.0f\n",
           (double)farthest, (unsigned)farthest_at, (double)FLUID_EARLY_ABSOLUTE,
           farthest > 0.0f ? (double)(FLUID_EARLY_ABSOLUTE / farthest) : INFINITY,
           early_held ? "at least" : "short of", (double)EARLY_MARGIN);
    held = held && early_held;

    // One simulation, each frame held to the judge as a machine would be, and its passes
    // worked out again fused.
    Sim one;
    Sim again;
    Make(&one, &grid);
    Make(&again, &grid);
    float pass_far[FLUID_CHECKS] = { 0 };
    uint32_t pass_at[FLUID_CHECKS] = { 0 };
    double ratio_worst = 0.0;
    uint32_t ratio_at = 0;
    double mass_worst = 0.0;
    uint32_t mass_at = 0;
    uint32_t judge_failures = 0;
    uint32_t *const every = (uint32_t *)calloc(frames, sizeof(uint32_t));
    double *const budgets = (double *)calloc(frames, sizeof(double));
    if (every == NULL || budgets == NULL) {
        fprintf(stderr, "fluid-envelope: no memory for %u frames' budgets\n", (unsigned)frames);
        return 2;
    }
    for (uint32_t k = 0; k < frames; ++k) {
        every[k] = k;
    }
    FluidDyeBudgets(&grid, every, frames, budgets);
    const time_t began = time(NULL);
    for (uint32_t k = 0; k < frames; ++k) {
        // A long run says where it is every 2000 frames, on standard error.
        if (k > 0u && k % 2000u == 0u) {
            const double spent = difftime(time(NULL), began);
            fprintf(stderr, "fluid-envelope: frame %u of %u, %.0f%%, %.0f s so far, %.0f s left\n",
                    (unsigned)k, (unsigned)frames, 100.0 * k / frames, spent,
                    spent * (frames - k) / k);
        }
        // The fields the frame starts from, kept in again's: the step moves one's on.
        memcpy(again.velocity, one.velocity, FluidVelocityFloats(&grid) * sizeof(float));
        memcpy(again.pressure, one.pressure, FluidPressureFloats(&grid) * sizeof(float));
        memcpy(again.dye, one.dye, FluidDyeFloats(&grid) * sizeof(float));
        Step(&one, &grid, FLUID_SEPARATE, k);
        // Each pass again, fused, from the separate simulation's own inputs to it: a rounding
        // apart and nothing more, which is what a machine's pass may differ by.
        FluidSplat force[FLUID_SPLATS_MAX];
        FluidSplat colour[FLUID_SPLATS_MAX];
        const uint32_t count = FluidScript(&grid, k, force, colour);
        const FluidFields *const s = &one.work.f;
        FluidFields *const m = &again.work.f;
        FluidSplatPass(FLUID_FUSED, grid.w, grid.h, FLUID_VELOCITY_CHANNELS, force, count,
                       again.velocity, m->splatted);
        FluidSplatPass(FLUID_FUSED, FluidDyeW(&grid), FluidDyeH(&grid), FLUID_DYE_CHANNELS, colour,
                       count, again.dye, m->dyed);
        FluidDivergencePass(&grid, s->splatted, m->divergence);
        FluidSolve(FLUID_FUSED, &grid, again.pressure, s->divergence, FLUID_JACOBI, m->pressure,
                   again.work.scratch);
        FluidGradientPass(FLUID_FUSED, &grid, s->splatted, s->pressure, m->projected);
        FluidAdvectPass(FLUID_FUSED, &grid, s->projected, 1u, FLUID_VELOCITY_CHANNELS,
                        s->projected, FLUID_VELOCITY_KEEP, m->velocity);
        FluidAdvectPass(FLUID_FUSED, &grid, s->projected, FLUID_DYE_SCALE, FLUID_DYE_CHANNELS,
                        s->dyed, FLUID_DYE_KEEP, m->dye);
        const float *const outs[7][2] = {
            { m->splatted, s->splatted },   { m->dyed, s->dyed },
            { m->divergence, s->divergence }, { m->pressure, s->pressure },
            { m->projected, s->projected }, { m->velocity, s->velocity },
            { m->dye, s->dye },
        };
        for (uint32_t c = 0; c < 7u; ++c) {
            const size_t n = FluidReadFloats(&grid, FluidCheckOutput((FluidCheck)c));
            const float far = Farthest(outs[c][0], outs[c][1], n);
            if (far > pass_far[c]) {
                pass_far[c] = far;
                pass_at[c] = k;
            }
        }
        // The judge on the separate simulation's frame, as on a machine's: every check passes.
        FluidFrame frame;
        frame.frame = k;
        frame.read[FLUID_READ_VELOCITY_IN] = again.velocity;
        frame.read[FLUID_READ_PRESSURE_IN] = again.pressure;
        frame.read[FLUID_READ_DYE_IN] = again.dye;
        frame.read[FLUID_READ_SPLATTED] = s->splatted;
        frame.read[FLUID_READ_DYED] = s->dyed;
        frame.read[FLUID_READ_DIVERGENCE] = s->divergence;
        frame.read[FLUID_READ_PRESSURE] = s->pressure;
        frame.read[FLUID_READ_PROJECTED] = s->projected;
        frame.read[FLUID_READ_VELOCITY] = s->velocity;
        frame.read[FLUID_READ_DYE] = s->dye;
        FluidVerdict verdicts[FLUID_CHECKS];
        FluidJudgeFrame(&grid, &frame, budgets[k], &again.work, verdicts);
        for (uint32_t c = 0; c < FLUID_CHECKS; ++c) {
            if (!verdicts[c].passed && judge_failures++ < 8u) {
                printf("frame %u: the judge fails the processor's own frame on check %u\n",
                       (unsigned)k, (unsigned)c);
            }
        }
        if (verdicts[FLUID_CHECK_PROJECTION].measure > ratio_worst) {
            ratio_worst = verdicts[FLUID_CHECK_PROJECTION].measure;
            ratio_at = k;
        }
        if (verdicts[FLUID_CHECK_MASS].measure > mass_worst) {
            mass_worst = verdicts[FLUID_CHECK_MASS].measure;
            mass_at = k;
        }
    }
    static const char *const PASSES[7] = { "force splats", "dye splats", "divergence",
                                           "Jacobi passes", "gradient", "velocity advection",
                                           "dye advection" };
    for (uint32_t c = 0; c < 7u; ++c) {
        const bool pass_held = pass_far[c] * PASS_MARGIN <= FLUID_PASS_ABSOLUTE;
        printf("%s: fused parts by %.3g at most, in frame %u; the budget %.3g is %s %.0f times "
               "it\n",
               PASSES[c], (double)pass_far[c], (unsigned)pass_at[c],
               (double)FLUID_PASS_ABSOLUTE, pass_held ? "at least" : "short of",
               (double)PASS_MARGIN);
        held = held && pass_held;
    }
    printf("projection: leaves at most %.4g of the divergence, in frame %u; the budget %.2g\n",
           ratio_worst, (unsigned)ratio_at, FLUID_DIVERGENCE_LEFT);
    printf("mass: %.4g of its budget off at most, in frame %u; the budget %.2g\n", mass_worst,
           (unsigned)mass_at, FLUID_MASS_OFF);
    printf("the judge failed the processor's own frames %u times\n", (unsigned)judge_failures);
    held = held && ratio_worst <= FLUID_DIVERGENCE_LEFT && mass_worst <= FLUID_MASS_OFF &&
           judge_failures == 0u;
    printf("%s\n", held ? "every budget holds" : "a budget does not hold");
    return held ? 0 : 1;
}
