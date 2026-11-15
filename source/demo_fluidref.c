// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "demo_fluidref.h"

// A multiply and the add that takes it are two statements everywhere below, so a compiler that
// contracts within an expression (clang's default) has none to contract; gcc in its GNU mode
// contracts across statements too, so the console's Makefile builds this file -ffp-contract=off.

/// @p x * @p y + @p z, rounded as @p arith says.
static float Mad(FluidArith arith, float x, float y, float z) {
    if (arith == FLUID_FUSED) {
        return fmaf(x, y, z);
    }
    const float product = x * y;
    return product + z;
}

size_t FluidVelocityFloats(const FluidGrid *grid) {
    return (size_t)grid->w * grid->h * FLUID_VELOCITY_CHANNELS;
}

size_t FluidPressureFloats(const FluidGrid *grid) {
    return (size_t)grid->w * grid->h;
}

size_t FluidDyeFloats(const FluidGrid *grid) {
    return (size_t)FluidDyeW(grid) * FluidDyeH(grid) * FLUID_DYE_CHANNELS;
}

// The script. Integer arithmetic for where and when, and the four operations, square roots
// left out too, for the floats: each is rounded the one way IEEE 754 says on every processor.

/// Up from nought to @p period and back over 2 * @p period.
static uint32_t Tri(uint32_t t, uint32_t period) {
    const uint32_t m = t % (2u * period);
    return m <= period ? m : 2u * period - m;
}

/// (@p dx, @p dy) turned by the angle whose half tangent is @p t, by the rational parametrisation
/// of the circle: (1 - t^2, 2t) / (1 + t^2) is the turn's cosine and sine, with no sine taken.
static void Turn(float dx, float dy, float t, float out[2]) {
    const float t2 = t * t;
    const float d = 1.0f + t2;
    const float one_less = 1.0f - t2;
    const float c = one_less / d;
    const float twice = 2.0f * t;
    const float s = twice / d;
    const float xc = dx * c;
    const float ys = dy * s;
    const float xs = dx * s;
    const float yc = dy * c;
    out[0] = xc - ys;
    out[1] = xs + yc;
}

/// A sweep from -1/2 to 1/2 and back over 2 * @p period frames, the half tangent a jet turns by.
static float Sweep(uint32_t frame, uint32_t period) {
    const float at = (float)Tri(frame, period);
    const float half = (float)period * 0.5f;
    const float centred = at - half;
    return centred / (float)period;
}

/// Grid coordinate @p num / @p den of the way across @p cells cells' centres.
static float Across(uint32_t cells, uint32_t num, uint32_t den) {
    return (float)((cells - 1u) * num) / (float)den;
}

/// The dye's colours, each emitter and burst taking one in turn.
static const float PALETTE[6][3] = {
    { 1.00f, 0.38f, 0.10f }, { 0.12f, 0.55f, 1.00f }, { 0.95f, 0.16f, 0.62f },
    { 0.22f, 1.00f, 0.48f }, { 1.00f, 0.82f, 0.16f }, { 0.55f, 0.26f, 1.00f },
};

/// How much dye a splat adds at its centre a frame, and the force: a jet's and a burst's.
#define DYE_STRENGTH 0.12f
#define JET_FORCE 0.35f
#define BURST_FORCE 0.9f
/// The radius squared's reciprocal in velocity cells: a jet four cells wide each side, a burst
/// six (a hair under 1/36, the nearest float).
#define JET_INV_R2 0.0625f
#define BURST_INV_R2 0.027777778f
/// A burst every BURST_EVERY frames from BURST_FIRST, BURST_FOR frames long.
#define BURST_EVERY 120u
#define BURST_FIRST 60u
#define BURST_FOR 12u

static const struct {
    /// Where, as eighths of the grid across and down.
    uint32_t x8;
    uint32_t y8;
    /// Which way it points before it turns: a unit vector, y down the screen.
    float dx;
    float dy;
    /// The frames its sweep takes each way, and how far in it starts.
    uint32_t period;
    uint32_t phase;
} EMITTERS[FLUID_EMITTERS] = {
    { 1u, 6u, 0.70710678f, -0.70710678f, 150u, 0u },
    { 7u, 6u, -0.70710678f, -0.70710678f, 130u, 65u },
    { 4u, 1u, 0.0f, 1.0f, 110u, 30u },
};

