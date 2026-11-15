// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string.h>

#include "music.h"

// Integers only, from the score to the last sample: a float rounds as the compiler pleases (a fused
// multiply on one processor, two roundings on another), and the music family holds a machine's
// samples to the host's one for one.

// ---------------------------------------------------------------------------------------------
// The chip's tables

// round(-log2(sin((i + 0.5) / 256 * pi / 2)) * 256): a quarter of a sine as attenuation, in 256ths
// of an octave. And round(4095 * 2^(-k / 256)): the amplitude of an attenuation's fraction.
static const uint16_t LOGSIN[256] = {
    2137, 1731, 1543, 1419, 1326, 1252, 1190, 1137, 1091, 1050, 1013, 979, 949, 920, 894, 869,
    846, 825, 804, 785, 767, 749, 732, 717, 701, 687, 672, 659, 646, 633, 621, 609,
    598, 587, 576, 566, 556, 546, 536, 527, 518, 509, 501, 492, 484, 476, 468, 461,
    453, 446, 439, 432, 425, 418, 411, 405, 399, 392, 386, 380, 375, 369, 363, 358,
    352, 347, 341, 336, 331, 326, 321, 316, 311, 307, 302, 297, 293, 289, 284, 280,
    276, 271, 267, 263, 259, 255, 251, 248, 244, 240, 236, 233, 229, 226, 222, 219,
    215, 212, 209, 205, 202, 199, 196, 193, 190, 187, 184, 181, 178, 175, 172, 169,
    167, 164, 161, 159, 156, 153, 151, 148, 146, 143, 141, 138, 136, 134, 131, 129,
    127, 125, 122, 120, 118, 116, 114, 112, 110, 108, 106, 104, 102, 100, 98, 96,
    94, 92, 91, 89, 87, 85, 83, 82, 80, 78, 77, 75, 74, 72, 70, 69,
    67, 66, 64, 63, 62, 60, 59, 57, 56, 55, 53, 52, 51, 49, 48, 47,
    46, 45, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30,
    29, 28, 27, 26, 25, 24, 23, 23, 22, 21, 20, 20, 19, 18, 17, 17,
    16, 15, 15, 14, 13, 13, 12, 12, 11, 10, 10, 9, 9, 8, 8, 7,
    7, 7, 6, 6, 5, 5, 5, 4, 4, 4, 3, 3, 3, 2, 2, 2,
    2, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
};
static const uint16_t EXP[256] = {
    4095, 4084, 4073, 4062, 4051, 4040, 4029, 4018, 4007, 3996, 3986, 3975, 3964, 3953, 3943, 3932,
    3921, 3911, 3900, 3890, 3879, 3869, 3858, 3848, 3837, 3827, 3817, 3806, 3796, 3786, 3776, 3765,
    3755, 3745, 3735, 3725, 3715, 3705, 3695, 3685, 3675, 3665, 3655, 3645, 3635, 3625, 3615, 3606,
    3596, 3586, 3577, 3567, 3557, 3548, 3538, 3528, 3519, 3509, 3500, 3490, 3481, 3472, 3462, 3453,
    3443, 3434, 3425, 3416, 3406, 3397, 3388, 3379, 3370, 3361, 3351, 3342, 3333, 3324, 3315, 3306,
    3297, 3289, 3280, 3271, 3262, 3253, 3244, 3236, 3227, 3218, 3209, 3201, 3192, 3183, 3175, 3166,
    3158, 3149, 3141, 3132, 3124, 3115, 3107, 3098, 3090, 3082, 3073, 3065, 3057, 3048, 3040, 3032,
    3024, 3016, 3007, 2999, 2991, 2983, 2975, 2967, 2959, 2951, 2943, 2935, 2927, 2919, 2911, 2903,
    2896, 2888, 2880, 2872, 2864, 2857, 2849, 2841, 2834, 2826, 2818, 2811, 2803, 2795, 2788, 2780,
    2773, 2765, 2758, 2750, 2743, 2736, 2728, 2721, 2713, 2706, 2699, 2691, 2684, 2677, 2670, 2662,
    2655, 2648, 2641, 2634, 2627, 2620, 2612, 2605, 2598, 2591, 2584, 2577, 2570, 2563, 2557, 2550,
    2543, 2536, 2529, 2522, 2515, 2509, 2502, 2495, 2488, 2481, 2475, 2468, 2461, 2455, 2448, 2442,
    2435, 2428, 2422, 2415, 2409, 2402, 2396, 2389, 2383, 2376, 2370, 2363, 2357, 2351, 2344, 2338,
    2332, 2325, 2319, 2313, 2307, 2300, 2294, 2288, 2282, 2276, 2269, 2263, 2257, 2251, 2245, 2239,
    2233, 2227, 2221, 2215, 2209, 2203, 2197, 2191, 2185, 2179, 2173, 2167, 2161, 2156, 2150, 2144,
    2138, 2132, 2127, 2121, 2115, 2109, 2104, 2098, 2092, 2087, 2081, 2075, 2070, 2064, 2059, 2053,
};

