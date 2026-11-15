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
/// the camera backs away from near to far over the run. Level 2 is a scene: seven of her down a
/// floor of two hundred units, textured with every level of detail, with a lane whose levels of
/// detail are each a colour, signs 5/256 of a unit in front of walls and marks painted on the
/// floor, so a frame shows what a machine does with the same things near and far. Level 3 is
/// that scene as heavy as a game's frame: a crowd of a hundred and sixty more of her, every
/// model drawn part by part, a draw and a uniform buffer each, fifteen hundred draws a frame,
/// over a floor whose texture is ASTC, 4096 texels a side. Level 4 is level 3 with that texture
/// 8192 a side.
///
/// A level's frame is not held to a hash: two machines that are both right differ in a shaded
/// frame's last bits and in which way a sample on an edge falls. Each judged frame is held to
/// bounds a pixel, worked out here (WimsyJudge).

#define WIMSY_LEVELS 5u

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

#define WIMSY_INSTANCES_MAX 168u

/// The models frame @p frame of a run of @p frames draws at @p level, in order. Returns how many.
uint32_t WimsyInstancesOf(uint32_t level, uint32_t frame, uint32_t frames, WimsyInstance *out);

/// The same for the models from @p first on, @p count of them at most, into @p out from its
/// start: a thread's share of a frame. Returns how many it wrote.
uint32_t WimsyInstancesFrom(uint32_t level, uint32_t frame, uint32_t frames, uint32_t first,
                            uint32_t count, WimsyInstance *out);

/// The light of @p level.
void WimsyLightOf(uint32_t level, WimsyLight *out);

/// Level 2's world, beside the models: quads whose corners are in the world, each vertex laid
/// out as the drawing suites' (gpu.h, GpuVertex): a position, a texture coordinate and a colour.
typedef struct {
    float x, y, z;
    float u, v;
    float r, g, b, a;
} WimsyWorldVertex;

/// What a run of the world's vertices is drawn with.
typedef enum {
    /// The floor's texture, every level of detail, a level between two weighed.
    WIMSY_MATERIAL_FLOOR,
    /// The lane's, whose levels are each one colour.
    WIMSY_MATERIAL_LANE,
    /// The vertex's colour.
    WIMSY_MATERIAL_FLAT,
    /// The vertex's colour, in the floor's own plane and brought in front of it by a depth bias.
    WIMSY_MATERIAL_MARK,
    /// The model's: its vertex's colour in the light. Not a world draw's.
    WIMSY_MATERIAL_MODEL,
} WimsyMaterial;

typedef struct {
    uint32_t first;
    uint32_t count;
    WimsyMaterial material;
} WimsyWorldDraw;

#define WIMSY_WORLD_VERTICES_MAX 1536u
#define WIMSY_WORLD_DRAWS_MAX 8u

/// The world of @p level, the same on every frame: its vertices, six a quad, into @p vertices
/// with how many in @p vertex_count, and its draws, in order, into @p draws. Returns how many
/// draws; none for a level with no world.
uint32_t WimsyWorldOf(uint32_t level, WimsyWorldVertex *vertices, uint32_t *vertex_count,
                      WimsyWorldDraw *draws);

/// The transform from the world to clip space on frame @p frame of @p frames at @p level, column
/// by column, which the world's draws read from their uniform buffer.
void WimsyViewOf(uint32_t level, uint32_t frame, uint32_t frames, float to_clip[16]);

/// The bias a mark's draw sets: this many of the depth buffer's steps and this many times the
/// depth's steepest change over a pixel, both towards the eye.
#define WIMSY_MARK_BIAS_UNITS -64.0f
#define WIMSY_MARK_BIAS_SLOPE -2.0f

/// The two textures, square, with every level down to one texel, their texels worked out from
/// where they are so no machine keeps a copy: the floor's is paving stones of 128 texels in
/// four greys, each smaller level the stones a fifth nearer their mean and every level from the
/// fifth that mean; the lane's is one colour a level, so the colour a pixel shows says the level
/// of detail it was drawn at.
typedef enum {
    WIMSY_TEXTURE_FLOOR,
    WIMSY_TEXTURE_LANE,
    /// The floor's at levels 3 and 4: the same stones in ASTC blocks of 8 by 8, 4096 and 8192
    /// texels a side.
    WIMSY_TEXTURE_FLOOR_4K,
    WIMSY_TEXTURE_FLOOR_8K,
    WIMSY_TEXTURES,
} WimsyTexture;

/// A side of level nought and how many levels.
uint32_t WimsyTextureSide(WimsyTexture texture);
uint32_t WimsyTextureLevels(WimsyTexture texture);

/// The floor's texture at @p level, and the texels a side of a block of @p texture: 8 for the
/// ASTC ones, nought for those of plain texels.
WimsyTexture WimsyFloorTexture(uint32_t level);
uint32_t WimsyTextureBlock(WimsyTexture texture);

/// The ASTC block at (@p bx, @p by), in blocks, of level @p level of an ASTC @p texture: a void
/// extent, every texel of it the colour WimsyTexel gives its first, which a stone's width makes
/// the colour of them all. Sixteen bytes.
void WimsyTextureBlockAt(WimsyTexture texture, uint32_t level, uint32_t bx, uint32_t by,
                         uint8_t out[16]);

/// How many codes more a channel of an ASTC texture's range gets each way: a machine whose host
/// samples no ASTC keeps the texture in another compressed format, and BC7 holds a flat block's
/// colour to within one.
#define WIMSY_ASTC_MARGIN 2.0

/// Texel (@p x, @p y) of level @p level of @p texture.
void WimsyTexel(WimsyTexture texture, uint32_t level, uint32_t x, uint32_t y, uint8_t out[4]);

