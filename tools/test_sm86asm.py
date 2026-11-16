# SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""tools/sm86asm.py writes the words ScarletNX's own tests, and a console's compiler, write for the same lines."""

import hashlib
import importlib.util
import os
import struct
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SPEC = importlib.util.spec_from_file_location("sm86asm", os.path.join(HERE, "sm86asm.py"))
asm = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(asm)


# ---- the words by another road: bits set one field at a time, as ScarletNX's tests/shader_sm86.c sets them (its
# fix, alu and set, after Mesa's NAK), with none of the assembler's code

def fix(opcode, **fields):
    word = opcode | 7 << 12 | 7 << 110 | 7 << 113
    return put(word, **fields)


def put(word, **fields):
    for name, value in fields.items():
        low, bits = (int(n) for n in name[1:].split("_"))
        assert 0 <= value < 1 << bits, name
        word = word & ~(((1 << bits) - 1) << low) | value << low
    return word


def alu(opcode9, form, dst, a, **fields):
    return fix(opcode9 | form << 9, b16_8=dst, b24_8=a, **fields)


def bra(index, target, guard=7, inverted=0):
    return fix(0x947, b12_3=guard, b15_1=inverted, b87_3=7, b34_48=((target - index - 1) * 4) & (1 << 48) - 1)


EXIT = fix(0x94D, b87_3=7)


def words(code, graphics):
    """the instructions of an assembled code area: (low | high << 64) each, up to the area's padding"""
    body = code[128:] if graphics else code
    out = []
    for at in range(0, len(body), 16):
        low, high = struct.unpack_from("<QQ", body, at)
        if low == 0 and high == 0:
            break
        out.append(low | high << 64)
    return out


class Table(unittest.TestCase):
    def test_the_copy_is_the_pinned_file(self):
        with open(os.path.join(HERE, "sm86", "PIN"), encoding="ascii") as file:
            pin = dict(line.split() for line in file if line.strip() and not line.startswith("#"))
        with open(os.path.join(HERE, "sm86", "sm86.decode"), "rb") as file:
            self.assertEqual(hashlib.sha256(file.read()).hexdigest(), pin["sha256"])
        self.assertRegex(pin["commit"], "^[0-9a-f]{40}$")

    def test_rows(self):
        table = asm.read_table()
        self.assertEqual(table["FADD"], {f: f << 9 | 0x021 for f in range(1, 8)})
        self.assertEqual(table["FMUL"], {1: 0x220, 4: 0x820, 5: 0xA20, 6: 0xC20})
        self.assertEqual(table["EXIT"], {4: 0x94D})
        self.assertEqual(table["HMMA"], {1: 0x23C})
        self.assertEqual(len(table), 177)


