// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <float.h>
#include <math.h>
#include <string.h>

#include "fractalref.h"
#include "gameref.h"

// Every multiply that meets an add here is an fma written out, in float and in double, so no
// compiler has a product and a sum of its own to fuse or keep apart: the console's build fuses
// what it may (gcc's -ffp-contract=fast outside strict ISO C), a computer's does not, and both
// must give the same bits. The one place that must not fuse, FractalIterateApart, rounds each
// product through a volatile.

const char *FractalSuite(FractalKind kind) {
    return kind == FRACTAL_JULIA ? "julia" : "mandelbrot";
}

uint32_t FractalZoomStep(uint32_t frame) {
    const uint32_t at = frame % FRACTAL_ZOOM_CYCLE;
    return at < FRACTAL_ZOOM_FRAMES ? at : FRACTAL_ZOOM_CYCLE - at;
}

/// Each zoom: where it starts, the point it closes on, how fast, and its iterations.
typedef struct {
    double start[2];
    double target[2];
    /// The step at the widest view, and what each frame multiplies it by.
    double step;
    double ratio;
    float seed[2];
    uint32_t iterations;
    /// One iteration more every this many frames into the zoom, as deeper views need.
    uint32_t iterations_every;
} FractalZoom;

static const FractalZoom ZOOMS[FRACTAL_KINDS] = {
    // Into the valley between the main cardioid and the disc to its left, 2^12 in 599 frames:
    // 2^(-12/599) a frame.
    [FRACTAL_MANDELBROT] = { { -0.6, 0.0 },
                             { -0.743643887037151, 0.131825904205330 },
                             0.005,
                             0x1.f8f0805a57dfbp-1,
                             { 0.0f, 0.0f },
                             128u,
                             2u },
    // c = -0.835 - 0.2321i, into the set's repelling fixed point alpha = (1 - sqrt(1 - 4c)) / 2,
    // where the set is self-similar and spirals in, 2^10 in 599 frames: 2^(-10/599) a frame.
    [FRACTAL_JULIA] = { { 0.0, 0.0 },
                        { -0.5475083268653449, -0.11078670882481491 },
                        0.005,
                        0x1.fa1c02c71115cp-1,
                        { -0.835f, -0.2321f },
                        192u,
                        2u },
};

uint32_t FractalPaletteWord(uint32_t index) {
    // Eight colours, deep blue to white to amber to brown and round again, eight steps between
    // each two; the steps round to the nearest byte.
    static const uint8_t KEYS[8][3] = {
        { 0, 7, 100 },   { 32, 107, 203 }, { 237, 255, 255 }, { 255, 170, 0 },
        { 110, 30, 10 }, { 0, 2, 40 },     { 20, 80, 160 },   { 120, 200, 230 },
    };
    const uint32_t at = index % FRACTAL_PALETTE;
    const uint8_t *const from = KEYS[at / 8u];
    const uint8_t *const to = KEYS[(at / 8u + 1u) % 8u];
    const uint32_t t = at % 8u;
    uint32_t word = 0xff000000u;
    for (uint32_t c = 0; c < 3u; ++c) {
        const uint32_t value = ((uint32_t)from[c] * (8u - t) + (uint32_t)to[c] * t + 4u) / 8u;
        word |= value << (8u * c);
    }
    return word;
}

void FractalFrameOf(FractalKind kind, uint32_t frame, FractalFrame *out) {
    memset(out, 0, sizeof(*out));
    const FractalZoom *const zoom = &ZOOMS[kind == FRACTAL_JULIA ? FRACTAL_JULIA : 0];
    const uint32_t step = FractalZoomStep(frame);
    // The view's scale, ratio^step, by repeated products: no library function, so every machine
    // gets the same bits.
    double scale = 1.0;
    for (uint32_t i = 0; i < step; ++i) {
        scale *= zoom->ratio;
    }
    // The centre closes on the target as the square of the scale, so the target drifts to the
    // middle of the screen while the view shrinks around it.
    const double pull = scale * scale;
    out->centre[0] = (float)fma(zoom->start[0] - zoom->target[0], pull, zoom->target[0]);
    out->centre[1] = (float)fma(zoom->start[1] - zoom->target[1], pull, zoom->target[1]);
    out->step = (float)(zoom->step * scale);
    out->iterations = zoom->iterations + step / zoom->iterations_every;
    out->seed[0] = zoom->seed[0];
    out->seed[1] = zoom->seed[1];
    out->julia = kind == FRACTAL_JULIA ? 1u : 0u;
    out->shift = (frame / FRACTAL_PALETTE_FRAMES) % FRACTAL_PALETTE;
    out->width = FRACTAL_WIDTH;
    out->height = FRACTAL_HEIGHT;
    for (uint32_t i = 0; i < FRACTAL_PALETTE; ++i) {
        out->palette[i].word = FractalPaletteWord(i);
    }
}