/// Where a burst goes, as eighths, in turn.
static const uint32_t BURSTS[6][2] = { { 3u, 3u }, { 5u, 5u }, { 2u, 4u },
                                       { 6u, 2u }, { 4u, 6u }, { 5u, 3u } };

/// The splat at velocity cell (@p x, @p y) for both fields: the force @p fx, @p fy into
/// @p force, and colour @p colour into @p dye, at the dye's centre and radius for the same place.
static void Splat(float x, float y, float inv_r2, float fx, float fy, uint32_t colour,
                  FluidSplat *force, FluidSplat *dye) {
    memset(force, 0, sizeof(*force));
    memset(dye, 0, sizeof(*dye));
    force->x = x;
    force->y = y;
    force->inv_r2 = inv_r2;
    force->value[0] = fx;
    force->value[1] = fy;
    // Dye texel I's centre is velocity cell (I + 1/2) / 2 - 1/2: velocity cell x is dye texel
    // 2x + 1/2, and a radius twice as many texels has a quarter of the reciprocal. Exact.
    const float x2 = x * 2.0f;
    const float y2 = y * 2.0f;
    dye->x = x2 + 0.5f;
    dye->y = y2 + 0.5f;
    dye->inv_r2 = inv_r2 * 0.25f;
    for (uint32_t c = 0; c < 3u; ++c) {
        dye->value[c] = PALETTE[colour % 6u][c] * DYE_STRENGTH;
    }
}

uint32_t FluidScript(const FluidGrid *grid, uint32_t frame, FluidSplat force[FLUID_SPLATS_MAX],
                     FluidSplat dye[FLUID_SPLATS_MAX]) {
    uint32_t count = 0;
    for (uint32_t e = 0; e < FLUID_EMITTERS; ++e) {
        float dir[2];
        Turn(EMITTERS[e].dx, EMITTERS[e].dy, Sweep(frame + EMITTERS[e].phase, EMITTERS[e].period),
             dir);
        const float fx = dir[0] * JET_FORCE;
        const float fy = dir[1] * JET_FORCE;
        // Each emitter's colour moves on every four seconds, the three apart.
        const uint32_t colour = e * 2u + frame / 240u;
        Splat(Across(grid->w, EMITTERS[e].x8, 8u), Across(grid->h, EMITTERS[e].y8, 8u),
              JET_INV_R2, fx, fy, colour, &force[count], &dye[count]);
        ++count;
    }
    if (frame >= BURST_FIRST && (frame - BURST_FIRST) % BURST_EVERY < BURST_FOR) {
        const uint32_t n = (frame - BURST_FIRST) / BURST_EVERY;
        float dir[2];
        // Each burst a direction of its own, a turn of up to a right angle either way off up.
        const float t = (float)((int32_t)((n * 7u) % 9u) - 4) * 0.25f;
        Turn(0.0f, -1.0f, t, dir);
        const float fx = dir[0] * BURST_FORCE;
        const float fy = dir[1] * BURST_FORCE;
        Splat(Across(grid->w, BURSTS[n % 6u][0], 8u), Across(grid->h, BURSTS[n % 6u][1], 8u),
              BURST_INV_R2, fx, fy, n + 3u, &force[count], &dye[count]);
        ++count;
    }
    return count;
}

// The passes.

static uint32_t ClampIndex(int64_t i, uint32_t n) {
    return i < 0 ? 0u : i >= (int64_t)n ? n - 1u : (uint32_t)i;
}

