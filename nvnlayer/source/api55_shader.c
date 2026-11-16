// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string.h>

#include "api55_control.h"
#include "layer.h"

// A DKSH, what uam compiles a shader into, as the two halves NVN takes a stage in: the code,
// which is the DKSH's code section as it is, and a control section written here.
//
// The code needs no change because uam lays it out as the console's own compiler does: 0x30
// bytes, the program header, the instructions, and the compiler's constants on a 0x100 boundary
// after them. ScarletNX finds the program at the offset the control section gives, past those
// 0x30 bytes, and the constants at theirs.

static uint32_t Word(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
           (uint32_t)bytes[3] << 24;
}

static void Put(uint8_t *section, uint32_t at, uint32_t value) {
    section[at] = (uint8_t)value;
    section[at + 1u] = (uint8_t)(value >> 8);
    section[at + 2u] = (uint8_t)(value >> 16);
    section[at + 3u] = (uint8_t)(value >> 24);
}

bool NvnLayerShaderControl(const void *dksh, size_t dksh_bytes, void *control,
                           NvnLayerShaderCode *code) {
    static const char *const WHAT = "NvnLayerShaderControl";
    const uint8_t *const module = dksh;
    if (module == NULL || control == NULL || code == NULL || ((uintptr_t)control & 7u) != 0u) {
        LayerComplain("%s: a module, 8-byte aligned memory for the section and a place for the "
                      "code's whereabouts are all needed",
                      WHAT);
        return false;
    }
    if (dksh_bytes < 4u * LAYER_DKSH_HEADER_WORDS || Word(module) != LAYER_DKSH_MAGIC) {
        LayerComplain("%s: %zu bytes that do not start a DKSH", WHAT, dksh_bytes);
        return false;
    }
    const uint32_t control_bytes = Word(module + 8);
    const uint32_t code_bytes = Word(module + 12);
    const uint32_t programs_at = Word(module + 16);
    const uint32_t programs = Word(module + 20);
    if (programs != 1u) {
        LayerComplain("%s: a DKSH of %u programs, and a stage takes a module of one", WHAT,
                      (unsigned)programs);
        return false;
    }
    if ((control_bytes & 0xffu) != 0u || (code_bytes & 0xffu) != 0u || code_bytes == 0u ||
        code_bytes >> 24 != 0u || control_bytes > dksh_bytes ||
        code_bytes > dksh_bytes - control_bytes || programs_at > control_bytes ||
        control_bytes - programs_at < 4u * LAYER_DKSH_PROGRAM_WORDS) {
        LayerComplain("%s: a DKSH of %zu bytes whose header gives a control section of 0x%x, a "
                      "code section of 0x%x and its program at 0x%x, which do not fit it",
                      WHAT, dksh_bytes, (unsigned)control_bytes, (unsigned)code_bytes,
                      (unsigned)programs_at);
        return false;
    }
    const uint8_t *const program = module + programs_at;
    const uint32_t type = Word(program);
    const uint32_t entry = Word(program + 4);
    const uint32_t constants_at = Word(program + 12);
    const uint32_t constants_bytes = Word(program + 16);
    // NVN's stage numbers are the DKSH's program types, vertex 0 to tessellation evaluation 4.
    if (type > (uint32_t)NVN_SHADER_STAGE_TESS_EVALUATION) {
        LayerComplain("%s: a DKSH program of type %u, and the layer takes the graphics stages, 0 "
                      "to %d: it runs no compute yet",
                      WHAT, (unsigned)type, (int)NVN_SHADER_STAGE_TESS_EVALUATION);
        return false;
    }
    if (entry < 0x30u || entry >= code_bytes) {
        LayerComplain("%s: the program starts 0x%x bytes into a code section of 0x%x, where an "
                      "NVN program starts 0x30 or more in",
                      WHAT, (unsigned)entry, (unsigned)code_bytes);
        return false;
    }
    if (type == (uint32_t)NVN_SHADER_STAGE_VERTEX && Word(program + 24) != 0u) {
        LayerComplain("%s: a vertex program with a second entry point at 0x%x, which the layer "
                      "does not take yet",
                      WHAT, (unsigned)Word(program + 24));
        return false;
    }
    // Without constants of its own a program's constants start where its code area ends, with
    // no bytes: what the section has to say either way is an offset on a 0x100 boundary.
    uint32_t constants = code_bytes;
    if (constants_bytes != 0u) {
        if ((constants_at & 0xffu) != 0u || constants_at < entry || constants_at > code_bytes ||
            constants_bytes > code_bytes - constants_at) {
            LayerComplain("%s: the program's constants are 0x%x bytes at 0x%x of a code section "
                          "of 0x%x that starts its program at 0x%x",
                          WHAT, (unsigned)constants_bytes, (unsigned)constants_at,
                          (unsigned)code_bytes, (unsigned)entry);
            return false;
        }
        constants = constants_at;
    }

    uint8_t *const section = control;
    memset(section, 0, NVN_LAYER_CONTROL_BYTES);
    Put(section, LAYER_SECTION_AT_MAGIC, LAYER_SECTION_MAGIC);
    Put(section, LAYER_SECTION_AT_ONE, 1u);
    Put(section, LAYER_SECTION_AT_FORMAT, LAYER_SECTION_FORMAT);
    Put(section, LAYER_SECTION_AT_CONSTANTS, constants);
    Put(section, LAYER_SECTION_AT_BYTES, code_bytes);
    Put(section, LAYER_SECTION_AT_PROGRAM, entry);
    Put(section, LAYER_SECTION_AT_STAGE, type);
    if (type == (uint32_t)NVN_SHADER_STAGE_FRAGMENT) {
        // The DKSH's early_fragment_tests, the second byte of the fragment program's own words.
        section[LAYER_SECTION_AT_EARLY_TESTS] = program[25] != 0u ? 1u : 0u;
    }

    if (type == (uint32_t)NVN_SHADER_STAGE_TESS_EVALUATION) {
        // The DKSH keeps the declarations as the 3D engine's tessellation mode word, the first
        // of the program's own: the domain in bits 0 and 1 (isolines, triangles, quads), the
        // spacing in bits 4 and 5, and what comes out in bits 8 and 9 (points, lines, triangles
        // of one winding, triangles of the other).
        static const uint32_t DOMAINS[4] = { 0x8e7au, 4u, 7u, 0x8e7au };
        static const uint32_t SPACINGS[4] = { 0x202u, 0x8e7bu, 0x8e7cu, 0x202u };
        const uint32_t mode = Word(program + 24);
        const uint32_t output = mode >> 8 & 3u;
        Put(section, LAYER_SECTION_AT_TESS_DOMAIN, DOMAINS[mode & 3u]);
        Put(section, LAYER_SECTION_AT_TESS_SPACING, SPACINGS[mode >> 4 & 3u]);
        Put(section, LAYER_SECTION_AT_TESS_POINTS, output == 0u ? 1u : 0u);
        Put(section, LAYER_SECTION_AT_TESS_WINDING, output == 3u ? 0x900u : 0x901u);
    }

    LayerControlTail *const tail = (LayerControlTail *)(void *)(section + LAYER_CONTROL_TAIL);
    tail->tag = LAYER_CONTROL_TAG;
    for (uint32_t i = 0; i < LAYER_DKSH_HEADER_WORDS; ++i) {
        tail->dksh[i] = Word(module + 4u * i);
    }
    // The program's header follows the DKSH's in the tail, and the header says so.
    tail->dksh[4] = 4u * LAYER_DKSH_HEADER_WORDS;
    for (uint32_t i = 0; i < LAYER_DKSH_PROGRAM_WORDS; ++i) {
        tail->program[i] = Word(program + 4u * i);
    }

    code->code = module + control_bytes;
    code->code_bytes = code_bytes;
    return true;
}
