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
    static const char *const NAMES[WIMSY_LEVELS] = { "sprites", "one lit model",
                                                     "a scene near and far", "a heavy frame",
                                                     "a heavy frame of 8K" };
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
        const uint8_t *const colour = WIMSY_MESH_PALETTE[0][a->colour];
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
/// Level 2 sees four times as far.
#define SCENE_FAR 1024.0f
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

// Level 2: the camera starts in front of the first model, looking down the lane, and backs
// away and up over the run, thirty six units from the first model at its end.
#define SCENE_MODELS 7u
static const float SCENE_MODEL_Z[SCENE_MODELS] = { 0.0f,   -6.0f,   -14.0f, -30.0f,
                                                   -60.0f, -110.0f, -180.0f };
/// Levels 3 and 4 add a crowd.
#define CROWD_MODELS 160u
#define CROWD_COLUMNS 8u
/// The seed of everything chance decides in a level: where a member of the crowd stands off its
/// place and which drawing it is. One number, so two runs draw the same crowd.
#define CROWD_SEED 0x57494D53u
_Static_assert(SCENE_MODELS + CROWD_MODELS <= WIMSY_INSTANCES_MAX, "the crowd fits");
#define SCENE_MODEL_X 3.25f
#define SCENE_MODEL_Y 1.125f
/// The floor, from its edge nearest the camera's first place to its far one, in rows this deep.
#define FLOOR_NEAR 2
#define FLOOR_ROWS 34u
#define FLOOR_ROW 6
/// How many units of the floor one repeat of each texture covers.
#define FLOOR_REPEAT 8.0f
#define LANE_REPEAT 2.0f
#define SIGN_GAP 0.01953125f

void WimsyViewOf(uint32_t level, uint32_t frame, uint32_t frames, float to_clip[16]) {
    memset(to_clip, 0, 16u * sizeof(float));
    if (level < 2u || level >= WIMSY_LEVELS) {
        return;
    }
    const float along = (float)WimsyPathAt(frame, frames) / 65536.0f;
    const float eye[3] = { 0.0f, 1.625f + 5.0f * along * along, 6.0f + 30.0f * along * along };
    const float target[3] = { 0.0f, 0.75f, -10.0f - 20.0f * along };
    // The camera's own axes in the world: where it looks, its right and its up.
    float ahead[3], right[3], up[3];
    float length = 0.0f;
    for (uint32_t k = 0; k < 3u; ++k) {
        ahead[k] = target[k] - eye[k];
        length += ahead[k] * ahead[k];
    }
    length = sqrtf(length);
    for (uint32_t k = 0; k < 3u; ++k) {
        ahead[k] /= length;
    }
    // It never looks along the world's up, so its right is level: ahead cross (0, 1, 0).
    const float level_length = sqrtf(ahead[0] * ahead[0] + ahead[2] * ahead[2]);
    right[0] = -ahead[2] / level_length;
    right[1] = 0.0f;
    right[2] = ahead[0] / level_length;
    up[0] = right[1] * ahead[2] - right[2] * ahead[1];
    up[1] = right[2] * ahead[0] - right[0] * ahead[2];
    up[2] = right[0] * ahead[1] - right[1] * ahead[0];
    const float aspect = (float)WIMSY_WIDTH / (float)WIMSY_HEIGHT;
    const float deep = SCENE_FAR / (VIEW_NEAR - SCENE_FAR);
    float behind = 0.0f, aside = 0.0f, above = 0.0f;
    for (uint32_t k = 0; k < 3u; ++k) {
        aside += right[k] * eye[k];
        above += up[k] * eye[k];
        behind += ahead[k] * eye[k];
    }
    for (uint32_t column = 0; column < 3u; ++column) {
        to_clip[column * 4u + 0u] = VIEW_FOCAL / aspect * right[column];
        to_clip[column * 4u + 1u] = VIEW_FOCAL * up[column];
        to_clip[column * 4u + 2u] = -deep * ahead[column];
        to_clip[column * 4u + 3u] = ahead[column];
    }
    to_clip[12u + 0u] = VIEW_FOCAL / aspect * -aside;
    to_clip[12u + 1u] = VIEW_FOCAL * -above;
    to_clip[12u + 2u] = deep * behind + VIEW_NEAR * deep;
    to_clip[12u + 3u] = -behind;
}

/// What chance gives model @p index, the same on every run: a whole number from the seed.
static uint32_t Chance(uint32_t index) {
    uint32_t chance = index * 2654435761u ^ CROWD_SEED;
    chance ^= chance >> 15;
    chance *= 0x2C1B3C6Du;
    chance ^= chance >> 12;
    return chance;
}

uint32_t WimsyInstanceVariant(uint32_t level, uint32_t index) {
    // The scene's seven are the first drawing; each of the crowd one of the five, by chance.
    return level >= 3u && index >= SCENE_MODELS ? Chance(index) % WIMSY_MESH_VARIANTS : 0u;
}