/// How far the level of detail a machine picks may be from the bounds the specification gives
/// it (OpenGL 4.6, 8.14.1: no less than the larger of the coordinates' changes along an axis,
/// no more than their sum): an eighth of a level, for the precision it keeps a level in.
#define WIMSY_LOD_MARGIN 0.125

/// The heavy levels' panels, drawn last over the frame, each a pixel a texel with no depth
/// test, so every pixel of one is a texel's bytes: what a compute program wrote and what a
/// chain of small targets came to, both this frame's.
///
/// Sixteen textures of 128 by 128 with their eight levels, each level written every frame by a
/// dispatch of its own through a view of that level alone, then sampled through another: a
/// texel is a whole number worked out from its place, its texture, its level and the frame. A
/// panel shows a texture's level nought and, beside it, the seven others one under the other.
#define WIMSY_COMPUTE_TEXTURES 16u
#define WIMSY_COMPUTE_SIDE 128u
#define WIMSY_COMPUTE_LEVELS 8u
void WimsyComputeTexel(uint32_t texture, uint32_t level, uint32_t frame, uint32_t x, uint32_t y,
                       uint8_t out[4]);

/// Sixteen chains of eleven targets of 64 by 64: a chain's first target is cleared to the
/// chain's colour, each later one drawn with the one before, a texel a pixel, and every one
/// then gets a square of sixteen pixels whose place and colour follow from the chain, the step
/// and the frame. A panel shows a chain's last target.
#define WIMSY_CHAINS 16u
#define WIMSY_CHAIN_STEPS 11u
#define WIMSY_CHAIN_SIDE 64u
#define WIMSY_CHAIN_SQUARE 16u
typedef struct {
    uint32_t x;
    uint32_t y;
    uint8_t colour[4];
} WimsyChainSquare;
void WimsyChainClear(uint32_t chain, uint8_t out[4]);
void WimsyChainSquareOf(uint32_t chain, uint32_t step, uint32_t frame, WimsyChainSquare *out);
/// Texel (@p x, @p y) of chain @p chain's last target on frame @p frame.
void WimsyChainTexel(uint32_t chain, uint32_t frame, uint32_t x, uint32_t y, uint8_t out[4]);

/// Two textures a frame that no frame before it had, as a game streams its textures in: ASTC in
/// blocks of 8 by 8, 512 texels a side, written whole by the processor straight into memory the
/// graphics unit samples, in the frame that first draws with them. Load n is frame n / 2's, the
/// first or the second by n's last bit. Every block is a void extent whose colour follows from
/// its place and the load's number, so a texel of one is that colour whatever decodes it. A
/// panel shows a load's first 64 by 64 texels, eight blocks a side.
#define WIMSY_LOADS 2u
#define WIMSY_LOAD_SIDE 512u
#define WIMSY_LOAD_BLOCKS (WIMSY_LOAD_SIDE / 8u)
#define WIMSY_LOAD_SHOWN 64u
void WimsyLoadTexel(uint32_t load, uint32_t x, uint32_t y, uint8_t out[4]);
void WimsyLoadBlock(uint32_t load, uint32_t bx, uint32_t by, uint8_t out[16]);

/// What a panel shows: a level of a compute texture, a chain's last target, or one of the
/// frame's loads.
typedef enum {
    WIMSY_PANEL_COMPUTE,
    WIMSY_PANEL_CHAIN,
    WIMSY_PANEL_LOAD,
} WimsyPanelKind;

/// A panel's square on the target: where its top left pixel is, its side, and what it shows.
typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t side;
    WimsyPanelKind kind;
    uint32_t index;
    uint32_t mip;
} WimsyPanel;
#define WIMSY_PANELS \
    (WIMSY_COMPUTE_TEXTURES * WIMSY_COMPUTE_LEVELS + WIMSY_CHAINS + WIMSY_LOADS)

/// What the processor reads back of each frame's compute work: the one texel of the first
/// texture's last level.
#define WIMSY_PEEK_TEXTURE 0u
#define WIMSY_PEEK_LEVEL (WIMSY_COMPUTE_LEVELS - 1u)

/// The panels of @p level, in the order they are drawn. Returns how many: none under level 3.
uint32_t WimsyPanelsOf(uint32_t level, WimsyPanel *out);

/// Whether the pixel at (@p x, @p y) is a panel's at @p level, and then its bytes on frame
/// @p frame.
bool WimsyPanelPixel(uint32_t level, uint32_t frame, uint32_t x, uint32_t y, uint8_t out[4]);

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
    /// A level of detail between two weighs them. Off, it takes the nearer one.
    bool between_levels;
    /// How many levels of detail too coarse it picks.
    uint32_t coarser;
    /// The depth bias. Off, a mark is drawn where the floor is.
    bool bias;
    /// The bits its depth buffer keeps, nought for all a float has.
    uint32_t depth_bits;
    /// A face thinner than WIMSY_EDGE_MARGIN turned just away from the eye is drawn, as on a
    /// machine whose own corners, a fraction of a pixel from the reference's, turn it towards it.
    /// Not a fault: the judge has to pass it.
    bool thin_faces;
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
#define WIMSY_DEPTH_MARGIN (1.0 / 2097152.0)
/// How many codes past the range the reference allows a channel may be: one for the byte a float
/// rounds to, one for the mean of four samples' bytes.
#define WIMSY_CHANNEL_MARGIN 2u
/// A pixel whose range is no wider than this in every channel is held to its colour; a wider one
/// is on an edge, held to the colours beside it, or between levels of detail, held to theirs.
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
