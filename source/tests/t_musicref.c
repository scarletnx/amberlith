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

// The demos' music, the rules its tracker plays by and its composer writes by, against answers
// worked out by hand: no sample is rendered here (the music family renders, and only when a mode
// asks for it), so these cost the run what composing four songs costs, a few hundred notes.

static const char *const SUITE = "reference";

static MusicSong song;
static MusicSong other;

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

static const char *const NOTE_NAMES[12] = { "C",  "C#", "D",  "D#", "E",  "F",
                                            "F#", "G",  "G#", "A",  "A#", "B" };

/// Whether a semitone above C is one of @p s's mode on its tonic.
static bool InMode(const MusicSong *s, uint32_t semitone) {
    for (unsigned d = 0; d < 7u; ++d) {
        if ((s->colour.tonic + s->scale[d]) % 12u == semitone % 12u) {
            return true;
        }
    }
    return false;
}

static const MusicCell *At(const MusicSong *s, unsigned position, uint32_t row, unsigned channel) {
    return &s->cells[s->order[position]][row][channel];
}

/// The channel the composer puts each part on (music.c), found from the instruments it plays.
static unsigned ChannelOf(const MusicSong *s, unsigned instrument) {
    for (unsigned p = 0; p < MUSIC_PATTERNS; ++p) {
        for (uint32_t r = 0; r < s->rows; ++r) {
            for (unsigned c = 0; c < MUSIC_CHANNELS; ++c) {
                if (s->cells[p][r][c].instrument == instrument) {
                    return c;
                }
            }
        }
    }
    return MUSIC_CHANNELS;
}

static void CheckPeriods(void) {
    Tally t = { 0, "" };
    // A cent is 1/1200 of an octave: the rounded periods keep every note within 6 of equal
    // temperament on the lowest one, and each octave halves the period (the last row, 113,
    // rounds 113.4).
    for (uint32_t n = 1; n <= MUSIC_NOTES; ++n) {
        const double exact = 856.0 * pow(2.0, -(double)(n - 1u) / 12.0);
        const uint32_t period = MusicPeriod(n);
        const double cents = 1200.0 * log2(exact / (double)period);
        Expect(&t, fabs((double)period - exact) <= 0.5 && fabs(cents) < 6.0,
               "note %u: period %u where 856 over 2 to the %u/12 is %.3f, %.1f cents off", n,
               (unsigned)period, (unsigned)(n - 1u), exact, cents);
    }
    Expect(&t, MusicPeriod(1) == 856u && MusicPeriod(13) == 428u && MusicPeriod(25) == 214u &&
                   MusicPeriod(36) == 113u,
           "the Cs and the last B: %u %u %u %u where 856 428 214 113", (unsigned)MusicPeriod(1),
           (unsigned)MusicPeriod(13), (unsigned)MusicPeriod(25), (unsigned)MusicPeriod(36));
    // 3546895 * 65536 / (period * 48000), by hand: 5657.35, 11314.71, 22629.41, 42855.70.
    static const uint32_t STEPS[4][2] = { { 856, 5657 }, { 428, 11314 }, { 214, 22629 },
                                          { 113, 42855 } };
    for (unsigned i = 0; i < 4u; ++i) {
        Expect(&t, MusicStep(STEPS[i][0]) == STEPS[i][1],
               "period %u steps %u a frame in 16.16 where %u", (unsigned)STEPS[i][0],
               (unsigned)MusicStep(STEPS[i][0]), (unsigned)STEPS[i][1]);
    }
    Report("the tracker's periods halve each octave from 856, each note within 6 cents of equal "
           "temperament",
           &t);
}

