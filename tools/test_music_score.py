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

# A small track that keeps every rule, to break one rule at a time; the score holds four of them.
SMALL = """demo a demo
key C ionian
tempo 120 6
gain 32
channels drums bass pad pad pad lead echo arp crash
voices kick=kick snare=snare hat=hat crash=crash riser=riser bass=sub pad=strings lead=horn,flute arp=bell
range bass C2 C3
range pad C3 C4
range lead C4 C5
range arp C3 C4
echo 3 8
glide 12
form IAB
home 1 4 5 1
lift 4 5 6 5
drop 6 4 1 5
groove K.h.S.h.K.K.S.h.
breakbeat h.h.H.h.h.h.H.h.
bass R-R-O-R-R-R-O-O-
breakbass R---------------
pad x---------------
arp 5-4-5-3-5-4-5-3-
rhythm x---x---x---x---x-------x---....
rhythm x-x-x---x---x---x-x-x---x-------
"""


def score(small=SMALL):
    return "".join(f"track {name}\n{small}\n" for name in music_score.TRACKS)


def refused(text):
    try:
        music_score.check(music_score.parse(text))
    except music_score.ScoreError as e:
        return str(e)
    return None


class MusicScoreTest(unittest.TestCase):
    def test_generated_file_is_current(self):
        made = music_score.generate(music_score.check(music_score.parse(SCORE)))
        have = open(os.path.join(ROOT, "source", "musicscore.c"), encoding="utf-8").read()
        self.assertEqual(have, made, "source/musicscore.c is not what tools/music-score.py makes of the score")

    def test_the_four_tracks_in_order(self):
        tracks = music_score.check(music_score.parse(SCORE))
        self.assertEqual([t["name"] for t in tracks], ["menu", "wimsy", "fractal", "fluid"])
        for t in tracks:
            self.assertEqual(len(t["roles"]), 9)
            self.assertGreaterEqual(len(t["pools"]["form"]), 2, f"{t['name']}: a seed needs forms to pick from")
            self.assertGreaterEqual(len(t["pools"]["rhythm"]), 2)

    def test_small_track_passes(self):
        self.assertIsNone(refused(score()))

    def test_note_and_key(self):
        tracks = music_score.check(music_score.parse(score()))
        self.assertEqual(tracks[0]["tonic"], 0)
        self.assertEqual(tracks[0]["range"]["bass"], (36, 48))
        self.assertEqual(music_score.note("A4", "x"), 69)
        self.assertEqual(music_score.note("F#2", "x"), 42)

    def test_refuses_a_diminished_degree(self):
        why = refused(score(SMALL.replace("home 1 4 5 1", "home 1 4 7 1")))
        self.assertIn("degree 7 is a diminished triad in ionian", why)

    def test_refuses_a_pattern_of_the_wrong_length(self):
        why = refused(score(SMALL.replace("groove K.h.S.h.K.K.S.h.", "groove K.h.S.h.K.K.S.h")))
        self.assertIn("is 15 long, where 16", why)

    def test_refuses_a_mark_the_pattern_has_not(self):
        why = refused(score(SMALL.replace("bass R-R-O-R-R-R-O-O-", "bass R-R-O-R-R-R-O-X-")))
        self.assertIn("has 'X'", why)

    def test_refuses_too_many_of_a_part(self):
        why = refused(score(SMALL.replace("drums bass pad pad pad", "drums bass pad pad bass")))
        self.assertIn("2 channels play bass, at most 1", why)

    def test_refuses_an_echo_with_no_lead(self):
        why = refused(score(SMALL.replace("pad lead echo", "pad pad echo").replace("pad pad pad pad", "pad pad pad bass")))
        self.assertIsNotNone(why)

    def test_refuses_a_form_without_an_a_or_with_a_late_intro(self):
        self.assertIn("has no A section", refused(score(SMALL.replace("form IAB", "form IBR"))))
        self.assertIn("an intro past its start", refused(score(SMALL.replace("form IAB", "form AIB"))))

    def test_refuses_a_range_under_an_octave(self):
        why = refused(score(SMALL.replace("range lead C4 C5", "range lead C4 G4")))
        self.assertIn("less than the octave", why)

    def test_refuses_a_missing_voice_and_an_unknown_field(self):
        self.assertIn("no patch for arp", refused(score(SMALL.replace(" arp=bell", ""))))
        self.assertIn("is no field", refused(score(SMALL.replace("glide 12", "glide 12\nswing 3"))))

    def test_refuses_numbers_out_of_range(self):
        self.assertIn("tempo 300 is outside 60 to 200", refused(score(SMALL.replace("tempo 120 6", "tempo 300 6"))))
        self.assertIn("echo rows 9 is outside 1 to 8", refused(score(SMALL.replace("echo 3 8", "echo 9 8"))))

    def test_refuses_tracks_out_of_order(self):
        text = score().replace("track menu", "track first").replace("track wimsy", "track menu")
        self.assertIn("in that order", refused(text))


if __name__ == "__main__":
    unittest.main()
