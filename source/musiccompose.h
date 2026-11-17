// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "musicscore.h"

#ifdef __cplusplus
extern "C" {
#endif

/// The composer: from a track's score and a seed, a song a bar at a time, written as a tracker's
/// rows of cells, one cell a channel. The seed picks the form, the progressions, the patterns,
/// the patches where the score offers two, and composes the lead's melodies; the same track and
/// seed always give the same song, and a pass that ends starts the next one from a seed of its
/// own, with no intro, so the music never plays the same pass twice.

typedef enum {
    MUSIC_TRACK_MENU,
    MUSIC_TRACK_WIMSY,
    MUSIC_TRACK_FRACTAL,
    MUSIC_TRACK_FLUID,
    MUSIC_TRACKS,
} MusicTrack;

/// A cell's note (a MIDI note, nought for none, MUSIC_NOTE_OFF to let the key up), its patch
/// (one past an index of MUSIC_PATCHES, nought to keep the channel's), its volume (0 to 63,
/// MUSIC_VOLUME_KEEP to keep the channel's) and an effect that moves every tick of the row but
/// its first: a slide up or down or a tone slide toward the note (@p param 32nds of a semitone a
/// tick), or a volume slide (@p param a tick, either way).
#define MUSIC_NOTE_OFF 255u
#define MUSIC_VOLUME_KEEP 255u

enum {
    MUSIC_FX_NONE,
    MUSIC_FX_SLIDE_UP,
    MUSIC_FX_SLIDE_DOWN,
    MUSIC_FX_TONE,
    MUSIC_FX_VOLUME,
};

typedef struct {
    uint8_t note;
    uint8_t patch;
    uint8_t volume;
    uint8_t effect;
    int8_t param;
} MusicCell;

/// The rows the lead's line holds: a section's, and the echo's reach back into the one before.
#define MUSIC_LEAD_ROWS (MUSIC_SECTION_BARS * MUSIC_BAR_ROWS)
#define MUSIC_ECHO_MAX 8u

typedef struct {
    MusicTrack track;
    /// The pass's seed and its number, its form and where the song is in it.
    uint32_t seed;
    uint32_t pass;
    uint8_t form[MUSIC_FORM_MAX];
    uint8_t form_count;
    uint8_t position;
    uint8_t bar;
    /// What the pass picked: the patch for each voice (one past its index), the progression for
    /// the home sections, the B section and the breakdown, the lead's rhythms for A and B.
    uint8_t patch[MUSIC_VOICES];
    uint8_t progression[3];
    uint8_t rhythm[2];
    /// What the section picked: its chords (a scale degree, 0 to 6, a bar), its patterns, and the
    /// lead's line (a note a row, nought held or silent, MUSIC_NOTE_OFF a rest) with the last
    /// rows of the section before for the echo.
    uint8_t chord[MUSIC_SECTION_BARS];
    uint8_t groove;
    uint8_t fill;
    uint8_t bass;
    uint8_t pad;
    uint8_t arp;
    uint8_t lead[MUSIC_LEAD_ROWS];
    uint8_t tail[MUSIC_ECHO_MAX];
    /// The pads' notes, and the next note a cascade plays.
    uint8_t voicing[3];
    uint8_t cascade;
} MusicComposer;

/// Starts the first pass @p seed composes of @p track, its intro first.
void MusicComposeStart(MusicComposer *c, MusicTrack track, uint32_t seed);
/// Writes the next bar's rows, then moves to the bar after it.
void MusicComposeBar(MusicComposer *c, MusicCell bar[MUSIC_BAR_ROWS][MUSIC_CHANNELS]);
/// The bars of the first pass @p seed composes of @p track, its intro included.
uint32_t MusicComposePassBars(MusicTrack track, uint32_t seed);
/// The part and the chord (a scale degree 0 to 6) of the bar the composer writes next.
MusicPart MusicComposePart(const MusicComposer *c);
uint32_t MusicComposeChord(const MusicComposer *c);
/// The MIDI note of scale step @p step (0 the tonic of octave -1, 7 a step an octave) in a key.
uint32_t MusicComposeNote(uint32_t tonic, MusicMode mode, uint32_t step);

#ifdef __cplusplus
}
#endif
