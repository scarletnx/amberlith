// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../astcimages.h"
#include "../bcnimages.h"
#include "../check.h"
#include "../coherref.h"
#include "../gpu.h"
#include "../picture.h"
#include "../report.h"
#include "../shaders.h"
#include "tests.h"

// Whole BCn textures, as a game's are: the astcimage suite's question asked of the other family
// of formats a game ships. The bcn suite proves each format and each mode on one block; here the
// astcimage suite's picture (source/bcnimages.h: gradients, hard edges, noise and alpha), encoded
// in every BCn format by the encoders a game's tools use, and twice by tools/bcn-images.py with
// BC7's eight modes and BC6H's fourteen in turn, is sampled texel by texel, and four of them as
// sRGB. An emulator whose host has no BCn textures decodes them itself; one that does samples them
// as they are, with its own GPU's interpolants.
//
// What each texel may be is source/bcnref.c's decode, which the reference suite holds to Pillow's
// and bcdec's. BC1 to BC5 leave the precision of an interpolated value to the implementation: the
// console's texture unit is NVIDIA's (BCN_RULE_NVIDIA), and a texel that reads it is exact. One
// that reads another known GPU's instead, the exact fraction rounded or BPTC's sixty-fourths
// (anything between the lowest and the highest of the three), is within the format's freedom but
// not the console's: the case passes with the bound tier and says how many moved and how far. BC6H
// and BC7 decode one way; an sRGB texel's red, green and blue may land on either byte beside its
// code turned linear, as an astcimage one's.
//
// Then the cost: a 2048x2048 texture of the BC1 blocks and one of the BC7 blocks, each made and
// fetched once at every sixteenth texel (what an upload costs, decoding included where the host
// decodes), and the BC7 one fetched 256 times, each texel once (what sampling it costs).

static const char *const SUITE = "bcnimage";

#define TARGET BCN_IMAGE_SIZE
#define BIG 2048u
#define BIG_STEP (BIG / TARGET)
#define SAMPLING_DRAWS (BIG_STEP * BIG_STEP)
#define UNIFORM_SIZE DK_UNIFORM_BUF_ALIGNMENT
#define CHANNELS (TARGET * TARGET * 4u)

/// fetch_step.frag's uniform block.
typedef struct {
    int32_t size[4];
    int32_t offset[4];
} FetchStep;

/// What each channel of the picture may be read back as, for the picture last judged.
static BcnImageAnswers g_answers;
static const BcnImage *g_answered;

static DkImageFormat FormatFor(const BcnImage *image) {
    switch (image->kind) {
    case BCN_BC1:
        if (!image->alpha) {
            return image->srgb ? DkImageFormat_RGB_BC1_sRGB : DkImageFormat_RGB_BC1;
        }
        return image->srgb ? DkImageFormat_RGBA_BC1_sRGB : DkImageFormat_RGBA_BC1;
    case BCN_BC2:
        return image->srgb ? DkImageFormat_RGBA_BC2_sRGB : DkImageFormat_RGBA_BC2;
    case BCN_BC3:
        return image->srgb ? DkImageFormat_RGBA_BC3_sRGB : DkImageFormat_RGBA_BC3;
    case BCN_BC4:
        return DkImageFormat_R_BC4_Unorm;
    case BCN_BC5:
        return DkImageFormat_RG_BC5_Unorm;
    case BCN_BC6H_UF:
        return DkImageFormat_RGBA_BC6H_UF16_Float;
    case BCN_BC7:
        return image->srgb ? DkImageFormat_RGBA_BC7_Unorm_sRGB : DkImageFormat_RGBA_BC7_Unorm;
    default:
        return DkImageFormat_None;
    }
}

static void Answer(const BcnImage *image) {
    if (g_answered != image) {
        BcnImageAnswer(image, &g_answers);
        g_answered = image;
    }
}

/// How a readback compares with the answers, and the error of what is off and wrong.
typedef struct {
    uint32_t wrong;
    uint32_t worst;
    /// Texels on another GPU's interpolants and not the console's, and the farthest channel.
    uint32_t off;
    uint32_t off_worst;
    uint32_t first_x;
    uint32_t first_y;
    uint8_t got[4];
    uint8_t want[4];
    double squares[ASTC_IMAGE_REGIONS];
} Judged;

