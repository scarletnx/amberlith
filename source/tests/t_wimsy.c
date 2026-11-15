// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <switch.h>

#include "../check.h"
#include "../coherref.h"
#include "../coverageref.h"
#include "../gameref.h"
#include "../gpu.h"
#include "../mode.h"
#include "../picture.h"
#include "../report.h"
#include "../shaders.h"
#include "../viewer.h"
#include "../wimsymesh.h"
#include "../wimsyref.h"
#include "tests.h"

// The game family's levels: Wimsy drawn as a game draws a character, a level a scene, from the
// lightest frame a game makes to the heaviest (wimsyref.h has what each level draws). A level
// runs the frames the game scene runs, on the target a game renders to, and presents each one:
// the main thread, on core 0, works out the frame and writes what it reads into one of two
// slots; the render thread, on core 1, takes an image from the swapchain, records and submits
// the frame, and presents it. Each frame is timed as the game scene's are.
//
// Five frames of a run are judged, its first, its quarters and its last: the render thread
// copies each out after its present and keeps it, and once the frames are over the reference
// works out the range every pixel may hold and each kept frame is held to it. A frame's image
// depends on its number and the run's length alone, so a machine that runs the frames slowly
// gets the same verdicts as one that keeps up.

static const char *const SUITE = "wimsy";

#define SWAP_IMAGES 2u
// The main thread is one frame ahead of the render thread, at most: two slots of what it writes.
#define SLOTS 2u

#define MAIN_CORE 0
#define RENDER_CORE 1
#define RENDER_PRIORITY 0x2B
#define STACK_SIZE 0x10000

#define BATCH_VERTICES (WIMSY_SPRITES * 6u)
// The image descriptors, in one set: the sprite, the judged target, the levels' textures, each
// level of each compute texture to sample, each chain target, and each level of each compute
// texture to store into.
#define IMAGE_TEXTURES 2u
#define IMAGE_COMPUTE (IMAGE_TEXTURES + WIMSY_TEXTURES)
#define IMAGE_CHAIN (IMAGE_COMPUTE + WIMSY_COMPUTE_TEXTURES * WIMSY_COMPUTE_LEVELS)
#define IMAGE_STORE (IMAGE_CHAIN + WIMSY_CHAINS * WIMSY_CHAIN_STEPS)
#define IMAGE_LOAD (IMAGE_STORE + WIMSY_COMPUTE_TEXTURES * WIMSY_COMPUTE_LEVELS)
#define IMAGE_PLAQUES (IMAGE_LOAD + WIMSY_LOADS)
#define IMAGE_DESCRIPTORS (IMAGE_PLAQUES + 1u)
/// What a judged frame of a heavy level reads back whole: every level of every compute texture,
/// a texture's 21,845 texels one after the other, then every chain's last target.
#define COMPUTE_TEXELS \
    ((WIMSY_COMPUTE_SIDE * WIMSY_COMPUTE_SIDE * 4u - 1u) / 3u)
#define SCREENS_BYTES \
    ((WIMSY_COMPUTE_TEXTURES * COMPUTE_TEXELS + WIMSY_CHAINS * WIMSY_CHAIN_SIDE * WIMSY_CHAIN_SIDE) * 4u)
// The textures a run loads, two a frame, each an image of its own up to this many: twenty
// seconds' worth, after which the oldest are written again.
#define LOAD_RING 1200u
#define LOAD_BYTES (WIMSY_LOAD_BLOCKS * WIMSY_LOAD_BLOCKS * 16u)
// A 512 by 512 texture of 8 by 8 blocks is 64 rows of 1024 bytes: blocks of 8 gobs.
#define LOAD_TILE 3u
// The worker threads: one a load, two that share the models' uniform buffers.
#define WORKERS 4u
#define WORKER_PRIORITY 0x2C
#define FILLS (WIMSY_COMPUTE_TEXTURES * WIMSY_COMPUTE_LEVELS)
#define SQUARES (WIMSY_CHAINS * WIMSY_CHAIN_STEPS)
// A draw's uniform buffer starts where the graphics unit lets one start.
#define INSTANCE_STRIDE DK_UNIFORM_BUF_ALIGNMENT
_Static_assert(sizeof(WimsyInstance) <= INSTANCE_STRIDE, "an instance fits a uniform buffer's step");
_Static_assert(sizeof(WimsyBlade) == 20u, "a blade is the instance buffer's step, no gap in it");
_Static_assert(sizeof(WimsyPuff) == 20u, "a puff is the instance buffer's step, no gap in it");
_Static_assert(sizeof(WimsyInstance) == 128u && sizeof(WimsyLight) == 32u,
               "the blocks are laid out as the shaders read them");
_Static_assert(sizeof(WimsyWorldVertex) == sizeof(GpuVertex) &&
                   offsetof(WimsyWorldVertex, u) == offsetof(GpuVertex, u) &&
                   offsetof(WimsyWorldVertex, r) == offsetof(GpuVertex, r),
               "a world vertex is laid out as the drawing suites' vertex");

/// The model's vertex as the vertex buffer holds it.
typedef struct {
    float position[3];
    float normal[3];
    uint8_t colour[4];
} ModelVertex;

static uint32_t frames = MODE_FRAMES_DEFAULT;
static bool timing = true;
static uint32_t levels;

void WimsySetFrames(uint32_t count) {
    frames = count >= 1u && count <= MODE_FRAMES_MAX ? count : MODE_FRAMES_DEFAULT;
}

void WimsySetTiming(bool on) {
    timing = on;
}

void WimsySetLevels(uint32_t mask) {
    levels = mask;
}

// The browser's run of a level as a demo, NULL in every run from the mode file; whether its frames
// go through the program's screen rather than a swapchain; and the frames the run still wants,
// which B makes fewer.
static const DemoRun *demo;
static bool on_screen;
static uint32_t frames_wanted;
static const uint8_t *last_shown;

void WimsySetDemo(const DemoRun *run) {
    demo = run;
}

// A person's camera at a heavy level: where it stands, eye high, and how it is turned, of 65536
// a turn. The left stick walks it over the floor, the right one turns it.
static bool free_camera;
static float free_eye[3];
static uint32_t free_heading;
static int32_t free_pitch;

void WimsySetFree(bool on) {
    free_camera = on;
}

/// A stick's push past its dead middle, from -1 to 1.
static float Push(int32_t value) {
    return value > 6000 || value < -6000 ? (float)value / 32767.0f : 0.0f;
}

/// One frame of the person's camera: turned two degrees and walked a quarter of a unit at a full
/// push, kept between the walls, over the floor's length, and from looking straight up or down.
static void SteerCamera(uint32_t k) {
    if (k == 0u) {
        free_eye[0] = 0.0f;
        free_eye[1] = 2.5f;
        free_eye[2] = 6.0f;
        free_heading = 0u;
        free_pitch = 0;
    }
    DemoSticks sticks = { { 0, 0 }, { 0, 0 } };
    if (demo != NULL && demo->sticks != NULL) {
        demo->sticks(&sticks);
    }
    free_heading = (uint32_t)((int32_t)free_heading - (int32_t)(Push(sticks.right[0]) * 364.0f)) & 0xFFFFu;
    free_pitch += (int32_t)(Push(sticks.right[1]) * 273.0f);
    free_pitch = free_pitch > 10922 ? 10922 : free_pitch < -10922 ? -10922 : free_pitch;
    float ahead[3], level_ahead[3];
    WimsyLookAt(free_heading, free_pitch, ahead);
    WimsyLookAt(free_heading, 0, level_ahead);
    const float walk = Push(sticks.left[1]) * 0.25f, aside = Push(sticks.left[0]) * 0.25f;
    free_eye[0] += level_ahead[0] * walk - level_ahead[2] * aside;
    free_eye[2] += level_ahead[2] * walk + level_ahead[0] * aside;
    free_eye[0] = free_eye[0] > 17.0f ? 17.0f : free_eye[0] < -17.0f ? -17.0f : free_eye[0];
    free_eye[2] = free_eye[2] > 6.0f ? 6.0f : free_eye[2] < -190.0f ? -190.0f : free_eye[2];
    WimsyCameraAsk(free_eye, ahead);
}

// The graphics device and everything a level's frames draw with, made before its first frame.
static uint32_t level;
static GpuContext gpu;
static DkShader common_vs;
static DkShader texture_fs;
static DkShader model_vs;
static DkShader lit_fs;
/// A heavy frame's materials: the light as four fragment programs, lit_fs the first.
#define MATERIALS 4u
static DkShader material_fs[MATERIALS - 1u];
static DkShader world_vs;
static DkShader grass_vs;
static DkShader smoke_vs;
static DkShader smoke_fs;
static DkGpuAddr puff_unit;
static WimsyPuff *puff_cpu[SLOTS];
static DkGpuAddr puff_gpu[SLOTS];
static uint32_t puff_count;
static DkGpuAddr blade_unit;
static DkGpuAddr blade_instances;
static uint32_t blade_count;
static DkShader flat_fs;
static DkShader fill_cs;
static DkImage compute_textures[WIMSY_COMPUTE_TEXTURES];
static DkImage chain_targets[WIMSY_CHAINS * WIMSY_CHAIN_STEPS];
static DkImage plaque_texture;
static DkImageLayout plaque_layout;
static uint8_t *screens_cpu[SLOTS];
static DkGpuAddr screens_gpu[SLOTS];
/// Whether the frame being recorded is one the judge keeps, whose textures are read back whole.
static bool frame_judged;
static DkGpuAddr chain_quad;
static DkImage level_textures[WIMSY_TEXTURES];
static DkImageLayout level_texture_layouts[WIMSY_TEXTURES];
static DkGpuAddr world_vertices;
static uint32_t world_vertex_count;
static WimsyWorldDraw world_draws[WIMSY_WORLD_DRAWS_MAX];
static uint32_t world_draw_count;
static DkImage sprite_texture;
static DkImageLayout sprite_layout;
static DkImage colour_samples;
static DkImageLayout colour_samples_layout;
static DkImage depth_samples;
static DkImageLayout depth_samples_layout;
static DkImage swap_images[SWAP_IMAGES];
static DkImageLayout swap_layouts[SWAP_IMAGES];
static DkSwapchain swapchain;
static DkSwapchainMaker swap_maker;
static DkGpuAddr image_set[SLOTS];
static DkImageDescriptor *image_descriptors[SLOTS];
static DkImage load_images[LOAD_RING];
static uint8_t *load_memory[LOAD_RING];
static uint32_t load_ring;
static uint8_t *peek_cpu[SLOTS];
static DkGpuAddr peek_gpu[SLOTS];
static DkGpuAddr sampler_set;
static DkGpuAddr shown_quad;
static DkGpuAddr model_vertices[WIMSY_MESH_VARIANTS];
static DkGpuAddr model_indices;
static DkGpuAddr light_block;

// What the main thread writes for a frame, one slot per frame in flight.
static GpuVertex *batch_cpu[SLOTS];
static DkGpuAddr batch_gpu[SLOTS];
static uint8_t *instance_cpu[SLOTS];
static DkGpuAddr instance_gpu[SLOTS];
static uint32_t instance_count[SLOTS];
static float *view_cpu[SLOTS];
static DkGpuAddr view_gpu[SLOTS];
static uint8_t *fill_cpu[SLOTS];
static DkGpuAddr fill_gpu[SLOTS];
static GpuVertex *square_cpu[SLOTS];
static DkGpuAddr square_gpu[SLOTS];