static void CheckTime(void) {
    Tally t = { 0, "" };
    // 120000 / bpm frames a tick: 1428.57 at 84, 1071.43 at 112, 960 at 125, 857.14 at 140; the
    // remainder carried, so bpm ticks make exactly 120000 frames, 2.5 seconds.
    static const uint32_t FIRST[4][5] = { { 84, 1428, 1429, 1428, 1429 },
                                          { 112, 1071, 1071, 1072, 1071 },
                                          { 125, 960, 960, 960, 960 },
                                          { 140, 857, 857, 857, 857 } };
    for (unsigned i = 0; i < 4u; ++i) {
        uint32_t carry = 0;
        uint32_t sum = 0;
        uint32_t got[4] = { 0 };
        for (uint32_t k = 0; k < FIRST[i][0]; ++k) {
            const uint32_t frames = MusicTickFrames(FIRST[i][0], &carry);
            got[k < 4u ? k : 3u] = k < 4u ? frames : got[3];
            sum += frames;
        }
        Expect(&t, sum == 120000u && memcmp(got, FIRST[i] + 1, sizeof(got)) == 0,
               "at %u bpm: ticks of %u %u %u %u frames, %u in %u ticks, where %u %u %u %u and "
               "120000",
               (unsigned)FIRST[i][0], (unsigned)got[0], (unsigned)got[1], (unsigned)got[2],
               (unsigned)got[3], (unsigned)sum, (unsigned)FIRST[i][0], (unsigned)FIRST[i][1],
               (unsigned)FIRST[i][2], (unsigned)FIRST[i][3], (unsigned)FIRST[i][4]);
    }
    Report("the tracker's tick lasts 2.5 seconds over the tempo, its remainder carried", &t);
}

static void CheckMix(void) {
    Tally t = { 0, "" };
    // Each channel alone, at 800: its own side whole, the other side three eighths, 300.
    static const int32_t WANT[4][2] = { { 800, 300 }, { 300, 800 }, { 300, 800 }, { 800, 300 } };
    for (unsigned c = 0; c < MUSIC_CHANNELS; ++c) {
        int32_t in[MUSIC_CHANNELS] = { 0, 0, 0, 0 };
        in[c] = 800;
        int32_t left = 0;
        int32_t right = 0;
        MusicPan(in, &left, &right);
        Expect(&t, left == WANT[c][0] && right == WANT[c][1],
               "channel %u alone at 800: left %d right %d where %d %d", c, (int)left, (int)right,
               (int)WANT[c][0], (int)WANT[c][1]);
    }
    // All four at their peak still fit in 16 bits after the 11/8 the player gives the mix.
    int32_t peak[MUSIC_CHANNELS] = { MUSIC_CHANNEL_PEAK, MUSIC_CHANNEL_PEAK, MUSIC_CHANNEL_PEAK,
                                     MUSIC_CHANNEL_PEAK };
    int32_t left = 0;
    int32_t right = 0;
    MusicPan(peak, &left, &right);
    Expect(&t, left * 11 / 8 <= 32767 && right * 11 / 8 <= 32767,
           "four channels at %d mix to %d and %d, over 32767 with the player's 11/8",
           MUSIC_CHANNEL_PEAK, (int)(left * 11 / 8), (int)(right * 11 / 8));
    // The low passes' corners from their coefficients: a = 1 - exp(-2 pi f / 48000), so f is
    // -ln(1 - a) 48000 / (2 pi): 4420 Hz for the A500's, 3276 Hz for the power light's.
    const double pi = 3.14159265358979323846;
    const double a500 = -log(1.0 - MUSIC_FILTER_A500_Q16 / 65536.0) * 48000.0 / (2.0 * pi);
    const double led = -log(1.0 - MUSIC_FILTER_LED_Q16 / 65536.0) * 48000.0 / (2.0 * pi);
    Expect(&t, fabs(a500 - 4420.0) < 5.0 && fabs(led - 3276.0) < 5.0,
           "the low passes' corners at %.0f and %.0f Hz where 4420 and 3276", a500, led);
    // A step of 1000 through one pole: after one frame a of it, 1000 * 28790 / 65536 = 439.3,
    // which in 24.8 fixed point is 112460 (256000 * 28790 >> 16, 112460.9 cut down); the state
    // then settles within the 2/256 its cut leaves of the step.
    const int32_t one = MusicFilterStep(0, 1000, MUSIC_FILTER_A500_Q16);
    int32_t settled = 0;
    for (unsigned i = 0; i < 64u; ++i) {
        settled = MusicFilterStep(settled, 1000, MUSIC_FILTER_A500_Q16);
    }
    Expect(&t, one == 112460 && 256000 - settled >= 0 && 256000 - settled <= 2,
           "a step of 1000: %d after one frame where 112460, %d after 64 where 255998 to 256000",
           (int)one, (int)settled);
    Report("the tracker's mix: channels 0 and 3 left, 1 and 2 right, three eighths across, and "
           "the A500's low passes",
           &t);
}

