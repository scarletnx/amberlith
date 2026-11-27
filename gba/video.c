// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The GBA's video suite (docs/families/Graphics.md#gbavideo): what the display shows cannot be
// read back, so each case asks what a program can read, as GBATEK documents it: the line counter
// and DISPSTAT's flags polled over two frames, a frame's length on two chained timers, the
// V-Blank, H-Blank and V-Counter interrupts by the line they come on, and halfword and byte stores
// to VRAM, palette RAM and OAM in each display mode. The judges are source/gbavideoref.c's.

#include <gba_interrupt.h>
#include <stdint.h>

#include "check.h"
#include "gbavideoref.h"
#include "report.h"
#include "video.h"

// The registers, as GBATEK maps them: DISPSTAT and VCOUNT side by side, so one 32-bit load reads
// both at once
#define GBA_DISPCNT ((volatile uint16_t *)0x04000000)
#define GBA_DISPSTAT ((volatile uint16_t *)0x04000004)
#define GBA_STATUS ((volatile uint32_t *)0x04000004)
#define GBA_TM0_COUNT ((volatile uint16_t *)0x04000100)
#define GBA_TM0_CONTROL ((volatile uint16_t *)0x04000102)
#define GBA_TM1_COUNT ((volatile uint16_t *)0x04000104)
#define GBA_TM1_CONTROL ((volatile uint16_t *)0x04000106)
#define GBA_TIMER_CASCADE 0x4u
#define GBA_TIMER_START 0x80u
#define GBA_STATUS_VBLANK 0x1u
#define GBA_STATUS_HBLANK 0x2u
#define GBA_STATUS_MATCH 0x4u
#define GBA_DISPCNT_MODE 0x7u
#define GBA_MODES 6u

// The most reads a wait makes before it gives up, some seconds' worth: on a machine whose counter
// stands still, the case fails saying so rather than the run never ending
#define VIDEO_READS_MAX 4000000u
// The lines the poll's and the interrupt's V-Count setting name
#define VIDEO_POLL_LYC 100u
#define VIDEO_IRQ_LYC 80u
// The frames the V-Blank and V-Counter interrupts are counted over
#define VIDEO_IRQ_FRAMES 4u

static const char *const SUITE = "gbavideo";

// Halfwords each mode's stores go to: BG VRAM, OBJ VRAM, OBJ palette entry 1 and the last OBJ's
// third attribute, none of them what the frontend's text console uses; each is put back after
static const uint32_t HALFWORD_AT[] = { 0x0600E000u, 0x06016000u, 0x05000202u, 0x070003FCu };
#define HALFWORDS (sizeof(HALFWORD_AT) / sizeof(HALFWORD_AT[0]))
// Bytes each mode stores: BG VRAM, the 16 KiB that are OBJ VRAM in modes 0 to 2 and BG VRAM in the
// bitmap modes, OBJ VRAM in every mode, the palette and OAM, each at an odd address
static const uint32_t BYTE_AT[] = { 0x0600E001u, 0x06012001u, 0x06016001u, 0x05000203u,
                                    0x070003FDu };
#define BYTES (sizeof(BYTE_AT) / sizeof(BYTE_AT[0]))
// What a byte store finds in its halfword, so either half changing shows
#define BYTE_BEFORE 0xBEEFu
// Stores through the mirrors: where each is written, and where it is read back
static const uint32_t MIRROR_AT[][2] = {
    { 0x06016000u, 0x0601E000u },
    { 0x0601E002u, 0x06016002u },
    { 0x0600E000u, 0x0602E000u },
};
#define MIRRORS (sizeof(MIRROR_AT) / sizeof(MIRROR_AT[0]))

typedef enum {
    WINDOW_DRAWING,
    WINDOW_HBLANK,
    WINDOW_VBLANK,
    WINDOWS,
} Window;

static struct {
    GbaVideoPoll poll;
    // What the interrupt handlers count, by line
    volatile uint32_t irq_lines[GBAVIDEO_LINES];
    volatile uint32_t irq_past;
    GbaVideoIrqs irqs;
    uint32_t frames[GBAVIDEO_FRAMES];
    GbaVideoStore stores[HALFWORDS * WINDOWS];
} g_video;

