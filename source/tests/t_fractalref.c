// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../check.h"
#include "../fractalref.h"
#include "../report.h"
#include "tests.h"

// The fractal zooms' arithmetic against answers worked out by hand: their frames, their
// iteration and their palette. On a computer also the judged
// frames whole, against the hashes a run on a machine is held to, and how far single precision
// holds the zoom; those work out frames of 230400 pixels, so the console leaves them to the
// computer.

static const char *const SUITE = "reference";

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

static FractalFrame block;
static FractalFrame other;

static void CheckFrameBlock(void) {
    Tally t = { 0, NULL };
    FractalFrameOf(FRACTAL_MANDELBROT, 0u, &block);
    const uint8_t *const bytes = (const uint8_t *)&block;
    uint32_t word = 0;
    // std140 as both programs declare the block: step at 8, iterations at 12, seed at 16, the
    // palette's first word at 48.
    memcpy(&word, bytes + 8, 4);
    Expect(&t, word == 0x3ba3d70au, "the Mandelbrot's first step, 0.005 as a float, at byte 8");
    memcpy(&word, bytes + 12, 4);
    Expect(&t, word == 128u, "its 128 iterations at byte 12");
    memcpy(&word, bytes + 48, 4);
    Expect(&t, word == 0xff640700u, "the palette's first colour, 0 7 100, at byte 48");
    memcpy(&word, bytes + 64, 4);
    Expect(&t, word == 0xff711404u, "its second, 4 20 113, 16 bytes on");
    Expect(&t, block.centre[0] == -0.6f && block.centre[1] == 0.0f, "its first centre -0.6, 0");
    Expect(&t, block.julia == 0u && block.seed[0] == 0.0f && block.shift == 0u,
           "no seed and no turn at frame 0");
    Expect(&t, block.width == 640u && block.height == 360u, "the target 640 by 360");
    FractalFrameOf(FRACTAL_JULIA, 0u, &block);
    memcpy(&word, bytes + 16, 4);
    Expect(&t, word == 0xbf55c28fu, "the Julia set's seed, -0.835 as a float, at byte 16");
    Expect(&t, block.julia == 1u && block.iterations == 192u && block.step == 0.005f &&
                   block.centre[0] == 0.0f && block.centre[1] == 0.0f &&
                   block.seed[1] == -0.2321f,
           "the Julia zoom's first frame: centre 0, step 0.005, 192 iterations, seed's -0.2321");
    Report("a fractal frame's uniform block is laid out as both programs read it", &t);
}

static void CheckZoomPath(void) {
    Tally t = { 0, NULL };
    Expect(&t,
           FractalZoomStep(0u) == 0u && FractalZoomStep(599u) == 599u &&
               FractalZoomStep(600u) == 598u && FractalZoomStep(1197u) == 1u &&
               FractalZoomStep(1198u) == 0u && FractalZoomStep(1199u) == 1u,
           "in over 600 frames, out over 598, in again");
    FractalFrameOf(FRACTAL_MANDELBROT, 600u, &block);
    FractalFrameOf(FRACTAL_MANDELBROT, 598u, &other);
    Expect(&t,
           block.step == other.step && block.centre[0] == other.centre[0] &&
               block.centre[1] == other.centre[1] && block.iterations == other.iterations,
           "frame 600 is frame 598's view");
    Expect(&t, block.shift == 44u && other.shift == 43u, "turned by 300 and 299 colours mod 64");
    FractalFrameOf(FRACTAL_MANDELBROT, 127u, &block);
    FractalFrameOf(FRACTAL_MANDELBROT, 128u, &other);
    Expect(&t, block.shift == 63u && other.shift == 0u, "the palette turns once in 128 frames");
    FractalFrameOf(FRACTAL_MANDELBROT, 599u, &block);
    Expect(&t, block.iterations == 128u + 299u, "the deepest Mandelbrot frame iterates 427 times");
    // Each frame's step a little under the one before, in over 599 frames: 2^-12 of the first
    // in all, to the last unit the 599 products leave.
    FractalFrameOf(FRACTAL_MANDELBROT, 0u, &other);
    const double ratio = (double)block.step / (double)other.step;
    Expect(&t, ratio > 0.99999 / 4096.0 && ratio < 1.00001 / 4096.0,
           "the Mandelbrot zoom shrinks its step 4096 times");
    FractalFrameOf(FRACTAL_JULIA, 599u, &block);
    FractalFrameOf(FRACTAL_JULIA, 0u, &other);
    const double julia_ratio = (double)block.step / (double)other.step;
    Expect(&t, julia_ratio > 0.99999 / 1024.0 && julia_ratio < 1.00001 / 1024.0,
           "the Julia zoom shrinks its step 1024 times");
    Report("the zooms go in for 600 frames, back out and in again, the palette turning", &t);
}

