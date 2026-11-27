// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>

#include "gbavideoref.h"

// DISPSTAT's flags, as GBATEK's "LCD I/O Interrupts and Status" lists them
#define STATUS_VBLANK 0x1u
#define STATUS_HBLANK 0x2u
#define STATUS_MATCH 0x4u

void GbaVideoPollAdd(GbaVideoPoll *poll, uint32_t status, uint32_t previous) {
    const uint32_t line = status >> 16;
    ++poll->samples;
    if (previous <= 0xFFFFu && line != previous) {
        ++poll->steps;
        if (previous == GBAVIDEO_LINES - 1u && line == 0u) {
            ++poll->wraps;
        } else if (line != previous + 1u && poll->bad_steps++ == 0u) {
            poll->bad_from = previous;
            poll->bad_to = line;
        }
    }
    if (line >= GBAVIDEO_LINES) {
        if (poll->past++ == 0u) {
            poll->first_past = line;
        }
        return;
    }
    GbaVideoLine *const l = &poll->lines[line];
    ++l->samples;
    if (status & STATUS_VBLANK) {
        ++l->vblank_set;
    } else {
        ++l->vblank_clear;
    }
    if (status & STATUS_HBLANK) {
        ++l->hblank_set;
    } else {
        ++l->hblank_clear;
    }
    if (status & STATUS_MATCH) {
        ++l->match_set;
    } else {
        ++l->match_clear;
    }
}

/// The first line the poll never saw, or GBAVIDEO_LINES.
static uint32_t FirstUnseen(const GbaVideoPoll *poll) {
    for (uint32_t i = 0; i < GBAVIDEO_LINES; ++i) {
        if (poll->lines[i].samples == 0u) {
            return i;
        }
    }
    return GBAVIDEO_LINES;
}

bool GbaVideoJudgeCount(const GbaVideoPoll *poll, char *out, size_t out_size) {
    const uint32_t unseen = FirstUnseen(poll);
    if (poll->past > 0u) {
        snprintf(out, out_size, "%lu of %lu reads had VCOUNT past 227, the first %lu",
                 (unsigned long)poll->past, (unsigned long)poll->samples,
                 (unsigned long)poll->first_past);
        return false;
    }
    if (poll->bad_steps > 0u) {
        snprintf(out, out_size,
                 "%lu of %lu changes of VCOUNT were not one up nor 227 to 0, the first %lu to %lu",
                 (unsigned long)poll->bad_steps, (unsigned long)poll->steps,
                 (unsigned long)poll->bad_from, (unsigned long)poll->bad_to);
        return false;
    }
    if (poll->wraps == 0u || unseen < GBAVIDEO_LINES) {
        snprintf(out, out_size,
                 "%lu reads saw %lu changes and %lu wraps from 227 to 0; line %lu never came",
                 (unsigned long)poll->samples, (unsigned long)poll->steps,
                 (unsigned long)poll->wraps, (unsigned long)unseen);
        return false;
    }
    snprintf(out, out_size, "%lu reads, %lu changes one up, %lu wraps from 227 to 0",
             (unsigned long)poll->samples, (unsigned long)poll->steps,
             (unsigned long)poll->wraps);
    return true;
}

bool GbaVideoJudgeVblankFlag(const GbaVideoPoll *poll, char *out, size_t out_size) {
    for (uint32_t i = 0; i < GBAVIDEO_LINES; ++i) {
        const GbaVideoLine *const l = &poll->lines[i];
        const bool in_vblank = i >= GBAVIDEO_DRAWN && i <= GBAVIDEO_VBLANK_FLAG_LAST;
        if (l->samples == 0u) {
            snprintf(out, out_size, "line %lu never came, so its flag was never read",
                     (unsigned long)i);
            return false;
        }
        if (in_vblank ? l->vblank_clear > 0u : l->vblank_set > 0u) {
            snprintf(out, out_size,
                     "line %lu had the V-Blank flag set on %lu of its %lu reads; it is set on "
                     "lines 160 to 226 alone",
                     (unsigned long)i, (unsigned long)l->vblank_set, (unsigned long)l->samples);
            return false;
        }
    }
    snprintf(out, out_size, "set on every read of lines 160 to 226 and none of the others");
    return true;
}

