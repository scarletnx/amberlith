// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// The fluid demo worked out on the processor: Stam's stable fluids on a grid of cells, each
/// frame a chain of passes in ping-pong between two targets a field (Harris, GPU Gems, chapter
/// 38). The splats a frame adds come from its number alone, so the whole run is known before it
/// starts. No libnx, so the host build checks all of it.
///
/// Every pass here is the arithmetic its fragment shader does, one operation a statement, in the
/// shader's order: a multiply and the add after it are rounded apart, so this file answers the
/// same on every processor and compiler (the console's build compiles it -ffp-contract=off), and
/// with FLUID_FUSED each pair is one fused multiply-add, as a graphics compiler may make it. No
/// transcendental is involved: the splat's kernel is a polynomial and its directions a table.
///
/// The grid's index space: cell (i, j) is texel (i, j) of its target, row j in memory order, its
/// centre at (i, j). The velocity and the pressure are on a w by h grid; the dye on one
/// FLUID_DYE_SCALE times as fine each way, which is what the screen shows.

/// The grid the scene runs: 16 by 9, so the dye fills a 1280 by 720 screen at five pixels a texel.
#define FLUID_W 128u
#define FLUID_H 72u
#define FLUID_DYE_SCALE 2u
/// The Jacobi passes a frame's pressure solve makes, each a pass of its own.
#define FLUID_JACOBI 40u
/// The most splats a frame adds: the three emitters and a burst.
#define FLUID_EMITTERS 3u
#define FLUID_SPLATS_MAX 4u

/// What each field keeps of itself a frame: the velocity and the dye through their advection,
/// and the pressure the solve starts from, the last frame's scaled.
#define FLUID_VELOCITY_KEEP 0.999f
#define FLUID_DYE_KEEP 0.994f
#define FLUID_PRESSURE_KEEP 0.8f

/// The channels each field's target has: RG32F, R32F, RGBA32F.
#define FLUID_VELOCITY_CHANNELS 2u
#define FLUID_DYE_CHANNELS 4u

typedef enum {
    /// Every multiply rounded, and the add after it rounded again.
    FLUID_SEPARATE,
    /// Every multiply and the add after it one fused operation, rounded once.
    FLUID_FUSED,
} FluidArith;

typedef struct {
    /// The velocity and pressure grid; the dye's is FLUID_DYE_SCALE times each.
    uint32_t w;
    uint32_t h;
} FluidGrid;

static inline uint32_t FluidDyeW(const FluidGrid *grid) {
    return grid->w * FLUID_DYE_SCALE;
}

static inline uint32_t FluidDyeH(const FluidGrid *grid) {
    return grid->h * FLUID_DYE_SCALE;
}

/// One splat, as the shader's uniform buffer holds it: a centre and the reciprocal of the
/// radius squared in its field's index space, and what it adds at the centre, the kernel
/// (1 - d^2 / r^2)^3 of it at a distance d inside the radius and nothing outside.
typedef struct {
    float x;
    float y;
    float inv_r2;
    float unused;
    float value[4];
} FluidSplat;

/// What frame @p frame adds: the force splats for the velocity and the dye splats for the dye,
/// each in its field's index space, the same splats in the same order. Hands back how many.
uint32_t FluidScript(const FluidGrid *grid, uint32_t frame, FluidSplat force[FLUID_SPLATS_MAX],
                     FluidSplat dye[FLUID_SPLATS_MAX]);

/// The fields a frame leaves, pass by pass, in its targets' layouts: a texel's channels in a
/// row, rows in order. The two the frame starts from are the last frame's velocity and dye and
/// pressure.
typedef struct {
    /// The velocity after the force splats, and its divergence.
    float *splatted;
    float *divergence;
    /// After the Jacobi passes, and the velocity with its gradient taken off.
    float *pressure;
    float *projected;
    /// The velocity advected along itself: the next frame's.
    float *velocity;
    /// The dye after its splats, and after its advection along the projected velocity: the
    /// next frame's.
    float *dyed;
    float *dye;
} FluidFields;

/// Floats each field holds: the velocity's, the pressure's (and the divergence's), the dye's.
size_t FluidVelocityFloats(const FluidGrid *grid);
size_t FluidPressureFloats(const FluidGrid *grid);
size_t FluidDyeFloats(const FluidGrid *grid);

/// The splat pass: @p in with each of @p count splats added, into @p out, a field of @p w by
/// @p h texels of @p channels.
void FluidSplatPass(FluidArith arith, uint32_t w, uint32_t h, uint32_t channels,
                    const FluidSplat *splats, uint32_t count, const float *in, float *out);

/// The divergence pass: half the central differences of @p velocity, a wall's neighbour taking
/// the cell's own velocity reversed, so nothing flows through it.
void FluidDivergencePass(const FluidGrid *grid, const float *velocity, float *out);

