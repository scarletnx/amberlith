// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../check.h"
#include "../drawpath_nvn.h"
#include "../picture.h"
#include "../report.h"
#include "../sceneref.h"
#include "../shaders.h"
#include "tests.h"

// The nvn family's command buffer cases: what a list does past the binds and the draws of the
// scenes (t_nvnscene.c), each asked with NVN's own calls on the command buffer of an NVN way
// (drawpath_nvn.h), and so down the two NVN ways only: the layer's own functions, whose deko3d
// commands the graphics unit answers on every machine, and the NVN the program looks up, which
// under ScarletNX is the NVN its games take. A function here whose name ends in Recorded makes
// those calls; tools/nvn-coverage.py counts them for the looked-up way alone.
//
// A case draws flat quads into the cells of a 64 by 64 target, sixteen pixels a side, each cell
// in a colour of its own, and judges every pixel: a cell holds its colour or the clear colour.
// What a list writes to memory lands in a page of the way's data, which the processor reads.

static const char *const SUITE = "nvn";

#define CELL 16u
#define CELLS_ACROSS (SCENE_SIZE / CELL)
#define CELLS (CELLS_ACROSS * CELLS_ACROSS)
/// What the page holds until a list writes it: no count and no value a case expects.
#define PAGE_PRESET 0xa5u
#define PAGE_BYTES 0x100u

typedef struct {
    float offset[4];
} Move;

typedef struct {
    float color[4];
    int32_t mode[4];
} Paint;

/// What a case draws with: an NVN way and its parts, the scene's program painting each vertex's
/// own colour, a quad for every cell, and the page.
typedef struct {
    DrawPath *path;
    DrawPathNvnParts nvn;
    int program;
    DrawBuffer vertices;
    DrawBuffer move;
    /// The same quads a whole depth range further from the eye: past the far plane.
    DrawBuffer far;
    DrawBuffer paint;
    DrawBuffer page;
    /// Vertices and indices of a case's own.
    DrawBuffer own;
    DrawBuffer indices;
} Kit;

#define OWN_VERTICES 32u

/// Cell @p k's colour: no two cells alike in any channel but alpha, none the clear colour.
static void CellColour(uint32_t k, uint8_t out[4]) {
    out[0] = (uint8_t)(0x30u + k * 12u);
    out[1] = (uint8_t)(0xf0u - k * 10u);
    out[2] = (uint8_t)(0x09u + k * 7u);
    out[3] = 0xffu;
}

static float ClipX(uint32_t pixel) {
    return (float)pixel * 2.0f / (float)SCENE_SIZE - 1.0f;
}

static float ClipY(uint32_t pixel) {
    return 1.0f - (float)pixel * 2.0f / (float)SCENE_SIZE;
}

/// Brings @p kind up with everything a case draws with. False with the case reported as failed.
/// What a kit's depth target is: none, 32 bits of float depth, or 24 of depth with a stencil.
typedef enum {
    KIT_NO_DEPTH,
    KIT_DEPTH,
    KIT_STENCIL,
} KitDepth;

static bool KitOpenOf(Kit *kit, DrawPathKind kind, KitDepth depth, uint32_t textures,
                      uint32_t texture_bytes, const char *name) {
    memset(kit, 0, sizeof(*kit));
    DrawPathSpec spec;
    memset(&spec, 0, sizeof(spec));
    spec.width = SCENE_SIZE;
    spec.height = SCENE_SIZE;
    spec.depth = depth != KIT_NO_DEPTH;
    spec.stencil = depth == KIT_STENCIL;
    spec.textures = textures;
    spec.texture_bytes = texture_bytes;
    spec.programs = 1;
    spec.data_bytes = 0x4000u;
    spec.command_bytes = 0x10000u;
    char why[320] = "";
    char detail[400];
    kit->path = DrawPathOpen(kind, &spec, why, sizeof(why));
    if (kit->path == NULL) {
        snprintf(detail, sizeof(detail), "the way through %s did not come up: %s",
                 DrawPathName(kind), why);
        ReportCase(SUITE, name, CHECK_NONE, false, detail);
        return false;
    }
    kit->program = DrawPathProgram(kit->path, ShaderData(SHADER_SCENE_VERT),
                                   ShaderSize(SHADER_SCENE_VERT), ShaderData(SHADER_SCENE_FRAG),
                                   ShaderSize(SHADER_SCENE_FRAG));
    if (!DrawPathNvn(kit->path, &kit->nvn) || kit->program < 0 ||
        !DrawPathAlloc(kit->path, CELLS * 6u * (uint32_t)sizeof(GpuVertex), &kit->vertices) ||
        !DrawPathAlloc(kit->path, (uint32_t)sizeof(Move), &kit->move) ||
        !DrawPathAlloc(kit->path, (uint32_t)sizeof(Move), &kit->far) ||
        !DrawPathAlloc(kit->path, (uint32_t)sizeof(Paint), &kit->paint) ||
        !DrawPathAlloc(kit->path, OWN_VERTICES * (uint32_t)sizeof(GpuVertex), &kit->own) ||
        !DrawPathAlloc(kit->path, 0x100u, &kit->indices) ||
        !DrawPathAlloc(kit->path, PAGE_BYTES, &kit->page)) {
        snprintf(detail, sizeof(detail), "bringing up what the case draws with through %s: %s",
                 DrawPathName(kind), DrawPathFailure(kit->path));
        ReportCase(SUITE, name, CHECK_NONE, false, detail);
        DrawPathClose(kit->path);
        return false;
    }
    GpuVertex quads[CELLS * 6u];
    for (uint32_t k = 0; k < CELLS; ++k) {
        uint8_t colour[4];
        CellColour(k, colour);
        const uint32_t x0 = k % CELLS_ACROSS * CELL;
        const uint32_t y0 = k / CELLS_ACROSS * CELL;
        const float r = (float)colour[0] / 255.0f;
        const float g = (float)colour[1] / 255.0f;
        const float b = (float)colour[2] / 255.0f;
        const GpuVertex top_left = { ClipX(x0), ClipY(y0), 0.5f, 0.0f, 0.0f, r, g, b, 1.0f };
        const GpuVertex top_right = { ClipX(x0 + CELL), ClipY(y0), 0.5f, 1.0f, 0.0f, r, g, b,
                                      1.0f };
        const GpuVertex bottom_right = { ClipX(x0 + CELL), ClipY(y0 + CELL), 0.5f, 1.0f, 1.0f,
                                         r, g, b, 1.0f };
        const GpuVertex bottom_left = { ClipX(x0), ClipY(y0 + CELL), 0.5f, 0.0f, 1.0f, r, g, b,
                                        1.0f };
        GpuVertex *const out = quads + k * 6u;
        out[0] = top_left;
        out[1] = top_right;
        out[2] = bottom_right;
        out[3] = top_left;
        out[4] = bottom_right;
        out[5] = bottom_left;
    }
    memcpy(kit->vertices.bytes, quads, sizeof(quads));
    const Move still = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    // Mode 1 of the scene's fragment stage: the vertex's own colour.
    const Paint flat = { { 0.0f, 0.0f, 0.0f, 0.0f }, { 1, 0, 0, 0 } };
    const Move beyond = { { 0.0f, 0.0f, 1.0f, 0.0f } };
    memcpy(kit->far.bytes, &beyond, sizeof(beyond));
    memcpy(kit->move.bytes, &still, sizeof(still));
    memcpy(kit->paint.bytes, &flat, sizeof(flat));
    memset(kit->page.bytes, PAGE_PRESET, PAGE_BYTES);
    return true;
}

static bool KitOpenFor(Kit *kit, DrawPathKind kind, bool depth, uint32_t textures,
                       uint32_t texture_bytes, const char *name) {
    return KitOpenOf(kit, kind, depth ? KIT_DEPTH : KIT_NO_DEPTH, textures, texture_bytes, name);
}

static bool KitOpenWith(Kit *kit, DrawPathKind kind, bool depth, const char *name) {
    return KitOpenFor(kit, kind, depth, 0, 0, name);
}

static bool KitOpen(Kit *kit, DrawPathKind kind, const char *name) {
    return KitOpenWith(kit, kind, false, name);
}

/// Starts a list with the program, the quads and the two uniform buffers bound; @p clear also
/// clears the target to the scene's clear colour.
static void KitBegin(Kit *kit, bool clear) {
    DrawPathBegin(kit->path);
    if (clear) {
        const float colour[4] = { (float)SCENE_CLEAR[0] / 255.0f, (float)SCENE_CLEAR[1] / 255.0f,
                                  (float)SCENE_CLEAR[2] / 255.0f,
                                  (float)SCENE_CLEAR[3] / 255.0f };
        DrawPathClear(kit->path, colour);
    }
    DrawPathUseProgram(kit->path, kit->program);
    DrawPathVertices(kit->path, &kit->vertices, CELLS * 6u * (uint32_t)sizeof(GpuVertex));
    DrawPathUniform(kit->path, DRAW_STAGE_VERTEX, 0, &kit->move, sizeof(Move));
    DrawPathUniform(kit->path, DRAW_STAGE_FRAGMENT, 0, &kit->paint, sizeof(Paint));
}

/// Draws cell @p k's quad.
static void KitCell(Kit *kit, uint32_t k) {
    DrawPathDraw(kit->path, k * 6u, 6);
}

/// A 64-bit word of the page, as the processor reads it.
static uint64_t PageWord(const Kit *kit, uint32_t at) {
    uint64_t word;
    memcpy(&word, (const uint8_t *)kit->page.bytes + at, sizeof(word));
    return word;
}

/// What a pixel has to hold.
typedef void (*Expected)(uint32_t x, uint32_t y, uint8_t out[4]);

/// Ends the case: the target read and every pixel judged against @p expected, together with
/// @p why, the case's own finding about what the list wrote to memory ("" for none). Takes the
/// way down.
static void KitJudgeWith(Kit *kit, const char *name, Expected expected, const char *why) {
    const uint8_t *const pixels = DrawPathRead(kit->path);
    if (pixels == NULL || DrawPathFailure(kit->path)[0] != '\0') {
        char detail[400];
        snprintf(detail, sizeof(detail), "%s",
                 DrawPathFailure(kit->path)[0] != '\0' ? DrawPathFailure(kit->path)
                                                        : "the target could not be read");
        ReportCase(SUITE, name, CHECK_NONE, false, detail);
        DrawPathClose(kit->path);
        return;
    }
    Picture *const picture = PictureBegin(SCENE_SIZE, SCENE_SIZE, PICTURE_COLOUR, pixels);
    uint32_t wrong = 0;
    uint32_t first_x = 0;
    uint32_t first_y = 0;
    uint8_t first[4] = { 0, 0, 0, 0 };
    uint8_t first_want[4] = { 0, 0, 0, 0 };
    for (uint32_t y = 0; y < SCENE_SIZE; ++y) {
        for (uint32_t x = 0; x < SCENE_SIZE; ++x) {
            uint8_t want[4];
            expected(x, y, want);
            const uint8_t *const pixel = GpuPixel(pixels, SCENE_SIZE, x, y);
            const bool right = memcmp(pixel, want, 4) == 0;
            PictureSet(picture, x, y, want, right ? PICTURE_MATCHED : PICTURE_WRONG);
            if (!right && wrong++ == 0u) {
                first_x = x;
                first_y = y;
                memcpy(first, pixel, 4);
                memcpy(first_want, want, 4);
            }
        }
    }
    char picture_why[200] = "the picture is right";
    if (wrong != 0u) {
        snprintf(picture_why, sizeof(picture_why),
                 "%u of %u pixels wrong; the first, (%u,%u) in cell %u, holds %02x%02x%02x%02x, "
                 "expected %02x%02x%02x%02x, which is %s",
                 (unsigned)wrong, (unsigned)(SCENE_SIZE * SCENE_SIZE), (unsigned)first_x,
                 (unsigned)first_y, (unsigned)(first_y / CELL * CELLS_ACROSS + first_x / CELL),
                 first[0], first[1], first[2], first[3], first_want[0], first_want[1],
                 first_want[2], first_want[3],
                 memcmp(first_want, SCENE_CLEAR, 4) == 0 ? "the clear colour" : "a cell's colour");
    }
    CHECK_TRUE(SUITE, name, CHECK_EXACT, wrong == 0u && why[0] == '\0', "%s%s%s", why,
               why[0] != '\0' ? "; " : "", picture_why);
    DrawPathClose(kit->path);
}

/// The cells KitJudge expects drawn, a bit a cell.
static uint32_t judged_cells;

static void CellsExpected(uint32_t x, uint32_t y, uint8_t out[4]) {
    const uint32_t k = y / CELL * CELLS_ACROSS + x / CELL;
    if ((judged_cells >> k & 1u) != 0u) {
        CellColour(k, out);
    } else {
        memcpy(out, SCENE_CLEAR, 4);
    }
}

/// KitJudgeWith for a picture of whole cells: a cell of @p drawn (a bit a cell) holds its
/// colour and any other the clear colour.
static void KitJudge(Kit *kit, const char *name, uint32_t drawn, const char *why) {
    judged_cells = drawn;
    KitJudgeWith(kit, name, CellsExpected, why);
}

/// Ends the open list; false with the way's failure in @p why.
static bool KitEnd(Kit *kit, char *why, size_t why_size) {
    if (!DrawPathEnd(kit->path)) {
        snprintf(why, why_size, "%s", DrawPathFailure(kit->path));
        return false;
    }
    return true;
}

// ---- counters

/// A report: the count, then the graphics unit's time when it was taken.
#define REPORT_BYTES 16u

/// Into the open list: the samples counter reset, cell 0 drawn and the counter reported at the
/// page's start, then reset and reported again with nothing drawn between.
static void SamplesRecorded(Kit *kit) {
    const NvnProcs *const n = kit->nvn.nvn;
    n->nvnCommandBufferResetCounter(kit->nvn.command, NVN_COUNTER_TYPE_SAMPLES_PASSED);
    KitCell(kit, 0);
    n->nvnCommandBufferReportCounter(kit->nvn.command, NVN_COUNTER_TYPE_SAMPLES_PASSED,
                                     kit->page.address);
    n->nvnCommandBufferResetCounter(kit->nvn.command, NVN_COUNTER_TYPE_SAMPLES_PASSED);
    n->nvnCommandBufferReportCounter(kit->nvn.command, NVN_COUNTER_TYPE_SAMPLES_PASSED,
                                     kit->page.address + REPORT_BYTES);
}

/// The samples a quad passes are its pixels: one cell, 256 of them, and none after a reset
/// with no draw.
static void SamplesCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[200] = "";
    KitBegin(&kit, true);
    SamplesRecorded(&kit);
    if (KitEnd(&kit, why, sizeof(why))) {
        const uint64_t passed = PageWord(&kit, 0);
        const uint64_t none = PageWord(&kit, REPORT_BYTES);
        if (passed != CELL * CELL || none != 0u) {
            snprintf(why, sizeof(why), "the counter reported 0x%llx samples after a quad of %u "
                     "pixels and 0x%llx after a reset with no draw, where 0 is expected (0x%02x "
                     "in every byte is a report never written)",
                     (unsigned long long)passed, (unsigned)(CELL * CELL),
                     (unsigned long long)none, PAGE_PRESET);
        }
    }
    KitJudge(&kit, name, 1u << 0, why);
}

#define VALUE_FIRST 0x5eed1234u
#define VALUE_SECOND 0x0badf00du

/// Into the open list: two values reported at the page's start, a report's 16 bytes apart.
static void ValuesRecorded(Kit *kit) {
    const NvnProcs *const n = kit->nvn.nvn;
    n->nvnCommandBufferReportValue(kit->nvn.command, VALUE_FIRST, kit->page.address);
    n->nvnCommandBufferReportValue(kit->nvn.command, VALUE_SECOND,
                                   kit->page.address + REPORT_BYTES);
}

/// A value a list reports is the four bytes at its address once the list has run, and nothing
/// lands past a report's 16 bytes. The twelve bytes after the value are not judged: whether a
/// value is written as a word alone or as a report, with a count's upper half and a time, is
/// the driver's, and no console has been asked here.
static void ValuesCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[200] = "";
    KitBegin(&kit, true);
    ValuesRecorded(&kit);
    if (KitEnd(&kit, why, sizeof(why))) {
        const uint32_t first = (uint32_t)PageWord(&kit, 0);
        const uint32_t second = (uint32_t)PageWord(&kit, REPORT_BYTES);
        const uint64_t after = PageWord(&kit, 2u * REPORT_BYTES);
        const uint64_t untouched = 0x0101010101010101ull * PAGE_PRESET;
        if (first != VALUE_FIRST || second != VALUE_SECOND || after != untouched) {
            snprintf(why, sizeof(why), "the page holds the words 0x%08x and, 16 bytes on, "
                     "0x%08x, where 0x%08x and 0x%08x were reported, and 16 bytes further "
                     "0x%016llx, where nothing was (0x%02x in every byte)",
                     (unsigned)first, (unsigned)second, (unsigned)VALUE_FIRST,
                     (unsigned)VALUE_SECOND, (unsigned long long)after, PAGE_PRESET);
        }
    }
    KitJudge(&kit, name, 0u, why);
}