/// The pixel's place in the plane, as the compute program works it out: its centre's distance
/// from the middle of the target, exact, times the step and added to the centre in one rounding.
static void Place(const FractalFrame *frame, uint32_t x, uint32_t y, float *ax, float *ay) {
    const float px = (float)x - ((float)(frame->width / 2u) - 0.5f);
    const float py = (float)y - ((float)(frame->height / 2u) - 0.5f);
    *ax = fmaf(px, frame->step, frame->centre[0]);
    *ay = fmaf(py, -frame->step, frame->centre[1]);
}

uint32_t FractalIterate(const FractalFrame *frame, uint32_t x, uint32_t y) {
    float ax = 0.0f;
    float ay = 0.0f;
    Place(frame, x, y, &ax, &ay);
    const bool julia = frame->julia != 0u;
    float zx = julia ? ax : 0.0f;
    float zy = julia ? ay : 0.0f;
    const float cx = julia ? frame->seed[0] : ax;
    const float cy = julia ? frame->seed[1] : ay;
    uint32_t n = 0;
    for (; n < frame->iterations; ++n) {
        const float yy = zy * zy;
        const float r2 = fmaf(zx, zx, yy);
        if (r2 > 4.0f) {
            break;
        }
        const float t = fmaf(-zy, zy, cx);
        const float nx = fmaf(zx, zx, t);
        const float ny = fmaf(zx + zx, zy, cy);
        zx = nx;
        zy = ny;
    }
    return n;
}

/// A product rounded on its own, which no compiler can fuse with the sum it feeds.
static float Product(float a, float b) {
    volatile float p = a * b;
    return p;
}

uint32_t FractalIterateApart(const FractalFrame *frame, uint32_t x, uint32_t y) {
    const float px = (float)x - ((float)(frame->width / 2u) - 0.5f);
    const float py = (float)y - ((float)(frame->height / 2u) - 0.5f);
    const float ax = Product(px, frame->step) + frame->centre[0];
    const float ay = Product(py, -frame->step) + frame->centre[1];
    const bool julia = frame->julia != 0u;
    float zx = julia ? ax : 0.0f;
    float zy = julia ? ay : 0.0f;
    const float cx = julia ? frame->seed[0] : ax;
    const float cy = julia ? frame->seed[1] : ay;
    uint32_t n = 0;
    for (; n < frame->iterations; ++n) {
        const float yy = Product(zy, zy);
        const float r2 = Product(zx, zx) + yy;
        if (r2 > 4.0f) {
            break;
        }
        const float t = cx - yy;
        const float nx = Product(zx, zx) + t;
        const float ny = Product(zx + zx, zy) + cy;
        zx = nx;
        zy = ny;
    }
    return n;
}

uint32_t FractalIterateDouble(const FractalFrame *frame, uint32_t x, uint32_t y) {
    const double px = (double)x - ((double)(frame->width / 2u) - 0.5);
    const double py = (double)y - ((double)(frame->height / 2u) - 0.5);
    const double ax = fma(px, (double)frame->step, (double)frame->centre[0]);
    const double ay = fma(py, -(double)frame->step, (double)frame->centre[1]);
    const bool julia = frame->julia != 0u;
    double zx = julia ? ax : 0.0;
    double zy = julia ? ay : 0.0;
    const double cx = julia ? (double)frame->seed[0] : ax;
    const double cy = julia ? (double)frame->seed[1] : ay;
    uint32_t n = 0;
    for (; n < frame->iterations; ++n) {
        const double yy = zy * zy;
        if (fma(zx, zx, yy) > 4.0) {
            break;
        }
        const double nx = fma(zx, zx, fma(-zy, zy, cx));
        const double ny = fma(zx + zx, zy, cy);
        zx = nx;
        zy = ny;
    }
    return n;
}

uint32_t FractalColour(const FractalFrame *frame, uint32_t count) {
    if (count >= frame->iterations) {
        return FRACTAL_INSIDE;
    }
    return frame->palette[(count + frame->shift) & (FRACTAL_PALETTE - 1u)].word;
}

