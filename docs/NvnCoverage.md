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
names 677; the layer has 422 of them in its API 55 build and 462 in its API 231 build; a case calls 457. The
other 220 have no call at all: a runtime could answer any of them wrongly and every case would still pass.

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
| [Textures](#textures) | 95 | 53 | 54 | 53 | 42 |
| [Samplers](#samplers) | 45 | 45 | 45 | 45 | 0 |
| [Programs](#programs) | 20 | 4 | 13 | 13 | 7 |
| [State objects](#state-objects) | 82 | 82 | 82 | 82 | 0 |
| [Queues](#queues) | 30 | 19 | 19 | 19 | 11 |
| [Sync and events](#sync-and-events) | 24 | 18 | 18 | 18 | 6 |
| [Windows and present](#windows-and-present) | 35 | 25 | 35 | 35 | 0 |
| [Counters and queries](#counters-and-queries) | 6 | 6 | 6 | 6 | 0 |
| [Commands: memory and recording](#commands-memory-and-recording) | 25 | 22 | 22 | 22 | 3 |
| [Commands: binds](#commands-binds) | 41 | 14 | 18 | 15 | 26 |
| [Commands: draws and dispatches](#commands-draws-and-dispatches) | 26 | 3 | 6 | 6 | 20 |
| [Commands: clears and copies](#commands-clears-and-copies) | 25 | 18 | 18 | 18 | 7 |
| [Commands: render state](#commands-render-state) | 36 | 23 | 23 | 23 | 13 |
| [DLSS](#dlss) | 58 | 0 | 0 | 0 | 58 |
| [Ray tracing](#ray-tracing) | 15 | 0 | 6 | 6 | 9 |
| [Debug](#debug) | 44 | 36 | 42 | 42 | 2 |
| **All** | **677** | **422** | **462** | **457** | **220** |

## The cases

| Label | Suite | Case | In |
| --- | --- | --- | --- |
| nvn-1 | nvn | a scene of five draws through deko3d holds what the processor works out | the NRO |
| nvn-2 | nvn | the same scene through the layer's own NVN functions holds what the processor works out | the NRO |
| nvn-3 | nvn | a texture drawn past its edges through deko3d repeats, mirrors, holds its edge and shows its border | the NRO |
| nvn-4 | nvn | the same texture past its edges through the layer's own NVN functions does the same | the NRO |
| nvn-5 | nvn | four quads drawn through deko3d by indices of one, two and four bytes and a base vertex are each in its quarter | the NRO |
| nvn-6 | nvn | the same four indexed quads through the layer's own NVN functions are each in its quarter | the NRO |
| nvn-7 | nvn | the samples counter reports a quad's 256 pixels and none after a reset, through the layer's own NVN functions | the NRO |
| nvn-8 | nvn | two values a list reports are in memory once it has run, through the layer's own NVN functions | the NRO |
| nvn-9 | nvn | two timestamp reports hold no count and times between the device's clock before and after, through the layer's own NVN functions | the NRO |
| nvn-10 | nvn | a timestamp report's word is the graphics unit's ticks, 384 every 625 nanoseconds, through the layer's own NVN functions | the NRO |
| nvn-11 | nvn | the render enable draws under on and under a condition that holds and skips the rest, through the layer's own NVN functions | the NRO |
| nvn-12 | nvn | a command buffer's memory used and free come to what it was given, and draws take some, through the layer's own NVN functions | the NRO |
| nvn-13 | nvn | a list out of command memory asks the program's callback and draws on, through the layer's own NVN functions | the NRO |
| nvn-14 | nvn | a list called from another and a list copied into it draw their cells, through the layer's own NVN functions | the NRO |
| nvn-15 | nvn | clears of an unsigned and a signed integer target hold the integers given, through the layer's own NVN functions | the NRO |
| nvn-16 | nvn | a clear of a texture that is no target reaches its region and its mask's channels, through the layer's own NVN functions | the NRO |
| nvn-17 | nvn | a range of a buffer cleared to a word holds it, and nothing beside the range, through the layer's own NVN functions | the NRO |
| nvn-18 | nvn | a command buffer gives back the copy strides it was set, through the layer's own NVN functions | the NRO |
| nvn-19 | nvn | targets discarded, then cleared and drawn, hold the clear and the draw, through the layer's own NVN functions | the NRO |
| nvn-20 | nvn | a scissor and a viewport set by number bound and place a quad, through the layer's own NVN functions | the NRO |
| nvn-21 | nvn | depth ranges move a quad across the depth test and depth bounds test the target's depth, through the layer's own NVN functions | the NRO |
| nvn-22 | nvn | the rasterizer discards, clamps depth and takes a flat colour from the provoking vertex, through the layer's own NVN functions | the NRO |
| nvn-23 | nvn | a point of sixteen pixels fills its cell, through the layer's own NVN functions | the NRO |
| nvn-24 | nvn | a strip with a restart index is two quads, through the layer's own NVN functions | the NRO |
| nvn-25 | nvn | a viewport swizzle of x and y turns a quad about the diagonal, through the layer's own NVN functions | the NRO |
| nvn-26 | nvn | draws around the tiled cache's actions are as without them, through the layer's own NVN functions | the NRO |
| nvn-27 | nvn | conservative rasterization draws every pixel a quad touches, through the layer's own NVN functions | the NRO |
| nvn-28 | nvn | a conservative quad dilated by half a pixel reaches the pixels a third of one away, through the layer's own NVN functions | the NRO |
| nvn-29 | nvn | a line of width four is four rows of pixels, through the layer's own NVN functions | the NRO |
| nvn-30 | nvn | patches of the size and the outer levels a list sets fill their cells, through the layer's own NVN functions | the NRO |
| nvn-31 | nvn | draws between debug markers, groups and checkpoints are as without them, through the layer's own NVN functions | the NRO |
| nvn-32 | nvn | the device says its modes and its builder its flags, and clear values register, through the layer's own NVN functions | the NRO |
| nvn-33 | nvn | a program set from the longer shader data draws, through the layer's own NVN functions | the NRO |
| nvn-34 | nvn | a texture's handle and a sampler's put together sample, and a texel fetch handle fetches, through the layer's own NVN functions | the NRO |
| nvn-35 | nvn | the program's NVN functions resolve | the NRO, the Switch 2 program |
| nvn-36 | nvn | the device initializes | the NRO, the Switch 2 program |
| nvn-37 | nvn | the device answers the layer's API version | the NRO, the Switch 2 program |
| nvn-38 | nvn | a queue, its pools and a 64x64 target come up | the NRO, the Switch 2 program |
| nvn-39 | nvn | clear to opaque red | the NRO, the Switch 2 program |
| nvn-40 | nvn | clear to a mid channel value | the NRO, the Switch 2 program |
| nvn-41 | nvn | two copy textures and a staging pool come up | the NRO, the Switch 2 program |
| nvn-42 | nvn | a picture copied into a texture and back keeps every texel | the NRO, the Switch 2 program |
| nvn-43 | nvn | a texture copied into another keeps every texel | the NRO, the Switch 2 program |
| nvn-44 | nvn | a buffer copied into another keeps every byte | the NRO, the Switch 2 program |
| nvn-45 | nvn | a texture copied to a buffer with MIRROR_Y lands upside down | the NRO, the Switch 2 program |
| nvn-46 | nvn | a texture copied into another with MIRROR_X lands mirrored | the NRO, the Switch 2 program |
| nvn-47 | nvn | a copy to a buffer with a row stride of 288 keeps the bytes between rows | the NRO, the Switch 2 program |
| nvn-48 | nvn | a texture builder gives back each number it was set | the NRO, the Switch 2 program |
| nvn-49 | nvn | a texture view says which of its parts were set, and with what | the NRO, the Switch 2 program |
| nvn-50 | nvn | a texture of three levels comes up and says what its builder had | the NRO, the Switch 2 program |
| nvn-51 | nvn | a picture copied into each level of a texture through a view comes back from that level | the NRO, the Switch 2 program |
| nvn-52 | nvn | a memory pool and its builder give back what they were set, and the pool maps its storage | the NRO, the Switch 2 program |
| nvn-53 | nvn | a page flushed from a cached pool is what a copy reads, and the copy is there after an invalidate | the NRO, the Switch 2 program |
| nvn-54 | nvn | a sync fenced on the queue behind a clear signals, and the clear is there with no finish | the NRO, the Switch 2 program |
| nvn-55 | nvn | a sync fenced inside the list behind a clear signals, and the clear is there with no finish | the NRO, the Switch 2 program |
| nvn-56 | nvn | a queue and a list told to wait for a sync that signalled run their clear | the NRO, the Switch 2 program |
| nvn-57 | nvn | a wait on a sync nothing fenced fails, with a timeout or without | the NRO, the Switch 2 program |
| nvn-58 | nvn | the queue has no error after work that was valid | the NRO, the Switch 2 program |
| nvn-59 | nvn | a queue builder gives back its flags and says which of its sizes were set | the NRO, the Switch 2 program |
| nvn-60 | nvn | an event keeps what the processor signals: a write, an add and an add that wraps | the NRO, the Switch 2 program |
| nvn-61 | nvn | a sampler builder gives back each number it was set | the NRO, the Switch 2 program |
| nvn-62 | nvn | a sampler says what its builder had, and is the same as another of that state alone | the NRO, the Switch 2 program |
| nvn-63 | nvn | the texture and sampler pools say where they are, and a builder registers as its sampler | the NRO, the Switch 2 program |
| nvn-64 | nvn | a buffer is a range of its pool: its address, its mapping, and what it and its builder say | the NRO, the Switch 2 program |
| nvn-65 | nvn | bytes flushed from one buffer and copied to another are there after an invalidate | the NRO, the Switch 2 program |
| nvn-66 | nvn | a blend state gives back its target, functions, equations and advanced mode | the NRO, the Switch 2 program |
| nvn-67 | nvn | a channel mask and a colour state give back a target's channels, blending, the logic operation and the alpha test | the NRO, the Switch 2 program |
| nvn-68 | nvn | a depth and stencil state gives back its depth test and each face's stencil | the NRO, the Switch 2 program |
| nvn-69 | nvn | a multisample state gives back its samples, its switches and its sample locations' grid | the NRO, the Switch 2 program |
| nvn-70 | nvn | a polygon state, a vertex attribute and a vertex stream give back what they were set | the NRO, the Switch 2 program |
| nvn-71 | nvn | the debug layer's calls are answered as a release driver answers them, and a label changes no object | the NRO, the Switch 2 program |
| nvn-72 | nvn | a window builder gives back what it was set | the NRO, the Switch 2 program |
| nvn-73 | nvn | a window acquires and presents both its textures | the NRO, the Switch 2 program |
| nvn-74 | nvn | a window says what it is of, and keeps the active textures, the interval and the crop it is set | the NRO, the Switch 2 program |
| nvn-75 | nvn | a texture the window hands with a sync is free once the sync signals, and presents | the NRO, the Switch 2 program |
| nvn-76 | nvn | the same scene through the NVN the program looks up holds what the processor works out | the NRO |
| nvn-77 | nvn | the same texture past its edges through the NVN the program looks up does the same | the NRO |
| nvn-78 | nvn | the same four indexed quads through the NVN the program looks up are each in its quarter | the NRO |
| nvn-79 | nvn | the samples counter reports a quad's 256 pixels and none after a reset, through the NVN the program looks up | the NRO |
| nvn-80 | nvn | two values a list reports are in memory once it has run, through the NVN the program looks up | the NRO |
| nvn-81 | nvn | two timestamp reports hold no count and times between the device's clock before and after, through the NVN the program looks up | the NRO |
| nvn-82 | nvn | a timestamp report's word is the graphics unit's ticks, 384 every 625 nanoseconds, through the NVN the program looks up | the NRO |
| nvn-83 | nvn | the render enable draws under on and under a condition that holds and skips the rest, through the NVN the program looks up | the NRO |
| nvn-84 | nvn | a command buffer's memory used and free come to what it was given, and draws take some, through the NVN the program looks up | the NRO |
| nvn-85 | nvn | a list out of command memory asks the program's callback and draws on, through the NVN the program looks up | the NRO |
| nvn-86 | nvn | a list called from another and a list copied into it draw their cells, through the NVN the program looks up | the NRO |
| nvn-87 | nvn | clears of an unsigned and a signed integer target hold the integers given, through the NVN the program looks up | the NRO |
| nvn-88 | nvn | a clear of a texture that is no target reaches its region and its mask's channels, through the NVN the program looks up | the NRO |
| nvn-89 | nvn | a range of a buffer cleared to a word holds it, and nothing beside the range, through the NVN the program looks up | the NRO |
| nvn-90 | nvn | a command buffer gives back the copy strides it was set, through the NVN the program looks up | the NRO |
| nvn-91 | nvn | targets discarded, then cleared and drawn, hold the clear and the draw, through the NVN the program looks up | the NRO |
| nvn-92 | nvn | a scissor and a viewport set by number bound and place a quad, through the NVN the program looks up | the NRO |
| nvn-93 | nvn | depth ranges move a quad across the depth test and depth bounds test the target's depth, through the NVN the program looks up | the NRO |
| nvn-94 | nvn | the rasterizer discards, clamps depth and takes a flat colour from the provoking vertex, through the NVN the program looks up | the NRO |
| nvn-95 | nvn | a point of sixteen pixels fills its cell, through the NVN the program looks up | the NRO |
| nvn-96 | nvn | a strip with a restart index is two quads, through the NVN the program looks up | the NRO |
| nvn-97 | nvn | a viewport swizzle of x and y turns a quad about the diagonal, through the NVN the program looks up | the NRO |
| nvn-98 | nvn | draws around the tiled cache's actions are as without them, through the NVN the program looks up | the NRO |
| nvn-99 | nvn | conservative rasterization draws every pixel a quad touches, through the NVN the program looks up | the NRO |
| nvn-100 | nvn | a conservative quad dilated by half a pixel reaches the pixels a third of one away, through the NVN the program looks up | the NRO |
| nvn-101 | nvn | a line of width four is four rows of pixels, through the NVN the program looks up | the NRO |
| nvn-102 | nvn | patches of the size and the outer levels a list sets fill their cells, through the NVN the program looks up | the NRO |
| nvn-103 | nvn | draws between debug markers, groups and checkpoints are as without them, through the NVN the program looks up | the NRO |
| nvn-104 | nvn | the device says its modes and its builder its flags, and clear values register, through the NVN the program looks up | the NRO |
| nvn-105 | nvn | a program set from the longer shader data draws, through the NVN the program looks up | the NRO |
| nvn-106 | nvn | a texture's handle and a sampler's put together sample, and a texel fetch handle fetches, through the NVN the program looks up | the NRO |
| nvn-107 | nvn | a clear between the Ounce's graphics and compute markers and groups is the clear | the Switch 2 program |
| nvn-108 | nvn | an Ounce program builder gives back what it was set, a ray tracing program's numbers too | the Switch 2 program |
| nvn-109 | nvn | an Ounce window builder starts with the driver's colour space and HDR metadata, and gives back what it is set | the Switch 2 program |
| nvn-110 | nvn | an Ounce window has its builder's HDR metadata, and gives back what it is set | the Switch 2 program |
| nvn2-1 | nvn2 | a device, a queue, a command buffer, their pools and a 256x256 target with its depth come up | the Switch 2 program |
| nvn2-2 | nvn2 | a compute program assembled by hand makes a program through the builder | the Switch 2 program |
| nvn2-3 | nvn2 | a dispatch's writes to a storage buffer, as the processor reads them once the queue finishes | the Switch 2 program |
| nvn2-4 | nvn2 | a dispatch's writes to a storage buffer, as the graphics unit copies them to another buffer | the Switch 2 program |
| nvn2-5 | nvn2 | the Mandelbrot's compute, mesh and fragment programs make two programs | the Switch 2 program |
| nvn2-6 | nvn2 | a compute program counts the Mandelbrot set's steps for every cell of a grid | the Switch 2 program |
| nvn2-7 | nvn2 | a mesh program draws the counts as bars, a fragment program colours them and the depth test keeps the nearest | the Switch 2 program |
| nvn2-8 | nvn2 | one warp's matrix instruction multiplies a 16 by 16 matrix of halves by a 16 by 8 one into floats | the Switch 2 program |

## Device

36 functions, 14 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnBootstrapLoader` | 55, 231 | nvn-35, nvn-76, nvn-77, nvn-78, nvn2-1 |
| `nvnDeviceGetProcAddress` | 55, 231 | nvn-35, nvn-76, nvn-77, nvn-78, nvn2-1 |
| `nvnDeviceApplyDeferredFinalizes` |  |  |
| `nvnDeviceBuilderGetFlags` | 55, 231 | nvn-104 |
| `nvnDeviceFinalize` | 55, 231 | nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnDeviceRegisterFastClearColori` | 55, 231 | nvn-104 |
| `nvnDeviceRegisterFastClearColorui` | 55, 231 | nvn-104 |
| `nvnDeviceSetIntermediateShaderCache` |  |  |
| `nvnDeviceBuilderSetDefaults` | 55, 231 | nvn-36, nvn-76, nvn-77, nvn-78, nvn-104, nvn2-1 |
| `nvnDeviceBuilderSetFlags` | 55, 231 | nvn-36, nvn-76, nvn-77, nvn-78, nvn-104, nvn2-1 |
| `nvnDeviceGetImageHandle` | 231 |  |
| `nvnDeviceFinalizeCommandHandle` |  |  |
| `nvnDeviceGetCurrentTimestampInNanoseconds` | 55, 231 | nvn-9, nvn-10, nvn-81, nvn-82 |
| `nvnDeviceGetDepthMode` | 55, 231 | nvn-104 |
| `nvnDeviceGetInteger` | 55, 231 | nvn-37, nvn-76, nvn-77, nvn-78, nvn2-1 |
| `nvnDeviceGetWindowOriginMode` | 55, 231 | nvn-104 |
| `nvnDeviceGetSeparateSamplerHandle` | 55, 231 | nvn-106 |
| `nvnDeviceGetSeparateTextureHandle` | 55, 231 | nvn-106 |
| `nvnDeviceGetTexelFetchHandle` | 55, 231 | nvn-106 |
| `nvnDeviceGetTextureHandle` | 55, 231 | nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnDeviceInitialize` | 55, 231 | nvn-36, nvn-76, nvn-77, nvn-78, nvn2-1 |
| `nvnDeviceRegisterFastClearColor` | 55, 231 | nvn-104 |
| `nvnDeviceRegisterFastClearDepth` | 55, 231 | nvn-104 |
| `nvnDeviceRegisterFastClearStencil` | 55, 231 | nvn-104 |
| `nvnDeviceSetDepthMode` | 55, 231 | nvn-76, nvn-77, nvn-78 |
| `nvnDeviceSetWindowOriginMode` | 55, 231 | nvn-76, nvn-77, nvn-78 |
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
| `nvnMemoryPoolBuilderGetDevice` | 55, 231 | nvn-52 |
| `nvnMemoryPoolBuilderGetFlags` | 55, 231 | nvn-52 |
| `nvnMemoryPoolBuilderGetMemory` | 55, 231 | nvn-52 |
| `nvnMemoryPoolBuilderGetSize` | 55, 231 | nvn-52 |
| `nvnMemoryPoolBuilderSetDefaults` | 55, 231 | nvn-15, nvn-16, nvn-38, nvn-41, nvn-50, nvn-52, nvn-73, nvn-76, nvn-77, nvn-78, nvn-87, nvn-88, nvn2-1 |
| `nvnMemoryPoolBuilderSetDevice` | 55, 231 | nvn-15, nvn-16, nvn-38, nvn-41, nvn-50, nvn-52, nvn-73, nvn-76, nvn-77, nvn-78, nvn-87, nvn-88, nvn2-1 |
| `nvnMemoryPoolBuilderSetFlags` | 55, 231 | nvn-15, nvn-16, nvn-38, nvn-41, nvn-50, nvn-52, nvn-73, nvn-76, nvn-77, nvn-78, nvn-87, nvn-88, nvn2-1 |
| `nvnMemoryPoolBuilderSetStorage` | 55, 231 | nvn-15, nvn-16, nvn-38, nvn-41, nvn-50, nvn-52, nvn-73, nvn-76, nvn-77, nvn-78, nvn-87, nvn-88, nvn2-1 |
| `nvnMemoryPoolBuilderSetNativeHandleNVX` |  |  |
| `nvnMemoryPoolFinalize` | 55, 231 | nvn-15, nvn-16, nvn-73, nvn-76, nvn-77, nvn-78, nvn-87, nvn-88, nvn-102, nvn-106 |
| `nvnMemoryPoolFlushMappedRange` | 55, 231 | nvn-53, nvn-65 |
| `nvnMemoryPoolGetBufferAddress` | 55, 231 | nvn-39, nvn-40, nvn-42, nvn-43, nvn-44, nvn-45, nvn-46, nvn-47, nvn-51, nvn-53, nvn-54, nvn-55, nvn-56, nvn-64, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106, nvn-107, nvn2-2, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-8 |
| `nvnMemoryPoolGetFlags` | 55, 231 | nvn-52 |
| `nvnMemoryPoolGetSize` | 55, 231 | nvn-52, nvn-71 |
| `nvnMemoryPoolInitialize` | 55, 231 | nvn-15, nvn-16, nvn-38, nvn-41, nvn-50, nvn-52, nvn-73, nvn-76, nvn-77, nvn-78, nvn-87, nvn-88, nvn2-1 |
| `nvnMemoryPoolInvalidateMappedRange` | 55, 231 | nvn-53 |
| `nvnMemoryPoolMap` | 55, 231 | nvn-39, nvn-40, nvn-41, nvn-42, nvn-43, nvn-44, nvn-45, nvn-46, nvn-47, nvn-51, nvn-52, nvn-53, nvn-54, nvn-55, nvn-56, nvn-60, nvn-63, nvn-64, nvn-65, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106, nvn-107, nvn2-2, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-8 |
| `nvnMemoryPoolMapVirtual` |  |  |

## Buffers

16 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnBufferBuilderGetDevice` | 55, 231 | nvn-64 |
| `nvnBufferBuilderGetMemoryOffset` | 55, 231 | nvn-64 |
| `nvnBufferBuilderGetMemoryPool` | 55, 231 | nvn-64 |
| `nvnBufferBuilderGetSize` | 55, 231 | nvn-64 |
| `nvnBufferBuilderSetDefaults` | 55, 231 | nvn-64, nvn-65, nvn-71 |
| `nvnBufferBuilderSetDevice` | 55, 231 | nvn-64, nvn-65, nvn-71 |
| `nvnBufferBuilderSetStorage` | 55, 231 | nvn-64, nvn-65, nvn-71 |
| `nvnBufferFinalize` | 55, 231 | nvn-64, nvn-65, nvn-71 |
| `nvnBufferFlushMappedRange` | 55, 231 | nvn-65 |
| `nvnBufferInvalidateMappedRange` | 55, 231 | nvn-65 |
| `nvnBufferGetAddress` | 55, 231 | nvn-64, nvn-65 |
| `nvnBufferGetMemoryOffset` | 55, 231 | nvn-64 |
| `nvnBufferGetMemoryPool` | 55, 231 | nvn-64 |
| `nvnBufferGetSize` | 55, 231 | nvn-64, nvn-71 |
| `nvnBufferInitialize` | 55, 231 | nvn-64, nvn-65, nvn-71 |
| `nvnBufferMap` | 55, 231 | nvn-64, nvn-65 |

## Textures

95 functions, 42 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnTextureBuilderGetGLTextureName` |  |  |
| `nvnTextureBuilderGetMemoryOffset` | 55, 231 | nvn-48 |
| `nvnTextureBuilderGetPackagedTextureLayout` |  |  |
| `nvnTextureBuilderGetRawStorageClass` |  |  |
| `nvnTextureBuilderGetSparseTileLayout` |  |  |
| `nvnTextureBuilderGetViewOffset` |  |  |
| `nvnTextureBuilderSetGLTextureName` |  |  |
| `nvnTextureGetNvRmSurfaceNVX` |  |  |
| `nvnTextureGetRawStorageClass` |  |  |
| `nvnTextureGetSparseTileLayout` |  |  |
| `nvnTextureInvalidateTexels` |  |  |
| `nvnTextureReadTexelsStrided` |  |  |
| `nvnTextureViewCompare` | 55, 231 | nvn-49 |
| `nvnTextureBuilderGetDepthStencilMode` |  |  |
| `nvnTextureBuilderGetDevice` | 55, 231 | nvn-48 |
| `nvnTextureBuilderGetMemoryPool` | 55, 231 | nvn-48 |
| `nvnTextureBuilderGetPackagedTextureData` |  |  |
| `nvnTextureBuilderGetSwizzle` |  |  |
| `nvnTextureBuilderGetDepth` | 55, 231 | nvn-48 |
| `nvnTextureBuilderGetFlags` | 55, 231 | nvn-48 |
| `nvnTextureBuilderGetFormat` | 55, 231 | nvn-48 |
| `nvnTextureBuilderGetHeight` | 55, 231 | nvn-48 |
| `nvnTextureBuilderGetLevels` | 55, 231 | nvn-48 |
| `nvnTextureBuilderGetSamples` | 55, 231 | nvn-48 |
| `nvnTextureBuilderGetStorageAlignment` | 55, 231 | nvn-15, nvn-16, nvn-38, nvn-50, nvn-73, nvn-76, nvn-77, nvn-78, nvn-87, nvn-88, nvn-102, nvn-106, nvn2-1 |
| `nvnTextureBuilderGetStorageClass` |  |  |
| `nvnTextureBuilderGetStorageSize` | 55, 231 | nvn-15, nvn-16, nvn-38, nvn-41, nvn-50, nvn-73, nvn-76, nvn-77, nvn-78, nvn-87, nvn-88, nvn-102, nvn-106, nvn2-1 |
| `nvnTextureBuilderGetStride` |  |  |
| `nvnTextureBuilderGetTarget` | 55, 231 | nvn-48 |
| `nvnTextureBuilderGetWidth` | 55, 231 | nvn-48 |
| `nvnTextureBuilderGetZCullStorageSize` |  |  |
| `nvnTextureBuilderSetDefaults` | 55, 231 | nvn-15, nvn-16, nvn-38, nvn-41, nvn-48, nvn-50, nvn-73, nvn-76, nvn-77, nvn-78, nvn-87, nvn-88, nvn-102, nvn-106, nvn2-1 |
| `nvnTextureBuilderSetDepth` | 55, 231 | nvn-48, nvn-50 |
| `nvnTextureBuilderSetDepthStencilMode` |  |  |
| `nvnTextureBuilderSetDevice` | 55, 231 | nvn-15, nvn-16, nvn-38, nvn-41, nvn-48, nvn-50, nvn-73, nvn-76, nvn-77, nvn-78, nvn-87, nvn-88, nvn-102, nvn-106, nvn2-1 |
| `nvnTextureBuilderSetFlags` | 55, 231 | nvn-38, nvn-41, nvn-48, nvn-50, nvn-73, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106, nvn2-1 |
| `nvnTextureBuilderSetFormat` | 55, 231 | nvn-15, nvn-16, nvn-38, nvn-41, nvn-48, nvn-50, nvn-73, nvn-76, nvn-77, nvn-78, nvn-87, nvn-88, nvn-102, nvn-106, nvn2-1 |
| `nvnTextureBuilderSetHeight` | 55, 231 | nvn-48 |
| `nvnTextureBuilderSetLevels` | 55, 231 | nvn-48, nvn-50 |
| `nvnTextureBuilderSetPackagedTextureData` |  |  |
| `nvnTextureBuilderSetPackagedTextureLayout` |  |  |
| `nvnTextureBuilderSetSamples` | 55, 231 | nvn-48, nvn-50 |
| `nvnTextureBuilderSetSize1D` |  |  |
| `nvnTextureBuilderSetSize2D` | 55, 231 | nvn-15, nvn-16, nvn-38, nvn-41, nvn-48, nvn-50, nvn-73, nvn-76, nvn-77, nvn-78, nvn-87, nvn-88, nvn-102, nvn-106, nvn2-1 |
| `nvnTextureBuilderSetSize3D` |  |  |
| `nvnTextureBuilderSetStorage` | 55, 231 | nvn-15, nvn-16, nvn-38, nvn-41, nvn-48, nvn-50, nvn-73, nvn-76, nvn-77, nvn-78, nvn-87, nvn-88, nvn-102, nvn-106, nvn2-1 |
| `nvnTextureBuilderSetStride` |  |  |
| `nvnTextureBuilderSetSwizzle` |  |  |
| `nvnTextureBuilderSetTarget` | 55, 231 | nvn-15, nvn-16, nvn-38, nvn-41, nvn-48, nvn-50, nvn-73, nvn-76, nvn-77, nvn-78, nvn-87, nvn-88, nvn-102, nvn-106, nvn2-1 |
| `nvnTextureBuilderSetWidth` | 55, 231 | nvn-48 |
| `nvnTextureCompare` |  |  |
| `nvnTextureFinalize` | 55, 231 | nvn-15, nvn-16, nvn-73, nvn-76, nvn-77, nvn-78, nvn-87, nvn-88, nvn-102, nvn-106 |
| `nvnTextureFlushTexels` |  |  |
| `nvnTextureGetDepth` | 55, 231 | nvn-50 |
| `nvnTextureGetDepthStencilMode` |  |  |
| `nvnTextureGetFlags` | 55, 231 | nvn-50 |
| `nvnTextureGetFormat` | 55, 231 | nvn-50 |
| `nvnTextureGetHeight` | 55, 231 | nvn-50 |
| `nvnTextureGetLevels` | 55, 231 | nvn-50 |
| `nvnTextureGetMemoryOffset` | 55, 231 | nvn-50 |
| `nvnTextureGetMemoryPool` | 55, 231 | nvn-50 |
| `nvnTextureGetSamples` | 55, 231 | nvn-50 |
| `nvnTextureGetStorageClass` |  |  |
| `nvnTextureGetStorageSize` |  |  |
| `nvnTextureGetStride` |  |  |
| `nvnTextureGetSwizzle` |  |  |
| `nvnTextureGetTarget` | 55, 231 | nvn-50 |
| `nvnTextureGetTextureAddress` |  |  |
| `nvnTextureGetViewOffset` |  |  |
| `nvnTextureGetWidth` | 55, 231 | nvn-50, nvn-71 |
| `nvnTextureGetZCullStorageSize` |  |  |
| `nvnTextureInitialize` | 55, 231 | nvn-15, nvn-16, nvn-38, nvn-41, nvn-50, nvn-73, nvn-76, nvn-77, nvn-78, nvn-87, nvn-88, nvn-102, nvn-106, nvn2-1 |
| `nvnTexturePoolFinalize` | 55, 231 | nvn-63, nvn-71, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnTexturePoolGetMemoryOffset` | 55, 231 | nvn-63 |
| `nvnTexturePoolGetMemoryPool` | 55, 231 | nvn-63 |
| `nvnTexturePoolGetSize` | 55, 231 | nvn-63, nvn-71 |
| `nvnTexturePoolInitialize` | 55, 231 | nvn-63, nvn-71, nvn-76, nvn-77, nvn-78 |
| `nvnTexturePoolRegisterImage` | 231 |  |
| `nvnTexturePoolRegisterTexture` | 55, 231 | nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnTextureReadTexels` |  |  |
| `nvnTextureViewGetDepthStencilMode` |  |  |
| `nvnTextureViewGetFormat` | 55, 231 | nvn-49 |
| `nvnTextureViewGetLayers` | 55, 231 | nvn-49 |
| `nvnTextureViewGetLevels` | 55, 231 | nvn-49 |
| `nvnTextureViewGetSwizzle` |  |  |
| `nvnTextureViewGetTarget` | 55, 231 | nvn-49 |
| `nvnTextureViewSetDefaults` | 55, 231 | nvn-49, nvn-51 |
| `nvnTextureViewSetDepthStencilMode` |  |  |
| `nvnTextureViewSetFormat` | 55, 231 | nvn-49 |
| `nvnTextureViewSetLayers` | 55, 231 | nvn-49 |
| `nvnTextureViewSetLevels` | 55, 231 | nvn-49, nvn-51 |
| `nvnTextureViewSetSwizzle` |  |  |
| `nvnTextureViewSetTarget` | 55, 231 | nvn-49 |
| `nvnTextureWriteTexels` |  |  |
| `nvnTextureWriteTexelsStrided` |  |  |

## Samplers

45 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnSamplerBuilderGetBorderColor` | 55, 231 | nvn-61, nvn-62 |
| `nvnSamplerBuilderGetBorderColori` | 55, 231 | nvn-61 |
| `nvnSamplerBuilderGetBorderColorui` | 55, 231 | nvn-61 |
| `nvnSamplerBuilderGetCompare` | 55, 231 | nvn-61, nvn-62 |
| `nvnSamplerBuilderGetDevice` | 55, 231 | nvn-61 |
| `nvnSamplerBuilderGetLodBias` | 55, 231 | nvn-61, nvn-62 |
| `nvnSamplerBuilderGetLodClamp` | 55, 231 | nvn-61, nvn-62 |
| `nvnSamplerBuilderGetMaxAnisotropy` | 55, 231 | nvn-61, nvn-62 |
| `nvnSamplerBuilderGetMinMagFilter` | 55, 231 | nvn-61, nvn-62 |
| `nvnSamplerBuilderGetReductionFilter` | 55, 231 | nvn-61, nvn-62 |
| `nvnSamplerBuilderGetWrapMode` | 55, 231 | nvn-61, nvn-62 |
| `nvnSamplerBuilderSetBorderColori` | 55, 231 | nvn-61 |
| `nvnSamplerBuilderSetBorderColorui` | 55, 231 | nvn-61 |
| `nvnSamplerGetBorderColori` | 55, 231 | nvn-62 |
| `nvnSamplerGetBorderColorui` | 55, 231 | nvn-62 |
| `nvnSamplerPoolRegisterSamplerBuilder` | 55, 231 | nvn-63 |
| `nvnSamplerBuilderSetBorderColor` | 55, 231 | nvn-61, nvn-62, nvn-63, nvn-71, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnSamplerBuilderSetCompare` | 55, 231 | nvn-61, nvn-62, nvn-63, nvn-71 |
| `nvnSamplerBuilderSetDefaults` | 55, 231 | nvn-61, nvn-62, nvn-63, nvn-71, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnSamplerBuilderSetDevice` | 55, 231 | nvn-61, nvn-62, nvn-63, nvn-71, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnSamplerBuilderSetLodBias` | 55, 231 | nvn-61, nvn-62, nvn-63, nvn-71 |
| `nvnSamplerBuilderSetLodClamp` | 55, 231 | nvn-61, nvn-62, nvn-63, nvn-71 |
| `nvnSamplerBuilderGetLodSnap` | 55, 231 | nvn-61 |
| `nvnSamplerBuilderSetLodSnap` | 55, 231 | nvn-61, nvn-62, nvn-63, nvn-71 |
| `nvnSamplerBuilderSetMaxAnisotropy` | 55, 231 | nvn-61, nvn-62, nvn-63, nvn-71 |
| `nvnSamplerBuilderSetMinMagFilter` | 55, 231 | nvn-61, nvn-62, nvn-63, nvn-71, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnSamplerBuilderSetReductionFilter` | 55, 231 | nvn-61, nvn-62, nvn-63, nvn-71 |
| `nvnSamplerBuilderSetWrapMode` | 55, 231 | nvn-61, nvn-62, nvn-63, nvn-71, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnSamplerCompare` | 55, 231 | nvn-62 |
| `nvnSamplerFinalize` | 55, 231 | nvn-62, nvn-63, nvn-71, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnSamplerGetBorderColor` | 55, 231 | nvn-62 |
| `nvnSamplerGetCompare` | 55, 231 | nvn-62 |
| `nvnSamplerGetLodBias` | 55, 231 | nvn-62 |
| `nvnSamplerGetLodClamp` | 55, 231 | nvn-62 |
| `nvnSamplerGetMaxAnisotropy` | 55, 231 | nvn-62 |
| `nvnSamplerGetMinMagFilter` | 55, 231 | nvn-62 |
| `nvnSamplerGetReductionFilter` | 55, 231 | nvn-62 |
| `nvnSamplerGetWrapMode` | 55, 231 | nvn-62 |
| `nvnSamplerInitialize` | 55, 231 | nvn-62, nvn-63, nvn-71, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnSamplerPoolFinalize` | 55, 231 | nvn-63, nvn-71, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnSamplerPoolGetMemoryOffset` | 55, 231 | nvn-63 |
| `nvnSamplerPoolGetMemoryPool` | 55, 231 | nvn-63 |
| `nvnSamplerPoolGetSize` | 55, 231 | nvn-63, nvn-71 |
| `nvnSamplerPoolInitialize` | 55, 231 | nvn-63, nvn-71, nvn-76, nvn-77, nvn-78 |
| `nvnSamplerPoolRegisterSampler` | 55, 231 | nvn-63, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |

## Programs

20 functions, 7 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnProgramGetSubtileSize` |  |  |
| `nvnProgramGetScratchBytesPerWarp` |  |  |
| `nvnProgramSetSampleShading` |  |  |
| `nvnProgramSetSubroutineLinkage` |  |  |
| `nvnProgramFinalize` | 55, 231 | nvn-30, nvn-33, nvn-34, nvn-71, nvn-76, nvn-77, nvn-78, nvn-102, nvn-105, nvn-106 |
| `nvnProgramInitialize` | 55, 231 | nvn-71, nvn-76, nvn-77, nvn-78, nvn-102, nvn-105, nvn-106 |
| `nvnProgramSetShaders` | 55, 231 | nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnProgramSetShadersExt` | 55, 231 | nvn-105 |
| `nvnProgramBuilderSetDefaults` | 231 | nvn-108, nvn2-2, nvn2-5, nvn2-8 |
| `nvnProgramBuilderSetDevice` | 231 | nvn-108, nvn2-2, nvn2-5, nvn2-8 |
| `nvnProgramBuilderGetDevice` | 231 | nvn-108 |
| `nvnProgramBuilderSetSampleShading` | 231 | nvn-108 |
| `nvnProgramBuilderGetSampleShading` | 231 | nvn-108 |
| `nvnProgramBuilderSetShaders` | 231 | nvn-108, nvn2-2, nvn2-5, nvn2-8 |
| `nvnProgramBuilderGetShaders` | 231 | nvn-108 |
| `nvnProgramBuilderGetShaderCount` | 231 | nvn-108 |
| `nvnProgramBuilderSetSubroutineLinkage` |  |  |
| `nvnProgramBuilderGetSubroutineLinkage` |  |  |
| `nvnProgramBuilderGetSubroutineLinkageCount` |  |  |
| `nvnProgramInitializeFromBuilder` | 231 | nvn2-2, nvn2-5, nvn2-8 |

## State objects

82 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnBlendStateGetAdvancedMode` | 55, 231 | nvn-66 |
| `nvnBlendStateGetAdvancedNormalizedDst` | 55, 231 | nvn-66 |
| `nvnBlendStateGetAdvancedOverlap` | 55, 231 | nvn-66 |
| `nvnBlendStateGetAdvancedPremultipliedSrc` | 55, 231 | nvn-66 |
| `nvnBlendStateSetAdvancedMode` | 55, 231 | nvn-66 |
| `nvnBlendStateSetAdvancedNormalizedDst` | 55, 231 | nvn-66 |
| `nvnBlendStateSetAdvancedOverlap` | 55, 231 | nvn-66 |
| `nvnBlendStateSetAdvancedPremultipliedSrc` | 55, 231 | nvn-66 |
| `nvnMultisampleStateGetSampleLocationsGrid` | 55, 231 | nvn-69 |
| `nvnMultisampleStateSetSampleLocations` | 55, 231 | nvn-69 |
| `nvnBlendStateSetBlendEquation` | 55, 231 | nvn-66 |
| `nvnBlendStateSetBlendFunc` | 55, 231 | nvn-66 |
| `nvnBlendStateSetBlendTarget` | 55, 231 | nvn-66 |
| `nvnBlendStateSetDefaults` | 55, 231 | nvn-66, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106, nvn2-7 |
| `nvnChannelMaskStateSetChannelMask` | 55, 231 | nvn-67 |
| `nvnChannelMaskStateSetDefaults` | 55, 231 | nvn-67, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106, nvn2-7 |
| `nvnColorStateGetBlendEnable` | 55, 231 | nvn-67 |
| `nvnColorStateSetAlphaTest` | 55, 231 | nvn-67 |
| `nvnColorStateSetBlendEnable` | 55, 231 | nvn-67 |
| `nvnColorStateSetDefaults` | 55, 231 | nvn-67, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106, nvn2-7 |
| `nvnColorStateSetLogicOp` | 55, 231 | nvn-67 |
| `nvnDepthStencilStateSetDefaults` | 55, 231 | nvn-68, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106, nvn2-7 |
| `nvnDepthStencilStateSetDepthFunc` | 55, 231 | nvn-68, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106, nvn2-7 |
| `nvnDepthStencilStateSetDepthTestEnable` | 55, 231 | nvn-68, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106, nvn2-7 |
| `nvnDepthStencilStateSetDepthWriteEnable` | 55, 231 | nvn-68, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106, nvn2-7 |
| `nvnDepthStencilStateSetStencilFunc` | 55, 231 | nvn-68 |
| `nvnDepthStencilStateSetStencilOp` | 55, 231 | nvn-68 |
| `nvnDepthStencilStateSetStencilTestEnable` | 55, 231 | nvn-68 |
| `nvnMultisampleStateSetAlphaToCoverageEnable` | 55, 231 | nvn-69 |
| `nvnMultisampleStateSetDefaults` | 55, 231 | nvn-69, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106, nvn2-7 |
| `nvnMultisampleStateSetMultisampleEnable` | 55, 231 | nvn-69 |
| `nvnMultisampleStateSetSamples` | 55, 231 | nvn-69 |
| `nvnPolygonStateSetCullFace` | 55, 231 | nvn-70, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106, nvn2-7 |
| `nvnPolygonStateSetDefaults` | 55, 231 | nvn-70, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106, nvn2-7 |
| `nvnPolygonStateSetFrontFace` | 55, 231 | nvn-70, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnPolygonStateSetPolygonMode` | 55, 231 | nvn-70 |
| `nvnPolygonStateSetPolygonOffsetEnables` | 55, 231 | nvn-70 |
| `nvnVertexAttribStateSetDefaults` | 55, 231 | nvn-70, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnVertexAttribStateSetFormat` | 55, 231 | nvn-70, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnVertexAttribStateSetStreamIndex` | 55, 231 | nvn-70, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnVertexStreamStateSetDefaults` | 55, 231 | nvn-70, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnVertexStreamStateSetDivisor` | 55, 231 | nvn-70 |
| `nvnVertexStreamStateSetStride` | 55, 231 | nvn-70, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnBlendStateGetBlendEquation` | 55, 231 | nvn-66 |
| `nvnBlendStateGetBlendFunc` | 55, 231 | nvn-66 |
| `nvnBlendStateGetBlendTarget` | 55, 231 | nvn-66 |
| `nvnChannelMaskStateGetChannelMask` | 55, 231 | nvn-67 |
| `nvnColorStateGetAlphaTest` | 55, 231 | nvn-67 |
| `nvnColorStateGetLogicOp` | 55, 231 | nvn-67 |
| `nvnDepthStencilStateGetDepthFunc` | 55, 231 | nvn-68 |
| `nvnDepthStencilStateGetDepthTestEnable` | 55, 231 | nvn-68 |
| `nvnDepthStencilStateGetDepthWriteEnable` | 55, 231 | nvn-68 |
| `nvnDepthStencilStateGetStencilFunc` | 55, 231 | nvn-68 |
| `nvnDepthStencilStateGetStencilOp` | 55, 231 | nvn-68 |
| `nvnDepthStencilStateGetStencilTestEnable` | 55, 231 | nvn-68 |
| `nvnMultisampleStateGetAlphaToCoverageDither` | 55, 231 | nvn-69 |
| `nvnMultisampleStateGetAlphaToCoverageEnable` | 55, 231 | nvn-69 |
| `nvnMultisampleStateGetCoverageModulationMode` | 55, 231 | nvn-69 |
| `nvnMultisampleStateGetCoverageToColorEnable` | 55, 231 | nvn-69 |
| `nvnMultisampleStateGetCoverageToColorOutput` | 55, 231 | nvn-69 |
| `nvnMultisampleStateGetMultisampleEnable` | 55, 231 | nvn-69 |
| `nvnMultisampleStateGetRasterSamples` | 55, 231 | nvn-69 |
| `nvnMultisampleStateGetSampleLocationsEnable` | 55, 231 | nvn-69 |
| `nvnMultisampleStateGetSampleLocationsGridEnable` | 55, 231 | nvn-69 |
| `nvnMultisampleStateGetSamples` | 55, 231 | nvn-69 |
| `nvnMultisampleStateSetAlphaToCoverageDither` | 55, 231 | nvn-69 |
| `nvnMultisampleStateSetCoverageModulationMode` | 55, 231 | nvn-69 |
| `nvnMultisampleStateSetCoverageToColorEnable` | 55, 231 | nvn-69 |
| `nvnMultisampleStateSetCoverageToColorOutput` | 55, 231 | nvn-69 |
| `nvnMultisampleStateSetRasterSamples` | 55, 231 | nvn-69 |
| `nvnMultisampleStateSetCoverageTruncationEnable` | 55, 231 | nvn-69 |
| `nvnMultisampleStateGetCoverageTruncationEnable` | 55, 231 | nvn-69 |
| `nvnMultisampleStateSetSampleLocationsEnable` | 55, 231 | nvn-69 |
| `nvnMultisampleStateSetSampleLocationsGridEnable` | 55, 231 | nvn-69 |
| `nvnPolygonStateGetCullFace` | 55, 231 | nvn-70 |
| `nvnPolygonStateGetFrontFace` | 55, 231 | nvn-70 |
| `nvnPolygonStateGetPolygonMode` | 55, 231 | nvn-70 |
| `nvnPolygonStateGetPolygonOffsetEnables` | 55, 231 | nvn-70 |
| `nvnVertexAttribStateGetFormat` | 55, 231 | nvn-70 |
| `nvnVertexAttribStateGetStreamIndex` | 55, 231 | nvn-70 |
| `nvnVertexStreamStateGetDivisor` | 55, 231 | nvn-70 |
| `nvnVertexStreamStateGetStride` | 55, 231 | nvn-70 |

## Queues

30 functions, 11 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnQueueBuilderGetCommandFlushThreshold` | 55, 231 | nvn-59 |
| `nvnQueueBuilderGetCommandMemorySize` | 55, 231 | nvn-59 |
| `nvnQueueBuilderGetComputeMemorySize` | 55, 231 | nvn-59 |
| `nvnQueueBuilderGetControlMemorySize` | 55, 231 | nvn-59 |
| `nvnQueueBuilderGetDevice` | 55, 231 | nvn-59 |
| `nvnQueueBuilderGetFlags` | 55, 231 | nvn-59 |
| `nvnQueueBuilderGetMemory` |  |  |
| `nvnQueueBuilderGetMemorySize` |  |  |
| `nvnQueueGetTotalCommandMemoryUsed` |  |  |
| `nvnQueueGetTotalComputeMemoryUsed` |  |  |
| `nvnQueueGetTotalControlMemoryUsed` |  |  |
| `nvnQueueResetMemoryUsageCounts` |  |  |
| `nvnQueueBuilderGetQueueMemorySize` |  |  |
| `nvnQueueBuilderSetCommandFlushThreshold` | 55, 231 | nvn-59 |
| `nvnQueueBuilderSetCommandMemorySize` | 55, 231 | nvn-59 |
| `nvnQueueBuilderSetComputeMemorySize` | 55, 231 | nvn-59 |
| `nvnQueueBuilderSetControlMemorySize` | 55, 231 | nvn-59 |
| `nvnQueueBuilderSetDefaults` | 55, 231 | nvn-38, nvn-59, nvn-76, nvn-77, nvn-78, nvn2-1 |
| `nvnQueueBuilderSetDevice` | 55, 231 | nvn-38, nvn-59, nvn-76, nvn-77, nvn-78, nvn2-1 |
| `nvnQueueBuilderSetFlags` | 55, 231 | nvn-59 |
| `nvnQueueBuilderGetQueuePriority` |  |  |
| `nvnQueueBuilderSetQueuePriority` |  |  |
| `nvnQueueBuilderSetQueueMemory` |  |  |
| `nvnQueueFinalize` | 55, 231 | nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnQueueFinish` | 55, 231 | nvn-39, nvn-40, nvn-42, nvn-43, nvn-44, nvn-45, nvn-46, nvn-47, nvn-51, nvn-53, nvn-54, nvn-55, nvn-56, nvn-58, nvn-65, nvn-73, nvn-74, nvn-75, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106, nvn-107, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnQueueFlush` | 55, 231 | nvn-54, nvn-55, nvn-56 |
| `nvnQueueGetError` | 55, 231 | nvn-58, nvn-71 |
| `nvnQueueInitialize` | 55, 231 | nvn-38, nvn-76, nvn-77, nvn-78, nvn2-1 |
| `nvnQueueSubmitCommands` | 55, 231 | nvn-39, nvn-40, nvn-42, nvn-43, nvn-44, nvn-45, nvn-46, nvn-47, nvn-51, nvn-53, nvn-54, nvn-55, nvn-56, nvn-65, nvn-73, nvn-74, nvn-75, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106, nvn-107, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnQueueBuilderSetPrimaryQueue` |  |  |

## Sync and events

24 functions, 6 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnSyncCreateGLSync` |  |  |
| `nvnSyncInitializeFromFencedGLSync` |  |  |
| `nvnSyncFinalize` | 55, 231 | nvn-54, nvn-55, nvn-56, nvn-57, nvn-71, nvn-75 |
| `nvnSyncInitialize` | 55, 231 | nvn-54, nvn-55, nvn-56, nvn-57, nvn-71, nvn-75 |
| `nvnSyncWait` | 55, 231 | nvn-54, nvn-55, nvn-56, nvn-57, nvn-75 |
| `nvnSyncGetNvRmSyncNVX` |  |  |
| `nvnSyncInitializeFromNvRmSyncNVX` |  |  |
| `nvnEventBuilderGetMemoryOffset` | 55, 231 | nvn-60 |
| `nvnEventBuilderGetMemoryPool` | 55, 231 | nvn-60 |
| `nvnEventBuilderGetStorage` | 55, 231 | nvn-60 |
| `nvnEventGetMemoryOffset` | 55, 231 | nvn-60 |
| `nvnEventGetMemoryPool` | 55, 231 | nvn-60 |
| `nvnEventBuilderSetDefaults` | 55, 231 | nvn-60 |
| `nvnEventBuilderSetStorage` | 55, 231 | nvn-60 |
| `nvnEventFinalize` | 55, 231 | nvn-60 |
| `nvnEventGetValue` | 55, 231 | nvn-60 |
| `nvnEventInitialize` | 55, 231 | nvn-60 |
| `nvnEventSignal` | 55, 231 | nvn-60 |
| `nvnQueueFenceSync` | 55, 231 | nvn-54, nvn-56 |
| `nvnQueueWaitSync` | 55, 231 | nvn-54, nvn-55, nvn-56 |
| `nvnCommandBufferFenceSync` | 55, 231 | nvn-55 |
| `nvnCommandBufferSignalEvent` |  |  |
| `nvnCommandBufferWaitEvent` |  |  |
| `nvnCommandBufferWaitSync` | 55, 231 | nvn-56 |

## Windows and present

35 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnQueueAcquireTexture` | 55, 231 | nvn-73, nvn-74 |
| `nvnQueuePresentTexture` | 55, 231 | nvn-73, nvn-74, nvn-75 |
| `nvnWindowBuilderGetDevice` | 55, 231 | nvn-72 |
| `nvnWindowBuilderGetNativeWindow` | 55, 231 | nvn-72 |
| `nvnWindowBuilderGetNumActiveTextures` | 55, 231 | nvn-72 |
| `nvnWindowGetNativeWindow` | 55, 231 | nvn-74 |
| `nvnWindowGetNumActiveTextures` | 55, 231 | nvn-74 |
| `nvnWindowGetNumTextures` | 55, 231 | nvn-74 |
| `nvnWindowSetNumActiveTextures` | 55, 231 | nvn-74 |
| `nvnWindowBuilderGetNumTextures` | 55, 231 | nvn-72 |
| `nvnWindowBuilderGetPresentInterval` | 55, 231 | nvn-72 |
| `nvnWindowBuilderGetTexture` | 55, 231 | nvn-72 |
| `nvnWindowGetCrop` | 55, 231 | nvn-74 |
| `nvnWindowGetPresentInterval` | 55, 231 | nvn-74 |
| `nvnWindowAcquireTexture` | 55, 231 | nvn-75 |
| `nvnWindowBuilderSetDefaults` | 55, 231 | nvn-72, nvn-73, nvn-109 |
| `nvnWindowBuilderSetDevice` | 55, 231 | nvn-72, nvn-73 |
| `nvnWindowBuilderSetNativeWindow` | 55, 231 | nvn-72, nvn-73 |
| `nvnWindowBuilderSetNumActiveTextures` | 55, 231 | nvn-72 |
| `nvnWindowBuilderSetPresentInterval` | 55, 231 | nvn-72 |
| `nvnWindowBuilderSetTextures` | 55, 231 | nvn-72, nvn-73 |
| `nvnWindowBuilderSetColorSpace` | 231 | nvn-109 |
| `nvnWindowBuilderGetColorSpace` | 231 | nvn-109 |
| `nvnWindowBuilderSetHdrDisplayMetadata` | 231 | nvn-109 |
| `nvnWindowBuilderGetHdrDisplayMetadata` | 231 | nvn-109 |
| `nvnWindowBuilderSetHdrContentMetadata` | 231 | nvn-109 |
| `nvnWindowBuilderGetHdrContentMetadata` | 231 | nvn-109 |
| `nvnWindowSetHdrDisplayMetadata` | 231 | nvn-110 |
| `nvnWindowGetHdrDisplayMetadata` | 231 | nvn-110 |
| `nvnWindowSetHdrContentMetadata` | 231 | nvn-110 |
| `nvnWindowGetHdrContentMetadata` | 231 | nvn-110 |
| `nvnWindowFinalize` | 55, 231 | nvn-73 |
| `nvnWindowInitialize` | 55, 231 | nvn-73 |
| `nvnWindowSetCrop` | 55, 231 | nvn-74 |
| `nvnWindowSetPresentInterval` | 55, 231 | nvn-74 |

## Counters and queries

6 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnDeviceGetTimestampInNanoseconds` | 55, 231 | nvn-9, nvn-10, nvn-81, nvn-82 |
| `nvnCommandBufferReportCounter` | 55, 231 | nvn-79, nvn-81, nvn-82, nvn-83 |
| `nvnCommandBufferReportValue` | 55, 231 | nvn-80 |
| `nvnCommandBufferResetCounter` | 55, 231 | nvn-79, nvn-83 |
| `nvnCommandBufferSetRenderEnable` | 55, 231 | nvn-83 |
| `nvnCommandBufferSetRenderEnableConditional` | 55, 231 | nvn-83 |

## Commands: memory and recording

25 functions, 3 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferGetCommandMemoryFree` | 55, 231 | nvn-84 |
| `nvnCommandBufferGetCommandMemorySize` | 55, 231 | nvn-84 |
| `nvnCommandBufferGetControlMemoryFree` | 55, 231 | nvn-84 |
| `nvnCommandBufferGetControlMemorySize` | 55, 231 | nvn-84 |
| `nvnCommandBufferSetControlMemoryCallbackEnabled` | 55, 231 | nvn-85 |
| `nvnCommandBufferAddCommandMemory` | 55, 231 | nvn-38, nvn-76, nvn-77, nvn-78, nvn-84, nvn-85, nvn2-1 |
| `nvnCommandBufferAddControlMemory` | 55, 231 | nvn-38, nvn-76, nvn-77, nvn-78, nvn-84, nvn2-1 |
| `nvnCommandBufferBarrier` | 55, 231 | nvn-39, nvn-40, nvn-42, nvn-43, nvn-45, nvn-46, nvn-47, nvn-51, nvn-54, nvn-55, nvn-56, nvn-76, nvn-77, nvn-78, nvn-87, nvn-88, nvn-102, nvn-106, nvn-107, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferBeginRecording` | 55, 231 | nvn-39, nvn-40, nvn-42, nvn-43, nvn-44, nvn-45, nvn-46, nvn-47, nvn-51, nvn-53, nvn-54, nvn-55, nvn-56, nvn-65, nvn-73, nvn-74, nvn-75, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106, nvn-107, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferSetRecordingMode` |  |  |
| `nvnCommandBufferSetComputeOverlapMode` |  |  |
| `nvnCommandBufferCallCommands` | 55, 231 | nvn-86 |
| `nvnCommandBufferCopyCommands` | 55, 231 | nvn-86 |
| `nvnCommandBufferEndRecording` | 55, 231 | nvn-39, nvn-40, nvn-42, nvn-43, nvn-44, nvn-45, nvn-46, nvn-47, nvn-51, nvn-53, nvn-54, nvn-55, nvn-56, nvn-65, nvn-73, nvn-74, nvn-75, nvn-76, nvn-77, nvn-78, nvn-86, nvn-102, nvn-106, nvn-107, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferFinalize` | 55, 231 | nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnCommandBufferGetCommandMemoryUsed` | 55, 231 | nvn-84 |
| `nvnCommandBufferGetControlMemoryUsed` | 55, 231 | nvn-84 |
| `nvnCommandBufferGetMemoryCallback` | 55, 231 | nvn-85 |
| `nvnCommandBufferGetMemoryCallbackData` | 55, 231 | nvn-85 |
| `nvnCommandBufferInitialize` | 55, 231 | nvn-38, nvn-76, nvn-77, nvn-78, nvn2-1 |
| `nvnCommandBufferIsRecording` | 55, 231 | nvn-84 |
| `nvnCommandBufferSetCommandMemoryCallbackEnabled` | 55, 231 | nvn-85 |
| `nvnCommandBufferSetComputeWorkloadPriority` |  |  |
| `nvnCommandBufferSetMemoryCallback` | 55, 231 | nvn-85 |
| `nvnCommandBufferSetMemoryCallbackData` | 55, 231 | nvn-85 |

## Commands: binds

41 functions, 26 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferBindCoverageModulationTable` |  |  |
| `nvnCommandBufferBindImagesDeferred` |  |  |
| `nvnCommandBufferBindSeparateSamplersDeferred` |  |  |
| `nvnCommandBufferBindSeparateTexturesDeferred` |  |  |
| `nvnCommandBufferBindStorageBuffersDeferred` |  |  |
| `nvnCommandBufferBindUniformBuffersDeferred` |  |  |
| `nvnCommandBufferSetProgramSubroutines` |  |  |
| `nvnCommandBufferBindShadingRateImage` |  |  |
| `nvnCommandBufferShadingRateImagePalette` |  |  |
| `nvnCommandBufferPushGraphicsConstants` | 231 |  |
| `nvnCommandBufferPushComputeConstants` | 231 |  |
| `nvnCommandBufferBindBlendState` | 55, 231 | nvn-76, nvn-77, nvn-78, nvn-102, nvn-106, nvn2-7 |
| `nvnCommandBufferBindChannelMaskState` | 55, 231 | nvn-76, nvn-77, nvn-78, nvn-102, nvn-106, nvn2-7 |
| `nvnCommandBufferBindColorState` | 55, 231 | nvn-76, nvn-77, nvn-78, nvn-102, nvn-106, nvn2-7 |
| `nvnCommandBufferBindDepthStencilState` | 55, 231 | nvn-76, nvn-77, nvn-78, nvn-102, nvn-106, nvn2-7 |
| `nvnCommandBufferBindImage` | 231 |  |
| `nvnCommandBufferBindImages` |  |  |
| `nvnCommandBufferBindMultisampleState` | 55, 231 | nvn-76, nvn-77, nvn-78, nvn-102, nvn-106, nvn2-7 |
| `nvnCommandBufferBindPolygonState` | 55, 231 | nvn-76, nvn-77, nvn-78, nvn-102, nvn-106, nvn2-7 |
| `nvnCommandBufferBindProgram` | 55, 231 | nvn-76, nvn-77, nvn-78, nvn-102, nvn-105, nvn-106, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferBindSeparateSampler` |  |  |
| `nvnCommandBufferBindSeparateSamplers` |  |  |
| `nvnCommandBufferBindSeparateTexture` |  |  |
| `nvnCommandBufferBindSeparateTextures` |  |  |
| `nvnCommandBufferBindStorageBuffer` | 231 | nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferBindStorageBuffers` |  |  |
| `nvnCommandBufferBindTexture` | 55, 231 | nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnCommandBufferBindTextures` |  |  |
| `nvnCommandBufferBindTexturesDeferred` |  |  |
| `nvnCommandBufferBindTransformFeedbackBuffer` |  |  |
| `nvnCommandBufferBindTransformFeedbackBuffers` |  |  |
| `nvnCommandBufferBindUniformBuffer` | 55, 231 | nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnCommandBufferBindUniformBuffers` |  |  |
| `nvnCommandBufferBindVertexAttribState` | 55, 231 | nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnCommandBufferBindVertexBuffer` | 55, 231 | nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnCommandBufferBindVertexBuffers` |  |  |
| `nvnCommandBufferBindVertexStreamState` | 55, 231 | nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnCommandBufferSetSamplerPool` | 55, 231 | nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnCommandBufferSetShaderScratchMemory` |  |  |
| `nvnCommandBufferSetTexturePool` | 55, 231 | nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnCommandBufferUpdateUniformBuffer` |  |  |

## Commands: draws and dispatches

26 functions, 20 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferDrawTexture` |  |  |
| `nvnCommandBufferBeginTransformFeedback` |  |  |
| `nvnCommandBufferSetGeometryProcessingMode` | 231 | nvn2-7 |
| `nvnCommandBufferDrawMeshTasks` | 231 | nvn2-7 |
| `nvnCommandBufferDrawMeshTasks3D` |  |  |
| `nvnCommandBufferDrawMeshTasksIndirect` |  |  |
| `nvnCommandBufferDrawMeshTasks3DIndirect` |  |  |
| `nvnCommandBufferMultiDrawMeshTasksIndirect` |  |  |
| `nvnCommandBufferMultiDrawMeshTasks3DIndirect` |  |  |
| `nvnCommandBufferMultiDrawMeshTasksIndirectCount` |  |  |
| `nvnCommandBufferMultiDrawMeshTasks3DIndirectCount` |  |  |
| `nvnCommandBufferDispatchCompute` | 231 | nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferDispatchComputeIndirect` |  |  |
| `nvnCommandBufferDrawArrays` | 55, 231 | nvn-76, nvn-77, nvn-78, nvn-95, nvn-101, nvn-102, nvn-106 |
| `nvnCommandBufferDrawArraysIndirect` |  |  |
| `nvnCommandBufferDrawArraysInstanced` |  |  |
| `nvnCommandBufferDrawElements` | 55, 231 | nvn-76, nvn-77, nvn-78, nvn-96, nvn-102, nvn-106 |
| `nvnCommandBufferDrawElementsBaseVertex` | 55, 231 | nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnCommandBufferDrawElementsIndirect` |  |  |
| `nvnCommandBufferDrawElementsInstanced` |  |  |
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
| `nvnCommandBufferClearTexturei` | 55, 231 | nvn-88 |
| `nvnCommandBufferGetCopyImageStride` | 55, 231 | nvn-90 |
| `nvnCommandBufferGetCopyRowStride` | 55, 231 | nvn-90 |
| `nvnCommandBufferResolveDepthBuffer` |  |  |
| `nvnCommandBufferTiledDownsample` |  |  |
| `nvnCommandBufferTiledDownsampleTextureView` |  |  |
| `nvnCommandBufferClearBuffer` | 55, 231 | nvn-89 |
| `nvnCommandBufferClearColor` | 55, 231 | nvn-39, nvn-40, nvn-54, nvn-55, nvn-56, nvn-73, nvn-74, nvn-75, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106, nvn-107, nvn2-7 |
| `nvnCommandBufferClearColori` | 55, 231 | nvn-87 |
| `nvnCommandBufferClearColorui` | 55, 231 | nvn-87 |
| `nvnCommandBufferClearDepthStencil` | 55, 231 | nvn-76, nvn-77, nvn-78, nvn-102, nvn-106, nvn2-7 |
| `nvnCommandBufferClearTexture` | 55, 231 | nvn-88 |
| `nvnCommandBufferClearTextureui` | 55, 231 | nvn-88 |
| `nvnCommandBufferCopyBufferToBuffer` | 55, 231 | nvn-44, nvn-53, nvn-65, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferCopyBufferToTexture` | 55, 231 | nvn-42, nvn-43, nvn-45, nvn-46, nvn-47, nvn-51, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106 |
| `nvnCommandBufferCopyTextureToBuffer` | 55, 231 | nvn-39, nvn-40, nvn-42, nvn-43, nvn-45, nvn-46, nvn-47, nvn-51, nvn-54, nvn-55, nvn-56, nvn-76, nvn-77, nvn-78, nvn-87, nvn-88, nvn-102, nvn-106, nvn-107, nvn2-7 |
| `nvnCommandBufferCopyTextureToTexture` | 55, 231 | nvn-43, nvn-46 |
| `nvnCommandBufferDiscardColor` | 55, 231 | nvn-91 |
| `nvnCommandBufferDiscardDepthStencil` | 55, 231 | nvn-91 |
| `nvnCommandBufferDownsample` |  |  |
| `nvnCommandBufferDownsampleTextureView` |  |  |
| `nvnCommandBufferRestoreZCullData` |  |  |
| `nvnCommandBufferSaveZCullData` |  |  |
| `nvnCommandBufferSetCopyImageStride` | 55, 231 | nvn-90 |
| `nvnCommandBufferSetCopyRowStride` | 55, 231 | nvn-42, nvn-43, nvn-44, nvn-45, nvn-46, nvn-47, nvn-51, nvn-90 |

## Commands: render state

36 functions, 13 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferOverrideSubtileSize` |  |  |
| `nvnCommandBufferSetColorReductionEnable` |  |  |
| `nvnCommandBufferSetColorReductionThresholds` |  |  |
| `nvnCommandBufferSetConservativeRasterDilate` | 55, 231 | nvn-100 |
| `nvnCommandBufferSetInnerTessellationLevels` | 55, 231 | nvn-102 |
| `nvnCommandBufferSetOuterTessellationLevels` | 55, 231 | nvn-102 |
| `nvnCommandBufferSetProvokingVertexMode` | 55, 231 | nvn-94 |
| `nvnCommandBufferSetSubpixelPrecisionBias` | 55, 231 | nvn-99 |
| `nvnCommandBufferSetZCullZF32CompressionEnable` |  |  |
| `nvnCommandBufferSetShadingRatePaletteMode` |  |  |
| `nvnCommandBufferSetAlphaRef` |  |  |
| `nvnCommandBufferSetBlendColor` |  |  |
| `nvnCommandBufferSetConservativeRasterEnable` | 55, 231 | nvn-99, nvn-100 |
| `nvnCommandBufferSetDepthBounds` | 55, 231 | nvn-93 |
| `nvnCommandBufferSetDepthClamp` | 55, 231 | nvn-94 |
| `nvnCommandBufferSetDepthRange` | 55, 231 | nvn-93 |
| `nvnCommandBufferSetDepthRanges` | 55, 231 | nvn-93 |
| `nvnCommandBufferSetLineWidth` | 55, 231 | nvn-101 |
| `nvnCommandBufferSetPatchSize` | 55, 231 | nvn-102 |
| `nvnCommandBufferSetPointSize` | 55, 231 | nvn-95 |
| `nvnCommandBufferSetPolygonOffsetClamp` |  |  |
| `nvnCommandBufferSetPrimitiveRestart` | 55, 231 | nvn-96 |
| `nvnCommandBufferSetRasterizerDiscard` | 55, 231 | nvn-94 |
| `nvnCommandBufferSetRenderTargets` | 55, 231 | nvn-39, nvn-40, nvn-54, nvn-55, nvn-56, nvn-73, nvn-74, nvn-75, nvn-76, nvn-77, nvn-78, nvn-87, nvn-102, nvn-106, nvn-107, nvn2-7 |
| `nvnCommandBufferSetSampleMask` |  |  |
| `nvnCommandBufferSetScissor` | 55, 231 | nvn-39, nvn-40, nvn-54, nvn-55, nvn-56, nvn-73, nvn-74, nvn-75, nvn-76, nvn-77, nvn-78, nvn-87, nvn-102, nvn-106, nvn-107, nvn2-7 |
| `nvnCommandBufferSetScissors` | 55, 231 | nvn-92 |
| `nvnCommandBufferSetStencilCullCriteria` |  |  |
| `nvnCommandBufferSetStencilMask` |  |  |
| `nvnCommandBufferSetStencilRef` |  |  |
| `nvnCommandBufferSetStencilValueMask` |  |  |
| `nvnCommandBufferSetTiledCacheAction` | 55, 231 | nvn-98 |
| `nvnCommandBufferSetTiledCacheTileSize` | 55, 231 | nvn-98 |
| `nvnCommandBufferSetViewport` | 55, 231 | nvn-39, nvn-40, nvn-54, nvn-55, nvn-56, nvn-73, nvn-74, nvn-75, nvn-76, nvn-77, nvn-78, nvn-102, nvn-106, nvn-107, nvn2-7 |
| `nvnCommandBufferSetViewportSwizzles` | 55, 231 | nvn-97 |
| `nvnCommandBufferSetViewports` | 55, 231 | nvn-92 |

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
| `nvnProgramBuilderSetMaxCallableRecursionDepth` | 231 | nvn-108 |
| `nvnProgramBuilderGetMaxCallableRecursionDepth` | 231 | nvn-108 |
| `nvnProgramBuilderSetMaxTraceRecursionDepth` | 231 | nvn-108 |
| `nvnProgramBuilderGetMaxTraceRecursionDepth` | 231 | nvn-108 |
| `nvnProgramBuilderSetProgramRayTracingFlags` | 231 | nvn-108 |
| `nvnProgramBuilderGetProgramRayTracingFlags` | 231 | nvn-108 |
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
| `nvnSetExternalDebuggerEnabledNVX` | 55, 231 | nvn-71 |
| `nvnDeviceGenerateDebugDomainId` | 55, 231 | nvn-71, nvn-103, nvn-107 |
| `nvnDeviceInstallDebugCallback` | 55, 231 | nvn-71 |
| `nvnDeviceIsExternalDebuggerAttached` | 55, 231 | nvn-71 |
| `nvnDeviceSetAutomaticCheckpointsByDebugGroup` | 55, 231 | nvn-71 |
| `nvnDeviceSetAutomaticCheckpointsEnable` | 55, 231 | nvn-71 |
| `nvnDeviceSetAutomaticCheckpointsMask` | 55, 231 | nvn-71 |
| `nvnDeviceSetAutomaticCheckpointsSamplingInterval` | 55, 231 | nvn-71 |
| `nvnDeviceSetCheckpointCallStacksEnable` | 55, 231 | nvn-71 |
| `nvnDeviceSetCheckpointMaxCallStacksDepth` | 55, 231 | nvn-71 |
| `nvnDeviceWalkDebugDatabase` | 55, 231 | nvn-71 |
| `nvnMemoryPoolSetDebugLabel` | 55, 231 | nvn-71 |
| `nvnBufferGetDebugID` | 55, 231 | nvn-71 |
| `nvnBufferSetDebugLabel` | 55, 231 | nvn-71 |
| `nvnTextureGetDebugID` | 55, 231 | nvn-71 |
| `nvnTextureSetDebugLabel` | 55, 231 | nvn-71 |
| `nvnSamplerGetDebugID` | 55, 231 | nvn-71 |
| `nvnSamplerSetDebugLabel` | 55, 231 | nvn-71 |
| `nvnProgramBuilderSetDebugDataHashes` |  |  |
| `nvnProgramBuilderGetDebugDataHashes` |  |  |
| `nvnCommandBufferInsertCheckpointDynamic` | 55, 231 | nvn-103 |
| `nvnCommandBufferInsertCheckpointStatic` | 55, 231 | nvn-103 |
| `nvnCommandBufferInsertDebugMarkerGraphics` | 231 | nvn-107 |
| `nvnCommandBufferInsertDebugMarkerCompute` | 231 | nvn-107 |
| `nvnCommandBufferPushDebugGroupGraphics` | 231 | nvn-107 |
| `nvnCommandBufferPushDebugGroupCompute` | 231 | nvn-107 |
| `nvnCommandBufferPopDebugGroupGraphics` | 231 | nvn-107 |
| `nvnCommandBufferPopDebugGroupCompute` | 231 | nvn-107 |
| `nvnCommandBufferInsertDebugMarker` | 55, 231 | nvn-103 |
| `nvnCommandBufferInsertDebugMarkerDynamic` | 55, 231 | nvn-103 |
| `nvnCommandBufferInsertDebugMarkerStatic` | 55, 231 | nvn-103 |
| `nvnCommandBufferPopDebugGroup` | 55, 231 | nvn-103 |
| `nvnCommandBufferPopDebugGroupId` | 55, 231 | nvn-103 |
| `nvnCommandBufferPushDebugGroup` | 55, 231 | nvn-103 |
| `nvnCommandBufferPushDebugGroupDynamic` | 55, 231 | nvn-103 |
| `nvnCommandBufferPushDebugGroupStatic` | 55, 231 | nvn-103 |
| `nvnCommandBufferSetDebugLabel` | 55, 231 | nvn-71 |
| `nvnDeviceSetDebugLabel` | 55, 231 | nvn-71 |
| `nvnProgramSetDebugLabel` | 55, 231 | nvn-71 |
| `nvnQueueSetDebugLabel` | 55, 231 | nvn-71 |
| `nvnSamplerPoolSetDebugLabel` | 55, 231 | nvn-71 |
| `nvnSyncSetDebugLabel` | 55, 231 | nvn-71 |
| `nvnTexturePoolSetDebugLabel` | 55, 231 | nvn-71 |
| `nvnWindowSetDebugLabel` | 55, 231 | nvn-74 |
