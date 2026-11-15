// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string.h>

#include "music.h"

// Integers only, from the composer's choices to the last sample: a float rounds as the
// compiler pleases (a fused multiply on one processor, two roundings on another), and the music
// family holds a machine's samples to the host's one for one.

// ---------------------------------------------------------------------------------------------
// The tables

static const uint8_t MODE_STEPS[MUSIC_MODES][7] = {
    [MUSIC_IONIAN] = { 0, 2, 4, 5, 7, 9, 11 },     [MUSIC_DORIAN] = { 0, 2, 3, 5, 7, 9, 10 },
    [MUSIC_PHRYGIAN] = { 0, 1, 3, 5, 7, 8, 10 },   [MUSIC_LYDIAN] = { 0, 2, 4, 6, 7, 9, 11 },
    [MUSIC_MIXOLYDIAN] = { 0, 2, 4, 5, 7, 9, 10 }, [MUSIC_AEOLIAN] = { 0, 2, 3, 5, 7, 8, 10 },
};

static const char *const MODE_NAMES[MUSIC_MODES] = {
    [MUSIC_IONIAN] = "ionian",         [MUSIC_DORIAN] = "dorian", [MUSIC_PHRYGIAN] = "phrygian",
    [MUSIC_LYDIAN] = "lydian",         [MUSIC_MIXOLYDIAN] = "mixolydian",
    [MUSIC_AEOLIAN] = "aeolian",
};

// round(856 * 2^(-n/12)): 856 is the lowest note's period, the octaves halve it.
static const uint16_t PERIODS[MUSIC_NOTES] = {
    856, 808, 763, 720, 679, 641, 605, 571, 539, 509, 480, 453,
    428, 404, 381, 360, 340, 321, 303, 286, 270, 254, 240, 227,
    214, 202, 191, 180, 170, 160, 151, 143, 135, 127, 120, 113,
};
#define PERIOD_MIN 113u
#define PERIOD_MAX 856u

// round(127 * sin(i * pi / 128)) for i from 0 to 64: a quarter of a sine of 256 steps.
static const int8_t QUARTER[65] = {
    0,   3,   6,   9,   12,  16,  19,  22,  25,  28,  31,  34,  37,  40,  43,  46,  49,
    51,  54,  57,  60,  63,  65,  68,  71,  73,  76,  78,  81,  83,  85,  88,  90,  92,
    94,  96,  98,  100, 102, 104, 106, 107, 109, 111, 112, 113, 115, 116, 117, 118, 120,
    121, 122, 122, 123, 124, 125, 125, 126, 126, 126, 127, 127, 127, 127,
};

// round(255 * sin(i * pi / 32)) for i from 0 to 31: half a sine, the vibrato's shape.
static const uint8_t VIBRATO[32] = {
    0,   25,  50,  74,  98,  120, 142, 162, 180, 197, 212, 225, 236, 244, 250, 254,
    255, 254, 250, 244, 236, 225, 212, 197, 180, 162, 142, 120, 98,  74,  50,  25,
};

const uint8_t *MusicModeSteps(MusicMode mode) {
    return MODE_STEPS[(unsigned)mode < MUSIC_MODES ? mode : MUSIC_IONIAN];
}

const char *MusicModeName(MusicMode mode) {
    return (unsigned)mode < MUSIC_MODES ? MODE_NAMES[mode] : "none";
}

uint32_t MusicPeriod(uint32_t note) {
    if (note < 1u) {
        return PERIODS[0];
    }
    return PERIODS[(note > MUSIC_NOTES ? MUSIC_NOTES : note) - 1u];
}

uint32_t MusicStep(uint32_t period) {
    const uint64_t p = period < PERIOD_MIN ? PERIOD_MIN : period > PERIOD_MAX ? PERIOD_MAX : period;
    return (uint32_t)(((uint64_t)MUSIC_PAULA_CLOCK << 16) / (p * MUSIC_RATE));
}

uint32_t MusicTickFrames(uint32_t bpm, uint32_t *carry) {
    // A tick lasts 2.5 / bpm seconds: 120000 frames over the bpm, the remainder carried so the
    // ticks keep the tempo exactly over a whole track.
    const uint32_t total = 120000u + *carry;
    *carry = total % bpm;
    return total / bpm;
}

static int32_t Sine256(uint32_t i) {
    i &= 255u;
    const uint32_t half = i & 127u;
    const int32_t value = half <= 64u ? QUARTER[half] : QUARTER[128u - half];
    return i < 128u ? value : -value;
}

void MusicPan(const int32_t channels[MUSIC_CHANNELS], int32_t *left, int32_t *right) {
    const int32_t a = channels[0] + channels[3];
    const int32_t b = channels[1] + channels[2];
    *left = a + ((b * 3) >> 3);
    *right = b + ((a * 3) >> 3);
}

int32_t MusicFilterStep(int32_t state, int32_t input, uint32_t coefficient) {
    const int64_t distance = ((int64_t)input << 8) - state;
    return state + (int32_t)((distance * (int64_t)coefficient) >> 16);
}

// ---------------------------------------------------------------------------------------------
// What the music made, and whether it may play

static struct {
    bool interactive;
    bool asked;
    uint64_t frames;
    uint32_t threads;
    uint32_t voices;
} g_music;

MusicMade MusicMadeSoFar(void) {
    MusicMade made;
    made.samples = __atomic_load_n(&g_music.frames, __ATOMIC_RELAXED);
    made.threads = __atomic_load_n(&g_music.threads, __ATOMIC_RELAXED);
    made.voices = __atomic_load_n(&g_music.voices, __ATOMIC_RELAXED);
    return made;
}

void MusicCountThread(void) {
    __atomic_add_fetch(&g_music.threads, 1u, __ATOMIC_RELAXED);
}

void MusicCountVoice(void) {
    __atomic_add_fetch(&g_music.voices, 1u, __ATOMIC_RELAXED);
}

void MusicSetInteractive(bool on) {
    __atomic_store_n(&g_music.interactive, on, __ATOMIC_RELEASE);
}

bool MusicInteractive(void) {
    return __atomic_load_n(&g_music.interactive, __ATOMIC_ACQUIRE);
}

void MusicSetAsked(bool on) {
    __atomic_store_n(&g_music.asked, on, __ATOMIC_RELEASE);
}

bool MusicMayPlay(void) {
    return MusicInteractive() || __atomic_load_n(&g_music.asked, __ATOMIC_ACQUIRE);
}

// ---------------------------------------------------------------------------------------------
// The tracks' colours

