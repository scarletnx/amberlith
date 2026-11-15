// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// What each track is made of (source/musicscore.txt, made into musicscore.c by
/// tools/music-score.py): its key, tempo and sound, which part each of the nine channels plays,
/// and the material a seed builds a song from: forms, chord progressions, drum, bass, pad and
/// arpeggio patterns and the lead's rhythms. The composer (musiccompose.c) picks and joins them.

#define MUSIC_CHANNELS 9u
#define MUSIC_BAR_ROWS 16u
#define MUSIC_SECTION_BARS 8u
#define MUSIC_FORM_MAX 8u

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

/// What a channel plays. The drums are one channel for the whole kit, as an AdLib tracker's:
/// kick, snare and hat take turns on it. Echo, double and harmony follow the lead: rows late
/// and softer, the same notes drifting sharp, and a third below.
typedef enum {
    MUSIC_ROLE_DRUMS,
    MUSIC_ROLE_BASS,
    MUSIC_ROLE_PAD,
    MUSIC_ROLE_LEAD,
    MUSIC_ROLE_ECHO,
    MUSIC_ROLE_DOUBLE,
    MUSIC_ROLE_HARMONY,
    MUSIC_ROLE_ARP,
    MUSIC_ROLE_CRASH,
    MUSIC_ROLES,
} MusicRole;

/// The sections a form joins: intro, A, B, breakdown and climax (I, A, B, R and C in the score).
typedef enum {
    MUSIC_PART_INTRO,
    MUSIC_PART_A,
    MUSIC_PART_B,
    MUSIC_PART_BREAK,
    MUSIC_PART_CLIMAX,
    MUSIC_PARTS,
} MusicPart;

/// The patches a track names, up to two for each (a seed picks one).
typedef enum {
    MUSIC_VOICE_KICK,
    MUSIC_VOICE_SNARE,
    MUSIC_VOICE_HAT,
    MUSIC_VOICE_CRASH,
    MUSIC_VOICE_RISER,
    MUSIC_VOICE_BASS,
    MUSIC_VOICE_PAD,
    MUSIC_VOICE_LEAD,
    MUSIC_VOICE_ARP,
    MUSIC_VOICES,
} MusicVoice;

/// The ranges a track's parts keep to, as the notes keyed (a patch's multiple moves what sounds).
typedef enum {
    MUSIC_RANGE_BASS,
    MUSIC_RANGE_PAD,
    MUSIC_RANGE_LEAD,
    MUSIC_RANGE_ARP,
    MUSIC_RANGES,
} MusicRange;

/// The material a seed picks from: forms (part letters), progressions (a scale degree 1 to 7
/// for each of four bars) for the home sections, the B section and the breakdown, and patterns
/// of 16 rows (the lead's rhythms 32).
typedef enum {
    MUSIC_POOL_FORM,
    MUSIC_POOL_HOME,
    MUSIC_POOL_LIFT,
    MUSIC_POOL_DROP,
    MUSIC_POOL_GROOVE,
    MUSIC_POOL_FILL,
    MUSIC_POOL_INTRO,
    MUSIC_POOL_BREAKBEAT,
    MUSIC_POOL_BASS,
    MUSIC_POOL_BREAKBASS,
    MUSIC_POOL_PAD,
    MUSIC_POOL_CLIMAX,
    MUSIC_POOL_ARP,
    MUSIC_POOL_RHYTHM,
    MUSIC_POOLS,
} MusicPool;

typedef struct {
    const char *const *entry;
    uint8_t count;
} MusicScorePool;

typedef struct {
    const char *name;
    const char *demo;
    uint8_t tonic;
    uint8_t mode;
    /// Quarter notes a minute (a row is a sixteenth), and ticks a row, when the effects move.
    uint16_t tempo;
    uint8_t speed;
    /// The mix's gain in 32nds.
    uint8_t gain;
    uint8_t role[MUSIC_CHANNELS];
    const char *voice[MUSIC_VOICES][2];
    uint8_t range[MUSIC_RANGES][2];
    /// The echo's rows late and its volume below the lead's; the pads' tone slide in 32nds of a
    /// semitone a tick, nought to strike each chord again.
    uint8_t echo_rows;
    uint8_t echo_drop;
    uint8_t glide;
    MusicScorePool pool[MUSIC_POOLS];
} MusicScore;

extern const MusicScore MUSIC_SCORES[];
extern const size_t MUSIC_SCORE_COUNT;

#ifdef __cplusplus
}
#endif
