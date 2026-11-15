// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The fragment stage of the scene the nvn suite draws down both of the program's ways to the
// graphics unit (source/drawpath.h). A uniform buffer picks where the colour comes from, and no
// arithmetic touches it, so every pixel is exact: a texture read, the vertex's flat colour, or
// the buffer's own.
//
// The two rules of docs/NvnLayer.md, "What a shader may use": binding = 1 is NVN's uniform
// buffer slot 0, and a texture is sampled through its handle in the block at binding 0, slot r's
// at 0x20 + 8 * r. The texture here is slot 1, the second half of the first pair; slot 0 holds
// another texture, which a read of the wrong handle shows.

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
        outColor = texture(sampler2D(handles.pair[0].zw), inUv);
    } else if (paint.mode.x == 1) {
        outColor = inColor;
    } else {
        outColor = paint.color;
    }
}