class Programs(unittest.TestCase):
    def test_the_compute_program_of_scarletnx_s_tests(self):
        # tests/shader_sm86.c, compute_program: R0 = TID.X; UR4:UR5 = c[0][0x20]; R2 = [R0 + UR4]; R2 *= 2;
        # [R0 + UR4 + 0x100] = R2
        code, control, _ = asm.assemble("""
            .stage compute
            .local 8 4 1
            .shared 0x40
                S2R R0, SR_TID.X
                ULDC.64 UR4, c[0][0x20]
                LDG.E R2, [R0 + UR4]
                FMUL R2, R2, 2.0f
                STG.E [R0 + UR4 + 0x100], R2
                EXIT
        """)
        self.assertEqual(words(code, False), [
            fix(0x919, b16_8=0, b72_8=0x21),
            fix(0xAB9, b16_6=4, b38_16=0x20, b54_5=0, b73_3=5),
            fix(0x981, b16_8=2, b24_8=0, b32_6=4, b72_1=1, b73_3=4, b81_3=7, b91_1=1),
            alu(0x020, 4, 2, 2, b32_32=0x40000000, b84_3=4),
            fix(0x986, b24_8=0, b32_8=2, b40_24=0x100, b64_6=4, b72_1=1, b73_3=4, b91_1=1),
            EXIT,
            bra(6, 6),
        ])
        self.assertEqual(len(code), 0x100)
        # the control section, where ScarletNX reads each field (nvn_object.c's stage_of, nvn_section.h)
        self.assertEqual(len(control), 0xD8)
        self.assertEqual(struct.unpack_from("<III", control, 0), (0x98761234, 0xC9, 11))
        self.assertEqual(struct.unpack_from("<I", control, 0x38)[0], 7 * 16)        # the code's bytes
        self.assertEqual(struct.unpack_from("<II", control, 0x3C), (0, 0x100))      # the constants': none, at 0x100
        self.assertEqual(struct.unpack_from("<H", control, 0x56)[0], 5)             # NVN's compute stage
        self.assertEqual(struct.unpack_from("<IIII", control, 0x58), (8, 4, 1, 0x40))

    def test_the_vertex_program_of_scarletnx_s_tests(self):
        # vertex_program: R0-R3 = a[0x80]; if (R0 < 5) R1 = 1.0; a[0x70] = R0-R3
        code, control, _ = asm.assemble("""
            .stage vertex
            .out position
                ALD.128 R0, a[0x80]
                ISETP.GE.U32.AND P0, PT, R0, 5, PT
                @P0 BRA store
                MOV R1, 1.0f
            store:
                AST.128 a[0x70], R0
                EXIT
        """)
        self.assertEqual(words(code, True), [
            fix(0x321, b16_8=0, b24_8=255, b32_8=255, b40_10=0x80, b74_2=3),
            alu(0x00C, 4, 255, 0, b32_32=5, b76_3=6, b81_3=0, b84_3=7, b87_3=7),
            bra(2, 4, guard=0),
            alu(0x002, 4, 1, 255, b32_32=0x3F800000, b72_4=0xF),
            fix(0x322, b24_8=255, b32_8=0, b64_8=255, b40_10=0x70, b74_2=3),
            EXIT,
            bra(6, 6),
        ])
        header = struct.unpack_from("<32I", code, 0)
        self.assertEqual(header[0], 0x020C0481)        # a vertex program's first word (ounce.md, "Stages")
        self.assertEqual(header[13], 0xF << 12)        # the position's four words, bits 428 to 431 of the map
        self.assertEqual(struct.unpack_from("<H", control, 0x56)[0], 0)
        self.assertEqual(struct.unpack_from("<I", control, 0x38)[0], 128 + 7 * 16)

    def test_the_fragment_program_of_scarletnx_s_tests(self):
        # fragment_program: R0 = a[0x80] interpolated, R1-R3 = 0. The test's MOV of a register leaves bits 72 to 75
        # nought and its MOV of a number sets them, as Mesa's NAK sets them for both: the assembler sets them
        code, control, _ = asm.assemble("""
            .stage fragment
            .in generic0.x
                IPA R0, a[0x80]
                MOV R1, RZ
                MOV R2, RZ
                MOV R3, RZ
                EXIT
        """)
        self.assertEqual(words(code, True), [
            fix(0x326, b16_8=0, b24_8=0, b32_8=255, b64_8=0x80 >> 2, b81_3=7),
            alu(0x002, 1, 1, 255, b32_8=255, b72_4=0xF),
            alu(0x002, 1, 2, 255, b32_8=255, b72_4=0xF),
            alu(0x002, 1, 3, 255, b32_8=255, b72_4=0xF),
            EXIT,
            bra(5, 5),
        ])
        header = struct.unpack_from("<32I", code, 0)
        self.assertEqual(header[0], 0x000C5482)
        self.assertEqual(header[6], 2)                 # generic 0's x read, with perspective
        self.assertEqual(header[18], 0xF)              # one colour target, its four components
        self.assertEqual(struct.unpack_from("<H", control, 0x56)[0], 1)

    def test_the_mesh_triangle_of_scarletnx_s_tests(self):
        # tests/vulkan_mesh_draw.inc, mesh_draw_triangle: three vertices' positions, the indices and the count,
        # through its support encoder's sm86_isbe (write, register, base, offset, size, access, skew, output)
        def isbe(reg, offset, size, access, skew):
            return fix(0x927, b32_8=reg, b24_8=255, b40_16=offset, b74_2=size, b76_2=access, b78_1=skew, b79_1=1)
        code, control, _ = asm.assemble("""
            .stage mesh
            .threads 1
            .payload 0x80
            .vertices 3
            .primitives 1
            .out position
                MOV R1, -1.0f
                ISBEWR.ATTR.SKEW.O.B32 [RZ + 0x0], R1
                ISBEWR.ATTR.SKEW.O.B32 [RZ + 0x84], R1
                MOV R5, 1
                ISBEWR.MAP.O.U8 [RZ + 0x5], R5
                ISBEWR.MAP.O.B32 [RZ + 0x3], R5
                EXIT
        """)
        self.assertEqual(words(code, True), [
            alu(0x002, 4, 1, 255, b32_32=0xBF800000, b72_4=0xF),
            isbe(1, 0x0, 2, 3, 1),
            isbe(1, 0x84, 2, 3, 1),
            alu(0x002, 4, 5, 255, b32_32=1, b72_4=0xF),
            isbe(5, 0x5, 0, 0, 0),
            isbe(5, 0x3, 2, 0, 0),
            EXIT,
            bra(7, 7),
        ])
        self.assertEqual(struct.unpack_from("<32I", code, 0)[0], 0x000C0481)   # a mesh program with no task program
        # the section's mesh fields (nvn_section.h): the stage, the invocations and the payload, no task program,
        # triangles, the most vertices and primitives
        self.assertEqual(struct.unpack_from("<H", control, 0x56)[0], 7)
        self.assertEqual(struct.unpack_from("<HH", control, 0x58), (1, 0x80))
        self.assertEqual((control[0x5E], control[0x5F]), (0, 3))
        self.assertEqual(struct.unpack_from("<HH", control, 0x60), (3, 1))

    def test_a_console_compiler_s_last_two_instructions(self):
        # ScarletNX's docs/platforms/nx/ounce.md, "Shaders": every program of an Ounce game ends with EXIT, 0x794d
        # with the control word 0x000fea0003800000, then a BRA to itself, 0xfffffff000007947 and 0x000fc0000383ffff.
        # The EXIT's stall of 5 cycles and its yield are scheduling, which the assembler leaves to fields by hand
        code, _, _ = asm.assemble(".stage compute\n.local 1 1 1\nEXIT ! 105:4=5 109:1=1\n")
        self.assertEqual(struct.unpack_from("<4Q", code, 0),
                         (0x794D, 0x000FEA0003800000, 0xFFFFFFF000007947, 0x000FC0000383FFFF))

    def test_the_forms_and_the_modifiers(self):
        def one(line):
            code, _, _ = asm.assemble(".stage compute\n.local 1 1 1\n%s\nEXIT\n" % line)
            return words(code, False)[0]
        # FFMA in the forms of tests/shader_sm86.c's test_alu_forms
        self.assertEqual(one("FFMA R1, -R2, |R3|, -R4"),
                         alu(0x023, 1, 1, 2, b32_8=3, b64_8=4, b72_1=1, b62_1=1, b75_1=1))
        self.assertEqual(one("FFMA R1, R2, R5, 1.0f"), alu(0x023, 2, 1, 2, b32_32=0x3F800000, b64_8=5))
        self.assertEqual(one("FFMA R1, R2, R6, c[3][0x28c]"), alu(0x023, 3, 1, 2, b38_16=0x28C, b54_5=3, b64_8=6))
        self.assertEqual(one("FFMA R1, R2, c[17][0xfffc], R7"),
                         alu(0x023, 5, 1, 2, b38_16=0xFFFC, b54_5=17, b64_8=7))
        self.assertEqual(one("IMAD R1, R2, UR4, RZ"), alu(0x024, 6, 1, 2, b32_6=4, b91_1=1, b64_8=255))
        self.assertEqual(one("IMAD R1, R2, R3, -UR5"), alu(0x024, 7, 1, 2, b64_8=3, b32_6=5, b63_1=1, b91_1=1))
        self.assertEqual(one("MOV R9, c[8][0x1e4]"), alu(0x002, 5, 9, 255, b38_16=0x1E4, b54_5=8, b72_4=0xF))
        # a guard, and the float modifiers at their bits (77 saturate, 78-79 the rounding, 80 FTZ)
        self.assertEqual(one("@!P3 FADD.FTZ.RZ.SAT R200, R100, R1"),
                         alu(0x021, 1, 200, 100, b32_8=1, b12_3=3, b15_1=1, b77_1=1, b78_2=3, b80_1=1))
        # the integer ones: a shift right of a signed word, as the translator reads it (the word in c, .HI)
        self.assertEqual(one("SHF.R.S32.HI R1, RZ, 12, R2"),
                         alu(0x019, 4, 1, 255, b32_32=12, b64_8=2, b73_2=2, b76_1=1, b80_1=1))
        self.assertEqual(one("LOP3.LUT R7, R6, 0xff, RZ, 0xc0"),
                         alu(0x012, 4, 7, 6, b32_32=0xFF, b64_8=255, b72_8=0xC0, b81_3=7, b87_4=0xF))
        self.assertEqual(one("IADD3 R1, R2, 1, RZ"), alu(0x010, 4, 1, 2, b32_32=1, b64_8=255, b81_3=7, b84_3=7))
        self.assertEqual(one("I2F.S32 R5, R1"), alu(0x106, 1, 5, 255, b32_8=1, b74_1=1, b75_2=2, b84_2=2))
        self.assertEqual(one("F2I.S32.TRUNC R14, R12"),
                         alu(0x105, 1, 14, 255, b32_8=12, b72_1=1, b75_2=2, b78_2=3, b84_2=2))
        self.assertEqual(one("FSETP.GT.FTZ.AND P1, PT, R2, c[3][0x10], P0"),
                         alu(0x00B, 5, 255, 2, b38_16=0x10, b54_5=3, b76_4=4, b80_1=1, b81_3=1, b84_3=7, b87_3=0))
        self.assertEqual(one("SEL R1, R2, R3, !P4"), alu(0x007, 1, 1, 2, b32_8=3, b87_3=4, b90_1=1))
        # the warp's matrix multiply, as tests/shader_sm86.c's test_warp_matrix sets it: A at 24, B at 32, C at 64,
        # and at 72 the variant, 8 for 16 by 8 by 16 and 16 for sums in floats
        for line, dst, c, variant in (("HMMA.16816.F16 R16, R8, R12, R14", 16, 14, 0x08),
                                      ("HMMA.16816.F32 R20, R8, R12, R24", 20, 24, 0x18),
                                      ("HMMA.1688.F16 R28, R8, R12, RZ", 28, 255, 0x00),
                                      ("HMMA.1688.F32 R32, R8, R12, R36", 32, 36, 0x10)):
            self.assertEqual(one(line), fix(0x23C, b16_8=dst, b24_8=8, b32_8=12, b64_8=c, b72_19=variant))
        self.assertEqual(one("LDC R6, c[1][R1 + 0x10]"), fix(0xB82, b16_8=6, b24_8=1, b38_16=0x10, b54_5=1, b73_3=4))

    def test_an_image_s_load_and_store(self):
        """SULD.P and SUST.P as ScarletNX's translator reads them (sm86_translate.c, surface): the coordinates'
        register at 24, a store's value at 32, the handle's place at 40 as its bank above its word, the shape at
        61 and the channels at 72"""
        code, _, _ = asm.assemble(""".stage compute
                .local 1 1 1
                SULD.P.2D.R R8, [R4], c[2][0x128]
                SUST.P.2D.R [R4], R8, c[2][0x120]
                SUST.P.3D.RGBA [R4], R8, c[2][0x130]
                EXIT
        """)
        self.assertEqual(words(code, False), [
            fix(0xB97, b16_8=8, b24_8=4, b40_19=2 << 14 | 0x4A, b61_3=3, b72_4=1, b81_3=7),
            fix(0xB9B, b24_8=4, b32_8=8, b40_19=2 << 14 | 0x48, b61_3=3, b72_4=1),
            fix(0xB9B, b24_8=4, b32_8=8, b40_19=2 << 14 | 0x4C, b61_3=5, b72_4=15),
            EXIT,
            bra(4, 4),
        ])

    def test_a_repeat_writes_its_lines_out(self):
        head = ".stage compute\n.local 1 1 1\n"
        body = "IADD3 R1, R1, R1, RZ\nFADD R2, R2, R1\n"
        looped, _, _ = asm.assemble(head + "MOV R1, 1\n.repeat 3\n" + body + ".end\nEXIT\n")
        written, _, _ = asm.assemble(head + "MOV R1, 1\n" + body * 3 + "EXIT\n")
        self.assertEqual(looped, written)
        self.assertEqual(len(words(looped, False)), 1 + 2 * 3 + 2)

    def test_constants_follow_the_code_on_a_0x100_boundary(self):
        code, control, _ = asm.assemble(".stage compute\n.local 1 1 1\n.const 0x11223344 1.0f\nEXIT\n")
        self.assertEqual(len(code), 0x200)
        self.assertEqual(code[0x100:0x108], struct.pack("<II", 0x11223344, 0x3F800000))
        self.assertEqual(struct.unpack_from("<III", control, 0x38), (32, 8, 0x100))
        self.assertEqual(struct.unpack_from("<I", control, 0x44)[0], 0x200)


