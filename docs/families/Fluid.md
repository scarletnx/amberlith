# The fluid demo

## Index

* [The passes](#the-passes)
* [The script](#the-script)
* [What is judged](#what-is-judged)
* [Where the budgets come from](#where-the-budgets-come-from)
* [The aliasing level](#the-aliasing-level)
* [What is timed](#what-is-timed)
* [In the browser](#in-the-browser)
* [What rests on reasoning](#what-rests-on-reasoning)

A 2D Navier-Stokes fluid made the way a game makes one: Stam's stable fluids ("Stable Fluids", SIGGRAPH 1999) on a
grid of cells, each frame a chain of fragment passes in ping-pong between two targets a field, as GPU Gems' chapter
38 lays it out (Harris, "Fast Fluid Dynamics Simulation on the GPU", 2004). Three jets and a burst now and then
stir it and put coloured dye in, the dye is what the screen shows, and every frame is presented and timed as the
game scene's are ([game](Game.md)). One of the [demo family](Demo.md)'s demos: only the word `demo` runs it, `all`
leaves it out, and `frames=` sets its length (600 without it). `source/demo_fluidref.c` is every pass on the
processor, operation for operation, and `source/tests/t_demo_fluid.c` the scene.

- **fluid**, eighteen cases, thirteen with `notime`. One says the scene came up (none tier): a graphics device, the
  fields' targets, the shaders, and a swapchain on the default window or the program's screen. Then the frames run,
  inside the case that holds the early frames to the processor's whole simulation; the ten after it hold every
  judged frame to its passes worked out again and to the invariants, one holds each judged frame's fields to the
  next's, and the five last carry the frames' times.
- **fluid-alias**, nineteen cases, fourteen with `notime`: the same demo with its targets drawn over their old
  selves and a compute pass's storage image over them ([The aliasing level](#the-aliasing-level)), and a case more
  for the compute pass's words.

## The passes

The velocity is RG32F and the pressure R32F, on a grid of 128 by 72 cells; the dye RGBA32F at twice that, 256 by
144, which fills 1280 by 720 at five pixels a texel. Each field has two targets, the divergence one. A cell's
centre is its texel's: cell (i, j) is texel (i, j), row j in memory order.

```
 the frame starts from        velocity V0         pressure P0          dye D0
                                  |                    |                   |
 force splats                 V0 -> V1                 |           D0 -> D1   dye splats
 divergence                   V1 -> DIV                |
 40 Jacobi passes                   DIV, P0 -> P1 -> P0 -> ... -> P0 (the 40th, back in P0)
 gradient                     V1, P0 -> V0  (the projected velocity)
 advection                    V0 along V0, x 0.999 -> V1            D1 along V0, x 0.994 -> D0
 shown                                                              D0 -> the swapchain's image
 the next frame starts from   V1                       P0                  D0
```

Every arrow is a pass over the whole of its target, one triangle that covers it, so no pixel lies on an edge
between two. Between a pass and the next that reads its target the list waits for the fragments to finish and
drops the texture cache (`DkBarrier_Fragments`, `DkInvalidateFlags_Image`), as a game's post chain does; the two
splats share one wait and so do the two advections, which read nothing the other writes. Forty seven draws a frame,
in one command list:

| pass | reads | writes | what it does |
|---|---|---|---|
| force splats | velocity | its other target | each splat adds its force, (1 - d^2 / r^2)^3 of it at a distance d inside its radius r |
| dye splats | dye | its other target | the same kernel, the splat's colour |
| divergence | splatted velocity | divergence | half the central differences; a wall's neighbour is the cell's own velocity reversed, so nothing flows through it |
| Jacobi | pressure, divergence | the other pressure target | a quarter of the four neighbours' sum less the divergence; a wall's neighbour is the cell's own pressure; the first pass scales the sum by 0.8, starting from the last frame's pressure as most game fluids do |
| gradient | splatted velocity, pressure | the velocity's first target | the velocity less half the pressure's central differences |
| velocity advection | projected velocity | its other target | each cell traced back along the velocity, the velocity sampled there, times 0.999 |
| dye advection | projected velocity, splatted dye | the dye's first target | each texel traced back along the velocity, sampled there twice as far, times 0.994 |
| shown | dye | the swapchain's image | the texture unit's own filter, each colour 1 - e^(-1.5 x) and its gamma; not judged |

The advections sample by hand: four texel fetches and two lerps in the shader, the point clamped to the edge
texels' centres. The texture unit's own filter weighs its four texels at the precision its hardware has (Vulkan
asks for four bits below a texel, `subTexelPrecisionBits`), and no reference on the processor could follow it
through hundreds of frames; the screen's filter, which nothing judges, is the hardware's.

Every pass is the shader's arithmetic and no other: no division, no transcendental, so the processor can work it
out to the bit. Where a multiply feeds an add the reference rounds the two apart, one statement each, and the
console's Makefile and the Android build compile it `-ffp-contract=off`; `FLUID_FUSED` fuses every such pair into
one rounding, as a graphics compiler may. The reference suite pins the bits of a 16 by 9 run of 64 frames, so a
processor or compiler that rounds otherwise says so there, not in the demo.

## The script

What a frame adds is a function of its number alone (`FluidScript`), worked out by the main thread into a uniform
buffer of its own each frame, the same floats the reference adds:

- three jets, at 1/8 and 7/8 across 6/8 down, pointing up and in, and at the middle 1/8 down, pointing down; each
  sweeps its direction back and forth by a turn whose half tangent goes from -1/2 to 1/2 (the rational
  parametrisation of the circle, so no sine is taken), over 150, 130 and 110 frames each way. A force of 0.35 cells
  a frame at the centre, a radius of four cells; each jet's colour moves on every 240 frames;
- a burst every 120 frames from frame 60, for twelve frames: a force of 0.9 and a radius of six cells, at one of six
  places in turn, pointing up turned by up to a right angle either way, a colour of its own.

## What is judged

A few frames of a run are judged, chosen by `FluidJudgedFrames`: frames 0, 1, 3, 7, 15, 31 and 63, the early ones,
and the last frame of each quarter, the run's last among them; eleven in a run of 600. The render thread copies
every field a judged frame starts from and each of its passes' outputs into memory of its own (ten copies, 2.2 MiB
a frame), and the judge works on them once the frames are over, so a frame's verdict does not depend on the time it
took.

| case | passes when | tier |
|---|---|---|
| the scene comes up | the device, the fields' targets, the shaders, the swapchain (in a quick run) all came up | none |
| *n* frames, frames 0 to 63's dye and velocity within 2^-10 of the processor's whole simulation | each early frame's dye and velocity are within 2^-10 of the reference run from frame 0 | ulp |
| *j* judged frames, the force splats, dye splats, divergence, 40 Jacobi passes, gradient, velocity advection, dye advection worked out again from each one's own inputs (seven cases) | every judged frame's pass is within 2^-12 and 2^-16 of its value of itself worked out again from that frame's own inputs, as the machine left them | ulp |
| *j* judged frames, the projection leaving at most half the divergence it was given | the root mean square of the divergence after the gradient is at most half the one before | ulp |
| *j* judged frames, the dye's mass within 25% of what the splats put in less the dissipation | the sum of the dye's colours is within 25% of the budget | ulp |
| *j* judged frames, every field a finite number and the dye never below nought | none of the ten copies holds a NaN or an infinity, and no dye is below nought | ulp |
| *p* pairs of judged frames in a row, the fields one leaves the next starts from, bit for bit | the velocity, pressure and dye a judged frame left are the ones the judged frame after it started from, every bit | exact |
| *j* judged frames, the compute pass's words in its image over the splat target's bytes (fluid-alias) | every word of the storage image is the one the compute pass stored that frame | exact |

The passes worked out again from the machine's own inputs are what the judge leans on: no difference between two
machines carries from one frame to the next there, so every judged frame of a run of any length is held as tightly
as the first, and a case names the pass that went wrong, the frame, the texel and the two values. The early frames
check what those cannot: the state a frame hands the next, a target lost or cleared between frames, the start.
The invariants check what every correct machine keeps: the interpolation the advections make is convex and the
splats only add, so a dye below nought is never a rounding.

A failed dye case's picture is the 128 by 128 window of that frame's dye around its first texel off, what the
machine left against what it should have, each colour as the screen shows it. Whatever the verdicts, a note after
the cases gives the farthest each check found and in which frame, so a pass says how near it came.

## Where the budgets come from

`tools/fluid-envelope.c` measures them on the processor, and `make -f host.mk host-run` runs it over 64 early frames
and a default run's 600 (`fluid-envelope 64 600`), failing when a budget no longer holds what it measured with its
margin; `fluid-envelope 64 36000` is the longest run's, minutes.

- **Early frames, 2^-10.** The whole simulation run twice, every multiply-add rounded apart and fused: the two part
  by at most 6.4e-6 over the first 64 frames, so the budget is 150 times that. They stay within 1e-5 to frame 700
  and part visibly only near frame 2500 (3.4e-3, and 0.12 by 3000): a fluid this viscous is chaotic slowly. The
  early frames stop at 63 for the processor's time, not because the simulation parts: the judge runs the whole
  simulation that far.
- **A pass, 2^-12 and 2^-16 of the value.** Each pass fused from the same inputs parts by at most 4.8e-7, but an
  advection's: where a texel's trace lands moves by a rounding of its place (2^-16 of a texel near the dye's far
  edge), and the sample by the field's slope times that, 9.4e-6 over 36000 frames; the budget is 26 times it.
- **Projection, a half.** Forty Jacobi passes leave at most 0.40 of the divergence over 36000 frames, and not
  nought whatever their number: on a grid where the velocity and the pressure share their cells, the divergence of a
  pressure gradient is the wide Laplacian, not the one the passes solve (80 passes still leave 0.30). With no
  projection a frame keeps all of it, 1.0; with 5 passes 0.75, with 10 0.62.
- **Mass, 25%.** A semi-Lagrangian advection conserves nothing exactly, and the divergence the projection leaves
  gathers dye or thins it: the dye ran from 0.90 to 1.04 of its budget over 3000 frames, and 0.158 off it at most
  over 36000 (frame 6026). The less divergence left, the nearer: with no projection the dye falls to 0.04 of its
  budget in 300 frames, with 5 passes it climbs to 1.31.

## The aliasing level

Fuddles asked for the biggest ping-pong stress test "cause it's a good way to test for memory fuckery". A game's
render targets live in one pool of its memory: when its dynamic resolution steps, a frame's targets land a little
further on and lie over the last frame's, and a compute pass's storage image at another place of the same memory
writes over them. A runtime that keeps targets on its own side has to know, then, which pixels memory holds: one
racing game's shadow layers, drawn a frame 128 KiB further on, were published and written back to memory when a
2 KB storage image went over the old ones (ScarletNX #2217). `fluid-alias` does that on purpose, on frames whose
answer the judge knows:

- one memory block holds the dye's splat target B, then the divergence, then the dye A that carries the state, one
  right after the other;
- every 40 frames from frame 20, B and the divergence are drawn 32 KiB further on for that frame alone, over their
  old selves and A's first bytes; the next frame's B, divergence and A cover them whole again, from their first
  byte on, so the shifted pair is left with pixels no one will read;
- every frame, after the judged copies, a compute pass writes a 32 by 32 R32_Uint storage image laid at that same
  32 KiB, over bytes B and the shifted B both cover.

No pass reads bytes it writes: the shifted B ends where A begins, and the shifted divergence lies over A only
between the splat's read of A and the advection's write of it. So on a console the fluid is the same fluid, frame
for frame, and every case holds as in `fluid`. The judged frames add the first three shifts', the frame after each
and the one after that, so the hand-on case sees the fields cross each shift; a runtime that publishes or writes
back the wrong layer hands the next frame other bytes, or moves a pass's output, and says so there.

## What is timed

As every demo of the family ([Demo](Demo.md)): frame 0 is the warm-up, and over the frames past it five cases carry
present to present's mean, 99th percentile and longest, and the frame's work's mean and 99th percentile: the forty
seven draws and the graphics unit's work, from the image in hand to the queue standing idle, a judged frame's ten
copies in it. A note ends the run, `fluid:` or `fluid-alias:`. `notime` leaves the five cases and the note out.

## In the browser

The browser runs it as a demo (`FluidDemo`): its frames and timing are the browser's options and the dye is shown
through the program's screen. Paused, the simulation holds at the frame it was about to draw and the last frame is
shown again; the time paused is no frame's, so the frame after it is timed from the end of the pause, and a note
says how often and how long. Stopped (B, a
restart, another level), it ends at the next frame: each case is judged on the judged frames the run reached, a note
says where it stopped and which judged frames it left out, and the cases keep the names of the frames asked, so a run
of a length compares with another whatever the person did. Stopped before frame 0 drew, every case after the
bring-up is a skip, `asked`.

## What rests on reasoning

No console has run the demo. What it expects of one: that a fragment shader's float adds and multiplies round to
the nearest, each alone or a pair fused (the budgets take either), that a 32-bit float target stores what the
shader wrote and a texel fetch hands it back unchanged, and that a pass reading a target the pass before it wrote,
after the barrier above, sees all of it. uam's fragment programs flush denormals where the reference keeps them:
that moves a value under 1.2e-38, which no budget sees. No shader indexes a vector by a component known only as it
runs, which uam 1.1.0 reads as the vector's first word. The aliasing level expects that images laid at overlapping
places of one memory block see each other's bytes, which deko3d's own memory model is.