void FluidSplatPass(FluidArith arith, uint32_t w, uint32_t h, uint32_t channels,
                    const FluidSplat *splats, uint32_t count, const float *in, float *out) {
    for (uint32_t j = 0; j < h; ++j) {
        for (uint32_t i = 0; i < w; ++i) {
            const size_t at = ((size_t)j * w + i) * channels;
            float v[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            for (uint32_t c = 0; c < channels; ++c) {
                v[c] = in[at + c];
            }
            for (uint32_t s = 0; s < count; ++s) {
                const float dx = (float)i - splats[s].x;
                const float dy = (float)j - splats[s].y;
                const float dx2 = dx * dx;
                const float d2 = Mad(arith, dy, dy, dx2);
                const float t = Mad(arith, -d2, splats[s].inv_r2, 1.0f);
                if (t > 0.0f) {
                    const float t2 = t * t;
                    const float weight = t2 * t;
                    for (uint32_t c = 0; c < channels; ++c) {
                        v[c] = Mad(arith, splats[s].value[c], weight, v[c]);
                    }
                }
            }
            for (uint32_t c = 0; c < channels; ++c) {
                out[at + c] = v[c];
            }
        }
    }
}

void FluidDivergencePass(const FluidGrid *grid, const float *velocity, float *out) {
    const uint32_t w = grid->w;
    const uint32_t h = grid->h;
    for (uint32_t j = 0; j < h; ++j) {
        for (uint32_t i = 0; i < w; ++i) {
            const float *const c = velocity + ((size_t)j * w + i) * 2u;
            const float left = i > 0u ? c[-2] : -c[0];
            const float right = i + 1u < w ? c[2] : -c[0];
            const float up = j > 0u ? c[-(ptrdiff_t)w * 2 + 1] : -c[1];
            const float down = j + 1u < h ? c[(ptrdiff_t)w * 2 + 1] : -c[1];
            const float across = right - left;
            const float along = down - up;
            const float sum = across + along;
            out[(size_t)j * w + i] = sum * 0.5f;
        }
    }
}

void FluidJacobiPass(FluidArith arith, const FluidGrid *grid, const float *pressure,
                     const float *divergence, float keep, float *out) {
    const uint32_t w = grid->w;
    const uint32_t h = grid->h;
    for (uint32_t j = 0; j < h; ++j) {
        for (uint32_t i = 0; i < w; ++i) {
            const float left = pressure[(size_t)j * w + ClampIndex((int64_t)i - 1, w)];
            const float right = pressure[(size_t)j * w + ClampIndex((int64_t)i + 1, w)];
            const float up = pressure[(size_t)ClampIndex((int64_t)j - 1, h) * w + i];
            const float down = pressure[(size_t)ClampIndex((int64_t)j + 1, h) * w + i];
            const float lr = left + right;
            const float lru = lr + up;
            const float sum = lru + down;
            const float less = Mad(arith, keep, sum, -divergence[(size_t)j * w + i]);
            out[(size_t)j * w + i] = less * 0.25f;
        }
    }
}

void FluidSolve(FluidArith arith, const FluidGrid *grid, const float *pressure,
                const float *divergence, uint32_t count, float *out, float *scratch) {
    // The passes alternate between the two, so the last lands in out: an odd count starts there.
    float *targets[2] = { count % 2u == 1u ? out : scratch, count % 2u == 1u ? scratch : out };
    const float *from = pressure;
    for (uint32_t n = 0; n < count; ++n) {
        float *const to = targets[n % 2u];
        FluidJacobiPass(arith, grid, from, divergence, n == 0u ? FLUID_PRESSURE_KEEP : 1.0f, to);
        from = to;
    }
    if (count == 0u) {
        memcpy(out, pressure, FluidPressureFloats(grid) * sizeof(float));
    }
}

void FluidGradientPass(FluidArith arith, const FluidGrid *grid, const float *velocity,
                       const float *pressure, float *out) {
    const uint32_t w = grid->w;
    const uint32_t h = grid->h;
    for (uint32_t j = 0; j < h; ++j) {
        for (uint32_t i = 0; i < w; ++i) {
            const float left = pressure[(size_t)j * w + ClampIndex((int64_t)i - 1, w)];
            const float right = pressure[(size_t)j * w + ClampIndex((int64_t)i + 1, w)];
            const float up = pressure[(size_t)ClampIndex((int64_t)j - 1, h) * w + i];
            const float down = pressure[(size_t)ClampIndex((int64_t)j + 1, h) * w + i];
            const size_t at = ((size_t)j * w + i) * 2u;
            const float across = right - left;
            const float along = down - up;
            out[at] = Mad(arith, -0.5f, across, velocity[at]);
            out[at + 1u] = Mad(arith, -0.5f, along, velocity[at + 1u]);
        }
    }
}

/// @p field (@p w by @p h, @p channels a texel) at index space point (@p x, @p y), from its four
/// nearest texels, the point clamped to the centres of the edge texels.
static void Bilerp(FluidArith arith, const float *field, uint32_t w, uint32_t h,
                   uint32_t channels, float x, float y, float out[4]) {
    const float cx = fminf(fmaxf(x, 0.0f), (float)(w - 1u));
    const float cy = fminf(fmaxf(y, 0.0f), (float)(h - 1u));
    const float x0 = floorf(cx);
    const float y0 = floorf(cy);
    const float fx = cx - x0;
    const float fy = cy - y0;
    const uint32_t i0 = (uint32_t)x0;
    const uint32_t j0 = (uint32_t)y0;
    const uint32_t i1 = i0 + 1u < w ? i0 + 1u : w - 1u;
    const uint32_t j1 = j0 + 1u < h ? j0 + 1u : h - 1u;
    const float *const a = field + ((size_t)j0 * w + i0) * channels;
    const float *const b = field + ((size_t)j0 * w + i1) * channels;
    const float *const c = field + ((size_t)j1 * w + i0) * channels;
    const float *const d = field + ((size_t)j1 * w + i1) * channels;
    for (uint32_t k = 0; k < channels; ++k) {
        const float ab = b[k] - a[k];
        const float top = Mad(arith, ab, fx, a[k]);
        const float cd = d[k] - c[k];
        const float bottom = Mad(arith, cd, fx, c[k]);
        const float tb = bottom - top;
        out[k] = Mad(arith, tb, fy, top);
    }
}

void FluidAdvectPass(FluidArith arith, const FluidGrid *grid, const float *velocity,
                     uint32_t scale, uint32_t channels, const float *field, float keep,
                     float *out) {
    const uint32_t fw = grid->w * scale;
    const uint32_t fh = grid->h * scale;
    // Field texel I's centre in velocity cells: (I + 1/2) / scale - 1/2, I times the step and the
    // offset, both exact for a scale of one or two.
    const float step = 1.0f / (float)scale;
    const float half_step = step * 0.5f;
    const float offset = half_step - 0.5f;
    const float reach = (float)scale;
    for (uint32_t j = 0; j < fh; ++j) {
        for (uint32_t i = 0; i < fw; ++i) {
            const float vx = Mad(arith, (float)i, step, offset);
            const float vy = Mad(arith, (float)j, step, offset);
            float u[4];
            Bilerp(arith, velocity, grid->w, grid->h, 2u, vx, vy, u);
            const float back_x = Mad(arith, -reach, u[0], (float)i);
            const float back_y = Mad(arith, -reach, u[1], (float)j);
            float v[4];
            Bilerp(arith, field, fw, fh, channels, back_x, back_y, v);
            const size_t at = ((size_t)j * fw + i) * channels;
            for (uint32_t k = 0; k < channels; ++k) {
                out[at + k] = v[k] * keep;
            }
        }
    }
}

void FluidStep(FluidArith arith, const FluidGrid *grid, uint32_t frame, const float *velocity,
               const float *pressure, const float *dye, FluidFields *out, float *scratch) {
    FluidSplat force[FLUID_SPLATS_MAX];
    FluidSplat colour[FLUID_SPLATS_MAX];
    const uint32_t count = FluidScript(grid, frame, force, colour);
    FluidSplatPass(arith, grid->w, grid->h, FLUID_VELOCITY_CHANNELS, force, count, velocity,
                   out->splatted);
    FluidSplatPass(arith, FluidDyeW(grid), FluidDyeH(grid), FLUID_DYE_CHANNELS, colour, count,
                   dye, out->dyed);
    FluidDivergencePass(grid, out->splatted, out->divergence);
    FluidSolve(arith, grid, pressure, out->divergence, FLUID_JACOBI, out->pressure, scratch);
    FluidGradientPass(arith, grid, out->splatted, out->pressure, out->projected);
    FluidAdvectPass(arith, grid, out->projected, 1u, FLUID_VELOCITY_CHANNELS, out->projected,
                    FLUID_VELOCITY_KEEP, out->velocity);
    FluidAdvectPass(arith, grid, out->projected, FLUID_DYE_SCALE, FLUID_DYE_CHANNELS, out->dyed,
                    FLUID_DYE_KEEP, out->dye);
}

uint32_t FluidHashFloats(const float *values, size_t count) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < count; ++i) {
        uint32_t bits;
        memcpy(&bits, &values[i], sizeof(bits));
        for (uint32_t b = 0; b < 4u; ++b) {
            hash ^= (bits >> (8u * b)) & 0xFFu;
            hash *= 16777619u;
        }
    }
    return hash;
}