/// Into the open list: the time reported at the page's start, a cell drawn, and the time again.
static void TimesRecorded(Kit *kit) {
    const NvnProcs *const n = kit->nvn.nvn;
    n->nvnCommandBufferReportCounter(kit->nvn.command, NVN_COUNTER_TYPE_TIMESTAMP,
                                     kit->page.address);
    KitCell(kit, 5);
    n->nvnCommandBufferReportCounter(kit->nvn.command, NVN_COUNTER_TYPE_TIMESTAMP,
                                     kit->page.address + REPORT_BYTES);
}

/// The device's clock now, and the times of the list's two reports, in nanoseconds.
static void TimesRead(Kit *kit, uint64_t *now, uint64_t first_second[2]) {
    const NvnProcs *const n = kit->nvn.nvn;
    if (now != NULL) {
        *now = n->nvnDeviceGetCurrentTimestampInNanoseconds(kit->nvn.device);
    }
    if (first_second != NULL) {
        for (uint32_t i = 0; i < 2u; ++i) {
            NVNcounterData report;
            memcpy(&report, (const uint8_t *)kit->page.bytes + i * REPORT_BYTES, sizeof(report));
            first_second[i] = n->nvnDeviceGetTimestampInNanoseconds(kit->nvn.device, &report);
        }
    }
}

/// How a timestamp report is judged: by the clock it was taken on, or by the unit of its word.
typedef enum {
    TIME_ORDER = 0,
    TIME_UNIT = 1,
} TimeKind;

/// TIME_ORDER: a timestamp report holds no count, and its time, which the device turns into
/// nanoseconds, is between the device's clock before the list and after it, the second report
/// no earlier than the first. TIME_UNIT: the word a report holds is the graphics unit's ticks,
/// 384 of them every 625 nanoseconds, which is what the device's nanoseconds come from.
static void TimesCase(DrawPathKind kind, TimeKind judged, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[320] = "";
    uint64_t before = 0;
    uint64_t after = 0;
    uint64_t times[2] = { 0, 0 };
    TimesRead(&kit, &before, NULL);
    KitBegin(&kit, true);
    TimesRecorded(&kit);
    if (KitEnd(&kit, why, sizeof(why))) {
        TimesRead(&kit, &after, times);
        const uint64_t ticks[2] = { PageWord(&kit, 8), PageWord(&kit, REPORT_BYTES + 8u) };
        if (judged == TIME_ORDER) {
            const uint64_t counts[2] = { PageWord(&kit, 0), PageWord(&kit, REPORT_BYTES) };
            if (counts[0] != 0u || counts[1] != 0u || before > times[0] || times[0] > times[1] ||
                times[1] > after) {
                snprintf(why, sizeof(why), "the reports' counts are 0x%llx and 0x%llx, where a "
                         "timestamp's is 0, and in nanoseconds the device's clock was %llu "
                         "before the list, the reports %llu and %llu, and the clock %llu after, "
                         "where each is no earlier than the one before it",
                         (unsigned long long)counts[0], (unsigned long long)counts[1],
                         (unsigned long long)before, (unsigned long long)times[0],
                         (unsigned long long)times[1], (unsigned long long)after);
            }
        } else if (times[0] != ticks[0] * 625u / 384u || times[1] != ticks[1] * 625u / 384u) {
            snprintf(why, sizeof(why), "the reports' words are %llu and %llu, and the device "
                     "turns them into %llu and %llu nanoseconds, where the graphics unit's "
                     "ticks, 384 every 625 nanoseconds, come to %llu and %llu",
                     (unsigned long long)ticks[0], (unsigned long long)ticks[1],
                     (unsigned long long)times[0], (unsigned long long)times[1],
                     (unsigned long long)(ticks[0] * 625u / 384u),
                     (unsigned long long)(ticks[1] * 625u / 384u));
        }
    }
    KitJudge(&kit, name, 1u << 5, why);
}

// ---- the render enable

/// Where the page holds the pairs of counts the render enable compares, each a report and the
/// one 16 bytes on: two the list before reported, two equal counts the processor wrote, with
/// times that differ, and two that differ.
#define PAIR_REPORTED 0u
#define PAIR_EQUAL 0x40u
#define PAIR_APART 0x80u

/// Into the open list, the one before: cell 12 drawn and its 256 samples reported, then no
/// samples reported after a reset.
static void PairRecorded(Kit *kit) {
    const NvnProcs *const n = kit->nvn.nvn;
    n->nvnCommandBufferResetCounter(kit->nvn.command, NVN_COUNTER_TYPE_SAMPLES_PASSED);
    KitCell(kit, 12);
    n->nvnCommandBufferReportCounter(kit->nvn.command, NVN_COUNTER_TYPE_SAMPLES_PASSED,
                                     kit->page.address + PAIR_REPORTED);
    n->nvnCommandBufferResetCounter(kit->nvn.command, NVN_COUNTER_TYPE_SAMPLES_PASSED);
    n->nvnCommandBufferReportCounter(kit->nvn.command, NVN_COUNTER_TYPE_SAMPLES_PASSED,
                                     kit->page.address + PAIR_REPORTED + REPORT_BYTES);
}

/// Into the open list: a cell drawn under each word of the render enable. Cells 1, 2, 4, 6 and
/// 8 are the ones it lets through.
static void EnablesRecorded(Kit *kit) {
    const NvnProcs *const n = kit->nvn.nvn;
    NVNcommandBuffer *const cmd = kit->nvn.command;
    const NVNbufferAddress page = kit->page.address;
    n->nvnCommandBufferSetRenderEnable(cmd, 0);
    KitCell(kit, 0);
    n->nvnCommandBufferSetRenderEnable(cmd, 1);
    KitCell(kit, 1);
    n->nvnCommandBufferSetRenderEnableConditional(
        cmd, NVN_CONDITIONAL_RENDER_MODE_RENDER_IF_EQUAL, page + PAIR_EQUAL);
    KitCell(kit, 2);
    n->nvnCommandBufferSetRenderEnableConditional(
        cmd, NVN_CONDITIONAL_RENDER_MODE_RENDER_IF_EQUAL, page + PAIR_APART);
    KitCell(kit, 3);
    n->nvnCommandBufferSetRenderEnableConditional(
        cmd, NVN_CONDITIONAL_RENDER_MODE_RENDER_IF_NOT_EQUAL, page + PAIR_APART);
    KitCell(kit, 4);
    n->nvnCommandBufferSetRenderEnableConditional(
        cmd, NVN_CONDITIONAL_RENDER_MODE_RENDER_IF_NOT_EQUAL, page + PAIR_EQUAL);
    KitCell(kit, 5);
    n->nvnCommandBufferSetRenderEnableConditional(
        cmd, NVN_CONDITIONAL_RENDER_MODE_RENDER_IF_NOT_EQUAL, page + PAIR_REPORTED);
    KitCell(kit, 6);
    n->nvnCommandBufferSetRenderEnableConditional(
        cmd, NVN_CONDITIONAL_RENDER_MODE_RENDER_IF_EQUAL, page + PAIR_REPORTED);
    KitCell(kit, 7);
    n->nvnCommandBufferSetRenderEnable(cmd, 1);
    KitCell(kit, 8);
}

/// The render enable: off and on, and the two conditions over a pair of counts 16 bytes apart,
/// equal and apart as the processor wrote them and apart as a list that finished before
/// reported them. A cell drawn under a word that holds is there, and one under a word that
/// does not is left clear.
static void EnablesCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[200] = "";
    const uint64_t equal[4] = { 7u, 1u, 7u, 2u };
    const uint64_t apart[4] = { 7u, 1u, 8u, 1u };
    memcpy((uint8_t *)kit.page.bytes + PAIR_EQUAL, equal, sizeof(equal));
    memcpy((uint8_t *)kit.page.bytes + PAIR_APART, apart, sizeof(apart));
    KitBegin(&kit, true);
    PairRecorded(&kit);
    if (KitEnd(&kit, why, sizeof(why))) {
        KitBegin(&kit, false);
        EnablesRecorded(&kit);
        KitEnd(&kit, why, sizeof(why));
    }
    KitJudge(&kit, name, 1u << 1 | 1u << 2 | 1u << 4 | 1u << 6 | 1u << 8 | 1u << 12, why);
}

// ---- the command buffer's memory and its recording

/// Keeps in @p why the first answer that is not the number expected.
static void Holds(char *why, size_t why_size, const char *what, unsigned long long got,
                  unsigned long long want) {
    if (got != want && why[0] == '\0') {
        snprintf(why, why_size, "%s is 0x%llx, where 0x%llx is expected", what, got, want);
    }
}

/// What the command buffer says of its memory, at one moment.
typedef struct {
    size_t command_size, command_used, command_free;
    size_t control_size, control_used, control_free;
    bool recording;
} MemoryAnswers;

/// The command buffer given its memory again, whole, and asked what it holds of it.
static void MemoryGivenRecorded(Kit *kit) {
    const NvnProcs *const n = kit->nvn.nvn;
    n->nvnCommandBufferAddCommandMemory(kit->nvn.command, kit->nvn.command_pool, 0,
                                        kit->nvn.command_bytes);
    n->nvnCommandBufferAddControlMemory(kit->nvn.command, kit->nvn.control,
                                        kit->nvn.control_bytes);
}

static void MemoryAskedRecorded(Kit *kit, MemoryAnswers *out) {
    const NvnProcs *const n = kit->nvn.nvn;
    const NVNcommandBuffer *const cmd = kit->nvn.command;
    out->command_size = n->nvnCommandBufferGetCommandMemorySize(cmd);
    out->command_used = n->nvnCommandBufferGetCommandMemoryUsed(cmd);
    out->command_free = n->nvnCommandBufferGetCommandMemoryFree(cmd);
    out->control_size = n->nvnCommandBufferGetControlMemorySize(cmd);
    out->control_used = n->nvnCommandBufferGetControlMemoryUsed(cmd);
    out->control_free = n->nvnCommandBufferGetControlMemoryFree(cmd);
    out->recording = n->nvnCommandBufferIsRecording(cmd);
}

/// A command buffer's memory as it answers for it: the sizes are the ones it was given; what
/// is used and what is free come to the size, before a list, in the middle of one and after
/// it; four draws take command memory; and it is recording between the list's begin and end
/// and at no other time. How many bytes a command takes is the driver's, and is not asked.
static void MemoryCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[240] = "";
    MemoryAnswers before, during, after;
    memset(&during, 0, sizeof(during));
    memset(&after, 0, sizeof(after));
    MemoryGivenRecorded(&kit);
    MemoryAskedRecorded(&kit, &before);
    KitBegin(&kit, true);
    for (uint32_t k = 0; k < 4u; ++k) {
        KitCell(&kit, k);
    }
    MemoryAskedRecorded(&kit, &during);
    char ended[160] = "";
    if (KitEnd(&kit, ended, sizeof(ended))) {
        MemoryAskedRecorded(&kit, &after);
        const MemoryAnswers *const at[3] = { &before, &during, &after };
        static const char *const when[3] = { "before the list", "in the list", "after the list" };
        for (int i = 0; i < 3; ++i) {
            char what[96];
            snprintf(what, sizeof(what), "%s, the command memory's size", when[i]);
            Holds(why, sizeof(why), what, at[i]->command_size, kit.nvn.command_bytes);
            snprintf(what, sizeof(what), "%s, the command memory used and free together",
                     when[i]);
            Holds(why, sizeof(why), what, at[i]->command_used + at[i]->command_free,
                  kit.nvn.command_bytes);
            snprintf(what, sizeof(what), "%s, the control memory's size", when[i]);
            Holds(why, sizeof(why), what, at[i]->control_size, kit.nvn.control_bytes);
            snprintf(what, sizeof(what), "%s, the control memory used and free together",
                     when[i]);
            Holds(why, sizeof(why), what, at[i]->control_used + at[i]->control_free,
                  kit.nvn.control_bytes);
            snprintf(what, sizeof(what), "%s, whether the command buffer is recording",
                     when[i]);
            Holds(why, sizeof(why), what, at[i]->recording, i == 1);
        }
        if (why[0] == '\0' && during.command_used <= before.command_used) {
            snprintf(why, sizeof(why), "0x%zx bytes of command memory were used before the list "
                     "and 0x%zx after four draws in it, where draws take some",
                     before.command_used, during.command_used);
        }
    } else {
        snprintf(why, sizeof(why), "%s", ended);
    }
    KitJudge(&kit, name, 0xfu, why);
}

/// The first part of the command memory the callback case gives, which no list of sixteen
/// cells a round fits in for long, and how many rounds it draws at most.
#define SMALL_COMMAND_BYTES 0x2000u
#define GROW_ROUNDS 400u

/// What the memory callback was called with the first time, and how often.
static struct {
    Kit *kit;
    uint32_t calls;
    NVNcommandBuffer *cmd;
    NVNcommandBufferMemoryEvent event;
    size_t least;
    void *data;
    bool added;
    uint32_t controls;
} grow;

/// The program's memory callback: the rest of the command memory the first time a list runs
/// out of it, and control memory of its own each time a list runs out of that.
static void Grow(NVNcommandBuffer *cmd, NVNcommandBufferMemoryEvent event, size_t least,
                 void *data) {
    static _Alignas(8) uint8_t more_control[4][0x4000];
    if (grow.calls++ == 0u) {
        grow.cmd = cmd;
        grow.event = event;
        grow.least = least;
        grow.data = data;
    }
    const NvnProcs *const n = grow.kit->nvn.nvn;
    if (event == NVN_COMMAND_BUFFER_MEMORY_EVENT_OUT_OF_COMMAND_MEMORY && !grow.added) {
        grow.added = true;
        n->nvnCommandBufferAddCommandMemory(cmd, grow.kit->nvn.command_pool,
                                            SMALL_COMMAND_BYTES,
                                            grow.kit->nvn.command_bytes - SMALL_COMMAND_BYTES);
    } else if (event == NVN_COMMAND_BUFFER_MEMORY_EVENT_OUT_OF_CONTROL_MEMORY) {
        n->nvnCommandBufferAddControlMemory(cmd, more_control[grow.controls++ % 4u],
                                            sizeof(more_control[0]));
    }
}

/// The command buffer given a small part of its memory and told whom to ask for more.
static void CallbackSetRecorded(Kit *kit, void **callback, void **data) {
    const NvnProcs *const n = kit->nvn.nvn;
    NVNcommandBuffer *const cmd = kit->nvn.command;
    n->nvnCommandBufferAddCommandMemory(cmd, kit->nvn.command_pool, 0, SMALL_COMMAND_BYTES);
    n->nvnCommandBufferSetMemoryCallback(cmd, Grow);
    n->nvnCommandBufferSetMemoryCallbackData(cmd, &grow);
    // Off and on again: the callback is asked while it is on.
    n->nvnCommandBufferSetCommandMemoryCallbackEnabled(cmd, 0);
    n->nvnCommandBufferSetCommandMemoryCallbackEnabled(cmd, 1);
    n->nvnCommandBufferSetControlMemoryCallbackEnabled(cmd, 0);
    n->nvnCommandBufferSetControlMemoryCallbackEnabled(cmd, 1);
    *callback = (void *)(uintptr_t)n->nvnCommandBufferGetMemoryCallback(cmd);
    *data = n->nvnCommandBufferGetMemoryCallbackData(cmd);
}

/// A list that runs out of command memory asks the program's callback, with the command
/// buffer, the event, a number of bytes above none and the program's own pointer, and with
/// the memory the callback adds it draws on: every cell is there. The command buffer gives
/// back the callback and the pointer it was set.
static void CallbackCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[320] = "";
    memset(&grow, 0, sizeof(grow));
    grow.kit = &kit;
    void *callback = NULL;
    void *data = NULL;
    CallbackSetRecorded(&kit, &callback, &data);
    KitBegin(&kit, true);
    uint32_t rounds = 0;
    for (; rounds < GROW_ROUNDS && grow.calls == 0u; ++rounds) {
        for (uint32_t k = 0; k < CELLS; ++k) {
            KitCell(&kit, k);
        }
    }
    if (KitEnd(&kit, why, sizeof(why))) {
        if (callback != (void *)(uintptr_t)Grow || data != (void *)&grow) {
            snprintf(why, sizeof(why), "the command buffer gave back the callback %p and the "
                     "pointer %p, where %p and %p were set",
                     callback, data, (void *)(uintptr_t)Grow, (void *)&grow);
        } else if (grow.calls == 0u) {
            snprintf(why, sizeof(why), "%u draws into 0x%x bytes of command memory never asked "
                     "the callback for more",
                     (unsigned)(rounds * CELLS), (unsigned)SMALL_COMMAND_BYTES);
        } else if (grow.cmd != kit.nvn.command ||
                   grow.event != NVN_COMMAND_BUFFER_MEMORY_EVENT_OUT_OF_COMMAND_MEMORY ||
                   grow.least == 0u || grow.data != (void *)&grow) {
            snprintf(why, sizeof(why), "the callback was first called with the command buffer "
                     "%p, event %d, 0x%zx bytes and the pointer %p, where the command buffer is "
                     "%p, a list out of command memory is event %d and asks for bytes, and the "
                     "pointer set is %p",
                     (void *)grow.cmd, (int)grow.event, grow.least, grow.data,
                     (void *)kit.nvn.command,
                     (int)NVN_COMMAND_BUFFER_MEMORY_EVENT_OUT_OF_COMMAND_MEMORY, (void *)&grow);
        }
    }
    grow.kit = NULL;
    KitJudge(&kit, name, 0xffffu, why);
}

