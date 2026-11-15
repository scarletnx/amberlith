// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The demo family's fractal zooms, the first of a frame's two passes: one invocation a pixel
// iterates z = z * z + c until |z| passes 2 or the frame's most iterations are spent, and
// stores how many it took. The Mandelbrot set starts z at nought and takes c from the pixel;
// a Julia set starts z at the pixel and takes c from the frame.
//
// Every value is precise and every multiply and add is written as the fma it compiles to, so
// the program is one sequence of single precision operations, each rounded once, that the
// processor repeats exactly (fractalref.c, FractalIterate): no compiler may fuse, split or
// reorder them. A machine that does a fused multiply and add as a multiply then an add rounds
// twice and lets other pixels go at other iterations.

#version 460

layout (local_size_x = 8, local_size_y = 8) in;

layout (std140, binding = 0) uniform Frame
{
    vec2 centre;
    float step;
    uint iterations;
    vec2 seed;
    uint julia;
    uint shift;
    uint width;
    uint height;
    uvec2 unused;
    uint palette[64];
} f;

layout (std430, binding = 0) writeonly buffer Counts
{
    uint counts[];
};

void main()
{
    uvec2 at = gl_GlobalInvocationID.xy;
    if (at.x >= f.width || at.y >= f.height) {
        return;
    }
    // The pixel's centre from the middle of the target, exact: whole numbers and halves.
    precise float px = float(at.x) - (float(f.width / 2u) - 0.5);
    precise float py = float(at.y) - (float(f.height / 2u) - 0.5);
    // The top row is the plane's highest.
    precise float ax = fma(px, f.step, f.centre.x);
    precise float ay = fma(py, -f.step, f.centre.y);
    precise float x = f.julia != 0u ? ax : 0.0;
    precise float y = f.julia != 0u ? ay : 0.0;
    precise float cx = f.julia != 0u ? f.seed.x : ax;
    precise float cy = f.julia != 0u ? f.seed.y : ay;
    uint n = 0u;
    for (; n < f.iterations; ++n) {
        precise float yy = y * y;
        precise float r2 = fma(x, x, yy);
        if (r2 > 4.0) {
            break;
        }
        precise float t = fma(-y, y, cx);
        precise float nx = fma(x, x, t);
        precise float ny = fma(x + x, y, cy);
        x = nx;
        y = ny;
    }
    counts[at.y * f.width + at.x] = n;
}