/// One Jacobi pass of the pressure solve: a quarter of the four neighbours' pressures, each
/// scaled by @p keep and a wall's the cell's own, less the divergence.
void FluidJacobiPass(FluidArith arith, const FluidGrid *grid, const float *pressure,
                     const float *divergence, float keep, float *out);

/// The @p count Jacobi passes of a frame from @p pressure, the first with FLUID_PRESSURE_KEEP
/// and the rest with one, ping-ponging between @p out and @p scratch; the result is in @p out.
void FluidSolve(FluidArith arith, const FluidGrid *grid, const float *pressure,
                const float *divergence, uint32_t count, float *out, float *scratch);

/// The gradient pass: @p velocity less half the central differences of @p pressure.
void FluidGradientPass(FluidArith arith, const FluidGrid *grid, const float *velocity,
                       const float *pressure, float *out);

/// The advection pass: each texel of @p field (@p scale times the grid each way, @p channels a
/// texel) traced back along @p velocity, sampled there by hand from its four nearest texels and
/// scaled by @p keep.
void FluidAdvectPass(FluidArith arith, const FluidGrid *grid, const float *velocity,
                     uint32_t scale, uint32_t channels, const float *field, float keep,
                     float *out);

/// One whole frame from the fields it starts with, every pass's output into @p out, the solve's
/// other target in @p scratch (a pressure's floats).
void FluidStep(FluidArith arith, const FluidGrid *grid, uint32_t frame, const float *velocity,
               const float *pressure, const float *dye, FluidFields *out, float *scratch);

/// FNV-1a over the bits of @p count floats, for a field pinned to a value.
uint32_t FluidHashFloats(const float *values, size_t count);

/// The aliasing level: every FLUID_ALIAS_EVERY frames from FLUID_ALIAS_FIRST, the dye's splat
/// target and the divergence, laid one after the other in the memory the dye's own target follows,
/// are drawn FLUID_ALIAS_SHIFT bytes further on for that frame alone, over their old selves and the
/// dye's first bytes; and every frame a compute pass writes a FLUID_ALIAS_SIDE square R32_Uint
/// storage image laid at that same shift, through bytes the splat target and its shifted self both
/// cover (t_demo_fluid.c has the layout). The judged frames of the first FLUID_ALIAS_JUDGED shifts
/// are the shift's, the one after and the one after that.
#define FLUID_ALIAS_FIRST 20u
#define FLUID_ALIAS_EVERY 40u
#define FLUID_ALIAS_SHIFT 0x8000u
#define FLUID_ALIAS_SIDE 32u
#define FLUID_ALIAS_JUDGED 3u

/// Whether frame @p frame draws the shifted pair.
bool FluidAliasShifted(uint32_t frame);

/// The word the compute pass stores at (@p x, @p y) of its image in frame @p frame.
uint32_t FluidAliasWord(uint32_t frame, uint32_t x, uint32_t y);

/// The frames a run of @p frames judges, lowest first: the early ones, 2^n - 1 up to
/// FLUID_EARLY_LAST, held to the whole simulation; each quarter's last frame, the run's last among
/// them; and with @p alias the first shifts' frames. Hands back how many.
#define FLUID_JUDGED_MAX 20u
#define FLUID_EARLY_LAST 63u
uint32_t FluidJudgedFrames(uint32_t frames, bool alias, uint32_t out[FLUID_JUDGED_MAX]);

/// What a comparison of a machine's field with the one expected found: how many floats were off
/// by more than the budget (a NaN or an infinity among them), the first of them and the
/// farthest.
typedef struct {
    uint32_t off;
    uint32_t first;
    float first_got;
    float first_want;
    float farthest;
    uint32_t farthest_at;
} FluidDiff;

/// @p got against @p want, @p count floats: off when the distance is past @p absolute plus
/// @p relative times the expected value's size, or either is not a finite number.
void FluidCompare(const float *got, const float *want, size_t count, float absolute,
                  float relative, FluidDiff *out);

/// The budgets a machine's fields are held to. The early frames' against the whole simulation
/// from frame 0: the farthest two correct roundings part by over those frames, a hundred times
/// over (tools/fluid-envelope.c measures it). A pass's against itself worked out again from the
/// machine's own inputs: an advection's sample moves by the field's slope times the rounding of
/// where it lands, the largest, and every other pass is a few roundings of its values.
#define FLUID_EARLY_ABSOLUTE 0x1p-10f
#define FLUID_PASS_ABSOLUTE 0x1p-12f
#define FLUID_PASS_RELATIVE 0x1p-16f

/// The root mean square of the divergence pass's stencil over @p velocity, in double.
double FluidDivergenceRms(const FluidGrid *grid, const float *velocity);

