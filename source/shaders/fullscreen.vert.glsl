// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One triangle over the whole target from three vertices with no attributes: (-1, -1), (3, -1)
// and (-1, 3) in clip space, by the vertex's number. A pass that shades every pixel once needs
// no vertex buffer, so any backend issues it as a draw of three vertices.

#version 460

void main()
{
    vec2 corner = vec2(float((gl_VertexID & 1) * 4 - 1), float((gl_VertexID & 2) * 2 - 1));
    gl_Position = vec4(corner, 0.0, 1.0);
}
