// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The fluid demo's gradient pass: the velocity less half the central differences of the
// pressure, a wall's neighbour the cell's own. demo_fluidref.c's FluidGradientPass.

#version 460

layout (location = 0) out vec4 outValue;

layout (binding = 0) uniform sampler2D velocity;
layout (binding = 1) uniform sampler2D pressure;

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
    const vec2 u = texelFetch(velocity, c, 0).xy;
    outValue = vec4(-0.5 * (right - left) + u.x, -0.5 * (down - up) + u.y, 0.0, 1.0);
}