/// Level 2's models: each stands on the floor at its place down the lane, left and right of it by
/// turns, turning a two hundred and fortieth of a turn a frame from a seventh of a turn apart.
static uint32_t SceneInstances(uint32_t level, uint32_t frame, uint32_t frames, uint32_t first,
                               uint32_t wanted, WimsyInstance *out) {
    float view[16];
    WimsyViewOf(level, frame, frames, view);
    const uint32_t all = level >= 3u ? SCENE_MODELS + CROWD_MODELS : SCENE_MODELS;
    const uint32_t end = first >= all ? first : wanted < all - first ? first + wanted : all;
    for (uint32_t i = first; i < end; ++i) {
        int32_t s = 0, c = 0;
        WimsySinCos(frame * ALONE_TURN + i * 9362u, &s, &c);
        float x = i % 2u == 0u ? -SCENE_MODEL_X : SCENE_MODEL_X;
        float z = i < SCENE_MODELS ? SCENE_MODEL_Z[i] : 0.0f;
        if (i >= SCENE_MODELS) {
            // The crowd: twenty rows of eight, four each side of the lane, nine units apart, each
            // member up to a unit aside and three along from its place by the run's one seed.
            const uint32_t member = i - SCENE_MODELS;
            const uint32_t column = member % CROWD_COLUMNS;
            const uint32_t chance = Chance(i);
            x = (6.0f + 3.0f * (float)(column / 2u)) * (column % 2u == 0u ? -1.0f : 1.0f) +
                0.25f * ((float)(chance >> 8 & 0xFu) * 0.5f - 3.75f);
            z = -3.0f - 9.0f * (float)(member / CROWD_COLUMNS) +
                0.5f * ((float)(chance >> 16 & 0xFu) * 0.75f - 5.625f);
        }
        const float rows[3][4] = {
            { FromQ30(c), 0.0f, FromQ30(s), x },
            { 0.0f, 1.0f, 0.0f, SCENE_MODEL_Y },
            { -FromQ30(s), 0.0f, FromQ30(c), z },
        };
        WimsyInstance *const instance = &out[i - first];
        memset(instance, 0, sizeof(*instance));
        for (uint32_t column = 0; column < 4u; ++column) {
            for (uint32_t row = 0; row < 4u; ++row) {
                float sum = column == 3u ? view[12u + row] : 0.0f;
                for (uint32_t k = 0; k < 3u; ++k) {
                    sum += view[k * 4u + row] * rows[k][column];
                }
                instance->to_clip[column * 4u + row] = sum;
            }
        }
        for (uint32_t row = 0; row < 3u; ++row) {
            for (uint32_t column = 0; column < 3u; ++column) {
                instance->turn[row][column] = rows[row][column];
            }
        }
        instance->tint[0] = instance->tint[1] = instance->tint[2] = instance->tint[3] = 1.0f;
        if (i >= SCENE_MODELS) {
            // Each of the crowd its own shade, so one drawn with another's uniform buffer shows.
            instance->tint[0] = 1.0f - 0.125f * (float)(i % 3u);
            instance->tint[1] = 1.0f - 0.125f * (float)(i % 4u);
            instance->tint[2] = 1.0f - 0.125f * (float)(i % 5u);
        }
    }
    return end - first;
}

static WimsyWorldVertex WorldCorner(float x, float y, float z, float u, float v,
                                    const uint8_t colour[3]) {
    const WimsyWorldVertex vertex = { x,
                                      y,
                                      z,
                                      u,
                                      v,
                                      (float)colour[0] / 255.0f,
                                      (float)colour[1] / 255.0f,
                                      (float)colour[2] / 255.0f,
                                      1.0f };
    return vertex;
}

/// Six vertices over four corners given around the quad.
static void WorldQuad(WimsyWorldVertex *out, uint32_t *count, const WimsyWorldVertex corner[4]) {
    static const uint32_t ORDER[6] = { 0, 1, 2, 0, 2, 3 };
    for (uint32_t i = 0; i < 6u; ++i) {
        out[(*count)++] = corner[ORDER[i]];
    }
}

/// A quad lying on the floor from (@p x0, @p z0) to (@p x1, @p z1), its texture coordinates the
/// place over @p repeat.
static void FloorQuad(WimsyWorldVertex *out, uint32_t *count, float x0, float z0, float x1,
                      float z1, float repeat, const uint8_t colour[3]) {
    const WimsyWorldVertex corner[4] = {
        WorldCorner(x0, 0.0f, z0, x0 / repeat, -z0 / repeat, colour),
        WorldCorner(x1, 0.0f, z0, x1 / repeat, -z0 / repeat, colour),
        WorldCorner(x1, 0.0f, z1, x1 / repeat, -z1 / repeat, colour),
        WorldCorner(x0, 0.0f, z1, x0 / repeat, -z1 / repeat, colour),
    };
    WorldQuad(out, count, corner);
}

/// A quad standing at @p z, facing the camera, from (@p x0, @p y0) to (@p x1, @p y1).
static void StandingQuad(WimsyWorldVertex *out, uint32_t *count, float x0, float y0, float x1,
                         float y1, float z, const uint8_t colour[3]) {
    const WimsyWorldVertex corner[4] = {
        WorldCorner(x0, y0, z, 0.0f, 0.0f, colour),
        WorldCorner(x1, y0, z, 0.0f, 0.0f, colour),
        WorldCorner(x1, y1, z, 0.0f, 0.0f, colour),
        WorldCorner(x0, y1, z, 0.0f, 0.0f, colour),
    };
    WorldQuad(out, count, corner);
}

