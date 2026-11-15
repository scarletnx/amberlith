// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// A blade of grass (wimsyref.h, WimsyBlade): the same three corners for every blade, placed by
// its instance's foot, height and lean, its tip swaying in the frame's wind by where the blade
// stands, the sums WimsyBladeCorner does on the same floats.

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
    // the wind of the frame in x, WimsyWindOf
    vec4 wind;
} u;

// WIMSY_BLADE_HALF_WIDTH and twice WIMSY_BLADE_SWAY
const float HALF_WIDTH = 0.0625;
const float SWAY = 0.1875;

void main()
{
    float phase = u.wind.x + inBlade.x * 0.125 + inBlade.y * 0.0625;
    phase -= floor(phase);
    float sway = (abs(phase * 2.0 - 1.0) - 0.5) * SWAY;
    vec3 at = vec3(inBlade.x + inUnit.x * HALF_WIDTH + inUnit.y * (inBlade.w + sway),
                   inUnit.y * inBlade.z, inBlade.y);
    gl_Position = u.toClip * vec4(at, 1.0);
    outUv = vec2(0.0);
    outColor = inColor;
}
