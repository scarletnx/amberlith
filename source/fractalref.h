// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// The demo family's fractal zooms, worked out on the processor: what each frame asks of the
/// graphics unit, and what it should draw. No libnx, so the host build checks all of it.
///
/// A frame is two passes, the same on any backend that issues them (deko3d here; anything else
/// that binds the same blocks and runs the same programs):
///
///  1. a dispatch of fractal.comp.glsl, FRACTAL_GROUPS_X by FRACTAL_GROUPS_Y groups of 8 by 8,
///     with the frame's FractalFrame as uniform buffer 0 and the counts, FRACTAL_PIXELS words,
///     as storage buffer 0: one word a pixel, the iterations it took;
///  2. once those stores are visible to the fragments, a draw of three vertices with
///     fullscreen.vert.glsl and fractal.frag.glsl, the same uniform buffer and storage buffer,
///     into an RGBA8 target of FRACTAL_WIDTH by FRACTAL_HEIGHT: each pixel its count's colour.
///
/// Every frame is a function of its number. The counts are held to FractalIterate, which repeats
/// the compute program's single precision operations one for one, each rounded once as the
/// console's FFMA, FMUL and FADD round (read out of the compiled program: none of them flushes a
/// denormal there), so a machine that runs the program as written gets every count exactly. The
/// colours are integers to bytes.

#define FRACTAL_WIDTH 640u
#define FRACTAL_HEIGHT 360u
#define FRACTAL_PIXELS (FRACTAL_WIDTH * FRACTAL_HEIGHT)
#define FRACTAL_IMAGE_BYTES (FRACTAL_PIXELS * 4u)
#define FRACTAL_GROUP 8u
#define FRACTAL_GROUPS_X (FRACTAL_WIDTH / FRACTAL_GROUP)
#define FRACTAL_GROUPS_Y (FRACTAL_HEIGHT / FRACTAL_GROUP)

typedef enum {
    FRACTAL_MANDELBROT,
    FRACTAL_JULIA,
    FRACTAL_KINDS,
} FractalKind;

/// The suite each zoom reports under: "mandelbrot" and "julia".
const char *FractalSuite(FractalKind kind);

/// A zoom runs in over FRACTAL_ZOOM_FRAMES frames and back out over as many less two, then in
/// again: frame k shows the view of FractalZoomStep(k), so a run of any length draws a zoom.
#define FRACTAL_ZOOM_FRAMES 600u
#define FRACTAL_ZOOM_CYCLE (2u * (FRACTAL_ZOOM_FRAMES - 1u))

/// How far into the zoom frame @p frame is, from 0 (the widest view) to FRACTAL_ZOOM_FRAMES - 1
/// (the deepest).
uint32_t FractalZoomStep(uint32_t frame);

/// The palette's colours, and how many frames each turn of it by one colour lasts.
#define FRACTAL_PALETTE 64u
#define FRACTAL_PALETTE_FRAMES 2u
/// What a pixel the iteration never lets go shows: opaque black, as the palette's words are
/// written, red in the low byte.
#define FRACTAL_INSIDE 0xff000000u

/// The uniform block both programs read, as std140 lays it out.
typedef struct {
    float centre[2];
    /// The distance in the plane between two neighbouring pixels' centres.
    float step;
    /// The most iterations; a pixel that has not escaped by then keeps this many.
    uint32_t iterations;
    /// c for a Julia set; nought for the Mandelbrot set, which takes c from the pixel.
    float seed[2];
    uint32_t julia;
    /// How far the palette is turned this frame.
    uint32_t shift;
    uint32_t width;
    uint32_t height;
    uint32_t unused[2];
    /// RGBA8 words, red in the low byte, as unpackUnorm4x8 reads them; std140 puts each element of
    /// an array of scalars 16 bytes from the next.
    struct {
        uint32_t word;
        uint32_t unused[3];
    } palette[FRACTAL_PALETTE];
} FractalFrame;

#define FRACTAL_FRAME_BYTES 1072u
_Static_assert(sizeof(FractalFrame) == FRACTAL_FRAME_BYTES, "the std140 block's size");
_Static_assert(offsetof(FractalFrame, seed) == 16u, "std140 puts a vec2 after a uint at 16");
_Static_assert(offsetof(FractalFrame, palette) == 48u, "std140 puts uint palette[64] at 48");
_Static_assert(sizeof(((FractalFrame *)0)->palette[0]) == 16u, "and each of its words 16 apart");

/// Frame @p frame of the @p kind zoom: its view, iterations, palette and shift.
void FractalFrameOf(FractalKind kind, uint32_t frame, FractalFrame *out);

/// The palette, the same every frame: a turning gradient through eight colours.
uint32_t FractalPaletteWord(uint32_t index);

/// The iterations pixel @p x, @p y of @p frame takes, as the compute program counts them.
uint32_t FractalIterate(const FractalFrame *frame, uint32_t x, uint32_t y);

/// The same with every fused multiply and add done as a multiply then an add, each rounded:
/// what a machine that does not fuse them counts, which the judge names when it sees it.
uint32_t FractalIterateApart(const FractalFrame *frame, uint32_t x, uint32_t y);

/// The same in double precision from the same pixel coordinates: the set the float zoom
/// approximates, for measuring how far single precision holds.
uint32_t FractalIterateDouble(const FractalFrame *frame, uint32_t x, uint32_t y);

/// The colour word the fragment program shows for @p count in @p frame.
uint32_t FractalColour(const FractalFrame *frame, uint32_t count);

/// Every pixel's count, FRACTAL_PIXELS words in row order, the top row first.
void FractalCounts(const FractalFrame *frame, uint32_t *counts);

/// The target the colours make of @p counts: FRACTAL_IMAGE_BYTES, four bytes a pixel.
void FractalImage(const FractalFrame *frame, const uint32_t *counts, uint8_t *pixels);

/// The frames a run's verdicts are on, in order: the widest view, four on the way and the
/// deepest. A run that ends or is stopped before one skips its case.
#define FRACTAL_JUDGED 6u
uint32_t FractalJudgedFrame(uint32_t index);

/// FNV-1a over little-endian words, as the game family hashes an image (GameHashWords): the
/// counts as words, the image as its bytes.
uint32_t FractalHash(const void *bytes, size_t count);

/// The hashes the judged frames' counts and images come to, pinned: the reference suite holds
/// FractalCounts and FractalImage to them on a computer, and a run on a machine compares what it
/// read back to them, working the frame out only when it differs.
uint32_t FractalPinnedCounts(FractalKind kind, uint32_t index);
uint32_t FractalPinnedImage(FractalKind kind, uint32_t index);

/// What a judged frame's counts came to against the reference: how many pixels have another
/// count, of those how many hold what a machine that multiplies and adds apart counts, and how
/// many are one iteration off; and the first wrong pixel.
typedef struct {
    uint32_t wrong;
    uint32_t apart;
    uint32_t off_by_one;
    uint32_t first_x;
    uint32_t first_y;
    uint32_t first_got;
    uint32_t first_want;
} FractalCountsVerdict;

/// Compares @p got with @p want, pixel by pixel; @p frame says what a machine that does not fuse
/// would count, which is worked out only at the wrong pixels.
void FractalJudgeCounts(const FractalFrame *frame, const uint32_t *got, const uint32_t *want,
                        FractalCountsVerdict *out);

/// The step of frame @p frame's view in units in the last place of the largest coordinate its
/// pixels reach: how far single precision still tells two neighbouring pixels apart.
double FractalStepUlps(const FractalFrame *frame);

#ifdef __cplusplus
}
#endif
