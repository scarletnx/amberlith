# SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""tools/mod-lacked.py reads a driver, the names it lacks and the cases each took from the notes of the mod's
report, and refuses a report whose notes and lines do not agree."""

import importlib.util
import json
import os
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SPEC = importlib.util.spec_from_file_location("mod_lacked", os.path.join(HERE, "mod-lacked.py"))
mod_lacked = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(mod_lacked)


def note(text):
    return json.dumps({"type": "note", "text": text})


def case(name, result="pass", reason=None, detail=""):
    record = {"type": "case", "suite": "nvn", "name": name, "tier": "none", "result": result}
    if reason:
        record["reason"] = reason
    record["detail"] = detail
    return json.dumps(record)


def unavailable(name, functions):
    return case(name, "skip", "unavailable", "the game's driver has no %s, which the case called: nothing was done "
                "and nought came back, so the verdict the case came to judged no answer (pass)" % functions)


HOST = [
    note("host: the NVN driver is the game's own, in a module of SDK 12.3.9 and build id 0123abcd"),
    note("host: the game's driver has no function for 4 of the 523 names this build looks up (1 of 2): "
         "nvnA, nvnB"),
    note("host: the game's driver has no function for 4 of the 523 names this build looks up (2 of 2): "
         "nvnC, nvnD"),
]
CASES = [
    case("a case that asked the driver"),
    unavailable("a queue builder gives back; what it was set", "nvnB"),
    unavailable("a state \"quoted\" gives back its switches", "nvnB, nvnD"),
    case("the window", "skip", "unavailable", "a program loaded into a game has no native window of its own"),
]
TOOK = [
    note('host: nvnB, which the game\'s driver has not, made 2 cases unavailable: '
         '"a queue builder gives back; what it was set"'),
    note('host: nvnB, more of the cases it made unavailable: "a state \\"quoted\\" gives back its switches"'
         .replace('\\"', '"')),
    note('host: nvnD, which the game\'s driver has not, made 1 case unavailable: '
         '"a state "quoted" gives back its switches"'),
    note("host: the game's NVN driver answers API 55.8"),
]
REPORT = "\n".join(HOST + CASES + TOOK) + "\n"


class Driver(unittest.TestCase):
    def test_the_driver_its_lacked_names_and_what_each_took(self):
        d = mod_lacked.driver(REPORT, "a report")
        self.assertEqual((d["api"], d["sdk"], d["build"], d["names"]), ("55.8", "12.3.9", "0123abcd", 523))
        self.assertEqual(d["lacked"], ["nvnA", "nvnB", "nvnC", "nvnD"])
        self.assertEqual(d["took"], {
            "nvnB": ["a queue builder gives back; what it was set", 'a state "quoted" gives back its switches'],
            "nvnD": ['a state "quoted" gives back its switches']})
        self.assertEqual(d["uncalled"], ["nvnA", "nvnC"])
        # a case unavailable for another reason is none of a lacked function's
        self.assertEqual(len(d["cases"]), 2)

    def test_a_run_of_two_launches_reads_as_one(self):
        again = "\n".join(HOST + CASES[:2] + ['{"type":"case","suite":"nvn","na'] + HOST + CASES + TOOK) + "\n"
        d = mod_lacked.driver(again, "a report")
        self.assertEqual(d["lacked"], ["nvnA", "nvnB", "nvnC", "nvnD"])
        self.assertEqual(len(d["cases"]), 2)

    def test_a_driver_with_every_name(self):
        text = "\n".join([HOST[0], note("host: the game's driver has a function for each of the 523 names this "
                                       "build looks up"), CASES[0]]) + "\n"
        d = mod_lacked.driver(text, "a report")
        self.assertEqual((d["lacked"], d["took"], d["cases"], d["api"]), ([], {}, [], None))

    def test_the_lines_of_one_driver(self):
        lines = mod_lacked.described(mod_lacked.driver(REPORT, "a report"), "a report")
        self.assertEqual(lines[0], "driver: NVN API 55.8, SDK 12.3.9, build id 0123abcd (a report)")
        self.assertEqual(lines[1], "  lacks 4 of the 523 names the mod looks up; the cases call 2 of them, which "
                                   "made 2 cases unavailable")
        self.assertEqual(lines[2:5], ["  nvnB: 2 cases", "      a queue builder gives back; what it was set",
                                      '      a state "quoted" gives back its switches'])
        self.assertEqual(lines[-1], "  lacked, and called by no case (2): nvnA, nvnC")


class Refusals(unittest.TestCase):
    def refused(self, text):
        with self.assertRaises(mod_lacked.Refused) as raised:
            mod_lacked.driver(text, "a report")
        return str(raised.exception)

    def test_a_report_of_another_frontend(self):
        self.assertIn("a report has no note of a game's driver", self.refused(CASES[0] + "\n"))

    def test_a_list_that_is_not_whole_says_both_numbers(self):
        self.assertIn("the lacked names are note 1 of 2, and the report has 1 of them",
                      self.refused("\n".join(HOST[:2] + CASES[:1]) + "\n"))
        short = "\n".join([HOST[0], HOST[1].replace("(1 of 2)", "(1 of 1)")]) + "\n"
        self.assertIn("the notes say the driver lacks 4 names, and their lists hold 2", self.refused(short))

    def test_a_case_only_its_line_or_only_a_note_has(self):
        text = "\n".join(HOST + CASES + [unavailable("one more case", "nvnA")] + TOOK) + "\n"
        self.assertIn('3 cases are unavailable for a lacked function by their lines and 2 by the notes: only its '
                      'line: "one more case"', self.refused(text))
        text = "\n".join(HOST + CASES[:2] + TOOK) + "\n"
        self.assertIn('only a note: "a state "quoted" gives back its switches"', self.refused(text))

    def test_a_note_whose_count_is_not_its_lists(self):
        text = "\n".join(HOST + CASES + [TOOK[0], TOOK[2], TOOK[3]]) + "\n"
        self.assertIn("nvnB made 2 cases unavailable by its note, whose list holds 1", self.refused(text))

    def test_a_function_the_lacked_names_do_not_have(self):
        text = REPORT.replace("nvnD, which", "nvnZ, which")
        self.assertIn("nvnZ made cases unavailable, and the lacked names do not have it", self.refused(text))


class Across(unittest.TestCase):
    def test_each_name_with_the_drivers_that_lack_it_the_most_lacked_first(self):
        one = mod_lacked.driver(REPORT, "one")
        other = mod_lacked.driver("\n".join([
            note("host: the NVN driver is the game's own, in a module of SDK 4.4.1 and build id ffff"),
            note("host: the game's driver has no function for 2 of the 523 names this build looks up (1 of 1): "
                 "nvnD, nvnE"),
            note("host: the game's NVN driver answers API 53.105")]) + "\n", "other")
        self.assertEqual(mod_lacked.across([one, other]), [
            "lacked names, and the drivers that lack each:",
            "  nvnD: 55.8 (SDK 12.3.9), 53.105 (SDK 4.4.1)",
            "  nvnA: 55.8 (SDK 12.3.9)", "  nvnB: 55.8 (SDK 12.3.9)", "  nvnC: 55.8 (SDK 12.3.9)",
            "  nvnE: 53.105 (SDK 4.4.1)"])
        self.assertEqual(mod_lacked.rows([other]), [
            ("api", "sdk", "build id", "function", "case"),
            ("53.105", "4.4.1", "ffff", "nvnD", ""), ("53.105", "4.4.1", "ffff", "nvnE", "")])
        self.assertIn(("55.8", "12.3.9", "0123abcd", "nvnB", "a queue builder gives back; what it was set"),
                      mod_lacked.rows([one]))


if __name__ == "__main__":
    unittest.main()