/// Two lists recorded and kept, a cell each, then into the list that is submitted: the first
/// called, the second copied, and a cell of its own.
static bool ListsRecorded(Kit *kit) {
    const NvnProcs *const n = kit->nvn.nvn;
    NVNcommandHandle kept[2] = { 0, 0 };
    static const uint32_t cells[2] = { 3, 6 };
    for (int i = 0; i < 2; ++i) {
        KitBegin(kit, i == 0);
        KitCell(kit, cells[i]);
        kept[i] = n->nvnCommandBufferEndRecording(kit->nvn.command);
    }
    KitBegin(kit, false);
    n->nvnCommandBufferCallCommands(kit->nvn.command, 1, &kept[0]);
    n->nvnCommandBufferCopyCommands(kit->nvn.command, 1, &kept[1]);
    KitCell(kit, 9);
    return kept[0] != 0u && kept[1] != 0u;
}

/// A list called from another and a list copied into it run where they are named: the cell
/// each draws is there with the cell the outer list draws itself, and the first list's clear
/// under all three.
static void ListsCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[200] = "";
    const bool kept = ListsRecorded(&kit);
    if (KitEnd(&kit, why, sizeof(why)) && !kept) {
        snprintf(why, sizeof(why), "nvnCommandBufferEndRecording gave no list for one of the "
                 "two that were kept");
    }
    KitJudge(&kit, name, 1u << 3 | 1u << 6 | 1u << 9, why);
}

// ---- clears, and what a copy and a target keep

/// The side of a case's own textures, and the bytes of one's texels.
#define SMALL 16
#define SMALL_BYTES (SMALL * SMALL * 4)
#define EXTRAS 3
/// The most textures a case's extras hold: the resolves' five.
#define EXTRAS_MOST 5

/// Textures of a case's own, in a pool of their own, and where their texels are read back:
/// taken down before the way that made them.
typedef struct {
    _Alignas(8) NVNmemoryPool pool;
    _Alignas(8) NVNtexture textures[EXTRAS_MOST];
    void *storage;
    bool pool_up;
    int made;
    /// The samples of the first texture, when it is a multisampled one: it is not read back.
    int first_samples;
    DrawBuffer read;
} Extras;

static void ExtrasDown(Kit *kit, Extras *extras) {
    const NvnProcs *const n = kit->nvn.nvn;
    for (int i = 0; i < extras->made; ++i) {
        n->nvnTextureFinalize(&extras->textures[i]);
    }
    extras->made = 0;
    if (extras->pool_up) {
        n->nvnMemoryPoolFinalize(&extras->pool);
        extras->pool_up = false;
    }
    free(extras->storage);
    extras->storage = NULL;
}

/// @p count 16 by 16 textures of @p formats. False with the step in @p why and nothing left.
static bool ExtrasUpOf(Kit *kit, Extras *extras, const NVNformat *formats, int count,
                       int first_samples, char *why, size_t why_size) {
    const NvnProcs *const n = kit->nvn.nvn;
    memset(extras, 0, sizeof(*extras));
    extras->first_samples = first_samples;
    if (!DrawPathAlloc(kit->path, (uint32_t)(count * SMALL_BYTES), &extras->read)) {
        snprintf(why, why_size, "no room to read the case's textures back: %s",
                 DrawPathFailure(kit->path));
        return false;
    }
    memset(extras->read.bytes, PAGE_PRESET, (size_t)(count * SMALL_BYTES));
    const size_t pool_bytes = 0x40000u;
    extras->storage = aligned_alloc(0x1000u, pool_bytes);
    if (extras->storage == NULL) {
        snprintf(why, why_size, "no memory for the pool of the case's textures");
        return false;
    }
    memset(extras->storage, 0, pool_bytes);
    _Alignas(8) NVNmemoryPoolBuilder pool_builder;
    n->nvnMemoryPoolBuilderSetDefaults(&pool_builder);
    n->nvnMemoryPoolBuilderSetDevice(&pool_builder, kit->nvn.device);
    n->nvnMemoryPoolBuilderSetStorage(&pool_builder, extras->storage, pool_bytes);
    n->nvnMemoryPoolBuilderSetFlags(&pool_builder, NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS |
                                                       NVN_MEMORY_POOL_FLAGS_GPU_CACHED);
    extras->pool_up = n->nvnMemoryPoolInitialize(&extras->pool, &pool_builder);
    if (!extras->pool_up) {
        snprintf(why, why_size, "the pool of the case's textures: %s",
                 NvnLayerError()[0] != '\0' ? NvnLayerError() : "false");
        ExtrasDown(kit, extras);
        return false;
    }
    size_t at = 0;
    for (; extras->made < count; ++extras->made) {
        _Alignas(8) NVNtextureBuilder builder;
        memset(&builder, 0, sizeof(builder));
        n->nvnTextureBuilderSetDevice(&builder, kit->nvn.device);
        n->nvnTextureBuilderSetDefaults(&builder);
        if (extras->made == 0 && first_samples > 1) {
            n->nvnTextureBuilderSetTarget(&builder, NVN_TEXTURE_TARGET_2D_MULTISAMPLE);
            n->nvnTextureBuilderSetSamples(&builder, first_samples);
        } else {
            n->nvnTextureBuilderSetTarget(&builder, NVN_TEXTURE_TARGET_2D);
        }
        n->nvnTextureBuilderSetFormat(&builder, formats[extras->made]);
        n->nvnTextureBuilderSetSize2D(&builder, SMALL, SMALL);
        const size_t size = n->nvnTextureBuilderGetStorageSize(&builder);
        const size_t alignment = n->nvnTextureBuilderGetStorageAlignment(&builder);
        if (size != 0u && alignment != 0u) {
            at = (at + alignment - 1u) / alignment * alignment;
        }
        if (size == 0u || alignment == 0u || at + size > pool_bytes) {
            snprintf(why, why_size, "a 16x16 texture of NVN format 0x%x asks for 0x%zx bytes at "
                     "an alignment of 0x%zx: %s",
                     (unsigned)formats[extras->made], size, alignment,
                     NvnLayerError()[0] != '\0' ? NvnLayerError() : "no pool here holds that");
            ExtrasDown(kit, extras);
            return false;
        }
        n->nvnTextureBuilderSetStorage(&builder, &extras->pool, (ptrdiff_t)at);
        if (!n->nvnTextureInitialize(&extras->textures[extras->made], &builder)) {
            snprintf(why, why_size, "a 16x16 texture of NVN format 0x%x: %s",
                     (unsigned)formats[extras->made],
                     NvnLayerError()[0] != '\0' ? NvnLayerError() : "false");
            ExtrasDown(kit, extras);
            return false;
        }
        at += size;
    }
    return true;
}

static bool ExtrasUp(Kit *kit, Extras *extras, const NVNformat *formats, int count, char *why,
                     size_t why_size) {
    return ExtrasUpOf(kit, extras, formats, count, 0, why, why_size);
}

/// Into the open list: every texture of @p extras copied to where the processor reads it,
/// behind a barrier.
static void ExtrasReadRecorded(Kit *kit, Extras *extras) {
    const NvnProcs *const n = kit->nvn.nvn;
    n->nvnCommandBufferBarrier(kit->nvn.command, NVN_BARRIER_ORDER_FRAGMENTS_BIT);
    const NVNcopyRegion whole = { 0, 0, 0, SMALL, SMALL, 1 };
    for (int i = extras->first_samples > 1 ? 1 : 0; i < extras->made; ++i) {
        n->nvnCommandBufferCopyTextureToBuffer(
            kit->nvn.command, &extras->textures[i], NULL, &whole,
            extras->read.address + (uint64_t)(i * SMALL_BYTES), 0);
    }
}

/// Keeps in @p why the first texel of texture @p which that is not @p inside within the square
/// from (4,4) to (12,12) or @p outside around it.
static void ExtrasJudge(const Extras *extras, int which, const char *whose,
                        const uint8_t inside[4], const uint8_t outside[4], char *why,
                        size_t why_size) {
    if (why[0] != '\0') {
        return;
    }
    const uint8_t *const texels = (const uint8_t *)extras->read.bytes + which * SMALL_BYTES;
    for (int y = 0; y < SMALL; ++y) {
        for (int x = 0; x < SMALL; ++x) {
            const bool in = x >= 4 && x < 12 && y >= 4 && y < 12;
            const uint8_t *const want = in ? inside : outside;
            const uint8_t *const got = texels + (y * SMALL + x) * 4;
            if (memcmp(got, want, 4) != 0) {
                snprintf(why, why_size, "%s: texel (%d,%d), %s the square, holds %02x %02x %02x "
                         "%02x, expected %02x %02x %02x %02x (0x%02x is a byte no copy wrote)",
                         whose, x, y, in ? "inside" : "outside", got[0], got[1], got[2], got[3],
                         want[0], want[1], want[2], want[3], PAGE_PRESET);
                return;
            }
        }
    }
}

/// Into the open list: an unsigned and a signed integer texture, each bound as the target and
/// cleared whole, then cleared again through a mask of two channels; then the way's own target
/// bound again, with its scissor.
static void IntegerClearsRecorded(Kit *kit, Extras *extras) {
    const NvnProcs *const n = kit->nvn.nvn;
    NVNcommandBuffer *const cmd = kit->nvn.command;
    static const uint32_t first_unsigned[4] = { 1u, 200u, 255u, 7u };
    static const uint32_t then_unsigned[4] = { 9u, 9u, 9u, 9u };
    static const int first_signed[4] = { -1, 127, -128, 5 };
    static const int then_signed[4] = { 3, 3, 3, 3 };
    const NVNtexture *target[1] = { &extras->textures[0] };
    n->nvnCommandBufferSetRenderTargets(cmd, 1, target, NULL, NULL, NULL);
    n->nvnCommandBufferSetScissor(cmd, 0, 0, SMALL, SMALL);
    n->nvnCommandBufferClearColorui(cmd, 0, first_unsigned, NVN_CLEAR_COLOR_MASK_RGBA);
    n->nvnCommandBufferClearColorui(cmd, 0, then_unsigned,
                                    NVN_CLEAR_COLOR_MASK_R_BIT | NVN_CLEAR_COLOR_MASK_B_BIT);
    target[0] = &extras->textures[1];
    n->nvnCommandBufferSetRenderTargets(cmd, 1, target, NULL, NULL, NULL);
    n->nvnCommandBufferClearColori(cmd, 0, first_signed, NVN_CLEAR_COLOR_MASK_RGBA);
    n->nvnCommandBufferClearColori(cmd, 0, then_signed,
                                   NVN_CLEAR_COLOR_MASK_G_BIT | NVN_CLEAR_COLOR_MASK_A_BIT);
    target[0] = kit->nvn.target;
    n->nvnCommandBufferSetRenderTargets(cmd, 1, target, NULL, NULL, NULL);
    n->nvnCommandBufferSetScissor(cmd, 0, 0, (int)SCENE_SIZE, (int)SCENE_SIZE);
}

/// A clear of an integer target holds the integers given, unsigned and signed, and a second
/// clear through a mask changes the channels the mask names and no other.
static void IntegerClearsCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[320] = "";
    Extras extras;
    static const NVNformat formats[2] = { NVN_FORMAT_RGBA8UI, NVN_FORMAT_RGBA8I };
    if (ExtrasUp(&kit, &extras, formats, 2, why, sizeof(why))) {
        KitBegin(&kit, true);
        IntegerClearsRecorded(&kit, &extras);
        KitCell(&kit, 10);
        ExtrasReadRecorded(&kit, &extras);
        if (KitEnd(&kit, why, sizeof(why))) {
            static const uint8_t want_unsigned[4] = { 9, 200, 9, 7 };
            static const uint8_t want_signed[4] = { 0xff, 3, 0x80, 3 };
            ExtrasJudge(&extras, 0, "the unsigned target", want_unsigned, want_unsigned, why,
                        sizeof(why));
            ExtrasJudge(&extras, 1, "the signed target", want_signed, want_signed, why,
                        sizeof(why));
        }
        ExtrasDown(&kit, &extras);
    }
    KitJudge(&kit, name, 1u << 10, why);
}

/// Into the open list: three textures that are no target, each cleared whole and then in the
/// square from (4,4) to (12,12) through a mask: a float clear of an RGBA8 texture, and the
/// unsigned and the signed clears of the two integer ones.
static void TextureClearsRecorded(Kit *kit, Extras *extras) {
    const NvnProcs *const n = kit->nvn.nvn;
    NVNcommandBuffer *const cmd = kit->nvn.command;
    const NVNcopyRegion whole = { 0, 0, 0, SMALL, SMALL, 1 };
    const NVNcopyRegion square = { 4, 4, 0, 8, 8, 1 };
    static const float red[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
    static const float cyan[4] = { 0.0f, 1.0f, 1.0f, 0.0f };
    n->nvnCommandBufferClearTexture(cmd, &extras->textures[0], NULL, &whole, red,
                                    NVN_CLEAR_COLOR_MASK_RGBA);
    n->nvnCommandBufferClearTexture(cmd, &extras->textures[0], NULL, &square, cyan,
                                    NVN_CLEAR_COLOR_MASK_G_BIT | NVN_CLEAR_COLOR_MASK_B_BIT);
    static const uint32_t first_unsigned[4] = { 10u, 20u, 30u, 40u };
    static const uint32_t then_unsigned[4] = { 50u, 60u, 70u, 80u };
    n->nvnCommandBufferClearTextureui(cmd, &extras->textures[1], NULL, &whole, first_unsigned,
                                      NVN_CLEAR_COLOR_MASK_RGBA);
    n->nvnCommandBufferClearTextureui(cmd, &extras->textures[1], NULL, &square, then_unsigned,
                                      NVN_CLEAR_COLOR_MASK_R_BIT | NVN_CLEAR_COLOR_MASK_A_BIT);
    static const int first_signed[4] = { -10, -20, -30, -40 };
    static const int then_signed[4] = { 1, 2, 3, 4 };
    n->nvnCommandBufferClearTexturei(cmd, &extras->textures[2], NULL, &whole, first_signed,
                                     NVN_CLEAR_COLOR_MASK_RGBA);
    n->nvnCommandBufferClearTexturei(cmd, &extras->textures[2], NULL, &square, then_signed,
                                     NVN_CLEAR_COLOR_MASK_G_BIT);
}

/// A clear of a texture that is no target reaches the region given and the channels its mask
/// names, in the words of the texture's kind, and leaves the list's own target and scissor as
/// they were: the cell drawn after the clears is in the way's target.
static void TextureClearsCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[320] = "";
    Extras extras;
    static const NVNformat formats[EXTRAS] = { NVN_FORMAT_RGBA8, NVN_FORMAT_RGBA8UI,
                                               NVN_FORMAT_RGBA8I };
    if (ExtrasUp(&kit, &extras, formats, EXTRAS, why, sizeof(why))) {
        KitBegin(&kit, true);
        TextureClearsRecorded(&kit, &extras);
        KitCell(&kit, 10);
        ExtrasReadRecorded(&kit, &extras);
        if (KitEnd(&kit, why, sizeof(why))) {
            static const uint8_t float_in[4] = { 0xff, 0xff, 0xff, 0xff };
            static const uint8_t float_out[4] = { 0xff, 0x00, 0x00, 0xff };
            static const uint8_t unsigned_in[4] = { 50, 20, 30, 80 };
            static const uint8_t unsigned_out[4] = { 10, 20, 30, 40 };
            static const uint8_t signed_in[4] = { 0xf6, 2, 0xe2, 0xd8 };
            static const uint8_t signed_out[4] = { 0xf6, 0xec, 0xe2, 0xd8 };
            ExtrasJudge(&extras, 0, "the RGBA8 texture", float_in, float_out, why, sizeof(why));
            ExtrasJudge(&extras, 1, "the unsigned texture", unsigned_in, unsigned_out, why,
                        sizeof(why));
            ExtrasJudge(&extras, 2, "the signed texture", signed_in, signed_out, why,
                        sizeof(why));
        }
        ExtrasDown(&kit, &extras);
    }
    KitJudge(&kit, name, 1u << 10, why);
}

#define FILL_AT 0x10u
#define FILL_BYTES 0x60u
#define FILL_WORD 0xc0ffee11u

