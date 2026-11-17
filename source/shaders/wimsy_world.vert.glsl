// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The Wimsy levels' world (wimsyref.h, WimsyWorldVertex): a corner in the world to clip space
// by the frame's transform, its texture coordinate as it is and its colour flat, the outputs
// the texture and flat fragment shaders read.

#version 460

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUv;
layout (location = 2) in vec4 inColor;

layout (location = 0) out vec2 outUv;
layout (location = 1) flat out vec4 outColor;

layout (std140, binding = 0) uniform View
{
    mat4 toClip;
} u;

void main()
{
    gl_Position = u.toClip * vec4(inPos, 1.0);
    outUv = inUv;
    outColor = inColor;
}
