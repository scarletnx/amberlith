// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// One level of one of the heavy levels' compute textures (wimsyref.h, WimsyComputeTexel): every
// texel a whole number from its place, its texture, its level and the frame, stored as the
// normalised value that byte is.

#version 460

layout (local_size_x = 8, local_size_y = 8) in;

layout (binding = 0) writeonly uniform image2D img;

layout (std140, binding = 0) uniform Params
{
    // the texture, the level, the frame, and the level's side
    uvec4 seed;
} p;

void main()
{
    uvec2 at = gl_GlobalInvocationID.xy;
    // Inside the test rather than after an early return, which uam compiles to a PRET and RET:
    // a translator without PRET drops the pipeline (the geometry family's square twins judge it).
    if (at.x < p.seed.w && at.y < p.seed.w) {
        uint r = (at.x * 5u + p.seed.z * 3u + p.seed.x * 16u) & 255u;
        uint g = (at.y * 7u + p.seed.z + p.seed.y * 32u) & 255u;
        uint b = ((at.x ^ at.y) * 9u + p.seed.x * 8u + p.seed.z * 2u) & 255u;
        imageStore(img, ivec2(at), vec4(float(r), float(g), float(b), 255.0) / 255.0);
    }
}