// The frames a decay at each rate takes to fall 96 dB are 39.28 s over 2^(rate - 1), as the chip's
// rate table halves; each entry is 4096 << 16 over that many frames at 48 kHz. An attack at each
// rate takes 2.826 s over 2^(rate - 1) to rise the same, exponentially: each entry, in 65536ths,
// is the share of the attenuation left that a frame takes off, 1 - 4095^(-1 / frames), so the
// attack reaches its top in those frames; 15 is at once.
static const uint32_t DECAY[16] = {
    0,      142,    285,    569,    1139,   2278,   4556,    9112,
    18224,  36447,  72895,  145790, 291580, 583159, 1166318, 2332637,
};
static const uint32_t ATTACK[16] = {
    0, 4, 8, 16, 32, 64, 128, 257, 512, 1021, 2025, 3988, 7734, 14555, 25878, 65536,
};

// round(f * 2^32 / 48000) for the eighth octave's notes, C8 (4186.01 Hz) to B8: each octave below
// halves it.
static const uint32_t STEP8[12] = {
    374557749, 396830112, 420426858, 445426740, 471913192, 499974611,
    529704648, 561202526, 594573365, 629928537, 667386037, 707070876,
};

// An operator's multiple, twice over: 0 is a half, and 11, 13 and 15 repeat 10, 12 and 15 as the
// chip's do.
static const uint8_t MULT2[16] = { 1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30 };

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

const uint8_t *MusicModeSteps(MusicMode mode) {
    return MODE_STEPS[(unsigned)mode < MUSIC_MODES ? mode : MUSIC_IONIAN];
}

const char *MusicModeName(MusicMode mode) {
    return (unsigned)mode < MUSIC_MODES ? MODE_NAMES[mode] : "none";
}

// ---------------------------------------------------------------------------------------------
// The instruments

// Each one's two operators: mult, tl, ar, dr, sl, rr, wave, sustain, vib, am.
#define OP(mult, tl, ar, dr, sl, rr, wave, sustain, vib, am)                                       \
    { mult, tl, ar, dr, sl, rr, wave, sustain, vib, am }
const MusicPatch MUSIC_PATCHES[] = {
    // A slapped bass: a bright modulator that dies faster than the note.
    { "slap",
      { OP(1, 24, 15, 5, 5, 7, 0, 1, 0, 0), OP(1, 0, 15, 3, 3, 7, 0, 1, 0, 0) },
      5, 0, 0, 0, 0 },
    // A round, deep bass, little modulation.
    { "deep",
      { OP(1, 40, 11, 2, 4, 5, 0, 1, 0, 0), OP(1, 2, 12, 1, 1, 5, 0, 1, 0, 0) },
      2, 0, 0, 0, 0 },
    // An electric piano: a half-sine modulator, a bell's attack and a soft tail.
    { "epiano",
      { OP(1, 28, 15, 5, 7, 6, 1, 1, 0, 0), OP(1, 6, 15, 3, 5, 6, 0, 1, 0, 0) },
      2, 0, 0, 0, 0 },
    // An organ: the two operators added, the second an octave up, swelling in.
    { "organ",
      { OP(1, 8, 6, 0, 0, 5, 0, 1, 1, 0), OP(2, 14, 6, 0, 0, 5, 1, 1, 1, 0) },
      3, 1, 0, 0, 0 },
    // Brass: a fed-back modulator, a slower attack.
    { "brass",
      { OP(1, 18, 11, 2, 3, 6, 0, 1, 0, 0), OP(1, 0, 12, 1, 1, 6, 0, 1, 0, 0) },
      6, 0, 0, 0, 0 },
    // A square lead: the OPL3's square carrier under a modulator an octave up.
    { "chip",
      { OP(2, 22, 15, 3, 4, 6, 0, 1, 0, 0), OP(1, 2, 15, 2, 2, 6, 6, 1, 0, 0) },
      3, 0, 0, 0, 0 },
    // A flute: a breath of modulation, a soft attack, vibrato.
    { "flute",
      { OP(2, 46, 9, 1, 2, 6, 0, 1, 1, 0), OP(1, 0, 10, 1, 1, 6, 0, 1, 1, 0) },
      0, 0, 0, 0, 0 },
    // A saw lead: the modulator fed back hard.
    { "saw",
      { OP(1, 10, 13, 2, 3, 6, 0, 1, 0, 0), OP(1, 0, 13, 1, 1, 6, 0, 1, 0, 0) },
      7, 0, 0, 0, 0 },
    // A bell: a modulator at 3.5 times the carrier, both dying away.
    { "bell",
      { OP(7, 26, 15, 5, 10, 5, 0, 0, 0, 0), OP(2, 6, 15, 4, 15, 5, 0, 0, 0, 0) },
      0, 0, 0, 0, 0 },
    // A pluck: a short, bright modulator over a half-sine.
    { "pluck",
      { OP(3, 24, 15, 6, 15, 6, 0, 0, 0, 0), OP(1, 4, 15, 4, 15, 6, 1, 0, 0, 0) },
      3, 0, 0, 0, 0 },
    // The drums. A kick: a sine falling from two and a half octaves up to A1.
    { "kick",
      { OP(1, 34, 15, 8, 15, 8, 0, 0, 0, 0), OP(1, 0, 15, 7, 15, 8, 0, 0, 0, 0) },
      0, 0, 7680, 24, 33 },
    // A snare: noise and a falling tone, added.
    { "snare",
      { OP(1, 6, 15, 8, 15, 8, 8, 0, 0, 0), OP(1, 8, 15, 8, 15, 8, 0, 0, 0, 0) },
      0, 1, 1792, 40, 50 },
    // A closed hat and an open one: the chip's metallic noise.
    { "hat",
      { OP(1, 63, 15, 15, 15, 15, 0, 0, 0, 0), OP(8, 10, 15, 10, 15, 10, 9, 0, 0, 0) },
      0, 1, 0, 0, 84 },
    { "hatopen",
      { OP(1, 63, 15, 15, 15, 15, 0, 0, 0, 0), OP(8, 10, 15, 7, 15, 9, 9, 0, 0, 0) },
      0, 1, 0, 0, 84 },
};
#undef OP
const size_t MUSIC_PATCH_COUNT = sizeof(MUSIC_PATCHES) / sizeof(MUSIC_PATCHES[0]);

