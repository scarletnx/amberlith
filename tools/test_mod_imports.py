# SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""tools/mod-imports.py holds the mod's module to its list of imports, to exporting nothing, to a dynamic section a
game's linker takes, to its MOD0 header and to nothing of libnx but the kernel's calls."""

import importlib.util
import os
import struct
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SPEC = importlib.util.spec_from_file_location("mod_imports", os.path.join(HERE, "mod-imports.py"))
mod_imports = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(mod_imports)

LIST = '''
# the driver
required nvnBootstrapLoader
optional _ZN2nv18InitializeGraphicsEPvm   # what a game calls first
'''

TABLE = '''
Symbol table '.dynsym' contains 5 entries:
   Num:    Value          Size Type    Bind   Vis      Ndx Name
     0: 0000000000000000     0 NOTYPE  LOCAL  DEFAULT  UND
     1: 0000000000000000     0 SECTION LOCAL  DEFAULT    1 .text
     2: 0000000000000000     0 NOTYPE  GLOBAL DEFAULT  UND nvnBootstrapLoader
     3: 0000000000000000     0 NOTYPE  WEAK   DEFAULT  UND _ZN2nv18InitializeGraphicsEPvm
'''

DYNAMIC = '''
Dynamic section at offset 0x22f960 contains 17 entries:
  Tag        Type                         Name/Value
 0x000000000000000c (INIT)               0xbba90
 0x0000000000000007 (RELA)               0xe4018
 0x0000000000000018 (BIND_NOW)
 0x0000000000000000 (NULL)               0x0
'''


def elf(word, magic=b"MOD0", vaddr=0):
    """An ELF64 of one loaded segment of 0x40 bytes at vaddr: an instruction, then word, and magic at 0x20."""
    segment = bytearray(0x40)
    struct.pack_into("<II", segment, 0, 0xd4207d00, word)
    segment[0x20:0x20 + len(magic)] = magic
    header = bytearray(0x40)
    header[:5] = b"\x7fELF\x02"
    struct.pack_into("<Q", header, 0x20, 0x40)
    struct.pack_into("<HH", header, 0x36, 0x38, 1)
    program = struct.pack("<IIQQQQQQ", 1, 5, 0x78, vaddr, vaddr, len(segment), len(segment), 0x1000)
    return bytes(header) + program + bytes(segment)


class List(unittest.TestCase):
    def test_a_line_is_a_kind_and_a_symbol_with_comments_left_out(self):
        wanted, problems = mod_imports.listed(LIST)
        self.assertEqual(problems, [])
        self.assertEqual(wanted, {"nvnBootstrapLoader": "required", "_ZN2nv18InitializeGraphicsEPvm": "optional"})

    def test_a_line_of_another_shape_and_a_symbol_named_twice_are_said_with_their_lines(self):
        wanted, problems = mod_imports.listed("required a\nsometimes b\nrequired\noptional a\n")
        self.assertEqual(wanted, {"a": "required"})
        self.assertEqual(len(problems), 3)
        self.assertIn('line 2 of the list is "sometimes b"', problems[0])
        self.assertIn('line 3 of the list is "required"', problems[1])
        self.assertIn("line 4 of the list names a again", problems[2])


class Symbols(unittest.TestCase):
    def setUp(self):
        self.wanted, _ = mod_imports.listed(LIST)

    def problems(self, table):
        return mod_imports.symbol_problems(self.wanted, mod_imports.dynamic_symbols(table))

    def test_the_lists_imports_and_nothing_else_pass(self):
        self.assertEqual(mod_imports.dynamic_symbols(TABLE), [
            ("nvnBootstrapLoader", "GLOBAL", "UND"), ("_ZN2nv18InitializeGraphicsEPvm", "WEAK", "UND")])
        self.assertEqual(self.problems(TABLE), [])

    def test_an_import_the_list_has_not_is_named(self):
        # what a C library that was not linked looks like: its functions become imports
        table = TABLE + "     4: 0000000000000000     0 NOTYPE  GLOBAL DEFAULT  UND malloc\n"
        self.assertEqual(len(self.problems(table)), 1)
        self.assertIn("malloc is imported (GLOBAL) and the list does not have it", self.problems(table)[0])

    def test_an_exported_symbol_is_named(self):
        table = TABLE + "     4: 0000000000000400     8 FUNC    GLOBAL DEFAULT    1 memcpy\n"
        self.assertEqual(len(self.problems(table)), 1)
        self.assertIn("memcpy is exported (GLOBAL, section 1)", self.problems(table)[0])

    def test_a_required_import_that_is_weak_and_an_optional_one_that_is_not_are_named(self):
        table = TABLE.replace("GLOBAL DEFAULT  UND nvnBootstrapLoader", "WEAK   DEFAULT  UND nvnBootstrapLoader")
        self.assertIn("nvnBootstrapLoader is required and imported WEAK, not GLOBAL", self.problems(table)[0])
        table = TABLE.replace("WEAK   DEFAULT  UND _ZN2nv", "GLOBAL DEFAULT  UND _ZN2nv")
        self.assertIn("_ZN2nv18InitializeGraphicsEPvm is optional and imported GLOBAL, not WEAK",
                      self.problems(table)[0])

    def test_a_line_of_the_list_the_module_does_not_import_is_named(self):
        table = "\n".join(line for line in TABLE.splitlines() if "nvnBootstrapLoader" not in line)
        self.assertEqual(self.problems(table),
                         ["nvnBootstrapLoader is in the list (required) and the module does not import it"])


class Dynamic(unittest.TestCase):
    def test_an_init_and_plain_relocations_pass(self):
        self.assertEqual(mod_imports.dynamic_problems(DYNAMIC), [])

    def test_no_init_is_said(self):
        problems = mod_imports.dynamic_problems(DYNAMIC.replace("(INIT)", "(FINI)"))
        self.assertEqual(len(problems), 1)
        self.assertIn("no INIT", problems[0])

    def test_packed_relocations_are_named_by_their_tags(self):
        problems = mod_imports.dynamic_problems(
            DYNAMIC + " 0x0000000000000024 (RELR)               0x100\n 0x0000000000000023 (RELRSZ)             8\n")
        self.assertEqual(len(problems), 2)
        self.assertIn("has RELR:", problems[0])
        self.assertIn("has RELRSZ:", problems[1])


class Header(unittest.TestCase):
    def test_the_word_after_the_first_instruction_leads_to_mod0(self):
        self.assertEqual(mod_imports.header_problems(elf(0x20)), [])

    def test_a_word_that_leads_elsewhere_names_what_is_there(self):
        self.assertEqual(mod_imports.header_problems(elf(0x24)),
                         ["the word after the first instruction is 0x24, and at 0x24 the file has "
                          "b'\\x00\\x00\\x00\\x00', not MOD0"])
        self.assertIn("at 0x1000 the file has nothing, not MOD0", mod_imports.header_problems(elf(0x1000))[0])
        self.assertIn("b'MOD1', not MOD0", mod_imports.header_problems(elf(0x20, b"MOD1"))[0])

    def test_no_segment_at_address_nought_and_no_elf_are_said(self):
        self.assertEqual(mod_imports.header_problems(elf(0x20, vaddr=0x1000)),
                         ["no segment is loaded at address nought, where a module's header starts"])
        self.assertEqual(mod_imports.header_problems(b"MOD0"), ["the file is no ELF64"])


class Map(unittest.TestCase):
    def test_the_kernels_calls_alone_pass_and_any_other_member_is_named_once(self):
        ours = "/opt/devkitpro/libnx/lib/libnx.a(svc.o)\n   build-mod/mod/host.o (svcQueryMemory)\n"
        self.assertEqual(mod_imports.map_problems(ours), [])
        more = ours + "libnx.a(init.o)\n libnx.a(switch_crt0.o) (__libnx_init)\nlibnx.a(init.o)\n"
        self.assertEqual(mod_imports.map_problems(more), [
            "libnx's init.o is linked: part of a start that never runs in a game's process",
            "libnx's switch_crt0.o is linked: part of a start that never runs in a game's process"])


if __name__ == "__main__":
    unittest.main()
