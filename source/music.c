// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <limits.h>
#include <string.h>

#include "music.h"

// Integers only, from the composer's cells to the last sample: a float rounds as the compiler
// pleases (a fused multiply on one processor, two roundings on another), and the music family holds
// a machine's samples to the host's one for one.

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

#define OP(mult, tl, ar, dr, sl, rr, wave, sustain, vib, am)                                       \
    { mult, tl, ar, dr, sl, rr, wave, sustain, vib, am }

// Built as an AdLib tracker's instruments are: sines, half and absolute sines, no square or saw
// lead; the kick and the bass with both operators at half the note, so the low end sits an octave
// under the key; the snare's, hat's and cymbal's noise out of a modulator fed back hard at a high
// multiple; slow attacks for the strings and pads; vibrato and tremolo on what sustains.
const MusicPatch MUSIC_PATCHES[] = {
    // The drums, each at the note it plays.
    { "kick",
      { OP(0, 0, 12, 7, 5, 8, 0, 1, 0, 0), OP(0, 0, 15, 6, 15, 8, 0, 0, 0, 0) },
      4, 0, 24 },
    { "snare",
      { OP(8, 0, 13, 3, 15, 15, 2, 0, 0, 0), OP(2, 0, 15, 6, 15, 15, 0, 0, 0, 0) },
      6, 0, 31 },
    { "hat",
      { OP(15, 0, 15, 0, 8, 4, 0, 0, 0, 0), OP(0, 0, 15, 6, 15, 5, 0, 1, 0, 0) },
      7, 0, 48 },
    { "crash",
      { OP(15, 0, 15, 0, 12, 10, 2, 1, 0, 0), OP(0, 0, 14, 4, 4, 6, 0, 1, 0, 0) },
      5, 0, 72 },
    { "riser",
      { OP(3, 0, 15, 0, 0, 8, 0, 1, 0, 0), OP(1, 0, 10, 0, 0, 8, 0, 1, 0, 0) },
      7, 0, 36 },
    // Basses.
    { "sub",
      { OP(0, 14, 15, 2, 15, 5, 0, 1, 1, 0), OP(0, 0, 15, 3, 3, 6, 0, 1, 1, 0) },
      3, 0, 0 },
    { "rubber",
      { OP(1, 22, 15, 5, 15, 6, 0, 0, 0, 0), OP(0, 0, 15, 3, 2, 6, 0, 1, 0, 0) },
      4, 0, 0 },
    // Pads.
    { "strings",
      { OP(2, 18, 2, 1, 15, 5, 0, 1, 1, 1), OP(2, 2, 5, 1, 3, 4, 1, 1, 1, 1) },
      4, 0, 0 },
    { "warm",
      { OP(1, 20, 2, 1, 2, 3, 0, 1, 1, 1), OP(1, 0, 2, 2, 2, 3, 0, 1, 0, 0) },
      6, 0, 0 },
    // Leads.
    { "bright",
      { OP(4, 24, 15, 8, 6, 6, 0, 1, 1, 1), OP(2, 0, 15, 5, 9, 6, 1, 1, 1, 1) },
      7, 0, 0 },
    { "horn",
      { OP(1, 22, 13, 3, 4, 6, 0, 1, 1, 0), OP(1, 0, 12, 2, 3, 6, 1, 1, 1, 0) },
      6, 0, 0 },
    { "flute",
      { OP(1, 34, 8, 2, 2, 6, 0, 1, 1, 0), OP(1, 0, 9, 1, 1, 6, 0, 1, 1, 0) },
      2, 0, 0 },
    // Bells and keys for the ostinatos.
    { "bell",
      { OP(6, 4, 2, 6, 6, 2, 0, 0, 1, 1), OP(4, 0, 15, 6, 9, 2, 0, 0, 1, 1) },
      0, 0, 0 },
    { "glass",
      { OP(5, 10, 15, 5, 15, 4, 0, 0, 0, 0), OP(3, 0, 15, 5, 12, 4, 0, 0, 0, 0) },
      1, 0, 0 },
    { "epiano",
      { OP(1, 20, 15, 6, 8, 5, 0, 0, 0, 0), OP(1, 0, 15, 3, 6, 5, 0, 0, 0, 0) },
      3, 0, 0 },
};
const size_t MUSIC_PATCH_COUNT = sizeof(MUSIC_PATCHES) / sizeof(MUSIC_PATCHES[0]);

