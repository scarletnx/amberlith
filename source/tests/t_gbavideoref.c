// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../check.h"
#include "../gbavideoref.h"
#include "../report.h"
#include "tests.h"

// The GBA video suite's judges against answers worked out by hand from GBATEK's timings and
// memory rules: a frame as the GBA shows it passes, and what a machine that gets one rule wrong
// would show is refused, each with the start of its sentence.

static const char *const SUITE = "reference";

static bool StartsWith(const char *text, const char *start) {
    return strncmp(text, start, strlen(start)) == 0;
}

/// Reads of one line, four of them: two in its drawing, two in its H-Blank, each with the flags the
/// GBA sets on that line. @p vblank_last is the last line the V-Blank flag is set on (226 on the
/// GBA), @p hblank_in_vblank whether the H-Blank flag rises in V-Blank's lines too.
static void AddLine(GbaVideoPoll *poll, uint32_t line, uint32_t *previous, uint32_t vblank_last,
                    bool hblank_in_vblank, uint32_t lyc) {
    for (uint32_t read = 0; read < 4u; ++read) {
        uint32_t status = line << 16;
        if (line >= GBAVIDEO_DRAWN && line <= vblank_last) {
            status |= 0x1u;
        }
        if (read >= 2u && (line < GBAVIDEO_DRAWN || hblank_in_vblank)) {
            status |= 0x2u;
        }
        if (line == lyc) {
            status |= 0x4u;
        }
        GbaVideoPollAdd(poll, status, *previous, 1u);
        *previous = line;
    }
}

/// A poll of two frames from line 0, with lines @p skip left out (GBAVIDEO_LINES for none).
static void Poll(GbaVideoPoll *poll, uint32_t vblank_last, bool hblank_in_vblank, uint32_t skip) {
    memset(poll, 0, sizeof(*poll));
    poll->lyc = 100u;
    uint32_t previous = 0x10000u;
    for (uint32_t frame = 0; frame < 2u; ++frame) {
        for (uint32_t line = 0; line < GBAVIDEO_LINES; ++line) {
            if (line != skip) {
                AddLine(poll, line, &previous, vblank_last, hblank_in_vblank, poll->lyc);
            }
        }
    }
}

static GbaVideoPoll g_poll;