// What the frames left: their times, and the judged ones' images.
static uint32_t frame_us[MODE_FRAMES_MAX];
static uint32_t sorted_us[MODE_FRAMES_MAX];
static uint32_t main_us[MODE_FRAMES_MAX];
static uint32_t render_us[MODE_FRAMES_MAX];
static uint32_t judged[WIMSY_JUDGED];
static uint32_t judged_count;
static uint8_t *kept[WIMSY_JUDGED];
static uint8_t *expected_pixels;
static uint8_t *pixel_verdicts;
static GameWorld world;
static WimsySprite sprites[WIMSY_SPRITES];
static WimsyInstance instances[WIMSY_INSTANCES_MAX];

static Thread render_thread;
static Semaphore frame_go;
static Semaphore slot_free;
static Thread workers[WORKERS];
static Semaphore work_go[WORKERS];
static Semaphore work_done;
static bool work_over;
static uint32_t work_frame;
static uint32_t work_slot;
static uint32_t work_models[WORKERS];

// What the render thread saw, read by the main thread once it is joined.
static struct {
    uint32_t presented;
    uint32_t copied;
    /// The longest a judged frame's copy took, which falls after its present.
    uint32_t copy_longest;
    /// What the processor read back of each frame's compute work: how many frames, how many of
    /// them another value, and the first of those with what it read.
    uint32_t peeked;
    uint32_t peek_wrong;
    uint32_t peek_wrong_at;
    uint8_t peek_got[4];
    /// The judged frames whose compute textures and chains were read back whole, the texels of
    /// them that were not what the processor works out, and the first of those, said.
    uint32_t screens_read;
    uint32_t screens_wrong;
    char screens_first[160];
} render;

static uint32_t Microseconds(uint64_t ticks) {
    const uint64_t us = armTicksToNs(ticks) / 1000u;
    return us > UINT32_MAX ? UINT32_MAX : (uint32_t)us;
}

static GpuVertex Corner(float x, float y, float u, float v) {
    const GpuVertex vertex = { x, y, 0.0f, u, v, 1.0f, 1.0f, 1.0f, 1.0f };
    return vertex;
}

/// Six vertices over the pixels from (@p x, @p y), @p side each way, of a target @p width by
/// @p height, in @p colour: pixel corners, which clip space holds exactly. The texture
/// coordinates run from nought to @p extent across it.
static void PixelSquare(GpuVertex out[6], uint32_t x, uint32_t y, uint32_t side, uint32_t width,
                        uint32_t height, const uint8_t colour[4], float extent) {
    const float left = (float)x / (float)(width / 2u) - 1.0f;
    const float right = (float)(x + side) / (float)(width / 2u) - 1.0f;
    const float top = 1.0f - (float)y / (float)(height / 2u);
    const float bottom = 1.0f - (float)(y + side) / (float)(height / 2u);
    const float u[6] = { 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f };
    const float v[6] = { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f };
    for (uint32_t i = 0; i < 6u; ++i) {
        const GpuVertex vertex = { u[i] != 0.0f ? right : left,
                                   v[i] != 0.0f ? bottom : top,
                                   0.0f,
                                   u[i] * extent,
                                   v[i] * extent,
                                   (float)colour[0] / 255.0f,
                                   (float)colour[1] / 255.0f,
                                   (float)colour[2] / 255.0f,
                                   (float)colour[3] / 255.0f };
        out[i] = vertex;
    }
}

/// Six vertices, two triangles, over the clip space rectangle from @p x0, @p y0 (its top left)
/// to @p x1, @p y1, the texture's @p u0 at its left and @p u1 at its right.
static void Rectangle(GpuVertex out[6], float x0, float y0, float x1, float y1, float u0,
                      float u1) {
    const GpuVertex top_left = Corner(x0, y0, u0, 0.0f);
    const GpuVertex top_right = Corner(x1, y0, u1, 0.0f);
    const GpuVertex bottom_right = Corner(x1, y1, u1, 1.0f);
    const GpuVertex bottom_left = Corner(x0, y1, u0, 1.0f);
    out[0] = top_left;
    out[1] = top_right;
    out[2] = bottom_right;
    out[3] = top_left;
    out[4] = bottom_right;
    out[5] = bottom_left;
}

static void RunWorkers(uint32_t slot, uint32_t k);

/// Everything frame @p k reads from the processor, into @p slot: level 0's batch of sprites,
/// stepped with the world, or a higher level's uniform buffers, a draw each.
static void WriteSlot(uint32_t slot, uint32_t k) {
    if (level == 0u) {
        if (k > 0u) {
            GameWorldStep(&world);
        }
        WimsySpritesOf(&world, sprites);
        for (uint32_t i = 0; i < WIMSY_SPRITES; ++i) {
            // A sprite's corners are pixel corners, half a pixel from any pixel's centre, and a
            // texel's edges fall on them, so which texel a pixel reads is no rounding's to decide.
            const float left = (float)sprites[i].x / (float)(WIMSY_WIDTH / 2u) - 1.0f;
            const float right = (float)(sprites[i].x + (int32_t)sprites[i].side) /
                                    (float)(WIMSY_WIDTH / 2u) -
                                1.0f;
            const float top = 1.0f - (float)sprites[i].y / (float)(WIMSY_HEIGHT / 2u);
            const float bottom = 1.0f - (float)(sprites[i].y + (int32_t)sprites[i].side) /
                                            (float)(WIMSY_HEIGHT / 2u);
            Rectangle(batch_cpu[slot] + i * 6u, left, top, right, bottom,
                      sprites[i].mirrored ? 1.0f : 0.0f, sprites[i].mirrored ? 0.0f : 1.0f);
        }
        return;
    }
    if (free_camera && level >= 3u) {
        SteerCamera(k);
    }
    WimsyViewOf(level, k, frames, view_cpu[slot]);
    // After the matrix, the frame's wind, which the grass's vertex program reads.
    view_cpu[slot][16] = WimsyWindOf(k);
    WimsyPuffScale(&view_cpu[slot][17]);
    view_cpu[slot][19] = 0.0f;
    // The frame's smoke, each puff where the frame's number has it.
    WimsyPuffsOf(puff_count, k, puff_cpu[slot]);
    if (level < 3u) {
        instance_count[slot] = WimsyInstancesOf(level, k, frames, instances);
        for (uint32_t i = 0; i < instance_count[slot]; ++i) {
            memcpy(instance_cpu[slot] + (size_t)i * INSTANCE_STRIDE, &instances[i],
                   sizeof(instances[i]));
        }
        return;
    }
    // A heavy frame's models and loads are the workers', done before the frame is handed over.
    RunWorkers(slot, k);
    instance_count[slot] = 0;
    for (uint32_t w = WIMSY_LOADS; w < WORKERS; ++w) {
        instance_count[slot] += work_models[w];
    }
    // A heavy frame's dispatches read the frame from a uniform buffer each, and its chains'
    // squares are where this frame puts them.
    for (uint32_t f = 0; f < FILLS; ++f) {
        const uint32_t mip = f % WIMSY_COMPUTE_LEVELS;
        const uint32_t seed[4] = { f / WIMSY_COMPUTE_LEVELS, mip, k, WIMSY_COMPUTE_SIDE >> mip };
        memcpy(fill_cpu[slot] + (size_t)f * DK_UNIFORM_BUF_ALIGNMENT, seed, sizeof(seed));
    }
    for (uint32_t s = 0; s < SQUARES; ++s) {
        WimsyChainSquare square;
        WimsyChainSquareOf(s / WIMSY_CHAIN_STEPS, s % WIMSY_CHAIN_STEPS, k, &square);
        PixelSquare(square_cpu[slot] + s * 6u, square.x, square.y, WIMSY_CHAIN_SQUARE,
                    WIMSY_CHAIN_SIDE, WIMSY_CHAIN_SIDE, square.colour, 1.0f);
    }
    // The frame's loads are images no frame before it drew with: their descriptors are written
    // where the frame's set has them.
    for (uint32_t l = 0; l < WIMSY_LOADS; ++l) {
        DkImageView view;
        dkImageViewDefaults(&view, &load_images[(k * WIMSY_LOADS + l) % load_ring]);
        dkImageDescriptorInitialize(&image_descriptors[slot][IMAGE_LOAD + l], &view, false, false);
    }
}

/// A worker's share of frame work_frame, into slot work_slot: a load's blocks, written straight
/// into the memory the graphics unit samples, or half the models' uniform buffers.
static void WorkerMain(void *argument) {
    const uint32_t worker = (uint32_t)(uintptr_t)argument;
    for (;;) {
        semaphoreWait(&work_go[worker]);
        if (__atomic_load_n(&work_over, __ATOMIC_ACQUIRE)) {
            return;
        }
        const uint32_t k = work_frame, slot = work_slot;
        if (worker < WIMSY_LOADS) {
            const uint32_t load = k * WIMSY_LOADS + worker;
            uint8_t *const memory = load_memory[load % load_ring];
            for (uint32_t by = 0; by < WIMSY_LOAD_BLOCKS; ++by) {
                for (uint32_t bx = 0; bx < WIMSY_LOAD_BLOCKS; ++bx) {
                    // A block is one 16 byte run of a gob, so its bytes stay together.
                    WimsyLoadBlock(load, bx, by,
                                   memory + CohBlockLinearOffset(bx * 16u, by, WIMSY_LOAD_BLOCKS * 16u,
                                                                 1u << LOAD_TILE));
                }
            }
        } else {
            static WimsyInstance share[WORKERS][WIMSY_INSTANCES_MAX];
            const uint32_t half = (WIMSY_INSTANCES_MAX + 1u) / 2u;
            const uint32_t first = (worker - WIMSY_LOADS) * half;
            const uint32_t count = WimsyInstancesFrom(level, k, frames, first, half, share[worker]);
            work_models[worker] = count;
            for (uint32_t i = 0; i < count; ++i) {
                memcpy(instance_cpu[slot] + (size_t)(first + i) * INSTANCE_STRIDE, &share[worker][i],
                       sizeof(share[worker][i]));
            }
        }
        semaphoreSignal(&work_done);
    }
}

/// Hands frame @p k's work for @p slot to the workers and waits for them all, as a game's frame
/// waits for its jobs before it is drawn.
static void RunWorkers(uint32_t slot, uint32_t k) {
    work_frame = k;
    work_slot = slot;
    for (uint32_t w = 0; w < WORKERS; ++w) {
        semaphoreSignal(&work_go[w]);
    }
    for (uint32_t w = 0; w < WORKERS; ++w) {
        semaphoreWait(&work_done);
    }
}

static void BindBlend(bool on) {
    DkColorState colour;
    dkColorStateDefaults(&colour);
    dkColorStateSetBlendEnable(&colour, 0, on);
    dkCmdBufBindColorState(gpu.cmdbuf, &colour);
    DkBlendState blend;
    dkBlendStateDefaults(&blend);
    // What is under a texel stays by as much as the texel is clear, as a game's sprites blend.
    dkBlendStateSetFactors(&blend, DkBlendFactor_SrcAlpha, DkBlendFactor_InvSrcAlpha,
                           DkBlendFactor_One, DkBlendFactor_InvSrcAlpha);
    dkCmdBufBindBlendState(gpu.cmdbuf, 0, &blend);
}

static void ClearColour(void) {
    dkCmdBufClearColorFloat(gpu.cmdbuf, 0, DkColorMask_RGBA, (float)WIMSY_CLEAR[0] / 255.0f,
                            (float)WIMSY_CLEAR[1] / 255.0f, (float)WIMSY_CLEAR[2] / 255.0f,
                            (float)WIMSY_CLEAR[3] / 255.0f);
}

/// Level 0's frame: the batch the main thread wrote, one draw, blended over the clear colour.
static void RecordSprites(uint32_t slot) {
    ClearColour();
    BindBlend(true);
    dkCmdBufBindTexture(gpu.cmdbuf, DkStage_Fragment, 0, dkMakeTextureHandle(0, 0));
    GpuBindVertices(&gpu, &common_vs, &texture_fs, batch_gpu[slot],
                    BATCH_VERTICES * (uint32_t)sizeof(GpuVertex));
    dkCmdBufDraw(gpu.cmdbuf, DkPrimitive_Triangles, BATCH_VERTICES, 1, 0, 0);
    BindBlend(false);
}

