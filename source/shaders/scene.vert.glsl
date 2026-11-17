// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The vertex stage of the scene the nvn suite draws down both of the program's ways to the
// graphics unit (source/drawpath.h): common.vert.glsl with the position moved by a uniform
// buffer, so a draw that got another draw's buffer lands somewhere else.
//
// binding = 1 is NVN's uniform buffer slot 0 (docs/NvnLayer.md, "What a shader may use").

#version 460

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUv;
layout (location = 2) in vec4 inColor;

layout (location = 0) out vec2 outUv;
layout (location = 1) flat out vec4 outColor;

layout (std140, binding = 1) uniform Move
{
    vec4 offset;
} move;

void main()
{
    gl_Position = vec4(inPos + move.offset.xyz, 1.0);
    outUv = inUv;
    outColor = inColor;
}