bool FluidAliasShifted(uint32_t frame) {
    return frame >= FLUID_ALIAS_FIRST && (frame - FLUID_ALIAS_FIRST) % FLUID_ALIAS_EVERY == 0u;
}

uint32_t FluidAliasWord(uint32_t frame, uint32_t x, uint32_t y) {
    return frame * (FLUID_ALIAS_SIDE * FLUID_ALIAS_SIDE) + y * FLUID_ALIAS_SIDE + x + 1u;
}

/// Adds @p k to the @p count sorted frames in @p out unless it is there or past the run.
static void AddFrame(uint32_t *out, uint32_t *count, uint32_t k, uint32_t frames) {
    if (k >= frames || *count >= FLUID_JUDGED_MAX) {
        return;
    }
    uint32_t at = 0;
    while (at < *count && out[at] < k) {
        ++at;
    }
    if (at < *count && out[at] == k) {
        return;
    }
    memmove(out + at + 1, out + at, (*count - at) * sizeof(uint32_t));
    out[at] = k;
    ++*count;
}

uint32_t FluidJudgedFrames(uint32_t frames, bool alias, uint32_t out[FLUID_JUDGED_MAX]) {
    uint32_t count = 0;
    for (uint32_t k = 1; k - 1u <= FLUID_EARLY_LAST; k *= 2u) {
        AddFrame(out, &count, k - 1u, frames);
    }
    uint32_t last = count > 0u ? out[count - 1u] : 0u;
    for (uint32_t q = 1; q <= 4u; ++q) {
        const uint32_t k = (uint32_t)(((uint64_t)frames * q) / 4u);
        // A quarter's last frame among the early ones adds nothing they do not judge.
        if (k > 0u && k - 1u > last) {
            AddFrame(out, &count, k - 1u, frames);
            last = k - 1u;
        }
    }
    for (uint32_t n = 0; alias && n < FLUID_ALIAS_JUDGED; ++n) {
        const uint32_t shift = FLUID_ALIAS_FIRST + n * FLUID_ALIAS_EVERY;
        for (uint32_t after = 0; after < 3u; ++after) {
            AddFrame(out, &count, shift + after, frames);
        }
    }
    return count;
}