const MusicPatch *MusicPatchNamed(const char *name) {
    for (size_t i = 0; name != NULL && i < MUSIC_PATCH_COUNT; ++i) {
        if (strcmp(MUSIC_PATCHES[i].name, name) == 0) {
            return &MUSIC_PATCHES[i];
        }
    }
    return NULL;
}

// Each part's own attenuation, in 256ths of an octave (43 a dB), so the mix sits: the chord's three
// voices and the arpeggio under the lead, the hats under the kick.
#define DB(x) ((int32_t)((x) * 43))
// Each channel hard to one side or in the middle, as the OPL3 sets them: the bass, kick and snare
// in the middle, the chord across both, the lead to the right and the rest against it on the left.
static const uint8_t PANS[MUSIC_FM_CHANNELS] = {
    [MUSIC_CH_BASS] = MUSIC_CENTRE,  [MUSIC_CH_CHORD1] = MUSIC_LEFT,
    [MUSIC_CH_CHORD2] = MUSIC_RIGHT, [MUSIC_CH_CHORD3] = MUSIC_LEFT,
    [MUSIC_CH_LEAD] = MUSIC_RIGHT,   [MUSIC_CH_COUNTER] = MUSIC_LEFT,
    [MUSIC_CH_ARP] = MUSIC_LEFT,     [MUSIC_CH_KICK] = MUSIC_CENTRE,
    [MUSIC_CH_SNARE] = MUSIC_CENTRE, [MUSIC_CH_HATS] = MUSIC_LEFT,
};
static const int32_t LEVELS[MUSIC_FM_CHANNELS] = {
    [MUSIC_CH_BASS] = DB(2),   [MUSIC_CH_CHORD1] = DB(4), [MUSIC_CH_CHORD2] = DB(4),
    [MUSIC_CH_CHORD3] = DB(4), [MUSIC_CH_LEAD] = DB(0),   [MUSIC_CH_COUNTER] = DB(3),
    [MUSIC_CH_ARP] = DB(4),    [MUSIC_CH_KICK] = DB(0),   [MUSIC_CH_SNARE] = DB(0),
    [MUSIC_CH_HATS] = DB(2),
};

uint32_t MusicPanOf(uint32_t channel) {
    return channel < MUSIC_FM_CHANNELS ? PANS[channel] : 0u;
}

// ---------------------------------------------------------------------------------------------
// The chip's arithmetic

uint32_t MusicPitchStep(uint32_t pitch) {
    const uint32_t note = pitch >> 8;
    const uint32_t fraction = pitch & 255u;
    const uint32_t semitone = note % 12u;
    const uint32_t octave = note / 12u;
    const uint32_t low = STEP8[semitone];
    const uint32_t high = semitone == 11u ? STEP8[0] * 2u : STEP8[semitone + 1u];
    const uint32_t step = low + (uint32_t)(((uint64_t)(high - low) * fraction) >> 8);
    // MIDI's ninth octave starts at C8, the table's; each one below halves it.
    return octave >= 9u ? step : step >> (9u - octave);
}

static uint32_t LogSin(uint32_t phase) {
    const uint32_t quarter = phase & 255u;
    return LOGSIN[(phase & 256u) != 0u ? 255u - quarter : quarter];
}

int32_t MusicWave(uint32_t wave, uint32_t phase, uint32_t attenuation, uint32_t noise) {
    phase &= 1023u;
    uint32_t log = 0;
    bool negative = false;
    switch (wave) {
    case MUSIC_WAVE_SINE:
        log = LogSin(phase);
        negative = (phase & 512u) != 0u;
        break;
    case MUSIC_WAVE_HALF_SINE:
        if ((phase & 512u) != 0u) {
            return 0;
        }
        log = LogSin(phase);
        break;
    case MUSIC_WAVE_ABS_SINE:
        log = LogSin(phase);
        break;
    case MUSIC_WAVE_QUARTER_SINE:
        if ((phase & 256u) != 0u) {
            return 0;
        }
        log = LogSin(phase);
        break;
    case MUSIC_WAVE_ALT_SINE:
        if ((phase & 512u) != 0u) {
            return 0;
        }
        log = LogSin(phase << 1);
        negative = (phase & 256u) != 0u;
        break;
    case MUSIC_WAVE_CAMEL_SINE:
        if ((phase & 512u) != 0u) {
            return 0;
        }
        log = LogSin(phase << 1);
        break;
    case MUSIC_WAVE_SQUARE:
        negative = (phase & 512u) != 0u;
        break;
    case MUSIC_WAVE_LOG_SAW:
        log = (phase & 511u) << 3;
        negative = (phase & 512u) != 0u;
        break;
    case MUSIC_WAVE_NOISE:
        negative = (noise & 1u) != 0u;
        break;
    case MUSIC_WAVE_METAL:
        negative = ((noise ^ (phase >> 9)) & 1u) != 0u;
        break;
    default:
        return 0;
    }
    const uint32_t total = log + attenuation;
    if (total >= 13u * 256u) {
        return 0;
    }
    const int32_t value = (int32_t)(EXP[total & 255u] >> (total >> 8));
    return negative ? -value : value;
}

