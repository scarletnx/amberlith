// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <string.h>

#include <switch.h>

#include "../audio.h"
#include "../audioref.h"
#include "../check.h"
#include "../music.h"
#include "../musicplay.h"
#include "../report.h"
#include "tests.h"

// The music family: the demos' music asked for by name, which is the one place outside the
// interactive mode where it plays (MusicSetAsked). First the tracks rendered on this machine and
// held to the host's samples (t_musicrender.c); then what the audio renderer makes of them, the
// game family's path to it: each voice at the output rate and at low quality, so the renderer
// passes every sample through unchanged, into the audio family's submix at a gain of nought,
// silent, where its aux effect records them. A recording is held to the tracker's samples one for
// one, wherever in its ring the renderer started writing them.

static const char *const SUITE = "music";

// The aux ring: 40 frames of 240 samples a channel, 0.2 s.
#define RING (40u * AUDIO_FRAME)
// Where a track's excerpt starts: a second in, where all four channels play.
#define EXCERPT_AT MUSIC_RATE
// The steps a looped excerpt plays before its recording is read: 100 renderer frames at least,
// the ring's 40 two and a half times over.
#define SETTLE_STEPS 100u
#define LIMIT_MS 5000u
// The stream case: the browser's track, a switch to Wimsy's after 0.4 s, then on to 0.15 s
// past the barrier; the reference holds every frame the stream may have queued by then.
#define SWITCH_AFTER (10u * MUSIC_STREAM_CHUNK)
#define STREAM_FRAMES (5u * MUSIC_RATE / 2u)
#define PAST_BARRIER (MUSIC_RATE * 15u / 100u)
#define STREAM_LIMIT_MS 10000u

static MusicPlayer player;
static int16_t frames[RING * 2u];
static int32_t ring_left[RING];
static int32_t ring_right[RING];
static int16_t reference[STREAM_FRAMES * 2u];

/// The recording against a loop of @p left and @p right: the rotation of the loop each channel's
/// ring holds, the renderer's delay and the voice's start included, or RING when none matches;
/// with the best one's matching samples in @p best_match when none does.
static uint32_t FindRotation(const int16_t *left, const int16_t *right, uint32_t *best_match,
                             uint32_t *best_at) {
    *best_match = 0;
    *best_at = 0;
    for (uint32_t r = 0; r < RING; ++r) {
        uint32_t i = 0;
        while (i < RING && ring_left[i] == left[(i + r) % RING] &&
               ring_right[i] == right[(i + r) % RING]) {
            ++i;
        }
        if (i == RING) {
            return r;
        }
    }
    // None: the rotation that matches most of the first frame, counted whole, for the line.
    uint32_t best_first = 0;
    for (uint32_t r = 0; r < RING; ++r) {
        uint32_t n = 0;
        for (uint32_t i = 0; i < AUDIO_FRAME; ++i) {
            n += ring_left[i] == left[(i + r) % RING] ? 1u : 0u;
        }
        if (n > best_first) {
            best_first = n;
            *best_at = r;
        }
    }
    for (uint32_t i = 0; i < RING; ++i) {
        *best_match += ring_left[i] == left[(i + *best_at) % RING] &&
                               ring_right[i] == right[(i + *best_at) % RING]
                           ? 1u
                           : 0u;
    }
    return RING;
}

static int32_t Peak32(const int32_t *x, size_t n) {
    int32_t peak = 0;
    for (size_t i = 0; i < n; ++i) {
        const int32_t v = x[i] < 0 ? -x[i] : x[i];
        peak = v > peak ? v : peak;
    }
    return peak;
}

/// Stops the aux on an update, so the ring holds still while it is read, and reads both channels.
static void ReadRing(void) {
    AudioCapture(false);
    AudioStep();
    size_t ring_frames = 0;
    const int32_t *ring = AudioCaptureRing(&ring_frames);
    AudioRefChannel32(ring, ring_frames, 2u, AUDIO_FRAME, 0u, ring_left);
    AudioRefChannel32(ring, ring_frames, 2u, AUDIO_FRAME, 1u, ring_right);
}

static const char STREAM_NAME[] = "the player's stream through the renderer's wave buffers "
                                  "comes back sample for sample, past a switch and its silence";

static const char THREADED_NAME[] = "the browser's player starts on a renderer of its own, takes "
                                    "a switch and fades out, the renderer closed after it";

