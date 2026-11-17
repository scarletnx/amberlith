// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../astcref.h"
#include "../check.h"
#include "../gameref.h"
#include "../report.h"
#include "../wimsymesh.h"
#include "../wimsyref.h"
#include "tests.h"

// The Wimsy levels' reference against answers worked out by hand, and its judge against machines
// made to order: the one that draws a level as asked has to pass, and each machine wrong in one
// way a real one can be has to be refused, so a frame the wimsy suite calls wrong is the
// machine's and a frame it passes was looked at.

static const char *const SUITE = "reference";

static uint8_t frame[WIMSY_IMAGE_BYTES];
static uint8_t verdicts[WIMSY_PIXELS];
static uint8_t texels[WIMSY_SPRITE_BYTES];
static GameWorld world;
static WimsySprite sprites[WIMSY_SPRITES];

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

static const uint8_t *Pixel(uint32_t x, uint32_t y) {
    return frame + ((size_t)y * WIMSY_WIDTH + x) * 4u;
}

static bool Is(const uint8_t *pixel, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return pixel[0] == r && pixel[1] == g && pixel[2] == b && pixel[3] == a;
}

static void CheckTurn(void) {
    Tally t = { 0, NULL };
    int32_t s = 0, c = 0;
    WimsySinCos(0u, &s, &c);
    Expect(&t, s == 0 && c == 1 << 30, "no turn");
    WimsySinCos(16384u, &s, &c);
    Expect(&t, s == 1 << 30 && c == 0, "a quarter turn");
    WimsySinCos(32768u, &s, &c);
    Expect(&t, s == 0 && c == -(1 << 30), "half a turn");
    WimsySinCos(49152u, &s, &c);
    Expect(&t, s == -(1 << 30) && c == 0, "three quarters");
    WimsySinCos(65536u + 16384u, &s, &c);
    Expect(&t, s == 1 << 30 && c == 0, "a turn and a quarter is a quarter");
    // Every 251st of the 65536, against the C library's, to 2^-24.
    for (uint32_t turn = 0; turn < 65536u; turn += 251u) {
        WimsySinCos(turn, &s, &c);
        const double angle = (double)turn * 6.283185307179586476925 / 65536.0;
        Expect(&t, fabs((double)s / 1073741824.0 - sin(angle)) < 1.0 / 16777216.0,
               "a sine 2^-24 from the library's");
        Expect(&t, fabs((double)c / 1073741824.0 - cos(angle)) < 1.0 / 16777216.0,
               "a cosine 2^-24 from the library's");
    }
    // The two swap an eighth of a turn in: either side of it they mirror each other.
    int32_t s_after = 0, c_after = 0;
    WimsySinCos(8192u - 5u, &s, &c);
    WimsySinCos(8192u + 5u, &s_after, &c_after);
    Expect(&t, s == c_after && c == s_after, "the swap at an eighth of a turn");
    Report("wimsy: the model's turn is the true sine and cosine to 2^-24, in whole numbers", &t);
}

static void CheckJudgedFrames(void) {
    Tally t = { 0, NULL };
    uint32_t judged[WIMSY_JUDGED];
    Expect(&t, WimsyJudgedFrames(600u, judged) == 5u && judged[0] == 0u && judged[1] == 149u &&
                   judged[2] == 299u && judged[3] == 449u && judged[4] == 599u,
           "600 frames: 0, 149, 299, 449 and 599");
    Expect(&t, WimsyJudgedFrames(1u, judged) == 1u && judged[0] == 0u, "one frame: itself");
    Expect(&t, WimsyJudgedFrames(2u, judged) == 2u && judged[0] == 0u && judged[1] == 1u,
           "two frames: both");
    Expect(&t, WimsyJudgedFrames(3u, judged) == 3u && judged[2] == 2u, "three frames: all three");
    Expect(&t, WimsyJudgedFrames(5u, judged) == 5u && judged[1] == 1u && judged[4] == 4u,
           "five frames: all five");
    Expect(&t, WimsyJudgedFrames(36000u, judged) == 5u && judged[4] == 35999u,
           "36000 frames: the last is 35999");
    Expect(&t, WimsyJudgedFrames(0u, judged) == 0u, "no frames: none");
    Expect(&t, WimsyPathAt(0u, 600u) == 0u && WimsyPathAt(599u, 600u) == 65536u &&
                   WimsyPathAt(0u, 1u) == 0u,
           "the path starts at nought and ends at 65536");
    Expect(&t, WimsyPathAt(1u, 3u) == 32768u, "the middle of three frames is half way");
    Report("wimsy: a run's judged frames are its first, its quarters and its last, along a path "
           "that ends with the run",
           &t);
}

/// The hash of an image or a texture, the game scene's.
static uint32_t Hash(const uint8_t *bytes, size_t count) {
    return GameHashWords(bytes, count);
}

static const uint8_t *Texel(uint32_t x, uint32_t y) {
    return texels + ((size_t)y * WIMSY_SPRITE + x) * 4u;
}

static void CheckSprite(void) {
    Tally t = { 0, NULL };
    WimsySpriteTexels(texels);
    // The drawing's leaves where they are in it, by the eighth of the picture: the dark leaf
    // at the top, the two lower ones on the left, the head on the right with its eyes.
    Expect(&t, Is(Texel(4, 4), 0, 0, 0, 0) && Is(Texel(124, 124), 0, 0, 0, 0),
           "the corners are clear");
    Expect(&t, Is(Texel(46, 30), 0x0c, 0x86, 0x94, 255), "the dark leaf at (46,30)");
    Expect(&t, Is(Texel(66, 30), 0x11, 0xce, 0xa1, 255), "the tall leaf at (66,30)");
    Expect(&t, Is(Texel(24, 78), 0x11, 0xce, 0x87, 255), "the left leaf at (24,78)");
    Expect(&t, Is(Texel(36, 104), 0x11, 0xce, 0x87, 255), "the lower leaf at (36,104)");
    Expect(&t, Is(Texel(100, 72), 0x3a, 0xe8, 0xef, 255), "the head at (100,72)");
    Expect(&t, Is(Texel(106, 58), 255, 255, 255, 255), "an eye's white at (106,58)");
    Expect(&t, Is(Texel(88, 70), 0, 0, 0, 255), "an eye's black at (88,70)");
    uint32_t opaque = 0, other = 0;
    for (uint32_t i = 0; i < WIMSY_SPRITE * WIMSY_SPRITE; ++i) {
        opaque += texels[i * 4u + 3u] == 255u ? 1u : 0u;
        other += texels[i * 4u + 3u] != 255u && texels[i * 4u + 3u] != 0u ? 1u : 0u;
    }
    // A third of the picture is her: 5,000 to 6,000 of the 16,384 texels.
    Expect(&t, other == 0u && opaque > 5000u && opaque < 6000u,
           "every texel opaque or clear, a third of them her");
    Expect(&t, Hash(texels, WIMSY_SPRITE_BYTES) == 0x289c0eedu, "the texture's hash");
    Report("wimsy: the sprite is the model seen from the front, the drawing's colours where the "
           "drawing has them",
           &t);
}

