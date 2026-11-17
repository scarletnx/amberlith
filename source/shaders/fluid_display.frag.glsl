// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the fluid demo shows: the dye, filtered by the texture unit, each colour taken to the
// screen as 1 - e^(-1.5 x) and its gamma. Not judged: the presented image is the compositor's.

#version 460

layout (location = 0) in vec2 inUv;
layout (location = 1) flat in vec4 inColor;

layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform sampler2D dye;

void main()
{
    const vec3 x = max(texture(dye, inUv).rgb, vec3(0.0));
    const vec3 lit = 1.0 - exp(-1.5 * x);
    outColor = vec4(pow(lit, vec3(1.0 / 2.2)) + vec3(0.02, 0.02, 0.03), 1.0);
}
