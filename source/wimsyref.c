// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "coverageref.h"
#include "wimsymesh.h"
#include "wimsyref.h"

const uint8_t WIMSY_CLEAR[4] = { 27u, 27u, 31u, 255u };

const char *WimsyLevelName(uint32_t level) {
    static const char *const NAMES[WIMSY_LEVELS] = { "sprites", "one lit model" };
    return level < WIMSY_LEVELS ? NAMES[level] : "no level";
}

uint32_t WimsyJudgedFrames(uint32_t frames, uint32_t out[WIMSY_JUDGED]) {
    uint32_t count = 0;
    for (uint32_t j = 0; j < WIMSY_JUDGED && frames > 0u; ++j) {
        const uint32_t frame = (uint32_t)((uint64_t)j * (frames - 1u) / (WIMSY_JUDGED - 1u));
        if (count == 0u || out[count - 1u] != frame) {
            out[count++] = frame;
        }
    }
    return count;
}

uint32_t WimsyPathAt(uint32_t frame, uint32_t frames) {
    if (frames < 2u || frame >= frames) {
        return frames < 2u ? 0u : 65536u;
    }
    return (uint32_t)((uint64_t)frame * 65536u / (frames - 1u));
}

#define Q30 ((int64_t)1 << 30)

static int64_t Mul30(int64_t a, int64_t b) {
    return (a * b) >> 30;
}

void WimsySinCos(uint32_t turn, int32_t *sine, int32_t *cosine) {
    const uint32_t quarter = (turn >> 14) & 3u;
    const uint32_t within = turn & 0x3FFFu;
    // Past an eighth of a turn the two swap, so the series only ever sums up to pi / 4.
    const bool swapped = within > 8192u;
    const uint32_t eighth = swapped ? 16384u - within : within;
    // 2 pi in 2^24ths, so the angle in radians is whole in 2^30ths after ten bits are dropped.
    const int64_t x = ((int64_t)eighth * 105414358) >> 10;
    const int64_t x2 = Mul30(x, x);
    int64_t s = Q30 - x2 / 72;
    s = Q30 - Mul30(x2, s) / 42;
    s = Q30 - Mul30(x2, s) / 20;
    s = Q30 - Mul30(x2, s) / 6;
    s = Mul30(x, s);
    int64_t c = Q30 - x2 / 90;
    c = Q30 - Mul30(x2, c) / 56;
    c = Q30 - Mul30(x2, c) / 30;
    c = Q30 - Mul30(x2, c) / 12;
    c = Q30 - Mul30(x2, c) / 2;
    const int64_t s0 = swapped ? c : s;
    const int64_t c0 = swapped ? s : c;
    const int64_t sines[4] = { s0, c0, -s0, -c0 };
    const int64_t cosines[4] = { c0, -s0, -c0, s0 };
    *sine = (int32_t)sines[quarter];
    *cosine = (int32_t)cosines[quarter];
}

// Level 0.

/// A mesh corner on the sprite's grid: twice the texel grid's resolution in every model unit, so
/// a texel's centre is a whole number too.
static int64_t Edge(int64_t ax, int64_t ay, int64_t bx, int64_t by, int64_t px, int64_t py) {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

void WimsySpriteTexels(uint8_t *texels) {
    memset(texels, 0, WIMSY_SPRITE_BYTES);
    // A texel's centre in the mesh's whole numbers: the model's -1 to 1 is WIMSY_SPRITE texels.
    const int64_t step = 2 * WIMSY_MESH_SCALE / (int64_t)WIMSY_SPRITE;
    for (uint32_t t = 0; t + 2u < WIMSY_MESH_INDICES; t += 3u) {
        const WimsyMeshVertex *const a = &WIMSY_MESH_VERTEX[WIMSY_MESH_INDEX[t]];
        const WimsyMeshVertex *const b = &WIMSY_MESH_VERTEX[WIMSY_MESH_INDEX[t + 1u]];
        const WimsyMeshVertex *const c = &WIMSY_MESH_VERTEX[WIMSY_MESH_INDEX[t + 2u]];
        // Counter clockwise with y up is facing the eye; the others are behind it.
        if (Edge(a->x, a->y, b->x, b->y, c->x, c->y) <= 0) {
            continue;
        }
        const uint8_t *const colour = WIMSY_MESH_PALETTE[a->colour];
        for (uint32_t row = 0; row < WIMSY_SPRITE; ++row) {
            const int64_t py = WIMSY_MESH_SCALE - ((int64_t)row * step + step / 2);
            for (uint32_t column = 0; column < WIMSY_SPRITE; ++column) {
                const int64_t px = (int64_t)column * step + step / 2 - WIMSY_MESH_SCALE;
                if (Edge(a->x, a->y, b->x, b->y, px, py) >= 0 &&
                    Edge(b->x, b->y, c->x, c->y, px, py) >= 0 &&
                    Edge(c->x, c->y, a->x, a->y, px, py) >= 0) {
                    uint8_t *const texel = texels + ((size_t)row * WIMSY_SPRITE + column) * 4u;
                    texel[0] = colour[0];
                    texel[1] = colour[1];
                    texel[2] = colour[2];
                    texel[3] = 255u;
                }
            }
        }
    }
}

void WimsySpritesOf(const GameWorld *world, WimsySprite out[WIMSY_SPRITES]) {
    for (uint32_t d = 0; d < WIMSY_SPRITES; ++d) {
        // The world is 256 by 256 in 16.16; a sprite's corner runs from one sprite off the top
        // left to the bottom right corner, so the edges cut sprites too.
        out[d].x = (int32_t)(((int64_t)world->x[d] * (WIMSY_WIDTH + WIMSY_SPRITE)) >> 24) -
                   (int32_t)WIMSY_SPRITE;
        out[d].y = (int32_t)(((int64_t)world->y[d] * (WIMSY_HEIGHT + WIMSY_SPRITE)) >> 24) -
                   (int32_t)WIMSY_SPRITE;
        out[d].side = d % 4u == 0u ? WIMSY_SPRITE * 2u : WIMSY_SPRITE;
        out[d].mirrored = (d >> 2 & 1u) != 0u;
    }
}

/// The sprites' image inside the rectangle at @p x0, @p y0, @p width by @p height, @p stride
/// pixels a row of @p pixels.
static void SpritesInto(const WimsySprite *sprites, uint32_t count, const uint8_t *texels,
                        int32_t x0, int32_t y0, uint32_t width, uint32_t height, uint8_t *pixels,
                        uint32_t stride) {
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            memcpy(pixels + ((size_t)y * stride + x) * 4u, WIMSY_CLEAR, 4u);
        }
    }
    for (uint32_t i = 0; i < count; ++i) {
        const WimsySprite *const sprite = &sprites[i];
        const int32_t side = (int32_t)sprite->side;
        const int32_t left = sprite->x > x0 ? sprite->x : x0;
        const int32_t top = sprite->y > y0 ? sprite->y : y0;
        const int32_t right =
            sprite->x + side < x0 + (int32_t)width ? sprite->x + side : x0 + (int32_t)width;
        const int32_t bottom =
            sprite->y + side < y0 + (int32_t)height ? sprite->y + side : y0 + (int32_t)height;
        const uint32_t scale = sprite->side / WIMSY_SPRITE;
        for (int32_t y = top; y < bottom; ++y) {
            const uint32_t v = (uint32_t)(y - sprite->y) / scale;
            for (int32_t x = left; x < right; ++x) {
                const uint32_t across = (uint32_t)(x - sprite->x) / scale;
                const uint32_t u = sprite->mirrored ? WIMSY_SPRITE - 1u - across : across;
                const uint8_t *const texel = texels + ((size_t)v * WIMSY_SPRITE + u) * 4u;
                if (texel[3] != 0u) {
                    memcpy(pixels + ((size_t)(y - y0) * stride + (uint32_t)(x - x0)) * 4u, texel,
                           4u);
                }
            }
        }
    }
}

