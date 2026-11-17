# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""tools/nvn-lookups.py finds the lookups and the calls a runtime that runs NVN itself cannot take."""

import importlib.util
import os
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SPEC = importlib.util.spec_from_file_location("nvn_lookups", os.path.join(HERE, "nvn-lookups.py"))
tool = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(tool)

SYMBOLS = """
Symbol table '.dynsym' contains 3 entries:
   Num:    Value          Size Type    Bind   Vis      Ndx Name
     0: 0000000000000000     0 NOTYPE  LOCAL  DEFAULT  UND
     1: 0000000000109700    16 FUNC    GLOBAL DEFAULT    1 nvnBootstrapLoader
"""

# NvnLoadProcs as it has to be: the C library by name, every lookup through a register
GOOD = """
0000000000109700 <nvnBootstrapLoader>:
  109700:	b	1095d0 <Lookup>

0000000000109b20 <NvnLoadProcs>:
  109b20:	stp	x29, x30, [sp, #-80]!
  109b58:	bl	146c40 <memset>
  109b64:	blr	x1
  109b84:	blr	x3
  109b90:	cbz	x0, 109c00 <NvnLoadProcs+0xe0>
  109ba0:	bl	146d00 <snprintf>
  109bb0:	b	109b20 <NvnLoadProcs+0x10>
  109bb4:	ret

0000000000109d00 <Nvnl_nvnQueueFlush>:
  109d00:	b.ne	109d10 <Nvnl_nvnQueueFlush+0x10>
  109d04:	b	120000 <dkQueueFlush>
"""

# what GCC 15 made of a lookup whose target it could tell: a compare, then the lookup's own code by name
SPECULATED = GOOD.replace("  109b84:	blr	x3\n",
                          "  109ba4:	b.ne	10abf4 <NvnLoadProcs+0x10d4>\n"
                          "  109bb0:	bl	1095d0 <Lookup>\n"
                          "  109bdc:	bl	1095d0 <Lookup>\n")


class Check(unittest.TestCase):
    def problems(self, symbols, code):
        return tool.problems(tool.exported(symbols), tool.functions(code))

    def test_a_program_a_runtime_can_take(self):
        self.assertEqual(self.problems(SYMBOLS, GOOD), [])

    def test_a_lookup_called_by_name(self):
        found = self.problems(SYMBOLS, SPECULATED)
        self.assertEqual(len(found), 1)
        self.assertIn("NvnLoadProcs calls Lookup by name 2 times, the first at 109bb0", found[0])

    def test_an_entry_point_reached_by_name(self):
        code = GOOD + """
0000000000110000 <Scene>:
  110000:	bl	109d00 <Nvnl_nvnQueueFlush>
  110004:	b	109700 <nvnBootstrapLoader>
  110008:	bl	109d10 <Nvnl_nvnQueueFlush+0x10>
"""
        found = self.problems(SYMBOLS, code)
        self.assertEqual(len(found), 2)
        self.assertIn("Scene reaches Nvnl_nvnQueueFlush with `bl` 1 times, the first at 110000", found[0])
        self.assertIn("Scene reaches nvnBootstrapLoader with `b` 1 times, the first at 110004", found[1])

    def test_the_bootstrap_not_exported(self):
        found = self.problems(SYMBOLS.replace("nvnBootstrapLoader", "somethingElse"), GOOD)
        self.assertEqual(found, ["nvnBootstrapLoader is not in the dynamic symbols: a runtime finds the "
                                 "program's NVN there"])
        found = self.problems(SYMBOLS.replace("GLOBAL", "LOCAL "), GOOD)
        self.assertEqual(len(found), 1)
        self.assertIn("nvnBootstrapLoader is FUNC LOCAL in section 1, not a defined global function", found[0])

    def test_no_loader_in_the_program(self):
        code = GOOD.replace("<NvnLoadProcs>:", "<Renamed>:").replace("<NvnLoadProcs+", "<Renamed+")
        self.assertEqual(self.problems(SYMBOLS, code),
                         ["NvnLoadProcs is not in the program's code: nothing to check its lookups in"])


if __name__ == "__main__":
    unittest.main()
