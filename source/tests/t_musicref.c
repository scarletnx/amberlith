// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
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

// The demos' music: the arithmetic its FM synthesizer does as the OPL3 does, and what its four
// scores hold, against answers worked out by hand. No sample of a track is rendered here (the music
// family renders, and only when a mode asks for it): these cost the run a few thousand operator
// readings and a walk over four scores.

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
        const int32_t got = MusicWave(MUSIC_WAVE_SINE, p, 0, 0);
        Expect(&t, fabs((double)got - want) <= 8.0, "the sine at %u reads %d where %.1f",
               (unsigned)p, (int)got, want);
    }
    // 4095 at none; 2^-1/2 of it at half an octave, 2896 (4095 * 0.7071, rounded); halved per
    // octave after, the exponential's entry shifted: 2047 at 256, 1448 at 384, 15 at 2048, and
    // nothing from 13 octaves.
    static const uint32_t ATTENUATION[6][2] = { { 0, 4095 },   { 128, 2896 }, { 256, 2047 },
                                                { 384, 1448 }, { 2048, 15 },  { 3328, 0 } };
    for (unsigned i = 0; i < 6u; ++i) {
        const int32_t got = MusicWave(MUSIC_WAVE_SQUARE, 100, ATTENUATION[i][0], 0);
        Expect(&t, got == (int32_t)ATTENUATION[i][1],
               "a square at attenuation %u reads %d where %u", (unsigned)ATTENUATION[i][0],
               (int)got, (unsigned)ATTENUATION[i][1]);
    }
    Report("the FM synthesizer's log-sine and exponential tables read a sine within 8 of 4095 sin, "
           "halving each octave",
           &t);
}