void WimsySpritesImage(const WimsySprite *sprites, uint32_t count, const uint8_t *texels,
                       uint8_t *pixels) {
    SpritesInto(sprites, count, texels, 0, 0, WIMSY_WIDTH, WIMSY_HEIGHT, pixels, WIMSY_WIDTH);
}

// Levels 1 and up.

/// The camera: the tangent of half its vertical angle is a half, and what it sees lies between
/// these two distances, mapped to depths nought and one.
#define VIEW_FOCAL 2.0f
#define VIEW_NEAR 0.5f
#define VIEW_FAR 256.0f
/// Level 1: how far the camera is at the path's start and how much farther at its end, the
/// model's turn a frame (a two hundred and fortieth of a turn) and its lean towards the eye.
#define ALONE_NEAR 3
#define ALONE_AWAY 45
#define ALONE_TURN 273u
#define ALONE_LEAN 2048u

static float FromQ30(int32_t value) {
    return (float)value / 1073741824.0f;
}

/// The instance of a model turned by @p rows, its middle at the world's and the camera @p away
/// from it down the z axis, looking at it.
static void InstanceFacing(WimsyInstance *out, const float rows[3][3], float away) {
    const float aspect = (float)WIMSY_WIDTH / (float)WIMSY_HEIGHT;
    const float deep = VIEW_FAR / (VIEW_NEAR - VIEW_FAR);
    memset(out, 0, sizeof(*out));
    for (uint32_t column = 0; column < 3u; ++column) {
        out->to_clip[column * 4u + 0u] = VIEW_FOCAL / aspect * rows[0][column];
        out->to_clip[column * 4u + 1u] = VIEW_FOCAL * rows[1][column];
        out->to_clip[column * 4u + 2u] = deep * rows[2][column];
        out->to_clip[column * 4u + 3u] = -rows[2][column];
    }
    out->to_clip[12u + 2u] = deep * -away + VIEW_NEAR * deep;
    out->to_clip[12u + 3u] = away;
    for (uint32_t row = 0; row < 3u; ++row) {
        for (uint32_t column = 0; column < 3u; ++column) {
            out->turn[row][column] = rows[row][column];
        }
    }
    out->tint[0] = out->tint[1] = out->tint[2] = out->tint[3] = 1.0f;
}

uint32_t WimsyInstancesOf(uint32_t level, uint32_t frame, uint32_t frames, WimsyInstance *out) {
    if (level != 1u) {
        return 0;
    }
    int32_t s = 0, c = 0, lean_s = 0, lean_c = 0;
    WimsySinCos(frame * ALONE_TURN, &s, &c);
    WimsySinCos(ALONE_LEAN, &lean_s, &lean_c);
    // A turn about the model's own y, then the lean about x that brings its top towards the eye.
    const float rows[3][3] = {
        { FromQ30(c), 0.0f, FromQ30(s) },
        { FromQ30(lean_s) * FromQ30(s), FromQ30(lean_c), -FromQ30(lean_s) * FromQ30(c) },
        { -FromQ30(lean_c) * FromQ30(s), FromQ30(lean_s), FromQ30(lean_c) * FromQ30(c) },
    };
    // The square of the way along, so the camera leaves slowly and the model is large for longer.
    const uint64_t along = WimsyPathAt(frame, frames);
    const uint32_t away = (uint32_t)ALONE_NEAR * 65536u +
                          (uint32_t)(((uint64_t)ALONE_AWAY * along * along) >> 16);
    InstanceFacing(&out[0], rows, (float)away / 65536.0f);
    return 1;
}

