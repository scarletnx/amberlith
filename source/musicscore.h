// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// The demos' scores as tables (source/musicscore.c, which tools/music-score.py makes from
/// source/musicscore.txt, the music as written).

/// A note of a melody: its MIDI number (0 a rest), its length in steps, and whether it glides in
/// from the note before.
typedef struct {
    uint8_t note;
    uint8_t steps;
    uint8_t glide;
} MusicScoreNote;

/// A bar's chord: its root as a pitch class, 0 for C, and whether it is minor.
typedef struct {
    uint8_t root;
    uint8_t minor;
} MusicScoreChord;

typedef enum {
    MUSIC_LANE_BASS,
    MUSIC_LANE_DRUMS,
    MUSIC_LANE_HATS,
    MUSIC_LANE_COMP,
    MUSIC_LANE_ARP,
} MusicLane;

/// A bar of one lane, a character a step (source/musicscore.txt says what each means).
typedef struct {
    MusicLane lane;
    const char *name;
    const char *grid;
} MusicScorePattern;

/// The patterns a section's bass, drums (and hats), chords and arpeggio play, by name.
typedef struct {
    const char *bass;
    const char *drums;
    const char *comp;
    const char *arp;
} MusicScoreParts;

typedef struct {
    const char *name;
    uint32_t bars;
    const MusicScoreChord *chords;
    const MusicScoreNote *lead;
    uint32_t lead_count;
    const MusicScoreNote *counter;
    uint32_t counter_count;
    MusicScoreParts parts;
    /// The drums' fill on the section's last bar.
    bool fill;
} MusicScoreSection;

/// The instruments of a track's parts, by music.c's patch names.
enum {
    MUSIC_VOICE_BASS,
    MUSIC_VOICE_CHORDS,
    MUSIC_VOICE_LEAD,
    MUSIC_VOICE_COUNTER,
    MUSIC_VOICE_ARP,
    MUSIC_SCORE_VOICES,
};

typedef struct {
    const char *name;
    const char *demo;
    const char *key;
    uint32_t tonic;
    uint32_t mode;
    /// Beats a minute (a beat is four steps in 4/4, six in 12/8), steps a bar, how late the second
    /// step of each pair comes in 64ths of a step, and the mix's gain in 32nds.
    uint32_t tempo;
    uint32_t steps;
    uint32_t swing;
    uint32_t gain;
    const char *voice[MUSIC_SCORE_VOICES];
    /// The sections in the order they play; the loop comes back to the second.
    const uint8_t *form;
    uint32_t form_count;
    const MusicScoreSection *sections;
    uint32_t section_count;
} MusicScore;

extern const MusicScorePattern MUSIC_SCORE_PATTERNS[];
extern const size_t MUSIC_SCORE_PATTERN_COUNT;
extern const MusicScore MUSIC_SCORES[];
extern const size_t MUSIC_SCORE_COUNT;

#ifdef __cplusplus
}
#endif
