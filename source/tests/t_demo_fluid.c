// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <switch.h>

#include "../check.h"
#include "../demo.h"
#include "../demo_fluidref.h"
#include "../gameref.h"
#include "../gpu.h"
#include "../mode.h"
#include "../picture.h"
#include "../report.h"
#include "../shaders.h"
#include "../viewer.h"
#include "tests.h"

// The fluid demo: a 2D Navier-Stokes fluid, Stam's stable fluids, made the way a game makes one,
// as fragment passes in ping-pong between two targets a field. Each frame the main thread, on
// core 0, writes the frame's splats; the render thread, on core 1, takes an image from the
// swapchain and records the frame's passes: the force splats into the velocity, the dye splats
// into the dye, the divergence, FLUID_JACOBI Jacobi passes of the pressure, the gradient taken
// off the velocity, the velocity advected along itself and the dye along it; then the dye drawn
// onto the image, submitted and presented. demo_fluidref.c is each pass on the processor.
//
// A few frames are judged (FluidJudgedFrames): the render thread copies every field each of
// their passes reads and writes into memory of its own, and once the frames are over each pass
// is worked out again from the frame's own inputs, the early frames are held to the processor's
// whole simulation from frame 0, and every judged frame to the invariants: the divergence the
// projection leaves, the dye's mass and no field gone wrong. A frame's fields depend on its
// number alone, so a machine that runs the frames slowly gets the same verdicts.

static const char *const SUITE = "fluid";

#define SWAP_WIDTH 1280u
#define SWAP_HEIGHT 720u
#define SWAP_IMAGES 2u
// The main thread is one frame ahead of the render thread, at most: two slots of splats.
#define SLOTS 2u

#define MAIN_CORE 0
#define RENDER_CORE 1
#define RENDER_PRIORITY 0x2B
#define STACK_SIZE 0x10000

// What this run asks: its frames, whether they are timed, and how a person stops it (the
// browser's B), NULL when nobody can. ran_frames is how many it ran, frames unless stopped.
static uint32_t frames = MODE_FRAMES_DEFAULT;
static bool timing = true;
static bool (*stop_asked)(void);
static uint32_t ran_frames;

static const FluidGrid GRID = { FLUID_W, FLUID_H };
#define DYE_W (FLUID_W * FLUID_DYE_SCALE)
#define DYE_H (FLUID_H * FLUID_DYE_SCALE)

// The fields' targets, two a field the passes ping-pong between and one for the divergence;
// their image descriptors in this order.
enum {
    VELOCITY_0,
    VELOCITY_1,
    PRESSURE_0,
    PRESSURE_1,
    DIVERGENCE,
    DYE_0,
    DYE_1,
    FIELDS,
};

// The graphics device and everything the frames draw with, made before the first frame.
static GpuContext gpu;
static DkShader position_vs;
static DkShader common_vs;
static DkShader splat_fs;
static DkShader divergence_fs;
static DkShader jacobi_fs;
static DkShader gradient_fs;
static DkShader advect_fs;
static DkShader display_fs;
static DkImage field_images[FIELDS];
static DkImageLayout field_layouts[FIELDS];
static DkImage swap_images[SWAP_IMAGES];
static DkImageLayout swap_layouts[SWAP_IMAGES];
static DkSwapchain swapchain;
static DkSwapchainMaker swap_maker;
// The program's screen was up when the scene began (a run with a screen): the frames are shown
// through it, the dye drawn into the main target and copied out, and no swapchain is made.
static bool on_screen;
static DkGpuAddr image_set;
static DkGpuAddr sampler_set;
static DkGpuAddr triangle;
static DkGpuAddr shown_quad;
// The passes' constants: the grid's for the divergence and the gradient, the Jacobi's first
// pass and the rest, and each advection's.
static DkGpuAddr grid_block;
static DkGpuAddr jacobi_first;
static DkGpuAddr jacobi_rest;
static DkGpuAddr advect_velocity;
static DkGpuAddr advect_dye;

// What the main thread writes for a frame, one slot per frame in flight: the uniform buffers of
// the two splat passes, a count and four splats each.
typedef struct {
    uint32_t count[4];
    FluidSplat splats[FLUID_SPLATS_MAX];
} SplatBlock;
_Static_assert(sizeof(FluidSplat) == 32u && sizeof(SplatBlock) == 144u,
               "a splat block is laid out as fluid_splat.frag.glsl's std140 block");

static SplatBlock *force_cpu[SLOTS];
static DkGpuAddr force_gpu[SLOTS];
static SplatBlock *dye_cpu[SLOTS];
static DkGpuAddr dye_gpu[SLOTS];

// The judged frames and where their copies land: one region a frame, each read at its offset.
static uint32_t judged[FLUID_JUDGED_MAX];
static uint32_t judged_count;
static size_t read_offset[FLUID_READS];
static size_t region_bytes;
static uint8_t *reads_cpu;
static DkGpuAddr reads_gpu;

// What the frames left: their times.
static uint32_t frame_us[MODE_FRAMES_MAX];
static uint32_t sorted_us[MODE_FRAMES_MAX];
static uint32_t main_us[MODE_FRAMES_MAX];
static uint32_t render_us[MODE_FRAMES_MAX];

static Thread render_thread;
static Semaphore frame_go;
static Semaphore slot_free;

// What the render thread saw, read by this one once it is joined.
static struct {
    uint32_t presented;
    uint32_t wrong_core;
    uint32_t wrong_core_at;
    uint32_t core_seen;
} render;

static uint32_t Microseconds(uint64_t ticks) {
    const uint64_t us = armTicksToNs(ticks) / 1000u;
    return us > UINT32_MAX ? UINT32_MAX : (uint32_t)us;
}

static const float *ReadOf(uint32_t judged_index, FluidRead read) {
    return (const float *)(reads_cpu + judged_index * region_bytes + read_offset[read]);
}

/// The field at @p index as a render target, with the passes' shared shaders bound.
static void Target(uint32_t index, uint32_t width, uint32_t height) {
    GpuSetTarget(&gpu, &field_images[index], width, height);
}

static void Texture(uint32_t binding, uint32_t index) {
    dkCmdBufBindTexture(gpu.cmdbuf, DkStage_Fragment, binding, dkMakeTextureHandle(index, 0));
}

