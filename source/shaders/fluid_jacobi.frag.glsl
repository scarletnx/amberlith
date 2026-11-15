// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One Jacobi pass of the fluid demo's pressure solve: a quarter of the four neighbours' pressures,
// their sum scaled by k.x (the first pass's start from the last frame's pressure), less the
// divergence; a wall's neighbour is the cell's own. demo_fluidref.c's FluidJacobiPass.

#version 460

layout (location = 0) out vec4 outValue;

layout (binding = 0) uniform sampler2D pressure;
layout (binding = 1) uniform sampler2D divergence;

layout (std140, binding = 0) uniform Pass
{
    ivec4 size;
    vec4 k;
} pass;

void main()
{
    const ivec2 c = ivec2(gl_FragCoord.xy);
    const ivec2 last = pass.size.xy - 1;
    const float left = texelFetch(pressure, ivec2(max(c.x - 1, 0), c.y), 0).x;
    const float right = texelFetch(pressure, ivec2(min(c.x + 1, last.x), c.y), 0).x;
    const float up = texelFetch(pressure, ivec2(c.x, max(c.y - 1, 0)), 0).x;
    const float down = texelFetch(pressure, ivec2(c.x, min(c.y + 1, last.y)), 0).x;
    const float sum = ((left + right) + up) + down;
    const float d = texelFetch(divergence, c, 0).x;
    outValue = vec4((pass.k.x * sum - d) * 0.25, 0.0, 0.0, 1.0);
}