static void FillRecorded(Kit *kit) {
    kit->nvn.nvn->nvnCommandBufferClearBuffer(kit->nvn.command, kit->page.address + FILL_AT,
                                              FILL_BYTES, FILL_WORD);
}

/// A range of a buffer cleared to a word holds that word in each of its words, and the bytes
/// before it and after it are untouched.
static void FillCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[240] = "";
    KitBegin(&kit, true);
    FillRecorded(&kit);
    if (KitEnd(&kit, why, sizeof(why))) {
        for (uint32_t at = 0; at < PAGE_BYTES && why[0] == '\0'; at += 4u) {
            uint32_t word;
            memcpy(&word, (const uint8_t *)kit.page.bytes + at, sizeof(word));
            const bool in = at >= FILL_AT && at < FILL_AT + FILL_BYTES;
            const uint32_t want = in ? FILL_WORD : 0x01010101u * PAGE_PRESET;
            if (word != want) {
                snprintf(why, sizeof(why), "the word at 0x%x of the page, %s the range of 0x%x "
                         "bytes at 0x%x that was cleared to 0x%08x, is 0x%08x, expected 0x%08x",
                         (unsigned)at, in ? "inside" : "outside", (unsigned)FILL_BYTES,
                         (unsigned)FILL_AT, (unsigned)FILL_WORD, (unsigned)word, (unsigned)want);
            }
        }
    }
    KitJudge(&kit, name, 0u, why);
}

/// The strides a command buffer was set for its copies, and what it then says they are.
static void StridesRecorded(Kit *kit, ptrdiff_t row, ptrdiff_t image, ptrdiff_t got[2]) {
    const NvnProcs *const n = kit->nvn.nvn;
    n->nvnCommandBufferSetCopyRowStride(kit->nvn.command, row);
    n->nvnCommandBufferSetCopyImageStride(kit->nvn.command, image);
    got[0] = n->nvnCommandBufferGetCopyRowStride(kit->nvn.command);
    got[1] = n->nvnCommandBufferGetCopyImageStride(kit->nvn.command);
}

/// A command buffer gives back the row stride and the image stride it was set for its copies,
/// and none once they are set back to rows and images packed tight. What an image stride does
/// to a copy of several layers is not asked: the layer has textures of one.
static void StridesCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[240] = "";
    ptrdiff_t set[2] = { -1, -1 };
    ptrdiff_t back[2] = { -1, -1 };
    KitBegin(&kit, true);
    StridesRecorded(&kit, 288, 0x4800, set);
    StridesRecorded(&kit, 0, 0, back);
    KitCell(&kit, 15);
    if (KitEnd(&kit, why, sizeof(why)) &&
        (set[0] != 288 || set[1] != 0x4800 || back[0] != 0 || back[1] != 0)) {
        snprintf(why, sizeof(why), "set to a row stride of 288 and an image stride of 0x4800, "
                 "the command buffer says %lld and 0x%llx; set back to 0 and 0, it says %lld "
                 "and %lld",
                 (long long)set[0], (unsigned long long)set[1], (long long)back[0],
                 (long long)back[1]);
    }
    KitJudge(&kit, name, 1u << 15, why);
}

static void DiscardsRecorded(Kit *kit) {
    const NvnProcs *const n = kit->nvn.nvn;
    n->nvnCommandBufferDiscardColor(kit->nvn.command, 0);
    n->nvnCommandBufferDiscardDepthStencil(kit->nvn.command);
}

/// A list gives up what its colour and depth targets hold, which leaves them undefined, then
/// clears both and draws: the cell drawn before the discards is gone with the clear, and the
/// one after is there under the depth test, as in a list with no discard.
static void DiscardsCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpenWith(&kit, kind, true, name)) {
        return;
    }
    char why[200] = "";
    const float clear[4] = { (float)SCENE_CLEAR[0] / 255.0f, (float)SCENE_CLEAR[1] / 255.0f,
                             (float)SCENE_CLEAR[2] / 255.0f, (float)SCENE_CLEAR[3] / 255.0f };
    KitBegin(&kit, true);
    DrawPathClearDepth(kit.path, 1.0f);
    KitCell(&kit, 1);
    DiscardsRecorded(&kit);
    DrawPathClear(kit.path, clear);
    DrawPathClearDepth(kit.path, 1.0f);
    DrawPathDepth(kit.path, true, true);
    KitCell(&kit, 2);
    KitEnd(&kit, why, sizeof(why));
    KitJudge(&kit, name, 1u << 2, why);
}

// ---- the render state a list sets by itself

/// A vertex of a case's own at a pixel boundary, in a cell's colour.
static GpuVertex OwnVertex(uint32_t x, uint32_t y, uint32_t cell) {
    uint8_t colour[4];
    CellColour(cell, colour);
    const GpuVertex vertex = { ClipX(x), ClipY(y), 0.5f, 0.0f, 0.0f, (float)colour[0] / 255.0f,
                               (float)colour[1] / 255.0f, (float)colour[2] / 255.0f, 1.0f };
    return vertex;
}

/// Binds the case's own vertices for the draws after it.
static void OwnBound(Kit *kit) {
    DrawPathVertices(kit->path, &kit->own, OWN_VERTICES * (uint32_t)sizeof(GpuVertex));
}

static void ViewsRecorded(Kit *kit) {
    const NvnProcs *const n = kit->nvn.nvn;
    NVNcommandBuffer *const cmd = kit->nvn.command;
    static const int square[4] = { 4, 4, 8, 8 };
    static const int whole[4] = { 0, 0, (int)SCENE_SIZE, (int)SCENE_SIZE };
    static const float quarter[4] = { 32.0f, 32.0f, 32.0f, 32.0f };
    static const float all[4] = { 0.0f, 0.0f, (float)SCENE_SIZE, (float)SCENE_SIZE };
    n->nvnCommandBufferSetScissors(cmd, 0, 1, square);
    KitCell(kit, 0);
    n->nvnCommandBufferSetScissors(cmd, 0, 1, whole);
    n->nvnCommandBufferSetViewports(cmd, 0, 1, quarter);
    KitCell(kit, 0);
    n->nvnCommandBufferSetViewports(cmd, 0, 1, all);
    KitCell(kit, 15);
}

static void ViewsExpected(uint32_t x, uint32_t y, uint8_t out[4]) {
    if ((x >= 4u && x < 12u && y >= 4u && y < 12u) ||
        (x >= 32u && x < 40u && y >= 32u && y < 40u)) {
        CellColour(0, out);
    } else if (x >= 48u && y >= 48u) {
        CellColour(15, out);
    } else {
        memcpy(out, SCENE_CLEAR, 4);
    }
}

/// The scissors and the viewports set by number, one of each: cell 0's quad under a scissor of
/// the eight pixels in its middle is that square alone; the same quad through a viewport of the
/// target's lower right quarter is the quarter's own top left sixteenth, eight pixels a side at
/// (32,32); and with both back over the whole target a cell is where it always is.
static void ViewsCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[200] = "";
    KitBegin(&kit, true);
    ViewsRecorded(&kit);
    KitEnd(&kit, why, sizeof(why));
    KitJudgeWith(&kit, name, ViewsExpected, why);
}

/// Into the open list, whose depth target holds 0.6 and whose depth test passes what is nearer:
/// a cell under each depth range and each depth bounds. The quads are at 0.5 of the range.
static void DepthsRecorded(Kit *kit) {
    const NvnProcs *const n = kit->nvn.nvn;
    NVNcommandBuffer *const cmd = kit->nvn.command;
    static const float far_range[2] = { 0.9f, 1.0f };
    static const float whole_range[2] = { 0.0f, 1.0f };
    KitCell(kit, 0);
    n->nvnCommandBufferSetDepthRange(cmd, 0.8f, 1.0f);
    KitCell(kit, 1);
    n->nvnCommandBufferSetDepthRange(cmd, 0.0f, 0.4f);
    KitCell(kit, 2);
    n->nvnCommandBufferSetDepthRanges(cmd, 0, 1, far_range);
    KitCell(kit, 3);
    n->nvnCommandBufferSetDepthRanges(cmd, 0, 1, whole_range);
    KitCell(kit, 4);
    n->nvnCommandBufferSetDepthBounds(cmd, 1, 0.7f, 1.0f);
    KitCell(kit, 5);
    n->nvnCommandBufferSetDepthBounds(cmd, 1, 0.5f, 0.7f);
    KitCell(kit, 6);
    n->nvnCommandBufferSetDepthBounds(cmd, 0, 0.7f, 1.0f);
    KitCell(kit, 7);
}

/// A depth range moves a quad's depth before the test, and the depth bounds test what the
/// target already holds: over a depth target of 0.6, a quad half way down its range is drawn
/// with the range 0 to 1 or 0 to 0.4 and hidden with 0.8 to 1 or 0.9 to 1, whichever call set
/// it; it is hidden while the bounds 0.7 to 1 are on, and drawn with 0.5 to 0.7, or with the
/// bounds off.
static void DepthsCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpenWith(&kit, kind, true, name)) {
        return;
    }
    char why[200] = "";
    KitBegin(&kit, true);
    DrawPathClearDepth(kit.path, 0.6f);
    DrawPathDepth(kit.path, true, false);
    DepthsRecorded(&kit);
    KitEnd(&kit, why, sizeof(why));
    KitJudge(&kit, name, 1u << 0 | 1u << 2 | 1u << 4 | 1u << 6 | 1u << 7, why);
}

/// Into the open list: a cell with the rasterizer off and one with it on; a cell past the far
/// plane without the depth clamp and one with it; and two quads of the case's own, whose first
/// and last vertices differ in colour, one under each provoking vertex.
static void RasterRecorded(Kit *kit) {
    const NvnProcs *const n = kit->nvn.nvn;
    NVNcommandBuffer *const cmd = kit->nvn.command;
    n->nvnCommandBufferSetRasterizerDiscard(cmd, 1);
    KitCell(kit, 0);
    n->nvnCommandBufferSetRasterizerDiscard(cmd, 0);
    KitCell(kit, 1);
    DrawPathUniform(kit->path, DRAW_STAGE_VERTEX, 0, &kit->far, sizeof(Move));
    n->nvnCommandBufferSetDepthClamp(cmd, 0);
    KitCell(kit, 2);
    n->nvnCommandBufferSetDepthClamp(cmd, 1);
    KitCell(kit, 3);
    n->nvnCommandBufferSetDepthClamp(cmd, 0);
    DrawPathUniform(kit->path, DRAW_STAGE_VERTEX, 0, &kit->move, sizeof(Move));
    OwnBound(kit);
    n->nvnCommandBufferSetProvokingVertexMode(cmd, 0);
    DrawPathDraw(kit->path, 0, 6);
    n->nvnCommandBufferSetProvokingVertexMode(cmd, 1);
    DrawPathDraw(kit->path, 6, 6);
}

/// The rasterizer's three switches: nothing is drawn while it discards; a quad past the far
/// plane is clipped away, and drawn once the depth clamp holds it at the plane; and a
/// triangle's flat colour is its first vertex's in mode 0 and its last's in mode 1. Cells 4 and
/// 5 are each two triangles whose one vertex of the cell's colour is the first, or the last,
/// the other two being another cell's.
static void RasterCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[200] = "";
    GpuVertex own[12];
    for (uint32_t quad = 0; quad < 2u; ++quad) {
        const uint32_t cell = 4u + quad;
        const uint32_t x0 = cell % CELLS_ACROSS * CELL;
        const uint32_t y0 = cell / CELLS_ACROSS * CELL;
        // The corners, top left, top right, bottom right, bottom left, in another cell's colour.
        GpuVertex corner[4] = { OwnVertex(x0, y0, 14), OwnVertex(x0 + CELL, y0, 14),
                                OwnVertex(x0 + CELL, y0 + CELL, 14),
                                OwnVertex(x0, y0 + CELL, 14) };
        static const uint32_t order[6] = { 0, 1, 2, 0, 2, 3 };
        for (uint32_t i = 0; i < 6u; ++i) {
            GpuVertex vertex = corner[order[i]];
            // The first vertex of each triangle for the first quad, the last for the second.
            if (i % 3u == (quad == 0u ? 0u : 2u)) {
                const GpuVertex mine = OwnVertex(0, 0, cell);
                vertex.r = mine.r;
                vertex.g = mine.g;
                vertex.b = mine.b;
            }
            own[quad * 6u + i] = vertex;
        }
    }
    memcpy(kit.own.bytes, own, sizeof(own));
    KitBegin(&kit, true);
    RasterRecorded(&kit);
    KitEnd(&kit, why, sizeof(why));
    KitJudge(&kit, name, 1u << 1 | 1u << 3 | 1u << 4 | 1u << 5, why);
}

static void PointRecorded(Kit *kit) {
    const NvnProcs *const n = kit->nvn.nvn;
    n->nvnCommandBufferSetPointSize(kit->nvn.command, (float)CELL);
    n->nvnCommandBufferDrawArrays(kit->nvn.command, NVN_DRAW_PRIMITIVE_POINTS, 0, 1);
}

/// A point of sixteen pixels at the middle of cell 2 is the cell: the square of that side
/// around it, every pixel whose centre is inside.
static void PointCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[200] = "";
    const GpuVertex point = OwnVertex(2u * CELL + CELL / 2u, CELL / 2u, 2);
    memcpy(kit.own.bytes, &point, sizeof(point));
    KitBegin(&kit, true);
    OwnBound(&kit);
    PointRecorded(&kit);
    KitEnd(&kit, why, sizeof(why));
    KitJudge(&kit, name, 1u << 2, why);
}

#define RESTART_INDEX 0xffffu

static void RestartRecorded(Kit *kit) {
    const NvnProcs *const n = kit->nvn.nvn;
    n->nvnCommandBufferSetPrimitiveRestart(kit->nvn.command, 1, (int)RESTART_INDEX);
    n->nvnCommandBufferDrawElements(kit->nvn.command, NVN_DRAW_PRIMITIVE_TRIANGLE_STRIP,
                                    NVN_INDEX_TYPE_UNSIGNED_SHORT, 9, kit->indices.address);
    n->nvnCommandBufferSetPrimitiveRestart(kit->nvn.command, 0, 0);
}

/// A triangle strip of nine indices whose fifth is the restart index is two strips: the quads
/// of cells 8 and 10, with nothing drawn between them, where a strip that went on through the
/// index would join them across cell 9.
static void RestartCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[200] = "";
    GpuVertex own[8];
    static const uint32_t cells[2] = { 8, 10 };
    for (uint32_t quad = 0; quad < 2u; ++quad) {
        const uint32_t x0 = cells[quad] % CELLS_ACROSS * CELL;
        const uint32_t y0 = cells[quad] / CELLS_ACROSS * CELL;
        own[quad * 4u + 0u] = OwnVertex(x0, y0, cells[quad]);
        own[quad * 4u + 1u] = OwnVertex(x0 + CELL, y0, cells[quad]);
        own[quad * 4u + 2u] = OwnVertex(x0, y0 + CELL, cells[quad]);
        own[quad * 4u + 3u] = OwnVertex(x0 + CELL, y0 + CELL, cells[quad]);
    }
    memcpy(kit.own.bytes, own, sizeof(own));
    static const uint16_t indices[9] = { 0, 1, 2, 3, RESTART_INDEX, 4, 5, 6, 7 };
    memcpy(kit.indices.bytes, indices, sizeof(indices));
    KitBegin(&kit, true);
    OwnBound(&kit);
    RestartRecorded(&kit);
    KitEnd(&kit, why, sizeof(why));
    KitJudge(&kit, name, 1u << 8 | 1u << 10, why);
}

static void SwizzleRecorded(Kit *kit) {
    const NvnProcs *const n = kit->nvn.nvn;
    static const NVNviewportSwizzle swapped[4] = { NVN_VIEWPORT_SWIZZLE_POSITIVE_Y,
                                                   NVN_VIEWPORT_SWIZZLE_POSITIVE_X,
                                                   NVN_VIEWPORT_SWIZZLE_POSITIVE_Z,
                                                   NVN_VIEWPORT_SWIZZLE_POSITIVE_W };
    static const NVNviewportSwizzle straight[4] = { NVN_VIEWPORT_SWIZZLE_POSITIVE_X,
                                                    NVN_VIEWPORT_SWIZZLE_POSITIVE_Y,
                                                    NVN_VIEWPORT_SWIZZLE_POSITIVE_Z,
                                                    NVN_VIEWPORT_SWIZZLE_POSITIVE_W };
    n->nvnCommandBufferSetViewportSwizzles(kit->nvn.command, 0, 1, swapped);
    KitCell(kit, 1);
    n->nvnCommandBufferSetViewportSwizzles(kit->nvn.command, 0, 1, straight);
    KitCell(kit, 0);
}

static void SwizzleExpected(uint32_t x, uint32_t y, uint8_t out[4]) {
    const uint32_t k = y / CELL * CELLS_ACROSS + x / CELL;
    if (k == 11u) {
        CellColour(1, out);
    } else if (k == 0u) {
        CellColour(0, out);
    } else {
        memcpy(out, SCENE_CLEAR, 4);
    }
}

