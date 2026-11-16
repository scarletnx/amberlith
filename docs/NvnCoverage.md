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
names 677; the layer has 350 of them in its API 55 build and 390 in its API 231 build; a case calls 385. The
other 292 have no call at all: a runtime could answer any of them wrongly and every case would still pass.

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
| [Device](#device) | 36 | 19 | 20 | 19 | 17 |
| [Memory pools](#memory-pools) | 18 | 16 | 16 | 16 | 2 |
| [Buffers](#buffers) | 16 | 16 | 16 | 16 | 0 |
| [Textures](#textures) | 95 | 53 | 54 | 53 | 42 |
| [Samplers](#samplers) | 45 | 45 | 45 | 45 | 0 |
| [Programs](#programs) | 20 | 4 | 13 | 13 | 7 |
| [State objects](#state-objects) | 82 | 16 | 16 | 16 | 66 |
| [Queues](#queues) | 30 | 19 | 19 | 19 | 11 |
| [Sync and events](#sync-and-events) | 24 | 18 | 18 | 18 | 6 |
| [Windows and present](#windows-and-present) | 35 | 25 | 35 | 35 | 0 |
| [Counters and queries](#counters-and-queries) | 6 | 6 | 6 | 6 | 0 |
| [Commands: memory and recording](#commands-memory-and-recording) | 25 | 22 | 22 | 22 | 3 |
| [Commands: binds](#commands-binds) | 41 | 14 | 18 | 15 | 26 |
| [Commands: draws and dispatches](#commands-draws-and-dispatches) | 26 | 3 | 6 | 6 | 20 |
| [Commands: clears and copies](#commands-clears-and-copies) | 25 | 18 | 18 | 18 | 7 |
| [Commands: render state](#commands-render-state) | 36 | 20 | 20 | 20 | 16 |
| [DLSS](#dlss) | 58 | 0 | 0 | 0 | 58 |
| [Ray tracing](#ray-tracing) | 15 | 0 | 6 | 6 | 9 |
| [Debug](#debug) | 44 | 36 | 42 | 42 | 2 |
| **All** | **677** | **350** | **390** | **385** | **292** |

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
| nvn-30 | nvn | draws between debug markers, groups and checkpoints are as without them, through the layer's own NVN functions | the NRO |
| nvn-31 | nvn | the device says its modes and its builder its flags, and clear values register, through the layer's own NVN functions | the NRO |
| nvn-32 | nvn | a program set from the longer shader data draws, through the layer's own NVN functions | the NRO |
| nvn-33 | nvn | the program's NVN functions resolve | the NRO, the Switch 2 program |
| nvn-34 | nvn | the device initializes | the NRO, the Switch 2 program |
| nvn-35 | nvn | the device answers the layer's API version | the NRO, the Switch 2 program |
| nvn-36 | nvn | a queue, its pools and a 64x64 target come up | the NRO, the Switch 2 program |
| nvn-37 | nvn | clear to opaque red | the NRO, the Switch 2 program |
| nvn-38 | nvn | clear to a mid channel value | the NRO, the Switch 2 program |
| nvn-39 | nvn | two copy textures and a staging pool come up | the NRO, the Switch 2 program |
| nvn-40 | nvn | a picture copied into a texture and back keeps every texel | the NRO, the Switch 2 program |
| nvn-41 | nvn | a texture copied into another keeps every texel | the NRO, the Switch 2 program |
| nvn-42 | nvn | a buffer copied into another keeps every byte | the NRO, the Switch 2 program |
| nvn-43 | nvn | a texture copied to a buffer with MIRROR_Y lands upside down | the NRO, the Switch 2 program |
| nvn-44 | nvn | a texture copied into another with MIRROR_X lands mirrored | the NRO, the Switch 2 program |
| nvn-45 | nvn | a copy to a buffer with a row stride of 288 keeps the bytes between rows | the NRO, the Switch 2 program |
| nvn-46 | nvn | a texture builder gives back each number it was set | the NRO, the Switch 2 program |
| nvn-47 | nvn | a texture view says which of its parts were set, and with what | the NRO, the Switch 2 program |
| nvn-48 | nvn | a texture of three levels comes up and says what its builder had | the NRO, the Switch 2 program |
| nvn-49 | nvn | a picture copied into each level of a texture through a view comes back from that level | the NRO, the Switch 2 program |
| nvn-50 | nvn | a memory pool and its builder give back what they were set, and the pool maps its storage | the NRO, the Switch 2 program |
| nvn-51 | nvn | a page flushed from a cached pool is what a copy reads, and the copy is there after an invalidate | the NRO, the Switch 2 program |
| nvn-52 | nvn | a sync fenced on the queue behind a clear signals, and the clear is there with no finish | the NRO, the Switch 2 program |
| nvn-53 | nvn | a sync fenced inside the list behind a clear signals, and the clear is there with no finish | the NRO, the Switch 2 program |
| nvn-54 | nvn | a queue and a list told to wait for a sync that signalled run their clear | the NRO, the Switch 2 program |
| nvn-55 | nvn | a wait on a sync nothing fenced fails, with a timeout or without | the NRO, the Switch 2 program |
| nvn-56 | nvn | the queue has no error after work that was valid | the NRO, the Switch 2 program |
| nvn-57 | nvn | a queue builder gives back its flags and says which of its sizes were set | the NRO, the Switch 2 program |
| nvn-58 | nvn | an event keeps what the processor signals: a write, an add and an add that wraps | the NRO, the Switch 2 program |
| nvn-59 | nvn | a sampler builder gives back each number it was set | the NRO, the Switch 2 program |
| nvn-60 | nvn | a sampler says what its builder had, and is the same as another of that state alone | the NRO, the Switch 2 program |
| nvn-61 | nvn | the texture and sampler pools say where they are, and a builder registers as its sampler | the NRO, the Switch 2 program |
| nvn-62 | nvn | a buffer is a range of its pool: its address, its mapping, and what it and its builder say | the NRO, the Switch 2 program |
| nvn-63 | nvn | bytes flushed from one buffer and copied to another are there after an invalidate | the NRO, the Switch 2 program |
| nvn-64 | nvn | the debug layer's calls are answered as a release driver answers them, and a label changes no object | the NRO, the Switch 2 program |
| nvn-65 | nvn | a window builder gives back what it was set | the NRO, the Switch 2 program |
| nvn-66 | nvn | a window acquires and presents both its textures | the NRO, the Switch 2 program |
| nvn-67 | nvn | a window says what it is of, and keeps the active textures, the interval and the crop it is set | the NRO, the Switch 2 program |
| nvn-68 | nvn | a texture the window hands with a sync is free once the sync signals, and presents | the NRO, the Switch 2 program |
| nvn-69 | nvn | the same scene through the NVN the program looks up holds what the processor works out | the NRO |
| nvn-70 | nvn | the same texture past its edges through the NVN the program looks up does the same | the NRO |
| nvn-71 | nvn | the same four indexed quads through the NVN the program looks up are each in its quarter | the NRO |
| nvn-72 | nvn | the samples counter reports a quad's 256 pixels and none after a reset, through the NVN the program looks up | the NRO |
| nvn-73 | nvn | two values a list reports are in memory once it has run, through the NVN the program looks up | the NRO |
| nvn-74 | nvn | two timestamp reports hold no count and times between the device's clock before and after, through the NVN the program looks up | the NRO |
| nvn-75 | nvn | a timestamp report's word is the graphics unit's ticks, 384 every 625 nanoseconds, through the NVN the program looks up | the NRO |
| nvn-76 | nvn | the render enable draws under on and under a condition that holds and skips the rest, through the NVN the program looks up | the NRO |
| nvn-77 | nvn | a command buffer's memory used and free come to what it was given, and draws take some, through the NVN the program looks up | the NRO |
| nvn-78 | nvn | a list out of command memory asks the program's callback and draws on, through the NVN the program looks up | the NRO |
| nvn-79 | nvn | a list called from another and a list copied into it draw their cells, through the NVN the program looks up | the NRO |
| nvn-80 | nvn | clears of an unsigned and a signed integer target hold the integers given, through the NVN the program looks up | the NRO |
| nvn-81 | nvn | a clear of a texture that is no target reaches its region and its mask's channels, through the NVN the program looks up | the NRO |
| nvn-82 | nvn | a range of a buffer cleared to a word holds it, and nothing beside the range, through the NVN the program looks up | the NRO |
| nvn-83 | nvn | a command buffer gives back the copy strides it was set, through the NVN the program looks up | the NRO |
| nvn-84 | nvn | targets discarded, then cleared and drawn, hold the clear and the draw, through the NVN the program looks up | the NRO |
| nvn-85 | nvn | a scissor and a viewport set by number bound and place a quad, through the NVN the program looks up | the NRO |
| nvn-86 | nvn | depth ranges move a quad across the depth test and depth bounds test the target's depth, through the NVN the program looks up | the NRO |
| nvn-87 | nvn | the rasterizer discards, clamps depth and takes a flat colour from the provoking vertex, through the NVN the program looks up | the NRO |
| nvn-88 | nvn | a point of sixteen pixels fills its cell, through the NVN the program looks up | the NRO |
| nvn-89 | nvn | a strip with a restart index is two quads, through the NVN the program looks up | the NRO |
| nvn-90 | nvn | a viewport swizzle of x and y turns a quad about the diagonal, through the NVN the program looks up | the NRO |
| nvn-91 | nvn | draws around the tiled cache's actions are as without them, through the NVN the program looks up | the NRO |
| nvn-92 | nvn | conservative rasterization draws every pixel a quad touches, through the NVN the program looks up | the NRO |
| nvn-93 | nvn | a conservative quad dilated by half a pixel reaches the pixels a third of one away, through the NVN the program looks up | the NRO |
| nvn-94 | nvn | a line of width four is four rows of pixels, through the NVN the program looks up | the NRO |
| nvn-95 | nvn | draws between debug markers, groups and checkpoints are as without them, through the NVN the program looks up | the NRO |
| nvn-96 | nvn | the device says its modes and its builder its flags, and clear values register, through the NVN the program looks up | the NRO |
| nvn-97 | nvn | a program set from the longer shader data draws, through the NVN the program looks up | the NRO |
| nvn-98 | nvn | a clear between the Ounce's graphics and compute markers and groups is the clear | the Switch 2 program |
| nvn-99 | nvn | an Ounce program builder gives back what it was set, a ray tracing program's numbers too | the Switch 2 program |
| nvn-100 | nvn | an Ounce window builder starts with the driver's colour space and HDR metadata, and gives back what it is set | the Switch 2 program |
| nvn-101 | nvn | an Ounce window has its builder's HDR metadata, and gives back what it is set | the Switch 2 program |
| nvn2-1 | nvn2 | a device, a queue, a command buffer, their pools and a 256x256 target with its depth come up | the Switch 2 program |
| nvn2-2 | nvn2 | a compute program assembled by hand makes a program through the builder | the Switch 2 program |
| nvn2-3 | nvn2 | a dispatch's writes to a storage buffer, as the processor reads them once the queue finishes | the Switch 2 program |
| nvn2-4 | nvn2 | a dispatch's writes to a storage buffer, as the graphics unit copies them to another buffer | the Switch 2 program |
| nvn2-5 | nvn2 | the Mandelbrot's compute, mesh and fragment programs make two programs | the Switch 2 program |
| nvn2-6 | nvn2 | a compute program counts the Mandelbrot set's steps for every cell of a grid | the Switch 2 program |
| nvn2-7 | nvn2 | a mesh program draws the counts as bars, a fragment program colours them and the depth test keeps the nearest | the Switch 2 program |
| nvn2-8 | nvn2 | one warp's matrix instruction multiplies a 16 by 16 matrix of halves by a 16 by 8 one into floats | the Switch 2 program |

## Device

36 functions, 17 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnBootstrapLoader` | 55, 231 | nvn-33, nvn-69, nvn-70, nvn-71, nvn2-1 |
| `nvnDeviceGetProcAddress` | 55, 231 | nvn-33, nvn-69, nvn-70, nvn-71, nvn2-1 |
| `nvnDeviceApplyDeferredFinalizes` |  |  |
| `nvnDeviceBuilderGetFlags` | 55, 231 | nvn-96 |
| `nvnDeviceFinalize` | 55, 231 | nvn-69, nvn-70, nvn-71 |
| `nvnDeviceRegisterFastClearColori` | 55, 231 | nvn-96 |
| `nvnDeviceRegisterFastClearColorui` | 55, 231 | nvn-96 |
| `nvnDeviceSetIntermediateShaderCache` |  |  |
| `nvnDeviceBuilderSetDefaults` | 55, 231 | nvn-34, nvn-69, nvn-70, nvn-71, nvn-96, nvn2-1 |
| `nvnDeviceBuilderSetFlags` | 55, 231 | nvn-34, nvn-69, nvn-70, nvn-71, nvn-96, nvn2-1 |
| `nvnDeviceGetImageHandle` | 231 |  |
| `nvnDeviceFinalizeCommandHandle` |  |  |
| `nvnDeviceGetCurrentTimestampInNanoseconds` | 55, 231 | nvn-9, nvn-10, nvn-74, nvn-75 |
| `nvnDeviceGetDepthMode` | 55, 231 | nvn-96 |
| `nvnDeviceGetInteger` | 55, 231 | nvn-35, nvn-69, nvn-70, nvn-71, nvn2-1 |
| `nvnDeviceGetWindowOriginMode` | 55, 231 | nvn-96 |
| `nvnDeviceGetSeparateSamplerHandle` |  |  |
| `nvnDeviceGetSeparateTextureHandle` |  |  |
| `nvnDeviceGetTexelFetchHandle` |  |  |
| `nvnDeviceGetTextureHandle` | 55, 231 | nvn-69, nvn-70, nvn-71 |
| `nvnDeviceInitialize` | 55, 231 | nvn-34, nvn-69, nvn-70, nvn-71, nvn2-1 |
| `nvnDeviceRegisterFastClearColor` | 55, 231 | nvn-96 |
| `nvnDeviceRegisterFastClearDepth` | 55, 231 | nvn-96 |
| `nvnDeviceRegisterFastClearStencil` | 55, 231 | nvn-96 |
| `nvnDeviceSetDepthMode` | 55, 231 | nvn-69, nvn-70, nvn-71 |
| `nvnDeviceSetWindowOriginMode` | 55, 231 | nvn-69, nvn-70, nvn-71 |
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
| `nvnMemoryPoolBuilderGetDevice` | 55, 231 | nvn-50 |
| `nvnMemoryPoolBuilderGetFlags` | 55, 231 | nvn-50 |
| `nvnMemoryPoolBuilderGetMemory` | 55, 231 | nvn-50 |
| `nvnMemoryPoolBuilderGetSize` | 55, 231 | nvn-50 |
| `nvnMemoryPoolBuilderSetDefaults` | 55, 231 | nvn-15, nvn-16, nvn-36, nvn-39, nvn-48, nvn-50, nvn-66, nvn-69, nvn-70, nvn-71, nvn-80, nvn-81, nvn2-1 |
| `nvnMemoryPoolBuilderSetDevice` | 55, 231 | nvn-15, nvn-16, nvn-36, nvn-39, nvn-48, nvn-50, nvn-66, nvn-69, nvn-70, nvn-71, nvn-80, nvn-81, nvn2-1 |
| `nvnMemoryPoolBuilderSetFlags` | 55, 231 | nvn-15, nvn-16, nvn-36, nvn-39, nvn-48, nvn-50, nvn-66, nvn-69, nvn-70, nvn-71, nvn-80, nvn-81, nvn2-1 |
| `nvnMemoryPoolBuilderSetStorage` | 55, 231 | nvn-15, nvn-16, nvn-36, nvn-39, nvn-48, nvn-50, nvn-66, nvn-69, nvn-70, nvn-71, nvn-80, nvn-81, nvn2-1 |
| `nvnMemoryPoolBuilderSetNativeHandleNVX` |  |  |
| `nvnMemoryPoolFinalize` | 55, 231 | nvn-15, nvn-16, nvn-66, nvn-69, nvn-70, nvn-71, nvn-80, nvn-81 |
| `nvnMemoryPoolFlushMappedRange` | 55, 231 | nvn-51, nvn-63 |
| `nvnMemoryPoolGetBufferAddress` | 55, 231 | nvn-37, nvn-38, nvn-40, nvn-41, nvn-42, nvn-43, nvn-44, nvn-45, nvn-49, nvn-51, nvn-52, nvn-53, nvn-54, nvn-62, nvn-69, nvn-70, nvn-71, nvn-98, nvn2-2, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-8 |
| `nvnMemoryPoolGetFlags` | 55, 231 | nvn-50 |
| `nvnMemoryPoolGetSize` | 55, 231 | nvn-50, nvn-64 |
| `nvnMemoryPoolInitialize` | 55, 231 | nvn-15, nvn-16, nvn-36, nvn-39, nvn-48, nvn-50, nvn-66, nvn-69, nvn-70, nvn-71, nvn-80, nvn-81, nvn2-1 |
| `nvnMemoryPoolInvalidateMappedRange` | 55, 231 | nvn-51 |
| `nvnMemoryPoolMap` | 55, 231 | nvn-37, nvn-38, nvn-39, nvn-40, nvn-41, nvn-42, nvn-43, nvn-44, nvn-45, nvn-49, nvn-50, nvn-51, nvn-52, nvn-53, nvn-54, nvn-58, nvn-61, nvn-62, nvn-63, nvn-69, nvn-70, nvn-71, nvn-98, nvn2-2, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-8 |
| `nvnMemoryPoolMapVirtual` |  |  |

## Buffers

16 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnBufferBuilderGetDevice` | 55, 231 | nvn-62 |
| `nvnBufferBuilderGetMemoryOffset` | 55, 231 | nvn-62 |
| `nvnBufferBuilderGetMemoryPool` | 55, 231 | nvn-62 |
| `nvnBufferBuilderGetSize` | 55, 231 | nvn-62 |
| `nvnBufferBuilderSetDefaults` | 55, 231 | nvn-62, nvn-63, nvn-64 |
| `nvnBufferBuilderSetDevice` | 55, 231 | nvn-62, nvn-63, nvn-64 |
| `nvnBufferBuilderSetStorage` | 55, 231 | nvn-62, nvn-63, nvn-64 |
| `nvnBufferFinalize` | 55, 231 | nvn-62, nvn-63, nvn-64 |
| `nvnBufferFlushMappedRange` | 55, 231 | nvn-63 |
| `nvnBufferInvalidateMappedRange` | 55, 231 | nvn-63 |
| `nvnBufferGetAddress` | 55, 231 | nvn-62, nvn-63 |
| `nvnBufferGetMemoryOffset` | 55, 231 | nvn-62 |
| `nvnBufferGetMemoryPool` | 55, 231 | nvn-62 |
| `nvnBufferGetSize` | 55, 231 | nvn-62, nvn-64 |
| `nvnBufferInitialize` | 55, 231 | nvn-62, nvn-63, nvn-64 |
| `nvnBufferMap` | 55, 231 | nvn-62, nvn-63 |

## Textures

95 functions, 42 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnTextureBuilderGetGLTextureName` |  |  |
| `nvnTextureBuilderGetMemoryOffset` | 55, 231 | nvn-46 |
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
| `nvnTextureViewCompare` | 55, 231 | nvn-47 |
| `nvnTextureBuilderGetDepthStencilMode` |  |  |
| `nvnTextureBuilderGetDevice` | 55, 231 | nvn-46 |
| `nvnTextureBuilderGetMemoryPool` | 55, 231 | nvn-46 |
| `nvnTextureBuilderGetPackagedTextureData` |  |  |
| `nvnTextureBuilderGetSwizzle` |  |  |
| `nvnTextureBuilderGetDepth` | 55, 231 | nvn-46 |
| `nvnTextureBuilderGetFlags` | 55, 231 | nvn-46 |
| `nvnTextureBuilderGetFormat` | 55, 231 | nvn-46 |
| `nvnTextureBuilderGetHeight` | 55, 231 | nvn-46 |
| `nvnTextureBuilderGetLevels` | 55, 231 | nvn-46 |
| `nvnTextureBuilderGetSamples` | 55, 231 | nvn-46 |
| `nvnTextureBuilderGetStorageAlignment` | 55, 231 | nvn-15, nvn-16, nvn-36, nvn-48, nvn-66, nvn-69, nvn-70, nvn-71, nvn-80, nvn-81, nvn2-1 |
| `nvnTextureBuilderGetStorageClass` |  |  |
| `nvnTextureBuilderGetStorageSize` | 55, 231 | nvn-15, nvn-16, nvn-36, nvn-39, nvn-48, nvn-66, nvn-69, nvn-70, nvn-71, nvn-80, nvn-81, nvn2-1 |
| `nvnTextureBuilderGetStride` |  |  |
| `nvnTextureBuilderGetTarget` | 55, 231 | nvn-46 |
| `nvnTextureBuilderGetWidth` | 55, 231 | nvn-46 |
| `nvnTextureBuilderGetZCullStorageSize` |  |  |
| `nvnTextureBuilderSetDefaults` | 55, 231 | nvn-15, nvn-16, nvn-36, nvn-39, nvn-46, nvn-48, nvn-66, nvn-69, nvn-70, nvn-71, nvn-80, nvn-81, nvn2-1 |
| `nvnTextureBuilderSetDepth` | 55, 231 | nvn-46, nvn-48 |
| `nvnTextureBuilderSetDepthStencilMode` |  |  |
| `nvnTextureBuilderSetDevice` | 55, 231 | nvn-15, nvn-16, nvn-36, nvn-39, nvn-46, nvn-48, nvn-66, nvn-69, nvn-70, nvn-71, nvn-80, nvn-81, nvn2-1 |
| `nvnTextureBuilderSetFlags` | 55, 231 | nvn-36, nvn-39, nvn-46, nvn-48, nvn-66, nvn-69, nvn-70, nvn-71, nvn2-1 |
| `nvnTextureBuilderSetFormat` | 55, 231 | nvn-15, nvn-16, nvn-36, nvn-39, nvn-46, nvn-48, nvn-66, nvn-69, nvn-70, nvn-71, nvn-80, nvn-81, nvn2-1 |
| `nvnTextureBuilderSetHeight` | 55, 231 | nvn-46 |
| `nvnTextureBuilderSetLevels` | 55, 231 | nvn-46, nvn-48 |
| `nvnTextureBuilderSetPackagedTextureData` |  |  |
| `nvnTextureBuilderSetPackagedTextureLayout` |  |  |
| `nvnTextureBuilderSetSamples` | 55, 231 | nvn-46, nvn-48 |
| `nvnTextureBuilderSetSize1D` |  |  |
| `nvnTextureBuilderSetSize2D` | 55, 231 | nvn-15, nvn-16, nvn-36, nvn-39, nvn-46, nvn-48, nvn-66, nvn-69, nvn-70, nvn-71, nvn-80, nvn-81, nvn2-1 |
| `nvnTextureBuilderSetSize3D` |  |  |
| `nvnTextureBuilderSetStorage` | 55, 231 | nvn-15, nvn-16, nvn-36, nvn-39, nvn-46, nvn-48, nvn-66, nvn-69, nvn-70, nvn-71, nvn-80, nvn-81, nvn2-1 |
| `nvnTextureBuilderSetStride` |  |  |
| `nvnTextureBuilderSetSwizzle` |  |  |
| `nvnTextureBuilderSetTarget` | 55, 231 | nvn-15, nvn-16, nvn-36, nvn-39, nvn-46, nvn-48, nvn-66, nvn-69, nvn-70, nvn-71, nvn-80, nvn-81, nvn2-1 |
| `nvnTextureBuilderSetWidth` | 55, 231 | nvn-46 |
| `nvnTextureCompare` |  |  |
| `nvnTextureFinalize` | 55, 231 | nvn-15, nvn-16, nvn-66, nvn-69, nvn-70, nvn-71, nvn-80, nvn-81 |
| `nvnTextureFlushTexels` |  |  |
| `nvnTextureGetDepth` | 55, 231 | nvn-48 |
| `nvnTextureGetDepthStencilMode` |  |  |
| `nvnTextureGetFlags` | 55, 231 | nvn-48 |
| `nvnTextureGetFormat` | 55, 231 | nvn-48 |
| `nvnTextureGetHeight` | 55, 231 | nvn-48 |
| `nvnTextureGetLevels` | 55, 231 | nvn-48 |
| `nvnTextureGetMemoryOffset` | 55, 231 | nvn-48 |
| `nvnTextureGetMemoryPool` | 55, 231 | nvn-48 |
| `nvnTextureGetSamples` | 55, 231 | nvn-48 |
| `nvnTextureGetStorageClass` |  |  |
| `nvnTextureGetStorageSize` |  |  |
| `nvnTextureGetStride` |  |  |
| `nvnTextureGetSwizzle` |  |  |
| `nvnTextureGetTarget` | 55, 231 | nvn-48 |
| `nvnTextureGetTextureAddress` |  |  |
| `nvnTextureGetViewOffset` |  |  |
| `nvnTextureGetWidth` | 55, 231 | nvn-48, nvn-64 |
| `nvnTextureGetZCullStorageSize` |  |  |
| `nvnTextureInitialize` | 55, 231 | nvn-15, nvn-16, nvn-36, nvn-39, nvn-48, nvn-66, nvn-69, nvn-70, nvn-71, nvn-80, nvn-81, nvn2-1 |
| `nvnTexturePoolFinalize` | 55, 231 | nvn-61, nvn-64, nvn-69, nvn-70, nvn-71 |
| `nvnTexturePoolGetMemoryOffset` | 55, 231 | nvn-61 |
| `nvnTexturePoolGetMemoryPool` | 55, 231 | nvn-61 |
| `nvnTexturePoolGetSize` | 55, 231 | nvn-61, nvn-64 |
| `nvnTexturePoolInitialize` | 55, 231 | nvn-61, nvn-64, nvn-69, nvn-70, nvn-71 |
| `nvnTexturePoolRegisterImage` | 231 |  |
| `nvnTexturePoolRegisterTexture` | 55, 231 | nvn-69, nvn-70, nvn-71 |
| `nvnTextureReadTexels` |  |  |
| `nvnTextureViewGetDepthStencilMode` |  |  |
| `nvnTextureViewGetFormat` | 55, 231 | nvn-47 |
| `nvnTextureViewGetLayers` | 55, 231 | nvn-47 |
| `nvnTextureViewGetLevels` | 55, 231 | nvn-47 |
| `nvnTextureViewGetSwizzle` |  |  |
| `nvnTextureViewGetTarget` | 55, 231 | nvn-47 |
| `nvnTextureViewSetDefaults` | 55, 231 | nvn-47, nvn-49 |
| `nvnTextureViewSetDepthStencilMode` |  |  |
| `nvnTextureViewSetFormat` | 55, 231 | nvn-47 |
| `nvnTextureViewSetLayers` | 55, 231 | nvn-47 |
| `nvnTextureViewSetLevels` | 55, 231 | nvn-47, nvn-49 |
| `nvnTextureViewSetSwizzle` |  |  |
| `nvnTextureViewSetTarget` | 55, 231 | nvn-47 |
| `nvnTextureWriteTexels` |  |  |
| `nvnTextureWriteTexelsStrided` |  |  |

## Samplers

45 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnSamplerBuilderGetBorderColor` | 55, 231 | nvn-59, nvn-60 |
| `nvnSamplerBuilderGetBorderColori` | 55, 231 | nvn-59 |
| `nvnSamplerBuilderGetBorderColorui` | 55, 231 | nvn-59 |
| `nvnSamplerBuilderGetCompare` | 55, 231 | nvn-59, nvn-60 |
| `nvnSamplerBuilderGetDevice` | 55, 231 | nvn-59 |
| `nvnSamplerBuilderGetLodBias` | 55, 231 | nvn-59, nvn-60 |
| `nvnSamplerBuilderGetLodClamp` | 55, 231 | nvn-59, nvn-60 |
| `nvnSamplerBuilderGetMaxAnisotropy` | 55, 231 | nvn-59, nvn-60 |
| `nvnSamplerBuilderGetMinMagFilter` | 55, 231 | nvn-59, nvn-60 |
| `nvnSamplerBuilderGetReductionFilter` | 55, 231 | nvn-59, nvn-60 |
| `nvnSamplerBuilderGetWrapMode` | 55, 231 | nvn-59, nvn-60 |
| `nvnSamplerBuilderSetBorderColori` | 55, 231 | nvn-59 |
| `nvnSamplerBuilderSetBorderColorui` | 55, 231 | nvn-59 |
| `nvnSamplerGetBorderColori` | 55, 231 | nvn-60 |
| `nvnSamplerGetBorderColorui` | 55, 231 | nvn-60 |
| `nvnSamplerPoolRegisterSamplerBuilder` | 55, 231 | nvn-61 |
| `nvnSamplerBuilderSetBorderColor` | 55, 231 | nvn-59, nvn-60, nvn-61, nvn-64, nvn-69, nvn-70, nvn-71 |
| `nvnSamplerBuilderSetCompare` | 55, 231 | nvn-59, nvn-60, nvn-61, nvn-64 |
| `nvnSamplerBuilderSetDefaults` | 55, 231 | nvn-59, nvn-60, nvn-61, nvn-64, nvn-69, nvn-70, nvn-71 |
| `nvnSamplerBuilderSetDevice` | 55, 231 | nvn-59, nvn-60, nvn-61, nvn-64, nvn-69, nvn-70, nvn-71 |
| `nvnSamplerBuilderSetLodBias` | 55, 231 | nvn-59, nvn-60, nvn-61, nvn-64 |
| `nvnSamplerBuilderSetLodClamp` | 55, 231 | nvn-59, nvn-60, nvn-61, nvn-64 |
| `nvnSamplerBuilderGetLodSnap` | 55, 231 | nvn-59 |
| `nvnSamplerBuilderSetLodSnap` | 55, 231 | nvn-59, nvn-60, nvn-61, nvn-64 |
| `nvnSamplerBuilderSetMaxAnisotropy` | 55, 231 | nvn-59, nvn-60, nvn-61, nvn-64 |
| `nvnSamplerBuilderSetMinMagFilter` | 55, 231 | nvn-59, nvn-60, nvn-61, nvn-64, nvn-69, nvn-70, nvn-71 |
| `nvnSamplerBuilderSetReductionFilter` | 55, 231 | nvn-59, nvn-60, nvn-61, nvn-64 |
| `nvnSamplerBuilderSetWrapMode` | 55, 231 | nvn-59, nvn-60, nvn-61, nvn-64, nvn-69, nvn-70, nvn-71 |
| `nvnSamplerCompare` | 55, 231 | nvn-60 |
| `nvnSamplerFinalize` | 55, 231 | nvn-60, nvn-61, nvn-64, nvn-69, nvn-70, nvn-71 |
| `nvnSamplerGetBorderColor` | 55, 231 | nvn-60 |
| `nvnSamplerGetCompare` | 55, 231 | nvn-60 |
| `nvnSamplerGetLodBias` | 55, 231 | nvn-60 |
| `nvnSamplerGetLodClamp` | 55, 231 | nvn-60 |
| `nvnSamplerGetMaxAnisotropy` | 55, 231 | nvn-60 |
| `nvnSamplerGetMinMagFilter` | 55, 231 | nvn-60 |
| `nvnSamplerGetReductionFilter` | 55, 231 | nvn-60 |
| `nvnSamplerGetWrapMode` | 55, 231 | nvn-60 |
| `nvnSamplerInitialize` | 55, 231 | nvn-60, nvn-61, nvn-64, nvn-69, nvn-70, nvn-71 |
| `nvnSamplerPoolFinalize` | 55, 231 | nvn-61, nvn-64, nvn-69, nvn-70, nvn-71 |
| `nvnSamplerPoolGetMemoryOffset` | 55, 231 | nvn-61 |
| `nvnSamplerPoolGetMemoryPool` | 55, 231 | nvn-61 |
| `nvnSamplerPoolGetSize` | 55, 231 | nvn-61, nvn-64 |
| `nvnSamplerPoolInitialize` | 55, 231 | nvn-61, nvn-64, nvn-69, nvn-70, nvn-71 |
| `nvnSamplerPoolRegisterSampler` | 55, 231 | nvn-61, nvn-69, nvn-70, nvn-71 |

## Programs

20 functions, 7 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnProgramGetSubtileSize` |  |  |
| `nvnProgramGetScratchBytesPerWarp` |  |  |
| `nvnProgramSetSampleShading` |  |  |
| `nvnProgramSetSubroutineLinkage` |  |  |
| `nvnProgramFinalize` | 55, 231 | nvn-32, nvn-64, nvn-69, nvn-70, nvn-71, nvn-97 |
| `nvnProgramInitialize` | 55, 231 | nvn-64, nvn-69, nvn-70, nvn-71, nvn-97 |
| `nvnProgramSetShaders` | 55, 231 | nvn-69, nvn-70, nvn-71 |
| `nvnProgramSetShadersExt` | 55, 231 | nvn-97 |
| `nvnProgramBuilderSetDefaults` | 231 | nvn-99, nvn2-2, nvn2-5, nvn2-8 |
| `nvnProgramBuilderSetDevice` | 231 | nvn-99, nvn2-2, nvn2-5, nvn2-8 |
| `nvnProgramBuilderGetDevice` | 231 | nvn-99 |
| `nvnProgramBuilderSetSampleShading` | 231 | nvn-99 |
| `nvnProgramBuilderGetSampleShading` | 231 | nvn-99 |
| `nvnProgramBuilderSetShaders` | 231 | nvn-99, nvn2-2, nvn2-5, nvn2-8 |
| `nvnProgramBuilderGetShaders` | 231 | nvn-99 |
| `nvnProgramBuilderGetShaderCount` | 231 | nvn-99 |
| `nvnProgramBuilderSetSubroutineLinkage` |  |  |
| `nvnProgramBuilderGetSubroutineLinkage` |  |  |
| `nvnProgramBuilderGetSubroutineLinkageCount` |  |  |
| `nvnProgramInitializeFromBuilder` | 231 | nvn2-2, nvn2-5, nvn2-8 |

## State objects

82 functions, 66 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnBlendStateGetAdvancedMode` |  |  |
| `nvnBlendStateGetAdvancedNormalizedDst` |  |  |
| `nvnBlendStateGetAdvancedOverlap` |  |  |
| `nvnBlendStateGetAdvancedPremultipliedSrc` |  |  |
| `nvnBlendStateSetAdvancedMode` |  |  |
| `nvnBlendStateSetAdvancedNormalizedDst` |  |  |
| `nvnBlendStateSetAdvancedOverlap` |  |  |
| `nvnBlendStateSetAdvancedPremultipliedSrc` |  |  |
| `nvnMultisampleStateGetSampleLocationsGrid` |  |  |
| `nvnMultisampleStateSetSampleLocations` |  |  |
| `nvnBlendStateSetBlendEquation` |  |  |
| `nvnBlendStateSetBlendFunc` |  |  |
| `nvnBlendStateSetBlendTarget` |  |  |
| `nvnBlendStateSetDefaults` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn2-7 |
| `nvnChannelMaskStateSetChannelMask` |  |  |
| `nvnChannelMaskStateSetDefaults` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn2-7 |
| `nvnColorStateGetBlendEnable` |  |  |
| `nvnColorStateSetAlphaTest` |  |  |
| `nvnColorStateSetBlendEnable` |  |  |
| `nvnColorStateSetDefaults` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn2-7 |
| `nvnColorStateSetLogicOp` |  |  |
| `nvnDepthStencilStateSetDefaults` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn2-7 |
| `nvnDepthStencilStateSetDepthFunc` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn2-7 |
| `nvnDepthStencilStateSetDepthTestEnable` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn2-7 |
| `nvnDepthStencilStateSetDepthWriteEnable` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn2-7 |
| `nvnDepthStencilStateSetStencilFunc` |  |  |
| `nvnDepthStencilStateSetStencilOp` |  |  |
| `nvnDepthStencilStateSetStencilTestEnable` |  |  |
| `nvnMultisampleStateSetAlphaToCoverageEnable` |  |  |
| `nvnMultisampleStateSetDefaults` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn2-7 |
| `nvnMultisampleStateSetMultisampleEnable` |  |  |
| `nvnMultisampleStateSetSamples` |  |  |
| `nvnPolygonStateSetCullFace` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn2-7 |
| `nvnPolygonStateSetDefaults` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn2-7 |
| `nvnPolygonStateSetFrontFace` | 55, 231 | nvn-69, nvn-70, nvn-71 |
| `nvnPolygonStateSetPolygonMode` |  |  |
| `nvnPolygonStateSetPolygonOffsetEnables` |  |  |
| `nvnVertexAttribStateSetDefaults` | 55, 231 | nvn-69, nvn-70, nvn-71 |
| `nvnVertexAttribStateSetFormat` | 55, 231 | nvn-69, nvn-70, nvn-71 |
| `nvnVertexAttribStateSetStreamIndex` | 55, 231 | nvn-69, nvn-70, nvn-71 |
| `nvnVertexStreamStateSetDefaults` | 55, 231 | nvn-69, nvn-70, nvn-71 |
| `nvnVertexStreamStateSetDivisor` |  |  |
| `nvnVertexStreamStateSetStride` | 55, 231 | nvn-69, nvn-70, nvn-71 |
| `nvnBlendStateGetBlendEquation` |  |  |
| `nvnBlendStateGetBlendFunc` |  |  |
| `nvnBlendStateGetBlendTarget` |  |  |
| `nvnChannelMaskStateGetChannelMask` |  |  |
| `nvnColorStateGetAlphaTest` |  |  |
| `nvnColorStateGetLogicOp` |  |  |
| `nvnDepthStencilStateGetDepthFunc` |  |  |
| `nvnDepthStencilStateGetDepthTestEnable` |  |  |
| `nvnDepthStencilStateGetDepthWriteEnable` |  |  |
| `nvnDepthStencilStateGetStencilFunc` |  |  |
| `nvnDepthStencilStateGetStencilOp` |  |  |
| `nvnDepthStencilStateGetStencilTestEnable` |  |  |
| `nvnMultisampleStateGetAlphaToCoverageDither` |  |  |
| `nvnMultisampleStateGetAlphaToCoverageEnable` |  |  |
| `nvnMultisampleStateGetCoverageModulationMode` |  |  |
| `nvnMultisampleStateGetCoverageToColorEnable` |  |  |
| `nvnMultisampleStateGetCoverageToColorOutput` |  |  |
| `nvnMultisampleStateGetMultisampleEnable` |  |  |
| `nvnMultisampleStateGetRasterSamples` |  |  |
| `nvnMultisampleStateGetSampleLocationsEnable` |  |  |
| `nvnMultisampleStateGetSampleLocationsGridEnable` |  |  |
| `nvnMultisampleStateGetSamples` |  |  |
| `nvnMultisampleStateSetAlphaToCoverageDither` |  |  |
| `nvnMultisampleStateSetCoverageModulationMode` |  |  |
| `nvnMultisampleStateSetCoverageToColorEnable` |  |  |
| `nvnMultisampleStateSetCoverageToColorOutput` |  |  |
| `nvnMultisampleStateSetRasterSamples` |  |  |
| `nvnMultisampleStateSetCoverageTruncationEnable` |  |  |
| `nvnMultisampleStateGetCoverageTruncationEnable` |  |  |
| `nvnMultisampleStateSetSampleLocationsEnable` |  |  |
| `nvnMultisampleStateSetSampleLocationsGridEnable` |  |  |
| `nvnPolygonStateGetCullFace` |  |  |
| `nvnPolygonStateGetFrontFace` |  |  |
| `nvnPolygonStateGetPolygonMode` |  |  |
| `nvnPolygonStateGetPolygonOffsetEnables` |  |  |
| `nvnVertexAttribStateGetFormat` |  |  |
| `nvnVertexAttribStateGetStreamIndex` |  |  |
| `nvnVertexStreamStateGetDivisor` |  |  |
| `nvnVertexStreamStateGetStride` |  |  |

## Queues

30 functions, 11 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnQueueBuilderGetCommandFlushThreshold` | 55, 231 | nvn-57 |
| `nvnQueueBuilderGetCommandMemorySize` | 55, 231 | nvn-57 |
| `nvnQueueBuilderGetComputeMemorySize` | 55, 231 | nvn-57 |
| `nvnQueueBuilderGetControlMemorySize` | 55, 231 | nvn-57 |
| `nvnQueueBuilderGetDevice` | 55, 231 | nvn-57 |
| `nvnQueueBuilderGetFlags` | 55, 231 | nvn-57 |
| `nvnQueueBuilderGetMemory` |  |  |
| `nvnQueueBuilderGetMemorySize` |  |  |
| `nvnQueueGetTotalCommandMemoryUsed` |  |  |
| `nvnQueueGetTotalComputeMemoryUsed` |  |  |
| `nvnQueueGetTotalControlMemoryUsed` |  |  |
| `nvnQueueResetMemoryUsageCounts` |  |  |
| `nvnQueueBuilderGetQueueMemorySize` |  |  |
| `nvnQueueBuilderSetCommandFlushThreshold` | 55, 231 | nvn-57 |
| `nvnQueueBuilderSetCommandMemorySize` | 55, 231 | nvn-57 |
| `nvnQueueBuilderSetComputeMemorySize` | 55, 231 | nvn-57 |
| `nvnQueueBuilderSetControlMemorySize` | 55, 231 | nvn-57 |
| `nvnQueueBuilderSetDefaults` | 55, 231 | nvn-36, nvn-57, nvn-69, nvn-70, nvn-71, nvn2-1 |
| `nvnQueueBuilderSetDevice` | 55, 231 | nvn-36, nvn-57, nvn-69, nvn-70, nvn-71, nvn2-1 |
| `nvnQueueBuilderSetFlags` | 55, 231 | nvn-57 |
| `nvnQueueBuilderGetQueuePriority` |  |  |
| `nvnQueueBuilderSetQueuePriority` |  |  |
| `nvnQueueBuilderSetQueueMemory` |  |  |
| `nvnQueueFinalize` | 55, 231 | nvn-69, nvn-70, nvn-71 |
| `nvnQueueFinish` | 55, 231 | nvn-37, nvn-38, nvn-40, nvn-41, nvn-42, nvn-43, nvn-44, nvn-45, nvn-49, nvn-51, nvn-52, nvn-53, nvn-54, nvn-56, nvn-63, nvn-66, nvn-67, nvn-68, nvn-69, nvn-70, nvn-71, nvn-98, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnQueueFlush` | 55, 231 | nvn-52, nvn-53, nvn-54 |
| `nvnQueueGetError` | 55, 231 | nvn-56, nvn-64 |
| `nvnQueueInitialize` | 55, 231 | nvn-36, nvn-69, nvn-70, nvn-71, nvn2-1 |
| `nvnQueueSubmitCommands` | 55, 231 | nvn-37, nvn-38, nvn-40, nvn-41, nvn-42, nvn-43, nvn-44, nvn-45, nvn-49, nvn-51, nvn-52, nvn-53, nvn-54, nvn-63, nvn-66, nvn-67, nvn-68, nvn-69, nvn-70, nvn-71, nvn-98, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnQueueBuilderSetPrimaryQueue` |  |  |

## Sync and events

24 functions, 6 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnSyncCreateGLSync` |  |  |
| `nvnSyncInitializeFromFencedGLSync` |  |  |
| `nvnSyncFinalize` | 55, 231 | nvn-52, nvn-53, nvn-54, nvn-55, nvn-64, nvn-68 |
| `nvnSyncInitialize` | 55, 231 | nvn-52, nvn-53, nvn-54, nvn-55, nvn-64, nvn-68 |
| `nvnSyncWait` | 55, 231 | nvn-52, nvn-53, nvn-54, nvn-55, nvn-68 |
| `nvnSyncGetNvRmSyncNVX` |  |  |
| `nvnSyncInitializeFromNvRmSyncNVX` |  |  |
| `nvnEventBuilderGetMemoryOffset` | 55, 231 | nvn-58 |
| `nvnEventBuilderGetMemoryPool` | 55, 231 | nvn-58 |
| `nvnEventBuilderGetStorage` | 55, 231 | nvn-58 |
| `nvnEventGetMemoryOffset` | 55, 231 | nvn-58 |
| `nvnEventGetMemoryPool` | 55, 231 | nvn-58 |
| `nvnEventBuilderSetDefaults` | 55, 231 | nvn-58 |
| `nvnEventBuilderSetStorage` | 55, 231 | nvn-58 |
| `nvnEventFinalize` | 55, 231 | nvn-58 |
| `nvnEventGetValue` | 55, 231 | nvn-58 |
| `nvnEventInitialize` | 55, 231 | nvn-58 |
| `nvnEventSignal` | 55, 231 | nvn-58 |
| `nvnQueueFenceSync` | 55, 231 | nvn-52, nvn-54 |
| `nvnQueueWaitSync` | 55, 231 | nvn-52, nvn-53, nvn-54 |
| `nvnCommandBufferFenceSync` | 55, 231 | nvn-53 |
| `nvnCommandBufferSignalEvent` |  |  |
| `nvnCommandBufferWaitEvent` |  |  |
| `nvnCommandBufferWaitSync` | 55, 231 | nvn-54 |

## Windows and present

35 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnQueueAcquireTexture` | 55, 231 | nvn-66, nvn-67 |
| `nvnQueuePresentTexture` | 55, 231 | nvn-66, nvn-67, nvn-68 |
| `nvnWindowBuilderGetDevice` | 55, 231 | nvn-65 |
| `nvnWindowBuilderGetNativeWindow` | 55, 231 | nvn-65 |
| `nvnWindowBuilderGetNumActiveTextures` | 55, 231 | nvn-65 |
| `nvnWindowGetNativeWindow` | 55, 231 | nvn-67 |
| `nvnWindowGetNumActiveTextures` | 55, 231 | nvn-67 |
| `nvnWindowGetNumTextures` | 55, 231 | nvn-67 |
| `nvnWindowSetNumActiveTextures` | 55, 231 | nvn-67 |
| `nvnWindowBuilderGetNumTextures` | 55, 231 | nvn-65 |
| `nvnWindowBuilderGetPresentInterval` | 55, 231 | nvn-65 |
| `nvnWindowBuilderGetTexture` | 55, 231 | nvn-65 |
| `nvnWindowGetCrop` | 55, 231 | nvn-67 |
| `nvnWindowGetPresentInterval` | 55, 231 | nvn-67 |
| `nvnWindowAcquireTexture` | 55, 231 | nvn-68 |
| `nvnWindowBuilderSetDefaults` | 55, 231 | nvn-65, nvn-66, nvn-100 |
| `nvnWindowBuilderSetDevice` | 55, 231 | nvn-65, nvn-66 |
| `nvnWindowBuilderSetNativeWindow` | 55, 231 | nvn-65, nvn-66 |
| `nvnWindowBuilderSetNumActiveTextures` | 55, 231 | nvn-65 |
| `nvnWindowBuilderSetPresentInterval` | 55, 231 | nvn-65 |
| `nvnWindowBuilderSetTextures` | 55, 231 | nvn-65, nvn-66 |
| `nvnWindowBuilderSetColorSpace` | 231 | nvn-100 |
| `nvnWindowBuilderGetColorSpace` | 231 | nvn-100 |
| `nvnWindowBuilderSetHdrDisplayMetadata` | 231 | nvn-100 |
| `nvnWindowBuilderGetHdrDisplayMetadata` | 231 | nvn-100 |
| `nvnWindowBuilderSetHdrContentMetadata` | 231 | nvn-100 |
| `nvnWindowBuilderGetHdrContentMetadata` | 231 | nvn-100 |
| `nvnWindowSetHdrDisplayMetadata` | 231 | nvn-101 |
| `nvnWindowGetHdrDisplayMetadata` | 231 | nvn-101 |
| `nvnWindowSetHdrContentMetadata` | 231 | nvn-101 |
| `nvnWindowGetHdrContentMetadata` | 231 | nvn-101 |
| `nvnWindowFinalize` | 55, 231 | nvn-66 |
| `nvnWindowInitialize` | 55, 231 | nvn-66 |
| `nvnWindowSetCrop` | 55, 231 | nvn-67 |
| `nvnWindowSetPresentInterval` | 55, 231 | nvn-67 |

## Counters and queries

6 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnDeviceGetTimestampInNanoseconds` | 55, 231 | nvn-9, nvn-10, nvn-74, nvn-75 |
| `nvnCommandBufferReportCounter` | 55, 231 | nvn-72, nvn-74, nvn-75, nvn-76 |
| `nvnCommandBufferReportValue` | 55, 231 | nvn-73 |
| `nvnCommandBufferResetCounter` | 55, 231 | nvn-72, nvn-76 |
| `nvnCommandBufferSetRenderEnable` | 55, 231 | nvn-76 |
| `nvnCommandBufferSetRenderEnableConditional` | 55, 231 | nvn-76 |

## Commands: memory and recording

25 functions, 3 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferGetCommandMemoryFree` | 55, 231 | nvn-77 |
| `nvnCommandBufferGetCommandMemorySize` | 55, 231 | nvn-77 |
| `nvnCommandBufferGetControlMemoryFree` | 55, 231 | nvn-77 |
| `nvnCommandBufferGetControlMemorySize` | 55, 231 | nvn-77 |
| `nvnCommandBufferSetControlMemoryCallbackEnabled` | 55, 231 | nvn-78 |
| `nvnCommandBufferAddCommandMemory` | 55, 231 | nvn-36, nvn-69, nvn-70, nvn-71, nvn-77, nvn-78, nvn2-1 |
| `nvnCommandBufferAddControlMemory` | 55, 231 | nvn-36, nvn-69, nvn-70, nvn-71, nvn-77, nvn2-1 |
| `nvnCommandBufferBarrier` | 55, 231 | nvn-37, nvn-38, nvn-40, nvn-41, nvn-43, nvn-44, nvn-45, nvn-49, nvn-52, nvn-53, nvn-54, nvn-69, nvn-70, nvn-71, nvn-80, nvn-81, nvn-98, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferBeginRecording` | 55, 231 | nvn-37, nvn-38, nvn-40, nvn-41, nvn-42, nvn-43, nvn-44, nvn-45, nvn-49, nvn-51, nvn-52, nvn-53, nvn-54, nvn-63, nvn-66, nvn-67, nvn-68, nvn-69, nvn-70, nvn-71, nvn-98, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferSetRecordingMode` |  |  |
| `nvnCommandBufferSetComputeOverlapMode` |  |  |
| `nvnCommandBufferCallCommands` | 55, 231 | nvn-79 |
| `nvnCommandBufferCopyCommands` | 55, 231 | nvn-79 |
| `nvnCommandBufferEndRecording` | 55, 231 | nvn-37, nvn-38, nvn-40, nvn-41, nvn-42, nvn-43, nvn-44, nvn-45, nvn-49, nvn-51, nvn-52, nvn-53, nvn-54, nvn-63, nvn-66, nvn-67, nvn-68, nvn-69, nvn-70, nvn-71, nvn-79, nvn-98, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferFinalize` | 55, 231 | nvn-69, nvn-70, nvn-71 |
| `nvnCommandBufferGetCommandMemoryUsed` | 55, 231 | nvn-77 |
| `nvnCommandBufferGetControlMemoryUsed` | 55, 231 | nvn-77 |
| `nvnCommandBufferGetMemoryCallback` | 55, 231 | nvn-78 |
| `nvnCommandBufferGetMemoryCallbackData` | 55, 231 | nvn-78 |
| `nvnCommandBufferInitialize` | 55, 231 | nvn-36, nvn-69, nvn-70, nvn-71, nvn2-1 |
| `nvnCommandBufferIsRecording` | 55, 231 | nvn-77 |
| `nvnCommandBufferSetCommandMemoryCallbackEnabled` | 55, 231 | nvn-78 |
| `nvnCommandBufferSetComputeWorkloadPriority` |  |  |
| `nvnCommandBufferSetMemoryCallback` | 55, 231 | nvn-78 |
| `nvnCommandBufferSetMemoryCallbackData` | 55, 231 | nvn-78 |

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
| `nvnCommandBufferBindBlendState` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn2-7 |
| `nvnCommandBufferBindChannelMaskState` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn2-7 |
| `nvnCommandBufferBindColorState` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn2-7 |
| `nvnCommandBufferBindDepthStencilState` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn2-7 |
| `nvnCommandBufferBindImage` | 231 |  |
| `nvnCommandBufferBindImages` |  |  |
| `nvnCommandBufferBindMultisampleState` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn2-7 |
| `nvnCommandBufferBindPolygonState` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn2-7 |
| `nvnCommandBufferBindProgram` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn-97, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferBindSeparateSampler` |  |  |
| `nvnCommandBufferBindSeparateSamplers` |  |  |
| `nvnCommandBufferBindSeparateTexture` |  |  |
| `nvnCommandBufferBindSeparateTextures` |  |  |
| `nvnCommandBufferBindStorageBuffer` | 231 | nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferBindStorageBuffers` |  |  |
| `nvnCommandBufferBindTexture` | 55, 231 | nvn-69, nvn-70, nvn-71 |
| `nvnCommandBufferBindTextures` |  |  |
| `nvnCommandBufferBindTexturesDeferred` |  |  |
| `nvnCommandBufferBindTransformFeedbackBuffer` |  |  |
| `nvnCommandBufferBindTransformFeedbackBuffers` |  |  |
| `nvnCommandBufferBindUniformBuffer` | 55, 231 | nvn-69, nvn-70, nvn-71 |
| `nvnCommandBufferBindUniformBuffers` |  |  |
| `nvnCommandBufferBindVertexAttribState` | 55, 231 | nvn-69, nvn-70, nvn-71 |
| `nvnCommandBufferBindVertexBuffer` | 55, 231 | nvn-69, nvn-70, nvn-71 |
| `nvnCommandBufferBindVertexBuffers` |  |  |
| `nvnCommandBufferBindVertexStreamState` | 55, 231 | nvn-69, nvn-70, nvn-71 |
| `nvnCommandBufferSetSamplerPool` | 55, 231 | nvn-69, nvn-70, nvn-71 |
| `nvnCommandBufferSetShaderScratchMemory` |  |  |
| `nvnCommandBufferSetTexturePool` | 55, 231 | nvn-69, nvn-70, nvn-71 |
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
| `nvnCommandBufferDrawArrays` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn-88, nvn-94 |
| `nvnCommandBufferDrawArraysIndirect` |  |  |
| `nvnCommandBufferDrawArraysInstanced` |  |  |
| `nvnCommandBufferDrawElements` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn-89 |
| `nvnCommandBufferDrawElementsBaseVertex` | 55, 231 | nvn-69, nvn-70, nvn-71 |
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
| `nvnCommandBufferClearTexturei` | 55, 231 | nvn-81 |
| `nvnCommandBufferGetCopyImageStride` | 55, 231 | nvn-83 |
| `nvnCommandBufferGetCopyRowStride` | 55, 231 | nvn-83 |
| `nvnCommandBufferResolveDepthBuffer` |  |  |
| `nvnCommandBufferTiledDownsample` |  |  |
| `nvnCommandBufferTiledDownsampleTextureView` |  |  |
| `nvnCommandBufferClearBuffer` | 55, 231 | nvn-82 |
| `nvnCommandBufferClearColor` | 55, 231 | nvn-37, nvn-38, nvn-52, nvn-53, nvn-54, nvn-66, nvn-67, nvn-68, nvn-69, nvn-70, nvn-71, nvn-98, nvn2-7 |
| `nvnCommandBufferClearColori` | 55, 231 | nvn-80 |
| `nvnCommandBufferClearColorui` | 55, 231 | nvn-80 |
| `nvnCommandBufferClearDepthStencil` | 55, 231 | nvn-69, nvn-70, nvn-71, nvn2-7 |
| `nvnCommandBufferClearTexture` | 55, 231 | nvn-81 |
| `nvnCommandBufferClearTextureui` | 55, 231 | nvn-81 |
| `nvnCommandBufferCopyBufferToBuffer` | 55, 231 | nvn-42, nvn-51, nvn-63, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferCopyBufferToTexture` | 55, 231 | nvn-40, nvn-41, nvn-43, nvn-44, nvn-45, nvn-49, nvn-69, nvn-70, nvn-71 |
| `nvnCommandBufferCopyTextureToBuffer` | 55, 231 | nvn-37, nvn-38, nvn-40, nvn-41, nvn-43, nvn-44, nvn-45, nvn-49, nvn-52, nvn-53, nvn-54, nvn-69, nvn-70, nvn-71, nvn-80, nvn-81, nvn-98, nvn2-7 |
| `nvnCommandBufferCopyTextureToTexture` | 55, 231 | nvn-41, nvn-44 |
| `nvnCommandBufferDiscardColor` | 55, 231 | nvn-84 |
| `nvnCommandBufferDiscardDepthStencil` | 55, 231 | nvn-84 |
| `nvnCommandBufferDownsample` |  |  |
| `nvnCommandBufferDownsampleTextureView` |  |  |
| `nvnCommandBufferRestoreZCullData` |  |  |
| `nvnCommandBufferSaveZCullData` |  |  |
| `nvnCommandBufferSetCopyImageStride` | 55, 231 | nvn-83 |
| `nvnCommandBufferSetCopyRowStride` | 55, 231 | nvn-40, nvn-41, nvn-42, nvn-43, nvn-44, nvn-45, nvn-49, nvn-83 |

## Commands: render state

36 functions, 16 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferOverrideSubtileSize` |  |  |
| `nvnCommandBufferSetColorReductionEnable` |  |  |
| `nvnCommandBufferSetColorReductionThresholds` |  |  |
| `nvnCommandBufferSetConservativeRasterDilate` | 55, 231 | nvn-93 |
| `nvnCommandBufferSetInnerTessellationLevels` |  |  |
| `nvnCommandBufferSetOuterTessellationLevels` |  |  |
| `nvnCommandBufferSetProvokingVertexMode` | 55, 231 | nvn-87 |
| `nvnCommandBufferSetSubpixelPrecisionBias` | 55, 231 | nvn-92 |
| `nvnCommandBufferSetZCullZF32CompressionEnable` |  |  |
| `nvnCommandBufferSetShadingRatePaletteMode` |  |  |
| `nvnCommandBufferSetAlphaRef` |  |  |
| `nvnCommandBufferSetBlendColor` |  |  |
| `nvnCommandBufferSetConservativeRasterEnable` | 55, 231 | nvn-92, nvn-93 |
| `nvnCommandBufferSetDepthBounds` | 55, 231 | nvn-86 |
| `nvnCommandBufferSetDepthClamp` | 55, 231 | nvn-87 |
| `nvnCommandBufferSetDepthRange` | 55, 231 | nvn-86 |
| `nvnCommandBufferSetDepthRanges` | 55, 231 | nvn-86 |
| `nvnCommandBufferSetLineWidth` | 55, 231 | nvn-94 |
| `nvnCommandBufferSetPatchSize` |  |  |
| `nvnCommandBufferSetPointSize` | 55, 231 | nvn-88 |
| `nvnCommandBufferSetPolygonOffsetClamp` |  |  |
| `nvnCommandBufferSetPrimitiveRestart` | 55, 231 | nvn-89 |
| `nvnCommandBufferSetRasterizerDiscard` | 55, 231 | nvn-87 |
| `nvnCommandBufferSetRenderTargets` | 55, 231 | nvn-37, nvn-38, nvn-52, nvn-53, nvn-54, nvn-66, nvn-67, nvn-68, nvn-69, nvn-70, nvn-71, nvn-80, nvn-98, nvn2-7 |
| `nvnCommandBufferSetSampleMask` |  |  |
| `nvnCommandBufferSetScissor` | 55, 231 | nvn-37, nvn-38, nvn-52, nvn-53, nvn-54, nvn-66, nvn-67, nvn-68, nvn-69, nvn-70, nvn-71, nvn-80, nvn-98, nvn2-7 |
| `nvnCommandBufferSetScissors` | 55, 231 | nvn-85 |
| `nvnCommandBufferSetStencilCullCriteria` |  |  |
| `nvnCommandBufferSetStencilMask` |  |  |
| `nvnCommandBufferSetStencilRef` |  |  |
| `nvnCommandBufferSetStencilValueMask` |  |  |
| `nvnCommandBufferSetTiledCacheAction` | 55, 231 | nvn-91 |
| `nvnCommandBufferSetTiledCacheTileSize` | 55, 231 | nvn-91 |
| `nvnCommandBufferSetViewport` | 55, 231 | nvn-37, nvn-38, nvn-52, nvn-53, nvn-54, nvn-66, nvn-67, nvn-68, nvn-69, nvn-70, nvn-71, nvn-98, nvn2-7 |
| `nvnCommandBufferSetViewportSwizzles` | 55, 231 | nvn-90 |
| `nvnCommandBufferSetViewports` | 55, 231 | nvn-85 |

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
| `nvnProgramBuilderSetMaxCallableRecursionDepth` | 231 | nvn-99 |
| `nvnProgramBuilderGetMaxCallableRecursionDepth` | 231 | nvn-99 |
| `nvnProgramBuilderSetMaxTraceRecursionDepth` | 231 | nvn-99 |
| `nvnProgramBuilderGetMaxTraceRecursionDepth` | 231 | nvn-99 |
| `nvnProgramBuilderSetProgramRayTracingFlags` | 231 | nvn-99 |
| `nvnProgramBuilderGetProgramRayTracingFlags` | 231 | nvn-99 |
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
| `nvnSetExternalDebuggerEnabledNVX` | 55, 231 | nvn-64 |
| `nvnDeviceGenerateDebugDomainId` | 55, 231 | nvn-64, nvn-95, nvn-98 |
| `nvnDeviceInstallDebugCallback` | 55, 231 | nvn-64 |
| `nvnDeviceIsExternalDebuggerAttached` | 55, 231 | nvn-64 |
| `nvnDeviceSetAutomaticCheckpointsByDebugGroup` | 55, 231 | nvn-64 |
| `nvnDeviceSetAutomaticCheckpointsEnable` | 55, 231 | nvn-64 |
| `nvnDeviceSetAutomaticCheckpointsMask` | 55, 231 | nvn-64 |
| `nvnDeviceSetAutomaticCheckpointsSamplingInterval` | 55, 231 | nvn-64 |
| `nvnDeviceSetCheckpointCallStacksEnable` | 55, 231 | nvn-64 |
| `nvnDeviceSetCheckpointMaxCallStacksDepth` | 55, 231 | nvn-64 |
| `nvnDeviceWalkDebugDatabase` | 55, 231 | nvn-64 |
| `nvnMemoryPoolSetDebugLabel` | 55, 231 | nvn-64 |
| `nvnBufferGetDebugID` | 55, 231 | nvn-64 |
| `nvnBufferSetDebugLabel` | 55, 231 | nvn-64 |
| `nvnTextureGetDebugID` | 55, 231 | nvn-64 |
| `nvnTextureSetDebugLabel` | 55, 231 | nvn-64 |
| `nvnSamplerGetDebugID` | 55, 231 | nvn-64 |
| `nvnSamplerSetDebugLabel` | 55, 231 | nvn-64 |
| `nvnProgramBuilderSetDebugDataHashes` |  |  |
| `nvnProgramBuilderGetDebugDataHashes` |  |  |
| `nvnCommandBufferInsertCheckpointDynamic` | 55, 231 | nvn-95 |
| `nvnCommandBufferInsertCheckpointStatic` | 55, 231 | nvn-95 |
| `nvnCommandBufferInsertDebugMarkerGraphics` | 231 | nvn-98 |
| `nvnCommandBufferInsertDebugMarkerCompute` | 231 | nvn-98 |
| `nvnCommandBufferPushDebugGroupGraphics` | 231 | nvn-98 |
| `nvnCommandBufferPushDebugGroupCompute` | 231 | nvn-98 |
| `nvnCommandBufferPopDebugGroupGraphics` | 231 | nvn-98 |
| `nvnCommandBufferPopDebugGroupCompute` | 231 | nvn-98 |
| `nvnCommandBufferInsertDebugMarker` | 55, 231 | nvn-95 |
| `nvnCommandBufferInsertDebugMarkerDynamic` | 55, 231 | nvn-95 |
| `nvnCommandBufferInsertDebugMarkerStatic` | 55, 231 | nvn-95 |
| `nvnCommandBufferPopDebugGroup` | 55, 231 | nvn-95 |
| `nvnCommandBufferPopDebugGroupId` | 55, 231 | nvn-95 |
| `nvnCommandBufferPushDebugGroup` | 55, 231 | nvn-95 |
| `nvnCommandBufferPushDebugGroupDynamic` | 55, 231 | nvn-95 |
| `nvnCommandBufferPushDebugGroupStatic` | 55, 231 | nvn-95 |
| `nvnCommandBufferSetDebugLabel` | 55, 231 | nvn-64 |
| `nvnDeviceSetDebugLabel` | 55, 231 | nvn-64 |
| `nvnProgramSetDebugLabel` | 55, 231 | nvn-64 |
| `nvnQueueSetDebugLabel` | 55, 231 | nvn-64 |
| `nvnSamplerPoolSetDebugLabel` | 55, 231 | nvn-64 |
| `nvnSyncSetDebugLabel` | 55, 231 | nvn-64 |
| `nvnTexturePoolSetDebugLabel` | 55, 231 | nvn-64 |
| `nvnWindowSetDebugLabel` | 55, 231 | nvn-67 |
