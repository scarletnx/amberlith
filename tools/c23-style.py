#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""The C23 frontends' sources held to docs/CodeStyle.md where a grep can tell: the spellings C23 has a form for, and a
local zeroed with memset. It reads the code outside comments and strings, prints each line it refuses with the form
the page asks for, and exits 1 when there is one.

    python3 tools/c23-style.py [FILE...]   (default: every .c and .h of the C23 frontends' folders)"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# the frontends that build as C23 (docs/CodeStyle.md, "Which code"); a folder not made yet is skipped
C23_DIRS = ("vita", "3ds", "nds")
RULES = (
    (re.compile(r"\bNULL\b"), "NULL: nullptr"),
    (re.compile(r"\b__attribute__\b"), "__attribute__: a [[...]] attribute, or alignas(N)"),
    (re.compile(r"\b_Static_assert\b"), "_Static_assert: static_assert"),
    (re.compile(r"\b_Noreturn\b"), "_Noreturn: [[noreturn]]"),
    (re.compile(r"\b__(atomic|sync)_\w+"), "a GCC atomic builtin: an _Atomic object through atomic_*_explicit"),
    (re.compile(r"^\s*\(void\)\s*\w+\s*;"), "(void)name;: [[maybe_unused]] on the parameter"),
    (re.compile(r"\bmemset\s*\(\s*&\s*\w+\s*,\s*0\s*,\s*sizeof"), "memset of a local to 0: = {0} where it is declared"),
)


def code_lines(text):
    """each line of C text with its comments and string and character literals blanked, as (number, line)"""
    out, state, line = [], None, []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "\n":
            out.append("".join(line))
            line = []
            if state == "line":
                state = None
            i += 1
            continue
        if state is None:
            if text.startswith("//", i):
                state = "line"
            elif text.startswith("/*", i):
                state = "block"
                i += 1
            elif c in "\"'":
                state = c
                line.append(c)
            else:
                line.append(c)
        elif state == "block":
            if text.startswith("*/", i):
                state = None
                i += 1
        elif state in "\"'":
            if c == "\\":
                i += 1
            elif c == state:
                state = None
                line.append(c)
        i += 1
    out.append("".join(line))
    return list(enumerate(out, 1))


def check(path, text):
    """what path's text breaks, a line each: path:line: the spelling and the form the page asks for"""
    found = []
    for number, line in code_lines(text):
        for rule, what in RULES:
            if rule.search(line):
                found.append(f"{path}:{number}: {what}")
    return found


def default_files():
    files = []
    for folder in C23_DIRS:
        top = os.path.join(ROOT, folder)
        for base, _, names in os.walk(top):
            files += [os.path.join(base, name) for name in sorted(names) if name.endswith((".c", ".h"))]
    return files


def main(argv):
    files = argv or default_files()
    found = []
    for path in files:
        with open(path, encoding="utf-8") as f:
            found += check(os.path.relpath(path, ROOT), f.read())
    for line in found:
        print(line)
    print(f"c23-style: {len(files)} files, {len(found)} lines to write the C23 way (docs/CodeStyle.md)")
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
