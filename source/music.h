// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// The demos' music: a tracker of four channels in the Amiga's manner, and a composer that writes
/// each track's patterns from a seed and the track's tonal colour. Every sample is worked out in
/// integers, so a track renders to the same samples on every machine, the host included, and the
/// music family holds a machine's samples to the host's (docs/families/Music.md).
///
/// The tracker: four channels, each playing one of the probe's own 8-bit samples (single cycles
/// of a square, pulses, a triangle, a saw, a sine, an organ; a kick, a snare and a hat), stepped
/// at the rate the Amiga's sound chip reads a sample at for a period (MUSIC_PAULA_CLOCK over the
/// period) and held between steps as that chip holds them; channels 0 and 3 to the left and 1 and
/// 2 to the right, each with three eighths of the other side's, as a player softens the chip's
/// hard stereo; then the A500's fixed low pass, and for some tracks the filter its power light
/// switches. Rows of a pattern, ticks of a row, the effects a tracker of that era has (arpeggio,
/// slides, tone portamento, vibrato, volume, cut, delay, retrigger), and a volume envelope per
/// instrument.
///
/// The composer: each track's colour is a mode on a tonic, a register for each part, timbres, a
/// voicing for its chords, a tempo and a groove. The form is A A B A, 32 bars: A's progression
/// twice, the second time closing on the tonic, a bridge on another progression that ends on the
/// chord leading home, and A again. Channel 0 is the bass on each chord's root, 1 the lead, whose
/// notes keep to the mode and fall on chord tones on the beats, 2 the chord as an arpeggio in the
/// track's voicing, and 3 the drums, or an echo of the lead.

#define MUSIC_RATE 48000u
#define MUSIC_CHANNELS 4u
/// The PAL Amiga's clock for its sound chip: a channel reads its next sample every period ticks.
#define MUSIC_PAULA_CLOCK 3546895u
/// The notes a pattern names, three octaves, 1 for the lowest; 0 is no note.
#define MUSIC_NOTES 36u
/// A cell's note that releases the channel's envelope.
#define MUSIC_NOTE_OFF 0x7Fu
#define MUSIC_ROWS_MAX 96u
/// Bars in a pattern, and patterns in a track: A's head, A's open and closed tails, the bridge's
/// head and tail.
#define MUSIC_PATTERN_BARS 4u
#define MUSIC_PATTERNS 5u
/// The positions of the form, A A B A at two patterns each, and its bars.
#define MUSIC_ORDER 8u
#define MUSIC_BARS (MUSIC_ORDER * MUSIC_PATTERN_BARS)
#define MUSIC_INSTRUMENTS 8u
#define MUSIC_BANK_MAX 10240u
/// A switch between tracks: the old one fades out over MUSIC_FADE_FRAMES, then
/// MUSIC_BARRIER_FRAMES of silence, then the new one from its first row.
#define MUSIC_FADE_FRAMES 2400u
#define MUSIC_BARRIER_FRAMES 48000u

typedef enum {
    MUSIC_TRACK_MENU,
    MUSIC_TRACK_WIMSY,
    MUSIC_TRACK_FRACTAL,
    MUSIC_TRACK_FLUID,
    MUSIC_TRACKS,
} MusicTrack;

typedef enum {
    MUSIC_IONIAN,
    MUSIC_DORIAN,
    MUSIC_PHRYGIAN,
    MUSIC_LYDIAN,
    MUSIC_MIXOLYDIAN,
    MUSIC_AEOLIAN,
    MUSIC_MODES,
} MusicMode;

/// The two notes an arpeggio adds to a chord's root.
typedef enum {
    /// The third and the fifth.
    MUSIC_VOICING_TRIAD,
    /// The third and the seventh: the shell of a seventh chord.
    MUSIC_VOICING_SHELL,
    /// The second and the fifth: a suspended chord.
    MUSIC_VOICING_SUS2,
    /// The fifth and the ninth: open, no third.
    MUSIC_VOICING_OPEN,
} MusicVoicing;

/// The waves the probe makes for its instruments.
typedef enum {
    MUSIC_WAVE_SQUARE32,
    MUSIC_WAVE_PULSE16,
    MUSIC_WAVE_PULSE32,
    MUSIC_WAVE_THIN32,
    MUSIC_WAVE_TRIANGLE32,
    MUSIC_WAVE_TRIANGLE64,
    MUSIC_WAVE_SAW16,
    MUSIC_WAVE_SAW32,
    MUSIC_WAVE_SINE64,
    MUSIC_WAVE_ORGAN32,
    MUSIC_WAVE_SQUARE64,
    MUSIC_WAVE_KICK,
    MUSIC_WAVE_SNARE,
    MUSIC_WAVE_HAT,
    MUSIC_WAVES,
} MusicWave;

/// The rhythms each part may take, per bar.
typedef enum {
    MUSIC_GROOVE_CALM,
    MUSIC_GROOVE_BOUNCE,
    MUSIC_GROOVE_SLOW,
    MUSIC_GROOVE_FLOW,
} MusicGroove;