/// A viewport swizzle that gives x the position's y and y its x turns cell 1's quad about the
/// target's diagonal: with the top left origin the cases draw in, it lands on cell 11, in cell
/// 1's colour. With the swizzle straight again a cell is where it always is.
static void SwizzleCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[200] = "";
    KitBegin(&kit, true);
    SwizzleRecorded(&kit);
    KitEnd(&kit, why, sizeof(why));
    KitJudgeWith(&kit, name, SwizzleExpected, why);
}

static void TilesRecorded(Kit *kit) {
    const NvnProcs *const n = kit->nvn.nvn;
    NVNcommandBuffer *const cmd = kit->nvn.command;
    n->nvnCommandBufferSetTiledCacheTileSize(cmd, 64, 64);
    n->nvnCommandBufferSetTiledCacheAction(cmd, NVN_TILED_CACHE_ACTION_FLUSH);
    n->nvnCommandBufferSetTiledCacheAction(cmd, NVN_TILED_CACHE_ACTION_DISABLE);
    KitCell(kit, 3);
    n->nvnCommandBufferSetTiledCacheAction(cmd, NVN_TILED_CACHE_ACTION_ENABLE);
    KitCell(kit, 4);
    n->nvnCommandBufferSetTiledCacheAction(cmd, NVN_TILED_CACHE_ACTION_FLUSH);
}

/// The tiled cache is how the graphics unit orders its work, and no picture shows it: with a
/// tile size set and the cache flushed, turned off and on around two draws, both cells are
/// there as without it.
static void TilesCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[200] = "";
    KitBegin(&kit, true);
    TilesRecorded(&kit);
    KitEnd(&kit, why, sizeof(why));
    KitJudge(&kit, name, 1u << 3 | 1u << 4, why);
}

// ---- lines and conservative rasterization

/// A quad of the case's own inside @p cell, @p inset pixels in from each of the cell's sides.
static void InsetQuad(GpuVertex out[6], uint32_t cell, float inset) {
    const float x0 = (float)(cell % CELLS_ACROSS * CELL) + inset;
    const float y0 = (float)(cell / CELLS_ACROSS * CELL) + inset;
    const float x1 = (float)(cell % CELLS_ACROSS * CELL + CELL) - inset;
    const float y1 = (float)(cell / CELLS_ACROSS * CELL + CELL) - inset;
    GpuVertex corner[4];
    for (int i = 0; i < 4; ++i) {
        corner[i] = OwnVertex(0, 0, cell);
        corner[i].x = ((i == 1 || i == 2) ? x1 : x0) * 2.0f / (float)SCENE_SIZE - 1.0f;
        corner[i].y = 1.0f - ((i >= 2) ? y1 : y0) * 2.0f / (float)SCENE_SIZE;
    }
    static const uint32_t order[6] = { 0, 1, 2, 0, 2, 3 };
    for (uint32_t i = 0; i < 6u; ++i) {
        out[i] = corner[order[i]];
    }
}

/// How far in from its cell's sides each quad of the conservative cases is: 0.6 of a pixel
/// leaves the outer ring of pixels touched with their centres outside, and 1.3 leaves it a
/// third of a pixel away.
#define INSET_TOUCHING 0.6f
#define INSET_NEAR 1.3f

static void TouchedRecorded(Kit *kit) {
    const NvnProcs *const n = kit->nvn.nvn;
    NVNcommandBuffer *const cmd = kit->nvn.command;
    DrawPathDraw(kit->path, 0, 6);
    n->nvnCommandBufferSetConservativeRasterEnable(cmd, 1);
    DrawPathDraw(kit->path, 6, 6);
    n->nvnCommandBufferSetSubpixelPrecisionBias(cmd, 4, 4);
    DrawPathDraw(kit->path, 12, 6);
    n->nvnCommandBufferSetSubpixelPrecisionBias(cmd, 0, 0);
    n->nvnCommandBufferSetConservativeRasterEnable(cmd, 0);
    DrawPathDraw(kit->path, 18, 6);
}

/// The picture of the two conservative cases: the cells of ringed (a bit a cell) hold their
/// colour but for their outer ring of pixels, the cells of whole all of it.
static uint32_t ringed_cells;
static uint32_t whole_cells;

static void RingsExpected(uint32_t x, uint32_t y, uint8_t out[4]) {
    const uint32_t k = y / CELL * CELLS_ACROSS + x / CELL;
    const bool ring = x % CELL == 0u || x % CELL == CELL - 1u || y % CELL == 0u ||
                      y % CELL == CELL - 1u;
    if ((whole_cells >> k & 1u) != 0u || ((ringed_cells >> k & 1u) != 0u && !ring)) {
        CellColour(k, out);
    } else {
        memcpy(out, SCENE_CLEAR, 4);
    }
}

/// Conservative rasterization draws every pixel a primitive touches, where the usual rule
/// draws the ones whose centre it covers: a quad 0.6 of a pixel inside its cell leaves the
/// cell's outer ring clear, and with nvnCommandBufferSetConservativeRasterEnable fills the
/// cell, with or without four more bits of subpixel precision, and leaves the ring clear again
/// once it is off.
static void TouchedCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[200] = "";
    GpuVertex own[24];
    for (uint32_t cell = 0; cell < 4u; ++cell) {
        InsetQuad(own + cell * 6u, cell, INSET_TOUCHING);
    }
    memcpy(kit.own.bytes, own, sizeof(own));
    KitBegin(&kit, true);
    OwnBound(&kit);
    TouchedRecorded(&kit);
    KitEnd(&kit, why, sizeof(why));
    ringed_cells = 1u << 0 | 1u << 3;
    whole_cells = 1u << 1 | 1u << 2;
    KitJudgeWith(&kit, name, RingsExpected, why);
}

static void DilatedRecorded(Kit *kit) {
    const NvnProcs *const n = kit->nvn.nvn;
    NVNcommandBuffer *const cmd = kit->nvn.command;
    n->nvnCommandBufferSetConservativeRasterEnable(cmd, 1);
    n->nvnCommandBufferSetConservativeRasterDilate(cmd, 0.0f);
    DrawPathDraw(kit->path, 0, 6);
    n->nvnCommandBufferSetConservativeRasterDilate(cmd, 0.5f);
    DrawPathDraw(kit->path, 6, 6);
    n->nvnCommandBufferSetConservativeRasterDilate(cmd, 0.0f);
    n->nvnCommandBufferSetConservativeRasterEnable(cmd, 0);
}

/// A dilation grows a primitive before conservative rasterization looks for the pixels it
/// touches: a quad 1.3 pixels inside its cell touches nothing of the outer ring, and with
/// nvnCommandBufferSetConservativeRasterDilate of half a pixel it does, and fills the cell.
static void DilatedCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[200] = "";
    GpuVertex own[12];
    InsetQuad(own, 8, INSET_NEAR);
    InsetQuad(own + 6, 9, INSET_NEAR);
    memcpy(kit.own.bytes, own, sizeof(own));
    KitBegin(&kit, true);
    OwnBound(&kit);
    DilatedRecorded(&kit);
    KitEnd(&kit, why, sizeof(why));
    ringed_cells = 1u << 8;
    whole_cells = 1u << 9;
    KitJudgeWith(&kit, name, RingsExpected, why);
}

/// The line the case draws: across the target's second row of cells, from 8 pixels in to 8
/// pixels from the far side, along the boundary between two rows of pixels.
#define LINE_Y 24u
#define LINE_FROM 8u
#define LINE_TO 56u
#define LINE_WIDTH 4u
/// The cell whose colour the line has.
#define LINE_CELL 6u

static void LineRecorded(Kit *kit) {
    const NvnProcs *const n = kit->nvn.nvn;
    n->nvnCommandBufferSetLineWidth(kit->nvn.command, (float)LINE_WIDTH);
    n->nvnCommandBufferDrawArrays(kit->nvn.command, NVN_DRAW_PRIMITIVE_LINES, 0, 2);
    n->nvnCommandBufferSetLineWidth(kit->nvn.command, 1.0f);
}

/// What the target read back holds, for the line's picture, and the first row of the line.
static const uint8_t *line_pixels;
static uint32_t line_top;

/// The line's picture: four rows from line_top in the line's colour between its ends, and the
/// clear colour everywhere else. The two columns at each end, and one beyond, are as they
/// came: where a wide line's end falls to the pixel is the rasterizer's own.
static void LineExpected(uint32_t x, uint32_t y, uint8_t out[4]) {
    const bool row = y >= line_top && y < line_top + LINE_WIDTH;
    if (row && (x + 3u >= LINE_FROM && x < LINE_FROM + 2u)) {
        memcpy(out, GpuPixel(line_pixels, SCENE_SIZE, x, y), 4);
    } else if (row && (x + 2u >= LINE_TO && x < LINE_TO + 3u)) {
        memcpy(out, GpuPixel(line_pixels, SCENE_SIZE, x, y), 4);
    } else if (row && x >= LINE_FROM && x < LINE_TO) {
        CellColour(LINE_CELL, out);
    } else {
        memcpy(out, SCENE_CLEAR, 4);
    }
}

/// A line of width four is four pixels thick: drawn level along the boundary between two rows
/// of pixels, it is four rows in the line's colour from end to end, two each side of the
/// boundary or shifted by one, which is the rasterizer's rounding and is not asked, and
/// nothing else.
static void LineCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[240] = "";
    const GpuVertex ends[2] = { OwnVertex(LINE_FROM, LINE_Y, LINE_CELL),
                                OwnVertex(LINE_TO, LINE_Y, LINE_CELL) };
    memcpy(kit.own.bytes, ends, sizeof(ends));
    KitBegin(&kit, true);
    OwnBound(&kit);
    LineRecorded(&kit);
    line_top = LINE_Y - LINE_WIDTH / 2u;
    if (KitEnd(&kit, why, sizeof(why))) {
        line_pixels = DrawPathRead(kit.path);
        if (line_pixels != NULL) {
            // The line's rows in the column half way along it.
            uint8_t colour[4];
            CellColour(LINE_CELL, colour);
            uint32_t rows = 0;
            uint32_t top = SCENE_SIZE;
            for (uint32_t y = 0; y < SCENE_SIZE; ++y) {
                if (memcmp(GpuPixel(line_pixels, SCENE_SIZE, SCENE_SIZE / 2u, y), colour, 4) ==
                    0) {
                    ++rows;
                    top = top == SCENE_SIZE ? y : top;
                }
            }
            if (rows != LINE_WIDTH || top + 3u < LINE_Y || top + 1u > LINE_Y) {
                snprintf(why, sizeof(why), "half way along, the line of width %u at the top of "
                         "row %u is %u rows from row %u, where it is %u rows from row %u, %u or "
                         "%u",
                         (unsigned)LINE_WIDTH, (unsigned)LINE_Y, (unsigned)rows, (unsigned)top,
                         (unsigned)LINE_WIDTH, (unsigned)(LINE_Y - 3u), (unsigned)(LINE_Y - 2u),
                         (unsigned)(LINE_Y - 1u));
            } else {
                line_top = top;
            }
        }
    }
    if (line_pixels == NULL) {
        // Nothing read: the judge says so, and reads nothing through the picture.
        KitJudge(&kit, name, 0u, why);
        return;
    }
    KitJudgeWith(&kit, name, LineExpected, why);
    line_pixels = NULL;
}

// ---- the rest of the render state a list sets by itself

/// Into the open list, with the case's own quads bound: two quads of an alpha of a fifth under
/// the alpha test "greater", the reference a half and then an eighth; then two opaque quads
/// blended as the source times the blend colour, which is red and blue and then green.
static void AlphaBlendRecorded(Kit *kit) {
    const NvnProcs *const n = kit->nvn.nvn;
    NVNcommandBuffer *const cmd = kit->nvn.command;
    static const float red_blue[4] = { 1.0f, 0.0f, 1.0f, 1.0f };
    static const float green[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
    _Alignas(8) NVNcolorState color;
    n->nvnColorStateSetDefaults(&color);
    n->nvnColorStateSetAlphaTest(&color, NVN_ALPHA_FUNC_GREATER);
    n->nvnCommandBufferBindColorState(cmd, &color);
    n->nvnCommandBufferSetAlphaRef(cmd, 0.5f);
    DrawPathDraw(kit->path, 0, 6);
    n->nvnCommandBufferSetAlphaRef(cmd, 0.125f);
    DrawPathDraw(kit->path, 6, 6);
    n->nvnColorStateSetAlphaTest(&color, NVN_ALPHA_FUNC_ALWAYS);
    n->nvnColorStateSetBlendEnable(&color, 0, 1);
    n->nvnCommandBufferBindColorState(cmd, &color);
    _Alignas(8) NVNblendState blend;
    n->nvnBlendStateSetDefaults(&blend);
    n->nvnBlendStateSetBlendTarget(&blend, 0);
    n->nvnBlendStateSetBlendFunc(&blend, NVN_BLEND_FUNC_CONSTANT_COLOR, NVN_BLEND_FUNC_ZERO,
                                 NVN_BLEND_FUNC_CONSTANT_ALPHA, NVN_BLEND_FUNC_ZERO);
    n->nvnCommandBufferBindBlendState(cmd, &blend);
    n->nvnCommandBufferSetBlendColor(cmd, red_blue);
    DrawPathDraw(kit->path, 12, 6);
    n->nvnCommandBufferSetBlendColor(cmd, green);
    DrawPathDraw(kit->path, 18, 6);
}

/// What an alpha of a fifth is in a target of eight bits a channel.
#define FIFTH_BYTE 0x33u

static void AlphaBlendExpected(uint32_t x, uint32_t y, uint8_t out[4]) {
    const uint32_t k = y / CELL * CELLS_ACROSS + x / CELL;
    memcpy(out, SCENE_CLEAR, 4);
    if (k >= 1u && k <= 3u) {
        CellColour(k, out);
    }
    if (k == 1u) {
        out[3] = FIFTH_BYTE;
    } else if (k == 2u) {
        out[1] = 0;
    } else if (k == 3u) {
        out[0] = 0;
        out[2] = 0;
    }
}

/// The alpha test's reference and the blend colour, each set by the list between two draws that
/// differ in nothing else. An alpha of a fifth is not greater than a half and is greater than an
/// eighth, so cell 0 stays clear and cell 1 holds its colour with that alpha; a source times a
/// blend colour of red and blue has no green left, and times one of green only its green.
static void AlphaBlendCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[200] = "";
    GpuVertex own[24];
    for (uint32_t k = 0; k < 4u; ++k) {
        InsetQuad(own + k * 6u, k, 0.0f);
        for (uint32_t i = 0; i < 6u && k < 2u; ++i) {
            own[k * 6u + i].a = 0.2f;
        }
    }
    memcpy(kit.own.bytes, own, sizeof(own));
    KitBegin(&kit, true);
    OwnBound(&kit);
    AlphaBlendRecorded(&kit);
    KitEnd(&kit, why, sizeof(why));
    KitJudgeWith(&kit, name, AlphaBlendExpected, why);
}

