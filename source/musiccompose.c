// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string.h>

#include "music.h"

// Integers and a hash of the seed only: the same track and seed write the same cells on every
// machine, so the music family can hold a machine's samples to the host's.

/// lowbias32: every bit of the input moves every bit of the output, so neighbouring seeds give
/// songs nothing alike.
static uint32_t Mix(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

/// @p d moved by octaves into the range @p lo to @p hi, which spans one at least.
static int32_t Fold(int32_t d, int32_t lo, int32_t hi) {
    while (d > hi) {
        d -= 7;
    }
    while (d < lo) {
        d += 7;
    }
    return d;
}

/// A number below @p n from the stream @p state draws from.
static uint32_t Pick(uint32_t *state, uint32_t n) {
    *state = Mix(*state + 0x9E3779B9u);
    return n != 0u ? (uint32_t)(((uint64_t)*state * n) >> 32) : 0u;
}

static const char *Entry(const MusicScore *s, MusicPool pool, uint32_t index) {
    const MusicScorePool *p = &s->pool[pool];
    return p->count != 0u ? p->entry[index % p->count] : NULL;
}

static uint32_t Count(const MusicScore *s, MusicPool pool) {
    return s->pool[pool].count;
}

uint32_t MusicComposeNote(uint32_t tonic, MusicMode mode, uint32_t step) {
    return tonic + 12u * (step / 7u) + MusicModeSteps(mode)[step % 7u];
}

static uint32_t Note(const MusicScore *s, int32_t step) {
    return MusicComposeNote(s->tonic, (MusicMode)s->mode, step < 0 ? 0u : (uint32_t)step);
}

/// The lowest scale step whose note is @p midi or above.
static int32_t StepFrom(const MusicScore *s, uint32_t midi) {
    int32_t step = 0;
    while (Note(s, step) < midi && step < 80) {
        ++step;
    }
    return step;
}

/// Whether scale step @p step is a note of the triad on degree @p chord.
static bool ChordTone(int32_t step, uint32_t chord) {
    const int32_t from = ((step - (int32_t)chord) % 7 + 7) % 7;
    return from == 0 || from == 2 || from == 4;
}

/// The chord tone nearest @p step, the one toward @p dir on a tie.
static int32_t NearestChordTone(int32_t step, uint32_t chord, int32_t dir) {
    for (int32_t d = 0; d < 4; ++d) {
        if (ChordTone(step + d * dir, chord)) {
            return step + d * dir;
        }
        if (ChordTone(step - d * dir, chord)) {
            return step - d * dir;
        }
    }
    return step;
}

static uint8_t Patch(const MusicPatch *patch) {
    return patch != NULL ? (uint8_t)(patch - MUSIC_PATCHES + 1) : 0u;
}

static const MusicPatch *PatchOf(uint8_t index) {
    return index != 0u && index <= MUSIC_PATCH_COUNT ? &MUSIC_PATCHES[index - 1u] : NULL;
}

static MusicPart PartOf(char letter) {
    switch (letter) {
    case 'I':
        return MUSIC_PART_INTRO;
    case 'B':
        return MUSIC_PART_B;
    case 'R':
        return MUSIC_PART_BREAK;
    case 'C':
        return MUSIC_PART_CLIMAX;
    default:
        return MUSIC_PART_A;
    }
}

/// The progression pool a part draws its chords from, and which of the pass's picks it takes.
static MusicPool ChordPool(MusicPart part) {
    return part == MUSIC_PART_B ? MUSIC_POOL_LIFT
           : part == MUSIC_PART_BREAK ? MUSIC_POOL_DROP
                                      : MUSIC_POOL_HOME;
}

static uint32_t FirstChord(const MusicComposer *c, MusicPart part) {
    const MusicScore *s = MusicScoreOf(c->track);
    const MusicPool pool = ChordPool(part);
    const char *p = Entry(s, pool, c->progression[pool - MUSIC_POOL_HOME]);
    return p != NULL ? (uint32_t)(p[0] - '1') : 0u;
}

// ---------------------------------------------------------------------------------------------
// The lead's melodies

/// One phrase of two bars over @p chords: a note on each onset of the rhythm @p cell, a chord
/// tone on each strong beat (the first and third of a bar) moving toward @p dir, steps between
/// them, the line turning back at the range's edges and, half the time, halfway.
static void Phrase(uint32_t *r, const char *cell, const uint8_t *chords, int32_t lo, int32_t hi,
                   int32_t start, int32_t out[32]) {
    int32_t prev = start;
    int32_t dir = Pick(r, 2u) != 0u ? 1 : -1;
    bool first = true;
    for (uint32_t row = 0; row < 32u; ++row) {
        out[row] = -1;
        if (cell[row] != 'x') {
            continue;
        }
        const uint32_t chord = chords[row / 16u];
        int32_t d;
        if (first) {
            d = start;
        } else if (row % 8u == 0u) {
            d = NearestChordTone(prev + dir, chord, dir);
        } else {
            const uint32_t roll = Pick(r, 10u);
            d = prev + (roll < 6u ? dir : roll < 8u ? 2 * dir : roll < 9u ? -dir : 0);
        }
        if (d < lo) {
            d = lo + (lo - d);
            dir = 1;
        } else if (d > hi) {
            d = hi - (d - hi);
            dir = -1;
        }
        if (row % 8u == 0u) {
            d = Fold(NearestChordTone(d, chord, dir), lo, hi);
        }
        if (row >= 16u && first == false && Pick(r, 4u) == 0u) {
            dir = -dir;
        }
        out[row] = d;
        prev = d;
        first = false;
    }
}

/// @p from moved to two bars of other chords: each note by the steps between the first bars'
/// roots (the nearer way), each strong beat to the nearest chord tone, all of it kept in range.
static void Answer(const int32_t from[32], const uint8_t *was, const uint8_t *now, int32_t lo,
                   int32_t hi, int32_t out[32]) {
    int32_t shift = ((int32_t)now[0] - (int32_t)was[0]) % 7;
    if (shift > 3) {
        shift -= 7;
    } else if (shift < -3) {
        shift += 7;
    }
    for (uint32_t row = 0; row < 32u; ++row) {
        if (from[row] < 0) {
            out[row] = -1;
            continue;
        }
        int32_t d = from[row] + shift;
        if (row % 8u == 0u) {
            d = NearestChordTone(d, now[row / 16u], 1);
        }
        out[row] = Fold(d, lo, hi);
    }
}

/// Ends a phrase on its last chord's root: the last note moves to the nearest one.
static void Cadence(int32_t phrase[32], uint32_t chord, int32_t lo, int32_t hi) {
    for (int32_t row = 31; row >= 0; --row) {
        if (phrase[row] < 0) {
            continue;
        }
        int32_t d = phrase[row];
        int32_t best = d;
        for (int32_t k = 0; k < 4; ++k) {
            if (((d + k - (int32_t)chord) % 7 + 7) % 7 == 0 && d + k <= hi) {
                best = d + k;
                break;
            }
            if (((d - k - (int32_t)chord) % 7 + 7) % 7 == 0 && d - k >= lo) {
                best = d - k;
                break;
            }
        }
        phrase[row] = best;
        return;
    }
}

/// Writes a phrase into the lead's line from row @p at: a step plus one on each onset, a rest
/// where the rhythm rests after a note, nothing where it holds.
static void Write(MusicComposer *c, const char *cell, const int32_t phrase[32], uint32_t at) {
    bool sounding = false;
    for (uint32_t row = 0; row < 32u; ++row) {
        uint8_t v = 0;
        if (cell[row] == 'x' && phrase[row] >= 0) {
            v = (uint8_t)(phrase[row] + 1);
            sounding = true;
        } else if (cell[row] == '.' && sounding) {
            v = MUSIC_NOTE_OFF;
            sounding = false;
        }
        c->lead[at + row] = v;
    }
}

/// The section's melody, four phrases of two bars: a statement, its answer over the next chords,
/// a contrasting phrase on the other rhythm a third higher, and the statement again ending on the
/// root. The climax plays the A section's melody; the B section leads with its own rhythm.
static void ComposeLead(MusicComposer *c, MusicPart part) {
    const MusicScore *s = MusicScoreOf(c->track);
    const MusicPart theme = part == MUSIC_PART_CLIMAX ? MUSIC_PART_A : part;
    uint32_t r = Mix(c->seed ^ (0xA11CEu + (uint32_t)theme * 0x9E3779B9u));
    const char *ra = Entry(s, MUSIC_POOL_RHYTHM, c->rhythm[theme == MUSIC_PART_B ? 1 : 0]);
    const char *rb = Entry(s, MUSIC_POOL_RHYTHM, c->rhythm[theme == MUSIC_PART_B ? 0 : 1]);
    if (ra == NULL || rb == NULL) {
        return;
    }
    const int32_t lo = StepFrom(s, s->range[MUSIC_RANGE_LEAD][0]);
    int32_t hi = StepFrom(s, s->range[MUSIC_RANGE_LEAD][1]);
    if (Note(s, hi) > s->range[MUSIC_RANGE_LEAD][1]) {
        --hi;
    }
    const int32_t mid = (lo + hi) / 2;
    int32_t a[32];
    int32_t answer[32];
    int32_t b[32];
    int32_t again[32];
    const int32_t start =
        Fold(NearestChordTone(mid + (int32_t)Pick(&r, 5u) - 2, c->chord[0], 1), lo, hi);
    Phrase(&r, ra, &c->chord[0], lo, hi, start, a);
    Answer(a, &c->chord[0], &c->chord[2], lo, hi, answer);
    const int32_t high = Fold(NearestChordTone(start + 2, c->chord[4], 1), lo, hi);
    Phrase(&r, rb, &c->chord[4], lo, hi, high, b);
    Answer(a, &c->chord[0], &c->chord[6], lo, hi, again);
    Cadence(again, c->chord[7], lo, hi);
    Write(c, ra, a, 0);
    Write(c, ra, answer, 32);
    Write(c, rb, b, 64);
    Write(c, ra, again, 96);
}

// ---------------------------------------------------------------------------------------------
// Passes and sections

static void EnterSection(MusicComposer *c) {
    const MusicScore *s = MusicScoreOf(c->track);
    const MusicPart part = (MusicPart)c->form[c->position];
    // The same part picks the same patterns all through a pass; a fill is the section's own.
    uint32_t r = Mix(c->seed ^ (0x5EC7u + (uint32_t)part * 0x9E3779B9u));
    const MusicPool pool = ChordPool(part);
    const char *progression = Entry(s, pool, c->progression[pool - MUSIC_POOL_HOME]);
    for (uint32_t bar = 0; bar < MUSIC_SECTION_BARS; ++bar) {
        c->chord[bar] = progression != NULL ? (uint8_t)(progression[bar % 4u] - '1') : 0u;
    }
    c->groove = (uint8_t)Pick(&r, Count(s, MUSIC_POOL_GROOVE));
    c->bass = (uint8_t)Pick(&r, Count(s, part == MUSIC_PART_BREAK ? MUSIC_POOL_BREAKBASS
                                                                  : MUSIC_POOL_BASS));
    c->pad = (uint8_t)Pick(&r, Count(s, part == MUSIC_PART_CLIMAX && Count(s, MUSIC_POOL_CLIMAX)
                                            ? MUSIC_POOL_CLIMAX
                                            : MUSIC_POOL_PAD));
    c->arp = (uint8_t)Pick(&r, Count(s, MUSIC_POOL_ARP));
    uint32_t own = Mix(c->seed ^ (0xF111u + c->position * 0x9E3779B9u));
    c->fill = (uint8_t)Pick(&own, Count(s, MUSIC_POOL_FILL));
    memcpy(c->tail, &c->lead[MUSIC_LEAD_ROWS - MUSIC_ECHO_MAX], MUSIC_ECHO_MAX);
    memset(c->lead, 0, sizeof(c->lead));
    if (part == MUSIC_PART_A || part == MUSIC_PART_B || part == MUSIC_PART_CLIMAX) {
        ComposeLead(c, part);
    }
    c->cascade = 0;
}

/// Starts a pass: its form (its intro left out after the first pass), its patches, its
/// progressions and its lead's rhythms.
static void Pass(MusicComposer *c, uint32_t seed, bool intro) {
    const MusicScore *s = MusicScoreOf(c->track);
    c->seed = seed;
    uint32_t r = seed;
    const char *form = Entry(s, MUSIC_POOL_FORM, Pick(&r, Count(s, MUSIC_POOL_FORM)));
    c->form_count = 0;
    for (uint32_t i = 0; form != NULL && form[i] != '\0' && c->form_count < MUSIC_FORM_MAX; ++i) {
        if (form[i] != 'I' || intro) {
            c->form[c->form_count++] = (uint8_t)PartOf(form[i]);
        }
    }
    if (c->form_count == 0u) {
        c->form[c->form_count++] = MUSIC_PART_A;
    }
    for (uint32_t v = 0; v < MUSIC_VOICES; ++v) {
        const uint32_t choice = s->voice[v][1] != NULL ? Pick(&r, 2u) : 0u;
        c->patch[v] = Patch(MusicPatchNamed(s->voice[v][choice]));
    }
    for (uint32_t k = 0; k < 3u; ++k) {
        c->progression[k] = (uint8_t)Pick(&r, Count(s, (MusicPool)(MUSIC_POOL_HOME + k)));
    }
    const uint32_t rhythms = Count(s, MUSIC_POOL_RHYTHM);
    c->rhythm[0] = (uint8_t)Pick(&r, rhythms);
    c->rhythm[1] = rhythms > 1u ? (uint8_t)((c->rhythm[0] + 1u + Pick(&r, rhythms - 1u)) % rhythms)
                                : c->rhythm[0];
    c->position = 0;
    c->bar = 0;
    EnterSection(c);
}

void MusicComposeStart(MusicComposer *c, MusicTrack track, uint32_t seed) {
    memset(c, 0, sizeof(*c));
    c->track = (unsigned)track < MUSIC_TRACKS ? track : MUSIC_TRACK_MENU;
    Pass(c, seed, true);
}

uint32_t MusicComposePassBars(MusicTrack track, uint32_t seed) {
    const MusicScore *s = MusicScoreOf(track);
    if (s == NULL) {
        return 0;
    }
    uint32_t r = seed;
    const char *form = Entry(s, MUSIC_POOL_FORM, Pick(&r, Count(s, MUSIC_POOL_FORM)));
    const size_t length = form != NULL ? strlen(form) : 0u;
    const uint32_t parts = length == 0u               ? 1u
                           : length > MUSIC_FORM_MAX ? MUSIC_FORM_MAX
                                                      : (uint32_t)length;
    return parts * MUSIC_SECTION_BARS;
}

MusicPart MusicComposePart(const MusicComposer *c) {
    return (MusicPart)c->form[c->position];
}

uint32_t MusicComposeChord(const MusicComposer *c) {
    return c->chord[c->bar];
}

// ---------------------------------------------------------------------------------------------
// A bar

static void Put(MusicCell *cell, uint32_t note, uint8_t patch, uint32_t volume, uint8_t effect,
                int32_t param) {
    cell->note = (uint8_t)note;
    cell->patch = patch;
    cell->volume = (uint8_t)volume;
    cell->effect = effect;
    cell->param = (int8_t)param;
}

/// The kit on one channel: each hit changes the patch, the hat's loudness marks the accents.
static void Drums(const MusicComposer *c, MusicCell bar[][MUSIC_CHANNELS], uint32_t ch,
                  MusicPart part, bool last) {
    const MusicScore *s = MusicScoreOf(c->track);
    const char *grid = Entry(s, MUSIC_POOL_GROOVE, c->groove);
    if (part == MUSIC_PART_INTRO && c->bar < 4u && Count(s, MUSIC_POOL_INTRO) != 0u) {
        grid = Entry(s, MUSIC_POOL_INTRO, c->groove);
    } else if (part == MUSIC_PART_BREAK) {
        grid = Entry(s, MUSIC_POOL_BREAKBEAT, c->groove);
    } else if (last && Count(s, MUSIC_POOL_FILL) != 0u) {
        grid = Entry(s, MUSIC_POOL_FILL, c->fill);
    }
    if (grid == NULL) {
        return;
    }
    for (uint32_t row = 0; row < MUSIC_BAR_ROWS; ++row) {
        uint32_t voice = MUSIC_VOICES;
        uint32_t volume = 0;
        switch (grid[row]) {
        case 'K':
            voice = MUSIC_VOICE_KICK;
            volume = 63;
            break;
        case 'S':
            voice = MUSIC_VOICE_SNARE;
            volume = 60;
            break;
        case 's':
            voice = MUSIC_VOICE_SNARE;
            volume = 44;
            break;
        case 'h':
            voice = MUSIC_VOICE_HAT;
            volume = 56;
            break;
        case 'H':
            voice = MUSIC_VOICE_HAT;
            volume = 63;
            break;
        default:
            break;
        }
        const MusicPatch *patch = voice < MUSIC_VOICES ? PatchOf(c->patch[voice]) : NULL;
        if (patch != NULL) {
            Put(&bar[row][ch], patch->note, c->patch[voice], volume, MUSIC_FX_NONE, 0);
        }
    }
}

/// The scale step of the lowest root of degree @p chord at or above note @p low.
static int32_t RootFrom(const MusicScore *s, uint32_t low, uint32_t chord) {
    int32_t step = StepFrom(s, low);
    while (((step - (int32_t)chord) % 7 + 7) % 7 != 0) {
        ++step;
    }
    return step;
}

static void Bass(const MusicComposer *c, MusicCell bar[][MUSIC_CHANNELS], uint32_t ch,
                 MusicPart part, uint32_t next) {
    const MusicScore *s = MusicScoreOf(c->track);
    const char *grid = Entry(s, part == MUSIC_PART_BREAK ? MUSIC_POOL_BREAKBASS : MUSIC_POOL_BASS,
                             c->bass);
    if (grid == NULL) {
        return;
    }
    const uint32_t chord = c->chord[c->bar];
    const int32_t root = RootFrom(s, s->range[MUSIC_RANGE_BASS][0], chord);
    const int32_t to = RootFrom(s, s->range[MUSIC_RANGE_BASS][0], next);
    for (uint32_t row = 0; row < MUSIC_BAR_ROWS; ++row) {
        const char mark = grid[row];
        int32_t step = -1;
        switch (mark | 0x20) {
        case 'r':
            step = root;
            break;
        case 'o':
            step = root + 7;
            break;
        case '5':
            step = root + 4;
            break;
        case '3':
            step = root + 2;
            break;
        case '7':
            step = root + 6;
            break;
        case 'a':
            step = to > root ? to - 1 : to + 1;
            break;
        default:
            break;
        }
        if (mark == '.') {
            Put(&bar[row][ch], MUSIC_NOTE_OFF, 0, MUSIC_VOLUME_KEEP, MUSIC_FX_NONE, 0);
        } else if (step >= 0) {
            const bool glide = mark >= 'a' && mark <= 'z';
            Put(&bar[row][ch], Note(s, step), c->patch[MUSIC_VOICE_BASS], row == 0u ? 58u : 54u,
                glide ? MUSIC_FX_TONE : MUSIC_FX_NONE, glide ? 24 : 0);
        }
    }
}

/// The chord's three notes in the pads' range, the inversion nearest the voicing before.
static void Voice(MusicComposer *c, uint32_t chord) {
    const MusicScore *s = MusicScoreOf(c->track);
    const uint32_t lo = s->range[MUSIC_RANGE_PAD][0];
    uint32_t best[3] = { 0, 0, 0 };
    int32_t best_distance = -1;
    for (uint32_t inversion = 0; inversion < 3u; ++inversion) {
        uint32_t v[3];
        for (uint32_t i = 0; i < 3u; ++i) {
            const uint32_t pc = Note(s, (int32_t)(chord + 2u * ((inversion + i) % 3u))) % 12u;
            uint32_t note = lo + (pc + 12u - lo % 12u) % 12u;
            while (i > 0u && note <= v[i - 1u]) {
                note += 12u;
            }
            v[i] = note;
        }
        int32_t distance = 0;
        for (uint32_t i = 0; i < 3u; ++i) {
            const int32_t was = c->voicing[i] != 0u ? (int32_t)c->voicing[i] : (int32_t)lo + 7;
            distance += (int32_t)v[i] > was ? (int32_t)v[i] - was : was - (int32_t)v[i];
        }
        if (best_distance < 0 || distance < best_distance) {
            best_distance = distance;
            memcpy(best, v, sizeof(best));
        }
    }
    for (uint32_t i = 0; i < 3u; ++i) {
        c->voicing[i] = (uint8_t)best[i];
    }
}

/// The pads: the chord on the pattern's strikes, gliding into each new chord when the score
/// asks, swelling in at the start of an intro or a breakdown; at the climax a cascade, each pad
/// in turn taking the next chord tone and holding it.
static void Pads(MusicComposer *c, MusicCell bar[][MUSIC_CHANNELS], const uint32_t *ch,
                 uint32_t count, MusicPart part) {
    const MusicScore *s = MusicScoreOf(c->track);
    const bool climax = part == MUSIC_PART_CLIMAX && Count(s, MUSIC_POOL_CLIMAX) != 0u;
    const char *grid = Entry(s, climax ? MUSIC_POOL_CLIMAX : MUSIC_POOL_PAD, c->pad);
    if (grid == NULL || count == 0u) {
        return;
    }
    const uint32_t chord = c->chord[c->bar];
    const bool fresh = c->bar == 0u;
    Voice(c, chord);
    const bool swell = fresh && (part == MUSIC_PART_INTRO || part == MUSIC_PART_BREAK);
    for (uint32_t row = 0; row < MUSIC_BAR_ROWS; ++row) {
        const char mark = grid[row];
        if (mark == 'c') {
            // The chord's notes up two octaves and back, one pad after another.
            const uint32_t k = c->cascade++ % 10u;
            const uint32_t at = k < 6u ? k : 10u - k;
            const uint32_t note = c->voicing[at % 3u] + 12u * (at / 3u);
            Put(&bar[row][ch[k % count]], note, c->patch[MUSIC_VOICE_PAD], 56, MUSIC_FX_NONE, 0);
            continue;
        }
        for (uint32_t i = 0; i < count; ++i) {
            MusicCell *cell = &bar[row][ch[i]];
            if (mark == 'x') {
                const bool glide = row == 0u && !fresh && s->glide != 0u;
                Put(cell, c->voicing[i % 3u], glide ? 0u : c->patch[MUSIC_VOICE_PAD],
                    swell ? 24u : glide ? MUSIC_VOLUME_KEEP : 56u,
                    glide ? MUSIC_FX_TONE : MUSIC_FX_NONE, glide ? s->glide : 0);
            } else if (mark == '.') {
                Put(cell, MUSIC_NOTE_OFF, 0, MUSIC_VOLUME_KEEP, MUSIC_FX_NONE, 0);
            }
            if (swell && cell->effect == MUSIC_FX_NONE) {
                cell->effect = MUSIC_FX_VOLUME;
                cell->param = 1;
            }
        }
    }
}

/// The lead's line at row @p at of the section, reaching back into the section before.
static uint8_t LeadAt(const MusicComposer *c, int32_t at) {
    if (at >= 0) {
        return c->lead[at];
    }
    return at >= -(int32_t)MUSIC_ECHO_MAX ? c->tail[MUSIC_ECHO_MAX + at] : 0u;
}

static void Lead(const MusicComposer *c, MusicCell bar[][MUSIC_CHANNELS], uint32_t ch,
                 MusicRole role, MusicPart part) {
    const MusicScore *s = MusicScoreOf(c->track);
    const bool on = role == MUSIC_ROLE_LEAD || role == MUSIC_ROLE_ECHO ||
                    (role == MUSIC_ROLE_DOUBLE && part == MUSIC_PART_CLIMAX) ||
                    (role == MUSIC_ROLE_HARMONY &&
                     (part == MUSIC_PART_B || part == MUSIC_PART_CLIMAX));
    if (!on) {
        return;
    }
    const int32_t late = role == MUSIC_ROLE_ECHO ? (int32_t)s->echo_rows : 0;
    for (uint32_t row = 0; row < MUSIC_BAR_ROWS; ++row) {
        const int32_t at = (int32_t)(c->bar * MUSIC_BAR_ROWS + row) - late;
        const uint8_t v = LeadAt(c, at);
        if (v == MUSIC_NOTE_OFF) {
            Put(&bar[row][ch], MUSIC_NOTE_OFF, 0, MUSIC_VOLUME_KEEP, MUSIC_FX_NONE, 0);
            continue;
        }
        if (v == 0u) {
            continue;
        }
        int32_t step = (int32_t)v - 1;
        uint32_t volume = (uint32_t)at % 4u == 0u ? 62u : 57u;
        uint8_t effect = MUSIC_FX_NONE;
        int32_t param = 0;
        switch (role) {
        case MUSIC_ROLE_ECHO:
            volume -= s->echo_drop;
            break;
        case MUSIC_ROLE_DOUBLE:
            volume -= 4u;
            effect = MUSIC_FX_SLIDE_UP;
            param = 1;
            break;
        case MUSIC_ROLE_HARMONY: {
            const uint32_t chord = c->chord[(uint32_t)at / MUSIC_BAR_ROWS % MUSIC_SECTION_BARS];
            step -= 2;
            if ((uint32_t)at % 8u == 0u && !ChordTone(step, chord)) {
                step = NearestChordTone(step - 1, chord, -1);
            }
            volume -= 8u;
            break;
        }
        default:
            break;
        }
        Put(&bar[row][ch], Note(s, step), c->patch[MUSIC_VOICE_LEAD], volume, effect, param);
    }
}

/// The ostinato: the pattern's digits are steps up the scale from the chord's root.
static void Arp(const MusicComposer *c, MusicCell bar[][MUSIC_CHANNELS], uint32_t ch,
                MusicPart part) {
    const MusicScore *s = MusicScoreOf(c->track);
    const char *grid = Entry(s, MUSIC_POOL_ARP, c->arp);
    if (grid == NULL || part == MUSIC_PART_B || (part == MUSIC_PART_INTRO && c->bar < 4u)) {
        return;
    }
    const int32_t root = RootFrom(s, s->range[MUSIC_RANGE_ARP][0], c->chord[c->bar]);
    for (uint32_t row = 0; row < MUSIC_BAR_ROWS; ++row) {
        const char mark = grid[row];
        if (mark >= '1' && mark <= '9') {
            Put(&bar[row][ch], Note(s, root + (mark - '1')), c->patch[MUSIC_VOICE_ARP],
                row % 4u == 0u ? 52u : 46u, MUSIC_FX_NONE, 0);
        } else if (mark == '.') {
            Put(&bar[row][ch], MUSIC_NOTE_OFF, 0, MUSIC_VOLUME_KEEP, MUSIC_FX_NONE, 0);
        }
    }
}

/// The cymbal on the first beat of every fourth bar of the A and B sections and the climax,
/// fading by volume over the bar; a riser climbing through the last bar of an intro or a
/// breakdown; the key let up on every other bar.
static void Crash(const MusicComposer *c, MusicCell bar[][MUSIC_CHANNELS], uint32_t ch,
                  MusicPart part, bool last) {
    const bool riser = last && (part == MUSIC_PART_INTRO || part == MUSIC_PART_BREAK);
    const bool crash = c->bar % 4u == 0u && part != MUSIC_PART_INTRO && part != MUSIC_PART_BREAK;
    const MusicPatch *patch = PatchOf(c->patch[riser ? MUSIC_VOICE_RISER : MUSIC_VOICE_CRASH]);
    if (!(riser || crash) || patch == NULL) {
        Put(&bar[0][ch], MUSIC_NOTE_OFF, 0, MUSIC_VOLUME_KEEP, MUSIC_FX_NONE, 0);
        return;
    }
    Put(&bar[0][ch], patch->note, c->patch[riser ? MUSIC_VOICE_RISER : MUSIC_VOICE_CRASH],
        riser ? 44u : 62u, riser ? MUSIC_FX_SLIDE_UP : MUSIC_FX_VOLUME, riser ? 2 : -1);
    for (uint32_t row = 1; row < MUSIC_BAR_ROWS; ++row) {
        Put(&bar[row][ch], 0, 0, MUSIC_VOLUME_KEEP, riser ? MUSIC_FX_SLIDE_UP : MUSIC_FX_VOLUME,
            riser ? 2 : -1);
    }
}

void MusicComposeBar(MusicComposer *c, MusicCell bar[MUSIC_BAR_ROWS][MUSIC_CHANNELS]) {
    const MusicScore *s = MusicScoreOf(c->track);
    for (uint32_t row = 0; row < MUSIC_BAR_ROWS; ++row) {
        for (uint32_t ch = 0; ch < MUSIC_CHANNELS; ++ch) {
            Put(&bar[row][ch], 0, 0, MUSIC_VOLUME_KEEP, MUSIC_FX_NONE, 0);
        }
    }
    const MusicPart part = (MusicPart)c->form[c->position];
    const bool last = c->bar + 1u == MUSIC_SECTION_BARS;
    const uint32_t next = !last ? c->chord[c->bar + 1u]
                          : c->position + 1u < c->form_count
                              ? FirstChord(c, (MusicPart)c->form[c->position + 1u])
                              : FirstChord(c, MUSIC_PART_A);
    uint32_t pads[3];
    uint32_t pad_count = 0;
    for (uint32_t ch = 0; ch < MUSIC_CHANNELS; ++ch) {
        switch ((MusicRole)s->role[ch]) {
        case MUSIC_ROLE_DRUMS:
            Drums(c, bar, ch, part, last);
            break;
        case MUSIC_ROLE_BASS:
            Bass(c, bar, ch, part, next);
            break;
        case MUSIC_ROLE_PAD:
            if (pad_count < 3u) {
                pads[pad_count++] = ch;
            }
            break;
        case MUSIC_ROLE_LEAD:
        case MUSIC_ROLE_ECHO:
        case MUSIC_ROLE_DOUBLE:
        case MUSIC_ROLE_HARMONY:
            Lead(c, bar, ch, (MusicRole)s->role[ch], part);
            break;
        case MUSIC_ROLE_ARP:
            Arp(c, bar, ch, part);
            break;
        case MUSIC_ROLE_CRASH:
            Crash(c, bar, ch, part, last);
            break;
        default:
            break;
        }
    }
    Pads(c, bar, pads, pad_count, part);
    if (++c->bar < MUSIC_SECTION_BARS) {
        return;
    }
    c->bar = 0;
    if (++c->position < c->form_count) {
        EnterSection(c);
        return;
    }
    ++c->pass;
    Pass(c, Mix(c->seed + c->pass), false);
}
