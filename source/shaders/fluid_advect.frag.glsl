// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The fluid demo's advection pass: the field's texel traced back along the velocity and sampled
// there, scaled by k.x. Both samples are made by hand from the four nearest texels: the texture
// unit's own filter weighs them at the precision its hardware has (Vulkan asks for four bits
// below a texel), and the processor's reference could follow none of them.
// demo_fluidref.c's FluidAdvectPass; size.xy is the field's, size.zw the velocity's, and k the
// keep, the step and offset from a field texel to a velocity cell, and the field's texels a cell.

#version 460

layout (location = 0) out vec4 outValue;

layout (binding = 0) uniform sampler2D velocity;
layout (binding = 1) uniform sampler2D field;

layout (std140, binding = 0) uniform Pass
{
    ivec4 size;
    vec4 k;
} pass;

vec4 Bilerp(sampler2D s, vec2 at, ivec2 size)
{
    const vec2 p = clamp(at, vec2(0.0), vec2(size - 1));
    const vec2 p0 = floor(p);
    const vec2 f = p - p0;
    const ivec2 i0 = ivec2(p0);
    const ivec2 i1 = min(i0 + 1, size - 1);
    const vec4 a = texelFetch(s, i0, 0);
    const vec4 b = texelFetch(s, ivec2(i1.x, i0.y), 0);
    const vec4 c = texelFetch(s, ivec2(i0.x, i1.y), 0);
    const vec4 d = texelFetch(s, i1, 0);
    const vec4 top = (b - a) * f.x + a;
    const vec4 bottom = (d - c) * f.x + c;
    return (bottom - top) * f.y + top;
}

void main()
{
    const ivec2 c = ivec2(gl_FragCoord.xy);
    const vec2 at = vec2(c) * pass.k.y + pass.k.z;
    const vec2 u = Bilerp(velocity, at, pass.size.zw).xy;
    const vec2 back = -pass.k.w * u + vec2(c);
    outValue = Bilerp(field, back, pass.size.xy) * pass.k.x;
}