static void CheckSprites(void) {
    Tally t = { 0, NULL };
    WimsySpriteTexels(texels);
    // One sprite at (100, 50): a texel a pixel, the clear ones leaving the target's colour.
    const WimsySprite one = { 100, 50, WIMSY_SPRITE, false };
    WimsySpritesImage(&one, 1u, texels, frame);
    Expect(&t, memcmp(Pixel(0, 0), WIMSY_CLEAR, 4u) == 0 &&
                   memcmp(Pixel(104, 54), WIMSY_CLEAR, 4u) == 0,
           "the target's colour outside her");
    Expect(&t, Is(Pixel(200, 122), 0x3a, 0xe8, 0xef, 255), "one sprite: the head at (200,122)");
    // Mirrored, the head is on the left: texel 100 is read at column 127 - 100.
    const WimsySprite mirrored = { 100, 50, WIMSY_SPRITE, true };
    WimsySpritesImage(&mirrored, 1u, texels, frame);
    Expect(&t, Is(Pixel(127, 122), 0x3a, 0xe8, 0xef, 255) &&
                   !Is(Pixel(200, 122), 0x3a, 0xe8, 0xef, 255),
           "mirrored: the head at (127,122)");
    // Twice the size, two pixels a texel: texel (100,72) covers (300..301, 194..195).
    const WimsySprite doubled = { 100, 50, WIMSY_SPRITE * 2u, false };
    WimsySpritesImage(&doubled, 1u, texels, frame);
    Expect(&t, Is(Pixel(300, 194), 0x3a, 0xe8, 0xef, 255) &&
                   Is(Pixel(301, 195), 0x3a, 0xe8, 0xef, 255) &&
                   Is(Pixel(312, 166), 255, 255, 255, 255) && Is(Pixel(313, 167), 255, 255, 255, 255),
           "doubled: each texel four pixels");
    // A later sprite over an earlier one, its clear texels leaving the earlier one's: the
    // second, 20 to the right, puts its dark leaf's texel (46,30) on the first one's tall leaf.
    const WimsySprite two[2] = { { 100, 50, WIMSY_SPRITE, false }, { 120, 50, WIMSY_SPRITE, false } };
    WimsySpritesImage(two, 2u, texels, frame);
    Expect(&t, Is(Pixel(166, 80), 0x0c, 0x86, 0x94, 255), "the later sprite is on top");
    Expect(&t, Is(Pixel(124, 128), 0x11, 0xce, 0x87, 255),
           "the earlier one shows through the later one's clear texels");
    // Hanging off the target's four edges: the part inside is drawn, nothing is written outside.
    const WimsySprite cut[4] = { { -100, 50, WIMSY_SPRITE, false },
                                 { (int32_t)WIMSY_WIDTH - 28, 50, WIMSY_SPRITE, false },
                                 { 300, -72, WIMSY_SPRITE, false },
                                 { 300, (int32_t)WIMSY_HEIGHT - 72, WIMSY_SPRITE, false } };
    WimsySpritesImage(cut, 4u, texels, frame);
    Expect(&t, Is(Pixel(0, 122), 0x3a, 0xe8, 0xef, 255), "cut by the left edge: texel (100,72)");
    Expect(&t, Is(Pixel(WIMSY_WIDTH - 4u, 128), 0x11, 0xce, 0x87, 255),
           "cut by the right edge: texel (24,78)");
    Expect(&t, Is(Pixel(400, 0), 0x3a, 0xe8, 0xef, 255), "cut by the top edge: texel (100,72)");
    Expect(&t, Is(Pixel(346, WIMSY_HEIGHT - 42u), 0x0c, 0x86, 0x94, 255),
           "cut by the bottom edge: texel (46,30)");
    // The frame's sprites from the world: the first cell's is doubled and not mirrored, the
    // fifth's mirrored, each corner where the cell's place puts it.
    GameWorldStart(&world);
    WimsySpritesOf(&world, sprites);
    const int32_t x0 = (int32_t)(((int64_t)world.x[0] * 1408) >> 24) - 128;
    const int32_t y5 = (int32_t)(((int64_t)world.y[5] * 848) >> 24) - 128;
    Expect(&t, sprites[0].side == 256u && !sprites[0].mirrored && sprites[0].x == x0 &&
                   sprites[1].side == 128u && sprites[4].side == 256u && sprites[4].mirrored &&
                   sprites[5].mirrored && sprites[5].y == y5 && !sprites[8].mirrored,
           "the world's sprites: every fourth doubled, every other four mirrored");
    bool inside = true;
    for (uint32_t d = 0; d < WIMSY_SPRITES; ++d) {
        inside = inside && sprites[d].x >= -128 && sprites[d].x < (int32_t)WIMSY_WIDTH &&
                 sprites[d].y >= -128 && sprites[d].y < (int32_t)WIMSY_HEIGHT;
    }
    Expect(&t, inside, "every sprite's corner within a sprite of the target");
    Report("wimsy: sprites cover one another in order, mirrored, doubled and cut by the target's "
           "edges",
           &t);
}

/// The verdict on frame @p at of @p frames at @p level as @p machine draws it.
static WimsyVerdict Verdict(uint32_t level, uint32_t at, uint32_t frames,
                            const WimsyMachine *machine) {
    WimsyVerdict verdict;
    WimsyRender(level, at, frames, machine, frame);
    WimsyJudge(level, at, frames, frame, NULL, verdicts, &verdict);
    return verdict;
}

static void CheckSpriteFrames(void) {
    Tally t = { 0, NULL };
    const WimsyMachine right = WimsyMachineRight(0u);
    WimsyVerdict verdict = Verdict(0u, 0u, 600u, &right);
    Expect(&t, WimsyVerdictPasses(&verdict) && verdict.exact && verdict.held == WIMSY_PIXELS &&
                   verdict.edge == 0u,
           "frame 0 as asked: every pixel held, bit for bit");
    Expect(&t, Hash(frame, WIMSY_IMAGE_BYTES) == 0x911b53e3u, "frame 0's hash");
    verdict = Verdict(0u, 599u, 600u, &right);
    Expect(&t, WimsyVerdictPasses(&verdict) && verdict.exact, "frame 599 as asked");
    Expect(&t, Hash(frame, WIMSY_IMAGE_BYTES) == 0x7bae75b3u, "frame 599's hash");
    // A frame late, the batch of the frame before: the sprites that moved a pixel are elsewhere.
    WimsyMachine late = right;
    late.frames_late = 1u;
    verdict = Verdict(0u, 300u, 600u, &late);
    Expect(&t, !WimsyVerdictPasses(&verdict) && verdict.wrong > 1000u && verdict.farthest > 100u,
           "a frame late is refused");
    WimsyMachine aside = right;
    aside.shift = 1u;
    verdict = Verdict(0u, 300u, 600u, &aside);
    Expect(&t, !WimsyVerdictPasses(&verdict) && verdict.wrong > 1000u,
           "a pixel to the right is refused");
    // One byte of one pixel off by one, and the judge says which pixel and by how much.
    WimsyRender(0u, 300u, 600u, &right, frame);
    frame[((size_t)123 * WIMSY_WIDTH + 456) * 4u + 1u] ^= 1u;
    WimsyJudge(0u, 300u, 600u, frame, NULL, verdicts, &verdict);
    Expect(&t, verdict.wrong == 1u && verdict.wrong_x == 456u && verdict.wrong_y == 123u &&
                   verdict.farthest == 1u && verdicts[(size_t)123 * WIMSY_WIDTH + 456] ==
                                                  WIMSY_PIXEL_WRONG &&
                   verdicts[(size_t)123 * WIMSY_WIDTH + 455] == WIMSY_PIXEL_HELD,
           "one byte off by one is one wrong pixel, named");
    Report("wimsy: level 0's frames are held bit for bit, and one a frame late, a pixel aside or "
           "a byte off is refused",
           &t);
}

static void CheckTransform(void) {
    Tally t = { 0, NULL };
    WimsyInstance instance;
    Expect(&t, WimsyInstancesOf(1u, 0u, 600u, &instance) == 1u, "level 1 draws one model");
    Expect(&t, WimsyInstancesOf(0u, 0u, 600u, &instance) == 0u, "level 0 draws none");
    // Frame 0: no turn, the camera 3 away. The model's middle is the target's, at w 3; a point
    // one to the right lands 2 / (16 / 9) / 3 of half the width from it, 240 pixels.
    WimsyInstancesOf(1u, 0u, 600u, &instance);
    const float *const m = instance.to_clip;
    Expect(&t, m[12] == 0.0f && m[13] == 0.0f && m[15] == 3.0f, "the middle at w 3");
    Expect(&t, fabsf(m[0] / m[15] * 640.0f - 240.0f) < 0.01f,
           "one to the right is 240 pixels right");
    // Depth nought at the near distance and one at the far one. The model leans, so the point of
    // it a distance d in front of the camera is its third row, the eye's direction, times 3 - d.
    float depth[2];
    for (uint32_t end = 0; end < 2u; ++end) {
        const float along = 3.0f - (end == 0u ? 0.5f : 256.0f);
        float clip_z = m[14], clip_w = m[15];
        for (uint32_t column = 0; column < 3u; ++column) {
            clip_z += m[column * 4u + 2u] * instance.turn[2][column] * along;
            clip_w += m[column * 4u + 3u] * instance.turn[2][column] * along;
        }
        depth[end] = clip_z / clip_w;
    }
    Expect(&t, fabsf(depth[0]) < 1e-5f && fabsf(depth[1] - 1.0f) < 1e-5f,
           "depths nought and one at the two ends of what the camera sees");
    // Its rows turn a normal and leave its length alone, at any frame.
    WimsyInstancesOf(1u, 77u, 600u, &instance);
    bool turned = true;
    for (uint32_t a = 0; a < 3u; ++a) {
        for (uint32_t b = 0; b < 3u; ++b) {
            float dot = 0.0f;
            for (uint32_t k = 0; k < 3u; ++k) {
                dot += instance.turn[a][k] * instance.turn[b][k];
            }
            turned = turned && fabsf(dot - (a == b ? 1.0f : 0.0f)) < 1e-5f;
        }
    }
    Expect(&t, turned, "the rows of a turn, at frame 77");
    // The last frame, whatever the run's length: the camera 48 away.
    WimsyInstancesOf(1u, 599u, 600u, &instance);
    Expect(&t, instance.to_clip[15] == 48.0f, "the last of 600 frames is 48 away");
    WimsyInstancesOf(1u, 119u, 120u, &instance);
    Expect(&t, instance.to_clip[15] == 48.0f, "the last of 120 frames is 48 away");
    // Half way along a run of three: 3 + 45 / 4.
    WimsyInstancesOf(1u, 1u, 3u, &instance);
    Expect(&t, instance.to_clip[15] == 14.25f, "half way along is 14.25 away");
    // Nothing of the model is nearer the eye than the near distance on any frame: its farthest
    // corner is under 1.5 from its middle and the camera never under 3.
    int32_t reach = 0;
    for (uint32_t v = 0; v < WIMSY_MESH_VERTICES; ++v) {
        const WimsyMeshVertex *const vertex = &WIMSY_MESH_VERTEX[v];
        const int32_t square = vertex->x * vertex->x + vertex->y * vertex->y + vertex->z * vertex->z;
        reach = square > reach ? square : reach;
    }
    Expect(&t, reach < 3 * WIMSY_MESH_SCALE / 2 * (3 * WIMSY_MESH_SCALE / 2),
           "no corner 1.5 from the model's middle");
    WimsyLight light;
    WimsyLightOf(1u, &light);
    const float length = sqrtf(light.light[0] * light.light[0] + light.light[1] * light.light[1] +
                               light.light[2] * light.light[2]);
    Expect(&t, fabsf(length - 1.0f) < 0.002f && light.shade[0] + light.shade[1] == 1.0f,
           "a light of length one, and a face turned to it keeps its whole colour");
    Report("wimsy: the model's transform puts her middle in the target's, the camera where the "
           "path says, and nothing nearer than it sees",
           &t);
}

