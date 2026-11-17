// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <switch.h>

#include "../check.h"
#include "../demo.h"
#include "../fractalref.h"
#include "../gpu.h"
#include "../mode.h"
#include "../picture.h"
#include "../report.h"
#include "../shaders.h"
#include "../viewer.h"
#include "tests.h"

// The demo family's fractal zooms, drawn on deko3d: a Mandelbrot and a Julia zoom, each frame a
// dispatch that counts every pixel's iterations into a storage buffer and a draw that colours
// the target from them (fractalref.h says the two passes as any backend issues them; Record
// below is deko3d's). Every frame is presented, through a swapchain on the default window, or
// through the program's screen while one is up, and timed. The judged frames' counts and
// colours are copied aside by the graphics unit as the frames run and judged once they are
// over, so judging takes no frame's time.

#define SWAP_WIDTH 1280u
#define SWAP_HEIGHT 720u
#define SWAP_IMAGES 2u
// The frame's uniform block, written by the processor into one of two slots a frame.
#define UNIFORM_SIZE 1280u
#define UNIFORM_SLOTS 2u
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

static uint32_t present_us[MODE_FRAMES_MAX];
static uint32_t work_us[MODE_FRAMES_MAX];
static uint32_t sorted_us[MODE_FRAMES_MAX];

/// What a run of the frames came to.
static struct {
    uint32_t ran;
    uint32_t presented;
    bool stopped;
    /// How often the person paused it, and for how long in all.
    uint32_t pauses;
    uint64_t paused_us;
} frames_seen;

