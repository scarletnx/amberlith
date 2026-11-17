// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// One light from a direction (wimsyref.h, WimsyLight): a face keeps shade.x of its colour turned
// away from it and gains shade.y of it turned to it, by the cosine between.

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
    outColor = vec4(inColor.rgb * (l.shade.x + l.shade.y * facing), 1.0);
}
