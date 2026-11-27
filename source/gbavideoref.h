// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// The judges of the GBA's video suite (gba/video.c, docs/families/Graphics.md#gbavideo): what the
/// GBA's display shows cannot be read back, so its cases read what a program can, the line
/// counter, the status flags, the interrupts and the video memory, and these judge it by the GBA's
/// documented timings and memory rules (GBATEK's "LCD Dimensions and Timings", "LCD I/O
/// Interrupts and Status" and "Writing 8bit Data to Video Memory"). Pure functions of what the
/// suite gathered, touching nothing but snprintf, so the reference suite pins them on the host.

/// A frame: 228 lines of 1232 cycles, the first 160 drawn, the rest V-Blank.
#define GBAVIDEO_LINES 228u
#define GBAVIDEO_DRAWN 160u
#define GBAVIDEO_LINE_CYCLES 1232u
#define GBAVIDEO_FRAME_CYCLES 280896u
_Static_assert(GBAVIDEO_FRAME_CYCLES == GBAVIDEO_LINES * GBAVIDEO_LINE_CYCLES,
               "a frame is 228 lines of 1232 cycles");
/// The V-Blank flag's last line: set on lines 160 to 226, clear on 227.
#define GBAVIDEO_VBLANK_FLAG_LAST 226u
/// How far a frame measured by polling may be from 280896 cycles: the polling loop's turn at each
/// end, well under the 912 cycles a line four cycles short would take from a frame.
#define GBAVIDEO_FRAME_SLACK 128u
/// The frames the frame case measures.
#define GBAVIDEO_FRAMES 3u

/// What reading DISPSTAT and VCOUNT together, in one 32-bit load, saw while VCOUNT named a line:
/// how many reads, and how many of them had each flag set and clear.
typedef struct {
    uint32_t samples;
    uint32_t vblank_set;
    uint32_t vblank_clear;
    uint32_t hblank_set;
    uint32_t hblank_clear;
    uint32_t match_set;
    uint32_t match_clear;
} GbaVideoLine;

/// A poll of DISPSTAT and VCOUNT over at least one whole frame.
typedef struct {
    GbaVideoLine lines[GBAVIDEO_LINES];
    uint32_t samples;
    /// Reads whose VCOUNT named no line (past 227), and the first such value.
    uint32_t past;
    uint32_t first_past;
    /// Changes of VCOUNT between two reads; those from 227 to 0; those neither one up nor that
    /// wrap, and the first of them.
    uint32_t steps;
    uint32_t wraps;
    uint32_t bad_steps;
    uint32_t bad_from;
    uint32_t bad_to;
    /// The line DISPSTAT's V-Count setting named during the poll.
    uint32_t lyc;
} GbaVideoPoll;

/// Adds @p reads reads in a row of DISPSTAT (the low half) and VCOUNT (the high half) that all
/// gave @p status to @p poll, @p previous the VCOUNT the read before them saw, or a value past
/// 0xFFFF for the first.
void GbaVideoPollAdd(GbaVideoPoll *poll, uint32_t status, uint32_t previous, uint32_t reads);

/// VCOUNT takes every value from 0 to 227, one up each change, 227 followed by 0, and nothing past.
bool GbaVideoJudgeCount(const GbaVideoPoll *poll, char *out, size_t out_size);
/// The V-Blank flag is set on every read of lines 160 to 226 and on none of the others.
bool GbaVideoJudgeVblankFlag(const GbaVideoPoll *poll, char *out, size_t out_size);
/// The H-Blank flag is seen both clear and set within every line, 0 to 227.
bool GbaVideoJudgeHblankFlag(const GbaVideoPoll *poll, char *out, size_t out_size);
/// The V-Counter flag is set on every read of the line the V-Count setting names and on none of
/// the others.
bool GbaVideoJudgeMatchFlag(const GbaVideoPoll *poll, char *out, size_t out_size);

/// @p count frames, each in cycles from one V-Blank flag's rise to the next: each within
/// GBAVIDEO_FRAME_SLACK of 280896.
bool GbaVideoJudgeFrames(const uint32_t *cycles, size_t count, char *out, size_t out_size);

/// The interrupts one source raised, counted by the line each came on: @p per_line, and the ones
/// taken on no line (VCOUNT past 227).
typedef struct {
    uint32_t per_line[GBAVIDEO_LINES];
    uint32_t past;
} GbaVideoIrqs;

/// Every interrupt came on @p line, @p times of them.
bool GbaVideoJudgeIrqOnLine(const GbaVideoIrqs *irqs, uint32_t line, uint32_t times, char *out,
                            size_t out_size);
/// Each line from @p first to @p last had @p times of them; the lines outside are not judged, and
/// the sentence says how many came there.
bool GbaVideoJudgeIrqEachLine(const GbaVideoIrqs *irqs, uint32_t first, uint32_t last,
                              uint32_t times, char *out, size_t out_size);

/// The offset into the 96 KiB of VRAM an address from 0x06000000 to 0x06FFFFFF reaches: the 96
/// KiB repeat every 128 KiB, the last 32 KiB of each 128 a second copy of the OBJ 32 KiB.
uint32_t GbaVideoVramOffset(uint32_t address);

/// The halfword at @p address & ~1 after a byte store of @p value at @p address, from what it was
/// before, in display mode @p mode: palette RAM and BG VRAM take the byte in both halves of the
/// halfword; OBJ VRAM and OAM ignore it. BG VRAM is the first 64 KiB in modes 0 to 2 and the first
/// 80 KiB in the bitmap modes 3 to 5, OBJ VRAM the rest.
uint16_t GbaVideoByteStore(uint32_t mode, uint32_t address, uint16_t before, uint8_t value);

/// One store a case made at @p address and what reading the halfword back at @p read_at gave: the
/// same address, or another that reaches the same memory.
typedef struct {
    uint32_t address;
    uint32_t read_at;
    uint16_t before;
    uint16_t stored;
    uint16_t after;
} GbaVideoStore;

/// Halfword stores: each read back as stored, where it was read.
bool GbaVideoJudgeHalfwords(const GbaVideoStore *stores, size_t count, char *out,
                            size_t out_size);
/// Byte stores in display mode @p mode, @p stored's low byte the value: each halfword read back as
/// GbaVideoByteStore gives it.
bool GbaVideoJudgeBytes(uint32_t mode, const GbaVideoStore *stores, size_t count, char *out,
                        size_t out_size);

#ifdef __cplusplus
}
#endif