void WimsyLightOf(uint32_t level, WimsyLight *out) {
    (void)level;
    // From above the eye's right shoulder; a face turned away keeps two fifths of its colour.
    const WimsyLight light = { { 0.30f, 0.45f, 0.84f, 0.0f }, { 0.40f, 0.60f, 0.0f, 0.0f } };
    *out = light;
}

WimsyMachine WimsyMachineRight(uint32_t level) {
    const WimsyMachine machine = { level == 0u ? 1u : 4u, level != 0u, true, true, 0u, 0u };
    return machine;
}

// The reference rasteriser. A frame is worked out a tile at a time, so what it keeps a sample is
// a tile's worth whatever the target's size.

#define TILE 32u
#define SAMPLES_MAX 4u
/// A sample and the four places WIMSY_EDGE_MARGIN from it.
#define PLACES 5u
static const double PLACE[PLACES][2] = {
    { 0.0, 0.0 }, { 1.0, 0.0 }, { -1.0, 0.0 }, { 0.0, 1.0 }, { 0.0, -1.0 },
};

/// A model's corner in the target: its pixel, its depth, one over its w, and its normal turned
/// into the world.
typedef struct {
    double x, y, z, over_w;
    double normal[3];
} Corner;

/// A triangle set up for a tile: its three edge functions, positive inside whichever way it is
/// wound, its depth as a plane over the target, and what its shading interpolates.
typedef struct {
    double a[3], b[3], c[3];
    /// How much an edge function changes over WIMSY_EDGE_MARGIN at most, and the depth; and
    /// over a pixel, from its centre to any sample's farthest place.
    double reach[3];
    double pixel_reach[3];
    double depth_x, depth_y, depth_0, depth_reach;
    double over_w[3];
    double normal[3][3];
    double colour[3];
    double left, right, top, bottom;
    /// Facing the eye, by more than a rounding could undo.
    bool front;
    /// Not facing away by more than a rounding could undo: it may be drawn.
    bool maybe;
} Triangle;

typedef struct {
    /// The nearest fragment at each of the sample's places, FLT_MAX where there is none.
    float nearest[PLACES];
    /// The depth nothing farther can show through: the farthest of the five when every place
    /// has a fragment, FLT_MAX when one has none and the clear colour may show.
    float cover;
    /// The range of the colours that may show.
    uint8_t low[3];
    uint8_t high[3];
    bool any;
} Sample;

/// What the ideal machine keeps a sample.
typedef struct {
    float depth;
    uint8_t colour[3];
} Drawn;

static Corner corners[WIMSY_MESH_VERTICES];
static Sample samples[TILE * TILE * SAMPLES_MAX];
static Drawn drawn[TILE * TILE * SAMPLES_MAX];
static uint8_t tile_low[TILE * TILE][4];
static uint8_t tile_high[TILE * TILE][4];
static WimsyInstance scene_instance[WIMSY_INSTANCES_MAX];
static uint8_t sprite_texels[WIMSY_SPRITE_BYTES];
static bool sprite_made;
static GameWorld sprite_world;
static WimsySprite frame_sprites[WIMSY_SPRITES];

/// A frame as the reference draws it.
typedef struct {
    uint32_t level;
    uint32_t sample_count;
    uint32_t instances;
    WimsyLight light;
    WimsyMachine machine;
    /// Which instance `corners` holds, and the target rectangle its corners span.
    uint32_t held;
    double left, right, top, bottom;
    /// Where each sample is inside a pixel, by the pixel's place in the 2 by 2 grid the sample
    /// locations are programmed over: the standard ones for four samples, the centre for one.
    double place[2][2][SAMPLES_MAX][2];
} Scene;

static void SamplePlaces(Scene *scene) {
    for (uint32_t py = 0; py < 2u; ++py) {
        for (uint32_t px = 0; px < 2u; ++px) {
            for (uint32_t s = 0; s < SAMPLES_MAX; ++s) {
                const uint8_t location = scene->sample_count == 4u
                                             ? CoverageLocation(COVERAGE_STANDARD_4X, 4u, px, py, s)
                                             : 0x88u;
                scene->place[py][px][s][0] = (double)(location & 0xFu) / 16.0;
                scene->place[py][px][s][1] = (double)(location >> 4) / 16.0;
            }
        }
    }
}

/// Where sample @p s of the pixel at (@p px, @p py) is inside it.
static void SamplePlace(const Scene *scene, uint32_t px, uint32_t py, uint32_t s, double *x,
                        double *y) {
    *x = scene->place[py & 1u][px & 1u][s][0];
    *y = scene->place[py & 1u][px & 1u][s][1];
}