static void CheckModelFrames(void) {
    Tally t = { 0, NULL };
    const WimsyMachine right = WimsyMachineRight(1u);
    // Near, where she fills two thirds of the target's height, and far, where she is 30 pixels.
    WimsyVerdict verdict = Verdict(1u, 0u, 600u, &right);
    Expect(&t, WimsyVerdictPasses(&verdict) && !verdict.exact, "the first frame as asked passes");
    // Her outline is some 5,000 pixels long there, and a pixel of it is on an edge when one of
    // its four samples is within the margin of it: a few in ten.
    Expect(&t, verdict.held + verdict.edge == WIMSY_PIXELS && verdict.edge > 500u &&
                   verdict.edge < 5000u,
           "near: every pixel judged, 500 to 5,000 of them on an edge");
    // The head's front on frame 0 leans by the model's lean, a thirty second of a turn: its
    // normal is (0, -sine, cosine), and its colour its own times what the light gives that,
    // within the two codes a channel is given.
    const uint8_t *const head = Pixel(780u, 400u);
    int32_t lean_sine = 0, lean_cosine = 0;
    WimsySinCos(2048u, &lean_sine, &lean_cosine);
    const double lit = 0.40 + 0.60 * (-0.45 * (double)lean_sine + 0.84 * (double)lean_cosine) /
                                  1073741824.0;
    Expect(&t, fabs((double)head[0] - 0x3a * lit) <= 2.5 && fabs((double)head[1] - 0xe8 * lit) <= 2.5 &&
                   fabs((double)head[2] - 0xef * lit) <= 2.5 && head[3] == 255u,
           "the head's front is its colour in the light");
    Expect(&t, memcmp(Pixel(8, 8), WIMSY_CLEAR, 4u) == 0, "the target's colour around her");
    verdict = Verdict(1u, 599u, 600u, &right);
    Expect(&t, WimsyVerdictPasses(&verdict) && verdict.edge > 10u && verdict.edge < 500u,
           "the last frame as asked passes, 10 to 500 pixels on an edge");
    uint32_t drawn_pixels = 0;
    for (uint32_t i = 0; i < WIMSY_PIXELS; ++i) {
        drawn_pixels += memcmp(frame + (size_t)i * 4u, WIMSY_CLEAR, 4u) != 0 ? 1u : 0u;
    }
    Expect(&t, drawn_pixels > 100u && drawn_pixels < 1500u, "far: she is 100 to 1,500 pixels");
    Report("wimsy: the machine that draws level 1 as asked is inside the reference's ranges, "
           "near and far",
           &t);
}

static void CheckModelMachines(void) {
    Tally t = { 0, NULL };
    // Frame 120 of 160: half a turn, so the leaves painted last are the farthest, and 28 away,
    // where she is 50 pixels high and every kind of pixel is there in a few thousand.
    const uint32_t at = 120u, frames = 160u;
    const WimsyMachine right = WimsyMachineRight(1u);
    WimsyVerdict verdict = Verdict(1u, at, frames, &right);
    Expect(&t, WimsyVerdictPasses(&verdict), "as asked passes");
    WimsyMachine wrong = right;
    wrong.samples = 1u;
    verdict = Verdict(1u, at, frames, &wrong);
    Expect(&t, !WimsyVerdictPasses(&verdict) && verdict.wrong > 20u,
           "one sample a pixel is refused");
    wrong = right;
    wrong.depth = false;
    verdict = Verdict(1u, at, frames, &wrong);
    Expect(&t, !WimsyVerdictPasses(&verdict) && verdict.wrong > 20u, "no depth test is refused");
    wrong = right;
    wrong.lit = false;
    verdict = Verdict(1u, at, frames, &wrong);
    Expect(&t, !WimsyVerdictPasses(&verdict) && verdict.wrong > 20u, "no light is refused");
    wrong = right;
    wrong.frames_late = 1u;
    verdict = Verdict(1u, at, frames, &wrong);
    Expect(&t, !WimsyVerdictPasses(&verdict), "a frame late is refused");
    wrong = right;
    wrong.shift = 1u;
    verdict = Verdict(1u, at, frames, &wrong);
    Expect(&t, !WimsyVerdictPasses(&verdict) && verdict.wrong > 20u,
           "a pixel to the right is refused");
    // Nothing drawn: the target's colour everywhere.
    for (uint32_t i = 0; i < WIMSY_PIXELS; ++i) {
        memcpy(frame + (size_t)i * 4u, WIMSY_CLEAR, 4u);
    }
    WimsyJudge(1u, at, frames, frame, NULL, verdicts, &verdict);
    Expect(&t, !WimsyVerdictPasses(&verdict) && verdict.wrong > 200u && verdict.farthest > 50u,
           "nothing drawn is refused");
    // With no culling the faces turned away are drawn too, behind the others: the same picture.
    wrong = right;
    wrong.cull = false;
    verdict = Verdict(1u, at, frames, &wrong);
    Expect(&t, WimsyVerdictPasses(&verdict), "no culling passes: the depth test hides them");
    Report("wimsy: level 1 drawn with one sample, no depth test, no light, a frame late, a pixel "
           "aside or not at all is refused",
           &t);
}

static void CheckScene(void) {
    Tally t = { 0, NULL };
    static WimsyWorldVertex vertices[WIMSY_WORLD_VERTICES_MAX];
    WimsyWorldDraw draws[WIMSY_WORLD_DRAWS_MAX];
    uint32_t count = 0;
    Expect(&t, WimsyWorldOf(1u, vertices, &count, draws) == 0u && count == 0u,
           "level 1 has no world");
    // 34 rows of six floor quads and a lane's, four standing quads and four marks.
    const uint32_t made = WimsyWorldOf(2u, vertices, &count, draws);
    Expect(&t, made == 4u && count == (34u * 6u + 34u + 4u + 4u) * 6u &&
                   count <= WIMSY_WORLD_VERTICES_MAX && draws[0].material == WIMSY_MATERIAL_FLOOR &&
                   draws[0].first == 0u && draws[0].count == 34u * 36u &&
                   draws[1].material == WIMSY_MATERIAL_LANE && draws[1].count == 34u * 6u &&
                   draws[2].material == WIMSY_MATERIAL_FLAT && draws[2].count == 24u &&
                   draws[3].material == WIMSY_MATERIAL_MARK && draws[3].count == 24u &&
                   draws[3].first + draws[3].count == count,
           "the floor, the lane, the walls with their signs and the marks, in that order");
    // The lane fills the floor's gap: its first quad runs from x -1 to 1 at the floor's near edge.
    Expect(&t, vertices[draws[1].first].x == -1.0f && vertices[draws[1].first + 1u].x == 1.0f &&
                   vertices[draws[1].first].z == 2.0f && vertices[draws[1].first].y == 0.0f &&
                   vertices[draws[1].first + 1u].u == 1.0f,
           "the lane between x -1 and 1, its texture once across");
    // On the left the sign is drawn before its wall, on the right after: 5/256 in front both ways.
    const WimsyWorldVertex *const flat = vertices + draws[2].first;
    Expect(&t, flat[0].z == -40.0f + 0.01953125f && flat[6].z == -40.0f && flat[12].z == -40.0f &&
                   flat[18].z == flat[0].z && flat[0].x < 0.0f && flat[18].x > 0.0f,
           "the signs 5/256 of a unit in front of their walls, drawn first on the left");
    // Nothing of the scene is nearer the eye than it sees, or farther, on any judged frame of a
    // run, and the camera ends 36 units from the first model, 6.625 up.
    float view[16];
    uint32_t judged[WIMSY_JUDGED];
    bool inside = true;
    const uint32_t frames_judged = WimsyJudgedFrames(600u, judged);
    for (uint32_t j = 0; j < frames_judged; ++j) {
        WimsyViewOf(2u, judged[j], 600u, view);
        for (uint32_t i = 0; i < count; ++i) {
            const float w = view[3] * vertices[i].x + view[7] * vertices[i].y +
                            view[11] * vertices[i].z + view[15];
            inside = inside && w > 0.5f && w < 1024.0f;
        }
    }
    Expect(&t, inside, "every corner of the world between the near and the far distance");
    WimsyViewOf(2u, 599u, 600u, view);
    // The eye is where the transform's w and x, y rows are all nought but for its own distance:
    // at the model's place (0, 1.125, 0) the w is the distance along the camera's look.
    const float w_model = view[7] * 1.125f + view[15];
    Expect(&t, w_model > 35.0f && w_model < 37.0f, "the first model 36 away on the last frame");
    WimsyInstance models[WIMSY_INSTANCES_MAX];
    Expect(&t, WimsyInstancesOf(2u, 0u, 600u, models) == 7u, "seven models");
    WimsyViewOf(2u, 0u, 600u, view);
    // A model's transform is the view's after its own place: its middle goes where the view
    // puts (-3.25, 1.125, 0).
    const float middle_w = view[3] * -3.25f + view[7] * 1.125f + view[15];
    Expect(&t, fabsf(models[0].to_clip[15] - middle_w) < 1e-4f && models[1].to_clip[15] > middle_w,
           "the first model at the view's (-3.25, 1.125, 0), the second farther");
    // The textures: the lane's levels each one colour, far from the next; the floor's stones
    // 128 texels wide, nearer their mean a level at a time, and the mean from level 5.
    uint8_t a[4], b[4];
    bool apart = true;
    for (uint32_t at = 0; at + 1u < WimsyTextureLevels(WIMSY_TEXTURE_LANE); ++at) {
        WimsyTexel(WIMSY_TEXTURE_LANE, at, 0u, 0u, a);
        WimsyTexel(WIMSY_TEXTURE_LANE, at + 1u, 3u, 1u, b);
        uint32_t far = 0;
        for (uint32_t k = 0; k < 3u; ++k) {
            const uint32_t d = a[k] > b[k] ? (uint32_t)(a[k] - b[k]) : (uint32_t)(b[k] - a[k]);
            far = d > far ? d : far;
        }
        apart = apart && far >= 60u && a[3] == 255u;
    }
    Expect(&t, apart && WimsyTextureSide(WIMSY_TEXTURE_LANE) == 1024u &&
                   WimsyTextureLevels(WIMSY_TEXTURE_LANE) == 11u,
           "the lane's eleven levels, each 60 codes from the next in some channel");
    WimsyTexel(WIMSY_TEXTURE_FLOOR, 0u, 5u, 9u, a);
    WimsyTexel(WIMSY_TEXTURE_FLOOR, 0u, 127u, 127u, b);
    Expect(&t, memcmp(a, b, 4u) == 0, "a stone is one colour over 128 texels");
    WimsyTexel(WIMSY_TEXTURE_FLOOR, 2u, 1u, 2u, b);
    Expect(&t, b[0] == (a[0] * 3u + 110u * 2u + 2u) / 5u && b[2] == (a[2] * 3u + 110u * 2u + 2u) / 5u,
           "level 2 of it two fifths nearer the mean");
    WimsyTexel(WIMSY_TEXTURE_FLOOR, 5u, 7u, 7u, b);
    Expect(&t, b[0] == 110u && b[1] == 110u && b[2] == 110u && WimsyTextureLevels(WIMSY_TEXTURE_FLOOR) == 12u,
           "level 5 the mean");
    Report("wimsy: level 2's world, view and textures are where and what worked out by hand", &t);
}