void FluidCompare(const float *got, const float *want, size_t count, float absolute,
                  float relative, FluidDiff *out) {
    memset(out, 0, sizeof(*out));
    for (size_t i = 0; i < count; ++i) {
        const float distance = fabsf(got[i] - want[i]);
        const float scaled = relative * fabsf(want[i]);
        const float limit = absolute + scaled;
        const bool finite = isfinite(got[i]) && isfinite(want[i]);
        // A NaN distance fails the comparison too, and counts as the farthest a float can be.
        const float far = finite && distance == distance ? distance : INFINITY;
        if (far > out->farthest) {
            out->farthest = far;
            out->farthest_at = (uint32_t)i;
        }
        if (finite && distance <= limit) {
            continue;
        }
        if (out->off++ == 0u) {
            out->first = (uint32_t)i;
            out->first_got = got[i];
            out->first_want = want[i];
        }
    }
}

double FluidDivergenceRms(const FluidGrid *grid, const float *velocity) {
    const uint32_t w = grid->w;
    const uint32_t h = grid->h;
    double sum = 0.0;
    for (uint32_t j = 0; j < h; ++j) {
        for (uint32_t i = 0; i < w; ++i) {
            const float *const c = velocity + ((size_t)j * w + i) * 2u;
            const double left = i > 0u ? c[-2] : -c[0];
            const double right = i + 1u < w ? c[2] : -c[0];
            const double up = j > 0u ? c[-(ptrdiff_t)w * 2 + 1] : -c[1];
            const double down = j + 1u < h ? c[(ptrdiff_t)w * 2 + 1] : -c[1];
            const double div = 0.5 * ((right - left) + (down - up));
            sum += div * div;
        }
    }
    return sqrt(sum / ((double)w * h));
}

double FluidDyeMass(const FluidGrid *grid, const float *dye) {
    const size_t texels = (size_t)FluidDyeW(grid) * FluidDyeH(grid);
    double sum = 0.0;
    for (size_t t = 0; t < texels; ++t) {
        sum += (double)dye[t * 4u] + (double)dye[t * 4u + 1u] + (double)dye[t * 4u + 2u];
    }
    return sum;
}