static void CheckLines(void) {
    char said[160];
    Poll(&g_poll, GBAVIDEO_VBLANK_FLAG_LAST, true, GBAVIDEO_LINES);
    const bool right = GbaVideoJudgeCount(&g_poll, said, sizeof(said)) &&
                       strcmp(said, "1824 reads, 455 changes one up, 1 wraps from 227 to 0") == 0 &&
                       GbaVideoJudgeVblankFlag(&g_poll, said, sizeof(said)) &&
                       GbaVideoJudgeHblankFlag(&g_poll, said, sizeof(said)) &&
                       GbaVideoJudgeMatchFlag(&g_poll, said, sizeof(said)) &&
                       strcmp(said, "set on every read of line 100 and none of the others") == 0;
    // A machine that keeps the V-Blank flag through line 227, as the line counter's wrap clears it
    Poll(&g_poll, GBAVIDEO_LINES - 1u, true, GBAVIDEO_LINES);
    const bool late_clear = !GbaVideoJudgeVblankFlag(&g_poll, said, sizeof(said)) &&
                            StartsWith(said, "line 227 had the V-Blank flag set on 8 of its 8");
    // One whose H-Blank flag rises only in the drawn lines
    Poll(&g_poll, GBAVIDEO_VBLANK_FLAG_LAST, false, GBAVIDEO_LINES);
    const bool drawn_only = !GbaVideoJudgeHblankFlag(&g_poll, said, sizeof(said)) &&
                            StartsWith(said,
                                       "line 160 had the H-Blank flag set on 0 and clear on 8");
    // One whose counter skips a line
    Poll(&g_poll, GBAVIDEO_VBLANK_FLAG_LAST, true, 100u);
    const bool skipped = !GbaVideoJudgeCount(&g_poll, said, sizeof(said)) &&
                         StartsWith(said, "2 of 453 changes of VCOUNT were not one up nor 227 to "
                                          "0, the first 99 to 101") &&
                         !GbaVideoJudgeMatchFlag(&g_poll, said, sizeof(said));
    // One whose frame has a 229th line
    Poll(&g_poll, GBAVIDEO_VBLANK_FLAG_LAST, true, GBAVIDEO_LINES);
    uint32_t previous = GBAVIDEO_LINES - 1u;
    GbaVideoPollAdd(&g_poll, 228u << 16, previous, 1u);
    const bool past = !GbaVideoJudgeCount(&g_poll, said, sizeof(said)) &&
                      StartsWith(said, "1 of 1825 reads had VCOUNT past 227, the first 228");
    // The same frames gathered as runs of identical reads, three in a line's drawing and two in its
    // H-Blank, as the suite gathers them: the same verdicts, the reads counted
    memset(&g_poll, 0, sizeof(g_poll));
    g_poll.lyc = 100u;
    previous = 0x10000u;
    for (uint32_t frame = 0; frame < 2u; ++frame) {
        for (uint32_t line = 0; line < GBAVIDEO_LINES; ++line) {
            const bool vblank = line >= GBAVIDEO_DRAWN && line <= GBAVIDEO_VBLANK_FLAG_LAST;
            const uint32_t flags = (vblank ? 0x1u : 0u) | (line == 100u ? 0x4u : 0u);
            GbaVideoPollAdd(&g_poll, (line << 16) | flags, previous, 3u);
            GbaVideoPollAdd(&g_poll, (line << 16) | flags | 0x2u, line, 2u);
            previous = line;
        }
    }
    const bool runs = GbaVideoJudgeCount(&g_poll, said, sizeof(said)) &&
                      strcmp(said, "2280 reads, 455 changes one up, 1 wraps from 227 to 0") == 0 &&
                      GbaVideoJudgeVblankFlag(&g_poll, said, sizeof(said)) &&
                      GbaVideoJudgeHblankFlag(&g_poll, said, sizeof(said)) &&
                      GbaVideoJudgeMatchFlag(&g_poll, said, sizeof(said));
    // One that sets the V-Counter flag a line late
    Poll(&g_poll, GBAVIDEO_VBLANK_FLAG_LAST, true, GBAVIDEO_LINES);
    g_poll.lyc = 99u;
    const bool late_match = !GbaVideoJudgeMatchFlag(&g_poll, said, sizeof(said)) &&
                            StartsWith(said, "line 99, with line 99 named, had the V-Counter flag "
                                             "set on 0 of its 8 reads");
    CHECK_TRUE(SUITE,
               "gbavideo: the line and flag judges pass the GBA's frames and refuse five wrong "
               "ones, each with its sentence",
               CHECK_NONE,
               right && runs && late_clear && drawn_only && skipped && past && late_match,
               "right %d, as runs %d, V-Blank through 227 %d, H-Blank drawn only %d, skipped %d, "
               "line 228 %d, late match %d",
               right, runs, late_clear, drawn_only, skipped, past, late_match);
}

static void CheckFrames(void) {
    char said[160];
    static const uint32_t RIGHT[] = { 280896u, 280896u + 128u, 280896u - 100u };
    const bool right = GbaVideoJudgeFrames(RIGHT, 3u, said, sizeof(said)) &&
                       strcmp(said, "frames of 280896 281024 280796 cycles; 280896 within 128") ==
                           0;
    // A machine whose line is 1228 cycles: 912 short a frame
    static const uint32_t SHORT_LINES[] = { 228u * 1228u };
    const bool short_lines = !GbaVideoJudgeFrames(SHORT_LINES, 1u, said, sizeof(said)) &&
                             strcmp(said, "frames of 279984 cycles; 280896 within 128 expected") ==
                                 0;
    static const uint32_t JUST_OUT[] = { 280896u, 280896u + 129u };
    const bool just_out = !GbaVideoJudgeFrames(JUST_OUT, 2u, said, sizeof(said));
    const bool none = !GbaVideoJudgeFrames(RIGHT, 0u, said, sizeof(said));
    CHECK_TRUE(SUITE,
               "gbavideo: the frame judge takes 280896 cycles within 128 and refuses lines of 1228 "
               "cycles",
               CHECK_NONE, right && short_lines && just_out && none,
               "right %d, 1228-cycle lines %d, 129 over %d, no frame %d", right, short_lines,
               just_out, none);
}