/// The projection's budget: the divergence after it at most this much of the divergence before.
/// Not nought: FLUID_JACOBI passes do not converge, and on a grid where the velocity and the
/// pressure share their cells the divergence of a pressure gradient is the wide Laplacian, not
/// the one the Jacobi passes solve, which leaves some whatever the passes.
#define FLUID_DIVERGENCE_LEFT 0.5

/// The dye's mass, the sum of every texel's three colours, in double.
double FluidDyeMass(const FluidGrid *grid, const float *dye);

/// The mass the splats of frames 0 to each of @p frames put in, less what the dye's keep took,
/// frame by frame: what the dye after that frame holds when its advection moves dye and makes
/// none. @p frames is @p count frames, lowest first; one pass over the script for all of them.
void FluidDyeBudgets(const FluidGrid *grid, const uint32_t *frames, uint32_t count,
                     double *out);

/// How far the dye's mass may be from its budget, a fraction of it: what the semi-Lagrangian
/// advection, which conserves nothing exactly, moved it by on the processor over a run of
/// MODE_FRAMES_MAX frames, with a margin.
#define FLUID_MASS_OFF 0.25

/// Floats that are not finite numbers, and dye below nought, in a field of @p count floats.
typedef struct {
    uint32_t not_finite;
    uint32_t not_finite_at;
    uint32_t negative;
    uint32_t negative_at;
} FluidSane;

void FluidSanity(const float *values, size_t count, bool dye, FluidSane *out);

/// A judged frame as a machine left it: the fields it started from and each pass's output, in
/// the targets' layouts.
typedef enum {
    FLUID_READ_VELOCITY_IN,
    FLUID_READ_PRESSURE_IN,
    FLUID_READ_DYE_IN,
    FLUID_READ_SPLATTED,
    FLUID_READ_DYED,
    FLUID_READ_DIVERGENCE,
    FLUID_READ_PRESSURE,
    FLUID_READ_PROJECTED,
    FLUID_READ_VELOCITY,
    FLUID_READ_DYE,
    FLUID_READS,
} FluidRead;

typedef enum { FLUID_KIND_VELOCITY, FLUID_KIND_PRESSURE, FLUID_KIND_DYE } FluidKind;

/// Which field a read is, and the floats it holds.
FluidKind FluidReadKind(FluidRead read);
size_t FluidReadFloats(const FluidGrid *grid, FluidRead read);

/// What a judged frame's line says each read is.
const char *FluidReadName(FluidRead read);

typedef struct {
    uint32_t frame;
    const float *read[FLUID_READS];
} FluidFrame;

/// What the judge holds a judged frame to, in the order the demo's cases report them: each pass
/// worked out again from the frame's own inputs, then the invariants.
typedef enum {
    FLUID_CHECK_FORCE,
    FLUID_CHECK_DYE_SPLAT,
    FLUID_CHECK_DIVERGENCE,
    FLUID_CHECK_SOLVE,
    FLUID_CHECK_GRADIENT,
    FLUID_CHECK_ADVECT_VELOCITY,
    FLUID_CHECK_ADVECT_DYE,
    FLUID_CHECK_PROJECTION,
    FLUID_CHECK_MASS,
    FLUID_CHECK_SANE,
    FLUID_CHECKS,
} FluidCheck;

/// What one check found in one frame. A pass's: the comparison. The projection's: the divergence
/// before and after it, @p measure their ratio. The mass's: the mass and its budget, @p measure
/// how far apart as a fraction of the budget. The sanity's: the first read gone wrong and what
/// was wrong in it, and how many reads were.
typedef struct {
    bool passed;
    FluidDiff diff;
    double before;
    double after;
    double measure;
    uint32_t wrong_reads;
    FluidRead first_read;
    FluidSane sane;
} FluidVerdict;

/// The judge's own fields, a frame's worth, for FluidJudgeFrame to work the passes out into.
typedef struct {
    FluidFields f;
    float *scratch;
} FluidWork;

/// The read a pass's check holds to the pass worked out again; FLUID_READS for an invariant.
FluidRead FluidCheckOutput(FluidCheck check);

/// Whether judged frame @p after, the frame right after @p before, starts from the very fields
/// @p before left: its velocity, pressure and dye, bit for bit. Where one differs, the read of
/// @p after and the float in it go to @p read and @p at.
bool FluidHandedOn(const FluidGrid *grid, const FluidFrame *before, const FluidFrame *after,
                   FluidRead *read, uint32_t *at);

/// Holds @p frame to every check; @p budget is FluidDyeBudgets' for its frame.
void FluidJudgeFrame(const FluidGrid *grid, const FluidFrame *frame, double budget,
                     FluidWork *work, FluidVerdict out[FLUID_CHECKS]);

#ifdef __cplusplus
}
#endif
