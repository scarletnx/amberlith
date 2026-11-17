#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes source/wimsypicture.c: Wimsy as Amberlith (static/wimsy-amberlith.svg) as the start screen draws her, a
picture of SIDE by SIDE pixels in a palette of at most COLOURS red, green, blue and alpha quadruples.

    tools/wimsy-picture.py [--svg static/wimsy-amberlith.svg] [--out source/wimsypicture.c]

rsvg-convert draws the drawing at SIDE pixels a side and ImageMagick (magick) takes it down to the palette with no
dithering, so an edge keeps its alpha and a flat colour stays flat. Both are needed only to write the file again,
which is committed, as icon.jpg is: `make wimsy-picture`. The program draws it over the page with each pixel's alpha.
"""

import argparse
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SIDE = 128
COLOURS = 64


class Refused(Exception):
    pass


def pixels(svg):
    """The drawing at SIDE by SIDE, in at most COLOURS colours: a list of (r, g, b, a), row by row."""
    with tempfile.TemporaryDirectory() as folder:
        png = os.path.join(folder, "wimsy.png")
        raw = os.path.join(folder, "wimsy.rgba")
        try:
            subprocess.run(["rsvg-convert", "-w", str(SIDE), "-h", str(SIDE), svg, "-o", png], check=True)
            subprocess.run(["magick", png, "-dither", "None", "-colors", str(COLOURS), "rgba:" + raw], check=True)
        except (OSError, subprocess.CalledProcessError) as error:
            raise Refused("drawing %s with rsvg-convert and magick: %s" % (svg, error))
        with open(raw, "rb") as file:
            data = file.read()
    if len(data) != SIDE * SIDE * 4:
        raise Refused("magick gave %d bytes, not the %d of %d by %d pixels" % (len(data), SIDE * SIDE * 4, SIDE, SIDE))
    return [tuple(data[i : i + 4]) for i in range(0, len(data), 4)]


def source(colours):
    """The C file for @p colours: the palette in order of first use, then each pixel's place in it."""
    palette = []
    places = {}
    indices = []
    for colour in colours:
        # Every fully transparent pixel is one colour, whatever its red, green and blue.
        colour = colour if colour[3] != 0 else (0, 0, 0, 0)
        if colour not in places:
            places[colour] = len(palette)
            palette.append(colour)
        indices.append(places[colour])
    if len(palette) > COLOURS + 1:
        raise Refused("the picture has %d colours, more than the %d of its palette" % (len(palette), COLOURS + 1))
    out = [
        "// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors",
        "// SPDX-License-Identifier: GPL-3.0-or-later",
        "//",
        "// Written by tools/wimsy-picture.py from static/wimsy-amberlith.svg (Wimsy is Liz's, ScarletNX's",
        "// dist/wimsy.svg, GPL-3.0-or-later); `make wimsy-picture` writes it again.",
        "",
        '#include "wimsypicture.h"',
        "",
        "const uint32_t WIMSY_PICTURE_COLOURS = %du;" % len(palette),
        "",
        "const uint8_t WIMSY_PICTURE_PALETTE[][4] = {",
    ]
    for r, g, b, a in palette:
        out.append("    { 0x%02x, 0x%02x, 0x%02x, 0x%02x }," % (r, g, b, a))
    out.append("};")
    out.append("")
    out.append("const uint8_t WIMSY_PICTURE[WIMSY_PICTURE_SIDE * WIMSY_PICTURE_SIDE] = {")
    for row in range(SIDE):
        line = indices[row * SIDE : (row + 1) * SIDE]
        for start in range(0, SIDE, 16):
            out.append("    " + " ".join("%d," % i for i in line[start : start + 16]))
    out.append("};")
    return "\n".join(out) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--svg", default=os.path.join(ROOT, "static", "wimsy-amberlith.svg"))
    parser.add_argument("--out", default=os.path.join(ROOT, "source", "wimsypicture.c"))
    args = parser.parse_args(argv)
    try:
        text = source(pixels(args.svg))
    except Refused as error:
        print("wimsy-picture.py: %s" % error, file=sys.stderr)
        return 2
    with open(args.out, "w", encoding="utf-8") as file:
        file.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