static void CheckSceneFrames(void) {
    Tally t = { 0, NULL };
    const WimsyMachine right = WimsyMachineRight(2u);
    WimsyVerdict verdict = Verdict(2u, 0u, 600u, &right);
    Expect(&t, WimsyVerdictPasses(&verdict) && verdict.held > 600000u, "the first frame as asked");
    // The lane at the bottom of the first frame is its level 3 going on 2: yellow, with no blue.
    const uint8_t *const lane = Pixel(640u, 600u);
    Expect(&t, lane[0] > 200u && lane[1] > 150u && lane[2] < 80u, "the lane's nearest part yellow");
    verdict = Verdict(2u, 599u, 600u, &right);
    Expect(&t, WimsyVerdictPasses(&verdict) && verdict.held > 900000u, "the last frame as asked");
    // Half way, each machine wrong in one way the scene is there to show.
    const uint32_t at = 299u, frames = 600u;
    WimsyMachine wrong = right;
    wrong.between_levels = false;
    verdict = Verdict(2u, at, frames, &wrong);
    Expect(&t, !WimsyVerdictPasses(&verdict) && verdict.wrong > 1000u,
           "the nearer level of detail alone is refused");
    wrong = right;
    wrong.coarser = 1u;
    verdict = Verdict(2u, at, frames, &wrong);
    Expect(&t, !WimsyVerdictPasses(&verdict) && verdict.wrong > 1000u,
           "a level of detail too coarse is refused");
    wrong = right;
    wrong.bias = false;
    verdict = Verdict(2u, at, frames, &wrong);
    Expect(&t, !WimsyVerdictPasses(&verdict) && verdict.wrong > 500u,
           "marks with no depth bias are refused");
    wrong = right;
    wrong.depth = false;
    verdict = Verdict(2u, at, frames, &wrong);
    Expect(&t, !WimsyVerdictPasses(&verdict) && verdict.wrong > 500u, "no depth test is refused");
    // A depth buffer of 16 bits cannot tell a sign from its wall from the camera's last place,
    // and one of 24 can.
    wrong = right;
    wrong.depth_bits = 16u;
    verdict = Verdict(2u, 599u, frames, &wrong);
    Expect(&t, !WimsyVerdictPasses(&verdict) && verdict.wrong > 100u,
           "16 bits of depth are refused on the last frame");
    wrong.depth_bits = 24u;
    verdict = Verdict(2u, 599u, frames, &wrong);
    Expect(&t, WimsyVerdictPasses(&verdict), "24 bits of depth pass on the last frame");
    Report("wimsy: level 2 as asked passes near and far, and the wrong level of detail, no depth "
           "bias, no depth test or 16 bits of depth are refused",
           &t);
}

