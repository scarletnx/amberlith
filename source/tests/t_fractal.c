// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <switch.h>

#include "../check.h"
#include "../demo.h"
#include "../fractalref.h"
#include "../fractalzoom.h"
#include "../gpu.h"
#include "../mode.h"
#include "../picture.h"
#include "../report.h"
#include "../shaders.h"
#include "../viewer.h"
#include "tests.h"

// The demo family's fractal zooms' way through deko3d (fractalzoom.h): each frame a dispatch
// that counts every pixel's iterations into a storage buffer and a draw that colours the target
// from them, presented through a swapchain on the default window, or through the program's
// screen while one is up.

#define SWAP_WIDTH 1280u
#define SWAP_HEIGHT 720u
#define SWAP_IMAGES 2u
// The frame's uniform block, written by the processor into one of two slots a frame.
#define UNIFORM_SIZE 1280u
#define UNIFORM_SLOTS FRACTAL_SLOTS
#define COUNTS_BYTES (FRACTAL_PIXELS * 4u)

_Static_assert(FRACTAL_FRAME_BYTES <= UNIFORM_SIZE, "a frame's block fits its slot");
_Static_assert(FRACTAL_WIDTH * 2u == SWAP_WIDTH && FRACTAL_HEIGHT * 2u == SWAP_HEIGHT,
               "the target shown at twice its size fills the presented image");

static GpuContext gpu;
static DkShader fractal_cs;
static DkShader fullscreen_vs;
static DkShader fractal_fs;
static DkShader common_vs;
static DkShader texture_fs;
static DkImage swap_images[SWAP_IMAGES];
static DkImageLayout swap_layouts[SWAP_IMAGES];
static DkSwapchainMaker swap_maker;
static DkSwapchain swapchain;
static bool on_screen;
static FractalFrame *uniform_cpu[UNIFORM_SLOTS];
static DkGpuAddr uniform_gpu[UNIFORM_SLOTS];
static DkGpuAddr counts;
static DkGpuAddr image_set;
static DkGpuAddr sampler_set;
static DkGpuAddr compose_quad;
// What each judged frame left, copied aside by the graphics unit in that frame's list.
static const uint8_t *kept_counts[FRACTAL_JUDGED];
static DkGpuAddr kept_counts_gpu[FRACTAL_JUDGED];
static const uint8_t *kept_image[FRACTAL_JUDGED];
static DkGpuAddr kept_image_gpu[FRACTAL_JUDGED];

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

static void MakeSwapchain(void) {
    swapchain = dkSwapchainCreate(&swap_maker);
}