/// The eight waveforms and the drums' two noises, at the points that tell them apart.
static void CheckWaves(void) {
    Tally t = { 0, "" };
    // { wave, phase, noise, reading }: the peaks and the halves each one keeps or silences.
    static const int32_t AT[][4] = {
        { MUSIC_WAVE_SINE, 256, 0, 4095 },      { MUSIC_WAVE_SINE, 768, 0, -4095 },
        { MUSIC_WAVE_HALF_SINE, 256, 0, 4095 }, { MUSIC_WAVE_HALF_SINE, 768, 0, 0 },
        { MUSIC_WAVE_ABS_SINE, 768, 0, 4095 },  { MUSIC_WAVE_QUARTER_SINE, 255, 0, 4095 },
        { MUSIC_WAVE_QUARTER_SINE, 384, 0, 0 }, { MUSIC_WAVE_QUARTER_SINE, 767, 0, 4095 },
        { MUSIC_WAVE_ALT_SINE, 128, 0, 4095 },  { MUSIC_WAVE_ALT_SINE, 384, 0, -4095 },
        { MUSIC_WAVE_ALT_SINE, 640, 0, 0 },     { MUSIC_WAVE_CAMEL_SINE, 384, 0, 4095 },
        { MUSIC_WAVE_CAMEL_SINE, 896, 0, 0 },   { MUSIC_WAVE_SQUARE, 1, 0, 4095 },
        { MUSIC_WAVE_SQUARE, 1000, 0, -4095 },  { MUSIC_WAVE_LOG_SAW, 0, 0, 4095 },
        { MUSIC_WAVE_LOG_SAW, 256, 0, 15 },     { MUSIC_WAVE_LOG_SAW, 512, 0, -4095 },
        { MUSIC_WAVE_NOISE, 300, 0, 4095 },     { MUSIC_WAVE_NOISE, 300, 1, -4095 },
        { MUSIC_WAVE_METAL, 100, 0, 4095 },     { MUSIC_WAVE_METAL, 600, 0, -4095 },
        { MUSIC_WAVE_METAL, 600, 1, 4095 },
    };
    for (size_t i = 0; i < sizeof(AT) / sizeof(AT[0]); ++i) {
        const int32_t got =
            MusicWave((uint32_t)AT[i][0], (uint32_t)AT[i][1], 0, (uint32_t)AT[i][2]);
        // A sine's peak is the table's entry half a step from the top: within 1 of 4095.
        const int32_t apart = got - AT[i][3];
        Expect(&t, apart >= -1 && apart <= 1 && (AT[i][3] != 0 || got == 0),
               "waveform %d at %d, noise %d, reads %d where %d", (int)AT[i][0], (int)AT[i][1],
               (int)AT[i][2], (int)got, (int)AT[i][3]);
    }
    Expect(&t, MusicWave(MUSIC_WAVES, 0, 0, 0) == 0, "a waveform past the last reads %d",
           (int)MusicWave(MUSIC_WAVES, 0, 0, 0));
    Report("the OPL3's eight waveforms and the drums' noises hold their peaks and silences", &t);
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

/// Each channel's side, by hand: the bass, kick and snare in the middle, the chord across both, the
/// lead to the right and the rest on the left; and every patch a score names is one the synthesizer
/// has.
static void CheckMix(void) {
    Tally t = { 0, "" };
    static const uint32_t SIDES[MUSIC_FM_CHANNELS] = {
        MUSIC_CENTRE, MUSIC_LEFT, MUSIC_RIGHT,  MUSIC_LEFT,   MUSIC_RIGHT,
        MUSIC_LEFT,   MUSIC_LEFT, MUSIC_CENTRE, MUSIC_CENTRE, MUSIC_LEFT,
    };
    for (uint32_t c = 0; c < MUSIC_FM_CHANNELS; ++c) {
        Expect(&t, MusicPanOf(c) == SIDES[c], "channel %u sits at %u where %u", (unsigned)c,
               (unsigned)MusicPanOf(c), (unsigned)SIDES[c]);
    }
    for (unsigned track = 0; track < MUSIC_TRACKS; ++track) {
        const MusicScore *score = MusicScoreOf((MusicTrack)track);
        for (unsigned v = 0; v < MUSIC_SCORE_VOICES; ++v) {
            Expect(&t, MusicPatchNamed(score->voice[v]) != NULL, "%s's part %u names %s, no patch",
                   score->name, v, score->voice[v]);
        }
    }
    static const char *const DRUMS[] = { "kick", "snare", "hat", "hatopen" };
    for (unsigned i = 0; i < 4u; ++i) {
        Expect(&t, MusicPatchNamed(DRUMS[i]) != NULL, "no %s patch", DRUMS[i]);
    }
    Expect(&t, MusicPatchNamed("nothing") == NULL && MusicPatchNamed(NULL) == NULL,
           "a name no patch has gives one");
    Report("each channel sits where the mix puts it, and every part plays a patch the synthesizer "
           "has",
           &t);
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

static const char *const NOTE_NAMES[12] = { "C",  "C#", "D",  "D#", "E",  "F",
                                            "F#", "G",  "G#", "A",  "A#", "B" };

/// Every note of the lead and the counter-melody is one of the key's, every chord a triad the key
/// holds (its third and fifth the mode's own), and every melody fills its section's steps.
static void CheckScores(void) {
    Tally t = { 0, "" };
    uint32_t notes = 0;
    for (unsigned track = 0; track < MUSIC_TRACKS; ++track) {
        const MusicScore *score = MusicScoreOf((MusicTrack)track);
        Expect(&t,
               strcmp(score->name, MusicTrackName((MusicTrack)track)) == 0 &&
                   MusicTrackNamed(score->name) == (MusicTrack)track,
               "track %u is %s", track, score->name);
        for (uint32_t i = 0; i < score->section_count; ++i) {
            const MusicScoreSection *s = &score->sections[i];
            for (unsigned part = 0; part < 2u; ++part) {
                const MusicScoreNote *line = part == 0u ? s->lead : s->counter;
                const uint32_t count = part == 0u ? s->lead_count : s->counter_count;
                uint32_t steps = 0;
                for (uint32_t k = 0; k < count; ++k) {
                    steps += line[k].steps;
                    if (line[k].note == 0u) {
                        continue;
                    }
                    ++notes;
                    Expect(&t, InKey(score, line[k].note), "%s %s %s: %s%d is not %s",
                           score->name, s->name, part == 0u ? "lead" : "counter",
                           NOTE_NAMES[line[k].note % 12u], line[k].note / 12 - 1, score->key);
                }
                Expect(&t, steps == s->bars * score->steps, "%s %s %s lasts %u steps where %u",
                       score->name, s->name, part == 0u ? "lead" : "counter", (unsigned)steps,
                       (unsigned)(s->bars * score->steps));
            }
            for (uint32_t b = 0; b < s->bars; ++b) {
                const MusicScoreChord c = s->chords[b];
                const bool triad = InKey(score, c.root) &&
                                   InKey(score, c.root + (c.minor ? 3u : 4u)) &&
                                   InKey(score, c.root + 7u);
                Expect(&t, triad, "%s %s bar %u: %s%s is no triad of %s", score->name, s->name,
                       (unsigned)b + 1u, NOTE_NAMES[c.root], c.minor ? "m" : "", score->key);
            }
        }
    }
    Expect(&t, notes > 300u, "only %u notes in the four scores", (unsigned)notes);
    Report("every note and chord of the four scores keeps to its key, every melody full", &t);
}

static const MusicScoreSection *SectionNamed(const MusicScore *score, const char *name) {
    for (uint32_t i = 0; i < score->section_count; ++i) {
        if (strcmp(score->sections[i].name, name) == 0) {
            return &score->sections[i];
        }
    }
    return NULL;
}

/// The form: the intro first, then sections of eight bars; the closing A home on the tonic; the
/// bridge, played once, starting away from it on chords A does not play in that order.
static void CheckForm(void) {
    Tally t = { 0, "" };
    for (unsigned track = 0; track < MUSIC_TRACKS; ++track) {
        const MusicScore *score = MusicScoreOf((MusicTrack)track);
        const MusicScoreSection *intro = &score->sections[score->form[0]];
        Expect(&t, strcmp(intro->name, "intro") == 0 && intro->bars >= 2u && intro->bars <= 4u,
               "%s opens with %s of %u bars", score->name, intro->name, (unsigned)intro->bars);
        for (uint32_t i = 1; i < score->form_count; ++i) {
            const MusicScoreSection *s = &score->sections[score->form[i]];
            Expect(&t, s->bars == 8u, "%s's %s has %u bars where 8", score->name, s->name,
                   (unsigned)s->bars);
        }
        const MusicScoreSection *a = SectionNamed(score, "A");
        const MusicScoreSection *closing = SectionNamed(score, "A2");
        const MusicScoreSection *bridge = SectionNamed(score, "B");
        Expect(&t, a != NULL && closing != NULL && bridge != NULL, "%s lacks A, A2 or B",
               score->name);
        if (a == NULL || closing == NULL || bridge == NULL) {
            continue;
        }
        const MusicScoreChord home = closing->chords[closing->bars - 1u];
        Expect(&t, home.root == score->tonic, "%s's A2 ends on %s, not the tonic %s", score->name,
               NOTE_NAMES[home.root], NOTE_NAMES[score->tonic]);
        Expect(&t, bridge->chords[0].root != score->tonic, "%s's bridge starts on the tonic",
               score->name);
        bool differs = false;
        for (uint32_t b = 0; b < 8u && !differs; ++b) {
            differs = bridge->chords[b].root != a->chords[b].root ||
                      bridge->chords[b].minor != a->chords[b].minor;
        }
        Expect(&t, differs, "%s's bridge plays A's chords", score->name);
        uint32_t plays_bridge = 0;
        for (uint32_t i = 0; i < score->form_count; ++i) {
            plays_bridge += strcmp(score->sections[score->form[i]].name, "B") == 0 ? 1u : 0u;
        }
        Expect(&t, plays_bridge == 1u, "%s plays its bridge %u times", score->name,
               (unsigned)plays_bridge);
    }
    Report("every track opens with its intro, then eight-bar sections, the bridge on chords of its "
           "own and A2 home on the tonic",
           &t);
}

/// The patterns: each a bar of 16 or 24 steps, the bass's on the root or silent on the first step,
/// and every one a section plays as long as its track's bar.
static void CheckPatterns(void) {
    Tally t = { 0, "" };
    for (size_t i = 0; i < MUSIC_SCORE_PATTERN_COUNT; ++i) {
        const MusicScorePattern *p = &MUSIC_SCORE_PATTERNS[i];
        const size_t length = strlen(p->grid);
        Expect(&t, length == 16u || length == 24u, "pattern %s is %u steps", p->name,
               (unsigned)length);
        if (p->lane == MUSIC_LANE_BASS) {
            Expect(&t, p->grid[0] == 'R' || p->grid[0] == '.',
                   "bass pattern %s starts on %c, not the root or nothing", p->name, p->grid[0]);
        }
    }
    static const MusicLane LANES[4] = { MUSIC_LANE_BASS, MUSIC_LANE_DRUMS, MUSIC_LANE_COMP,
                                        MUSIC_LANE_ARP };
    for (unsigned track = 0; track < MUSIC_TRACKS; ++track) {
        const MusicScore *score = MusicScoreOf((MusicTrack)track);
        for (uint32_t i = 0; i < score->section_count; ++i) {
            const MusicScoreParts *parts = &score->sections[i].parts;
            const char *const names[4] = { parts->bass, parts->drums, parts->comp, parts->arp };
            for (unsigned k = 0; k < 4u; ++k) {
                bool found = false;
                for (size_t j = 0; j < MUSIC_SCORE_PATTERN_COUNT; ++j) {
                    const MusicScorePattern *p = &MUSIC_SCORE_PATTERNS[j];
                    if (p->lane == LANES[k] && strcmp(p->name, names[k]) == 0) {
                        found = strlen(p->grid) == score->steps;
                    }
                }
                Expect(&t, found, "%s %s: no %u-step pattern %s", score->name,
                       score->sections[i].name, (unsigned)score->steps, names[k]);
            }
        }
    }
    Report("every pattern a section plays fills its bar, the bass's on the root", &t);
}

void RunReferenceMusic(void) {
    CheckTables();
    CheckWaves();
    CheckPitch();
    CheckEnvelope();
    CheckMix();
    CheckScores();
    CheckForm();
    CheckPatterns();
}