void FractalCounts(const FractalFrame *frame, uint32_t *counts) {
    for (uint32_t y = 0; y < frame->height; ++y) {
        for (uint32_t x = 0; x < frame->width; ++x) {
            counts[(size_t)y * frame->width + x] = FractalIterate(frame, x, y);
        }
    }
}

void FractalImage(const FractalFrame *frame, const uint32_t *counts, uint8_t *pixels) {
    const size_t count = (size_t)frame->width * frame->height;
    for (size_t i = 0; i < count; ++i) {
        const uint32_t word = FractalColour(frame, counts[i]);
        pixels[i * 4u] = (uint8_t)word;
        pixels[i * 4u + 1u] = (uint8_t)(word >> 8u);
        pixels[i * 4u + 2u] = (uint8_t)(word >> 16u);
        pixels[i * 4u + 3u] = (uint8_t)(word >> 24u);
    }
}

uint32_t FractalJudgedFrame(uint32_t index) {
    static const uint32_t FRAMES[FRACTAL_JUDGED] = { 0u, 120u, 240u, 360u, 480u, 599u };
    return index < FRACTAL_JUDGED ? FRAMES[index] : 0u;
}

uint32_t FractalHash(const void *bytes, size_t count) {
    return GameHashWords((const uint8_t *)bytes, count);
}

// FractalCounts and FractalImage's hashes at each judged frame, as the reference suite printed
// them on a computer; it fails, naming the hash it got, the day the arithmetic above changes.
static const uint32_t PINNED_COUNTS[FRACTAL_KINDS][FRACTAL_JUDGED] = {
    [FRACTAL_MANDELBROT] = { 0x3b946559u, 0x2e58b221u, 0x4bb33774u, 0x7c021e51u, 0xf90bece5u,
                             0xd89d3dafu },
    [FRACTAL_JULIA] = { 0xf1e78277u, 0xf2c65dc7u, 0x0eacef8bu, 0x8ad08b0eu, 0xf767a264u,
                        0xe20f7aa6u },
};
static const uint32_t PINNED_IMAGE[FRACTAL_KINDS][FRACTAL_JUDGED] = {
    [FRACTAL_MANDELBROT] = { 0x54e68c81u, 0x16ce9e37u, 0xb0d30e26u, 0xb66e5007u, 0x96a6553du,
                             0x3db3c218u },
    [FRACTAL_JULIA] = { 0x83b2d097u, 0x8954b85eu, 0xc20c1250u, 0xfd9e6a16u, 0x2df2376au,
                        0x6b9921afu },
};

uint32_t FractalPinnedCounts(FractalKind kind, uint32_t index) {
    return kind < FRACTAL_KINDS && index < FRACTAL_JUDGED ? PINNED_COUNTS[kind][index] : 0u;
}

uint32_t FractalPinnedImage(FractalKind kind, uint32_t index) {
    return kind < FRACTAL_KINDS && index < FRACTAL_JUDGED ? PINNED_IMAGE[kind][index] : 0u;
}

void FractalJudgeCounts(const FractalFrame *frame, const uint32_t *got, const uint32_t *want,
                        FractalCountsVerdict *out) {
    memset(out, 0, sizeof(*out));
    for (uint32_t y = 0; y < frame->height; ++y) {
        for (uint32_t x = 0; x < frame->width; ++x) {
            const size_t at = (size_t)y * frame->width + x;
            if (got[at] == want[at]) {
                continue;
            }
            if (out->wrong++ == 0u) {
                out->first_x = x;
                out->first_y = y;
                out->first_got = got[at];
                out->first_want = want[at];
            }
            out->apart += got[at] == FractalIterateApart(frame, x, y) ? 1u : 0u;
            out->off_by_one += got[at] + 1u == want[at] || want[at] + 1u == got[at] ? 1u : 0u;
        }
    }
}

double FractalStepUlps(const FractalFrame *frame) {
    // The farthest corner's coordinates, each the largest magnitude a pixel of the frame takes.
    const double half_x = ((double)(frame->width / 2u) - 0.5) * (double)frame->step;
    const double half_y = ((double)(frame->height / 2u) - 0.5) * (double)frame->step;
    const double reach_x = fabs((double)frame->centre[0]) + half_x;
    const double reach_y = fabs((double)frame->centre[1]) + half_y;
    const double reach = reach_x > reach_y ? reach_x : reach_y;
    // A float's unit in the last place at that magnitude: 2^(exponent - 23).
    int exponent = 0;
    (void)frexp(reach, &exponent);
    return (double)frame->step / ldexp(1.0, exponent - 1 - 23);
}
