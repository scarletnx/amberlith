// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The fluid demo's divergence pass: half the central differences of the velocity, a wall's
// neighbour taking the cell's own velocity reversed, so nothing flows through it.
// demo_fluidref.c's FluidDivergencePass.

#version 460

layout (location = 0) out vec4 outValue;

layout (binding = 0) uniform sampler2D velocity;

layout (std140, binding = 0) uniform Pass
{
    ivec4 size;
    vec4 k;
} pass;

void main()
{
    const ivec2 c = ivec2(gl_FragCoord.xy);
    const vec2 here = texelFetch(velocity, c, 0).xy;
    const float left = c.x > 0 ? texelFetch(velocity, c - ivec2(1, 0), 0).x : -here.x;
    const float right = c.x + 1 < pass.size.x ? texelFetch(velocity, c + ivec2(1, 0), 0).x : -here.x;
    const float up = c.y > 0 ? texelFetch(velocity, c - ivec2(0, 1), 0).y : -here.y;
    const float down = c.y + 1 < pass.size.y ? texelFetch(velocity, c + ivec2(0, 1), 0).y : -here.y;
    outValue = vec4(((right - left) + (down - up)) * 0.5, 0.0, 0.0, 1.0);
}
