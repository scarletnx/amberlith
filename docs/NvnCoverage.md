# NVN coverage

## Index

* [How to read it](#how-to-read-it)
* [How the cases are read](#how-the-cases-are-read)
* [By group](#by-group)
* [The cases](#the-cases)
* [Device](#device)
* [Memory pools](#memory-pools)
* [Buffers](#buffers)
* [Textures](#textures)
* [Samplers](#samplers)
* [Programs](#programs)
* [State objects](#state-objects)
* [Queues](#queues)
* [Sync and events](#sync-and-events)
* [Windows and present](#windows-and-present)
* [Counters and queries](#counters-and-queries)
* [Commands: memory and recording](#commands-memory-and-recording)
* [Commands: binds](#commands-binds)
* [Commands: draws and dispatches](#commands-draws-and-dispatches)
* [Commands: clears and copies](#commands-clears-and-copies)
* [Commands: render state](#commands-render-state)
* [DLSS](#dlss)
* [Ray tracing](#ray-tracing)
* [Debug](#debug)

<!-- Written by tools/nvn-coverage.py: change the cases, the layer or the tool, and run it. -->

Which of the NVN functions ScarletNX answers a case of the probe calls. ScarletNX's `nvn.hook` at 3f7d34fadf3f
names 677; the layer has 502 of them in its API 55 build and 619 in its API 231 build; a case calls 619. The
other 58 have no call at all: a runtime could answer any of them wrongly and every case would still pass.

## How to read it

- **A function** is a `hook` or a `route` line of `tools/nvn/nvn.hook`, the copy of ScarletNX's
  `src/nx/gpu/nvn/nvn.hook` that `tools/nvn/PIN` names by commit and SHA-256.
- **Its group** is the hook's own `family`, with the command buffer's functions told apart by what
  they record and four groups taken by name from every family but DLSS's: debug, ray tracing,
  sync and events, counters and queries. `GROUPS` in the tool is the rule, tried in its order.
- **Layer** is the API versions whose build of [the layer](NvnLayer.md) has the function: a case
  can call only what the layer has.
- **Cases** are the cases whose run calls the function, by the labels of [The cases](#the-cases).
  Called says nothing of judged: a builder's setter is called by every case that makes the object,
  and only a case that reads the result back holds the runtime to it.

## How the cases are read

From the C files, with no compiler and no run, so the table is the same wherever it is made:

- **the NRO** (API 55): `source/tests/t_nvn.c`, `source/tests/t_nvnscene.c`, `source/drawpath.c`, `source/drawpath_nvn.c`, `source/tests/t_nvncmd.c`, from `RunNvn`.
- **the Switch 2 program** (API 231): `source/tests/t_nvn.c`, `ounce/t_nvn2.c`, from `RunNvn` and `RunNvn2`.

A call belongs to the case whose `ReportStart` opened before it: to the end of the brace of an
`if (ReportStart(...))`, or, after an `if (!ReportStart(...))` that leaves, until the case's own
`CHECK_` or `ReportCase` in that brace. A function called inside a case is the case's too, with
the functions it calls, and a table of functions counts for whoever names the table. Where a
function's `if` only compares its parameters with constants a call passed, the way that call takes
is the one read; any other choice counts both ways. A call of the layer's `NvnLoadProcs` is a call
of the two functions it gets every other through. A call no case is open for is **outside a
case**. Of the scene's three ways (`source/drawpath.h`) only the one
through the NVN the program looks up is counted: the other two are deko3d's calls and the layer's
own bodies, which no runtime's NVN takes. `#if NVN_LAYER_API` chooses a build's lines.

`tools/nvn-coverage.py --check` says whether this file is what the tree gives, and the host build's
tests run it; `--gaps GROUP` lists a group's functions with no case.

## By group

| Group | Functions | Layer, API 55 | Layer, API 231 | Called by a case | No case |
| --- | ---: | ---: | ---: | ---: | ---: |
| [Device](#device) | 36 | 22 | 23 | 23 | 13 |
| [Memory pools](#memory-pools) | 18 | 18 | 18 | 18 | 0 |
| [Buffers](#buffers) | 16 | 16 | 16 | 16 | 0 |
| [Textures](#textures) | 95 | 94 | 95 | 95 | 0 |
| [Samplers](#samplers) | 45 | 45 | 45 | 45 | 0 |
| [Programs](#programs) | 20 | 4 | 13 | 13 | 7 |
| [State objects](#state-objects) | 82 | 82 | 82 | 82 | 0 |
| [Queues](#queues) | 30 | 30 | 30 | 30 | 0 |
| [Sync and events](#sync-and-events) | 24 | 22 | 22 | 22 | 2 |
| [Windows and present](#windows-and-present) | 35 | 25 | 35 | 35 | 0 |
| [Counters and queries](#counters-and-queries) | 6 | 6 | 6 | 6 | 0 |
| [Commands: memory and recording](#commands-memory-and-recording) | 25 | 22 | 22 | 22 | 3 |
| [Commands: binds](#commands-binds) | 41 | 24 | 39 | 39 | 2 |
| [Commands: draws and dispatches](#commands-draws-and-dispatches) | 26 | 15 | 26 | 26 | 0 |
| [Commands: clears and copies](#commands-clears-and-copies) | 25 | 18 | 18 | 18 | 7 |
| [Commands: render state](#commands-render-state) | 36 | 23 | 23 | 23 | 13 |
| [DLSS](#dlss) | 58 | 0 | 58 | 58 | 0 |
| [Ray tracing](#ray-tracing) | 15 | 0 | 6 | 6 | 9 |
| [Debug](#debug) | 44 | 36 | 42 | 42 | 2 |
| **All** | **677** | **502** | **619** | **619** | **58** |

## The cases

| Label | Suite | Case | In |
| --- | --- | --- | --- |
| nvn-1 | nvn | a scene of five draws through deko3d holds what the processor works out | the NRO |
| nvn-2 | nvn | the same scene through the layer's own NVN functions holds what the processor works out | the NRO |
| nvn-3 | nvn | a texture drawn past its edges through deko3d repeats, mirrors, holds its edge and shows its border | the NRO |
| nvn-4 | nvn | the same texture past its edges through the layer's own NVN functions does the same | the NRO |
| nvn-5 | nvn | four quads drawn through deko3d by indices of one, two and four bytes and a base vertex are each in its quarter | the NRO |
| nvn-6 | nvn | the same four indexed quads through the layer's own NVN functions are each in its quarter | the NRO |
| nvn-7 | nvn | the scene of five draws bound through the layer's own calls that take several holds what the processor works out | the NRO |
| nvn-8 | nvn | the scene of five draws bound through the layer's own deferred calls holds what the processor works out | the NRO |
| nvn-9 | nvn | four quads drawn through deko3d from one uniform buffer updated before each are each its own colour | the NRO |
| nvn-10 | nvn | the same four quads of an updated uniform buffer through the layer's own NVN functions are each its own colour | the NRO |
| nvn-11 | nvn | eight instances of a quad drawn through deko3d, by instanced draws and by draws that read their numbers, are each in its cell | the NRO |
| nvn-12 | nvn | the same eight instances through the layer's own NVN functions are each in its cell | the NRO |
| nvn-13 | nvn | the samples counter reports a quad's 256 pixels and none after a reset, through the layer's own NVN functions | the NRO |
| nvn-14 | nvn | two values a list reports are in memory once it has run, through the layer's own NVN functions | the NRO |
| nvn-15 | nvn | two timestamp reports hold no count and times between the device's clock before and after, through the layer's own NVN functions | the NRO |
| nvn-16 | nvn | a timestamp report's word is the graphics unit's ticks, 384 every 625 nanoseconds, through the layer's own NVN functions | the NRO |
| nvn-17 | nvn | the render enable draws under on and under a condition that holds and skips the rest, through the layer's own NVN functions | the NRO |
| nvn-18 | nvn | a command buffer's memory used and free come to what it was given, and draws take some, through the layer's own NVN functions | the NRO |
| nvn-19 | nvn | a list out of command memory asks the program's callback and draws on, through the layer's own NVN functions | the NRO |
| nvn-20 | nvn | a list called from another and a list copied into it draw their cells, through the layer's own NVN functions | the NRO |
| nvn-21 | nvn | clears of an unsigned and a signed integer target hold the integers given, through the layer's own NVN functions | the NRO |
| nvn-22 | nvn | a clear of a texture that is no target reaches its region and its mask's channels, through the layer's own NVN functions | the NRO |
| nvn-23 | nvn | a range of a buffer cleared to a word holds it, and nothing beside the range, through the layer's own NVN functions | the NRO |
| nvn-24 | nvn | a command buffer gives back the copy strides it was set, through the layer's own NVN functions | the NRO |
| nvn-25 | nvn | targets discarded, then cleared and drawn, hold the clear and the draw, through the layer's own NVN functions | the NRO |
| nvn-26 | nvn | a scissor and a viewport set by number bound and place a quad, through the layer's own NVN functions | the NRO |
| nvn-27 | nvn | depth ranges move a quad across the depth test and depth bounds test the target's depth, through the layer's own NVN functions | the NRO |
| nvn-28 | nvn | the rasterizer discards, clamps depth and takes a flat colour from the provoking vertex, through the layer's own NVN functions | the NRO |
| nvn-29 | nvn | a point of sixteen pixels fills its cell, through the layer's own NVN functions | the NRO |
| nvn-30 | nvn | a strip with a restart index is two quads, through the layer's own NVN functions | the NRO |
| nvn-31 | nvn | a viewport swizzle of x and y turns a quad about the diagonal, through the layer's own NVN functions | the NRO |
| nvn-32 | nvn | draws around the tiled cache's actions are as without them, through the layer's own NVN functions | the NRO |
| nvn-33 | nvn | conservative rasterization draws every pixel a quad touches, through the layer's own NVN functions | the NRO |
| nvn-34 | nvn | a conservative quad dilated by half a pixel reaches the pixels a third of one away, through the layer's own NVN functions | the NRO |
| nvn-35 | nvn | a line of width four is four rows of pixels, through the layer's own NVN functions | the NRO |
| nvn-36 | nvn | patches of the size and the outer levels a list sets fill their cells, through the layer's own NVN functions | the NRO |
| nvn-37 | nvn | draws between debug markers, groups and checkpoints are as without them, through the layer's own NVN functions | the NRO |
| nvn-38 | nvn | the device says its modes and its builder its flags, and clear values register, through the layer's own NVN functions | the NRO |
| nvn-39 | nvn | a program set from the longer shader data draws, through the layer's own NVN functions | the NRO |
| nvn-40 | nvn | a texture's handle and a sampler's put together sample, and a texel fetch handle fetches, through the layer's own NVN functions | the NRO |
| nvn-41 | nvn | the program's NVN functions resolve | the NRO, the Switch 2 program |
| nvn-42 | nvn | the device initializes | the NRO, the Switch 2 program |
| nvn-43 | nvn | the device answers the layer's API version | the NRO, the Switch 2 program |
| nvn-44 | nvn | a queue, its pools and a 64x64 target come up | the NRO, the Switch 2 program |
| nvn-45 | nvn | clear to opaque red | the NRO, the Switch 2 program |
| nvn-46 | nvn | clear to a mid channel value | the NRO, the Switch 2 program |
| nvn-47 | nvn | two copy textures and a staging pool come up | the NRO, the Switch 2 program |
| nvn-48 | nvn | a picture copied into a texture and back keeps every texel | the NRO, the Switch 2 program |
| nvn-49 | nvn | a texture copied into another keeps every texel | the NRO, the Switch 2 program |
| nvn-50 | nvn | a buffer copied into another keeps every byte | the NRO, the Switch 2 program |
| nvn-51 | nvn | a texture copied to a buffer with MIRROR_Y lands upside down | the NRO, the Switch 2 program |
| nvn-52 | nvn | a texture copied into another with MIRROR_X lands mirrored | the NRO, the Switch 2 program |
| nvn-53 | nvn | a copy to a buffer with a row stride of 288 keeps the bytes between rows | the NRO, the Switch 2 program |
| nvn-54 | nvn | a texture builder gives back each number it was set | the NRO, the Switch 2 program |
| nvn-55 | nvn | a texture view says which of its parts were set, and with what | the NRO, the Switch 2 program |
| nvn-56 | nvn | a texture of three levels comes up and says what its builder had | the NRO, the Switch 2 program |
| nvn-57 | nvn | a picture copied into each level of a texture through a view comes back from that level | the NRO, the Switch 2 program |
| nvn-58 | nvn | a memory pool and its builder give back what they were set, and the pool maps its storage | the NRO, the Switch 2 program |
| nvn-59 | nvn | a page flushed from a cached pool is what a copy reads, and the copy is there after an invalidate | the NRO, the Switch 2 program |
| nvn-60 | nvn | a sync fenced on the queue behind a clear signals, and the clear is there with no finish | the NRO, the Switch 2 program |
| nvn-61 | nvn | a sync fenced inside the list behind a clear signals, and the clear is there with no finish | the NRO, the Switch 2 program |
| nvn-62 | nvn | a queue and a list told to wait for a sync that signalled run their clear | the NRO, the Switch 2 program |
| nvn-63 | nvn | a wait on a sync nothing fenced fails, with a timeout or without | the NRO, the Switch 2 program |
| nvn-64 | nvn | with no OpenGL, a sync from an OpenGL sync and an OpenGL sync from a sync both fail | the NRO, the Switch 2 program |
| nvn-65 | nvn | a pool builder given a native handle forgets its storage, and takes storage again | the NRO, the Switch 2 program |
| nvn-66 | nvn | the queue has no error after work that was valid | the NRO, the Switch 2 program |
| nvn-67 | nvn | a queue builder gives back its flags and says which of its sizes were set | the NRO, the Switch 2 program |
| nvn-68 | nvn | an event keeps what the processor signals: a write, an add and an add that wraps | the NRO, the Switch 2 program |
| nvn-69 | nvn | a sampler builder gives back each number it was set | the NRO, the Switch 2 program |
| nvn-70 | nvn | a sampler says what its builder had, and is the same as another of that state alone | the NRO, the Switch 2 program |
| nvn-71 | nvn | the texture and sampler pools say where they are, and a builder registers as its sampler | the NRO, the Switch 2 program |
| nvn-72 | nvn | a buffer is a range of its pool: its address, its mapping, and what it and its builder say | the NRO, the Switch 2 program |
| nvn-73 | nvn | bytes flushed from one buffer and copied to another are there after an invalidate | the NRO, the Switch 2 program |
| nvn-74 | nvn | a blend state gives back its target, functions, equations and advanced mode | the NRO, the Switch 2 program |
| nvn-75 | nvn | a channel mask and a colour state give back a target's channels, blending, the logic operation and the alpha test | the NRO, the Switch 2 program |
| nvn-76 | nvn | a depth and stencil state gives back its depth test and each face's stencil | the NRO, the Switch 2 program |
| nvn-77 | nvn | a multisample state gives back its samples, its switches and its sample locations' grid | the NRO, the Switch 2 program |
| nvn-78 | nvn | a polygon state, a vertex attribute and a vertex stream give back what they were set | the NRO, the Switch 2 program |
| nvn-79 | nvn | a texture builder and a view give back their swizzle, depth and stencil mode and the rest | the NRO, the Switch 2 program |
| nvn-80 | nvn | a texture says its swizzle, storage and address, and is the same as its twin alone | the NRO, the Switch 2 program |
| nvn-81 | nvn | texels the processor writes into a texture are what it and the graphics unit read back | the NRO, the Switch 2 program |
| nvn-82 | nvn | a part of a texture written and read with row strides keeps the bytes between rows | the NRO, the Switch 2 program |
| nvn-83 | nvn | a 1D texture, each layer of an array and each slice of a 3D texture keep a picture of their own | the NRO, the Switch 2 program |
| nvn-84 | nvn | a queue builder gives back its memory and priority, and a queue's memory counts are nought once reset | the NRO, the Switch 2 program |
| nvn-85 | nvn | a list writes an event's value over the one the processor signalled | the NRO, the Switch 2 program |
| nvn-86 | nvn | a list adds to an event's value | the NRO, the Switch 2 program |
| nvn-87 | nvn | a list that waits for an event's value runs when the processor signals it, and not before | the NRO, the Switch 2 program |
| nvn-88 | nvn | a virtual pool's page is the physical pool's page mapped into it | the NRO, the Switch 2 program |
| nvn-89 | nvn | the debug layer's calls are answered as a release driver answers them, and a label changes no object | the NRO, the Switch 2 program |
| nvn-90 | nvn | a window builder gives back what it was set | the NRO, the Switch 2 program |
| nvn-91 | nvn | a window acquires and presents both its textures | the NRO, the Switch 2 program |
| nvn-92 | nvn | a window says what it is of, and keeps the active textures, the interval and the crop it is set | the NRO, the Switch 2 program |
| nvn-93 | nvn | a texture the window hands with a sync is free once the sync signals, and presents | the NRO, the Switch 2 program |
| nvn-94 | nvn | the same scene through the NVN the program looks up holds what the processor works out | the NRO |
| nvn-95 | nvn | the same texture past its edges through the NVN the program looks up does the same | the NRO |
| nvn-96 | nvn | the same four indexed quads through the NVN the program looks up are each in its quarter | the NRO |
| nvn-97 | nvn | the scene of five draws bound through the looked up calls that take several holds what the processor works out | the NRO |
| nvn-98 | nvn | the scene of five draws bound through the looked up deferred calls holds what the processor works out | the NRO |
| nvn-99 | nvn | the same four quads of an updated uniform buffer through the NVN the program looks up are each its own colour | the NRO |
| nvn-100 | nvn | the same eight instances through the NVN the program looks up are each in its cell | the NRO |
| nvn-101 | nvn | a texture drawn with no program and as many draws as a word says, through the NVN the program looks up, are each in its place | the NRO |
| nvn-102 | nvn | what a capture took of two draws, paused for a third between them, is drawn again a row down through the NVN the program looks up | the NRO |
| nvn-103 | nvn | the same capture into a buffer bound by the call that takes several is drawn again a row down | the NRO |
| nvn-104 | nvn | the samples counter reports a quad's 256 pixels and none after a reset, through the NVN the program looks up | the NRO |
| nvn-105 | nvn | two values a list reports are in memory once it has run, through the NVN the program looks up | the NRO |
| nvn-106 | nvn | two timestamp reports hold no count and times between the device's clock before and after, through the NVN the program looks up | the NRO |
| nvn-107 | nvn | a timestamp report's word is the graphics unit's ticks, 384 every 625 nanoseconds, through the NVN the program looks up | the NRO |
| nvn-108 | nvn | the render enable draws under on and under a condition that holds and skips the rest, through the NVN the program looks up | the NRO |
| nvn-109 | nvn | a command buffer's memory used and free come to what it was given, and draws take some, through the NVN the program looks up | the NRO |
| nvn-110 | nvn | a list out of command memory asks the program's callback and draws on, through the NVN the program looks up | the NRO |
| nvn-111 | nvn | a list called from another and a list copied into it draw their cells, through the NVN the program looks up | the NRO |
| nvn-112 | nvn | clears of an unsigned and a signed integer target hold the integers given, through the NVN the program looks up | the NRO |
| nvn-113 | nvn | a clear of a texture that is no target reaches its region and its mask's channels, through the NVN the program looks up | the NRO |
| nvn-114 | nvn | a range of a buffer cleared to a word holds it, and nothing beside the range, through the NVN the program looks up | the NRO |
| nvn-115 | nvn | a command buffer gives back the copy strides it was set, through the NVN the program looks up | the NRO |
| nvn-116 | nvn | targets discarded, then cleared and drawn, hold the clear and the draw, through the NVN the program looks up | the NRO |
| nvn-117 | nvn | a scissor and a viewport set by number bound and place a quad, through the NVN the program looks up | the NRO |
| nvn-118 | nvn | depth ranges move a quad across the depth test and depth bounds test the target's depth, through the NVN the program looks up | the NRO |
| nvn-119 | nvn | the rasterizer discards, clamps depth and takes a flat colour from the provoking vertex, through the NVN the program looks up | the NRO |
| nvn-120 | nvn | a point of sixteen pixels fills its cell, through the NVN the program looks up | the NRO |
| nvn-121 | nvn | a strip with a restart index is two quads, through the NVN the program looks up | the NRO |
| nvn-122 | nvn | a viewport swizzle of x and y turns a quad about the diagonal, through the NVN the program looks up | the NRO |
| nvn-123 | nvn | draws around the tiled cache's actions are as without them, through the NVN the program looks up | the NRO |
| nvn-124 | nvn | conservative rasterization draws every pixel a quad touches, through the NVN the program looks up | the NRO |
| nvn-125 | nvn | a conservative quad dilated by half a pixel reaches the pixels a third of one away, through the NVN the program looks up | the NRO |
| nvn-126 | nvn | a line of width four is four rows of pixels, through the NVN the program looks up | the NRO |
| nvn-127 | nvn | patches of the size and the outer levels a list sets fill their cells, through the NVN the program looks up | the NRO |
| nvn-128 | nvn | draws between debug markers, groups and checkpoints are as without them, through the NVN the program looks up | the NRO |
| nvn-129 | nvn | the device says its modes and its builder its flags, and clear values register, through the NVN the program looks up | the NRO |
| nvn-130 | nvn | a program set from the longer shader data draws, through the NVN the program looks up | the NRO |
| nvn-131 | nvn | a texture's handle and a sampler's put together sample, and a texel fetch handle fetches, through the NVN the program looks up | the NRO |
| nvn-132 | nvn | a clear between the Ounce's graphics and compute markers and groups is the clear | the Switch 2 program |
| nvn-133 | nvn | an Ounce program builder gives back what it was set, a ray tracing program's numbers too | the Switch 2 program |
| nvn-134 | nvn | an Ounce window builder starts with the driver's colour space and HDR metadata, and gives back what it is set | the Switch 2 program |
| nvn-135 | nvn | an Ounce window has its builder's HDR metadata, and gives back what it is set | the Switch 2 program |
| nvn2-1 | nvn2 | a device, a queue, a command buffer, their pools and a 256x256 target with its depth come up | the Switch 2 program |
| nvn2-2 | nvn2 | a compute program assembled by hand makes a program through the builder | the Switch 2 program |
| nvn2-3 | nvn2 | a dispatch's writes to a storage buffer, as the processor reads them once the queue finishes | the Switch 2 program |
| nvn2-4 | nvn2 | a dispatch's writes to a storage buffer, as the graphics unit copies them to another buffer | the Switch 2 program |
| nvn2-5 | nvn2 | the same dispatch, its storage buffer bound by the call that takes several | the Switch 2 program |
| nvn2-6 | nvn2 | the same dispatch, its storage buffer bound by the deferred call | the Switch 2 program |
| nvn2-7 | nvn2 | the same dispatch, its counts of blocks read by the graphics unit from a buffer | the Switch 2 program |
| nvn2-8 | nvn2 | the Mandelbrot's compute, mesh and fragment programs make two programs | the Switch 2 program |
| nvn2-9 | nvn2 | a compute program counts the Mandelbrot set's steps for every cell of a grid | the Switch 2 program |
| nvn2-10 | nvn2 | a mesh program draws the counts as bars, a fragment program colours them and the depth test keeps the nearest | the Switch 2 program |
| nvn2-11 | nvn2 | the same picture by a mesh draw of a grid of workgroups | the Switch 2 program |
| nvn2-12 | nvn2 | the same picture by a mesh draw that reads its numbers | the Switch 2 program |
| nvn2-13 | nvn2 | the same picture by a mesh draw of a grid that reads its numbers | the Switch 2 program |
| nvn2-14 | nvn2 | the same picture by two mesh draws read from one buffer, a half of the rows each | the Switch 2 program |
| nvn2-15 | nvn2 | the same picture by the mesh draws of a grid read from one buffer | the Switch 2 program |
| nvn2-16 | nvn2 | the same picture by as many mesh draws as a count in a buffer says, a half each | the Switch 2 program |
| nvn2-17 | nvn2 | the same picture by as many mesh draws of a grid as a count in a buffer says | the Switch 2 program |
| nvn2-18 | nvn2 | one warp's matrix instruction multiplies a 16 by 16 matrix of halves by a 16 by 8 one into floats | the Switch 2 program |
| nvn2-19 | nvn2 | a dispatch multiplies and adds by the two words pushed into a root table, the second pushed again alone | the Switch 2 program |
| nvn2-20 | nvn2 | the same bars with the green and blue a fragment program reads from a root table pushed | the Switch 2 program |
| nvn2-21 | nvn2 | the same picture with no texture bound as the shading rate image | the Switch 2 program |
| nvn2-22 | nvn2 | a dispatch loads each texel of one image and stores three times it plus its column into another | the Switch 2 program |
| nvn2-23 | nvn2 | the same images bound by the call that takes several | the Switch 2 program |
| nvn2-24 | nvn2 | the same images bound by the deferred call | the Switch 2 program |
| nvn2-25 | nvn2 | a dispatch samples a texture bound apart from its sampler, slot 1 of each, past the texture's edge | the Switch 2 program |
| nvn2-26 | nvn2 | the same texture and sampler bound by the calls that take several | the Switch 2 program |
| nvn2-27 | nvn2 | the same texture and sampler bound by the deferred calls | the Switch 2 program |
| nvn2-28 | nvn2 | a DLSS network builder gives back what it was set | the Switch 2 program |
| nvn2-29 | nvn2 | the parameters of a DLSS evaluation give back what they were set | the Switch 2 program |
| nvn2-30 | nvn2 | a DLSS builder's render sizes are in order within its output, and a network is resized within it alone | the Switch 2 program |
| nvn2-31 | nvn2 | a DLSS evaluation of a smooth picture into four times its side is within a tenth of a plain filter | the Switch 2 program |

## Device

36 functions, 13 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnBootstrapLoader` | 55, 231 | nvn-41, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn2-1 |
| `nvnDeviceGetProcAddress` | 55, 231 | nvn-41, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn2-1 |
| `nvnDeviceApplyDeferredFinalizes` |  |  |
| `nvnDeviceBuilderGetFlags` | 55, 231 | nvn-129 |
| `nvnDeviceFinalize` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnDeviceRegisterFastClearColori` | 55, 231 | nvn-129 |
| `nvnDeviceRegisterFastClearColorui` | 55, 231 | nvn-129 |
| `nvnDeviceSetIntermediateShaderCache` |  |  |
| `nvnDeviceBuilderSetDefaults` | 55, 231 | nvn-42, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-129, nvn2-1 |
| `nvnDeviceBuilderSetFlags` | 55, 231 | nvn-42, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-129, nvn2-1 |
| `nvnDeviceGetImageHandle` | 231 | nvn2-22, nvn2-23, nvn2-24 |
| `nvnDeviceFinalizeCommandHandle` |  |  |
| `nvnDeviceGetCurrentTimestampInNanoseconds` | 55, 231 | nvn-15, nvn-16, nvn-106, nvn-107 |
| `nvnDeviceGetDepthMode` | 55, 231 | nvn-129 |
| `nvnDeviceGetInteger` | 55, 231 | nvn-43, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn2-1, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27 |
| `nvnDeviceGetWindowOriginMode` | 55, 231 | nvn-129 |
| `nvnDeviceGetSeparateSamplerHandle` | 55, 231 | nvn-131, nvn2-25, nvn2-26, nvn2-27 |
| `nvnDeviceGetSeparateTextureHandle` | 55, 231 | nvn-131, nvn2-25, nvn2-26, nvn2-27 |
| `nvnDeviceGetTexelFetchHandle` | 55, 231 | nvn-131 |
| `nvnDeviceGetTextureHandle` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnDeviceInitialize` | 55, 231 | nvn-42, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn2-1 |
| `nvnDeviceRegisterFastClearColor` | 55, 231 | nvn-129 |
| `nvnDeviceRegisterFastClearDepth` | 55, 231 | nvn-129 |
| `nvnDeviceRegisterFastClearStencil` | 55, 231 | nvn-129 |
| `nvnDeviceSetDepthMode` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103 |
| `nvnDeviceSetWindowOriginMode` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103 |
| `nvnDeviceGetUniformBufferHandle` |  |  |
| `nvnDeviceCommandBufferSetDefaults` |  |  |
| `nvnDeviceCommandBufferSetDevice` |  |  |
| `nvnDeviceCommandBufferSetStorage` |  |  |
| `nvnDeviceCommandBufferSetCommandMemorySize` |  |  |
| `nvnDeviceCommandBufferSetComputeMemorySize` |  |  |
| `nvnDeviceMessageBufferSetDefaults` |  |  |
| `nvnDeviceMessageBufferSetDevice` |  |  |
| `nvnDeviceMessageBufferSetStorage` |  |  |
| `nvnDeviceMessageBufferEndMessages` |  |  |

## Memory pools

18 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnMemoryPoolBuilderGetDevice` | 55, 231 | nvn-58 |
| `nvnMemoryPoolBuilderGetFlags` | 55, 231 | nvn-58 |
| `nvnMemoryPoolBuilderGetMemory` | 55, 231 | nvn-58, nvn-65 |
| `nvnMemoryPoolBuilderGetSize` | 55, 231 | nvn-58, nvn-65 |
| `nvnMemoryPoolBuilderSetDefaults` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-56, nvn-58, nvn-65, nvn-83, nvn-88, nvn-91, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-112, nvn-113, nvn2-1, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27 |
| `nvnMemoryPoolBuilderSetDevice` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-56, nvn-58, nvn-65, nvn-83, nvn-88, nvn-91, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-112, nvn-113, nvn2-1, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27 |
| `nvnMemoryPoolBuilderSetFlags` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-56, nvn-58, nvn-83, nvn-88, nvn-91, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-112, nvn-113, nvn2-1, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27 |
| `nvnMemoryPoolBuilderSetStorage` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-56, nvn-58, nvn-65, nvn-83, nvn-88, nvn-91, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-112, nvn-113, nvn2-1, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27 |
| `nvnMemoryPoolBuilderSetNativeHandleNVX` | 55, 231 | nvn-65 |
| `nvnMemoryPoolFinalize` | 55, 231 | nvn-21, nvn-22, nvn-83, nvn-88, nvn-91, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-112, nvn-113, nvn-127, nvn-131 |
| `nvnMemoryPoolFlushMappedRange` | 55, 231 | nvn-59, nvn-73 |
| `nvnMemoryPoolGetBufferAddress` | 55, 231 | nvn-45, nvn-46, nvn-48, nvn-49, nvn-50, nvn-51, nvn-52, nvn-53, nvn-57, nvn-59, nvn-60, nvn-61, nvn-62, nvn-72, nvn-80, nvn-81, nvn-83, nvn-87, nvn-88, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn-132, nvn2-2, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-8, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-31 |
| `nvnMemoryPoolGetFlags` | 55, 231 | nvn-58 |
| `nvnMemoryPoolGetSize` | 55, 231 | nvn-58, nvn-89 |
| `nvnMemoryPoolInitialize` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-56, nvn-58, nvn-83, nvn-88, nvn-91, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-112, nvn-113, nvn2-1, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27 |
| `nvnMemoryPoolInvalidateMappedRange` | 55, 231 | nvn-59 |
| `nvnMemoryPoolMap` | 55, 231 | nvn-45, nvn-46, nvn-47, nvn-48, nvn-49, nvn-50, nvn-51, nvn-52, nvn-53, nvn-57, nvn-58, nvn-59, nvn-60, nvn-61, nvn-62, nvn-68, nvn-71, nvn-72, nvn-73, nvn-81, nvn-83, nvn-87, nvn-88, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn-132, nvn2-2, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-8, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-31 |
| `nvnMemoryPoolMapVirtual` | 55, 231 | nvn-88 |

## Buffers

16 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnBufferBuilderGetDevice` | 55, 231 | nvn-72 |
| `nvnBufferBuilderGetMemoryOffset` | 55, 231 | nvn-72 |
| `nvnBufferBuilderGetMemoryPool` | 55, 231 | nvn-72 |
| `nvnBufferBuilderGetSize` | 55, 231 | nvn-72 |
| `nvnBufferBuilderSetDefaults` | 55, 231 | nvn-72, nvn-73, nvn-89 |
| `nvnBufferBuilderSetDevice` | 55, 231 | nvn-72, nvn-73, nvn-89 |
| `nvnBufferBuilderSetStorage` | 55, 231 | nvn-72, nvn-73, nvn-89 |
| `nvnBufferFinalize` | 55, 231 | nvn-72, nvn-73, nvn-89 |
| `nvnBufferFlushMappedRange` | 55, 231 | nvn-73 |
| `nvnBufferInvalidateMappedRange` | 55, 231 | nvn-73 |
| `nvnBufferGetAddress` | 55, 231 | nvn-72, nvn-73 |
| `nvnBufferGetMemoryOffset` | 55, 231 | nvn-72 |
| `nvnBufferGetMemoryPool` | 55, 231 | nvn-72 |
| `nvnBufferGetSize` | 55, 231 | nvn-72, nvn-89 |
| `nvnBufferInitialize` | 55, 231 | nvn-72, nvn-73, nvn-89 |
| `nvnBufferMap` | 55, 231 | nvn-72, nvn-73 |

## Textures

95 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnTextureBuilderGetGLTextureName` | 55, 231 | nvn-79 |
| `nvnTextureBuilderGetMemoryOffset` | 55, 231 | nvn-54 |
| `nvnTextureBuilderGetPackagedTextureLayout` | 55, 231 | nvn-79 |
| `nvnTextureBuilderGetRawStorageClass` | 55, 231 | nvn-80 |
| `nvnTextureBuilderGetSparseTileLayout` | 55, 231 | nvn-80 |
| `nvnTextureBuilderGetViewOffset` | 55, 231 | nvn-80 |
| `nvnTextureBuilderSetGLTextureName` | 55, 231 | nvn-79 |
| `nvnTextureGetNvRmSurfaceNVX` | 55, 231 | nvn-80 |
| `nvnTextureGetRawStorageClass` | 55, 231 | nvn-80 |
| `nvnTextureGetSparseTileLayout` | 55, 231 | nvn-80 |
| `nvnTextureInvalidateTexels` | 55, 231 | nvn-81 |
| `nvnTextureReadTexelsStrided` | 55, 231 | nvn-82 |
| `nvnTextureViewCompare` | 55, 231 | nvn-55 |
| `nvnTextureBuilderGetDepthStencilMode` | 55, 231 | nvn-79 |
| `nvnTextureBuilderGetDevice` | 55, 231 | nvn-54 |
| `nvnTextureBuilderGetMemoryPool` | 55, 231 | nvn-54 |
| `nvnTextureBuilderGetPackagedTextureData` | 55, 231 | nvn-79 |
| `nvnTextureBuilderGetSwizzle` | 55, 231 | nvn-79 |
| `nvnTextureBuilderGetDepth` | 55, 231 | nvn-54 |
| `nvnTextureBuilderGetFlags` | 55, 231 | nvn-54 |
| `nvnTextureBuilderGetFormat` | 55, 231 | nvn-54 |
| `nvnTextureBuilderGetHeight` | 55, 231 | nvn-54 |
| `nvnTextureBuilderGetLevels` | 55, 231 | nvn-54 |
| `nvnTextureBuilderGetSamples` | 55, 231 | nvn-54 |
| `nvnTextureBuilderGetStorageAlignment` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-56, nvn-83, nvn-91, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-112, nvn-113, nvn-127, nvn-131, nvn2-1, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-31 |
| `nvnTextureBuilderGetStorageClass` | 55, 231 | nvn-80 |
| `nvnTextureBuilderGetStorageSize` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-56, nvn-80, nvn-83, nvn-91, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-112, nvn-113, nvn-127, nvn-131, nvn2-1, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-31 |
| `nvnTextureBuilderGetStride` | 55, 231 | nvn-79 |
| `nvnTextureBuilderGetTarget` | 55, 231 | nvn-54 |
| `nvnTextureBuilderGetWidth` | 55, 231 | nvn-54 |
| `nvnTextureBuilderGetZCullStorageSize` | 55, 231 | nvn-80 |
| `nvnTextureBuilderSetDefaults` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-54, nvn-56, nvn-79, nvn-80, nvn-83, nvn-91, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-112, nvn-113, nvn-127, nvn-131, nvn2-1, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-31 |
| `nvnTextureBuilderSetDepth` | 55, 231 | nvn-54, nvn-56, nvn-83 |
| `nvnTextureBuilderSetDepthStencilMode` | 55, 231 | nvn-79 |
| `nvnTextureBuilderSetDevice` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-54, nvn-56, nvn-79, nvn-80, nvn-83, nvn-91, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-112, nvn-113, nvn-127, nvn-131, nvn2-1, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-31 |
| `nvnTextureBuilderSetFlags` | 55, 231 | nvn-44, nvn-47, nvn-54, nvn-56, nvn-80, nvn-83, nvn-91, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-1, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-31 |
| `nvnTextureBuilderSetFormat` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-54, nvn-56, nvn-80, nvn-83, nvn-91, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-112, nvn-113, nvn-127, nvn-131, nvn2-1, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-31 |
| `nvnTextureBuilderSetHeight` | 55, 231 | nvn-54 |
| `nvnTextureBuilderSetLevels` | 55, 231 | nvn-54, nvn-56 |
| `nvnTextureBuilderSetPackagedTextureData` | 55, 231 | nvn-79 |
| `nvnTextureBuilderSetPackagedTextureLayout` | 55, 231 | nvn-79 |
| `nvnTextureBuilderSetSamples` | 55, 231 | nvn-54, nvn-56 |
| `nvnTextureBuilderSetSize1D` | 55, 231 | nvn-83 |
| `nvnTextureBuilderSetSize2D` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-54, nvn-56, nvn-80, nvn-83, nvn-91, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-112, nvn-113, nvn-127, nvn-131, nvn2-1, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-31 |
| `nvnTextureBuilderSetSize3D` | 55, 231 | nvn-83 |
| `nvnTextureBuilderSetStorage` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-54, nvn-56, nvn-80, nvn-83, nvn-91, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-112, nvn-113, nvn-127, nvn-131, nvn2-1, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-31 |
| `nvnTextureBuilderSetStride` | 55, 231 | nvn-79 |
| `nvnTextureBuilderSetSwizzle` | 55, 231 | nvn-79, nvn-80 |
| `nvnTextureBuilderSetTarget` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-54, nvn-56, nvn-80, nvn-83, nvn-91, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-112, nvn-113, nvn-127, nvn-131, nvn2-1, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-31 |
| `nvnTextureBuilderSetWidth` | 55, 231 | nvn-54 |
| `nvnTextureCompare` | 55, 231 | nvn-80 |
| `nvnTextureFinalize` | 55, 231 | nvn-21, nvn-22, nvn-80, nvn-83, nvn-91, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-112, nvn-113, nvn-127, nvn-131 |
| `nvnTextureFlushTexels` | 55, 231 | nvn-81 |
| `nvnTextureGetDepth` | 55, 231 | nvn-56 |
| `nvnTextureGetDepthStencilMode` | 55, 231 | nvn-80 |
| `nvnTextureGetFlags` | 55, 231 | nvn-56 |
| `nvnTextureGetFormat` | 55, 231 | nvn-56 |
| `nvnTextureGetHeight` | 55, 231 | nvn-56 |
| `nvnTextureGetLevels` | 55, 231 | nvn-56 |
| `nvnTextureGetMemoryOffset` | 55, 231 | nvn-56 |
| `nvnTextureGetMemoryPool` | 55, 231 | nvn-56 |
| `nvnTextureGetSamples` | 55, 231 | nvn-56 |
| `nvnTextureGetStorageClass` | 55, 231 | nvn-80 |
| `nvnTextureGetStorageSize` | 55, 231 | nvn-80 |
| `nvnTextureGetStride` | 55, 231 | nvn-80 |
| `nvnTextureGetSwizzle` | 55, 231 | nvn-80 |
| `nvnTextureGetTarget` | 55, 231 | nvn-56 |
| `nvnTextureGetTextureAddress` | 55, 231 | nvn-80 |
| `nvnTextureGetViewOffset` | 55, 231 | nvn-80 |
| `nvnTextureGetWidth` | 55, 231 | nvn-56, nvn-89 |
| `nvnTextureGetZCullStorageSize` | 55, 231 | nvn-80 |
| `nvnTextureInitialize` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-56, nvn-80, nvn-83, nvn-91, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-112, nvn-113, nvn-127, nvn-131, nvn2-1, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-31 |
| `nvnTexturePoolFinalize` | 55, 231 | nvn-71, nvn-89, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnTexturePoolGetMemoryOffset` | 55, 231 | nvn-71 |
| `nvnTexturePoolGetMemoryPool` | 55, 231 | nvn-71 |
| `nvnTexturePoolGetSize` | 55, 231 | nvn-71, nvn-89 |
| `nvnTexturePoolInitialize` | 55, 231 | nvn-71, nvn-89, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27 |
| `nvnTexturePoolRegisterImage` | 231 | nvn2-22, nvn2-23, nvn2-24 |
| `nvnTexturePoolRegisterTexture` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-25, nvn2-26, nvn2-27 |
| `nvnTextureReadTexels` | 55, 231 | nvn-81 |
| `nvnTextureViewGetDepthStencilMode` | 55, 231 | nvn-79 |
| `nvnTextureViewGetFormat` | 55, 231 | nvn-55 |
| `nvnTextureViewGetLayers` | 55, 231 | nvn-55 |
| `nvnTextureViewGetLevels` | 55, 231 | nvn-55 |
| `nvnTextureViewGetSwizzle` | 55, 231 | nvn-79 |
| `nvnTextureViewGetTarget` | 55, 231 | nvn-55 |
| `nvnTextureViewSetDefaults` | 55, 231 | nvn-55, nvn-57, nvn-79 |
| `nvnTextureViewSetDepthStencilMode` | 55, 231 | nvn-79 |
| `nvnTextureViewSetFormat` | 55, 231 | nvn-55 |
| `nvnTextureViewSetLayers` | 55, 231 | nvn-55 |
| `nvnTextureViewSetLevels` | 55, 231 | nvn-55, nvn-57 |
| `nvnTextureViewSetSwizzle` | 55, 231 | nvn-79 |
| `nvnTextureViewSetTarget` | 55, 231 | nvn-55 |
| `nvnTextureWriteTexels` | 55, 231 | nvn-81 |
| `nvnTextureWriteTexelsStrided` | 55, 231 | nvn-82 |

## Samplers

45 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnSamplerBuilderGetBorderColor` | 55, 231 | nvn-69, nvn-70 |
| `nvnSamplerBuilderGetBorderColori` | 55, 231 | nvn-69 |
| `nvnSamplerBuilderGetBorderColorui` | 55, 231 | nvn-69 |
| `nvnSamplerBuilderGetCompare` | 55, 231 | nvn-69, nvn-70 |
| `nvnSamplerBuilderGetDevice` | 55, 231 | nvn-69 |
| `nvnSamplerBuilderGetLodBias` | 55, 231 | nvn-69, nvn-70 |
| `nvnSamplerBuilderGetLodClamp` | 55, 231 | nvn-69, nvn-70 |
| `nvnSamplerBuilderGetMaxAnisotropy` | 55, 231 | nvn-69, nvn-70 |
| `nvnSamplerBuilderGetMinMagFilter` | 55, 231 | nvn-69, nvn-70 |
| `nvnSamplerBuilderGetReductionFilter` | 55, 231 | nvn-69, nvn-70 |
| `nvnSamplerBuilderGetWrapMode` | 55, 231 | nvn-69, nvn-70 |
| `nvnSamplerBuilderSetBorderColori` | 55, 231 | nvn-69 |
| `nvnSamplerBuilderSetBorderColorui` | 55, 231 | nvn-69 |
| `nvnSamplerGetBorderColori` | 55, 231 | nvn-70 |
| `nvnSamplerGetBorderColorui` | 55, 231 | nvn-70 |
| `nvnSamplerPoolRegisterSamplerBuilder` | 55, 231 | nvn-71 |
| `nvnSamplerBuilderSetBorderColor` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn-89, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnSamplerBuilderSetCompare` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn-89 |
| `nvnSamplerBuilderSetDefaults` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn-89, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-25, nvn2-26, nvn2-27 |
| `nvnSamplerBuilderSetDevice` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn-89, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-25, nvn2-26, nvn2-27 |
| `nvnSamplerBuilderSetLodBias` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn-89 |
| `nvnSamplerBuilderSetLodClamp` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn-89 |
| `nvnSamplerBuilderGetLodSnap` | 55, 231 | nvn-69 |
| `nvnSamplerBuilderSetLodSnap` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn-89 |
| `nvnSamplerBuilderSetMaxAnisotropy` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn-89 |
| `nvnSamplerBuilderSetMinMagFilter` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn-89, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-25, nvn2-26, nvn2-27 |
| `nvnSamplerBuilderSetReductionFilter` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn-89 |
| `nvnSamplerBuilderSetWrapMode` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn-89, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-25, nvn2-26, nvn2-27 |
| `nvnSamplerCompare` | 55, 231 | nvn-70 |
| `nvnSamplerFinalize` | 55, 231 | nvn-70, nvn-71, nvn-89, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnSamplerGetBorderColor` | 55, 231 | nvn-70 |
| `nvnSamplerGetCompare` | 55, 231 | nvn-70 |
| `nvnSamplerGetLodBias` | 55, 231 | nvn-70 |
| `nvnSamplerGetLodClamp` | 55, 231 | nvn-70 |
| `nvnSamplerGetMaxAnisotropy` | 55, 231 | nvn-70 |
| `nvnSamplerGetMinMagFilter` | 55, 231 | nvn-70 |
| `nvnSamplerGetReductionFilter` | 55, 231 | nvn-70 |
| `nvnSamplerGetWrapMode` | 55, 231 | nvn-70 |
| `nvnSamplerInitialize` | 55, 231 | nvn-70, nvn-71, nvn-89, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-25, nvn2-26, nvn2-27 |
| `nvnSamplerPoolFinalize` | 55, 231 | nvn-71, nvn-89, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnSamplerPoolGetMemoryOffset` | 55, 231 | nvn-71 |
| `nvnSamplerPoolGetMemoryPool` | 55, 231 | nvn-71 |
| `nvnSamplerPoolGetSize` | 55, 231 | nvn-71, nvn-89 |
| `nvnSamplerPoolInitialize` | 55, 231 | nvn-71, nvn-89, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27 |
| `nvnSamplerPoolRegisterSampler` | 55, 231 | nvn-71, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-25, nvn2-26, nvn2-27 |

## Programs

20 functions, 7 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnProgramGetSubtileSize` |  |  |
| `nvnProgramGetScratchBytesPerWarp` |  |  |
| `nvnProgramSetSampleShading` |  |  |
| `nvnProgramSetSubroutineLinkage` |  |  |
| `nvnProgramFinalize` | 55, 231 | nvn-36, nvn-39, nvn-40, nvn-89, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-130, nvn-131 |
| `nvnProgramInitialize` | 55, 231 | nvn-89, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-130, nvn-131 |
| `nvnProgramSetShaders` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnProgramSetShadersExt` | 55, 231 | nvn-130 |
| `nvnProgramBuilderSetDefaults` | 231 | nvn-133, nvn2-2, nvn2-8, nvn2-18, nvn2-19, nvn2-20, nvn2-22, nvn2-25 |
| `nvnProgramBuilderSetDevice` | 231 | nvn-133, nvn2-2, nvn2-8, nvn2-18, nvn2-19, nvn2-20, nvn2-22, nvn2-25 |
| `nvnProgramBuilderGetDevice` | 231 | nvn-133 |
| `nvnProgramBuilderSetSampleShading` | 231 | nvn-133 |
| `nvnProgramBuilderGetSampleShading` | 231 | nvn-133 |
| `nvnProgramBuilderSetShaders` | 231 | nvn-133, nvn2-2, nvn2-8, nvn2-18, nvn2-19, nvn2-20, nvn2-22, nvn2-25 |
| `nvnProgramBuilderGetShaders` | 231 | nvn-133 |
| `nvnProgramBuilderGetShaderCount` | 231 | nvn-133 |
| `nvnProgramBuilderSetSubroutineLinkage` |  |  |
| `nvnProgramBuilderGetSubroutineLinkage` |  |  |
| `nvnProgramBuilderGetSubroutineLinkageCount` |  |  |
| `nvnProgramInitializeFromBuilder` | 231 | nvn2-2, nvn2-8, nvn2-18, nvn2-19, nvn2-20, nvn2-22, nvn2-25 |

## State objects

82 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnBlendStateGetAdvancedMode` | 55, 231 | nvn-74 |
| `nvnBlendStateGetAdvancedNormalizedDst` | 55, 231 | nvn-74 |
| `nvnBlendStateGetAdvancedOverlap` | 55, 231 | nvn-74 |
| `nvnBlendStateGetAdvancedPremultipliedSrc` | 55, 231 | nvn-74 |
| `nvnBlendStateSetAdvancedMode` | 55, 231 | nvn-74 |
| `nvnBlendStateSetAdvancedNormalizedDst` | 55, 231 | nvn-74 |
| `nvnBlendStateSetAdvancedOverlap` | 55, 231 | nvn-74 |
| `nvnBlendStateSetAdvancedPremultipliedSrc` | 55, 231 | nvn-74 |
| `nvnMultisampleStateGetSampleLocationsGrid` | 55, 231 | nvn-77 |
| `nvnMultisampleStateSetSampleLocations` | 55, 231 | nvn-77 |
| `nvnBlendStateSetBlendEquation` | 55, 231 | nvn-74 |
| `nvnBlendStateSetBlendFunc` | 55, 231 | nvn-74 |
| `nvnBlendStateSetBlendTarget` | 55, 231 | nvn-74 |
| `nvnBlendStateSetDefaults` | 55, 231 | nvn-74, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21 |
| `nvnChannelMaskStateSetChannelMask` | 55, 231 | nvn-75 |
| `nvnChannelMaskStateSetDefaults` | 55, 231 | nvn-75, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21, nvn2-31 |
| `nvnColorStateGetBlendEnable` | 55, 231 | nvn-75 |
| `nvnColorStateSetAlphaTest` | 55, 231 | nvn-75 |
| `nvnColorStateSetBlendEnable` | 55, 231 | nvn-75 |
| `nvnColorStateSetDefaults` | 55, 231 | nvn-75, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21 |
| `nvnColorStateSetLogicOp` | 55, 231 | nvn-75 |
| `nvnDepthStencilStateSetDefaults` | 55, 231 | nvn-76, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21 |
| `nvnDepthStencilStateSetDepthFunc` | 55, 231 | nvn-76, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21 |
| `nvnDepthStencilStateSetDepthTestEnable` | 55, 231 | nvn-76, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21 |
| `nvnDepthStencilStateSetDepthWriteEnable` | 55, 231 | nvn-76, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21 |
| `nvnDepthStencilStateSetStencilFunc` | 55, 231 | nvn-76 |
| `nvnDepthStencilStateSetStencilOp` | 55, 231 | nvn-76 |
| `nvnDepthStencilStateSetStencilTestEnable` | 55, 231 | nvn-76 |
| `nvnMultisampleStateSetAlphaToCoverageEnable` | 55, 231 | nvn-77 |
| `nvnMultisampleStateSetDefaults` | 55, 231 | nvn-77, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21 |
| `nvnMultisampleStateSetMultisampleEnable` | 55, 231 | nvn-77 |
| `nvnMultisampleStateSetSamples` | 55, 231 | nvn-77 |
| `nvnPolygonStateSetCullFace` | 55, 231 | nvn-78, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21 |
| `nvnPolygonStateSetDefaults` | 55, 231 | nvn-78, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21 |
| `nvnPolygonStateSetFrontFace` | 55, 231 | nvn-78, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnPolygonStateSetPolygonMode` | 55, 231 | nvn-78 |
| `nvnPolygonStateSetPolygonOffsetEnables` | 55, 231 | nvn-78 |
| `nvnVertexAttribStateSetDefaults` | 55, 231 | nvn-78, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnVertexAttribStateSetFormat` | 55, 231 | nvn-78, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnVertexAttribStateSetStreamIndex` | 55, 231 | nvn-78, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnVertexStreamStateSetDefaults` | 55, 231 | nvn-78, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnVertexStreamStateSetDivisor` | 55, 231 | nvn-78, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnVertexStreamStateSetStride` | 55, 231 | nvn-78, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnBlendStateGetBlendEquation` | 55, 231 | nvn-74 |
| `nvnBlendStateGetBlendFunc` | 55, 231 | nvn-74 |
| `nvnBlendStateGetBlendTarget` | 55, 231 | nvn-74 |
| `nvnChannelMaskStateGetChannelMask` | 55, 231 | nvn-75 |
| `nvnColorStateGetAlphaTest` | 55, 231 | nvn-75 |
| `nvnColorStateGetLogicOp` | 55, 231 | nvn-75 |
| `nvnDepthStencilStateGetDepthFunc` | 55, 231 | nvn-76 |
| `nvnDepthStencilStateGetDepthTestEnable` | 55, 231 | nvn-76 |
| `nvnDepthStencilStateGetDepthWriteEnable` | 55, 231 | nvn-76 |
| `nvnDepthStencilStateGetStencilFunc` | 55, 231 | nvn-76 |
| `nvnDepthStencilStateGetStencilOp` | 55, 231 | nvn-76 |
| `nvnDepthStencilStateGetStencilTestEnable` | 55, 231 | nvn-76 |
| `nvnMultisampleStateGetAlphaToCoverageDither` | 55, 231 | nvn-77 |
| `nvnMultisampleStateGetAlphaToCoverageEnable` | 55, 231 | nvn-77 |
| `nvnMultisampleStateGetCoverageModulationMode` | 55, 231 | nvn-77 |
| `nvnMultisampleStateGetCoverageToColorEnable` | 55, 231 | nvn-77 |
| `nvnMultisampleStateGetCoverageToColorOutput` | 55, 231 | nvn-77 |
| `nvnMultisampleStateGetMultisampleEnable` | 55, 231 | nvn-77 |
| `nvnMultisampleStateGetRasterSamples` | 55, 231 | nvn-77 |
| `nvnMultisampleStateGetSampleLocationsEnable` | 55, 231 | nvn-77 |
| `nvnMultisampleStateGetSampleLocationsGridEnable` | 55, 231 | nvn-77 |
| `nvnMultisampleStateGetSamples` | 55, 231 | nvn-77 |
| `nvnMultisampleStateSetAlphaToCoverageDither` | 55, 231 | nvn-77 |
| `nvnMultisampleStateSetCoverageModulationMode` | 55, 231 | nvn-77 |
| `nvnMultisampleStateSetCoverageToColorEnable` | 55, 231 | nvn-77 |
| `nvnMultisampleStateSetCoverageToColorOutput` | 55, 231 | nvn-77 |
| `nvnMultisampleStateSetRasterSamples` | 55, 231 | nvn-77 |
| `nvnMultisampleStateSetCoverageTruncationEnable` | 55, 231 | nvn-77 |
| `nvnMultisampleStateGetCoverageTruncationEnable` | 55, 231 | nvn-77 |
| `nvnMultisampleStateSetSampleLocationsEnable` | 55, 231 | nvn-77 |
| `nvnMultisampleStateSetSampleLocationsGridEnable` | 55, 231 | nvn-77 |
| `nvnPolygonStateGetCullFace` | 55, 231 | nvn-78 |
| `nvnPolygonStateGetFrontFace` | 55, 231 | nvn-78 |
| `nvnPolygonStateGetPolygonMode` | 55, 231 | nvn-78 |
| `nvnPolygonStateGetPolygonOffsetEnables` | 55, 231 | nvn-78 |
| `nvnVertexAttribStateGetFormat` | 55, 231 | nvn-78 |
| `nvnVertexAttribStateGetStreamIndex` | 55, 231 | nvn-78 |
| `nvnVertexStreamStateGetDivisor` | 55, 231 | nvn-78 |
| `nvnVertexStreamStateGetStride` | 55, 231 | nvn-78 |

## Queues

30 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnQueueBuilderGetCommandFlushThreshold` | 55, 231 | nvn-67 |
| `nvnQueueBuilderGetCommandMemorySize` | 55, 231 | nvn-67 |
| `nvnQueueBuilderGetComputeMemorySize` | 55, 231 | nvn-67 |
| `nvnQueueBuilderGetControlMemorySize` | 55, 231 | nvn-67 |
| `nvnQueueBuilderGetDevice` | 55, 231 | nvn-67 |
| `nvnQueueBuilderGetFlags` | 55, 231 | nvn-67 |
| `nvnQueueBuilderGetMemory` | 55, 231 | nvn-84 |
| `nvnQueueBuilderGetMemorySize` | 55, 231 | nvn-84 |
| `nvnQueueGetTotalCommandMemoryUsed` | 55, 231 | nvn-84 |
| `nvnQueueGetTotalComputeMemoryUsed` | 55, 231 | nvn-84 |
| `nvnQueueGetTotalControlMemoryUsed` | 55, 231 | nvn-84 |
| `nvnQueueResetMemoryUsageCounts` | 55, 231 | nvn-84 |
| `nvnQueueBuilderGetQueueMemorySize` | 55, 231 | nvn-84 |
| `nvnQueueBuilderSetCommandFlushThreshold` | 55, 231 | nvn-67 |
| `nvnQueueBuilderSetCommandMemorySize` | 55, 231 | nvn-67 |
| `nvnQueueBuilderSetComputeMemorySize` | 55, 231 | nvn-67 |
| `nvnQueueBuilderSetControlMemorySize` | 55, 231 | nvn-67 |
| `nvnQueueBuilderSetDefaults` | 55, 231 | nvn-44, nvn-67, nvn-84, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn2-1 |
| `nvnQueueBuilderSetDevice` | 55, 231 | nvn-44, nvn-67, nvn-84, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn2-1 |
| `nvnQueueBuilderSetFlags` | 55, 231 | nvn-67 |
| `nvnQueueBuilderGetQueuePriority` | 55, 231 | nvn-84 |
| `nvnQueueBuilderSetQueuePriority` | 55, 231 | nvn-84 |
| `nvnQueueBuilderSetQueueMemory` | 55, 231 | nvn-84 |
| `nvnQueueFinalize` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnQueueFinish` | 55, 231 | nvn-45, nvn-46, nvn-48, nvn-49, nvn-50, nvn-51, nvn-52, nvn-53, nvn-57, nvn-59, nvn-60, nvn-61, nvn-62, nvn-66, nvn-73, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-86, nvn-87, nvn-88, nvn-91, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn-132, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-31 |
| `nvnQueueFlush` | 55, 231 | nvn-60, nvn-61, nvn-62, nvn-87 |
| `nvnQueueGetError` | 55, 231 | nvn-66, nvn-89 |
| `nvnQueueInitialize` | 55, 231 | nvn-44, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn2-1 |
| `nvnQueueSubmitCommands` | 55, 231 | nvn-45, nvn-46, nvn-48, nvn-49, nvn-50, nvn-51, nvn-52, nvn-53, nvn-57, nvn-59, nvn-60, nvn-61, nvn-62, nvn-73, nvn-81, nvn-83, nvn-85, nvn-86, nvn-87, nvn-88, nvn-91, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn-132, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-31 |
| `nvnQueueBuilderSetPrimaryQueue` | 55, 231 | nvn-84 |

## Sync and events

24 functions, 2 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnSyncCreateGLSync` | 55, 231 | nvn-64 |
| `nvnSyncInitializeFromFencedGLSync` | 55, 231 | nvn-64 |
| `nvnSyncFinalize` | 55, 231 | nvn-60, nvn-61, nvn-62, nvn-63, nvn-64, nvn-87, nvn-89, nvn-93 |
| `nvnSyncInitialize` | 55, 231 | nvn-60, nvn-61, nvn-62, nvn-63, nvn-64, nvn-87, nvn-89, nvn-93 |
| `nvnSyncWait` | 55, 231 | nvn-60, nvn-61, nvn-62, nvn-63, nvn-87, nvn-93 |
| `nvnSyncGetNvRmSyncNVX` |  |  |
| `nvnSyncInitializeFromNvRmSyncNVX` |  |  |
| `nvnEventBuilderGetMemoryOffset` | 55, 231 | nvn-68 |
| `nvnEventBuilderGetMemoryPool` | 55, 231 | nvn-68 |
| `nvnEventBuilderGetStorage` | 55, 231 | nvn-68 |
| `nvnEventGetMemoryOffset` | 55, 231 | nvn-68 |
| `nvnEventGetMemoryPool` | 55, 231 | nvn-68 |
| `nvnEventBuilderSetDefaults` | 55, 231 | nvn-68, nvn-85, nvn-86, nvn-87 |
| `nvnEventBuilderSetStorage` | 55, 231 | nvn-68, nvn-85, nvn-86, nvn-87 |
| `nvnEventFinalize` | 55, 231 | nvn-68, nvn-85, nvn-86, nvn-87 |
| `nvnEventGetValue` | 55, 231 | nvn-68, nvn-85, nvn-86 |
| `nvnEventInitialize` | 55, 231 | nvn-68, nvn-85, nvn-86, nvn-87 |
| `nvnEventSignal` | 55, 231 | nvn-68, nvn-85, nvn-86, nvn-87 |
| `nvnQueueFenceSync` | 55, 231 | nvn-60, nvn-62, nvn-87 |
| `nvnQueueWaitSync` | 55, 231 | nvn-60, nvn-61, nvn-62 |
| `nvnCommandBufferFenceSync` | 55, 231 | nvn-61 |
| `nvnCommandBufferSignalEvent` | 55, 231 | nvn-85, nvn-86 |
| `nvnCommandBufferWaitEvent` | 55, 231 | nvn-87 |
| `nvnCommandBufferWaitSync` | 55, 231 | nvn-62 |

## Windows and present

35 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnQueueAcquireTexture` | 55, 231 | nvn-91, nvn-92 |
| `nvnQueuePresentTexture` | 55, 231 | nvn-91, nvn-92, nvn-93 |
| `nvnWindowBuilderGetDevice` | 55, 231 | nvn-90 |
| `nvnWindowBuilderGetNativeWindow` | 55, 231 | nvn-90 |
| `nvnWindowBuilderGetNumActiveTextures` | 55, 231 | nvn-90 |
| `nvnWindowGetNativeWindow` | 55, 231 | nvn-92 |
| `nvnWindowGetNumActiveTextures` | 55, 231 | nvn-92 |
| `nvnWindowGetNumTextures` | 55, 231 | nvn-92 |
| `nvnWindowSetNumActiveTextures` | 55, 231 | nvn-92 |
| `nvnWindowBuilderGetNumTextures` | 55, 231 | nvn-90 |
| `nvnWindowBuilderGetPresentInterval` | 55, 231 | nvn-90 |
| `nvnWindowBuilderGetTexture` | 55, 231 | nvn-90 |
| `nvnWindowGetCrop` | 55, 231 | nvn-92 |
| `nvnWindowGetPresentInterval` | 55, 231 | nvn-92 |
| `nvnWindowAcquireTexture` | 55, 231 | nvn-93 |
| `nvnWindowBuilderSetDefaults` | 55, 231 | nvn-90, nvn-91, nvn-134 |
| `nvnWindowBuilderSetDevice` | 55, 231 | nvn-90, nvn-91 |
| `nvnWindowBuilderSetNativeWindow` | 55, 231 | nvn-90, nvn-91 |
| `nvnWindowBuilderSetNumActiveTextures` | 55, 231 | nvn-90 |
| `nvnWindowBuilderSetPresentInterval` | 55, 231 | nvn-90 |
| `nvnWindowBuilderSetTextures` | 55, 231 | nvn-90, nvn-91 |
| `nvnWindowBuilderSetColorSpace` | 231 | nvn-134 |
| `nvnWindowBuilderGetColorSpace` | 231 | nvn-134 |
| `nvnWindowBuilderSetHdrDisplayMetadata` | 231 | nvn-134 |
| `nvnWindowBuilderGetHdrDisplayMetadata` | 231 | nvn-134 |
| `nvnWindowBuilderSetHdrContentMetadata` | 231 | nvn-134 |
| `nvnWindowBuilderGetHdrContentMetadata` | 231 | nvn-134 |
| `nvnWindowSetHdrDisplayMetadata` | 231 | nvn-135 |
| `nvnWindowGetHdrDisplayMetadata` | 231 | nvn-135 |
| `nvnWindowSetHdrContentMetadata` | 231 | nvn-135 |
| `nvnWindowGetHdrContentMetadata` | 231 | nvn-135 |
| `nvnWindowFinalize` | 55, 231 | nvn-91 |
| `nvnWindowInitialize` | 55, 231 | nvn-91 |
| `nvnWindowSetCrop` | 55, 231 | nvn-92 |
| `nvnWindowSetPresentInterval` | 55, 231 | nvn-92 |

## Counters and queries

6 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnDeviceGetTimestampInNanoseconds` | 55, 231 | nvn-15, nvn-16, nvn-106, nvn-107 |
| `nvnCommandBufferReportCounter` | 55, 231 | nvn-104, nvn-106, nvn-107, nvn-108 |
| `nvnCommandBufferReportValue` | 55, 231 | nvn-105 |
| `nvnCommandBufferResetCounter` | 55, 231 | nvn-104, nvn-108 |
| `nvnCommandBufferSetRenderEnable` | 55, 231 | nvn-108 |
| `nvnCommandBufferSetRenderEnableConditional` | 55, 231 | nvn-108 |

## Commands: memory and recording

25 functions, 3 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferGetCommandMemoryFree` | 55, 231 | nvn-109 |
| `nvnCommandBufferGetCommandMemorySize` | 55, 231 | nvn-109 |
| `nvnCommandBufferGetControlMemoryFree` | 55, 231 | nvn-109 |
| `nvnCommandBufferGetControlMemorySize` | 55, 231 | nvn-109 |
| `nvnCommandBufferSetControlMemoryCallbackEnabled` | 55, 231 | nvn-110 |
| `nvnCommandBufferAddCommandMemory` | 55, 231 | nvn-44, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-109, nvn-110, nvn2-1 |
| `nvnCommandBufferAddControlMemory` | 55, 231 | nvn-44, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-109, nvn2-1 |
| `nvnCommandBufferBarrier` | 55, 231 | nvn-45, nvn-46, nvn-48, nvn-49, nvn-51, nvn-52, nvn-53, nvn-57, nvn-60, nvn-61, nvn-62, nvn-81, nvn-83, nvn-87, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-112, nvn-113, nvn-127, nvn-131, nvn-132, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-31 |
| `nvnCommandBufferBeginRecording` | 55, 231 | nvn-45, nvn-46, nvn-48, nvn-49, nvn-50, nvn-51, nvn-52, nvn-53, nvn-57, nvn-59, nvn-60, nvn-61, nvn-62, nvn-73, nvn-81, nvn-83, nvn-85, nvn-86, nvn-87, nvn-88, nvn-91, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn-132, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-31 |
| `nvnCommandBufferSetRecordingMode` |  |  |
| `nvnCommandBufferSetComputeOverlapMode` |  |  |
| `nvnCommandBufferCallCommands` | 55, 231 | nvn-111 |
| `nvnCommandBufferCopyCommands` | 55, 231 | nvn-111 |
| `nvnCommandBufferEndRecording` | 55, 231 | nvn-45, nvn-46, nvn-48, nvn-49, nvn-50, nvn-51, nvn-52, nvn-53, nvn-57, nvn-59, nvn-60, nvn-61, nvn-62, nvn-73, nvn-81, nvn-83, nvn-85, nvn-86, nvn-87, nvn-88, nvn-91, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-111, nvn-127, nvn-131, nvn-132, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-31 |
| `nvnCommandBufferFinalize` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferGetCommandMemoryUsed` | 55, 231 | nvn-109 |
| `nvnCommandBufferGetControlMemoryUsed` | 55, 231 | nvn-109 |
| `nvnCommandBufferGetMemoryCallback` | 55, 231 | nvn-110 |
| `nvnCommandBufferGetMemoryCallbackData` | 55, 231 | nvn-110 |
| `nvnCommandBufferInitialize` | 55, 231 | nvn-44, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn2-1 |
| `nvnCommandBufferIsRecording` | 55, 231 | nvn-109 |
| `nvnCommandBufferSetCommandMemoryCallbackEnabled` | 55, 231 | nvn-110 |
| `nvnCommandBufferSetComputeWorkloadPriority` |  |  |
| `nvnCommandBufferSetMemoryCallback` | 55, 231 | nvn-110 |
| `nvnCommandBufferSetMemoryCallbackData` | 55, 231 | nvn-110 |

## Commands: binds

41 functions, 2 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferBindCoverageModulationTable` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferBindImagesDeferred` | 231 | nvn2-24 |
| `nvnCommandBufferBindSeparateSamplersDeferred` | 231 | nvn2-27 |
| `nvnCommandBufferBindSeparateTexturesDeferred` | 231 | nvn2-27 |
| `nvnCommandBufferBindStorageBuffersDeferred` | 231 | nvn2-6 |
| `nvnCommandBufferBindUniformBuffersDeferred` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferSetProgramSubroutines` |  |  |
| `nvnCommandBufferBindShadingRateImage` | 231 | nvn2-21 |
| `nvnCommandBufferShadingRateImagePalette` |  |  |
| `nvnCommandBufferPushGraphicsConstants` | 231 | nvn2-20 |
| `nvnCommandBufferPushComputeConstants` | 231 | nvn2-19 |
| `nvnCommandBufferBindBlendState` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21 |
| `nvnCommandBufferBindChannelMaskState` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21, nvn2-31 |
| `nvnCommandBufferBindColorState` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21 |
| `nvnCommandBufferBindDepthStencilState` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21 |
| `nvnCommandBufferBindImage` | 231 | nvn2-22 |
| `nvnCommandBufferBindImages` | 231 | nvn2-23 |
| `nvnCommandBufferBindMultisampleState` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21 |
| `nvnCommandBufferBindPolygonState` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21 |
| `nvnCommandBufferBindProgram` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-130, nvn-131, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27 |
| `nvnCommandBufferBindSeparateSampler` | 231 | nvn2-25 |
| `nvnCommandBufferBindSeparateSamplers` | 231 | nvn2-26 |
| `nvnCommandBufferBindSeparateTexture` | 231 | nvn2-25 |
| `nvnCommandBufferBindSeparateTextures` | 231 | nvn2-26 |
| `nvnCommandBufferBindStorageBuffer` | 231 | nvn2-3, nvn2-4, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-25, nvn2-26, nvn2-27 |
| `nvnCommandBufferBindStorageBuffers` | 231 | nvn2-5 |
| `nvnCommandBufferBindTexture` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferBindTextures` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferBindTexturesDeferred` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferBindTransformFeedbackBuffer` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferBindTransformFeedbackBuffers` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferBindUniformBuffer` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferBindUniformBuffers` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferBindVertexAttribState` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferBindVertexBuffer` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferBindVertexBuffers` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferBindVertexStreamState` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferSetSamplerPool` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-25, nvn2-26, nvn2-27 |
| `nvnCommandBufferSetShaderScratchMemory` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferSetTexturePool` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27 |
| `nvnCommandBufferUpdateUniformBuffer` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |

## Commands: draws and dispatches

26 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferDrawTexture` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferBeginTransformFeedback` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferSetGeometryProcessingMode` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21 |
| `nvnCommandBufferDrawMeshTasks` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21 |
| `nvnCommandBufferDrawMeshTasks3D` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21 |
| `nvnCommandBufferDrawMeshTasksIndirect` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21 |
| `nvnCommandBufferDrawMeshTasks3DIndirect` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21 |
| `nvnCommandBufferMultiDrawMeshTasksIndirect` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21 |
| `nvnCommandBufferMultiDrawMeshTasks3DIndirect` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21 |
| `nvnCommandBufferMultiDrawMeshTasksIndirectCount` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21 |
| `nvnCommandBufferMultiDrawMeshTasks3DIndirectCount` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21 |
| `nvnCommandBufferDispatchCompute` | 231 | nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27 |
| `nvnCommandBufferDispatchComputeIndirect` | 231 | nvn2-7 |
| `nvnCommandBufferDrawArrays` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-120, nvn-126, nvn-127, nvn-131 |
| `nvnCommandBufferDrawArraysIndirect` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferDrawArraysInstanced` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferDrawElements` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-121, nvn-127, nvn-131 |
| `nvnCommandBufferDrawElementsBaseVertex` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferDrawElementsIndirect` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferDrawElementsInstanced` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferDrawTransformFeedback` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferEndTransformFeedback` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferMultiDrawArraysIndirectCount` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferMultiDrawElementsIndirectCount` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferPauseTransformFeedback` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |
| `nvnCommandBufferResumeTransformFeedback` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131 |

## Commands: clears and copies

25 functions, 7 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferClearTexturei` | 55, 231 | nvn-113 |
| `nvnCommandBufferGetCopyImageStride` | 55, 231 | nvn-115 |
| `nvnCommandBufferGetCopyRowStride` | 55, 231 | nvn-115 |
| `nvnCommandBufferResolveDepthBuffer` |  |  |
| `nvnCommandBufferTiledDownsample` |  |  |
| `nvnCommandBufferTiledDownsampleTextureView` |  |  |
| `nvnCommandBufferClearBuffer` | 55, 231 | nvn-114 |
| `nvnCommandBufferClearColor` | 55, 231 | nvn-45, nvn-46, nvn-60, nvn-61, nvn-62, nvn-87, nvn-91, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn-132, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21, nvn2-31 |
| `nvnCommandBufferClearColori` | 55, 231 | nvn-112 |
| `nvnCommandBufferClearColorui` | 55, 231 | nvn-112 |
| `nvnCommandBufferClearDepthStencil` | 55, 231 | nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21, nvn2-31 |
| `nvnCommandBufferClearTexture` | 55, 231 | nvn-113 |
| `nvnCommandBufferClearTextureui` | 55, 231 | nvn-113 |
| `nvnCommandBufferCopyBufferToBuffer` | 55, 231 | nvn-50, nvn-59, nvn-73, nvn-88, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-25, nvn2-26, nvn2-27 |
| `nvnCommandBufferCopyBufferToTexture` | 55, 231 | nvn-48, nvn-49, nvn-51, nvn-52, nvn-53, nvn-57, nvn-83, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-31 |
| `nvnCommandBufferCopyTextureToBuffer` | 55, 231 | nvn-45, nvn-46, nvn-48, nvn-49, nvn-51, nvn-52, nvn-53, nvn-57, nvn-60, nvn-61, nvn-62, nvn-81, nvn-83, nvn-87, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-112, nvn-113, nvn-127, nvn-131, nvn-132, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-31 |
| `nvnCommandBufferCopyTextureToTexture` | 55, 231 | nvn-49, nvn-52 |
| `nvnCommandBufferDiscardColor` | 55, 231 | nvn-116 |
| `nvnCommandBufferDiscardDepthStencil` | 55, 231 | nvn-116 |
| `nvnCommandBufferDownsample` |  |  |
| `nvnCommandBufferDownsampleTextureView` |  |  |
| `nvnCommandBufferRestoreZCullData` |  |  |
| `nvnCommandBufferSaveZCullData` |  |  |
| `nvnCommandBufferSetCopyImageStride` | 55, 231 | nvn-115 |
| `nvnCommandBufferSetCopyRowStride` | 55, 231 | nvn-48, nvn-49, nvn-50, nvn-51, nvn-52, nvn-53, nvn-57, nvn-81, nvn-83, nvn-115 |

## Commands: render state

36 functions, 13 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferOverrideSubtileSize` |  |  |
| `nvnCommandBufferSetColorReductionEnable` |  |  |
| `nvnCommandBufferSetColorReductionThresholds` |  |  |
| `nvnCommandBufferSetConservativeRasterDilate` | 55, 231 | nvn-125 |
| `nvnCommandBufferSetInnerTessellationLevels` | 55, 231 | nvn-127 |
| `nvnCommandBufferSetOuterTessellationLevels` | 55, 231 | nvn-127 |
| `nvnCommandBufferSetProvokingVertexMode` | 55, 231 | nvn-119 |
| `nvnCommandBufferSetSubpixelPrecisionBias` | 55, 231 | nvn-124 |
| `nvnCommandBufferSetZCullZF32CompressionEnable` |  |  |
| `nvnCommandBufferSetShadingRatePaletteMode` |  |  |
| `nvnCommandBufferSetAlphaRef` |  |  |
| `nvnCommandBufferSetBlendColor` |  |  |
| `nvnCommandBufferSetConservativeRasterEnable` | 55, 231 | nvn-124, nvn-125 |
| `nvnCommandBufferSetDepthBounds` | 55, 231 | nvn-118 |
| `nvnCommandBufferSetDepthClamp` | 55, 231 | nvn-119 |
| `nvnCommandBufferSetDepthRange` | 55, 231 | nvn-118 |
| `nvnCommandBufferSetDepthRanges` | 55, 231 | nvn-118 |
| `nvnCommandBufferSetLineWidth` | 55, 231 | nvn-126 |
| `nvnCommandBufferSetPatchSize` | 55, 231 | nvn-127 |
| `nvnCommandBufferSetPointSize` | 55, 231 | nvn-120 |
| `nvnCommandBufferSetPolygonOffsetClamp` |  |  |
| `nvnCommandBufferSetPrimitiveRestart` | 55, 231 | nvn-121 |
| `nvnCommandBufferSetRasterizerDiscard` | 55, 231 | nvn-119 |
| `nvnCommandBufferSetRenderTargets` | 55, 231 | nvn-45, nvn-46, nvn-60, nvn-61, nvn-62, nvn-87, nvn-91, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-112, nvn-127, nvn-131, nvn-132, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21, nvn2-31 |
| `nvnCommandBufferSetSampleMask` |  |  |
| `nvnCommandBufferSetScissor` | 55, 231 | nvn-45, nvn-46, nvn-60, nvn-61, nvn-62, nvn-87, nvn-91, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-112, nvn-127, nvn-131, nvn-132, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21, nvn2-31 |
| `nvnCommandBufferSetScissors` | 55, 231 | nvn-117 |
| `nvnCommandBufferSetStencilCullCriteria` |  |  |
| `nvnCommandBufferSetStencilMask` |  |  |
| `nvnCommandBufferSetStencilRef` |  |  |
| `nvnCommandBufferSetStencilValueMask` |  |  |
| `nvnCommandBufferSetTiledCacheAction` | 55, 231 | nvn-123 |
| `nvnCommandBufferSetTiledCacheTileSize` | 55, 231 | nvn-123 |
| `nvnCommandBufferSetViewport` | 55, 231 | nvn-45, nvn-46, nvn-60, nvn-61, nvn-62, nvn-87, nvn-91, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-99, nvn-100, nvn-101, nvn-102, nvn-103, nvn-127, nvn-131, nvn-132, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-20, nvn2-21, nvn2-31 |
| `nvnCommandBufferSetViewportSwizzles` | 55, 231 | nvn-122 |
| `nvnCommandBufferSetViewports` | 55, 231 | nvn-117 |

## DLSS

58 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnDlssNetworkBuilderSetDefaults` | 231 | nvn2-28, nvn2-30, nvn2-31 |
| `nvnDlssNetworkBuilderSetDevice` | 231 | nvn2-28, nvn2-30, nvn2-31 |
| `nvnDlssNetworkBuilderGetDevice` | 231 | nvn2-28 |
| `nvnDlssNetworkBuilderSetFlags` | 231 | nvn2-28 |
| `nvnDlssNetworkBuilderGetFlags` | 231 | nvn2-28 |
| `nvnDlssNetworkBuilderSetMethod` | 231 | nvn2-28 |
| `nvnDlssNetworkBuilderGetMethod` | 231 | nvn2-28 |
| `nvnDlssNetworkBuilderSetPreset` | 231 | nvn2-28 |
| `nvnDlssNetworkBuilderGetPreset` | 231 | nvn2-28 |
| `nvnDlssNetworkBuilderSetOutputResolution` | 231 | nvn2-28, nvn2-30, nvn2-31 |
| `nvnDlssNetworkBuilderGetOutputResolution` | 231 | nvn2-28 |
| `nvnDlssNetworkBuilderSetMaxEvaluationEntries` | 231 | nvn2-28 |
| `nvnDlssNetworkBuilderGetMaxEvaluationEntries` | 231 | nvn2-28 |
| `nvnDlssNetworkBuilderSetStorage` | 231 | nvn2-28, nvn2-30, nvn2-31 |
| `nvnDlssNetworkBuilderGetMemory` | 231 | nvn2-28 |
| `nvnDlssNetworkBuilderGetMemorySize` | 231 | nvn2-28 |
| `nvnDlssNetworkBuilderSetTextureIDs` | 231 | nvn2-28 |
| `nvnDlssNetworkBuilderGetTexturePool` | 231 | nvn2-28 |
| `nvnDlssNetworkBuilderGetTextureIDs` | 231 | nvn2-28 |
| `nvnDlssNetworkBuilderGetOptimalSettings` | 231 | nvn2-30 |
| `nvnDlssNetworkBuilderGetStorageSize` | 231 | nvn2-30, nvn2-31 |
| `nvnDlssNetworkInitialize` | 231 | nvn2-30, nvn2-31 |
| `nvnDlssNetworkFinalize` | 231 | nvn2-30, nvn2-31 |
| `nvnDlssNetworkResize` | 231 | nvn2-30 |
| `nvnCommandBufferEvaluateDlss` | 231 | nvn2-31 |
| `nvnDlssParametersSetDefaults` | 231 | nvn2-29, nvn2-31 |
| `nvnDlssParametersSetInputColor` | 231 | nvn2-29, nvn2-31 |
| `nvnDlssParametersSetOutputColor` | 231 | nvn2-29, nvn2-31 |
| `nvnDlssParametersSetMotionVectors` | 231 | nvn2-29, nvn2-31 |
| `nvnDlssParametersSetDepth` | 231 | nvn2-29, nvn2-31 |
| `nvnDlssParametersSetBiasCurrentColorMask` | 231 | nvn2-29 |
| `nvnDlssParametersSetExposureTexture` | 231 | nvn2-29 |
| `nvnDlssParametersSetJitterOffset` | 231 | nvn2-29 |
| `nvnDlssParametersGetJitterOffset` | 231 | nvn2-29 |
| `nvnDlssParametersSetMotionVectorScale` | 231 | nvn2-29 |
| `nvnDlssParametersGetMotionVectorScale` | 231 | nvn2-29 |
| `nvnDlssParametersSetPreExposure` | 231 | nvn2-29 |
| `nvnDlssParametersGetPreExposure` | 231 | nvn2-29 |
| `nvnDlssParametersSetHistoryReset` | 231 | nvn2-29, nvn2-31 |
| `nvnDlssParametersGetHistoryReset` | 231 | nvn2-29 |
| `nvnDlssParametersSetEvaluationIndex` | 231 | nvn2-29 |
| `nvnDlssParametersGetEvaluationIndex` | 231 | nvn2-29 |
| `nvnDlssParametersSetInputSubrectangleSize` | 231 | nvn2-29, nvn2-31 |
| `nvnDlssParametersGetInputSubrectangleSize` | 231 | nvn2-29 |
| `nvnDlssParametersSetInputColorSubrectangleBase` | 231 | nvn2-29 |
| `nvnDlssParametersGetInputColorSubrectangleBase` | 231 | nvn2-29 |
| `nvnDlssParametersSetDepthSubrectangleBase` | 231 | nvn2-29 |
| `nvnDlssParametersGetDepthSubrectangleBase` | 231 | nvn2-29 |
| `nvnDlssParametersSetMotionVectorSubrectangleBase` | 231 | nvn2-29 |
| `nvnDlssParametersGetMotionVectorSubrectangleBase` | 231 | nvn2-29 |
| `nvnDlssParametersSetBiasCurrentColorSubrectangleBase` | 231 | nvn2-29 |
| `nvnDlssParametersGetBiasCurrentColorSubrectangleBase` | 231 | nvn2-29 |
| `nvnDlssParametersSetOutputSubrectangleBase` | 231 | nvn2-29 |
| `nvnDlssParametersGetOutputSubrectangleBase` | 231 | nvn2-29 |
| `nvnDlssParametersSetOverlayDebugLayer` | 231 | nvn2-29 |
| `nvnDlssParametersSetOverlayFullScreen` | 231 | nvn2-29 |
| `nvnDlssParametersSetOverlayShowNans` | 231 | nvn2-29 |
| `nvnDlssParametersSetOverlayJitterDebug` | 231 | nvn2-29 |

## Ray tracing

15 functions, 9 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnDeviceCalculateAccelerationStructureMemoryRequirements` |  |  |
| `nvnProgramGetShaderGroupHandle` |  |  |
| `nvnProgramBuilderSetRayTracingModules` |  |  |
| `nvnProgramBuilderSetMaxCallableRecursionDepth` | 231 | nvn-133 |
| `nvnProgramBuilderGetMaxCallableRecursionDepth` | 231 | nvn-133 |
| `nvnProgramBuilderSetMaxTraceRecursionDepth` | 231 | nvn-133 |
| `nvnProgramBuilderGetMaxTraceRecursionDepth` | 231 | nvn-133 |
| `nvnProgramBuilderSetProgramRayTracingFlags` | 231 | nvn-133 |
| `nvnProgramBuilderGetProgramRayTracingFlags` | 231 | nvn-133 |
| `nvnQueueBuilderGetRayTracingMemorySize` |  |  |
| `nvnQueueBuilderSetRayTracingMemorySize` |  |  |
| `nvnQueueGetTotalRayTracingMemoryUsed` |  |  |
| `nvnCommandBufferBindAccelerationStructure` |  |  |
| `nvnCommandBufferBuildAccelerationStructures` |  |  |
| `nvnCommandBufferCompactAccelerationStructure` |  |  |

## Debug

44 functions, 2 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnSetExternalDebuggerEnabledNVX` | 55, 231 | nvn-89 |
| `nvnDeviceGenerateDebugDomainId` | 55, 231 | nvn-89, nvn-128, nvn-132 |
| `nvnDeviceInstallDebugCallback` | 55, 231 | nvn-89 |
| `nvnDeviceIsExternalDebuggerAttached` | 55, 231 | nvn-89 |
| `nvnDeviceSetAutomaticCheckpointsByDebugGroup` | 55, 231 | nvn-89 |
| `nvnDeviceSetAutomaticCheckpointsEnable` | 55, 231 | nvn-89 |
| `nvnDeviceSetAutomaticCheckpointsMask` | 55, 231 | nvn-89 |
| `nvnDeviceSetAutomaticCheckpointsSamplingInterval` | 55, 231 | nvn-89 |
| `nvnDeviceSetCheckpointCallStacksEnable` | 55, 231 | nvn-89 |
| `nvnDeviceSetCheckpointMaxCallStacksDepth` | 55, 231 | nvn-89 |
| `nvnDeviceWalkDebugDatabase` | 55, 231 | nvn-89 |
| `nvnMemoryPoolSetDebugLabel` | 55, 231 | nvn-89 |
| `nvnBufferGetDebugID` | 55, 231 | nvn-89 |
| `nvnBufferSetDebugLabel` | 55, 231 | nvn-89 |
| `nvnTextureGetDebugID` | 55, 231 | nvn-89 |
| `nvnTextureSetDebugLabel` | 55, 231 | nvn-89 |
| `nvnSamplerGetDebugID` | 55, 231 | nvn-89 |
| `nvnSamplerSetDebugLabel` | 55, 231 | nvn-89 |
| `nvnProgramBuilderSetDebugDataHashes` |  |  |
| `nvnProgramBuilderGetDebugDataHashes` |  |  |
| `nvnCommandBufferInsertCheckpointDynamic` | 55, 231 | nvn-128 |
| `nvnCommandBufferInsertCheckpointStatic` | 55, 231 | nvn-128 |
| `nvnCommandBufferInsertDebugMarkerGraphics` | 231 | nvn-132 |
| `nvnCommandBufferInsertDebugMarkerCompute` | 231 | nvn-132 |
| `nvnCommandBufferPushDebugGroupGraphics` | 231 | nvn-132 |
| `nvnCommandBufferPushDebugGroupCompute` | 231 | nvn-132 |
| `nvnCommandBufferPopDebugGroupGraphics` | 231 | nvn-132 |
| `nvnCommandBufferPopDebugGroupCompute` | 231 | nvn-132 |
| `nvnCommandBufferInsertDebugMarker` | 55, 231 | nvn-128 |
| `nvnCommandBufferInsertDebugMarkerDynamic` | 55, 231 | nvn-128 |
| `nvnCommandBufferInsertDebugMarkerStatic` | 55, 231 | nvn-128 |
| `nvnCommandBufferPopDebugGroup` | 55, 231 | nvn-128 |
| `nvnCommandBufferPopDebugGroupId` | 55, 231 | nvn-128 |
| `nvnCommandBufferPushDebugGroup` | 55, 231 | nvn-128 |
| `nvnCommandBufferPushDebugGroupDynamic` | 55, 231 | nvn-128 |
| `nvnCommandBufferPushDebugGroupStatic` | 55, 231 | nvn-128 |
| `nvnCommandBufferSetDebugLabel` | 55, 231 | nvn-89 |
| `nvnDeviceSetDebugLabel` | 55, 231 | nvn-89 |
| `nvnProgramSetDebugLabel` | 55, 231 | nvn-89 |
| `nvnQueueSetDebugLabel` | 55, 231 | nvn-89 |
| `nvnSamplerPoolSetDebugLabel` | 55, 231 | nvn-89 |
| `nvnSyncSetDebugLabel` | 55, 231 | nvn-89 |
| `nvnTexturePoolSetDebugLabel` | 55, 231 | nvn-89 |
| `nvnWindowSetDebugLabel` | 55, 231 | nvn-92 |