static volatile uint16_t *Half(uint32_t address) {
    return (volatile uint16_t *)(uintptr_t)address;
}

static uint32_t Status(void) {
    return *GBA_STATUS;
}

/// Waits until VCOUNT goes down, from 227 to 0 on a correct machine; false when it did not within
/// VIDEO_READS_MAX reads.
static bool WaitFrameStart(void) {
    uint32_t previous = Status() >> 16;
    for (uint32_t i = 0; i < VIDEO_READS_MAX; ++i) {
        const uint32_t line = Status() >> 16;
        if (line < previous) {
            return true;
        }
        previous = line;
    }
    return false;
}

/// Waits until @p flag of DISPSTAT goes from clear to set, on a drawn line when @p drawn.
static bool WaitRise(uint32_t flag, bool drawn) {
    bool was = (Status() & flag) != 0u;
    for (uint32_t i = 0; i < VIDEO_READS_MAX; ++i) {
        const uint32_t status = Status();
        const bool now = (status & flag) != 0u;
        if (now && !was && (!drawn || (status >> 16) < GBAVIDEO_DRAWN)) {
            return true;
        }
        was = now;
    }
    return false;
}

/// Waits until the H-Blank flag falls on a drawn line, the start of that line's drawing.
static bool WaitDrawing(void) {
    bool was = (Status() & GBA_STATUS_HBLANK) != 0u;
    for (uint32_t i = 0; i < VIDEO_READS_MAX; ++i) {
        const uint32_t status = Status();
        const bool now = (status & GBA_STATUS_HBLANK) != 0u;
        if (!now && was && (status >> 16) < GBAVIDEO_DRAWN - 1u) {
            return true;
        }
        was = now;
    }
    return false;
}

static bool WaitWindow(Window window) {
    switch (window) {
    case WINDOW_DRAWING:
        return WaitDrawing();
    case WINDOW_HBLANK:
        return WaitRise(GBA_STATUS_HBLANK, true);
    default:
        return WaitRise(GBA_STATUS_VBLANK, false);
    }
}

static void SetLyc(uint32_t line) {
    *GBA_DISPSTAT = (uint16_t)((*GBA_DISPSTAT & 0xFFu) | (line << 8));
}

// ---- the line counter and the flags, polled ----

static void Lines(void) {
    static const char *const NAMES[] = {
        "VCOUNT counts every line from 0 to 227, one up at a time, and 227 is followed by 0",
        "the V-Blank flag is set on lines 160 to 226 and on no other line",
        "the H-Blank flag goes both clear and set within every line, 0 to 227",
        "the V-Counter flag is set on the line DISPSTAT names and on no other line",
    };
    bool (*const JUDGES[])(const GbaVideoPoll *, char *, size_t) = {
        GbaVideoJudgeCount,
        GbaVideoJudgeVblankFlag,
        GbaVideoJudgeHblankFlag,
        GbaVideoJudgeMatchFlag,
    };
    bool started[4] = {};
    bool any = false;
    for (uint32_t i = 0; i < 4u; ++i) {
        started[i] = ReportStart(SUITE, NAMES[i]);
        any = any || started[i];
    }
    if (!any) {
        return;
    }
    const uint16_t ime = REG_IME;
    const uint16_t dispstat = *GBA_DISPSTAT;
    REG_IME = 0;
    SetLyc(VIDEO_POLL_LYC);
    g_video.poll = (GbaVideoPoll){ .lyc = VIDEO_POLL_LYC };
    const bool began = WaitFrameStart();
    uint32_t previous = 0x10000u;
    while (began && g_video.poll.wraps < 2u && g_video.poll.samples < VIDEO_READS_MAX) {
        const uint32_t status = Status();
        GbaVideoPollAdd(&g_video.poll, status, previous);
        previous = status >> 16;
    }
    *GBA_DISPSTAT = dispstat;
    REG_IME = ime;
    for (uint32_t i = 0; i < 4u; ++i) {
        if (!started[i]) {
            continue;
        }
        char said[160];
        const bool passed = began && JUDGES[i](&g_video.poll, said, sizeof(said));
        CHECK_TRUE(SUITE, NAMES[i], CHECK_EXACT, passed, "%s",
                   began ? said : "VCOUNT never went down from 227 to 0 to start a frame");
    }
}

