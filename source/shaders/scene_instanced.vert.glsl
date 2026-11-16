// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The vertex stage of the instanced scene the nvn suite draws down the program's ways to the
// graphics unit (source/drawpath.h): a vertex's position moved by its instance's place, and
// the instance's colour handed on flat. The place and the colour come from a second vertex
// stream that steps once an instance (DrawPathInstances), so an instance that reads another's
// entry lands in another cell in another colour.

#version 460

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUv;
layout (location = 2) in vec4 inColor;
layout (location = 3) in vec4 inPlace;
layout (location = 4) in vec4 inTint;

layout (location = 0) out vec2 outUv;
layout (location = 1) flat out vec4 outColor;

void main()
{
    gl_Position = vec4(inPos + inPlace.xyz, 1.0);
    outUv = inUv;
    outColor = inTint;
}
