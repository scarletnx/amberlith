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
/// that scene as heavy as a game's frame: a crowd of a hundred and sixty more of her, each one
/// of her five drawings and a little off its place by one fixed seed, every
/// model drawn part by part, a draw and a uniform buffer each, fifteen hundred draws a frame,
/// between hills and among grass that each come from a seed of their own, over a floor whose
/// texture is ASTC, 4096 texels a side. Level 4 is level 3 with that texture
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

/// Which of her drawings (wimsymesh.h, WIMSY_MESH_VARIANT) model @p index of @p level is: the
/// first for every model but the heavy levels' crowd, whose members are each one of them all, by
/// the level's seed.
uint32_t WimsyInstanceVariant(uint32_t level, uint32_t index);

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
    /// A screen on a wall: a texture shown as it is, the texel under the place and no other,
    /// which texture the draw says.
    WIMSY_MATERIAL_SCREEN,
} WimsyMaterial;

/// What a screen shows: a level of a compute texture, a chain's last target, one of the frame's
/// loads, or the plaques' texture.
typedef enum {
    WIMSY_SCREEN_COMPUTE,
    WIMSY_SCREEN_CHAIN,
    WIMSY_SCREEN_LOAD,
    WIMSY_SCREEN_PLAQUES,
} WimsyScreen;

typedef struct {
    uint32_t first;
    uint32_t count;
    WimsyMaterial material;
    /// A screen's texture: its kind, which of them, and for a compute texture which level.
    WimsyScreen screen;
    uint32_t index;
    uint32_t mip;
} WimsyWorldDraw;

#define WIMSY_WORLD_VERTICES_MAX 6000u
#define WIMSY_WORLD_DRAWS_MAX 160u
/// The hills of a heavy level: either side of the floor, this many columns of six units out
/// from it, a row a floor's row, two triangles a square, each one colour.
#define WIMSY_HILL_COLUMNS 8u
#define WIMSY_HILL_ROWS 34u
#define WIMSY_HILL_VERTICES (2u * WIMSY_HILL_COLUMNS * WIMSY_HILL_ROWS * 6u)

/// Where a heavy level's camera is and looks @p along its walk, from 0 at the run's first frame
/// to 1 at its last: a walk made to check the scene by. Down the lane between the grass; along
/// the left wall and its compute textures; round, and along the right wall, its chains and
/// loads; down the lane into the smoke and through it; then up, and round to look back at it
/// all. The judged frames, a quarter of the run apart, are the scene from its start, a wall
/// each, the smoke from inside and the scene from its far end. @p ahead is a unit long.
void WimsyWalkAt(float along, float eye[3], float ahead[3]);

/// The direction a camera looks with @p heading, of 65536 a turn from down the lane leftwards,
/// and @p pitch, of 65536 a turn upwards: what a person turning a camera gives WimsyViewFrom.
void WimsyLookAt(uint32_t heading, int32_t pitch, float ahead[3]);

/// Puts a heavy level's camera at @p eye looking along @p ahead, a unit long, until asked
/// otherwise, in place of its walk: a person's camera. No frame of such a run is the walk's, so
/// none is judged. WimsyCameraWalk gives the walk back.
void WimsyCameraAsk(const float eye[3], const float ahead[3]);
void WimsyCameraWalk(void);

/// The matrix from the world to clip space of a camera at @p eye looking along @p ahead, a unit
/// long and not straight up or down.
void WimsyViewFrom(const float eye[3], const float ahead[3], float to_clip[16]);

/// The world of @p level, the same on every frame: its vertices, six a quad, into @p vertices
/// with how many in @p vertex_count, and its draws, in order, into @p draws. Returns how many
/// draws; none for a level with no world. A heavy level's fifth draw is its hills: a field of
/// heights from one seed, worked out in whole numbers, rising from the floor's edge to 24 units,
/// each triangle the colour of its height in a light baked into it. After them its two walls,
/// one along each edge of the floor, and on the walls its screens, a draw a screen: on the left
/// every level of every compute texture, a texture every 3.25 units down the wall, on the right
/// every chain's last target and the frame's two loads; last, unless the legend was taken away,
/// one draw of a plaque under each texture, chain and load saying which it is.
uint32_t WimsyWorldOf(uint32_t level, WimsyWorldVertex *vertices, uint32_t *vertex_count,
                      WimsyWorldDraw *draws);

/// How high the hills of side @p side (0 left, 1 right) are at column @p column out from the
/// floor's edge and row @p row down it, in 256ths of a unit. Nought at column nought, where the
/// hills meet the floor, and past the field.
int32_t WimsyHillHeight(uint32_t side, uint32_t column, uint32_t row);

/// A blade of grass, as the instance buffer holds it: a triangle standing on the floor across
/// the lane's way, its foot WIMSY_BLADE_HALF_WIDTH either side of (x, z), its tip `height` up
/// and `lean` aside when the wind is still.
typedef struct {
    float x, z, height, lean;
    uint8_t colour[4];
} WimsyBlade;

/// How thick the grass is, from none to a field: a density is this many blades, the first of
/// the next density's, so a thicker field is the thinner one with blades between.
#define WIMSY_GRASS_DENSITIES 5u
#define WIMSY_BLADES_MAX 262144u
#define WIMSY_BLADE_HALF_WIDTH 0.0625f
/// How far a blade's tip sways either side in the wind, and how many frames a sway takes.
#define WIMSY_BLADE_SWAY 0.09375f
#define WIMSY_WIND_FRAMES 96u