uint32_t MusicDecayStep(uint32_t rate) {
    return DECAY[rate & 15u];
}

uint32_t MusicAttackShare(uint32_t rate) {
    return ATTACK[rate & 15u];
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
// The tracks

const MusicScore *MusicScoreOf(MusicTrack track) {
    return (unsigned)track < MUSIC_TRACKS && (size_t)track < MUSIC_SCORE_COUNT
               ? &MUSIC_SCORES[track]
               : NULL;
}

const char *MusicTrackName(MusicTrack track) {
    const MusicScore *score = MusicScoreOf(track);
    return score != NULL ? score->name : "none";
}

MusicTrack MusicTrackNamed(const char *name) {
    for (unsigned i = 0; name != NULL && i < MUSIC_TRACKS; ++i) {
        if (strcmp(MusicTrackName((MusicTrack)i), name) == 0) {
            return (MusicTrack)i;
        }
    }
    return MUSIC_TRACKS;
}

static const char *Grid(MusicLane lane, const char *name, bool fill) {
    char wanted[48];
    size_t length = strlen(name);
    if (length + 5u > sizeof(wanted)) {
        return NULL;
    }
    memcpy(wanted, name, length);
    memcpy(wanted + length, fill ? "fill" : "", fill ? 5u : 1u);
    for (size_t i = 0; i < MUSIC_SCORE_PATTERN_COUNT; ++i) {
        const MusicScorePattern *pattern = &MUSIC_SCORE_PATTERNS[i];
        if (pattern->lane == lane && strcmp(pattern->name, wanted) == 0) {
            return pattern->grid;
        }
    }
    return NULL;
}

// ---------------------------------------------------------------------------------------------
// A channel

enum {
    STAGE_IDLE,
    STAGE_ATTACK,
    STAGE_DECAY,
    STAGE_SUSTAIN,
    STAGE_RELEASE,
};

#define ATTENUATION_MAX ((int32_t)(4095 << 16))

static void KeyOn(MusicChannel *ch, const MusicPatch *patch, int32_t pitch, int32_t level,
                  bool glide, uint32_t glide_frames, uint32_t gate) {
    if (patch == NULL) {
        return;
    }
    ch->gate = gate;
    ch->level = level;
    if (glide && ch->active && ch->key && ch->patch == patch && glide_frames > 0u) {
        // A glide keeps the note sounding and moves its pitch.
        ch->target = pitch;
        ch->glide = (pitch - ch->pitch) / (int32_t)glide_frames;
        if (ch->glide == 0) {
            ch->pitch = pitch;
        }
        return;
    }
    const bool fresh = !ch->active || ch->patch != patch;
    ch->patch = patch;
    ch->pitch = pitch;
    ch->target = pitch;
    ch->glide = 0;
    ch->drop = patch->drop;
    ch->drop_left = patch->drop_rate;
    ch->key = true;
    ch->active = true;
    ch->age = 0;
    ch->stepped_pitch = -1;
    for (unsigned k = 0; k < 2u; ++k) {
        MusicOperatorState *op = &ch->op[k];
        if (fresh) {
            op->phase = 0;
            op->out[0] = 0;
            op->out[1] = 0;
            op->attenuation = ATTENUATION_MAX;
        }
        // An attack starts from where the operator is, as the chip's does.
        op->stage = STAGE_ATTACK;
    }
}

static void KeyOff(MusicChannel *ch) {
    ch->key = false;
    ch->gate = 0;
    for (unsigned k = 0; k < 2u; ++k) {
        if (ch->op[k].stage != STAGE_IDLE) {
            ch->op[k].stage = STAGE_RELEASE;
        }
    }
}

static void Envelope(MusicOperatorState *op, const MusicOperator *set) {
    switch (op->stage) {
    case STAGE_ATTACK: {
        const int64_t take = ((int64_t)op->attenuation * ATTACK[set->ar]) >> 16;
        op->attenuation -= (int32_t)take + (set->ar != 0u ? 1 : 0);
        if (op->attenuation < (1 << 16) || set->ar == 15u) {
            op->attenuation = 0;
            op->stage = STAGE_DECAY;
        }
        break;
    }
    case STAGE_DECAY: {
        const int32_t floor = set->sl >= 15u ? ATTENUATION_MAX : (int32_t)(set->sl * 128u) << 16;
        op->attenuation += (int32_t)DECAY[set->dr];
        if (op->attenuation >= floor) {
            op->attenuation = floor;
            op->stage = set->sustain != 0u ? STAGE_SUSTAIN : STAGE_RELEASE;
        }
        break;
    }
    case STAGE_RELEASE:
        op->attenuation += (int32_t)DECAY[set->rr];
        if (op->attenuation >= ATTENUATION_MAX) {
            op->attenuation = ATTENUATION_MAX;
            op->stage = STAGE_IDLE;
        }
        break;
    default:
        break;
    }
}

uint32_t MusicEnvelopeFrames(uint32_t rate, bool attack) {
    MusicOperator set = { 1, 0, 15, 0, 15, 0, 0, 0, 0, 0 };
    MusicOperatorState op = { 0 };
    if (attack) {
        set.ar = (uint8_t)(rate & 15u);
        op.attenuation = ATTENUATION_MAX;
        op.stage = STAGE_ATTACK;
    } else {
        set.dr = (uint8_t)(rate & 15u);
        op.stage = STAGE_DECAY;
    }
    // A limit past the slowest rate's 39 s, for rate nought, which never moves.
    uint32_t frames = 0;
    while (op.stage == (attack ? STAGE_ATTACK : STAGE_DECAY) && frames < 4000000u) {
        Envelope(&op, &set);
        ++frames;
    }
    return frames;
}

// The vibrato a patch asks for, 14 cents either way, and the one a lead's long note grows after
// half a second, to 16 cents over the next half; and the tremolo, 1 dB.
#define VIB_DEPTH 36
#define LATE_DEPTH 40
#define LATE_START 24000u
#define LATE_RAMP 24000u

static int32_t ChannelSample(MusicChannel *ch, bool late, int32_t vib, int32_t am, uint32_t noise) {
    const MusicPatch *patch = ch->patch;
    if (ch->glide != 0) {
        ch->pitch += ch->glide;
        if ((ch->glide > 0 && ch->pitch >= ch->target) ||
            (ch->glide < 0 && ch->pitch <= ch->target)) {
            ch->pitch = ch->target;
            ch->glide = 0;
        }
    }
    if (ch->drop > 0 && --ch->drop_left == 0u) {
        ch->drop -= (ch->drop >> 6) + 1;
        ch->drop_left = patch->drop_rate;
        if (ch->drop < 0) {
            ch->drop = 0;
        }
    }
    int32_t pitch = ch->pitch + ch->drop;
    if (patch->op[1].vib != 0u) {
        pitch += vib * VIB_DEPTH / 64;
    }
    if (late && ch->age > LATE_START) {
        const uint32_t into = ch->age - LATE_START;
        const int32_t depth =
            into >= LATE_RAMP ? LATE_DEPTH : (int32_t)((uint32_t)LATE_DEPTH * into / LATE_RAMP);
        pitch += vib * depth / 64;
    }
    if (pitch != ch->stepped_pitch) {
        const uint32_t base = MusicPitchStep(pitch < 0 ? 0u : (uint32_t)pitch);
        for (unsigned k = 0; k < 2u; ++k) {
            ch->op[k].step = (uint32_t)(((uint64_t)base * MULT2[patch->op[k].mult & 15u]) >> 1);
        }
        ch->stepped_pitch = pitch;
    }
    int32_t attenuation[2];
    for (unsigned k = 0; k < 2u; ++k) {
        const MusicOperator *set = &patch->op[k];
        Envelope(&ch->op[k], set);
        attenuation[k] = (ch->op[k].attenuation >> 16) + set->tl * 32 + (set->am != 0u ? am : 0);
    }
    // The level of the part reaches what is heard: the carrier, or both when they are added.
    attenuation[1] += ch->level;
    if (patch->additive != 0u) {
        attenuation[0] += ch->level;
    }
    MusicOperatorState *mod = &ch->op[0];
    MusicOperatorState *car = &ch->op[1];
    const int32_t feedback =
        patch->feedback != 0u ? (mod->out[0] + mod->out[1]) >> (9u - patch->feedback) : 0;
    const int32_t m = MusicWave(patch->op[0].wave, (mod->phase >> 22) + (uint32_t)feedback,
                                (uint32_t)attenuation[0], noise);
    mod->out[1] = mod->out[0];
    mod->out[0] = m;
    mod->phase += mod->step;
    const uint32_t moved = patch->additive != 0u ? 0u : (uint32_t)m;
    const int32_t c =
        MusicWave(patch->op[1].wave, (car->phase >> 22) + moved, (uint32_t)attenuation[1], noise);
    car->out[1] = car->out[0];
    car->out[0] = c;
    car->phase += car->step;
    ++ch->age;
    if (ch->gate != 0u && --ch->gate == 0u) {
        KeyOff(ch);
    }
    if (mod->stage == STAGE_IDLE && car->stage == STAGE_IDLE) {
        ch->active = false;
    }
    return patch->additive != 0u ? m + c : c;
}

// ---------------------------------------------------------------------------------------------
// The score

static const MusicScoreSection *Section(const MusicPlayer *p, uint32_t position) {
    const MusicScore *score = p->song.score;
    return &score->sections[score->form[position % score->form_count]];
}

static uint32_t NextPosition(const MusicScore *score, uint32_t position) {
    return position + 1u < score->form_count ? position + 1u : score->form_count > 1u ? 1u : 0u;
}

static void EnterSection(MusicPlayer *p) {
    const MusicScoreSection *s = Section(p, p->position);
    p->grid[0] = Grid(MUSIC_LANE_BASS, s->parts.bass, false);
    p->grid[1] = Grid(MUSIC_LANE_DRUMS, s->parts.drums, false);
    p->grid[2] = s->fill ? Grid(MUSIC_LANE_DRUMS, s->parts.drums, true) : p->grid[1];
    p->grid[3] = Grid(MUSIC_LANE_HATS, s->parts.drums, false);
    p->grid[4] = Grid(MUSIC_LANE_COMP, s->parts.comp, false);
    p->grid[5] = Grid(MUSIC_LANE_ARP, s->parts.arp, false);
    p->note_index[0] = 0;
    p->note_index[1] = 0;
    p->note_left[0] = 0;
    p->note_left[1] = 0;
}

/// The frames step @p step of a bar lasts, the swing moving frames from each pair's second step to
/// its first, the fraction carried in @p carry.
static uint32_t StepFrames(const MusicScore *score, uint32_t step, uint32_t *carry) {
    const uint32_t beat = score->steps / 4u;
    const uint64_t base = ((uint64_t)MUSIC_RATE * 60u << 16) / ((uint64_t)score->tempo * beat);
    const uint64_t swung = (step & 1u) == 0u ? base * (64u + score->swing) / 64u
                                             : base * (64u - score->swing) / 64u;
    const uint64_t total = swung + *carry;
    *carry = (uint32_t)(total & 0xFFFFu);
    return (uint32_t)(total >> 16);
}

static uint32_t BaseStepFrames(const MusicScore *score) {
    return (uint32_t)((uint64_t)MUSIC_RATE * 60u / ((uint64_t)score->tempo * (score->steps / 4u)));
}

/// The chord of bar @p bar of the form's entry @p position: its root's pitch class and whether it
/// is minor.
static MusicScoreChord ChordAt(const MusicPlayer *p, uint32_t position, uint32_t bar) {
    const MusicScoreSection *s = Section(p, position);
    if (bar >= s->bars) {
        return ChordAt(p, NextPosition(p->song.score, position), 0);
    }
    return s->chords[bar];
}

/// The chord's three voices in close position between G3 and E5, the inversion nearest the chord
/// before; the six notes an arpeggio climbs from the root near C5; and the bass's root from C2.
static void Voice(MusicPlayer *p, MusicScoreChord chord) {
    const int32_t tones[3] = { (int32_t)chord.root, (int32_t)chord.root + (chord.minor ? 3 : 4),
                               (int32_t)chord.root + 7 };
    int32_t best[3] = { 0, 0, 0 };
    int32_t best_distance = -1;
    for (unsigned inversion = 0; inversion < 3u; ++inversion) {
        int32_t v[3];
        for (unsigned i = 0; i < 3u; ++i) {
            const int32_t pc = tones[(inversion + i) % 3u] % 12;
            int32_t note = 55 + ((pc - 55 % 12 + 12) % 12);
            while (i > 0u && note <= v[i - 1u]) {
                note += 12;
            }
            v[i] = note;
        }
        int32_t distance = 0;
        for (unsigned i = 0; i < 3u; ++i) {
            const int32_t was = p->voicing[i] != 0 ? p->voicing[i] : 60 + 4 * (int32_t)i;
            distance += v[i] > was ? v[i] - was : was - v[i];
        }
        if (best_distance < 0 || distance < best_distance) {
            best_distance = distance;
            memcpy(best, v, sizeof(best));
        }
    }
    memcpy(p->voicing, best, sizeof(best));
    int32_t root = 67 + (((int32_t)chord.root - 67 % 12 + 12) % 12);
    const int32_t steps[6] = { 0, chord.minor ? 3 : 4, 7, 12, chord.minor ? 15 : 16, 19 };
    for (unsigned i = 0; i < 6u; ++i) {
        p->arpeggio[i] = root + steps[i];
    }
    p->root = 36 + (int32_t)chord.root;
}

/// How many steps a grid's mark at @p step lasts: itself and the holds after it.
static uint32_t Held(const char *grid, uint32_t step, uint32_t steps) {
    uint32_t length = 1;
    while (step + length < steps && grid[step + length] == '-') {
        ++length;
    }
    return length;
}

/// The frames a note of @p steps steps sounds before its key comes up: a quarter step short.
static uint32_t Gate(const MusicScore *score, uint32_t steps) {
    const uint32_t step = BaseStepFrames(score);
    return steps * step - step / 4u;
}

static void Melody(MusicPlayer *p, unsigned part) {
    const MusicScoreSection *s = Section(p, p->position);
    const MusicScoreNote *notes = part == 0u ? s->lead : s->counter;
    const uint32_t count = part == 0u ? s->lead_count : s->counter_count;
    MusicChannel *ch = &p->channels[part == 0u ? MUSIC_CH_LEAD : MUSIC_CH_COUNTER];
    if (p->note_left[part] == 0u && p->note_index[part] < count) {
        const MusicScoreNote *n = &notes[p->note_index[part]++];
        p->note_left[part] = n->steps;
        if (n->note == 0u) {
            KeyOff(ch);
        } else {
            const MusicScore *score = p->song.score;
            const uint32_t glide = n->glide != 0u ? BaseStepFrames(score) : 0u;
            KeyOn(ch, p->song.patch[part == 0u ? MUSIC_VOICE_LEAD : MUSIC_VOICE_COUNTER],
                  (int32_t)n->note << 8, LEVELS[part == 0u ? MUSIC_CH_LEAD : MUSIC_CH_COUNTER],
                  n->glide != 0u, glide, Gate(score, n->steps));
        }
    }
    if (p->note_left[part] > 0u) {
        --p->note_left[part];
    }
}

/// What every part does on the step that starts now.
static void Sequence(MusicPlayer *p) {
    const MusicScore *score = p->song.score;
    const MusicScoreSection *s = Section(p, p->position);
    const uint32_t steps = score->steps;
    const uint32_t step = p->step;
    if (step == 0u) {
        Voice(p, s->chords[p->bar]);
    }
    Melody(p, 0);
    Melody(p, 1);

    const char *bass = p->grid[0];
    if (bass != NULL) {
        const char mark = bass[step];
        int32_t note = -1;
        bool glide = false;
        switch (mark) {
        case 'R':
            note = p->root;
            break;
        case '3':
            note = p->root + (s->chords[p->bar].minor ? 3 : 4);
            break;
        case '5':
            note = p->root + 7;
            break;
        case '7':
            note = p->root + 10;
            break;
        case '8':
            note = p->root + 12;
            break;
        case 'A':
            note = 36 + (int32_t)ChordAt(p, p->position, p->bar + 1u).root - 1;
            glide = true;
            break;
        case '.':
            KeyOff(&p->channels[MUSIC_CH_BASS]);
            break;
        default:
            break;
        }
        if (note >= 0) {
            KeyOn(&p->channels[MUSIC_CH_BASS], p->song.patch[MUSIC_VOICE_BASS], note << 8,
                  LEVELS[MUSIC_CH_BASS], glide, BaseStepFrames(score) / 2u,
                  Gate(score, Held(bass, step, steps)));
        }
    }

    const bool last = p->bar + 1u == s->bars;
    const char *drums = last ? p->grid[2] : p->grid[1];
    if (drums != NULL) {
        const char mark = drums[step];
        if (mark == 'K') {
            const MusicPatch *kick = MusicPatchNamed("kick");
            KeyOn(&p->channels[MUSIC_CH_KICK], kick, kick->note << 8, LEVELS[MUSIC_CH_KICK], false,
                  0, Gate(score, 2));
        } else if (mark == 'S' || mark == 's') {
            const MusicPatch *snare = MusicPatchNamed("snare");
            KeyOn(&p->channels[MUSIC_CH_SNARE], snare, snare->note << 8,
                  LEVELS[MUSIC_CH_SNARE] + (mark == 's' ? DB(7) : 0), false, 0, Gate(score, 2));
        }
    }
    const char *hats = p->grid[3];
    if (hats != NULL && hats[step] != '.') {
        const char mark = hats[step];
        const MusicPatch *hat = MusicPatchNamed(mark == 'o' ? "hatopen" : "hat");
        KeyOn(&p->channels[MUSIC_CH_HATS], hat, hat->note << 8,
              LEVELS[MUSIC_CH_HATS] + (mark == 'h' ? DB(3) : 0), false, 0,
              Gate(score, mark == 'o' ? 3 : 1));
    }

    const char *comp = p->grid[4];
    if (comp != NULL) {
        if (comp[step] == 'X') {
            const uint32_t gate = Gate(score, Held(comp, step, steps));
            for (unsigned i = 0; i < 3u; ++i) {
                KeyOn(&p->channels[MUSIC_CH_CHORD1 + i], p->song.patch[MUSIC_VOICE_CHORDS],
                      p->voicing[i] << 8, LEVELS[MUSIC_CH_CHORD1 + i], false, 0, gate);
            }
        } else if (comp[step] == '.') {
            for (unsigned i = 0; i < 3u; ++i) {
                KeyOff(&p->channels[MUSIC_CH_CHORD1 + i]);
            }
        }
    }
    const char *arp = p->grid[5];
    if (arp != NULL && arp[step] >= '1' && arp[step] <= '6') {
        KeyOn(&p->channels[MUSIC_CH_ARP], p->song.patch[MUSIC_VOICE_ARP],
              p->arpeggio[arp[step] - '1'] << 8, LEVELS[MUSIC_CH_ARP], false, 0, Gate(score, 2));
    }
}

/// Moves to the next step, the next bar and the next section as each ends.
static void Advance(MusicPlayer *p) {
    const MusicScore *score = p->song.score;
    if (++p->step < score->steps) {
        return;
    }
    p->step = 0;
    if (++p->bar < Section(p, p->position)->bars) {
        return;
    }
    p->bar = 0;
    p->position = NextPosition(score, p->position);
    EnterSection(p);
}

static void Begin(MusicPlayer *p, MusicTrack track) {
    memset(&p->song, 0, sizeof(p->song));
    p->song.track = (unsigned)track < MUSIC_TRACKS ? track : MUSIC_TRACK_MENU;
    p->song.score = MusicScoreOf(p->song.track);
    for (unsigned i = 0; i < MUSIC_SCORE_VOICES; ++i) {
        p->song.patch[i] = MusicPatchNamed(p->song.score->voice[i]);
    }
    memset(p->channels, 0, sizeof(p->channels));
    memset(p->voicing, 0, sizeof(p->voicing));
    memset(p->coupling, 0, sizeof(p->coupling));
    p->state = MUSIC_PLAYING;
    p->position = 0;
    p->bar = 0;
    p->step = 0;
    p->step_left = 0;
    p->step_carry = 0;
    p->started = false;
    p->lfo = 0;
    p->noise = 0x1F35A7BDu;
    p->track_frames = 0;
    p->next = MUSIC_TRACKS;
    EnterSection(p);
}

void MusicStart(MusicPlayer *player, MusicTrack track) {
    player->frames = 0;
    player->clipped = 0;
    Begin(player, track);
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
            Begin(player, track);
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

// The tremolo's and the vibrato's triangles: 3.7 Hz and 6.07 Hz, as the chip's.
#define AM_PERIOD 12972u
#define VIB_PERIOD 7908u

/// The card's coupling: a high pass near 6 Hz, so no constant part of a half-sine thumps. Its state
/// keeps 8 bits below the sample and rounds, so the arithmetic leaves no constant of its own.
static int32_t Couple(int32_t *state, int32_t x) {
    const int64_t kept = ((int64_t)state[1] * 65483 + 32768) >> 16;
    const int32_t y = ((x - state[0]) << 8) + (int32_t)kept;
    state[0] = x;
    state[1] = y;
    return (y + 128) >> 8;
}

void MusicRender(MusicPlayer *p, int16_t *out, size_t frames) {
    for (size_t f = 0; f < frames; ++f) {
        if (p->state == MUSIC_SILENT || p->state == MUSIC_BARRIER) {
            out[2u * f] = 0;
            out[2u * f + 1u] = 0;
            if (p->state == MUSIC_BARRIER && --p->barrier_left == 0u) {
                if ((unsigned)p->next < MUSIC_TRACKS) {
                    Begin(p, p->next);
                } else {
                    p->state = MUSIC_SILENT;
                }
            }
            ++p->frames;
            continue;
        }
        const MusicScore *score = p->song.score;
        if (p->step_left == 0u) {
            if (p->started) {
                Advance(p);
            }
            p->started = true;
            Sequence(p);
            p->step_left = StepFrames(score, p->step, &p->step_carry);
        }
        --p->step_left;
        const uint32_t vib_at = p->lfo % VIB_PERIOD;
        const uint32_t half = VIB_PERIOD / 2u;
        const int32_t vib = vib_at < half ? (int32_t)(vib_at * 128u / half) - 64
                                          : 64 - (int32_t)((vib_at - half) * 128u / half);
        const uint32_t am_at = p->lfo % AM_PERIOD;
        const int32_t am = am_at < AM_PERIOD / 2u
                               ? (int32_t)(am_at * 43u / (AM_PERIOD / 2u))
                               : 43 - (int32_t)((am_at - AM_PERIOD / 2u) * 43u / (AM_PERIOD / 2u));
        ++p->lfo;
        uint32_t n = p->noise;
        n ^= n << 13;
        n ^= n >> 17;
        n ^= n << 5;
        p->noise = n;
        int32_t left = 0;
        int32_t right = 0;
        for (unsigned c = 0; c < MUSIC_FM_CHANNELS; ++c) {
            MusicChannel *ch = &p->channels[c];
            if (!ch->active) {
                continue;
            }
            const int32_t v =
                ChannelSample(ch, c == MUSIC_CH_LEAD || c == MUSIC_CH_COUNTER, vib, am, n >> 31);
            if ((PANS[c] & MUSIC_LEFT) != 0u) {
                left += v;
            }
            if ((PANS[c] & MUSIC_RIGHT) != 0u) {
                right += v;
            }
        }
        int32_t l = Couple(&p->coupling[0], left * (int32_t)score->gain / 32);
        int32_t r = Couple(&p->coupling[2], right * (int32_t)score->gain / 32);
        if (p->state == MUSIC_FADING) {
            l = l * (int32_t)p->fade_left / (int32_t)MUSIC_FADE_FRAMES;
            r = r * (int32_t)p->fade_left / (int32_t)MUSIC_FADE_FRAMES;
        }
        out[2u * f] = Clip(p, l);
        out[2u * f + 1u] = Clip(p, r);
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
    const MusicScore *score = song->score;
    uint64_t frames = 0;
    uint32_t carry = 0;
    for (uint32_t position = 0; position < score->form_count; ++position) {
        const MusicScoreSection *s = &score->sections[score->form[position]];
        for (uint32_t bar = 0; bar < s->bars; ++bar) {
            for (uint32_t step = 0; step < score->steps; ++step) {
                frames += StepFrames(score, step, &carry);
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
