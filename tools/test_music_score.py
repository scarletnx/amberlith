# SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""The demos' scores: source/musicscore.c is what tools/music-score.py makes of source/musicscore.txt, and the
generator refuses a score that breaks a rule, saying where and with what."""

import importlib.util
import os
import unittest

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SPEC = importlib.util.spec_from_file_location("music_score", os.path.join(ROOT, "tools", "music-score.py"))
music_score = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(music_score)

SCORE = open(os.path.join(ROOT, "source", "musicscore.txt"), encoding="utf-8").read()

# A small track that keeps every rule, to break one rule at a time.
SMALL = """pattern bass b R---------------
pattern drums d K...............
pattern drums dfill K.......S.......
pattern hats d h.h.h.h.h.h.h.h.
pattern comp c X---------------
pattern arp a ................
track t
demo a demo
key C ionian
tempo 120
steps 16
swing 0
gain 32
voices bass=slap chords=epiano lead=brass counter=flute arp=pluck
form intro A
chords intro | C |
chords A | C | F |
lead A | C5:8 E5:8 | F5:16 |
parts intro bass=b drums=d chords=c arp=a
parts A bass=b drums=d fill chords=c arp=a
"""


class Generated(unittest.TestCase):
    def test_the_tables_are_what_the_score_makes(self):
        want = music_score.generate(*music_score.parse(SCORE))
        have = open(os.path.join(ROOT, "source", "musicscore.c"), encoding="utf-8").read()
        self.assertEqual(have, want, "source/musicscore.c is stale: run tools/music-score.py")

    def test_four_tracks_in_the_players_order(self):
        tracks, _ = music_score.parse(SCORE)
        self.assertEqual([t["name"] for t in tracks], ["menu", "wimsy", "fractal", "fluid"])

    def test_notes_and_chords(self):
        self.assertEqual(music_score.note("A4:4", "x"), (69, 4, 0))
        self.assertEqual(music_score.note("Bb4:2~", "x"), (70, 2, 1))
        self.assertEqual(music_score.note("C#5:1", "x"), (73, 1, 0))
        self.assertEqual(music_score.note("r:6", "x"), (0, 6, 0))
        self.assertEqual(music_score.chord("Bb", "x"), (10, 0))
        self.assertEqual(music_score.chord("F#m", "x"), (6, 1))

    def test_a_small_score_parses(self):
        tracks, _ = music_score.parse(SMALL)
        self.assertEqual(tracks[0]["sections"]["A"]["lead"], [[(72, 8, 0), (76, 8, 0)], [(77, 16, 0)]])


class Refused(unittest.TestCase):
    def refused(self, old, new, says):
        with self.assertRaises(music_score.ScoreError) as caught:
            music_score.parse(SMALL.replace(old, new))
        self.assertIn(says, str(caught.exception))

    def test_a_short_bar(self):
        self.refused("| F5:16 |", "| F5:12 |", "lead bar 2 lasts 12 steps, where 16")

    def test_a_note_that_is_none(self):
        self.refused("E5:8", "H5:8", "'H5:8' is no note")

    def test_a_chord_that_is_none(self):
        self.refused("| C | F |", "| C | Fmaj7 |", "'Fmaj7' is no chord")

    def test_a_missing_fill(self):
        self.refused("pattern drums dfill K.......S.......\n", "", "no drums pattern dfill")

    def test_a_pattern_of_the_wrong_length(self):
        self.refused("pattern comp c X---------------", "pattern comp c X-------", "is 8 steps, the bar 16")

    def test_a_grid_mark_its_lane_lacks(self):
        self.refused("pattern hats d h.h.h.h.h.h.h.h.", "pattern hats d h.h.K.h.h.h.h.h.", "holds 'K'")

    def test_a_form_that_plays_an_unwritten_section(self):
        self.refused("form intro A", "form intro A B", "the form plays B")

    def test_a_mode_that_is_none(self):
        self.refused("key C ionian", "key C major", "its mode none of")

    def test_a_tempo_out_of_range(self):
        self.refused("tempo 120", "tempo 400", "tempo 400, where 40 to 240")


if __name__ == "__main__":
    unittest.main()
