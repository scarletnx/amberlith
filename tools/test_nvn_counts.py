# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""The counts docs/NvnLayer.md and docs/families/Nvn.md spell out of the layer's lists are the lists' own."""

import importlib.util
import os
import re
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SPEC = importlib.util.spec_from_file_location("nvn_coverage", os.path.join(HERE, "nvn-coverage.py"))
tool = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(tool)

UNITS = ("one two three four five six seven eight nine ten eleven twelve thirteen fourteen fifteen "
         "sixteen seventeen eighteen nineteen").split()
TENS = "twenty thirty forty fifty sixty seventy eighty ninety".split()
# NvnProcs carries two functions no list has: nvnDeviceGetInteger and nvnDeviceGetProcAddress.
BY_HAND = 2


def number(text):
    """A count as these docs spell it: "sixteen", "a hundred and seven", "five hundred and twenty seven"."""
    total = 0
    for word in text.lower().split():
        if word == "and":
            continue
        if word == "a":
            total += 1
        elif word == "hundred":
            total *= 100
        elif word in UNITS:
            total += UNITS.index(word) + 1
        elif word in TENS:
            total += (TENS.index(word) + 2) * 10
        else:
            raise ValueError("not a number word: %r in %r" % (word, text))
    return total


def spelled(path, pattern):
    with open(os.path.join(ROOT, path), encoding="utf-8") as file:
        found = re.findall(pattern, file.read())
    if len(found) != 1:
        raise AssertionError("%s: %d matches of %r, want one" % (path, len(found), pattern))
    return number(found[0])


def listed(api):
    """How many names the layer's lists give the build of @p api."""
    with open(os.path.join(ROOT, tool.PROCS), encoding="utf-8") as file:
        lists = tool.read_lists(file.read())
    return sum(1 for apis in lists.values() if api in apis)


class Numbers(unittest.TestCase):
    def test_a_number_as_the_docs_spell_it(self):
        self.assertEqual(number("sixteen"), 16)
        self.assertEqual(number("a hundred and seven"), 107)
        self.assertEqual(number("Five hundred and twenty seven"), 527)
        self.assertEqual(number("two hundred\nand four"), 204)
        with self.assertRaises(ValueError):
            number("five hundred or so")


class Counts(unittest.TestCase):
    def test_the_layer_s_doc_counts_the_api_55_build_s_functions(self):
        self.assertEqual(spelled("docs/NvnLayer.md", r"(?m)^([A-Z][a-z ]+) functions, each recording the deko3d"),
                         listed(55) + BY_HAND)

    def test_the_family_s_doc_counts_what_the_lookup_gives(self):
        self.assertEqual(spelled("docs/families/Nvn.md",
                                 r"the lookup gives every function the layer has, ([a-z\s]+?)\."),
                         listed(55) + BY_HAND)

    def test_the_layer_s_doc_counts_the_names_the_api_231_build_has_more(self):
        self.assertEqual(spelled("docs/NvnLayer.md", r"the API 231 build has ([a-z ]+?) names more \("),
                         listed(231) - listed(55))


if __name__ == "__main__":
    unittest.main()