/// Judges @p pixels, where pixel (x, y) shows texel (x, y) * step + offset of a texture that
/// repeats the picture, into @p out and a picture of what was expected.
static void Judge(const BcnImage *image, const uint8_t *pixels, uint32_t step, uint32_t ox,
                  uint32_t oy, Judged *out) {
    Answer(image);
    memset(out, 0, sizeof(*out));
    Picture *const picture = PictureBegin(TARGET, TARGET, PICTURE_COLOUR, pixels);
    for (uint32_t y = 0; y < TARGET; ++y) {
        for (uint32_t x = 0; x < TARGET; ++x) {
            const uint32_t tx = (x * step + ox) % TARGET;
            const uint32_t ty = (y * step + oy) % TARGET;
            const uint8_t *const pixel = GpuPixel(pixels, TARGET, x, y);
            const uint32_t base = (ty * TARGET + tx) * 4u;
            uint32_t away = 0;
            const BcnTexelVerdict verdict = BcnImageVerdict(&g_answers, tx, ty, pixel, &away);
            PictureSet(picture, x, y, &g_answers.exact[1][base],
                       verdict == BCN_TEXEL_WRONG ? PICTURE_WRONG : PICTURE_MATCHED);
            if (verdict == BCN_TEXEL_OFF) {
                ++out->off;
                out->off_worst = away > out->off_worst ? away : out->off_worst;
            }
            if (verdict != BCN_TEXEL_WRONG) {
                continue;
            }
            for (uint32_t c = 0; c < 4u; ++c) {
                const int32_t d = (int32_t)pixel[c] - (int32_t)g_answers.exact[1][base + c];
                out->squares[AstcImageRegion(tx, ty)] += (double)d * (double)d;
            }
            if (out->wrong == 0u) {
                out->first_x = tx;
                out->first_y = ty;
                memcpy(out->got, pixel, 4);
                memcpy(out->want, &g_answers.exact[1][base], 4);
            }
            out->worst = away > out->worst ? away : out->worst;
            ++out->wrong;
        }
    }
}

/// PSNR over @p channels channels whose squared errors add to @p squares: "exact" for none.
static void Psnr(char *out, size_t size, double squares, double channels) {
    if (squares <= 0.0) {
        snprintf(out, size, "exact");
        return;
    }
    snprintf(out, size, "%.1f dB", 10.0 * log10(255.0 * 255.0 * channels / squares));
}

static void Report(const char *name, const BcnImage *image, const char *what, const Judged *j) {
    char whole[16];
    char region[ASTC_IMAGE_REGIONS][16];
    double all = 0.0;
    for (uint32_t r = 0; r < ASTC_IMAGE_REGIONS; ++r) {
        all += j->squares[r];
        Psnr(region[r], sizeof(region[r]), j->squares[r], CHANNELS / ASTC_IMAGE_REGIONS);
    }
    Psnr(whole, sizeof(whole), all, CHANNELS);
    char wrong[256] = "";
    if (j->wrong != 0u) {
        snprintf(wrong, sizeof(wrong),
                 "; texel %u,%u of the picture is %02x%02x%02x%02x, the console's "
                 "%02x%02x%02x%02x",
                 (unsigned)j->first_x, (unsigned)j->first_y, j->got[0], j->got[1], j->got[2],
                 j->got[3], j->want[0], j->want[1], j->want[2], j->want[3]);
    }
    char off[160] = "";
    if (j->off != 0u) {
        snprintf(off, sizeof(off),
                 "; %u texels on another GPU's interpolants and not the console's, by at most %u",
                 (unsigned)j->off, (unsigned)j->off_worst);
    }
    const CheckTier tier =
        BcnHasRules(image->kind) && (j->off != 0u || j->wrong != 0u) ? CHECK_BOUND : CHECK_EXACT;
    if (j->wrong == 0u && j->off == 0u) {
        ReportCase(SUITE, name, tier, true, NULL);
        return;
    }
    // A pass within the bound says how far it moved, as a failure says what went wrong.
    char detail[512];
    snprintf(detail, sizeof(detail),
             "%s %s: %u of %u texels wrong, worst channel %u away; PSNR %s (%s %s, %s %s, %s %s, "
             "%s %s)%s%s",
             image->name, what, (unsigned)j->wrong, (unsigned)(TARGET * TARGET),
             (unsigned)j->worst, whole, ASTC_IMAGE_REGION[0], region[0], ASTC_IMAGE_REGION[1],
             region[1], ASTC_IMAGE_REGION[2], region[2], ASTC_IMAGE_REGION[3], region[3], wrong,
             off);
    ReportCase(SUITE, name, tier, j->wrong == 0u, detail);
}