uint32_t WimsyWorldOf(uint32_t level, WimsyWorldVertex *vertices, uint32_t *vertex_count,
                      WimsyWorldDraw *draws) {
    *vertex_count = 0;
    if (level < 2u || level >= WIMSY_LEVELS) {
        return 0;
    }
    static const uint8_t WHITE[3] = { 255, 255, 255 };
    static const uint8_t WALL[3] = { 200, 40, 40 };
    static const uint8_t SIGN[3] = { 40, 200, 80 };
    static const uint8_t MARK[3] = { 240, 200, 40 };
    uint32_t count = 0, draw = 0;
    // The floor either side of the lane, a row at a time from the camera's end.
    static const float COLUMNS[7] = { -18.0f, -12.0f, -6.0f, -1.0f, 1.0f, 6.0f, 12.0f };
    static const float WIDTHS[7] = { 6.0f, 6.0f, 5.0f, 2.0f, 5.0f, 6.0f, 6.0f };
    draws[draw].first = count;
    for (uint32_t row = 0; row < FLOOR_ROWS; ++row) {
        const float z0 = (float)(FLOOR_NEAR - (int32_t)row * FLOOR_ROW);
        for (uint32_t column = 0; column < 7u; ++column) {
            if (column != 3u) {
                FloorQuad(vertices, &count, COLUMNS[column], z0, COLUMNS[column] + WIDTHS[column],
                          z0 - (float)FLOOR_ROW, FLOOR_REPEAT, WHITE);
            }
        }
    }
    draws[draw].count = count - draws[draw].first;
    draws[draw++].material = WIMSY_MATERIAL_FLOOR;
    // The lane between them, its texture once across and once every two units along.
    draws[draw].first = count;
    for (uint32_t row = 0; row < FLOOR_ROWS; ++row) {
        const float z0 = (float)(FLOOR_NEAR - (int32_t)row * FLOOR_ROW);
        const float z1 = z0 - (float)FLOOR_ROW;
        const WimsyWorldVertex corner[4] = {
            WorldCorner(-1.0f, 0.0f, z0, 0.0f, -z0 / LANE_REPEAT, WHITE),
            WorldCorner(1.0f, 0.0f, z0, 1.0f, -z0 / LANE_REPEAT, WHITE),
            WorldCorner(1.0f, 0.0f, z1, 1.0f, -z1 / LANE_REPEAT, WHITE),
            WorldCorner(-1.0f, 0.0f, z1, 0.0f, -z1 / LANE_REPEAT, WHITE),
        };
        WorldQuad(vertices, &count, corner);
    }
    draws[draw].count = count - draws[draw].first;
    draws[draw++].material = WIMSY_MATERIAL_LANE;
    // Two walls forty units down, a sign five two hundred and fifty sixths of a unit in front of
    // each: from the camera's last place, 76 units off, that is 28 steps of a 24 bit depth
    // buffer and a ninth of a step of a 16 bit one. On the left the sign is drawn first and on
    // the right the wall, so the depth test decides both ways round.
    draws[draw].first = count;
    StandingQuad(vertices, &count, -8.5f, 0.5f, -4.5f, 3.5f, -40.0f + SIGN_GAP, SIGN);
    StandingQuad(vertices, &count, -10.0f, 0.0f, -3.0f, 4.5f, -40.0f, WALL);
    StandingQuad(vertices, &count, 3.0f, 0.0f, 10.0f, 4.5f, -40.0f, WALL);
    StandingQuad(vertices, &count, 4.5f, 0.5f, 8.5f, 3.5f, -40.0f + SIGN_GAP, SIGN);
    draws[draw].count = count - draws[draw].first;
    draws[draw++].material = WIMSY_MATERIAL_FLAT;
    // Marks painted on the floor, in its plane, nearer and farther.
    draws[draw].first = count;
    static const float MARKS[4][2] = { { -2.0f, -5.0f }, { -20.0f, -26.0f }, { -80.0f, -92.0f },
                                       { -150.0f, -174.0f } };
    for (uint32_t i = 0; i < 4u; ++i) {
        FloorQuad(vertices, &count, 2.0f, MARKS[i][0], 4.0f, MARKS[i][1], FLOOR_REPEAT, MARK);
    }
    draws[draw].count = count - draws[draw].first;
    draws[draw++].material = WIMSY_MATERIAL_MARK;
    *vertex_count = count;
    return draw;
}

uint32_t WimsyTextureSide(WimsyTexture texture) {
    static const uint32_t SIDES[WIMSY_TEXTURES] = { 2048u, 1024u, 4096u, 8192u };
    return texture < WIMSY_TEXTURES ? SIDES[texture] : 1u;
}

uint32_t WimsyTextureLevels(WimsyTexture texture) {
    static const uint32_t LEVELS[WIMSY_TEXTURES] = { 12u, 11u, 13u, 14u };
    return texture < WIMSY_TEXTURES ? LEVELS[texture] : 1u;
}

WimsyTexture WimsyFloorTexture(uint32_t level) {
    return level == 3u ? WIMSY_TEXTURE_FLOOR_4K : level == 4u ? WIMSY_TEXTURE_FLOOR_8K
                                                             : WIMSY_TEXTURE_FLOOR;
}

uint32_t WimsyTextureBlock(WimsyTexture texture) {
    return texture == WIMSY_TEXTURE_FLOOR_4K || texture == WIMSY_TEXTURE_FLOOR_8K ? 8u : 0u;
}

