// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The demo family's fractal zooms, the second pass: each pixel reads the count the compute pass
// stored for it and shows that count's colour of the frame's palette, turned by the frame's
// shift, or black where the iteration never let the pixel go. Integers to a colour of eight
// bit channels: every pixel has one right answer.
//
// The palette is an array of scalars, each 16 bytes from the next as std140 lays them out, and
// never a vector indexed by a component it works out: uam 1.1.0 compiles palette[i >> 2][i & 3]
// to a load at the vector's address with the component's bytes masked off, so it reads the
// vector's first word whatever i & 3 is.

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
    uvec2 at = uvec2(gl_FragCoord.xy);
    uint n = counts[at.y * f.width + at.x];
    uint index = (n + f.shift) & 63u;
    uint word = n >= f.iterations ? 0xff000000u : f.palette[index];
    outColor = unpackUnorm4x8(word);
}
