// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
    Expect(&t, strcmp(said, "900000 pixels within their colour's range, 21600 on an edge within "
                            "the colours beside it") == 0,
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
    CheckSaid();
}
