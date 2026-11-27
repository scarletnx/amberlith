// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The GBA's side of platform.h (docs/Frontends.md, docs/GBA.md): its log is mGBA's debug registers,
// which mGBA prints, and the cartridge's SRAM, which an emulator saves as a file; no sockets and no
// drawing surface, since its steps run neither suite.

#include <stdint.h>

#include "platform.h"

// mGBA's debug registers: 0xC0DE written to the first turns the others on, and it reads back 0x1DEA
// then; a message goes in the 256-byte buffer, and its level with the send bit writes it to the log.
#define GBA_DEBUG_ENABLE ((volatile uint16_t *)0x04FFF780)
#define GBA_DEBUG_STRING ((volatile uint8_t *)0x04FFF600)
#define GBA_DEBUG_FLAGS ((volatile uint16_t *)0x04FFF700)
#define GBA_DEBUG_PIECE 256u
#define GBA_DEBUG_INFO 3u
#define GBA_DEBUG_SEND 0x100u

// The cartridge's SRAM, read and written a byte at a time: the log's last 32 KiB as a ring, after
// a head of 16 bytes, the mark, where the next byte goes (little-endian) and whether it wrapped.
#define GBA_SRAM ((volatile uint8_t *)0x0E000000)
#define GBA_SRAM_SIZE 0x8000u
#define GBA_LOG_HEAD 16u
#define GBA_LOG_RING (GBA_SRAM_SIZE - GBA_LOG_HEAD)
static const char GBA_LOG_MARK[8] = { 'A', 'M', 'B', 'R', 'L', 'O', 'G', '1' };

// The save type an emulator looks for in the cartridge to give it 32 KiB of SRAM: nothing reads it,
// so gba.mk names it to the link, which would leave it out otherwise
extern const char GBA_SAVE_TYPE[];
const char GBA_SAVE_TYPE[] = "SRAM_V113";

static struct {
    bool started;
    bool debug;
    uint32_t at;
    bool wrapped;
} g_platform_gba;

static void SramWriteHead(void) {
    for (uint32_t i = 0; i < sizeof(GBA_LOG_MARK); ++i) {
        GBA_SRAM[i] = (uint8_t)GBA_LOG_MARK[i];
    }
    for (uint32_t i = 0; i < 4u; ++i) {
        GBA_SRAM[8u + i] = (uint8_t)(g_platform_gba.at >> (8u * i));
    }
    GBA_SRAM[12] = g_platform_gba.wrapped ? 1u : 0u;
}

static void SramAppend(const char *bytes, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        GBA_SRAM[GBA_LOG_HEAD + g_platform_gba.at] = (uint8_t)bytes[i];
        if (++g_platform_gba.at == GBA_LOG_RING) {
            g_platform_gba.at = 0;
            g_platform_gba.wrapped = true;
        }
    }
}

static void DebugSend(const char *bytes, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        GBA_DEBUG_STRING[i] = (uint8_t)bytes[i];
    }
    if (length < GBA_DEBUG_PIECE) {
        GBA_DEBUG_STRING[length] = 0u;
    }
    *GBA_DEBUG_FLAGS = (uint16_t)(GBA_DEBUG_INFO | GBA_DEBUG_SEND);
}

void PlatformLog(const char *line, size_t length) {
    if (!g_platform_gba.started) {
        g_platform_gba.started = true;
        *GBA_DEBUG_ENABLE = 0xC0DEu;
        g_platform_gba.debug = *GBA_DEBUG_ENABLE == 0x1DEAu;
        SramWriteHead();
    }
    // A line longer than the buffer goes in pieces of 256 bytes, the last one shorter, empty for a
    // line whose length is a multiple of 256: a reader joins a piece of 256 to the one after it.
    if (g_platform_gba.debug) {
        size_t at = 0;
        do {
            const size_t piece = length - at < GBA_DEBUG_PIECE ? length - at : GBA_DEBUG_PIECE;
            DebugSend(line + at, piece);
            at += piece;
            if (piece < GBA_DEBUG_PIECE) {
                break;
            }
        } while (true);
    }
    SramAppend(line, length);
    SramAppend("\n", 1u);
    SramWriteHead();
}

bool PlatformLogIsStandardOutput(void) {
    return false;
}

const char *PlatformError(void) {
    return "the GBA has no network";
}

bool PlatformSocketsOpen(void) {
    return false;
}

void PlatformSocketsClose(void) {}

size_t PlatformNetworkOff(char *out, size_t out_size) {
    if (out != nullptr && out_size > 0u) {
        out[0] = '\0';
    }
    return 0;
}
