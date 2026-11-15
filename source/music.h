// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "musicscore.h"

#ifdef __cplusplus
extern "C" {
#endif

/// The demos' music: four tracks written note by note (source/musicscore.txt), played by an FM
/// synthesizer in the manner of the OPL3 an AdLib-era card carried. Every sample is worked out in
/// integers, so a track renders to the same samples on every machine, the host included, and the
/// music family holds a machine's samples to the host's (docs/families/Music.md).
///
/// The synthesizer: ten channels of two operators each, bass, three chord voices, lead,
/// counter-melody, arpeggio, kick, snare and hats. An operator is a phase stepped at its note's
/// frequency times its multiple, read through one of the OPL3's eight waveforms (or noise, for
/// the drums) in the log domain, a log-sine table and an exponential one as the chip computes,
/// its attenuation the sum of its envelope (attack, decay to a sustain level, release, at the
/// chip's sixteen rates), its total level and the tremolo. The modulator's output moves the
/// carrier's phase (or the two are added), the modulator can feed itself back, and each channel
/// sits left, right or in the middle. The sum goes through the card's coupling, a high pass near
/// 10 Hz, and the track's gain.

#define MUSIC_RATE 48000u
#define MUSIC_FM_CHANNELS 10u
/// A switch between tracks: the old one fades out over MUSIC_FADE_FRAMES, then
/// MUSIC_BARRIER_FRAMES of silence, then the new one from its first bar.
#define MUSIC_FADE_FRAMES 2400u
#define MUSIC_BARRIER_FRAMES 48000u

typedef enum {
    MUSIC_TRACK_MENU,
    MUSIC_TRACK_WIMSY,
    MUSIC_TRACK_FRACTAL,
    MUSIC_TRACK_FLUID,
    MUSIC_TRACKS,
} MusicTrack;

/// The modes a score's key names, in musicscore.txt's order.
typedef enum {
    MUSIC_IONIAN,
    MUSIC_DORIAN,
    MUSIC_PHRYGIAN,
    MUSIC_LYDIAN,
    MUSIC_MIXOLYDIAN,
    MUSIC_AEOLIAN,
    MUSIC_MODES,
} MusicMode;

/// The semitones above the tonic of each degree of @p mode, and its name.
const uint8_t *MusicModeSteps(MusicMode mode);
const char *MusicModeName(MusicMode mode);

/// A track's score, its name, and the track a name gives (MUSIC_TRACKS for none).
const MusicScore *MusicScoreOf(MusicTrack track);
const char *MusicTrackName(MusicTrack track);
MusicTrack MusicTrackNamed(const char *name);

/// The channels, and which side each sits on.
enum {
    MUSIC_CH_BASS,
    MUSIC_CH_CHORD1,
    MUSIC_CH_CHORD2,
    MUSIC_CH_CHORD3,
    MUSIC_CH_LEAD,
    MUSIC_CH_COUNTER,
    MUSIC_CH_ARP,
    MUSIC_CH_KICK,
    MUSIC_CH_SNARE,
    MUSIC_CH_HATS,
};
enum {
    MUSIC_LEFT = 1,
    MUSIC_RIGHT = 2,
    MUSIC_CENTRE = 3,
};
uint32_t MusicPanOf(uint32_t channel);

/// The OPL3's waveforms, 0 to 7, and two noises for the drums: white noise, and noise whose sign
/// a high square of the operator's own phase flips, as the chip's hats sound.
enum {
    MUSIC_WAVE_SINE,
    MUSIC_WAVE_HALF_SINE,
    MUSIC_WAVE_ABS_SINE,
    MUSIC_WAVE_QUARTER_SINE,
    MUSIC_WAVE_ALT_SINE,
    MUSIC_WAVE_CAMEL_SINE,
    MUSIC_WAVE_SQUARE,
    MUSIC_WAVE_LOG_SAW,
    MUSIC_WAVE_NOISE,
    MUSIC_WAVE_METAL,
    MUSIC_WAVES,
};

/// An operator's settings, the chip's fields: frequency multiple (0 is a half), total level in
/// 0.75 dB steps, attack, decay and release rates 0 to 15, sustain level in 3 dB steps (15 is
/// silence), waveform, whether the envelope holds at the sustain level while the key is down,
/// and the vibrato and tremolo.
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

/// An instrument: its two operators (the modulator first), the modulator's feedback 0 to 7, and
/// whether the two are added rather than one moving the other. Past the chip: a pitch that falls
/// from @p drop 256ths of a semitone above the note to it, a 64th of what is left each
/// @p drop_rate frames (the drums), and the note a drum plays at.
typedef struct {
    const char *name;
    MusicOperator op[2];
    uint8_t feedback;
    uint8_t additive;
    uint16_t drop;
    uint16_t drop_rate;
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
/// An operator's output, -4095 to 4095, for a 10-bit phase, its waveform, its attenuation in 256ths
/// of an octave (6.02 dB each 256), and the noise generator's bit.
int32_t MusicWave(uint32_t wave, uint32_t phase, uint32_t attenuation, uint32_t noise);
/// What a decay at @p rate adds to the attenuation each frame, and what share of the attenuation
/// an attack at @p rate takes off each frame, in 65536ths; attenuations are in 65536ths of a
/// 256th of an octave.
uint32_t MusicDecayStep(uint32_t rate);
uint32_t MusicAttackShare(uint32_t rate);
/// The frames an operator's envelope takes, run as it plays, for an attack at @p rate from silence
/// to its top, or a decay at @p rate from its top to silence (4000000 for a rate that never ends).
uint32_t MusicEnvelopeFrames(uint32_t rate, bool attack);

/// One operator as it plays.
typedef struct {
    uint32_t phase;
    uint32_t step;
    int32_t attenuation;
    uint8_t stage;
    int32_t out[2];
} MusicOperatorState;

/// One channel as it plays.
typedef struct {
    const MusicPatch *patch;
    MusicOperatorState op[2];
    bool key;
    bool active;
    /// Its pitch in 256ths of a semitone, the pitch a glide goes to and its step a frame, the pitch
    /// drop still to fall and the frames to its next fall, and the extra attenuation of its part
    /// and accent in 256ths of an octave.
    int32_t pitch;
    int32_t target;
    int32_t glide;
    int32_t drop;
    uint32_t drop_left;
    int32_t level;
    /// Frames since its key went down, for the vibrato that comes in late on a long note, and the
    /// frames until its key comes up (nought: it stays down).
    uint32_t age;
    uint32_t gate;
    /// The pitch its operators' steps were worked out for, and those steps.
    int32_t stepped_pitch;
} MusicChannel;

typedef enum {
    MUSIC_PLAYING,
    MUSIC_FADING,
    MUSIC_BARRIER,
    MUSIC_SILENT,
} MusicState;

/// A track ready to play: its score, and the patches its parts name.
typedef struct {
    MusicTrack track;
    const MusicScore *score;
    const MusicPatch *patch[MUSIC_SCORE_VOICES];
} MusicSong;

/// A track playing, and what it does next: a switch or a stop.
typedef struct {
    MusicSong song;
    MusicState state;
    MusicChannel channels[MUSIC_FM_CHANNELS];
    /// Where the score is: the form's entry, the bar in its section, the step in the bar, and the
    /// frames the step has left, the remainder of a step's frames carried to the next in 65536ths.
    uint32_t position;
    uint32_t bar;
    uint32_t step;
    uint32_t step_left;
    uint32_t step_carry;
    bool started;
    /// The lead's and counter-melody's notes: the next one's index and the steps the one sounding
    /// has left.
    uint32_t note_index[2];
    uint32_t note_left[2];
    /// The section's patterns: bass, drums, the drums' fill, hats, chords, arpeggio.
    const char *grid[6];
    /// The chord sounding: its three voices' notes, the six an arpeggio climbs, and the bass's
    /// root.
    int32_t voicing[3];
    int32_t arpeggio[6];
    int32_t root;
    uint32_t lfo;
    uint32_t noise;
    int32_t coupling[4];
    uint32_t fade_left;
    uint32_t barrier_left;
    MusicTrack next;
    uint64_t frames;
    uint64_t track_frames;
    uint64_t clipped;
} MusicPlayer;

/// Starts @p track from its first bar, its intro.
void MusicStart(MusicPlayer *player, MusicTrack track);
/// Starts with the silence barrier, then @p track from its first bar: what a switch sounds like
/// after its fade, for a stream that takes over from another one's silence.
void MusicStartAfterBarrier(MusicPlayer *player, MusicTrack track);
/// Fades out what plays, then the barrier, then @p track from its first bar; from silence, the
/// barrier is skipped. MUSIC_TRACKS fades out and stays silent; inside the barrier it ends it.
void MusicSwitch(MusicPlayer *player, MusicTrack track);
/// Writes @p frames stereo frames, left then right, into @p out.
void MusicRender(MusicPlayer *player, int16_t *out, size_t frames);
/// The frames of one pass of a track, its intro and its form once.
uint64_t MusicFormFrames(const MusicSong *song);

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
