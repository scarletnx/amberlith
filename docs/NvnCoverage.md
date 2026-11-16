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
names 677; the layer has 486 of them in its API 55 build and 536 in its API 231 build; a case calls 531. The
other 146 have no call at all: a runtime could answer any of them wrongly and every case would still pass.

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
| [Device](#device) | 36 | 22 | 23 | 22 | 14 |
| [Memory pools](#memory-pools) | 18 | 16 | 16 | 16 | 2 |
| [Buffers](#buffers) | 16 | 16 | 16 | 16 | 0 |
| [Textures](#textures) | 95 | 94 | 95 | 94 | 1 |
| [Samplers](#samplers) | 45 | 45 | 45 | 45 | 0 |
| [Programs](#programs) | 20 | 4 | 13 | 13 | 7 |
| [State objects](#state-objects) | 82 | 82 | 82 | 82 | 0 |
| [Queues](#queues) | 30 | 30 | 30 | 30 | 0 |
| [Sync and events](#sync-and-events) | 24 | 18 | 18 | 18 | 6 |
| [Windows and present](#windows-and-present) | 35 | 25 | 35 | 35 | 0 |
| [Counters and queries](#counters-and-queries) | 6 | 6 | 6 | 6 | 0 |
| [Commands: memory and recording](#commands-memory-and-recording) | 25 | 22 | 22 | 22 | 3 |
| [Commands: binds](#commands-binds) | 41 | 22 | 28 | 25 | 16 |
| [Commands: draws and dispatches](#commands-draws-and-dispatches) | 26 | 7 | 18 | 18 | 8 |
| [Commands: clears and copies](#commands-clears-and-copies) | 25 | 18 | 18 | 18 | 7 |
| [Commands: render state](#commands-render-state) | 36 | 23 | 23 | 23 | 13 |
| [DLSS](#dlss) | 58 | 0 | 0 | 0 | 58 |
| [Ray tracing](#ray-tracing) | 15 | 0 | 6 | 6 | 9 |
| [Debug](#debug) | 44 | 36 | 42 | 42 | 2 |
| **All** | **677** | **486** | **536** | **531** | **146** |

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
| nvn-64 | nvn | the queue has no error after work that was valid | the NRO, the Switch 2 program |
| nvn-65 | nvn | a queue builder gives back its flags and says which of its sizes were set | the NRO, the Switch 2 program |
| nvn-66 | nvn | an event keeps what the processor signals: a write, an add and an add that wraps | the NRO, the Switch 2 program |
| nvn-67 | nvn | a sampler builder gives back each number it was set | the NRO, the Switch 2 program |
| nvn-68 | nvn | a sampler says what its builder had, and is the same as another of that state alone | the NRO, the Switch 2 program |
| nvn-69 | nvn | the texture and sampler pools say where they are, and a builder registers as its sampler | the NRO, the Switch 2 program |
| nvn-70 | nvn | a buffer is a range of its pool: its address, its mapping, and what it and its builder say | the NRO, the Switch 2 program |
| nvn-71 | nvn | bytes flushed from one buffer and copied to another are there after an invalidate | the NRO, the Switch 2 program |
| nvn-72 | nvn | a blend state gives back its target, functions, equations and advanced mode | the NRO, the Switch 2 program |
| nvn-73 | nvn | a channel mask and a colour state give back a target's channels, blending, the logic operation and the alpha test | the NRO, the Switch 2 program |
| nvn-74 | nvn | a depth and stencil state gives back its depth test and each face's stencil | the NRO, the Switch 2 program |
| nvn-75 | nvn | a multisample state gives back its samples, its switches and its sample locations' grid | the NRO, the Switch 2 program |
| nvn-76 | nvn | a polygon state, a vertex attribute and a vertex stream give back what they were set | the NRO, the Switch 2 program |
| nvn-77 | nvn | a texture builder and a view give back their swizzle, depth and stencil mode and the rest | the NRO, the Switch 2 program |
| nvn-78 | nvn | a texture says its swizzle, storage and address, and is the same as its twin alone | the NRO, the Switch 2 program |
| nvn-79 | nvn | texels the processor writes into a texture are what it and the graphics unit read back | the NRO, the Switch 2 program |
| nvn-80 | nvn | a part of a texture written and read with row strides keeps the bytes between rows | the NRO, the Switch 2 program |
| nvn-81 | nvn | a 1D texture, each layer of an array and each slice of a 3D texture keep a picture of their own | the NRO, the Switch 2 program |
| nvn-82 | nvn | a queue builder gives back its memory and priority, and a queue's memory counts are nought once reset | the NRO, the Switch 2 program |
| nvn-83 | nvn | the debug layer's calls are answered as a release driver answers them, and a label changes no object | the NRO, the Switch 2 program |
| nvn-84 | nvn | a window builder gives back what it was set | the NRO, the Switch 2 program |
| nvn-85 | nvn | a window acquires and presents both its textures | the NRO, the Switch 2 program |
| nvn-86 | nvn | a window says what it is of, and keeps the active textures, the interval and the crop it is set | the NRO, the Switch 2 program |
| nvn-87 | nvn | a texture the window hands with a sync is free once the sync signals, and presents | the NRO, the Switch 2 program |
| nvn-88 | nvn | the same scene through the NVN the program looks up holds what the processor works out | the NRO |
| nvn-89 | nvn | the same texture past its edges through the NVN the program looks up does the same | the NRO |
| nvn-90 | nvn | the same four indexed quads through the NVN the program looks up are each in its quarter | the NRO |
| nvn-91 | nvn | the scene of five draws bound through the looked up calls that take several holds what the processor works out | the NRO |
| nvn-92 | nvn | the scene of five draws bound through the looked up deferred calls holds what the processor works out | the NRO |
| nvn-93 | nvn | the same four quads of an updated uniform buffer through the NVN the program looks up are each its own colour | the NRO |
| nvn-94 | nvn | the same eight instances through the NVN the program looks up are each in its cell | the NRO |
| nvn-95 | nvn | the samples counter reports a quad's 256 pixels and none after a reset, through the NVN the program looks up | the NRO |
| nvn-96 | nvn | two values a list reports are in memory once it has run, through the NVN the program looks up | the NRO |
| nvn-97 | nvn | two timestamp reports hold no count and times between the device's clock before and after, through the NVN the program looks up | the NRO |
| nvn-98 | nvn | a timestamp report's word is the graphics unit's ticks, 384 every 625 nanoseconds, through the NVN the program looks up | the NRO |
| nvn-99 | nvn | the render enable draws under on and under a condition that holds and skips the rest, through the NVN the program looks up | the NRO |
| nvn-100 | nvn | a command buffer's memory used and free come to what it was given, and draws take some, through the NVN the program looks up | the NRO |
| nvn-101 | nvn | a list out of command memory asks the program's callback and draws on, through the NVN the program looks up | the NRO |
| nvn-102 | nvn | a list called from another and a list copied into it draw their cells, through the NVN the program looks up | the NRO |
| nvn-103 | nvn | clears of an unsigned and a signed integer target hold the integers given, through the NVN the program looks up | the NRO |
| nvn-104 | nvn | a clear of a texture that is no target reaches its region and its mask's channels, through the NVN the program looks up | the NRO |
| nvn-105 | nvn | a range of a buffer cleared to a word holds it, and nothing beside the range, through the NVN the program looks up | the NRO |
| nvn-106 | nvn | a command buffer gives back the copy strides it was set, through the NVN the program looks up | the NRO |
| nvn-107 | nvn | targets discarded, then cleared and drawn, hold the clear and the draw, through the NVN the program looks up | the NRO |
| nvn-108 | nvn | a scissor and a viewport set by number bound and place a quad, through the NVN the program looks up | the NRO |
| nvn-109 | nvn | depth ranges move a quad across the depth test and depth bounds test the target's depth, through the NVN the program looks up | the NRO |
| nvn-110 | nvn | the rasterizer discards, clamps depth and takes a flat colour from the provoking vertex, through the NVN the program looks up | the NRO |
| nvn-111 | nvn | a point of sixteen pixels fills its cell, through the NVN the program looks up | the NRO |
| nvn-112 | nvn | a strip with a restart index is two quads, through the NVN the program looks up | the NRO |
| nvn-113 | nvn | a viewport swizzle of x and y turns a quad about the diagonal, through the NVN the program looks up | the NRO |
| nvn-114 | nvn | draws around the tiled cache's actions are as without them, through the NVN the program looks up | the NRO |
| nvn-115 | nvn | conservative rasterization draws every pixel a quad touches, through the NVN the program looks up | the NRO |
| nvn-116 | nvn | a conservative quad dilated by half a pixel reaches the pixels a third of one away, through the NVN the program looks up | the NRO |
| nvn-117 | nvn | a line of width four is four rows of pixels, through the NVN the program looks up | the NRO |
| nvn-118 | nvn | patches of the size and the outer levels a list sets fill their cells, through the NVN the program looks up | the NRO |
| nvn-119 | nvn | draws between debug markers, groups and checkpoints are as without them, through the NVN the program looks up | the NRO |
| nvn-120 | nvn | the device says its modes and its builder its flags, and clear values register, through the NVN the program looks up | the NRO |
| nvn-121 | nvn | a program set from the longer shader data draws, through the NVN the program looks up | the NRO |
| nvn-122 | nvn | a texture's handle and a sampler's put together sample, and a texel fetch handle fetches, through the NVN the program looks up | the NRO |
| nvn-123 | nvn | a clear between the Ounce's graphics and compute markers and groups is the clear | the Switch 2 program |
| nvn-124 | nvn | an Ounce program builder gives back what it was set, a ray tracing program's numbers too | the Switch 2 program |
| nvn-125 | nvn | an Ounce window builder starts with the driver's colour space and HDR metadata, and gives back what it is set | the Switch 2 program |
| nvn-126 | nvn | an Ounce window has its builder's HDR metadata, and gives back what it is set | the Switch 2 program |
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

## Device

36 functions, 14 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnBootstrapLoader` | 55, 231 | nvn-41, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn2-1 |
| `nvnDeviceGetProcAddress` | 55, 231 | nvn-41, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn2-1 |
| `nvnDeviceApplyDeferredFinalizes` |  |  |
| `nvnDeviceBuilderGetFlags` | 55, 231 | nvn-120 |
| `nvnDeviceFinalize` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnDeviceRegisterFastClearColori` | 55, 231 | nvn-120 |
| `nvnDeviceRegisterFastClearColorui` | 55, 231 | nvn-120 |
| `nvnDeviceSetIntermediateShaderCache` |  |  |
| `nvnDeviceBuilderSetDefaults` | 55, 231 | nvn-42, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-120, nvn2-1 |
| `nvnDeviceBuilderSetFlags` | 55, 231 | nvn-42, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-120, nvn2-1 |
| `nvnDeviceGetImageHandle` | 231 |  |
| `nvnDeviceFinalizeCommandHandle` |  |  |
| `nvnDeviceGetCurrentTimestampInNanoseconds` | 55, 231 | nvn-15, nvn-16, nvn-97, nvn-98 |
| `nvnDeviceGetDepthMode` | 55, 231 | nvn-120 |
| `nvnDeviceGetInteger` | 55, 231 | nvn-43, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn2-1 |
| `nvnDeviceGetWindowOriginMode` | 55, 231 | nvn-120 |
| `nvnDeviceGetSeparateSamplerHandle` | 55, 231 | nvn-122 |
| `nvnDeviceGetSeparateTextureHandle` | 55, 231 | nvn-122 |
| `nvnDeviceGetTexelFetchHandle` | 55, 231 | nvn-122 |
| `nvnDeviceGetTextureHandle` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnDeviceInitialize` | 55, 231 | nvn-42, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn2-1 |
| `nvnDeviceRegisterFastClearColor` | 55, 231 | nvn-120 |
| `nvnDeviceRegisterFastClearDepth` | 55, 231 | nvn-120 |
| `nvnDeviceRegisterFastClearStencil` | 55, 231 | nvn-120 |
| `nvnDeviceSetDepthMode` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94 |
| `nvnDeviceSetWindowOriginMode` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94 |
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

18 functions, 2 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnMemoryPoolBuilderGetDevice` | 55, 231 | nvn-58 |
| `nvnMemoryPoolBuilderGetFlags` | 55, 231 | nvn-58 |
| `nvnMemoryPoolBuilderGetMemory` | 55, 231 | nvn-58 |
| `nvnMemoryPoolBuilderGetSize` | 55, 231 | nvn-58 |
| `nvnMemoryPoolBuilderSetDefaults` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-56, nvn-58, nvn-81, nvn-85, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-103, nvn-104, nvn2-1 |
| `nvnMemoryPoolBuilderSetDevice` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-56, nvn-58, nvn-81, nvn-85, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-103, nvn-104, nvn2-1 |
| `nvnMemoryPoolBuilderSetFlags` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-56, nvn-58, nvn-81, nvn-85, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-103, nvn-104, nvn2-1 |
| `nvnMemoryPoolBuilderSetStorage` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-56, nvn-58, nvn-81, nvn-85, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-103, nvn-104, nvn2-1 |
| `nvnMemoryPoolBuilderSetNativeHandleNVX` |  |  |
| `nvnMemoryPoolFinalize` | 55, 231 | nvn-21, nvn-22, nvn-81, nvn-85, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-103, nvn-104, nvn-118, nvn-122 |
| `nvnMemoryPoolFlushMappedRange` | 55, 231 | nvn-59, nvn-71 |
| `nvnMemoryPoolGetBufferAddress` | 55, 231 | nvn-45, nvn-46, nvn-48, nvn-49, nvn-50, nvn-51, nvn-52, nvn-53, nvn-57, nvn-59, nvn-60, nvn-61, nvn-62, nvn-70, nvn-78, nvn-79, nvn-81, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122, nvn-123, nvn2-2, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-8, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18 |
| `nvnMemoryPoolGetFlags` | 55, 231 | nvn-58 |
| `nvnMemoryPoolGetSize` | 55, 231 | nvn-58, nvn-83 |
| `nvnMemoryPoolInitialize` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-56, nvn-58, nvn-81, nvn-85, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-103, nvn-104, nvn2-1 |
| `nvnMemoryPoolInvalidateMappedRange` | 55, 231 | nvn-59 |
| `nvnMemoryPoolMap` | 55, 231 | nvn-45, nvn-46, nvn-47, nvn-48, nvn-49, nvn-50, nvn-51, nvn-52, nvn-53, nvn-57, nvn-58, nvn-59, nvn-60, nvn-61, nvn-62, nvn-66, nvn-69, nvn-70, nvn-71, nvn-79, nvn-81, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122, nvn-123, nvn2-2, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-8, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18 |
| `nvnMemoryPoolMapVirtual` |  |  |

## Buffers

16 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnBufferBuilderGetDevice` | 55, 231 | nvn-70 |
| `nvnBufferBuilderGetMemoryOffset` | 55, 231 | nvn-70 |
| `nvnBufferBuilderGetMemoryPool` | 55, 231 | nvn-70 |
| `nvnBufferBuilderGetSize` | 55, 231 | nvn-70 |
| `nvnBufferBuilderSetDefaults` | 55, 231 | nvn-70, nvn-71, nvn-83 |
| `nvnBufferBuilderSetDevice` | 55, 231 | nvn-70, nvn-71, nvn-83 |
| `nvnBufferBuilderSetStorage` | 55, 231 | nvn-70, nvn-71, nvn-83 |
| `nvnBufferFinalize` | 55, 231 | nvn-70, nvn-71, nvn-83 |
| `nvnBufferFlushMappedRange` | 55, 231 | nvn-71 |
| `nvnBufferInvalidateMappedRange` | 55, 231 | nvn-71 |
| `nvnBufferGetAddress` | 55, 231 | nvn-70, nvn-71 |
| `nvnBufferGetMemoryOffset` | 55, 231 | nvn-70 |
| `nvnBufferGetMemoryPool` | 55, 231 | nvn-70 |
| `nvnBufferGetSize` | 55, 231 | nvn-70, nvn-83 |
| `nvnBufferInitialize` | 55, 231 | nvn-70, nvn-71, nvn-83 |
| `nvnBufferMap` | 55, 231 | nvn-70, nvn-71 |

## Textures

95 functions, 1 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnTextureBuilderGetGLTextureName` | 55, 231 | nvn-77 |
| `nvnTextureBuilderGetMemoryOffset` | 55, 231 | nvn-54 |
| `nvnTextureBuilderGetPackagedTextureLayout` | 55, 231 | nvn-77 |
| `nvnTextureBuilderGetRawStorageClass` | 55, 231 | nvn-78 |
| `nvnTextureBuilderGetSparseTileLayout` | 55, 231 | nvn-78 |
| `nvnTextureBuilderGetViewOffset` | 55, 231 | nvn-78 |
| `nvnTextureBuilderSetGLTextureName` | 55, 231 | nvn-77 |
| `nvnTextureGetNvRmSurfaceNVX` | 55, 231 | nvn-78 |
| `nvnTextureGetRawStorageClass` | 55, 231 | nvn-78 |
| `nvnTextureGetSparseTileLayout` | 55, 231 | nvn-78 |
| `nvnTextureInvalidateTexels` | 55, 231 | nvn-79 |
| `nvnTextureReadTexelsStrided` | 55, 231 | nvn-80 |
| `nvnTextureViewCompare` | 55, 231 | nvn-55 |
| `nvnTextureBuilderGetDepthStencilMode` | 55, 231 | nvn-77 |
| `nvnTextureBuilderGetDevice` | 55, 231 | nvn-54 |
| `nvnTextureBuilderGetMemoryPool` | 55, 231 | nvn-54 |
| `nvnTextureBuilderGetPackagedTextureData` | 55, 231 | nvn-77 |
| `nvnTextureBuilderGetSwizzle` | 55, 231 | nvn-77 |
| `nvnTextureBuilderGetDepth` | 55, 231 | nvn-54 |
| `nvnTextureBuilderGetFlags` | 55, 231 | nvn-54 |
| `nvnTextureBuilderGetFormat` | 55, 231 | nvn-54 |
| `nvnTextureBuilderGetHeight` | 55, 231 | nvn-54 |
| `nvnTextureBuilderGetLevels` | 55, 231 | nvn-54 |
| `nvnTextureBuilderGetSamples` | 55, 231 | nvn-54 |
| `nvnTextureBuilderGetStorageAlignment` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-56, nvn-81, nvn-85, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-103, nvn-104, nvn-118, nvn-122, nvn2-1 |
| `nvnTextureBuilderGetStorageClass` | 55, 231 | nvn-78 |
| `nvnTextureBuilderGetStorageSize` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-56, nvn-78, nvn-81, nvn-85, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-103, nvn-104, nvn-118, nvn-122, nvn2-1 |
| `nvnTextureBuilderGetStride` | 55, 231 | nvn-77 |
| `nvnTextureBuilderGetTarget` | 55, 231 | nvn-54 |
| `nvnTextureBuilderGetWidth` | 55, 231 | nvn-54 |
| `nvnTextureBuilderGetZCullStorageSize` | 55, 231 | nvn-78 |
| `nvnTextureBuilderSetDefaults` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-54, nvn-56, nvn-77, nvn-78, nvn-81, nvn-85, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-103, nvn-104, nvn-118, nvn-122, nvn2-1 |
| `nvnTextureBuilderSetDepth` | 55, 231 | nvn-54, nvn-56, nvn-81 |
| `nvnTextureBuilderSetDepthStencilMode` | 55, 231 | nvn-77 |
| `nvnTextureBuilderSetDevice` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-54, nvn-56, nvn-77, nvn-78, nvn-81, nvn-85, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-103, nvn-104, nvn-118, nvn-122, nvn2-1 |
| `nvnTextureBuilderSetFlags` | 55, 231 | nvn-44, nvn-47, nvn-54, nvn-56, nvn-78, nvn-81, nvn-85, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122, nvn2-1 |
| `nvnTextureBuilderSetFormat` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-54, nvn-56, nvn-78, nvn-81, nvn-85, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-103, nvn-104, nvn-118, nvn-122, nvn2-1 |
| `nvnTextureBuilderSetHeight` | 55, 231 | nvn-54 |
| `nvnTextureBuilderSetLevels` | 55, 231 | nvn-54, nvn-56 |
| `nvnTextureBuilderSetPackagedTextureData` | 55, 231 | nvn-77 |
| `nvnTextureBuilderSetPackagedTextureLayout` | 55, 231 | nvn-77 |
| `nvnTextureBuilderSetSamples` | 55, 231 | nvn-54, nvn-56 |
| `nvnTextureBuilderSetSize1D` | 55, 231 | nvn-81 |
| `nvnTextureBuilderSetSize2D` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-54, nvn-56, nvn-78, nvn-81, nvn-85, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-103, nvn-104, nvn-118, nvn-122, nvn2-1 |
| `nvnTextureBuilderSetSize3D` | 55, 231 | nvn-81 |
| `nvnTextureBuilderSetStorage` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-54, nvn-56, nvn-78, nvn-81, nvn-85, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-103, nvn-104, nvn-118, nvn-122, nvn2-1 |
| `nvnTextureBuilderSetStride` | 55, 231 | nvn-77 |
| `nvnTextureBuilderSetSwizzle` | 55, 231 | nvn-77, nvn-78 |
| `nvnTextureBuilderSetTarget` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-54, nvn-56, nvn-78, nvn-81, nvn-85, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-103, nvn-104, nvn-118, nvn-122, nvn2-1 |
| `nvnTextureBuilderSetWidth` | 55, 231 | nvn-54 |
| `nvnTextureCompare` | 55, 231 | nvn-78 |
| `nvnTextureFinalize` | 55, 231 | nvn-21, nvn-22, nvn-78, nvn-81, nvn-85, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-103, nvn-104, nvn-118, nvn-122 |
| `nvnTextureFlushTexels` | 55, 231 | nvn-79 |
| `nvnTextureGetDepth` | 55, 231 | nvn-56 |
| `nvnTextureGetDepthStencilMode` | 55, 231 | nvn-78 |
| `nvnTextureGetFlags` | 55, 231 | nvn-56 |
| `nvnTextureGetFormat` | 55, 231 | nvn-56 |
| `nvnTextureGetHeight` | 55, 231 | nvn-56 |
| `nvnTextureGetLevels` | 55, 231 | nvn-56 |
| `nvnTextureGetMemoryOffset` | 55, 231 | nvn-56 |
| `nvnTextureGetMemoryPool` | 55, 231 | nvn-56 |
| `nvnTextureGetSamples` | 55, 231 | nvn-56 |
| `nvnTextureGetStorageClass` | 55, 231 | nvn-78 |
| `nvnTextureGetStorageSize` | 55, 231 | nvn-78 |
| `nvnTextureGetStride` | 55, 231 | nvn-78 |
| `nvnTextureGetSwizzle` | 55, 231 | nvn-78 |
| `nvnTextureGetTarget` | 55, 231 | nvn-56 |
| `nvnTextureGetTextureAddress` | 55, 231 | nvn-78 |
| `nvnTextureGetViewOffset` | 55, 231 | nvn-78 |
| `nvnTextureGetWidth` | 55, 231 | nvn-56, nvn-83 |
| `nvnTextureGetZCullStorageSize` | 55, 231 | nvn-78 |
| `nvnTextureInitialize` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-56, nvn-78, nvn-81, nvn-85, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-103, nvn-104, nvn-118, nvn-122, nvn2-1 |
| `nvnTexturePoolFinalize` | 55, 231 | nvn-69, nvn-83, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnTexturePoolGetMemoryOffset` | 55, 231 | nvn-69 |
| `nvnTexturePoolGetMemoryPool` | 55, 231 | nvn-69 |
| `nvnTexturePoolGetSize` | 55, 231 | nvn-69, nvn-83 |
| `nvnTexturePoolInitialize` | 55, 231 | nvn-69, nvn-83, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94 |
| `nvnTexturePoolRegisterImage` | 231 |  |
| `nvnTexturePoolRegisterTexture` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnTextureReadTexels` | 55, 231 | nvn-79 |
| `nvnTextureViewGetDepthStencilMode` | 55, 231 | nvn-77 |
| `nvnTextureViewGetFormat` | 55, 231 | nvn-55 |
| `nvnTextureViewGetLayers` | 55, 231 | nvn-55 |
| `nvnTextureViewGetLevels` | 55, 231 | nvn-55 |
| `nvnTextureViewGetSwizzle` | 55, 231 | nvn-77 |
| `nvnTextureViewGetTarget` | 55, 231 | nvn-55 |
| `nvnTextureViewSetDefaults` | 55, 231 | nvn-55, nvn-57, nvn-77 |
| `nvnTextureViewSetDepthStencilMode` | 55, 231 | nvn-77 |
| `nvnTextureViewSetFormat` | 55, 231 | nvn-55 |
| `nvnTextureViewSetLayers` | 55, 231 | nvn-55 |
| `nvnTextureViewSetLevels` | 55, 231 | nvn-55, nvn-57 |
| `nvnTextureViewSetSwizzle` | 55, 231 | nvn-77 |
| `nvnTextureViewSetTarget` | 55, 231 | nvn-55 |
| `nvnTextureWriteTexels` | 55, 231 | nvn-79 |
| `nvnTextureWriteTexelsStrided` | 55, 231 | nvn-80 |

## Samplers

45 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnSamplerBuilderGetBorderColor` | 55, 231 | nvn-67, nvn-68 |
| `nvnSamplerBuilderGetBorderColori` | 55, 231 | nvn-67 |
| `nvnSamplerBuilderGetBorderColorui` | 55, 231 | nvn-67 |
| `nvnSamplerBuilderGetCompare` | 55, 231 | nvn-67, nvn-68 |
| `nvnSamplerBuilderGetDevice` | 55, 231 | nvn-67 |
| `nvnSamplerBuilderGetLodBias` | 55, 231 | nvn-67, nvn-68 |
| `nvnSamplerBuilderGetLodClamp` | 55, 231 | nvn-67, nvn-68 |
| `nvnSamplerBuilderGetMaxAnisotropy` | 55, 231 | nvn-67, nvn-68 |
| `nvnSamplerBuilderGetMinMagFilter` | 55, 231 | nvn-67, nvn-68 |
| `nvnSamplerBuilderGetReductionFilter` | 55, 231 | nvn-67, nvn-68 |
| `nvnSamplerBuilderGetWrapMode` | 55, 231 | nvn-67, nvn-68 |
| `nvnSamplerBuilderSetBorderColori` | 55, 231 | nvn-67 |
| `nvnSamplerBuilderSetBorderColorui` | 55, 231 | nvn-67 |
| `nvnSamplerGetBorderColori` | 55, 231 | nvn-68 |
| `nvnSamplerGetBorderColorui` | 55, 231 | nvn-68 |
| `nvnSamplerPoolRegisterSamplerBuilder` | 55, 231 | nvn-69 |
| `nvnSamplerBuilderSetBorderColor` | 55, 231 | nvn-67, nvn-68, nvn-69, nvn-83, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnSamplerBuilderSetCompare` | 55, 231 | nvn-67, nvn-68, nvn-69, nvn-83 |
| `nvnSamplerBuilderSetDefaults` | 55, 231 | nvn-67, nvn-68, nvn-69, nvn-83, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnSamplerBuilderSetDevice` | 55, 231 | nvn-67, nvn-68, nvn-69, nvn-83, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnSamplerBuilderSetLodBias` | 55, 231 | nvn-67, nvn-68, nvn-69, nvn-83 |
| `nvnSamplerBuilderSetLodClamp` | 55, 231 | nvn-67, nvn-68, nvn-69, nvn-83 |
| `nvnSamplerBuilderGetLodSnap` | 55, 231 | nvn-67 |
| `nvnSamplerBuilderSetLodSnap` | 55, 231 | nvn-67, nvn-68, nvn-69, nvn-83 |
| `nvnSamplerBuilderSetMaxAnisotropy` | 55, 231 | nvn-67, nvn-68, nvn-69, nvn-83 |
| `nvnSamplerBuilderSetMinMagFilter` | 55, 231 | nvn-67, nvn-68, nvn-69, nvn-83, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnSamplerBuilderSetReductionFilter` | 55, 231 | nvn-67, nvn-68, nvn-69, nvn-83 |
| `nvnSamplerBuilderSetWrapMode` | 55, 231 | nvn-67, nvn-68, nvn-69, nvn-83, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnSamplerCompare` | 55, 231 | nvn-68 |
| `nvnSamplerFinalize` | 55, 231 | nvn-68, nvn-69, nvn-83, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnSamplerGetBorderColor` | 55, 231 | nvn-68 |
| `nvnSamplerGetCompare` | 55, 231 | nvn-68 |
| `nvnSamplerGetLodBias` | 55, 231 | nvn-68 |
| `nvnSamplerGetLodClamp` | 55, 231 | nvn-68 |
| `nvnSamplerGetMaxAnisotropy` | 55, 231 | nvn-68 |
| `nvnSamplerGetMinMagFilter` | 55, 231 | nvn-68 |
| `nvnSamplerGetReductionFilter` | 55, 231 | nvn-68 |
| `nvnSamplerGetWrapMode` | 55, 231 | nvn-68 |
| `nvnSamplerInitialize` | 55, 231 | nvn-68, nvn-69, nvn-83, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnSamplerPoolFinalize` | 55, 231 | nvn-69, nvn-83, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnSamplerPoolGetMemoryOffset` | 55, 231 | nvn-69 |
| `nvnSamplerPoolGetMemoryPool` | 55, 231 | nvn-69 |
| `nvnSamplerPoolGetSize` | 55, 231 | nvn-69, nvn-83 |
| `nvnSamplerPoolInitialize` | 55, 231 | nvn-69, nvn-83, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94 |
| `nvnSamplerPoolRegisterSampler` | 55, 231 | nvn-69, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |

## Programs

20 functions, 7 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnProgramGetSubtileSize` |  |  |
| `nvnProgramGetScratchBytesPerWarp` |  |  |
| `nvnProgramSetSampleShading` |  |  |
| `nvnProgramSetSubroutineLinkage` |  |  |
| `nvnProgramFinalize` | 55, 231 | nvn-36, nvn-39, nvn-40, nvn-83, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-121, nvn-122 |
| `nvnProgramInitialize` | 55, 231 | nvn-83, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-121, nvn-122 |
| `nvnProgramSetShaders` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnProgramSetShadersExt` | 55, 231 | nvn-121 |
| `nvnProgramBuilderSetDefaults` | 231 | nvn-124, nvn2-2, nvn2-8, nvn2-18 |
| `nvnProgramBuilderSetDevice` | 231 | nvn-124, nvn2-2, nvn2-8, nvn2-18 |
| `nvnProgramBuilderGetDevice` | 231 | nvn-124 |
| `nvnProgramBuilderSetSampleShading` | 231 | nvn-124 |
| `nvnProgramBuilderGetSampleShading` | 231 | nvn-124 |
| `nvnProgramBuilderSetShaders` | 231 | nvn-124, nvn2-2, nvn2-8, nvn2-18 |
| `nvnProgramBuilderGetShaders` | 231 | nvn-124 |
| `nvnProgramBuilderGetShaderCount` | 231 | nvn-124 |
| `nvnProgramBuilderSetSubroutineLinkage` |  |  |
| `nvnProgramBuilderGetSubroutineLinkage` |  |  |
| `nvnProgramBuilderGetSubroutineLinkageCount` |  |  |
| `nvnProgramInitializeFromBuilder` | 231 | nvn2-2, nvn2-8, nvn2-18 |

## State objects

82 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnBlendStateGetAdvancedMode` | 55, 231 | nvn-72 |
| `nvnBlendStateGetAdvancedNormalizedDst` | 55, 231 | nvn-72 |
| `nvnBlendStateGetAdvancedOverlap` | 55, 231 | nvn-72 |
| `nvnBlendStateGetAdvancedPremultipliedSrc` | 55, 231 | nvn-72 |
| `nvnBlendStateSetAdvancedMode` | 55, 231 | nvn-72 |
| `nvnBlendStateSetAdvancedNormalizedDst` | 55, 231 | nvn-72 |
| `nvnBlendStateSetAdvancedOverlap` | 55, 231 | nvn-72 |
| `nvnBlendStateSetAdvancedPremultipliedSrc` | 55, 231 | nvn-72 |
| `nvnMultisampleStateGetSampleLocationsGrid` | 55, 231 | nvn-75 |
| `nvnMultisampleStateSetSampleLocations` | 55, 231 | nvn-75 |
| `nvnBlendStateSetBlendEquation` | 55, 231 | nvn-72 |
| `nvnBlendStateSetBlendFunc` | 55, 231 | nvn-72 |
| `nvnBlendStateSetBlendTarget` | 55, 231 | nvn-72 |
| `nvnBlendStateSetDefaults` | 55, 231 | nvn-72, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnChannelMaskStateSetChannelMask` | 55, 231 | nvn-73 |
| `nvnChannelMaskStateSetDefaults` | 55, 231 | nvn-73, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnColorStateGetBlendEnable` | 55, 231 | nvn-73 |
| `nvnColorStateSetAlphaTest` | 55, 231 | nvn-73 |
| `nvnColorStateSetBlendEnable` | 55, 231 | nvn-73 |
| `nvnColorStateSetDefaults` | 55, 231 | nvn-73, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnColorStateSetLogicOp` | 55, 231 | nvn-73 |
| `nvnDepthStencilStateSetDefaults` | 55, 231 | nvn-74, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnDepthStencilStateSetDepthFunc` | 55, 231 | nvn-74, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnDepthStencilStateSetDepthTestEnable` | 55, 231 | nvn-74, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnDepthStencilStateSetDepthWriteEnable` | 55, 231 | nvn-74, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnDepthStencilStateSetStencilFunc` | 55, 231 | nvn-74 |
| `nvnDepthStencilStateSetStencilOp` | 55, 231 | nvn-74 |
| `nvnDepthStencilStateSetStencilTestEnable` | 55, 231 | nvn-74 |
| `nvnMultisampleStateSetAlphaToCoverageEnable` | 55, 231 | nvn-75 |
| `nvnMultisampleStateSetDefaults` | 55, 231 | nvn-75, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnMultisampleStateSetMultisampleEnable` | 55, 231 | nvn-75 |
| `nvnMultisampleStateSetSamples` | 55, 231 | nvn-75 |
| `nvnPolygonStateSetCullFace` | 55, 231 | nvn-76, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnPolygonStateSetDefaults` | 55, 231 | nvn-76, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnPolygonStateSetFrontFace` | 55, 231 | nvn-76, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnPolygonStateSetPolygonMode` | 55, 231 | nvn-76 |
| `nvnPolygonStateSetPolygonOffsetEnables` | 55, 231 | nvn-76 |
| `nvnVertexAttribStateSetDefaults` | 55, 231 | nvn-76, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnVertexAttribStateSetFormat` | 55, 231 | nvn-76, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnVertexAttribStateSetStreamIndex` | 55, 231 | nvn-76, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnVertexStreamStateSetDefaults` | 55, 231 | nvn-76, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnVertexStreamStateSetDivisor` | 55, 231 | nvn-76, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnVertexStreamStateSetStride` | 55, 231 | nvn-76, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnBlendStateGetBlendEquation` | 55, 231 | nvn-72 |
| `nvnBlendStateGetBlendFunc` | 55, 231 | nvn-72 |
| `nvnBlendStateGetBlendTarget` | 55, 231 | nvn-72 |
| `nvnChannelMaskStateGetChannelMask` | 55, 231 | nvn-73 |
| `nvnColorStateGetAlphaTest` | 55, 231 | nvn-73 |
| `nvnColorStateGetLogicOp` | 55, 231 | nvn-73 |
| `nvnDepthStencilStateGetDepthFunc` | 55, 231 | nvn-74 |
| `nvnDepthStencilStateGetDepthTestEnable` | 55, 231 | nvn-74 |
| `nvnDepthStencilStateGetDepthWriteEnable` | 55, 231 | nvn-74 |
| `nvnDepthStencilStateGetStencilFunc` | 55, 231 | nvn-74 |
| `nvnDepthStencilStateGetStencilOp` | 55, 231 | nvn-74 |
| `nvnDepthStencilStateGetStencilTestEnable` | 55, 231 | nvn-74 |
| `nvnMultisampleStateGetAlphaToCoverageDither` | 55, 231 | nvn-75 |
| `nvnMultisampleStateGetAlphaToCoverageEnable` | 55, 231 | nvn-75 |
| `nvnMultisampleStateGetCoverageModulationMode` | 55, 231 | nvn-75 |
| `nvnMultisampleStateGetCoverageToColorEnable` | 55, 231 | nvn-75 |
| `nvnMultisampleStateGetCoverageToColorOutput` | 55, 231 | nvn-75 |
| `nvnMultisampleStateGetMultisampleEnable` | 55, 231 | nvn-75 |
| `nvnMultisampleStateGetRasterSamples` | 55, 231 | nvn-75 |
| `nvnMultisampleStateGetSampleLocationsEnable` | 55, 231 | nvn-75 |
| `nvnMultisampleStateGetSampleLocationsGridEnable` | 55, 231 | nvn-75 |
| `nvnMultisampleStateGetSamples` | 55, 231 | nvn-75 |
| `nvnMultisampleStateSetAlphaToCoverageDither` | 55, 231 | nvn-75 |
| `nvnMultisampleStateSetCoverageModulationMode` | 55, 231 | nvn-75 |
| `nvnMultisampleStateSetCoverageToColorEnable` | 55, 231 | nvn-75 |
| `nvnMultisampleStateSetCoverageToColorOutput` | 55, 231 | nvn-75 |
| `nvnMultisampleStateSetRasterSamples` | 55, 231 | nvn-75 |
| `nvnMultisampleStateSetCoverageTruncationEnable` | 55, 231 | nvn-75 |
| `nvnMultisampleStateGetCoverageTruncationEnable` | 55, 231 | nvn-75 |
| `nvnMultisampleStateSetSampleLocationsEnable` | 55, 231 | nvn-75 |
| `nvnMultisampleStateSetSampleLocationsGridEnable` | 55, 231 | nvn-75 |
| `nvnPolygonStateGetCullFace` | 55, 231 | nvn-76 |
| `nvnPolygonStateGetFrontFace` | 55, 231 | nvn-76 |
| `nvnPolygonStateGetPolygonMode` | 55, 231 | nvn-76 |
| `nvnPolygonStateGetPolygonOffsetEnables` | 55, 231 | nvn-76 |
| `nvnVertexAttribStateGetFormat` | 55, 231 | nvn-76 |
| `nvnVertexAttribStateGetStreamIndex` | 55, 231 | nvn-76 |
| `nvnVertexStreamStateGetDivisor` | 55, 231 | nvn-76 |
| `nvnVertexStreamStateGetStride` | 55, 231 | nvn-76 |

## Queues

30 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnQueueBuilderGetCommandFlushThreshold` | 55, 231 | nvn-65 |
| `nvnQueueBuilderGetCommandMemorySize` | 55, 231 | nvn-65 |
| `nvnQueueBuilderGetComputeMemorySize` | 55, 231 | nvn-65 |
| `nvnQueueBuilderGetControlMemorySize` | 55, 231 | nvn-65 |
| `nvnQueueBuilderGetDevice` | 55, 231 | nvn-65 |
| `nvnQueueBuilderGetFlags` | 55, 231 | nvn-65 |
| `nvnQueueBuilderGetMemory` | 55, 231 | nvn-82 |
| `nvnQueueBuilderGetMemorySize` | 55, 231 | nvn-82 |
| `nvnQueueGetTotalCommandMemoryUsed` | 55, 231 | nvn-82 |
| `nvnQueueGetTotalComputeMemoryUsed` | 55, 231 | nvn-82 |
| `nvnQueueGetTotalControlMemoryUsed` | 55, 231 | nvn-82 |
| `nvnQueueResetMemoryUsageCounts` | 55, 231 | nvn-82 |
| `nvnQueueBuilderGetQueueMemorySize` | 55, 231 | nvn-82 |
| `nvnQueueBuilderSetCommandFlushThreshold` | 55, 231 | nvn-65 |
| `nvnQueueBuilderSetCommandMemorySize` | 55, 231 | nvn-65 |
| `nvnQueueBuilderSetComputeMemorySize` | 55, 231 | nvn-65 |
| `nvnQueueBuilderSetControlMemorySize` | 55, 231 | nvn-65 |
| `nvnQueueBuilderSetDefaults` | 55, 231 | nvn-44, nvn-65, nvn-82, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn2-1 |
| `nvnQueueBuilderSetDevice` | 55, 231 | nvn-44, nvn-65, nvn-82, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn2-1 |
| `nvnQueueBuilderSetFlags` | 55, 231 | nvn-65 |
| `nvnQueueBuilderGetQueuePriority` | 55, 231 | nvn-82 |
| `nvnQueueBuilderSetQueuePriority` | 55, 231 | nvn-82 |
| `nvnQueueBuilderSetQueueMemory` | 55, 231 | nvn-82 |
| `nvnQueueFinalize` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnQueueFinish` | 55, 231 | nvn-45, nvn-46, nvn-48, nvn-49, nvn-50, nvn-51, nvn-52, nvn-53, nvn-57, nvn-59, nvn-60, nvn-61, nvn-62, nvn-64, nvn-71, nvn-79, nvn-80, nvn-81, nvn-82, nvn-85, nvn-86, nvn-87, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122, nvn-123, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18 |
| `nvnQueueFlush` | 55, 231 | nvn-60, nvn-61, nvn-62 |
| `nvnQueueGetError` | 55, 231 | nvn-64, nvn-83 |
| `nvnQueueInitialize` | 55, 231 | nvn-44, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn2-1 |
| `nvnQueueSubmitCommands` | 55, 231 | nvn-45, nvn-46, nvn-48, nvn-49, nvn-50, nvn-51, nvn-52, nvn-53, nvn-57, nvn-59, nvn-60, nvn-61, nvn-62, nvn-71, nvn-79, nvn-81, nvn-85, nvn-86, nvn-87, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122, nvn-123, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18 |
| `nvnQueueBuilderSetPrimaryQueue` | 55, 231 | nvn-82 |

## Sync and events

24 functions, 6 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnSyncCreateGLSync` |  |  |
| `nvnSyncInitializeFromFencedGLSync` |  |  |
| `nvnSyncFinalize` | 55, 231 | nvn-60, nvn-61, nvn-62, nvn-63, nvn-83, nvn-87 |
| `nvnSyncInitialize` | 55, 231 | nvn-60, nvn-61, nvn-62, nvn-63, nvn-83, nvn-87 |
| `nvnSyncWait` | 55, 231 | nvn-60, nvn-61, nvn-62, nvn-63, nvn-87 |
| `nvnSyncGetNvRmSyncNVX` |  |  |
| `nvnSyncInitializeFromNvRmSyncNVX` |  |  |
| `nvnEventBuilderGetMemoryOffset` | 55, 231 | nvn-66 |
| `nvnEventBuilderGetMemoryPool` | 55, 231 | nvn-66 |
| `nvnEventBuilderGetStorage` | 55, 231 | nvn-66 |
| `nvnEventGetMemoryOffset` | 55, 231 | nvn-66 |
| `nvnEventGetMemoryPool` | 55, 231 | nvn-66 |
| `nvnEventBuilderSetDefaults` | 55, 231 | nvn-66 |
| `nvnEventBuilderSetStorage` | 55, 231 | nvn-66 |
| `nvnEventFinalize` | 55, 231 | nvn-66 |
| `nvnEventGetValue` | 55, 231 | nvn-66 |
| `nvnEventInitialize` | 55, 231 | nvn-66 |
| `nvnEventSignal` | 55, 231 | nvn-66 |
| `nvnQueueFenceSync` | 55, 231 | nvn-60, nvn-62 |
| `nvnQueueWaitSync` | 55, 231 | nvn-60, nvn-61, nvn-62 |
| `nvnCommandBufferFenceSync` | 55, 231 | nvn-61 |
| `nvnCommandBufferSignalEvent` |  |  |
| `nvnCommandBufferWaitEvent` |  |  |
| `nvnCommandBufferWaitSync` | 55, 231 | nvn-62 |

## Windows and present

35 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnQueueAcquireTexture` | 55, 231 | nvn-85, nvn-86 |
| `nvnQueuePresentTexture` | 55, 231 | nvn-85, nvn-86, nvn-87 |
| `nvnWindowBuilderGetDevice` | 55, 231 | nvn-84 |
| `nvnWindowBuilderGetNativeWindow` | 55, 231 | nvn-84 |
| `nvnWindowBuilderGetNumActiveTextures` | 55, 231 | nvn-84 |
| `nvnWindowGetNativeWindow` | 55, 231 | nvn-86 |
| `nvnWindowGetNumActiveTextures` | 55, 231 | nvn-86 |
| `nvnWindowGetNumTextures` | 55, 231 | nvn-86 |
| `nvnWindowSetNumActiveTextures` | 55, 231 | nvn-86 |
| `nvnWindowBuilderGetNumTextures` | 55, 231 | nvn-84 |
| `nvnWindowBuilderGetPresentInterval` | 55, 231 | nvn-84 |
| `nvnWindowBuilderGetTexture` | 55, 231 | nvn-84 |
| `nvnWindowGetCrop` | 55, 231 | nvn-86 |
| `nvnWindowGetPresentInterval` | 55, 231 | nvn-86 |
| `nvnWindowAcquireTexture` | 55, 231 | nvn-87 |
| `nvnWindowBuilderSetDefaults` | 55, 231 | nvn-84, nvn-85, nvn-125 |
| `nvnWindowBuilderSetDevice` | 55, 231 | nvn-84, nvn-85 |
| `nvnWindowBuilderSetNativeWindow` | 55, 231 | nvn-84, nvn-85 |
| `nvnWindowBuilderSetNumActiveTextures` | 55, 231 | nvn-84 |
| `nvnWindowBuilderSetPresentInterval` | 55, 231 | nvn-84 |
| `nvnWindowBuilderSetTextures` | 55, 231 | nvn-84, nvn-85 |
| `nvnWindowBuilderSetColorSpace` | 231 | nvn-125 |
| `nvnWindowBuilderGetColorSpace` | 231 | nvn-125 |
| `nvnWindowBuilderSetHdrDisplayMetadata` | 231 | nvn-125 |
| `nvnWindowBuilderGetHdrDisplayMetadata` | 231 | nvn-125 |
| `nvnWindowBuilderSetHdrContentMetadata` | 231 | nvn-125 |
| `nvnWindowBuilderGetHdrContentMetadata` | 231 | nvn-125 |
| `nvnWindowSetHdrDisplayMetadata` | 231 | nvn-126 |
| `nvnWindowGetHdrDisplayMetadata` | 231 | nvn-126 |
| `nvnWindowSetHdrContentMetadata` | 231 | nvn-126 |
| `nvnWindowGetHdrContentMetadata` | 231 | nvn-126 |
| `nvnWindowFinalize` | 55, 231 | nvn-85 |
| `nvnWindowInitialize` | 55, 231 | nvn-85 |
| `nvnWindowSetCrop` | 55, 231 | nvn-86 |
| `nvnWindowSetPresentInterval` | 55, 231 | nvn-86 |

## Counters and queries

6 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnDeviceGetTimestampInNanoseconds` | 55, 231 | nvn-15, nvn-16, nvn-97, nvn-98 |
| `nvnCommandBufferReportCounter` | 55, 231 | nvn-95, nvn-97, nvn-98, nvn-99 |
| `nvnCommandBufferReportValue` | 55, 231 | nvn-96 |
| `nvnCommandBufferResetCounter` | 55, 231 | nvn-95, nvn-99 |
| `nvnCommandBufferSetRenderEnable` | 55, 231 | nvn-99 |
| `nvnCommandBufferSetRenderEnableConditional` | 55, 231 | nvn-99 |

## Commands: memory and recording

25 functions, 3 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferGetCommandMemoryFree` | 55, 231 | nvn-100 |
| `nvnCommandBufferGetCommandMemorySize` | 55, 231 | nvn-100 |
| `nvnCommandBufferGetControlMemoryFree` | 55, 231 | nvn-100 |
| `nvnCommandBufferGetControlMemorySize` | 55, 231 | nvn-100 |
| `nvnCommandBufferSetControlMemoryCallbackEnabled` | 55, 231 | nvn-101 |
| `nvnCommandBufferAddCommandMemory` | 55, 231 | nvn-44, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-100, nvn-101, nvn2-1 |
| `nvnCommandBufferAddControlMemory` | 55, 231 | nvn-44, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-100, nvn2-1 |
| `nvnCommandBufferBarrier` | 55, 231 | nvn-45, nvn-46, nvn-48, nvn-49, nvn-51, nvn-52, nvn-53, nvn-57, nvn-60, nvn-61, nvn-62, nvn-79, nvn-81, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-103, nvn-104, nvn-118, nvn-122, nvn-123, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18 |
| `nvnCommandBufferBeginRecording` | 55, 231 | nvn-45, nvn-46, nvn-48, nvn-49, nvn-50, nvn-51, nvn-52, nvn-53, nvn-57, nvn-59, nvn-60, nvn-61, nvn-62, nvn-71, nvn-79, nvn-81, nvn-85, nvn-86, nvn-87, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122, nvn-123, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18 |
| `nvnCommandBufferSetRecordingMode` |  |  |
| `nvnCommandBufferSetComputeOverlapMode` |  |  |
| `nvnCommandBufferCallCommands` | 55, 231 | nvn-102 |
| `nvnCommandBufferCopyCommands` | 55, 231 | nvn-102 |
| `nvnCommandBufferEndRecording` | 55, 231 | nvn-45, nvn-46, nvn-48, nvn-49, nvn-50, nvn-51, nvn-52, nvn-53, nvn-57, nvn-59, nvn-60, nvn-61, nvn-62, nvn-71, nvn-79, nvn-81, nvn-85, nvn-86, nvn-87, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-102, nvn-118, nvn-122, nvn-123, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18 |
| `nvnCommandBufferFinalize` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnCommandBufferGetCommandMemoryUsed` | 55, 231 | nvn-100 |
| `nvnCommandBufferGetControlMemoryUsed` | 55, 231 | nvn-100 |
| `nvnCommandBufferGetMemoryCallback` | 55, 231 | nvn-101 |
| `nvnCommandBufferGetMemoryCallbackData` | 55, 231 | nvn-101 |
| `nvnCommandBufferInitialize` | 55, 231 | nvn-44, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn2-1 |
| `nvnCommandBufferIsRecording` | 55, 231 | nvn-100 |
| `nvnCommandBufferSetCommandMemoryCallbackEnabled` | 55, 231 | nvn-101 |
| `nvnCommandBufferSetComputeWorkloadPriority` |  |  |
| `nvnCommandBufferSetMemoryCallback` | 55, 231 | nvn-101 |
| `nvnCommandBufferSetMemoryCallbackData` | 55, 231 | nvn-101 |

## Commands: binds

41 functions, 16 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferBindCoverageModulationTable` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnCommandBufferBindImagesDeferred` |  |  |
| `nvnCommandBufferBindSeparateSamplersDeferred` |  |  |
| `nvnCommandBufferBindSeparateTexturesDeferred` |  |  |
| `nvnCommandBufferBindStorageBuffersDeferred` | 231 | nvn2-6 |
| `nvnCommandBufferBindUniformBuffersDeferred` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnCommandBufferSetProgramSubroutines` |  |  |
| `nvnCommandBufferBindShadingRateImage` |  |  |
| `nvnCommandBufferShadingRateImagePalette` |  |  |
| `nvnCommandBufferPushGraphicsConstants` | 231 |  |
| `nvnCommandBufferPushComputeConstants` | 231 |  |
| `nvnCommandBufferBindBlendState` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferBindChannelMaskState` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferBindColorState` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferBindDepthStencilState` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferBindImage` | 231 |  |
| `nvnCommandBufferBindImages` |  |  |
| `nvnCommandBufferBindMultisampleState` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferBindPolygonState` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferBindProgram` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-121, nvn-122, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18 |
| `nvnCommandBufferBindSeparateSampler` |  |  |
| `nvnCommandBufferBindSeparateSamplers` |  |  |
| `nvnCommandBufferBindSeparateTexture` |  |  |
| `nvnCommandBufferBindSeparateTextures` |  |  |
| `nvnCommandBufferBindStorageBuffer` | 231 | nvn2-3, nvn2-4, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18 |
| `nvnCommandBufferBindStorageBuffers` | 231 | nvn2-5 |
| `nvnCommandBufferBindTexture` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnCommandBufferBindTextures` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnCommandBufferBindTexturesDeferred` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnCommandBufferBindTransformFeedbackBuffer` |  |  |
| `nvnCommandBufferBindTransformFeedbackBuffers` |  |  |
| `nvnCommandBufferBindUniformBuffer` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnCommandBufferBindUniformBuffers` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnCommandBufferBindVertexAttribState` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnCommandBufferBindVertexBuffer` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnCommandBufferBindVertexBuffers` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnCommandBufferBindVertexStreamState` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnCommandBufferSetSamplerPool` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnCommandBufferSetShaderScratchMemory` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnCommandBufferSetTexturePool` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnCommandBufferUpdateUniformBuffer` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |

## Commands: draws and dispatches

26 functions, 8 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferDrawTexture` |  |  |
| `nvnCommandBufferBeginTransformFeedback` |  |  |
| `nvnCommandBufferSetGeometryProcessingMode` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferDrawMeshTasks` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferDrawMeshTasks3D` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferDrawMeshTasksIndirect` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferDrawMeshTasks3DIndirect` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferMultiDrawMeshTasksIndirect` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferMultiDrawMeshTasks3DIndirect` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferMultiDrawMeshTasksIndirectCount` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferMultiDrawMeshTasks3DIndirectCount` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferDispatchCompute` | 231 | nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18 |
| `nvnCommandBufferDispatchComputeIndirect` | 231 | nvn2-7 |
| `nvnCommandBufferDrawArrays` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-111, nvn-117, nvn-118, nvn-122 |
| `nvnCommandBufferDrawArraysIndirect` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnCommandBufferDrawArraysInstanced` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnCommandBufferDrawElements` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-112, nvn-118, nvn-122 |
| `nvnCommandBufferDrawElementsBaseVertex` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnCommandBufferDrawElementsIndirect` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnCommandBufferDrawElementsInstanced` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnCommandBufferDrawTransformFeedback` |  |  |
| `nvnCommandBufferEndTransformFeedback` |  |  |
| `nvnCommandBufferMultiDrawArraysIndirectCount` |  |  |
| `nvnCommandBufferMultiDrawElementsIndirectCount` |  |  |
| `nvnCommandBufferPauseTransformFeedback` |  |  |
| `nvnCommandBufferResumeTransformFeedback` |  |  |

## Commands: clears and copies

25 functions, 7 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferClearTexturei` | 55, 231 | nvn-104 |
| `nvnCommandBufferGetCopyImageStride` | 55, 231 | nvn-106 |
| `nvnCommandBufferGetCopyRowStride` | 55, 231 | nvn-106 |
| `nvnCommandBufferResolveDepthBuffer` |  |  |
| `nvnCommandBufferTiledDownsample` |  |  |
| `nvnCommandBufferTiledDownsampleTextureView` |  |  |
| `nvnCommandBufferClearBuffer` | 55, 231 | nvn-105 |
| `nvnCommandBufferClearColor` | 55, 231 | nvn-45, nvn-46, nvn-60, nvn-61, nvn-62, nvn-85, nvn-86, nvn-87, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122, nvn-123, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferClearColori` | 55, 231 | nvn-103 |
| `nvnCommandBufferClearColorui` | 55, 231 | nvn-103 |
| `nvnCommandBufferClearDepthStencil` | 55, 231 | nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferClearTexture` | 55, 231 | nvn-104 |
| `nvnCommandBufferClearTextureui` | 55, 231 | nvn-104 |
| `nvnCommandBufferCopyBufferToBuffer` | 55, 231 | nvn-50, nvn-59, nvn-71, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18 |
| `nvnCommandBufferCopyBufferToTexture` | 55, 231 | nvn-48, nvn-49, nvn-51, nvn-52, nvn-53, nvn-57, nvn-81, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122 |
| `nvnCommandBufferCopyTextureToBuffer` | 55, 231 | nvn-45, nvn-46, nvn-48, nvn-49, nvn-51, nvn-52, nvn-53, nvn-57, nvn-60, nvn-61, nvn-62, nvn-79, nvn-81, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-103, nvn-104, nvn-118, nvn-122, nvn-123, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferCopyTextureToTexture` | 55, 231 | nvn-49, nvn-52 |
| `nvnCommandBufferDiscardColor` | 55, 231 | nvn-107 |
| `nvnCommandBufferDiscardDepthStencil` | 55, 231 | nvn-107 |
| `nvnCommandBufferDownsample` |  |  |
| `nvnCommandBufferDownsampleTextureView` |  |  |
| `nvnCommandBufferRestoreZCullData` |  |  |
| `nvnCommandBufferSaveZCullData` |  |  |
| `nvnCommandBufferSetCopyImageStride` | 55, 231 | nvn-106 |
| `nvnCommandBufferSetCopyRowStride` | 55, 231 | nvn-48, nvn-49, nvn-50, nvn-51, nvn-52, nvn-53, nvn-57, nvn-79, nvn-81, nvn-106 |

## Commands: render state

36 functions, 13 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferOverrideSubtileSize` |  |  |
| `nvnCommandBufferSetColorReductionEnable` |  |  |
| `nvnCommandBufferSetColorReductionThresholds` |  |  |
| `nvnCommandBufferSetConservativeRasterDilate` | 55, 231 | nvn-116 |
| `nvnCommandBufferSetInnerTessellationLevels` | 55, 231 | nvn-118 |
| `nvnCommandBufferSetOuterTessellationLevels` | 55, 231 | nvn-118 |
| `nvnCommandBufferSetProvokingVertexMode` | 55, 231 | nvn-110 |
| `nvnCommandBufferSetSubpixelPrecisionBias` | 55, 231 | nvn-115 |
| `nvnCommandBufferSetZCullZF32CompressionEnable` |  |  |
| `nvnCommandBufferSetShadingRatePaletteMode` |  |  |
| `nvnCommandBufferSetAlphaRef` |  |  |
| `nvnCommandBufferSetBlendColor` |  |  |
| `nvnCommandBufferSetConservativeRasterEnable` | 55, 231 | nvn-115, nvn-116 |
| `nvnCommandBufferSetDepthBounds` | 55, 231 | nvn-109 |
| `nvnCommandBufferSetDepthClamp` | 55, 231 | nvn-110 |
| `nvnCommandBufferSetDepthRange` | 55, 231 | nvn-109 |
| `nvnCommandBufferSetDepthRanges` | 55, 231 | nvn-109 |
| `nvnCommandBufferSetLineWidth` | 55, 231 | nvn-117 |
| `nvnCommandBufferSetPatchSize` | 55, 231 | nvn-118 |
| `nvnCommandBufferSetPointSize` | 55, 231 | nvn-111 |
| `nvnCommandBufferSetPolygonOffsetClamp` |  |  |
| `nvnCommandBufferSetPrimitiveRestart` | 55, 231 | nvn-112 |
| `nvnCommandBufferSetRasterizerDiscard` | 55, 231 | nvn-110 |
| `nvnCommandBufferSetRenderTargets` | 55, 231 | nvn-45, nvn-46, nvn-60, nvn-61, nvn-62, nvn-85, nvn-86, nvn-87, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-103, nvn-118, nvn-122, nvn-123, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferSetSampleMask` |  |  |
| `nvnCommandBufferSetScissor` | 55, 231 | nvn-45, nvn-46, nvn-60, nvn-61, nvn-62, nvn-85, nvn-86, nvn-87, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-103, nvn-118, nvn-122, nvn-123, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferSetScissors` | 55, 231 | nvn-108 |
| `nvnCommandBufferSetStencilCullCriteria` |  |  |
| `nvnCommandBufferSetStencilMask` |  |  |
| `nvnCommandBufferSetStencilRef` |  |  |
| `nvnCommandBufferSetStencilValueMask` |  |  |
| `nvnCommandBufferSetTiledCacheAction` | 55, 231 | nvn-114 |
| `nvnCommandBufferSetTiledCacheTileSize` | 55, 231 | nvn-114 |
| `nvnCommandBufferSetViewport` | 55, 231 | nvn-45, nvn-46, nvn-60, nvn-61, nvn-62, nvn-85, nvn-86, nvn-87, nvn-88, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-118, nvn-122, nvn-123, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferSetViewportSwizzles` | 55, 231 | nvn-113 |
| `nvnCommandBufferSetViewports` | 55, 231 | nvn-108 |

## DLSS

58 functions, 58 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnDlssNetworkBuilderSetDefaults` |  |  |
| `nvnDlssNetworkBuilderSetDevice` |  |  |
| `nvnDlssNetworkBuilderGetDevice` |  |  |
| `nvnDlssNetworkBuilderSetFlags` |  |  |
| `nvnDlssNetworkBuilderGetFlags` |  |  |
| `nvnDlssNetworkBuilderSetMethod` |  |  |
| `nvnDlssNetworkBuilderGetMethod` |  |  |
| `nvnDlssNetworkBuilderSetPreset` |  |  |
| `nvnDlssNetworkBuilderGetPreset` |  |  |
| `nvnDlssNetworkBuilderSetOutputResolution` |  |  |
| `nvnDlssNetworkBuilderGetOutputResolution` |  |  |
| `nvnDlssNetworkBuilderSetMaxEvaluationEntries` |  |  |
| `nvnDlssNetworkBuilderGetMaxEvaluationEntries` |  |  |
| `nvnDlssNetworkBuilderSetStorage` |  |  |
| `nvnDlssNetworkBuilderGetMemory` |  |  |
| `nvnDlssNetworkBuilderGetMemorySize` |  |  |
| `nvnDlssNetworkBuilderSetTextureIDs` |  |  |
| `nvnDlssNetworkBuilderGetTexturePool` |  |  |
| `nvnDlssNetworkBuilderGetTextureIDs` |  |  |
| `nvnDlssNetworkBuilderGetOptimalSettings` |  |  |
| `nvnDlssNetworkBuilderGetStorageSize` |  |  |
| `nvnDlssNetworkInitialize` |  |  |
| `nvnDlssNetworkFinalize` |  |  |
| `nvnDlssNetworkResize` |  |  |
| `nvnCommandBufferEvaluateDlss` |  |  |
| `nvnDlssParametersSetDefaults` |  |  |
| `nvnDlssParametersSetInputColor` |  |  |
| `nvnDlssParametersSetOutputColor` |  |  |
| `nvnDlssParametersSetMotionVectors` |  |  |
| `nvnDlssParametersSetDepth` |  |  |
| `nvnDlssParametersSetBiasCurrentColorMask` |  |  |
| `nvnDlssParametersSetExposureTexture` |  |  |
| `nvnDlssParametersSetJitterOffset` |  |  |
| `nvnDlssParametersGetJitterOffset` |  |  |
| `nvnDlssParametersSetMotionVectorScale` |  |  |
| `nvnDlssParametersGetMotionVectorScale` |  |  |
| `nvnDlssParametersSetPreExposure` |  |  |
| `nvnDlssParametersGetPreExposure` |  |  |
| `nvnDlssParametersSetHistoryReset` |  |  |
| `nvnDlssParametersGetHistoryReset` |  |  |
| `nvnDlssParametersSetEvaluationIndex` |  |  |
| `nvnDlssParametersGetEvaluationIndex` |  |  |
| `nvnDlssParametersSetInputSubrectangleSize` |  |  |
| `nvnDlssParametersGetInputSubrectangleSize` |  |  |
| `nvnDlssParametersSetInputColorSubrectangleBase` |  |  |
| `nvnDlssParametersGetInputColorSubrectangleBase` |  |  |
| `nvnDlssParametersSetDepthSubrectangleBase` |  |  |
| `nvnDlssParametersGetDepthSubrectangleBase` |  |  |
| `nvnDlssParametersSetMotionVectorSubrectangleBase` |  |  |
| `nvnDlssParametersGetMotionVectorSubrectangleBase` |  |  |
| `nvnDlssParametersSetBiasCurrentColorSubrectangleBase` |  |  |
| `nvnDlssParametersGetBiasCurrentColorSubrectangleBase` |  |  |
| `nvnDlssParametersSetOutputSubrectangleBase` |  |  |
| `nvnDlssParametersGetOutputSubrectangleBase` |  |  |
| `nvnDlssParametersSetOverlayDebugLayer` |  |  |
| `nvnDlssParametersSetOverlayFullScreen` |  |  |
| `nvnDlssParametersSetOverlayShowNans` |  |  |
| `nvnDlssParametersSetOverlayJitterDebug` |  |  |

## Ray tracing

15 functions, 9 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnDeviceCalculateAccelerationStructureMemoryRequirements` |  |  |
| `nvnProgramGetShaderGroupHandle` |  |  |
| `nvnProgramBuilderSetRayTracingModules` |  |  |
| `nvnProgramBuilderSetMaxCallableRecursionDepth` | 231 | nvn-124 |
| `nvnProgramBuilderGetMaxCallableRecursionDepth` | 231 | nvn-124 |
| `nvnProgramBuilderSetMaxTraceRecursionDepth` | 231 | nvn-124 |
| `nvnProgramBuilderGetMaxTraceRecursionDepth` | 231 | nvn-124 |
| `nvnProgramBuilderSetProgramRayTracingFlags` | 231 | nvn-124 |
| `nvnProgramBuilderGetProgramRayTracingFlags` | 231 | nvn-124 |
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
| `nvnSetExternalDebuggerEnabledNVX` | 55, 231 | nvn-83 |
| `nvnDeviceGenerateDebugDomainId` | 55, 231 | nvn-83, nvn-119, nvn-123 |
| `nvnDeviceInstallDebugCallback` | 55, 231 | nvn-83 |
| `nvnDeviceIsExternalDebuggerAttached` | 55, 231 | nvn-83 |
| `nvnDeviceSetAutomaticCheckpointsByDebugGroup` | 55, 231 | nvn-83 |
| `nvnDeviceSetAutomaticCheckpointsEnable` | 55, 231 | nvn-83 |
| `nvnDeviceSetAutomaticCheckpointsMask` | 55, 231 | nvn-83 |
| `nvnDeviceSetAutomaticCheckpointsSamplingInterval` | 55, 231 | nvn-83 |
| `nvnDeviceSetCheckpointCallStacksEnable` | 55, 231 | nvn-83 |
| `nvnDeviceSetCheckpointMaxCallStacksDepth` | 55, 231 | nvn-83 |
| `nvnDeviceWalkDebugDatabase` | 55, 231 | nvn-83 |
| `nvnMemoryPoolSetDebugLabel` | 55, 231 | nvn-83 |
| `nvnBufferGetDebugID` | 55, 231 | nvn-83 |
| `nvnBufferSetDebugLabel` | 55, 231 | nvn-83 |
| `nvnTextureGetDebugID` | 55, 231 | nvn-83 |
| `nvnTextureSetDebugLabel` | 55, 231 | nvn-83 |
| `nvnSamplerGetDebugID` | 55, 231 | nvn-83 |
| `nvnSamplerSetDebugLabel` | 55, 231 | nvn-83 |
| `nvnProgramBuilderSetDebugDataHashes` |  |  |
| `nvnProgramBuilderGetDebugDataHashes` |  |  |
| `nvnCommandBufferInsertCheckpointDynamic` | 55, 231 | nvn-119 |
| `nvnCommandBufferInsertCheckpointStatic` | 55, 231 | nvn-119 |
| `nvnCommandBufferInsertDebugMarkerGraphics` | 231 | nvn-123 |
| `nvnCommandBufferInsertDebugMarkerCompute` | 231 | nvn-123 |
| `nvnCommandBufferPushDebugGroupGraphics` | 231 | nvn-123 |
| `nvnCommandBufferPushDebugGroupCompute` | 231 | nvn-123 |
| `nvnCommandBufferPopDebugGroupGraphics` | 231 | nvn-123 |
| `nvnCommandBufferPopDebugGroupCompute` | 231 | nvn-123 |
| `nvnCommandBufferInsertDebugMarker` | 55, 231 | nvn-119 |
| `nvnCommandBufferInsertDebugMarkerDynamic` | 55, 231 | nvn-119 |
| `nvnCommandBufferInsertDebugMarkerStatic` | 55, 231 | nvn-119 |
| `nvnCommandBufferPopDebugGroup` | 55, 231 | nvn-119 |
| `nvnCommandBufferPopDebugGroupId` | 55, 231 | nvn-119 |
| `nvnCommandBufferPushDebugGroup` | 55, 231 | nvn-119 |
| `nvnCommandBufferPushDebugGroupDynamic` | 55, 231 | nvn-119 |
| `nvnCommandBufferPushDebugGroupStatic` | 55, 231 | nvn-119 |
| `nvnCommandBufferSetDebugLabel` | 55, 231 | nvn-83 |
| `nvnDeviceSetDebugLabel` | 55, 231 | nvn-83 |
| `nvnProgramSetDebugLabel` | 55, 231 | nvn-83 |
| `nvnQueueSetDebugLabel` | 55, 231 | nvn-83 |
| `nvnSamplerPoolSetDebugLabel` | 55, 231 | nvn-83 |
| `nvnSyncSetDebugLabel` | 55, 231 | nvn-83 |
| `nvnTexturePoolSetDebugLabel` | 55, 231 | nvn-83 |
| `nvnWindowSetDebugLabel` | 55, 231 | nvn-86 |