static void CheckHeavy(void) {
    Tally t = { 0, NULL };
    static WimsyInstance models[WIMSY_INSTANCES_MAX];
    // The scene's seven and a crowd of 160: the first of the crowd 6 left of the lane and 3
    // down it, the eighth 15 right, the ninth in the second row, 12 down.
    Expect(&t, WimsyInstancesOf(3u, 0u, 600u, models) == 167u &&
                   WimsyInstancesOf(4u, 0u, 600u, models) == 167u,
           "167 models at levels 3 and 4");
    float view[16];
    WimsyViewOf(3u, 0u, 600u, view);
    // A member stands within a unit aside and three along of its place in the rows: its w, the
    // distance along the camera's look, is within 3.2 of the place's.
    static const float PLACE[3][2] = { { -6.0f, -3.0f }, { 15.0f, -3.0f }, { -6.0f, -12.0f } };
    static const uint32_t MEMBER[3] = { 7u, 14u, 15u };
    for (uint32_t i = 0; i < 3u; ++i) {
        const float w = view[3] * PLACE[i][0] + view[7] * 1.125f + view[11] * PLACE[i][1] + view[15];
        Expect(&t, fabsf(models[MEMBER[i]].to_clip[15] - w) < 3.2f, "a member of the crowd's place");
    }
    // Each member one of her five drawings, by the one seed: all five are there, the scene's seven
    // are the first, level 2 has no other, and asking twice gives the same.
    uint32_t drawn_as[WIMSY_MESH_VARIANTS] = { 0, 0, 0, 0, 0 };
    bool known = true;
    for (uint32_t i = 0; i < 167u; ++i) {
        const uint32_t variant = WimsyInstanceVariant(3u, i);
        known = known && variant < WIMSY_MESH_VARIANTS && (i >= 7u || variant == 0u) &&
                variant == WimsyInstanceVariant(4u, i) && WimsyInstanceVariant(2u, i) == 0u;
        drawn_as[variant < WIMSY_MESH_VARIANTS ? variant : 0u] += 1u;
    }
    bool every = true;
    for (uint32_t variant = 0; variant < WIMSY_MESH_VARIANTS; ++variant) {
        every = every && drawn_as[variant] >= 20u;
    }
    Expect(&t, known && every, "the crowd is all five drawings, twenty of each at least");
    Expect(&t, strcmp(WIMSY_MESH_VARIANT[0], "wimsy") == 0 && strcmp(WIMSY_MESH_VARIANT[1], "fuddles") == 0 &&
                   WIMSY_MESH_PALETTE[0][8][0] == 0x3a && WIMSY_MESH_PALETTE[0][10][0] == 255 &&
                   WIMSY_MESH_PALETTE[4][0][1] == 0xaa && WIMSY_MESH_PALETTE[1][11][2] == 0x1b,
           "the drawings' palettes: the first one's head, the rims' white, the Default theme's teal, "
           "Fuddles' pupils");
    Expect(&t, models[7].tint[0] == 0.875f && models[7].tint[1] == 0.625f &&
                   models[7].tint[2] == 0.75f && models[6].tint[0] == 1.0f,
           "each of the crowd its own shade, the scene's seven their whole colour");
    bool seen = true;
    for (uint32_t i = 0; i < 167u; ++i) {
        // A model's middle is its transform's last column: in front of the near distance by more
        // than her reach, short of the far one.
        seen = seen && models[i].to_clip[15] > 2.0f && models[i].to_clip[15] < 1000.0f;
    }
    Expect(&t, seen, "every model between the near and the far distance");
    // The floors: 4096 and 8192 a side, ASTC blocks of 8 by 8, each block a void extent of its
    // stone's colour, which the suite's ASTC decoder gives back for all 64 texels, every way of
    // making eight bits of it.
    Expect(&t, WimsyFloorTexture(2u) == WIMSY_TEXTURE_FLOOR && WimsyFloorTexture(3u) == WIMSY_TEXTURE_FLOOR_4K &&
                   WimsyFloorTexture(4u) == WIMSY_TEXTURE_FLOOR_8K &&
                   WimsyTextureSide(WIMSY_TEXTURE_FLOOR_4K) == 4096u &&
                   WimsyTextureLevels(WIMSY_TEXTURE_FLOOR_4K) == 13u &&
                   WimsyTextureSide(WIMSY_TEXTURE_FLOOR_8K) == 8192u &&
                   WimsyTextureLevels(WIMSY_TEXTURE_FLOOR_8K) == 14u &&
                   WimsyTextureBlock(WIMSY_TEXTURE_FLOOR_8K) == 8u &&
                   WimsyTextureBlock(WIMSY_TEXTURE_FLOOR) == 0u && WimsyTextureBlock(WIMSY_TEXTURE_LANE) == 0u,
           "the three floors' sides, levels and blocks");
    bool decoded = true, flat = true;
    static const uint32_t AT[4][3] = { { 0, 0, 0 }, { 0, 37, 501 }, { 3, 9, 2 }, { 12, 0, 0 } };
    for (uint32_t i = 0; i < 4u; ++i) {
        uint8_t block[16], want[4];
        uint16_t texels[ASTC_MAX_TEXELS][4];
        WimsyTextureBlockAt(WIMSY_TEXTURE_FLOOR_4K, AT[i][0], AT[i][1], AT[i][2], block);
        WimsyTexel(WIMSY_TEXTURE_FLOOR_4K, AT[i][0], AT[i][1] * 8u, AT[i][2] * 8u, want);
        decoded = decoded && AstcDecodeBlock(block, 8u, 8u, texels) == ASTC_BLOCK_CONSTANT;
        for (uint32_t texel = 0; texel < 64u; ++texel) {
            for (uint32_t k = 0; k < 4u; ++k) {
                uint8_t rounded = 0, truncated = 0;
                AstcUnorm8Answers(texels[texel][k], &rounded, &truncated);
                decoded = decoded && rounded == want[k] && truncated == want[k] &&
                          AstcUnorm8Fp16(texels[texel][k]) == want[k];
            }
        }
        // Every texel of the block is the stone's: the last as the first.
        uint8_t last[4];
        WimsyTexel(WIMSY_TEXTURE_FLOOR_4K, AT[i][0], AT[i][1] * 8u + 7u, AT[i][2] * 8u + 7u, last);
        flat = flat && (AT[i][0] > 4u || memcmp(last, want, 4u) == 0);
    }
    Expect(&t, decoded, "a block decodes to its stone's colour, every way of making eight bits");
    Expect(&t, flat, "a block's texels are one colour");
    // The machine that draws the heavy frame as asked passes, near at level 3 and far at level
    // 4; one whose uniform buffers are a frame late is refused.
    WimsyMachine machine = WimsyMachineRight(3u);
    WimsyVerdict verdict = Verdict(3u, 0u, 600u, &machine);
    Expect(&t, WimsyVerdictPasses(&verdict) && verdict.held > 500000u, "level 3's first frame as asked");
    machine.frames_late = 1u;
    verdict = Verdict(3u, 300u, 600u, &machine);
    Expect(&t, !WimsyVerdictPasses(&verdict) && verdict.wrong > 500u, "level 3 a frame late is refused");
    // A machine that draws the thin faces turned just away is as right as one that culls them.
    // On frames 14 and 75 of 120, with no smoke to blur them, a far model's rim is such a face
    // and one sample of a pixel is the rim's: a reference sure of every face's side refused
    // that pixel, as it refused a desktop card's.
    machine = WimsyMachineRight(3u);
    machine.thin_faces = true;
    WimsySmokeAsk(0u);
    verdict = Verdict(3u, 14u, 120u, &machine);
    Expect(&t, WimsyVerdictPasses(&verdict), "thin faces turned just away, drawn, pass at frame 14");
    verdict = Verdict(3u, 75u, 120u, &machine);
    Expect(&t, WimsyVerdictPasses(&verdict), "thin faces turned just away, drawn, pass at frame 75");
    WimsySmokeAsk(WIMSY_SMOKE_DENSITIES);
    // So is a machine whose samples are a 64th of a pixel aside, half the edge margin. In a
    // field of grass two blades may each reach some of the places a sample may be and both miss
    // the sample itself, where a third shows through: a reference that took the two together
    // for a cover refused 11 pixels of level 3's first frame and 29 of level 4's, and one to
    // three a frame of a desktop card's.
    machine = WimsyMachineRight(3u);
    machine.nudge = 2u;
    verdict = Verdict(3u, 0u, 600u, &machine);
    Expect(&t, WimsyVerdictPasses(&verdict), "a 64th of a pixel aside passes at level 3");
    machine = WimsyMachineRight(4u);
    machine.nudge = 2u;
    verdict = Verdict(4u, 0u, 600u, &machine);
    Expect(&t, WimsyVerdictPasses(&verdict), "a 64th of a pixel aside passes at level 4");
    machine = WimsyMachineRight(4u);
    // The last frame looks back at the scene through the smoke: under a puff a pixel is a
    // range, so fewer are held to one colour there than in a frame without.
    verdict = Verdict(4u, 599u, 600u, &machine);
    Expect(&t, WimsyVerdictPasses(&verdict) && verdict.held > 200000u, "level 4's last frame as asked");
    Report("wimsy: the heavy levels' crowd, their floors' ASTC blocks, and the machine that draws "
           "them as asked",
           &t);
}