/// Every note the bass, the chords (their arpeggios too) and the lead play keeps to the mode.
static void CheckModes(void) {
    Tally t = { 0, "" };
    uint32_t notes = 0;
    for (unsigned track = 0; track < MUSIC_TRACKS; ++track) {
        MusicCompose(&song, (MusicTrack)track, MusicColourOf((MusicTrack)track)->seed);
        const unsigned bass = ChannelOf(&song, MUSIC_INSTRUMENT_BASS);
        const unsigned chord = ChannelOf(&song, MUSIC_INSTRUMENT_CHORD);
        const unsigned lead = ChannelOf(&song, MUSIC_INSTRUMENT_LEAD);
        for (unsigned p = 0; p < MUSIC_PATTERNS; ++p) {
            uint32_t arp = 0;
            for (uint32_t r = 0; r < song.rows; ++r) {
                const unsigned parts[3] = { bass, chord, lead };
                for (unsigned k = 0; k < 3u; ++k) {
                    const MusicCell *cell = &song.cells[p][r][parts[k]];
                    if (cell->note == 0u || cell->note == MUSIC_NOTE_OFF) {
                        continue;
                    }
                    ++notes;
                    Expect(&t, cell->note <= MUSIC_NOTES && InMode(&song, cell->note - 1u),
                           "%s, pattern %u row %u channel %u: %s%u is not %s %s",
                           song.colour.name, p, (unsigned)r, parts[k],
                           NOTE_NAMES[(cell->note - 1u) % 12u], (cell->note - 1u) / 12u + 1u,
                           NOTE_NAMES[song.colour.tonic],
                           MusicModeName((MusicMode)song.colour.mode));
                    if (parts[k] == chord) {
                        arp = cell->param;
                        const uint32_t top = cell->note + ((arp >> 4) > (arp & 15u)
                                                               ? (arp >> 4)
                                                               : (arp & 15u));
                        Expect(&t,
                               InMode(&song, cell->note - 1u + (arp >> 4)) &&
                                   InMode(&song, cell->note - 1u + (arp & 15u)) &&
                                   top <= MUSIC_NOTES,
                               "%s, pattern %u row %u: the arpeggio %02X on note %u leaves the "
                               "mode or the tracker's three octaves",
                               song.colour.name, p, (unsigned)r, (unsigned)arp,
                               (unsigned)cell->note);
                    }
                }
            }
        }
    }
    Expect(&t, notes > 400u, "only %u notes in the four tracks", (unsigned)notes);
    Report("every note and arpeggio of the four tracks keeps to its mode and the tracker's three "
           "octaves",
           &t);
}

/// The semitones from a degree's root to its chord tone @p above degrees up.
static uint32_t Above(const MusicSong *s, uint32_t degree, uint32_t above) {
    const uint32_t top = degree + above;
    return s->scale[top % 7u] + (top >= 7u ? 12u : 0u) - s->scale[degree];
}