typedef enum {
    MUSIC_DRUMS_STRAIGHT,
    MUSIC_DRUMS_SHUFFLE,
    MUSIC_DRUMS_TWELVE,
    /// No drums: channel 3 plays the lead again, later and softer.
    MUSIC_DRUMS_ECHO,
} MusicDrums;

typedef enum {
    MUSIC_FILTER_A500,
    /// The A500's filter and the one its power light switches.
    MUSIC_FILTER_LED,
} MusicFilter;

/// One part's sound: a wave, its volume, and its envelope in ticks: rising by @p attack a tick to
/// 255, falling by @p decay a tick to @p sustain, and by @p release a tick once released.
typedef struct {
    uint8_t wave;
    uint8_t volume;
    uint8_t attack;
    uint8_t decay;
    uint8_t sustain;
    uint8_t release;
} MusicVoice;

/// A track's tonal colour: what makes it the track it is before a note is chosen.
typedef struct {
    const char *name;
    /// The demo it accompanies, and the words a person reads about its colour.
    const char *demo;
    const char *colour;
    uint32_t seed;
    /// The tonic as a pitch class, 0 for C, and the mode on it.
    uint8_t tonic;
    uint8_t mode;
    /// The tracker's tempo (a tick lasts 2.5 / bpm seconds), ticks a row, and the ticks moved from
    /// the second half of each beat to the first: a shuffle.
    uint8_t bpm;
    uint8_t speed;
    uint8_t swing;
    uint8_t rows_per_beat;
    uint8_t beats_per_bar;
    /// The lowest note each part's register starts from, 0 to 35, and its sound.
    uint8_t lead_base;
    uint8_t chord_base;
    uint8_t bass_base;
    MusicVoice lead;
    MusicVoice chord;
    MusicVoice bass;
    uint8_t voicing;
    uint8_t groove;
    uint8_t drums;
    uint8_t filter;
    /// The lead's vibrato on held notes, a tracker's 4xy, and whether it glides into some notes.
    uint8_t vibrato;
    bool glide;
} MusicColour;

const MusicColour *MusicColourOf(MusicTrack track);
/// The track a name gives, MUSIC_TRACKS for a name no track has.
MusicTrack MusicTrackNamed(const char *name);

/// A cell of a pattern: one channel's word on one row.
typedef struct {
    uint8_t note;
    uint8_t instrument;
    uint8_t effect;
    uint8_t param;
} MusicCell;

/// The tracker's effects, by the numbers trackers of that era give them.
enum {
    MUSIC_FX_ARPEGGIO = 0x0,
    MUSIC_FX_SLIDE_UP = 0x1,
    MUSIC_FX_SLIDE_DOWN = 0x2,
    MUSIC_FX_PORTAMENTO = 0x3,
    MUSIC_FX_VIBRATO = 0x4,
    MUSIC_FX_VOLUME_SLIDE = 0xA,
    MUSIC_FX_VOLUME = 0xC,
    /// 0xE with the parameter's high nibble: 0x9 retrigger, 0xC cut, 0xD delay.
    MUSIC_FX_EXTENDED = 0xE,
};

/// The instruments a song's cells name, 1 to MUSIC_INSTRUMENTS.
enum {
    MUSIC_INSTRUMENT_LEAD = 1,
    MUSIC_INSTRUMENT_CHORD = 2,
    MUSIC_INSTRUMENT_BASS = 3,
    MUSIC_INSTRUMENT_KICK = 4,
    MUSIC_INSTRUMENT_SNARE = 5,
    MUSIC_INSTRUMENT_HAT = 6,
    MUSIC_INSTRUMENT_ECHO = 7,
};

typedef struct {
    uint32_t offset;
    uint32_t length;
    /// A looped wave repeats from loop_start to its end; a one-shot (loop_start == length) stops.
    uint32_t loop_start;
    MusicVoice voice;
} MusicInstrument;

/// A track composed: its patterns and the form's order, what the judge reads of the composition,
/// and the samples its instruments play.
typedef struct {
    MusicTrack track;
    uint32_t seed;
    MusicColour colour;
    uint32_t rows;
    uint32_t rows_per_bar;
    MusicCell cells[MUSIC_PATTERNS][MUSIC_ROWS_MAX][MUSIC_CHANNELS];
    uint8_t order[MUSIC_ORDER];
    /// 'A' or 'B' for each position.
    char section[MUSIC_ORDER];
    /// The scale degree, 0 to 6, each bar's chord stands on, in the form's order of bars, and the
    /// two semitone offsets its arpeggio adds.
    uint8_t chord[MUSIC_BARS];
    uint8_t arp[MUSIC_BARS];
    /// The pitch classes of the mode on the tonic, low to high from the tonic.
    uint8_t scale[7];
    MusicInstrument instruments[MUSIC_INSTRUMENTS + 1u];
    int8_t bank[MUSIC_BANK_MAX];
    uint32_t bank_used;
} MusicSong;

