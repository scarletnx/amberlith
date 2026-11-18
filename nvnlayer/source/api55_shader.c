// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
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
    // NVN's stage numbers are the DKSH's program types, vertex 0 to compute 5.
    if (type > (uint32_t)NVN_SHADER_STAGE_COMPUTE) {
        LayerComplain("%s: a DKSH program of type %u, where NVN's stages are 0 to %d", WHAT,
                      (unsigned)type, (int)NVN_SHADER_STAGE_COMPUTE);
        return false;
    }
    const bool compute = type == (uint32_t)NVN_SHADER_STAGE_COMPUTE;
    // A graphics program follows the 0x30 bytes before its header; ScarletNX takes a compute
    // program from the start of the code area too, as an older SDK lays one out.
    if ((!compute && entry < 0x30u) || entry >= code_bytes) {
        LayerComplain("%s: the program starts 0x%x bytes into a code section of 0x%x, where an "
                      "NVN %s program starts %s in",
                      WHAT, (unsigned)entry, (unsigned)code_bytes,
                      compute ? "compute" : "graphics", compute ? "inside it" : "0x30 or more");
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
    Put(section, LAYER_SECTION_AT_DRIVER_WORDS, LAYER_SECTION_DRIVER_WORD_0);
    Put(section, LAYER_SECTION_AT_DRIVER_WORDS + 4u, LAYER_SECTION_DRIVER_WORD_1);
    Put(section, LAYER_SECTION_AT_CONSTANTS, constants);
    Put(section, LAYER_SECTION_AT_BYTES, code_bytes);
    Put(section, LAYER_SECTION_AT_PROGRAM, entry);
    Put(section, LAYER_SECTION_AT_STAGE, type);
    if (type == (uint32_t)NVN_SHADER_STAGE_FRAGMENT) {
        // The DKSH's early_fragment_tests, the second byte of the fragment program's own words.
        section[LAYER_SECTION_AT_EARLY_TESTS] = program[25] != 0u ? 1u : 0u;
    }
    if (compute) {
        for (uint32_t i = 0; i < 3u; ++i) {
            Put(section, LAYER_SECTION_AT_BLOCK + 4u * i, Word(program + 24u + 4u * i));
        }
        Put(section, LAYER_SECTION_AT_SHARED, Word(program + 36));
        Put(section, LAYER_SECTION_AT_CONSTANT_BYTES, constants_bytes);
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

bool NvnLayerShaderCapture(void *control, int buffer, const NvnLayerCaptureGroup *groups,
                           int count) {
    static const char *const WHAT = "NvnLayerShaderCapture";
    uint8_t *const section = control;
    if (section == NULL || Word(section + LAYER_SECTION_AT_MAGIC) != LAYER_SECTION_MAGIC) {
        LayerComplain("%s: a section NvnLayerShaderControl wrote is needed", WHAT);
        return false;
    }
    if (buffer < 0 || buffer >= LAYER_SECTION_CAPTURE_BUFFERS || groups == NULL || count < 1 ||
        count > LAYER_SECTION_GROUPS) {
        LayerComplain("%s: %d groups for buffer %d, where a stage captures 1 to %d groups into "
                      "each of buffers 0 to %d",
                      WHAT, count, buffer, LAYER_SECTION_GROUPS,
                      LAYER_SECTION_CAPTURE_BUFFERS - 1);
        return false;
    }
    uint32_t words = 0;
    for (int i = 0; i < count; ++i) {
        const bool generic = groups[i].output >= NVN_LAYER_CAPTURE_GENERIC(0) &&
                             groups[i].output <= NVN_LAYER_CAPTURE_GENERIC(31);
        if ((groups[i].output != NVN_LAYER_CAPTURE_POSITION && !generic) ||
            groups[i].count < 1 || groups[i].first > 3 || groups[i].count > 4 - groups[i].first) {
            LayerComplain("%s: group %d takes %d words from word %d of output %d, where an "
                          "output is the position, %d, or a generic one, %d to %d, and has "
                          "four words",
                          WHAT, i, groups[i].count, groups[i].first, groups[i].output,
                          NVN_LAYER_CAPTURE_POSITION, NVN_LAYER_CAPTURE_GENERIC(0),
                          NVN_LAYER_CAPTURE_GENERIC(31));
            return false;
        }
        words += groups[i].count;
    }
    const uint32_t at = (uint32_t)buffer * LAYER_SECTION_GROUPS;
    memset(section + LAYER_SECTION_AT_GROUP_OUTPUT + at, 0, LAYER_SECTION_GROUPS);
    memset(section + LAYER_SECTION_AT_GROUP_FIRST + at, 0, LAYER_SECTION_GROUPS);
    memset(section + LAYER_SECTION_AT_GROUP_COUNT + at, 0, LAYER_SECTION_GROUPS);
    for (int i = 0; i < count; ++i) {
        section[LAYER_SECTION_AT_GROUP_OUTPUT + at + (uint32_t)i] = groups[i].output;
        section[LAYER_SECTION_AT_GROUP_FIRST + at + (uint32_t)i] = groups[i].first;
        section[LAYER_SECTION_AT_GROUP_COUNT + at + (uint32_t)i] = groups[i].count;
    }
    // A captured vertex is its words one after the other, so the stride is four bytes a word.
    section[LAYER_SECTION_AT_CAPTURE_STRIDE + 2u * (uint32_t)buffer] = (uint8_t)(4u * words);
    section[LAYER_SECTION_AT_CAPTURE_STRIDE + 2u * (uint32_t)buffer + 1u] =
        (uint8_t)((4u * words) >> 8);
    section[LAYER_SECTION_AT_CAPTURE_STREAM + (uint32_t)buffer] = 0;
    section[LAYER_SECTION_AT_CAPTURE_GROUPS + (uint32_t)buffer] = (uint8_t)count;
    section[LAYER_SECTION_AT_CAPTURES] = 1;
    return true;
}

bool LayerSectionCapture(const uint8_t *section,
                         LayerCapture buffers[LAYER_SECTION_CAPTURE_BUFFERS], bool *captures,
                         const char *what) {
    if (section == NULL || Word(section + LAYER_SECTION_AT_MAGIC) != LAYER_SECTION_MAGIC) {
        LayerComplain("%s: the stage's section at %p is not one NvnLayerShaderControl wrote", what,
                      (const void *)section);
        return false;
    }
    memset(buffers, 0, sizeof(LayerCapture) * LAYER_SECTION_CAPTURE_BUFFERS);
    *captures = section[LAYER_SECTION_AT_CAPTURES] != 0u;
    if (!*captures) {
        return true;
    }
    for (uint32_t buffer = 0; buffer < LAYER_SECTION_CAPTURE_BUFFERS; ++buffer) {
        LayerCapture *const into = &buffers[buffer];
        const uint32_t at = buffer * LAYER_SECTION_GROUPS;
        const uint32_t groups = section[LAYER_SECTION_AT_CAPTURE_GROUPS + buffer];
        into->stride = (uint32_t)section[LAYER_SECTION_AT_CAPTURE_STRIDE + 2u * buffer] |
                       (uint32_t)section[LAYER_SECTION_AT_CAPTURE_STRIDE + 2u * buffer + 1u] << 8;
        into->stream = section[LAYER_SECTION_AT_CAPTURE_STREAM + buffer];
        for (uint32_t group = 0; group < groups && group < LAYER_SECTION_GROUPS; ++group) {
            const uint32_t output = section[LAYER_SECTION_AT_GROUP_OUTPUT + at + group];
            const uint32_t first = section[LAYER_SECTION_AT_GROUP_FIRST + at + group];
            const uint32_t count = section[LAYER_SECTION_AT_GROUP_COUNT + at + group];
            const bool generic = output >= NVN_LAYER_CAPTURE_GENERIC(0) &&
                                 output <= NVN_LAYER_CAPTURE_GENERIC(31);
            if ((output != NVN_LAYER_CAPTURE_POSITION && !generic) || first > 3u ||
                count > 4u - first) {
                LayerComplain("%s: group %u of capture buffer %u takes %u words from word %u of "
                              "output %u, where an output is the position, %d, or a generic "
                              "one, %d to %d, and has four words",
                              what, (unsigned)group, (unsigned)buffer, (unsigned)count,
                              (unsigned)first, (unsigned)output, NVN_LAYER_CAPTURE_POSITION,
                              NVN_LAYER_CAPTURE_GENERIC(0), NVN_LAYER_CAPTURE_GENERIC(31));
                return false;
            }
            for (uint32_t word = first; word < first + count; ++word) {
                if (into->words < LAYER_CAPTURE_WORDS) {
                    into->slots[into->words] =
                        (uint8_t)(generic ? LAYER_CAPTURE_SLOT_GENERIC(
                                                output - NVN_LAYER_CAPTURE_GENERIC(0), word)
                                          : LAYER_CAPTURE_SLOT_POSITION(word));
                }
                ++into->words;
            }
        }
        if (into->words > LAYER_CAPTURE_WORDS) {
            LayerComplain("%s: capture buffer %u takes %u words a vertex, where a buffer takes "
                          "at most %d",
                          what, (unsigned)buffer, (unsigned)into->words, LAYER_CAPTURE_WORDS);
            return false;
        }
    }
    return true;
}
