# The demo family

## Index

* [The family](#the-family)
* [mandelbrot and julia](#mandelbrot-and-julia)
  * [A frame, as any backend issues it](#a-frame-as-any-backend-issues-it)
  * [The zooms](#the-zooms)
  * [What is judged](#what-is-judged)
  * [How far single precision holds](#how-far-single-precision-holds)
  * [What is timed](#what-is-timed)
  * [What rests on reasoning](#what-rests-on-reasoning)
* [fluid and fluid-alias](#fluid-and-fluid-alias)

<!-- counts: family demo -->
| Suite | Cases | With a judge | Console | Driver | Reference | Emulator | Spec | Without one | Not yet known |
|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| mandelbrot | 14 | 7 | 0 | 0 | 0 | 0 | 7 | 7 | 0 |
| julia | 14 | 7 | 0 | 0 | 0 | 0 | 7 | 7 | 0 |
| fluid | 19 | 13 | 0 | 0 | 0 | 0 | 13 | 6 | 0 |
| fluid-alias | 20 | 14 | 0 | 0 | 0 | 0 | 14 | 6 | 0 |
| **All** | **67** | **41** | **0** | **0** | **0** | **0** | **41** | **26** | **0** |
<!-- /counts -->

## The family

Demos a person watches, each one a graphics workload a game runs, drawn frame after frame, presented and timed, and
judged like every other case: chosen frames against the answer the processor works out. Only the word `demo` runs it
([The mode file](../ModeFile.md)), and `all` leaves it out: a demo is there to be watched and adds seconds of frames
to a run that times everything else. A run that does not name it makes no device, no frame and no answer for it: not
one of its cases, references or buffers costs a run anything until the word asks for them. The start screen has no
row for it, and a file that names it keeps it whatever the screen changes. `frames=<n>` sets how many frames each
demo draws, as it does the game scene's; 600 without it, ten seconds at 60 a second.

What every demo of the family does:

- **Every frame is a function of its number.** What a frame draws comes from its number and nothing else: no clock,
  no input, no state carried over that a slow frame could change. A machine that runs the frames slowly draws the
  same images as one that keeps up, and gets the same verdicts.
- **Chosen frames are judged** against the processor's answer for that frame, exact wherever the arithmetic the
  demo's programs do has one answer, and the tolerance named where it has more. A demo's section says which frames,
  what is held exact, and the limits of the arithmetic it draws with.
- **Every frame is presented and timed** as the game family's scene is ([What is timed](Game.md#what-is-timed)):
  frame 0 is the warm-up and left out, then present to present (mean, 99th percentile, longest) and the frame's work,
  from the image in hand to the queue standing idle after the frame's submit (mean, 99th percentile). With `notime`
  the frames run and are judged, and no case carries a time.
- **Presented like the game scene**: through a swapchain on the default window in a quick run from the mode file,
  through the program's screen in a run with a screen, the target copied back and scaled up by the processor, where
  present to present is the screen's.
- **A demo is a function of how it is run** (`source/demo.h`, `DemoRun`): the frames, whether it times them, whether
  a person is in front of it, and two questions it asks once a frame, whether that person stopped it and whether
  they hold it paused. A step of the run asks it with the mode file's words, and nobody stops or pauses it; a screen
  that lets a person pick a demo asks it with its own. Paused, a demo shows its last frame again and draws no new
  one, and the time it stays paused is in no frame's time; the frames case says how often and how long. Frames a
  stopped demo did not reach are skipped as `asked`, saying the frame it stopped at. A third question, the pad's
  sticks, is for a demo a person steers; a step of the run has nobody to ask.

## mandelbrot and julia

Two suites, one zoom each, done entirely on the graphics unit: a compute program counts every pixel's iterations and
a fragment program colours them. **mandelbrot** zooms into the valley between the Mandelbrot set's main cardioid and
the disc to its left; **julia** zooms into the Julia set of c = -0.835 - 0.2321i at its repelling fixed point,
where the set is self-similar and spirals in.

Each suite has fourteen cases, nine with `notime`: the bring-up (none tier: a device with a compute queue, the five
programs, the counts buffer, the copies of the judged frames, and the way to the display), the frames (none tier:
every frame the run asked for drawn and presented, or as many as ran before the person stopped them; the frames run
inside it, so a machine that ends the program there loses that case), six judged frames (exact), five times, and,
in a scripted run, that the demos' music made nothing while the zoom ran ([the music family](Music.md)).
A note after the frames says how far into single precision the deepest frame goes, and one after the times sums them
up.

### A frame, as any backend issues it

Two passes, the same on any backend that issues them: deko3d here (`source/tests/t_fractal.c`, `Record`), and
anything else that binds the same blocks and runs the same programs (`source/fractalref.h` says them):

1. a dispatch of `fractal.comp.glsl`, 80 by 45 groups of 8 by 8 invocations, one a pixel of the 640 by 360 target,
   with the frame's block as uniform buffer 0 and the counts, a word a pixel, as storage buffer 0. Each invocation
   iterates z = z * z + c until |z| passes 2 or the frame's iterations are spent, and stores how many it took. The
   program's whole body sits inside the test of the pixel's place rather than after an early return, which uam
   compiles to a PRET and a RET: Eden's shader translator drops a pipeline with a PRET, and the geometry family's
   square twins judge that apart ([Graphics](Graphics.md)), so a zoom that fails is about the zoom;
2. once those stores are visible to the fragments (deko3d: `DkBarrier_Primitives` with the shader caches dropped, the
   order suite's pattern), a draw of three vertices with no attributes, `fullscreen.vert.glsl` and
   `fractal.frag.glsl`, the same two buffers bound to the fragment stage, into the 640 by 360 RGBA8 target: each
   pixel the colour of its count, or black where the iteration never let it go.

The frame's block is 1072 bytes in std140 (`FractalFrame`): the view's centre and step, the most iterations, the
Julia seed and a flag that picks it, the palette's turn, the target's size, and the palette, 64 colours, an array of
scalars that std140 puts 16 bytes apart. The processor
writes it into one of two slots before the frame's list, and the list starts by dropping the level two cache, as the
game scene's does. The target is then shown at twice its size: a textured quad over the 1280 by 720 image the
swapchain gave, or the copy the screen shows. On a judged frame the graphics unit also copies the counts and the
target aside, and they are judged once every frame has run, so judging takes no frame's time.

### The zooms

Frame k shows the view of the zoom's step min(k mod 1198, 1198 - k mod 1198): in over 600 frames, back out over the
next 598, and in again, so a run of any length draws a zoom. At step s the view's step between two pixels is 0.005
times r^s, r = 2^(-12/599) for the Mandelbrot zoom and 2^(-10/599) for the Julia one, worked out by repeated products
of a constant written out in hexadecimal, so no library function makes two machines differ: the deepest frame is 4096
times narrower than the widest, 1024 for the Julia set. The centre closes on the target as the square of that scale,
so the target drifts to the middle of the screen as the view shrinks. The iterations grow one every two steps, from
128 to 427 for the Mandelbrot zoom and 192 to 491 for the Julia one. The palette, eight colours with eight steps
between each two, turns by one colour every two frames.

### What is judged

Frames 0, 120, 240, 360, 480 and 599, the widest view, four on the way and the deepest, each a case:

| case | passes when | tier |
|---|---|---|
| frame *k* draws the counts and colours the processor works out | the counts the compute pass stored, as words, and the target's bytes, row by row, hash (FNV-1a) to the pinned hashes of that frame | exact |

Exact, because the compute program is one sequence of single precision operations, each rounded once, that the
processor repeats one for one (`source/fractalref.c`, `FractalIterate`). Every value in the program is `precise`
and every multiply and add is written as the `fma` it is meant to be, so no compiler may fuse, split or reorder them.
Read out of the compiled program, the loop is exactly what the source says: an FMUL for y squared, an FFMA for the
radius, the comparison with 4, and three FFMAs and an FADD for the next z; the pixel's place is one FFMA a coordinate
from the pixel's distance to the middle, which is exact. None of the compute program's FADD, FMUL and FFMA flushes a
denormal, where the fragment programs' do, so the processor keeps them too. The fragment pass maps a whole number to
four bytes through a table, which has one answer. The table is an array of scalars, never a vector the program picks
a component of: the toolchain's uam (1.1.0) compiles `palette[i >> 2][i & 3]` in a uniform block to a load at the
vector's address with the component's bytes masked off, so it reads the vector's first word whatever `i & 3` is, on
the console as under any machine that runs the compiled program. ScarletNX drew exactly that in a first version of
the zoom, every count right and three colours in four a step or three back along the palette.

The hashes are pinned in `source/fractalref.c`, and the reference suite on a computer works out the twelve frames
whole and holds them to the pins, so a run on a machine does no arithmetic on a frame that passes. A frame whose
hashes differ is worked out there and then on the machine's own processor, and the case says how many counts differ,
the first of them, how many of those are what a machine that multiplies and adds apart would count and how many are
one iteration off, how many pixels have another colour, and whether that processor's own answer differs from the pin,
which would make the judge, not the graphics unit, the one that moved. Its picture is that frame: what it drew
against what it should have.

### How far single precision holds

A float's pixel coordinates stop telling two neighbouring pixels apart once the step between them nears a unit in
the last place of the coordinate; long before that the orbit's own roundings move the count of pixels near the set's
edge. The zooms stop where both still leave a picture worth looking at, and the reference suite pins where that is:

- the Mandelbrot zoom's deepest step is 20.5 units in the last place of the farthest coordinate it reaches (0.744),
  the Julia zoom's 81.9; every frame of both keeps at least 16;
- against the same zoom worked out in double precision from the same coordinates, 210 of the 230400 pixels of the
  Mandelbrot zoom's widest frame escape at another iteration, and 54036 of its deepest, 23 in 100; the Julia zoom's,
  694 and 5893;
- a machine that does each fused multiply and add as a multiply then an add counts 234 and 51140 pixels otherwise,
  774 and 6250 of the Julia zoom's, so it fails every judged frame, and the case names it.

So the deepest Mandelbrot frames are single precision's picture of the set, not the set's own: what a game drawing
with floats would show, and what the console's graphics unit draws. A deeper zoom wants perturbation, one reference
orbit worked out in higher precision and every pixel iterated as a small float difference from it, or double
precision, which the console's graphics unit runs at a thirty second of the single precision rate.

### What is timed

The five times of every demo (see [The family](#the-family)), and a note: the frames, the warm-up, the frame's work
and present to present with their means, 99th percentiles and longest, and the frames over 33333 us, two frames at
60 Hz. The frame's work is the processor writing the frame's block, recording and submitting the list, and the
graphics unit's two passes, the copies of a judged frame and the quad shown, up to the queue standing idle.

### What rests on reasoning

No console has run the zooms. What they expect of one, besides what the order and fmath suites already check: that
the compiled program's single precision operations round as IEEE 754 does, to nearest, ties to even, a fused
multiply and add once, and that a compute program keeps denormals, as its compiled instructions say.

## fluid and fluid-alias

A 2D Navier-Stokes fluid, Stam's stable fluids in forty seven fragment passes a frame in ping-pong between two
targets a field: each judged frame's passes worked out again from the fields the machine left, the early frames held
to the processor's whole simulation; and **fluid-alias**, the same with its targets drawn over their old selves now
and then and a compute pass's storage image over them, as a game's dynamic resolution leaves its pool. Their page:
[The fluid demo](Fluid.md).