/// The blades of @p density: none, 2,048, 16,384, 65,536 and 262,144. None past the last.
uint32_t WimsyBladeCount(uint32_t density);

/// The density @p level's grass has: what WimsyGrassAsk asked for, or the level's own, none
/// below the heavy levels, 3 at level 3 and 4 at level 4.
uint32_t WimsyGrassOf(uint32_t level);

/// Asks for @p density at every level that has grass, until asked otherwise;
/// WIMSY_GRASS_DENSITIES gives each level its own again.
void WimsyGrassAsk(uint32_t density);

/// Blades @p first to @p first + @p count of the field into @p out, the same on every run:
/// where each stands, how tall, how far it leans and its green, all from one seed. Returns how
/// many it wrote, fewer past the thickest density's last. One draw of three vertices and an
/// instance a blade draws a density.
uint32_t WimsyBladesOf(uint32_t first, uint32_t count, WimsyBlade *out);

/// The wind of frame @p frame: where in its sway the field is, from 0 to 1, the same for the
/// same frame on every run.
float WimsyWindOf(uint32_t frame);

/// Where corner @p corner of @p blade is in the world in wind @p wind: its left foot, its right
/// foot, its tip, which sways by where the blade stands. The arithmetic the vertex program does
/// on the same floats.
void WimsyBladeCorner(const WimsyBlade *blade, float wind, uint32_t corner, float out[3]);

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

/// What the processor reads back of each frame's compute work: the one texel of the first
/// texture's last level.
#define WIMSY_PEEK_TEXTURE 0u
#define WIMSY_PEEK_LEVEL (WIMSY_COMPUTE_LEVELS - 1u)

/// The plaques: one texture of all the screens' names, a plaque 128 by 32 texels, four a row.
/// Plaque n names compute texture n, then chain n - 16, then load n - 32: two lines of the
/// 8 by 16 font, sixteen characters each.
#define WIMSY_PLAQUES (WIMSY_COMPUTE_TEXTURES + WIMSY_CHAINS + WIMSY_LOADS)
#define WIMSY_PLAQUE_WIDTH 128u
#define WIMSY_PLAQUE_HEIGHT 32u
#define WIMSY_PLAQUES_SIDE 512u
void WimsyPlaqueTexel(uint32_t x, uint32_t y, uint8_t out[4]);

/// Whether the plaques are drawn: they are unless WimsyLegendAsk(false) took them away.
void WimsyLegendAsk(bool on);
bool WimsyLegendOf(void);

/// A puff of smoke, as the instance buffer holds it: a square that faces the camera, its middle
/// in the world and its half side in the world's units, its grey, and in the colour's last byte
/// how much of what is behind it the middle hides. It thins to nothing at its rim: at (u, v)
/// from the middle, each from -1 to 1, it hides that much times 1 - u*u - v*v, or nothing.
typedef struct {
    float x, y, z, size;
    uint8_t colour[4];
} WimsyPuff;

/// How thick the smoke is: none, 64, 256 and 1,024 puffs, a density's the first of the next's.
#define WIMSY_SMOKE_DENSITIES 4u
#define WIMSY_PUFFS_MAX 1024u
/// How many frames a puff takes to rise, thicken and thin away before it starts again.
#define WIMSY_PUFF_FRAMES 240u

uint32_t WimsyPuffCount(uint32_t density);

/// The density @p level's smoke has: what WimsySmokeAsk asked for, or the level's own, none
/// below the heavy levels, 2 at level 3 and 3 at level 4.
uint32_t WimsySmokeOf(uint32_t level);

/// Asks for @p density at every level that has smoke; WIMSY_SMOKE_DENSITIES gives each level
/// its own again.
void WimsySmokeAsk(uint32_t density);

/// The first @p count puffs as frame @p frame has them, in the order they are drawn, into
/// @p out: each rises and drifts from where one seed puts it, by the frame's number alone.
/// Returns how many it wrote.
uint32_t WimsyPuffsOf(uint32_t count, uint32_t frame, WimsyPuff *out);

/// What a puff's half side is multiplied by to give its reach in clip space, across and up.
void WimsyPuffScale(float out[2]);

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
    /// The world's draws it leaves out, a bit a draw for the world's first thirty, and
    /// WIMSY_LEAVES_OUT_GRASS for the grass: nought draws them all.
    uint32_t left_out;
    /// The wind. Off, the grass stands as frame nought has it.
    bool wind;
    /// The smoke. Off, none is drawn.
    bool smoke;
    /// The smoke is behind what is nearer. Off, a puff is drawn over everything.
    bool smoke_depth;
    /// How many 128ths of a pixel to the right its samples are of where the level puts them, as
    /// on a machine whose corners land a fraction of a pixel from the reference's. Under
    /// WIMSY_EDGE_MARGIN it is not a fault: the judge has to pass it.
    uint32_t nudge;
    /// The screens show the frame's textures. Off, they show the frame before's.
    bool screens;
} WimsyMachine;

#define WIMSY_LEAVES_OUT_GRASS 0x80000000u

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