/// Turns instance @p index's model into the target.
static void HoldInstance(Scene *scene, uint32_t index) {
    const WimsyInstance *const instance = &scene_instance[index];
    const float *const m = instance->to_clip;
    scene->left = scene->top = DBL_MAX;
    scene->right = scene->bottom = -DBL_MAX;
    for (uint32_t v = 0; v < WIMSY_MESH_VERTICES; ++v) {
        const WimsyMeshVertex *const vertex = &WIMSY_MESH_VERTEX[v];
        // The floats the vertex buffer holds, through the matrix the uniform buffer holds.
        const double x = (double)((float)vertex->x / (float)WIMSY_MESH_SCALE);
        const double y = (double)((float)vertex->y / (float)WIMSY_MESH_SCALE);
        const double z = (double)((float)vertex->z / (float)WIMSY_MESH_SCALE);
        const double cx = (double)m[0] * x + (double)m[4] * y + (double)m[8] * z + (double)m[12];
        const double cy = (double)m[1] * x + (double)m[5] * y + (double)m[9] * z + (double)m[13];
        const double cz = (double)m[2] * x + (double)m[6] * y + (double)m[10] * z + (double)m[14];
        const double cw = (double)m[3] * x + (double)m[7] * y + (double)m[11] * z + (double)m[15];
        Corner *const corner = &corners[v];
        corner->over_w = 1.0 / cw;
        corner->x = (cx * corner->over_w + 1.0) * 0.5 * (double)WIMSY_WIDTH;
        corner->y = (1.0 - cy * corner->over_w) * 0.5 * (double)WIMSY_HEIGHT;
        corner->z = cz * corner->over_w;
        const double n[3] = { (double)((float)vertex->nx / 127.0f),
                              (double)((float)vertex->ny / 127.0f),
                              (double)((float)vertex->nz / 127.0f) };
        for (uint32_t row = 0; row < 3u; ++row) {
            corner->normal[row] = (double)instance->turn[row][0] * n[0] +
                                  (double)instance->turn[row][1] * n[1] +
                                  (double)instance->turn[row][2] * n[2];
        }
        scene->left = corner->x < scene->left ? corner->x : scene->left;
        scene->right = corner->x > scene->right ? corner->x : scene->right;
        scene->top = corner->y < scene->top ? corner->y : scene->top;
        scene->bottom = corner->y > scene->bottom ? corner->y : scene->bottom;
    }
    scene->held = index;
}

static double Least(double a, double b, double c) {
    const double least = a < b ? a : b;
    return least < c ? least : c;
}

static double Most(double a, double b, double c) {
    const double most = a > b ? a : b;
    return most > c ? most : c;
}

/// Sets up triangle @p t of the instance held. False when it lies outside the tile at
/// (@p x0, @p y0) or has no area at all.
static bool SetUp(const Scene *scene, uint32_t t, double x0, double y0, Triangle *out) {
    const Corner *const v[3] = { &corners[WIMSY_MESH_INDEX[t]], &corners[WIMSY_MESH_INDEX[t + 1u]],
                                 &corners[WIMSY_MESH_INDEX[t + 2u]] };
    out->left = Least(v[0]->x, v[1]->x, v[2]->x) - WIMSY_EDGE_MARGIN;
    out->right = Most(v[0]->x, v[1]->x, v[2]->x) + WIMSY_EDGE_MARGIN;
    out->top = Least(v[0]->y, v[1]->y, v[2]->y) - WIMSY_EDGE_MARGIN;
    out->bottom = Most(v[0]->y, v[1]->y, v[2]->y) + WIMSY_EDGE_MARGIN;
    if (out->right < x0 || out->left > x0 + (double)TILE || out->bottom < y0 ||
        out->top > y0 + (double)TILE) {
        return false;
    }
    // Twice the area with y downwards: negative for a triangle counter clockwise in clip space,
    // which is the front (the fixed suite's faces case).
    const double area = (v[1]->x - v[0]->x) * (v[2]->y - v[0]->y) -
                        (v[1]->y - v[0]->y) * (v[2]->x - v[0]->x);
    const double size = fabs(area);
    if (size < 1e-12) {
        return false;
    }
    out->front = area < -1e-7;
    out->maybe = !scene->machine.cull || area < 1e-7;
    const double sign = area < 0.0 ? -1.0 : 1.0;
    for (uint32_t i = 0; i < 3u; ++i) {
        // Edge i is the one across from corner i, so its function over the area is that corner's
        // weight.
        const Corner *const from = v[(i + 1u) % 3u];
        const Corner *const to = v[(i + 2u) % 3u];
        out->a[i] = -(to->y - from->y) * sign;
        out->b[i] = (to->x - from->x) * sign;
        out->c[i] = -(out->a[i] * from->x + out->b[i] * from->y);
        const double steepest = fabs(out->a[i]) > fabs(out->b[i]) ? fabs(out->a[i])
                                                                 : fabs(out->b[i]);
        out->reach[i] = steepest * WIMSY_EDGE_MARGIN;
        out->pixel_reach[i] = (fabs(out->a[i]) + fabs(out->b[i])) * (0.5 + WIMSY_EDGE_MARGIN);
        out->over_w[i] = v[i]->over_w / size;
        memcpy(out->normal[i], v[i]->normal, sizeof(out->normal[i]));
    }
    out->depth_x = (out->a[0] * v[0]->z + out->a[1] * v[1]->z + out->a[2] * v[2]->z) / size;
    out->depth_y = (out->b[0] * v[0]->z + out->b[1] * v[1]->z + out->b[2] * v[2]->z) / size;
    out->depth_0 = (out->c[0] * v[0]->z + out->c[1] * v[1]->z + out->c[2] * v[2]->z) / size;
    const double steepest = fabs(out->depth_x) > fabs(out->depth_y) ? fabs(out->depth_x)
                                                                    : fabs(out->depth_y);
    out->depth_reach = steepest * WIMSY_EDGE_MARGIN;
    const uint8_t *const colour = WIMSY_MESH_PALETTE[WIMSY_MESH_VERTEX[WIMSY_MESH_INDEX[t]].colour];
    const float *const tint = scene_instance[scene->held].tint;
    for (uint32_t k = 0; k < 3u; ++k) {
        // The byte as the vertex buffer's normalised attribute gives it, times the tint.
        out->colour[k] = (double)((float)colour[k] / 255.0f) * (double)tint[k];
    }
    return true;
}

