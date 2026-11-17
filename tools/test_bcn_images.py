# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""source/bcnimages.c is laid out by tools/bcn-images.py's writer, for the picture it makes today.

Neither etcpak, Pillow nor imagecodecs is needed: the blocks and hashes are read back from the C file and
written out again with the tool's own writer, which must give the file byte for byte, and the pictures and
blocks the tool encodes itself (BC7's and BC6H's modes in turn, the mode blocks) are encoded again and
compared. The blocks' decodes are held by the reference suite, which decodes them to the hashes the outside
decoders gave."""

import importlib.util
import os
import re
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SPEC = importlib.util.spec_from_file_location("bcn_images", os.path.join(HERE, "bcn-images.py"))
bcn_images = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(bcn_images)

PATH = os.path.join(HERE, "..", "source", "bcnimages.c")


def parse():
    text = open(PATH).read()
    source_hash = int(re.search(r"BCN_IMAGE_SOURCE_HASH = 0x([0-9a-f]{8})u;", text).group(1), 16)
    arrays = {m.group(1): bytes(int(x, 16) for x in re.findall(r"0x([0-9a-f]{2})", m.group(2)))
              for m in re.finditer(r"static const uint8_t BLOCKS_(\w+)\[\d+\] = \{([^}]*)\};", text)}
    rows = re.findall(r'\{ "([^"]+)", (BCN_\w+), (true|false), (true|false), BLOCKS_(\w+), (\d+), '
                      r'0x([0-9a-f]{8})u \}', text)
    modes = re.findall(r'\{ "([^"]+)", (BCN_\w+), \{ ([0-9a-fx, ]+) \}, 0x([0-9a-f]{8})u, 0x([0-9a-f]{8})u \}',
                       text)
    return text, source_hash, arrays, rows, modes


class Images(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text, cls.source_hash, cls.arrays, cls.rows, cls.modes = parse()
        cls.texels = [bcn_images.block_texels(bcn_images.astc_images().picture(), n)
                      for n in range(bcn_images.BLOCKS)]

    def test_the_picture_is_the_astcimage_suites(self):
        picture = bcn_images.astc_images().picture()
        self.assertEqual(bcn_images.fnv1a(picture), self.source_hash)

    def test_the_table_is_the_tools_list(self):
        self.assertEqual([(r[0], r[1], r[4], r[2] == "true", r[3] == "true") for r in self.rows],
                         [image[:5] for image in bcn_images.IMAGES])
        self.assertEqual(list(self.arrays), list(bcn_images.ARRAYS))

    def test_every_picture_has_its_blocks(self):
        for name, kind, _, _, array, count, _ in self.rows:
            size = 8 if kind in ("BCN_BC1", "BCN_BC4") else 16
            self.assertEqual(int(count), bcn_images.BLOCKS, name)
            self.assertEqual(len(self.arrays[array]), size * bcn_images.BLOCKS, name)

    def test_bc7_takes_every_mode_in_turn(self):
        blocks = self.arrays["BC7_MODES"]
        for n in range(bcn_images.BLOCKS):
            first = blocks[16 * n]
            self.assertEqual(first & ((2 << (n % 8)) - 1), 1 << (n % 8), "block %d" % n)
        self.assertEqual(bcn_images.bc7_in_every_mode(self.texels), blocks)

    def test_bc6h_takes_every_mode_in_turn(self):
        blocks = self.arrays["BC6H"]
        for n in range(bcn_images.BLOCKS):
            code, code_bits = bcn_images.BC6_MODES[n % 14][:2]
            self.assertEqual(blocks[16 * n] & ((1 << code_bits) - 1), code, "block %d" % n)
        self.assertEqual(bcn_images.bc6h_in_every_mode(self.texels), blocks)

    def test_the_mode_blocks_are_the_tools(self):
        made = bcn_images.mode_blocks()
        self.assertEqual([(m[0], m[1]) for m in self.modes], [(m[0], m[1]) for m in made])
        for (name, _, text, _, signed), (_, _, block, _, signed_kind) in zip(self.modes, made):
            self.assertEqual(bytes(int(x, 16) for x in text.split(", ")), block, name)
            self.assertEqual(signed == "00000000", signed_kind is None, name)

    def test_the_file_is_the_writers_output(self):
        image_hashes = [int(r[6], 16) for r in self.rows]
        mode_hashes = [(int(m[3], 16), int(m[4], 16)) for m in self.modes]
        self.assertEqual(bcn_images.c_file(self.source_hash, self.arrays, image_hashes, mode_hashes),
                         self.text)


if __name__ == "__main__":
    unittest.main()