static void CheckPrecision(void) {
    Tally t = { 0, NULL };
    double least[FRACTAL_KINDS] = { 1e30, 1e30 };
    for (uint32_t kind = 0; kind < FRACTAL_KINDS; ++kind) {
        for (uint32_t k = 0; k < FRACTAL_ZOOM_FRAMES; ++k) {
            FractalFrameOf((FractalKind)kind, k, &block);
            const double ulps = FractalStepUlps(&block);
            least[kind] = ulps < least[kind] ? ulps : least[kind];
        }
    }
    // A pixel's step at least 16 floats apart wherever it lands, and the Mandelbrot zoom close to
    // that: where single precision stops telling neighbours apart is where the zoom ends.
    Expect(&t, least[FRACTAL_MANDELBROT] >= 16.0 && least[FRACTAL_MANDELBROT] < 32.0,
           "the Mandelbrot zoom ends between 16 and 32 units in the last place");
    Expect(&t, least[FRACTAL_JULIA] >= 16.0, "the Julia zoom keeps 16 units in the last place");
    CHECK_TRUE(SUITE, "every frame of both zooms keeps neighbouring pixels 16 floats apart",
               CHECK_NONE, t.wrong == 0u,
               "the least, in units in the last place of the farthest coordinate: Mandelbrot "
               "%.2f, Julia %.2f; %s",
               least[FRACTAL_MANDELBROT], least[FRACTAL_JULIA],
               t.first != NULL ? t.first : "both hold");
}

static void CheckIterate(void) {
    Tally t = { 0, NULL };
    memset(&block, 0, sizeof(block));
    block.width = FRACTAL_WIDTH;
    block.height = FRACTAL_HEIGHT;
    block.iterations = 100u;
    block.step = 1.0f;
    // Pixel 320, 180 sits half a step right of and below the middle: c = 0.5 - 0.5i. Its orbit in
    // halves and quarters, every value exact in a float: |z|^2 is 0.5, 1.25, 2.3125, 2.91,
    // then 12.6, past 4 at the sixth look, so 5.
    Expect(&t, FractalIterate(&block, 320u, 180u) == 5u, "c = 0.5 - 0.5i escapes after 5");
    Expect(&t, FractalIterateApart(&block, 320u, 180u) == 5u,
           "the same apart, where no product rounds");
    Expect(&t, FractalIterateDouble(&block, 320u, 180u) == 5u, "the same in double precision");
    // Every pixel at c = 0 with a step of nought: z stays nought, never escapes.
    block.step = 0.0f;
    Expect(&t, FractalIterate(&block, 7u, 9u) == 100u, "c = 0 keeps the most iterations");
    // A Julia set of c = 0 is the unit circle: z = 0.5 - 0.5i squares towards nought, and
    // 2.5 + 2.5i starts past 2.
    block.julia = 1u;
    block.step = 1.0f;
    Expect(&t, FractalIterate(&block, 320u, 180u) == 100u, "inside the unit circle stays");
    block.step = 5.0f;
    Expect(&t, FractalIterate(&block, 320u, 179u) == 0u, "2.5 + 2.5i is out at once");
    Report("the iteration counts escapes worked out by hand", &t);
}

static void CheckPalette(void) {
    Tally t = { 0, NULL };
    // Eight steps from 0 7 100 to 32 107 203, each the nearest byte: the fourth is 16, 57 and
    // 151.5, the half rounding up as (sum + 4) / 8 does, to 152.
    Expect(&t, FractalPaletteWord(0u) == 0xff640700u, "colour 0 is 0 7 100");
    Expect(&t, FractalPaletteWord(4u) == 0xff983910u, "colour 4 is 16 57 152");
    Expect(&t, FractalPaletteWord(8u) == 0xffcb6b20u, "colour 8 is 32 107 203");
    Expect(&t, FractalPaletteWord(64u) == FractalPaletteWord(0u), "64 colours, then round again");
    FractalFrameOf(FRACTAL_MANDELBROT, 6u, &block);
    Expect(&t, block.shift == 3u, "frame 6 turns the palette by 3");
    Expect(&t, FractalColour(&block, 0u) == FractalPaletteWord(3u), "count 0 shows colour 3");
    Expect(&t, FractalColour(&block, 62u) == FractalPaletteWord(1u), "count 62 shows colour 1");
    Expect(&t, FractalColour(&block, block.iterations) == FRACTAL_INSIDE,
           "the most iterations shows black");
    uint32_t counts[2] = { 0u, block.iterations };
    uint8_t pixels[8];
    block.width = 2u;
    block.height = 1u;
    FractalImage(&block, counts, pixels);
    const uint32_t colour = FractalPaletteWord(3u);
    Expect(&t,
           pixels[0] == (uint8_t)colour && pixels[1] == (uint8_t)(colour >> 8) &&
               pixels[2] == (uint8_t)(colour >> 16) && pixels[3] == 0xffu && pixels[4] == 0u &&
               pixels[5] == 0u && pixels[6] == 0u && pixels[7] == 0xffu,
           "the image's bytes red first, black opaque");
    Report("a count's colour is the turned palette's, black where it never escaped", &t);
}