/// The dye frame @p frame's splats add, all three colours, over the texels inside each radius.
static double SplatMass(const FluidGrid *grid, uint32_t frame) {
    FluidSplat force[FLUID_SPLATS_MAX];
    FluidSplat dye[FLUID_SPLATS_MAX];
    const uint32_t count = FluidScript(grid, frame, force, dye);
    const int64_t w = FluidDyeW(grid);
    const int64_t h = FluidDyeH(grid);
    double sum = 0.0;
    for (uint32_t s = 0; s < count; ++s) {
        const double reach = 1.0 / sqrt((double)dye[s].inv_r2) + 1.0;
        const double colour = (double)dye[s].value[0] + dye[s].value[1] + dye[s].value[2];
        const int64_t x0 = (int64_t)floor(dye[s].x - reach);
        const int64_t x1 = (int64_t)ceil(dye[s].x + reach);
        const int64_t y0 = (int64_t)floor(dye[s].y - reach);
        const int64_t y1 = (int64_t)ceil(dye[s].y + reach);
        for (int64_t j = y0 < 0 ? 0 : y0; j <= y1 && j < h; ++j) {
            for (int64_t i = x0 < 0 ? 0 : x0; i <= x1 && i < w; ++i) {
                const double dx = (double)i - dye[s].x;
                const double dy = (double)j - dye[s].y;
                const double t = 1.0 - (dx * dx + dy * dy) * dye[s].inv_r2;
                if (t > 0.0) {
                    sum += colour * t * t * t;
                }
            }
        }
    }
    return sum;
}

void FluidDyeBudgets(const FluidGrid *grid, const uint32_t *frames, uint32_t count,
                     double *out) {
    double budget = 0.0;
    uint32_t k = 0;
    for (uint32_t i = 0; i < count; ++i) {
        for (; k <= frames[i]; ++k) {
            budget = (budget + SplatMass(grid, k)) * (double)FLUID_DYE_KEEP;
        }
        out[i] = budget;
    }
}

void FluidSanity(const float *values, size_t count, bool dye, FluidSane *out) {
    memset(out, 0, sizeof(*out));
    for (size_t i = 0; i < count; ++i) {
        if (!isfinite(values[i])) {
            if (out->not_finite++ == 0u) {
                out->not_finite_at = (uint32_t)i;
            }
        } else if (dye && values[i] < 0.0f && out->negative++ == 0u) {
            out->negative_at = (uint32_t)i;
        }
    }
}

// The judge.

FluidKind FluidReadKind(FluidRead read) {
    static const FluidKind KINDS[FLUID_READS] = {
        FLUID_KIND_VELOCITY, FLUID_KIND_PRESSURE, FLUID_KIND_DYE,      FLUID_KIND_VELOCITY,
        FLUID_KIND_DYE,      FLUID_KIND_PRESSURE, FLUID_KIND_PRESSURE, FLUID_KIND_VELOCITY,
        FLUID_KIND_VELOCITY, FLUID_KIND_DYE,
    };
    return (unsigned)read < FLUID_READS ? KINDS[read] : FLUID_KIND_PRESSURE;
}

size_t FluidReadFloats(const FluidGrid *grid, FluidRead read) {
    const FluidKind kind = FluidReadKind(read);
    return kind == FLUID_KIND_VELOCITY   ? FluidVelocityFloats(grid)
           : kind == FLUID_KIND_PRESSURE ? FluidPressureFloats(grid)
                                         : FluidDyeFloats(grid);
}

const char *FluidReadName(FluidRead read) {
    static const char *const NAMES[FLUID_READS] = {
        "the velocity it started from",
        "the pressure it started from",
        "the dye it started from",
        "the splatted velocity",
        "the splatted dye",
        "the divergence",
        "the pressure",
        "the projected velocity",
        "the velocity",
        "the dye",
    };
    return (unsigned)read < FLUID_READS ? NAMES[read] : "no field";
}

FluidRead FluidCheckOutput(FluidCheck check) {
    static const FluidRead OUTPUTS[FLUID_CHECKS] = {
        FLUID_READ_SPLATTED,  FLUID_READ_DYED,     FLUID_READ_DIVERGENCE, FLUID_READ_PRESSURE,
        FLUID_READ_PROJECTED, FLUID_READ_VELOCITY, FLUID_READ_DYE,        FLUID_READS,
        FLUID_READS,          FLUID_READS,
    };
    return (unsigned)check < FLUID_CHECKS ? OUTPUTS[check] : FLUID_READS;
}

