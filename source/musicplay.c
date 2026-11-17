// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <string.h>

#include <switch.h>

#include "audio.h"
#include "musicplay.h"

// The stream: the tracker renders a chunk at a time into the next of four wave buffers on each
// of two voices, left and right, started together so the renderer plays their buffers in step.

static struct {
    bool on;
    MusicPlayer player;
    int16_t *left[MUSIC_STREAM_BUFFERS];
    int16_t *right[MUSIC_STREAM_BUFFERS];
    int16_t frames[MUSIC_STREAM_CHUNK * 2u];
    unsigned mix;
    float gain;
    uint64_t queued;
    uint32_t dry;
} g_stream;

/// Renders the next chunk into wave buffer @p slot of both voices.
static void Render(uint32_t slot) {
    MusicRender(&g_stream.player, g_stream.frames, MUSIC_STREAM_CHUNK);
    int16_t *left = g_stream.left[slot % MUSIC_STREAM_BUFFERS];
    int16_t *right = g_stream.right[slot % MUSIC_STREAM_BUFFERS];
    for (uint32_t i = 0; i < MUSIC_STREAM_CHUNK; ++i) {
        left[i] = g_stream.frames[2u * i];
        right[i] = g_stream.frames[2u * i + 1u];
    }
    AudioSync(left, MUSIC_STREAM_CHUNK * sizeof(int16_t));
    AudioSync(right, MUSIC_STREAM_CHUNK * sizeof(int16_t));
    __atomic_add_fetch(&g_stream.queued, MUSIC_STREAM_CHUNK, __ATOMIC_RELEASE);
}

uint32_t MusicSeedNow(void) {
    if (!MusicInteractive()) {
        return MUSIC_SEED_FIXED;
    }
    const uint64_t tick = armGetSystemTick();
    return (uint32_t)(tick ^ (tick >> 32));
}

bool MusicStreamOpen(MusicTrack track, uint32_t seed, unsigned mix, float gain,
                     bool barrier_first, char *error, size_t error_size) {
    if (!MusicMayPlay()) {
        snprintf(error, error_size,
                 "the music plays only in the interactive mode or the music family's cases, and "
                 "neither is on: no stream, no voice");
        return false;
    }
    if ((unsigned)track >= MUSIC_TRACKS) {
        snprintf(error, error_size, "track %u is none of the %u tracks", (unsigned)track,
                 (unsigned)MUSIC_TRACKS);
        return false;
    }
    if (!AudioIsOpen()) {
        snprintf(error, error_size, "the audio renderer is not open: AudioOpen first");
        return false;
    }
    for (uint32_t i = 0; i < MUSIC_STREAM_BUFFERS; ++i) {
        g_stream.left[i] = AudioAlloc(MUSIC_STREAM_CHUNK);
        g_stream.right[i] = AudioAlloc(MUSIC_STREAM_CHUNK);
        if (g_stream.left[i] == NULL || g_stream.right[i] == NULL) {
            snprintf(error, error_size,
                     "no room in the renderer's memory pool for the stream's %u buffers of %u "
                     "samples",
                     (unsigned)(2u * MUSIC_STREAM_BUFFERS), (unsigned)MUSIC_STREAM_CHUNK);
            return false;
        }
    }
    if (barrier_first) {
        MusicStartAfterBarrier(&g_stream.player, track, seed);
    } else {
        MusicStart(&g_stream.player, track, seed);
    }
    g_stream.mix = mix;
    g_stream.gain = gain;
    g_stream.queued = 0;
    g_stream.dry = 0;
    Render(0);
    AudioPlay(MUSIC_VOICE_LEFT, g_stream.left[0], MUSIC_STREAM_CHUNK, false, AUDIO_QUALITY_LOW, mix,
              gain, 0.0f);
    AudioPlay(MUSIC_VOICE_RIGHT, g_stream.right[0], MUSIC_STREAM_CHUNK, false, AUDIO_QUALITY_LOW,
              mix, 0.0f, gain);
    MusicCountVoice();
    MusicCountVoice();
    g_stream.on = true;
    MusicStreamFeed();
    return true;
}

void MusicStreamFeed(void) {
    if (!g_stream.on) {
        return;
    }
    const uint32_t queued = AudioVoiceQueued(MUSIC_VOICE_LEFT);
    if (queued > 1u && AudioVoiceConsumed(MUSIC_VOICE_LEFT) >= queued) {
        ++g_stream.dry;
    }
    while (AudioVoiceQueued(MUSIC_VOICE_LEFT) - AudioVoiceConsumed(MUSIC_VOICE_LEFT) <
           MUSIC_STREAM_BUFFERS) {
        const uint32_t slot = AudioVoiceQueued(MUSIC_VOICE_LEFT);
        Render(slot);
        if (!AudioQueue(MUSIC_VOICE_LEFT, g_stream.left[slot % MUSIC_STREAM_BUFFERS],
                        MUSIC_STREAM_CHUNK) ||
            !AudioQueue(MUSIC_VOICE_RIGHT, g_stream.right[slot % MUSIC_STREAM_BUFFERS],
                        MUSIC_STREAM_CHUNK)) {
            break;
        }
    }
}

void MusicStreamSwitch(MusicTrack track, uint32_t seed) {
    if (g_stream.on) {
        MusicSwitch(&g_stream.player, track, seed);
    }
}

bool MusicStreamOn(void) {
    return g_stream.on;
}

const MusicPlayer *MusicStreamPlayer(void) {
    return &g_stream.player;
}

uint64_t MusicStreamQueued(void) {
    return __atomic_load_n(&g_stream.queued, __ATOMIC_ACQUIRE);
}

uint32_t MusicStreamDry(void) {
    return g_stream.dry;
}

