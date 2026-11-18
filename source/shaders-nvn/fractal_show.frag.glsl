// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The fractal zooms through a game's NVN (mod/fractal_nvn.c): the frame shown on the window's
// image, the target at twice its size as the plain program's quad shows it, coloured from the
// counts as fractal.frag colours the target. The window shows an image's first row at the
// bottom, NVN's lower left origin, so the plane's lowest row is drawn there. Built only for NVN's
// layout (uam --nvn, mod.mk).

#version 460

layout (location = 0) out vec4 outColor;

layout (std140, binding = 0) uniform Frame
{
    vec2 centre;
    float step;
    uint iterations;
    vec2 seed;
    uint julia;
    uint shift;
    uint width;
    uint height;
    uvec2 unused;
    uint palette[64];
} f;

layout (std430, binding = 0) readonly buffer Counts
{
    uint counts[];
};

void main()
{
    uvec2 at = uvec2(gl_FragCoord.xy) / 2u;
    at.y = f.height - 1u - at.y;
    uint n = counts[at.y * f.width + at.x];
    uint index = (n + f.shift) & 63u;
    uint word = n >= f.iterations ? 0xff000000u : f.palette[index];
    outColor = unpackUnorm4x8(word);
}