static void ExcerptName(char *name, size_t size, MusicTrack track) {
    snprintf(name, size,
             "the %s track, looped on two voices at the output rate, comes back from the "
             "renderer sample for sample",
             MusicTrackName(track));
}

static void Excerpt(MusicTrack track, int16_t *left, int16_t *right) {
    const char *const track_name = MusicTrackName(track);
    char name[128];
    ExcerptName(name, sizeof(name), track);
    if (!ReportStart(SUITE, name)) {
        return;
    }
    MusicStart(&player, track, MUSIC_SEED_FIXED);
    for (uint32_t done = 0; done < EXCERPT_AT; done += RING) {
        MusicRender(&player, frames, RING);
    }
    MusicRender(&player, frames, RING);
    for (uint32_t i = 0; i < RING; ++i) {
        left[i] = frames[2u * i];
        right[i] = frames[2u * i + 1u];
    }
    AudioSync(left, RING * sizeof(int16_t));
    AudioSync(right, RING * sizeof(int16_t));
    AudioCapture(false);
    AudioStep();
    AudioPlay(MUSIC_VOICE_LEFT, left, RING, true, AUDIO_QUALITY_LOW, AUDIO_SUBMIX, 1.0f, 0.0f);
    AudioPlay(MUSIC_VOICE_RIGHT, right, RING, true, AUDIO_QUALITY_LOW, AUDIO_SUBMIX, 0.0f, 1.0f);
    MusicCountVoice();
    MusicCountVoice();
    AudioCapture(true);
    bool stepped = true;
    const uint64_t start = AudioNowMs();
    for (uint32_t i = 0; i < SETTLE_STEPS && stepped; ++i) {
        stepped = AudioStep() && AudioNowMs() - start < LIMIT_MS;
    }
    ReadRing();
    AudioStop(MUSIC_VOICE_LEFT);
    AudioStop(MUSIC_VOICE_RIGHT);
    AudioStep();
    if (!stepped) {
        ReportCase(SUITE, name, CHECK_EXACT, false,
                   AudioError()[0] != '\0' ? AudioError()
                                           : "100 renderer frames took over 5000 ms");
        return;
    }
    uint32_t best_match = 0;
    uint32_t best_at = 0;
    const uint32_t rotation = FindRotation(left, right, &best_match, &best_at);
    if (rotation < RING) {
        ReportNote("music: the aux's ring holds %s's loop %u samples late", track_name,
                   (unsigned)((RING - rotation) % RING));
    }
    CHECK_TRUE(SUITE, name, CHECK_EXACT, rotation < RING,
               "no turn of the %u-sample loop is what the aux recorded: the closest, %u samples "
               "in, holds %u of its %u samples a channel; the recording's peak %d, the loop's "
               "%.0f",
               (unsigned)RING, (unsigned)best_at, (unsigned)best_match, (unsigned)RING,
               (int)Peak32(ring_left, RING), 32768.0 * AudioRefPeak16(left, RING));
}

