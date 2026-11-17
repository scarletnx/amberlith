# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""source/wimsymesh.c is what tools/wimsy-mesh.py makes of static/wimsy.svg today, and the mesh it holds is one a
machine may cull the back faces of: every part closed, wound one way, its counts the header's."""

import importlib.util
import os
import re
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SPEC = importlib.util.spec_from_file_location("wimsy_mesh", os.path.join(HERE, "wimsy-mesh.py"))
wimsy_mesh = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(wimsy_mesh)

SVG = os.path.join(HERE, "..", "static", "wimsy.svg")
SOURCE = os.path.join(HERE, "..", "source", "wimsymesh.c")
HEADER = os.path.join(HERE, "..", "source", "wimsymesh.h")


class Outline(unittest.TestCase):
    def test_an_arc_ends_where_the_path_says(self):
        points = wimsy_mesh.arc_points((0.0, 0.0), 10.0, 0, 1, (10.0, 10.0), 4)
        self.assertEqual(len(points), 4)
        self.assertEqual(points[-1], (10.0, 10.0))
        # A quarter of the circle around (10, 0) or (0, 10): every chord end is 10 from one centre.
        for centre in ((10.0, 0.0), (0.0, 10.0)):
            if all(abs(((x - centre[0]) ** 2 + (y - centre[1]) ** 2) ** 0.5 - 10.0) < 1e-9 for x, y in points):
                return
        self.fail("the chord ends are on neither circle through the arc's ends")

    def test_a_run_of_arguments_repeats_its_command(self):
        once = wimsy_mesh.path_outline("M 0,0 A 10,10 0 0 1 10,10 A 10,10 0 0 1 0,20 Z")
        run = wimsy_mesh.path_outline("M 0,0 A 10,10 0 0 1 10,10 10,10 0 0 1 0,20 Z")
        self.assertEqual(once, run)
        self.assertEqual(len(once), 1 + 2 * wimsy_mesh.SEGMENTS)

    def test_a_command_the_tool_does_not_read_is_refused(self):
        self.assertRaises(ValueError, wimsy_mesh.path_outline, "M 0,0 L 10,10 Z")
        self.assertRaises(ValueError, wimsy_mesh.path_outline, "M 0,0 A 10,5 0 0 1 10,10 Z")

    def test_an_inset_moves_every_edge_in_by_the_distance(self):
        square = [(0.0, 0.0), (4.0, 0.0), (4.0, 4.0), (0.0, 4.0)]
        self.assertEqual(wimsy_mesh.inset(square, 1.0), [(1.0, 1.0), (3.0, 1.0), (3.0, 3.0), (1.0, 3.0)])
        self.assertEqual(wimsy_mesh.inset(square, -1.0), [(-1.0, -1.0), (5.0, -1.0), (5.0, 5.0), (-1.0, 5.0)])


