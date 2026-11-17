#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes static/icon.svg, Amberlith's cover: Wimsy as Amberlith (static/wimsy-amberlith.svg) over a dark amber
sun cut by bands, on the dark ground, with the program's name.

    tools/cover.py [--wimsy static/wimsy-amberlith.svg] [--out static/icon.svg] [--check]

The drawing is taken whole but for its XML declaration, its comments before the svg element, its metadata and the
svg element itself, and set into the cover's 256 units at a 0.4 scale. --check writes nothing and exits 1 when the
cover is not what the drawing gives. `make icon` turns the cover into icon.jpg.
"""

import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

HEAD = """<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" viewBox="0 0 256 256">
  <!-- Amberlith's cover: Wimsy as Amberlith (Wimsy is Liz's, ScarletNX's dist/wimsy.svg, GPL-3.0-or-later), in amber
       with a test probe, over a dark amber sun cut by bands on the dark ground, with the program's name. Written by
       tools/cover.py from static/wimsy-amberlith.svg; the source of icon.jpg: make icon. -->
  <defs>
    <linearGradient id="sun" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#c86a10"/>
      <stop offset="0.45" stop-color="#7a3a06"/>
      <stop offset="0.9" stop-color="#3a1c04"/>
    </linearGradient>
    <mask id="bands">
      <rect width="256" height="256" fill="#fff"/>
      <rect x="0" y="103" width="256" height="6" fill="#000"/>
      <rect x="0" y="128" width="256" height="9" fill="#000"/>
      <rect x="0" y="153" width="256" height="12" fill="#000"/>
      <rect x="0" y="178" width="256" height="15" fill="#000"/>
    </mask>
  </defs>
  <rect width="256" height="256" fill="#1b1b1f"/>
  <circle cx="128" cy="112" r="90" fill="url(#sun)" mask="url(#bands)"/>
  <g fill="none" transform="translate(24,2) scale(0.4)">
"""
TAIL = """</g>
  <text x="128" y="238" text-anchor="middle" font-family="DejaVu Sans, Liberation Sans, sans-serif" font-weight="bold" font-size="30" letter-spacing="1" fill="#f4e6c8">Amberlith</text>
</svg>
"""


class Refused(Exception):
    pass


def body_of(text, path):
    """The drawing inside its svg element, its metadata left out."""
    start = re.search(r"<svg\b[^>]*>", text)
    end = text.rfind("</svg>")
    if start is None or end < 0:
        raise Refused("%s: no svg element" % path)
    body = text[start.end() : end]
    body = re.sub(r"\s*<metadata>.*?</metadata>", "", body, flags=re.S)
    return body.strip("\n") + "\n"


def cover(wimsy_path):
    with open(wimsy_path, encoding="utf-8") as file:
        return HEAD + body_of(file.read(), wimsy_path) + TAIL


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--wimsy", default=os.path.join(ROOT, "static", "wimsy-amberlith.svg"))
    parser.add_argument("--out", default=os.path.join(ROOT, "static", "icon.svg"))
    parser.add_argument("--check", action="store_true", help="write nothing; exit 1 when the cover is stale")
    args = parser.parse_args(argv)
    try:
        text = cover(args.wimsy)
    except (Refused, OSError) as error:
        print("cover.py: %s" % error, file=sys.stderr)
        return 2
    if args.check:
        with open(args.out, encoding="utf-8") as file:
            if file.read() != text:
                print("cover.py: %s is not what %s gives; run tools/cover.py" % (args.out, args.wimsy))
                return 1
        return 0
    with open(args.out, "w", encoding="utf-8") as file:
        file.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