/// The colour triangle @p t has at (@p x, @p y), nought to 255 a channel: its range, since a
/// point too far outside it to weigh its corners may get any shade of its colour.
static void Shade(const Scene *scene, const Triangle *t, double x, double y, double low[3],
                  double high[3]) {
    double dark = 1.0, lit = 1.0;
    if (scene->machine.lit) {
        const double shade = (double)scene->light.shade[0];
        const double more = (double)scene->light.shade[1];
        double weight[3];
        double sum = 0.0;
        for (uint32_t i = 0; i < 3u; ++i) {
            weight[i] = (t->a[i] * x + t->b[i] * y + t->c[i]) * t->over_w[i];
            sum += weight[i];
        }
        double normal[3] = { 0.0, 0.0, 0.0 };
        for (uint32_t i = 0; i < 3u; ++i) {
            for (uint32_t k = 0; k < 3u; ++k) {
                normal[k] += weight[i] * t->normal[i][k];
            }
        }
        const double length =
            sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
        dark = shade;
        lit = shade + more;
        if (sum > 0.0 && length > 1e-9 * sum) {
            const double facing = (normal[0] * (double)scene->light.light[0] +
                                   normal[1] * (double)scene->light.light[1] +
                                   normal[2] * (double)scene->light.light[2]) /
                                  length;
            dark = lit = shade + more * (facing > 0.0 ? facing : 0.0);
        }
    }
    for (uint32_t k = 0; k < 3u; ++k) {
        const double least = t->colour[k] * dark * 255.0;
        const double most = t->colour[k] * lit * 255.0;
        low[k] = least < 0.0 ? 0.0 : least > 255.0 ? 255.0 : least;
        high[k] = most < 0.0 ? 0.0 : most > 255.0 ? 255.0 : most;
    }
}

static void Widen(Sample *sample, const double low[3], const double high[3]) {
    for (uint32_t k = 0; k < 3u; ++k) {
        const uint8_t least = (uint8_t)floor(low[k]);
        const uint8_t most = (uint8_t)ceil(high[k]);
        if (!sample->any || least < sample->low[k]) {
            sample->low[k] = least;
        }
        if (!sample->any || most > sample->high[k]) {
            sample->high[k] = most;
        }
    }
    sample->any = true;
}

/// The pixels of the tile at (@p x0, @p y0) a triangle's rectangle reaches.
typedef struct {
    uint32_t left, right, top, bottom;
} Span;

static bool SpanOf(const Triangle *t, double x0, double y0, Span *out) {
    const double left = floor(t->left - x0), top = floor(t->top - y0);
    const double right = floor(t->right - x0), bottom = floor(t->bottom - y0);
    if (right < 0.0 || bottom < 0.0 || left >= (double)TILE || top >= (double)TILE) {
        return false;
    }
    out->left = left < 0.0 ? 0u : (uint32_t)left;
    out->top = top < 0.0 ? 0u : (uint32_t)top;
    out->right = right >= (double)TILE ? TILE - 1u : (uint32_t)right;
    out->bottom = bottom >= (double)TILE ? TILE - 1u : (uint32_t)bottom;
    return true;
}

/// Whether no sample of the pixel at (@p x, @p y), nor any place the margin moves one to, can be
/// inside @p t.
static bool PixelOutside(const Triangle *t, double x, double y) {
    return t->a[0] * (x + 0.5) + t->b[0] * (y + 0.5) + t->c[0] + t->pixel_reach[0] < 0.0 ||
           t->a[1] * (x + 0.5) + t->b[1] * (y + 0.5) + t->c[1] + t->pixel_reach[1] < 0.0 ||
           t->a[2] * (x + 0.5) + t->b[2] * (y + 0.5) + t->c[2] + t->pixel_reach[2] < 0.0;
}

/// For every triangle of every instance that reaches the tile, @p each.
typedef void (*EachTriangle)(const Scene *scene, const Triangle *t, uint32_t x0, uint32_t y0);

static void OverTile(Scene *scene, uint32_t x0, uint32_t y0, EachTriangle each) {
    static Triangle triangle;
    for (uint32_t i = 0; i < scene->instances; ++i) {
        if (scene->held != i) {
            HoldInstance(scene, i);
        }
        if (scene->right + 1.0 < (double)x0 || scene->left - 1.0 > (double)(x0 + TILE) ||
            scene->bottom + 1.0 < (double)y0 || scene->top - 1.0 > (double)(y0 + TILE)) {
            continue;
        }
        for (uint32_t t = 0; t + 2u < WIMSY_MESH_INDICES; t += 3u) {
            if (SetUp(scene, t, (double)x0, (double)y0, &triangle)) {
                each(scene, &triangle, x0, y0);
            }
        }
    }
}