class Mesh(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.mesh = wimsy_mesh.build(SVG)

    def test_the_file_is_the_tools_output(self):
        with open(SOURCE) as f:
            self.assertEqual(wimsy_mesh.c_file(self.mesh), f.read())

    def test_the_header_has_the_counts(self):
        with open(HEADER) as f:
            header = f.read()
        for name, count in (("VERTICES", len(self.mesh.vertices)), ("INDICES", len(self.mesh.indices)),
                            ("COLOURS", wimsy_mesh.COLOURS), ("PARTS", len(self.mesh.parts)),
                            ("VARIANTS", len(self.mesh.palettes))):
            self.assertEqual(int(re.search(r"#define WIMSY_MESH_%s (\d+)u" % name, header).group(1)), count, name)
        self.assertEqual(int(re.search(r"#define WIMSY_MESH_SCALE (\d+)", header).group(1)), wimsy_mesh.SCALE)

    def test_five_leaves_then_four_eyes(self):
        self.assertEqual([name for name, _, _ in self.mesh.parts],
                         ["leaf %u" % i for i in range(5)] + ["eye %u" % i for i in range(4)])
        at = 0
        for _, first, count in self.mesh.parts:
            self.assertEqual(first, at)
            self.assertEqual(count % 3, 0)
            at += count
        self.assertEqual(at, len(self.mesh.indices))

    def test_every_leaf_is_closed_and_wound_one_way(self):
        # Each edge of a closed surface wound one way is walked once in each direction. Vertices of
        # one place and different normals are one corner, so edges are keyed by position.
        for name, first, count in self.mesh.parts[:5]:
            walked = {}
            for t in range(first, first + count, 3):
                corners = [self.mesh.vertices[i][:3] for i in self.mesh.indices[t:t + 3]]
                for a, b in zip(corners, corners[1:] + corners[:1]):
                    walked[(a, b)] = walked.get((a, b), 0) + 1
            for (a, b), times in walked.items():
                self.assertEqual(times, 1, "%s: an edge walked twice one way" % name)
                self.assertEqual(walked.get((b, a)), 1, "%s: an edge with no triangle on its other side" % name)

    def test_every_triangle_faces_the_way_its_normals_point(self):
        for t in range(0, len(self.mesh.indices), 3):
            a, b, c = (self.mesh.vertices[i] for i in self.mesh.indices[t:t + 3])
            u = [b[k] - a[k] for k in range(3)]
            v = [c[k] - a[k] for k in range(3)]
            face = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
            for corner in (a, b, c):
                self.assertGreater(sum(face[k] * corner[3 + k] for k in range(3)), 0, "triangle %u" % (t // 3))
            self.assertEqual(len({a[6], b[6], c[6]}), 1, "triangle %u has two colours" % (t // 3))

    def test_a_later_leaf_is_nearer_the_eye(self):
        fronts = []
        for _, first, count in self.mesh.parts[:5]:
            zs = [self.mesh.vertices[i][2] for i in self.mesh.indices[first:first + count]]
            fronts.append((min(zs), max(zs)))
        for (_, front), (back, _) in zip(fronts, fronts[1:]):
            self.assertLess(front, back, "two leaves share a depth")

    def test_the_eyes_stand_on_the_last_leaf(self):
        head_front = max(self.mesh.vertices[i][2]
                         for i in self.mesh.indices[self.mesh.parts[4][1]:self.mesh.parts[4][1] + self.mesh.parts[4][2]])
        for name, first, count in self.mesh.parts[5:]:
            zs = [self.mesh.vertices[i][2] for i in self.mesh.indices[first:first + count]]
            self.assertEqual(min(zs), head_front, name)
            self.assertGreater(max(zs), head_front, name)

    def test_the_first_palette_is_the_drawings(self):
        first = self.mesh.palettes[0]
        self.assertEqual([first[i] for i in (0, 2, 4, 6, 8)],
                         [(0x0c, 0x86, 0x94), (0x11, 0xce, 0xa1), (0x11, 0xce, 0x87), (0x11, 0xce, 0x87),
                          (0x3a, 0xe8, 0xef)])
        self.assertEqual(first[wimsy_mesh.COLOUR_WHITE], (255, 255, 255))
        self.assertEqual(first[wimsy_mesh.COLOUR_BLACK], (0, 0, 0))
        # A leaf's back is its front darkened.
        self.assertEqual(first[1], tuple(c * wimsy_mesh.BACK_SHADE // 256 for c in first[0]))

    def test_a_colour_is_a_leafs_front_or_back_the_rim_or_a_pupil(self):
        for _, first, count in self.mesh.parts[:5]:
            leaf = self.mesh.parts.index((_, first, count))
            used = {self.mesh.vertices[i][6] for i in self.mesh.indices[first:first + count]}
            self.assertEqual(used, {leaf * 2, leaf * 2 + 1, wimsy_mesh.COLOUR_WHITE})

    def test_every_drawing_has_its_palette(self):
        self.assertEqual(len(self.mesh.palettes), len(wimsy_mesh.VARIANTS))
        for palette in self.mesh.palettes:
            self.assertEqual(len(palette), wimsy_mesh.COLOURS)
            self.assertEqual(palette[wimsy_mesh.COLOUR_WHITE], (255, 255, 255))
        names = [name for name, _ in wimsy_mesh.VARIANTS]
        # Fuddles' leaves are gradients: the mean of the first one's stops. The Default theme's are
        # its six colours' teals and greens.
        fuddles = self.mesh.palettes[names.index("fuddles")]
        self.assertEqual(fuddles[wimsy_mesh.COLOUR_BLACK], (0x09, 0x0e, 0x1b))
        self.assertNotEqual(fuddles[0], self.mesh.palettes[0][0])
        default = self.mesh.palettes[names.index("default")]
        self.assertEqual([default[i] for i in (0, 2, 4, 6, 8)],
                         [(0, 170, 170), (85, 255, 85), (0, 170, 0), (0, 170, 0), (85, 255, 255)])

    def test_no_two_drawings_are_one(self):
        # wimsy-dist's Sepia theme's Wimsy is Accord's file, byte for byte: a drawing counted twice.
        self.assertEqual(len({tuple(palette) for palette in self.mesh.palettes}), len(self.mesh.palettes))
        folder = os.path.dirname(SVG)
        files = [open(os.path.join(folder, name), "rb").read() for _, name in wimsy_mesh.VARIANTS]
        self.assertEqual(len(set(files)), len(files))

    def test_a_gradients_colour_is_the_mean_of_its_stops(self):
        import xml.etree.ElementTree as ET
        root = ET.fromstring(
            '<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink">'
            '<linearGradient id="a"><stop stop-color="#000000"/><stop style="stop-color:#102030"/></linearGradient>'
            '<linearGradient id="b" xlink:href="#a"/></svg>')
        self.assertEqual(wimsy_mesh.paint(root, "url(#a)"), (8, 16, 24))
        self.assertEqual(wimsy_mesh.paint(root, "url(#b)"), (8, 16, 24))
        self.assertEqual(wimsy_mesh.paint(root, "#0a0B0c"), (10, 11, 12))


if __name__ == "__main__":
    unittest.main()