/// Makes a @p size by @p size texture of @p image's blocks, repeated, written by the processor in
/// the block linear layout a driver gives a texture that tall. False with a sentence in @p why.
static bool MakeTexture(GpuContext *gpu, const BcnImage *image, uint32_t size, DkImage *out,
                        char *why, size_t why_size) {
    const DkImageFormat format = FormatFor(image);
    const uint32_t bytes = BcnBlockBytes(image->kind);
    const uint32_t across = size / 4u;
    const uint32_t rows = size / 4u;
    // The smallest block height that holds the rows, at most sixteen gobs.
    uint32_t tile = 0;
    while (tile < 4u && (8u << tile) < rows) {
        ++tile;
    }
    const uint32_t gobs = 1u << tile;
    DkImageLayout layout;
    uint8_t *memory = NULL;
    if (format == DkImageFormat_None ||
        !GpuCreateImage(gpu, out, &layout, format, size, size, DkImageFlags_CustomTileSize, tile,
                        &memory)) {
        snprintf(why, why_size, "%s", gpu->failure[0] != '\0' ? gpu->failure : "no image for it");
        return false;
    }
    const uint32_t row_bytes = across * bytes;
    const uint64_t expected = (uint64_t)((row_bytes + 63u) / 64u) *
                              ((rows + 8u * gobs - 1u) / (8u * gobs)) * gobs * 512u;
    if (dkImageLayoutGetSize(&layout) != expected) {
        snprintf(why, why_size,
                 "the library laid it out in %llu bytes, the block linear rule says %llu",
                 (unsigned long long)dkImageLayoutGetSize(&layout), (unsigned long long)expected);
        return false;
    }
    const uint32_t picture_blocks = TARGET / 4u;
    for (uint32_t by = 0; by < rows; ++by) {
        for (uint32_t bx = 0; bx < across; ++bx) {
            const uint8_t *const block =
                image->blocks +
                bytes * ((by % picture_blocks) * picture_blocks + bx % picture_blocks);
            // A block is 8 or 16 bytes of one 16 byte run of a gob, so its bytes stay together.
            memcpy(memory + CohBlockLinearOffset(bx * bytes, by, row_bytes, gobs), block, bytes);
        }
    }
    return true;
}

