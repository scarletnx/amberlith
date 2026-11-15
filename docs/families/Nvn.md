# The nvn family

## Index

* [Why a layer](#why-a-layer)
* [What a pass proves, and what it does not](#what-a-pass-proves-and-what-it-does-not)
* [The cases](#the-cases)
* [The scene, down three ways](#the-scene-down-three-ways)
* [Where nvn/nvn.h comes from](#where-nvnnvnh-comes-from)

The graphics questions asked through NVN, the API a game draws with, so that a runtime which runs a game's NVN calls
itself is asked them too. Only the word `nvn` runs it (see [The mode file](../ModeFile.md)); the start screen has no
row for it, and a file that names it keeps it whatever the screen changes. Its one suite is `nvn`, twenty one cases.

[NVN coverage](../NvnCoverage.md) lists every NVN function ScarletNX answers with the cases that call it, and the
ones no case calls yet.

## Why a layer

A homebrew program has no NVN: each game carries its own driver. `nvnlayer/` is one, a module of its own
([The NVN layer](../NvnLayer.md)), built here for API 55, on deko3d: the NVN functions a game gets by name from
`nvnBootstrapLoader` and the `nvnDeviceGetProcAddress` it hands back, each recording the deko3d commands that do
what NVN says it does. The program exports `nvnBootstrapLoader` (the layer's link flag), as a game's driver does,
and a case looks up every function through it (`NvnLoadProcs`), never by name.

- **On the console and in Eden**, the layer runs: its deko3d commands reach the graphics unit, and what the unit does
  with them is the answer the case holds.
- **Under ScarletNX**, the runtime finds `nvnBootstrapLoader` among the program's exported functions, as it finds a
  game's, and puts its own NVN in place of every function the case looks up; none of the layer runs past its name
  lookup. The same case then asks ScarletNX's NVN, the path every game takes, where the other drawing suites, which
  call deko3d, reach ScarletNX's engine methods path instead.

`nvnDeviceGetInteger` is written in assembly in the shape of the console's NVN drivers (a bounded switch through a
byte table, its cases setting w0), because ScarletNX reads a game's NVN API version from that code before the game
asks. The layer answers 55.8; ScarletNX answers 55.15 when it cannot read a version, so the version case tells
whether it read the program's.

## What a pass proves, and what it does not

The reference is the graphics unit running what the layer recorded, never the official driver.

- A pass on ScarletNX says its NVN gives the same pixels the console's graphics unit gives for the commands the layer
  chose for those NVN calls. Where the layer and the driver would choose the same commands, which is wherever NVN
  defines the result (a clear's bytes, a copy's texels, a blend's equation), that is the console's answer.
- It says nothing about what the driver alone decides and a game can see without the graphics unit: a texture's
  storage size and alignment, the command memory each call takes, the layouts of its objects, the transform it gives
  a present, the device's answers past the API version. Those come from deko3d's rules in the layer, so the console
  and Eden answer deko3d's, and ScarletNX answers the driver's; no case compares them.
- Where NVN leaves an order to the program, the layer takes the strictest: every order bit of
  `nvnCommandBufferBarrier` is deko3d's full barrier, so a case reads the ordered answer whatever it asked for.
- A choice NVN makes and the layer does not have yet (a format, a target, a flag, a view) is refused with its value
  in `NvnLayerError`, which the failing case's line carries; on ScarletNX that text is always empty.

## The cases

- **the program's NVN functions resolve** (none tier): the lookup gives every function the layer has, a hundred and
  forty three.
- **the device initializes** (none tier): `nvnDeviceInitialize` with no flags. If it fails, the rest are skipped as
  `depends`.
- **the device answers the layer's API version** (exact): items 0 and 1 of `nvnDeviceGetInteger` are 55 and 8.
- **a queue, its pools and a 64x64 target come up** (none tier): a queue, a command pool, a readback pool, an RGBA8
  target in a pool of its own, and a command buffer with its command and control memory.
- **clear to opaque red**, **clear to a mid channel value** (exact): the target cleared through
  `nvnCommandBufferClearColor`, ordered before `nvnCommandBufferCopyTextureToBuffer` by a barrier, copied to the
  readback pool and compared pixel by pixel, after the pool is filled with a byte no clear here makes. The clear
  suite's colours.
- **two copy textures and a staging pool come up** (none tier): two 64x64 RGBA8 textures in a pool of their own,
  and a processor-visible pool holding a picture of 64x64 texels, every texel four bytes of its own.
- **a picture copied into a texture and back keeps every texel**, **a texture copied into another keeps every
  texel**, **a buffer copied into another keeps every byte** (exact): `nvnCommandBufferCopyBufferToTexture`,
  `CopyTextureToTexture`, `CopyTextureToBuffer` and `CopyBufferToBuffer`, a barrier between each two, every byte of
  the readback pool judged after it is filled with a byte no texel holds.
- **a texture copied to a buffer with MIRROR_Y lands upside down**, **a texture copied into another with MIRROR_X
  lands mirrored** (exact): the copy flags a game mirrors with, which the layer gives deko3d as FlipY (the copy
  engine stepping the buffer backwards, the copy suite's FlipY) and FlipX (the 2D engine).
- **a copy to a buffer with a row stride of 288 keeps the bytes between rows** (exact):
  `nvnCommandBufferSetCopyRowStride`, rows 288 bytes apart and the 32 between them untouched.

  These are ScarletNX's NVN copy tests (`nvn_copy_formats.c`, `vulkan_nvn_copy_flags.inc`,
  `vulkan_nvn_compat.inc`) as far as the layer reaches: the same textures, flags and strides through the program's
  NVN, so ScarletNX answers with `nvn_copy.c`, the path a game's copies take, and the console and Eden with what
  deko3d sends the copy and 2D engines for them. ScarletNX 9bef605 passes all seven; Eden 6a9235c22d passes five
  and fails the two mirrors, the copy suite's FlipY to a buffer (nothing lands) and the 2D engine's FlipX between
  images (the texels copied unmirrored), where ScarletNX's NVN copy does both.
- **a texture builder gives back each number it was set** (exact): every setter of a builder that keeps a number
  (the flags, the target, the format, the width, the height, the depth, the levels, the samples, the pool and the
  offset), each with a number that is not the default, then the getters; and `nvnTextureBuilderSetSize2D` as the
  width and the height. The builder makes no texture, so the numbers need not be one a machine has. The line names
  the first getter that gave another number, with both.
- **a texture view says which of its parts were set, and with what** (exact): a new view has no levels, layers,
  format or target set and its getters say so; each part set comes back with what was set while the others stay
  unset; `nvnTextureViewCompare` holds for two new views and for two set the same, and not for a view against a new
  one or against one of another number of levels.
- **a texture of three levels comes up and says what its builder had** (exact): a 64x64 RGBA8 texture with levels
  down to 16x16, a page into a pool of its own, and its ten getters, the pool and the offset among them.
- **a picture copied into each level of a texture through a view comes back from that level** (exact): a picture
  of its own for each level, the first copied with no view and the others through a view of that level alone
  (`nvnTextureViewSetLevels`), then each read back through its view. Every byte of the readback is judged, so a
  level that took another's picture or kept nothing shows as the first byte wrong, with its level.

  What a getter is held to is NVN's own word for it, that it gives what the setter took: on the console and in Eden
  the layer answers from its record, so only ScarletNX's answers are news there. The levels are the graphics
  unit's.
- **a window acquires and presents both its textures** (none tier): two 1280x720 textures on the default window,
  each acquired with `nvnQueueAcquireTexture`, cleared and presented once. Nothing is read back. Skipped as
  `unavailable` while the program's screen holds the window, which a run begun on the start screen keeps up to its
  end, `noprogress` or not: a quick run from the mode file runs it, as the game family presents only then, and the
  start screen says so when the file names nvn.

## The scene, down three ways

Three of the suite's cases draw one scene: through deko3d's calls, as every drawing suite draws; through the
layer's own NVN functions; and through the NVN the program looks up. `source/drawpath.h` is what makes that one
recording: the calls a scene draws with (a target, a depth target, programs of two stages, vertices, uniform
buffers, textures), with a way under them for each, and `source/tests/t_nvnscene.c` records the scene once against
it. Every way is judged by the same function against the same pixels, `source/sceneref.c`, which the host build
checks. The device of the NVN ways is set to the upper left origin and a near plane at zero, the deko3d way's
conventions, so the scene's vertices mean the same pixels.

The scene is a 64 by 64 target in four quarters, five draws in one list, exact tier:

- top left, an 8 by 8 texture drawn four pixels a texel, sampled nearest through texture slot 1, with a decoy of
  other texels in slot 0: the texture is found through its slot's handle;
- top right, a quad drawn with the depth test and write on, then a farther quad of another colour that the depth
  test has to hide;
- bottom left, a quad the fragment stage's uniform buffer colours;
- bottom right, the clear colour and a 16 by 16 square whose vertices are the target's top left corner, moved
  there by the vertex stage's uniform buffer, which the three draws before it bind another buffer for.

The cases, the first two before the suite's lookup and the third after its window case:

- **a scene of five draws through deko3d holds what the processor works out** (exact): on every machine, the
  engine methods path.
- **the same scene through the layer's own NVN functions holds what the processor works out** (exact): the
  layer's bodies on every machine (`NvnLayerOwnProcs`), so that this case failing alone is the layer's
  translation. It runs before anything looks the program's NVN up, since ScarletNX takes a function over at the
  address its name resolves to, the layer's own from then on with it.
- **the same scene through the NVN the program looks up holds what the processor works out** (exact): under
  ScarletNX, the NVN its games take; on the console and in Eden, the layer again.

A failing case says how many pixels are wrong and what the first one shows: a texel of the decoy (the wrong
handle), the far quad (no depth test), the clear colour where a quad should be, the moved square in the top left
corner (the vertex stage read another draw's buffer).

Under ScarletNX 3e9ad88 on RADV all three pass, and the three pictures are the same bytes. No console and no
emulator of the graphics unit has run them yet.

One shader binary serves every way because the scene's shaders keep to the layer's two rules ([The NVN
layer](../NvnLayer.md#what-a-shader-may-use)): uniform blocks from `binding = 1`, and textures by the handle in the
block at `binding = 0`.

## Where nvn/nvn.h comes from

[The NVN layer](../NvnLayer.md#where-its-numbers-come-from) says where its types, its prototypes and its offsets
come from. None holds driver code: names, numbers, sizes and argument types.