// ---- a frame's length ----

static uint32_t Cycles(void) {
    uint16_t high;
    uint16_t low;
    do {
        high = *GBA_TM1_COUNT;
        low = *GBA_TM0_COUNT;
    } while (high != *GBA_TM1_COUNT);
    return ((uint32_t)high << 16) | low;
}

static void Frames(void) {
    static const char *const NAME = "a frame lasts 280896 cycles, from one V-Blank to the next";
    if (!ReportStart(SUITE, NAME)) {
        return;
    }
    const uint16_t ime = REG_IME;
    REG_IME = 0;
    *GBA_TM0_CONTROL = 0;
    *GBA_TM1_CONTROL = 0;
    *GBA_TM0_COUNT = 0;
    *GBA_TM1_COUNT = 0;
    // TM1 counts TM0's overflows, TM0 every cycle: a 32-bit count of cycles
    *GBA_TM1_CONTROL = GBA_TIMER_START | GBA_TIMER_CASCADE;
    *GBA_TM0_CONTROL = GBA_TIMER_START;
    bool risen = WaitRise(GBA_STATUS_VBLANK, false);
    uint32_t at = Cycles();
    uint32_t measured = 0;
    while (risen && measured < GBAVIDEO_FRAMES) {
        risen = WaitRise(GBA_STATUS_VBLANK, false);
        const uint32_t now = Cycles();
        g_video.frames[measured++] = now - at;
        at = now;
    }
    *GBA_TM0_CONTROL = 0;
    *GBA_TM1_CONTROL = 0;
    REG_IME = ime;
    char said[160];
    const bool passed = risen && GbaVideoJudgeFrames(g_video.frames, measured, said, sizeof(said));
    CHECK_TRUE(SUITE, NAME, CHECK_BOUND, passed, "%s",
               risen ? said : "the V-Blank flag stopped rising before three frames were measured");
}

// ---- the interrupts ----

/// The line an interrupt came on: the one VCOUNT names while @p flag, set through that line, still
/// is, else the one before, which the handler was entered past.
static void CountIrq(uint32_t flag) {
    const uint32_t status = Status();
    uint32_t line = status >> 16;
    if (flag != 0u && (status & flag) == 0u) {
        line = line == 0u ? GBAVIDEO_LINES - 1u : line - 1u;
    }
    if (line < GBAVIDEO_LINES) {
        g_video.irq_lines[line] = g_video.irq_lines[line] + 1u;
    } else {
        g_video.irq_past = g_video.irq_past + 1u;
    }
}

static void OnVblank(void) {
    CountIrq(0u);
}

static void OnHblank(void) {
    CountIrq(GBA_STATUS_HBLANK);
}

static void OnMatch(void) {
    CountIrq(GBA_STATUS_MATCH);
}

/// Counts @p mask's interrupts, @p handler counting them, from the start of a frame through
/// @p frames frames (from one V-Blank's start to the next when @p from_vblank), into g_video.irqs;
/// false when the frames never came.
static bool CountIrqs(irqMASK mask, IntFn handler, uint32_t frames, bool from_vblank) {
    const uint16_t ime = REG_IME;
    REG_IME = 0;
    irqSet(mask, handler);
    irqEnable((int)mask);
    for (uint32_t i = 0; i < GBAVIDEO_LINES; ++i) {
        g_video.irq_lines[i] = 0;
    }
    g_video.irq_past = 0;
    bool came = from_vblank ? WaitRise(GBA_STATUS_VBLANK, false) : WaitFrameStart();
    // What was raised while the master enable was off is not this window's
    REG_IF = (uint16_t)mask;
    REG_IME = 1;
    for (uint32_t i = 0; came && i < frames; ++i) {
        came = from_vblank ? WaitRise(GBA_STATUS_VBLANK, false) : WaitFrameStart();
    }
    REG_IME = 0;
    if (mask != IRQ_VBLANK) {
        irqDisable((int)mask);
    }
    irqSet(mask, nullptr);
    REG_IF = (uint16_t)mask;
    REG_IME = ime;
    for (uint32_t i = 0; i < GBAVIDEO_LINES; ++i) {
        g_video.irqs.per_line[i] = g_video.irq_lines[i];
    }
    g_video.irqs.past = g_video.irq_past;
    return came;
}