const MusicPatch *MusicPatchNamed(const char *name) {
    for (size_t i = 0; name != NULL && i < MUSIC_PATCH_COUNT; ++i) {
        if (strcmp(MUSIC_PATCHES[i].name, name) == 0) {
            return &MUSIC_PATCHES[i];
        }
    }
    return NULL;
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

int32_t MusicWave(uint32_t wave, uint32_t phase, uint32_t attenuation) {
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

/// A key going down: the phases start over and the attack starts from where each envelope is,
/// as the chip's do.
static void KeyOn(MusicChannel *ch, int32_t pitch) {
    ch->pitch = pitch;
    ch->target = pitch;
    ch->stepped = INT32_MIN;
    ch->active = true;
    for (unsigned k = 0; k < 2u; ++k) {
        MusicOperatorState *op = &ch->op[k];
        op->stage = STAGE_ATTACK;
        op->phase[0] = 0;
        op->phase[1] = 0;
    }
}

static void KeyOff(MusicChannel *ch) {
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

/// The vibrato: 14 cents either way at the triangle's peak (64), as a share of the phase step.
static uint32_t Vibrato(uint32_t step, int32_t vib) {
    return step + (uint32_t)(int32_t)(((int64_t)step * vib * 133) >> 20);
}

/// The channel's next frame on each chip, left then right.
static void ChannelSample(MusicChannel *ch, int32_t vib, int32_t am, int32_t out[2]) {
    const MusicPatch *patch = ch->patch;
    if (ch->pitch != ch->stepped) {
        for (unsigned side = 0; side < 2u; ++side) {
            const int32_t pitch = ch->pitch + (side != 0u ? MUSIC_SURROUND : 0);
            const uint32_t base = MusicPitchStep(pitch < 0 ? 0u : (uint32_t)pitch);
            for (unsigned k = 0; k < 2u; ++k) {
                ch->op[k].step[side] =
                    (uint32_t)(((uint64_t)base * MULT2[patch->op[k].mult & 15u]) >> 1);
            }
        }
        ch->stepped = ch->pitch;
    }
    int32_t attenuation[2];
    for (unsigned k = 0; k < 2u; ++k) {
        const MusicOperator *set = &patch->op[k];
        Envelope(&ch->op[k], set);
        // The carrier's level is the channel's volume, as a tracker sets it; the modulator's is
        // its total level, or the volume too when the two are added.
        const uint32_t level = k == 1u || patch->additive != 0u
                                   ? 63u - (k == 1u ? ch->volume : ch->mod_volume)
                                   : set->tl;
        attenuation[k] =
            (ch->op[k].attenuation >> 16) + (int32_t)level * 32 + (set->am != 0u ? am : 0);
    }
    MusicOperatorState *mod = &ch->op[0];
    MusicOperatorState *car = &ch->op[1];
    for (unsigned side = 0; side < 2u; ++side) {
        const int32_t feedback =
            patch->feedback != 0u
                ? (mod->out[side][0] + mod->out[side][1]) >> (9u - patch->feedback)
                : 0;
        const int32_t m = MusicWave(patch->op[0].wave,
                                    (mod->phase[side] >> 22) + (uint32_t)feedback,
                                    (uint32_t)attenuation[0]);
        mod->out[side][1] = mod->out[side][0];
        mod->out[side][0] = m;
        mod->phase[side] +=
            patch->op[0].vib != 0u ? Vibrato(mod->step[side], vib) : mod->step[side];
        const uint32_t moved = patch->additive != 0u ? 0u : (uint32_t)m;
        const int32_t c = MusicWave(patch->op[1].wave, (car->phase[side] >> 22) + moved,
                                    (uint32_t)attenuation[1]);
        car->out[side][1] = car->out[side][0];
        car->out[side][0] = c;
        car->phase[side] +=
            patch->op[1].vib != 0u ? Vibrato(car->step[side], vib) : car->step[side];
        out[side] = patch->additive != 0u ? m + c : c;
    }
    if (mod->stage == STAGE_IDLE && car->stage == STAGE_IDLE) {
        ch->active = false;
    }
}

// ---------------------------------------------------------------------------------------------
// The tracker

/// What a cell asks of its channel on the row's first tick: a patch (its volumes with it), a
/// volume, a key let up, a note struck, or a tone slide's goal.
static void Cell(MusicChannel *ch, const MusicCell *cell) {
    ch->effect = cell->effect;
    ch->param = cell->param;
    if (cell->patch != 0u && cell->patch <= MUSIC_PATCH_COUNT) {
        const MusicPatch *patch = &MUSIC_PATCHES[cell->patch - 1u];
        if (patch != ch->patch) {
            ch->stepped = INT32_MIN;
        }
        ch->patch = patch;
        ch->volume = (uint8_t)(63u - patch->op[1].tl);
        ch->mod_volume = (uint8_t)(63u - patch->op[0].tl);
    }
    if (cell->volume != MUSIC_VOLUME_KEEP) {
        ch->volume = cell->volume > 63u ? 63u : cell->volume;
        if (ch->patch != NULL && ch->patch->additive != 0u) {
            ch->mod_volume = ch->volume;
        }
    }
    if (cell->note == MUSIC_NOTE_OFF) {
        KeyOff(ch);
    } else if (cell->note != 0u && ch->patch != NULL) {
        const int32_t pitch = (int32_t)cell->note << 8;
        if (cell->effect == MUSIC_FX_TONE && ch->active) {
            ch->target = pitch;
        } else {
            KeyOn(ch, pitch);
        }
    }
}

static uint8_t Volume(int32_t v) {
    return (uint8_t)(v < 0 ? 0 : v > 63 ? 63 : v);
}

/// What the row's effect does on each tick after its first.
static void Effect(MusicChannel *ch) {
    const int32_t by = (int32_t)ch->param * 8;
    switch (ch->effect) {
    case MUSIC_FX_SLIDE_UP:
        ch->pitch += by;
        ch->target = ch->pitch;
        break;
    case MUSIC_FX_SLIDE_DOWN:
        ch->pitch -= by;
        ch->target = ch->pitch;
        break;
    case MUSIC_FX_TONE:
        if (ch->pitch < ch->target) {
            ch->pitch = ch->pitch + by > ch->target ? ch->target : ch->pitch + by;
        } else if (ch->pitch > ch->target) {
            ch->pitch = ch->pitch - by < ch->target ? ch->target : ch->pitch - by;
        }
        break;
    case MUSIC_FX_VOLUME:
        ch->volume = Volume((int32_t)ch->volume + ch->param);
        if (ch->patch != NULL && ch->patch->additive != 0u) {
            ch->mod_volume = Volume((int32_t)ch->mod_volume + ch->param);
        }
        break;
    default:
        break;
    }
}

/// The frames of the tick that starts now, its fraction carried in @p carry: a row is a sixteenth
/// at the score's tempo, split into its speed's ticks.
static uint32_t TickFrames(const MusicScore *score, uint32_t *carry) {
    const uint64_t tick =
        ((uint64_t)MUSIC_RATE * 60u << 16) / ((uint64_t)score->tempo * 4u * score->speed);
    const uint64_t total = tick + *carry;
    *carry = (uint32_t)(total & 0xFFFFu);
    return (uint32_t)(total >> 16);
}

static void Row(MusicPlayer *p) {
    for (unsigned c = 0; c < MUSIC_CHANNELS; ++c) {
        Cell(&p->channels[c], &p->bar[p->row][c]);
    }
}

/// The tick that starts now: the first one of the song, of a row (and of a bar, which the
/// composer writes first), or one that moves the effects.
static void Tick(MusicPlayer *p) {
    const MusicScore *score = MusicScoreOf(p->track);
    if (!p->started) {
        p->started = true;
        MusicComposeBar(&p->composer, p->bar);
        Row(p);
        return;
    }
    if (++p->tick < score->speed) {
        for (unsigned c = 0; c < MUSIC_CHANNELS; ++c) {
            Effect(&p->channels[c]);
        }
        return;
    }
    p->tick = 0;
    if (++p->row == MUSIC_BAR_ROWS) {
        p->row = 0;
        MusicComposeBar(&p->composer, p->bar);
    }
    Row(p);
}

static void Begin(MusicPlayer *p, MusicTrack track, uint32_t seed) {
    p->track = (unsigned)track < MUSIC_TRACKS ? track : MUSIC_TRACK_MENU;
    p->seed = seed;
    MusicComposeStart(&p->composer, p->track, seed);
    memset(p->channels, 0, sizeof(p->channels));
    memset(p->bar, 0, sizeof(p->bar));
    memset(p->coupling, 0, sizeof(p->coupling));
    p->state = MUSIC_PLAYING;
    p->row = 0;
    p->tick = 0;
    p->tick_left = 0;
    p->tick_carry = 0;
    p->started = false;
    p->lfo = 0;
    p->track_frames = 0;
    p->next = MUSIC_TRACKS;
    p->next_seed = 0;
}

void MusicStart(MusicPlayer *player, MusicTrack track, uint32_t seed) {
    player->frames = 0;
    player->clipped = 0;
    Begin(player, track, seed);
}

void MusicStartAfterBarrier(MusicPlayer *player, MusicTrack track, uint32_t seed) {
    MusicStart(player, track, seed);
    if ((unsigned)track < MUSIC_TRACKS) {
        player->next = track;
        player->next_seed = seed;
        player->state = MUSIC_BARRIER;
        player->barrier_left = MUSIC_BARRIER_FRAMES;
    }
}

void MusicSwitch(MusicPlayer *player, MusicTrack track, uint32_t seed) {
    if (player->state == MUSIC_SILENT) {
        if ((unsigned)track < MUSIC_TRACKS) {
            Begin(player, track, seed);
        }
        return;
    }
    player->next = (unsigned)track < MUSIC_TRACKS ? track : MUSIC_TRACKS;
    player->next_seed = seed;
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
                    Begin(p, p->next, p->next_seed);
                } else {
                    p->state = MUSIC_SILENT;
                }
            }
            ++p->frames;
            continue;
        }
        const MusicScore *score = MusicScoreOf(p->track);
        if (p->tick_left == 0u) {
            Tick(p);
            p->tick_left = TickFrames(score, &p->tick_carry);
        }
        --p->tick_left;
        const uint32_t vib_at = p->lfo % VIB_PERIOD;
        const uint32_t half = VIB_PERIOD / 2u;
        const int32_t vib = vib_at < half ? (int32_t)(vib_at * 128u / half) - 64
                                          : 64 - (int32_t)((vib_at - half) * 128u / half);
        const uint32_t am_at = p->lfo % AM_PERIOD;
        const int32_t am = am_at < AM_PERIOD / 2u
                               ? (int32_t)(am_at * 43u / (AM_PERIOD / 2u))
                               : 43 - (int32_t)((am_at - AM_PERIOD / 2u) * 43u / (AM_PERIOD / 2u));
        ++p->lfo;
        int32_t left = 0;
        int32_t right = 0;
        for (unsigned c = 0; c < MUSIC_CHANNELS; ++c) {
            MusicChannel *ch = &p->channels[c];
            if (!ch->active) {
                continue;
            }
            int32_t v[2];
            ChannelSample(ch, vib, am, v);
            left += v[0];
            right += v[1];
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

uint64_t MusicPassFrames(MusicTrack track, uint32_t seed) {
    const MusicScore *score = MusicScoreOf(track);
    if (score == NULL) {
        return 0;
    }
    const uint64_t ticks =
        (uint64_t)MusicComposePassBars(track, seed) * MUSIC_BAR_ROWS * score->speed;
    uint64_t frames = 0;
    uint32_t carry = 0;
    for (uint64_t t = 0; t < ticks; ++t) {
        frames += TickFrames(score, &carry);
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