/// The device with a compute queue, the programs, the counts, the judged frames' copies, and a
/// way to the display: the program's screen when it is up, a swapchain otherwise.
static bool BringUp(char *why, size_t why_size) {
    GpuSpec spec;
    GpuSpecDefaults(&spec, FRACTAL_WIDTH, FRACTAL_HEIGHT);
    spec.compute = true;
    spec.code_size = 64u * 1024u;
    spec.data_size = 14u * 1024u * 1024u;
    spec.image_size = 10u * 1024u * 1024u;
    if (!GpuOpenSpec(&gpu, &spec)) {
        snprintf(why, why_size, "the graphics device: %s", gpu.failure);
        return false;
    }
    const bool loaded =
        GpuLoadShader(&gpu, &fractal_cs, ShaderData(SHADER_FRACTAL_COMP),
                      ShaderSize(SHADER_FRACTAL_COMP)) &&
        GpuLoadShader(&gpu, &fullscreen_vs, ShaderData(SHADER_FULLSCREEN_VERT),
                      ShaderSize(SHADER_FULLSCREEN_VERT)) &&
        GpuLoadShader(&gpu, &fractal_fs, ShaderData(SHADER_FRACTAL_FRAG),
                      ShaderSize(SHADER_FRACTAL_FRAG)) &&
        GpuLoadShader(&gpu, &common_vs, ShaderData(SHADER_COMMON_VERT),
                      ShaderSize(SHADER_COMMON_VERT)) &&
        GpuLoadShader(&gpu, &texture_fs, ShaderData(SHADER_TEXTURE_FRAG),
                      ShaderSize(SHADER_TEXTURE_FRAG));
    if (!loaded) {
        snprintf(why, why_size, "the programs: %s", gpu.failure);
        return false;
    }
    bool images = true;
    for (uint32_t i = 0; i < SWAP_IMAGES && images; ++i) {
        images = GpuCreateImage(&gpu, &swap_images[i], &swap_layouts[i],
                                DkImageFormat_RGBA8_Unorm, SWAP_WIDTH, SWAP_HEIGHT,
                                DkImageFlags_UsageRender | DkImageFlags_UsagePresent |
                                    DkImageFlags_HwCompression,
                                0, NULL);
    }
    if (!images) {
        snprintf(why, why_size, "the presented images: %s", gpu.failure);
        return false;
    }

    why[0] = '\0';
    for (uint32_t s = 0; s < UNIFORM_SLOTS; ++s) {
        uniform_cpu[s] = (FractalFrame *)Scratch(UNIFORM_SIZE, DK_UNIFORM_BUF_ALIGNMENT,
                                                 &uniform_gpu[s], "a frame's uniform block", why,
                                                 why_size);
    }
    (void)Scratch(COUNTS_BYTES, 256u, &counts, "the counts", why, why_size);
    for (uint32_t j = 0; j < FRACTAL_JUDGED; ++j) {
        kept_counts[j] = (const uint8_t *)Scratch(COUNTS_BYTES, 256u, &kept_counts_gpu[j],
                                                  "a judged frame's counts", why, why_size);
        kept_image[j] = (const uint8_t *)Scratch(FRACTAL_IMAGE_BYTES, 256u, &kept_image_gpu[j],
                                                 "a judged frame's image", why, why_size);
    }
    GpuVertex *const quad = (GpuVertex *)Scratch(6u * (uint32_t)sizeof(GpuVertex), 256u,
                                                 &compose_quad, "the quad shown", why, why_size);
    DkImageDescriptor *const image_descriptor = (DkImageDescriptor *)Scratch(
        (uint32_t)sizeof(DkImageDescriptor), DK_IMAGE_DESCRIPTOR_ALIGNMENT, &image_set,
        "the image descriptor", why, why_size);
    DkSamplerDescriptor *const sampler_descriptor = (DkSamplerDescriptor *)Scratch(
        (uint32_t)sizeof(DkSamplerDescriptor), DK_SAMPLER_DESCRIPTOR_ALIGNMENT, &sampler_set,
        "the sampler descriptor", why, why_size);
    if (why[0] != '\0') {
        return false;
    }

    // The target over the whole presented image, two pixels a texel each way, sampled nearest:
    // every pixel centre a quarter of a texel from the texel's edge.
    static const float CORNERS[6][4] = { { -1, 1, 0, 0 }, { 1, 1, 1, 0 },  { 1, -1, 1, 1 },
                                         { -1, 1, 0, 0 }, { 1, -1, 1, 1 }, { -1, -1, 0, 1 } };
    for (uint32_t i = 0; i < 6u; ++i) {
        memset(&quad[i], 0, sizeof(quad[i]));
        quad[i].x = CORNERS[i][0];
        quad[i].y = CORNERS[i][1];
        quad[i].u = CORNERS[i][2];
        quad[i].v = CORNERS[i][3];
        quad[i].r = quad[i].g = quad[i].b = quad[i].a = 1.0f;
    }
    DkImageView view;
    dkImageViewDefaults(&view, &gpu.image);
    dkImageDescriptorInitialize(image_descriptor, &view, false, false);
    DkSampler sampler;
    dkSamplerDefaults(&sampler);
    sampler.minFilter = DkFilter_Nearest;
    sampler.magFilter = DkFilter_Nearest;
    sampler.mipFilter = DkMipFilter_None;
    sampler.wrapMode[0] = DkWrapMode_ClampToEdge;
    sampler.wrapMode[1] = DkWrapMode_ClampToEdge;
    sampler.wrapMode[2] = DkWrapMode_ClampToEdge;
    dkSamplerDescriptorInitialize(sampler_descriptor, &sampler);

    // The window is the program's screen's while it is up, and stays so, as the game family's
    // scene leaves it.
    on_screen = ViewerScreenUp();
    if (!on_screen) {
        const DkImage *const presented[SWAP_IMAGES] = { &swap_images[0], &swap_images[1] };
        dkSwapchainMakerDefaults(&swap_maker, gpu.device, nwindowGetDefault(), presented,
                                 SWAP_IMAGES);
        uint32_t aborted = 0;
        if (!AbortGuarded(MakeSwapchain, &aborted) || swapchain == NULL) {
            snprintf(why, why_size, "the swapchain on the default window: 0x%x",
                     (unsigned)aborted);
            return false;
        }
    }
    return true;
}