static void CheckLand(void) {
    Tally t = { 0, NULL };
    static WimsyWorldVertex vertices[WIMSY_WORLD_VERTICES_MAX], again[WIMSY_WORLD_VERTICES_MAX];
    WimsyWorldDraw draws[WIMSY_WORLD_DRAWS_MAX];
    uint32_t count = 0, count_again = 0;
    // A heavy level's world is level 2's, then the hills, its fifth draw: either side, 8 columns
    // by 34 rows of squares, two triangles each; then the walls and what is on them.
    const uint32_t made = WimsyWorldOf(3u, vertices, &count, draws);
    Expect(&t, made == 153u && made <= WIMSY_WORLD_DRAWS_MAX && count <= WIMSY_WORLD_VERTICES_MAX &&
                   draws[4].material == WIMSY_MATERIAL_FLAT &&
                   draws[4].count == 2u * 8u * 34u * 6u && draws[4].count == WIMSY_HILL_VERTICES &&
                   draws[4].first == draws[3].first + draws[3].count &&
                   draws[5].first == draws[4].first + draws[4].count,
           "the hills are the heavy world's fifth draw, 6528 vertices after level 2's");
    Expect(&t, WimsyWorldOf(4u, again, &count_again, draws) == 153u && count_again == count &&
                   memcmp(vertices, again, count * sizeof(vertices[0])) == 0,
           "level 4's world is level 3's, and asking twice gives the same");
    WimsyWorldOf(3u, vertices, &count, draws);
    // They meet the floor at its edge, 18 units out, at its height; no higher than 24 units; a
    // triangle is one colour; and the field has relief: a place over 12 units, the two sides
    // not each other's mirror.
    bool meets = true, bounded = true, one_colour = true;
    int32_t highest = 0;
    uint32_t differ = 0;
    for (uint32_t row = 0; row <= WIMSY_HILL_ROWS; ++row) {
        meets = meets && WimsyHillHeight(0u, 0u, row) == 0 && WimsyHillHeight(1u, 0u, row) == 0;
        for (uint32_t column = 0; column <= WIMSY_HILL_COLUMNS; ++column) {
            const int32_t left = WimsyHillHeight(0u, column, row);
            const int32_t right = WimsyHillHeight(1u, column, row);
            bounded = bounded && left >= 0 && right >= 0 && left <= 24 * 256 && right <= 24 * 256;
            highest = left > highest ? left : highest;
            highest = right > highest ? right : highest;
            differ += left != right ? 1u : 0u;
        }
    }
    for (uint32_t i = draws[4].first; i + 2u < draws[4].first + draws[4].count; i += 3u) {
        for (uint32_t k = 0; k < 3u; ++k) {
            const WimsyWorldVertex *const vertex = &vertices[i + k];
            one_colour = one_colour && vertex->r == vertices[i].r && vertex->g == vertices[i].g &&
                         vertex->b == vertices[i].b;
            bounded = bounded && fabsf(vertex->x) >= 18.0f && fabsf(vertex->x) <= 66.0f &&
                      vertex->y >= 0.0f && vertex->y <= 24.0f && vertex->z <= 2.0f &&
                      vertex->z >= -202.0f;
        }
    }
    Expect(&t, meets, "the hills are at the floor's height where they meet it");
    Expect(&t, bounded, "every corner of the hills inside the field, nought to 24 units high");
    Expect(&t, one_colour, "a triangle of the hills is one colour");
    Expect(&t, highest > 12 * 256 && differ > 200u,
           "the field has relief, and two sides of its own");
    Expect(&t, WimsyHillHeight(2u, 1u, 1u) == 0 && WimsyHillHeight(0u, 9u, 1u) == 0 &&
                   WimsyHillHeight(0u, 1u, 35u) == 0,
           "no hill past the field");
    // The first square's first triangle on the left: its corners, mirrored, and a green of the
    // low ground in the light.
    const WimsyWorldVertex *const first = &vertices[draws[4].first];
    Expect(&t, first[0].x == -18.0f && first[0].y == 0.0f && first[0].z == 2.0f &&
                   first[1].x == -24.0f && first[1].z == 2.0f && first[2].x == -24.0f &&
                   first[2].z == -4.0f &&
                   first[1].y == (float)WimsyHillHeight(0u, 1u, 0u) / 256.0f &&
                   first[0].g > first[0].r && first[0].g > first[0].b,
           "the first triangle of the hills");
    // The grass: five densities, each the first blades of the next, none below the heavy levels,
    // level 3's a field of 65,536 and level 4's of 262,144 unless a density is asked for.
    Expect(&t, WimsyBladeCount(0u) == 0u && WimsyBladeCount(1u) == 2048u &&
                   WimsyBladeCount(2u) == 16384u && WimsyBladeCount(3u) == 65536u &&
                   WimsyBladeCount(4u) == 262144u && WimsyBladeCount(4u) == WIMSY_BLADES_MAX &&
                   WimsyBladeCount(5u) == 0u,
           "the five densities' blades");
    Expect(&t, WimsyGrassOf(0u) == 0u && WimsyGrassOf(2u) == 0u && WimsyGrassOf(3u) == 3u &&
                   WimsyGrassOf(4u) == 4u && WimsyGrassOf(5u) == 0u,
           "each level's own density");
    WimsyGrassAsk(1u);
    const bool asked = WimsyGrassOf(3u) == 1u && WimsyGrassOf(4u) == 1u && WimsyGrassOf(2u) == 0u;
    WimsyGrassAsk(0u);
    const bool none = WimsyGrassOf(3u) == 0u && WimsyGrassOf(4u) == 0u;
    WimsyGrassAsk(WIMSY_GRASS_DENSITIES);
    Expect(&t, asked && none && WimsyGrassOf(3u) == 3u && WimsyGrassOf(4u) == 4u,
           "a density asked for is every heavy level's, until each has its own again");
    enum { SOME = 4096 };
    static WimsyBlade blades[SOME], later[SOME];
    Expect(&t, WimsyBladesOf(0u, SOME, blades) == SOME && WimsyBladesOf(2048u, 2048u, later) == 2048u &&
                   memcmp(&blades[2048], later, 2048u * sizeof(later[0])) == 0 &&
                   WimsyBladesOf(WIMSY_BLADES_MAX - 16u, SOME, later) == 16u &&
                   WimsyBladesOf(WIMSY_BLADES_MAX, SOME, later) == 0u,
           "blades by their numbers: a thicker field's first are the thinner one's");
    bool placed = true, cornered = true, swaying = true;
    uint32_t sides[2] = { 0, 0 };
    for (uint32_t i = 0; i < SOME; ++i) {
        const WimsyBlade *const blade = &blades[i];
        placed = placed && fabsf(blade->x) >= 4.25f && fabsf(blade->x) <= 17.75f &&
                 blade->z <= 1.0f && blade->z >= -199.0f && blade->height >= 0.375f &&
                 blade->height <= 0.8125f && fabsf(blade->lean) <= 0.234375f &&
                 blade->colour[1] >= 150u && blade->colour[3] == 255u;
        sides[blade->x < 0.0f ? 0u : 1u] += 1u;
        // Its feet a sixteenth either side whatever the wind; its tip its lean and a sway of
        // 3/32 at most aside, at its height.
        for (uint32_t frame = 0; frame < WIMSY_WIND_FRAMES; frame += 31u) {
            float corner[3][3];
            for (uint32_t k = 0; k < 3u; ++k) {
                WimsyBladeCorner(blade, WimsyWindOf(frame), k, corner[k]);
            }
            cornered = cornered && corner[0][0] == blade->x - 0.0625f && corner[0][1] == 0.0f &&
                       corner[1][0] == blade->x + 0.0625f && corner[1][1] == 0.0f &&
                       corner[2][1] == blade->height && corner[0][2] == blade->z &&
                       corner[1][2] == blade->z && corner[2][2] == blade->z;
            swaying = swaying &&
                      fabsf(corner[2][0] - (blade->x + blade->lean)) <= WIMSY_BLADE_SWAY + 1e-6f;
        }
    }
    Expect(&t, placed && sides[0] == SOME / 2u && sides[1] == SOME / 2u,
           "every blade on the floor beside the lane, half each side");
    Expect(&t, cornered, "a blade's feet stand and its tip keeps its height");
    Expect(&t, swaying, "a tip sways no farther than 3/32 of a unit");
    // The wind comes round every 96 frames, and half a sway apart a tip is somewhere else: by
    // hand, a blade at x 8 and z -16 is at the start of its sway in still wind (8/8 - 16/16 is
    // nought), its tip 3/32 right of its lean, and 3/32 left of it 48 frames later.
    const WimsyBlade by_hand = { 8.0f, -16.0f, 0.5f, 0.125f, { 0, 0, 0, 255 } };
    float tip[2][3];
    WimsyBladeCorner(&by_hand, WimsyWindOf(0u), 2u, tip[0]);
    WimsyBladeCorner(&by_hand, WimsyWindOf(48u), 2u, tip[1]);
    Expect(&t, WimsyWindOf(0u) == 0.0f && WimsyWindOf(96u) == 0.0f && WimsyWindOf(48u) == 0.5f &&
                   WimsyWindOf(24u) == 0.25f && tip[0][0] == 8.0f + 0.125f + 0.09375f &&
                   tip[1][0] == 8.0f + 0.125f - 0.09375f && tip[0][1] == 0.5f,
           "the wind's turn, and a tip worked out by hand at either end of its sway");
    // A machine that leaves the hills out, or the grass, is refused where the camera sees them.
    // The hills from the lane past the walls, four fifths of the way through the walk; the grass
    // from the walk's start.
    WimsyMachine machine = WimsyMachineRight(3u);
    machine.left_out = 1u << 4;
    WimsyVerdict verdict = Verdict(3u, 480u, 600u, &machine);
    Expect(&t, !WimsyVerdictPasses(&verdict) && verdict.wrong > 2000u, "no hills is refused");
    machine.left_out = WIMSY_LEAVES_OUT_GRASS;
    verdict = Verdict(3u, 0u, 600u, &machine);
    Expect(&t, !WimsyVerdictPasses(&verdict) && verdict.wrong > 200u, "no grass is refused");
    // And one whose grass stands still: right at frame nought, refused a quarter of a sway on.
    machine = WimsyMachineRight(3u);
    machine.wind = false;
    verdict = Verdict(3u, 0u, 600u, &machine);
    Expect(&t, WimsyVerdictPasses(&verdict), "still grass is the right grass at frame nought");
    verdict = Verdict(3u, 24u, 600u, &machine);
    Expect(&t, !WimsyVerdictPasses(&verdict) && verdict.wrong > 200u, "no wind is refused");
    // The thinnest grass asked for at level 3, drawn as asked, passes.
    WimsyGrassAsk(1u);
    machine = WimsyMachineRight(3u);
    verdict = Verdict(3u, 24u, 600u, &machine);
    WimsyGrassAsk(WIMSY_GRASS_DENSITIES);
    Expect(&t, WimsyVerdictPasses(&verdict), "the thinnest grass, as asked, passes");
    Report("wimsy: the heavy levels' hills and grass, from their seeds, and the machines without "
           "them refused",
           &t);
}

