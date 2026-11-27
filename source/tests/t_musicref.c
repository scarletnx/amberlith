// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../check.h"
#include "../music.h"
#include "../report.h"
#include "tests.h"

// The demos' music: the arithmetic its FM synthesizer does as the OPL2 does, against answers
// worked out by hand, and what the composer writes for many seeds of each track. No sample of a
// track is rendered here (the music family renders, and only when a mode asks for it): these cost
// the run a few thousand operator readings and the composing of a few hundred passes.

static const char *const SUITE = "reference";

typedef struct {
    uint32_t wrong;
    char first[160];
} Tally;

static void Expect(Tally *tally, bool condition, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
static void Expect(Tally *tally, bool condition, const char *fmt, ...) {
    if (condition || tally->wrong++ != 0u) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    vsnprintf(tally->first, sizeof(tally->first), fmt, args);
    va_end(args);
}

static void Report(const char *name, const Tally *tally) {
    CHECK_TRUE(SUITE, name, CHECK_NONE, tally->wrong == 0u, "%u readings wrong, the first: %s",
               (unsigned)tally->wrong, tally->wrong != 0u ? tally->first : "none");
}

static const double PI = 3.14159265358979323846;

/// The tables, read through a sine at no attenuation and a square at a few: a sine's every reading
/// within 8 of 4095 sin, which the two tables' roundings stay inside; a square halving each octave
/// of attenuation, by hand.
static void CheckTables(void) {
    Tally t = { 0, "" };
    for (uint32_t p = 0; p < 1024u; ++p) {
        const double want = 4095.0 * sin(((double)p + 0.5) * 2.0 * PI / 1024.0);
        const int32_t got = MusicWave(MUSIC_WAVE_SINE, p, 0);
        Expect(&t, fabs((double)got - want) <= 8.0, "the sine at %u reads %d where %.1f",
               (unsigned)p, (int)got, want);
    }
    // 4095 at none; 2^-1/2 of it at half an octave, 2896 (4095 * 0.7071, rounded); halved per
    // octave after, the exponential's entry shifted: 2047 at 256, 1448 at 384, 15 at 2048, and
    // nothing from 13 octaves.
    static const uint32_t ATTENUATION[6][2] = { { 0, 4095 },   { 128, 2896 }, { 256, 2047 },
                                                { 384, 1448 }, { 2048, 15 },  { 3328, 0 } };
    for (unsigned i = 0; i < 6u; ++i) {
        const int32_t got = MusicWave(MUSIC_WAVE_SQUARE, 100, ATTENUATION[i][0]);
        Expect(&t, got == (int32_t)ATTENUATION[i][1],
               "a square at attenuation %u reads %d where %u", (unsigned)ATTENUATION[i][0],
               (int)got, (unsigned)ATTENUATION[i][1]);
    }
    Report("the FM synthesizer's log-sine and exponential tables read a sine within 8 of 4095 sin, "
           "halving each octave",
           &t);
}

/// The eight waveforms, at the points that tell them apart.
static void CheckWaves(void) {
    Tally t = { 0, "" };
    // { wave, phase, reading }: the peaks and the halves each one keeps or silences.
    static const int32_t AT[][3] = {
        { MUSIC_WAVE_SINE, 256, 4095 },      { MUSIC_WAVE_SINE, 768, -4095 },
        { MUSIC_WAVE_HALF_SINE, 256, 4095 }, { MUSIC_WAVE_HALF_SINE, 768, 0 },
        { MUSIC_WAVE_ABS_SINE, 768, 4095 },  { MUSIC_WAVE_QUARTER_SINE, 255, 4095 },
        { MUSIC_WAVE_QUARTER_SINE, 384, 0 }, { MUSIC_WAVE_QUARTER_SINE, 767, 4095 },
        { MUSIC_WAVE_ALT_SINE, 128, 4095 },  { MUSIC_WAVE_ALT_SINE, 384, -4095 },
        { MUSIC_WAVE_ALT_SINE, 640, 0 },     { MUSIC_WAVE_CAMEL_SINE, 384, 4095 },
        { MUSIC_WAVE_CAMEL_SINE, 896, 0 },   { MUSIC_WAVE_SQUARE, 1, 4095 },
        { MUSIC_WAVE_SQUARE, 1000, -4095 },  { MUSIC_WAVE_LOG_SAW, 0, 4095 },
        { MUSIC_WAVE_LOG_SAW, 256, 15 },     { MUSIC_WAVE_LOG_SAW, 512, -4095 },
    };
    for (size_t i = 0; i < sizeof(AT) / sizeof(AT[0]); ++i) {
        const int32_t got = MusicWave((uint32_t)AT[i][0], (uint32_t)AT[i][1], 0);
        // A sine's peak is the table's entry half a step from the top: within 1 of 4095.
        const int32_t apart = got - AT[i][2];
        Expect(&t, apart >= -1 && apart <= 1 && (AT[i][2] != 0 || got == 0),
               "waveform %d at %d reads %d where %d", (int)AT[i][0], (int)AT[i][1], (int)got,
               (int)AT[i][2]);
    }
    Expect(&t, MusicWave(MUSIC_WAVES, 0, 0) == 0, "a waveform past the last reads %d",
           (int)MusicWave(MUSIC_WAVES, 0, 0));
    Report("the chip's eight waveforms hold their peaks and silences", &t);
}

/// A note's phase step: A4's by hand, every note within a cent of equal temperament on A4 = 440,
/// and a pitch between two notes between their steps.
static void CheckPitch(void) {
    Tally t = { 0, "" };
    // 440 * 2^32 / 48000 = 39370533.5; C4, 261.63 Hz, 23409859; A3 half of A4.
    static const uint32_t BY_HAND[3][2] = { { 69, 39370533 }, { 60, 23409859 }, { 57, 19685266 } };
    for (unsigned i = 0; i < 3u; ++i) {
        const uint32_t got = MusicPitchStep(BY_HAND[i][0] << 8);
        Expect(&t, got == BY_HAND[i][1], "note %u steps %u a frame where %u",
               (unsigned)BY_HAND[i][0], (unsigned)got, (unsigned)BY_HAND[i][1]);
    }
    for (uint32_t note = 24; note <= 108u; ++note) {
        const double hz = (double)MusicPitchStep(note << 8) * 48000.0 / 4294967296.0;
        const double want = 440.0 * pow(2.0, ((double)note - 69.0) / 12.0);
        const double cents = 1200.0 * log2(hz / want);
        Expect(&t, fabs(cents) < 1.0, "note %u sounds at %.3f Hz where %.3f, %.2f cents off",
               (unsigned)note, hz, want, cents);
        const uint32_t half = MusicPitchStep((note << 8) + 128u);
        Expect(&t, half > MusicPitchStep(note << 8) && half < MusicPitchStep((note + 1u) << 8),
               "half a semitone above note %u steps %u, outside %u to %u", (unsigned)note,
               (unsigned)half, (unsigned)MusicPitchStep(note << 8),
               (unsigned)MusicPitchStep((note + 1u) << 8));
    }
    Report("a note's phase step: A4 at 440 Hz, every note within a cent of equal temperament", &t);
}

/// The envelope as it plays: a decay at rate r falls 96 dB in 39.28 s over 2^(r - 1), an attack at
/// r rises in 2.826 s over 2^(r - 1), the chip's tables; rate 15 attacks at once, 0 never moves.
static void CheckEnvelope(void) {
    Tally t = { 0, "" };
    for (uint32_t rate = 1; rate <= 15u; ++rate) {
        const double decay = 39.28 / pow(2.0, (double)rate - 1.0) * 48000.0;
        const uint32_t got = MusicEnvelopeFrames(rate, false);
        Expect(&t, fabs((double)got / decay - 1.0) < 0.01,
               "a decay at rate %u lasts %u frames where %.0f", (unsigned)rate, (unsigned)got,
               decay);
        if (rate < 15u) {
            const double attack = 2.826 / pow(2.0, (double)rate - 1.0) * 48000.0;
            const uint32_t rose = MusicEnvelopeFrames(rate, true);
            // The fastest rates' few frames round to whole ones: 3% there.
            Expect(&t, fabs((double)rose / attack - 1.0) < 0.03,
                   "an attack at rate %u lasts %u frames where %.0f", (unsigned)rate,
                   (unsigned)rose, attack);
        }
    }
    Expect(&t,
           MusicEnvelopeFrames(15, true) == 1u && MusicEnvelopeFrames(0, true) == 4000000u &&
               MusicEnvelopeFrames(0, false) == 4000000u,
           "rate 15 attacks in %u frames where 1, rate 0 in %u and decays in %u where never",
           (unsigned)MusicEnvelopeFrames(15, true), (unsigned)MusicEnvelopeFrames(0, true),
           (unsigned)MusicEnvelopeFrames(0, false));
    Report("the envelope's attack and decay keep the OPL's rates, within 1% to 3%", &t);
}

/// Every voice a score names is a patch the synthesizer has, the drums with the notes they play
/// and the rest with none; and the right chip's step is 4.7 cents above the left's, 2^(12 / 3072).
static void CheckPatches(void) {
    Tally t = { 0, "" };
    for (unsigned track = 0; track < MUSIC_TRACKS; ++track) {
        const MusicScore *score = MusicScoreOf((MusicTrack)track);
        for (unsigned v = 0; v < MUSIC_VOICES; ++v) {
            for (unsigned k = 0; k < 2u; ++k) {
                const char *name = score->voice[v][k];
                if (name == NULL) {
                    Expect(&t, k == 1u, "%s's voice %u names no patch", score->name, v);
                    continue;
                }
                const MusicPatch *patch = MusicPatchNamed(name);
                Expect(&t, patch != NULL, "%s's voice %u names %s, no patch", score->name, v,
                       name);
                const bool drum = v <= MUSIC_VOICE_RISER;
                Expect(&t, patch == NULL || (patch->note != 0u) == drum,
                       "%s's voice %u plays %s, whose note is %u, where %s", score->name, v, name,
                       patch != NULL ? (unsigned)patch->note : 0u,
                       drum ? "a drum's" : "none (a melodic patch)");
            }
        }
    }
    Expect(&t, MusicPatchNamed("nothing") == NULL && MusicPatchNamed(NULL) == NULL,
           "a name no patch has gives one");
    for (uint32_t note = 24; note <= 96u; note += 12u) {
        const double ratio = (double)MusicPitchStep((note << 8) + MUSIC_SURROUND) /
                             (double)MusicPitchStep(note << 8);
        const double cents = 1200.0 * log2(ratio);
        Expect(&t, fabs(cents - 4.6875) < 0.2, "the right chip sits %.2f cents above the left "
               "at note %u, where 4.69", cents, (unsigned)note);
    }
    Report("every voice a score names is a patch the synthesizer has, the drums at their notes, "
           "and the right chip 4.7 cents above the left",
           &t);
}

// ---------------------------------------------------------------------------------------------
// The composer

#define SONG_ROWS (MUSIC_FORM_MAX * MUSIC_SECTION_BARS * MUSIC_BAR_ROWS)

/// One first pass, every row of every channel, with each bar's part and chord.
static struct {
    MusicCell cell[SONG_ROWS][MUSIC_CHANNELS];
    uint8_t part[MUSIC_FORM_MAX * MUSIC_SECTION_BARS];
    uint8_t chord[MUSIC_FORM_MAX * MUSIC_SECTION_BARS];
    uint32_t bars;
    uint8_t form[MUSIC_FORM_MAX];
    uint32_t form_count;
} g_song;

static MusicComposer composer;
static MusicCell bar[MUSIC_BAR_ROWS][MUSIC_CHANNELS];

static void Compose(MusicTrack track, uint32_t seed) {
    MusicComposeStart(&composer, track, seed);
    memcpy(g_song.form, composer.form, sizeof(g_song.form));
    g_song.form_count = composer.form_count;
    g_song.bars = MusicComposePassBars(track, seed);
    for (uint32_t b = 0; b < g_song.bars; ++b) {
        g_song.part[b] = (uint8_t)MusicComposePart(&composer);
        g_song.chord[b] = (uint8_t)MusicComposeChord(&composer);
        MusicComposeBar(&composer, bar);
        memcpy(g_song.cell[b * MUSIC_BAR_ROWS], bar, sizeof(bar));
    }
}

static uint64_t SongHash(void) {
    uint64_t hash = MUSIC_HASH_START;
    const uint8_t *bytes = (const uint8_t *)g_song.cell;
    const size_t size = (size_t)g_song.bars * MUSIC_BAR_ROWS * MUSIC_CHANNELS * sizeof(MusicCell);
    for (size_t i = 0; i < size; ++i) {
        hash = (hash ^ bytes[i]) * 0x100000001B3ull;
    }
    return hash;
}

static bool InKey(const MusicScore *score, uint32_t note) {
    const uint8_t *steps = MusicModeSteps((MusicMode)score->mode);
    for (unsigned d = 0; d < 7u; ++d) {
        if ((score->tonic + steps[d]) % 12u == note % 12u) {
            return true;
        }
    }
    return false;
}

/// Whether @p note is a note of the triad on degree @p chord of the score's key.
static bool OnChord(const MusicScore *score, uint32_t chord, uint32_t note) {
    for (uint32_t i = 0; i < 3u; ++i) {
        const uint32_t tone =
            MusicComposeNote(score->tonic, (MusicMode)score->mode, chord + 2u * i) % 12u;
        if (tone == note % 12u) {
            return true;
        }
    }
    return false;
}

static bool Sounds(const MusicCell *cell) {
    return cell->note != 0u && cell->note != MUSIC_NOTE_OFF;
}

#define SEEDS 32u

/// For 32 seeds of each track: every note in the key, the lead's notes in its range and on the
/// bar's chord on the first and third beats, the pads' notes always on it.
static void CheckHarmony(void) {
    Tally t = { 0, "" };
    for (unsigned track = 0; track < MUSIC_TRACKS; ++track) {
        const MusicScore *s = MusicScoreOf((MusicTrack)track);
        for (uint32_t seed = 1; seed <= SEEDS; ++seed) {
            Compose((MusicTrack)track, seed);
            for (uint32_t row = 0; row < g_song.bars * MUSIC_BAR_ROWS; ++row) {
                const uint32_t chord = g_song.chord[row / MUSIC_BAR_ROWS];
                for (uint32_t ch = 0; ch < MUSIC_CHANNELS; ++ch) {
                    const MusicCell *cell = &g_song.cell[row][ch];
                    const MusicRole role = (MusicRole)s->role[ch];
                    if (!Sounds(cell) || role == MUSIC_ROLE_DRUMS || role == MUSIC_ROLE_CRASH) {
                        continue;
                    }
                    Expect(&t, InKey(s, cell->note), "%s seed %u row %u channel %u plays note %u, "
                           "out of the key", s->name, (unsigned)seed, (unsigned)row, (unsigned)ch,
                           cell->note);
                    if (role == MUSIC_ROLE_PAD) {
                        Expect(&t, OnChord(s, chord, cell->note), "%s seed %u row %u: pad note "
                               "%u is not on degree %u's triad", s->name, (unsigned)seed, (unsigned)row,
                               cell->note, (unsigned)(chord + 1u));
                    }
                    if (role == MUSIC_ROLE_LEAD) {
                        Expect(&t,
                               cell->note >= s->range[MUSIC_RANGE_LEAD][0] &&
                                   cell->note <= s->range[MUSIC_RANGE_LEAD][1],
                               "%s seed %u row %u: lead note %u outside %u to %u", s->name,
                               (unsigned)seed, (unsigned)row, cell->note, s->range[MUSIC_RANGE_LEAD][0],
                               s->range[MUSIC_RANGE_LEAD][1]);
                        Expect(&t, row % 8u != 0u || OnChord(s, chord, cell->note),
                               "%s seed %u row %u: the lead's strong beat %u is not on degree "
                               "%u's triad",
                               s->name, (unsigned)seed, (unsigned)row, cell->note,
                               (unsigned)(chord + 1u));
                    }
                }
            }
        }
    }
    Report("every note 32 seeds compose of each track is in its key, the pads on the bar's "
           "chord, the lead in range and on the chord at the first and third beats",
           &t);
}

/// Each seed its own song: 64 seeds of each track give 64 different first passes, a seed twice
/// the same one; a form starts with its intro, holds an A section, and is 8 bars a section; the
/// pass after the first has no intro.
static void CheckForm(void) {
    Tally t = { 0, "" };
    static uint64_t hashes[64];
    for (unsigned track = 0; track < MUSIC_TRACKS; ++track) {
        const MusicScore *s = MusicScoreOf((MusicTrack)track);
        uint32_t same = 0;
        for (uint32_t seed = 0; seed < 64u; ++seed) {
            Compose((MusicTrack)track, seed + 1u);
            hashes[seed] = SongHash();
            for (uint32_t before = 0; before < seed; ++before) {
                same += hashes[before] == hashes[seed] ? 1u : 0u;
            }
            bool a = false;
            for (uint32_t i = 0; i < g_song.form_count; ++i) {
                a = a || g_song.form[i] == MUSIC_PART_A;
            }
            Expect(&t,
                   g_song.form_count > 1u && g_song.form[0] == MUSIC_PART_INTRO && a &&
                       g_song.bars == g_song.form_count * MUSIC_SECTION_BARS,
                   "%s seed %u: %u sections, the first part %u, %s A, %u bars", s->name,
                   (unsigned)(seed + 1u), (unsigned)g_song.form_count, g_song.form[0],
                   a ? "with an" : "no", (unsigned)g_song.bars);
            // The next pass: no intro in its form.
            MusicComposeBar(&composer, bar);
            bool intro = false;
            for (uint32_t i = 0; i < composer.form_count; ++i) {
                intro = intro || composer.form[i] == MUSIC_PART_INTRO;
            }
            Expect(&t, composer.pass == 1u && !intro, "%s seed %u: pass %u after the first, %s",
                   s->name, (unsigned)(seed + 1u), (unsigned)composer.pass, intro ? "with an intro" : "no intro");
        }
        Compose((MusicTrack)track, 1u);
        const uint64_t again = SongHash();
        Expect(&t, same == 0u && again == hashes[0], "%s: %u pairs of 64 seeds composed the same "
               "pass; seed 1 composed %016llx twice and %016llx once", s->name, (unsigned)same,
               (unsigned long long)hashes[0], (unsigned long long)again);
    }
    Report("each seed composes a song of its own, 64 seeds 64 first passes, a seed twice the "
           "same; the intro only on the first pass",
           &t);
}

/// What each channel plays, over 16 seeds: the kit on its channel, at its patches' notes; the
/// echo the lead's notes its rows late and its drop softer; the double the lead's notes drifting
/// sharp, only at the climax; the harmony under the lead, only in the B section and the climax;
/// the cymbal only on the first beat of a bar.
static void CheckRoles(void) {
    Tally t = { 0, "" };
    for (unsigned track = 0; track < MUSIC_TRACKS; ++track) {
        const MusicScore *s = MusicScoreOf((MusicTrack)track);
        int32_t channel[MUSIC_ROLES];
        for (unsigned r = 0; r < MUSIC_ROLES; ++r) {
            channel[r] = -1;
        }
        for (unsigned ch = 0; ch < MUSIC_CHANNELS; ++ch) {
            channel[s->role[ch]] = (int32_t)ch;
        }
        for (uint32_t seed = 1; seed <= 16u; ++seed) {
            Compose((MusicTrack)track, seed);
            const uint32_t rows = g_song.bars * MUSIC_BAR_ROWS;
            for (uint32_t row = 0; row < rows; ++row) {
                const MusicPart part = (MusicPart)g_song.part[row / MUSIC_BAR_ROWS];
                const MusicCell *line = g_song.cell[row];
                const MusicCell *lead = channel[MUSIC_ROLE_LEAD] >= 0
                                            ? &line[channel[MUSIC_ROLE_LEAD]]
                                            : NULL;
                if (channel[MUSIC_ROLE_DRUMS] >= 0) {
                    const MusicCell *d = &line[channel[MUSIC_ROLE_DRUMS]];
                    const MusicPatch *p = d->patch != 0u ? &MUSIC_PATCHES[d->patch - 1u] : NULL;
                    Expect(&t, !Sounds(d) || (p != NULL && p->note == d->note),
                           "%s seed %u row %u: the kit plays note %u on %s", s->name, (unsigned)seed,
                           (unsigned)row,
                           d->note, p != NULL ? p->name : "no patch");
                }
                if (channel[MUSIC_ROLE_ECHO] >= 0 && Sounds(&line[channel[MUSIC_ROLE_ECHO]])) {
                    const MusicCell *e = &line[channel[MUSIC_ROLE_ECHO]];
                    const MusicCell *was = row >= s->echo_rows
                                               ? &g_song.cell[row - s->echo_rows]
                                                             [channel[MUSIC_ROLE_LEAD]]
                                               : NULL;
                    Expect(&t,
                           was != NULL && was->note == e->note &&
                               was->volume == e->volume + s->echo_drop,
                           "%s seed %u row %u: the echo plays %u at %u, the lead %u rows before "
                           "%u at %u",
                           s->name, (unsigned)seed, (unsigned)row, e->note, e->volume, s->echo_rows,
                           was != NULL ? was->note : 0u, was != NULL ? was->volume : 0u);
                }
                if (channel[MUSIC_ROLE_DOUBLE] >= 0) {
                    const MusicCell *d = &line[channel[MUSIC_ROLE_DOUBLE]];
                    Expect(&t,
                           !Sounds(d) || (part == MUSIC_PART_CLIMAX && lead != NULL &&
                                          lead->note == d->note &&
                                          d->effect == MUSIC_FX_SLIDE_UP),
                           "%s seed %u row %u (part %u): the double plays %u, the lead %u",
                           s->name, (unsigned)seed, (unsigned)row, part, d->note, lead != NULL ? lead->note : 0u);
                }
                if (channel[MUSIC_ROLE_HARMONY] >= 0) {
                    const MusicCell *h = &line[channel[MUSIC_ROLE_HARMONY]];
                    Expect(&t,
                           !Sounds(h) ||
                               ((part == MUSIC_PART_B || part == MUSIC_PART_CLIMAX) &&
                                lead != NULL && Sounds(lead) && h->note < lead->note),
                           "%s seed %u row %u (part %u): the harmony plays %u, the lead %u",
                           s->name, (unsigned)seed, (unsigned)row, part, h->note, lead != NULL ? lead->note : 0u);
                }
                if (channel[MUSIC_ROLE_CRASH] >= 0) {
                    const MusicCell *c = &line[channel[MUSIC_ROLE_CRASH]];
                    Expect(&t, !Sounds(c) || row % MUSIC_BAR_ROWS == 0u,
                           "%s seed %u row %u: the cymbal strikes off a bar's first beat",
                           s->name, (unsigned)seed, (unsigned)row);
                }
            }
        }
    }
    Report("each channel plays its part: the kit on one channel, the echo the lead's notes late "
           "and softer, the double and the harmony where the form asks",
           &t);
}

void RunReferenceMusic(void) {
    CheckTables();
    CheckWaves();
    CheckPitch();
    CheckEnvelope();
    CheckPatches();
    CheckHarmony();
    CheckForm();
    CheckRoles();
}