/// Hands what the list holds so far to the queue and goes on recording, with no wait: a game
/// submits a frame in some forty pieces, and so does a heavy frame, one a compute texture, one a
/// chain and one every twenty models.
static void SubmitPart(void) {
    dkQueueSubmitCommands(gpu.queue, dkCmdBufFinishList(gpu.cmdbuf));
    dkQueueFlush(gpu.queue);
}

/// A heavy frame's compute work: each level of each compute texture written by a dispatch of its
/// own, through a view of that level alone.
static void RecordFills(uint32_t slot) {
    const DkShader *const shaders[] = { &fill_cs };
    dkCmdBufBindShaders(gpu.cmdbuf, DkStageFlag_Compute, shaders, 1);
    for (uint32_t f = 0; f < FILLS; ++f) {
        const uint32_t side = WIMSY_COMPUTE_SIDE >> (f % WIMSY_COMPUTE_LEVELS);
        dkCmdBufBindUniformBuffer(gpu.cmdbuf, DkStage_Compute, 0,
                                  fill_gpu[slot] + (DkGpuAddr)f * DK_UNIFORM_BUF_ALIGNMENT,
                                  DK_UNIFORM_BUF_ALIGNMENT);
        dkCmdBufBindImage(gpu.cmdbuf, DkStage_Compute, 0, dkMakeImageHandle(IMAGE_STORE + f));
        dkCmdBufDispatchCompute(gpu.cmdbuf, (side + 7u) / 8u, (side + 7u) / 8u, 1);
        if (f % WIMSY_COMPUTE_LEVELS == WIMSY_COMPUTE_LEVELS - 1u) {
            SubmitPart();
        }
    }
    // What the dispatches stored is sampled later in the list, and one texel of it copied out
    // for the processor to read once the frame is drawn, as a game reads back a result.
    dkCmdBufBarrier(gpu.cmdbuf, DkBarrier_Full, DkInvalidateFlags_Image | DkInvalidateFlags_L2Cache);
    DkImageView view;
    dkImageViewDefaults(&view, &compute_textures[WIMSY_PEEK_TEXTURE]);
    view.mipLevelOffset = (uint8_t)WIMSY_PEEK_LEVEL;
    view.mipLevelCount = 1;
    const DkImageRect texel = { 0, 0, 0, 1, 1, 1 };
    const DkCopyBuf out = { peek_gpu[slot], 0, 0 };
    dkCmdBufCopyImageToBuffer(gpu.cmdbuf, &view, &texel, &out, 0);
    if (frame_judged) {
        // A judged frame's compute textures whole, level after level, for the processor to hold
        // to what it works out itself: the wall's screens only show them.
        DkGpuAddr at = screens_gpu[slot];
        for (uint32_t f = 0; f < FILLS; ++f) {
            const uint32_t side = WIMSY_COMPUTE_SIDE >> (f % WIMSY_COMPUTE_LEVELS);
            dkImageViewDefaults(&view, &compute_textures[f / WIMSY_COMPUTE_LEVELS]);
            view.mipLevelOffset = (uint8_t)(f % WIMSY_COMPUTE_LEVELS);
            view.mipLevelCount = 1;
            const DkImageRect whole = { 0, 0, 0, side, side, 1 };
            const DkCopyBuf into = { at, 0, 0 };
            dkCmdBufCopyImageToBuffer(gpu.cmdbuf, &view, &whole, &into, 0);
            at += (DkGpuAddr)side * side * 4u;
        }
    }
    dkCmdBufBarrier(gpu.cmdbuf, DkBarrier_Full, DkInvalidateFlags_Image);
}

/// A judged frame's chains read back: each one's last target, after the compute textures.
static void RecordChainsBack(uint32_t slot) {
    dkCmdBufBarrier(gpu.cmdbuf, DkBarrier_Full, DkInvalidateFlags_Image | DkInvalidateFlags_L2Cache);
    DkGpuAddr at = screens_gpu[slot] + (DkGpuAddr)WIMSY_COMPUTE_TEXTURES * COMPUTE_TEXELS * 4u;
    for (uint32_t c = 0; c < WIMSY_CHAINS; ++c) {
        DkImageView view;
        dkImageViewDefaults(&view, &chain_targets[c * WIMSY_CHAIN_STEPS + WIMSY_CHAIN_STEPS - 1u]);
        const DkImageRect whole = { 0, 0, 0, WIMSY_CHAIN_SIDE, WIMSY_CHAIN_SIDE, 1 };
        const DkCopyBuf into = { at, 0, 0 };
        dkCmdBufCopyImageToBuffer(gpu.cmdbuf, &view, &whole, &into, 0);
        at += (DkGpuAddr)WIMSY_CHAIN_SIDE * WIMSY_CHAIN_SIDE * 4u;
    }
    dkCmdBufBarrier(gpu.cmdbuf, DkBarrier_Full, DkInvalidateFlags_Image);
}

/// Holds what a judged frame read back to what the processor works out for frame @p k: counts
/// the texels that differ and keeps the first.
static void JudgeScreens(uint32_t k, uint32_t slot) {
    const uint8_t *got = screens_cpu[slot];
    ++render.screens_read;
    for (uint32_t f = 0; f < FILLS + WIMSY_CHAINS; ++f) {
        const bool chain = f >= FILLS;
        const uint32_t which = chain ? f - FILLS : f / WIMSY_COMPUTE_LEVELS;
        const uint32_t mip = chain ? 0u : f % WIMSY_COMPUTE_LEVELS;
        const uint32_t side = chain ? WIMSY_CHAIN_SIDE : WIMSY_COMPUTE_SIDE >> mip;
        for (uint32_t y = 0; y < side; ++y) {
            for (uint32_t x = 0; x < side; ++x, got += 4u) {
                uint8_t want[4];
                if (chain) {
                    WimsyChainTexel(which, k, x, y, want);
                } else {
                    WimsyComputeTexel(which, mip, k, x, y, want);
                }
                if (memcmp(got, want, 4u) != 0 && render.screens_wrong++ == 0u) {
                    snprintf(render.screens_first, sizeof(render.screens_first),
                             "frame %u, %s %u level %u at (%u,%u): read %02x %02x %02x %02x, "
                             "worked out %02x %02x %02x %02x",
                             (unsigned)k, chain ? "chain" : "compute texture", (unsigned)which,
                             (unsigned)mip, (unsigned)x, (unsigned)y, got[0], got[1], got[2],
                             got[3], want[0], want[1], want[2], want[3]);
                }
            }
        }
    }
}

/// A heavy frame's small targets: sixteen chains of eleven, each target a pass of its own, drawn
/// with the one before it and then given this frame's square.
static void RecordChains(uint32_t slot) {
    for (uint32_t s = 0; s < SQUARES; ++s) {
        const uint32_t chain = s / WIMSY_CHAIN_STEPS;
        GpuSetTarget(&gpu, &chain_targets[s], WIMSY_CHAIN_SIDE, WIMSY_CHAIN_SIDE);
        if (s % WIMSY_CHAIN_STEPS == 0u) {
            uint8_t clear[4];
            WimsyChainClear(chain, clear);
            dkCmdBufClearColorFloat(gpu.cmdbuf, 0, DkColorMask_RGBA, (float)clear[0] / 255.0f,
                                    (float)clear[1] / 255.0f, (float)clear[2] / 255.0f, 1.0f);
        } else {
            // The pass before wrote the target this one samples.
            dkCmdBufBarrier(gpu.cmdbuf, DkBarrier_Full, DkInvalidateFlags_Image);
            dkCmdBufBindTexture(gpu.cmdbuf, DkStage_Fragment, 0,
                                dkMakeTextureHandle(IMAGE_CHAIN + s - 1u, 0));
            GpuBindVertices(&gpu, &common_vs, &texture_fs, chain_quad,
                            6u * (uint32_t)sizeof(GpuVertex));
            dkCmdBufDraw(gpu.cmdbuf, DkPrimitive_Triangles, 6, 1, 0, 0);
        }
        GpuBindVertices(&gpu, &common_vs, &flat_fs, square_gpu[slot],
                        SQUARES * 6u * (uint32_t)sizeof(GpuVertex));
        dkCmdBufDraw(gpu.cmdbuf, DkPrimitive_Triangles, 6, 1, s * 6u, 0);
        if (s % WIMSY_CHAIN_STEPS == WIMSY_CHAIN_STEPS - 1u) {
            SubmitPart();
        }
    }
    dkCmdBufBarrier(gpu.cmdbuf, DkBarrier_Full, DkInvalidateFlags_Image);
}