/// Into the open list: the stencil cleared to 0x0f and tested for equality. Cell 0 with the
/// reference 0x0f, cell 1 with 0x0e, cell 2 with 0x0e under a compare mask of 0xf0. Then cell 3
/// always, replacing the stencil with the reference 0xff under a write mask of 0x30, and over
/// it the case's own quad where the stencil equals 0x3f.
static void StencilRecorded(Kit *kit) {
    const NvnProcs *const n = kit->nvn.nvn;
    NVNcommandBuffer *const cmd = kit->nvn.command;
    n->nvnCommandBufferClearDepthStencil(cmd, 1.0f, 1, 0x0f, 0xff);
    _Alignas(8) NVNdepthStencilState state;
    n->nvnDepthStencilStateSetDefaults(&state);
    n->nvnDepthStencilStateSetDepthTestEnable(&state, 0);
    n->nvnDepthStencilStateSetDepthWriteEnable(&state, 0);
    n->nvnDepthStencilStateSetStencilTestEnable(&state, 1);
    n->nvnDepthStencilStateSetStencilFunc(&state, NVN_FACE_FRONT_AND_BACK,
                                          NVN_STENCIL_FUNC_EQUAL);
    n->nvnDepthStencilStateSetStencilOp(&state, NVN_FACE_FRONT_AND_BACK, NVN_STENCIL_OP_KEEP,
                                        NVN_STENCIL_OP_KEEP, NVN_STENCIL_OP_KEEP);
    n->nvnCommandBufferBindDepthStencilState(cmd, &state);
    n->nvnCommandBufferSetStencilValueMask(cmd, NVN_FACE_FRONT_AND_BACK, 0xff);
    n->nvnCommandBufferSetStencilMask(cmd, NVN_FACE_FRONT_AND_BACK, 0xff);
    n->nvnCommandBufferSetStencilRef(cmd, NVN_FACE_FRONT_AND_BACK, 0x0f);
    KitCell(kit, 0);
    n->nvnCommandBufferSetStencilRef(cmd, NVN_FACE_FRONT_AND_BACK, 0x0e);
    KitCell(kit, 1);
    n->nvnCommandBufferSetStencilValueMask(cmd, NVN_FACE_FRONT_AND_BACK, 0xf0);
    KitCell(kit, 2);
    n->nvnCommandBufferSetStencilValueMask(cmd, NVN_FACE_FRONT_AND_BACK, 0xff);

    n->nvnDepthStencilStateSetStencilFunc(&state, NVN_FACE_FRONT_AND_BACK,
                                          NVN_STENCIL_FUNC_ALWAYS);
    n->nvnDepthStencilStateSetStencilOp(&state, NVN_FACE_FRONT_AND_BACK, NVN_STENCIL_OP_KEEP,
                                        NVN_STENCIL_OP_KEEP, NVN_STENCIL_OP_REPLACE);
    n->nvnCommandBufferBindDepthStencilState(cmd, &state);
    n->nvnCommandBufferSetStencilRef(cmd, NVN_FACE_FRONT_AND_BACK, 0xff);
    n->nvnCommandBufferSetStencilMask(cmd, NVN_FACE_FRONT_AND_BACK, 0x30);
    KitCell(kit, 3);

    n->nvnDepthStencilStateSetStencilFunc(&state, NVN_FACE_FRONT_AND_BACK,
                                          NVN_STENCIL_FUNC_EQUAL);
    n->nvnDepthStencilStateSetStencilOp(&state, NVN_FACE_FRONT_AND_BACK, NVN_STENCIL_OP_KEEP,
                                        NVN_STENCIL_OP_KEEP, NVN_STENCIL_OP_KEEP);
    n->nvnCommandBufferBindDepthStencilState(cmd, &state);
    n->nvnCommandBufferSetStencilMask(cmd, NVN_FACE_FRONT_AND_BACK, 0xff);
    n->nvnCommandBufferSetStencilRef(cmd, NVN_FACE_FRONT_AND_BACK, 0x3f);
    OwnBound(kit);
    DrawPathDraw(kit->path, 0, 6);
}

/// The cell whose colour the stencil case's last quad has, over cell 3.
#define STENCIL_OVER 9u

static void StencilExpected(uint32_t x, uint32_t y, uint8_t out[4]) {
    const uint32_t k = y / CELL * CELLS_ACROSS + x / CELL;
    memcpy(out, SCENE_CLEAR, 4);
    if (k == 0u || k == 2u) {
        CellColour(k, out);
    } else if (k == 3u) {
        CellColour(STENCIL_OVER, out);
    }
}

/// The stencil's reference, compare mask and write mask, each set by the list. Over a stencil
/// of 0x0f, "equal" draws with the reference 0x0f and not with 0x0e, and draws with 0x0e once
/// the compare mask 0xf0 leaves the low bits out. A replace with 0xff under the write mask 0x30
/// leaves 0x3f, which the last quad is drawn over cell 3 for: a runtime that writes every bit
/// leaves 0xff there, and cell 3 its own colour.
static void StencilCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpenOf(&kit, kind, KIT_STENCIL, 0, 0, name)) {
        return;
    }
    char why[200] = "";
    GpuVertex own[6];
    InsetQuad(own, 3, 0.0f);
    const GpuVertex over = OwnVertex(0, 0, STENCIL_OVER);
    for (uint32_t i = 0; i < 6u; ++i) {
        own[i].r = over.r;
        own[i].g = over.g;
        own[i].b = over.b;
    }
    memcpy(kit.own.bytes, own, sizeof(own));
    KitBegin(&kit, true);
    StencilRecorded(&kit);
    KitEnd(&kit, why, sizeof(why));
    KitJudgeWith(&kit, name, StencilExpected, why);
}

/// Into the open list: cell 0 under a sample mask without the first sample and cell 1 under
/// one with it alone; then, with the polygon offset on for filled polygons and the depth test
/// on, cell 2 with an offset of 2 to the 23rd units and no clamp, and cell 3 with the same
/// offset clamped to a twentieth.
static void MaskOffsetRecorded(Kit *kit) {
    const NvnProcs *const n = kit->nvn.nvn;
    NVNcommandBuffer *const cmd = kit->nvn.command;
    n->nvnCommandBufferSetSampleMask(cmd, 0xfffe);
    KitCell(kit, 0);
    n->nvnCommandBufferSetSampleMask(cmd, 0x1);
    KitCell(kit, 1);
    n->nvnCommandBufferSetSampleMask(cmd, 0xffff);
    _Alignas(8) NVNpolygonState polygon;
    n->nvnPolygonStateSetDefaults(&polygon);
    n->nvnPolygonStateSetCullFace(&polygon, NVN_FACE_NONE);
    n->nvnPolygonStateSetPolygonOffsetEnables(&polygon, NVN_POLYGON_OFFSET_ENABLE_FILL);
    n->nvnCommandBufferBindPolygonState(cmd, &polygon);
    n->nvnCommandBufferSetPolygonOffsetClamp(cmd, 0.0f, 8388608.0f, 0.0f);
    KitCell(kit, 2);
    n->nvnCommandBufferSetPolygonOffsetClamp(cmd, 0.0f, 8388608.0f, 0.05f);
    KitCell(kit, 3);
}

/// The sample mask and the polygon offset's clamp. A target of one sample has the first sample
/// alone, so a mask without it draws nothing and one with it draws. Over a depth of 0.6 in a
/// target of 24 bits, where a unit of offset is 2 to the minus 24th, a quad at 0.5 offset by 2
/// to the 23rd units is at 1 and hidden, and with the offset clamped to a twentieth it is at
/// 0.55 and drawn.
static void MaskOffsetCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpenOf(&kit, kind, KIT_STENCIL, 0, 0, name)) {
        return;
    }
    char why[200] = "";
    KitBegin(&kit, true);
    DrawPathClearDepth(kit.path, 0.6f);
    DrawPathDepth(kit.path, true, false);
    MaskOffsetRecorded(&kit);
    KitEnd(&kit, why, sizeof(why));
    KitJudge(&kit, name, 1u << 1 | 1u << 3, why);
}

/// Into the open list: the five hints set, two cells drawn, and each hint set back.
static void HintsRecorded(Kit *kit) {
    const NvnProcs *const n = kit->nvn.nvn;
    NVNcommandBuffer *const cmd = kit->nvn.command;
    n->nvnCommandBufferSetStencilCullCriteria(cmd, NVN_STENCIL_FUNC_ALWAYS, 0, 0xff);
    n->nvnCommandBufferOverrideSubtileSize(cmd, 1, 8);
    n->nvnCommandBufferSetColorReductionThresholds(cmd, 0, 0, 0);
    n->nvnCommandBufferSetColorReductionEnable(cmd, 1);
    n->nvnCommandBufferSetZCullZF32CompressionEnable(cmd, 1);
    KitCell(kit, 5);
    KitCell(kit, 6);
    n->nvnCommandBufferSetZCullZF32CompressionEnable(cmd, 0);
    n->nvnCommandBufferSetColorReductionEnable(cmd, 0);
    n->nvnCommandBufferOverrideSubtileSize(cmd, 0, 0);
}

/// Five hints to the graphics unit, which speed its work and show in no picture: what its
/// coarse culling may take of the stencil test and of float depth, the tiled cache's subtile,
/// and the shortcut for blends the source alpha makes plain, which no draw here asks for,
/// nothing being blended. Two cells drawn with all of them set are there as without them.
static void HintsCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[200] = "";
    KitBegin(&kit, true);
    HintsRecorded(&kit);
    KitEnd(&kit, why, sizeof(why));
    KitJudge(&kit, name, 1u << 5 | 1u << 6, why);
}

// ---- a program's sample shading, and the depth target's own business

/// Into the open list: a program of the case's own, its sample shading set to what its code
/// says before its shaders are, bound for the draws after it. False when the program was not
/// made or took no shaders; @p subtile is what it says of the tiled cache's subtile.
static bool ShadedRecorded(Kit *kit, NVNprogram *program, bool *made, int *subtile) {
    const NvnProcs *const n = kit->nvn.nvn;
    *made = n->nvnProgramInitialize(program, kit->nvn.device);
    if (!*made) {
        return false;
    }
    n->nvnProgramSetSampleShading(program, 0);
    if (!n->nvnProgramSetShaders(program, 2, kit->nvn.stages[kit->program])) {
        return false;
    }
    *subtile = n->nvnProgramGetSubtileSize(program);
    n->nvnCommandBufferBindProgram(kit->nvn.command, program,
                                   NVN_SHADER_STAGE_VERTEX_BIT | NVN_SHADER_STAGE_FRAGMENT_BIT);
    return true;
}

/// A program whose sample shading is left to its code draws as any other, and asks for no
/// subtile of the tiled cache: a driver's nvnProgramSetShaders leaves that at nought and
/// nothing in NVN sets it.
static void ShadedCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[240] = "";
    static _Alignas(8) NVNprogram program;
    bool made = false;
    int subtile = -1;
    KitBegin(&kit, true);
    const bool set = ShadedRecorded(&kit, &program, &made, &subtile);
    for (uint32_t k = 12; k < 15u; ++k) {
        KitCell(&kit, k);
    }
    if (KitEnd(&kit, why, sizeof(why)) && !set) {
        snprintf(why, sizeof(why), "%s answered false: %s",
                 made ? "nvnProgramSetShaders" : "nvnProgramInitialize",
                 NvnLayerError()[0] != '\0' ? NvnLayerError() : "no reason given");
    } else if (why[0] == '\0' && subtile != 0) {
        snprintf(why, sizeof(why), "the program's subtile size is %d, where a program asks for "
                                   "none, 0",
                 subtile);
    }
    if (made) {
        kit.nvn.nvn->nvnProgramFinalize(&program);
    }
    KitJudge(&kit, name, set ? 1u << 12 | 1u << 13 | 1u << 14 : 0u, why);
}

// ---- subroutines

/// The first word of a linkage map and the bytes of its header, as in t_nvn.c.
#define LINKAGE_MAGIC 0xfabffabcu
#define LINKAGE_HEADER_BYTES 80u

/// Into the open list: a program of the case's own of the scene's two stages, which declare no
/// subroutine, offered a linkage map for its fragment stage and bound for the draws after it,
/// and a choice of two subroutines for that stage. False when the program was not made or took
/// no shaders; @p took is what it said of the map.
static bool ChosenRecorded(Kit *kit, NVNprogram *program, bool *made, int *took) {
    const NvnProcs *const n = kit->nvn.nvn;
    *made = n->nvnProgramInitialize(program, kit->nvn.device);
    if (!*made) {
        return false;
    }
    if (!n->nvnProgramSetShaders(program, 2, kit->nvn.stages[kit->program])) {
        return false;
    }
    static _Alignas(8) uint32_t header[LINKAGE_HEADER_BYTES / 4u];
    memset(header, 0, sizeof(header));
    header[0] = LINKAGE_MAGIC;
    header[1] = NVN_SHADER_STAGE_FRAGMENT;
    header[5] = LINKAGE_HEADER_BYTES;
    const NVNsubroutineLinkageMapPtr maps[1] = { header };
    *took = n->nvnProgramSetSubroutineLinkage(program, 1, maps);
    n->nvnCommandBufferBindProgram(kit->nvn.command, program,
                                   NVN_SHADER_STAGE_VERTEX_BIT | NVN_SHADER_STAGE_FRAGMENT_BIT);
    static const int chosen[2] = { 1, 0 };
    n->nvnCommandBufferSetProgramSubroutines(kit->nvn.command, program,
                                             NVN_SHADER_STAGE_FRAGMENT, 0, 2, chosen);
    return true;
}

/// A list's choice of subroutines reaches a stage through the linkage map its program kept, and
/// a driver that finds no map there when the list runs does nothing with the choice: so says the
/// code of an NVN 55 driver. The scene's stages declare no subroutine, so their program refuses
/// the map it is offered, and three cells drawn under a choice for it, the last after a second
/// choice, are there as without one.
static void ChosenCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[240] = "";
    static _Alignas(8) NVNprogram program;
    bool made = false;
    int took = -1;
    KitBegin(&kit, true);
    const bool set = ChosenRecorded(&kit, &program, &made, &took);
    if (set) {
        KitCell(&kit, 8);
        KitCell(&kit, 9);
        static const int again[1] = { 3 };
        kit.nvn.nvn->nvnCommandBufferSetProgramSubroutines(kit.nvn.command, &program,
                                                           NVN_SHADER_STAGE_FRAGMENT, 1, 1, again);
        KitCell(&kit, 10);
    }
    if (KitEnd(&kit, why, sizeof(why)) && !set) {
        snprintf(why, sizeof(why), "%s answered false: %s",
                 made ? "nvnProgramSetShaders" : "nvnProgramInitialize",
                 NvnLayerError()[0] != '\0' ? NvnLayerError() : "no reason given");
    } else if (why[0] == '\0' && took != 0) {
        snprintf(why, sizeof(why), "the program answered %d to a linkage map, where one whose "
                                   "stages declare no subroutine takes none, 0",
                 took);
    }
    if (made) {
        kit.nvn.nvn->nvnProgramFinalize(&program);
    }
    KitJudge(&kit, name, set ? 1u << 8 | 1u << 9 | 1u << 10 : 0u, why);
}

/// Into the open list, between depth tested draws: the depth target written out as values, and
/// the coarse culling's data saved to the page's memory and restored from it.
static void DepthKeptRecorded(Kit *kit) {
    const NvnProcs *const n = kit->nvn.nvn;
    NVNcommandBuffer *const cmd = kit->nvn.command;
    KitCell(kit, 0);
    n->nvnCommandBufferResolveDepthBuffer(cmd);
    KitCell(kit, 1);
    n->nvnCommandBufferSaveZCullData(cmd, kit->page.address, PAGE_BYTES);
    KitCell(kit, 2);
    n->nvnCommandBufferRestoreZCullData(cmd, kit->page.address, PAGE_BYTES);
    KitCell(kit, 3);
}

/// The depth target's own business with the graphics unit, which no picture shows: written out
/// as values where it was kept compressed, and its coarse culling's data saved and restored.
/// Over a depth of 0.6 the four cells, each at 0.5, are drawn around the three calls as without
/// them, and a quad past the depth already there would not be.
static void DepthKeptCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpenWith(&kit, kind, true, name)) {
        return;
    }
    char why[200] = "";
    KitBegin(&kit, true);
    DrawPathClearDepth(kit.path, 0.6f);
    DrawPathDepth(kit.path, true, false);
    DepthKeptRecorded(&kit);
    KitEnd(&kit, why, sizeof(why));
    KitJudge(&kit, name, 0xfu, why);
}

// ---- a multisampled target resolved

/// The colours the multisampled target is cleared to before each of the four resolves, each a
/// byte a channel that no other has.
static const uint8_t RESOLVED[4][4] = { { 0x33, 0x66, 0x99, 0xff },
                                        { 0x99, 0x33, 0x66, 0xff },
                                        { 0x66, 0x99, 0x33, 0xff },
                                        { 0xcc, 0x33, 0x99, 0xff } };

/// Into the open list: the multisampled texture, the first of @p extras, cleared to a colour
/// and resolved into one of the four others, by each of NVN's four resolves in turn.
static void ResolvesRecorded(Kit *kit, Extras *extras) {
    const NvnProcs *const n = kit->nvn.nvn;
    NVNcommandBuffer *const cmd = kit->nvn.command;
    const NVNtexture *const many = &extras->textures[0];
    const NVNtexture *target[1] = { many };
    _Alignas(8) NVNmultisampleState samples;
    n->nvnMultisampleStateSetDefaults(&samples);
    n->nvnMultisampleStateSetMultisampleEnable(&samples, 1);
    n->nvnMultisampleStateSetSamples(&samples, extras->first_samples);
    _Alignas(8) NVNtextureView view;
    n->nvnTextureViewSetDefaults(&view);
    for (int i = 0; i < 4; ++i) {
        const float colour[4] = { (float)RESOLVED[i][0] / 255.0f, (float)RESOLVED[i][1] / 255.0f,
                                  (float)RESOLVED[i][2] / 255.0f,
                                  (float)RESOLVED[i][3] / 255.0f };
        const NVNtexture *const one = &extras->textures[1 + i];
        n->nvnCommandBufferSetRenderTargets(cmd, 1, target, NULL, NULL, NULL);
        n->nvnCommandBufferBindMultisampleState(cmd, &samples);
        n->nvnCommandBufferSetScissor(cmd, 0, 0, SMALL, SMALL);
        n->nvnCommandBufferClearColor(cmd, 0, colour, NVN_CLEAR_COLOR_MASK_RGBA);
        n->nvnCommandBufferBarrier(cmd, NVN_BARRIER_ORDER_FRAGMENTS_BIT |
                                            NVN_BARRIER_INVALIDATE_TEXTURE_BIT);
        if (i == 0) {
            n->nvnCommandBufferDownsample(cmd, many, one);
        } else if (i == 1) {
            n->nvnCommandBufferDownsampleTextureView(cmd, many, &view, one, &view);
        } else if (i == 2) {
            n->nvnCommandBufferTiledDownsample(cmd, many, one);
        } else {
            n->nvnCommandBufferTiledDownsampleTextureView(cmd, many, &view, one, &view);
        }
    }
    n->nvnMultisampleStateSetDefaults(&samples);
    n->nvnCommandBufferBindMultisampleState(cmd, &samples);
    target[0] = kit->nvn.target;
    n->nvnCommandBufferSetRenderTargets(cmd, 1, target, NULL, NULL, NULL);
    n->nvnCommandBufferSetScissor(cmd, 0, 0, (int)SCENE_SIZE, (int)SCENE_SIZE);
}

