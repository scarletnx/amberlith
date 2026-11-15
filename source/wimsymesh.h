// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Wimsy, ScarletNX's mascot, as a mesh: her drawing (static/wimsy.svg, by Liz) with each leaf a
/// slab and each eye a short cylinder, made by tools/wimsy-mesh.py into wimsymesh.c. The game
/// family's levels draw her (wimsyref.h), the first from the front as a sprite, the rest in three
/// dimensions.
///
/// The model spans -1 to 1 across the drawing, x to the right, y up, z towards the eye. Every
/// part is closed and wound counter clockwise seen from outside, so a machine may cull the faces
/// turned away. The triangles are in the order the drawing paints them: those facing +z, drawn
/// in order with no depth test, give the drawing.

#define WIMSY_MESH_VERTICES 1694u
#define WIMSY_MESH_INDICES 5040u
#define WIMSY_MESH_COLOURS 10u
#define WIMSY_MESH_PARTS 9u

/// A position's whole numbers to the model's units: 8192 to one.
#define WIMSY_MESH_SCALE 8192

typedef struct {
    /// The position, WIMSY_MESH_SCALE to one unit.
    int16_t x, y, z;
    /// The normal, 127 to one: its length is one to within a hundredth.
    int8_t nx, ny, nz;
    /// The colour, by its place in WIMSY_MESH_PALETTE. The three corners of a triangle share it.
    uint8_t colour;
} WimsyMeshVertex;

/// A leaf or an eye: its name and its indices, a closed surface of its own.
typedef struct {
    const char *name;
    uint32_t first_index;
    uint32_t index_count;
} WimsyMeshPart;

extern const uint8_t WIMSY_MESH_PALETTE[WIMSY_MESH_COLOURS][3];
extern const WimsyMeshPart WIMSY_MESH_PART[WIMSY_MESH_PARTS];
extern const WimsyMeshVertex WIMSY_MESH_VERTEX[WIMSY_MESH_VERTICES];
extern const uint16_t WIMSY_MESH_INDEX[WIMSY_MESH_INDICES];

#ifdef __cplusplus
}
#endif