/// The first pass: the nearest fragment at each place of each sample, from the triangles that
/// face the eye.
static void PassNearest(const Scene *scene, const Triangle *t, uint32_t x0, uint32_t y0) {
    Span span;
    if (!t->front || !SpanOf(t, (double)x0, (double)y0, &span)) {
        return;
    }
    for (uint32_t py = span.top; py <= span.bottom; ++py) {
        for (uint32_t px = span.left; px <= span.right; ++px) {
            if (PixelOutside(t, (double)(x0 + px), (double)(y0 + py))) {
                continue;
            }
            for (uint32_t s = 0; s < scene->sample_count; ++s) {
                double sx, sy;
                SamplePlace(scene, x0 + px, y0 + py, s, &sx, &sy);
                Sample *const sample = &samples[(py * TILE + px) * SAMPLES_MAX + s];
                for (uint32_t place = 0; place < PLACES; ++place) {
                    const double x = (double)(x0 + px) + sx + PLACE[place][0] * WIMSY_EDGE_MARGIN;
                    const double y = (double)(y0 + py) + sy + PLACE[place][1] * WIMSY_EDGE_MARGIN;
                    if (t->a[0] * x + t->b[0] * y + t->c[0] < 0.0 ||
                        t->a[1] * x + t->b[1] * y + t->c[1] < 0.0 ||
                        t->a[2] * x + t->b[2] * y + t->c[2] < 0.0) {
                        continue;
                    }
                    const float depth = (float)(t->depth_x * x + t->depth_y * y + t->depth_0);
                    if (depth < sample->nearest[place]) {
                        sample->nearest[place] = depth;
                    }
                }
            }
        }
    }
}

/// The second pass: every triangle that comes within the margin of a sample and is not surely
/// behind what covers it adds its colour there to the sample's range.
static void PassRange(const Scene *scene, const Triangle *t, uint32_t x0, uint32_t y0) {
    Span span;
    if (!t->maybe || !SpanOf(t, (double)x0, (double)y0, &span)) {
        return;
    }
    for (uint32_t py = span.top; py <= span.bottom; ++py) {
        for (uint32_t px = span.left; px <= span.right; ++px) {
            if (PixelOutside(t, (double)(x0 + px), (double)(y0 + py))) {
                continue;
            }
            bool centre_shaded = false;
            double centre_low[3], centre_high[3];
            for (uint32_t s = 0; s < scene->sample_count; ++s) {
                double sx, sy;
                SamplePlace(scene, x0 + px, y0 + py, s, &sx, &sy);
                const double x = (double)(x0 + px) + sx;
                const double y = (double)(y0 + py) + sy;
                if (x < t->left || x > t->right || y < t->top || y > t->bottom ||
                    t->a[0] * x + t->b[0] * y + t->c[0] + t->reach[0] < 0.0 ||
                    t->a[1] * x + t->b[1] * y + t->c[1] + t->reach[1] < 0.0 ||
                    t->a[2] * x + t->b[2] * y + t->c[2] + t->reach[2] < 0.0) {
                    continue;
                }
                Sample *const sample = &samples[(py * TILE + px) * SAMPLES_MAX + s];
                const double depth = t->depth_x * x + t->depth_y * y + t->depth_0;
                if (scene->machine.depth && sample->cover < FLT_MAX &&
                    depth - t->depth_reach > (double)sample->cover + WIMSY_DEPTH_MARGIN) {
                    continue;
                }
                // A machine shades a pixel once, at its centre or wherever in the pixel it
                // likes when it multisamples: the range runs from one to the other.
                if (!centre_shaded) {
                    Shade(scene, t, (double)(x0 + px) + 0.5, (double)(y0 + py) + 0.5, centre_low,
                          centre_high);
                    centre_shaded = true;
                }
                double low[3], high[3];
                Shade(scene, t, x, y, low, high);
                for (uint32_t k = 0; k < 3u; ++k) {
                    low[k] = low[k] < centre_low[k] ? low[k] : centre_low[k];
                    high[k] = high[k] > centre_high[k] ? high[k] : centre_high[k];
                }
                Widen(sample, low, high);
            }
        }
    }
}

/// The ideal machine's pass: each sample takes the nearest fragment's colour at the pixel's
/// centre, or the last one's when the machine has no depth test.
static void PassDraw(const Scene *scene, const Triangle *t, uint32_t x0, uint32_t y0) {
    Span span;
    if ((scene->machine.cull && !t->front) || !SpanOf(t, (double)x0, (double)y0, &span)) {
        return;
    }
    for (uint32_t py = span.top; py <= span.bottom; ++py) {
        for (uint32_t px = span.left; px <= span.right; ++px) {
            if (PixelOutside(t, (double)(x0 + px), (double)(y0 + py))) {
                continue;
            }
            bool shaded = false;
            uint8_t colour[3] = { 0, 0, 0 };
            for (uint32_t s = 0; s < scene->sample_count; ++s) {
                double sx, sy;
                SamplePlace(scene, x0 + px, y0 + py, s, &sx, &sy);
                const double x = (double)(x0 + px) + sx;
                const double y = (double)(y0 + py) + sy;
                if (t->a[0] * x + t->b[0] * y + t->c[0] < 0.0 ||
                    t->a[1] * x + t->b[1] * y + t->c[1] < 0.0 ||
                    t->a[2] * x + t->b[2] * y + t->c[2] < 0.0) {
                    continue;
                }
                Drawn *const at = &drawn[(py * TILE + px) * SAMPLES_MAX + s];
                const float depth = (float)(t->depth_x * x + t->depth_y * y + t->depth_0);
                if (scene->machine.depth && depth >= at->depth) {
                    continue;
                }
                if (!shaded) {
                    double low[3], high[3];
                    Shade(scene, t, (double)(x0 + px) + 0.5, (double)(y0 + py) + 0.5, low, high);
                    for (uint32_t k = 0; k < 3u; ++k) {
                        colour[k] = (uint8_t)floor((low[k] + high[k]) * 0.5 + 0.5);
                    }
                    shaded = true;
                }
                at->depth = depth;
                memcpy(at->colour, colour, 3u);
            }
        }
    }
}