static void CheckSmoke(void) {
    Tally t = { 0, NULL };
    // Four densities, each the first puffs of the next; none below the heavy levels, 256 puffs
    // at level 3 and 1,024 at level 4 unless a density is asked for.
    Expect(&t, WimsyPuffCount(0u) == 0u && WimsyPuffCount(1u) == 64u && WimsyPuffCount(2u) == 256u &&
                   WimsyPuffCount(3u) == 1024u && WimsyPuffCount(3u) == WIMSY_PUFFS_MAX &&
                   WimsyPuffCount(4u) == 0u,
           "the four densities' puffs");
    Expect(&t, WimsySmokeOf(2u) == 0u && WimsySmokeOf(3u) == 2u && WimsySmokeOf(4u) == 3u &&
                   WimsySmokeOf(5u) == 0u,
           "each level's own smoke");
    WimsySmokeAsk(1u);
    const bool asked = WimsySmokeOf(3u) == 1u && WimsySmokeOf(4u) == 1u && WimsySmokeOf(2u) == 0u;
    WimsySmokeAsk(WIMSY_SMOKE_DENSITIES);
    Expect(&t, asked && WimsySmokeOf(3u) == 2u, "a density asked for, then each level's own again");
    static WimsyPuff puffs[WIMSY_PUFFS_MAX], fewer[64], later[WIMSY_PUFFS_MAX];
    Expect(&t, WimsyPuffsOf(WIMSY_PUFFS_MAX, 77u, puffs) == WIMSY_PUFFS_MAX &&
                   WimsyPuffsOf(64u, 77u, fewer) == 64u && memcmp(puffs, fewer, sizeof(fewer)) == 0 &&
                   WimsyPuffsOf(WIMSY_PUFFS_MAX + 9u, 77u, later) == WIMSY_PUFFS_MAX,
           "a thicker smoke's first puffs are the thinner one's");
    // Every puff inside its bounds at every age; it comes round every 240 frames; it is
    // nothing when it starts and never hides more than 62 of 255.
    bool bounded = true, round_again = true;
    uint32_t thickest = 0, starting = 0;
    for (uint32_t frame = 0; frame < WIMSY_PUFF_FRAMES; frame += 7u) {
        WimsyPuffsOf(WIMSY_PUFFS_MAX, frame, puffs);
        WimsyPuffsOf(WIMSY_PUFFS_MAX, frame + WIMSY_PUFF_FRAMES, later);
        round_again = round_again && memcmp(puffs, later, sizeof(puffs)) == 0;
        for (uint32_t i = 0; i < WIMSY_PUFFS_MAX; ++i) {
            const WimsyPuff *const puff = &puffs[i];
            bounded = bounded && puff->x >= -16.0f && puff->x <= 17.5f && puff->y >= 0.5f &&
                      puff->y <= 7.5f && puff->z <= -50.0f && puff->z >= -90.0f &&
                      puff->size >= 1.0f && puff->size < 3.0f && puff->colour[3] <= 62u &&
                      puff->colour[0] >= 176u && puff->colour[0] == puff->colour[1];
            thickest = puff->colour[3] > thickest ? puff->colour[3] : thickest;
            starting += puff->colour[3] == 0u ? 1u : 0u;
        }
    }
    Expect(&t, bounded, "every puff in the smoke's place, a unit to three in half side");
    Expect(&t, round_again, "a puff comes round every 240 frames");
    Expect(&t, thickest >= 55u && starting > 0u, "a puff thickens from nothing and thins away");
    // One puff by hand, over a frame: it rises 3 units and drifts 1.5 in its 240 frames, a
    // 240th of that a frame, until it starts again.
    WimsyPuffsOf(1u, 10u, puffs);
    WimsyPuffsOf(1u, 11u, later);
    const float rise = later[0].y - puffs[0].y, drift = later[0].x - puffs[0].x;
    Expect(&t, (fabsf(rise - 3.0f / 240.0f) < 1e-4f && fabsf(drift - 1.5f / 240.0f) < 1e-4f) ||
                   (rise < -2.9f && drift < -1.4f),
           "a puff rises and drifts a 240th of its way a frame");
    float scale[2];
    WimsyPuffScale(scale);
    Expect(&t, scale[0] == 1.125f && scale[1] == 2.0f, "a puff's reach in clip space a unit");
    // The machine that draws the smoke as asked passes; one that draws none, and one that
    // draws it over what is nearer, the walls 10 units in front of it, are refused.
    WimsyMachine machine = WimsyMachineRight(3u);
    WimsyVerdict verdict = Verdict(3u, 120u, 600u, &machine);
    Expect(&t, WimsyVerdictPasses(&verdict), "level 3's smoke as asked");
    machine.smoke = false;
    verdict = Verdict(3u, 120u, 600u, &machine);
    Expect(&t, !WimsyVerdictPasses(&verdict) && verdict.wrong > 2000u, "no smoke is refused");
    machine = WimsyMachineRight(3u);
    machine.smoke_depth = false;
    verdict = Verdict(3u, 120u, 600u, &machine);
    Expect(&t, !WimsyVerdictPasses(&verdict) && verdict.wrong > 2000u,
           "smoke over what is nearer is refused");
    // The thickest smoke, at level 4 as its own and at level 3 as asked, passes.
    machine = WimsyMachineRight(4u);
    verdict = Verdict(4u, 300u, 600u, &machine);
    Expect(&t, WimsyVerdictPasses(&verdict), "level 4's smoke as asked");
    WimsySmokeAsk(0u);
    machine = WimsyMachineRight(3u);
    verdict = Verdict(3u, 120u, 600u, &machine);
    WimsySmokeAsk(WIMSY_SMOKE_DENSITIES);
    Expect(&t, WimsyVerdictPasses(&verdict), "no smoke, as asked, passes");
    Report("wimsy: the heavy levels' smoke, its puffs by seed and frame, and the machines "
           "without it or with it over everything refused",
           &t);
}