/// Records frame @p k's two passes from uniform slot @p slot into the open list, then the copies
/// the judge reads when it is judged frame @p judged (FRACTAL_JUDGED when it is none), then the
/// target onto presented image @p image or into the readback the screen shows.
static void Record(uint32_t slot, uint32_t judged, int image) {
    DkCmdBuf const list = gpu.cmdbuf;
    // The processor rewrote the slot after the graphics unit last read it, through memory the
    // unit caches in its level two cache: what was there is dropped. The previous frame's wait for
    // idle wrote everything it drew back first.
    dkCmdBufBarrier(list, DkBarrier_None, DkInvalidateFlags_L2Cache);

    // The counts: one invocation a pixel.
    const DkShader *const compute[] = { &fractal_cs };
    dkCmdBufBindShaders(list, DkStageFlag_Compute, compute, 1);
    dkCmdBufBindUniformBuffer(list, DkStage_Compute, 0, uniform_gpu[slot], UNIFORM_SIZE);
    dkCmdBufBindStorageBuffer(list, DkStage_Compute, 0, counts, COUNTS_BYTES);
    dkCmdBufDispatchCompute(list, FRACTAL_GROUPS_X, FRACTAL_GROUPS_Y, 1);
    // Every store done before a fragment loads, as the order suite holds a machine to.
    dkCmdBufBarrier(list, DkBarrier_Primitives, DkInvalidateFlags_Shader);

    // The colours: three vertices, no attributes, over the whole target.
    const DkShader *const graphics[] = { &fullscreen_vs, &fractal_fs };
    dkCmdBufBindShaders(list, DkStageFlag_GraphicsMask, graphics, 2);
    dkCmdBufBindVtxAttribState(list, NULL, 0);
    dkCmdBufBindVtxBufferState(list, NULL, 0);
    dkCmdBufBindUniformBuffer(list, DkStage_Fragment, 0, uniform_gpu[slot], UNIFORM_SIZE);
    dkCmdBufBindStorageBuffer(list, DkStage_Fragment, 0, counts, COUNTS_BYTES);
    dkCmdBufDraw(list, DkPrimitive_Triangles, 3, 1, 0, 0);
    dkCmdBufBarrier(list, DkBarrier_Full, DkInvalidateFlags_Image);

    DkImageView target;
    dkImageViewDefaults(&target, &gpu.image);
    const DkImageRect whole = { 0, 0, 0, FRACTAL_WIDTH, FRACTAL_HEIGHT, 1 };
    if (judged < FRACTAL_JUDGED) {
        const DkCopyBuf aside = { kept_image_gpu[judged], 0, 0 };
        dkCmdBufCopyImageToBuffer(list, &target, &whole, &aside, 0);
        dkCmdBufCopyBuffer(list, counts, kept_counts_gpu[judged], COUNTS_BYTES);
    }
    if (on_screen) {
        const DkCopyBuf readback = { dkMemBlockGetGpuAddr(gpu.read_memory), 0, 0 };
        dkCmdBufCopyImageToBuffer(list, &target, &whole, &readback, 0);
        return;
    }
    GpuSetTarget(&gpu, &swap_images[image], SWAP_WIDTH, SWAP_HEIGHT);
    dkCmdBufBindImageDescriptorSet(list, image_set, 1);
    dkCmdBufBindSamplerDescriptorSet(list, sampler_set, 1);
    dkCmdBufBindTexture(list, DkStage_Fragment, 0, dkMakeTextureHandle(0, 0));
    GpuBindVertices(&gpu, &common_vs, &texture_fs, compose_quad,
                    6u * (uint32_t)sizeof(GpuVertex));
    dkCmdBufDraw(list, DkPrimitive_Triangles, 6, 1, 0, 0);
}