/// The world's draws, after the models, into the same target: none culled, the floor and the
/// lane through their textures, every level of detail of them, and the marks with their bias.
static void RecordWorld(uint32_t slot) {
    if (world_draw_count == 0u) {
        return;
    }
    DkRasterizerState rasterizer;
    dkRasterizerStateDefaults(&rasterizer);
    rasterizer.cullMode = DkFace_None;
    dkCmdBufBindRasterizerState(gpu.cmdbuf, &rasterizer);
    for (uint32_t d = 0; d < world_draw_count; ++d) {
        const WimsyWorldDraw *const draw = &world_draws[d];
        const bool screen = draw->material == WIMSY_MATERIAL_SCREEN;
        const bool textured = screen || draw->material == WIMSY_MATERIAL_FLOOR ||
                              draw->material == WIMSY_MATERIAL_LANE;
        if (screen) {
            // A screen shows its texture as it is: the texel under the place, the first sampler.
            const uint32_t image =
                draw->screen == WIMSY_SCREEN_COMPUTE
                    ? IMAGE_COMPUTE + draw->index * WIMSY_COMPUTE_LEVELS + draw->mip
                    : draw->screen == WIMSY_SCREEN_CHAIN
                          ? IMAGE_CHAIN + draw->index * WIMSY_CHAIN_STEPS + WIMSY_CHAIN_STEPS - 1u
                          : draw->screen == WIMSY_SCREEN_LOAD ? IMAGE_LOAD + draw->index
                                                              : IMAGE_PLAQUES;
            dkCmdBufBindTexture(gpu.cmdbuf, DkStage_Fragment, 0, dkMakeTextureHandle(image, 0));
        } else if (textured) {
            // Its descriptor, after the sprite's and the target's, with the second sampler.
            const uint32_t image = 2u + (uint32_t)(draw->material == WIMSY_MATERIAL_FLOOR
                                                       ? WimsyFloorTexture(level)
                                                       : WIMSY_TEXTURE_LANE);
            dkCmdBufBindTexture(gpu.cmdbuf, DkStage_Fragment, 0, dkMakeTextureHandle(image, 1));
        }
        GpuBindVertices(&gpu, &world_vs, textured ? &texture_fs : &flat_fs, world_vertices,
                        world_vertex_count * (uint32_t)sizeof(GpuVertex));
        dkCmdBufBindUniformBuffer(gpu.cmdbuf, DkStage_Vertex, 0, view_gpu[slot],
                                  DK_UNIFORM_BUF_ALIGNMENT);
        const bool biased = draw->material == WIMSY_MATERIAL_MARK;
        rasterizer.depthBiasEnableMask = biased ? DkPolygonFlag_Fill : 0u;
        dkCmdBufBindRasterizerState(gpu.cmdbuf, &rasterizer);
        dkCmdBufSetDepthBias(gpu.cmdbuf, biased ? WIMSY_MARK_BIAS_UNITS : 0.0f, 0.0f,
                             biased ? WIMSY_MARK_BIAS_SLOPE : 0.0f);
        dkCmdBufDraw(gpu.cmdbuf, DkPrimitive_Triangles, draw->count, 1, draw->first, 0);
    }
    rasterizer.depthBiasEnableMask = 0u;
    dkCmdBufBindRasterizerState(gpu.cmdbuf, &rasterizer);
    if (blade_count != 0u) {
        // The grass: one draw of a blade's three corners, an instance a blade.
        const DkShader *const shaders[] = { &grass_vs, &flat_fs };
        dkCmdBufBindShaders(gpu.cmdbuf, DkStageFlag_Vertex | DkStageFlag_Fragment, shaders, 2);
        static const DkVtxAttribState attributes[] = {
            { .bufferId = 0, .isFixed = 0, .offset = 0, .size = DkVtxAttribSize_2x32,
              .type = DkVtxAttribType_Float, .isBgra = 0 },
            { .bufferId = 1, .isFixed = 0, .offset = offsetof(WimsyBlade, x),
              .size = DkVtxAttribSize_4x32, .type = DkVtxAttribType_Float, .isBgra = 0 },
            { .bufferId = 1, .isFixed = 0, .offset = offsetof(WimsyBlade, colour),
              .size = DkVtxAttribSize_4x8, .type = DkVtxAttribType_Unorm, .isBgra = 0 },
        };
        static const DkVtxBufferState buffers[] = { { 2u * sizeof(float), 0 },
                                                    { sizeof(WimsyBlade), 1 } };
        dkCmdBufBindVtxAttribState(gpu.cmdbuf, attributes,
                                   sizeof(attributes) / sizeof(attributes[0]));
        dkCmdBufBindVtxBufferState(gpu.cmdbuf, buffers, 2);
        dkCmdBufBindVtxBuffer(gpu.cmdbuf, 0, blade_unit, 6u * (uint32_t)sizeof(float));
        dkCmdBufBindVtxBuffer(gpu.cmdbuf, 1, blade_instances,
                              blade_count * (uint32_t)sizeof(WimsyBlade));
        dkCmdBufBindUniformBuffer(gpu.cmdbuf, DkStage_Vertex, 0, view_gpu[slot],
                                  DK_UNIFORM_BUF_ALIGNMENT);
        dkCmdBufDraw(gpu.cmdbuf, DkPrimitive_Triangles, 3, blade_count, 0, 0);
    }
    if (puff_count != 0u) {
        // The smoke, last: blended over the frame in the order the buffer has it, behind what
        // is nearer and leaving the depth buffer as it was. One draw, an instance a puff.
        DkDepthStencilState depth;
        dkDepthStencilStateDefaults(&depth);
        depth.depthTestEnable = 1;
        depth.depthWriteEnable = 0;
        depth.depthCompareOp = DkCompareOp_Less;
        depth.stencilTestEnable = 0;
        dkCmdBufBindDepthStencilState(gpu.cmdbuf, &depth);
        BindBlend(true);
        const DkShader *const shaders[] = { &smoke_vs, &smoke_fs };
        dkCmdBufBindShaders(gpu.cmdbuf, DkStageFlag_Vertex | DkStageFlag_Fragment, shaders, 2);
        static const DkVtxAttribState attributes[] = {
            { .bufferId = 0, .isFixed = 0, .offset = 0, .size = DkVtxAttribSize_2x32,
              .type = DkVtxAttribType_Float, .isBgra = 0 },
            { .bufferId = 1, .isFixed = 0, .offset = offsetof(WimsyPuff, x),
              .size = DkVtxAttribSize_4x32, .type = DkVtxAttribType_Float, .isBgra = 0 },
            { .bufferId = 1, .isFixed = 0, .offset = offsetof(WimsyPuff, colour),
              .size = DkVtxAttribSize_4x8, .type = DkVtxAttribType_Unorm, .isBgra = 0 },
        };
        static const DkVtxBufferState buffers[] = { { 2u * sizeof(float), 0 },
                                                    { sizeof(WimsyPuff), 1 } };
        dkCmdBufBindVtxAttribState(gpu.cmdbuf, attributes,
                                   sizeof(attributes) / sizeof(attributes[0]));
        dkCmdBufBindVtxBufferState(gpu.cmdbuf, buffers, 2);
        dkCmdBufBindVtxBuffer(gpu.cmdbuf, 0, puff_unit, 12u * (uint32_t)sizeof(float));
        dkCmdBufBindVtxBuffer(gpu.cmdbuf, 1, puff_gpu[slot],
                              puff_count * (uint32_t)sizeof(WimsyPuff));
        dkCmdBufBindUniformBuffer(gpu.cmdbuf, DkStage_Vertex, 0, view_gpu[slot],
                                  DK_UNIFORM_BUF_ALIGNMENT);
        dkCmdBufDraw(gpu.cmdbuf, DkPrimitive_Triangles, 6, puff_count, 0, 0);
        BindBlend(false);
        depth.depthWriteEnable = 1;
        dkCmdBufBindDepthStencilState(gpu.cmdbuf, &depth);
    }
}

/// A higher level's frame: the models into a target of four samples behind a depth buffer, the
/// faces turned away culled, then the samples resolved into the target the frame is judged in.
static void RecordModels(uint32_t slot) {
    DkImageView colour, depth;
    dkImageViewDefaults(&colour, &colour_samples);
    dkImageViewDefaults(&depth, &depth_samples);
    dkCmdBufBindRenderTarget(gpu.cmdbuf, &colour, &depth);
    DkMultisampleState samples;
    dkMultisampleStateDefaults(&samples);
    samples.mode = DkMsMode_4x;
    samples.rasterizerMode = DkMsMode_4x;
    memcpy(samples.sampleLocations, COVERAGE_STANDARD_4X, sizeof(samples.sampleLocations));
    dkCmdBufBindMultisampleState(gpu.cmdbuf, &samples);
    ClearColour();
    DkDepthStencilState state;
    dkDepthStencilStateDefaults(&state);
    state.depthTestEnable = 0;
    state.depthWriteEnable = 1;
    state.stencilTestEnable = 0;
    dkCmdBufBindDepthStencilState(gpu.cmdbuf, &state);
    dkCmdBufClearDepthStencil(gpu.cmdbuf, true, 1.0f, 0, 0);
    state.depthTestEnable = 1;
    state.depthCompareOp = DkCompareOp_Less;
    dkCmdBufBindDepthStencilState(gpu.cmdbuf, &state);
    DkRasterizerState rasterizer;
    dkRasterizerStateDefaults(&rasterizer);
    rasterizer.cullMode = DkFace_Back;
    rasterizer.frontFace = DkFrontFace_CCW;
    dkCmdBufBindRasterizerState(gpu.cmdbuf, &rasterizer);

    const DkShader *const shaders[] = { &model_vs, &lit_fs };
    dkCmdBufBindShaders(gpu.cmdbuf, DkStageFlag_Vertex | DkStageFlag_Fragment, shaders, 2);
    static const DkVtxAttribState attributes[] = {
        { .bufferId = 0, .isFixed = 0, .offset = offsetof(ModelVertex, position),
          .size = DkVtxAttribSize_3x32, .type = DkVtxAttribType_Float, .isBgra = 0 },
        { .bufferId = 0, .isFixed = 0, .offset = offsetof(ModelVertex, normal),
          .size = DkVtxAttribSize_3x32, .type = DkVtxAttribType_Float, .isBgra = 0 },
        { .bufferId = 0, .isFixed = 0, .offset = offsetof(ModelVertex, colour),
          .size = DkVtxAttribSize_4x8, .type = DkVtxAttribType_Unorm, .isBgra = 0 },
    };
    static const DkVtxBufferState buffer = { sizeof(ModelVertex), 0 };
    dkCmdBufBindVtxAttribState(gpu.cmdbuf, attributes, sizeof(attributes) / sizeof(attributes[0]));
    dkCmdBufBindVtxBufferState(gpu.cmdbuf, &buffer, 1);
    dkCmdBufBindIdxBuffer(gpu.cmdbuf, DkIdxFormat_Uint16, model_indices);
    dkCmdBufBindUniformBuffer(gpu.cmdbuf, DkStage_Fragment, 0, light_block,
                              DK_UNIFORM_BUF_ALIGNMENT);
    for (uint32_t i = 0; i < instance_count[slot]; ++i) {
        const DkGpuAddr block = instance_gpu[slot] + (DkGpuAddr)i * INSTANCE_STRIDE;
        // Her vertices in the colours of the drawing this model is.
        dkCmdBufBindVtxBuffer(gpu.cmdbuf, 0, model_vertices[WimsyInstanceVariant(level, i)],
                              WIMSY_MESH_VERTICES * (uint32_t)sizeof(ModelVertex));
        if (level < 3u) {
            dkCmdBufBindUniformBuffer(gpu.cmdbuf, DkStage_Vertex, 0, block, INSTANCE_STRIDE);
            dkCmdBufDrawIndexed(gpu.cmdbuf, DkPrimitive_Triangles, WIMSY_MESH_INDICES, 1, 0, 0, 0);
            continue;
        }
        // A heavy frame draws her as a game draws a character of several materials: a draw a
        // part, each with its material's fragment program and its uniform buffer bound again.
        for (uint32_t part = 0; part < WIMSY_MESH_PARTS; ++part) {
            const uint32_t material = part % MATERIALS;
            const DkShader *const programs[] = {
                &model_vs, material == 0u ? &lit_fs : &material_fs[material - 1u]
            };
            dkCmdBufBindShaders(gpu.cmdbuf, DkStageFlag_Vertex | DkStageFlag_Fragment, programs, 2);
            dkCmdBufBindUniformBuffer(gpu.cmdbuf, DkStage_Vertex, 0, block, INSTANCE_STRIDE);
            dkCmdBufDrawIndexed(gpu.cmdbuf, DkPrimitive_Triangles,
                                WIMSY_MESH_PART[part].index_count, 1,
                                WIMSY_MESH_PART[part].first_index, 0, 0);
        }
        if (i % 20u == 19u) {
            SubmitPart();
        }
    }
    RecordWorld(slot);

    // The resolve runs on the 2D engine, which the 3D pipeline's own barriers do not order.
    dkCmdBufBarrier(gpu.cmdbuf, DkBarrier_Full, 0);
    DkImageView resolved;
    dkImageViewDefaults(&resolved, &gpu.image);
    dkCmdBufResolveImage(gpu.cmdbuf, &colour, &resolved);
    dkMultisampleStateDefaults(&samples);
    dkCmdBufBindMultisampleState(gpu.cmdbuf, &samples);
    rasterizer.cullMode = DkFace_None;
    dkCmdBufBindRasterizerState(gpu.cmdbuf, &rasterizer);
    state.depthTestEnable = 0;
    state.depthWriteEnable = 0;
    dkCmdBufBindDepthStencilState(gpu.cmdbuf, &state);
}

/// Which judged frame @p k is, or WIMSY_JUDGED when it is none.
static uint32_t JudgedAt(uint32_t k) {
    for (uint32_t j = 0; j < judged_count; ++j) {
        if (judged[j] == k) {
            return j;
        }
    }
    return WIMSY_JUDGED;
}