static const MusicColour COLOURS[MUSIC_TRACKS] = {
    [MUSIC_TRACK_MENU] = {
        .name = "menu",
        .demo = "the demo browser",
        .colour = "C mixolydian, a major with its seventh lowered: unhurried, plucked eighths "
                  "over a triangle bass, open fifths and ninths, a kit on the beat",
        .seed = 0x6D656E75u,
        .tonic = 0,
        .mode = MUSIC_MIXOLYDIAN,
        .bpm = 112,
        .speed = 6,
        .swing = 0,
        .rows_per_beat = 4,
        .beats_per_bar = 4,
        .lead_base = 12,
        .chord_base = 0,
        .bass_base = 0,
        .lead = { MUSIC_WAVE_PULSE32, 44, 255, 6, 150, 24 },
        .chord = { MUSIC_WAVE_THIN32, 30, 255, 22, 0, 40 },
        .bass = { MUSIC_WAVE_TRIANGLE64, 60, 255, 2, 200, 32 },
        .voicing = MUSIC_VOICING_OPEN,
        .groove = MUSIC_GROOVE_CALM,
        .drums = MUSIC_DRUMS_STRAIGHT,
        .filter = MUSIC_FILTER_A500,
        .vibrato = 0x42,
        .glide = false,
    },
    [MUSIC_TRACK_WIMSY] = {
        .name = "wimsy",
        .demo = "Wimsy's levels",
        .colour = "F lydian, a major with its fourth raised: bright and playful, a high pulse "
                  "lead, shuffled eighths, chords as fast arpeggios on the offbeats, a bouncing "
                  "square bass",
        .seed = 0x57494D53u,
        .tonic = 5,
        .mode = MUSIC_LYDIAN,
        .bpm = 140,
        .speed = 6,
        .swing = 1,
        .rows_per_beat = 4,
        .beats_per_bar = 4,
        .lead_base = 0,
        .chord_base = 12,
        .bass_base = 0,
        .lead = { MUSIC_WAVE_PULSE16, 40, 255, 10, 120, 30 },
        .chord = { MUSIC_WAVE_SQUARE32, 22, 255, 30, 0, 48 },
        .bass = { MUSIC_WAVE_SQUARE64, 44, 255, 8, 110, 40 },
        .voicing = MUSIC_VOICING_TRIAD,
        .groove = MUSIC_GROOVE_BOUNCE,
        .drums = MUSIC_DRUMS_SHUFFLE,
        .filter = MUSIC_FILTER_A500,
        .vibrato = 0x62,
        .glide = false,
    },
    [MUSIC_TRACK_FRACTAL] = {
        .name = "fractal",
        .demo = "the Mandelbrot and Julia zooms",
        .colour = "E phrygian, a minor with its second lowered: dark and slow, a soft triangle "
                  "lead that swells, its echo for drums, seventh chords held on an organ, a sine "
                  "bass, the power light's filter on",
        .seed = 0x46524143u,
        .tonic = 4,
        .mode = MUSIC_PHRYGIAN,
        .bpm = 84,
        .speed = 6,
        .swing = 0,
        .rows_per_beat = 4,
        .beats_per_bar = 4,
        .lead_base = 12,
        .chord_base = 0,
        .bass_base = 0,
        .lead = { MUSIC_WAVE_TRIANGLE32, 50, 40, 2, 200, 8 },
        .chord = { MUSIC_WAVE_ORGAN32, 26, 16, 1, 170, 6 },
        .bass = { MUSIC_WAVE_SINE64, 62, 64, 1, 220, 10 },
        .voicing = MUSIC_VOICING_SHELL,
        .groove = MUSIC_GROOVE_SLOW,
        .drums = MUSIC_DRUMS_ECHO,
        .filter = MUSIC_FILTER_LED,
        .vibrato = 0x33,
        .glide = false,
    },
    [MUSIC_TRACK_FLUID] = {
        .name = "fluid",
        .demo = "the fluid simulation",
        .colour = "D dorian, a minor with its sixth raised: twelve eight, a saw lead that "
                  "glides between notes, suspended chords that swell, a rolling triangle bass",
        .seed = 0x464C5549u,
        .tonic = 2,
        .mode = MUSIC_DORIAN,
        .bpm = 125,
        .speed = 4,
        .swing = 0,
        .rows_per_beat = 6,
        .beats_per_bar = 4,
        .lead_base = 12,
        .chord_base = 0,
        .bass_base = 0,
        .lead = { MUSIC_WAVE_SAW32, 38, 96, 4, 170, 16 },
        .chord = { MUSIC_WAVE_PULSE32, 22, 24, 2, 140, 10 },
        .bass = { MUSIC_WAVE_TRIANGLE64, 58, 255, 6, 140, 20 },
        .voicing = MUSIC_VOICING_SUS2,
        .groove = MUSIC_GROOVE_FLOW,
        .drums = MUSIC_DRUMS_TWELVE,
        .filter = MUSIC_FILTER_A500,
        .vibrato = 0x52,
        .glide = true,
    },
};

const MusicColour *MusicColourOf(MusicTrack track) {
    return (unsigned)track < MUSIC_TRACKS ? &COLOURS[track] : NULL;
}

MusicTrack MusicTrackNamed(const char *name) {
    for (unsigned i = 0; name != NULL && i < MUSIC_TRACKS; ++i) {
        if (strcmp(COLOURS[i].name, name) == 0) {
            return (MusicTrack)i;
        }
    }
    return MUSIC_TRACKS;
}

// ---------------------------------------------------------------------------------------------
// The samples

// The drums are played at the third octave's C, whose period, 214, reads 16574 samples a second.
#define DRUM_NOTE 25u
#define KICK_LENGTH 4096u
#define SNARE_LENGTH 3072u
#define HAT_LENGTH 1024u