bool FluidHandedOn(const FluidGrid *grid, const FluidFrame *before, const FluidFrame *after,
                   FluidRead *read, uint32_t *at) {
    static const FluidRead PAIRS[3][2] = {
        { FLUID_READ_VELOCITY, FLUID_READ_VELOCITY_IN },
        { FLUID_READ_PRESSURE, FLUID_READ_PRESSURE_IN },
        { FLUID_READ_DYE, FLUID_READ_DYE_IN },
    };
    for (uint32_t p = 0; p < 3u; ++p) {
        const float *const left = before->read[PAIRS[p][0]];
        const float *const started = after->read[PAIRS[p][1]];
        const size_t count = FluidReadFloats(grid, PAIRS[p][0]);
        for (size_t i = 0; i < count; ++i) {
            // Bits, not values: a NaN handed on is the same NaN, and a nought keeps its sign.
            if (memcmp(&left[i], &started[i], sizeof(float)) != 0) {
                *read = PAIRS[p][1];
                *at = (uint32_t)i;
                return false;
            }
        }
    }
    return true;
}

/// A pass's output @p got against @p want, @p count floats, within the pass budget.
static void PassVerdict(const float *got, const float *want, size_t count, FluidVerdict *out) {
    FluidCompare(got, want, count, FLUID_PASS_ABSOLUTE, FLUID_PASS_RELATIVE, &out->diff);
    out->passed = out->diff.off == 0u;
}

void FluidJudgeFrame(const FluidGrid *grid, const FluidFrame *frame, double budget,
                     FluidWork *work, FluidVerdict out[FLUID_CHECKS]) {
    memset(out, 0, sizeof(FluidVerdict) * FLUID_CHECKS);
    const float *const *const r = frame->read;
    const size_t v = FluidVelocityFloats(grid);
    const size_t p = FluidPressureFloats(grid);
    const size_t d = FluidDyeFloats(grid);
    FluidSplat force[FLUID_SPLATS_MAX];
    FluidSplat colour[FLUID_SPLATS_MAX];
    const uint32_t count = FluidScript(grid, frame->frame, force, colour);

    FluidSplatPass(FLUID_SEPARATE, grid->w, grid->h, FLUID_VELOCITY_CHANNELS, force, count,
                   r[FLUID_READ_VELOCITY_IN], work->f.splatted);
    PassVerdict(r[FLUID_READ_SPLATTED], work->f.splatted, v, &out[FLUID_CHECK_FORCE]);
    FluidSplatPass(FLUID_SEPARATE, FluidDyeW(grid), FluidDyeH(grid), FLUID_DYE_CHANNELS, colour,
                   count, r[FLUID_READ_DYE_IN], work->f.dyed);
    PassVerdict(r[FLUID_READ_DYED], work->f.dyed, d, &out[FLUID_CHECK_DYE_SPLAT]);
    FluidDivergencePass(grid, r[FLUID_READ_SPLATTED], work->f.divergence);
    PassVerdict(r[FLUID_READ_DIVERGENCE], work->f.divergence, p, &out[FLUID_CHECK_DIVERGENCE]);
    FluidSolve(FLUID_SEPARATE, grid, r[FLUID_READ_PRESSURE_IN], r[FLUID_READ_DIVERGENCE],
               FLUID_JACOBI, work->f.pressure, work->scratch);
    PassVerdict(r[FLUID_READ_PRESSURE], work->f.pressure, p, &out[FLUID_CHECK_SOLVE]);
    FluidGradientPass(FLUID_SEPARATE, grid, r[FLUID_READ_SPLATTED], r[FLUID_READ_PRESSURE],
                      work->f.projected);
    PassVerdict(r[FLUID_READ_PROJECTED], work->f.projected, v, &out[FLUID_CHECK_GRADIENT]);
    FluidAdvectPass(FLUID_SEPARATE, grid, r[FLUID_READ_PROJECTED], 1u, FLUID_VELOCITY_CHANNELS,
                    r[FLUID_READ_PROJECTED], FLUID_VELOCITY_KEEP, work->f.velocity);
    PassVerdict(r[FLUID_READ_VELOCITY], work->f.velocity, v, &out[FLUID_CHECK_ADVECT_VELOCITY]);
    FluidAdvectPass(FLUID_SEPARATE, grid, r[FLUID_READ_PROJECTED], FLUID_DYE_SCALE,
                    FLUID_DYE_CHANNELS, r[FLUID_READ_DYED], FLUID_DYE_KEEP, work->f.dye);
    PassVerdict(r[FLUID_READ_DYE], work->f.dye, d, &out[FLUID_CHECK_ADVECT_DYE]);

    FluidVerdict *const projection = &out[FLUID_CHECK_PROJECTION];
    projection->before = FluidDivergenceRms(grid, r[FLUID_READ_SPLATTED]);
    projection->after = FluidDivergenceRms(grid, r[FLUID_READ_PROJECTED]);
    projection->measure =
        projection->before > 0.0 ? projection->after / projection->before : INFINITY;
    projection->passed = projection->measure <= FLUID_DIVERGENCE_LEFT;

    FluidVerdict *const mass = &out[FLUID_CHECK_MASS];
    mass->after = FluidDyeMass(grid, r[FLUID_READ_DYE]);
    mass->before = budget;
    mass->measure = budget > 0.0 ? fabs(mass->after / budget - 1.0) : INFINITY;
    mass->passed = mass->measure <= FLUID_MASS_OFF;

    FluidVerdict *const sane = &out[FLUID_CHECK_SANE];
    for (uint32_t read = 0; read < FLUID_READS; ++read) {
        FluidSane found;
        FluidSanity(r[read], FluidReadFloats(grid, (FluidRead)read),
                    FluidReadKind((FluidRead)read) == FLUID_KIND_DYE, &found);
        if (found.not_finite + found.negative > 0u && sane->wrong_reads++ == 0u) {
            sane->first_read = (FluidRead)read;
            sane->sane = found;
        }
    }
    sane->passed = sane->wrong_reads == 0u;
}