static void CheckIrqs(void) {
    char said[160];
    GbaVideoIrqs irqs = { { 0 }, 0 };
    irqs.per_line[160] = 4u;
    const bool on_line = GbaVideoJudgeIrqOnLine(&irqs, 160u, 4u, said, sizeof(said)) &&
                         strcmp(said, "4, each on line 160") == 0;
    // A machine that raises V-Blank's interrupt a line late
    irqs.per_line[160] = 0u;
    irqs.per_line[161] = 4u;
    const bool late = !GbaVideoJudgeIrqOnLine(&irqs, 160u, 4u, said, sizeof(said)) &&
                      StartsWith(said, "0 of 4 expected on line 160, 4 in all, 4 of them on line "
                                       "161");
    // One that raises one more on line 0
    irqs.per_line[161] = 0u;
    irqs.per_line[160] = 4u;
    irqs.per_line[0] = 1u;
    const bool extra = !GbaVideoJudgeIrqOnLine(&irqs, 160u, 4u, said, sizeof(said));
    memset(&irqs, 0, sizeof(irqs));
    for (uint32_t i = 0; i < GBAVIDEO_LINES; ++i) {
        irqs.per_line[i] = 1u;
    }
    const bool each = GbaVideoJudgeIrqEachLine(&irqs, 0u, 159u, 1u, said, sizeof(said)) &&
                      strcmp(said, "1 on each of lines 0 to 159; 68 outside them, not judged") == 0;
    // One that takes line 37's twice, and one that loses line 159's
    irqs.per_line[37] = 2u;
    const bool twice = !GbaVideoJudgeIrqEachLine(&irqs, 0u, 159u, 1u, said, sizeof(said)) &&
                       StartsWith(said, "line 37 had 2, 1 expected on each of lines 0 to 159");
    irqs.per_line[37] = 1u;
    irqs.per_line[159] = 0u;
    const bool lost = !GbaVideoJudgeIrqEachLine(&irqs, 0u, 159u, 1u, said, sizeof(said));
    CHECK_TRUE(SUITE,
               "gbavideo: the interrupt judges take one a line and one a frame on its line, and "
               "refuse one late, one more and one lost",
               CHECK_NONE, on_line && late && extra && each && twice && lost,
               "on line %d, late %d, extra %d, each line %d, twice %d, lost %d", on_line, late,
               extra, each, twice, lost);
}