/// Sets @p scene to frame @p frame of @p frames at @p level as @p machine draws it.
static void SceneOf(Scene *scene, uint32_t level, uint32_t frame, uint32_t frames,
                    const WimsyMachine *machine) {
    memset(scene, 0, sizeof(*scene));
    scene->level = level;
    scene->machine = *machine;
    scene->sample_count = machine->samples == 4u ? 4u : 1u;
    scene->held = UINT32_MAX;
    SamplePlaces(scene);
    const uint32_t shown = frame >= machine->frames_late ? frame - machine->frames_late : 0u;
    if (level == 0u) {
        if (!sprite_made) {
            WimsySpriteTexels(sprite_texels);
            sprite_made = true;
        }
        GameWorldStart(&sprite_world);
        for (uint32_t k = 0; k < shown; ++k) {
            GameWorldStep(&sprite_world);
        }
        WimsySpritesOf(&sprite_world, frame_sprites);
        return;
    }
    scene->instances = WimsyInstancesOf(level, shown, frames, scene_instance);
    WimsyLightOf(level, &scene->light);
}

/// The range each pixel of the tile at (@p x0, @p y0) may hold, into tile_low and tile_high.
static void TileRanges(Scene *scene, uint32_t x0, uint32_t y0) {
    if (scene->level == 0u) {
        SpritesInto(frame_sprites, WIMSY_SPRITES, sprite_texels, (int32_t)x0, (int32_t)y0, TILE,
                    TILE, &tile_low[0][0], TILE);
        memcpy(tile_high, tile_low, sizeof(tile_high));
        return;
    }
    for (uint32_t i = 0; i < TILE * TILE * SAMPLES_MAX; ++i) {
        Sample *const sample = &samples[i];
        for (uint32_t place = 0; place < PLACES; ++place) {
            sample->nearest[place] = FLT_MAX;
        }
        sample->any = false;
    }
    OverTile(scene, x0, y0, PassNearest);
    const double clear[3] = { (double)WIMSY_CLEAR[0], (double)WIMSY_CLEAR[1],
                              (double)WIMSY_CLEAR[2] };
    for (uint32_t i = 0; i < TILE * TILE * SAMPLES_MAX; ++i) {
        Sample *const sample = &samples[i];
        sample->cover = 0.0f;
        for (uint32_t place = 0; place < PLACES; ++place) {
            sample->cover =
                sample->nearest[place] > sample->cover ? sample->nearest[place] : sample->cover;
        }
        if (sample->cover == FLT_MAX) {
            Widen(sample, clear, clear);
        }
    }
    OverTile(scene, x0, y0, PassRange);
    for (uint32_t p = 0; p < TILE * TILE; ++p) {
        for (uint32_t k = 0; k < 3u; ++k) {
            uint32_t low = 0, high = 0;
            for (uint32_t s = 0; s < scene->sample_count; ++s) {
                low += samples[p * SAMPLES_MAX + s].low[k];
                high += samples[p * SAMPLES_MAX + s].high[k];
            }
            low /= scene->sample_count;
            high = (high + scene->sample_count - 1u) / scene->sample_count;
            tile_low[p][k] = (uint8_t)(low > WIMSY_CHANNEL_MARGIN ? low - WIMSY_CHANNEL_MARGIN : 0u);
            tile_high[p][k] =
                (uint8_t)(high + WIMSY_CHANNEL_MARGIN < 255u ? high + WIMSY_CHANNEL_MARGIN : 255u);
        }
        tile_low[p][3] = tile_high[p][3] = WIMSY_CLEAR[3];
    }
}

void WimsyRender(uint32_t level, uint32_t frame, uint32_t frames, const WimsyMachine *machine,
                 uint8_t *pixels) {
    static Scene scene;
    SceneOf(&scene, level, frame, frames, machine);
    // The picture lands `shift` pixels to the right: what is drawn at x shows at x + shift.
    const uint32_t shift = machine->shift < WIMSY_WIDTH ? machine->shift : WIMSY_WIDTH;
    for (uint32_t y = 0; y < WIMSY_HEIGHT; ++y) {
        for (uint32_t x = 0; x < shift; ++x) {
            memcpy(pixels + ((size_t)y * WIMSY_WIDTH + x) * 4u, WIMSY_CLEAR, 4u);
        }
    }
    for (uint32_t y0 = 0; y0 < WIMSY_HEIGHT; y0 += TILE) {
        for (uint32_t x0 = 0; x0 < WIMSY_WIDTH; x0 += TILE) {
            if (level == 0u) {
                SpritesInto(frame_sprites, WIMSY_SPRITES, sprite_texels, (int32_t)x0, (int32_t)y0,
                            TILE, TILE, &tile_low[0][0], TILE);
            } else {
                for (uint32_t i = 0; i < TILE * TILE * SAMPLES_MAX; ++i) {
                    drawn[i].depth = FLT_MAX;
                    memcpy(drawn[i].colour, WIMSY_CLEAR, 3u);
                }
                OverTile(&scene, x0, y0, PassDraw);
                for (uint32_t p = 0; p < TILE * TILE; ++p) {
                    for (uint32_t k = 0; k < 3u; ++k) {
                        uint32_t sum = 0;
                        for (uint32_t s = 0; s < scene.sample_count; ++s) {
                            sum += drawn[p * SAMPLES_MAX + s].colour[k];
                        }
                        tile_low[p][k] =
                            (uint8_t)((sum + scene.sample_count / 2u) / scene.sample_count);
                    }
                    tile_low[p][3] = WIMSY_CLEAR[3];
                }
            }
            for (uint32_t y = 0; y < TILE && y0 + y < WIMSY_HEIGHT; ++y) {
                for (uint32_t x = 0; x < TILE; ++x) {
                    if (x0 + x + shift < WIMSY_WIDTH) {
                        memcpy(pixels + ((size_t)(y0 + y) * WIMSY_WIDTH + x0 + x + shift) * 4u,
                               tile_low[y * TILE + x], 4u);
                    }
                }
            }
        }
    }
}