bool GbaVideoJudgeHblankFlag(const GbaVideoPoll *poll, char *out, size_t out_size) {
    for (uint32_t i = 0; i < GBAVIDEO_LINES; ++i) {
        const GbaVideoLine *const l = &poll->lines[i];
        if (l->hblank_set == 0u || l->hblank_clear == 0u) {
            snprintf(out, out_size,
                     "line %lu had the H-Blank flag set on %lu and clear on %lu of its %lu reads",
                     (unsigned long)i, (unsigned long)l->hblank_set,
                     (unsigned long)l->hblank_clear, (unsigned long)l->samples);
            return false;
        }
    }
    snprintf(out, out_size, "both clear and set within each of the 228 lines");
    return true;
}

bool GbaVideoJudgeMatchFlag(const GbaVideoPoll *poll, char *out, size_t out_size) {
    if (poll->lyc >= GBAVIDEO_LINES) {
        snprintf(out, out_size, "the V-Count setting named line %lu, which no frame has",
                 (unsigned long)poll->lyc);
        return false;
    }
    for (uint32_t i = 0; i < GBAVIDEO_LINES; ++i) {
        const GbaVideoLine *const l = &poll->lines[i];
        const bool named = i == poll->lyc;
        if (l->samples == 0u || (named ? l->match_clear > 0u : l->match_set > 0u)) {
            snprintf(out, out_size,
                     "line %lu, with line %lu named, had the V-Counter flag set on %lu of its %lu "
                     "reads",
                     (unsigned long)i, (unsigned long)poll->lyc, (unsigned long)l->match_set,
                     (unsigned long)l->samples);
            return false;
        }
    }
    snprintf(out, out_size, "set on every read of line %lu and none of the others",
             (unsigned long)poll->lyc);
    return true;
}

bool GbaVideoJudgeFrames(const uint32_t *cycles, size_t count, char *out, size_t out_size) {
    if (count == 0u) {
        snprintf(out, out_size, "no frame was measured");
        return false;
    }
    int written = snprintf(out, out_size, "frames of");
    bool passed = true;
    for (size_t i = 0; i < count; ++i) {
        const uint32_t c = cycles[i];
        const uint32_t off = c > GBAVIDEO_FRAME_CYCLES ? c - GBAVIDEO_FRAME_CYCLES
                                                         : GBAVIDEO_FRAME_CYCLES - c;
        passed = passed && off <= GBAVIDEO_FRAME_SLACK;
        if (written >= 0 && (size_t)written < out_size) {
            written += snprintf(out + written, out_size - (size_t)written, " %lu",
                                (unsigned long)c);
        }
    }
    if (written >= 0 && (size_t)written < out_size) {
        snprintf(out + written, out_size - (size_t)written, " cycles; 280896 within %u%s",
                 GBAVIDEO_FRAME_SLACK, passed ? "" : " expected");
    }
    return passed;
}

/// How many interrupts @p irqs has from @p first to @p last.
static uint32_t Between(const GbaVideoIrqs *irqs, uint32_t first, uint32_t last) {
    uint32_t total = 0;
    for (uint32_t i = first; i <= last && i < GBAVIDEO_LINES; ++i) {
        total += irqs->per_line[i];
    }
    return total;
}

bool GbaVideoJudgeIrqOnLine(const GbaVideoIrqs *irqs, uint32_t line, uint32_t times, char *out,
                            size_t out_size) {
    const uint32_t all = Between(irqs, 0u, GBAVIDEO_LINES - 1u) + irqs->past;
    const uint32_t there = line < GBAVIDEO_LINES ? irqs->per_line[line] : 0u;
    if (there == times && all == times) {
        snprintf(out, out_size, "%lu, each on line %lu", (unsigned long)times,
                 (unsigned long)line);
        return true;
    }
    uint32_t other = GBAVIDEO_LINES;
    for (uint32_t i = 0; i < GBAVIDEO_LINES && other == GBAVIDEO_LINES; ++i) {
        if (i != line && irqs->per_line[i] > 0u) {
            other = i;
        }
    }
    if (other < GBAVIDEO_LINES) {
        snprintf(out, out_size,
                 "%lu of %lu expected on line %lu, %lu in all, %lu of them on line %lu",
                 (unsigned long)there, (unsigned long)times, (unsigned long)line,
                 (unsigned long)all, (unsigned long)irqs->per_line[other], (unsigned long)other);
    } else {
        snprintf(out, out_size, "%lu of %lu expected on line %lu, %lu in all, %lu on no line",
                 (unsigned long)there, (unsigned long)times, (unsigned long)line,
                 (unsigned long)all, (unsigned long)irqs->past);
    }
    return false;
}

