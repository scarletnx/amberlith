// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gameref.h"

#ifdef __cplusplus
extern "C" {
#endif

/// The game family's levels, worked out on the processor: Wimsy (wimsymesh.h) drawn as a game
/// draws a character, from the lightest frame a game makes to the heaviest, each level a scene
/// whose every frame is a function of its number and of how many frames the run has. No libnx,
/// so the host build checks all of it before the console build draws a frame.
///
/// Level 0 is two dimensional: sprites of her, moved by the game scene's world (gameref.h), one
/// batch of vertices the processor rewrites every frame. Level 1 is her model alone, lit by one
/// light, turning a fixed angle a frame behind a depth buffer with four samples a pixel, while
/// the camera backs away from near to far over the run.
///
/// A level's frame is not held to a hash: two machines that are both right differ in a shaded
/// frame's last bits and in which way a sample on an edge falls. Each judged frame is held to
/// bounds a pixel, worked out here (WimsyJudge).

#define WIMSY_LEVELS 2u

/// What a level is called on a line.
const char *WimsyLevelName(uint32_t level);

/// The target every level draws into, as large as the console's own screen.
#define WIMSY_WIDTH 1280u
#define WIMSY_HEIGHT 720u
#define WIMSY_PIXELS (WIMSY_WIDTH * WIMSY_HEIGHT)
#define WIMSY_IMAGE_BYTES (WIMSY_PIXELS * 4u)

/// What the target is cleared to.
extern const uint8_t WIMSY_CLEAR[4];

/// The frames of a run that are judged: its first, each quarter of the way and its last, so the
/// camera's whole path is, however many frames the run has. Returns how many, WIMSY_JUDGED at
/// most, fewer when the run is too short to have them apart; in @p out, ascending.
#define WIMSY_JUDGED 5u
uint32_t WimsyJudgedFrames(uint32_t frames, uint32_t out[WIMSY_JUDGED]);

/// How far along the camera's path frame @p frame of @p frames is, nought to 65536.
uint32_t WimsyPathAt(uint32_t frame, uint32_t frames);

/// The sine and cosine of @p turn sixty five thousand five hundred and thirty sixths of a turn,
/// 2^30 to one, in integer arithmetic alone, so every machine turns the model by the same matrix.
/// Within 2^-20 of the true ones.
void WimsySinCos(uint32_t turn, int32_t *sine, int32_t *cosine);

/// Level 0. The sprite is the model seen from the front, WIMSY_SPRITE texels a side: a texel
/// holds the colour of the last triangle facing the eye that covers its centre, opaque, or
/// nought, clear. Integer arithmetic, so the texture is the same bytes on every machine.
#define WIMSY_SPRITE 128u
#define WIMSY_SPRITE_BYTES (WIMSY_SPRITE * WIMSY_SPRITE * 4u)
void WimsySpriteTexels(uint8_t *texels);

/// The sprites of a frame: where each one's top left pixel is (it may hang off the target), its
/// side in pixels, one or two pixels a texel, and whether it is mirrored. Drawn in order, a later
/// one over an earlier one.
#define WIMSY_SPRITES 96u
typedef struct {
    int32_t x;
    int32_t y;
    uint32_t side;
    bool mirrored;
} WimsySprite;

/// The sprites of the frame @p world is at, one for each of its first WIMSY_SPRITES cells.
void WimsySpritesOf(const GameWorld *world, WimsySprite out[WIMSY_SPRITES]);

/// The image @p count sprites leave over the clear colour, WIMSY_IMAGE_BYTES in row order, the
/// top row first: a texel that is not clear replaces what is under it.
void WimsySpritesImage(const WimsySprite *sprites, uint32_t count, const uint8_t *texels,
                       uint8_t *pixels);

/// Levels 1 and up. What a draw of the model reads from its uniform buffer, laid out as the
/// shader's block is (std140): the transform from the model to clip space, column by column, the
/// rows of the model's rotation, which turn a normal, and a colour every vertex's is multiplied
/// by.
typedef struct {
    float to_clip[16];
    float turn[3][4];
    float tint[4];
} WimsyInstance;

/// What every draw of a frame reads: the direction the light comes from, in the world, and how
/// much of a colour a face turned away keeps (x) and a face turned to the light adds (y).
typedef struct {
    float light[4];
    float shade[4];
} WimsyLight;

#define WIMSY_INSTANCES_MAX 1u

/// The models frame @p frame of a run of @p frames draws at @p level, in order. Returns how many.
uint32_t WimsyInstancesOf(uint32_t level, uint32_t frame, uint32_t frames, WimsyInstance *out);

/// The light of @p level.
void WimsyLightOf(uint32_t level, WimsyLight *out);

/// How an imagined machine draws a frame, for WimsyRender: the reference suite's machines, each
/// wrong in one way a real one can be.
typedef struct {
    /// Samples a pixel, 1 or 4; a level that asks for four gets one from a machine that ignores it.
    uint32_t samples;
    /// The depth test. Off, a later triangle is drawn over an earlier one.
    bool depth;
    /// The light. Off, every face keeps its whole colour.
    bool lit;
    /// The faces turned away are culled.
    bool cull;
    /// How many frames late the uniform buffers it reads are.
    uint32_t frames_late;
    /// How many pixels to the right the picture lands.
    uint32_t shift;
} WimsyMachine;

/// The machine that draws a level as the level asks.
WimsyMachine WimsyMachineRight(uint32_t level);

/// Frame @p frame of @p frames at @p level as @p machine draws it: WIMSY_IMAGE_BYTES in row
/// order. Each sample takes the colour its nearest fragment has at the pixel's centre, rounded to
/// a byte, and a pixel the mean of its samples.
void WimsyRender(uint32_t level, uint32_t frame, uint32_t frames, const WimsyMachine *machine,
                 uint8_t *pixels);

/// How far, in pixels, an edge may be from where the reference puts it before a sample's answer
/// is another one: the reference moves every sample this far each way and allows whatever any of
/// those places would show. And how far apart, in depth, two surfaces must be before the reference
/// is sure which one hides the other.
#define WIMSY_EDGE_MARGIN (1.0 / 32.0)
#define WIMSY_DEPTH_MARGIN (1.0 / 262144.0)
/// How many codes past the range the reference allows a channel may be: one for the byte a float
/// rounds to, one for the mean of four samples' bytes.
#define WIMSY_CHANNEL_MARGIN 2u
/// A pixel whose range is no wider than this in every channel is held to its colour; a wider one
/// is on an edge, held to the colours beside it.
#define WIMSY_HELD_WIDTH 8u

/// What the judge made of a frame.
typedef struct {
    /// Pixels held to a colour, pixels on an edge, and pixels outside what they were held to.
    uint32_t held;
    uint32_t edge;
    uint32_t wrong;
    /// The first wrong pixel the judge met: where, what it holds and the range it was held to.
    uint32_t wrong_x;
    uint32_t wrong_y;
    uint8_t wrong_got[4];
    uint8_t wrong_low[4];
    uint8_t wrong_high[4];
    /// The farthest a channel is outside its range, in codes.
    uint32_t farthest;
    /// Every range is one value: the frame is held bit for bit (level 0).
    bool exact;
} WimsyVerdict;

/// The verdict a pixel gets in @p verdicts: the picture module's PICTURE_MATCHED, PICTURE_WRONG
/// and PICTURE_UNJUDGED, as numbers so this header needs no other.
#define WIMSY_PIXEL_HELD 0u
#define WIMSY_PIXEL_WRONG 1u
#define WIMSY_PIXEL_EDGE 2u

/// Holds @p got, a machine's frame @p frame of @p frames at @p level, to the reference. In
/// @p expected, when not NULL, WIMSY_IMAGE_BYTES: each pixel's nearest colour inside its range,
/// which is the machine's own where it is inside; in @p verdicts, when not NULL, WIMSY_PIXELS
/// verdicts.
void WimsyJudge(uint32_t level, uint32_t frame, uint32_t frames, const uint8_t *got,
                uint8_t *expected, uint8_t *verdicts, WimsyVerdict *out);

/// Whether @p verdict passes: no pixel outside its range.
bool WimsyVerdictPasses(const WimsyVerdict *verdict);

/// What a line says of @p verdict. Returns the length; the text is always terminated.
size_t WimsyDescribeVerdict(char *out, size_t out_size, const WimsyVerdict *verdict);

#ifdef __cplusplus
}
#endif