/// Records, submits and presents frame @p k from @p slot, and keeps its image when it is one the
/// judge wants. Hands back the render thread's part of the frame's work: from the image in hand
/// to the queue standing idle after the frame's submit, so the graphics unit's work is in it and
/// the wait for an image is not; the present comes after it, and a judged frame's copy after
/// that.
static uint64_t DrawFrame(uint32_t k, uint32_t slot) {
    frame_judged = level >= 3u && JudgedAt(k) < WIMSY_JUDGED;
    const int image = on_screen ? 0 : dkQueueAcquireImage(gpu.queue, swapchain);
    const uint64_t began = armGetSystemTick();
    GpuBeginPass(&gpu);
    // The processor rewrote this slot after the graphics unit last read it, through memory the
    // unit caches in its level two cache: what the previous frame left there is dropped. The
    // previous frame's wait for idle wrote everything it drew back first.
    dkCmdBufBarrier(gpu.cmdbuf, DkBarrier_None, DkInvalidateFlags_L2Cache);
    dkCmdBufBindImageDescriptorSet(gpu.cmdbuf, image_set[slot], IMAGE_DESCRIPTORS);
    dkCmdBufBindSamplerDescriptorSet(gpu.cmdbuf, sampler_set, 2);
    if (level == 0u) {
        RecordSprites(slot);
    } else {
        if (level >= 3u) {
            RecordFills(slot);
            RecordChains(slot);
            if (frame_judged) {
                RecordChainsBack(slot);
            }
            // Back to the whole target's viewport, which the last chain's pass left at its own.
            GpuSetTarget(&gpu, NULL, WIMSY_WIDTH, WIMSY_HEIGHT);
        }
        RecordModels(slot);
    }

    if (on_screen) {
        // Through the program's screen: the target read back and composed by the processor,
        // the judged frames kept from the same copy.
        GpuSubmit(&gpu);
        const uint64_t worked = armGetSystemTick() - began;
        const uint8_t *const pixels = GpuReadBack(&gpu);
        if (pixels == NULL) {
            return worked;
        }
        const uint32_t j = JudgedAt(k);
        if (j < WIMSY_JUDGED && kept[j] != NULL) {
            memcpy(kept[j], pixels, WIMSY_IMAGE_BYTES);
            ++render.copied;
        }
        last_shown = pixels;
        if (ViewerShowImage(pixels, WIMSY_WIDTH, WIMSY_HEIGHT)) {
            ++render.presented;
        }
        return worked;
    }

    // The target drawn onto the image the display shows, a pixel a pixel.
    dkCmdBufBarrier(gpu.cmdbuf, DkBarrier_Full, DkInvalidateFlags_Image);
    GpuSetTarget(&gpu, &swap_images[image], WIMSY_WIDTH, WIMSY_HEIGHT);
    dkCmdBufBindTexture(gpu.cmdbuf, DkStage_Fragment, 0, dkMakeTextureHandle(1, 0));
    GpuBindVertices(&gpu, &common_vs, &texture_fs, shown_quad, 6u * (uint32_t)sizeof(GpuVertex));
    dkCmdBufDraw(gpu.cmdbuf, DkPrimitive_Triangles, 6, 1, 0, 0);
    GpuSubmit(&gpu);
    const uint64_t worked = armGetSystemTick() - began;
    // Presented whatever the submit came to: an image taken and never queued back would leave
    // the swapchain one short for the destroy that follows a fault.
    dkQueuePresentImage(gpu.queue, swapchain, image);
    if (!gpu.ready) {
        return worked;
    }
    ++render.presented;
    if (level >= 3u) {
        // The queue stood idle: the texel the list copied out is in memory the processor reads.
        uint8_t want[4];
        WimsyComputeTexel(WIMSY_PEEK_TEXTURE, WIMSY_PEEK_LEVEL, k, 0u, 0u, want);
        ++render.peeked;
        if (memcmp(peek_cpu[slot], want, 4u) != 0 && render.peek_wrong++ == 0u) {
            render.peek_wrong_at = k;
            memcpy(render.peek_got, peek_cpu[slot], 4u);
        }
        if (frame_judged) {
            JudgeScreens(k, slot);
        }
    }

    const uint32_t j = JudgedAt(k);
    if (j < WIMSY_JUDGED && kept[j] != NULL) {
        const uint64_t copy_began = armGetSystemTick();
        const uint8_t *const pixels = GpuReadBack(&gpu);
        if (pixels != NULL) {
            memcpy(kept[j], pixels, WIMSY_IMAGE_BYTES);
            ++render.copied;
        }
        const uint32_t took = Microseconds(armGetSystemTick() - copy_began);
        render.copy_longest = took > render.copy_longest ? took : render.copy_longest;
    }
    return worked;
}

/// The last frame shown again while the demo is paused (demo.h): the screen's copy of it, which
/// still holds it. No pass runs and nothing is judged.
static void ShowAgain(void) {
    if (on_screen && last_shown != NULL) {
        (void)ViewerShowImage(last_shown, WIMSY_WIDTH, WIMSY_HEIGHT);
    } else {
        svcSleepThread(16666667ull);
    }
}

/// Held while the person keeps the demo paused, the last frame shown again; hands back the ticks
/// it held, which no frame's time counts.
static uint64_t HoldWhilePaused(void) {
    if (demo == NULL || demo->paused == NULL || !demo->paused()) {
        return 0u;
    }
    const uint64_t from = armGetSystemTick();
    while (demo->paused() && (demo->stopped == NULL || !demo->stopped())) {
        ShowAgain();
    }
    return armGetSystemTick() - from;
}

static void RenderMain(void *argument) {
    (void)argument;
    uint64_t previous = 0;
    for (uint32_t k = 0; k < frames; ++k) {
        semaphoreWait(&frame_go);
        if (k >= __atomic_load_n(&frames_wanted, __ATOMIC_ACQUIRE)) {
            break;
        }
        if (k == 0u) {
            previous = armGetSystemTick();
        }
        const uint64_t worked = gpu.ready ? DrawFrame(k, k % SLOTS) : 0u;
        // After the present was queued: the wait for the display is in the next frame's acquire.
        const uint64_t now = armGetSystemTick();
        frame_us[k] = Microseconds(now - previous);
        render_us[k] = Microseconds(worked);
        previous = now + HoldWhilePaused();
        semaphoreSignal(&slot_free);
    }
}

static void MakeSwapchain(void) {
    swapchain = dkSwapchainCreate(&swap_maker);
}

/// Carves @p size bytes of scratch or says which part did not fit.
static void *Scratch(uint32_t size, uint32_t alignment, DkGpuAddr *address, const char *what,
                     char *why, size_t why_size) {
    const uint32_t used = gpu.data_used;
    void *const bytes = GpuAllocData(&gpu, size, alignment, address);
    if (bytes == NULL && why[0] == '\0') {
        snprintf(why, why_size, "no scratch left for %s: %u bytes asked, %u of the %u used",
                 what, (unsigned)size, (unsigned)used, (unsigned)gpu.spec.data_size);
    }
    return bytes;
}

#define STAGING_BYTES (16u * 1024u * 1024u)

/// Whether this level draws with @p texture: the lane's and its own floor's, from level 2.
static bool LevelHas(WimsyTexture texture) {
    return level >= 2u && (texture == WIMSY_TEXTURE_LANE || texture == WimsyFloorTexture(level));
}

/// Fills every level of @p image with @p texture's texels, or its blocks, a level a copy from
/// staging the processor fills, the one block of it for them all.
static bool UploadLevels(const DkImage *image, WimsyTexture texture, uint8_t *staging,
                         DkGpuAddr staging_gpu, char *why, size_t why_size) {
    const uint32_t block = WimsyTextureBlock(texture);
    for (uint32_t at = 0; at < WimsyTextureLevels(texture); ++at) {
        const uint32_t side = WimsyTextureSide(texture) >> at;
        // A row of the staging is the level's texels, four bytes each, or its blocks, sixteen;
        // the copy engine wants its pitch aligned, which a level under eight texels' is not.
        const uint32_t across = block != 0u ? (side + block - 1u) / block : side;
        const uint32_t unit = block != 0u ? 16u : 4u;
        const uint32_t pitch = (across * unit + DK_IMAGE_LINEAR_STRIDE_ALIGNMENT - 1u) &
                               ~(DK_IMAGE_LINEAR_STRIDE_ALIGNMENT - 1u);
        if ((uint64_t)pitch * across > STAGING_BYTES) {
            snprintf(why, why_size, "level %u of a texture is %llu bytes, over the staging's %u",
                     (unsigned)at, (unsigned long long)pitch * across, (unsigned)STAGING_BYTES);
            return false;
        }
        for (uint32_t y = 0; y < across; ++y) {
            for (uint32_t x = 0; x < across; ++x) {
                uint8_t *const to = staging + (size_t)y * pitch + (size_t)x * unit;
                if (block != 0u) {
                    WimsyTextureBlockAt(texture, at, x, y, to);
                } else {
                    WimsyTexel(texture, at, x, y, to);
                }
            }
        }
        GpuBeginList(&gpu);
        // The staging was rewritten since the last level's copy read it.
        dkCmdBufBarrier(gpu.cmdbuf, DkBarrier_Full, DkInvalidateFlags_L2Cache);
        DkImageView view;
        dkImageViewDefaults(&view, image);
        view.mipLevelOffset = (uint8_t)at;
        const DkImageRect rect = { 0, 0, 0, side, side, 1 };
        const DkCopyBuf source = { staging_gpu, pitch, 0 };
        dkCmdBufCopyBufferToImage(gpu.cmdbuf, &source, &view, &rect, 0);
        GpuSubmit(&gpu);
        if (!gpu.ready) {
            snprintf(why, why_size, "the upload of level %u of a texture: %s", (unsigned)at,
                     gpu.failure);
            return false;
        }
    }
    return true;
}

