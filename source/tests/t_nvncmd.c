// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
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
    DrawBuffer paint;
    DrawBuffer page;
} Kit;

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
static bool KitOpen(Kit *kit, DrawPathKind kind, const char *name) {
    memset(kit, 0, sizeof(*kit));
    DrawPathSpec spec;
    memset(&spec, 0, sizeof(spec));
    spec.width = SCENE_SIZE;
    spec.height = SCENE_SIZE;
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
        !DrawPathAlloc(kit->path, (uint32_t)sizeof(Paint), &kit->paint) ||
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
    memcpy(kit->move.bytes, &still, sizeof(still));
    memcpy(kit->paint.bytes, &flat, sizeof(flat));
    memset(kit->page.bytes, PAGE_PRESET, PAGE_BYTES);
    return true;
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

/// Ends the case: the target read and every pixel judged, a cell of @p drawn (a bit a cell)
/// holding its colour and any other the clear colour, together with @p why, the case's own
/// finding about what the list wrote to memory ("" for none). Takes the way down.
static void KitJudge(Kit *kit, const char *name, uint32_t drawn, const char *why) {
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
            const uint32_t k = y / CELL * CELLS_ACROSS + x / CELL;
            uint8_t want[4];
            if ((drawn >> k & 1u) != 0u) {
                CellColour(k, want);
            } else {
                memcpy(want, SCENE_CLEAR, 4);
            }
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
    const uint32_t cell = first_y / CELL * CELLS_ACROSS + first_x / CELL;
    CHECK_TRUE(SUITE, name, CHECK_EXACT, wrong == 0u && why[0] == '\0',
               "%s%s%u of %u pixels wrong; the first, (%u,%u) in cell %u, which is %s, holds "
               "%02x%02x%02x%02x, expected %02x%02x%02x%02x",
               why, why[0] != '\0' ? "; " : "", (unsigned)wrong,
               (unsigned)(SCENE_SIZE * SCENE_SIZE), (unsigned)first_x, (unsigned)first_y,
               (unsigned)cell, (drawn >> cell & 1u) != 0u ? "drawn" : "left clear", first[0],
               first[1], first[2], first[3], first_want[0], first_want[1], first_want[2],
               first_want[3]);
    DrawPathClose(kit->path);
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
}