/// The stream as the interactive mode plays it, through MusicStreamFeed and the voices' wave
/// buffers, across a switch: the last 0.2 s the aux recorded, unrolled from wherever its ring
/// was last written, are a stretch of the stream as the tracker renders it on its own, one for
/// one.
static void Stream(void) {
    const char *const NAME = STREAM_NAME;
    if (!ReportStart(SUITE, NAME)) {
        return;
    }
    char error[200] = "";
    AudioCapture(false);
    AudioStep();
    if (!MusicStreamOpen(MUSIC_TRACK_MENU, MUSIC_SEED_FIXED, AUDIO_SUBMIX, 1.0f, false, error,
                         sizeof(error))) {
        ReportCase(SUITE, NAME, CHECK_EXACT, false, error);
        return;
    }
    AudioCapture(true);
    uint64_t switched_at = 0;
    const uint64_t start = AudioNowMs();
    bool stepped = true;
    while (stepped) {
        if (switched_at == 0u && MusicStreamQueued() >= SWITCH_AFTER) {
            switched_at = MusicStreamQueued();
            MusicStreamSwitch(MUSIC_TRACK_WIMSY, MUSIC_SEED_FIXED);
        }
        if (switched_at != 0u &&
            AudioVoicePlayed(MUSIC_VOICE_LEFT) >=
                switched_at + MUSIC_FADE_FRAMES + MUSIC_BARRIER_FRAMES + PAST_BARRIER) {
            break;
        }
        MusicStreamFeed();
        stepped = AudioStep() && AudioNowMs() - start < STREAM_LIMIT_MS &&
                  MusicStreamQueued() + MUSIC_STREAM_BUFFERS * MUSIC_STREAM_CHUNK <= STREAM_FRAMES;
    }
    ReadRing();
    const uint64_t queued = MusicStreamQueued();
    const uint32_t dry = MusicStreamDry();
    const uint64_t played = AudioVoicePlayed(MUSIC_VOICE_LEFT);
    MusicStreamClose();
    AudioStep();
    if (!stepped) {
        ReportCase(SUITE, NAME, CHECK_EXACT, false,
                   AudioError()[0] != '\0'
                       ? AudioError()
                       : "the voices did not play past the barrier in 10 s, or the stream "
                         "outgrew the frames the case holds");
        return;
    }
    // The stream as the tracker renders it alone, switched where the stream was.
    MusicStart(&player, MUSIC_TRACK_MENU, MUSIC_SEED_FIXED);
    MusicRender(&player, reference, (size_t)switched_at);
    MusicSwitch(&player, MUSIC_TRACK_WIMSY, MUSIC_SEED_FIXED);
    MusicRender(&player, reference + 2u * switched_at, (size_t)(queued - switched_at));
    // The ring's oldest frame is where the aux would write next, at a frame's edge.
    bool found = false;
    uint64_t at = 0;
    uint32_t edge = 0;
    for (uint32_t w = 0; w < RING && !found; w += AUDIO_FRAME) {
        for (uint64_t s = 0; s + RING <= queued && !found; ++s) {
            uint32_t i = 0;
            while (i < RING && ring_left[(w + i) % RING] == reference[2u * (s + i)] &&
                   ring_right[(w + i) % RING] == reference[2u * (s + i) + 1u]) {
                ++i;
            }
            if (i == RING) {
                found = true;
                at = s;
                edge = w;
            }
        }
    }
    const int32_t peak = Peak32(ring_left, RING) > Peak32(ring_right, RING)
                             ? Peak32(ring_left, RING)
                             : Peak32(ring_right, RING);
    const uint64_t music_from = switched_at + MUSIC_FADE_FRAMES + MUSIC_BARRIER_FRAMES;
    if (found) {
        ReportNote("music: the stream's last %u frames recorded, from the ring's sample %u, were "
                   "frames %llu to %llu of its %llu queued; the switch at %llu, the next track "
                   "from %llu; %u times dry",
                   (unsigned)RING, (unsigned)edge, (unsigned long long)at,
                   (unsigned long long)(at + RING), (unsigned long long)queued,
                   (unsigned long long)switched_at, (unsigned long long)music_from,
                   (unsigned)dry);
    }
    if (found) {
        CHECK_TRUE(SUITE, NAME, CHECK_EXACT, peak > 0,
                   "the recording is frames %llu to %llu of the stream, all nought: the barrier, "
                   "where the case wants the next track's first, from %llu; the voices played "
                   "%llu, %u times dry",
                   (unsigned long long)at, (unsigned long long)(at + RING),
                   (unsigned long long)music_from, (unsigned long long)played, (unsigned)dry);
    } else {
        char detail[320];
        snprintf(detail, sizeof(detail),
                 "no stretch of the %llu frames queued is the recording, unrolled from any of "
                 "its frames' edges (its peak %d); the voices played %llu, the switch at %llu, "
                 "the next track from %llu, %u times dry",
                 (unsigned long long)queued, (int)peak, (unsigned long long)played,
                 (unsigned long long)switched_at, (unsigned long long)music_from, (unsigned)dry);
        ReportCase(SUITE, NAME, CHECK_EXACT, false, detail);
    }
}

/// Waits on the player's thread until its stream has queued @p frames, at most @p limit_ms.
static bool QueuedPast(uint64_t frames, uint64_t limit_ms) {
    const uint64_t start = AudioNowMs();
    while (MusicStreamQueued() < frames) {
        if (AudioNowMs() - start > limit_ms) {
            return false;
        }
        svcSleepThread(10000000ll);
    }
    return true;
}

