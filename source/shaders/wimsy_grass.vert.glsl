// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// A blade of a heavy level's grass (wimsyref.h, WimsyBlade): the same three corners for every
// blade, placed by its instance's foot, height and lean. A term of each sum is nought, so the
// place is the float the reference works out.

#version 460

layout (location = 0) in vec2 inUnit;
// its foot's x and z, its height, its lean
layout (location = 1) in vec4 inBlade;
layout (location = 2) in vec4 inColor;

layout (location = 0) out vec2 outUv;
layout (location = 1) flat out vec4 outColor;

layout (std140, binding = 0) uniform View
{
    mat4 toClip;
} u;

// WIMSY_BLADE_HALF_WIDTH
const float HALF_WIDTH = 0.0625;

void main()
{
    vec3 at = vec3(inBlade.x + inUnit.x * HALF_WIDTH + inUnit.y * inBlade.w,
                   inUnit.y * inBlade.z, inBlade.y);
    gl_Position = u.toClip * vec4(at, 1.0);
    outUv = vec2(0.0);
    outColor = inColor;
}