/// The device, the shaders, the images and everything a frame of this level reads that no frame
/// changes. False with the reason.
static bool BringUp(char *why, size_t why_size) {
    GpuSpec spec;
    GpuSpecDefaults(&spec, WIMSY_WIDTH, WIMSY_HEIGHT);
    spec.code_size = 64u * 1024u;
    // The scratch every frame reads, and at level 2 the staging its textures' levels go through,
    // as large as the floor's level nought.
    // A heavy level's grass too, a field's 262,144 blades of 20 bytes.
    spec.data_size = (level >= 3u ? 32u : level == 2u ? 24u : 4u) * 1024u * 1024u;
    // The presented images, the sprite, the two targets of four samples a higher level has, and
    // level 2's two textures with their levels.
    spec.image_size = (level >= 3u ? 160u : level == 2u ? 96u : 64u) * 1024u * 1024u;
    // A heavy frame's fifteen hundred draws, its dispatches and its passes, a few words each.
    spec.command_size = (level >= 3u ? 8192u : 512u) * 1024u;
    spec.compute = level >= 3u;
    // A heavy level's loads, each an image of its own the processor writes.
    load_ring = frames * WIMSY_LOADS < LOAD_RING ? frames * WIMSY_LOADS : LOAD_RING;
    spec.cpu_image_size = level >= 3u ? load_ring * LOAD_BYTES + 1024u * 1024u : 0u;
    if (!GpuOpenSpec(&gpu, &spec)) {
        snprintf(why, why_size, "the graphics device: %s", gpu.failure);
        return false;
    }
    const bool loaded =
        GpuLoadShader(&gpu, &common_vs, ShaderData(SHADER_COMMON_VERT),
                      ShaderSize(SHADER_COMMON_VERT)) &&
        GpuLoadShader(&gpu, &texture_fs, ShaderData(SHADER_TEXTURE_FRAG),
                      ShaderSize(SHADER_TEXTURE_FRAG)) &&
        GpuLoadShader(&gpu, &model_vs, ShaderData(SHADER_WIMSY_VERT),
                      ShaderSize(SHADER_WIMSY_VERT)) &&
        GpuLoadShader(&gpu, &lit_fs, ShaderData(SHADER_WIMSY_LIT_FRAG),
                      ShaderSize(SHADER_WIMSY_LIT_FRAG)) &&
        GpuLoadShader(&gpu, &material_fs[0], ShaderData(SHADER_WIMSY_LIT_1_FRAG),
                      ShaderSize(SHADER_WIMSY_LIT_1_FRAG)) &&
        GpuLoadShader(&gpu, &material_fs[1], ShaderData(SHADER_WIMSY_LIT_2_FRAG),
                      ShaderSize(SHADER_WIMSY_LIT_2_FRAG)) &&
        GpuLoadShader(&gpu, &material_fs[2], ShaderData(SHADER_WIMSY_LIT_3_FRAG),
                      ShaderSize(SHADER_WIMSY_LIT_3_FRAG)) &&
        GpuLoadShader(&gpu, &world_vs, ShaderData(SHADER_WIMSY_WORLD_VERT),
                      ShaderSize(SHADER_WIMSY_WORLD_VERT)) &&
        GpuLoadShader(&gpu, &grass_vs, ShaderData(SHADER_WIMSY_GRASS_VERT),
                      ShaderSize(SHADER_WIMSY_GRASS_VERT)) &&
        GpuLoadShader(&gpu, &smoke_vs, ShaderData(SHADER_WIMSY_SMOKE_VERT),
                      ShaderSize(SHADER_WIMSY_SMOKE_VERT)) &&
        GpuLoadShader(&gpu, &smoke_fs, ShaderData(SHADER_WIMSY_SMOKE_FRAG),
                      ShaderSize(SHADER_WIMSY_SMOKE_FRAG)) &&
        GpuLoadShader(&gpu, &flat_fs, ShaderData(SHADER_FLAT_FRAG), ShaderSize(SHADER_FLAT_FRAG)) &&
        GpuLoadShader(&gpu, &fill_cs, ShaderData(SHADER_WIMSY_FILL_COMP),
                      ShaderSize(SHADER_WIMSY_FILL_COMP));
    if (!loaded) {
        snprintf(why, why_size, "the shaders: %s", gpu.failure);
        return false;
    }
    bool images = GpuCreateTexture(&gpu, &sprite_texture, &sprite_layout, DkImageFormat_RGBA8_Unorm,
                                   WIMSY_SPRITE, WIMSY_SPRITE);
    if (images && level >= 3u) {
        images = GpuCreateTexture(&gpu, &plaque_texture, &plaque_layout, DkImageFormat_RGBA8_Unorm,
                                  WIMSY_PLAQUES_SIDE, WIMSY_PLAQUES_SIDE);
    }
    if (images && level != 0u) {
        images = GpuCreateRenderImage(&gpu, &colour_samples, &colour_samples_layout,
                                      DkImageFormat_RGBA8_Unorm, WIMSY_WIDTH, WIMSY_HEIGHT,
                                      DkMsMode_4x) &&
                 GpuCreateRenderImage(&gpu, &depth_samples, &depth_samples_layout,
                                      DkImageFormat_Z24S8, WIMSY_WIDTH, WIMSY_HEIGHT, DkMsMode_4x);
    }
    for (uint32_t t = 0; t < WIMSY_TEXTURES && images; ++t) {
        if (LevelHas((WimsyTexture)t)) {
            images = GpuCreateMipTexture(&gpu, &level_textures[t], &level_texture_layouts[t],
                                         WimsyTextureBlock((WimsyTexture)t) != 0u
                                             ? DkImageFormat_RGBA_ASTC_8x8
                                             : DkImageFormat_RGBA8_Unorm,
                                         WimsyTextureSide((WimsyTexture)t),
                                         WimsyTextureSide((WimsyTexture)t),
                                         WimsyTextureLevels((WimsyTexture)t));
        }
    }
    for (uint32_t t = 0; t < WIMSY_COMPUTE_TEXTURES && images && level >= 3u; ++t) {
        // Stored into by a compute program and sampled: every level, in the ordinary image block.
        DkImageLayoutMaker maker;
        dkImageLayoutMakerDefaults(&maker, gpu.device);
        maker.type = DkImageType_2D;
        maker.flags = DkImageFlags_UsageLoadStore;
        maker.format = DkImageFormat_RGBA8_Unorm;
        maker.dimensions[0] = WIMSY_COMPUTE_SIDE;
        maker.dimensions[1] = WIMSY_COMPUTE_SIDE;
        maker.mipLevels = WIMSY_COMPUTE_LEVELS;
        DkImageLayout layout;
        images = GpuCreateFromMaker(&gpu, &compute_textures[t], &layout, &maker, NULL);
    }
    for (uint32_t s = 0; s < SQUARES && images && level >= 3u; ++s) {
        DkImageLayout layout;
        images = GpuCreateRenderImage(&gpu, &chain_targets[s], &layout, DkImageFormat_RGBA8_Unorm,
                                      WIMSY_CHAIN_SIDE, WIMSY_CHAIN_SIDE, DkMsMode_1x);
    }
    for (uint32_t l = 0; l < load_ring && images && level >= 3u; ++l) {
        DkImageLayout layout;
        images = GpuCreateImage(&gpu, &load_images[l], &layout, DkImageFormat_RGBA_ASTC_8x8,
                                WIMSY_LOAD_SIDE, WIMSY_LOAD_SIDE, DkImageFlags_CustomTileSize,
                                LOAD_TILE, &load_memory[l]);
        if (images && dkImageLayoutGetSize(&layout) != LOAD_BYTES) {
            snprintf(why, why_size,
                     "the library laid a load out in %llu bytes, the block linear rule says %u",
                     (unsigned long long)dkImageLayoutGetSize(&layout), (unsigned)LOAD_BYTES);
            return false;
        }
    }
    for (uint32_t i = 0; i < SWAP_IMAGES && images; ++i) {
        images = GpuCreateImage(&gpu, &swap_images[i], &swap_layouts[i],
                                DkImageFormat_RGBA8_Unorm, WIMSY_WIDTH, WIMSY_HEIGHT,
                                DkImageFlags_UsageRender | DkImageFlags_UsagePresent |
                                    DkImageFlags_HwCompression,
                                0, NULL);
    }
    if (!images) {
        snprintf(why, why_size, "the texture, the targets and the presented images: %s",
                 gpu.failure);
        return false;
    }

    why[0] = '\0';
    for (uint32_t s = 0; s < SLOTS; ++s) {
        batch_cpu[s] = (GpuVertex *)Scratch(BATCH_VERTICES * (uint32_t)sizeof(GpuVertex), 256u,
                                            &batch_gpu[s], "the sprites", why, why_size);
        instance_cpu[s] = (uint8_t *)Scratch(WIMSY_INSTANCES_MAX * INSTANCE_STRIDE,
                                             DK_UNIFORM_BUF_ALIGNMENT, &instance_gpu[s],
                                             "the uniform buffers", why, why_size);
        instance_count[s] = 0;
        view_cpu[s] = (float *)Scratch(DK_UNIFORM_BUF_ALIGNMENT, DK_UNIFORM_BUF_ALIGNMENT,
                                       &view_gpu[s], "the view", why, why_size);
        fill_cpu[s] = (uint8_t *)Scratch(FILLS * DK_UNIFORM_BUF_ALIGNMENT, DK_UNIFORM_BUF_ALIGNMENT,
                                         &fill_gpu[s], "the dispatches' uniform buffers", why,
                                         why_size);
        square_cpu[s] = (GpuVertex *)Scratch(SQUARES * 6u * (uint32_t)sizeof(GpuVertex), 256u,
                                             &square_gpu[s], "the chains' squares", why, why_size);
        puff_cpu[s] = (WimsyPuff *)Scratch(WIMSY_PUFFS_MAX * (uint32_t)sizeof(WimsyPuff), 256u,
                                           &puff_gpu[s], "the smoke", why, why_size);
        if (fill_cpu[s] != NULL) {
            memset(fill_cpu[s], 0, FILLS * DK_UNIFORM_BUF_ALIGNMENT);
        }
        image_descriptors[s] = (DkImageDescriptor *)Scratch(
            IMAGE_DESCRIPTORS * (uint32_t)sizeof(DkImageDescriptor), DK_IMAGE_DESCRIPTOR_ALIGNMENT,
            &image_set[s], "the image descriptors", why, why_size);
        peek_cpu[s] = (uint8_t *)Scratch(DK_UNIFORM_BUF_ALIGNMENT, DK_UNIFORM_BUF_ALIGNMENT,
                                         &peek_gpu[s], "the texel read back", why, why_size);
        screens_cpu[s] = (uint8_t *)Scratch(SCREENS_BYTES, 256u, &screens_gpu[s],
                                            "the textures read back", why, why_size);
        if (peek_cpu[s] != NULL) {
            memset(peek_cpu[s], 0, DK_UNIFORM_BUF_ALIGNMENT);
        }
    }
    GpuVertex *const chain_vertices = (GpuVertex *)Scratch(
        6u * (uint32_t)sizeof(GpuVertex), 256u, &chain_quad, "a chain's quad", why, why_size);
    GpuVertex *const world = (GpuVertex *)Scratch(
        WIMSY_WORLD_VERTICES_MAX * (uint32_t)sizeof(GpuVertex), 256u, &world_vertices,
        "the world's vertices", why, why_size);
    GpuVertex *const shown = (GpuVertex *)Scratch(6u * (uint32_t)sizeof(GpuVertex), 256u,
                                                  &shown_quad, "the quad shown", why, why_size);
    float *const unit = (float *)Scratch(6u * (uint32_t)sizeof(float), 256u, &blade_unit,
                                         "a blade's corners", why, why_size);
    float *const square = (float *)Scratch(12u * (uint32_t)sizeof(float), 256u, &puff_unit,
                                           "a puff's corners", why, why_size);
    const uint32_t grass = WimsyBladeCount(WimsyGrassOf(level));
    WimsyBlade *const blades = (WimsyBlade *)Scratch(
        (grass != 0u ? grass : 1u) * (uint32_t)sizeof(WimsyBlade), 256u, &blade_instances,
        "the grass", why, why_size);
    ModelVertex *vertices[WIMSY_MESH_VARIANTS];
    for (uint32_t variant = 0; variant < WIMSY_MESH_VARIANTS; ++variant) {
        vertices[variant] = (ModelVertex *)Scratch(
            WIMSY_MESH_VERTICES * (uint32_t)sizeof(ModelVertex), 256u, &model_vertices[variant],
            "the model's vertices", why, why_size);
    }
    uint16_t *const indices = (uint16_t *)Scratch(
        WIMSY_MESH_INDICES * (uint32_t)sizeof(uint16_t), 256u, &model_indices,
        "the model's indices", why, why_size);
    WimsyLight *const light = (WimsyLight *)Scratch(DK_UNIFORM_BUF_ALIGNMENT,
                                                    DK_UNIFORM_BUF_ALIGNMENT, &light_block,
                                                    "the light", why, why_size);
    // The descriptors no frame changes, made once and copied into each slot's set.
    static DkImageDescriptor described[IMAGE_DESCRIPTORS];
    DkSamplerDescriptor *const sampler_descriptor = (DkSamplerDescriptor *)Scratch(
        2u * (uint32_t)sizeof(DkSamplerDescriptor), DK_SAMPLER_DESCRIPTOR_ALIGNMENT, &sampler_set,
        "the sampler descriptors", why, why_size);
    if (why[0] != '\0') {
        return false;
    }
    world_draw_count = WimsyWorldOf(level, (WimsyWorldVertex *)world, &world_vertex_count,
                                    world_draws);
    // The corners WimsyBladeCorner places: the two feet and the tip.
    static const float UNIT[6] = { -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f };
    memcpy(unit, UNIT, sizeof(UNIT));
    // A puff's square, two triangles, from -1 to 1 either way.
    static const float SQUARE[12] = { -1.0f, -1.0f, 1.0f, -1.0f, 1.0f,  1.0f,
                                      -1.0f, -1.0f, 1.0f, 1.0f,  -1.0f, 1.0f };
    memcpy(square, SQUARE, sizeof(SQUARE));
    puff_count = WimsyPuffCount(WimsySmokeOf(level));
    blade_count = WimsyBladesOf(0u, grass, blades);
    static const uint8_t WHITE[4] = { 255, 255, 255, 255 };
    PixelSquare(chain_vertices, 0u, 0u, WIMSY_CHAIN_SIDE, WIMSY_CHAIN_SIDE, WIMSY_CHAIN_SIDE, WHITE,
                1.0f);

    Rectangle(shown, -1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 1.0f);
    for (uint32_t variant = 0; variant < WIMSY_MESH_VARIANTS; ++variant) {
        for (uint32_t v = 0; v < WIMSY_MESH_VERTICES; ++v) {
            const WimsyMeshVertex *const from = &WIMSY_MESH_VERTEX[v];
            ModelVertex *const to = &vertices[variant][v];
            // The floats the reference reads the mesh as (wimsyref.c, HoldInstance).
            to->position[0] = (float)from->x / (float)WIMSY_MESH_SCALE;
            to->position[1] = (float)from->y / (float)WIMSY_MESH_SCALE;
            to->position[2] = (float)from->z / (float)WIMSY_MESH_SCALE;
            to->normal[0] = (float)from->nx / 127.0f;
            to->normal[1] = (float)from->ny / 127.0f;
            to->normal[2] = (float)from->nz / 127.0f;
            memcpy(to->colour, WIMSY_MESH_PALETTE[variant][from->colour], 3u);
            to->colour[3] = 255u;
        }
    }
    memcpy(indices, WIMSY_MESH_INDEX, WIMSY_MESH_INDICES * sizeof(uint16_t));
    memset(light, 0, DK_UNIFORM_BUF_ALIGNMENT);
    WimsyLightOf(level, light);

    DkImageView view;
    dkImageViewDefaults(&view, &sprite_texture);
    dkImageDescriptorInitialize(&described[0], &view, false, false);
    dkImageViewDefaults(&view, &gpu.image);
    dkImageDescriptorInitialize(&described[1], &view, false, false);
    DkSampler sampler;
    dkSamplerDefaults(&sampler);
    sampler.minFilter = DkFilter_Nearest;
    sampler.magFilter = DkFilter_Nearest;
    sampler.mipFilter = DkMipFilter_None;
    sampler.wrapMode[0] = DkWrapMode_ClampToEdge;
    sampler.wrapMode[1] = DkWrapMode_ClampToEdge;
    sampler.wrapMode[2] = DkWrapMode_ClampToEdge;
    dkSamplerDescriptorInitialize(&sampler_descriptor[0], &sampler);
    // The world's: every level of detail, a level between two weighed, around the edges.
    dkSamplerDefaults(&sampler);
    sampler.minFilter = DkFilter_Linear;
    sampler.magFilter = DkFilter_Linear;
    sampler.mipFilter = DkMipFilter_Linear;
    sampler.wrapMode[0] = DkWrapMode_Repeat;
    sampler.wrapMode[1] = DkWrapMode_Repeat;
    sampler.wrapMode[2] = DkWrapMode_Repeat;
    sampler.maxAnisotropy = 1.0f;
    dkSamplerDescriptorInitialize(&sampler_descriptor[1], &sampler);
    for (uint32_t t = 0; t < WIMSY_TEXTURES; ++t) {
        // A texture the level has not gets the sprite in its place, which nothing reads.
        dkImageViewDefaults(&view, LevelHas((WimsyTexture)t) ? &level_textures[t] : &sprite_texture);
        dkImageDescriptorInitialize(&described[IMAGE_TEXTURES + t], &view, false, false);
    }
    for (uint32_t f = 0; f < FILLS; ++f) {
        // A level of a compute texture twice: to sample, and to store into. Under level 3 the
        // sprite stands in, which nothing reads.
        dkImageViewDefaults(&view, level >= 3u ? &compute_textures[f / WIMSY_COMPUTE_LEVELS]
                                               : &sprite_texture);
        if (level >= 3u) {
            view.mipLevelOffset = (uint8_t)(f % WIMSY_COMPUTE_LEVELS);
            view.mipLevelCount = 1;
        }
        dkImageDescriptorInitialize(&described[IMAGE_COMPUTE + f], &view, false, false);
        dkImageDescriptorInitialize(&described[IMAGE_STORE + f], &view, level >= 3u, false);
    }
    for (uint32_t s = 0; s < SQUARES; ++s) {
        dkImageViewDefaults(&view, level >= 3u ? &chain_targets[s] : &sprite_texture);
        dkImageDescriptorInitialize(&described[IMAGE_CHAIN + s], &view, false, false);
    }
    for (uint32_t l = 0; l < WIMSY_LOADS; ++l) {
        // A frame's loads are written into its slot's set each frame (WriteSlot).
        dkImageViewDefaults(&view, &sprite_texture);
        dkImageDescriptorInitialize(&described[IMAGE_LOAD + l], &view, false, false);
    }
    dkImageViewDefaults(&view, level >= 3u ? &plaque_texture : &sprite_texture);
    dkImageDescriptorInitialize(&described[IMAGE_PLAQUES], &view, false, false);
    for (uint32_t s = 0; s < SLOTS; ++s) {
        memcpy(image_descriptors[s], described, sizeof(described));
    }

    static uint8_t texels[WIMSY_SPRITE_BYTES];
    WimsySpriteTexels(texels);
    if (!GpuUploadTexture(&gpu, &sprite_texture, DkImageFormat_RGBA8_Unorm, WIMSY_SPRITE,
                          WIMSY_SPRITE, texels)) {
        snprintf(why, why_size, "the sprite's upload: %s", gpu.failure);
        return false;
    }
    if (level >= 3u) {
        // The plaques' names, drawn by the processor in the screens' font.
        static uint8_t names[WIMSY_PLAQUES_SIDE * WIMSY_PLAQUES_SIDE * 4u];
        for (uint32_t y = 0; y < WIMSY_PLAQUES_SIDE; ++y) {
            for (uint32_t x = 0; x < WIMSY_PLAQUES_SIDE; ++x) {
                WimsyPlaqueTexel(x, y, &names[((size_t)y * WIMSY_PLAQUES_SIDE + x) * 4u]);
            }
        }
        if (!GpuUploadTexture(&gpu, &plaque_texture, DkImageFormat_RGBA8_Unorm, WIMSY_PLAQUES_SIDE,
                              WIMSY_PLAQUES_SIDE, names)) {
            snprintf(why, why_size, "the plaques' upload: %s", gpu.failure);
            return false;
        }
    }

    if (level >= 2u) {
        // As large as the largest level nought there is, 16 MiB: the 2048 by 2048 floor's
        // texels and the 8192 by 8192 one's blocks alike.
        DkGpuAddr staging_gpu = 0;
        uint8_t *const staging = (uint8_t *)Scratch(STAGING_BYTES, DK_MEMBLOCK_ALIGNMENT,
                                                    &staging_gpu, "the textures' staging", why,
                                                    why_size);
        if (staging == NULL) {
            return false;
        }
        for (uint32_t t = 0; t < WIMSY_TEXTURES; ++t) {
            if (LevelHas((WimsyTexture)t) &&
                !UploadLevels(&level_textures[t], (WimsyTexture)t, staging, staging_gpu, why,
                              why_size)) {
                return false;
            }
        }
    }

    if (on_screen) {
        return true;
    }
    const DkImage *const presented[SWAP_IMAGES] = { &swap_images[0], &swap_images[1] };
    dkSwapchainMakerDefaults(&swap_maker, gpu.device, nwindowGetDefault(), presented, SWAP_IMAGES);
    uint32_t aborted = 0;
    if (!AbortGuarded(MakeSwapchain, &aborted) || swapchain == NULL) {
        snprintf(why, why_size, "the swapchain on the default window: 0x%x", (unsigned)aborted);
        return false;
    }
    return true;
}

