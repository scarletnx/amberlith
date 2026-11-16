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
names 677; the layer has 489 of them in its API 55 build and 539 in its API 231 build; a case calls 534. The
other 143 have no call at all: a runtime could answer any of them wrongly and every case would still pass.

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
| [Memory pools](#memory-pools) | 18 | 17 | 17 | 17 | 1 |
| [Buffers](#buffers) | 16 | 16 | 16 | 16 | 0 |
| [Textures](#textures) | 95 | 94 | 95 | 94 | 1 |
| [Samplers](#samplers) | 45 | 45 | 45 | 45 | 0 |
| [Programs](#programs) | 20 | 4 | 13 | 13 | 7 |
| [State objects](#state-objects) | 82 | 82 | 82 | 82 | 0 |
| [Queues](#queues) | 30 | 30 | 30 | 30 | 0 |
| [Sync and events](#sync-and-events) | 24 | 20 | 20 | 20 | 4 |
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
| **All** | **677** | **489** | **539** | **534** | **143** |

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
| nvn-83 | nvn | a list writes an event's value over the one the processor signalled | the NRO, the Switch 2 program |
| nvn-84 | nvn | a list adds to an event's value | the NRO, the Switch 2 program |
| nvn-85 | nvn | a list that waits for an event's value runs when the processor signals it, and not before | the NRO, the Switch 2 program |
| nvn-86 | nvn | a virtual pool's page is the physical pool's page mapped into it | the NRO, the Switch 2 program |
| nvn-87 | nvn | the debug layer's calls are answered as a release driver answers them, and a label changes no object | the NRO, the Switch 2 program |
| nvn-88 | nvn | a window builder gives back what it was set | the NRO, the Switch 2 program |
| nvn-89 | nvn | a window acquires and presents both its textures | the NRO, the Switch 2 program |
| nvn-90 | nvn | a window says what it is of, and keeps the active textures, the interval and the crop it is set | the NRO, the Switch 2 program |
| nvn-91 | nvn | a texture the window hands with a sync is free once the sync signals, and presents | the NRO, the Switch 2 program |
| nvn-92 | nvn | the same scene through the NVN the program looks up holds what the processor works out | the NRO |
| nvn-93 | nvn | the same texture past its edges through the NVN the program looks up does the same | the NRO |
| nvn-94 | nvn | the same four indexed quads through the NVN the program looks up are each in its quarter | the NRO |
| nvn-95 | nvn | the scene of five draws bound through the looked up calls that take several holds what the processor works out | the NRO |
| nvn-96 | nvn | the scene of five draws bound through the looked up deferred calls holds what the processor works out | the NRO |
| nvn-97 | nvn | the same four quads of an updated uniform buffer through the NVN the program looks up are each its own colour | the NRO |
| nvn-98 | nvn | the same eight instances through the NVN the program looks up are each in its cell | the NRO |
| nvn-99 | nvn | the samples counter reports a quad's 256 pixels and none after a reset, through the NVN the program looks up | the NRO |
| nvn-100 | nvn | two values a list reports are in memory once it has run, through the NVN the program looks up | the NRO |
| nvn-101 | nvn | two timestamp reports hold no count and times between the device's clock before and after, through the NVN the program looks up | the NRO |
| nvn-102 | nvn | a timestamp report's word is the graphics unit's ticks, 384 every 625 nanoseconds, through the NVN the program looks up | the NRO |
| nvn-103 | nvn | the render enable draws under on and under a condition that holds and skips the rest, through the NVN the program looks up | the NRO |
| nvn-104 | nvn | a command buffer's memory used and free come to what it was given, and draws take some, through the NVN the program looks up | the NRO |
| nvn-105 | nvn | a list out of command memory asks the program's callback and draws on, through the NVN the program looks up | the NRO |
| nvn-106 | nvn | a list called from another and a list copied into it draw their cells, through the NVN the program looks up | the NRO |
| nvn-107 | nvn | clears of an unsigned and a signed integer target hold the integers given, through the NVN the program looks up | the NRO |
| nvn-108 | nvn | a clear of a texture that is no target reaches its region and its mask's channels, through the NVN the program looks up | the NRO |
| nvn-109 | nvn | a range of a buffer cleared to a word holds it, and nothing beside the range, through the NVN the program looks up | the NRO |
| nvn-110 | nvn | a command buffer gives back the copy strides it was set, through the NVN the program looks up | the NRO |
| nvn-111 | nvn | targets discarded, then cleared and drawn, hold the clear and the draw, through the NVN the program looks up | the NRO |
| nvn-112 | nvn | a scissor and a viewport set by number bound and place a quad, through the NVN the program looks up | the NRO |
| nvn-113 | nvn | depth ranges move a quad across the depth test and depth bounds test the target's depth, through the NVN the program looks up | the NRO |
| nvn-114 | nvn | the rasterizer discards, clamps depth and takes a flat colour from the provoking vertex, through the NVN the program looks up | the NRO |
| nvn-115 | nvn | a point of sixteen pixels fills its cell, through the NVN the program looks up | the NRO |
| nvn-116 | nvn | a strip with a restart index is two quads, through the NVN the program looks up | the NRO |
| nvn-117 | nvn | a viewport swizzle of x and y turns a quad about the diagonal, through the NVN the program looks up | the NRO |
| nvn-118 | nvn | draws around the tiled cache's actions are as without them, through the NVN the program looks up | the NRO |
| nvn-119 | nvn | conservative rasterization draws every pixel a quad touches, through the NVN the program looks up | the NRO |
| nvn-120 | nvn | a conservative quad dilated by half a pixel reaches the pixels a third of one away, through the NVN the program looks up | the NRO |
| nvn-121 | nvn | a line of width four is four rows of pixels, through the NVN the program looks up | the NRO |
| nvn-122 | nvn | patches of the size and the outer levels a list sets fill their cells, through the NVN the program looks up | the NRO |
| nvn-123 | nvn | draws between debug markers, groups and checkpoints are as without them, through the NVN the program looks up | the NRO |
| nvn-124 | nvn | the device says its modes and its builder its flags, and clear values register, through the NVN the program looks up | the NRO |
| nvn-125 | nvn | a program set from the longer shader data draws, through the NVN the program looks up | the NRO |
| nvn-126 | nvn | a texture's handle and a sampler's put together sample, and a texel fetch handle fetches, through the NVN the program looks up | the NRO |
| nvn-127 | nvn | a clear between the Ounce's graphics and compute markers and groups is the clear | the Switch 2 program |
| nvn-128 | nvn | an Ounce program builder gives back what it was set, a ray tracing program's numbers too | the Switch 2 program |
| nvn-129 | nvn | an Ounce window builder starts with the driver's colour space and HDR metadata, and gives back what it is set | the Switch 2 program |
| nvn-130 | nvn | an Ounce window has its builder's HDR metadata, and gives back what it is set | the Switch 2 program |
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
| `nvnBootstrapLoader` | 55, 231 | nvn-41, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn2-1 |
| `nvnDeviceGetProcAddress` | 55, 231 | nvn-41, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn2-1 |
| `nvnDeviceApplyDeferredFinalizes` |  |  |
| `nvnDeviceBuilderGetFlags` | 55, 231 | nvn-124 |
| `nvnDeviceFinalize` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnDeviceRegisterFastClearColori` | 55, 231 | nvn-124 |
| `nvnDeviceRegisterFastClearColorui` | 55, 231 | nvn-124 |
| `nvnDeviceSetIntermediateShaderCache` |  |  |
| `nvnDeviceBuilderSetDefaults` | 55, 231 | nvn-42, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-124, nvn2-1 |
| `nvnDeviceBuilderSetFlags` | 55, 231 | nvn-42, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-124, nvn2-1 |
| `nvnDeviceGetImageHandle` | 231 |  |
| `nvnDeviceFinalizeCommandHandle` |  |  |
| `nvnDeviceGetCurrentTimestampInNanoseconds` | 55, 231 | nvn-15, nvn-16, nvn-101, nvn-102 |
| `nvnDeviceGetDepthMode` | 55, 231 | nvn-124 |
| `nvnDeviceGetInteger` | 55, 231 | nvn-43, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn2-1 |
| `nvnDeviceGetWindowOriginMode` | 55, 231 | nvn-124 |
| `nvnDeviceGetSeparateSamplerHandle` | 55, 231 | nvn-126 |
| `nvnDeviceGetSeparateTextureHandle` | 55, 231 | nvn-126 |
| `nvnDeviceGetTexelFetchHandle` | 55, 231 | nvn-126 |
| `nvnDeviceGetTextureHandle` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnDeviceInitialize` | 55, 231 | nvn-42, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn2-1 |
| `nvnDeviceRegisterFastClearColor` | 55, 231 | nvn-124 |
| `nvnDeviceRegisterFastClearDepth` | 55, 231 | nvn-124 |
| `nvnDeviceRegisterFastClearStencil` | 55, 231 | nvn-124 |
| `nvnDeviceSetDepthMode` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98 |
| `nvnDeviceSetWindowOriginMode` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98 |
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

18 functions, 1 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnMemoryPoolBuilderGetDevice` | 55, 231 | nvn-58 |
| `nvnMemoryPoolBuilderGetFlags` | 55, 231 | nvn-58 |
| `nvnMemoryPoolBuilderGetMemory` | 55, 231 | nvn-58 |
| `nvnMemoryPoolBuilderGetSize` | 55, 231 | nvn-58 |
| `nvnMemoryPoolBuilderSetDefaults` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-56, nvn-58, nvn-81, nvn-86, nvn-89, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-107, nvn-108, nvn2-1 |
| `nvnMemoryPoolBuilderSetDevice` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-56, nvn-58, nvn-81, nvn-86, nvn-89, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-107, nvn-108, nvn2-1 |
| `nvnMemoryPoolBuilderSetFlags` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-56, nvn-58, nvn-81, nvn-86, nvn-89, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-107, nvn-108, nvn2-1 |
| `nvnMemoryPoolBuilderSetStorage` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-56, nvn-58, nvn-81, nvn-86, nvn-89, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-107, nvn-108, nvn2-1 |
| `nvnMemoryPoolBuilderSetNativeHandleNVX` |  |  |
| `nvnMemoryPoolFinalize` | 55, 231 | nvn-21, nvn-22, nvn-81, nvn-86, nvn-89, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-107, nvn-108, nvn-122, nvn-126 |
| `nvnMemoryPoolFlushMappedRange` | 55, 231 | nvn-59, nvn-71 |
| `nvnMemoryPoolGetBufferAddress` | 55, 231 | nvn-45, nvn-46, nvn-48, nvn-49, nvn-50, nvn-51, nvn-52, nvn-53, nvn-57, nvn-59, nvn-60, nvn-61, nvn-62, nvn-70, nvn-78, nvn-79, nvn-81, nvn-85, nvn-86, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126, nvn-127, nvn2-2, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-8, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18 |
| `nvnMemoryPoolGetFlags` | 55, 231 | nvn-58 |
| `nvnMemoryPoolGetSize` | 55, 231 | nvn-58, nvn-87 |
| `nvnMemoryPoolInitialize` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-56, nvn-58, nvn-81, nvn-86, nvn-89, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-107, nvn-108, nvn2-1 |
| `nvnMemoryPoolInvalidateMappedRange` | 55, 231 | nvn-59 |
| `nvnMemoryPoolMap` | 55, 231 | nvn-45, nvn-46, nvn-47, nvn-48, nvn-49, nvn-50, nvn-51, nvn-52, nvn-53, nvn-57, nvn-58, nvn-59, nvn-60, nvn-61, nvn-62, nvn-66, nvn-69, nvn-70, nvn-71, nvn-79, nvn-81, nvn-85, nvn-86, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126, nvn-127, nvn2-2, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-8, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18 |
| `nvnMemoryPoolMapVirtual` | 55, 231 | nvn-86 |

## Buffers

16 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnBufferBuilderGetDevice` | 55, 231 | nvn-70 |
| `nvnBufferBuilderGetMemoryOffset` | 55, 231 | nvn-70 |
| `nvnBufferBuilderGetMemoryPool` | 55, 231 | nvn-70 |
| `nvnBufferBuilderGetSize` | 55, 231 | nvn-70 |
| `nvnBufferBuilderSetDefaults` | 55, 231 | nvn-70, nvn-71, nvn-87 |
| `nvnBufferBuilderSetDevice` | 55, 231 | nvn-70, nvn-71, nvn-87 |
| `nvnBufferBuilderSetStorage` | 55, 231 | nvn-70, nvn-71, nvn-87 |
| `nvnBufferFinalize` | 55, 231 | nvn-70, nvn-71, nvn-87 |
| `nvnBufferFlushMappedRange` | 55, 231 | nvn-71 |
| `nvnBufferInvalidateMappedRange` | 55, 231 | nvn-71 |
| `nvnBufferGetAddress` | 55, 231 | nvn-70, nvn-71 |
| `nvnBufferGetMemoryOffset` | 55, 231 | nvn-70 |
| `nvnBufferGetMemoryPool` | 55, 231 | nvn-70 |
| `nvnBufferGetSize` | 55, 231 | nvn-70, nvn-87 |
| `nvnBufferInitialize` | 55, 231 | nvn-70, nvn-71, nvn-87 |
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
| `nvnTextureBuilderGetStorageAlignment` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-56, nvn-81, nvn-89, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-107, nvn-108, nvn-122, nvn-126, nvn2-1 |
| `nvnTextureBuilderGetStorageClass` | 55, 231 | nvn-78 |
| `nvnTextureBuilderGetStorageSize` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-56, nvn-78, nvn-81, nvn-89, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-107, nvn-108, nvn-122, nvn-126, nvn2-1 |
| `nvnTextureBuilderGetStride` | 55, 231 | nvn-77 |
| `nvnTextureBuilderGetTarget` | 55, 231 | nvn-54 |
| `nvnTextureBuilderGetWidth` | 55, 231 | nvn-54 |
| `nvnTextureBuilderGetZCullStorageSize` | 55, 231 | nvn-78 |
| `nvnTextureBuilderSetDefaults` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-54, nvn-56, nvn-77, nvn-78, nvn-81, nvn-89, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-107, nvn-108, nvn-122, nvn-126, nvn2-1 |
| `nvnTextureBuilderSetDepth` | 55, 231 | nvn-54, nvn-56, nvn-81 |
| `nvnTextureBuilderSetDepthStencilMode` | 55, 231 | nvn-77 |
| `nvnTextureBuilderSetDevice` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-54, nvn-56, nvn-77, nvn-78, nvn-81, nvn-89, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-107, nvn-108, nvn-122, nvn-126, nvn2-1 |
| `nvnTextureBuilderSetFlags` | 55, 231 | nvn-44, nvn-47, nvn-54, nvn-56, nvn-78, nvn-81, nvn-89, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126, nvn2-1 |
| `nvnTextureBuilderSetFormat` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-54, nvn-56, nvn-78, nvn-81, nvn-89, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-107, nvn-108, nvn-122, nvn-126, nvn2-1 |
| `nvnTextureBuilderSetHeight` | 55, 231 | nvn-54 |
| `nvnTextureBuilderSetLevels` | 55, 231 | nvn-54, nvn-56 |
| `nvnTextureBuilderSetPackagedTextureData` | 55, 231 | nvn-77 |
| `nvnTextureBuilderSetPackagedTextureLayout` | 55, 231 | nvn-77 |
| `nvnTextureBuilderSetSamples` | 55, 231 | nvn-54, nvn-56 |
| `nvnTextureBuilderSetSize1D` | 55, 231 | nvn-81 |
| `nvnTextureBuilderSetSize2D` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-54, nvn-56, nvn-78, nvn-81, nvn-89, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-107, nvn-108, nvn-122, nvn-126, nvn2-1 |
| `nvnTextureBuilderSetSize3D` | 55, 231 | nvn-81 |
| `nvnTextureBuilderSetStorage` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-54, nvn-56, nvn-78, nvn-81, nvn-89, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-107, nvn-108, nvn-122, nvn-126, nvn2-1 |
| `nvnTextureBuilderSetStride` | 55, 231 | nvn-77 |
| `nvnTextureBuilderSetSwizzle` | 55, 231 | nvn-77, nvn-78 |
| `nvnTextureBuilderSetTarget` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-54, nvn-56, nvn-78, nvn-81, nvn-89, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-107, nvn-108, nvn-122, nvn-126, nvn2-1 |
| `nvnTextureBuilderSetWidth` | 55, 231 | nvn-54 |
| `nvnTextureCompare` | 55, 231 | nvn-78 |
| `nvnTextureFinalize` | 55, 231 | nvn-21, nvn-22, nvn-78, nvn-81, nvn-89, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-107, nvn-108, nvn-122, nvn-126 |
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
| `nvnTextureGetWidth` | 55, 231 | nvn-56, nvn-87 |
| `nvnTextureGetZCullStorageSize` | 55, 231 | nvn-78 |
| `nvnTextureInitialize` | 55, 231 | nvn-21, nvn-22, nvn-44, nvn-47, nvn-56, nvn-78, nvn-81, nvn-89, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-107, nvn-108, nvn-122, nvn-126, nvn2-1 |
| `nvnTexturePoolFinalize` | 55, 231 | nvn-69, nvn-87, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnTexturePoolGetMemoryOffset` | 55, 231 | nvn-69 |
| `nvnTexturePoolGetMemoryPool` | 55, 231 | nvn-69 |
| `nvnTexturePoolGetSize` | 55, 231 | nvn-69, nvn-87 |
| `nvnTexturePoolInitialize` | 55, 231 | nvn-69, nvn-87, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98 |
| `nvnTexturePoolRegisterImage` | 231 |  |
| `nvnTexturePoolRegisterTexture` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
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
| `nvnSamplerBuilderSetBorderColor` | 55, 231 | nvn-67, nvn-68, nvn-69, nvn-87, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnSamplerBuilderSetCompare` | 55, 231 | nvn-67, nvn-68, nvn-69, nvn-87 |
| `nvnSamplerBuilderSetDefaults` | 55, 231 | nvn-67, nvn-68, nvn-69, nvn-87, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnSamplerBuilderSetDevice` | 55, 231 | nvn-67, nvn-68, nvn-69, nvn-87, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnSamplerBuilderSetLodBias` | 55, 231 | nvn-67, nvn-68, nvn-69, nvn-87 |
| `nvnSamplerBuilderSetLodClamp` | 55, 231 | nvn-67, nvn-68, nvn-69, nvn-87 |
| `nvnSamplerBuilderGetLodSnap` | 55, 231 | nvn-67 |
| `nvnSamplerBuilderSetLodSnap` | 55, 231 | nvn-67, nvn-68, nvn-69, nvn-87 |
| `nvnSamplerBuilderSetMaxAnisotropy` | 55, 231 | nvn-67, nvn-68, nvn-69, nvn-87 |
| `nvnSamplerBuilderSetMinMagFilter` | 55, 231 | nvn-67, nvn-68, nvn-69, nvn-87, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnSamplerBuilderSetReductionFilter` | 55, 231 | nvn-67, nvn-68, nvn-69, nvn-87 |
| `nvnSamplerBuilderSetWrapMode` | 55, 231 | nvn-67, nvn-68, nvn-69, nvn-87, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnSamplerCompare` | 55, 231 | nvn-68 |
| `nvnSamplerFinalize` | 55, 231 | nvn-68, nvn-69, nvn-87, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnSamplerGetBorderColor` | 55, 231 | nvn-68 |
| `nvnSamplerGetCompare` | 55, 231 | nvn-68 |
| `nvnSamplerGetLodBias` | 55, 231 | nvn-68 |
| `nvnSamplerGetLodClamp` | 55, 231 | nvn-68 |
| `nvnSamplerGetMaxAnisotropy` | 55, 231 | nvn-68 |
| `nvnSamplerGetMinMagFilter` | 55, 231 | nvn-68 |
| `nvnSamplerGetReductionFilter` | 55, 231 | nvn-68 |
| `nvnSamplerGetWrapMode` | 55, 231 | nvn-68 |
| `nvnSamplerInitialize` | 55, 231 | nvn-68, nvn-69, nvn-87, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnSamplerPoolFinalize` | 55, 231 | nvn-69, nvn-87, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnSamplerPoolGetMemoryOffset` | 55, 231 | nvn-69 |
| `nvnSamplerPoolGetMemoryPool` | 55, 231 | nvn-69 |
| `nvnSamplerPoolGetSize` | 55, 231 | nvn-69, nvn-87 |
| `nvnSamplerPoolInitialize` | 55, 231 | nvn-69, nvn-87, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98 |
| `nvnSamplerPoolRegisterSampler` | 55, 231 | nvn-69, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |

## Programs

20 functions, 7 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnProgramGetSubtileSize` |  |  |
| `nvnProgramGetScratchBytesPerWarp` |  |  |
| `nvnProgramSetSampleShading` |  |  |
| `nvnProgramSetSubroutineLinkage` |  |  |
| `nvnProgramFinalize` | 55, 231 | nvn-36, nvn-39, nvn-40, nvn-87, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-125, nvn-126 |
| `nvnProgramInitialize` | 55, 231 | nvn-87, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-125, nvn-126 |
| `nvnProgramSetShaders` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnProgramSetShadersExt` | 55, 231 | nvn-125 |
| `nvnProgramBuilderSetDefaults` | 231 | nvn-128, nvn2-2, nvn2-8, nvn2-18 |
| `nvnProgramBuilderSetDevice` | 231 | nvn-128, nvn2-2, nvn2-8, nvn2-18 |
| `nvnProgramBuilderGetDevice` | 231 | nvn-128 |
| `nvnProgramBuilderSetSampleShading` | 231 | nvn-128 |
| `nvnProgramBuilderGetSampleShading` | 231 | nvn-128 |
| `nvnProgramBuilderSetShaders` | 231 | nvn-128, nvn2-2, nvn2-8, nvn2-18 |
| `nvnProgramBuilderGetShaders` | 231 | nvn-128 |
| `nvnProgramBuilderGetShaderCount` | 231 | nvn-128 |
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
| `nvnBlendStateSetDefaults` | 55, 231 | nvn-72, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnChannelMaskStateSetChannelMask` | 55, 231 | nvn-73 |
| `nvnChannelMaskStateSetDefaults` | 55, 231 | nvn-73, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnColorStateGetBlendEnable` | 55, 231 | nvn-73 |
| `nvnColorStateSetAlphaTest` | 55, 231 | nvn-73 |
| `nvnColorStateSetBlendEnable` | 55, 231 | nvn-73 |
| `nvnColorStateSetDefaults` | 55, 231 | nvn-73, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnColorStateSetLogicOp` | 55, 231 | nvn-73 |
| `nvnDepthStencilStateSetDefaults` | 55, 231 | nvn-74, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnDepthStencilStateSetDepthFunc` | 55, 231 | nvn-74, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnDepthStencilStateSetDepthTestEnable` | 55, 231 | nvn-74, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnDepthStencilStateSetDepthWriteEnable` | 55, 231 | nvn-74, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnDepthStencilStateSetStencilFunc` | 55, 231 | nvn-74 |
| `nvnDepthStencilStateSetStencilOp` | 55, 231 | nvn-74 |
| `nvnDepthStencilStateSetStencilTestEnable` | 55, 231 | nvn-74 |
| `nvnMultisampleStateSetAlphaToCoverageEnable` | 55, 231 | nvn-75 |
| `nvnMultisampleStateSetDefaults` | 55, 231 | nvn-75, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnMultisampleStateSetMultisampleEnable` | 55, 231 | nvn-75 |
| `nvnMultisampleStateSetSamples` | 55, 231 | nvn-75 |
| `nvnPolygonStateSetCullFace` | 55, 231 | nvn-76, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnPolygonStateSetDefaults` | 55, 231 | nvn-76, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnPolygonStateSetFrontFace` | 55, 231 | nvn-76, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnPolygonStateSetPolygonMode` | 55, 231 | nvn-76 |
| `nvnPolygonStateSetPolygonOffsetEnables` | 55, 231 | nvn-76 |
| `nvnVertexAttribStateSetDefaults` | 55, 231 | nvn-76, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnVertexAttribStateSetFormat` | 55, 231 | nvn-76, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnVertexAttribStateSetStreamIndex` | 55, 231 | nvn-76, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnVertexStreamStateSetDefaults` | 55, 231 | nvn-76, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnVertexStreamStateSetDivisor` | 55, 231 | nvn-76, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnVertexStreamStateSetStride` | 55, 231 | nvn-76, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
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
| `nvnQueueBuilderSetDefaults` | 55, 231 | nvn-44, nvn-65, nvn-82, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn2-1 |
| `nvnQueueBuilderSetDevice` | 55, 231 | nvn-44, nvn-65, nvn-82, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn2-1 |
| `nvnQueueBuilderSetFlags` | 55, 231 | nvn-65 |
| `nvnQueueBuilderGetQueuePriority` | 55, 231 | nvn-82 |
| `nvnQueueBuilderSetQueuePriority` | 55, 231 | nvn-82 |
| `nvnQueueBuilderSetQueueMemory` | 55, 231 | nvn-82 |
| `nvnQueueFinalize` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnQueueFinish` | 55, 231 | nvn-45, nvn-46, nvn-48, nvn-49, nvn-50, nvn-51, nvn-52, nvn-53, nvn-57, nvn-59, nvn-60, nvn-61, nvn-62, nvn-64, nvn-71, nvn-79, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-86, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126, nvn-127, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18 |
| `nvnQueueFlush` | 55, 231 | nvn-60, nvn-61, nvn-62, nvn-85 |
| `nvnQueueGetError` | 55, 231 | nvn-64, nvn-87 |
| `nvnQueueInitialize` | 55, 231 | nvn-44, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn2-1 |
| `nvnQueueSubmitCommands` | 55, 231 | nvn-45, nvn-46, nvn-48, nvn-49, nvn-50, nvn-51, nvn-52, nvn-53, nvn-57, nvn-59, nvn-60, nvn-61, nvn-62, nvn-71, nvn-79, nvn-81, nvn-83, nvn-84, nvn-85, nvn-86, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126, nvn-127, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18 |
| `nvnQueueBuilderSetPrimaryQueue` | 55, 231 | nvn-82 |

## Sync and events

24 functions, 4 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnSyncCreateGLSync` |  |  |
| `nvnSyncInitializeFromFencedGLSync` |  |  |
| `nvnSyncFinalize` | 55, 231 | nvn-60, nvn-61, nvn-62, nvn-63, nvn-85, nvn-87, nvn-91 |
| `nvnSyncInitialize` | 55, 231 | nvn-60, nvn-61, nvn-62, nvn-63, nvn-85, nvn-87, nvn-91 |
| `nvnSyncWait` | 55, 231 | nvn-60, nvn-61, nvn-62, nvn-63, nvn-85, nvn-91 |
| `nvnSyncGetNvRmSyncNVX` |  |  |
| `nvnSyncInitializeFromNvRmSyncNVX` |  |  |
| `nvnEventBuilderGetMemoryOffset` | 55, 231 | nvn-66 |
| `nvnEventBuilderGetMemoryPool` | 55, 231 | nvn-66 |
| `nvnEventBuilderGetStorage` | 55, 231 | nvn-66 |
| `nvnEventGetMemoryOffset` | 55, 231 | nvn-66 |
| `nvnEventGetMemoryPool` | 55, 231 | nvn-66 |
| `nvnEventBuilderSetDefaults` | 55, 231 | nvn-66, nvn-83, nvn-84, nvn-85 |
| `nvnEventBuilderSetStorage` | 55, 231 | nvn-66, nvn-83, nvn-84, nvn-85 |
| `nvnEventFinalize` | 55, 231 | nvn-66, nvn-83, nvn-84, nvn-85 |
| `nvnEventGetValue` | 55, 231 | nvn-66, nvn-83, nvn-84 |
| `nvnEventInitialize` | 55, 231 | nvn-66, nvn-83, nvn-84, nvn-85 |
| `nvnEventSignal` | 55, 231 | nvn-66, nvn-83, nvn-84, nvn-85 |
| `nvnQueueFenceSync` | 55, 231 | nvn-60, nvn-62, nvn-85 |
| `nvnQueueWaitSync` | 55, 231 | nvn-60, nvn-61, nvn-62 |
| `nvnCommandBufferFenceSync` | 55, 231 | nvn-61 |
| `nvnCommandBufferSignalEvent` | 55, 231 | nvn-83, nvn-84 |
| `nvnCommandBufferWaitEvent` | 55, 231 | nvn-85 |
| `nvnCommandBufferWaitSync` | 55, 231 | nvn-62 |

## Windows and present

35 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnQueueAcquireTexture` | 55, 231 | nvn-89, nvn-90 |
| `nvnQueuePresentTexture` | 55, 231 | nvn-89, nvn-90, nvn-91 |
| `nvnWindowBuilderGetDevice` | 55, 231 | nvn-88 |
| `nvnWindowBuilderGetNativeWindow` | 55, 231 | nvn-88 |
| `nvnWindowBuilderGetNumActiveTextures` | 55, 231 | nvn-88 |
| `nvnWindowGetNativeWindow` | 55, 231 | nvn-90 |
| `nvnWindowGetNumActiveTextures` | 55, 231 | nvn-90 |
| `nvnWindowGetNumTextures` | 55, 231 | nvn-90 |
| `nvnWindowSetNumActiveTextures` | 55, 231 | nvn-90 |
| `nvnWindowBuilderGetNumTextures` | 55, 231 | nvn-88 |
| `nvnWindowBuilderGetPresentInterval` | 55, 231 | nvn-88 |
| `nvnWindowBuilderGetTexture` | 55, 231 | nvn-88 |
| `nvnWindowGetCrop` | 55, 231 | nvn-90 |
| `nvnWindowGetPresentInterval` | 55, 231 | nvn-90 |
| `nvnWindowAcquireTexture` | 55, 231 | nvn-91 |
| `nvnWindowBuilderSetDefaults` | 55, 231 | nvn-88, nvn-89, nvn-129 |
| `nvnWindowBuilderSetDevice` | 55, 231 | nvn-88, nvn-89 |
| `nvnWindowBuilderSetNativeWindow` | 55, 231 | nvn-88, nvn-89 |
| `nvnWindowBuilderSetNumActiveTextures` | 55, 231 | nvn-88 |
| `nvnWindowBuilderSetPresentInterval` | 55, 231 | nvn-88 |
| `nvnWindowBuilderSetTextures` | 55, 231 | nvn-88, nvn-89 |
| `nvnWindowBuilderSetColorSpace` | 231 | nvn-129 |
| `nvnWindowBuilderGetColorSpace` | 231 | nvn-129 |
| `nvnWindowBuilderSetHdrDisplayMetadata` | 231 | nvn-129 |
| `nvnWindowBuilderGetHdrDisplayMetadata` | 231 | nvn-129 |
| `nvnWindowBuilderSetHdrContentMetadata` | 231 | nvn-129 |
| `nvnWindowBuilderGetHdrContentMetadata` | 231 | nvn-129 |
| `nvnWindowSetHdrDisplayMetadata` | 231 | nvn-130 |
| `nvnWindowGetHdrDisplayMetadata` | 231 | nvn-130 |
| `nvnWindowSetHdrContentMetadata` | 231 | nvn-130 |
| `nvnWindowGetHdrContentMetadata` | 231 | nvn-130 |
| `nvnWindowFinalize` | 55, 231 | nvn-89 |
| `nvnWindowInitialize` | 55, 231 | nvn-89 |
| `nvnWindowSetCrop` | 55, 231 | nvn-90 |
| `nvnWindowSetPresentInterval` | 55, 231 | nvn-90 |

## Counters and queries

6 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnDeviceGetTimestampInNanoseconds` | 55, 231 | nvn-15, nvn-16, nvn-101, nvn-102 |
| `nvnCommandBufferReportCounter` | 55, 231 | nvn-99, nvn-101, nvn-102, nvn-103 |
| `nvnCommandBufferReportValue` | 55, 231 | nvn-100 |
| `nvnCommandBufferResetCounter` | 55, 231 | nvn-99, nvn-103 |
| `nvnCommandBufferSetRenderEnable` | 55, 231 | nvn-103 |
| `nvnCommandBufferSetRenderEnableConditional` | 55, 231 | nvn-103 |

## Commands: memory and recording

25 functions, 3 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferGetCommandMemoryFree` | 55, 231 | nvn-104 |
| `nvnCommandBufferGetCommandMemorySize` | 55, 231 | nvn-104 |
| `nvnCommandBufferGetControlMemoryFree` | 55, 231 | nvn-104 |
| `nvnCommandBufferGetControlMemorySize` | 55, 231 | nvn-104 |
| `nvnCommandBufferSetControlMemoryCallbackEnabled` | 55, 231 | nvn-105 |
| `nvnCommandBufferAddCommandMemory` | 55, 231 | nvn-44, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-104, nvn-105, nvn2-1 |
| `nvnCommandBufferAddControlMemory` | 55, 231 | nvn-44, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-104, nvn2-1 |
| `nvnCommandBufferBarrier` | 55, 231 | nvn-45, nvn-46, nvn-48, nvn-49, nvn-51, nvn-52, nvn-53, nvn-57, nvn-60, nvn-61, nvn-62, nvn-79, nvn-81, nvn-85, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-107, nvn-108, nvn-122, nvn-126, nvn-127, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18 |
| `nvnCommandBufferBeginRecording` | 55, 231 | nvn-45, nvn-46, nvn-48, nvn-49, nvn-50, nvn-51, nvn-52, nvn-53, nvn-57, nvn-59, nvn-60, nvn-61, nvn-62, nvn-71, nvn-79, nvn-81, nvn-83, nvn-84, nvn-85, nvn-86, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126, nvn-127, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18 |
| `nvnCommandBufferSetRecordingMode` |  |  |
| `nvnCommandBufferSetComputeOverlapMode` |  |  |
| `nvnCommandBufferCallCommands` | 55, 231 | nvn-106 |
| `nvnCommandBufferCopyCommands` | 55, 231 | nvn-106 |
| `nvnCommandBufferEndRecording` | 55, 231 | nvn-45, nvn-46, nvn-48, nvn-49, nvn-50, nvn-51, nvn-52, nvn-53, nvn-57, nvn-59, nvn-60, nvn-61, nvn-62, nvn-71, nvn-79, nvn-81, nvn-83, nvn-84, nvn-85, nvn-86, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-106, nvn-122, nvn-126, nvn-127, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18 |
| `nvnCommandBufferFinalize` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnCommandBufferGetCommandMemoryUsed` | 55, 231 | nvn-104 |
| `nvnCommandBufferGetControlMemoryUsed` | 55, 231 | nvn-104 |
| `nvnCommandBufferGetMemoryCallback` | 55, 231 | nvn-105 |
| `nvnCommandBufferGetMemoryCallbackData` | 55, 231 | nvn-105 |
| `nvnCommandBufferInitialize` | 55, 231 | nvn-44, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn2-1 |
| `nvnCommandBufferIsRecording` | 55, 231 | nvn-104 |
| `nvnCommandBufferSetCommandMemoryCallbackEnabled` | 55, 231 | nvn-105 |
| `nvnCommandBufferSetComputeWorkloadPriority` |  |  |
| `nvnCommandBufferSetMemoryCallback` | 55, 231 | nvn-105 |
| `nvnCommandBufferSetMemoryCallbackData` | 55, 231 | nvn-105 |

## Commands: binds

41 functions, 16 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferBindCoverageModulationTable` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnCommandBufferBindImagesDeferred` |  |  |
| `nvnCommandBufferBindSeparateSamplersDeferred` |  |  |
| `nvnCommandBufferBindSeparateTexturesDeferred` |  |  |
| `nvnCommandBufferBindStorageBuffersDeferred` | 231 | nvn2-6 |
| `nvnCommandBufferBindUniformBuffersDeferred` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnCommandBufferSetProgramSubroutines` |  |  |
| `nvnCommandBufferBindShadingRateImage` |  |  |
| `nvnCommandBufferShadingRateImagePalette` |  |  |
| `nvnCommandBufferPushGraphicsConstants` | 231 |  |
| `nvnCommandBufferPushComputeConstants` | 231 |  |
| `nvnCommandBufferBindBlendState` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferBindChannelMaskState` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferBindColorState` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferBindDepthStencilState` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferBindImage` | 231 |  |
| `nvnCommandBufferBindImages` |  |  |
| `nvnCommandBufferBindMultisampleState` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferBindPolygonState` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferBindProgram` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-125, nvn-126, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18 |
| `nvnCommandBufferBindSeparateSampler` |  |  |
| `nvnCommandBufferBindSeparateSamplers` |  |  |
| `nvnCommandBufferBindSeparateTexture` |  |  |
| `nvnCommandBufferBindSeparateTextures` |  |  |
| `nvnCommandBufferBindStorageBuffer` | 231 | nvn2-3, nvn2-4, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18 |
| `nvnCommandBufferBindStorageBuffers` | 231 | nvn2-5 |
| `nvnCommandBufferBindTexture` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnCommandBufferBindTextures` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnCommandBufferBindTexturesDeferred` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnCommandBufferBindTransformFeedbackBuffer` |  |  |
| `nvnCommandBufferBindTransformFeedbackBuffers` |  |  |
| `nvnCommandBufferBindUniformBuffer` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnCommandBufferBindUniformBuffers` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnCommandBufferBindVertexAttribState` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnCommandBufferBindVertexBuffer` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnCommandBufferBindVertexBuffers` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnCommandBufferBindVertexStreamState` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnCommandBufferSetSamplerPool` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnCommandBufferSetShaderScratchMemory` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnCommandBufferSetTexturePool` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnCommandBufferUpdateUniformBuffer` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |

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
| `nvnCommandBufferDrawArrays` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-115, nvn-121, nvn-122, nvn-126 |
| `nvnCommandBufferDrawArraysIndirect` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnCommandBufferDrawArraysInstanced` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnCommandBufferDrawElements` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-116, nvn-122, nvn-126 |
| `nvnCommandBufferDrawElementsBaseVertex` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnCommandBufferDrawElementsIndirect` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnCommandBufferDrawElementsInstanced` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
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
| `nvnCommandBufferClearTexturei` | 55, 231 | nvn-108 |
| `nvnCommandBufferGetCopyImageStride` | 55, 231 | nvn-110 |
| `nvnCommandBufferGetCopyRowStride` | 55, 231 | nvn-110 |
| `nvnCommandBufferResolveDepthBuffer` |  |  |
| `nvnCommandBufferTiledDownsample` |  |  |
| `nvnCommandBufferTiledDownsampleTextureView` |  |  |
| `nvnCommandBufferClearBuffer` | 55, 231 | nvn-109 |
| `nvnCommandBufferClearColor` | 55, 231 | nvn-45, nvn-46, nvn-60, nvn-61, nvn-62, nvn-85, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126, nvn-127, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferClearColori` | 55, 231 | nvn-107 |
| `nvnCommandBufferClearColorui` | 55, 231 | nvn-107 |
| `nvnCommandBufferClearDepthStencil` | 55, 231 | nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferClearTexture` | 55, 231 | nvn-108 |
| `nvnCommandBufferClearTextureui` | 55, 231 | nvn-108 |
| `nvnCommandBufferCopyBufferToBuffer` | 55, 231 | nvn-50, nvn-59, nvn-71, nvn-86, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18 |
| `nvnCommandBufferCopyBufferToTexture` | 55, 231 | nvn-48, nvn-49, nvn-51, nvn-52, nvn-53, nvn-57, nvn-81, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126 |
| `nvnCommandBufferCopyTextureToBuffer` | 55, 231 | nvn-45, nvn-46, nvn-48, nvn-49, nvn-51, nvn-52, nvn-53, nvn-57, nvn-60, nvn-61, nvn-62, nvn-79, nvn-81, nvn-85, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-107, nvn-108, nvn-122, nvn-126, nvn-127, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferCopyTextureToTexture` | 55, 231 | nvn-49, nvn-52 |
| `nvnCommandBufferDiscardColor` | 55, 231 | nvn-111 |
| `nvnCommandBufferDiscardDepthStencil` | 55, 231 | nvn-111 |
| `nvnCommandBufferDownsample` |  |  |
| `nvnCommandBufferDownsampleTextureView` |  |  |
| `nvnCommandBufferRestoreZCullData` |  |  |
| `nvnCommandBufferSaveZCullData` |  |  |
| `nvnCommandBufferSetCopyImageStride` | 55, 231 | nvn-110 |
| `nvnCommandBufferSetCopyRowStride` | 55, 231 | nvn-48, nvn-49, nvn-50, nvn-51, nvn-52, nvn-53, nvn-57, nvn-79, nvn-81, nvn-110 |

## Commands: render state

36 functions, 13 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferOverrideSubtileSize` |  |  |
| `nvnCommandBufferSetColorReductionEnable` |  |  |
| `nvnCommandBufferSetColorReductionThresholds` |  |  |
| `nvnCommandBufferSetConservativeRasterDilate` | 55, 231 | nvn-120 |
| `nvnCommandBufferSetInnerTessellationLevels` | 55, 231 | nvn-122 |
| `nvnCommandBufferSetOuterTessellationLevels` | 55, 231 | nvn-122 |
| `nvnCommandBufferSetProvokingVertexMode` | 55, 231 | nvn-114 |
| `nvnCommandBufferSetSubpixelPrecisionBias` | 55, 231 | nvn-119 |
| `nvnCommandBufferSetZCullZF32CompressionEnable` |  |  |
| `nvnCommandBufferSetShadingRatePaletteMode` |  |  |
| `nvnCommandBufferSetAlphaRef` |  |  |
| `nvnCommandBufferSetBlendColor` |  |  |
| `nvnCommandBufferSetConservativeRasterEnable` | 55, 231 | nvn-119, nvn-120 |
| `nvnCommandBufferSetDepthBounds` | 55, 231 | nvn-113 |
| `nvnCommandBufferSetDepthClamp` | 55, 231 | nvn-114 |
| `nvnCommandBufferSetDepthRange` | 55, 231 | nvn-113 |
| `nvnCommandBufferSetDepthRanges` | 55, 231 | nvn-113 |
| `nvnCommandBufferSetLineWidth` | 55, 231 | nvn-121 |
| `nvnCommandBufferSetPatchSize` | 55, 231 | nvn-122 |
| `nvnCommandBufferSetPointSize` | 55, 231 | nvn-115 |
| `nvnCommandBufferSetPolygonOffsetClamp` |  |  |
| `nvnCommandBufferSetPrimitiveRestart` | 55, 231 | nvn-116 |
| `nvnCommandBufferSetRasterizerDiscard` | 55, 231 | nvn-114 |
| `nvnCommandBufferSetRenderTargets` | 55, 231 | nvn-45, nvn-46, nvn-60, nvn-61, nvn-62, nvn-85, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-107, nvn-122, nvn-126, nvn-127, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferSetSampleMask` |  |  |
| `nvnCommandBufferSetScissor` | 55, 231 | nvn-45, nvn-46, nvn-60, nvn-61, nvn-62, nvn-85, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-107, nvn-122, nvn-126, nvn-127, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferSetScissors` | 55, 231 | nvn-112 |
| `nvnCommandBufferSetStencilCullCriteria` |  |  |
| `nvnCommandBufferSetStencilMask` |  |  |
| `nvnCommandBufferSetStencilRef` |  |  |
| `nvnCommandBufferSetStencilValueMask` |  |  |
| `nvnCommandBufferSetTiledCacheAction` | 55, 231 | nvn-118 |
| `nvnCommandBufferSetTiledCacheTileSize` | 55, 231 | nvn-118 |
| `nvnCommandBufferSetViewport` | 55, 231 | nvn-45, nvn-46, nvn-60, nvn-61, nvn-62, nvn-85, nvn-89, nvn-90, nvn-91, nvn-92, nvn-93, nvn-94, nvn-95, nvn-96, nvn-97, nvn-98, nvn-122, nvn-126, nvn-127, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17 |
| `nvnCommandBufferSetViewportSwizzles` | 55, 231 | nvn-117 |
| `nvnCommandBufferSetViewports` | 55, 231 | nvn-112 |

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
| `nvnProgramBuilderSetMaxCallableRecursionDepth` | 231 | nvn-128 |
| `nvnProgramBuilderGetMaxCallableRecursionDepth` | 231 | nvn-128 |
| `nvnProgramBuilderSetMaxTraceRecursionDepth` | 231 | nvn-128 |
| `nvnProgramBuilderGetMaxTraceRecursionDepth` | 231 | nvn-128 |
| `nvnProgramBuilderSetProgramRayTracingFlags` | 231 | nvn-128 |
| `nvnProgramBuilderGetProgramRayTracingFlags` | 231 | nvn-128 |
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
| `nvnSetExternalDebuggerEnabledNVX` | 55, 231 | nvn-87 |
| `nvnDeviceGenerateDebugDomainId` | 55, 231 | nvn-87, nvn-123, nvn-127 |
| `nvnDeviceInstallDebugCallback` | 55, 231 | nvn-87 |
| `nvnDeviceIsExternalDebuggerAttached` | 55, 231 | nvn-87 |
| `nvnDeviceSetAutomaticCheckpointsByDebugGroup` | 55, 231 | nvn-87 |
| `nvnDeviceSetAutomaticCheckpointsEnable` | 55, 231 | nvn-87 |
| `nvnDeviceSetAutomaticCheckpointsMask` | 55, 231 | nvn-87 |
| `nvnDeviceSetAutomaticCheckpointsSamplingInterval` | 55, 231 | nvn-87 |
| `nvnDeviceSetCheckpointCallStacksEnable` | 55, 231 | nvn-87 |
| `nvnDeviceSetCheckpointMaxCallStacksDepth` | 55, 231 | nvn-87 |
| `nvnDeviceWalkDebugDatabase` | 55, 231 | nvn-87 |
| `nvnMemoryPoolSetDebugLabel` | 55, 231 | nvn-87 |
| `nvnBufferGetDebugID` | 55, 231 | nvn-87 |
| `nvnBufferSetDebugLabel` | 55, 231 | nvn-87 |
| `nvnTextureGetDebugID` | 55, 231 | nvn-87 |
| `nvnTextureSetDebugLabel` | 55, 231 | nvn-87 |
| `nvnSamplerGetDebugID` | 55, 231 | nvn-87 |
| `nvnSamplerSetDebugLabel` | 55, 231 | nvn-87 |
| `nvnProgramBuilderSetDebugDataHashes` |  |  |
| `nvnProgramBuilderGetDebugDataHashes` |  |  |
| `nvnCommandBufferInsertCheckpointDynamic` | 55, 231 | nvn-123 |
| `nvnCommandBufferInsertCheckpointStatic` | 55, 231 | nvn-123 |
| `nvnCommandBufferInsertDebugMarkerGraphics` | 231 | nvn-127 |
| `nvnCommandBufferInsertDebugMarkerCompute` | 231 | nvn-127 |
| `nvnCommandBufferPushDebugGroupGraphics` | 231 | nvn-127 |
| `nvnCommandBufferPushDebugGroupCompute` | 231 | nvn-127 |
| `nvnCommandBufferPopDebugGroupGraphics` | 231 | nvn-127 |
| `nvnCommandBufferPopDebugGroupCompute` | 231 | nvn-127 |
| `nvnCommandBufferInsertDebugMarker` | 55, 231 | nvn-123 |
| `nvnCommandBufferInsertDebugMarkerDynamic` | 55, 231 | nvn-123 |
| `nvnCommandBufferInsertDebugMarkerStatic` | 55, 231 | nvn-123 |
| `nvnCommandBufferPopDebugGroup` | 55, 231 | nvn-123 |
| `nvnCommandBufferPopDebugGroupId` | 55, 231 | nvn-123 |
| `nvnCommandBufferPushDebugGroup` | 55, 231 | nvn-123 |
| `nvnCommandBufferPushDebugGroupDynamic` | 55, 231 | nvn-123 |
| `nvnCommandBufferPushDebugGroupStatic` | 55, 231 | nvn-123 |
| `nvnCommandBufferSetDebugLabel` | 55, 231 | nvn-87 |
| `nvnDeviceSetDebugLabel` | 55, 231 | nvn-87 |
| `nvnProgramSetDebugLabel` | 55, 231 | nvn-87 |
| `nvnQueueSetDebugLabel` | 55, 231 | nvn-87 |
| `nvnSamplerPoolSetDebugLabel` | 55, 231 | nvn-87 |
| `nvnSyncSetDebugLabel` | 55, 231 | nvn-87 |
| `nvnTexturePoolSetDebugLabel` | 55, 231 | nvn-87 |
| `nvnWindowSetDebugLabel` | 55, 231 | nvn-90 |