/// A target of four samples a pixel, cleared, is resolved into a texture of one: every texel is
/// the clear's colour, all four samples of it being that. Four clears and four resolves, by
/// nvnCommandBufferDownsample, its form that takes views, and the two tiled ones, each into a
/// texture of its own; a cell drawn afterwards into the way's own target is there too.
static void ResolvesCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[320] = "";
    Extras extras;
    static const NVNformat formats[5] = { NVN_FORMAT_RGBA8, NVN_FORMAT_RGBA8, NVN_FORMAT_RGBA8,
                                          NVN_FORMAT_RGBA8, NVN_FORMAT_RGBA8 };
    if (ExtrasUpOf(&kit, &extras, formats, 5, 4, why, sizeof(why))) {
        KitBegin(&kit, true);
        ResolvesRecorded(&kit, &extras);
        KitCell(&kit, 10);
        ExtrasReadRecorded(&kit, &extras);
        if (KitEnd(&kit, why, sizeof(why))) {
            static const char *const WHOSE[4] = {
                "the texture nvnCommandBufferDownsample resolved into",
                "the texture nvnCommandBufferDownsampleTextureView resolved into",
                "the texture nvnCommandBufferTiledDownsample resolved into",
                "the texture nvnCommandBufferTiledDownsampleTextureView resolved into",
            };
            for (int i = 0; i < 4; ++i) {
                ExtrasJudge(&extras, 1 + i, WHOSE[i], RESOLVED[i], RESOLVED[i], why, sizeof(why));
            }
        }
        ExtrasDown(&kit, &extras);
    }
    KitJudge(&kit, name, 1u << 10, why);
}

// ---- patches

/// The outer levels the patches of the tessellation case are drawn with, which its evaluation
/// stage writes to alpha: the first two patches, then the third.
#define PATCH_LEVEL_FIRST 3u
#define PATCH_LEVEL_THEN 5u

/// Into the open list: a program of the case's own, the way's vertex and fragment stages with
/// a tessellation evaluation stage between them and no control stage, so the levels are the
/// list's; then two patches of four vertices at one outer level and a third at another.
static bool PatchesRecorded(Kit *kit, NVNprogram *program, bool *made) {
    const NvnProcs *const n = kit->nvn.nvn;
    NVNcommandBuffer *const cmd = kit->nvn.command;
    NVNshaderData stages[3];
    stages[0] = kit->nvn.stages[kit->program][0];
    stages[2] = kit->nvn.stages[kit->program][1];
    if (!DrawPathNvnStage(kit->path, ShaderData(SHADER_QUAD_TESE), ShaderSize(SHADER_QUAD_TESE),
                          &stages[1])) {
        return false;
    }
    *made = n->nvnProgramInitialize(program, kit->nvn.device);
    if (!*made || !n->nvnProgramSetShaders(program, 3, stages)) {
        return false;
    }
    static const float first[4] = { (float)PATCH_LEVEL_FIRST, (float)PATCH_LEVEL_FIRST,
                                    (float)PATCH_LEVEL_FIRST, (float)PATCH_LEVEL_FIRST };
    static const float then[4] = { (float)PATCH_LEVEL_THEN, (float)PATCH_LEVEL_THEN,
                                   (float)PATCH_LEVEL_THEN, (float)PATCH_LEVEL_THEN };
    static const float inner[2] = { 2.0f, 2.0f };
    n->nvnCommandBufferBindProgram(cmd, program,
                                   NVN_SHADER_STAGE_VERTEX_BIT |
                                       NVN_SHADER_STAGE_TESS_EVALUATION_BIT |
                                       NVN_SHADER_STAGE_FRAGMENT_BIT);
    n->nvnCommandBufferSetPatchSize(cmd, 4);
    n->nvnCommandBufferSetInnerTessellationLevels(cmd, inner);
    n->nvnCommandBufferSetOuterTessellationLevels(cmd, first);
    n->nvnCommandBufferDrawArrays(cmd, NVN_DRAW_PRIMITIVE_PATCHES, 0, 8);
    n->nvnCommandBufferSetOuterTessellationLevels(cmd, then);
    n->nvnCommandBufferDrawArrays(cmd, NVN_DRAW_PRIMITIVE_PATCHES, 8, 4);
    return true;
}

static const uint32_t PATCH_CELLS[3] = { 5, 10, 15 };

static void PatchesExpected(uint32_t x, uint32_t y, uint8_t out[4]) {
    const uint32_t k = y / CELL * CELLS_ACROSS + x / CELL;
    memcpy(out, SCENE_CLEAR, 4);
    for (uint32_t i = 0; i < 3u; ++i) {
        if (k == PATCH_CELLS[i]) {
            CellColour(k, out);
            out[3] = (uint8_t)(i < 2u ? PATCH_LEVEL_FIRST : PATCH_LEVEL_THEN);
        }
    }
}

/// Patches drawn through a tessellation evaluation stage with no control stage take their
/// levels from the list: three patches of four vertices, the size
/// nvnCommandBufferSetPatchSize gave, each fill their cell, in the colour of their first vertex
/// and with the outer level the list had set in alpha, 3 for the first two and 5 for the
/// third. The inner levels are set and change nothing here: a quad tessellated finer covers the
/// same pixels.
static void PatchesCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[240] = "";
    GpuVertex own[12];
    for (uint32_t i = 0; i < 3u; ++i) {
        const uint32_t cell = PATCH_CELLS[i];
        const uint32_t x0 = cell % CELLS_ACROSS * CELL;
        const uint32_t y0 = cell / CELLS_ACROSS * CELL;
        // The evaluation stage's corners: 0 to 1 along one side, 3 to 2 along the other.
        own[i * 4u + 0u] = OwnVertex(x0, y0, cell);
        own[i * 4u + 1u] = OwnVertex(x0 + CELL, y0, cell);
        own[i * 4u + 2u] = OwnVertex(x0 + CELL, y0 + CELL, cell);
        own[i * 4u + 3u] = OwnVertex(x0, y0 + CELL, cell);
    }
    memcpy(kit.own.bytes, own, sizeof(own));
    static _Alignas(8) NVNprogram program;
    bool made = false;
    KitBegin(&kit, true);
    OwnBound(&kit);
    const bool drawn = PatchesRecorded(&kit, &program, &made);
    if (KitEnd(&kit, why, sizeof(why)) && !drawn) {
        snprintf(why, sizeof(why), "the program of three stages was not made: %s",
                 DrawPathFailure(kit.path)[0] != '\0'
                     ? DrawPathFailure(kit.path)
                     : NvnLayerError()[0] != '\0' ? NvnLayerError()
                                                  : "nvnProgramInitialize or "
                                                    "nvnProgramSetShaders answered false");
    }
    if (made) {
        kit.nvn.nvn->nvnProgramFinalize(&program);
    }
    KitJudgeWith(&kit, name, PatchesExpected, why);
}

// ---- the debug layer's marks in a list

/// Into the open list, around three draws: markers and groups of each kind, and a checkpoint
/// of each. False when a checkpoint was not taken.
static bool MarksRecorded(Kit *kit) {
    const NvnProcs *const n = kit->nvn.nvn;
    NVNcommandBuffer *const cmd = kit->nvn.command;
    const NVNdebugDomainId domain = n->nvnDeviceGenerateDebugDomainId(kit->nvn.device, "marks");
    n->nvnCommandBufferInsertDebugMarker(cmd, "before the cells");
    n->nvnCommandBufferPushDebugGroup(cmd, domain, "three cells");
    KitCell(kit, 5);
    n->nvnCommandBufferInsertDebugMarkerDynamic(cmd, domain, "a text the list has to keep");
    n->nvnCommandBufferPushDebugGroupDynamic(cmd, domain, "a group named by a kept text");
    const bool dynamic = n->nvnCommandBufferInsertCheckpointDynamic(cmd, "a kept checkpoint");
    KitCell(kit, 6);
    n->nvnCommandBufferPopDebugGroupId(cmd, domain);
    n->nvnCommandBufferInsertDebugMarkerStatic(cmd, domain, "a text that stays where it is");
    n->nvnCommandBufferPushDebugGroupStatic(cmd, domain, "a group named by a standing text");
    const bool fixed = n->nvnCommandBufferInsertCheckpointStatic(cmd, "a standing checkpoint");
    KitCell(kit, 7);
    n->nvnCommandBufferPopDebugGroup(cmd);
    n->nvnCommandBufferPopDebugGroup(cmd);
    return dynamic && fixed;
}

/// The debug layer's marks name parts of a list and record nothing a picture shows: with
/// markers, groups and checkpoints of each kind between them, three cells are drawn as without,
/// and each checkpoint answers that it was taken, as a release driver answers.
static void MarksCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[200] = "";
    KitBegin(&kit, true);
    const bool taken = MarksRecorded(&kit);
    if (KitEnd(&kit, why, sizeof(why)) && !taken) {
        snprintf(why, sizeof(why), "a checkpoint answered false, where a release driver "
                 "answers that it was taken");
    }
    KitJudge(&kit, name, 1u << 5 | 1u << 6 | 1u << 7, why);
}

// ---- the device and a program of its own

/// What the device and a builder say of themselves, and whether each clear value was taken.
typedef struct {
    int flags;
    NVNwindowOriginMode origin;
    NVNdepthMode depth;
    bool taken[5];
} DeviceAnswers;

#define DEVICE_FLAGS (NVN_DEVICE_FLAG_DEFERRED_FINALIZE | NVN_DEVICE_FLAG_DEBUG_ENABLE_LEVEL_1)

static void DeviceAskedRecorded(Kit *kit, DeviceAnswers *out) {
    const NvnProcs *const n = kit->nvn.nvn;
    _Alignas(8) NVNdeviceBuilder builder;
    n->nvnDeviceBuilderSetDefaults(&builder);
    n->nvnDeviceBuilderSetFlags(&builder, DEVICE_FLAGS);
    out->flags = n->nvnDeviceBuilderGetFlags(&builder);
    out->origin = n->nvnDeviceGetWindowOriginMode(kit->nvn.device);
    out->depth = n->nvnDeviceGetDepthMode(kit->nvn.device);
    static const float colour[4] = { 0.25f, 0.5f, 0.75f, 1.0f };
    static const int signed_colour[4] = { -1, 2, -3, 4 };
    static const uint32_t unsigned_colour[4] = { 1u, 2u, 3u, 4u };
    out->taken[0] = n->nvnDeviceRegisterFastClearColor(kit->nvn.device, colour, NVN_FORMAT_RGBA8);
    out->taken[1] =
        n->nvnDeviceRegisterFastClearColori(kit->nvn.device, signed_colour, NVN_FORMAT_RGBA8I);
    out->taken[2] = n->nvnDeviceRegisterFastClearColorui(kit->nvn.device, unsigned_colour,
                                                         NVN_FORMAT_RGBA8UI);
    out->taken[3] = n->nvnDeviceRegisterFastClearDepth(kit->nvn.device, 0.5f);
    out->taken[4] = n->nvnDeviceRegisterFastClearStencil(kit->nvn.device, 0x5a);
}

/// A device builder gives back its flags; the device says the origin and the depth mode it was
/// set, the top left origin and a near plane at zero, which every way of a scene sets; and each
/// of the five clear values registered with it is taken. A clear after them is the same clear:
/// the target cleared and a cell drawn are as in every case.
static void DeviceCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[240] = "";
    DeviceAnswers got;
    memset(&got, 0, sizeof(got));
    DeviceAskedRecorded(&kit, &got);
    KitBegin(&kit, true);
    KitCell(&kit, 13);
    if (KitEnd(&kit, why, sizeof(why))) {
        Holds(why, sizeof(why), "the device builder's flags", (unsigned)got.flags, DEVICE_FLAGS);
        Holds(why, sizeof(why), "the device's window origin mode", (unsigned)got.origin,
              NVN_WINDOW_ORIGIN_MODE_UPPER_LEFT);
        Holds(why, sizeof(why), "the device's depth mode", (unsigned)got.depth,
              NVN_DEPTH_MODE_NEAR_IS_ZERO);
        static const char *const which[5] = {
            "whether the float clear colour was taken", "whether the signed clear colour was taken",
            "whether the unsigned clear colour was taken", "whether the clear depth was taken",
            "whether the clear stencil was taken" };
        for (int i = 0; i < 5; ++i) {
            Holds(why, sizeof(why), which[i], got.taken[i], 1);
        }
    }
    KitJudge(&kit, name, 1u << 13, why);
}

/// A program of the case's own, set from the way's program's stages in NVN 55's longer shader
/// data, and bound for the draws after it. False when it was not made or not set.
static bool LongProgramRecorded(Kit *kit, NVNprogram *program, bool *made) {
    const NvnProcs *const n = kit->nvn.nvn;
    struct {
        NVNshaderData data;
        uint64_t more;
    } stages[2];
    memset(stages, 0, sizeof(stages));
    stages[0].data = kit->nvn.stages[kit->program][0];
    stages[1].data = kit->nvn.stages[kit->program][1];
    *made = n->nvnProgramInitialize(program, kit->nvn.device);
    if (!*made || !n->nvnProgramSetShadersExt(program, 2, stages)) {
        return false;
    }
    n->nvnCommandBufferBindProgram(kit->nvn.command, program,
                                   NVN_SHADER_STAGE_VERTEX_BIT | NVN_SHADER_STAGE_FRAGMENT_BIT);
    return true;
}

/// A program set through nvnProgramSetShadersExt, whose shader data is eight bytes longer a
/// stage than nvnProgramSetShaders', draws as the same stages set the short way: three cells.
static void LongProgramCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) || !KitOpen(&kit, kind, name)) {
        return;
    }
    char why[240] = "";
    static _Alignas(8) NVNprogram program;
    bool made = false;
    KitBegin(&kit, true);
    const bool set = LongProgramRecorded(&kit, &program, &made);
    for (uint32_t k = 9; k < 12u; ++k) {
        KitCell(&kit, k);
    }
    if (KitEnd(&kit, why, sizeof(why)) && !set) {
        snprintf(why, sizeof(why), "%s answered false: %s",
                 made ? "nvnProgramSetShadersExt" : "nvnProgramInitialize",
                 NvnLayerError()[0] != '\0' ? NvnLayerError() : "no reason given");
    }
    if (made) {
        kit.nvn.nvn->nvnProgramFinalize(&program);
    }
    KitJudge(&kit, name, set ? 1u << 9 | 1u << 10 | 1u << 11 : 0u, why);
}

// ---- the handles a device makes apart

/// The side of the handle case's two textures, and where it draws them.
#define HANDLE_SIDE 4u
#define HANDLE_SEPARATE_CELL 5u
#define HANDLE_FETCH_CELL 10u

/// Texel (@p x, @p y) of the texture the case samples: no two alike, none the clear colour.
static void HandleTexel(uint32_t x, uint32_t y, uint8_t out[4]) {
    out[0] = (uint8_t)(0x10u + x * 0x30u);
    out[1] = (uint8_t)(0x18u + y * 0x38u);
    out[2] = (uint8_t)(0xe0u - (x + y * HANDLE_SIDE) * 9u);
    out[3] = 0xffu;
}

