// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "musiccompose.h"

#ifdef __cplusplus
extern "C" {
#endif

/// The demos' music: an AdLib tracker's song, composed afresh for each seed (musiccompose.c)
/// from the material each track's score holds, and played by an FM synthesizer in the manner of
/// the OPL2 an AdLib card carried. Every sample is worked out in integers, so a track and a seed
/// render to the same samples on every machine, the host included, and the music family holds a
/// machine's samples to the host's (docs/families/Music.md).
///
/// The synthesizer: nine channels of two operators each. An operator is a phase stepped at its
/// note's frequency times its multiple, read through one of the chip's eight waveforms in the
/// log domain (a log-sine table and an exponential one, as the chip computes), its attenuation
/// the sum of its envelope (attack, decay to a sustain level, release, at the chip's sixteen
/// rates), its total level or the channel's volume, and the tremolo. The modulator's output moves
/// the carrier's phase (or the two are added), and the modulator can feed itself back: at
/// feedback 7 it turns to noise, which is where the drums' and cymbals' noise comes from. The
/// player reads the composer's cells a row at a time and moves the effects (slides, tone slides,
/// volume slides) each tick, as a tracker does. Two chips play every channel, the right one about
/// 5 cents higher, the surround an AdLib player gives a mono card; each side goes through the
/// card's coupling, a high pass near 6 Hz, and the track's gain.

#define MUSIC_RATE 48000u
/// A switch between tracks: the old one fades out over MUSIC_FADE_FRAMES, then
/// MUSIC_BARRIER_FRAMES of silence, then the new one from its first bar.
#define MUSIC_FADE_FRAMES 2400u
#define MUSIC_BARRIER_FRAMES 48000u
/// The seed every run outside the interactive mode plays: the music family's cases, the quick
/// runs and the pinned hashes. The interactive mode seeds each start from the clock.
#define MUSIC_SEED_FIXED 1u
/// The right chip's pitch above the left's, in 256ths of a semitone (4.7 cents): the left and right
/// of a RAD tune played this way correlate as its recording does, 0.39 to its 0.43.
#define MUSIC_SURROUND 12

const uint8_t *MusicModeSteps(MusicMode mode);
const char *MusicModeName(MusicMode mode);

/// A track's score, its name, and the track a name gives (MUSIC_TRACKS for none).
const MusicScore *MusicScoreOf(MusicTrack track);
const char *MusicTrackName(MusicTrack track);
MusicTrack MusicTrackNamed(const char *name);

/// The chip's eight waveforms.
enum {
    MUSIC_WAVE_SINE,
    MUSIC_WAVE_HALF_SINE,
    MUSIC_WAVE_ABS_SINE,
    MUSIC_WAVE_QUARTER_SINE,
    MUSIC_WAVE_ALT_SINE,
    MUSIC_WAVE_CAMEL_SINE,
    MUSIC_WAVE_SQUARE,
    MUSIC_WAVE_LOG_SAW,
    MUSIC_WAVES,
};

/// An operator's settings, the chip's fields: frequency multiple (0 is a half), total level in
/// 0.75 dB steps, attack, decay and release rates 0 to 15, sustain level in 3 dB steps (15 is
/// -93 dB), waveform, whether the envelope holds at the sustain level while the key is down, and
/// the vibrato and tremolo.
typedef struct {
    uint8_t mult;
    uint8_t tl;
    uint8_t ar;
    uint8_t dr;
    uint8_t sl;
    uint8_t rr;
    uint8_t wave;
    uint8_t sustain;
    uint8_t vib;
    uint8_t am;
} MusicOperator;

/// An instrument: its two operators (the modulator first), the modulator's feedback 0 to 7,
/// whether the two are added rather than one moving the other, and the note a drum plays at
/// (nought for a melodic patch).
typedef struct {
    const char *name;
    MusicOperator op[2];
    uint8_t feedback;
    uint8_t additive;
    uint8_t note;
} MusicPatch;

extern const MusicPatch MUSIC_PATCHES[];
extern const size_t MUSIC_PATCH_COUNT;
/// The patch a name gives, NULL for none.
const MusicPatch *MusicPatchNamed(const char *name);

/// The chip's arithmetic, one piece at a time, for the reference suite.
///
/// A note's phase step a frame, in 32-bit fractions of a cycle, for a pitch in 256ths of a
/// semitone above MIDI note 0 (A4 is 69 * 256).
uint32_t MusicPitchStep(uint32_t pitch);
/// An operator's output, -4095 to 4095, for a 10-bit phase, its waveform and its attenuation in
/// 256ths of an octave (6.02 dB each 256).
int32_t MusicWave(uint32_t wave, uint32_t phase, uint32_t attenuation);
/// What a decay at @p rate adds to the attenuation each frame, and what share of the attenuation
/// an attack at @p rate takes off each frame, in 65536ths; attenuations are in 65536ths of a
/// 256th of an octave.
uint32_t MusicDecayStep(uint32_t rate);
uint32_t MusicAttackShare(uint32_t rate);
/// The frames an operator's envelope takes, run as it plays, for an attack at @p rate from silence
/// to its top, or a decay at @p rate from its top to silence (4000000 for a rate that never ends).
uint32_t MusicEnvelopeFrames(uint32_t rate, bool attack);

/// One operator as it plays: its envelope, and its phase, phase step and last two outputs on each
/// chip.
typedef struct {
    int32_t attenuation;
    uint8_t stage;
    uint32_t phase[2];
    uint32_t step[2];
    int32_t out[2][2];
} MusicOperatorState;

/// One channel as it plays: its patch, its pitch in 256ths of a semitone and where a tone slide
/// takes it (and the pitch its phase steps were worked out for), the carrier's volume 0 to 63 (and
/// the modulator's, for an added patch), and the effect its row asked for.
typedef struct {
    const MusicPatch *patch;
    MusicOperatorState op[2];
    bool active;
    int32_t pitch;
    int32_t target;
    int32_t stepped;
    uint8_t volume;
    uint8_t mod_volume;
    uint8_t effect;
    int8_t param;
} MusicChannel;

typedef enum {
    MUSIC_PLAYING,
    MUSIC_FADING,
    MUSIC_BARRIER,
    MUSIC_SILENT,
} MusicState;

/// A track playing, and what it does next: a switch or a stop.
typedef struct {
    MusicTrack track;
    uint32_t seed;
    MusicComposer composer;
    MusicState state;
    MusicChannel channels[MUSIC_CHANNELS];
    /// The bar the composer wrote, the row playing in it, the tick in the row and the frames the
    /// tick has left, a tick's fraction of a frame carried in 65536ths.
    MusicCell bar[MUSIC_BAR_ROWS][MUSIC_CHANNELS];
    uint32_t row;
    uint32_t tick;
    uint32_t tick_left;
    uint32_t tick_carry;
    bool started;
    uint32_t lfo;
    int32_t coupling[4];
    uint32_t fade_left;
    uint32_t barrier_left;
    MusicTrack next;
    uint32_t next_seed;
    uint64_t frames;
    uint64_t track_frames;
    uint64_t clipped;
} MusicPlayer;

/// Starts @p track from its first bar, its intro, composed from @p seed.
void MusicStart(MusicPlayer *player, MusicTrack track, uint32_t seed);
/// Starts with the silence barrier, then @p track from its first bar: what a switch sounds like
/// after its fade, for a stream that takes over from another one's silence.
void MusicStartAfterBarrier(MusicPlayer *player, MusicTrack track, uint32_t seed);
/// Fades out what plays, then the barrier, then @p track from its first bar, composed from
/// @p seed; from silence, the barrier is skipped. MUSIC_TRACKS fades out and stays silent; inside
/// the barrier it ends it.
void MusicSwitch(MusicPlayer *player, MusicTrack track, uint32_t seed);
/// Writes @p frames stereo frames, left then right, into @p out.
void MusicRender(MusicPlayer *player, int16_t *out, size_t frames);
/// The frames of the first pass a seed composes of a track, its intro included.
uint64_t MusicPassFrames(MusicTrack track, uint32_t seed);

/// FNV-1a over 16-bit samples, low byte first: what the music family compares.
uint64_t MusicHash(uint64_t hash, const int16_t *samples, size_t count);
#define MUSIC_HASH_START 0xCBF29CE484222325ull

/// What the music made since the program started: frames rendered, threads and voices the player
/// started. Outside the interactive mode, and outside the music family's cases, each is nought
/// (the game and demo families' last cases hold it to that).
typedef struct {
    uint64_t samples;
    uint32_t threads;
    uint32_t voices;
} MusicMade;
MusicMade MusicMadeSoFar(void);
void MusicCountThread(void);
void MusicCountVoice(void);

/// Whether the music may play: in the interactive mode, where a person browses and runs the
/// demos, and inside the music family's cases, which ask for it. Neither is on by default:
/// the interactive screens set the first.
void MusicSetInteractive(bool on);
bool MusicInteractive(void);
void MusicSetAsked(bool on);
bool MusicMayPlay(void);

#ifdef __cplusplus
}
#endif
