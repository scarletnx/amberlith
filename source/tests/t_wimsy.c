// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <switch.h>

#include "../check.h"
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
// A draw's uniform buffer starts where the graphics unit lets one start.
#define INSTANCE_STRIDE DK_UNIFORM_BUF_ALIGNMENT
_Static_assert(sizeof(WimsyInstance) <= INSTANCE_STRIDE, "an instance fits a uniform buffer's step");
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

// The graphics device and everything a level's frames draw with, made before its first frame.
static uint32_t level;
static GpuContext gpu;
static DkShader common_vs;
static DkShader texture_fs;
static DkShader model_vs;
static DkShader lit_fs;
static DkShader world_vs;
static DkShader flat_fs;
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
static DkGpuAddr image_set;
static DkGpuAddr sampler_set;
static DkGpuAddr shown_quad;
static DkGpuAddr model_vertices;
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

// What the render thread saw, read by the main thread once it is joined.
static struct {
    uint32_t presented;
    uint32_t copied;
    /// The longest a judged frame's copy took, which falls after its present.
    uint32_t copy_longest;
} render;

static uint32_t Microseconds(uint64_t ticks) {
    const uint64_t us = armTicksToNs(ticks) / 1000u;
    return us > UINT32_MAX ? UINT32_MAX : (uint32_t)us;
}

