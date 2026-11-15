// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// wimsy_lit.frag's light, as a program of its own: a heavy frame's model is several materials,
// each its own fragment program. What tells this one from the others is a constant times
// shade.z, which the level leaves nought, so no pixel shows it.

#version 460

layout (location = 0) in vec3 inNormal;
layout (location = 1) flat in vec4 inColor;

layout (location = 0) out vec4 outColor;

layout (std140, binding = 0) uniform Light
{
    vec4 light;
    vec4 shade;
} l;

void main()
{
    float facing = max(dot(normalize(inNormal), l.light.xyz), 0.0);
    outColor = vec4(inColor.rgb * (l.shade.x + l.shade.y * facing) + l.shade.z * 2.0, 1.0);
}