#ifdef AMBERLITH_HOST
static uint32_t counts[FRACTAL_PIXELS];
static uint8_t image[FRACTAL_IMAGE_BYTES];

/// The judged frames, worked out whole, against the hashes a machine's run is held to.
static void CheckPinned(FractalKind kind) {
    char name[96];
    snprintf(name, sizeof(name), "the %s zoom's judged frames come to their pinned hashes",
             FractalSuite(kind));
    char got[FRACTAL_JUDGED * 24u] = "";
    uint32_t wrong = 0;
    for (uint32_t j = 0; j < FRACTAL_JUDGED; ++j) {
        FractalFrameOf(kind, FractalJudgedFrame(j), &block);
        FractalCounts(&block, counts);
        FractalImage(&block, counts, image);
        const uint32_t counts_hash = FractalHash(counts, sizeof(counts));
        const uint32_t image_hash = FractalHash(image, sizeof(image));
        wrong += counts_hash != FractalPinnedCounts(kind, j) ? 1u : 0u;
        wrong += image_hash != FractalPinnedImage(kind, j) ? 1u : 0u;
        const size_t used = strlen(got);
        snprintf(got + used, sizeof(got) - used, "%s0x%08xu/0x%08xu", j > 0u ? " " : "",
                 (unsigned)counts_hash, (unsigned)image_hash);
    }
    CHECK_TRUE(SUITE, name, CHECK_NONE, wrong == 0u,
               "%u of %u hashes differ; counts and image of each judged frame: %s",
               (unsigned)wrong, (unsigned)(2u * FRACTAL_JUDGED), got);
}

/// How far single precision holds, and what a machine that does not fuse would draw: at the
/// widest and the deepest frame, the pixels whose count differs in double precision, and those a
/// machine that multiplies and adds apart counts otherwise. The deepest frame's numbers are the
/// family's page's.
static void CheckLimits(FractalKind kind, const uint32_t want_double[2],
                        const uint32_t want_apart[2]) {
    uint32_t in_double[2] = { 0u, 0u };
    uint32_t apart[2] = { 0u, 0u };
    for (uint32_t i = 0; i < 2u; ++i) {
        FractalFrameOf(kind, i == 0u ? 0u : FRACTAL_ZOOM_FRAMES - 1u, &block);
        for (uint32_t y = 0; y < FRACTAL_HEIGHT; ++y) {
            for (uint32_t x = 0; x < FRACTAL_WIDTH; ++x) {
                const uint32_t n = FractalIterate(&block, x, y);
                in_double[i] += FractalIterateDouble(&block, x, y) != n ? 1u : 0u;
                apart[i] += FractalIterateApart(&block, x, y) != n ? 1u : 0u;
            }
        }
    }
    char name[120];
    snprintf(name, sizeof(name),
             "the %s zoom in double precision and multiplied and added apart: the pixels that "
             "differ",
             FractalSuite(kind));
    CHECK_TRUE(SUITE, name, CHECK_NONE,
               in_double[0] == want_double[0] && in_double[1] == want_double[1] &&
                   apart[0] == want_apart[0] && apart[1] == want_apart[1],
               "of %u pixels, at frame 0 and frame %u: %u and %u in double precision (want %u "
               "and %u), %u and %u apart (want %u and %u)",
               (unsigned)FRACTAL_PIXELS, (unsigned)(FRACTAL_ZOOM_FRAMES - 1u),
               (unsigned)in_double[0], (unsigned)in_double[1], (unsigned)want_double[0],
               (unsigned)want_double[1], (unsigned)apart[0], (unsigned)apart[1],
               (unsigned)want_apart[0], (unsigned)want_apart[1]);
}
#endif

void RunReferenceFractal(void) {
    CheckFrameBlock();
    CheckZoomPath();
    CheckPrecision();
    CheckIterate();
    CheckPalette();
#ifdef AMBERLITH_HOST
    CheckPinned(FRACTAL_MANDELBROT);
    CheckPinned(FRACTAL_JULIA);
    static const uint32_t MANDELBROT_DOUBLE[2] = { 210u, 54036u };
    static const uint32_t MANDELBROT_APART[2] = { 234u, 51140u };
    static const uint32_t JULIA_DOUBLE[2] = { 694u, 5893u };
    static const uint32_t JULIA_APART[2] = { 774u, 6250u };
    CheckLimits(FRACTAL_MANDELBROT, MANDELBROT_DOUBLE, MANDELBROT_APART);
    CheckLimits(FRACTAL_JULIA, JULIA_DOUBLE, JULIA_APART);
#endif
}
