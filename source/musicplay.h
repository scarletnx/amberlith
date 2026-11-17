// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "music.h"

#ifdef __cplusplus
extern "C" {
#endif

/// The demos' music on the console: the tracker's samples (music.h) streamed through the audio
/// renderer audio.c holds, the game family's path (nn::audio's renderer in the hooks mode), on two
/// voices of its own at the output rate and at low quality, which passes a sample through
/// unchanged. Nothing here runs unless MusicMayPlay() says so: the interactive mode, or the music
/// family's cases. Otherwise every call is refused and says so, and no thread, voice or sample is
/// made (MusicMadeSoFar).
///
/// Two ways in. A demo that does not step the renderer itself, and the browser, call
/// MusicPlayStart: it opens the renderer, starts a thread that feeds the stream and steps the
/// renderer, and MusicPlayStop fades it out and closes it all. A demo that steps the renderer
/// itself (the game scene's audio thread) opens the stream on its renderer with MusicStreamOpen
/// and calls MusicStreamFeed before each AudioStep.

/// The voices the stream plays on, left and right; the game scene's tone is on voice 0.
#define MUSIC_VOICE_LEFT 6u
#define MUSIC_VOICE_RIGHT 7u
/// A wave buffer's frames, 40 ms, eight of the renderer's frames; four are queued a voice.
#define MUSIC_STREAM_CHUNK 1920u
#define MUSIC_STREAM_BUFFERS 4u
/// The gain MusicPlayStart plays at, into the final mix: -6 dB under the tracker's own level.
#define MUSIC_PLAY_GAIN 0.5f

/// The seed a start or a switch composes from: in the interactive mode the console's tick, so no
/// two starts play the same song; outside it MUSIC_SEED_FIXED, the one the cases pin.
uint32_t MusicSeedNow(void);

/// Opens the stream of @p track composed from @p seed on the open renderer, into @p mix at
/// @p gain, from the silence barrier when @p barrier_first (a demo taking over from the browser's
/// music). False, with @p error saying why, when the music may not play, the renderer is not
/// open, or its pool has no room.
bool MusicStreamOpen(MusicTrack track, uint32_t seed, unsigned mix, float gain,
                     bool barrier_first, char *error, size_t error_size);
/// Queues what the voices will run out of next. Before each AudioStep.
void MusicStreamFeed(void);
/// The next track, composed from @p seed, after the fade and the barrier; MUSIC_TRACKS fades out
/// and stays silent.
void MusicStreamSwitch(MusicTrack track, uint32_t seed);
/// Whether the stream is open, and the player it renders from.
bool MusicStreamOn(void);
const MusicPlayer *MusicStreamPlayer(void);
/// Frames queued since the stream opened, and the times a feed found every buffer played: the
/// voices ran dry, and a person heard a gap.
uint64_t MusicStreamQueued(void);
uint32_t MusicStreamDry(void);
/// Stops the voices on the next update. The renderer stays open.
void MusicStreamClose(void);

/// The threaded player: the renderer opened, the stream of the track named @p track
/// (MusicTrackName's names: menu, wimsy, fractal, fluid) composed from MusicSeedNow() into the
/// final mix at MUSIC_PLAY_GAIN, a thread on core 2 feeding and stepping it. False, with @p error
/// saying why: the music may not play, the name is no track's, the renderer is open already
/// (MusicStreamOpen is the way in then), or it failed.
bool MusicPlayStart(const char *track, char *error, size_t error_size);
/// MusicPlayStart into @p mix at @p gain: the music family's case plays it into the submix the
/// speakers hear at a gain of nought.
bool MusicPlayStartInto(const char *track, unsigned mix, float gain, char *error,
                        size_t error_size);
/// The player's next track, composed from MusicSeedNow(), after the fade and the barrier. False,
/// with @p error saying why, for a name no track has or a player that is not running.
bool MusicPlaySwitch(const char *track, char *error, size_t error_size);
/// Fades the player out, stops its thread and closes the renderer. Nothing when it is not running.
void MusicPlayStop(void);
bool MusicPlayOn(void);
/// The switches the player's thread took since it started, read once it has stopped.
uint32_t MusicPlaySwitches(void);

#ifdef __cplusplus
}
#endif