// The notes, as the game scene's.

static bool Append(char *out, size_t out_size, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

static bool Append(char *out, size_t out_size, const char *fmt, ...) {
    const size_t used = strlen(out);
    if (used + 1u >= out_size) {
        return false;
    }
    va_list args;
    va_start(args, fmt);
    const int wrote = vsnprintf(out + used, out_size - used, fmt, args);
    va_end(args);
    return wrote >= 0 && (size_t)wrote < out_size - used;
}

static void AppendMeasure(char *out, size_t out_size, const char *what,
                          const GameFrameStats *stats) {
    Append(out, out_size, "; %s: mean %u us, 99th percentile %u us, longest %u us at frame %u",
           what, (unsigned)stats->mean, (unsigned)stats->p99, (unsigned)stats->longest,
           (unsigned)stats->longest_at);
}

size_t FluidDescribeFrames(char *out, size_t out_size, const char *suite,
                           const GameTimes *times) {
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';
    if (times == NULL || suite == NULL) {
        return 0;
    }
    const GameFrameStats *const present = &times->present;
    const uint64_t total = present->total + times->warmup_present;
    Append(out, out_size,
           "%s: %u frame%s in %.2f s; frame 0, the warm-up, left out of the times (%u us "
           "present to present, %u us the main thread's part, %u us the render thread's)",
           suite, (unsigned)times->frames, times->frames == 1u ? "" : "s", (double)total / 1e6,
           (unsigned)times->warmup_present, (unsigned)times->warmup_main,
           (unsigned)times->warmup_render);
    if (present->frames == 0u) {
        Append(out, out_size, "; no frame past it to time");
        return strlen(out);
    }
    Append(out, out_size, "; the %u past it", (unsigned)present->frames);
    AppendMeasure(out, out_size, "present to present", present);
    if (present->hitches == 0u) {
        Append(out, out_size, ", none over %u us, two frames at 60 Hz", (unsigned)GAME_HITCH_US);
        return strlen(out);
    }
    Append(out, out_size, ", %u over %u us, two frames at 60 Hz: frame%s",
           (unsigned)present->hitches, (unsigned)GAME_HITCH_US,
           present->hitches == 1u ? "" : "s");
    const uint32_t listed =
        present->hitches < GAME_HITCHES_LISTED ? present->hitches : GAME_HITCHES_LISTED;
    for (uint32_t i = 0; i < listed; ++i) {
        Append(out, out_size, "%s %u", i == 0u ? "" : ",", (unsigned)present->hitch_at[i]);
    }
    if (present->hitches > listed) {
        Append(out, out_size, " and %u more", (unsigned)(present->hitches - listed));
    }
    return strlen(out);
}

size_t FluidDescribeParts(char *out, size_t out_size, const char *suite,
                          const GameTimes *times) {
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';
    if (times == NULL || suite == NULL) {
        return 0;
    }
    Append(out, out_size, "%s: past the warm-up", suite);
    if (times->present.frames == 0u) {
        Append(out, out_size, ", no frame to time");
        return strlen(out);
    }
    AppendMeasure(out, out_size, "the main thread's part", &times->main_part);
    AppendMeasure(out, out_size, "the render thread's part, the graphics unit's in it",
                  &times->render_part);
    return strlen(out);
}
