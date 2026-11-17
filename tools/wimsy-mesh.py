#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Builds source/wimsymesh.c: Wimsy, ScarletNX's mascot, as a mesh, for the game family's levels.

static/wimsy.svg is her drawing (ScarletNX's dist/wimsy.svg, by Liz, GPL-3.0-or-later): five leaves, each a path of
three circular arcs filled with one colour and stroked white, and four circles, her eyes. Each leaf becomes a slab:
its path flattened to SEGMENTS chords an arc, a front face set in by half the stroke, a white bevel from there down
to the slab's edge, half the stroke outside the path, a white wall and a back face in the leaf's colour darkened. The leaves are stacked in the order
the drawing paints them, a gap between two, so that seen from the front the nearer leaf is the one painted later.
Each eye is a short cylinder standing on the last leaf, the white ones taller than the black.

Every number written is a whole one: a position in 1/8192 of the model's half width (the drawing's 512 units are
the model's -1 to 1, y up), a normal in 1/127, a colour by its place in the palette. Integer rounding of doubles
that are far from a half, so every machine writes the same file.

    tools/wimsy-mesh.py [--svg static/wimsy.svg] [--out source/wimsymesh.c]

The triangles are in painting order: leaf by leaf as the drawing has them, each leaf's back, wall, bevel and front,
then the eyes. Drawn in that order with no depth test, the ones that face the eye give the drawing.

Her other drawings (wimsy-dist's, the GPL-3.0-or-later ones: Fuddles', Amberlith's, Accord's, which is its Sepia
theme's too, the same file, and the Default theme's) are the same five leaves and eyes in other colours, with
details inside the leaves that the mesh leaves out. Each is a palette for the one mesh: a leaf's colour is its path's fill, the mean of its stops where the
fill is a gradient; the eyes' black is the first pupil's. The Default theme's drawing is 32 by 32 squares in six
colours, which name the palette by themselves: the two teals for the first leaf and the head, the light green for
the second leaf and the dark one for the two below.
"""

import argparse
import math
import os
import re
import xml.etree.ElementTree as ET

SVG_NS = "{http://www.w3.org/2000/svg}"
# Chords an arc is flattened to, and sides an eye's circle has.
SEGMENTS = 12
EYE_SIDES = 20
# One model unit is half the drawing's side.
HALF = 256.0
SCALE = 8192
# A leaf's thickness, the step from one leaf's front to the next one's, and how far a bevel drops.
THICKNESS = 0.05
LAYER = 0.06
BEVEL = 0.012
# How far the eyes stand out of the last leaf's front: the black ones, then the white.
EYE_BLACK = 0.010
EYE_WHITE = 0.018
# A back face's colour is its leaf's times this, in 1/256.
BACK_SHADE = 154
MITER_LIMIT = 3.0
WHITE = (255, 255, 255)
# The drawings, the first the one the mesh is made of: a name and its file in static/.
VARIANTS = (("wimsy", "wimsy.svg"), ("fuddles", "wimsy-fuddles.svg"), ("sunset", "wimsy-sunset.svg"),
            ("accord", "wimsy-accord.svg"), ("default", "wimsy-default.svg"))
# Where each leaf's path starts in every drawing but the Default theme's, and a pupil's radius.
LEAF_STARTS = ((192.28931, 12.793038), (283.32464, 18.471579), (3.1326032, 310.41929), (80.650839, 503.85583),
               (508.04553, 190.77006))
PUPIL_RADIUS = 23.114985
# The Default theme's six colours, and the leaves' and the pupils' among them.
DEFAULT_COLOURS = {"#000000", "#00aa00", "#00aaaa", "#55ff55", "#55ffff", "#ffffff"}
DEFAULT_LEAVES = ("#00aaaa", "#55ff55", "#00aa00", "#00aa00", "#55ffff")
# The palette: a leaf's front and back, two each, then the white of the rims and the pupils' black.
COLOUR_WHITE = 10
COLOUR_BLACK = 11
COLOURS = 12


def style_of(element):
    return dict(part.split(":", 1) for part in element.get("style", "").split(";") if ":" in part)


def colour_of(text):
    match = re.fullmatch(r"#([0-9a-fA-F]{6})", text.strip())
    if not match:
        raise ValueError("not a colour of six hex digits: %r" % text)
    value = int(match.group(1), 16)
    return (value >> 16, value >> 8 & 0xFF, value & 0xFF)


def arc_points(start, radius, large, sweep, end, segments):
    """The chords' ends of a circular arc from @p start to @p end, the end included, the start not: the
    specification's conversion from the endpoints to a centre (SVG 1.1, F.6.5) for equal radii and no rotation."""
    (x1, y1), (x2, y2) = start, end
    hx, hy = (x1 - x2) / 2.0, (y1 - y2) / 2.0
    lam = (hx * hx + hy * hy) / (radius * radius)
    if lam > 1.0:
        radius *= math.sqrt(lam)
    factor = math.sqrt(max(0.0, radius * radius / (hx * hx + hy * hy) - 1.0))
    if large == sweep:
        factor = -factor
    cx = factor * hy + (x1 + x2) / 2.0
    cy = -factor * hx + (y1 + y2) / 2.0
    a1 = math.atan2(y1 - cy, x1 - cx)
    a2 = math.atan2(y2 - cy, x2 - cx)
    delta = a2 - a1
    if sweep and delta < 0.0:
        delta += 2.0 * math.pi
    if not sweep and delta > 0.0:
        delta -= 2.0 * math.pi
    points = [(cx + radius * math.cos(a1 + delta * i / segments), cy + radius * math.sin(a1 + delta * i / segments))
              for i in range(1, segments)]
    return points + [end]


def path_outline(d):
    """The outline of a path made of one move, circular arcs and a close, as the drawing's are."""
    tokens = re.findall(r"[A-Za-z]|-?\d+(?:\.\d+)?(?:[eE]-?\d+)?", d)
    points = []
    at = 0
    command = None
    while at < len(tokens):
        # A command's letter is written once for a run of its arguments; a move's run would be lines.
        if tokens[at].isalpha():
            command = tokens[at]
            at += 1
            if command == "Z":
                continue
        if command == "M" and not points:
            points.append((float(tokens[at]), float(tokens[at + 1])))
            at += 2
            command = None
        elif command == "A" and points:
            rx, ry, rotation, large, sweep, x, y = (float(t) for t in tokens[at:at + 7])
            if rx != ry or rotation != 0.0:
                raise ValueError("an arc that is not a circle's: %r" % (tokens[at:at + 7],))
            points.extend(arc_points(points[-1], rx, int(large), int(sweep), (x, y), SEGMENTS))
            at += 7
        else:
            raise ValueError("a path this tool does not read, at %r of %r" % (tokens[at - 1:at + 7], d))
    if math.dist(points[0], points[-1]) < 1e-6:
        points.pop()
    return points


def to_model(point):
    return ((point[0] - HALF) / HALF, (HALF - point[1]) / HALF)


def area(points):
    return sum(points[i][0] * points[(i + 1) % len(points)][1] - points[(i + 1) % len(points)][0] * points[i][1]
               for i in range(len(points))) / 2.0


def unit(vector):
    length = math.hypot(*vector)
    return tuple(c / length for c in vector)


def edge_normals(points):
    """For a counter clockwise outline, each edge's outward normal: edge i runs from point i to the next."""
    count = len(points)
    return [unit((points[(i + 1) % count][1] - points[i][1], points[i][0] - points[(i + 1) % count][0]))
            for i in range(count)]


def vertex_normals(points):
    normals = edge_normals(points)
    return [unit((normals[i - 1][0] + normals[i][0], normals[i - 1][1] + normals[i][1])) for i in range(len(points))]


def inset(points, distance):
    """Each point moved in along its corner's bisector so both its edges move in by @p distance, a sharp corner's
    move held to MITER_LIMIT of that."""
    normals = edge_normals(points)
    moved = []
    for i, (x, y) in enumerate(points):
        (ax, ay), (bx, by) = normals[i - 1], normals[i]
        scale = 1.0 / max(1.0 + ax * bx + ay * by, 2.0 / (MITER_LIMIT * MITER_LIMIT))
        mx, my = (ax + bx) * scale, (ay + by) * scale
        moved.append((x - distance * mx, y - distance * my))
    return moved


def centroid(points):
    twice = 0.0
    cx = cy = 0.0
    for i in range(len(points)):
        (x0, y0), (x1, y1) = points[i], points[(i + 1) % len(points)]
        cross = x0 * y1 - x1 * y0
        twice += cross
        cx += (x0 + x1) * cross
        cy += (y0 + y1) * cross
    return (cx / (3.0 * twice), cy / (3.0 * twice))


def star_shaped(centre, points):
    """Whether a fan from @p centre covers the counter clockwise outline with triangles that all face one way."""
    for i in range(len(points)):
        (x0, y0), (x1, y1) = points[i], points[(i + 1) % len(points)]
        if (x0 - centre[0]) * (y1 - centre[1]) - (x1 - centre[0]) * (y0 - centre[1]) <= 0.0:
            return False
    return True


def shade(rgb):
    return tuple(c * BACK_SHADE // 256 for c in rgb)


def palette_of(leaves, black):
    """The palette of a drawing whose leaves are filled @p leaves and whose pupils are @p black."""
    palette = []
    for rgb in leaves:
        palette.extend((rgb, shade(rgb)))
    return palette + [WHITE, black]


def fill_of(element):
    return (element.get("fill") or style_of(element).get("fill") or "").strip()


def paint(root, fill):
    """The colour a fill gives: its own, or the mean of the stops of the gradient it names."""
    named = re.fullmatch(r"url\(#([^)]+)\)", fill)
    if not named:
        return colour_of(fill)
    by_id = {e.get("id"): e for e in root.iter() if e.get("id")}
    gradient = by_id[named.group(1)]
    for _ in range(4):
        stops = [e for e in gradient if e.tag == SVG_NS + "stop"]
        linked = gradient.get("{http://www.w3.org/1999/xlink}href") or gradient.get("href")
        if stops or not linked:
            break
        gradient = by_id[linked.lstrip("#")]
    colours = [colour_of(e.get("stop-color") or style_of(e)["stop-color"]) for e in stops]
    if not colours:
        raise ValueError("a gradient with no stops: %r" % fill)
    return tuple(round(sum(c[k] for c in colours) / len(colours)) for k in range(3))


def variant_palette(svg_path):
    """The palette of one of her drawings: its five leaves' fills, in the order the first drawing paints them."""
    root = ET.parse(svg_path).getroot()
    fills = {fill_of(e).lower() for e in root.iter() if fill_of(e)}
    if fills == DEFAULT_COLOURS:
        return palette_of([colour_of(c) for c in DEFAULT_LEAVES], (0, 0, 0))
    paths = [e for e in root.iter(SVG_NS + "path") if e.get("d")]
    uses = [e for e in root.iter(SVG_NS + "use")]
    leaves = []
    for start in LEAF_STARTS:
        colour = None
        for path in paths:
            numbers = re.findall(r"-?\d+(?:\.\d+)?", path.get("d"))[:2]
            if len(numbers) < 2 or math.dist(start, (float(numbers[0]), float(numbers[1]))) > 0.01:
                continue
            # The leaf's own fill, or the fill of the element that draws it by its name.
            candidates = [path] + [u for u in uses if (u.get("{http://www.w3.org/1999/xlink}href") or
                                                       u.get("href") or "").lstrip("#") == path.get("id")]
            for candidate in candidates:
                if fill_of(candidate) not in ("", "none"):
                    colour = paint(root, fill_of(candidate))
                    break
            if colour:
                break
        if colour is None:
            raise ValueError("%s: no filled leaf starts at %r" % (svg_path, start))
        leaves.append(colour)
    pupils = [e for e in root.iter(SVG_NS + "circle") if abs(float(e.get("r")) - PUPIL_RADIUS) < 0.01]
    if not pupils:
        raise ValueError("%s: no pupil" % svg_path)
    return palette_of(leaves, paint(root, fill_of(pupils[0])))


class Mesh:
    def __init__(self):
        self.palettes = []
        self.vertices = []
        self.indices = []
        self.parts = []

    def vertex(self, x, y, z, normal, colour):
        nx, ny, nz = unit(normal)
        self.vertices.append((round(x * SCALE), round(y * SCALE), round(z * SCALE), round(nx * 127), round(ny * 127),
                              round(nz * 127), colour))
        return len(self.vertices) - 1

    def fan(self, centre, ring, z, up, rgb):
        """A face at @p z over the counter clockwise @p ring, facing +z when @p up and -z otherwise."""
        normal = (0.0, 0.0, 1.0 if up else -1.0)
        hub = self.vertex(centre[0], centre[1], z, normal, rgb)
        rim = [self.vertex(x, y, z, normal, rgb) for x, y in ring]
        for i in range(len(rim)):
            a, b = rim[i], rim[(i + 1) % len(rim)]
            self.indices.extend((hub, a, b) if up else (hub, b, a))

    def band(self, upper, upper_z, lower, lower_z, normals, rgb):
        """The strip between two counter clockwise rings of as many points, @p upper nearer the eye, facing out."""
        top = [self.vertex(x, y, upper_z, n, rgb) for (x, y), n in zip(upper, normals)]
        bottom = [self.vertex(x, y, lower_z, n, rgb) for (x, y), n in zip(lower, normals)]
        for i in range(len(top)):
            j = (i + 1) % len(top)
            self.indices.extend((top[i], bottom[i], bottom[j], top[i], bottom[j], top[j]))

    def part(self, name, first_index):
        self.parts.append((name, first_index, len(self.indices) - first_index))


def build(svg_path):
    root = ET.parse(svg_path).getroot()
    leaves = []
    eyes = []
    for element in root:
        style = style_of(element)
        if element.tag == SVG_NS + "path":
            outline = [to_model(p) for p in path_outline(element.get("d"))]
            if area(outline) < 0.0:
                outline.reverse()
            leaves.append((outline, colour_of(style["fill"]), float(style["stroke-width"]) / 2.0 / HALF))
        elif element.tag == SVG_NS + "circle":
            turn = re.fullmatch(r"rotate\((-?[\d.]+)\)", element.get("transform", "rotate(0)"))
            if not turn:
                raise ValueError("a transform this tool does not read: %r" % element.get("transform"))
            angle = math.radians(float(turn.group(1)))
            cx, cy = float(element.get("cx")), float(element.get("cy"))
            centre = to_model((cx * math.cos(angle) - cy * math.sin(angle), cx * math.sin(angle) + cy * math.cos(angle)))
            eyes.append((centre, float(element.get("r")) / HALF, colour_of(style["fill"])))
    if len(leaves) != 5 or len(eyes) != 4:
        raise ValueError("the drawing has %u leaves and %u eyes, not five and four" % (len(leaves), len(eyes)))

    mesh = Mesh()
    front_of_last = 0.0
    for number, (outline, rgb, rim) in enumerate(leaves):
        first = len(mesh.indices)
        front = (number - (len(leaves) - 1) / 2.0) * LAYER + THICKNESS / 2.0
        back = front - THICKNESS
        # The stroke is centred on the path: half of it outside, where the slab's edge is, half inside.
        face = inset(outline, rim)
        outline = inset(outline, -rim)
        centre = centroid(face)
        if not star_shaped(centre, face) or not star_shaped(centre, outline):
            raise ValueError("leaf %u is not covered by a fan from its middle" % number)
        normals = vertex_normals(outline)
        mesh.fan(centre, outline, back, False, number * 2 + 1)
        mesh.band(outline, front - BEVEL, outline, back, [(nx, ny, 0.0) for nx, ny in normals], COLOUR_WHITE)
        mesh.band(face, front, outline, front - BEVEL, [(nx, ny, 1.0) for nx, ny in normals], COLOUR_WHITE)
        mesh.fan(centre, face, front, True, number * 2)
        mesh.part("leaf %u" % number, first)
        front_of_last = front
    # The black ones first, as the drawing has them, so the white ones paint over them.
    for number, (centre, radius, rgb) in enumerate(eyes):
        first = len(mesh.indices)
        height = EYE_BLACK if rgb == (0, 0, 0) else EYE_WHITE
        colour = COLOUR_BLACK if rgb == (0, 0, 0) else COLOUR_WHITE
        ring = [(centre[0] + radius * math.cos(2.0 * math.pi * i / EYE_SIDES),
                 centre[1] + radius * math.sin(2.0 * math.pi * i / EYE_SIDES)) for i in range(EYE_SIDES)]
        mesh.band(ring, front_of_last + height, ring, front_of_last,
                  [(x - centre[0], y - centre[1], 0.0) for x, y in ring], colour)
        mesh.fan(centre, ring, front_of_last + height, True, colour)
        mesh.part("eye %u" % number, first)
    for x, y, z, *_ in mesh.vertices:
        if max(abs(x), abs(y), abs(z)) > 32767:
            raise ValueError("a vertex outside what sixteen bits hold")
    if len(mesh.vertices) > 65535:
        raise ValueError("%u vertices, more than a sixteen bit index reaches" % len(mesh.vertices))
    # The first drawing's palette from the fills the mesh was made of, then each other drawing's.
    mesh.palettes = [palette_of([rgb for _, rgb, _ in leaves], (0, 0, 0))]
    folder = os.path.dirname(os.path.abspath(svg_path))
    for _, name in VARIANTS[1:]:
        mesh.palettes.append(variant_palette(os.path.join(folder, name)))
    return mesh


def c_file(mesh):
    lines = [
        "// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors",
        "// SPDX-License-Identifier: GPL-3.0-or-later",
        "",
        "// Written by tools/wimsy-mesh.py from static/wimsy.svg; run it again rather than editing this file.",
        "",
        '#include "wimsymesh.h"',
        "",
        "_Static_assert(WIMSY_MESH_VERTICES == %uu, \"the header's vertex count is the table's\");" % len(mesh.vertices),
        "_Static_assert(WIMSY_MESH_INDICES == %uu, \"the header's index count is the table's\");" % len(mesh.indices),
        "_Static_assert(WIMSY_MESH_COLOURS == %uu, \"the header's palette is the table's\");" % COLOURS,
        "_Static_assert(WIMSY_MESH_VARIANTS == %uu, \"the header's drawings are the table's\");" % len(mesh.palettes),
        "_Static_assert(WIMSY_MESH_PARTS == %uu, \"the header's part count is the table's\");" % len(mesh.parts),
        "",
        "const char *const WIMSY_MESH_VARIANT[WIMSY_MESH_VARIANTS] = {",
        "    " + " ".join('"%s",' % name for name, _ in VARIANTS),
        "};",
        "",
        "const uint8_t WIMSY_MESH_PALETTE[WIMSY_MESH_VARIANTS][WIMSY_MESH_COLOURS][3] = {",
    ]
    for palette in mesh.palettes:
        lines.append("    { " + " ".join("{ %u, %u, %u }," % rgb for rgb in palette) + " },")
    lines.extend(["};", "", "const WimsyMeshPart WIMSY_MESH_PART[WIMSY_MESH_PARTS] = {"])
    lines.extend('    { "%s", %u, %u },' % part for part in mesh.parts)
    lines.extend(["};", "", "const WimsyMeshVertex WIMSY_MESH_VERTEX[WIMSY_MESH_VERTICES] = {"])
    lines.extend("    { %d, %d, %d, %d, %d, %d, %u }," % vertex for vertex in mesh.vertices)
    lines.extend(["};", "", "const uint16_t WIMSY_MESH_INDEX[WIMSY_MESH_INDICES] = {"])
    for at in range(0, len(mesh.indices), 12):
        lines.append("    " + " ".join("%u," % i for i in mesh.indices[at:at + 12]))
    lines.append("};")
    return "\n".join(lines) + "\n"


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--svg", default=os.path.join(here, "..", "static", "wimsy.svg"))
    parser.add_argument("--out", default=os.path.join(here, "..", "source", "wimsymesh.c"))
    args = parser.parse_args()
    mesh = build(args.svg)
    with open(args.out, "w") as f:
        f.write(c_file(mesh))
    print("%u vertices, %u triangles, %u parts, %u drawings of %u colours" %
          (len(mesh.vertices), len(mesh.indices) // 3, len(mesh.parts), len(mesh.palettes), COLOURS))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