static void Irqs(void) {
    static const char *const VBLANK = "the V-Blank interrupt comes once a frame, on line 160";
    static const char *const HBLANK = "the H-Blank interrupt comes once in each drawn line, 0 to 159";
    static const char *const MATCH =
        "the V-Counter interrupt comes once a frame, on the line DISPSTAT names";
    static const char *const NEVER = "the frames to count over never came";
    char said[160];
    if (ReportStart(SUITE, VBLANK)) {
        const bool came = CountIrqs(IRQ_VBLANK, OnVblank, VIDEO_IRQ_FRAMES, false);
        const bool passed = came && GbaVideoJudgeIrqOnLine(&g_video.irqs, GBAVIDEO_DRAWN,
                                                           VIDEO_IRQ_FRAMES, said, sizeof(said));
        CHECK_TRUE(SUITE, VBLANK, CHECK_EXACT, passed, "%s", came ? said : NEVER);
    }
    if (ReportStart(SUITE, HBLANK)) {
        const bool came = CountIrqs(IRQ_HBLANK, OnHblank, 1u, true);
        const bool passed = came && GbaVideoJudgeIrqEachLine(&g_video.irqs, 0u,
                                                             GBAVIDEO_DRAWN - 1u, 1u, said,
                                                             sizeof(said));
        CHECK_TRUE(SUITE, HBLANK, CHECK_EXACT, passed, "%s", came ? said : NEVER);
    }
    if (ReportStart(SUITE, MATCH)) {
        const uint16_t dispstat = *GBA_DISPSTAT;
        SetLyc(VIDEO_IRQ_LYC);
        const bool came = CountIrqs(IRQ_VCOUNT, OnMatch, VIDEO_IRQ_FRAMES, false);
        *GBA_DISPSTAT = (uint16_t)((*GBA_DISPSTAT & 0xFFu) | (dispstat & 0xFF00u));
        const bool passed = came && GbaVideoJudgeIrqOnLine(&g_video.irqs, VIDEO_IRQ_LYC,
                                                           VIDEO_IRQ_FRAMES, said, sizeof(said));
        CHECK_TRUE(SUITE, MATCH, CHECK_EXACT, passed, "%s", came ? said : NEVER);
    }
}

// ---- video memory ----

static void Halfwords(uint32_t mode) {
    static const char *const NAMES[GBA_MODES] = {
        "mode 0: halfwords stored to VRAM, palette RAM and OAM while drawing, in H-Blank and in "
        "V-Blank read back as stored",
        "mode 1: halfwords stored to VRAM, palette RAM and OAM while drawing, in H-Blank and in "
        "V-Blank read back as stored",
        "mode 2: halfwords stored to VRAM, palette RAM and OAM while drawing, in H-Blank and in "
        "V-Blank read back as stored",
        "mode 3: halfwords stored to VRAM, palette RAM and OAM while drawing, in H-Blank and in "
        "V-Blank read back as stored",
        "mode 4: halfwords stored to VRAM, palette RAM and OAM while drawing, in H-Blank and in "
        "V-Blank read back as stored",
        "mode 5: halfwords stored to VRAM, palette RAM and OAM while drawing, in H-Blank and in "
        "V-Blank read back as stored",
    };
    if (!ReportStart(SUITE, NAMES[mode])) {
        return;
    }
    size_t count = 0;
    bool came = true;
    for (uint32_t w = 0; came && w < WINDOWS; ++w) {
        came = WaitWindow((Window)w);
        for (uint32_t i = 0; came && i < HALFWORDS; ++i) {
            volatile uint16_t *const at = Half(HALFWORD_AT[i]);
            const uint16_t stored = (uint16_t)(0xA55Au ^ (mode << 12) ^ (w << 8) ^ (i << 4));
            const uint16_t before = *at;
            *at = stored;
            const uint16_t after = *at;
            *at = before;
            g_video.stores[count++] = (GbaVideoStore){ .address = HALFWORD_AT[i],
                                                       .read_at = HALFWORD_AT[i],
                                                       .before = before,
                                                       .stored = stored,
                                                       .after = after };
        }
    }
    char said[160];
    const bool passed = came && GbaVideoJudgeHalfwords(g_video.stores, count, said, sizeof(said));
    CHECK_TRUE(SUITE, NAMES[mode], CHECK_EXACT, passed, "%s",
               came ? said : "a drawn line, an H-Blank or a V-Blank never came");
}