static void CheckScreens(void) {
    Tally t = { 0, NULL };
    static WimsyWorldVertex vertices[WIMSY_WORLD_VERTICES_MAX];
    static WimsyWorldDraw draws[WIMSY_WORLD_DRAWS_MAX];
    uint32_t count = 0;
    // After the hills: the two walls, one draw; a screen a draw, the compute textures' 128
    // levels, the 16 chains and the 2 loads; and the plaques, one draw of 34.
    const uint32_t made = WimsyWorldOf(3u, vertices, &count, draws);
    bool screens = made == 153u && draws[5].material == WIMSY_MATERIAL_FLAT && draws[5].count == 12u;
    for (uint32_t d = 6; d < 152u && screens; ++d) {
        const uint32_t shown = d - 6u;
        screens = draws[d].material == WIMSY_MATERIAL_SCREEN && draws[d].count == 6u &&
                  (shown < 128u
                       ? draws[d].screen == WIMSY_SCREEN_COMPUTE && draws[d].index == shown / 8u &&
                             draws[d].mip == shown % 8u
                       : shown < 144u
                             ? draws[d].screen == WIMSY_SCREEN_CHAIN && draws[d].index == shown - 128u
                             : draws[d].screen == WIMSY_SCREEN_LOAD && draws[d].index == shown - 144u);
    }
    Expect(&t, screens, "the walls, then 128 compute screens, 16 chains' and 2 loads', a draw each");
    Expect(&t, draws[152].material == WIMSY_MATERIAL_SCREEN &&
                   draws[152].screen == WIMSY_SCREEN_PLAQUES && draws[152].count == 34u * 6u &&
                   draws[152].first + draws[152].count == count && count <= WIMSY_WORLD_VERTICES_MAX,
           "the plaques, the last draw, 34 of them");
    // The first compute texture's first level: on the left wall a 64th of a unit in front of
    // it, from 2 units down the lane to 4, its top 5.5 units up, read from the lane left to
    // right; its second level beside it, half the side.
    const WimsyWorldVertex *const first = &vertices[draws[6].first];
    const WimsyWorldVertex *const second = &vertices[draws[7].first];
    Expect(&t, first[0].x == -17.859375f && first[0].y == 5.5f && first[0].z == -2.0f &&
                   first[0].u == 0.0f && first[0].v == 0.0f && first[1].z == -4.0f &&
                   first[1].u == 1.0f && first[2].y == 3.5f && first[2].v == 1.0f &&
                   second[0].z == -4.125f && second[1].z == -5.125f && second[2].y == 4.5f,
           "the first compute texture's first two levels on the left wall");
    // The first chain on the right wall, read left to right up the lane; the first load after
    // the sixteenth chain, showing an eighth of its texture either way.
    const WimsyWorldVertex *const chain = &vertices[draws[134].first];
    const WimsyWorldVertex *const load = &vertices[draws[150].first];
    Expect(&t, chain[0].x == 17.859375f && chain[0].z == -4.0f && chain[0].u == 0.0f &&
                   chain[1].z == -2.0f && chain[1].u == 1.0f && load[0].z == -56.0f &&
                   load[1].z == -54.0f && load[1].u == 0.125f && load[2].v == 0.125f,
           "the first chain and the first load on the right wall");
    // The walls themselves: 64 units from the floor's near end, 7 tall, an eighth inside its edge.
    const WimsyWorldVertex *const wall = &vertices[draws[5].first];
    Expect(&t, wall[0].x == -17.875f && wall[0].y == 7.0f && wall[0].z == 2.0f &&
                   wall[1].z == -62.0f && wall[2].y == 0.0f && wall[6].x == 17.875f,
           "the two walls along the floor's edges");
    // Without the legend the plaques go and nothing else does.
    WimsyLegendAsk(false);
    uint32_t bare = 0;
    const bool without = !WimsyLegendOf() && WimsyWorldOf(3u, vertices, &bare, draws) == 152u &&
                         bare == count - 34u * 6u;
    WimsyLegendAsk(true);
    Expect(&t, without && WimsyLegendOf(), "the legend taken away is the plaques' draw gone");
    // A plaque's texels: the board's colour past its text, ink in its first letter, another
    // plaque's number another texel somewhere, and past the last plaque only board.
    uint8_t board[4], ink[4], next[4];
    WimsyPlaqueTexel(127u, 31u, board);
    uint32_t inked = 0, differ = 0, beyond = 0;
    for (uint32_t y = 0; y < 16u; ++y) {
        for (uint32_t x = 0; x < 128u; ++x) {
            WimsyPlaqueTexel(x, y, ink);
            WimsyPlaqueTexel(x + 128u, y, next);
            inked += x < 8u && memcmp(ink, board, 4u) != 0 ? 1u : 0u;
            differ += memcmp(ink, next, 4u) != 0 ? 1u : 0u;
            WimsyPlaqueTexel(x + 256u, y + 288u, next);
            beyond += memcmp(next, board, 4u) != 0 ? 1u : 0u;
        }
    }
    Expect(&t, board[0] == 24u && board[3] == 255u && inked > 10u && differ > 4u && beyond == 0u,
           "a plaque is its name in ink on its board");
    // A compute texel worked out by hand: texture 3, level 2, frame 10, texel (5, 9).
    uint8_t texel[4];
    WimsyComputeTexel(3u, 2u, 10u, 5u, 9u, texel);
    Expect(&t, texel[0] == 5u * 5u + 30u + 48u && texel[1] == 63u + 10u + 64u &&
                   texel[2] == (uint8_t)((5u ^ 9u) * 9u + 24u + 20u) && texel[3] == 255u,
           "a compute texel from its place, texture, level and frame");
    WimsyComputeTexel(15u, 7u, 600u, 0u, 0u, texel);
    Expect(&t, texel[0] == (uint8_t)(1800u + 240u) && texel[1] == (uint8_t)(600u + 224u),
           "a compute texel past a byte wraps");
    // A chain: its clear, a square's place inside the target on every frame, and the later
    // square over the earlier one. On frame 0, chain 0's step 0 square is at (0, 0) and step 10's
    // at (1, 12): the texel (2, 13) is step 10's, and (63, 63), past every square, nobody's.
    WimsyChainSquare square;
    bool fits = true;
    for (uint32_t frame = 0; frame < 700u; frame += 7u) {
        for (uint32_t step = 0; step < WIMSY_CHAIN_STEPS; ++step) {
            WimsyChainSquareOf(frame % WIMSY_CHAINS, step, frame, &square);
            fits = fits && square.x + WIMSY_CHAIN_SQUARE <= WIMSY_CHAIN_SIDE &&
                   square.y + WIMSY_CHAIN_SQUARE <= WIMSY_CHAIN_SIDE;
        }
    }
    Expect(&t, fits, "every square whole inside its target");
    WimsyChainSquareOf(0u, 0u, 0u, &square);
    WimsyChainSquare last;
    WimsyChainSquareOf(0u, 10u, 0u, &last);
    Expect(&t, square.x == 0u && square.y == 0u && last.x == 1u && last.y == 12u, "frame 0's squares");
    uint8_t clear[4];
    WimsyChainClear(0u, clear);
    WimsyChainTexel(0u, 0u, 2u, 13u, texel);
    Expect(&t, memcmp(texel, last.colour, 4u) == 0 && memcmp(last.colour, square.colour, 4u) != 0,
           "a later square is over an earlier one");
    WimsyChainTexel(0u, 0u, 63u, 63u, texel);
    Expect(&t, memcmp(texel, clear, 4u) == 0 && clear[0] == 16u && clear[3] == 255u,
           "a texel no square reaches is the chain's clear");
    // A load: the frame's two differ, a block is one colour and a void extent the suite's decoder
    // gives back.
    uint8_t other[4], same[4], block[16];
    WimsyLoadTexel(600u, 9u, 20u, texel);
    WimsyLoadTexel(601u, 9u, 20u, other);
    WimsyLoadTexel(600u, 15u, 23u, same);
    Expect(&t, texel[0] == (uint8_t)(29u + 4200u) && texel[1] == (uint8_t)(62u + 1800u) &&
                   memcmp(texel, same, 4u) == 0 && memcmp(texel, other, 4u) != 0,
           "a load's block is one colour, from its place and the load's number");
    WimsyLoadBlock(600u, 1u, 2u, block);
    uint16_t decoded[ASTC_MAX_TEXELS][4];
    Expect(&t, AstcDecodeBlock(block, 8u, 8u, decoded) == ASTC_BLOCK_CONSTANT &&
                   decoded[63][0] == texel[0] * 257u && decoded[0][2] == texel[2] * 257u,
           "a load's block decodes to its colour");
    // The machine as asked passes in front of each wall, a quarter and a half of the way through
    // the walk; one whose screens show the frame before's textures is refused at both, and so
    // is one whose every buffer is a frame late.
    const WimsyMachine right = WimsyMachineRight(3u);
    WimsyVerdict verdict = Verdict(3u, 150u, 600u, &right);
    Expect(&t, WimsyVerdictPasses(&verdict), "level 3 before the compute wall, as asked");
    verdict = Verdict(3u, 300u, 600u, &right);
    Expect(&t, WimsyVerdictPasses(&verdict), "level 3 before the chains' wall, as asked");
    WimsyMachine late = right;
    late.screens = false;
    verdict = Verdict(3u, 150u, 600u, &late);
    Expect(&t, !WimsyVerdictPasses(&verdict) && verdict.wrong > 2000u,
           "compute screens a frame late are refused");
    verdict = Verdict(3u, 300u, 600u, &late);
    Expect(&t, !WimsyVerdictPasses(&verdict) && verdict.wrong > 2000u,
           "chains' screens a frame late are refused");
    late = right;
    late.frames_late = 1u;
    verdict = Verdict(3u, 300u, 600u, &late);
    Expect(&t, !WimsyVerdictPasses(&verdict) && verdict.wrong > 2000u, "a frame late is refused");
    // Without its plaques, as asked, it passes too.
    WimsyLegendAsk(false);
    verdict = Verdict(3u, 150u, 600u, &right);
    WimsyLegendAsk(true);
    Expect(&t, WimsyVerdictPasses(&verdict), "level 3 without the legend, as asked");
    // A thread's share of the models is the same models.
    static WimsyInstance whole[WIMSY_INSTANCES_MAX], part[WIMSY_INSTANCES_MAX];
    const uint32_t models = WimsyInstancesOf(3u, 77u, 600u, whole);
    const uint32_t first_half = WimsyInstancesFrom(3u, 77u, 600u, 0u, 84u, part);
    const uint32_t second_half = WimsyInstancesFrom(3u, 77u, 600u, 84u, 200u, part + 84u);
    Expect(&t, models == 167u && first_half == 84u && second_half == 83u &&
                   memcmp(whole, part, sizeof(WimsyInstance) * 167u) == 0 &&
                   WimsyInstancesFrom(3u, 77u, 600u, 167u, 5u, part) == 0u,
           "the models in two shares are the models");
    Report("wimsy: the heavy levels' walls: the screens of what a compute program stores, a "
           "chain of targets comes to and the processor loads, and their plaques",
           &t);
}

static void CheckSaid(void) {
    Tally t = { 0, NULL };
    char said[400];
    WimsyVerdict verdict;
    memset(&verdict, 0, sizeof(verdict));
    verdict.held = WIMSY_PIXELS;
    verdict.exact = true;
    WimsyDescribeVerdict(said, sizeof(said), &verdict);
    Expect(&t, strcmp(said, "every one of 921600 pixels is the reference's") == 0, "bit for bit");
    verdict.exact = false;
    verdict.held = 900000u;
    verdict.edge = 21600u;
    WimsyDescribeVerdict(said, sizeof(said), &verdict);
    Expect(&t, strcmp(said, "900000 pixels held to a colour, 21600 to a wider range, on an edge "
                            "or between levels of detail") == 0,
           "within the ranges");
    verdict.edge = 21000u;
    verdict.wrong = 600u;
    verdict.farthest = 37u;
    verdict.wrong_x = 640u;
    verdict.wrong_y = 360u;
    const uint8_t got[4] = { 0x1b, 0x1b, 0x1f, 0xff }, low[4] = { 0x30, 0x80, 0x90, 0xff },
                  high[4] = { 0x34, 0x84, 0x94, 0xff };
    memcpy(verdict.wrong_got, got, 4u);
    memcpy(verdict.wrong_low, low, 4u);
    memcpy(verdict.wrong_high, high, 4u);
    WimsyDescribeVerdict(said, sizeof(said), &verdict);
    Expect(&t, strcmp(said, "600 of 921600 pixels outside their range, by 37 at most; the one at "
                            "(640,360) is 1b 1b 1f ff, held to 30 80 90 ff up to 34 84 94 ff") == 0,
           "outside the ranges");
    char cut[16];
    Expect(&t, WimsyDescribeVerdict(cut, sizeof(cut), &verdict) == 15u && cut[15] == '\0',
           "cut to the room there is");
    Expect(&t, strcmp(WimsyLevelName(0u), "sprites") == 0 &&
                   strcmp(WimsyLevelName(1u), "one lit model") == 0 &&
                   strcmp(WimsyLevelName(2u), "a scene near and far") == 0 &&
                   strcmp(WimsyLevelName(3u), "a heavy frame") == 0 &&
                   strcmp(WimsyLevelName(4u), "a heavy frame of 8K") == 0 &&
                   strcmp(WimsyLevelName(WIMSY_LEVELS), "no level") == 0,
           "the levels' names");
    Report("wimsy: what a line says of a verdict", &t);
}

void RunReferenceWimsy(void) {
    CheckTurn();
    CheckJudgedFrames();
    CheckSprite();
    CheckSprites();
    CheckSpriteFrames();
    CheckTransform();
    CheckModelFrames();
    CheckModelMachines();
    CheckScene();
    CheckSceneFrames();
    CheckHeavy();
    CheckLand();
    CheckSmoke();
    CheckScreens();
    CheckSaid();
}