void MusicStreamClose(void) {
    if (!g_stream.on) {
        return;
    }
    AudioStop(MUSIC_VOICE_LEFT);
    AudioStop(MUSIC_VOICE_RIGHT);
    g_stream.on = false;
}

// ---------------------------------------------------------------------------------------------
// The threaded player

#define PLAY_CORE 2
// Below the game scene's audio thread (0x28) and its file thread (0x2E): the music waits for them.
#define PLAY_PRIORITY 0x30
#define PLAY_STACK 0x8000
// How long a stop waits for the fade to reach the speakers: the fade, the four buffers queued
// after it, and a margin.
#define STOP_LIMIT_MS 1000u

static struct {
    bool on;
    Thread thread;
    // Read and written across threads: the track asked for next (MUSIC_TRACKS + 1 for none), and
    // whether to stop.
    uint32_t next;
    bool stop;
    uint32_t switches;
} g_play;

static void PlayMain(void *argument) {
    (void)argument;
    while (!__atomic_load_n(&g_play.stop, __ATOMIC_ACQUIRE)) {
        const uint32_t next =
            __atomic_exchange_n(&g_play.next, MUSIC_TRACKS + 1u, __ATOMIC_ACQ_REL);
        if (next <= MUSIC_TRACKS) {
            MusicStreamSwitch((MusicTrack)next, MusicSeedNow());
            ++g_play.switches;
        }
        MusicStreamFeed();
        if (!AudioStep()) {
            break;
        }
    }
    // Faded out, and stepped until the silence after the fade is what the voices play.
    MusicStreamSwitch(MUSIC_TRACKS, MUSIC_SEED_FIXED);
    const uint64_t start = AudioNowMs();
    uint32_t until = 0;
    while (AudioNowMs() - start < STOP_LIMIT_MS) {
        MusicStreamFeed();
        if (until == 0u && MusicStreamPlayer()->state == MUSIC_SILENT) {
            until = AudioVoiceQueued(MUSIC_VOICE_LEFT);
        }
        if ((until != 0u && AudioVoiceConsumed(MUSIC_VOICE_LEFT) + 1u >= until) || !AudioStep()) {
            break;
        }
    }
    MusicStreamClose();
    AudioStep();
}

static bool Track(const char *name, MusicTrack *track, char *error, size_t error_size) {
    *track = MusicTrackNamed(name);
    if (*track == MUSIC_TRACKS) {
        snprintf(error, error_size, "\"%s\" names no track: menu, wimsy, fractal or fluid",
                 name != NULL ? name : "(none)");
        return false;
    }
    return true;
}

bool MusicPlayStart(const char *name, char *error, size_t error_size) {
    return MusicPlayStartInto(name, AUDIO_FINAL_MIX, MUSIC_PLAY_GAIN, error, error_size);
}

bool MusicPlayStartInto(const char *name, unsigned mix, float gain, char *error,
                        size_t error_size) {
    MusicTrack track = MUSIC_TRACKS;
    if (!Track(name, &track, error, error_size)) {
        return false;
    }
    if (g_play.on) {
        snprintf(error, error_size, "the player is running already: MusicPlaySwitch to change "
                                    "its track");
        return false;
    }
    if (!MusicMayPlay()) {
        snprintf(error, error_size,
                 "the music plays only in the interactive mode or the music family's cases, and "
                 "neither is on: no thread, no voice");
        return false;
    }
    if (AudioIsOpen()) {
        snprintf(error, error_size, "the audio renderer is open already: the thread stepping it "
                                    "feeds the music (MusicStreamOpen, MusicStreamFeed)");
        return false;
    }
    char why[160] = "";
    if (!AudioOpen(why, sizeof(why))) {
        snprintf(error, error_size, "the audio renderer: %s", why);
        return false;
    }
    if (!MusicStreamOpen(track, MusicSeedNow(), mix, gain, false, error, error_size)) {
        AudioClose();
        return false;
    }
    __atomic_store_n(&g_play.next, MUSIC_TRACKS + 1u, __ATOMIC_RELEASE);
    __atomic_store_n(&g_play.stop, false, __ATOMIC_RELEASE);
    g_play.switches = 0;
    Result rc = threadCreate(&g_play.thread, PlayMain, NULL, NULL, PLAY_STACK, PLAY_PRIORITY,
                             PLAY_CORE);
    if (R_SUCCEEDED(rc)) {
        rc = threadStart(&g_play.thread);
        if (R_FAILED(rc)) {
            threadClose(&g_play.thread);
        }
    }
    if (R_FAILED(rc)) {
        snprintf(error, error_size, "the music's thread did not start: 0x%x", (unsigned)rc);
        MusicStreamClose();
        AudioStep();
        AudioClose();
        return false;
    }
    MusicCountThread();
    g_play.on = true;
    return true;
}

bool MusicPlaySwitch(const char *name, char *error, size_t error_size) {
    MusicTrack track = MUSIC_TRACKS;
    if (!Track(name, &track, error, error_size)) {
        return false;
    }
    if (!g_play.on) {
        snprintf(error, error_size, "the player is not running: MusicPlayStart(\"%s\") starts it",
                 name);
        return false;
    }
    __atomic_store_n(&g_play.next, (uint32_t)track, __ATOMIC_RELEASE);
    return true;
}

void MusicPlayStop(void) {
    if (!g_play.on) {
        return;
    }
    __atomic_store_n(&g_play.stop, true, __ATOMIC_RELEASE);
    threadWaitForExit(&g_play.thread);
    threadClose(&g_play.thread);
    AudioClose();
    g_play.on = false;
}

bool MusicPlayOn(void) {
    return g_play.on;
}

uint32_t MusicPlaySwitches(void) {
    return g_play.switches;
}
