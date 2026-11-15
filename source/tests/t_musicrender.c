// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../check.h"
#include "../music.h"
#include "../report.h"
#include "tests.h"

// The music family's first cases, the ones that need no renderer: each track rendered whole on
// this machine and held to the samples the host build renders, and a switch between two tracks
// held to the tracks rendered alone. The host build runs them too, so the hashes below are the
// host's own and a change to the tracker or the composer moves them here, in the same commit.

static const char *const SUITE = "music";

/// One pass of each track's form as the host renders it: its frames and the FNV-1a of its
/// samples, left then right, each low byte first.
static const struct {
    uint64_t frames;
    uint64_t hash;
} RENDERED[MUSIC_TRACKS] = {
    [MUSIC_TRACK_MENU] = { 3291428u, 0x38b9f042892ab13cull },
    [MUSIC_TRACK_WIMSY] = { 2633142u, 0x82bfd13edd47c126ull },
    [MUSIC_TRACK_FRACTAL] = { 4388571u, 0x330d69cbc7415604ull },
    [MUSIC_TRACK_FLUID] = { 2949120u, 0x6b82c89566e849dfull },
};

// A chunk at a time, so the longest track needs no buffer of its own: 0.1 s.
#define CHUNK 4800u

static MusicPlayer player;
static MusicPlayer alone;
static int16_t chunk[CHUNK * 2u];
static int16_t want[CHUNK * 2u];

static void RenderTrack(MusicTrack track) {
    const MusicColour *colour = MusicColourOf(track);
    char name[96];
    snprintf(name, sizeof(name), "the %s track's 32 bars render to the host's samples",
             colour->name);
    if (!ReportStart(SUITE, name)) {
        return;
    }
    MusicStart(&player, track);
    const uint64_t frames = MusicFormFrames(&player.song);
    uint64_t hash = MUSIC_HASH_START;
    uint32_t peak = 0;
    for (uint64_t done = 0; done < frames;) {
        const size_t n = (size_t)(frames - done < CHUNK ? frames - done : CHUNK);
        MusicRender(&player, chunk, n);
        hash = MusicHash(hash, chunk, n * 2u);
        for (size_t i = 0; i < n * 2u; ++i) {
            const uint32_t magnitude = (uint32_t)(chunk[i] < 0 ? -(int32_t)chunk[i] : chunk[i]);
            peak = magnitude > peak ? magnitude : peak;
        }
        done += n;
    }
    ReportNote("music: %s, %s: %llu frames, %.1f s, hash %016llx, peak %u, %llu samples clipped",
               colour->name, colour->demo, (unsigned long long)frames,
               (double)frames / MUSIC_RATE, (unsigned long long)hash, (unsigned)peak,
               (unsigned long long)player.clipped);
    CHECK_TRUE(SUITE, name, CHECK_EXACT,
               frames == RENDERED[track].frames && hash == RENDERED[track].hash &&
                   player.clipped == 0u,
               "%llu frames hashing to %016llx, %llu samples clipped, where the host's %llu "
               "frames hash to %016llx and clip none",
               (unsigned long long)frames, (unsigned long long)hash,
               (unsigned long long)player.clipped, (unsigned long long)RENDERED[track].frames,
               (unsigned long long)RENDERED[track].hash);
}

/// Renders @p frames from @p from and @p alone_from side by side, and counts where they differ;
/// @p fade scales the second as the switch's fade does, from @p fade_at frames left.
static uint64_t Compare(uint64_t frames, bool fade, uint32_t fade_at, uint64_t *first_wrong) {
    uint64_t wrong = 0;
    uint32_t left = fade_at;
    for (uint64_t done = 0; done < frames;) {
        const size_t n = (size_t)(frames - done < CHUNK ? frames - done : CHUNK);
        MusicRender(&player, chunk, n);
        MusicRender(&alone, want, n);
        for (size_t i = 0; i < n; ++i) {
            int32_t l = want[2u * i];
            int32_t r = want[2u * i + 1u];
            if (fade) {
                l = l * (int32_t)left / (int32_t)MUSIC_FADE_FRAMES;
                r = r * (int32_t)left / (int32_t)MUSIC_FADE_FRAMES;
                --left;
            }
            if (chunk[2u * i] != l || chunk[2u * i + 1u] != r) {
                if (wrong++ == 0u) {
                    *first_wrong = done + i;
                }
            }
        }
        done += n;
    }
    return wrong;
}

