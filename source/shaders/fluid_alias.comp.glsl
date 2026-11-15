// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The fluid demo's aliasing level: stores frame * 1024 + 32 y + x + 1 at each word of a 32 by 32
// R32_Uint storage image whose bytes the dye's splat target, and that target drawn a shift on,
// also cover. demo_fluidref.c's FluidAliasWord.

#version 460

layout (local_size_x = 8, local_size_y = 8) in;

layout (binding = 0, r32ui) writeonly uniform uimage2D words;

layout (std140, binding = 0) uniform Frame
{
    uvec4 frame;
} f;

void main()
{
    const ivec2 c = ivec2(gl_GlobalInvocationID.xy);
    imageStore(words, c, uvec4(f.frame.x * 1024u + uint(c.y) * 32u + uint(c.x) + 1u));
}
