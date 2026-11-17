// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The fluid demo's splat pass: the field's texel with each splat added, (1 - d^2 / r^2)^3 of its
// value at a distance d inside its radius. demo_fluidref.c's FluidSplatPass, operation for
// operation; a polynomial kernel, so no transcendental reaches the simulation.

#version 460

layout (location = 0) out vec4 outValue;

layout (binding = 0) uniform sampler2D field;

struct Splat
{
    vec4 at;
    vec4 value;
};

layout (std140, binding = 0) uniform Splats
{
    uvec4 count;
    Splat s[4];
} p;

void main()
{
    const ivec2 c = ivec2(gl_FragCoord.xy);
    vec4 v = texelFetch(field, c, 0);
    const vec2 at = vec2(c);
    for (uint i = 0u; i < p.count.x; ++i) {
        const float dx = at.x - p.s[i].at.x;
        const float dy = at.y - p.s[i].at.y;
        const float d2 = dy * dy + dx * dx;
        const float t = 1.0 - d2 * p.s[i].at.z;
        if (t > 0.0) {
            v = p.s[i].value * (t * t * t) + v;
        }
    }
    outValue = v;
}