/// A switch from the browser's track to Wimsy's: the first fades out over MUSIC_FADE_FRAMES as
/// it would have played on, then MUSIC_BARRIER_FRAMES of nought, then Wimsy's from its first row.
static void RenderSwitch(void) {
    static const char NAME[] = "a switch fades the track out over 50 ms, keeps a second of "
                               "silence, then starts the next one from its first row";
    if (!ReportStart(SUITE, NAME)) {
        return;
    }
    const uint64_t before = MUSIC_RATE / 2u;
    MusicStart(&player, MUSIC_TRACK_MENU);
    MusicStart(&alone, MUSIC_TRACK_MENU);
    uint64_t at = 0;
    const uint64_t lead_in = Compare(before, false, 0, &at);
    MusicSwitch(&player, MUSIC_TRACK_WIMSY);
    uint64_t fade_at = 0;
    const uint64_t faded = Compare(MUSIC_FADE_FRAMES, true, MUSIC_FADE_FRAMES, &fade_at);
    uint64_t silent = 0;
    uint64_t loud_at = 0;
    for (uint64_t done = 0; done < MUSIC_BARRIER_FRAMES;) {
        const size_t n = (size_t)(MUSIC_BARRIER_FRAMES - done < CHUNK ? MUSIC_BARRIER_FRAMES - done
                                                                       : CHUNK);
        MusicRender(&player, chunk, n);
        for (size_t i = 0; i < n; ++i) {
            if (chunk[2u * i] == 0 && chunk[2u * i + 1u] == 0) {
                ++silent;
            } else if (loud_at == 0u) {
                loud_at = done + i + 1u;
            }
        }
        done += n;
    }
    MusicStart(&alone, MUSIC_TRACK_WIMSY);
    uint64_t next_at = 0;
    const uint64_t next = Compare(MUSIC_RATE / 2u, false, 0, &next_at);
    CHECK_TRUE(SUITE, NAME, CHECK_EXACT,
               lead_in == 0u && faded == 0u && silent == MUSIC_BARRIER_FRAMES && next == 0u &&
                   player.song.track == MUSIC_TRACK_WIMSY,
               "%llu frames of the first track before the switch differ from it alone (the "
               "first at %llu); %llu of the %u faded frames from its faded self (the first at "
               "%llu); %llu of %u barrier frames silent (the first loud one %llu); %llu of the "
               "next track's first %u frames differ from it alone (the first at %llu)",
               (unsigned long long)lead_in, (unsigned long long)at, (unsigned long long)faded,
               (unsigned)MUSIC_FADE_FRAMES, (unsigned long long)fade_at,
               (unsigned long long)silent, (unsigned)MUSIC_BARRIER_FRAMES,
               (unsigned long long)loud_at, (unsigned long long)next,
               (unsigned)(MUSIC_RATE / 2u), (unsigned long long)next_at);
}