/// Writes @p track's song from @p seed; the colour's own seed is the track as it ships.
void MusicCompose(MusicSong *song, MusicTrack track, uint32_t seed);

/// The semitones above the tonic of each degree of @p mode.
const uint8_t *MusicModeSteps(MusicMode mode);
const char *MusicModeName(MusicMode mode);

/// The period of a note, 1 to MUSIC_NOTES, as the Amiga's tracker tunes it: 856 for the lowest,
/// each semitone 2 to the -1/12 times the one below, rounded.
uint32_t MusicPeriod(uint32_t note);
/// How far a channel steps through its sample each output sample at @p period: the chip's rate
/// over the output's, in 16.16 fixed point.
uint32_t MusicStep(uint32_t period);
/// The frames a tick lasts at @p bpm, 2.5 / bpm seconds, the remainder carried from tick to tick
/// in @p carry so the tempo holds exactly over a track.
uint32_t MusicTickFrames(uint32_t bpm, uint32_t *carry);

/// One output frame from the four channels' values: the left takes channels 0 and 3 and three
/// eighths of 1 and 2, the right the other way round. Each value is a sample times its gain, at most
/// MUSIC_CHANNEL_PEAK in size.
#define MUSIC_CHANNEL_PEAK 8160
void MusicPan(const int32_t channels[MUSIC_CHANNELS], int32_t *left, int32_t *right);

/// The A500's low pass, one pole near 4.4 kHz, and the power light's, two more near 3.3 kHz, in
/// 16-bit fixed point: what one step moves a state of @p state toward @p input.
int32_t MusicFilterStep(int32_t state, int32_t input, uint32_t coefficient);
#define MUSIC_FILTER_A500_Q16 28790u
#define MUSIC_FILTER_LED_Q16 22853u

/// A channel of the tracker as it plays.
typedef struct {
    uint32_t instrument;
    uint32_t note;
    uint32_t period;
    uint32_t base_period;
    uint32_t target_period;
    uint32_t portamento;
    uint32_t position;
    uint32_t step;
    bool playing;
    int32_t volume;
    int32_t envelope;
    bool released;
    bool attacking;
    uint32_t vibrato_position;
    uint8_t vibrato;
    MusicCell cell;
    /// What the last tick's gain was, and this tick's, ramped between over its first samples.
    int32_t gain_was;
    int32_t gain;
} MusicChannel;

typedef enum {
    MUSIC_PLAYING,
    MUSIC_FADING,
    MUSIC_BARRIER,
    MUSIC_SILENT,
} MusicState;

/// A tracker playing a track, and what it does next: a switch or a stop.
typedef struct {
    MusicSong song;
    MusicState state;
    MusicChannel channels[MUSIC_CHANNELS];
    uint32_t position;
    uint32_t row;
    uint32_t tick;
    uint32_t row_ticks;
    uint32_t tick_left;
    uint32_t tick_carry;
    uint32_t fade_left;
    uint32_t barrier_left;
    uint32_t tick_elapsed;
    /// The track after the barrier, MUSIC_TRACKS for none: the fade ends in silence.
    MusicTrack next;
    /// The low passes' states, left and right, in 24.8 fixed point: the A500's, and the power
    /// light's two poles a side.
    int32_t filter[2];
    int32_t led[4];
    /// Frames rendered since the player started, and those of the current track.
    uint64_t frames;
    uint64_t track_frames;
    /// Samples clamped to 16 bits.
    uint64_t clipped;
} MusicPlayer;

/// Starts @p track from its first row.
void MusicStart(MusicPlayer *player, MusicTrack track);
/// The same with a seed of the caller's.
void MusicStartSeeded(MusicPlayer *player, MusicTrack track, uint32_t seed);
/// Starts with the silence barrier, then @p track from its first row: what a switch sounds like
/// after its fade, for a stream that takes over from another one's silence.
void MusicStartAfterBarrier(MusicPlayer *player, MusicTrack track);
/// Fades out what plays, then the barrier, then @p track from its first row; from silence, the
/// barrier is skipped. MUSIC_TRACKS fades out and stays silent.
void MusicSwitch(MusicPlayer *player, MusicTrack track);
/// Writes @p frames stereo frames, left then right, into @p out.
void MusicRender(MusicPlayer *player, int16_t *out, size_t frames);
/// The frames one pass of a track's form lasts, from its first row to its last.
uint64_t MusicFormFrames(const MusicSong *song);

/// FNV-1a over 16-bit samples, low byte first: what the music family compares.
uint64_t MusicHash(uint64_t hash, const int16_t *samples, size_t count);
#define MUSIC_HASH_START 0xCBF29CE484222325ull

/// What the music made since the program started: samples rendered, threads and voices the
/// player started. Outside the interactive mode, and outside the music family's cases, each is
/// nought (the game family's last case holds it to that).
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