/// Each bar's chord: no diminished triad, its arpeggio the voicing's two tones above the root,
/// the bass on its root on the bar's first row, the lead on a chord tone on every beat.
static void CheckChords(void) {
    Tally t = { 0, "" };
    for (unsigned track = 0; track < MUSIC_TRACKS; ++track) {
        MusicCompose(&song, (MusicTrack)track, MusicColourOf((MusicTrack)track)->seed);
        const unsigned bass = ChannelOf(&song, MUSIC_INSTRUMENT_BASS);
        const unsigned lead = ChannelOf(&song, MUSIC_INSTRUMENT_LEAD);
        for (unsigned bar = 0; bar < MUSIC_BARS; ++bar) {
            const unsigned position = bar / MUSIC_PATTERN_BARS;
            const uint32_t first = (bar % MUSIC_PATTERN_BARS) * song.rows_per_bar;
            const uint32_t degree = song.chord[bar];
            Expect(&t, degree < 7u && Above(&song, degree, 4u) == 7u,
                   "%s bar %u: degree %u's fifth is %u semitones", song.colour.name, bar + 1u,
                   (unsigned)degree + 1u, (unsigned)Above(&song, degree, 4u));
            uint32_t x = 0;
            uint32_t y = 0;
            switch ((MusicVoicing)song.colour.voicing) {
            case MUSIC_VOICING_TRIAD:
                x = Above(&song, degree, 2u);
                y = Above(&song, degree, 4u);
                break;
            case MUSIC_VOICING_SHELL:
                x = Above(&song, degree, 2u);
                y = Above(&song, degree, 6u);
                break;
            case MUSIC_VOICING_SUS2:
                x = Above(&song, degree, 1u);
                y = Above(&song, degree, 4u);
                break;
            case MUSIC_VOICING_OPEN:
                x = Above(&song, degree, 4u);
                y = Above(&song, degree, 1u) + 12u;
                break;
            }
            Expect(&t, song.arp[bar] == ((x << 4) | y),
                   "%s bar %u: arpeggio %02X where the voicing gives %X%X", song.colour.name,
                   bar + 1u, (unsigned)song.arp[bar], (unsigned)x, (unsigned)y);
            const MusicCell *root = At(&song, position, first, bass);
            Expect(&t,
                   root->note != 0u &&
                       (root->note - 1u) % 12u ==
                           (song.colour.tonic + song.scale[degree]) % 12u,
                   "%s bar %u: the bass's first note is %u, not the root of degree %u",
                   song.colour.name, bar + 1u, (unsigned)root->note, (unsigned)degree + 1u);
            for (uint32_t r = 0; r < song.rows_per_bar; r += song.colour.rows_per_beat) {
                const MusicCell *cell = At(&song, position, first + r, lead);
                if (cell->note == 0u || cell->note == MUSIC_NOTE_OFF) {
                    continue;
                }
                const uint32_t pc = (cell->note - 1u + 12u - song.colour.tonic) % 12u;
                const bool tone = pc == song.scale[degree] ||
                                  pc == song.scale[(degree + 2u) % 7u] ||
                                  pc == song.scale[(degree + 4u) % 7u];
                Expect(&t, tone, "%s bar %u row %u: the lead's %s is no tone of degree %u",
                       song.colour.name, bar + 1u, (unsigned)r,
                       NOTE_NAMES[(cell->note - 1u) % 12u], (unsigned)degree + 1u);
            }
        }
    }
    Report("every bar's chord is whole in its mode and voicing, its root in the bass, the lead on "
           "its tones on the beat",
           &t);
}

/// A A B A: the head again at each A, the closed A ending on the tonic and the open one away
/// from it, the bridge on chords of its own ending on the one that leads home.
static void CheckForm(void) {
    Tally t = { 0, "" };
    for (unsigned track = 0; track < MUSIC_TRACKS; ++track) {
        MusicCompose(&song, (MusicTrack)track, MusicColourOf((MusicTrack)track)->seed);
        const char *name = song.colour.name;
        Expect(&t, memcmp(song.section, "AAAABBAA", MUSIC_ORDER) == 0,
               "%s: sections %.8s where AAAABBAA", name, song.section);
        Expect(&t,
               song.order[0] == song.order[2] && song.order[0] == song.order[6] &&
                   song.order[3] == song.order[7] && song.order[1] != song.order[3] &&
                   song.order[4] != song.order[0] && song.order[5] != song.order[4],
               "%s: order %u %u %u %u %u %u %u %u", name, song.order[0], song.order[1],
               song.order[2], song.order[3], song.order[4], song.order[5], song.order[6],
               song.order[7]);
        const uint8_t *bars = song.chord;
        Expect(&t, bars[0] == 0u && bars[15] == 0u && bars[31] == 0u && bars[7] != 0u,
               "%s: the A sections start on %u, the open one ends on %u, the closed ones on %u "
               "and %u, where the tonic, 1, but the open one",
               name, bars[0] + 1u, bars[7] + 1u, bars[15] + 1u, bars[31] + 1u);
        Expect(&t, bars[16] != 0u && bars[23] != 0u && bars[23] == bars[14],
               "%s: the bridge runs from %u to %u where neither the tonic and the last the "
               "closed A's chord before the tonic, %u",
               name, bars[16] + 1u, bars[23] + 1u, bars[14] + 1u);
        Expect(&t, memcmp(bars + 16, bars, 8u) != 0,
               "%s: the bridge's progression is A's", name);
        // The closed A's last bar holds the tonic in the lead.
        const unsigned lead = ChannelOf(&song, MUSIC_INSTRUMENT_LEAD);
        const MusicCell *last = At(&song, 7, 3u * song.rows_per_bar, lead);
        Expect(&t, last->note != 0u && (last->note - 1u) % 12u == song.colour.tonic,
               "%s: the last bar's lead is note %u, not the tonic %s", name,
               (unsigned)last->note, NOTE_NAMES[song.colour.tonic]);
    }
    Report("every track's form is A A B A, the closed A home on the tonic and the bridge on its "
           "own chords toward it",
           &t);
}