/// The last frame shown again while the demo is paused: the screen's copy of it, or the target,
/// which still holds it, drawn onto the next presented image. No pass runs and no case's copy is
/// made.
static void ShowAgain(void) {
    if (on_screen) {
        (void)ViewerShowImage((const uint8_t *)dkMemBlockGetCpuAddr(gpu.read_memory),
                              FRACTAL_WIDTH, FRACTAL_HEIGHT);
        return;
    }
    const int image = dkQueueAcquireImage(gpu.queue, swapchain);
    GpuBeginPass(&gpu);
    GpuSetTarget(&gpu, &swap_images[image], SWAP_WIDTH, SWAP_HEIGHT);
    dkCmdBufBindImageDescriptorSet(gpu.cmdbuf, image_set, 1);
    dkCmdBufBindSamplerDescriptorSet(gpu.cmdbuf, sampler_set, 1);
    dkCmdBufBindTexture(gpu.cmdbuf, DkStage_Fragment, 0, dkMakeTextureHandle(0, 0));
    GpuBindVertices(&gpu, &common_vs, &texture_fs, compose_quad,
                    6u * (uint32_t)sizeof(GpuVertex));
    dkCmdBufDraw(gpu.cmdbuf, DkPrimitive_Triangles, 6, 1, 0, 0);
    GpuSubmit(&gpu);
    dkQueuePresentImage(gpu.queue, swapchain, image);
}

static void Close(void) {
    if (swapchain != NULL) {
        dkQueueWaitIdle(gpu.queue);
        dkSwapchainDestroy(swapchain);
        swapchain = NULL;
    }
    GpuClose(&gpu);
}

static int Acquire(void) {
    return on_screen ? 0 : dkQueueAcquireImage(gpu.queue, swapchain);
}

static FractalFrame *Block(uint32_t slot) {
    return uniform_cpu[slot];
}

static void Draw(uint32_t slot, uint32_t judged, int image) {
    GpuBeginPass(&gpu);
    Record(slot, judged, image);
    GpuSubmit(&gpu);
}

static bool Present(int image) {
    if (!on_screen) {
        dkQueuePresentImage(gpu.queue, swapchain, image);
        return true;
    }
    return gpu.ready && ViewerShowImage((const uint8_t *)dkMemBlockGetCpuAddr(gpu.read_memory),
                                        FRACTAL_WIDTH, FRACTAL_HEIGHT);
}

static bool Ready(void) {
    return gpu.ready;
}

static const char *Failure(void) {
    return gpu.failure;
}

static const uint8_t *KeptCounts(uint32_t j) {
    return kept_counts[j];
}

static const uint8_t *KeptImage(uint32_t j) {
    return kept_image[j];
}

static const FractalWay DEKO3D = {
    .up = BringUp,
    .acquire = Acquire,
    .block = Block,
    .draw = Draw,
    .present = Present,
    .show_again = ShowAgain,
    .ready = Ready,
    .failure = Failure,
    .kept_counts = KeptCounts,
    .kept_image = KeptImage,
    .close = Close,
};

void RunMandelbrotDemo(const DemoRun *run) {
    FractalZoom(&DEKO3D, FRACTAL_MANDELBROT, run);
}

void RunJuliaDemo(const DemoRun *run) {
    FractalZoom(&DEKO3D, FRACTAL_JULIA, run);
}

void RunMandelbrot(void) {
    const MusicMade before = MusicMadeSoFar();
    const DemoRun run = DemoScriptRun();
    RunMandelbrotDemo(&run);
    MusicJudgeOff(FractalSuite(FRACTAL_MANDELBROT), "while the mandelbrot zoom ran", before);
}

void RunJulia(void) {
    const MusicMade before = MusicMadeSoFar();
    const DemoRun run = DemoScriptRun();
    RunJuliaDemo(&run);
    MusicJudgeOff(FractalSuite(FRACTAL_JULIA), "while the julia zoom ran", before);
}