/// Into the open list: a program of the way's vertex stage and the handles fragment stage; the
/// first texture's handle in slot 1 and the second texture's sampler's in slot 2, each made
/// apart, and the first texture's texel fetch handle in slot 3; then a cell drawn sampling
/// through the pair, and one fetching texels.
static bool HandlesRecorded(Kit *kit, NVNprogram *program, bool *made, const DrawBuffer paints[2],
                            int sampled, int wrapped) {
    const NvnProcs *const n = kit->nvn.nvn;
    NVNcommandBuffer *const cmd = kit->nvn.command;
    NVNshaderData stages[2];
    stages[0] = kit->nvn.stages[kit->program][0];
    if (!DrawPathNvnStage(kit->path, ShaderData(SHADER_HANDLES_FRAG),
                          ShaderSize(SHADER_HANDLES_FRAG), &stages[1])) {
        return false;
    }
    *made = n->nvnProgramInitialize(program, kit->nvn.device);
    if (!*made || !n->nvnProgramSetShaders(program, 2, stages)) {
        return false;
    }
    n->nvnCommandBufferBindProgram(cmd, program,
                                   NVN_SHADER_STAGE_VERTEX_BIT | NVN_SHADER_STAGE_FRAGMENT_BIT);
    n->nvnCommandBufferBindTexture(
        cmd, NVN_SHADER_STAGE_FRAGMENT, 1,
        n->nvnDeviceGetSeparateTextureHandle(kit->nvn.device, kit->nvn.texture_base + sampled));
    n->nvnCommandBufferBindTexture(
        cmd, NVN_SHADER_STAGE_FRAGMENT, 2,
        n->nvnDeviceGetSeparateSamplerHandle(kit->nvn.device, kit->nvn.sampler_base + wrapped));
    n->nvnCommandBufferBindTexture(
        cmd, NVN_SHADER_STAGE_FRAGMENT, 3,
        n->nvnDeviceGetTexelFetchHandle(kit->nvn.device, kit->nvn.texture_base + sampled));
    DrawPathUniform(kit->path, DRAW_STAGE_FRAGMENT, 0, &paints[0], sizeof(Paint));
    KitCell(kit, HANDLE_SEPARATE_CELL);
    DrawPathUniform(kit->path, DRAW_STAGE_FRAGMENT, 0, &paints[1], sizeof(Paint));
    KitCell(kit, HANDLE_FETCH_CELL);
    return true;
}

static void HandlesExpected(uint32_t x, uint32_t y, uint8_t out[4]) {
    const uint32_t k = y / CELL * CELLS_ACROSS + x / CELL;
    if (k == HANDLE_SEPARATE_CELL) {
        // The texture twice across the cell and twice down, two pixels a texel.
        HandleTexel(x % CELL / 2u % HANDLE_SIDE, y % CELL / 2u % HANDLE_SIDE, out);
    } else if (k == HANDLE_FETCH_CELL) {
        // The texture once, four pixels a texel.
        HandleTexel(x % CELL / 4u, y % CELL / 4u, out);
    } else {
        memcpy(out, SCENE_CLEAR, 4);
    }
}

/// The handle of a texture alone and the handle of a sampler alone, put together by the shader
/// as a console's compiled shader puts a separate pair together, sample that texture through
/// that sampler: the case's first texture with the second texture's sampler, which repeats
/// where the first's own holds its edge, over coordinates to 2, so the cell shows the texture
/// four times. And the texel fetch handle reads the first texture's texels with no sampler:
/// the cell shows it once.
static void HandlesCase(DrawPathKind kind, const char *name) {
    Kit kit;
    if (!ReportStart(SUITE, name) ||
        !KitOpenFor(&kit, kind, false, 2, 2u * HANDLE_SIDE * HANDLE_SIDE * 4u, name)) {
        return;
    }
    char why[240] = "";
    uint8_t texels[HANDLE_SIDE * HANDLE_SIDE * 4u];
    uint8_t other[HANDLE_SIDE * HANDLE_SIDE * 4u];
    for (uint32_t y = 0; y < HANDLE_SIDE; ++y) {
        for (uint32_t x = 0; x < HANDLE_SIDE; ++x) {
            HandleTexel(x, y, texels + (y * HANDLE_SIDE + x) * 4u);
            // The second texture's texels are the first's turned about, which a read of the
            // wrong texture shows.
            HandleTexel(HANDLE_SIDE - 1u - x, HANDLE_SIDE - 1u - y,
                        other + (y * HANDLE_SIDE + x) * 4u);
        }
    }
    const DrawSampler held = { false, DRAW_WRAP_CLAMP_TO_EDGE, { 0.0f, 0.0f, 0.0f, 0.0f } };
    const DrawSampler repeated = { false, DRAW_WRAP_REPEAT, { 0.0f, 0.0f, 0.0f, 0.0f } };
    const int sampled =
        DrawPathTextureSampled(kit.path, HANDLE_SIDE, HANDLE_SIDE, texels, &held);
    const int wrapped =
        DrawPathTextureSampled(kit.path, HANDLE_SIDE, HANDLE_SIDE, other, &repeated);
    DrawBuffer paints[2];
    bool ok = sampled >= 0 && wrapped >= 0;
    for (int mode = 0; ok && mode < 2; ++mode) {
        ok = DrawPathAlloc(kit.path, (uint32_t)sizeof(Paint), &paints[mode]);
        if (ok) {
            const Paint paint = { { 0.0f, 0.0f, 0.0f, 0.0f }, { mode, 0, 0, 0 } };
            memcpy(paints[mode].bytes, &paint, sizeof(paint));
        }
    }
    static _Alignas(8) NVNprogram program;
    bool made = false;
    bool drawn = false;
    if (ok) {
        KitBegin(&kit, true);
        drawn = HandlesRecorded(&kit, &program, &made, paints, sampled, wrapped);
        ok = KitEnd(&kit, why, sizeof(why));
    }
    if (why[0] == '\0' && !(ok && drawn)) {
        snprintf(why, sizeof(why), "the textures or the program of the handles' stage were not "
                 "made: %s",
                 DrawPathFailure(kit.path)[0] != '\0'
                     ? DrawPathFailure(kit.path)
                     : NvnLayerError()[0] != '\0' ? NvnLayerError() : "a call answered false");
    }
    if (made) {
        kit.nvn.nvn->nvnProgramFinalize(&program);
    }
    KitJudgeWith(&kit, name, HandlesExpected, why);
}

// ---- the cases, by way

void NvnCommandFirst(void) {
    SamplesCase(DRAW_PATH_LAYER, "the samples counter reports a quad's 256 pixels and none "
                                 "after a reset, through the layer's own NVN functions");
    ValuesCase(DRAW_PATH_LAYER, "two values a list reports are in memory once it has run, "
                                "through the layer's own NVN functions");
    TimesCase(DRAW_PATH_LAYER, TIME_ORDER,
              "two timestamp reports hold no count and times between the device's clock "
              "before and after, through the layer's own NVN functions");
    TimesCase(DRAW_PATH_LAYER, TIME_UNIT,
              "a timestamp report's word is the graphics unit's ticks, 384 every 625 "
              "nanoseconds, through the layer's own NVN functions");
    EnablesCase(DRAW_PATH_LAYER, "the render enable draws under on and under a condition "
                                 "that holds and skips the rest, through the layer's own NVN "
                                 "functions");
    MemoryCase(DRAW_PATH_LAYER, "a command buffer's memory used and free come to what it "
                                "was given, and draws take some, through the layer's own NVN "
                                "functions");
    CallbackCase(DRAW_PATH_LAYER, "a list out of command memory asks the program's callback "
                                  "and draws on, through the layer's own NVN functions");
    ListsCase(DRAW_PATH_LAYER, "a list called from another and a list copied into it draw "
                               "their cells, through the layer's own NVN functions");
    IntegerClearsCase(DRAW_PATH_LAYER, "clears of an unsigned and a signed integer target "
                                       "hold the integers given, through the layer's own NVN "
                                       "functions");
    TextureClearsCase(DRAW_PATH_LAYER, "a clear of a texture that is no target reaches its "
                                       "region and its mask's channels, through the layer's "
                                       "own NVN functions");
    FillCase(DRAW_PATH_LAYER, "a range of a buffer cleared to a word holds it, and nothing "
                              "beside the range, through the layer's own NVN functions");
    StridesCase(DRAW_PATH_LAYER, "a command buffer gives back the copy strides it was set, "
                                 "through the layer's own NVN functions");
    DiscardsCase(DRAW_PATH_LAYER, "targets discarded, then cleared and drawn, hold the clear "
                                  "and the draw, through the layer's own NVN functions");
    ViewsCase(DRAW_PATH_LAYER, "a scissor and a viewport set by number bound and place a "
                               "quad, through the layer's own NVN functions");
    DepthsCase(DRAW_PATH_LAYER, "depth ranges move a quad across the depth test and depth "
                                "bounds test the target's depth, through the layer's own NVN "
                                "functions");
    RasterCase(DRAW_PATH_LAYER, "the rasterizer discards, clamps depth and takes a flat colour "
                                "from the provoking vertex, through the layer's own NVN "
                                "functions");
    PointCase(DRAW_PATH_LAYER, "a point of sixteen pixels fills its cell, through the layer's "
                               "own NVN functions");
    RestartCase(DRAW_PATH_LAYER, "a strip with a restart index is two quads, through the "
                                 "layer's own NVN functions");
    SwizzleCase(DRAW_PATH_LAYER, "a viewport swizzle of x and y turns a quad about the "
                                 "diagonal, through the layer's own NVN functions");
    TilesCase(DRAW_PATH_LAYER, "draws around the tiled cache's actions are as without them, "
                               "through the layer's own NVN functions");
    AlphaBlendCase(DRAW_PATH_LAYER, "the alpha test's reference and the blend colour a list "
                                    "sets decide two draws each, through the layer's own NVN "
                                    "functions");
    StencilCase(DRAW_PATH_LAYER, "the stencil's reference, compare mask and write mask a list "
                                 "sets decide what is drawn, through the layer's own NVN "
                                 "functions");
    MaskOffsetCase(DRAW_PATH_LAYER, "a sample mask without the first sample draws nothing, and "
                                    "a clamp holds a polygon offset, through the layer's own "
                                    "NVN functions");
    HintsCase(DRAW_PATH_LAYER, "draws under five hints to the graphics unit are as without "
                               "them, through the layer's own NVN functions");
    ShadedCase(DRAW_PATH_LAYER, "a program whose sample shading is its code's draws and asks "
                                "for no subtile, through the layer's own NVN functions");
    ChosenCase(DRAW_PATH_LAYER, "draws after subroutines chosen for a program with no linkage "
                                "map are as without them, through the layer's own NVN functions");
    ResolvesCase(DRAW_PATH_LAYER, "a cleared target of four samples resolves into its colour by "
                                  "each of the four downsamples, through the layer's own NVN "
                                  "functions");
    DepthKeptCase(DRAW_PATH_LAYER, "depth tested draws around a depth target's resolve and its "
                                   "culling data's save and restore are as without them, "
                                   "through the layer's own NVN functions");
    TouchedCase(DRAW_PATH_LAYER, "conservative rasterization draws every pixel a quad "
                                 "touches, through the layer's own NVN functions");
    DilatedCase(DRAW_PATH_LAYER, "a conservative quad dilated by half a pixel reaches the "
                                 "pixels a third of one away, through the layer's own NVN "
                                 "functions");
    LineCase(DRAW_PATH_LAYER, "a line of width four is four rows of pixels, through the "
                              "layer's own NVN functions");
    PatchesCase(DRAW_PATH_LAYER, "patches of the size and the outer levels a list sets fill "
                                 "their cells, through the layer's own NVN functions");
    MarksCase(DRAW_PATH_LAYER, "draws between debug markers, groups and checkpoints are as "
                               "without them, through the layer's own NVN functions");
    DeviceCase(DRAW_PATH_LAYER, "the device says its modes and its builder its flags, and "
                                "clear values register, through the layer's own NVN functions");
    LongProgramCase(DRAW_PATH_LAYER, "a program set from the longer shader data draws, through "
                                     "the layer's own NVN functions");
    HandlesCase(DRAW_PATH_LAYER, "a texture's handle and a sampler's put together sample, and "
                                 "a texel fetch handle fetches, through the layer's own NVN "
                                 "functions");
}

void NvnCommandLast(void) {
    SamplesCase(DRAW_PATH_NVN, "the samples counter reports a quad's 256 pixels and none "
                               "after a reset, through the NVN the program looks up");
    ValuesCase(DRAW_PATH_NVN, "two values a list reports are in memory once it has run, "
                              "through the NVN the program looks up");
    TimesCase(DRAW_PATH_NVN, TIME_ORDER,
              "two timestamp reports hold no count and times between the device's clock "
              "before and after, through the NVN the program looks up");
    TimesCase(DRAW_PATH_NVN, TIME_UNIT,
              "a timestamp report's word is the graphics unit's ticks, 384 every 625 "
              "nanoseconds, through the NVN the program looks up");
    EnablesCase(DRAW_PATH_NVN, "the render enable draws under on and under a condition that "
                               "holds and skips the rest, through the NVN the program looks "
                               "up");
    MemoryCase(DRAW_PATH_NVN, "a command buffer's memory used and free come to what it was "
                              "given, and draws take some, through the NVN the program looks "
                              "up");
    CallbackCase(DRAW_PATH_NVN, "a list out of command memory asks the program's callback and "
                                "draws on, through the NVN the program looks up");
    ListsCase(DRAW_PATH_NVN, "a list called from another and a list copied into it draw their "
                             "cells, through the NVN the program looks up");
    IntegerClearsCase(DRAW_PATH_NVN, "clears of an unsigned and a signed integer target hold "
                                     "the integers given, through the NVN the program looks "
                                     "up");
    TextureClearsCase(DRAW_PATH_NVN, "a clear of a texture that is no target reaches its "
                                     "region and its mask's channels, through the NVN the "
                                     "program looks up");
    FillCase(DRAW_PATH_NVN, "a range of a buffer cleared to a word holds it, and nothing "
                            "beside the range, through the NVN the program looks up");
    StridesCase(DRAW_PATH_NVN, "a command buffer gives back the copy strides it was set, "
                               "through the NVN the program looks up");
    DiscardsCase(DRAW_PATH_NVN, "targets discarded, then cleared and drawn, hold the clear and "
                                "the draw, through the NVN the program looks up");
    ViewsCase(DRAW_PATH_NVN, "a scissor and a viewport set by number bound and place a quad, "
                             "through the NVN the program looks up");
    DepthsCase(DRAW_PATH_NVN, "depth ranges move a quad across the depth test and depth bounds "
                              "test the target's depth, through the NVN the program looks up");
    RasterCase(DRAW_PATH_NVN, "the rasterizer discards, clamps depth and takes a flat colour "
                              "from the provoking vertex, through the NVN the program looks "
                              "up");
    PointCase(DRAW_PATH_NVN, "a point of sixteen pixels fills its cell, through the NVN the "
                             "program looks up");
    RestartCase(DRAW_PATH_NVN, "a strip with a restart index is two quads, through the NVN the "
                               "program looks up");
    SwizzleCase(DRAW_PATH_NVN, "a viewport swizzle of x and y turns a quad about the diagonal, "
                               "through the NVN the program looks up");
    TilesCase(DRAW_PATH_NVN, "draws around the tiled cache's actions are as without them, "
                             "through the NVN the program looks up");
    AlphaBlendCase(DRAW_PATH_NVN, "the alpha test's reference and the blend colour a list sets "
                                  "decide two draws each, through the NVN the program looks up");
    StencilCase(DRAW_PATH_NVN, "the stencil's reference, compare mask and write mask a list "
                               "sets decide what is drawn, through the NVN the program looks "
                               "up");
    MaskOffsetCase(DRAW_PATH_NVN, "a sample mask without the first sample draws nothing, and a "
                                  "clamp holds a polygon offset, through the NVN the program "
                                  "looks up");
    HintsCase(DRAW_PATH_NVN, "draws under five hints to the graphics unit are as without them, "
                             "through the NVN the program looks up");
    ShadedCase(DRAW_PATH_NVN, "a program whose sample shading is its code's draws and asks for "
                              "no subtile, through the NVN the program looks up");
    ChosenCase(DRAW_PATH_NVN, "draws after subroutines chosen for a program with no linkage map "
                              "are as without them, through the NVN the program looks up");
    ResolvesCase(DRAW_PATH_NVN, "a cleared target of four samples resolves into its colour by "
                                "each of the four downsamples, through the NVN the program looks "
                                "up");
    DepthKeptCase(DRAW_PATH_NVN, "depth tested draws around a depth target's resolve and its "
                                 "culling data's save and restore are as without them, through "
                                 "the NVN the program looks up");
    TouchedCase(DRAW_PATH_NVN, "conservative rasterization draws every pixel a quad touches, "
                               "through the NVN the program looks up");
    DilatedCase(DRAW_PATH_NVN, "a conservative quad dilated by half a pixel reaches the pixels "
                               "a third of one away, through the NVN the program looks up");
    LineCase(DRAW_PATH_NVN, "a line of width four is four rows of pixels, through the NVN the "
                            "program looks up");
    PatchesCase(DRAW_PATH_NVN, "patches of the size and the outer levels a list sets fill "
                               "their cells, through the NVN the program looks up");
    MarksCase(DRAW_PATH_NVN, "draws between debug markers, groups and checkpoints are as "
                             "without them, through the NVN the program looks up");
    DeviceCase(DRAW_PATH_NVN, "the device says its modes and its builder its flags, and clear "
                              "values register, through the NVN the program looks up");
    LongProgramCase(DRAW_PATH_NVN, "a program set from the longer shader data draws, through "
                                   "the NVN the program looks up");
    HandlesCase(DRAW_PATH_NVN, "a texture's handle and a sampler's put together sample, and a "
                               "texel fetch handle fetches, through the NVN the program looks "
                               "up");
}
