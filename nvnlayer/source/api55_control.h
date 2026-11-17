// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

#include "nvnlayer.h"

// A stage's control section as NvnLayerShaderControl writes it, for the two that read it:
// ScarletNX, which reads the first part as it reads a game's, and the API 55 build's
// nvnProgramSetShaders, which reads the layer's own part after it. No deko3d type is named
// here, so the writer also builds where the program's own tests run.

// What ScarletNX's nvnProgramSetShaders reads of a control section (its nvn_object.c, stage_of):
// the magic, a one, the format, and then, far into the section, the constants' offset in the
// code area, the code area's bytes, the program's offset in it and the stage. A fragment stage
// also says whether it asked for the early tests.
#define LAYER_SECTION_MAGIC 0x98761234u
#define LAYER_SECTION_FORMAT 8u
#define LAYER_SECTION_AT_MAGIC 0x000u
#define LAYER_SECTION_AT_ONE 0x004u
#define LAYER_SECTION_AT_FORMAT 0x008u
#define LAYER_SECTION_AT_CONSTANTS 0x700u
#define LAYER_SECTION_AT_BYTES 0x704u
#define LAYER_SECTION_AT_PROGRAM 0x708u
#define LAYER_SECTION_AT_STAGE 0x714u
#define LAYER_SECTION_AT_EARLY_TESTS 0x718u
// A tessellation evaluation stage's four declarations, each in the word GLSL names it by, as
// ScarletNX reads them (its nvn_object.c, tess_of): the domain (triangles 4, quads 7, isolines
// 0x8e7a), the spacing (equal 0x202, fractional odd 0x8e7b, fractional even 0x8e7c), whether it
// makes points, and the winding (clockwise 0x900, counterclockwise 0x901).
#define LAYER_SECTION_AT_TESS_DOMAIN 0x738u
#define LAYER_SECTION_AT_TESS_SPACING 0x73cu
#define LAYER_SECTION_AT_TESS_POINTS 0x740u
#define LAYER_SECTION_AT_TESS_WINDING 0x744u

// What ScarletNX reads of the capture a stage's outputs go through, transform feedback (its
// nvn_object.c, xfb_of): a byte that says the stage captures; for buffer i of four its stream,
// its stride (16 bits) and how many groups it has; and for group j of a buffer the output it
// takes words of, the first of them and how many.
#define LAYER_SECTION_AT_CAPTURES 0x6ecu
#define LAYER_SECTION_AT_CAPTURE_STREAM 0x6e4u
#define LAYER_SECTION_AT_CAPTURE_STRIDE 0x6dcu
#define LAYER_SECTION_AT_CAPTURE_GROUPS 0x6e8u
#define LAYER_SECTION_AT_GROUP_OUTPUT 0x0dcu
#define LAYER_SECTION_AT_GROUP_FIRST 0x2dcu
#define LAYER_SECTION_AT_GROUP_COUNT 0x4dcu
#define LAYER_SECTION_CAPTURE_BUFFERS 4
#define LAYER_SECTION_GROUPS 0x80

// A DKSH's header and its program's, in words (deko3d's source/dksh.h): the magic, the header's
// bytes, the control and code sections' bytes, the programs' offset and count; then, for a
// program, its type, entry point, registers, the constants' offset and bytes.
#define LAYER_DKSH_MAGIC 0x48534b44u
#define LAYER_DKSH_HEADER_WORDS 6u
#define LAYER_DKSH_PROGRAM_WORDS 16u

/// The layer's part of the section, LAYER_CONTROL_TAIL bytes in: the DKSH's header with its one
/// program's right after it, which is what dkShaderInitialize takes as a control section apart
/// from the code, and room for the DkShader the API 55 build's nvnProgramSetShaders makes of it.
#define LAYER_CONTROL_TAG 0x53434c4eu // "NLCS"
#define LAYER_CONTROL_TAIL 0x800u
typedef struct {
    uint32_t tag;
    uint32_t reserved;
    uint32_t dksh[LAYER_DKSH_HEADER_WORDS];
    uint32_t program[LAYER_DKSH_PROGRAM_WORDS];
    uint32_t pad[8];
    _Alignas(8) uint8_t shader[128];
} LayerControlTail;
_Static_assert(LAYER_CONTROL_TAIL + sizeof(LayerControlTail) == NVN_LAYER_CONTROL_BYTES,
               "the section's two parts are the bytes the header promises");