/// Inside the barrier a second switch changes only what follows it, and a stop ends it: the
/// barrier keeps its length and is followed by the newer track, or by silence at once.
static void RenderBarrierSwitch(void) {
    static const char NAME[] = "a switch inside the silence keeps it and starts the newer track "
                               "after it, and a stop there stays silent";
    if (!ReportStart(SUITE, NAME)) {
        return;
    }
    const uint64_t into = 1000u;
    MusicStart(&player, MUSIC_TRACK_MENU);
    MusicRender(&player, chunk, CHUNK);
    MusicSwitch(&player, MUSIC_TRACK_WIMSY);
    MusicRender(&player, chunk, MUSIC_FADE_FRAMES);
    MusicRender(&player, chunk, (size_t)into);
    MusicSwitch(&player, MUSIC_TRACK_FLUID);
    uint64_t loud = 0;
    for (uint64_t done = into; done < MUSIC_BARRIER_FRAMES;) {
        const size_t n = (size_t)(MUSIC_BARRIER_FRAMES - done < CHUNK ? MUSIC_BARRIER_FRAMES - done
                                                                       : CHUNK);
        MusicRender(&player, chunk, n);
        for (size_t i = 0; i < 2u * n; ++i) {
            loud += chunk[i] != 0 ? 1u : 0u;
        }
        done += n;
    }
    MusicStart(&alone, MUSIC_TRACK_FLUID);
    uint64_t next_at = 0;
    const uint64_t next = Compare(MUSIC_RATE / 4u, false, 0, &next_at);
    const MusicTrack newer = player.song.track;

    MusicStart(&player, MUSIC_TRACK_MENU);
    MusicRender(&player, chunk, CHUNK);
    MusicSwitch(&player, MUSIC_TRACK_WIMSY);
    MusicRender(&player, chunk, MUSIC_FADE_FRAMES);
    MusicRender(&player, chunk, (size_t)into);
    MusicSwitch(&player, MUSIC_TRACKS);
    uint64_t after_stop = 0;
    for (uint64_t done = 0; done < MUSIC_RATE + MUSIC_BARRIER_FRAMES;) {
        MusicRender(&player, chunk, CHUNK);
        for (size_t i = 0; i < 2u * CHUNK; ++i) {
            after_stop += chunk[i] != 0 ? 1u : 0u;
        }
        done += CHUNK;
    }
    CHECK_TRUE(SUITE, NAME, CHECK_EXACT,
               loud == 0u && next == 0u && newer == MUSIC_TRACK_FLUID && after_stop == 0u &&
                   player.state == MUSIC_SILENT,
               "%llu samples sounded in the rest of the barrier; after it track %u, %llu of its "
               "first %u frames unlike the fluid track alone (the first at %llu); after a stop "
               "inside it %llu samples sounded over two seconds, the player %s",
               (unsigned long long)loud, (unsigned)newer, (unsigned long long)next,
               (unsigned)(MUSIC_RATE / 4u), (unsigned long long)next_at,
               (unsigned long long)after_stop,
               player.state == MUSIC_SILENT ? "silent" : "not silent");
}

void RunMusicRender(void) {
    for (unsigned track = 0; track < MUSIC_TRACKS; ++track) {
        RenderTrack((MusicTrack)track);
    }
    RenderSwitch();
    RenderBarrierSwitch();
}

void MusicJudgeOff(const char *suite, const char *where, MusicMade before) {
    char name[160];
    snprintf(name, sizeof(name), "the demos' music made nothing %s: no thread, voice or sample",
             where);
    const MusicMade now = MusicMadeSoFar();
    const uint64_t frames = now.samples - before.samples;
    const uint32_t threads = now.threads - before.threads;
    const uint32_t voices = now.voices - before.voices;
    if ((frames != 0u || threads != 0u || voices != 0u) && MusicInteractive()) {
        // The browser played it on purpose: a person began this run and chose the music.
        char detail[256];
        snprintf(detail, sizeof(detail),
                 "the browser's music rendered %llu frames, started %u threads and %u voices %s, "
                 "on purpose in the interactive mode; a run from the mode file holds them to "
                 "nought",
                 (unsigned long long)frames, (unsigned)threads, (unsigned)voices, where);
        ReportSkip(suite, name, REPORT_REASON_ASKED, detail);
        return;
    }
    if (!ReportStart(suite, name)) {
        return;
    }
    CHECK_TRUE(suite, name, CHECK_EXACT, frames == 0u && threads == 0u && voices == 0u,
               "the music rendered %llu frames, started %u threads and %u voices %s, where "
               "nought: it plays only in the interactive mode (%s now) and the music family",
               (unsigned long long)frames, (unsigned)threads, (unsigned)voices, where,
               MusicInteractive() ? "on" : "off");
}