class TheTableDecides(unittest.TestCase):
    def test_a_form_the_table_lacks_is_refused(self):
        table = asm.read_table()
        del table["FMUL"][6]
        with self.assertRaises(asm.Refused) as refused:
            asm.assemble(".stage compute\n.local 1 1 1\nFMUL R1, R2, UR3\nEXIT\n", table)
        self.assertIn("line 3: FMUL has no form 6 in sm86.decode, which holds its forms 1, 4, 5",
                      str(refused.exception))
        del table["EXIT"]
        with self.assertRaises(asm.Refused) as refused:
            asm.assemble(".stage compute\n.local 1 1 1\nEXIT\n", table)
        self.assertIn("EXIT is not in sm86.decode", str(refused.exception))

    def test_a_row_of_another_shape_is_refused(self):
        import tempfile
        with tempfile.NamedTemporaryFile("w", suffix=".decode") as file:
            file.write("FADD ---- 001 000100001\nFMUL ---- 0001 000100000\n")
            file.flush()
            with self.assertRaises(asm.Refused) as refused:
                asm.read_table(file.name)
        self.assertIn(":2: a row that is not NAME ---- FFF OOOOOOOOO: FMUL ---- 0001 000100000",
                      str(refused.exception))


class Refusals(unittest.TestCase):
    def refused(self, body, says, head=".stage compute\n.local 1 1 1\n"):
        with self.assertRaises(asm.Refused) as refused:
            asm.assemble(head + body)
        self.assertIn(says, str(refused.exception))

    def test_each_names_the_line_and_the_value(self):
        self.refused("FROB R1, R2\nEXIT\n", "line 3: FROB is not an instruction the assembler writes yet")
        self.refused("IMAD R1, -R2, R3, RZ\nEXIT\n", "IMAD: this source takes no -")
        self.refused("SUST.P.R [R4], R8, c[2][0x120]\nEXIT\n", "SUST needs .P, the image's shape")
        self.refused("SUST.P.2D.R [R4 + 4], R8, c[2][0x120]\nEXIT\n", "SUST takes [R]")
        self.refused("SULD.P.2D.R R8, [R4], c[2][0x122]\nEXIT\n", "the offset 290 (0x122)")
        self.refused("IADD3 R1, R2, |R3|, RZ\nEXIT\n", "IADD3: this source takes no | |")
        self.refused("FFMA R1, R2, 1.0f, 2.0f\nEXIT\n", "no form takes these sources together: a number and a number")
        self.refused("MOV R256, R1\nEXIT\n", "R256 is past the last register, 254")
        self.refused("MOV R1, UR64\nEXIT\n", "UR64 is past the last uniform register, 62")
        self.refused("MOV R1, P7\nEXIT\n", "P7 is past the last predicate, P6")
        self.refused("FADD.FAST R1, R2, R3\nEXIT\n", "FADD takes no .FAST: its modifiers are .RN, .RM, .RP, .RZ")
        self.refused("FADD.RN.RZ R1, R2, R3\nEXIT\n", "two modifiers of one kind")
        self.refused("ISETP.GE.AND P0, PT, R0, 5, PT\nEXIT\n", "ISETP needs its integers' kind, .S32 or .U32")
        self.refused("BRA nowhere\nEXIT\n", "BRA to nowhere, which is no label of the program")
        self.refused("S2R R0, SR_CLOCK\nEXIT\n", "S2R of SR_CLOCK: the special registers the assembler names are")
        self.refused("LDG R2, [R0 + UR4]\nEXIT\n", "LDG needs .E")
        self.refused("HMMA.F32 R16, R8, R12, RZ\nEXIT\n", "HMMA needs its shape, .16816 or .1688")
        self.refused("HMMA.16816 R16, R8, R12, RZ\nEXIT\n", "HMMA needs its sums' type, .F16 or .F32")
        self.refused("HMMA.16816.F32 R16, R8, 1.0f, RZ\nEXIT\n", "HMMA has no form 4 in sm86.decode")
        self.refused("STG.E [R0 + UR4 + 0x800000], R2\nEXIT\n", "the offset 8388608")
        self.refused("MOV R1, c[32][0]\nEXIT\n", "the constant buffer's bank 32 (0x20) does not fit the 5 bits at 54")
        self.refused("MOV R1, R2 ! 200:4=1\nEXIT\n", "the field 200:4 is past the instruction's 128 bits")
        self.refused("MOV R1, R2\n", "the program ends on MOV, where its last instruction has to be an EXIT or a BRA")
        self.refused("x:\nx:\nEXIT\n", "line 4: the label x is there twice")
        self.refused(".repeat 2\nx: NOP\n.end\nEXIT\n", "line 4: the label x is there twice")
        self.refused(".repeat 0\nNOP\n.end\nEXIT\n", "line 3: .repeat takes a count from 1 to 256, and has 0")
        self.refused(".repeat 2\n.repeat 2\nNOP\n.end\n.end\nEXIT\n", "line 4: .repeat inside the .repeat of line 3")
        self.refused("NOP\n.end\nEXIT\n", "line 4: .end with no .repeat before it")
        self.refused(".repeat 2\nNOP\nEXIT\n", "line 3: .repeat with no .end after it")
        self.refused("EXIT\n", "the source names no .stage", head="")
        self.refused("EXIT\n", ".stage takes one of vertex, fragment, compute, task, mesh", head=".stage geometry\n")
        self.refused("EXIT\n", "a compute program needs .local X Y Z", head=".stage compute\n")
        self.refused("EXIT\n", ".local takes 3 values, and has 2", head=".stage compute\n.local 8 8\n")
        self.refused("EXIT\n", ".vertices is no directive of a compute program", head=".stage compute\n.vertices 3\n")
        self.refused("EXIT\n", "a mesh program needs .threads 1 to 32, one warp, and has 64",
                     head=".stage mesh\n.threads 64\n.vertices 3\n.primitives 1\n")
        self.refused("EXIT\n", ".out colour is not position or generic0 to generic31",
                     head=".stage vertex\n.out colour\n")


if __name__ == "__main__":
    unittest.main()