/// A seed makes one song, and another seed another melody in the same colour.
static void CheckSeeds(void) {
    Tally t = { 0, "" };
    for (unsigned track = 0; track < MUSIC_TRACKS; ++track) {
        const uint32_t seed = MusicColourOf((MusicTrack)track)->seed;
        MusicCompose(&song, (MusicTrack)track, seed);
        MusicCompose(&other, (MusicTrack)track, seed);
        Expect(&t, memcmp(&song, &other, sizeof(song)) == 0, "%s: one seed, two songs",
               song.colour.name);
        MusicCompose(&other, (MusicTrack)track, seed + 1u);
        const unsigned lead = ChannelOf(&song, MUSIC_INSTRUMENT_LEAD);
        bool differs = false;
        for (unsigned p = 0; p < MUSIC_PATTERNS && !differs; ++p) {
            for (uint32_t r = 0; r < song.rows && !differs; ++r) {
                differs = song.cells[p][r][lead].note != other.cells[p][r][lead].note;
            }
        }
        Expect(&t,
               differs && memcmp(song.scale, other.scale, sizeof(song.scale)) == 0 &&
                   memcmp(song.section, other.section, sizeof(song.section)) == 0 &&
                   song.colour.mode == other.colour.mode,
               "%s: seed %08X and the next: the lead %s, the mode %s", song.colour.name,
               (unsigned)seed, differs ? "differs" : "is the same",
               song.colour.mode == other.colour.mode ? "the same" : "differs");
    }
    Report("a seed composes one song, and the next seed another melody in the same mode and form",
           &t);
}

/// Each track's colour is its own, and every wave the probe makes stays inside 8 bits, a loop
/// with no constant part to thump.
static void CheckColours(void) {
    Tally t = { 0, "" };
    for (unsigned a = 0; a < MUSIC_TRACKS; ++a) {
        const MusicColour *x = MusicColourOf((MusicTrack)a);
        Expect(&t, MusicTrackNamed(x->name) == (MusicTrack)a, "%s names track %u", x->name,
               (unsigned)MusicTrackNamed(x->name));
        for (unsigned b = a + 1u; b < MUSIC_TRACKS; ++b) {
            const MusicColour *y = MusicColourOf((MusicTrack)b);
            Expect(&t,
                   x->mode != y->mode && x->bpm != y->bpm && x->lead.wave != y->lead.wave &&
                       x->groove != y->groove && x->voicing != y->voicing,
                   "%s and %s share a mode, a tempo, a lead, a groove or a voicing", x->name,
                   y->name);
        }
        MusicCompose(&song, (MusicTrack)a, x->seed);
        for (unsigned i = 1; i <= MUSIC_INSTRUMENTS; ++i) {
            const MusicInstrument *in = &song.instruments[i];
            if (in->length == 0u) {
                continue;
            }
            int32_t sum = 0;
            bool inside = in->offset + in->length <= song.bank_used;
            for (uint32_t k = 0; inside && k < in->length; ++k) {
                const int8_t v = song.bank[in->offset + k];
                sum += v;
                inside = v >= -127;
            }
            const bool looped = in->loop_start < in->length;
            Expect(&t, inside && (!looped || (sum >= -(int32_t)in->length &&
                                              sum <= (int32_t)in->length)),
                   "%s instrument %u: wave %u of %u samples sums to %d, or leaves 8 bits",
                   x->name, i, (unsigned)in->voice.wave, (unsigned)in->length, (int)sum);
        }
    }
    Expect(&t, MusicTrackNamed("nothing") == MUSIC_TRACKS && MusicTrackNamed(NULL) == MUSIC_TRACKS,
           "a name no track has gives %u", (unsigned)MusicTrackNamed("nothing"));
    Report("every track's colour is its own, and the probe's waves fit 8 bits, the looped ones "
           "about nought",
           &t);
}

void RunReferenceMusic(void) {
    CheckPeriods();
    CheckTime();
    CheckMix();
    CheckModes();
    CheckChords();
    CheckForm();
    CheckSeeds();
    CheckColours();
}