static uint32_t Microseconds(uint64_t ticks) {
    const uint64_t us = armTicksToNs(ticks) / 1000u;
    return us > UINT32_MAX ? UINT32_MAX : (uint32_t)us;
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

/// Which judged frame @p k is, or FRACTAL_JUDGED when it is none.
static uint32_t JudgedIndex(uint32_t k) {
    for (uint32_t j = 0; j < FRACTAL_JUDGED; ++j) {
        if (FractalJudgedFrame(j) == k) {
            return j;
        }
    }
    return FRACTAL_JUDGED;
}

/// Runs @p run's frames of zoom @p kind, until they are over, the person stops them or the
/// queue faults.
static void RunFrames(FractalKind kind, const DemoRun *run) {
    memset(&frames_seen, 0, sizeof(frames_seen));
    uint64_t previous = armGetSystemTick();
    for (uint32_t k = 0; k < run->frames && gpu.ready; ++k) {
        if (run->stopped != NULL && run->stopped()) {
            frames_seen.stopped = true;
            break;
        }
        // Paused after a frame was shown: the last one shown again until the person goes on, and
        // the next frame's time counted from then.
        if (k > 0u && run->paused != NULL && run->paused()) {
            const uint64_t from = armGetSystemTick();
            ++frames_seen.pauses;
            while (gpu.ready && run->paused() && (run->stopped == NULL || !run->stopped())) {
                ShowAgain();
            }
            frames_seen.paused_us += Microseconds(armGetSystemTick() - from);
            previous += armGetSystemTick() - from;
            if (run->stopped != NULL && run->stopped()) {
                frames_seen.stopped = true;
                break;
            }
        }
        const int image = on_screen ? 0 : dkQueueAcquireImage(gpu.queue, swapchain);
        const uint64_t began = armGetSystemTick();
        const uint32_t slot = k % UNIFORM_SLOTS;
        FractalFrameOf(kind, k, uniform_cpu[slot]);
        GpuBeginPass(&gpu);
        Record(slot, JudgedIndex(k), image);
        GpuSubmit(&gpu);
        const uint64_t worked = armGetSystemTick() - began;
        // Presented whatever the submit came to: an image taken and never queued back would
        // leave the swapchain one short for the destroy that follows a fault.
        if (!on_screen) {
            dkQueuePresentImage(gpu.queue, swapchain, image);
        }
        const bool shown =
            gpu.ready &&
            (!on_screen ||
             ViewerShowImage((const uint8_t *)dkMemBlockGetCpuAddr(gpu.read_memory),
                             FRACTAL_WIDTH, FRACTAL_HEIGHT));
        frames_seen.presented += shown ? 1u : 0u;
        // After the present was queued: the wait for the display is in the next frame's acquire.
        const uint64_t now = armGetSystemTick();
        present_us[k] = Microseconds(now - previous);
        work_us[k] = Microseconds(worked);
        previous = now;
        ++frames_seen.ran;
    }
}

/// The verdict on judged frame @p j, frame @p frame of zoom @p kind: its counts and its image
/// against the pinned hashes, and, only when one differs, against the frame worked out here.
static void JudgeFrame(FractalKind kind, const char *suite, const char *name, uint32_t j,
                       uint32_t frame) {
    const uint32_t *const got_counts = (const uint32_t *)kept_counts[j];
    const uint8_t *const got_image = kept_image[j];
    const uint32_t counts_hash = FractalHash(got_counts, COUNTS_BYTES);
    const uint32_t image_hash = FractalHash(got_image, FRACTAL_IMAGE_BYTES);
    const uint32_t pinned_counts = FractalPinnedCounts(kind, j);
    const uint32_t pinned_image = FractalPinnedImage(kind, j);
    if (counts_hash == pinned_counts && image_hash == pinned_image) {
        // A run that asks for every case's picture gets this frame's too, every pixel matched.
        uint8_t *const same = PictureWritesEveryCase() ? (uint8_t *)malloc(FRACTAL_IMAGE_BYTES)
                                                       : NULL;
        uint8_t *const matched = same != NULL ? (uint8_t *)calloc(FRACTAL_PIXELS, 1u) : NULL;
        if (same != NULL && matched != NULL) {
            memcpy(same, got_image, FRACTAL_IMAGE_BYTES);
            (void)PictureBeginOwn(FRACTAL_WIDTH, FRACTAL_HEIGHT, PICTURE_COLOUR, got_image, same,
                                  matched);
        }
        ReportCase(suite, name, CHECK_EXACT, true, "");
        free(same);
        free(matched);
        return;
    }

    // Worked out on this processor, for what differs and the picture; and whether it gives the
    // pinned answer here, since a judge that does not would be blamed on the graphics unit.
    static FractalFrame block;
    FractalFrameOf(kind, frame, &block);
    uint32_t *const want = (uint32_t *)malloc(COUNTS_BYTES);
    uint8_t *const expected = (uint8_t *)malloc(FRACTAL_IMAGE_BYTES);
    uint8_t *const verdicts = (uint8_t *)malloc(FRACTAL_PIXELS);
    if (want == NULL || expected == NULL || verdicts == NULL) {
        char detail[240];
        snprintf(detail, sizeof(detail),
                 "counts hash 0x%08x, pinned 0x%08x; image hash 0x%08x, pinned 0x%08x; no memory "
                 "to work the frame out",
                 (unsigned)counts_hash, (unsigned)pinned_counts, (unsigned)image_hash,
                 (unsigned)pinned_image);
        ReportCase(suite, name, CHECK_EXACT, false, detail);
        free(want);
        free(expected);
        free(verdicts);
        return;
    }
    FractalCounts(&block, want);
    FractalImage(&block, want, expected);
    const uint32_t want_counts_hash = FractalHash(want, COUNTS_BYTES);
    const uint32_t want_image_hash = FractalHash(expected, FRACTAL_IMAGE_BYTES);
    FractalCountsVerdict verdict;
    FractalJudgeCounts(&block, got_counts, want, &verdict);
    uint32_t wrong_pixels = 0;
    for (uint32_t i = 0; i < FRACTAL_PIXELS; ++i) {
        const bool same = memcmp(got_image + (size_t)i * 4u, expected + (size_t)i * 4u, 4u) == 0;
        verdicts[i] = (uint8_t)(same ? PICTURE_MATCHED : PICTURE_WRONG);
        wrong_pixels += same ? 0u : 1u;
    }
    (void)PictureBeginOwn(FRACTAL_WIDTH, FRACTAL_HEIGHT, PICTURE_COLOUR, got_image, expected,
                          verdicts);

    char counted[200] = "every count is the processor's";
    if (verdict.wrong > 0u) {
        snprintf(counted, sizeof(counted),
                 "%u of %u counts differ, the first at %u,%u: %u for %u; %u of them what a "
                 "machine that multiplies and adds apart counts, %u one iteration off",
                 (unsigned)verdict.wrong, (unsigned)FRACTAL_PIXELS, (unsigned)verdict.first_x,
                 (unsigned)verdict.first_y, (unsigned)verdict.first_got,
                 (unsigned)verdict.first_want, (unsigned)verdict.apart,
                 (unsigned)verdict.off_by_one);
    }
    char judge[160] = "";
    if (want_counts_hash != pinned_counts || want_image_hash != pinned_image) {
        snprintf(judge, sizeof(judge),
                 "; this processor works the frame out to 0x%08x and 0x%08x, not the pinned "
                 "0x%08x and 0x%08x: the judge differs here, not only the graphics unit",
                 (unsigned)want_counts_hash, (unsigned)want_image_hash, (unsigned)pinned_counts,
                 (unsigned)pinned_image);
    }
    char detail[REPORT_NOTE_MAX + 1];
    snprintf(detail, sizeof(detail), "%s; %u of %u pixels another colour (its picture)%s", counted,
             (unsigned)wrong_pixels, (unsigned)FRACTAL_PIXELS, judge);
    ReportCase(suite, name, CHECK_EXACT, false, detail);
    free(want);
    free(expected);
    free(verdicts);
}

static void Close(void) {
    if (swapchain != NULL) {
        dkQueueWaitIdle(gpu.queue);
        dkSwapchainDestroy(swapchain);
        swapchain = NULL;
    }
    GpuClose(&gpu);
}

/// One zoom from the bring-up to its times; every case after one that did not happen skipped as
/// depending on it, saying why.
static void RunZoom(FractalKind kind, const DemoRun *run) {
    const char *const suite = FractalSuite(kind);
    static const char BRING_UP[] = "a graphics device with compute, the programs and a way to "
                                   "the display come up";
    char frames_name[96];
    snprintf(frames_name, sizeof(frames_name), "%u frames of the zoom run, every one presented",
             (unsigned)run->frames);
    char judged_names[FRACTAL_JUDGED][96];
    for (uint32_t j = 0; j < FRACTAL_JUDGED; ++j) {
        snprintf(judged_names[j], sizeof(judged_names[j]),
                 "frame %u draws the counts and colours the processor works out",
                 (unsigned)FractalJudgedFrame(j));
    }
    static DemoTimeCase time_cases[DEMO_TIME_CASES];
    (void)DemoTimeCases(NULL, run->frames, time_cases);

    char why[400] = "";
    bool up = false;
    if (ReportStart(suite, BRING_UP)) {
        up = BringUp(why, sizeof(why));
        CHECK_TRUE(suite, BRING_UP, CHECK_NONE, up, "%s", why);
    } else {
        snprintf(why, sizeof(why), "the bring-up did not run");
    }
    bool ran = false;
    if (up && ReportStart(suite, frames_name)) {
        RunFrames(kind, run);
        ran = frames_seen.ran > 0u;
        char detail[400];
        char paused[96] = "";
        if (frames_seen.pauses > 0u) {
            snprintf(paused, sizeof(paused), ", paused %u times for %llu ms out of the times",
                     (unsigned)frames_seen.pauses,
                     (unsigned long long)(frames_seen.paused_us / 1000u));
        }
        snprintf(detail, sizeof(detail), "%u of %u frames ran, %u presented%s%s; %s",
                 (unsigned)frames_seen.ran, (unsigned)run->frames,
                 (unsigned)frames_seen.presented, paused,
                 frames_seen.stopped ? ", then stopped" : "",
                 gpu.failure[0] != '\0' ? gpu.failure : "the graphics queue reported no fault");
        CHECK_TRUE(suite, frames_name, CHECK_NONE,
                   frames_seen.presented == frames_seen.ran &&
                       (frames_seen.ran == run->frames || frames_seen.stopped),
                   "%s", detail);
        static FractalFrame deepest;
        FractalFrameOf(kind, FRACTAL_ZOOM_FRAMES - 1u, &deepest);
        ReportNote("%s: the deepest frame's step is %.1f units in the last place of the farthest "
                   "coordinate a pixel reaches; single precision holds the zoom to there, and a "
                   "deeper one wants perturbation or double precision",
                   suite, FractalStepUlps(&deepest));
    } else if (up) {
        snprintf(why, sizeof(why), "the frames' case did not run, so there are no frames");
    } else {
        ReportSkip(suite, frames_name, REPORT_REASON_DEPENDS,
                   "the zoom did not come up, so its frames did not run");
    }

    for (uint32_t j = 0; j < FRACTAL_JUDGED; ++j) {
        const uint32_t frame = FractalJudgedFrame(j);
        if (!ran) {
            ReportSkip(suite, judged_names[j], REPORT_REASON_DEPENDS,
                       up ? why : "the zoom did not come up, so its frames did not run");
        } else if (frame >= frames_seen.ran) {
            char detail[160];
            if (frames_seen.stopped) {
                snprintf(detail, sizeof(detail), "stopped at frame %u by the person watching",
                         (unsigned)frames_seen.ran);
            } else {
                snprintf(detail, sizeof(detail), "the run's %u frames ended before frame %u%s",
                         (unsigned)frames_seen.ran, (unsigned)frame,
                         gpu.ready ? "" : ", at a fault of the graphics queue");
            }
            ReportSkip(suite, judged_names[j],
                       frames_seen.stopped ? REPORT_REASON_ASKED : REPORT_REASON_DEPENDS, detail);
        } else if (ReportStart(suite, judged_names[j])) {
            JudgeFrame(kind, suite, judged_names[j], j, frame);
        }
    }

    if (run->timing) {
        static DemoTimes times;
        if (ran) {
            DemoTimesOf(&times, frames_seen.ran, present_us, work_us, sorted_us);
        }
        const uint32_t timed = ran ? DemoTimeCases(&times, run->frames, time_cases) : 0u;
        char detail[160];
        snprintf(detail, sizeof(detail), "%u frames past the warm-up; %u of %u presented",
                 (unsigned)timed, (unsigned)frames_seen.presented, (unsigned)frames_seen.ran);
        for (uint32_t i = 0; i < DEMO_TIME_CASES; ++i) {
            if (!ran) {
                ReportSkip(suite, time_cases[i].name, REPORT_REASON_DEPENDS,
                           up ? why : "the zoom did not come up, so its frames did not run");
            } else if (timed == 0u) {
                ReportSkip(suite, time_cases[i].name, REPORT_REASON_DEPENDS,
                           "the run's one frame is the warm-up, which the times leave out");
            } else if (ReportStart(suite, time_cases[i].name)) {
                ReportCaseTime(suite, time_cases[i].name, CHECK_NONE,
                               frames_seen.presented == frames_seen.ran, detail,
                               time_cases[i].us);
            }
        }
        if (ran) {
            char note[REPORT_NOTE_MAX + 1];
            DemoDescribeTimes(note, sizeof(note), suite, &times);
            ReportNote("%s", note);
        }
    }
    Close();
}

void RunMandelbrotDemo(const DemoRun *run) {
    RunZoom(FRACTAL_MANDELBROT, run);
}

void RunJuliaDemo(const DemoRun *run) {
    RunZoom(FRACTAL_JULIA, run);
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