void WimsyTextureBlockAt(WimsyTexture texture, uint32_t level, uint32_t bx, uint32_t by,
                         uint8_t out[16]) {
    uint8_t colour[4];
    WimsyTexel(texture, level, bx * 8u, by * 8u, colour);
    // A void extent of the LDR profile (the ASTC chapter of the Khronos Data Format
    // Specification, 23.17): nine bits of 0x1FC, the dynamic range bit clear, two reserved ones
    // set, four extent coordinates of thirteen ones for no extent, then the colour, sixteen bits
    // a channel, each byte twice so every way of making eight bits of it gives the byte back.
    static const uint8_t HEAD[8] = { 0xFC, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    memcpy(out, HEAD, 8u);
    for (uint32_t k = 0; k < 4u; ++k) {
        out[8u + k * 2u] = colour[k];
        out[9u + k * 2u] = colour[k];
    }
}

void WimsyTexel(WimsyTexture texture, uint32_t level, uint32_t x, uint32_t y, uint8_t out[4]) {
    // The lane's levels, each far from its neighbours in some channel.
    static const uint8_t LANE[11][3] = {
        { 230, 40, 40 },  { 40, 200, 60 },  { 50, 80, 230 },   { 230, 210, 40 },
        { 220, 60, 210 }, { 40, 210, 220 }, { 240, 240, 240 }, { 240, 130, 30 },
        { 110, 50, 180 }, { 30, 110, 60 },  { 90, 90, 90 },
    };
    static const uint8_t STONE[4][3] = {
        { 96, 96, 104 }, { 124, 118, 110 }, { 84, 92, 100 }, { 136, 132, 126 }
    };
    out[3] = 255u;
    if (texture == WIMSY_TEXTURE_LANE) {
        memcpy(out, LANE[level < 11u ? level : 10u], 3u);
        return;
    }
    if (level >= 5u) {
        // The greys' mean, which smaller levels of a picture come to.
        out[0] = out[1] = out[2] = 110u;
        return;
    }
    // A stone is 128 texels of level nought. Each level is a fifth nearer the mean, as a level
    // made by averaging the one before loses its contrast.
    uint32_t stone = (x >> (7u - level)) * 73856093u ^ (y >> (7u - level)) * 19349663u;
    stone ^= stone >> 13;
    stone *= 0x5BD1E995u;
    stone ^= stone >> 15;
    for (uint32_t k = 0; k < 3u; ++k) {
        out[k] = (uint8_t)((STONE[stone & 3u][k] * (5u - level) + 110u * level + 2u) / 5u);
    }
}

uint32_t WimsyInstancesFrom(uint32_t level, uint32_t frame, uint32_t frames, uint32_t first,
                            uint32_t count, WimsyInstance *out) {
    if (level >= 2u && level < WIMSY_LEVELS) {
        return SceneInstances(level, frame, frames, first, count, out);
    }
    return first == 0u && count > 0u ? WimsyInstancesOf(level, frame, frames, out) : 0u;
}

uint32_t WimsyInstancesOf(uint32_t level, uint32_t frame, uint32_t frames, WimsyInstance *out) {
    if (level >= 2u && level < WIMSY_LEVELS) {
        return SceneInstances(level, frame, frames, 0u, WIMSY_INSTANCES_MAX, out);
    }
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

void WimsyComputeTexel(uint32_t texture, uint32_t level, uint32_t frame, uint32_t x, uint32_t y,
                       uint8_t out[4]) {
    out[0] = (uint8_t)(x * 5u + frame * 3u + texture * 16u);
    out[1] = (uint8_t)(y * 7u + frame + level * 32u);
    out[2] = (uint8_t)((x ^ y) * 9u + texture * 8u + frame * 2u);
    out[3] = 255u;
}

void WimsyChainClear(uint32_t chain, uint8_t out[4]) {
    out[0] = (uint8_t)(16u + chain * 8u);
    out[1] = 24u;
    out[2] = 40u;
    out[3] = 255u;
}

void WimsyChainSquareOf(uint32_t chain, uint32_t step, uint32_t frame, WimsyChainSquare *out) {
    static const uint8_t COLOURS[8][3] = {
        { 230, 60, 60 },  { 60, 200, 80 },  { 70, 100, 230 },  { 230, 210, 60 },
        { 210, 80, 210 }, { 60, 210, 220 }, { 240, 240, 240 }, { 240, 140, 40 },
    };
    // Anywhere the square fits whole, 49 places a side.
    const uint32_t places = WIMSY_CHAIN_SIDE - WIMSY_CHAIN_SQUARE + 1u;
    out->x = (chain * 7u + step * 5u + frame) % places;
    out->y = (step * 11u + chain * 3u + frame / 2u) % places;
    memcpy(out->colour, COLOURS[(chain + step) % 8u], 3u);
    out->colour[3] = 255u;
}

void WimsyChainTexel(uint32_t chain, uint32_t frame, uint32_t x, uint32_t y, uint8_t out[4]) {
    // The last square drawn over the texel, or the first target's clear.
    for (uint32_t step = WIMSY_CHAIN_STEPS; step-- > 0u;) {
        WimsyChainSquare square;
        WimsyChainSquareOf(chain, step, frame, &square);
        if (x >= square.x && x < square.x + WIMSY_CHAIN_SQUARE && y >= square.y &&
            y < square.y + WIMSY_CHAIN_SQUARE) {
            memcpy(out, square.colour, 4u);
            return;
        }
    }
    WimsyChainClear(chain, out);
}

void WimsyLoadTexel(uint32_t load, uint32_t x, uint32_t y, uint8_t out[4]) {
    const uint32_t bx = x / 8u, by = y / 8u;
    out[0] = (uint8_t)(bx * 29u + load * 7u);
    out[1] = (uint8_t)(by * 31u + load * 3u);
    out[2] = (uint8_t)((bx + by) * 17u + load * 11u);
    out[3] = 255u;
}

void WimsyLoadBlock(uint32_t load, uint32_t bx, uint32_t by, uint8_t out[16]) {
    uint8_t colour[4];
    WimsyLoadTexel(load, bx * 8u, by * 8u, colour);
    // A void extent, as the floor's blocks are (WimsyTextureBlockAt).
    static const uint8_t HEAD[8] = { 0xFC, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    memcpy(out, HEAD, 8u);
    for (uint32_t k = 0; k < 4u; ++k) {
        out[8u + k * 2u] = colour[k];
        out[9u + k * 2u] = colour[k];
    }
}

uint32_t WimsyPanelsOf(uint32_t level, WimsyPanel *out) {
    if (level < 3u || level >= WIMSY_LEVELS) {
        return 0;
    }
    uint32_t count = 0;
    for (uint32_t t = 0; t < WIMSY_COMPUTE_TEXTURES; ++t) {
        // Two rows of six above the scene and four below it, a panel 192 wide: level nought,
        // and the other levels in a column of 64 beside it.
        const uint32_t left = t < 12u ? 64u + (t % 6u) * 192u : (t - 12u) * 192u;
        const uint32_t top = t < 12u ? 8u + (t / 6u) * 136u : 536u;
        uint32_t down = 0;
        for (uint32_t mip = 0; mip < WIMSY_COMPUTE_LEVELS; ++mip) {
            const WimsyPanel panel = { mip == 0u ? left : left + WIMSY_COMPUTE_SIDE,
                                       mip == 0u ? top : top + down, WIMSY_COMPUTE_SIDE >> mip,
                                       WIMSY_PANEL_COMPUTE, t, mip };
            out[count++] = panel;
            down += mip == 0u ? 0u : WIMSY_COMPUTE_SIDE >> mip;
        }
    }
    for (uint32_t c = 0; c < WIMSY_CHAINS; ++c) {
        // Two rows of eight beside the last four textures.
        const WimsyPanel panel = { 768u + (c % 8u) * WIMSY_CHAIN_SIDE,
                                   536u + (c / 8u) * WIMSY_CHAIN_SIDE, WIMSY_CHAIN_SIDE,
                                   WIMSY_PANEL_CHAIN, c, 0u };
        out[count++] = panel;
    }
    for (uint32_t l = 0; l < WIMSY_LOADS; ++l) {
        // In the margin left of the textures' first rows.
        const WimsyPanel panel = { 0u, 8u + l * 136u, WIMSY_LOAD_SHOWN, WIMSY_PANEL_LOAD, l, 0u };
        out[count++] = panel;
    }
    return count;
}

/// The panel the pixel at (@p x, @p y) is in at @p level, with its bytes on frame @p frame, or
/// NULL.
static const WimsyPanel *PanelAt(uint32_t level, uint32_t frame, uint32_t x, uint32_t y,
                                 uint8_t out[4]) {
    static WimsyPanel panels[WIMSY_PANELS];
    static uint32_t made_for = UINT32_MAX, count;
    if (made_for != level) {
        count = WimsyPanelsOf(level, panels);
        made_for = level;
    }
    for (uint32_t i = 0; i < count; ++i) {
        const WimsyPanel *const panel = &panels[i];
        if (x < panel->x || x >= panel->x + panel->side || y < panel->y ||
            y >= panel->y + panel->side) {
            continue;
        }
        if (panel->kind == WIMSY_PANEL_CHAIN) {
            WimsyChainTexel(panel->index, frame, x - panel->x, y - panel->y, out);
        } else if (panel->kind == WIMSY_PANEL_LOAD) {
            WimsyLoadTexel(frame * WIMSY_LOADS + panel->index, x - panel->x, y - panel->y, out);
        } else {
            WimsyComputeTexel(panel->index, panel->mip, frame, x - panel->x, y - panel->y, out);
        }
        return panel;
    }
    return NULL;
}

bool WimsyPanelPixel(uint32_t level, uint32_t frame, uint32_t x, uint32_t y, uint8_t out[4]) {
    return PanelAt(level, frame, x, y, out) != NULL;
}

WimsyMachine WimsyMachineRight(uint32_t level) {
    const WimsyMachine machine = {
        level == 0u ? 1u : 4u, level != 0u, true, true, 0u, 0u, true, 0u, true, 0u, false
    };
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
    /// A world vertex's texture coordinates.
    double u, v;
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
    /// What a machine draws: facing the eye as its own corners have it.
    bool drawn;
    WimsyMaterial material;
    /// The texture coordinates over w and one over w, each a plane over the target: at (x, y)
    /// the coordinate is its plane's value over the last one's.
    double u_plane[3], v_plane[3], w_plane[3];
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
static WimsyWorldVertex world_vertices[WIMSY_WORLD_VERTICES_MAX];
static WimsyWorldDraw world_draws[WIMSY_WORLD_DRAWS_MAX];
static Corner world_corners[WIMSY_WORLD_VERTICES_MAX];
static uint8_t sprite_texels[WIMSY_SPRITE_BYTES];
static bool sprite_made;
static GameWorld sprite_world;
static WimsySprite frame_sprites[WIMSY_SPRITES];

/// A frame as the reference draws it.
typedef struct {
    uint32_t level;
    uint32_t sample_count;
    uint32_t instances;
    uint32_t world_count;
    uint32_t draws;
    /// The judge's frame, whose colours are ranges, or a machine's, whose colours are its own.
    bool judging;
    /// The frame whose buffers the machine reads: this one, or an earlier one's when it is late.
    uint32_t shown;
    WimsyLight light;
    WimsyMachine machine;
    /// Which instance `corners` holds, and the target rectangle its corners span; and every
    /// instance's rectangle, left, right, top and bottom, so one that misses a tile is not held.
    uint32_t held;
    double left, right, top, bottom;
    double span[WIMSY_INSTANCES_MAX][4];
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

/// Sets up the triangle of corners @p v, drawn with @p material in @p colour. False when it lies
/// outside the tile at (@p x0, @p y0) or has no area at all.
static bool SetUp(const Scene *scene, const Corner *const v[3], const double colour[3],
                  WimsyMaterial material, double x0, double y0, Triangle *out) {
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
    // Only a model's faces are culled; the world's quads are drawn whichever way they face. A
    // triangle thinner than the margin faces whichever way a machine's own corners make it: it
    // may be drawn, and it is sure to face the eye only when it is thicker than that.
    const bool culled = material == WIMSY_MATERIAL_MODEL && scene->machine.cull;
    const double longest =
        Most(hypot(v[1]->x - v[0]->x, v[1]->y - v[0]->y), hypot(v[2]->x - v[1]->x, v[2]->y - v[1]->y),
             hypot(v[0]->x - v[2]->x, v[0]->y - v[2]->y));
    const double thin = longest * WIMSY_EDGE_MARGIN;
    out->front = culled ? area < -thin : size > thin;
    out->maybe = !culled || area < thin;
    out->drawn = !culled || area < (scene->machine.thin_faces ? thin : 0.0);
    out->material = material;
    const double sign = area < 0.0 ? -1.0 : 1.0;
    memset(out->u_plane, 0, sizeof(out->u_plane));
    memset(out->v_plane, 0, sizeof(out->v_plane));
    memset(out->w_plane, 0, sizeof(out->w_plane));
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
        const double edge[3] = { out->a[i], out->b[i], out->c[i] };
        for (uint32_t k = 0; k < 3u; ++k) {
            out->u_plane[k] += edge[k] * out->over_w[i] * v[i]->u;
            out->v_plane[k] += edge[k] * out->over_w[i] * v[i]->v;
            out->w_plane[k] += edge[k] * out->over_w[i];
        }
    }
    out->depth_x = (out->a[0] * v[0]->z + out->a[1] * v[1]->z + out->a[2] * v[2]->z) / size;
    out->depth_y = (out->b[0] * v[0]->z + out->b[1] * v[1]->z + out->b[2] * v[2]->z) / size;
    out->depth_0 = (out->c[0] * v[0]->z + out->c[1] * v[1]->z + out->c[2] * v[2]->z) / size;
    const double steepest = fabs(out->depth_x) > fabs(out->depth_y) ? fabs(out->depth_x)
                                                                    : fabs(out->depth_y);
    out->depth_reach = steepest * WIMSY_EDGE_MARGIN;
    if (material == WIMSY_MATERIAL_MARK && scene->machine.bias) {
        // Towards the eye by the bias's slope part, its factor times the steepest change over a
        // pixel, and its steps of a 24 bit buffer.
        out->depth_0 += (double)WIMSY_MARK_BIAS_SLOPE * steepest +
                        (double)WIMSY_MARK_BIAS_UNITS / 16777216.0;
    }
    memcpy(out->colour, colour, sizeof(out->colour));
    return true;
}

/// The texel of @p texture's level @p level at (@p x, @p y), which wrap around its sides.
static void TexelAt(WimsyTexture texture, uint32_t level, int64_t x, int64_t y, double out[3]) {
    const int64_t side = (int64_t)(WimsyTextureSide(texture) >> level);
    const int64_t size = side > 0 ? side : 1;
    uint8_t texel[4];
    WimsyTexel(texture, level, (uint32_t)(((x % size) + size) % size),
               (uint32_t)(((y % size) + size) % size), texel);
    out[0] = (double)texel[0];
    out[1] = (double)texel[1];
    out[2] = (double)texel[2];
}

/// What a lookup of level @p level at (@p u, @p v) gives, the four texels around it weighed.
static void Bilinear(WimsyTexture texture, uint32_t level, double u, double v, double out[3]) {
    const double side = (double)(WimsyTextureSide(texture) >> level);
    const double x = u * (side > 1.0 ? side : 1.0) - 0.5, y = v * (side > 1.0 ? side : 1.0) - 0.5;
    const double fx = x - floor(x), fy = y - floor(y);
    double texel[4][3];
    for (uint32_t i = 0; i < 4u; ++i) {
        TexelAt(texture, level, (int64_t)floor(x) + (int64_t)(i & 1u),
                (int64_t)floor(y) + (int64_t)(i >> 1), texel[i]);
    }
    for (uint32_t k = 0; k < 3u; ++k) {
        const double top = texel[0][k] + (texel[1][k] - texel[0][k]) * fx;
        const double bottom = texel[2][k] + (texel[3][k] - texel[2][k]) * fx;
        out[k] = top + (bottom - top) * fy;
    }
}

/// The range a lookup at (@p u, @p v) may give at any level of detail from @p least to @p most:
/// every texel a lookup there can weigh, on every level it can reach.
static void TexelRange(WimsyTexture texture, double u, double v, double least, double most,
                       double low[3], double high[3]) {
    const uint32_t last = WimsyTextureLevels(texture) - 1u;
    const uint32_t from = least <= 0.0 ? 0u : (uint32_t)floor(least);
    const uint32_t to = most <= 0.0 ? 0u : most >= (double)last ? last : (uint32_t)ceil(most);
    for (uint32_t k = 0; k < 3u; ++k) {
        low[k] = 255.0;
        high[k] = 0.0;
    }
    for (uint32_t level = from < last ? from : last; level <= to; ++level) {
        const double side = (double)(WimsyTextureSide(texture) >> level);
        const double x = u * (side > 1.0 ? side : 1.0) - 0.5, y = v * (side > 1.0 ? side : 1.0) - 0.5;
        // A sixteenth of a texel either way, for where a machine's own arithmetic puts the lookup.
        for (int64_t ty = (int64_t)floor(y - 0.0625); ty <= (int64_t)floor(y + 0.0625) + 1; ++ty) {
            for (int64_t tx = (int64_t)floor(x - 0.0625); tx <= (int64_t)floor(x + 0.0625) + 1;
                 ++tx) {
                double texel[3];
                TexelAt(texture, level, tx, ty, texel);
                for (uint32_t k = 0; k < 3u; ++k) {
                    low[k] = texel[k] < low[k] ? texel[k] : low[k];
                    high[k] = texel[k] > high[k] ? texel[k] : high[k];
                }
            }
        }
    }
}

/// A textured triangle's coordinates at (@p x, @p y) and, in texels of level nought, the larger
/// of each one's two changes over a pixel. False where the triangle's plane is behind the eye.
static bool Coordinates(const Triangle *t, WimsyTexture texture, double x, double y, double *u,
                        double *v, double change[4]) {
    const double w = t->w_plane[0] * x + t->w_plane[1] * y + t->w_plane[2];
    if (w <= 0.0) {
        return false;
    }
    *u = (t->u_plane[0] * x + t->u_plane[1] * y + t->u_plane[2]) / w;
    *v = (t->v_plane[0] * x + t->v_plane[1] * y + t->v_plane[2]) / w;
    const double side = (double)WimsyTextureSide(texture);
    change[0] = (t->u_plane[0] - *u * t->w_plane[0]) / w * side;
    change[1] = (t->u_plane[1] - *u * t->w_plane[1]) / w * side;
    change[2] = (t->v_plane[0] - *v * t->w_plane[0]) / w * side;
    change[3] = (t->v_plane[1] - *v * t->w_plane[1]) / w * side;
    return true;
}

/// A textured triangle's colour at (@p x, @p y): a machine's own, or for the judge the range of
/// what any machine the specification allows may give.
static void ShadeTexture(const Scene *scene, const Triangle *t, double x, double y, double low[3],
                         double high[3]) {
    const WimsyTexture texture = t->material == WIMSY_MATERIAL_FLOOR
                                     ? WimsyFloorTexture(scene->level)
                                     : WIMSY_TEXTURE_LANE;
    const double last = (double)(WimsyTextureLevels(texture) - 1u);
    double u = 0.0, v = 0.0, change[4];
    if (!Coordinates(t, texture, x, y, &u, &v, change)) {
        for (uint32_t k = 0; k < 3u; ++k) {
            low[k] = 0.0;
            high[k] = 255.0;
        }
        return;
    }
    if (!scene->judging) {
        // The level of detail the specification's own formula gives: the longer of the two
        // changes over a pixel, each a length.
        const double along_x = sqrt(change[0] * change[0] + change[2] * change[2]);
        const double along_y = sqrt(change[1] * change[1] + change[3] * change[3]);
        const double longer = along_x > along_y ? along_x : along_y;
        double level = (longer > 0.0 ? log2(longer) : 0.0) + (double)scene->machine.coarser;
        level = level < 0.0 ? 0.0 : level > last ? last : level;
        if (!scene->machine.between_levels) {
            level = floor(level + 0.5);
        }
        double finer[3], coarser[3];
        Bilinear(texture, (uint32_t)floor(level), u, v, finer);
        Bilinear(texture, (uint32_t)ceil(level), u, v, coarser);
        for (uint32_t k = 0; k < 3u; ++k) {
            low[k] = high[k] = finer[k] + (coarser[k] - finer[k]) * (level - floor(level));
        }
        return;
    }
    // A machine measures the changes over a block of two by two pixels, so anywhere within a
    // pixel of this one; and may take any level from the larger of the two coordinates' changes
    // to their sum (OpenGL 4.6, 8.14.1), an eighth of a level either way.
    double least = 1e30, most = 0.0;
    static const double AROUND[5][2] = { { 0, 0 }, { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    for (uint32_t i = 0; i < 5u; ++i) {
        double pu = 0.0, pv = 0.0;
        if (i > 0u && !Coordinates(t, texture, x + AROUND[i][0], y + AROUND[i][1], &pu, &pv, change)) {
            continue;
        }
        const double in_u = fabs(change[0]) > fabs(change[1]) ? fabs(change[0]) : fabs(change[1]);
        const double in_v = fabs(change[2]) > fabs(change[3]) ? fabs(change[2]) : fabs(change[3]);
        const double larger = in_u > in_v ? in_u : in_v;
        least = larger < least ? larger : least;
        most = in_u + in_v > most ? in_u + in_v : most;
    }
    double from = (least > 0.0 ? log2(least) : -64.0) - WIMSY_LOD_MARGIN;
    double to = (most > 0.0 ? log2(most) : -64.0) + WIMSY_LOD_MARGIN;
    from = from < 0.0 ? 0.0 : from > last ? last : from;
    to = to < 0.0 ? 0.0 : to > last ? last : to;
    if (texture != WIMSY_TEXTURE_LANE) {
        TexelRange(texture, u, v, from, to, low, high);
        if (WimsyTextureBlock(texture) != 0u) {
            for (uint32_t k = 0; k < 3u; ++k) {
                low[k] = low[k] > WIMSY_ASTC_MARGIN ? low[k] - WIMSY_ASTC_MARGIN : 0.0;
                high[k] = high[k] < 255.0 - WIMSY_ASTC_MARGIN ? high[k] + WIMSY_ASTC_MARGIN : 255.0;
            }
        }
        return;
    }
    // The lane's levels are each one colour, so a level between two is that far from one to the
    // other: the range is over the levels of detail alone, its ends and each whole level between.
    for (uint32_t k = 0; k < 3u; ++k) {
        low[k] = 255.0;
        high[k] = 0.0;
    }
    for (double level = from;; level = floor(level) + 1.0 < to ? floor(level) + 1.0 : to) {
        double finer[3], coarser[3];
        TexelAt(texture, (uint32_t)floor(level), 0, 0, finer);
        TexelAt(texture, (uint32_t)ceil(level), 0, 0, coarser);
        for (uint32_t k = 0; k < 3u; ++k) {
            const double value = finer[k] + (coarser[k] - finer[k]) * (level - floor(level));
            low[k] = value < low[k] ? value : low[k];
            high[k] = value > high[k] ? value : high[k];
        }
        if (level >= to) {
            break;
        }
    }
}

/// The colour triangle @p t has at (@p x, @p y), nought to 255 a channel: its range, since a
/// point too far outside a model's triangle to weigh its corners may get any shade of its colour,
/// and a texture's level of detail is a machine's to pick within bounds.
static void Shade(const Scene *scene, const Triangle *t, double x, double y, double low[3],
                  double high[3]) {
    if (t->material == WIMSY_MATERIAL_FLOOR || t->material == WIMSY_MATERIAL_LANE) {
        ShadeTexture(scene, t, x, y, low, high);
        return;
    }
    double dark = 1.0, lit = 1.0;
    if (scene->machine.lit && t->material == WIMSY_MATERIAL_MODEL) {
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
        if (scene->span[i][1] + 1.0 < (double)x0 || scene->span[i][0] - 1.0 > (double)(x0 + TILE) ||
            scene->span[i][3] + 1.0 < (double)y0 || scene->span[i][2] - 1.0 > (double)(y0 + TILE)) {
            continue;
        }
        if (scene->held != i) {
            HoldInstance(scene, i);
        }
        const float *const tint = scene_instance[i].tint;
        const uint8_t (*const drawing)[3] = WIMSY_MESH_PALETTE[WimsyInstanceVariant(scene->level, i)];
        for (uint32_t t = 0; t + 2u < WIMSY_MESH_INDICES; t += 3u) {
            const Corner *const v[3] = { &corners[WIMSY_MESH_INDEX[t]],
                                         &corners[WIMSY_MESH_INDEX[t + 1u]],
                                         &corners[WIMSY_MESH_INDEX[t + 2u]] };
            const uint8_t *const palette = drawing[WIMSY_MESH_VERTEX[WIMSY_MESH_INDEX[t]].colour];
            // The byte as the vertex buffer's normalised attribute gives it, times the tint.
            const double colour[3] = { (double)((float)palette[0] / 255.0f) * (double)tint[0],
                                       (double)((float)palette[1] / 255.0f) * (double)tint[1],
                                       (double)((float)palette[2] / 255.0f) * (double)tint[2] };
            if (SetUp(scene, v, colour, WIMSY_MATERIAL_MODEL, (double)x0, (double)y0, &triangle)) {
                each(scene, &triangle, x0, y0);
            }
        }
    }
    for (uint32_t d = 0; d < scene->draws; ++d) {
        const WimsyWorldDraw *const draw = &world_draws[d];
        for (uint32_t t = draw->first; t + 2u < draw->first + draw->count; t += 3u) {
            const Corner *const v[3] = { &world_corners[t], &world_corners[t + 1u],
                                         &world_corners[t + 2u] };
            const double colour[3] = { (double)world_vertices[t].r, (double)world_vertices[t].g,
                                       (double)world_vertices[t].b };
            if (SetUp(scene, v, colour, draw->material, (double)x0, (double)y0, &triangle)) {
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
    if (!t->drawn || !SpanOf(t, (double)x0, (double)y0, &span)) {
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
                float depth = (float)(t->depth_x * x + t->depth_y * y + t->depth_0);
                if (scene->machine.depth_bits != 0u) {
                    // A buffer of that many bits keeps the step under the depth.
                    const double steps = (double)((1u << scene->machine.depth_bits) - 1u);
                    depth = (float)(floor((double)depth * steps) / steps);
                }
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
    scene->shown = shown;
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
    for (uint32_t i = 0; i < scene->instances; ++i) {
        HoldInstance(scene, i);
        scene->span[i][0] = scene->left;
        scene->span[i][1] = scene->right;
        scene->span[i][2] = scene->top;
        scene->span[i][3] = scene->bottom;
    }
    WimsyLightOf(level, &scene->light);
    scene->draws = WimsyWorldOf(level, world_vertices, &scene->world_count, world_draws);
    float view[16];
    WimsyViewOf(level, shown, frames, view);
    for (uint32_t i = 0; i < scene->world_count; ++i) {
        // The floats the vertex buffer holds, through the matrix the uniform buffer holds.
        const WimsyWorldVertex *const vertex = &world_vertices[i];
        const double position[3] = { (double)vertex->x, (double)vertex->y, (double)vertex->z };
        double clip[4];
        for (uint32_t row = 0; row < 4u; ++row) {
            clip[row] = (double)view[12u + row];
            for (uint32_t k = 0; k < 3u; ++k) {
                clip[row] += (double)view[k * 4u + row] * position[k];
            }
        }
        Corner *const corner = &world_corners[i];
        memset(corner, 0, sizeof(*corner));
        corner->over_w = 1.0 / clip[3];
        corner->x = (clip[0] * corner->over_w + 1.0) * 0.5 * (double)WIMSY_WIDTH;
        corner->y = (1.0 - clip[1] * corner->over_w) * 0.5 * (double)WIMSY_HEIGHT;
        corner->z = clip[2] * corner->over_w;
        corner->u = (double)vertex->u;
        corner->v = (double)vertex->v;
    }
}

/// The panels' pixels of the tile at (@p x0, @p y0), drawn over what the scene left there: each
/// its texel's bytes, into tile_low, and tile_high too when @p both.
static void PanelsOver(const Scene *scene, uint32_t x0, uint32_t y0, bool both) {
    if (scene->level < 3u) {
        return;
    }
    for (uint32_t p = 0; p < TILE * TILE; ++p) {
        uint8_t texel[4];
        const WimsyPanel *const panel =
            PanelAt(scene->level, scene->shown, x0 + p % TILE, y0 + p / TILE, texel);
        if (panel == NULL) {
            continue;
        }
        memcpy(tile_low[p], texel, 4u);
        if (both) {
            memcpy(tile_high[p], texel, 4u);
        }
        if (both && panel->kind == WIMSY_PANEL_LOAD) {
            // An ASTC texel, which a machine may keep in a format of its own (WIMSY_ASTC_MARGIN).
            for (uint32_t k = 0; k < 3u; ++k) {
                const uint32_t margin = (uint32_t)WIMSY_ASTC_MARGIN;
                tile_low[p][k] = (uint8_t)(texel[k] > margin ? texel[k] - margin : 0u);
                tile_high[p][k] = (uint8_t)(texel[k] < 255u - margin ? texel[k] + margin : 255u);
            }
        }
    }
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
    PanelsOver(scene, x0, y0, true);
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
                PanelsOver(&scene, x0, y0, false);
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
    scene.judging = true;
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
                         "%u pixels held to a colour, %u to a wider range, on an edge or "
                         "between levels of detail",
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