bool GbaVideoJudgeIrqEachLine(const GbaVideoIrqs *irqs, uint32_t first, uint32_t last,
                              uint32_t times, char *out, size_t out_size) {
    const uint32_t outside = Between(irqs, 0u, GBAVIDEO_LINES - 1u) -
                             Between(irqs, first, last) + irqs->past;
    for (uint32_t i = first; i <= last; ++i) {
        const uint32_t got = i < GBAVIDEO_LINES ? irqs->per_line[i] : 0u;
        if (got != times) {
            snprintf(out, out_size,
                     "line %lu had %lu, %lu expected on each of lines %lu to %lu; %lu outside them",
                     (unsigned long)i, (unsigned long)got, (unsigned long)times,
                     (unsigned long)first, (unsigned long)last, (unsigned long)outside);
            return false;
        }
    }
    snprintf(out, out_size, "%lu on each of lines %lu to %lu; %lu outside them, not judged",
             (unsigned long)times, (unsigned long)first, (unsigned long)last,
             (unsigned long)outside);
    return true;
}

uint32_t GbaVideoVramOffset(uint32_t address) {
    const uint32_t offset = address & 0x1FFFFu;
    return offset >= 0x18000u ? offset - 0x8000u : offset;
}

uint16_t GbaVideoByteStore(uint32_t mode, uint32_t address, uint16_t before, uint8_t value) {
    const uint16_t doubled = (uint16_t)(value * 0x101u);
    switch (address >> 24) {
    case 0x05u:
        return doubled;
    case 0x06u: {
        const uint32_t bg_end = mode >= 3u ? 0x14000u : 0x10000u;
        return GbaVideoVramOffset(address) < bg_end ? doubled : before;
    }
    default:
        return before;
    }
}

/// The first store whose halfword read back as other than @p expected gave, or @p count.
static size_t FirstWrong(const GbaVideoStore *stores, size_t count, const uint16_t *expected) {
    for (size_t i = 0; i < count; ++i) {
        if (stores[i].after != expected[i]) {
            return i;
        }
    }
    return count;
}

#define STORES_MAX 32u

bool GbaVideoJudgeHalfwords(const GbaVideoStore *stores, size_t count, char *out,
                            size_t out_size) {
    uint16_t expected[STORES_MAX];
    if (count > STORES_MAX) {
        snprintf(out, out_size, "%lu stores, past the %u this judge holds", (unsigned long)count,
                 STORES_MAX);
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        expected[i] = stores[i].stored;
    }
    const size_t wrong = FirstWrong(stores, count, expected);
    if (wrong < count) {
        const GbaVideoStore *const s = &stores[wrong];
        snprintf(out, out_size,
                 "0x%04x stored at 0x%08lx read back 0x%04x at 0x%08lx (it held 0x%04x before)",
                 s->stored, (unsigned long)s->address, s->after, (unsigned long)s->read_at,
                 s->before);
        return false;
    }
    snprintf(out, out_size, "%lu halfwords, each read back as stored", (unsigned long)count);
    return true;
}

bool GbaVideoJudgeBytes(uint32_t mode, const GbaVideoStore *stores, size_t count, char *out,
                        size_t out_size) {
    uint16_t expected[STORES_MAX];
    if (count > STORES_MAX) {
        snprintf(out, out_size, "%lu stores, past the %u this judge holds", (unsigned long)count,
                 STORES_MAX);
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        expected[i] = GbaVideoByteStore(mode, stores[i].address, stores[i].before,
                                        (uint8_t)stores[i].stored);
    }
    const size_t wrong = FirstWrong(stores, count, expected);
    if (wrong < count) {
        const GbaVideoStore *const s = &stores[wrong];
        snprintf(out, out_size,
                 "mode %lu: byte 0x%02x stored at 0x%08lx left its halfword 0x%04x, from 0x%04x; "
                 "0x%04x expected",
                 (unsigned long)mode, (unsigned)(s->stored & 0xFFu), (unsigned long)s->address,
                 s->after, s->before, expected[wrong]);
        return false;
    }
    snprintf(out, out_size, "mode %lu: %lu bytes, each halfword as the memory's rule leaves it",
             (unsigned long)mode, (unsigned long)count);
    return true;
}