/// Draws @p draws quads over the target, the n-th fetching every @p step-th texel of @p image
/// from offset (n % step, n / step), and reads the target back: the last draw's texels.
static const uint8_t *Fetch(GpuContext *gpu, const DkShader *shaders, const DkImage *image,
                            uint32_t size, uint32_t step, uint32_t draws) {
    DkGpuAddr image_set = 0;
    DkGpuAddr sampler_set = 0;
    DkGpuAddr uniforms = 0;
    DkImageDescriptor *const images = (DkImageDescriptor *)GpuAllocData(
        gpu, (uint32_t)sizeof(DkImageDescriptor), DK_IMAGE_DESCRIPTOR_ALIGNMENT, &image_set);
    DkSamplerDescriptor *const samplers = (DkSamplerDescriptor *)GpuAllocData(
        gpu, (uint32_t)sizeof(DkSamplerDescriptor), DK_SAMPLER_DESCRIPTOR_ALIGNMENT, &sampler_set);
    uint8_t *const steps =
        (uint8_t *)GpuAllocData(gpu, UNIFORM_SIZE * draws, DK_UNIFORM_BUF_ALIGNMENT, &uniforms);
    if (images == NULL || samplers == NULL || steps == NULL) {
        return NULL;
    }
    DkImageView view;
    dkImageViewDefaults(&view, image);
    dkImageDescriptorInitialize(&images[0], &view, false, false);
    DkSampler sampler;
    dkSamplerDefaults(&sampler);
    sampler.minFilter = DkFilter_Nearest;
    sampler.magFilter = DkFilter_Nearest;
    sampler.mipFilter = DkMipFilter_None;
    dkSamplerDescriptorInitialize(&samplers[0], &sampler);
    for (uint32_t n = 0; n < draws; ++n) {
        const FetchStep s = { { (int32_t)size, (int32_t)size, (int32_t)step, (int32_t)step },
                              { (int32_t)(n % step), (int32_t)(n / step), 0, 0 } };
        memset(steps + UNIFORM_SIZE * n, 0, UNIFORM_SIZE);
        memcpy(steps + UNIFORM_SIZE * n, &s, sizeof(s));
    }

    static const GpuVertex quad[6] = {
        { -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
        { 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
        { -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
        { -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    };
    GpuClear(gpu, 0.0f, 0.0f, 0.0f, 0.0f);
    GpuBeginPass(gpu);
    dkCmdBufBindImageDescriptorSet(gpu->cmdbuf, image_set, 1);
    dkCmdBufBindSamplerDescriptorSet(gpu->cmdbuf, sampler_set, 1);
    dkCmdBufBindTexture(gpu->cmdbuf, DkStage_Fragment, 0, dkMakeTextureHandle(0, 0));
    if (!GpuBindTriangleState(gpu, &shaders[0], &shaders[1], quad, 6)) {
        return NULL;
    }
    for (uint32_t n = 0; n < draws; ++n) {
        dkCmdBufBindUniformBuffer(gpu->cmdbuf, DkStage_Fragment, 0, uniforms + UNIFORM_SIZE * n,
                                  UNIFORM_SIZE);
        dkCmdBufDraw(gpu->cmdbuf, DkPrimitive_Triangles, 6, 1, 0, 0);
    }
    GpuSubmit(gpu);
    return gpu->ready ? GpuReadBack(gpu) : NULL;
}

static void Fidelity(GpuContext *gpu, const DkShader *shaders, const BcnImage *image) {
    char name[128];
    snprintf(name, sizeof(name), "a picture in %s blocks decodes as the console's, every texel",
             image->name);
    if (!ReportStart(SUITE, name)) {
        return;
    }
    DkImage texture;
    char why[288];
    if (!MakeTexture(gpu, image, TARGET, &texture, why, sizeof(why))) {
        ReportCase(SUITE, name, CHECK_NONE, false, why);
        return;
    }
    const uint8_t *const pixels = Fetch(gpu, shaders, &texture, TARGET, 1u, 1u);
    if (pixels == NULL) {
        ReportCase(SUITE, name, CHECK_NONE, false,
                   gpu->failure[0] != '\0' ? gpu->failure : "the draw or readback did not happen");
        return;
    }
    Judged j;
    Judge(image, pixels, 1u, 0u, 0u, &j);
    Report(name, image, "every texel", &j);
}

/// Makes a 2048x2048 texture of @p image and fetches every sixteenth texel: the upload's cost.
static bool Upload(GpuContext *gpu, const DkShader *shaders, const BcnImage *image,
                   DkImage *texture) {
    char name[128];
    snprintf(name, sizeof(name), "a %ux%u texture of %s blocks decodes all over, every %uth texel",
             BIG, BIG, image->name, BIG_STEP);
    if (!ReportStart(SUITE, name)) {
        return false;
    }
    char why[288];
    if (!MakeTexture(gpu, image, BIG, texture, why, sizeof(why))) {
        ReportCase(SUITE, name, CHECK_NONE, false, why);
        return false;
    }
    const uint8_t *const pixels = Fetch(gpu, shaders, texture, BIG, BIG_STEP, 1u);
    if (pixels == NULL) {
        ReportCase(SUITE, name, CHECK_NONE, false,
                   gpu->failure[0] != '\0' ? gpu->failure : "the draw or readback did not happen");
        return false;
    }
    Judged j;
    Judge(image, pixels, BIG_STEP, 0u, 0u, &j);
    Report(name, image, "every sixteenth texel", &j);
    return true;
}

/// Fetches every texel of the 2048x2048 BC7 texture once, 256 draws of every sixteenth: what
/// sampling it costs, once it is uploaded.
static void Sampling(GpuContext *gpu, const DkShader *shaders, const BcnImage *image,
                     const DkImage *texture, bool made) {
    char name[128];
    snprintf(name, sizeof(name),
             "the %ux%u texture of %s blocks fetched %u times over, each texel once", BIG, BIG,
             image->name, SAMPLING_DRAWS);
    if (!ReportStart(SUITE, name)) {
        return;
    }
    if (!made) {
        ReportCase(SUITE, name, CHECK_NONE, false, "no texture: the case that makes it did not");
        return;
    }
    const uint8_t *const pixels = Fetch(gpu, shaders, texture, BIG, BIG_STEP, SAMPLING_DRAWS);
    if (pixels == NULL) {
        ReportCase(SUITE, name, CHECK_NONE, false,
                   gpu->failure[0] != '\0' ? gpu->failure : "the draws or readback did not happen");
        return;
    }
    Judged j;
    Judge(image, pixels, BIG_STEP, BIG_STEP - 1u, BIG_STEP - 1u, &j);
    Report(name, image, "the last draw's texels", &j);
}

void RunBcnImage(void) {
    GpuSpec spec;
    GpuSpecDefaults(&spec, TARGET, TARGET);
    spec.code_size = 32u * 1024u;
    spec.data_size = UNIFORM_SIZE * SAMPLING_DRAWS + 64u * 1024u;
    // The two large textures (4 MiB of BC7 and 2 MiB of BC1) and the thirteen pictures, each
    // rounded to its layout.
    spec.cpu_image_size = 8u * 1024u * 1024u;
    g_answered = NULL;

    if (!ReportStart(SUITE, "graphics device opens with room for the textures")) {
        return;
    }
    GpuContext gpu;
    const bool opened = GpuOpenSpec(&gpu, &spec);
    CHECK_TRUE(SUITE, "graphics device opens with room for the textures", CHECK_NONE, opened, "%s",
               gpu.failure);
    if (!opened) {
        return;
    }
    if (!ReportStart(SUITE, "the fetching shader loads")) {
        GpuClose(&gpu);
        return;
    }
    DkShader shaders[2];
    const bool loaded = GpuLoadShader(&gpu, &shaders[0], ShaderData(SHADER_COMMON_VERT),
                                      ShaderSize(SHADER_COMMON_VERT)) &&
                        GpuLoadShader(&gpu, &shaders[1], ShaderData(SHADER_FETCH_STEP_FRAG),
                                      ShaderSize(SHADER_FETCH_STEP_FRAG));
    CHECK_TRUE(SUITE, "the fetching shader loads", CHECK_NONE, loaded, "%s", gpu.failure);
    if (!loaded) {
        GpuClose(&gpu);
        return;
    }
    for (uint32_t i = 0; i < BCN_IMAGES; ++i) {
        Fidelity(&gpu, shaders, &BCN_IMAGE[i]);
    }
    // BC1's blocks at half a byte a texel and BC7's at a byte, as the astcimage suite's 8x8 and
    // 4x4 are at a quarter and a byte.
    DkImage big1;
    DkImage big7;
    Upload(&gpu, shaders, &BCN_IMAGE[BCN_IMAGE_BC1], &big1);
    const bool made = Upload(&gpu, shaders, &BCN_IMAGE[BCN_IMAGE_BC7], &big7);
    Sampling(&gpu, shaders, &BCN_IMAGE[BCN_IMAGE_BC7], &big7, made);
    GpuClose(&gpu);
}
