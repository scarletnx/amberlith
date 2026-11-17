// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The Wimsy levels' model (wimsyref.h, WimsyInstance): a corner to clip space by the draw's
// transform, its normal turned into the world by the rows of the model's rotation, and its colour
// times the draw's tint, flat since a triangle's three corners share one.

#version 460

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec4 inColor;

layout (location = 0) out vec3 outNormal;
layout (location = 1) flat out vec4 outColor;

layout (std140, binding = 0) uniform Instance
{
    mat4 toClip;
    vec4 turn[3];
    vec4 tint;
} u;

void main()
{
    gl_Position = u.toClip * vec4(inPos, 1.0);
    outNormal = vec3(dot(u.turn[0].xyz, inNormal), dot(u.turn[1].xyz, inNormal),
                     dot(u.turn[2].xyz, inNormal));
    outColor = inColor * u.tint;
}