static void Bytes(uint32_t mode) {
    static const char *const NAMES[GBA_MODES] = {
        "mode 0: a byte stored to BG VRAM or palette RAM fills both halves of its halfword, one "
        "to OBJ VRAM or OAM is ignored",
        "mode 1: a byte stored to BG VRAM or palette RAM fills both halves of its halfword, one "
        "to OBJ VRAM or OAM is ignored",
        "mode 2: a byte stored to BG VRAM or palette RAM fills both halves of its halfword, one "
        "to OBJ VRAM or OAM is ignored",
        "mode 3: a byte stored to BG VRAM or palette RAM fills both halves of its halfword, one "
        "to OBJ VRAM or OAM is ignored",
        "mode 4: a byte stored to BG VRAM or palette RAM fills both halves of its halfword, one "
        "to OBJ VRAM or OAM is ignored",
        "mode 5: a byte stored to BG VRAM or palette RAM fills both halves of its halfword, one "
        "to OBJ VRAM or OAM is ignored",
    };
    if (!ReportStart(SUITE, NAMES[mode])) {
        return;
    }
    const uint8_t value = (uint8_t)(0x50u + mode);
    for (uint32_t i = 0; i < BYTES; ++i) {
        const uint32_t address = BYTE_AT[i];
        volatile uint16_t *const half = Half(address & ~1u);
        const uint16_t kept = *half;
        *half = BYTE_BEFORE;
        *(volatile uint8_t *)(uintptr_t)address = value;
        const uint16_t after = *half;
        *half = kept;
        g_video.stores[i] = (GbaVideoStore){
            .address = address, .read_at = address, .before = BYTE_BEFORE, .stored = value,
            .after = after
        };
    }
    char said[160];
    const bool passed = GbaVideoJudgeBytes(mode, g_video.stores, BYTES, said, sizeof(said));
    CHECK_TRUE(SUITE, NAMES[mode], CHECK_EXACT, passed, "%s", said);
}

static void Mirrors(void) {
    static const char *const NAME =
        "VRAM's 96 KiB repeat every 128 KiB, the last 32 KiB of each a second OBJ 32 KiB";
    if (!ReportStart(SUITE, NAME)) {
        return;
    }
    for (uint32_t i = 0; i < MIRRORS; ++i) {
        volatile uint16_t *const written = Half(MIRROR_AT[i][0]);
        volatile uint16_t *const read = Half(MIRROR_AT[i][1]);
        const uint16_t stored = (uint16_t)(0xC3A5u ^ (i * 0x1111u));
        const uint16_t before = *written;
        *written = stored;
        const uint16_t after = *read;
        *written = before;
        g_video.stores[i] = (GbaVideoStore){ .address = MIRROR_AT[i][0],
                                             .read_at = MIRROR_AT[i][1],
                                             .before = before,
                                             .stored = stored,
                                             .after = after };
    }
    char said[160];
    const bool passed = GbaVideoJudgeHalfwords(g_video.stores, MIRRORS, said, sizeof(said));
    CHECK_TRUE(SUITE, NAME, CHECK_EXACT, passed, "%s", said);
}

void RunGbaVideo(void) {
    Lines();
    Frames();
    Irqs();
    const uint16_t dispcnt = *GBA_DISPCNT;
    for (uint32_t mode = 0; mode < GBA_MODES; ++mode) {
        *GBA_DISPCNT = (uint16_t)((dispcnt & ~GBA_DISPCNT_MODE) | mode);
        Halfwords(mode);
        Bytes(mode);
    }
    *GBA_DISPCNT = dispcnt;
    Mirrors();
}