/// The player the browser starts, with a thread and a renderer of its own, played here into the
/// submix the speakers hear at a gain of nought: it starts, a switch reaches its thread, a name no
/// track has is refused saying so, and a stop fades it out, joins the thread and closes the
/// renderer; one thread and two voices counted, and its stream never ran dry.
static void Threaded(void) {
    if (!ReportStart(SUITE, THREADED_NAME)) {
        return;
    }
    const MusicMade before = MusicMadeSoFar();
    char error[200] = "";
    char refused[200] = "";
    const bool started = MusicPlayStartInto("menu", AUDIO_SUBMIX, 1.0f, error, sizeof(error));
    bool switched = false;
    bool wrong = true;
    bool played = false;
    if (started) {
        played = QueuedPast(MUSIC_RATE / 2u, 3000u);
        switched = MusicPlaySwitch("wimsy", error, sizeof(error));
        wrong = MusicPlaySwitch("nothing", refused, sizeof(refused));
        // Past the four buffers queued before the switch, the fade and a chunk of the barrier.
        const uint64_t at = MusicStreamQueued();
        played = QueuedPast(at + (MUSIC_STREAM_BUFFERS + 1u) * MUSIC_STREAM_CHUNK +
                                MUSIC_FADE_FRAMES,
                            3000u) &&
                 played;
        MusicPlayStop();
    }
    const MusicMade after = MusicMadeSoFar();
    const uint32_t threads = after.threads - before.threads;
    const uint32_t voices = after.voices - before.voices;
    const bool silent = MusicStreamPlayer()->state == MUSIC_SILENT;
    CHECK_TRUE(SUITE, THREADED_NAME, CHECK_EXACT,
               started && played && switched && !wrong && strstr(refused, "\"nothing\"") != NULL &&
                   MusicPlaySwitches() == 1u && silent && !MusicPlayOn() && !AudioIsOpen() &&
                   threads == 1u && voices == 2u && MusicStreamDry() == 0u,
               "started %d (%s), played on %d, switched %d, a wrong name refused %d (\"%.60s\"), "
               "%u switches taken, faded out %d, still on %d, the renderer open %d, %u threads "
               "and %u voices counted, %u times dry",
               (int)started, error[0] != '\0' ? error : "no error", (int)played, (int)switched,
               (int)!wrong, refused, (unsigned)MusicPlaySwitches(), (int)silent,
               (int)MusicPlayOn(), (int)AudioIsOpen(), (unsigned)threads, (unsigned)voices,
               (unsigned)MusicStreamDry());
}

void RunMusic(void) {
    MusicSetAsked(true);
    const uint64_t start = AudioNowMs();
    RunMusicRender();
    const uint64_t rendered = AudioNowMs();
    char error[160] = "";
    const bool open = AudioOpen(error, sizeof(error));
    const char *names[MUSIC_TRACKS + 2u];
    static char excerpt_names[MUSIC_TRACKS][128];
    for (unsigned t = 0; t < MUSIC_TRACKS; ++t) {
        ExcerptName(excerpt_names[t], sizeof(excerpt_names[t]), (MusicTrack)t);
        names[t] = excerpt_names[t];
    }
    names[MUSIC_TRACKS] = STREAM_NAME;
    names[MUSIC_TRACKS + 1u] = THREADED_NAME;
    if (!open) {
        for (unsigned i = 0; i <= MUSIC_TRACKS + 1u; ++i) {
            ReportSkip(SUITE, names[i], REPORT_REASON_UNAVAILABLE, error);
        }
        MusicSetAsked(false);
        return;
    }
    ReportNote("music: renderer revision %u at 48 kHz; each voice at low quality and the output "
               "rate, into a submix at gain 0, recorded by its aux effect",
               AudioRevision());
    AudioSubmixGain(0.0f);
    int16_t *left = AudioAlloc(RING);
    int16_t *right = AudioAlloc(RING);
    for (unsigned t = 0; t < MUSIC_TRACKS; ++t) {
        if (left == NULL || right == NULL) {
            ReportSkip(SUITE, names[t], REPORT_REASON_UNAVAILABLE,
                       "no room in the renderer's memory pool");
        } else {
            Excerpt((MusicTrack)t, left, right);
        }
    }
    Stream();
    AudioCapture(false);
    AudioStep();
    if (AudioError()[0] != '\0') {
        ReportNote("music: the renderer said: %s", AudioError());
    }
    AudioClose();
    Threaded();
    const uint64_t end = AudioNowMs();
    ReportNote("music: the family took %llu ms: %llu rendering the tracks and the switch, %llu "
               "through the renderer",
               (unsigned long long)(end - start), (unsigned long long)(rendered - start),
               (unsigned long long)(end - rendered));
    MusicSetAsked(false);
}
