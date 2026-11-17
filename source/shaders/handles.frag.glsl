// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The nvn suite's fragment stage for the handles a device makes apart from the one of a
// texture with its sampler (t_nvncmd.c). It follows the NVN layer's rules for a shader
// (docs/NvnLayer.md): the stage's texture handles are the block at binding 0, slot r in the 64
// bits at 0x20 + 8 * r, and its own uniform block is binding 1.
//
// Mode 0 samples through a texture's handle in slot 1 and a sampler's in slot 2, put together
// as a console's compiled shader puts a separate pair together: the low words of the two, one
// over the other. The coordinates run to 2, so the sampler's wrap shows. Mode 1 fetches texels,
// with no sampler, through the texel fetch handle in slot 3.

#version 460
#extension GL_ARB_bindless_texture : require

layout (location = 0) in vec2 inUv;
layout (location = 1) flat in vec4 inColor;

layout (location = 0) out vec4 outColor;

layout (std140, binding = 0) uniform Handles
{
    uvec4 lead[2];
    uvec4 pair[8];
} handles;

layout (std140, binding = 1) uniform Paint
{
    vec4 color;
    ivec4 mode;
} paint;

void main()
{
    if (paint.mode.x == 0) {
        outColor = texture(sampler2D(uvec2(handles.pair[0].z | handles.pair[1].x, 0u)),
                           inUv * 2.0);
    } else {
        outColor = texelFetch(sampler2D(uvec2(handles.pair[1].z, 0u)), ivec2(inUv * 4.0), 0);
    }
}