/// One pass over the whole of the bound target: a triangle that covers it.
static void Pass(const DkShader *fragment, DkGpuAddr block, uint32_t block_size) {
    GpuBindPositions(&gpu, &position_vs, fragment, triangle, 3u * 16u);
    dkCmdBufBindUniformBuffer(gpu.cmdbuf, DkStage_Fragment, 0, block, block_size);
    dkCmdBufDraw(gpu.cmdbuf, DkPrimitive_Triangles, 3, 1, 0, 0);
}

/// What the passes after a pass that wrote a target wait for: its fragments done, and the
/// texture cache that may hold the target's old texels dropped.
static void AfterPass(void) {
    dkCmdBufBarrier(gpu.cmdbuf, DkBarrier_Fragments, DkInvalidateFlags_Image);
}

/// Copies field @p index into read @p read of judged frame @p judged_index.
static void CopyOut(uint32_t index, uint32_t judged_index, FluidRead read) {
    const bool dye = index == DYE_0 || index == DYE_1;
    const uint32_t width = dye ? DYE_W : FLUID_W;
    const uint32_t height = dye ? DYE_H : FLUID_H;
    DkImageView view;
    dkImageViewDefaults(&view, &field_images[index]);
    const DkImageRect whole = { 0, 0, 0, width, height, 1 };
    const DkCopyBuf to = { reads_gpu + judged_index * region_bytes + read_offset[read], 0, 0 };
    dkCmdBufCopyImageToBuffer(gpu.cmdbuf, &view, &whole, &to, 0);
}

/// The fields' indices at a frame's start: which of each pair holds the velocity, the pressure
/// and the dye the frame starts from.
static uint32_t velocity_at;
static uint32_t pressure_at;
static uint32_t dye_at;

/// Records, submits and presents frame @p k from @p slot; @p judged_index is the frame's place
/// in the judged list, or -1. Hands back the render thread's part of the frame's work: from the
/// image in hand to the queue standing idle after the frame's submit.
static uint64_t DrawFrame(uint32_t k, uint32_t slot, int judged_index) {
    const int image = on_screen ? 0 : dkQueueAcquireImage(gpu.queue, swapchain);
    const uint64_t began = armGetSystemTick();
    GpuBeginPass(&gpu);
    // The processor rewrote this slot's splats after the graphics unit last read them, through
    // memory the unit caches in its level two cache: what the frame before last left there is
    // dropped. The last frame's wait for idle wrote every target back first.
    dkCmdBufBarrier(gpu.cmdbuf, DkBarrier_None, DkInvalidateFlags_L2Cache);
    dkCmdBufBindImageDescriptorSet(gpu.cmdbuf, image_set, FIELDS);
    dkCmdBufBindSamplerDescriptorSet(gpu.cmdbuf, sampler_set, 2);

    const uint32_t v_in = VELOCITY_0 + velocity_at;
    const uint32_t v_other = VELOCITY_0 + (velocity_at ^ 1u);
    const uint32_t p_in = PRESSURE_0 + pressure_at;
    const uint32_t d_in = DYE_0 + dye_at;
    const uint32_t d_other = DYE_0 + (dye_at ^ 1u);
    const uint32_t j = (uint32_t)judged_index;
    if (judged_index >= 0) {
        CopyOut(v_in, j, FLUID_READ_VELOCITY_IN);
        CopyOut(p_in, j, FLUID_READ_PRESSURE_IN);
        CopyOut(d_in, j, FLUID_READ_DYE_IN);
        // The copies read what the passes below write over.
        dkCmdBufBarrier(gpu.cmdbuf, DkBarrier_Full, 0);
    }

    // The splats: the velocity into its other target, the dye into its.
    Target(v_other, FLUID_W, FLUID_H);
    Texture(0, v_in);
    Pass(&splat_fs, force_gpu[slot], sizeof(SplatBlock));
    Target(d_other, DYE_W, DYE_H);
    Texture(0, d_in);
    Pass(&splat_fs, dye_gpu[slot], sizeof(SplatBlock));
    AfterPass();

    Target(DIVERGENCE, FLUID_W, FLUID_H);
    Texture(0, v_other);
    Pass(&divergence_fs, grid_block, 32u);
    AfterPass();

    // The pressure solve, ping-ponging from the last frame's pressure.
    uint32_t p_from = p_in;
    for (uint32_t n = 0; n < FLUID_JACOBI; ++n) {
        const uint32_t p_to = p_from == PRESSURE_0 ? PRESSURE_1 : PRESSURE_0;
        Target(p_to, FLUID_W, FLUID_H);
        Texture(0, p_from);
        Texture(1, DIVERGENCE);
        Pass(&jacobi_fs, n == 0u ? jacobi_first : jacobi_rest, 32u);
        AfterPass();
        p_from = p_to;
    }

    // The projection, into the velocity's first target; then both advections along it.
    Target(v_in, FLUID_W, FLUID_H);
    Texture(0, v_other);
    Texture(1, p_from);
    Pass(&gradient_fs, grid_block, 32u);
    AfterPass();
    if (judged_index >= 0) {
        CopyOut(v_other, j, FLUID_READ_SPLATTED);
        dkCmdBufBarrier(gpu.cmdbuf, DkBarrier_Full, 0);
    }
    Target(v_other, FLUID_W, FLUID_H);
    Texture(0, v_in);
    Texture(1, v_in);
    Pass(&advect_fs, advect_velocity, 32u);
    Target(d_in, DYE_W, DYE_H);
    Texture(0, v_in);
    Texture(1, d_other);
    Pass(&advect_fs, advect_dye, 32u);
    dkCmdBufBarrier(gpu.cmdbuf, DkBarrier_Full, DkInvalidateFlags_Image);
    if (judged_index >= 0) {
        CopyOut(d_other, j, FLUID_READ_DYED);
        CopyOut(DIVERGENCE, j, FLUID_READ_DIVERGENCE);
        CopyOut(p_from, j, FLUID_READ_PRESSURE);
        CopyOut(v_in, j, FLUID_READ_PROJECTED);
        CopyOut(v_other, j, FLUID_READ_VELOCITY);
        CopyOut(d_in, j, FLUID_READ_DYE);
    }

    // The dye shown: onto the swapchain's image, or the main target the screen is fed from.
    if (on_screen) {
        GpuSetTarget(&gpu, NULL, 0, 0);
    } else {
        GpuSetTarget(&gpu, &swap_images[image], SWAP_WIDTH, SWAP_HEIGHT);
    }
    dkCmdBufBindTexture(gpu.cmdbuf, DkStage_Fragment, 0, dkMakeTextureHandle(d_in, 1));
    GpuBindVertices(&gpu, &common_vs, &display_fs, shown_quad, 6u * (uint32_t)sizeof(GpuVertex));
    dkCmdBufDraw(gpu.cmdbuf, DkPrimitive_Triangles, 6, 1, 0, 0);
    if (on_screen) {
        dkCmdBufBarrier(gpu.cmdbuf, DkBarrier_Full, DkInvalidateFlags_Image);
        DkImageView view;
        dkImageViewDefaults(&view, &gpu.image);
        const DkImageRect whole = { 0, 0, 0, DYE_W, DYE_H, 1 };
        const DkCopyBuf to = { dkMemBlockGetGpuAddr(gpu.read_memory), 0, 0 };
        dkCmdBufCopyImageToBuffer(gpu.cmdbuf, &view, &whole, &to, 0);
    }
    GpuSubmit(&gpu);
    const uint64_t worked = armGetSystemTick() - began;
    // Presented whatever the submit came to: an image taken and never queued back would leave
    // the swapchain one short for the destroy that follows a fault.
    if (!on_screen) {
        dkQueuePresentImage(gpu.queue, swapchain, image);
    }
    velocity_at ^= 1u;
    pressure_at = p_from - PRESSURE_0;
    if (!gpu.ready) {
        return worked;
    }
    if (!on_screen ||
        ViewerShowImage((const uint8_t *)dkMemBlockGetCpuAddr(gpu.read_memory), DYE_W, DYE_H)) {
        ++render.presented;
    }
    return worked;
}

