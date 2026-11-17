// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// A puff of smoke (wimsyref.h, WimsyPuff): a square that faces the camera, its four corners at
// its middle's depth, its half side in the world's units made clip space's by the view's scale.

#version 460

layout (location = 0) in vec2 inUnit;
// its middle and its half side
layout (location = 1) in vec4 inPuff;
layout (location = 2) in vec4 inColor;

layout (location = 0) out vec2 outUv;
layout (location = 1) flat out vec4 outColor;

layout (std140, binding = 0) uniform View
{
    mat4 toClip;
    // the wind of the frame in x; in y and z a puff's reach across and up, WimsyPuffScale
    vec4 wind;
} u;

void main()
{
    vec4 middle = u.toClip * vec4(inPuff.xyz, 1.0);
    gl_Position = vec4(middle.xy + inUnit * inPuff.w * u.wind.yz, middle.zw);
    outUv = inUnit;
    outColor = inColor;
}