static GpuVertex Corner(float x, float y, float u, float v) {
    const GpuVertex vertex = { x, y, 0.0f, u, v, 1.0f, 1.0f, 1.0f, 1.0f };
    return vertex;
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
    instance_count[slot] = WimsyInstancesOf(level, k, frames, instances);
    for (uint32_t i = 0; i < instance_count[slot]; ++i) {
        memcpy(instance_cpu[slot] + (size_t)i * INSTANCE_STRIDE, &instances[i],
               sizeof(instances[i]));
    }
    WimsyViewOf(level, k, frames, view_cpu[slot]);
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
        const bool textured =
            draw->material == WIMSY_MATERIAL_FLOOR || draw->material == WIMSY_MATERIAL_LANE;
        if (textured) {
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
    dkCmdBufBindVtxBuffer(gpu.cmdbuf, 0, model_vertices,
                          WIMSY_MESH_VERTICES * (uint32_t)sizeof(ModelVertex));
    dkCmdBufBindIdxBuffer(gpu.cmdbuf, DkIdxFormat_Uint16, model_indices);
    dkCmdBufBindUniformBuffer(gpu.cmdbuf, DkStage_Fragment, 0, light_block,
                              DK_UNIFORM_BUF_ALIGNMENT);
    for (uint32_t i = 0; i < instance_count[slot]; ++i) {
        const DkGpuAddr block = instance_gpu[slot] + (DkGpuAddr)i * INSTANCE_STRIDE;
        if (level < 3u) {
            dkCmdBufBindUniformBuffer(gpu.cmdbuf, DkStage_Vertex, 0, block, INSTANCE_STRIDE);
            dkCmdBufDrawIndexed(gpu.cmdbuf, DkPrimitive_Triangles, WIMSY_MESH_INDICES, 1, 0, 0, 0);
            continue;
        }
        // A heavy frame draws her as a game draws a character of several materials: a draw a
        // part, each with its uniform buffer bound again.
        for (uint32_t part = 0; part < WIMSY_MESH_PARTS; ++part) {
            dkCmdBufBindUniformBuffer(gpu.cmdbuf, DkStage_Vertex, 0, block, INSTANCE_STRIDE);
            dkCmdBufDrawIndexed(gpu.cmdbuf, DkPrimitive_Triangles,
                                WIMSY_MESH_PART[part].index_count, 1,
                                WIMSY_MESH_PART[part].first_index, 0, 0);
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
    const int image = dkQueueAcquireImage(gpu.queue, swapchain);
    const uint64_t began = armGetSystemTick();
    GpuBeginPass(&gpu);
    // The processor rewrote this slot after the graphics unit last read it, through memory the
    // unit caches in its level two cache: what the previous frame left there is dropped. The
    // previous frame's wait for idle wrote everything it drew back first.
    dkCmdBufBarrier(gpu.cmdbuf, DkBarrier_None, DkInvalidateFlags_L2Cache);
    dkCmdBufBindImageDescriptorSet(gpu.cmdbuf, image_set, 2u + WIMSY_TEXTURES);
    dkCmdBufBindSamplerDescriptorSet(gpu.cmdbuf, sampler_set, 2);
    if (level == 0u) {
        RecordSprites(slot);
    } else {
        RecordModels(slot);
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

static void RenderMain(void *argument) {
    (void)argument;
    uint64_t previous = 0;
    for (uint32_t k = 0; k < frames; ++k) {
        semaphoreWait(&frame_go);
        if (k == 0u) {
            previous = armGetSystemTick();
        }
        const uint64_t worked = gpu.ready ? DrawFrame(k, k % SLOTS) : 0u;
        // After the present was queued: the wait for the display is in the next frame's acquire.
        const uint64_t now = armGetSystemTick();
        frame_us[k] = Microseconds(now - previous);
        render_us[k] = Microseconds(worked);
        previous = now;
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
    spec.data_size = (level >= 2u ? 24u : 4u) * 1024u * 1024u;
    // The presented images, the sprite, the two targets of four samples a higher level has, and
    // level 2's two textures with their levels.
    spec.image_size = (level >= 2u ? 96u : 64u) * 1024u * 1024u;
    // A heavy frame's fifteen hundred draws, a bind and a draw each.
    spec.command_size = (level >= 3u ? 2048u : 512u) * 1024u;
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
        GpuLoadShader(&gpu, &world_vs, ShaderData(SHADER_WIMSY_WORLD_VERT),
                      ShaderSize(SHADER_WIMSY_WORLD_VERT)) &&
        GpuLoadShader(&gpu, &flat_fs, ShaderData(SHADER_FLAT_FRAG), ShaderSize(SHADER_FLAT_FRAG));
    if (!loaded) {
        snprintf(why, why_size, "the shaders: %s", gpu.failure);
        return false;
    }
    bool images = GpuCreateTexture(&gpu, &sprite_texture, &sprite_layout, DkImageFormat_RGBA8_Unorm,
                                   WIMSY_SPRITE, WIMSY_SPRITE);
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
    }
    GpuVertex *const world = (GpuVertex *)Scratch(
        WIMSY_WORLD_VERTICES_MAX * (uint32_t)sizeof(GpuVertex), 256u, &world_vertices,
        "the world's vertices", why, why_size);
    GpuVertex *const shown = (GpuVertex *)Scratch(6u * (uint32_t)sizeof(GpuVertex), 256u,
                                                  &shown_quad, "the quad shown", why, why_size);
    ModelVertex *const vertices = (ModelVertex *)Scratch(
        WIMSY_MESH_VERTICES * (uint32_t)sizeof(ModelVertex), 256u, &model_vertices,
        "the model's vertices", why, why_size);
    uint16_t *const indices = (uint16_t *)Scratch(
        WIMSY_MESH_INDICES * (uint32_t)sizeof(uint16_t), 256u, &model_indices,
        "the model's indices", why, why_size);
    WimsyLight *const light = (WimsyLight *)Scratch(DK_UNIFORM_BUF_ALIGNMENT,
                                                    DK_UNIFORM_BUF_ALIGNMENT, &light_block,
                                                    "the light", why, why_size);
    DkImageDescriptor *const image_descriptors = (DkImageDescriptor *)Scratch(
        (2u + WIMSY_TEXTURES) * (uint32_t)sizeof(DkImageDescriptor), DK_IMAGE_DESCRIPTOR_ALIGNMENT,
        &image_set, "the image descriptors", why, why_size);
    DkSamplerDescriptor *const sampler_descriptor = (DkSamplerDescriptor *)Scratch(
        2u * (uint32_t)sizeof(DkSamplerDescriptor), DK_SAMPLER_DESCRIPTOR_ALIGNMENT, &sampler_set,
        "the sampler descriptors", why, why_size);
    if (why[0] != '\0') {
        return false;
    }
    world_draw_count = WimsyWorldOf(level, (WimsyWorldVertex *)world, &world_vertex_count,
                                    world_draws);

    Rectangle(shown, -1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 1.0f);
    for (uint32_t v = 0; v < WIMSY_MESH_VERTICES; ++v) {
        const WimsyMeshVertex *const from = &WIMSY_MESH_VERTEX[v];
        // The floats the reference reads the mesh as (wimsyref.c, HoldInstance).
        vertices[v].position[0] = (float)from->x / (float)WIMSY_MESH_SCALE;
        vertices[v].position[1] = (float)from->y / (float)WIMSY_MESH_SCALE;
        vertices[v].position[2] = (float)from->z / (float)WIMSY_MESH_SCALE;
        vertices[v].normal[0] = (float)from->nx / 127.0f;
        vertices[v].normal[1] = (float)from->ny / 127.0f;
        vertices[v].normal[2] = (float)from->nz / 127.0f;
        memcpy(vertices[v].colour, WIMSY_MESH_PALETTE[from->colour], 3u);
        vertices[v].colour[3] = 255u;
    }
    memcpy(indices, WIMSY_MESH_INDEX, WIMSY_MESH_INDICES * sizeof(uint16_t));
    memset(light, 0, DK_UNIFORM_BUF_ALIGNMENT);
    WimsyLightOf(level, light);

    DkImageView view;
    dkImageViewDefaults(&view, &sprite_texture);
    dkImageDescriptorInitialize(&image_descriptors[0], &view, false, false);
    dkImageViewDefaults(&view, &gpu.image);
    dkImageDescriptorInitialize(&image_descriptors[1], &view, false, false);
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
        dkImageDescriptorInitialize(&image_descriptors[2u + t], &view, false, false);
    }

    static uint8_t texels[WIMSY_SPRITE_BYTES];
    WimsySpriteTexels(texels);
    if (!GpuUploadTexture(&gpu, &sprite_texture, DkImageFormat_RGBA8_Unorm, WIMSY_SPRITE,
                          WIMSY_SPRITE, texels)) {
        snprintf(why, why_size, "the sprite's upload: %s", gpu.failure);
        return false;
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
    GameWorldStart(&world);
    for (uint32_t k = 0; k < frames; ++k) {
        semaphoreWait(&slot_free);
        const uint64_t began = armGetSystemTick();
        WriteSlot(k % SLOTS, k);
        main_us[k] = Microseconds(armGetSystemTick() - began);
        semaphoreSignal(&frame_go);
    }
    Finish(&render_thread);
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
    static char bring_up[96];
    static char frames_name[96];
    static char judged_names[WIMSY_JUDGED][96];
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
    const char *after[1u + WIMSY_JUDGED + GAME_TIME_CASES];
    size_t after_count = 0;
    after[after_count++] = frames_name;
    for (uint32_t j = 0; j < judged_count; ++j) {
        after[after_count++] = judged_names[j];
    }
    for (uint32_t i = 0; timing && i < GAME_TIME_CASES; ++i) {
        after[after_count++] = time_names[i];
    }

    const char *depends = NULL;
    char why[400] = "";
    if (!ReportStart(SUITE, bring_up)) {
        depends = "the level's bring-up did not run, so neither did its frames";
    } else if (ViewerScreenUp()) {
        // The window is the program's screen's while it is up, and a swapchain made on it then
        // is what Eden never came back from (the game scene shows its frames through the screen;
        // these are too large for it).
        ReportSkip(SUITE, bring_up, REPORT_REASON_UNAVAILABLE,
                   "the program's screen is up on the window, and the level presents through a "
                   "swapchain of its own there: it runs in a quick run from the mode file");
        depends = "the level did not come up, so its frames did not run";
    } else {
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
        char detail[400];
        snprintf(detail, sizeof(detail), "%u of %u frames were presented and %u of %u kept; %s",
                 (unsigned)render.presented, (unsigned)frames, (unsigned)render.copied,
                 (unsigned)judged_count,
                 !ran ? why : gpu.failure[0] != '\0' ? gpu.failure
                                                    : "the graphics queue reported no fault");
        const bool whole = ran && render.presented == frames && render.copied == judged_count;
        ReportCase(SUITE, frames_name, CHECK_NONE, whole, whole ? NULL : detail);
        if (whole) {
            for (uint32_t j = 0; j < judged_count; ++j) {
                JudgeFrame(judged_names[j], j);
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
    for (level = 0; level < WIMSY_LEVELS; ++level) {
        if ((levels >> level & 1u) != 0u) {
            RunLevel();
        }
    }
    free(expected_pixels);
    free(pixel_verdicts);
    expected_pixels = NULL;
    pixel_verdicts = NULL;
}