// The frame the main thread stopped at: the render thread draws none from it on.
static uint32_t last_frame;

static void RenderMain(void *argument) {
    (void)argument;
    uint64_t previous = 0;
    uint32_t next = 0;
    for (uint32_t k = 0; k < frames; ++k) {
        semaphoreWait(&frame_go);
        if (k >= __atomic_load_n(&last_frame, __ATOMIC_ACQUIRE)) {
            break;
        }
        if (k == 0u) {
            previous = armGetSystemTick();
        }
        const uint32_t core = svcGetCurrentProcessorNumber();
        if (core != RENDER_CORE && render.wrong_core++ == 0u) {
            render.wrong_core_at = k;
            render.core_seen = core;
        }
        const int judged_index = next < judged_count && judged[next] == k ? (int)next++ : -1;
        const uint64_t worked = gpu.ready ? DrawFrame(k, k % SLOTS, judged_index) : 0u;
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

/// A pass's constant block: the target's size and the velocity grid's, and four numbers.
static DkGpuAddr Block(uint32_t width, uint32_t height, float k0, float k1, float k2, float k3,
                       char *why, size_t why_size) {
    DkGpuAddr address = 0;
    uint32_t *const words = (uint32_t *)Scratch(DK_UNIFORM_BUF_ALIGNMENT, DK_UNIFORM_BUF_ALIGNMENT,
                                                &address, "a pass's constants", why, why_size);
    if (words == NULL) {
        return 0;
    }
    memset(words, 0, DK_UNIFORM_BUF_ALIGNMENT);
    words[0] = width;
    words[1] = height;
    words[2] = FLUID_W;
    words[3] = FLUID_H;
    const float k[4] = { k0, k1, k2, k3 };
    memcpy(words + 4, k, sizeof(k));
    return address;
}

/// The graphics side: shaders, the fields' targets cleared to nought, the presented images, the
/// scratch every frame reads and the memory the judged frames are copied into.
static bool BringUp(char *why, size_t why_size) {
    size_t at = 0;
    for (uint32_t r = 0; r < FLUID_READS; ++r) {
        read_offset[r] = at;
        at += FluidReadFloats(&GRID, (FluidRead)r) * sizeof(float);
    }
    region_bytes = (at + 255u) & ~(size_t)255u;

    GpuSpec spec;
    GpuSpecDefaults(&spec, DYE_W, DYE_H);
    spec.code_size = 64u * 1024u;
    spec.data_size = (uint32_t)(region_bytes * judged_count) + 1024u * 1024u;
    spec.image_size = 8u * 1024u * 1024u;
    spec.command_size = 512u * 1024u;
    if (!GpuOpenSpec(&gpu, &spec)) {
        snprintf(why, why_size, "the graphics device: %s", gpu.failure);
        return false;
    }
    const struct {
        DkShader *shader;
        ShaderId id;
    } SHADERS[] = {
        { &position_vs, SHADER_POSITION_VERT },     { &common_vs, SHADER_COMMON_VERT },
        { &splat_fs, SHADER_FLUID_SPLAT_FRAG },     { &divergence_fs, SHADER_FLUID_DIVERGENCE_FRAG },
        { &jacobi_fs, SHADER_FLUID_JACOBI_FRAG },   { &gradient_fs, SHADER_FLUID_GRADIENT_FRAG },
        { &advect_fs, SHADER_FLUID_ADVECT_FRAG },   { &display_fs, SHADER_FLUID_DISPLAY_FRAG },
    };
    for (size_t i = 0; i < sizeof(SHADERS) / sizeof(SHADERS[0]); ++i) {
        if (!GpuLoadShader(&gpu, SHADERS[i].shader, ShaderData(SHADERS[i].id),
                           ShaderSize(SHADERS[i].id))) {
            snprintf(why, why_size, "the shaders: %s", gpu.failure);
            return false;
        }
    }
    static const DkImageFormat FORMATS[FIELDS] = {
        DkImageFormat_RG32_Float, DkImageFormat_RG32_Float, DkImageFormat_R32_Float,
        DkImageFormat_R32_Float,  DkImageFormat_R32_Float,  DkImageFormat_RGBA32_Float,
        DkImageFormat_RGBA32_Float,
    };
    bool images = true;
    for (uint32_t f = 0; f < FIELDS && images; ++f) {
        const bool dye = f == DYE_0 || f == DYE_1;
        images = GpuCreateRenderImage(&gpu, &field_images[f], &field_layouts[f], FORMATS[f],
                                      dye ? DYE_W : FLUID_W, dye ? DYE_H : FLUID_H,
                                      DkMsMode_1x);
    }
    for (uint32_t i = 0; i < SWAP_IMAGES && images; ++i) {
        images = GpuCreateImage(&gpu, &swap_images[i], &swap_layouts[i],
                                DkImageFormat_RGBA8_Unorm, SWAP_WIDTH, SWAP_HEIGHT,
                                DkImageFlags_UsageRender | DkImageFlags_UsagePresent |
                                    DkImageFlags_HwCompression,
                                0, NULL);
    }
    if (!images) {
        snprintf(why, why_size, "the fields' targets and the presented images: %s", gpu.failure);
        return false;
    }

    why[0] = '\0';
    for (uint32_t s = 0; s < SLOTS; ++s) {
        force_cpu[s] = (SplatBlock *)Scratch(DK_UNIFORM_BUF_ALIGNMENT, DK_UNIFORM_BUF_ALIGNMENT,
                                             &force_gpu[s], "the force splats", why, why_size);
        dye_cpu[s] = (SplatBlock *)Scratch(DK_UNIFORM_BUF_ALIGNMENT, DK_UNIFORM_BUF_ALIGNMENT,
                                           &dye_gpu[s], "the dye splats", why, why_size);
    }
    float *const corners = (float *)Scratch(3u * 16u, 256u, &triangle, "the passes' triangle",
                                            why, why_size);
    GpuVertex *const quad = (GpuVertex *)Scratch(6u * (uint32_t)sizeof(GpuVertex), 256u,
                                                 &shown_quad, "the quad shown", why, why_size);
    DkImageDescriptor *const image_descriptors = (DkImageDescriptor *)Scratch(
        FIELDS * (uint32_t)sizeof(DkImageDescriptor), DK_IMAGE_DESCRIPTOR_ALIGNMENT,
        &image_set, "the image descriptors", why, why_size);
    DkSamplerDescriptor *const sampler_descriptors = (DkSamplerDescriptor *)Scratch(
        2u * (uint32_t)sizeof(DkSamplerDescriptor), DK_SAMPLER_DESCRIPTOR_ALIGNMENT, &sampler_set,
        "the sampler descriptors", why, why_size);
    reads_cpu = (uint8_t *)Scratch((uint32_t)(region_bytes * judged_count), 256u, &reads_gpu,
                                   "the judged frames' copies", why, why_size);
    // The velocity's step and offset from a field texel to a velocity cell, and the field's
    // texels a cell (demo_fluidref.c's FluidAdvectPass works out the same).
    const float step = 1.0f / (float)FLUID_DYE_SCALE;
    const float half_step = step * 0.5f;
    const float offset = half_step - 0.5f;
    grid_block = Block(FLUID_W, FLUID_H, 0.0f, 0.0f, 0.0f, 0.0f, why, why_size);
    jacobi_first = Block(FLUID_W, FLUID_H, FLUID_PRESSURE_KEEP, 0.0f, 0.0f, 0.0f, why, why_size);
    jacobi_rest = Block(FLUID_W, FLUID_H, 1.0f, 0.0f, 0.0f, 0.0f, why, why_size);
    advect_velocity = Block(FLUID_W, FLUID_H, FLUID_VELOCITY_KEEP, 1.0f, 0.0f, 1.0f, why, why_size);
    advect_dye = Block(DYE_W, DYE_H, FLUID_DYE_KEEP, step, offset, (float)FLUID_DYE_SCALE, why,
                       why_size);
    if (why[0] != '\0') {
        return false;
    }

    // One triangle over the whole target, so no pixel lies on an edge between two.
    const float covering[3][4] = { { -1.0f, -1.0f, 0.0f, 1.0f },
                                   { 3.0f, -1.0f, 0.0f, 1.0f },
                                   { -1.0f, 3.0f, 0.0f, 1.0f } };
    memcpy(corners, covering, sizeof(covering));
    static const GpuVertex SHOWN[6] = {
        { -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
        { 1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
        { -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
        { 1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
        { -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    };
    memcpy(quad, SHOWN, sizeof(SHOWN));

    DkImageView view;
    for (uint32_t f = 0; f < FIELDS; ++f) {
        dkImageViewDefaults(&view, &field_images[f]);
        dkImageDescriptorInitialize(&image_descriptors[f], &view, false, false);
    }
    // Sampler 0 for the passes' texel fetches, which filter nothing; 1 for the dye shown.
    for (uint32_t s = 0; s < 2u; ++s) {
        DkSampler sampler;
        dkSamplerDefaults(&sampler);
        sampler.minFilter = s == 0u ? DkFilter_Nearest : DkFilter_Linear;
        sampler.magFilter = s == 0u ? DkFilter_Nearest : DkFilter_Linear;
        sampler.mipFilter = DkMipFilter_None;
        sampler.wrapMode[0] = DkWrapMode_ClampToEdge;
        sampler.wrapMode[1] = DkWrapMode_ClampToEdge;
        sampler.wrapMode[2] = DkWrapMode_ClampToEdge;
        dkSamplerDescriptorInitialize(&sampler_descriptors[s], &sampler);
    }

    // Every field starts at nought: the velocity still, no pressure, no dye.
    GpuBeginPass(&gpu);
    for (uint32_t f = 0; f < FIELDS; ++f) {
        const bool dye = f == DYE_0 || f == DYE_1;
        Target(f, dye ? DYE_W : FLUID_W, dye ? DYE_H : FLUID_H);
        dkCmdBufClearColorFloat(gpu.cmdbuf, 0, DkColorMask_RGBA, 0.0f, 0.0f, 0.0f, 0.0f);
    }
    GpuSubmit(&gpu);
    velocity_at = 0;
    pressure_at = 0;
    dye_at = 0;
    if (!gpu.ready) {
        snprintf(why, why_size, "clearing the fields: %s", gpu.failure);
        return false;
    }
    return true;
}

static Result StartThread(Thread *thread, ThreadFunc entry, int priority, int core) {
    Result rc = threadCreate(thread, entry, NULL, NULL, STACK_SIZE, priority, core);
    if (R_SUCCEEDED(rc)) {
        rc = threadStart(thread);
        if (R_FAILED(rc)) {
            threadClose(thread);
        }
    }
    return rc;
}

/// The main thread's part of frame @p k: the script's splats into @p slot's blocks.
static void WriteSlot(uint32_t k, uint32_t slot) {
    FluidSplat force[FLUID_SPLATS_MAX];
    FluidSplat dye[FLUID_SPLATS_MAX];
    const uint32_t count = FluidScript(&GRID, k, force, dye);
    memset(force_cpu[slot], 0, sizeof(SplatBlock));
    memset(dye_cpu[slot], 0, sizeof(SplatBlock));
    force_cpu[slot]->count[0] = count;
    dye_cpu[slot]->count[0] = count;
    memcpy(force_cpu[slot]->splats, force, count * sizeof(FluidSplat));
    memcpy(dye_cpu[slot]->splats, dye, count * sizeof(FluidSplat));
}

/// Runs the frames: starts the render thread, drives the loop, and waits for it. False with the
/// reason when the thread did not start, and then no frame ran.
static bool RunFrames(char *why, size_t why_size) {
    memset(&render, 0, sizeof(render));
    semaphoreInit(&frame_go, 0);
    semaphoreInit(&slot_free, SLOTS);
    __atomic_store_n(&last_frame, frames, __ATOMIC_RELEASE);
    ran_frames = frames;
    const Result rc = StartThread(&render_thread, RenderMain, RENDER_PRIORITY, RENDER_CORE);
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
    for (uint32_t k = 0; k < frames; ++k) {
        semaphoreWait(&slot_free);
        // Looked at once a frame, before the frame is written: a person who pressed B has the
        // render thread stop where it is, with what it drew so far.
        if (stop_asked != NULL && stop_asked()) {
            ran_frames = k;
            __atomic_store_n(&last_frame, k, __ATOMIC_RELEASE);
            semaphoreSignal(&frame_go);
            break;
        }
        const uint64_t began = armGetSystemTick();
        WriteSlot(k, k % SLOTS);
        main_us[k] = Microseconds(armGetSystemTick() - began);
        semaphoreSignal(&frame_go);
    }
    threadWaitForExit(&render_thread);
    threadClose(&render_thread);
    if (pinned) {
        const Result back = svcSetThreadCoreMask(CUR_THREAD_HANDLE, ideal, (u32)mask);
        if (R_FAILED(back)) {
            ReportNote("fluid: the main thread could not be put back on core %d, mask 0x%llx: "
                       "0x%x, so it stays on core %d alone",
                       (int)ideal, (unsigned long long)mask, (unsigned)back, MAIN_CORE);
        }
    }
    return true;
}

// The judge's own fields, worked out after the frames: the whole simulation for the early frames,
// and a frame's passes for FluidJudgeFrame.
typedef struct {
    float *velocity;
    float *pressure;
    float *dye;
    FluidWork work;
} Fields;

static bool MakeFields(Fields *out) {
    memset(out, 0, sizeof(*out));
    const size_t v = FluidVelocityFloats(&GRID);
    const size_t p = FluidPressureFloats(&GRID);
    const size_t d = FluidDyeFloats(&GRID);
    float **const slots[] = { &out->velocity,          &out->pressure,
                              &out->dye,               &out->work.f.splatted,
                              &out->work.f.divergence, &out->work.f.pressure,
                              &out->work.f.projected,  &out->work.f.velocity,
                              &out->work.f.dyed,       &out->work.f.dye,
                              &out->work.scratch };
    const size_t sizes[] = { v, p, d, v, p, p, v, v, d, d, p };
    bool made = true;
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        *slots[i] = (float *)calloc(sizes[i], sizeof(float));
        made = made && *slots[i] != NULL;
    }
    return made;
}

static void FreeFields(Fields *fields) {
    float *const all[] = { fields->velocity,          fields->pressure,
                           fields->dye,               fields->work.f.splatted,
                           fields->work.f.divergence, fields->work.f.pressure,
                           fields->work.f.projected,  fields->work.f.velocity,
                           fields->work.f.dyed,       fields->work.f.dye,
                           fields->work.scratch };
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); ++i) {
        free(all[i]);
    }
}

/// Where float @p index of a field of @p kind is.
static void Where(char *out, size_t out_size, uint32_t index, FluidKind kind) {
    const uint32_t channels = kind == FLUID_KIND_VELOCITY ? FLUID_VELOCITY_CHANNELS
                              : kind == FLUID_KIND_DYE    ? FLUID_DYE_CHANNELS
                                                          : 1u;
    const uint32_t width = kind == FLUID_KIND_DYE ? DYE_W : FLUID_W;
    const uint32_t texel = index / channels;
    snprintf(out, out_size, "texel (%u, %u) channel %u", (unsigned)(texel % width),
             (unsigned)(texel / width), (unsigned)(index % channels));
}

/// The dye's colour as the picture shows it: each channel's 1 - e^(-1.5 x), cut to a byte.
static void Shade(const float *texel, uint8_t out[4]) {
    for (uint32_t c = 0; c < 3u; ++c) {
        const float x = texel[c] > 0.0f ? texel[c] : 0.0f;
        const float lit = 1.0f - expf(-1.5f * x);
        out[c] = (uint8_t)(lit * 255.0f + 0.5f);
    }
    out[3] = 0xFF;
}

/// The picture of a dye a judge found off, opened for the case that reports next: the window of
/// PICTURE_MAX_DIMENSION texels a side around its first float off, what the machine left against
/// what it should have, a texel a pixel.
static uint8_t picture_got[PICTURE_MAX_DIMENSION * PICTURE_MAX_DIMENSION * 4u];

static void ShowDye(const float *got, const float *want, uint32_t first, float absolute,
                    float relative) {
    const uint32_t side = PICTURE_MAX_DIMENSION < DYE_W ? PICTURE_MAX_DIMENSION : DYE_W;
    const uint32_t tall = PICTURE_MAX_DIMENSION < DYE_H ? PICTURE_MAX_DIMENSION : DYE_H;
    const uint32_t texel = first / FLUID_DYE_CHANNELS;
    const uint32_t fx = texel % DYE_W;
    const uint32_t fy = texel / DYE_W;
    const uint32_t x0 = fx < side / 2u ? 0u : fx - side / 2u > DYE_W - side ? DYE_W - side
                                                                             : fx - side / 2u;
    const uint32_t y0 = fy < tall / 2u ? 0u : fy - tall / 2u > DYE_H - tall ? DYE_H - tall
                                                                             : fy - tall / 2u;
    for (uint32_t y = 0; y < tall; ++y) {
        for (uint32_t x = 0; x < side; ++x) {
            Shade(got + ((size_t)(y0 + y) * DYE_W + x0 + x) * 4u,
                  picture_got + ((size_t)y * side + x) * 4u);
        }
    }
    Picture *const picture = PictureBegin(side, tall, PICTURE_COLOUR, picture_got);
    for (uint32_t y = 0; y < tall && picture != NULL; ++y) {
        for (uint32_t x = 0; x < side; ++x) {
            const size_t at = ((size_t)(y0 + y) * DYE_W + x0 + x) * 4u;
            FluidDiff diff;
            FluidCompare(got + at, want + at, 4u, absolute, relative, &diff);
            uint8_t expected[4];
            Shade(want + at, expected);
            PictureSet(picture, x, y, expected, diff.off == 0u ? PICTURE_MATCHED : PICTURE_WRONG);
        }
    }
}

/// Moves the early judge's simulation on to the end of frame @p last.
static void SimulateTo(Fields *sim, uint32_t *done, uint32_t last) {
    for (; *done <= last; ++*done) {
        FluidStep(FLUID_SEPARATE, &GRID, *done, sim->velocity, sim->pressure, sim->dye,
                  &sim->work.f, sim->work.scratch);
        memcpy(sim->velocity, sim->work.f.velocity, FluidVelocityFloats(&GRID) * sizeof(float));
        memcpy(sim->pressure, sim->work.f.pressure, FluidPressureFloats(&GRID) * sizeof(float));
        memcpy(sim->dye, sim->work.f.dye, FluidDyeFloats(&GRID) * sizeof(float));
    }
}

/// The early frames against the whole simulation from frame 0, the dye and the velocity each
/// leaves; the picture is the first frame off's dye.
static void JudgeEarly(const char *name, Fields *sim) {
    uint32_t done = 0;
    uint32_t off_frames = 0;
    uint32_t off_frame = 0;
    FluidRead off_read = FLUID_READ_DYE;
    FluidDiff off = { 0 };
    float farthest = 0.0f;
    uint32_t farthest_frame = 0;
    for (uint32_t i = 0; i < judged_count && judged[i] <= FLUID_EARLY_LAST; ++i) {
        SimulateTo(sim, &done, judged[i]);
        const FluidRead reads[2] = { FLUID_READ_DYE, FLUID_READ_VELOCITY };
        const float *const wants[2] = { sim->dye, sim->velocity };
        bool counted = false;
        for (uint32_t r = 0; r < 2u; ++r) {
            FluidDiff diff;
            FluidCompare(ReadOf(i, reads[r]), wants[r], FluidReadFloats(&GRID, reads[r]),
                         FLUID_EARLY_ABSOLUTE, 0.0f, &diff);
            if (diff.farthest > farthest) {
                farthest = diff.farthest;
                farthest_frame = judged[i];
            }
            if (diff.off == 0u || counted) {
                continue;
            }
            counted = true;
            if (off_frames++ == 0u) {
                off_frame = judged[i];
                off_read = reads[r];
                off = diff;
                if (reads[r] == FLUID_READ_DYE) {
                    ShowDye(ReadOf(i, FLUID_READ_DYE), sim->dye, diff.first, FLUID_EARLY_ABSOLUTE,
                            0.0f);
                }
            }
        }
    }
    if (off_frames == 0u) {
        ReportCase(SUITE, name, CHECK_ULP, true, NULL);
        return;
    }
    char where[64];
    Where(where, sizeof(where), off.first, FluidReadKind(off_read));
    char detail[400];
    snprintf(detail, sizeof(detail),
             "%u early frames off; the first, frame %u, in %s: %u floats past 2^-10, the first "
             "at %s, %.9g where %.9g; the farthest %.9g, in frame %u%s",
             (unsigned)off_frames, (unsigned)off_frame, FluidReadName(off_read),
             (unsigned)off.off, where, (double)off.first_got, (double)off.first_want,
             (double)farthest, (unsigned)farthest_frame,
             off_read == FLUID_READ_DYE ? " (its picture is that frame's dye around that texel)"
                                        : "");
    ReportCase(SUITE, name, CHECK_ULP, false, detail);
}

/// What the judged frames came to on one check: how many were off, the first and the farthest.
typedef struct {
    uint32_t frames_off;
    uint32_t first;
    uint32_t first_index;
    FluidVerdict verdict;
    double worst;
    uint32_t worst_frame;
} Tally;

static void Count(Tally *tally, FluidCheck check, uint32_t index, const FluidVerdict *verdict) {
    const double measure = FluidCheckOutput(check) != FLUID_READS ? verdict->diff.farthest
                           : check == FLUID_CHECK_SANE           ? verdict->wrong_reads
                                                                 : verdict->measure;
    if (!(measure <= tally->worst)) {
        tally->worst = measure;
        tally->worst_frame = judged[index];
    }
    if (!verdict->passed && tally->frames_off++ == 0u) {
        tally->first = judged[index];
        tally->first_index = index;
        tally->verdict = *verdict;
    }
}

/// Every judged frame held to every check, each check's case reported.
static void JudgeFrames(const char *const names[FLUID_CHECKS], Fields *fields) {
    static double budgets[FLUID_JUDGED_MAX];
    FluidDyeBudgets(&GRID, judged, judged_count, budgets);
    Tally tallies[FLUID_CHECKS];
    memset(tallies, 0, sizeof(tallies));
    for (uint32_t i = 0; i < judged_count; ++i) {
        FluidFrame frame;
        frame.frame = judged[i];
        for (uint32_t r = 0; r < FLUID_READS; ++r) {
            frame.read[r] = ReadOf(i, (FluidRead)r);
        }
        FluidVerdict verdicts[FLUID_CHECKS];
        FluidJudgeFrame(&GRID, &frame, budgets[i], &fields->work, verdicts);
        for (uint32_t c = 0; c < FLUID_CHECKS; ++c) {
            Count(&tallies[c], (FluidCheck)c, i, &verdicts[c]);
        }
    }
    for (uint32_t c = 0; c < FLUID_CHECKS; ++c) {
        const Tally *const t = &tallies[c];
        if (!ReportStart(SUITE, names[c])) {
            continue;
        }
        if (t->frames_off == 0u) {
            ReportCase(SUITE, names[c], CHECK_ULP, true, NULL);
            continue;
        }
        const FluidVerdict *const v = &t->verdict;
        const FluidRead output = FluidCheckOutput((FluidCheck)c);
        char detail[480];
        if (output != FLUID_READS) {
            if (c == FLUID_CHECK_ADVECT_DYE) {
                // Worked out again for the picture: the judge's fields hold the last frame's.
                FluidAdvectPass(FLUID_SEPARATE, &GRID, ReadOf(t->first_index, FLUID_READ_PROJECTED),
                                FLUID_DYE_SCALE, FLUID_DYE_CHANNELS,
                                ReadOf(t->first_index, FLUID_READ_DYED), FLUID_DYE_KEEP,
                                fields->work.f.dye);
                ShowDye(ReadOf(t->first_index, FLUID_READ_DYE), fields->work.f.dye,
                        v->diff.first, FLUID_PASS_ABSOLUTE, FLUID_PASS_RELATIVE);
            }
            char where[64];
            Where(where, sizeof(where), v->diff.first, FluidReadKind(output));
            snprintf(detail, sizeof(detail),
                     "%u of %u judged frames off; the first, frame %u: %u floats of %s past "
                     "2^-12 and 2^-16 of the value, the first at %s, %.9g where %.9g; the "
                     "farthest %.9g, in frame %u%s",
                     (unsigned)t->frames_off, (unsigned)judged_count, (unsigned)t->first,
                     (unsigned)v->diff.off, FluidReadName(output), where,
                     (double)v->diff.first_got, (double)v->diff.first_want, t->worst,
                     (unsigned)t->worst_frame,
                     c == FLUID_CHECK_ADVECT_DYE
                         ? " (its picture is that frame's dye around that texel)"
                         : "");
        } else if (c == FLUID_CHECK_PROJECTION) {
            snprintf(detail, sizeof(detail),
                     "%u of %u judged frames left more; the first, frame %u: a root mean square "
                     "divergence of %.6g before the projection and %.6g after it; the most, %.4g "
                     "of it, in frame %u",
                     (unsigned)t->frames_off, (unsigned)judged_count, (unsigned)t->first,
                     v->before, v->after, t->worst, (unsigned)t->worst_frame);
        } else if (c == FLUID_CHECK_MASS) {
            snprintf(detail, sizeof(detail),
                     "%u of %u judged frames off; the first, frame %u: %.6g of dye where the "
                     "splats put in %.6g less the dissipation; the farthest %.4g of it, in "
                     "frame %u",
                     (unsigned)t->frames_off, (unsigned)judged_count, (unsigned)t->first,
                     v->after, v->before, t->worst, (unsigned)t->worst_frame);
        } else {
            const bool nan = v->sane.not_finite > 0u;
            const uint32_t at = nan ? v->sane.not_finite_at : v->sane.negative_at;
            snprintf(detail, sizeof(detail),
                     "%u of %u judged frames gone wrong; the first, frame %u, in %u of its %u "
                     "fields, the first %s: %u floats not finite and %u dye below nought, the "
                     "first float %u, %.9g",
                     (unsigned)t->frames_off, (unsigned)judged_count, (unsigned)t->first,
                     (unsigned)v->wrong_reads, (unsigned)FLUID_READS,
                     FluidReadName(v->first_read), (unsigned)v->sane.not_finite,
                     (unsigned)v->sane.negative, (unsigned)at,
                     (double)ReadOf(t->first_index, v->first_read)[at]);
        }
        ReportCase(SUITE, names[c], CHECK_ULP, false, detail);
    }
}

static GameTimeCase time_cases[GAME_TIME_CASES];

/// The timed cases and the notes, as the game scene's: frame 0 left out as the warm-up.
static void Times(void) {
    static GameTimes times;
    GameTimesOf(&times, ran_frames, frame_us, main_us, render_us, sorted_us);
    // The names keep the frames asked (time_cases has them): a run the browser's B stopped is
    // still the same run's line, its frames said in the detail.
    static GameTimeCase measured[GAME_TIME_CASES];
    const uint32_t timed = GameTimeCases(&times, ran_frames, measured);
    for (uint32_t i = 0; i < GAME_TIME_CASES; ++i) {
        time_cases[i].us = measured[i].us;
    }
    char detail[160];
    snprintf(detail, sizeof(detail), "%u frames past the warm-up; %u of %u presented",
             (unsigned)timed, (unsigned)render.presented, (unsigned)ran_frames);
    for (uint32_t i = 0; i < GAME_TIME_CASES; ++i) {
        if (timed == 0u) {
            ReportSkip(SUITE, time_cases[i].name, REPORT_REASON_DEPENDS,
                       "the run's one frame is the warm-up, which the times leave out");
        } else if (ReportStart(SUITE, time_cases[i].name)) {
            ReportCaseTime(SUITE, time_cases[i].name, CHECK_NONE, render.presented == frames,
                           detail, time_cases[i].us);
        }
    }
    char note[REPORT_NOTE_MAX + 1];
    FluidDescribeFrames(note, sizeof(note), &times);
    ReportNote("%s", note);
    FluidDescribeParts(note, sizeof(note), &times);
    ReportNote("%s", note);
}

/// Every case after the bring-up, skipped as depending on what did not happen: @p why.
static void SkipFrom(const char *const names[], size_t count, const char *why) {
    for (size_t i = 0; i < count; ++i) {
        ReportSkip(SUITE, names[i], REPORT_REASON_DEPENDS, why);
    }
    for (uint32_t i = 0; timing && i < GAME_TIME_CASES; ++i) {
        ReportSkip(SUITE, time_cases[i].name, REPORT_REASON_DEPENDS, why);
    }
}

/// The scene, for the frames and the timing this run asks.
static void RunScene(void) {
    static const char BRING_UP[] = "a graphics device, the fields' targets, the shaders and a way "
                                   "to the display come up";
    judged_count = FluidJudgedFrames(frames, judged);
    uint32_t early = 0;
    for (uint32_t i = 0; i < judged_count && judged[i] <= FLUID_EARLY_LAST; ++i) {
        early = judged[i];
    }
    static char frames_name[128];
    snprintf(frames_name, sizeof(frames_name),
             "%u frames, frames 0 to %u's dye and velocity within 2^-10 of the processor's whole "
             "simulation",
             (unsigned)frames, (unsigned)early);
    static char names[FLUID_CHECKS][128];
    // Each at most 127 characters with the count before it: the report keeps no more of a name.
    static const char *const WHAT[FLUID_CHECKS] = {
        "the force splats worked out again from each one's own velocity",
        "the dye splats worked out again from each one's own dye",
        "the divergence worked out again from each one's own splatted velocity",
        "the 40 Jacobi passes worked out again from each one's own pressure and divergence",
        "the gradient worked out again from each one's own velocity and pressure",
        "the velocity advection worked out again from each one's own projected velocity",
        "the dye advection worked out again from each one's own dye and projected velocity",
        "the projection leaving at most half the divergence it was given",
        "the dye's mass within 25% of what the splats put in less the dissipation",
        "every field a finite number and the dye never below nought",
    };
    _Static_assert(FLUID_JACOBI == 40u, "the Jacobi case's name says how many passes");
    const char *all[FLUID_CHECKS + 1u];
    all[0] = frames_name;
    for (uint32_t i = 0; i < FLUID_CHECKS; ++i) {
        snprintf(names[i], sizeof(names[i]), "%u judged frames, %s", (unsigned)judged_count,
                 WHAT[i]);
        all[i + 1u] = names[i];
    }
    (void)GameTimeCases(NULL, frames, time_cases);
    const size_t after_count = sizeof(all) / sizeof(all[0]) - 1u;

    if (!ReportStart(SUITE, BRING_UP)) {
        SkipFrom(all, sizeof(all) / sizeof(all[0]),
                 "the scene's bring-up did not run, so neither did its frames");
        return;
    }
    char why[400] = "";
    bool up = BringUp(why, sizeof(why));
    if (up) {
        // The window is the program's screen's while it is up, and stays so, as the game
        // scene's does.
        on_screen = ViewerScreenUp();
        if (on_screen) {
            ReportNote("fluid: the program's screen is up, so the frames are shown through it, "
                       "copied out by the processor, rather than through a swapchain; present to "
                       "present is that screen's, not a quick run's");
        } else {
            const DkImage *const presented[SWAP_IMAGES] = { &swap_images[0], &swap_images[1] };
            dkSwapchainMakerDefaults(&swap_maker, gpu.device, nwindowGetDefault(), presented,
                                     SWAP_IMAGES);
            uint32_t aborted = 0;
            up = AbortGuarded(MakeSwapchain, &aborted) && swapchain != NULL;
            if (!up) {
                snprintf(why, sizeof(why), "the swapchain on the default window: 0x%x",
                         (unsigned)aborted);
            }
        }
    }
    CHECK_TRUE(SUITE, BRING_UP, CHECK_NONE, up, "%s", why);

    if (!up) {
        SkipFrom(all, sizeof(all) / sizeof(all[0]),
                 "the scene did not come up, so its frames did not run");
    } else if (ReportStart(SUITE, frames_name)) {
        const bool ran = RunFrames(why, sizeof(why));
        Fields sim;
        Fields work;
        const bool room = MakeFields(&sim) && MakeFields(&work);
        // A run the browser's B stopped is judged on the judged frames it reached.
        const uint32_t planned = judged_count;
        while (judged_count > 0u && judged[judged_count - 1u] >= ran_frames) {
            --judged_count;
        }
        if (ran && ran_frames < frames) {
            char left[160] = "";
            for (uint32_t i = judged_count; i < planned; ++i) {
                const size_t used = strlen(left);
                snprintf(left + used, sizeof(left) - used, "%s%u", used > 0u ? ", " : "",
                         (unsigned)judged[i]);
            }
            ReportNote("fluid: stopped at frame %u of %u by the browser's B; the judged frames "
                       "past it are left out: %s",
                       (unsigned)ran_frames, (unsigned)frames, left);
        }
        if (ran && ran_frames == 0u) {
            ReportSkip(SUITE, frames_name, REPORT_REASON_ASKED,
                       "stopped at frame 0 by the browser's B, before any frame ran");
            for (uint32_t i = 0; i < after_count; ++i) {
                ReportSkip(SUITE, all[i + 1u], REPORT_REASON_ASKED,
                           "stopped at frame 0 by the browser's B, before any frame ran");
            }
        } else if (!ran || render.presented < ran_frames || !room) {
            char detail[400];
            snprintf(detail, sizeof(detail), "%s",
                     !ran   ? why
                     : room ? "" : "no memory for the judge's own fields");
            if (ran && room) {
                snprintf(detail, sizeof(detail), "%u of %u frames were presented; %s",
                         (unsigned)render.presented, (unsigned)ran_frames,
                         gpu.failure[0] != '\0' ? gpu.failure
                                                : "the graphics queue reported no fault");
            }
            ReportCase(SUITE, frames_name, CHECK_NONE, false, detail);
            SkipFrom(all + 1, after_count, "the frames did not all run, so there is nothing whole "
                                           "to judge");
        } else {
            JudgeEarly(frames_name, &sim);
            JudgeFrames(all + 1, &work);
            if (render.wrong_core > 0u) {
                ReportNote("fluid: the render thread ran on core %u in %u of %u frames, the first "
                           "%u, where it was made on core %d",
                           (unsigned)render.core_seen, (unsigned)render.wrong_core,
                           (unsigned)ran_frames, (unsigned)render.wrong_core_at, RENDER_CORE);
            }
            if (timing) {
                Times();
            }
        }
        FreeFields(&sim);
        FreeFields(&work);
    } else {
        SkipFrom(all + 1, after_count,
                 "the frames' case did not run, so there are no frames to judge");
    }

    if (swapchain != NULL) {
        dkQueueWaitIdle(gpu.queue);
        dkSwapchainDestroy(swapchain);
        swapchain = NULL;
    }
    GpuClose(&gpu);
}

void FluidDemo(const DemoRun *run) {
    frames = run->frames >= 1u && run->frames <= MODE_FRAMES_MAX ? run->frames
                                                                  : MODE_FRAMES_DEFAULT;
    timing = run->timing;
    stop_asked = run->stopped;
    RunScene();
}

void RunFluid(void) {
    const DemoRun run = DemoScriptRun();
    FluidDemo(&run);
}
