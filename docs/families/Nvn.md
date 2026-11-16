# The nvn family

## Index

* [Why a layer](#why-a-layer)
* [What a pass proves, and what it does not](#what-a-pass-proves-and-what-it-does-not)
* [The cases](#the-cases)
* [The scene, down three ways](#the-scene-down-three-ways)
  * [A texture past its edges](#a-texture-past-its-edges)
  * [Indexed draws](#indexed-draws)
  * [The binds that take several, and the deferred ones](#the-binds-that-take-several-and-the-deferred-ones)
  * [A uniform buffer updated inside the list](#a-uniform-buffer-updated-inside-the-list)
  * [Instanced draws, and draws that read their numbers](#instanced-draws-and-draws-that-read-their-numbers)
  * [The draws deko3d has none of](#the-draws-deko3d-has-none-of)
  * [A capture of what a vertex stage writes](#a-capture-of-what-a-vertex-stage-writes)
* [The command buffer, down two ways](#the-command-buffer-down-two-ways)
  * [Counters and the render enable](#counters-and-the-render-enable)
  * [The command buffer's memory and lists in lists](#the-command-buffers-memory-and-lists-in-lists)
  * [Clears, strides and discards](#clears-strides-and-discards)
  * [The debug layer's marks](#the-debug-layers-marks)
  * [The render state a list sets by itself](#the-render-state-a-list-sets-by-itself)
  * [The device and a program](#the-device-and-a-program)
* [Where nvn/nvn.h comes from](#where-nvnnvnh-comes-from)

The graphics questions asked through NVN, the API a game draws with, so that a runtime which runs a game's NVN calls
itself is asked them too. Only the word `nvn` runs it (see [The mode file](../ModeFile.md)); the start screen has no
row for it, and a file that names it keeps it whatever the screen changes. Its one suite is `nvn`, two hundred cases.

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

- **the program's NVN functions resolve** (none tier): the lookup gives every function the layer has, five hundred
  and thirty two.
- **the device initializes** (none tier): `nvnDeviceInitialize` with no flags. If it fails, the rest are skipped as
  `depends`.
- **the device answers the layer's API version** (exact): items 0 and 1 of `nvnDeviceGetInteger` are 55 and 8.
- **a queue, its pools and a 64x64 target come up** (none tier): a queue, a command pool, a readback pool, an RGBA8
  target in a pool of its own, and a command buffer with its command and control memory.
- **clear to opaque red**, **clear to a mid channel value** (exact): the target cleared through
  `nvnCommandBufferClearColor`, ordered before `nvnCommandBufferCopyTextureToBuffer` by a barrier, copied to the
  readback pool and compared pixel by pixel, after the pool is filled with a byte no clear here makes. The clear
  suite's colours.
- **clears before and after deferred finalizes, the shader cache switch and a list's handle given up are there**
  (exact): `nvnDeviceFinalizeCommandHandle` for the list that just ran, `nvnDeviceApplyDeferredFinalizes` and
  `nvnDeviceSetIntermediateShaderCache` on and off, between two clears. A release driver returns from each at
  once and nothing of them can be read, so the case holds a runtime to going on.
- **two copy textures and a staging pool come up** (none tier): two 64x64 RGBA8 textures in a pool of their own,
  and a processor-visible pool holding a picture of 64x64 texels, every texel four bytes of its own.
- **a picture copied into a texture and back keeps every texel**, **a texture copied into another keeps every
  texel**, **a buffer copied into another keeps every byte** (exact): `nvnCommandBufferCopyBufferToTexture`,
  `CopyTextureToTexture`, `CopyTextureToBuffer` and `CopyBufferToBuffer`, a barrier between each two, every byte of
  the readback pool judged after it is filled with a byte no texel holds.
- **a texture copied to a buffer with MIRROR_Y lands upside down**, **a texture copied into another with MIRROR_X
  lands mirrored** (exact): the copy flags a game mirrors with, which the layer gives deko3d as FlipY (the copy
  engine stepping the buffer backwards, the copy suite's FlipY) and, between two textures, as a blit with FlipX
  (the 2D engine): deko3d's copy between images takes no mirror.
- **a copy to a buffer with a row stride of 288 keeps the bytes between rows** (exact):
  `nvnCommandBufferSetCopyRowStride`, rows 288 bytes apart and the 32 between them untouched.

  These are ScarletNX's NVN copy tests (`nvn_copy_formats.c`, `vulkan_nvn_copy_flags.inc`,
  `vulkan_nvn_compat.inc`) as far as the layer reaches: the same textures, flags and strides through the program's
  NVN, so ScarletNX answers with `nvn_copy.c`, the path a game's copies take, and the console and Eden with what
  deko3d sends the copy and 2D engines for them. ScarletNX 9bef605 passes all seven; Eden 6a9235c22d passes five
  and fails the copy suite's FlipY to a buffer (nothing lands), where ScarletNX's NVN copy mirrors. Its other
  failure then, the texels copied unmirrored between images, was the layer's own: it asked deko3d's copy for the
  mirror, which that copy drops. ScarletNX f492ffb showed it once the suite ran on the layer's own functions, and
  passes it with the blit; Eden has not run it since.
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
- **a memory pool and its builder give back what they were set, and the pool maps its storage** (exact): a pool
  of two pages, cached to the processor and to the graphics unit, made through a builder: the builder's device,
  memory, size and flags, the pool's size and flags, and `nvnMemoryPoolMap`, which is the storage's own address,
  a pool being the memory the program gave it.
- **a page flushed from a cached pool is what a copy reads, and the copy is there after an invalidate** (exact):
  a page written through that pool's mapping, `nvnMemoryPoolFlushMappedRange`, a copy of it by the graphics unit
  into the pool's second page, `nvnMemoryPoolInvalidateMappedRange`, and both pages read, every byte. Every other
  pool of the suite is uncached to the processor, where neither call has anything to do.
- **a sync fenced on the queue behind a clear signals, and the clear is there with no finish**, **a sync fenced
  inside the list behind a clear signals, and the clear is there with no finish** (exact): the clear cases' list
  submitted and flushed (`nvnQueueFlush`) and never finished, as a game paces a frame: a sync fenced with
  `nvnQueueFenceSync` after the submit, or with `nvnCommandBufferFenceSync` as the list's last command, has to
  answer `nvnSyncWait` within ten seconds as signalled or satisfied, and the readback is judged right then. A line
  gives the wait's answer and the first pixel wrong.
- **a queue and a list told to wait for a sync that signalled run their clear** (exact): a sync fenced behind no
  work and waited for, then `nvnQueueWaitSync` before the submit and `nvnCommandBufferWaitSync` as the list's first
  command, and the same question of a second sync behind the work.
- **a wait on a sync nothing fenced fails, with a timeout or without** (exact): `nvnSyncWait` answers failed, not
  timed out, for a sync no queue and no list was told to signal. ScarletNX says so of the driver it measured; the
  layer answers the same on the console.
- **with no OpenGL, a sync from an OpenGL sync and an OpenGL sync from a sync both fail** (exact):
  `nvnSyncInitializeFromFencedGLSync` answers false with a status of one, and `nvnSyncCreateGLSync` answers four
  and leaves the place for the OpenGL sync as it was. The probe makes no OpenGL context; ScarletNX says both of
  the driver it read, and the layer answers the same.
- **a pool builder given a native handle forgets its storage, and takes storage again** (exact):
  `nvnMemoryPoolBuilderSetNativeHandleNVX` leaves the builder with no memory and no size, and storage set after it
  is given back. A native handle is the system's memory manager's, for an image another service made, so no pool
  is made of this one: what a pool of one holds is a game's to ask.
- **the queue has no error after work that was valid** (exact): `nvnQueueGetError`.
- **a queue builder gives back its flags and says which of its sizes were set** (exact): the command, compute and
  control memory sizes and the flush threshold, each unset on a new builder and then given back with what was set;
  the flags and the device. The builder makes no queue.
- **an event keeps what the processor signals: a write, an add and an add that wraps** (exact): an event in the
  last word of the readback pool's first page: where its builder and the event say it is, `nvnEventGetValue` after
  `nvnEventSignal` with a write, an add and an add of 0xffffffff, and that word read through the pool's mapping.
- **a sampler builder gives back each number it was set** (exact): the filters, the three wrap modes, the compare
  mode and function, the reduction filter, the level clamp, bias and snap, the greatest anisotropy and the border
  colour, none of them a default and every float exact, so the bits are compared; then the signed and the unsigned
  border colours, set and read back.
- **a sampler says what its builder had, and is the same as another of that state alone** (exact): the same
  getters on a sampler, its float border colour read as signed and unsigned words, and `nvnSamplerCompare`, which
  holds for two samplers made from one builder and not for one with another level bias.
- **the texture and sampler pools say where they are, and a builder registers as its sampler** (exact): a texture
  pool and a sampler pool in the readback pool's memory give back their memory pool, offset and number of
  descriptors; `nvnSamplerPoolRegisterSamplerBuilder` writes the 32 bytes `nvnSamplerPoolRegisterSampler` writes
  for a sampler made from that builder, read through the pool's mapping.
- **a buffer is a range of its pool: its address, its mapping, and what it and its builder say** (exact): a
  buffer of 0x800 bytes at 0x400 of the cached pool: the builder's device, pool, offset and size, the buffer's
  pool, offset and size, and `nvnBufferGetAddress` and `nvnBufferMap`, which are the pool's address and mapping
  moved by the offset.
- **bytes flushed from one buffer and copied to another are there after an invalidate** (exact): two buffers of
  that pool, the first written through `nvnBufferMap` and flushed (`nvnBufferFlushMappedRange`), a copy by the
  graphics unit between their addresses, and the second read through its mapping after
  `nvnBufferInvalidateMappedRange`. Every byte of the pool is judged, the ones around the buffers too.
- **the debug layer's calls are answered as a release driver answers them, and a label changes no object**
  (exact): no external debugger is attached; `nvnDeviceInstallDebugCallback` and `nvnDeviceWalkDebugDatabase`
  never call back; each of the six checkpoint switches of the device answers that it was taken; two debug
  domains made one after the other are numbered one apart; a buffer, a texture and a sampler have the debug
  number 0; and with a label set on the device, the queue, a pool, a texture, the command buffer, a buffer, a
  sampler, a program, a sync and the texture and sampler pools, each still says of itself what it said. The
  window gets its label in the window's case. These are what ScarletNX read in an NVN 55 release driver, the
  driver a game ships with; the layer answers the same on the console, so only a runtime's answers are news.
- **a window builder gives back what it was set** (exact): the device, the native window, three textures that
  are only named, each by its place and how many, a present interval of two and two active textures. The builder
  makes no window.
- **a blend state gives back its target, functions, equations and advanced mode**, **a channel mask and a colour
  state give back channels, blending, the logic operation and the alpha test**, **a depth and stencil
  state gives back its depth test and each face's stencil**, **a multisample state gives back its samples, its
  switches and its sample locations' grid**, **a polygon state, a vertex attribute and a vertex stream give back
  what they were set** (exact): every setter of each state object with a number that is no default, and its
  getters. A face's stencil state is set for the front, for the back and for both in one call; a flag cleared
  leaves the one beside it; the grid of sixteen sample locations with eight raster samples is one pixel by two,
  which is ScarletNX's answer for a driver and the layer's too. No state here is bound: what a draw makes of the
  fields is for cases that draw with them.
- **a texture builder and a view give back their swizzle, depth and stencil mode and the rest** (exact): the
  channel swizzle, the depth and stencil mode, the stride, the GL texture name, the packaged texture's data and
  the twelve bits of its layout a driver keeps, on a builder; the swizzle and the mode on a view, unset on a new
  one.
- **a texture says its swizzle, storage and address, and is the same as its twin alone** (exact): two 16x16
  textures from one builder at one place: the swizzle the builder had, the builder's storage size, the levels
  texture's address (its pool's and its offset), `nvnTextureCompare` for the twins and against the levels
  texture, a view offset of nought for no view. Of a driver's own account three things are held to ScarletNX's
  reading of a driver: raw storage class 2 for a colour format, a sparse tile of noughts for a texture that is
  not sparse, and the width and height NVIDIA's surface starts with. The storage classes, the ZCull sizes and the
  stride are asked and compared with nothing: a failing line carries them.
- **texels the processor writes into a texture are what it and the graphics unit read back** (exact):
  `nvnTextureWriteTexels` of a 64x64 picture, `nvnTextureFlushTexels`, the graphics unit's copy of the texture
  to the readback pool, `nvnTextureInvalidateTexels` and `nvnTextureReadTexels`: both readbacks, every byte.
- **a part of a texture written and read with row strides keeps the bytes between rows** (exact): a 16 by 8 part
  at (8, 4), `nvnTextureWriteTexelsStrided` from rows 80 bytes apart and `nvnTextureReadTexelsStrided` into rows
  96 apart, the bytes between the destination's rows as they were.
- **a 1D texture, each layer of an array and each slice of a 3D texture keep a picture of their own** (exact): a
  texture of 64 texels in a row (`nvnTextureBuilderSetSize1D`), a 16x16 array of three layers and a 16x16x4
  texture (`nvnTextureBuilderSetSize3D`), each layer and slice copied into through a copy region's depth and read
  back from it, every byte of the readback judged.
- **a queue builder gives back its memory and priority, and a queue's memory counts are nought once reset**
  (exact): `nvnQueueBuilderSetQueueMemory`, `SetQueuePriority` and `SetPrimaryQueue` with their getters, and the
  command, control and compute memory a queue has used right after `nvnQueueResetMemoryUsageCounts`. What the
  counts come to after work is a driver's own, as the queue memory a builder asks for is, and held to nothing.
- **a list writes an event's value over the one the processor signalled**, **a list adds to an event's value**
  (exact): `nvnCommandBufferSignalEvent` in its two modes, the value read with `nvnEventGetValue` once the queue
  has finished. The sum is skipped as `unavailable` where the layer answers: deko3d has a write by the graphics
  unit and no sum.
- **a list that waits for an event's value runs when the processor signals it, and not before** (exact):
  `nvnCommandBufferWaitEvent` for 9 ahead of a clear and a copy, flushed while the event is 7 with a sync fenced
  behind it. The sync must run out a wait of a twentieth of a second, and signal once the processor writes 9, with
  the clear there. Skipped as `unavailable` where the layer answers.
- **a virtual pool's page is the physical pool's page mapped into it** (exact): a physical pool of two 64 KiB
  pages and a virtual one, `nvnMemoryPoolMapVirtual` of the physical pool's second page as the virtual pool's
  first, and bytes the processor wrote through the physical pool copied by the graphics unit from the virtual
  pool's address. Skipped as `unavailable` where the layer answers: deko3d has no such memory.
- **a window acquires and presents both its textures** (none tier): two 1280x720 textures on the default window,
  each acquired with `nvnQueueAcquireTexture`, cleared and presented once. Nothing is read back. Skipped as
  `unavailable` while the program's screen holds the window, which a run begun on the start screen keeps up to its
  end, `noprogress` or not: a quick run from the mode file runs it, as the game family presents only then, and the
  start screen says so when the file names nvn. The two cases after it use its window, and are skipped with it.
- **a window says what it is of, and keeps the active textures, the interval and the crop it is set** (exact):
  that window's native window and number of textures; `nvnWindowSetNumActiveTextures` with one and then with both,
  `nvnWindowSetPresentInterval` with two and `nvnWindowSetCrop` with a quarter of the texture sixteen texels in,
  each read back; one frame acquired, cleared and presented under that crop and interval; then the interval and
  the crop set back to the whole texture, and read back.
- **a texture the window hands with a sync is free once the sync signals, and presents** (exact):
  `nvnWindowAcquireTexture`, the window's own acquire, which hands a texture at once and a sync for when the
  display has let it go, where `nvnQueueAcquireTexture` has the queue wait. The answer is success with a texture
  of the window, and `nvnSyncWait` on that sync within ten seconds is signalled or satisfied; the texture is then
  cleared and presented.

  What a window getter is held to is that it gives what the setter took. What a new window answers before
  anything is set (its crop, its active textures) is the driver's, and no case holds a runtime to it. Nor does any
  hold it to which textures a window with fewer active ones hands: deko3d takes whichever the display gives back,
  so the layer refuses an acquire from such a window, and the case sets all of them active before it acquires.

The Switch 2 program runs these and two more, of the Ounce's windows (API 231):

- **an Ounce window builder starts with the driver's colour space and HDR metadata, and gives back what it is
  set** (exact): a new builder has colour space 0, Rec. 709's primaries with a D65 white point, a display of 0.03
  to 1000 nits and content light levels of 200 and 60, which is what ScarletNX read in an Ounce game's driver; then
  a colour space, a display's ten numbers and a content's two, none of them a default and every float exact, come
  back bit for bit.
- **an Ounce window has its builder's HDR metadata, and gives back what it is set** (exact): the suite's window,
  made from a builder left as new, answers those first numbers; `nvnWindowSetHdrDisplayMetadata` and
  `nvnWindowSetHdrContentMetadata` with others come back bit for bit.

  No machine but ScarletNX runs that program, so these two say that its API 231 windows keep the numbers; the
  first numbers are its reading of the driver, which no console has been asked here.

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

### A texture past its edges

Three more cases draw a second scene the same three ways, with the first scene's shaders: one 4 by 4 texture in
each quarter of the target, over coordinates from -1 to 3, so four of it across and down, two pixels a texel and
every pixel centre a quarter of a texel from a texel's edge. Each quarter samples it nearest through a sampler of
its own wrap mode (`DrawPathTextureSampled`):

- top left, `REPEAT`: the texture four times each way;
- top right, `MIRRORED_REPEAT`: every other one turned around;
- bottom left, `CLAMP_TO_EDGE`: the texture once, its edge texels drawn out to the quarter's sides;
- bottom right, `CLAMP_TO_BORDER`: the texture once in opaque green, the sampler's border colour
  (`nvnSamplerBuilderSetBorderColor`).

The cases are **a texture drawn past its edges through deko3d repeats, mirrors, holds its edge and shows its
border**, **the same texture past its edges through the layer's own NVN functions does the same** and **the same
texture past its edges through the NVN the program looks up does the same** (exact). What each pixel has to hold
is worked out in `source/tests/t_nvnscene.c`, with no reference of the host build's. A failing case says what the
first wrong pixel shows: another texel of the texture (another wrap mode), the border colour, or the clear
colour.

### Indexed draws

Three more draw a third scene the three ways (`DrawPathDrawIndexed`): a quad of one flat colour in each quarter,
each of four vertices and six indices, the quarters' vertices one after another in one vertex buffer.

- top left: indices of two bytes, `nvnCommandBufferDrawElements`;
- top right: indices of four bytes;
- bottom left: indices of one byte;
- bottom right: the top left quarter's own indices with a base vertex of twelve,
  `nvnCommandBufferDrawElementsBaseVertex`. Drawn with no base they are the top left quad again, and the quarter
  keeps the clear colour.

The cases are **four quads drawn through deko3d by indices of one, two and four bytes and a base vertex are each
in its quarter**, **the same four indexed quads through the layer's own NVN functions are each in its quarter**
and **the same four indexed quads through the NVN the program looks up are each in its quarter** (exact). A
failing case says whose colour the first wrong pixel has.

### The binds that take several, and the deferred ones

The first scene again, twice, down the two NVN ways, with every vertex buffer, uniform buffer and texture bound
through other calls (`DrawPathSpec.binds`); the pixels are the first scene's.

- **the scene of five draws bound through the layer's own calls that take several holds what the processor works
  out**, and **... through the looked up calls that take several ...** (exact): `nvnCommandBufferBindVertexBuffers`,
  `BindUniformBuffers` and `BindTextures`, and at the start of the list the two hints a game records there,
  `nvnCommandBufferBindCoverageModulationTable` (a table nothing reads, no draw modulating coverage) and
  `nvnCommandBufferSetShaderScratchMemory` (a page of the data pool nothing else is in).
- **the scene of five draws bound through the layer's own deferred calls holds what the processor works out**, and
  **... through the looked up deferred calls ...** (exact): `nvnCommandBufferBindUniformBuffersDeferred` and
  `BindTexturesDeferred`, which read their ranges and handles when the list runs. Each bind points at a range of
  its own that stays as it is until the list has run, so a runtime that reads late and one that reads at once
  draw the same; a runtime that read every bind from one place would draw the last range for all.

### A uniform buffer updated inside the list

A fourth scene the three ways: a quad in each quarter, all coloured by one uniform buffer of the fragment stage,
which the list updates before each draw (`DrawPathUpdate`, `nvnCommandBufferUpdateUniformBuffer`). The processor
leaves the clear colour in the buffer. The cases are **four quads drawn through deko3d from one uniform buffer
updated before each are each its own colour**, **the same four quads of an updated uniform buffer through the
layer's own NVN functions are each its own colour** and **... through the NVN the program looks up ...** (exact):
updates that do not land leave the clear colour, and updates that all land before the first draw make the four
quads the last colour.

### Instanced draws, and draws that read their numbers

A fifth scene the three ways: eight cells, four across and two down, each one instance of the same quad. The
quad's vertices are the first cell's; an instance's entry of a second vertex stream, which steps once an instance
(`nvnVertexStreamStateSetDivisor`, `DrawPathInstances`), moves it to its cell and colours it
(`scene_instanced.vert.glsl`). Two instances a draw:

- cells 0 and 1: `nvnCommandBufferDrawArraysInstanced`;
- cells 2 and 3: `nvnCommandBufferDrawElementsInstanced`, from base instance 2;
- cells 4 and 5: `nvnCommandBufferDrawArraysIndirect`, its count, instances, first vertex and base instance 4 in a
  buffer the graphics unit reads;
- cells 6 and 7: `nvnCommandBufferDrawElementsIndirect`, from base instance 6.

The cases are **eight instances of a quad drawn through deko3d, by instanced draws and by draws that read their
numbers, are each in its cell**, **the same eight instances through the layer's own NVN functions are each in its
cell** and **the same eight instances through the NVN the program looks up are each in its cell** (exact). A draw
that starts at instance nought draws the first two cells again and leaves its own the clear colour; a failing
case says whose colour the first wrong pixel has.

### The draws deko3d has none of

A sixth scene, for the NVN the program looks up alone: deko3d has neither of its draws, so the deko3d way cannot
draw it and the layer lacks them.

- the top left quarter: the first scene's 8 by 8 texture drawn with no program, four pixels a texel, through its
  own sampler (`nvnCommandBufferDrawTexture`, its two regions each a corner's x and y and the other corner's, in
  the target's pixels and the texture's texels);
- the top right quarter: the clear colour;
- the bottom row: the instanced scene's cells 4 to 7, two by `nvnCommandBufferMultiDrawArraysIndirectCount` and
  two by `nvnCommandBufferMultiDrawElementsIndirectCount`, each draw one instance from its own base instance.
  The word the graphics unit reads says two draws, and each buffer holds a third draw's numbers, cell 0's: a
  runtime that draws as many as the most it was given paints over the texture.

The case is **a texture drawn with no program and as many draws as a word says, through the NVN the program looks
up, are each in its place** (exact), skipped as `unavailable` where the layer answers. Which row of the target a
region's y of nought is follows the device's window origin, the upper left here, as every draw of the scenes
does; no console has said so of this draw.

### A capture of what a vertex stage writes

A seventh scene, for the NVN the program looks up alone as the sixth is: transform feedback, which deko3d has no
call for and the layer lacks.

- the top row: the instanced scene's program draws cells 0, 1 and 2, one instance a draw, with its vertex
  stage's outputs captured into a buffer (`nvnCommandBufferBindTransformFeedbackBuffer`). The capture begins
  before cell 0 (`nvnCommandBufferBeginTransformFeedback`), is paused for cell 1
  (`PauseTransformFeedback`), resumed for cell 2 (`ResumeTransformFeedback`) and ended
  (`EndTransformFeedback`);
- the bottom row: the capture buffer bound as the vertices and drawn with as many as the capture wrote
  (`nvnCommandBufferDrawTransformFeedback`), through one instance that moves it down a row and gives it cell 7's
  colour. A captured vertex is the position the stage wrote, its texture coordinate and its colour, which is a
  vertex of the scene, so the row holds cells 0 and 2 again and nothing under cell 1.

A capture that a pause does not stop paints under cell 1; one that a resume does not start again leaves the place
under cell 2 clear; one that wrote nothing leaves the row clear. What the stage's capture takes of a vertex is
said in its control section by `NvnLayerShaderCapture`, where a console's compiler writes it.

The cases are **what a capture took of two draws, paused for a third between them, is drawn again a row down
through the NVN the program looks up** and **the same capture into a buffer bound by the call that takes several
is drawn again a row down** (`nvnCommandBufferBindTransformFeedbackBuffers`), both exact and skipped as
`unavailable` where the layer answers.

One shader binary serves every way because the scene's shaders keep to the layer's two rules ([The NVN
layer](../NvnLayer.md#what-a-shader-may-use)): uniform blocks from `binding = 1`, and textures by the handle in the
block at `binding = 0`.

## The command buffer, down two ways

What a list does past the binds and the draws of the scenes is asked with NVN's own calls, which deko3d's way has
no word for, on the command buffer of an NVN way (`source/drawpath_nvn.h`), so each case of
`source/tests/t_nvncmd.c` runs twice: **through the layer's own NVN functions**, before the suite's lookup, where
the graphics unit answers the layer's deko3d commands on every machine, and **through the NVN the program looks
up**, after the scenes. A case draws flat quads with the scene's shaders into the cells of a 64 by 64 target,
sixteen pixels a side and each in a colour of its own, and judges every pixel: a cell holds its colour or the clear
colour. What a list writes to memory lands in a page the processor reads, filled first with a byte no case
expects. Exact tier.

### Counters and the render enable

- **the samples counter reports a quad's 256 pixels and none after a reset**: `nvnCommandBufferResetCounter`, one
  cell drawn, `nvnCommandBufferReportCounter` of the samples passed, then a reset and a report with nothing drawn
  between. The counts are 256 and 0.
- **two values a list reports are in memory once it has run**: `nvnCommandBufferReportValue` twice, a report's
  16 bytes apart: each value is the four bytes at its address, and nothing lands past the second report. The
  twelve bytes after a value are not judged: deko3d writes a value as a whole report, with a time, and ScarletNX's
  NVN as the word alone, and no console has been asked here which the driver does.
- **two timestamp reports hold no count and times between the device's clock before and after**: a timestamp
  report, a cell drawn, another. Each holds a count of 0; `nvnDeviceGetTimestampInNanoseconds` of the first is no
  earlier than `nvnDeviceGetCurrentTimestampInNanoseconds` read before the list, the second no earlier than the
  first, and the clock read after the list no earlier than the second.
- **a timestamp report's word is the graphics unit's ticks, 384 every 625 nanoseconds**: the nanoseconds the
  device gives for a report are its word times 625 over 384. The graphics unit's timer runs at 614.4 MHz, which is
  what the console's reports hold and what deko3d converts by; a runtime that writes the reports itself has to
  write that unit, since a program may read the word.
- **the render enable draws under on and under a condition that holds and skips the rest**: nine cells, each
  drawn after one word of the render enable: `nvnCommandBufferSetRenderEnable` off and on, then
  `nvnCommandBufferSetRenderEnableConditional` if equal and if not equal over two counts the processor wrote equal
  (their times differ), over two it wrote apart, and over the 256 and the 0 a list that finished before reported,
  then on again. The five under a word that holds are drawn, and the four others are left clear. NVN compares the
  count at the address with the one 16 bytes on, as two reports lie; the layer sends the 3D engine's render enable
  itself, which deko3d never writes. A clear under the render enable is not asked: no console has said here
  whether it obeys.

### The command buffer's memory and lists in lists

- **a command buffer's memory used and free come to what it was given, and draws take some**: the command
  buffer is given its command and control memory again, whole, and asked before a list, after four draws in it
  and after it: `nvnCommandBufferGetCommandMemorySize` and `GetControlMemorySize` are the bytes given; the used
  and the free of each (`GetCommandMemoryUsed`, `GetCommandMemoryFree`, `GetControlMemoryUsed`,
  `GetControlMemoryFree`) come to its size each time; more command memory is used after the draws than before
  the list; and `nvnCommandBufferIsRecording` holds in the list and at no other time. How many bytes a command
  takes is the driver's and is not asked: the layer answers with what deko3d wrote, which it finds by the fill
  byte it left in the memory, and with no control memory used, since deko3d keeps its own.
- **a list out of command memory asks the program's callback and draws on**: the command buffer is given
  0x2000 bytes, a callback (`nvnCommandBufferSetMemoryCallback`) and a pointer for it
  (`SetMemoryCallbackData`), both read back, and the two switches of the callback turned off and on
  (`SetCommandMemoryCallbackEnabled`, `SetControlMemoryCallbackEnabled`); then the sixteen cells are drawn again
  and again until the callback is called, 6400 draws at most. Its first call has the command buffer, the event
  for command memory, a number of bytes above none and the program's pointer; it adds the rest of the memory,
  and every cell is there. A call for control memory is given control memory and is not judged: the layer
  never makes one.
- **a list called from another and a list copied into it draw their cells**: two lists recorded and kept, a cell
  each, the first with the clear; the list that is submitted names the first with
  `nvnCommandBufferCallCommands`, the second with `nvnCommandBufferCopyCommands`, and draws a cell itself. The
  three cells are there. The layer calls a copied list where it lies, deko3d having no copy, so it asks of a
  copy what NVN asks of a call alone, that the list's memory is still there when the queue runs it.

`nvnCommandBufferSetRecordingMode`, `SetComputeOverlapMode` and `SetComputeWorkloadPriority` are the Switch 2
program's to set, before a dispatch of [the nvn2 suite](Nvn2.md): nothing a program can read or draw changes with
them here, and what they change on a console is the driver's.

### Clears, strides and discards

Three of these make 16 by 16 textures of their own, in a pool of their own, and read each back with
`nvnCommandBufferCopyTextureToBuffer`, every texel judged.

- **clears of an unsigned and a signed integer target hold the integers given**: an RGBA8UI and an RGBA8I
  texture, each bound as the target and cleared whole (`nvnCommandBufferClearColorui` with 1, 200, 255 and 7;
  `nvnCommandBufferClearColori` with -1, 127, -128 and 5), then cleared again through a mask of two channels,
  which changes those two and no other. The way's own target is bound again after them, and a cell drawn there.
- **a clear of a texture that is no target reaches its region and its mask's channels**: `nvnCommandBufferClearTexture`
  on an RGBA8 texture, `ClearTextureui` and `ClearTexturei` on the two integer ones: each texture cleared whole,
  then the square from (4,4) to (12,12) through a mask. Inside the square the masked channels are the second
  clear's and the others the first's; outside it everything is the first's. The cell drawn after the clears, with
  no target bound again, is in the way's target: a clear of a texture leaves the list's targets and scissor alone.
  The layer has to bind the texture to clear it, deko3d clearing targets only, and puts both back.
- **a range of a buffer cleared to a word holds it, and nothing beside the range**: `nvnCommandBufferClearBuffer`
  over 0x60 bytes at 0x10 of the page, every word of the page judged. deko3d has no fill: the layer fills a unit
  of its own memory with the word and has the graphics unit copy from it.
- **a command buffer gives back the copy strides it was set**: `nvnCommandBufferSetCopyRowStride` and
  `SetCopyImageStride`, then `GetCopyRowStride` and `GetCopyImageStride`: 288 and 0x4800, and 0 and 0 once set
  back. What an image stride does to a copy of several layers is not asked: the layer has textures of one.
- **targets discarded, then cleared and drawn, hold the clear and the draw**: a cell drawn,
  `nvnCommandBufferDiscardColor` and `DiscardDepthStencil`, which leave the targets undefined, then both cleared
  and another cell drawn under the depth test: the first cell is gone and the second there, as with no discard.
  What a discarded target holds before it is cleared is not asked.


### The debug layer's marks

- **draws between debug markers, groups and checkpoints are as without them**: three cells drawn between
  `nvnCommandBufferInsertDebugMarker`, `InsertDebugMarkerDynamic` and `InsertDebugMarkerStatic`,
  `PushDebugGroup`, `PushDebugGroupDynamic` and `PushDebugGroupStatic`, `PopDebugGroup` and `PopDebugGroupId`, and
  `InsertCheckpointDynamic` and `InsertCheckpointStatic`, which both answer that the checkpoint was taken. The
  marks name parts of a list for the debug layer and record nothing a picture shows.

The Switch 2 program has the Ounce's own, **a clear between the Ounce's graphics and compute markers and groups
is the clear** (exact), in the suite's first part: `nvnCommandBufferPushDebugGroupGraphics`,
`InsertDebugMarkerGraphics`, their compute kin and the two pops around the clear cases' clear.
`nvnProgramBuilderSetDebugDataHashes` and `GetDebugDataHashes` have no case: what the getter gives back for the
hashes set is the driver's, and ScarletNX keeps none.

### The render state a list sets by itself

- **a scissor and a viewport set by number bound and place a quad**: `nvnCommandBufferSetScissors` and
  `SetViewports`, one of each from number 0. Cell 0's quad under a scissor of the eight pixels in its middle is
  that square alone; through a viewport of the target's lower right quarter it is the quarter's own top left
  sixteenth, eight pixels a side at (32,32); with both back over the target a cell is where it always is.
- **depth ranges move a quad across the depth test and depth bounds test the target's depth**: a depth target
  holding 0.6 and a test that passes what is nearer; the quads lie half way down the range. With the range 0 to 1
  or 0 to 0.4 a quad is drawn and with 0.8 to 1 or 0.9 to 1 it is hidden, set by `nvnCommandBufferSetDepthRange`
  or by `SetDepthRanges` for viewport 0. `SetDepthBounds` tests what the target holds: on at 0.7 to 1 the quad is
  hidden, on at 0.5 to 0.7 or off it is drawn.
- **the rasterizer discards, clamps depth and takes a flat colour from the provoking vertex**:
  `nvnCommandBufferSetRasterizerDiscard` on, nothing drawn, and off, drawn; a quad a whole depth range past the
  far plane is clipped away, and drawn under `SetDepthClamp`; and of two quads whose triangles have one vertex of
  the cell's colour, the first or the last, each is that colour under `SetProvokingVertexMode` 0 or 1, the scene's
  fragment stage taking the colour flat.
- **a point of sixteen pixels fills its cell**: `nvnCommandBufferSetPointSize` and one point at a cell's middle.
- **a strip with a restart index is two quads**: `nvnCommandBufferSetPrimitiveRestart` with 0xffff, and a
  triangle strip of nine two byte indices, the fifth 0xffff: the quads of cells 8 and 10 and nothing across cell 9.
- **a viewport swizzle of x and y turns a quad about the diagonal**: `nvnCommandBufferSetViewportSwizzles` giving
  x the position's y and y its x: cell 1's quad lands on cell 11.
- **draws around the tiled cache's actions are as without them**: `nvnCommandBufferSetTiledCacheTileSize` and
  `SetTiledCacheAction` (flush, disable, enable, flush) around two draws. The tiled cache is how the graphics unit
  orders its work and no picture shows it, so the case holds a runtime only to drawing on.
- **the alpha test's reference and the blend colour a list sets decide two draws each**:
  `nvnCommandBufferSetAlphaRef` and `SetBlendColor`. A quad whose alpha is a fifth, under the test "greater", is
  gone with a reference of a half and there, with that alpha, with one of an eighth; a quad blended as its colour
  times the blend colour keeps its red and blue under a blend colour of red and blue, and only its green under
  green.
- **the stencil's reference, compare mask and write mask a list sets decide what is drawn**:
  `nvnCommandBufferSetStencilRef`, `SetStencilValueMask` and `SetStencilMask`, over a target of 24 bits of depth
  and 8 of stencil cleared to 0x0f. "Equal" draws with the reference 0x0f, not with 0x0e, and with 0x0e once the
  compare mask is 0xf0; a replace by 0xff under the write mask 0x30 leaves 0x3f, which a last quad is drawn for.
- **a sample mask without the first sample draws nothing, and a clamp holds a polygon offset**:
  `nvnCommandBufferSetSampleMask` over a target of one sample, and `SetPolygonOffsetClamp` with the offset on for
  filled polygons: over a depth of 0.6 in 24 bits a quad at 0.5 offset by 2 to the 23rd units is at 1 and hidden,
  and clamped to a twentieth it is at 0.55 and drawn.
- **draws under five hints to the graphics unit are as without them**: `nvnCommandBufferSetStencilCullCriteria`,
  `OverrideSubtileSize`, `SetColorReductionThresholds`, `SetColorReductionEnable` and
  `SetZCullZF32CompressionEnable`, which speed the graphics unit's work and show in no picture; the layer has no
  deko3d call for any and records nothing.
- **a program whose sample shading is its code's draws and asks for no subtile**: `nvnProgramSetSampleShading`
  with nought before the shaders are set, and `nvnProgramGetSubtileSize`, which is nought for every program: a
  driver's `nvnProgramSetShaders` leaves it there and nothing in NVN sets it. The layer lacks the other modes of
  sample shading, deko3d running a fragment stage as its code says.
- **a cleared target of four samples resolves into its colour by each of the four downsamples**:
  `nvnCommandBufferDownsample`, `DownsampleTextureView`, `TiledDownsample` and `TiledDownsampleTextureView`, each
  from a 16 by 16 texture of four samples cleared to a colour of its own into a texture of one sample, which is
  then read: every texel is the clear's colour. The tiled ones are the same resolve a tile of the tiled cache at a
  time, and the layer makes all four deko3d's resolve.
- **depth tested draws around a depth target's resolve and its culling data's save and restore are as without
  them**: `nvnCommandBufferResolveDepthBuffer`, `SaveZCullData` and `RestoreZCullData`, the graphics unit's own
  business with a depth target, which no picture shows; the layer records nothing for them.

- **conservative rasterization draws every pixel a quad touches**: four quads each 0.6 of a pixel inside its
  cell. By the usual rule the cell's outer ring of pixels, touched but with centres outside, stays clear; with
  `nvnCommandBufferSetConservativeRasterEnable` the cell is full, and still with four more bits of subpixel
  precision each way (`SetSubpixelPrecisionBias`); with it off the ring is clear again.
- **a conservative quad dilated by half a pixel reaches the pixels a third of one away**: two quads 1.3 pixels
  inside their cells, conservative: with no dilation the ring is clear, and with
  `nvnCommandBufferSetConservativeRasterDilate` of 0.5 the cell is full.
- **a line of width four is four rows of pixels**: `nvnCommandBufferSetLineWidth` and one level line along the
  boundary between two rows, from 8 pixels in to 8 from the far side. Half way along it is four rows thick, from
  one to three rows above the boundary, and every pixel between its ends is judged against those rows. Which of
  the three it is, and the two pixels at each end, are the rasterizer's rounding and are not asked.

- **patches of the size and the outer levels a list sets fill their cells**: a program of the case's own, the
  scene's vertex and fragment stages with a tessellation evaluation stage between them (`quad.tese`, the geometry
  suite's) and no control stage, so the levels are the list's. `nvnCommandBufferSetPatchSize` of four,
  `SetInnerTessellationLevels` and `SetOuterTessellationLevels`, and three patches: each fills its cell in its
  first vertex's colour, with the outer level the list had set in alpha, which the evaluation stage writes there:
  3 for the first two patches and 5 for the third. The inner levels change nothing a picture of a flat quad
  shows. `source/drawpath_nvn.h` hands such a case the shader data of a stage past a scene's two
  (`DrawPathNvnStage`).

The Ounce's `nvnCommandBufferSetShadingRatePaletteMode` is set by the nvn2 suite's draw with no shading rate image.

### The device and a program

- **the device says its modes and its builder its flags, and clear values register**: a device builder gives
  back the flags it was set (`nvnDeviceBuilderGetFlags`); the way's device says the top left origin and the near
  plane at zero every way of a scene sets (`nvnDeviceGetWindowOriginMode`, `nvnDeviceGetDepthMode`); and each of
  `nvnDeviceRegisterFastClearColor`, `Colori`, `Colorui`, `Depth` and `Stencil` answers that the value was
  taken. A registered value is a hint to the graphics unit's compression, so the clear and the cell after it are
  as in every case.
- **a program set from the longer shader data draws**: a program of the case's own, set with
  `nvnProgramSetShadersExt` from the way's program's two stages, each in NVN 55's shader data of eight bytes
  more, and bound: three cells drawn with it are there.

- **a texture's handle and a sampler's put together sample, and a texel fetch handle fetches**: two 4 by 4
  textures, the first sampled nearest and held at its edge, the second, its texels turned about, repeating.
  `nvnDeviceGetSeparateTextureHandle` of the first and `nvnDeviceGetSeparateSamplerHandle` of the second's
  sampler go in two texture slots, and the fragment stage (`handles.frag`) puts their low words one over the other,
  as a console's compiled shader puts a separate pair together, and samples over coordinates to 2: the cell shows
  the first texture four times, two pixels a texel. A cell beside it fetches texels through
  `nvnDeviceGetTexelFetchHandle` of the first texture, with no sampler: the texture once, four pixels a texel. A
  failing pixel is the other texture's (the wrong descriptor), an edge texel drawn out (the first texture's own
  sampler), or nothing.

The Switch 2 program has **an Ounce program builder gives back what it was set, a ray tracing program's numbers
too** (exact), in the suite's first part: the builder's device, its shader data by address and count, its sample
shading mode, and the greatest callable and trace recursion depths and the flags of a ray tracing program. The
builder makes no program. ScarletNX draws no ray tracing yet, so nothing else of it has a case: the acceleration
structures, a program's shader groups and a queue's ray tracing memory are answers it makes up.

Without a case: subroutine linkage, which needs shaders with subroutines and the map a console's compiler writes
of them; `nvnProgramSetSampleShading`'s modes past the first, which need a multisampled target to show; and
`nvnProgramGetScratchBytesPerWarp`, a number of ray tracing's that ScarletNX makes up.

## Where nvn/nvn.h comes from

[The NVN layer](../NvnLayer.md#where-its-numbers-come-from) says where its types, its prototypes and its offsets
come from. None holds driver code: names, numbers, sizes and argument types.