void WimsyJudge(uint32_t level, uint32_t frame, uint32_t frames, const uint8_t *got,
                uint8_t *expected, uint8_t *verdicts, WimsyVerdict *out) {
    static Scene scene;
    const WimsyMachine right = WimsyMachineRight(level);
    SceneOf(&scene, level, frame, frames, &right);
    memset(out, 0, sizeof(*out));
    out->exact = true;
    for (uint32_t y0 = 0; y0 < WIMSY_HEIGHT; y0 += TILE) {
        for (uint32_t x0 = 0; x0 < WIMSY_WIDTH; x0 += TILE) {
            TileRanges(&scene, x0, y0);
            for (uint32_t y = 0; y < TILE && y0 + y < WIMSY_HEIGHT; ++y) {
                for (uint32_t x = 0; x < TILE; ++x) {
                    const size_t at = (size_t)(y0 + y) * WIMSY_WIDTH + x0 + x;
                    const uint8_t *const pixel = got + at * 4u;
                    const uint8_t *const low = tile_low[y * TILE + x];
                    const uint8_t *const high = tile_high[y * TILE + x];
                    uint32_t outside = 0, widest = 0;
                    uint8_t nearest[4];
                    for (uint32_t k = 0; k < 4u; ++k) {
                        const uint32_t under = pixel[k] < low[k] ? (uint32_t)(low[k] - pixel[k]) : 0u;
                        const uint32_t over = pixel[k] > high[k] ? (uint32_t)(pixel[k] - high[k]) : 0u;
                        outside = under + over > outside ? under + over : outside;
                        widest = (uint32_t)(high[k] - low[k]) > widest ? (uint32_t)(high[k] - low[k])
                                                                       : widest;
                        nearest[k] = pixel[k] < low[k] ? low[k] : pixel[k] > high[k] ? high[k] : pixel[k];
                    }
                    out->exact = out->exact && widest == 0u;
                    uint8_t verdict = WIMSY_PIXEL_HELD;
                    if (outside > 0u) {
                        verdict = WIMSY_PIXEL_WRONG;
                        if (out->wrong++ == 0u) {
                            out->wrong_x = x0 + x;
                            out->wrong_y = y0 + y;
                            memcpy(out->wrong_got, pixel, 4u);
                            memcpy(out->wrong_low, low, 4u);
                            memcpy(out->wrong_high, high, 4u);
                        }
                        out->farthest = outside > out->farthest ? outside : out->farthest;
                    } else if (widest > WIMSY_HELD_WIDTH) {
                        verdict = WIMSY_PIXEL_EDGE;
                        ++out->edge;
                    } else {
                        ++out->held;
                    }
                    if (expected != NULL) {
                        memcpy(expected + at * 4u, nearest, 4u);
                    }
                    if (verdicts != NULL) {
                        verdicts[at] = verdict;
                    }
                }
            }
        }
    }
}

bool WimsyVerdictPasses(const WimsyVerdict *verdict) {
    return verdict->wrong == 0u;
}

size_t WimsyDescribeVerdict(char *out, size_t out_size, const WimsyVerdict *verdict) {
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    int wrote;
    if (verdict->wrong == 0u && verdict->exact) {
        wrote = snprintf(out, out_size, "every one of %u pixels is the reference's",
                         (unsigned)verdict->held);
    } else if (verdict->wrong == 0u) {
        wrote = snprintf(out, out_size,
                         "%u pixels within their colour's range, %u on an edge within the "
                         "colours beside it",
                         (unsigned)verdict->held, (unsigned)verdict->edge);
    } else {
        wrote = snprintf(out, out_size,
                         "%u of %u pixels outside their range, by %u at most; the one at "
                         "(%u,%u) is %02x %02x %02x %02x, held to %02x %02x %02x %02x up to "
                         "%02x %02x %02x %02x",
                         (unsigned)verdict->wrong,
                         (unsigned)(verdict->held + verdict->edge + verdict->wrong),
                         (unsigned)verdict->farthest, (unsigned)verdict->wrong_x,
                         (unsigned)verdict->wrong_y, verdict->wrong_got[0], verdict->wrong_got[1],
                         verdict->wrong_got[2], verdict->wrong_got[3], verdict->wrong_low[0],
                         verdict->wrong_low[1], verdict->wrong_low[2], verdict->wrong_low[3],
                         verdict->wrong_high[0], verdict->wrong_high[1], verdict->wrong_high[2],
                         verdict->wrong_high[3]);
    }
    return wrote < 0 ? 0u : (size_t)wrote < out_size ? (size_t)wrote : out_size - 1u;
}