static uint32_t Noise(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static int8_t Clamp8(int32_t v) {
    return (int8_t)(v > 127 ? 127 : v < -127 ? -127 : v);
}

/// Writes @p wave into @p out, @p length samples, and says whether it loops.
static uint32_t MakeWave(MusicWave wave, int8_t *out, bool *loops) {
    *loops = true;
    switch (wave) {
    case MUSIC_WAVE_SQUARE32:
    case MUSIC_WAVE_SQUARE64: {
        const uint32_t n = wave == MUSIC_WAVE_SQUARE32 ? 32u : 64u;
        const int8_t level = wave == MUSIC_WAVE_SQUARE32 ? 88 : 80;
        for (uint32_t i = 0; i < n; ++i) {
            out[i] = i < n / 2u ? level : (int8_t)-level;
        }
        return n;
    }
    case MUSIC_WAVE_PULSE16:
    case MUSIC_WAVE_PULSE32: {
        // A quarter of the cycle high: three times as far above nought as the rest is below, so
        // the wave has no constant part to thump with.
        const uint32_t n = wave == MUSIC_WAVE_PULSE16 ? 16u : 32u;
        for (uint32_t i = 0; i < n; ++i) {
            out[i] = i < n / 4u ? 96 : -32;
        }
        return n;
    }
    case MUSIC_WAVE_THIN32:
        for (uint32_t i = 0; i < 32u; ++i) {
            out[i] = i < 4u ? 112 : -16;
        }
        return 32u;
    case MUSIC_WAVE_TRIANGLE32:
    case MUSIC_WAVE_TRIANGLE64: {
        const uint32_t n = wave == MUSIC_WAVE_TRIANGLE32 ? 32u : 64u;
        for (uint32_t i = 0; i < n; ++i) {
            const int32_t up = (int32_t)(i * 4u * 127u / n);
            out[i] = Clamp8(i < n / 4u        ? up
                            : i < 3u * n / 4u ? 254 - up
                                              : up - 508);
        }
        return n;
    }
    case MUSIC_WAVE_SAW16:
    case MUSIC_WAVE_SAW32: {
        const uint32_t n = wave == MUSIC_WAVE_SAW16 ? 16u : 32u;
        for (uint32_t i = 0; i < n; ++i) {
            out[i] = Clamp8((int32_t)(i * 240u / (n - 1u)) - 120);
        }
        return n;
    }
    case MUSIC_WAVE_SINE64:
        for (uint32_t i = 0; i < 64u; ++i) {
            out[i] = (int8_t)Sine256(i * 4u);
        }
        return 64u;
    case MUSIC_WAVE_ORGAN32:
        // The first, second and fourth harmonics at 4, 2 and 1 parts in 7.
        for (uint32_t i = 0; i < 32u; ++i) {
            const int32_t sum = 4 * Sine256(i * 8u) + 2 * Sine256(i * 16u) + Sine256(i * 32u);
            out[i] = Clamp8(sum * 6 / 49);
        }
        return 32u;
    case MUSIC_WAVE_KICK: {
        // A sine that falls from 160 Hz to 48 Hz, its amplitude dying away.
        *loops = false;
        const uint64_t per_hz = 259137u; // 2^32 over 16574
        uint64_t increment = 160u * per_hz;
        const uint64_t end = 48u * per_hz;
        uint32_t phase = 0;
        uint64_t amplitude = 65536u;
        for (uint32_t i = 0; i < KICK_LENGTH; ++i) {
            out[i] = Clamp8((int32_t)((Sine256(phase >> 24) * (int64_t)amplitude) >> 16));
            phase += (uint32_t)increment;
            increment = end + (((increment - end) * 65180u) >> 16);
            amplitude = (amplitude * 65460u) >> 16;
        }
        return KICK_LENGTH;
    }
    case MUSIC_WAVE_SNARE: {
        // Noise and a 185 Hz tone, the tone dying first.
        *loops = false;
        uint32_t state = 0x534E4152u;
        uint32_t phase = 0;
        uint64_t noise = 52000u;
        uint64_t tone = 40000u;
        for (uint32_t i = 0; i < SNARE_LENGTH; ++i) {
            const int32_t n = (int32_t)(Noise(&state) >> 24) - 128;
            const int64_t sum = n * (int64_t)noise + Sine256(phase >> 24) * (int64_t)tone;
            out[i] = Clamp8((int32_t)(sum >> 16));
            phase += 185u * 259137u;
            noise = (noise * 65400u) >> 16;
            tone = (tone * 65200u) >> 16;
        }
        return SNARE_LENGTH;
    }
    case MUSIC_WAVE_HAT: {
        // Noise less the sample before it: its low end taken off, a short decay.
        *loops = false;
        uint32_t state = 0x48415421u;
        int32_t previous = 0;
        uint64_t amplitude = 65536u;
        for (uint32_t i = 0; i < HAT_LENGTH; ++i) {
            const int32_t n = (int32_t)(Noise(&state) >> 24) - 128;
            out[i] = Clamp8((int32_t)(((int64_t)(n - previous) * (int64_t)amplitude) >> 17));
            previous = n;
            amplitude = (amplitude * 65000u) >> 16;
        }
        return HAT_LENGTH;
    }
    case MUSIC_WAVES:
        break;
    }
    out[0] = 0;
    return 1u;
}

static void Instrument(MusicSong *song, unsigned slot, MusicVoice voice) {
    // A wave already in the bank is shared: the echo plays the lead's.
    for (unsigned i = 1; i <= MUSIC_INSTRUMENTS; ++i) {
        const MusicInstrument *other = &song->instruments[i];
        if (i != slot && other->length > 0u && other->voice.wave == voice.wave) {
            song->instruments[slot] = *other;
            song->instruments[slot].voice = voice;
            return;
        }
    }
    bool loops = true;
    MusicInstrument *instrument = &song->instruments[slot];
    instrument->offset = song->bank_used;
    instrument->length = MakeWave((MusicWave)voice.wave, song->bank + song->bank_used, &loops);
    instrument->loop_start = loops ? 0u : instrument->length;
    instrument->voice = voice;
    song->bank_used += instrument->length;
}

// ---------------------------------------------------------------------------------------------
// The composer

enum {
    PATTERN_HEAD,
    PATTERN_OPEN,
    PATTERN_CLOSED,
    PATTERN_BRIDGE_HEAD,
    PATTERN_BRIDGE_TAIL,
};

// The channels: the bass and the drums (or the lead's echo) to the left, the lead and the chords
// to the right.
enum {
    CH_BASS = 0,
    CH_LEAD = 1,
    CH_CHORD = 2,
    CH_DRUMS = 3,
};

static const uint8_t FORM[MUSIC_ORDER] = {
    PATTERN_HEAD, PATTERN_OPEN,        PATTERN_HEAD,        PATTERN_CLOSED,
    PATTERN_BRIDGE_HEAD, PATTERN_BRIDGE_TAIL, PATTERN_HEAD, PATTERN_CLOSED,
};
static const char SECTIONS[MUSIC_ORDER] = { 'A', 'A', 'A', 'A', 'B', 'B', 'A', 'A' };

/// Each mode's progressions, a degree a bar: A's eight bars as the open A ends them, the
/// bridge's eight ending on the chord that leads home, and that chord. No degree whose triad is
/// diminished in its mode appears.
typedef struct {
    uint8_t a[3][8];
    uint8_t b[2][8];
    uint8_t home;
} Progressions;

static const Progressions PROGRESSIONS[MUSIC_MODES] = {
    [MUSIC_IONIAN] = { { { 0, 5, 3, 4, 0, 5, 3, 4 }, { 0, 4, 5, 3, 0, 4, 3, 4 },
                         { 0, 3, 0, 4, 0, 3, 1, 4 } },
                       { { 3, 4, 2, 5, 3, 4, 1, 4 }, { 5, 3, 0, 4, 5, 3, 1, 4 } },
                       4 },
    [MUSIC_DORIAN] = { { { 0, 3, 0, 3, 0, 3, 6, 3 }, { 0, 2, 3, 0, 0, 2, 6, 3 },
                         { 0, 6, 3, 0, 0, 6, 2, 3 } },
                       { { 6, 2, 3, 4, 6, 2, 4, 3 }, { 4, 3, 6, 2, 4, 3, 6, 3 } },
                       3 },
    [MUSIC_PHRYGIAN] = { { { 0, 1, 0, 1, 0, 1, 6, 1 }, { 0, 6, 5, 1, 0, 6, 5, 1 },
                           { 0, 1, 2, 1, 0, 1, 6, 1 } },
                         { { 5, 6, 5, 2, 5, 6, 2, 1 }, { 3, 5, 2, 6, 3, 5, 6, 1 } },
                         1 },
    [MUSIC_LYDIAN] = { { { 0, 1, 5, 1, 0, 1, 4, 1 }, { 0, 1, 2, 1, 0, 1, 5, 1 },
                         { 0, 6, 1, 0, 0, 6, 5, 1 } },
                       { { 5, 2, 1, 4, 5, 2, 4, 1 }, { 2, 5, 4, 1, 2, 5, 4, 1 } },
                       1 },
    [MUSIC_MIXOLYDIAN] = { { { 0, 6, 3, 0, 0, 6, 3, 6 }, { 0, 3, 6, 0, 0, 3, 4, 6 },
                             { 0, 4, 6, 3, 0, 4, 3, 6 } },
                           { { 3, 0, 4, 5, 3, 0, 4, 6 }, { 5, 4, 3, 0, 5, 4, 3, 6 } },
                           6 },
    [MUSIC_AEOLIAN] = { { { 0, 5, 2, 6, 0, 5, 2, 6 }, { 0, 6, 5, 6, 0, 6, 5, 4 },
                          { 0, 3, 5, 6, 0, 3, 4, 6 } },
                        { { 3, 3, 5, 5, 2, 2, 6, 6 }, { 5, 6, 2, 5, 3, 6, 3, 6 } },
                        6 },
};

#define END 0xFFu
#define CELLS 5u

/// The lead's rhythms, the rows its notes start on in a bar, per groove; the last row of each
/// list ends it.
static const uint8_t LEAD_CELLS[4][CELLS][9] = {
    [MUSIC_GROOVE_CALM] = { { 0, 4, 6, 8, 12, END }, { 0, 2, 4, 8, 12, END },
                            { 0, 6, 8, 12, 14, END }, { 0, 4, 8, 10, 12, END },
                            { 0, 4, 8, 12, END } },
    [MUSIC_GROOVE_BOUNCE] = { { 0, 2, 4, 6, 8, 12, END }, { 0, 3, 6, 8, 10, 12, 14, END },
                              { 0, 4, 6, 8, 12, 14, END }, { 0, 2, 6, 8, 10, 14, END },
                              { 0, 2, 4, 8, 10, 12, END } },
    [MUSIC_GROOVE_SLOW] = { { 0, 8, END }, { 0, 6, 8, 12, END }, { 0, 4, 8, END },
                            { 0, 12, 14, END }, { 0, 8, 12, END } },
    [MUSIC_GROOVE_FLOW] = { { 0, 6, 10, 12, 18, END }, { 0, 4, 6, 12, 16, 18, 22, END },
                            { 0, 6, 12, 18, END }, { 0, 2, 4, 6, 12, 18, END },
                            { 0, 8, 12, 20, END } },
};

/// The bar that answers a phrase, sparser than the cells.
static const uint8_t ANSWER_CELLS[4][6] = {
    [MUSIC_GROOVE_CALM] = { 0, 4, 8, END },
    [MUSIC_GROOVE_BOUNCE] = { 0, 4, 8, 12, END },
    [MUSIC_GROOVE_SLOW] = { 0, END },
    [MUSIC_GROOVE_FLOW] = { 0, 6, 12, END },
};

/// The bass's rows and what it plays on each: the root, the fifth, the octave, or a step into the
/// next bar's root.
enum { BASS_ROOT, BASS_FIFTH, BASS_OCTAVE, BASS_APPROACH };
typedef struct {
    uint8_t row;
    uint8_t tone;
} BassHit;
static const BassHit BASS_HITS[4][8] = {
    [MUSIC_GROOVE_CALM] = { { 0, BASS_ROOT }, { 4, BASS_ROOT }, { 6, BASS_FIFTH },
                            { 8, BASS_ROOT }, { 12, BASS_OCTAVE }, { 14, BASS_APPROACH },
                            { END, 0 } },
    [MUSIC_GROOVE_BOUNCE] = { { 0, BASS_ROOT }, { 4, BASS_OCTAVE }, { 6, BASS_ROOT },
                              { 8, BASS_FIFTH }, { 12, BASS_OCTAVE }, { 14, BASS_APPROACH },
                              { END, 0 } },
    [MUSIC_GROOVE_SLOW] = { { 0, BASS_ROOT }, { 12, BASS_FIFTH }, { END, 0 } },
    [MUSIC_GROOVE_FLOW] = { { 0, BASS_ROOT }, { 4, BASS_FIFTH }, { 8, BASS_OCTAVE },
                            { 12, BASS_ROOT }, { 16, BASS_FIFTH }, { 20, BASS_APPROACH },
                            { END, 0 } },
};

/// The drums: a row each, what it strikes and how loud.
typedef struct {
    uint8_t row;
    uint8_t instrument;
    uint8_t volume;
} DrumHit;
#define K MUSIC_INSTRUMENT_KICK
#define S MUSIC_INSTRUMENT_SNARE
#define H MUSIC_INSTRUMENT_HAT
static const DrumHit DRUM_BARS[3][3][13] = {
    // The bar, the fill that ends a pattern, and the bridge's half time.
    [MUSIC_DRUMS_STRAIGHT] = {
        { { 0, K, 64 }, { 2, H, 24 }, { 4, S, 50 }, { 6, H, 24 }, { 8, K, 64 }, { 10, H, 24 },
          { 12, S, 50 }, { 14, H, 24 }, { END, 0, 0 } },
        { { 0, K, 64 }, { 2, H, 24 }, { 4, S, 50 }, { 6, H, 24 }, { 8, K, 64 }, { 10, H, 24 },
          { 12, S, 44 }, { 14, S, 48 }, { 15, S, 54 }, { END, 0, 0 } },
        { { 0, K, 64 }, { 4, H, 24 }, { 8, S, 50 }, { 12, H, 24 }, { END, 0, 0 } },
    },
    [MUSIC_DRUMS_SHUFFLE] = {
        { { 0, K, 64 }, { 2, H, 26 }, { 4, S, 52 }, { 6, H, 26 }, { 8, K, 64 }, { 10, K, 56 },
          { 12, S, 52 }, { 14, H, 26 }, { END, 0, 0 } },
        { { 0, K, 64 }, { 2, H, 26 }, { 4, S, 52 }, { 6, H, 26 }, { 8, K, 64 }, { 10, S, 40 },
          { 12, S, 44 }, { 13, S, 48 }, { 14, S, 52 }, { 15, S, 56 }, { END, 0, 0 } },
        { { 0, K, 64 }, { 4, H, 26 }, { 8, S, 52 }, { 12, H, 26 }, { 14, K, 56 },
          { END, 0, 0 } },
    },
    [MUSIC_DRUMS_TWELVE] = {
        { { 0, K, 64 }, { 2, H, 20 }, { 4, H, 24 }, { 6, S, 46 }, { 8, H, 20 }, { 10, H, 24 },
          { 12, K, 60 }, { 14, H, 20 }, { 16, H, 24 }, { 18, S, 46 }, { 20, H, 20 },
          { 22, H, 24 }, { END, 0, 0 } },
        { { 0, K, 64 }, { 2, H, 20 }, { 4, H, 24 }, { 6, S, 46 }, { 8, H, 20 }, { 10, H, 24 },
          { 12, K, 60 }, { 14, H, 20 }, { 16, H, 24 }, { 18, S, 40 }, { 20, S, 46 },
          { 22, S, 52 }, { END, 0, 0 } },
        { { 0, K, 64 }, { 6, H, 24 }, { 12, S, 46 }, { 18, H, 24 }, { END, 0, 0 } },
    },
};
#undef K
#undef S
#undef H

/// The lead's next step from the last: the first bar of a motif rises more than it falls, the
/// second the other way round.
static const int8_t RISING[10] = { -1, 1, 1, 2, 0, -1, 1, 3, -2, 1 };
static const int8_t FALLING[10] = { 1, -1, -1, -2, 0, 1, -1, -3, 2, -1 };

/// The lead's range in steps of the mode above its register's tonic.
#define LEAD_LOW 0
#define LEAD_HIGH 11
#define LEAD_ANCHOR_A 4
#define LEAD_ANCHOR_B 7

typedef struct {
    uint8_t cell[2];
    int8_t delta[2][9];
} Motif;

typedef struct {
    MusicSong *song;
    uint32_t random;
    uint32_t rows_per_beat;
    uint32_t rows_per_bar;
    uint8_t groove;
} Composer;

static uint32_t Pick(Composer *c, uint32_t n) {
    return Noise(&c->random) % n;
}

static MusicCell *Cell(MusicSong *song, unsigned pattern, uint32_t row, unsigned channel) {
    return &song->cells[pattern][row][channel];
}

/// The note, 1 to MUSIC_NOTES, of the mode's @p step above @p base's tonic; steps may run past
/// the octave either way.
static uint32_t NoteOf(const MusicSong *song, uint32_t base, int32_t step) {
    const int32_t octave = step >= 0 ? step / 7 : -((-step + 6) / 7);
    const int32_t degree = step - octave * 7;
    const int32_t semitone =
        (int32_t)base + song->colour.tonic + octave * 12 + song->scale[degree];
    if (semitone < 0) {
        return 1u;
    }
    return semitone >= (int32_t)MUSIC_NOTES ? MUSIC_NOTES : (uint32_t)semitone + 1u;
}

/// Semitones from the root of @p degree up to its chord tone @p above degrees higher.
static uint32_t Interval(const MusicSong *song, uint32_t degree, uint32_t above) {
    const uint32_t top = degree + above;
    return song->scale[top % 7u] + (top >= 7u ? 12u : 0u) - song->scale[degree];
}

static uint8_t Arpeggio(const MusicSong *song, uint32_t degree) {
    uint32_t x = 0;
    uint32_t y = 0;
    switch ((MusicVoicing)song->colour.voicing) {
    case MUSIC_VOICING_TRIAD:
        x = Interval(song, degree, 2u);
        y = Interval(song, degree, 4u);
        break;
    case MUSIC_VOICING_SHELL:
        x = Interval(song, degree, 2u);
        y = Interval(song, degree, 6u);
        break;
    case MUSIC_VOICING_SUS2:
        x = Interval(song, degree, 1u);
        y = Interval(song, degree, 4u);
        break;
    case MUSIC_VOICING_OPEN:
        x = Interval(song, degree, 4u);
        y = Interval(song, degree, 1u) + 12u;
        break;
    }
    return (uint8_t)((x << 4) | y);
}

static bool IsChordTone(int32_t step, uint32_t degree) {
    const int32_t d = ((step % 7) + 7) % 7;
    return d == (int32_t)degree || d == (int32_t)((degree + 2u) % 7u) ||
           d == (int32_t)((degree + 4u) % 7u);
}

/// The chord tone of @p degree nearest @p step inside the lead's range, upward on a tie.
static int32_t NearestChordTone(int32_t step, uint32_t degree) {
    for (int32_t distance = 0; distance < 7; ++distance) {
        if (step + distance <= LEAD_HIGH && IsChordTone(step + distance, degree)) {
            return step + distance;
        }
        if (step - distance >= LEAD_LOW && IsChordTone(step - distance, degree)) {
            return step - distance;
        }
    }
    return step;
}

static int32_t Clamp(int32_t v, int32_t low, int32_t high) {
    return v < low ? low : v > high ? high : v;
}

static void PutLead(Composer *c, unsigned pattern, uint32_t row, int32_t step) {
    MusicCell *cell = Cell(c->song, pattern, row, CH_LEAD);
    cell->note = (uint8_t)NoteOf(c->song, c->song->colour.lead_base, step);
    cell->instrument = MUSIC_INSTRUMENT_LEAD;
}

/// The next chord tone of @p degree from @p step toward @p direction, or the other way at the
/// edge of the lead's range.
static int32_t NextChordTone(int32_t step, uint32_t degree, int32_t direction) {
    for (int32_t pass = 0; pass < 2; ++pass, direction = -direction) {
        for (int32_t s = step + direction; s >= LEAD_LOW && s <= LEAD_HIGH; s += direction) {
            if (IsChordTone(s, degree)) {
                return s;
            }
        }
    }
    return step;
}

/// One bar of the lead from a rhythm and its steps: a chord tone on every beat, the rest moving
/// by the motif's steps, all inside the lead's range. A step the snap to a chord tone undid, or
/// a note a third time, moves on instead.
static int32_t LeadBar(Composer *c, unsigned pattern, uint32_t bar, uint32_t degree,
                       const uint8_t *rows, const int8_t *delta, int32_t step, int32_t *again) {
    for (uint32_t k = 0; rows[k] != END; ++k) {
        const bool strong = rows[k] % c->rows_per_beat == 0u;
        int32_t next = Clamp(step + delta[k], LEAD_LOW, LEAD_HIGH);
        if (strong) {
            next = NearestChordTone(next, degree);
        }
        if (next == step && (delta[k] != 0 || *again >= 1)) {
            const int32_t direction = delta[k] < 0 ? -1 : 1;
            next = strong ? NextChordTone(step, degree, direction)
                          : Clamp(step + direction, LEAD_LOW, LEAD_HIGH);
            if (next == step) {
                next = Clamp(step - direction, LEAD_LOW, LEAD_HIGH);
            }
        }
        *again = next == step ? *again + 1 : 0;
        PutLead(c, pattern, bar * c->rows_per_bar + rows[k], next);
        step = next;
    }
    return step;
}

/// The last bar of a phrase: one long note from the bar's first row, on @p goal's nearest chord
/// tone (the tonic itself when @p tonic), released before the next phrase.
static int32_t LongNote(Composer *c, unsigned pattern, uint32_t bar, uint32_t degree,
                        int32_t step, bool tonic) {
    int32_t goal = NearestChordTone(step, degree);
    if (tonic) {
        goal = step >= 4 ? 7 : 0;
    } else if (((goal % 7) + 7) % 7 == 0) {
        // An open ending stays off the tonic: the chord's third instead.
        goal = Clamp(goal + 2, LEAD_LOW, LEAD_HIGH);
        goal = NearestChordTone(goal, degree);
    }
    PutLead(c, pattern, bar * c->rows_per_bar, goal);
    MusicCell *off = Cell(c->song, pattern,
                          bar * c->rows_per_bar + c->rows_per_bar - c->rows_per_beat, CH_LEAD);
    off->note = MUSIC_NOTE_OFF;
    return goal;
}

static void MakeMotif(Composer *c, Motif *m) {
    for (unsigned b = 0; b < 2u; ++b) {
        m->cell[b] = (uint8_t)Pick(c, CELLS);
        for (unsigned k = 0; k < 9u; ++k) {
            m->delta[b][k] = b == 0u ? RISING[Pick(c, 10u)] : FALLING[Pick(c, 10u)];
        }
    }
}

/// A pattern's lead: two bars of the motif, the first again, then an answer; or, for a tail,
/// the motif's two bars, a new bar and the phrase's last note.
static void LeadPattern(Composer *c, unsigned pattern, const Motif *m, const uint8_t *chords,
                        int32_t anchor, int ending) {
    const uint8_t(*cells)[9] = LEAD_CELLS[c->groove];
    int32_t step = anchor;
    int32_t again = 0;
    step = LeadBar(c, pattern, 0, chords[0], cells[m->cell[0]], m->delta[0], step, &again);
    step = LeadBar(c, pattern, 1, chords[1], cells[m->cell[1]], m->delta[1], step, &again);
    if (ending < 0) {
        step = LeadBar(c, pattern, 2, chords[2], cells[m->cell[0]], m->delta[0], step, &again);
        int8_t answer[9];
        for (unsigned k = 0; k < 9u; ++k) {
            answer[k] = FALLING[Pick(c, 10u)];
        }
        LeadBar(c, pattern, 3, chords[3], ANSWER_CELLS[c->groove], answer, step, &again);
        // The phrase breathes before the next one.
        MusicCell *off = Cell(c->song, pattern, 3u * c->rows_per_bar + c->rows_per_bar -
                                                    c->rows_per_beat / 2u, CH_LEAD);
        if (off->note == 0u) {
            off->note = MUSIC_NOTE_OFF;
        }
        return;
    }
    int8_t fresh[9];
    for (unsigned k = 0; k < 9u; ++k) {
        fresh[k] = RISING[Pick(c, 10u)];
    }
    step = LeadBar(c, pattern, 2, chords[2], cells[Pick(c, CELLS)], fresh, step, &again);
    LongNote(c, pattern, 3, chords[3], step, ending == 1);
}

/// Vibrato on the lead's long notes, from their second beat; a glide into some of its steps.
static void LeadEffects(Composer *c, unsigned pattern) {
    MusicSong *song = c->song;
    const uint32_t rows = song->rows;
    int32_t last = -1;
    for (uint32_t row = 0; row < rows; ++row) {
        MusicCell *cell = Cell(song, pattern, row, CH_LEAD);
        if (cell->note == 0u || cell->note == MUSIC_NOTE_OFF) {
            continue;
        }
        uint32_t end = row + 1u;
        while (end < rows && Cell(song, pattern, end, CH_LEAD)->note == 0u) {
            ++end;
        }
        if (song->colour.vibrato != 0u && end - row >= 2u * c->rows_per_beat) {
            for (uint32_t r = row + c->rows_per_beat; r < end; ++r) {
                Cell(song, pattern, r, CH_LEAD)->effect = MUSIC_FX_VIBRATO;
                Cell(song, pattern, r, CH_LEAD)->param = song->colour.vibrato;
            }
        }
        if (song->colour.glide && last >= 0 && row % c->rows_per_beat != 0u &&
            Pick(c, 3u) == 0u) {
            const int32_t apart = (int32_t)cell->note - last;
            if (apart != 0 && apart >= -4 && apart <= 4) {
                cell->effect = MUSIC_FX_PORTAMENTO;
                cell->param = 0x20;
            }
        }
        last = cell->note;
    }
}

/// The echo: the lead three quarters of a beat later on the drums' channel, softer, across the
/// stereo from it.
static void Echo(Composer *c, unsigned pattern) {
    const uint32_t delay = c->rows_per_beat * 3u / 4u;
    for (uint32_t row = delay; row < c->song->rows; ++row) {
        const MusicCell *lead = Cell(c->song, pattern, row - delay, CH_LEAD);
        MusicCell *echo = Cell(c->song, pattern, row, CH_DRUMS);
        echo->note = lead->note;
        echo->instrument = lead->instrument != 0u ? MUSIC_INSTRUMENT_ECHO : 0u;
        if (lead->effect == MUSIC_FX_VIBRATO) {
            echo->effect = lead->effect;
            echo->param = lead->param;
        }
    }
}

static void BassBar(Composer *c, unsigned pattern, uint32_t bar, uint32_t degree,
                    uint32_t next_degree) {
    MusicSong *song = c->song;
    const BassHit *hits = BASS_HITS[c->groove];
    const uint32_t base = song->colour.bass_base;
    for (unsigned k = 0; hits[k].row != END; ++k) {
        int32_t step = (int32_t)degree;
        switch (hits[k].tone) {
        case BASS_FIFTH:
            step = (int32_t)degree + 4;
            break;
        case BASS_OCTAVE:
            step = (int32_t)degree + 7;
            break;
        case BASS_APPROACH:
            // A step below the next root, an octave up when that is below the register; the
            // fifth when the chord stays.
            step = next_degree != degree ? (int32_t)next_degree - 1 : (int32_t)degree + 4;
            step += step < 0 ? 7 : 0;
            break;
        default:
            break;
        }
        MusicCell *cell = Cell(song, pattern, bar * c->rows_per_bar + hits[k].row, CH_BASS);
        cell->note = (uint8_t)NoteOf(song, base, step);
        cell->instrument = MUSIC_INSTRUMENT_BASS;
    }
}

static void ChordBar(Composer *c, unsigned pattern, uint32_t bar, uint32_t degree, bool held) {
    MusicSong *song = c->song;
    const uint8_t arp = Arpeggio(song, degree);
    uint32_t root = NoteOf(song, song->colour.chord_base, (int32_t)degree);
    const uint32_t reach = (arp >> 4) > (arp & 15u) ? (arp >> 4) : (arp & 15u);
    if (root + reach > MUSIC_NOTES && root > 12u) {
        root -= 12u;
    }
    const uint32_t first = bar * c->rows_per_bar;
    uint32_t every = 0;
    uint32_t start = 0;
    uint32_t hold = 2u;
    switch (c->groove) {
    case MUSIC_GROOVE_CALM:
        every = 2u;
        break;
    case MUSIC_GROOVE_BOUNCE:
        every = 4u;
        start = 2u;
        break;
    default:
        every = c->rows_per_bar / 2u;
        hold = every;
        break;
    }
    if (held) {
        // The bridge holds its chords: struck on the bar and halfway, sounding throughout.
        every = c->rows_per_bar / 2u;
        start = 0u;
        hold = every;
    }
    for (uint32_t row = start; row < c->rows_per_bar; row += every) {
        MusicCell *cell = Cell(song, pattern, first + row, CH_CHORD);
        cell->note = (uint8_t)root;
        cell->instrument = MUSIC_INSTRUMENT_CHORD;
        for (uint32_t r = row; r < row + hold && r < c->rows_per_bar; ++r) {
            MusicCell *sounding = Cell(song, pattern, first + r, CH_CHORD);
            sounding->effect = MUSIC_FX_ARPEGGIO;
            sounding->param = arp;
        }
    }
}

static void DrumBar(Composer *c, unsigned pattern, uint32_t bar, unsigned kind) {
    const DrumHit *hits = DRUM_BARS[c->song->colour.drums][kind];
    for (unsigned k = 0; hits[k].row != END; ++k) {
        MusicCell *cell = Cell(c->song, pattern, bar * c->rows_per_bar + hits[k].row, CH_DRUMS);
        cell->note = DRUM_NOTE;
        cell->instrument = hits[k].instrument;
        cell->effect = MUSIC_FX_VOLUME;
        cell->param = hits[k].volume;
    }
}

void MusicCompose(MusicSong *song, MusicTrack track, uint32_t seed) {
    memset(song, 0, sizeof(*song));
    const MusicColour *colour = MusicColourOf(track);
    if (colour == NULL) {
        track = MUSIC_TRACK_MENU;
        colour = &COLOURS[track];
    }
    song->track = track;
    song->seed = seed;
    song->colour = *colour;
    song->rows_per_bar = (uint32_t)colour->rows_per_beat * colour->beats_per_bar;
    song->rows = song->rows_per_bar * MUSIC_PATTERN_BARS;
    for (unsigned d = 0; d < 7u; ++d) {
        song->scale[d] = MusicModeSteps((MusicMode)colour->mode)[d];
    }
    MusicVoice echo = colour->lead;
    echo.volume = (uint8_t)(colour->lead.volume * 2u / 5u);
    Instrument(song, MUSIC_INSTRUMENT_LEAD, colour->lead);
    Instrument(song, MUSIC_INSTRUMENT_CHORD, colour->chord);
    Instrument(song, MUSIC_INSTRUMENT_BASS, colour->bass);
    if (colour->drums == MUSIC_DRUMS_ECHO) {
        Instrument(song, MUSIC_INSTRUMENT_ECHO, echo);
    } else {
        Instrument(song, MUSIC_INSTRUMENT_KICK,
                   (MusicVoice){ MUSIC_WAVE_KICK, 64, 255, 0, 255, 0 });
        Instrument(song, MUSIC_INSTRUMENT_SNARE,
                   (MusicVoice){ MUSIC_WAVE_SNARE, 64, 255, 0, 255, 0 });
        Instrument(song, MUSIC_INSTRUMENT_HAT, (MusicVoice){ MUSIC_WAVE_HAT, 64, 255, 0, 255, 0 });
    }

    Composer c = {
        .song = song,
        .random = (seed != 0u ? seed : 1u) ^ ((uint32_t)track * 0x9E3779B9u),
        .rows_per_beat = colour->rows_per_beat,
        .rows_per_bar = song->rows_per_bar,
        .groove = colour->groove,
    };
    if (c.random == 0u) {
        c.random = 1u;
    }
    // Warm the generator, so seeds that differ in a bit or two part at once.
    for (unsigned i = 0; i < 8u; ++i) {
        (void)Noise(&c.random);
    }

    const Progressions *table = &PROGRESSIONS[colour->mode];
    const uint8_t *a = table->a[Pick(&c, 3u)];
    const uint8_t *b = table->b[Pick(&c, 2u)];
    uint8_t chords[MUSIC_PATTERNS][MUSIC_PATTERN_BARS];
    memcpy(chords[PATTERN_HEAD], a, 4u);
    memcpy(chords[PATTERN_OPEN], a + 4, 4u);
    memcpy(chords[PATTERN_CLOSED], a + 4, 4u);
    chords[PATTERN_CLOSED][2] = table->home;
    chords[PATTERN_CLOSED][3] = 0;
    memcpy(chords[PATTERN_BRIDGE_HEAD], b, 4u);
    memcpy(chords[PATTERN_BRIDGE_TAIL], b + 4, 4u);

    for (unsigned position = 0; position < MUSIC_ORDER; ++position) {
        song->order[position] = FORM[position];
        song->section[position] = SECTIONS[position];
        for (unsigned bar = 0; bar < MUSIC_PATTERN_BARS; ++bar) {
            const uint8_t degree = chords[FORM[position]][bar];
            song->chord[position * MUSIC_PATTERN_BARS + bar] = degree;
            song->arp[position * MUSIC_PATTERN_BARS + bar] = Arpeggio(song, degree);
        }
    }

    Motif motif_a;
    Motif motif_b;
    MakeMotif(&c, &motif_a);
    MakeMotif(&c, &motif_b);
    LeadPattern(&c, PATTERN_HEAD, &motif_a, chords[PATTERN_HEAD], LEAD_ANCHOR_A, -1);
    LeadPattern(&c, PATTERN_OPEN, &motif_a, chords[PATTERN_OPEN], LEAD_ANCHOR_A, 0);
    LeadPattern(&c, PATTERN_CLOSED, &motif_a, chords[PATTERN_CLOSED], LEAD_ANCHOR_A, 1);
    LeadPattern(&c, PATTERN_BRIDGE_HEAD, &motif_b, chords[PATTERN_BRIDGE_HEAD], LEAD_ANCHOR_B,
                -1);
    LeadPattern(&c, PATTERN_BRIDGE_TAIL, &motif_b, chords[PATTERN_BRIDGE_TAIL], LEAD_ANCHOR_B, 0);

    for (unsigned pattern = 0; pattern < MUSIC_PATTERNS; ++pattern) {
        const bool bridge = pattern == PATTERN_BRIDGE_HEAD || pattern == PATTERN_BRIDGE_TAIL;
        LeadEffects(&c, pattern);
        for (unsigned bar = 0; bar < MUSIC_PATTERN_BARS; ++bar) {
            const uint32_t degree = chords[pattern][bar];
            // The bar after a pattern's last is the next pattern's first, whichever plays.
            const uint32_t next = bar + 1u < MUSIC_PATTERN_BARS ? chords[pattern][bar + 1u]
                                  : pattern == PATTERN_BRIDGE_TAIL || pattern == PATTERN_OPEN
                                      ? chords[PATTERN_HEAD][0]
                                  : pattern == PATTERN_BRIDGE_HEAD ? chords[PATTERN_BRIDGE_TAIL][0]
                                  : pattern == PATTERN_HEAD        ? chords[PATTERN_OPEN][0]
                                                                   : chords[PATTERN_BRIDGE_HEAD][0];
            BassBar(&c, pattern, bar, degree, next);
            ChordBar(&c, pattern, bar, degree, bridge && colour->groove != MUSIC_GROOVE_SLOW);
            if (colour->drums != MUSIC_DRUMS_ECHO) {
                // A fill ends each eight bars, where a section or its half closes.
                const bool fill = bar == MUSIC_PATTERN_BARS - 1u && pattern != PATTERN_HEAD &&
                                  pattern != PATTERN_BRIDGE_HEAD;
                const unsigned kind = bridge && !fill ? 2u : fill ? 1u : 0u;
                DrumBar(&c, pattern, bar, kind);
            }
        }
        if (colour->drums == MUSIC_DRUMS_ECHO) {
            Echo(&c, pattern);
        }
    }
}

// ---------------------------------------------------------------------------------------------
// The tracker

static const MusicInstrument *InstrumentOf(const MusicPlayer *p, const MusicChannel *ch) {
    return ch->instrument >= 1u && ch->instrument <= MUSIC_INSTRUMENTS &&
                   p->song.instruments[ch->instrument].length > 0u
               ? &p->song.instruments[ch->instrument]
               : NULL;
}

static void Trigger(MusicPlayer *p, MusicChannel *ch, uint32_t note) {
    ch->note = note;
    ch->period = MusicPeriod(note);
    ch->target_period = ch->period;
    ch->position = 0;
    ch->playing = InstrumentOf(p, ch) != NULL;
    ch->envelope = 0;
    ch->attacking = true;
    ch->released = false;
    ch->vibrato_position = 0;
}

static uint32_t RowTicks(const MusicPlayer *p) {
    const MusicColour *colour = &p->song.colour;
    const uint32_t in_beat = p->row % colour->rows_per_beat;
    const uint32_t speed = colour->speed;
    if (colour->swing == 0u) {
        return speed;
    }
    return in_beat < colour->rows_per_beat / 2u ? speed + colour->swing : speed - colour->swing;
}

static void Envelope(const MusicPlayer *p, MusicChannel *ch) {
    const MusicInstrument *instrument = InstrumentOf(p, ch);
    if (instrument == NULL) {
        return;
    }
    const MusicVoice *v = &instrument->voice;
    if (ch->released) {
        ch->envelope = ch->envelope > v->release ? ch->envelope - v->release : 0;
    } else if (ch->attacking) {
        ch->envelope += v->attack;
        if (ch->envelope >= 255) {
            ch->envelope = 255;
            ch->attacking = false;
        }
    } else if (ch->envelope > v->sustain) {
        ch->envelope =
            ch->envelope - v->decay > (int32_t)v->sustain ? ch->envelope - v->decay : v->sustain;
    }
}

/// What a channel's cell asks on the row's first tick.
static void RowCell(MusicPlayer *p, MusicChannel *ch, const MusicCell *cell) {
    ch->cell = *cell;
    if (cell->instrument != 0u) {
        ch->instrument = cell->instrument;
        const MusicInstrument *instrument = InstrumentOf(p, ch);
        ch->volume = instrument != NULL ? instrument->voice.volume : 0;
    }
    const uint32_t ext = cell->effect == MUSIC_FX_EXTENDED ? cell->param >> 4 : 0u;
    if (cell->note == MUSIC_NOTE_OFF) {
        ch->released = true;
    } else if (cell->note != 0u) {
        if (cell->effect == MUSIC_FX_PORTAMENTO && ch->playing) {
            ch->target_period = MusicPeriod(cell->note);
            ch->note = cell->note;
            if (cell->param != 0u) {
                ch->portamento = cell->param;
            }
            ch->released = false;
        } else if (!(ext == 0xDu && (cell->param & 15u) != 0u)) {
            Trigger(p, ch, cell->note);
        }
    }
    if (cell->effect == MUSIC_FX_VOLUME) {
        ch->volume = cell->param > 64u ? 64 : cell->param;
    }
    if (cell->effect == MUSIC_FX_VIBRATO && cell->param != 0u) {
        const uint8_t speed = cell->param & 0xF0u ? cell->param & 0xF0u : ch->vibrato & 0xF0u;
        const uint8_t depth = cell->param & 0x0Fu ? cell->param & 0x0Fu : ch->vibrato & 0x0Fu;
        ch->vibrato = (uint8_t)(speed | depth);
    }
    if (ext == 0xCu && (cell->param & 15u) == 0u) {
        ch->volume = 0;
    }
}

/// The period a channel sounds at on this tick: its own, moved by an arpeggio or a vibrato.
static uint32_t SoundingPeriod(const MusicPlayer *p, MusicChannel *ch) {
    const MusicCell *cell = &ch->cell;
    if (cell->effect == MUSIC_FX_ARPEGGIO && cell->param != 0u) {
        const uint32_t phase = p->tick % 3u;
        const uint32_t add = phase == 0u ? 0u : phase == 1u ? cell->param >> 4 : cell->param & 15u;
        return add == 0u ? ch->period : MusicPeriod(ch->note + add);
    }
    if (cell->effect == MUSIC_FX_VIBRATO && p->tick > 0u) {
        const uint32_t depth = ch->vibrato & 15u;
        const int32_t delta = (int32_t)((VIBRATO[ch->vibrato_position & 31u] * depth) >> 7);
        const int32_t period =
            (int32_t)ch->period + ((ch->vibrato_position & 32u) != 0u ? -delta : delta);
        ch->vibrato_position = (ch->vibrato_position + (ch->vibrato >> 4)) & 63u;
        return (uint32_t)Clamp(period, PERIOD_MIN, PERIOD_MAX);
    }
    return ch->period;
}

/// What a channel's cell does on the ticks after the first.
static void LaterTick(MusicPlayer *p, MusicChannel *ch) {
    const MusicCell *cell = &ch->cell;
    const uint32_t x = cell->param >> 4;
    const uint32_t y = cell->param & 15u;
    switch (cell->effect) {
    case MUSIC_FX_SLIDE_UP:
        ch->period = ch->period > PERIOD_MIN + cell->param ? ch->period - cell->param : PERIOD_MIN;
        break;
    case MUSIC_FX_SLIDE_DOWN:
        ch->period = ch->period + cell->param < PERIOD_MAX ? ch->period + cell->param : PERIOD_MAX;
        break;
    case MUSIC_FX_PORTAMENTO:
        if (ch->period < ch->target_period) {
            const uint32_t up = ch->period + ch->portamento;
            ch->period = up < ch->target_period ? up : ch->target_period;
        } else if (ch->period > ch->target_period) {
            ch->period = ch->period > ch->target_period + ch->portamento
                             ? ch->period - ch->portamento
                             : ch->target_period;
        }
        break;
    case MUSIC_FX_VOLUME_SLIDE:
        ch->volume = x != 0u ? (ch->volume + (int32_t)x > 64 ? 64 : ch->volume + (int32_t)x)
                             : (ch->volume > (int32_t)y ? ch->volume - (int32_t)y : 0);
        break;
    case MUSIC_FX_EXTENDED:
        if (x == 0xCu && p->tick == y) {
            ch->volume = 0;
        } else if (x == 0xDu && p->tick == y && cell->note != 0u && cell->note != MUSIC_NOTE_OFF) {
            Trigger(p, ch, cell->note);
        } else if (x == 0x9u && y != 0u && p->tick % y == 0u) {
            Trigger(p, ch, ch->note);
        }
        break;
    default:
        break;
    }
}

/// The next tick: a new row on its first, its cells read; then every channel's effects and
/// envelope, and the gain and step it plays at until the tick after.
static void Tick(MusicPlayer *p) {
    if (p->row_ticks == 0u) {
        // The first tick of the track.
        p->row_ticks = RowTicks(p);
    } else if (++p->tick >= p->row_ticks) {
        p->tick = 0;
        if (++p->row >= p->song.rows) {
            p->row = 0;
            p->position = (p->position + 1u) % MUSIC_ORDER;
        }
        p->row_ticks = RowTicks(p);
    }
    const unsigned pattern = p->song.order[p->position];
    for (unsigned c = 0; c < MUSIC_CHANNELS; ++c) {
        MusicChannel *ch = &p->channels[c];
        if (p->tick == 0u) {
            RowCell(p, ch, &p->song.cells[pattern][p->row][c]);
        } else {
            LaterTick(p, ch);
        }
        Envelope(p, ch);
        ch->step = MusicStep(SoundingPeriod(p, ch));
        ch->gain_was = ch->gain;
        ch->gain = ch->playing ? ch->volume * ch->envelope : 0;
    }
    p->tick_left = MusicTickFrames(p->song.colour.bpm, &p->tick_carry);
    p->tick_elapsed = 0;
}

// The frames over which a channel's gain moves from one tick's to the next's, so a step in
// volume does not click.
#define RAMP 32u

static int32_t ChannelSample(const MusicPlayer *p, MusicChannel *ch, uint32_t into_tick) {
    const MusicInstrument *instrument = InstrumentOf(p, ch);
    if (!ch->playing || instrument == NULL) {
        ch->playing = false;
        return 0;
    }
    uint32_t index = ch->position >> 16;
    if (index >= instrument->length) {
        if (instrument->loop_start >= instrument->length) {
            ch->playing = false;
            return 0;
        }
        const uint32_t loop = instrument->length - instrument->loop_start;
        index = instrument->loop_start + (index - instrument->loop_start) % loop;
        ch->position = (index << 16) | (ch->position & 0xFFFFu);
    }
    const int32_t gain = into_tick < RAMP ? ch->gain_was + (ch->gain - ch->gain_was) *
                                                               (int32_t)into_tick / (int32_t)RAMP
                                          : ch->gain;
    ch->position += ch->step;
    return (p->song.bank[instrument->offset + index] * gain) >> 8;
}

static void Begin(MusicPlayer *p, MusicTrack track, uint32_t seed) {
    MusicCompose(&p->song, track, seed);
    memset(p->channels, 0, sizeof(p->channels));
    memset(p->filter, 0, sizeof(p->filter));
    memset(p->led, 0, sizeof(p->led));
    p->state = MUSIC_PLAYING;
    p->position = 0;
    p->row = 0;
    p->tick = 0;
    p->row_ticks = 0;
    p->tick_left = 0;
    p->tick_elapsed = 0;
    p->tick_carry = 0;
    p->track_frames = 0;
    p->next = MUSIC_TRACKS;
}

void MusicStartSeeded(MusicPlayer *player, MusicTrack track, uint32_t seed) {
    player->frames = 0;
    player->clipped = 0;
    Begin(player, track, seed);
}

void MusicStart(MusicPlayer *player, MusicTrack track) {
    const MusicColour *colour = MusicColourOf(track);
    MusicStartSeeded(player, track, colour != NULL ? colour->seed : COLOURS[0].seed);
}

void MusicStartAfterBarrier(MusicPlayer *player, MusicTrack track) {
    MusicStart(player, track);
    if ((unsigned)track < MUSIC_TRACKS) {
        player->next = track;
        player->state = MUSIC_BARRIER;
        player->barrier_left = MUSIC_BARRIER_FRAMES;
    }
}

void MusicSwitch(MusicPlayer *player, MusicTrack track) {
    if (player->state == MUSIC_SILENT) {
        if ((unsigned)track < MUSIC_TRACKS) {
            Begin(player, track, COLOURS[track].seed);
        }
        return;
    }
    player->next = (unsigned)track < MUSIC_TRACKS ? track : MUSIC_TRACKS;
    if (player->state == MUSIC_PLAYING) {
        player->state = MUSIC_FADING;
        player->fade_left = MUSIC_FADE_FRAMES;
    } else if (player->state == MUSIC_BARRIER && player->next == MUSIC_TRACKS) {
        // A stop inside the barrier: already silent, and nothing comes after it.
        player->state = MUSIC_SILENT;
    }
}

static int16_t Clip(MusicPlayer *p, int32_t v) {
    if (v > 32767 || v < -32768) {
        ++p->clipped;
        return v > 0 ? 32767 : -32768;
    }
    return (int16_t)v;
}

void MusicRender(MusicPlayer *p, int16_t *out, size_t frames) {
    for (size_t f = 0; f < frames; ++f) {
        if (p->state == MUSIC_SILENT || p->state == MUSIC_BARRIER) {
            out[2u * f] = 0;
            out[2u * f + 1u] = 0;
            if (p->state == MUSIC_BARRIER && --p->barrier_left == 0u) {
                if ((unsigned)p->next < MUSIC_TRACKS) {
                    Begin(p, p->next, COLOURS[p->next].seed);
                } else {
                    p->state = MUSIC_SILENT;
                }
            }
            ++p->frames;
            continue;
        }
        if (p->tick_left == 0u) {
            Tick(p);
        }
        int32_t values[MUSIC_CHANNELS];
        const uint32_t into = p->tick_elapsed;
        for (unsigned c = 0; c < MUSIC_CHANNELS; ++c) {
            values[c] = ChannelSample(p, &p->channels[c], into);
        }
        int32_t left = 0;
        int32_t right = 0;
        MusicPan(values, &left, &right);
        left = left * 11 / 8;
        right = right * 11 / 8;
        p->filter[0] = MusicFilterStep(p->filter[0], left, MUSIC_FILTER_A500_Q16);
        p->filter[1] = MusicFilterStep(p->filter[1], right, MUSIC_FILTER_A500_Q16);
        int32_t l = p->filter[0] >> 8;
        int32_t r = p->filter[1] >> 8;
        if (p->song.colour.filter == MUSIC_FILTER_LED) {
            p->led[0] = MusicFilterStep(p->led[0], l, MUSIC_FILTER_LED_Q16);
            p->led[1] = MusicFilterStep(p->led[1], p->led[0] >> 8, MUSIC_FILTER_LED_Q16);
            p->led[2] = MusicFilterStep(p->led[2], r, MUSIC_FILTER_LED_Q16);
            p->led[3] = MusicFilterStep(p->led[3], p->led[2] >> 8, MUSIC_FILTER_LED_Q16);
            l = p->led[1] >> 8;
            r = p->led[3] >> 8;
        }
        if (p->state == MUSIC_FADING) {
            l = l * (int32_t)p->fade_left / (int32_t)MUSIC_FADE_FRAMES;
            r = r * (int32_t)p->fade_left / (int32_t)MUSIC_FADE_FRAMES;
        }
        out[2u * f] = Clip(p, l);
        out[2u * f + 1u] = Clip(p, r);
        ++p->tick_elapsed;
        --p->tick_left;
        ++p->frames;
        ++p->track_frames;
        if (p->state == MUSIC_FADING && --p->fade_left == 0u) {
            if ((unsigned)p->next < MUSIC_TRACKS) {
                p->state = MUSIC_BARRIER;
                p->barrier_left = MUSIC_BARRIER_FRAMES;
            } else {
                p->state = MUSIC_SILENT;
            }
        }
    }
    __atomic_add_fetch(&g_music.frames, (uint64_t)frames, __ATOMIC_RELAXED);
}

uint64_t MusicFormFrames(const MusicSong *song) {
    uint64_t frames = 0;
    uint32_t carry = 0;
    const MusicColour *colour = &song->colour;
    for (unsigned position = 0; position < MUSIC_ORDER; ++position) {
        for (uint32_t row = 0; row < song->rows; ++row) {
            const uint32_t in_beat = row % colour->rows_per_beat;
            const uint32_t ticks = colour->swing == 0u ? colour->speed
                                   : in_beat < colour->rows_per_beat / 2u
                                       ? colour->speed + colour->swing
                                       : colour->speed - colour->swing;
            for (uint32_t t = 0; t < ticks; ++t) {
                frames += MusicTickFrames(colour->bpm, &carry);
            }
        }
    }
    return frames;
}

uint64_t MusicHash(uint64_t hash, const int16_t *samples, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        const uint16_t v = (uint16_t)samples[i];
        hash = (hash ^ (v & 0xFFu)) * 0x100000001B3ull;
        hash = (hash ^ (v >> 8)) * 0x100000001B3ull;
    }
    return hash;
}
