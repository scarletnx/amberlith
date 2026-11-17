# The game family

## Index

* [The scene](#the-scene)
* [What is judged](#what-is-judged)
* [What is timed](#what-is-timed)
* [The levels](#the-levels)
* [What rests on reasoning](#what-rests-on-reasoning)

<!-- counts: family game -->
| Suite | Cases | With a judge | Console | Driver | Reference | Emulator | Spec | Without one | Not yet known |
|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| game | 14 | 6 | 0 | 0 | 0 | 0 | 6 | 8 | 0 |
| wimsy | 73 | 28 | 0 | 0 | 0 | 0 | 28 | 45 | 0 |
| **All** | **87** | **34** | **0** | **0** | **0** | **0** | **34** | **53** | **0** |
<!-- /counts -->

Every other family asks the machine one thing at a time. A game asks everything at once: it draws,
plays sound, reads its files, calls services and computes on several threads in the same frame,
and presents the frame when the display is ready for it. The game family is one scene that does
that, judges what each part produced against what the processor works out, and times every frame.
Only the word `game` runs it (see [The mode file](../ModeFile.md)), since no console has run it
yet; the start screen has no row for it, and a file that names it keeps it whatever the screen
changes. `frames=<n>` sets how many frames the scene runs, from 1 to 36000; without it, 600, ten
seconds at 60 a second. The family's second suite, `wimsy`, is the same question at the sizes a
game comes in, a level a scene, each run only when a `level=<n>` word names it: see [The
levels](#the-levels).

- **game**, seven of its cases left out by `notime`, the ones that carry times. One says the scene came up (none tier): a graphics
  device, a way to the display (a swapchain, or the program's screen), the audio renderer and the fs
  family's big file, which the scene writes the first time as the fs suite does. Then the frames
  run, inside the case that judges their images; the four after it judge the sound, the reads, the
  services and the cores from what the frames left, and the seven after them carry the frames'
  times. If the scene does not come up, its bring-up is skipped (it ended the program before), or
  its frames do not run, every case after is skipped as `depends`, saying which, but the last: that
  the demos' music made nothing while the scene ran ([the music family](Music.md)), which holds
  whatever the scene did.

## The scene

Four threads over the three cores the program has, as a game spreads its work:

| thread | core, priority | each frame |
|---|---|---|
| main | 0, the program's own (44) | reads the pad, asks hid for its style set and the time service for the steady clock, steps a world of 1024 cells, writes the frame's fill quads, texels and 1024 uniform buffers into one of two slots, and hands the frame to the render thread and a read to the file thread |
| render | 1, 43 | takes an image from the swapchain, records and submits the frame, presents it, and hashes the copy of the target the frame made; in a run with a screen, shows that copy on the screen instead |
| audio | 2, 40 | keeps the audio renderer stepping, one update and a wait per renderer frame (5 ms), a tone looping on a voice |
| file | 2, 46 | reads the frame's 64 KiB of the big file and hashes it |

In [hooks mode](../Hooks.md) the file thread reads through nn::fs::ReadFile, the main thread asks
nn::time for the steady clock, and the audio thread's renderer is nn::audio's, as the audio
family's is in that mode.

The main thread is put on core 0 alone for the frames and back where it was after, as the kernel
family does; the others are made on their cores. The main thread runs at most one frame ahead of
the render thread: it waits for a slot the render thread has finished with before writing the
next frame, so the frame loop goes at the pace the display gives the render thread.

**What a frame draws.** The target is 128 by 128, offscreen, with a depth buffer, and every frame
draws all of it, in one command list:

- the fill suite's work: eight quads over the whole target, each depth tested and written. The
  nearest is quad `frame mod 8`, which is the last one drawn in only one frame of eight, so a depth
  test that lets everything through leaves the wrong colour in seven. It shows in the top right and
  bottom left quarters;
- the texture suite's: a 16 by 16 texture copied from the processor's slot into the image at the
  start of the list, then sampled, nearest, over the top left quarter, four by four pixels a texel,
  every pixel centre far from a texel's edge. Its texels change every frame;
- the stream suite's: the bottom right quarter as 32 by 32 cells of two by two pixels, one draw
  each, each reading a 1 KiB uniform buffer of its own that the main thread rewrites every frame
  with the cell's word: its position in the world, its number and the frame.

The slots are memory the graphics unit caches, rewritten by the processor every other frame, so
each list starts by dropping the level two cache (the previous frame's wait for idle wrote it back
first). An emulator that serves a buffer or a texture from what it held before the processor's
write draws an old frame's word or texel, and the image says so. After the draws, the target is
copied out for the judge and drawn, as a square in the middle, onto the 1280 by 720 image the
swapchain gave; the list is submitted, the queue drained, and the image presented.

**Presenting.** The swapchain is deko3d's, on the program's default window, with two images, as a
game made with it presents. Taking an image blocks until the display gives one back, which is what
paces a frame to the display. Every image taken is presented, a frame whose submit faulted
included, so the swapchain has them all back when it is destroyed.

A run with a screen (a detailed one, or one the start screen made quick) has the program's screen
on that window from the start screen on, and there the scene leaves it alone: it shows each frame
through that screen, its copy of the target scaled up five times by the processor, and makes no
swapchain, so the frames are paced by the screen's own buffers. A note says so, since present to
present is then the screen's and not a quick run's. Under Eden both ways of sharing the window
failed: a device made after the screen was closed never finished its first submit, and a screen
made again after the swapchain was destroyed never came back, the run ending inside the scene
with no summary. The scene runs last, after the net family, so no suite makes a device after it.

**The sound** is the audio family's path: the renderer opened as that family opens it, a 1 kHz tone
at half of full scale looping over a second on voice 0, into the submix that reaches the speakers at
a gain of nought, so the scene is silent, and the aux effect on that submix recording it. The audio
thread steps the renderer until the frames are over and the voice has played 100 renderer frames
(24000 samples), whatever the frames took, so the aux ring holds the same steady wave on every
machine. It waits for those at most 5000 ms after the frames, as the audio family's own wait does;
past that the sound's case fails with the samples the voice had played, and the scene goes on to
its other cases. Anything the renderer reported against a parameter is a note, `game: the renderer
said: ...`, as in the audio family.

## What is judged

None of it depends on the time a frame took: a frame's number decides its image (the world is
stepped in integers, `source/gameref.c`) and its read, and the sound is read once it has settled.
A machine that runs the frames slowly gets the same verdicts as one that keeps up.

| case | passes when | tier |
|---|---|---|
| the scene comes up | the device, the shaders, the images, the scratch, the card's file, the renderer and the swapchain (in a quick run) all came up | none |
| *n* frames draw the image the processor works out, every frame | every frame's four quarters hash (FNV-1a, row by row) to what `gameref.c` gives for that frame, worked out before the first frame | exact |
| the audio thread's tone keeps the level the renderer's model gives | the aux ring's 1 kHz level is within 3% of what the audio family's model of the renderer's default quality table gives at a ratio of one (`AudioRefResampledLevel`) | level |
| the file thread's *n* reads of 64 KiB hold the file's bytes | every read succeeded, whole, and hashes to the chunk of the pattern the fs family writes (`source/fsref.h`); the reads visit every chunk once in 1024 frames | exact |
| hid and the steady clock answer on each of *n* frames | hid's supported style set is the same every frame and holds the standard styles the program set; the steady clock never fails, never goes back and keeps one source | exact |
| the main, render, audio and file threads run on the cores they were given | each thread, asked its core every frame (the audio thread every step), is on its own | exact |
| the demos' music made nothing while the scene ran: no thread, voice or sample | from the scene's start to its end the music rendered no frame and started no thread and no voice, so none of the frames' time was its: it plays only in the interactive mode and the music family (`MusicMadeSoFar`) | exact |

A frame whose image is wrong fails the case with how many frames were, the first of them and which
quarters it got wrong, and its picture is that frame: what it drew against what it should have.
What reaches the display is not judged: the presented image is the compositor's once queued, and
nothing a program can read back.

## What is timed

Frame 0 is the warm-up: it pays for the device's first work, as a drawing suite's first case does,
and is left out of every measure, the way a drawing suite's warm-up pass is; the first note says what
it took. Over the frames past it, three times per frame, seven cases that carry them as their `us`,
which is what [Time per case](../TimePerCase.md)'s tools and a baseline read. The cases pass when
every frame was presented; their names carry the number of frames, so runs of two `frames=` never
compare. A run of one frame has none past the warm-up, and its seven are skipped saying so.

- **Present to present** is what a player feels: its mean, its 99th percentile and the longest. The
  render thread reads the system tick once each frame's present is queued, and a frame's time runs
  from the one before; the wait for the display is in the next frame's taking of an image, so on a
  machine that keeps up it is the display's period, about 16667 us at 60 Hz, whatever the frame
  cost. It moves only when frames are dropped, and the longest is a hitch as the baseline sees one.
- **The main thread's part** of a frame, from taking its slot to handing the frame over: the
  services, the world's step and the slot's writes. Mean and 99th percentile.
- **The render thread's part**, from the image in hand to the queue standing idle after the
  frame's submit: recording the list, the submit and the graphics unit's work, with no wait for the
  display in it. Mean and 99th percentile.

The two parts are what show a regression while there is room to spare: a machine that gets slower
moves them long before a frame stops fitting in 16.7 ms and present to present moves at all, and
which of the two moved names the path, the processor's or the graphics unit's. None of the three is
the graphics unit's busy time a drawing case carries, and `tools/perf.py` compares the scene's cases
one by one but leaves them out of its per-suite sums, where adding a mean to a percentile would mean
nothing.

The 99th percentile is the nearest rank, the ceiling of 99 in 100 of the frames timed. Under 100
frames past the warm-up that rank is the last: the 99th percentile is the longest frame, and says
nothing the longest does not. From 100 it leaves the slowest frame in a hundred out; at the default
600, the five slowest.

Two notes give the rest, the frames' and the work's:

```json
{"type":"note","text":"game: 600 frames in 10.07 s; frame 0, the warm-up, left out of the times (103895 us present to present, 365 us the main thread's part, 100599 us the render thread's); the 599 past it; present to present: mean 16642 us, 99th percentile 17192 us, longest 30548 us at frame 152, none over 33333 us, two frames at 60 Hz"}
{"type":"note","text":"game: past the warm-up; the main thread's part: mean 187 us, 99th percentile 311 us, longest 362 us at frame 153; the render thread's part, the graphics unit's in it: mean 1132 us, 99th percentile 1882 us, longest 7287 us at frame 152; the audio thread waited for 2016 renderer frames, the longest wait 11023 us, 1 over 10000 us; the file thread made 600 reads of 64 KiB, the longest 50 us"}
```

Those are ScarletNX's, headless on RADV: every frame at the display's period but frame 152, which
missed one, and the two parts together under a tenth of the period.

A frame over 33333 us is a hitch: more than two frames at 60 Hz, a stutter a player sees. The first
note names the first eight by number and counts the rest; the longest frame is the case a baseline
holds. With `notime` the seven timing cases and the two notes are left out and the scene runs the
same frames.

## The levels

The scene above is one small frame, the same on every run. The `wimsy` suite draws Wimsy,
ScarletNX's mascot, as a game draws a character, from the lightest frame a game makes upwards, a
level a scene. A level runs when a word names it, `level=<n>`, one word a level
([the mode file](../ModeFile.md)); none runs without one, and `frames=<n>` is theirs as it is the
scene's. Five levels:

| level | a frame |
|---|---|
| 0, sprites | 96 sprites of her, 128 texels a side, a pixel or four a texel, half of them mirrored, some hanging off the target: one batch of 576 vertices the main thread rewrites every frame as the scene's world moves them, drawn in one draw and blended over the clear colour, as a 2D game's are |
| 1, one lit model | her model, 1680 triangles, turning a two hundred and fortieth of a turn a frame, while the camera backs away from 3 of her half widths to 48 over the run: a transform in a uniform buffer the main thread rewrites every frame, a normal a vertex and a colour of four normalised bytes, one light worked out a pixel, the faces turned away culled, a Z24S8 depth buffer, and four samples a pixel resolved before the frame is shown |
| 2, a scene near and far | seven of her down a floor 204 units long, the camera backing away and up from 6 units to 36 from the nearest: the floor, 204 quads through a 2048 by 2048 texture of paving stones with its twelve levels of detail, a level between two weighed; a lane down its middle through a 1024 by 1024 texture whose eleven levels are each one colour; two walls 40 units down, a sign 5/256 of a unit in front of each, drawn before its wall on the left and after it on the right; and four marks painted in the floor's own plane, brought in front of it by a depth bias. Eleven draws |
| 3, a heavy frame | level 2's scene with a crowd of 160 more of her, twenty rows of eight either side of the lane, each one of her five drawings and up to a unit aside and three along from its place, both by one fixed seed, so every run draws the same crowd, each her own shade, and every model drawn as a game draws a character of several materials, a draw a part with its material's fragment program, one of four that give the same light, and its uniform buffer bound again: 1,509 draws a frame, 280,560 triangles of hers, 167 uniform buffers the main thread rewrites. Past the floor's edges, hills: a field of heights from one seed, eight columns of six units either side, rising to 24 units, 1,088 triangles each one colour, grass low, then dry grass, rock and snow, in a light worked into the colour in whole numbers. On the floor beside the lane, a field of grass from another seed, 65,536 blades, one draw of three vertices and an instance a blade, its foot, height, lean and green in a buffer stepped an instance; the vertex program sways each tip in a wind that is the frame's number, a sway every 96 frames, so a frame's grass is the same on every machine. Last, smoke: 256 puffs from a third seed, 50 to 90 units down the floor, each a square that faces the camera and thins from its middle to nothing at its rim, rising and drifting by the frame's number, blended over the frame in the buffer's order behind what is nearer, one draw of an instance a puff whose buffer the main thread rewrites every frame. The floor's texture is ASTC in blocks of 8 by 8, 4096 texels a side, thirteen levels. Before the scene, 128 dispatches of a compute program, each storing one level of one of sixteen 128 by 128 textures through a view of that level alone, and 176 passes, sixteen chains of eleven 64 by 64 targets, each target drawn with the one before it and given a square the main thread moves every frame. All of them are shown in the scene, on two walls along the floor's edges, a screen a draw sampled texel by texel: on the left wall every level of every compute texture, on the right every chain's last target and the frame's two loads, all of them this frame's, each with a plaque under it that says which it is. A load is a 512 by 512 ASTC texture no frame before had, written whole by a worker thread straight into the memory the graphics unit samples, 64 KiB each, 7.7 MiB a second at 60 frames. Four worker threads share the frame with the main thread: two write a load each, two the 167 uniform buffers, half each; the frame is handed to the render thread when all four are done. And the render thread reads back, every frame, a texel the compute program stored, and on the five judged frames every texel of every level and of every chain's last target, held bit for bit to what the processor works out: a wall only shows them. The frame goes to the queue in 41 submissions, one a compute texture, one a chain, one every twenty models and the last, as a game's does |
| 4, a heavy frame of 8K | level 3 with the floor's texture 8192 texels a side, fourteen levels, 22 MiB of blocks, and the grass and the smoke four times as thick, 262,144 blades and 1,024 puffs |

**The model** is `source/wimsymesh.c`, which `tools/wimsy-mesh.py` makes from her drawing,
`static/wimsy.svg`: each of its five leaves a slab with a white bevel where the drawing strokes
it, each eye a short cylinder, every part closed and wound one way. The sprite is that model seen
from the front, drawn by the processor in whole numbers, so its texels are the same bytes on every
machine. Her four other drawings in `static/` (wimsy-dist's: Fuddles', Amberlith's, Accord's,
its Sepia theme's too, and the Default theme's) are the same leaves and eyes in other colours,
so each is a palette for the one mesh, a leaf's colour its path's fill or the mean of its
gradient's stops; what they draw inside the leaves is left out.

**A frame** is drawn into a 1280 by 720 target, the console's own screen, by two threads as the
scene's are (and four more at the heavy levels, on cores 2, 2, 0 and 2): the main thread, on core 0, works the frame out and writes what it reads into one of
two slots, at most a frame ahead; the render thread, on core 1, takes an image from a swapchain
on the default window, records the frame, draws the target onto the image, submits, waits for
the queue and presents. A level has a device of its own, sized for it. In a run with a screen
the window is the program's screen's, so each level's first case is a `skip`, `unavailable`, and
its others `depends`: the levels run in a quick run from the mode file.

**What is judged** is five frames of a run: its first, each quarter of the way and its last
(frames 0, 149, 299, 449 and 599 of 600), so the camera's whole path, which ends with the run
whatever `frames=` is. The render thread copies each out after its present and keeps it (the
copy is in that frame's time to the next present, so present to present leaves the judged frames
out, as it leaves the warm-up), and once
the frames are over each is held to what the processor works out, a case a frame. A frame's image
depends on its number and the run's length alone.

Level 0's frames are held bit for bit, the exact tier: a sprite's corners are pixel corners and a
texel's edges fall on them, a texel is opaque or clear, and the batch's order decides which
sprite is on top.

A frame with a model is held to no hash ([Design](../Design.md#no-screenshots-compared)): two
machines that are both right differ in a shaded frame's last bits and in which way a sample on an
edge falls. The reference, `source/wimsyref.c`, draws the frame on the processor from the matrices
the uniform buffers held and works out the range each channel of each pixel may hold:

- a sample (four a pixel, at the standard locations the level programs) is moved a thirty second
  of a pixel each way, and what any of those five places would show is allowed: a sample that
  near an edge may fall either side of it. Where all five have a surface the clear colour is
  not allowed, and neither is a surface behind the farthest of them;
- two surfaces within 2^-21 of each other in depth, 8 steps of a 24 bit buffer, may each hide
  the other;
- a surface's colour is worked out at the pixel's centre and at the sample, and both are
  allowed with what lies between, since a machine may shade a multisampled pixel anywhere in it;
- a texture's level of detail may be anywhere the specification lets a machine put it (OpenGL
  4.6, 8.14.1: no less than the larger of the two coordinates' changes over a pixel, no more
  than their sum), measured anywhere within a pixel of this one and an eighth of a level more
  either way; the range holds every texel a lookup there can weigh on every level that reaches;
- a pixel's range is the mean of its samples' ranges and two codes more each way, one for the
  byte a float rounds to and one for the mean's own rounding.

The heavy levels' floor is ASTC a machine can be held to: each block is a void extent of its
stone's colour, which every decoder gives back whichever way it makes eight bits of it (the
reference suite holds the blocks to `source/astcref.c`), and a stone is whole blocks on every
level. A machine whose host samples no ASTC keeps the texture in a format of its own; BC7 holds
a flat block to within a code, so that floor's ranges are two codes wider each way.

Level 2 is there for what a machine does with the same things near and far, and its frames
show each. The lane's colour is the level of detail it was drawn at: red, green, blue, yellow,
magenta, cyan, white, orange and on, a band blending into the next where a level between two is
weighed, so a machine that picks a coarser or a finer level than the specification allows, or
takes the nearer level alone, is outside the lane's ranges, which are a few codes wide wherever
one coordinate's change is much the larger, as it is far down the lane. A sign is 5/256 of a
unit in front of its wall: from the camera's last place, 76 units off, 28 steps of a 24 bit
depth buffer apart and a ninth of a step of a 16 bit one, where the reference is sure of two
surfaces 8 steps apart; a machine that loses depth precision with distance shows the wall
through the sign. A mark lies in the floor's plane, where only its bias (64 steps and twice the
depth's steepest change over a pixel) puts it in front: a machine that drops the bias leaves the
floor over it, near and far.

A pixel inside its range passes. One whose range is no wider than eight codes is held to a
colour; a wider one is on an edge, of a surface or of a stone of the floor, held to the colours
beside it, or between levels of detail, held to theirs. This is the position tier, and
a passing frame's note says how many pixels were each:

```json
{"type":"note","text":"wimsy: level 1, frame 0: 920130 pixels held to a colour, 1470 to a wider range, on an edge or between levels of detail"}
```

A frame with a pixel outside its range fails, saying how many are, by how much at most, and one
of them with its range. With the word `images` each judged frame leaves its
[pictures](../FailurePictures.md): what the machine drew, the nearest picture inside the ranges,
and the verdicts, grey for a pixel held to a colour, blue for one held to a wider range, red for one outside.

What the ranges refuse is checked by the reference suite on machines made to order
([the judge](Judge.md)): one that draws the level as asked passes near and far, and one that draws
it with one sample a pixel, with no depth test, with no light, a frame late, a pixel aside or
not at all is refused, as is one that draws level 2 with the nearer level of detail alone, a
level too coarse, no depth bias or 16 bits of depth.

What the heavy levels' screens show is held twice. A compute texel is a whole number worked
out from its place, its texture, its level and the frame (`WimsyComputeTexel`), stored as the
normalised value that byte is, and a chain's last target is its clear and eleven squares, the
later over the earlier (`WimsyChainTexel`). One more case a heavy level reads them back: a texel
each frame, and on the five judged frames every level of every compute texture and every chain's
last target whole, each texel held to those bytes; the copies are a list of their own after the
frame's present, in no frame's time. A texture a frame old, a level never stored,
a target a pass never reached or a store the copy does not see is that case's. In the frame a
screen's pixel is held to the texels under it, a 32nd of a pixel to spare, so a wall seen from
the lane holds a compute texture to two or three texels a pixel and a screen a frame old is
refused there; a load's block, a void extent of a colour from its place and the load's number
(`WimsyLoadTexel`), is held on its screen to the two codes BC7 is given. A screen too far or too
slanted to tell its texels apart, more than sixteen to a pixel, is held to nothing: the case that
reads back holds it.

**The walk** of a heavy level is made to check it by (`WimsyWalkAt`): down the lane between the
grass; along the left wall and its compute textures; round, and along the right wall, its chains
and loads; down the lane into the smoke and through it; then up, and round to look back at it
all. The five judged frames are the scene from its start, a wall each, the smoke from inside and
the whole scene from its far end. The camera is inside the scene, so the near distance cuts
what is beside and behind it, and the reference cuts the same triangles the same way. In the
browser the pause menu has four rows of a heavy level's own ([the UI](../UI.md)): the grass's
and the smoke's density (`WimsyGrassAsk`, `WimsySmokeAsk`), the plaques (`WimsyLegendAsk`), and
the camera, the walk or the person's own, moved by the sticks (`WimsyCameraAsk`), whose frames
are nobody's to judge. A run from the mode file has each level's own, the legend and the walk.

These are the ways a game on the console's one memory shares it between the processor and the
graphics unit, each every frame: worker threads writing a texture's blocks and a draw's uniform
buffers through mappings that stay for the run, into memory the graphics unit reads in the same
frame with no copy between; descriptors rewritten for images the frame is the first to use; and
the processor reading memory the graphics unit wrote, once the queue stands idle. What an emulator does for them is what a game's shadow
maps, light probes and blur chains cost it: a dispatch and a pass each a submission's worth of
state, and memory the graphics unit wrote read through another view in the same frame.

**What is timed** is what the scene times: seven cases a level, named with it
(`level 1, 600 frames, present to present: the mean`), and two notes. After the last level, one
case more, as the scene's last: the demos' music made nothing while the levels ran. A judged frame's copy is
made after its present, so neither part holds it; it falls in the next frame's present to
present, and the second note says what the longest one took.

In an emulator a failure at level 0 points at a vertex buffer the processor rewrote every frame
served from what was held before (`buffer_cache/buffer_cache.h`), at blending, or at a sprite a
texel aside; at level 1, at the uniform buffer in the same way, a normalised byte attribute, the
face the winding gives through the flipped viewport, a Z24S8 buffer of four samples, the sample
locations, or the resolve, which the library makes on the 2D engine; at level 2, at the level
of detail it picks (a bias a setting adds, anisotropy it forces, a level of a texture it did not
upload), at the depth format it keeps a Z24S8 buffer in, or at the depth bias and its units in
that format. A level whose first frame
passes and whose later ones show the faces turned away points at the shadow RAM: to present a
compressed image the library writes the window origin with its flip set while the shadow RAM
passes methods through, and again while it replays them, which puts the tracked value back; a
machine that keeps that method out of its shadow RAM is left with the flip, and with it the
other face in front, from the first present on.

## What rests on reasoning

No console has run the scene. What it expects of one, besides what the other families already
check: that hid answers GetSupportedNpadStyleSet with the set the program gave, standard styles
included; and that a deko3d swapchain on the default window paces at the display.

Nor the levels. What they expect of one: that the resolve of a target of four samples is the
mean of each pixel's four, as the msaa suite's resolve case holds it; that a second device and a
second swapchain on the default window come up after the first were closed; that the shadow
RAM's replay puts the window origin back after a present, as Eden's does; and that a channel is
never more than two codes from what the reference's arithmetic gives. And of the heavy levels:
that a compute program's store through a view of one level lands in that level and is what a
view of the whole texture samples later in the same frame, past a full barrier; that an image's
memory the processor wrote in the block linear order, a tile of eight gobs, is what the unit
samples with no copy between, as the coherency family's cases expect of a smaller image; that an
ASTC block of one colour decodes to that colour; that a copy of a texel to a buffer is
there for the processor once the queue is idle; and that a vertex buffer stepped an instance
gives each of the 65,536 or 262,144 instances of one draw its own blade, as the vertex suite's per instance
attribute does nine; and that a blend of a puff over the target lands within a code of the
arithmetic's, a puff after a puff, as the blend suite holds one blend. The compute program reads its uniform block's
vector by fixed components alone, and works in whole numbers up to the one division by 255, so
neither a vector's component picked at run time nor a denormal is in it. ScarletNX f492ffb on a
desktop card and on lavapipe is inside every range of the five levels.