static void Finish(Thread *thread) {
    threadWaitForExit(thread);
    threadClose(thread);
}

/// Runs the frames: starts the render thread, drives the loop, and waits for it to end. False
/// with the reason when the thread did not start, and then no frame ran.
static bool RunFrames(char *why, size_t why_size) {
    memset(&render, 0, sizeof(render));
    semaphoreInit(&frame_go, 0);
    semaphoreInit(&slot_free, SLOTS);
    Result rc = threadCreate(&render_thread, RenderMain, NULL, NULL, STACK_SIZE, RENDER_PRIORITY,
                             RENDER_CORE);
    if (R_SUCCEEDED(rc)) {
        rc = threadStart(&render_thread);
        if (R_FAILED(rc)) {
            threadClose(&render_thread);
        }
    }
    if (R_FAILED(rc)) {
        snprintf(why, why_size, "the render thread did not start: 0x%x", (unsigned)rc);
        return false;
    }
    // This thread on core 0 alone for the frames, whatever core it was on, and back after.
    s32 ideal = 0;
    u64 mask = 0;
    const bool pinned = R_SUCCEEDED(svcGetThreadCoreMask(&ideal, &mask, CUR_THREAD_HANDLE)) &&
                        R_SUCCEEDED(svcSetThreadCoreMask(CUR_THREAD_HANDLE, MAIN_CORE,
                                                         1u << MAIN_CORE));
    // A heavy level's workers: the loads' on the core the main and render threads leave free,
    // the models' one there and one beside the main thread.
    static const int WORKER_CORE[WORKERS] = { 2, 2, MAIN_CORE, 2 };
    uint32_t started = 0;
    __atomic_store_n(&work_over, false, __ATOMIC_RELEASE);
    semaphoreInit(&work_done, 0);
    for (uint32_t w = 0; w < WORKERS && level >= 3u; ++w) {
        semaphoreInit(&work_go[w], 0);
        if (R_FAILED(threadCreate(&workers[w], WorkerMain, (void *)(uintptr_t)w, NULL, STACK_SIZE,
                                  WORKER_PRIORITY, WORKER_CORE[w])) ||
            R_FAILED(threadStart(&workers[w]))) {
            break;
        }
        ++started;
    }
    const bool staffed = level < 3u || started == WORKERS;
    __atomic_store_n(&frames_wanted, frames, __ATOMIC_RELEASE);
    GameWorldStart(&world);
    for (uint32_t k = 0; k < frames && staffed; ++k) {
        if (demo != NULL && demo->stopped != NULL && demo->stopped()) {
            // B: the frames end here, and the render thread waiting for the next one leaves.
            __atomic_store_n(&frames_wanted, k, __ATOMIC_RELEASE);
            semaphoreSignal(&frame_go);
            break;
        }
        semaphoreWait(&slot_free);
        const uint64_t began = armGetSystemTick();
        WriteSlot(k % SLOTS, k);
        main_us[k] = Microseconds(armGetSystemTick() - began);
        semaphoreSignal(&frame_go);
    }
    if (!staffed) {
        // The render thread waits for frames nobody makes: it is let through them, drawing none.
        gpu.ready = false;
        snprintf(gpu.failure, sizeof(gpu.failure), "only %u of the %u worker threads started",
                 (unsigned)started, (unsigned)WORKERS);
        for (uint32_t k = 0; k < frames; ++k) {
            semaphoreSignal(&frame_go);
        }
    }
    Finish(&render_thread);
    __atomic_store_n(&work_over, true, __ATOMIC_RELEASE);
    for (uint32_t w = 0; w < started; ++w) {
        semaphoreSignal(&work_go[w]);
        Finish(&workers[w]);
    }
    if (pinned && R_FAILED(svcSetThreadCoreMask(CUR_THREAD_HANDLE, ideal, (u32)mask))) {
        ReportNote("wimsy: the main thread could not be put back on core %d, mask 0x%llx, so it "
                   "stays on core %d alone",
                   (int)ideal, (unsigned long long)mask, MAIN_CORE);
    }
    return true;
}

static GameTimeCase time_cases[GAME_TIME_CASES];
static char time_names[GAME_TIME_CASES][GAME_TIME_NAME_MAX + 16u];