static void CheckMemory(void) {
    char said[160];
    // The 96 KiB repeat every 128 KiB, the last 32 KiB of each a second OBJ 32 KiB
    const bool offsets = GbaVideoVramOffset(0x06000000u) == 0u &&
                         GbaVideoVramOffset(0x06017FFEu) == 0x17FFEu &&
                         GbaVideoVramOffset(0x06018000u) == 0x10000u &&
                         GbaVideoVramOffset(0x0601FFFEu) == 0x17FFEu &&
                         GbaVideoVramOffset(0x06020000u) == 0u &&
                         GbaVideoVramOffset(0x0603A000u) == 0x12000u;
    // GBATEK's "Writing 8bit Data to Video Memory": BG VRAM and palette take the byte twice, OBJ
    // VRAM and OAM ignore it; BG VRAM reaches 0x6013FFF in the bitmap modes
    const bool rule = GbaVideoByteStore(0u, 0x0600E001u, 0xBEEFu, 0x5Au) == 0x5A5Au &&
                      GbaVideoByteStore(0u, 0x06012001u, 0xBEEFu, 0x5Au) == 0xBEEFu &&
                      GbaVideoByteStore(2u, 0x06012001u, 0xBEEFu, 0x5Au) == 0xBEEFu &&
                      GbaVideoByteStore(3u, 0x06012001u, 0xBEEFu, 0x5Au) == 0x5A5Au &&
                      GbaVideoByteStore(5u, 0x06013FFFu, 0xBEEFu, 0x5Au) == 0x5A5Au &&
                      GbaVideoByteStore(5u, 0x06014000u, 0xBEEFu, 0x5Au) == 0xBEEFu &&
                      GbaVideoByteStore(0u, 0x05000203u, 0x1234u, 0x5Au) == 0x5A5Au &&
                      GbaVideoByteStore(4u, 0x070003FDu, 0x1234u, 0x5Au) == 0x1234u;
    GbaVideoStore bytes[] = {
        { 0x0600E001u, 0x0600E001u, 0xBEEFu, 0x5Au, 0x5A5Au },
        { 0x06012001u, 0x06012001u, 0xBEEFu, 0x5Au, 0xBEEFu },
        { 0x05000203u, 0x05000203u, 0x1234u, 0x5Au, 0x5A5Au },
        { 0x070003FDu, 0x070003FDu, 0x1234u, 0x5Au, 0x1234u },
    };
    const bool right = GbaVideoJudgeBytes(0u, bytes, 4u, said, sizeof(said)) &&
                       strcmp(said, "mode 0: 4 bytes, each halfword as the memory's rule leaves "
                                    "it") == 0;
    // In mode 3 the second address is BG VRAM: left unchanged, it is refused
    const bool bitmap = !GbaVideoJudgeBytes(3u, bytes, 4u, said, sizeof(said)) &&
                        StartsWith(said, "mode 3: byte 0x5a stored at 0x06012001 left its "
                                         "halfword 0xbeef, from 0xbeef; 0x5a5a expected");
    // A machine that stores the byte alone, in its half of the halfword
    bytes[0].after = 0x5AEFu;
    const bool lone_byte = !GbaVideoJudgeBytes(0u, bytes, 4u, said, sizeof(said));
    // One that writes the byte twice into OAM too
    bytes[0].after = 0x5A5Au;
    bytes[3].after = 0x5A5Au;
    const bool oam = !GbaVideoJudgeBytes(0u, bytes, 4u, said, sizeof(said));
    GbaVideoStore halves[] = {
        { 0x06016000u, 0x0601E000u, 0x0000u, 0xC3A5u, 0xC3A5u },
        { 0x0600E000u, 0x0602E000u, 0x1111u, 0x5AA5u, 0x5AA5u },
    };
    const bool mirrored = GbaVideoJudgeHalfwords(halves, 2u, said, sizeof(said));
    // A store lost while a line was drawn
    halves[1].after = 0x1111u;
    const bool lost = !GbaVideoJudgeHalfwords(halves, 2u, said, sizeof(said)) &&
                      strcmp(said, "0x5aa5 stored at 0x0600e000 read back 0x1111 at 0x0602e000 "
                                   "(it held 0x1111 before)") == 0;
    CHECK_TRUE(SUITE,
               "gbavideo: the memory judges take the GBA's byte rule and mirrors, and refuse a "
               "lone byte, OAM taking one and a lost store",
               CHECK_NONE,
               offsets && rule && right && bitmap && lone_byte && oam && mirrored && lost,
               "offsets %d, byte rule %d, right %d, bitmap mode %d, lone byte %d, OAM %d, "
               "mirrors %d, lost %d",
               offsets, rule, right, bitmap, lone_byte, oam, mirrored, lost);
}

void RunReferenceGbaVideo(void) {
    CheckLines();
    CheckFrames();
    CheckIrqs();
    CheckMemory();
}
