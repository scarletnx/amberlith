// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// A puff of smoke's colour, thinning from its middle to nothing at its rim (wimsyref.h,
// WimsyPuff): what it hides of the frame behind it is its alpha times 1 - u*u - v*v.

#version 460

layout (location = 0) in vec2 inUv;
layout (location = 1) flat in vec4 inColor;

layout (location = 0) out vec4 outColor;

void main()
{
    float fall = max(1.0 - dot(inUv, inUv), 0.0);
    outColor = vec4(inColor.rgb, inColor.a * fall);
}