/// The timed cases' names for this level: the game scene's, with the level before them.
static void NameTimes(void) {
    (void)GameTimeCases(NULL, frames, time_cases);
    for (uint32_t i = 0; i < GAME_TIME_CASES; ++i) {
        snprintf(time_names[i], sizeof(time_names[i]), "level %u, %.*s", (unsigned)level,
                 (int)GAME_TIME_NAME_MAX, time_cases[i].name);
    }
}

/// The timed cases and the notes: frame 0 left out as the warm-up, each measure's cases carrying
/// its number as their time, as the game scene's do.
static void Times(void) {
    static GameTimes times;
    GameTimesOf(&times, frames, frame_us, main_us, render_us, sorted_us);
    const uint32_t timed = GameTimeCases(&times, frames, time_cases);
    char detail[160];
    snprintf(detail, sizeof(detail), "%u frames past the warm-up; %u of %u presented",
             (unsigned)timed, (unsigned)render.presented, (unsigned)frames);
    for (uint32_t i = 0; i < GAME_TIME_CASES; ++i) {
        if (timed == 0u) {
            ReportSkip(SUITE, time_names[i], REPORT_REASON_DEPENDS,
                       "the run's one frame is the warm-up, which the times leave out");
        } else if (ReportStart(SUITE, time_names[i])) {
            ReportCaseTime(SUITE, time_names[i], CHECK_NONE, render.presented == frames, detail,
                           time_cases[i].us);
        }
    }
    char scene[64];
    snprintf(scene, sizeof(scene), "wimsy: level %u, %s", (unsigned)level, WimsyLevelName(level));
    char note[REPORT_NOTE_MAX + 1];
    GameDescribeFramesOf(note, sizeof(note), scene, &times);
    ReportNote("%s", note);
    GameDescribePartsOf(note, sizeof(note), scene, &times);
    ReportNote("%s; the %u copies the judge keeps, each after its frame's present, took %u us at "
               "most",
               note, (unsigned)render.copied, (unsigned)render.copy_longest);
}

/// One judged frame's case: the kept image held to the reference, its picture the judge's own.
static void JudgeFrame(const char *name, uint32_t j) {
    if (!ReportStart(SUITE, name)) {
        return;
    }
    WimsyVerdict verdict;
    WimsyJudge(level, judged[j], frames, kept[j], expected_pixels, pixel_verdicts, &verdict);
    (void)PictureBeginOwn(WIMSY_WIDTH, WIMSY_HEIGHT, PICTURE_COLOUR, kept[j], expected_pixels,
                          pixel_verdicts);
    char detail[400];
    WimsyDescribeVerdict(detail, sizeof(detail), &verdict);
    // Bit for bit where every range is one value; elsewhere the samples on an edge may fall
    // either way and a colour is held to a range.
    ReportCase(SUITE, name, verdict.exact ? CHECK_EXACT : CHECK_POSITION,
               WimsyVerdictPasses(&verdict), WimsyVerdictPasses(&verdict) ? NULL : detail);
    if (WimsyVerdictPasses(&verdict) && !verdict.exact) {
        ReportNote("wimsy: level %u, frame %u: %s", (unsigned)level, (unsigned)judged[j], detail);
    }
}

static void RunLevel(void) {
    on_screen = false;
    last_shown = NULL;
    static char bring_up[96];
    static char frames_name[96];
    static char judged_names[WIMSY_JUDGED][96];
    static char peek_name[112];
    snprintf(peek_name, sizeof(peek_name),
             "level %u: the processor reads what a compute program stored, on each of %u frames",
             (unsigned)level, (unsigned)frames);
    snprintf(bring_up, sizeof(bring_up),
             "level %u, %s: a graphics device and a way to the display come up", (unsigned)level,
             WimsyLevelName(level));
    snprintf(frames_name, sizeof(frames_name), "level %u: %u frames are drawn and presented",
             (unsigned)level, (unsigned)frames);
    judged_count = WimsyJudgedFrames(frames, judged);
    for (uint32_t j = 0; j < judged_count; ++j) {
        snprintf(judged_names[j], sizeof(judged_names[j]),
                 "level %u, frame %u of %u is what the processor works out", (unsigned)level,
                 (unsigned)judged[j], (unsigned)frames);
    }
    NameTimes();
    const char *after[2u + WIMSY_JUDGED + GAME_TIME_CASES];
    size_t after_count = 0;
    after[after_count++] = frames_name;
    for (uint32_t j = 0; j < judged_count; ++j) {
        after[after_count++] = judged_names[j];
    }
    if (level >= 3u) {
        after[after_count++] = peek_name;
    }
    for (uint32_t i = 0; timing && i < GAME_TIME_CASES; ++i) {
        after[after_count++] = time_names[i];
    }

    const char *depends = NULL;
    char why[400] = "";
    if (!ReportStart(SUITE, bring_up)) {
        depends = "the level's bring-up did not run, so neither did its frames";
    } else if (ViewerScreenUp() && demo == NULL) {
        // The window is the program's screen's while it is up, and a swapchain made on it then
        // is what Eden never came back from; the browser's run of a level shows its frames
        // through that screen instead.
        ReportSkip(SUITE, bring_up, REPORT_REASON_UNAVAILABLE,
                   "the program's screen is up on the window, and the level presents through a "
                   "swapchain of its own there: it runs in a quick run from the mode file, or "
                   "from the browser's demos");
        depends = "the level did not come up, so its frames did not run";
    } else {
        on_screen = ViewerScreenUp();
        if (on_screen) {
            ReportNote("wimsy: level %u's frames go through the program's screen, read back and "
                       "composed by the processor, rather than a swapchain: their times are that "
                       "way's, not a quick run's",
                       (unsigned)level);
        }
        bool up = BringUp(why, sizeof(why));
        for (uint32_t j = 0; j < judged_count && up; ++j) {
            kept[j] = (uint8_t *)malloc(WIMSY_IMAGE_BYTES);
            up = kept[j] != NULL;
        }
        if (up && (expected_pixels == NULL || pixel_verdicts == NULL)) {
            expected_pixels = (uint8_t *)malloc(WIMSY_IMAGE_BYTES);
            pixel_verdicts = (uint8_t *)malloc(WIMSY_PIXELS);
            up = expected_pixels != NULL && pixel_verdicts != NULL;
        }
        if (!up && why[0] == '\0') {
            snprintf(why, sizeof(why), "no memory for the %u frames the judge keeps, %u bytes each",
                     (unsigned)judged_count, (unsigned)WIMSY_IMAGE_BYTES);
        }
        CHECK_TRUE(SUITE, bring_up, CHECK_NONE, up, "%s", why);
        if (!up) {
            depends = "the level did not come up, so its frames did not run";
        }
    }

    if (depends == NULL && !ReportStart(SUITE, frames_name)) {
        // The frames' case is skipped, its line says why (it ended the program before).
        --after_count;
        memmove(after, after + 1, after_count * sizeof(after[0]));
        depends = "the frames' case did not run, so there are no frames to judge";
    } else if (depends == NULL) {
        const bool ran = RunFrames(why, sizeof(why));
        const uint32_t drawn = __atomic_load_n(&frames_wanted, __ATOMIC_ACQUIRE);
        if (ran && drawn < frames) {
            // Stopped by the browser's B: the judged frames it drew are judged, the others and
            // the times are skipped as asked, and the frames' case says how far it got.
            char said[200];
            snprintf(said, sizeof(said),
                     "stopped at frame %u of %u by the browser's B: %u of %u frames were "
                     "presented",
                     (unsigned)drawn, (unsigned)frames, (unsigned)render.presented,
                     (unsigned)frames);
            ReportSkip(SUITE, frames_name, REPORT_REASON_ASKED, said);
            for (uint32_t j = 0; j < judged_count; ++j) {
                if (judged[j] < drawn && kept[j] != NULL) {
                    JudgeFrame(judged_names[j], j);
                } else {
                    ReportSkip(SUITE, judged_names[j], REPORT_REASON_ASKED, said);
                }
            }
            if (level >= 3u) {
                ReportSkip(SUITE, peek_name, REPORT_REASON_ASKED, said);
            }
            for (uint32_t i = 0; timing && i < GAME_TIME_CASES; ++i) {
                ReportSkip(SUITE, time_names[i], REPORT_REASON_ASKED, said);
            }
            after_count = 0;
        } else {
            char detail[400];
            snprintf(detail, sizeof(detail),
                     "%u of %u frames were presented and %u of %u kept; %s",
                     (unsigned)render.presented, (unsigned)frames, (unsigned)render.copied,
                     (unsigned)judged_count,
                     !ran ? why : gpu.failure[0] != '\0' ? gpu.failure
                                                        : "the graphics queue reported no fault");
            const bool whole =
                ran && render.presented == frames && render.copied == judged_count;
            ReportCase(SUITE, frames_name, CHECK_NONE, whole, whole ? NULL : detail);
            if (whole) {
                for (uint32_t j = 0; j < judged_count; ++j) {
                    if (free_camera && level >= 3u) {
                        ReportSkip(SUITE, judged_names[j], REPORT_REASON_ASKED,
                                   "the camera was the person's, by the pause menu's row: no "
                                   "frame of the run is the walk's");
                    } else {
                        JudgeFrame(judged_names[j], j);
                    }
                }
                if (level >= 3u && ReportStart(SUITE, peek_name)) {
                    uint8_t want[4];
                    WimsyComputeTexel(WIMSY_PEEK_TEXTURE, WIMSY_PEEK_LEVEL, render.peek_wrong_at,
                                      0u, 0u, want);
                    CHECK_TRUE(SUITE, peek_name, CHECK_EXACT,
                               render.peeked == frames && render.peek_wrong == 0u &&
                                   render.screens_read == judged_count &&
                                   render.screens_wrong == 0u,
                               "%u of %u frames read back, %u of them another texel; the first, "
                               "frame %u, read %02x %02x %02x %02x where the program stored %02x "
                               "%02x %02x %02x; %u of %u judged frames read back whole, %u texels "
                               "of them not what the processor works out%s%s",
                               (unsigned)render.peeked, (unsigned)frames,
                               (unsigned)render.peek_wrong, (unsigned)render.peek_wrong_at,
                               render.peek_got[0], render.peek_got[1], render.peek_got[2],
                               render.peek_got[3], want[0], want[1], want[2], want[3],
                               (unsigned)render.screens_read, (unsigned)judged_count,
                               (unsigned)render.screens_wrong,
                               render.screens_wrong != 0u ? ": " : "", render.screens_first);
                }
                if (timing) {
                    Times();
                }
            } else {
                --after_count;
                memmove(after, after + 1, after_count * sizeof(after[0]));
                depends = "not every frame was presented and kept, so none is judged or timed";
            }
        }
    }
    if (depends != NULL) {
        for (size_t i = 0; i < after_count; ++i) {
            ReportSkip(SUITE, after[i], REPORT_REASON_DEPENDS, depends);
        }
    }

    if (swapchain != NULL) {
        dkQueueWaitIdle(gpu.queue);
        dkSwapchainDestroy(swapchain);
        swapchain = NULL;
    }
    GpuClose(&gpu);
    for (uint32_t j = 0; j < WIMSY_JUDGED; ++j) {
        free(kept[j]);
        kept[j] = NULL;
    }
}

void RunWimsy(void) {
    const MusicMade before = MusicMadeSoFar();
    for (level = 0; level < WIMSY_LEVELS; ++level) {
        if ((levels >> level & 1u) != 0u) {
            RunLevel();
            // A person's camera goes with the run that had it.
            WimsyCameraWalk();
        }
    }
    if (levels != 0u) {
        MusicJudgeOff(SUITE, "while the levels ran", before);
    }
    free(expected_pixels);
    free(pixel_verdicts);
    expected_pixels = NULL;
    pixel_verdicts = NULL;
}
