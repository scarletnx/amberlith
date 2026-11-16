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
names 677; the layer has 430 of them in its API 55 build and 470 in its API 231 build; a case calls 465. The
other 212 have no call at all: a runtime could answer any of them wrongly and every case would still pass.

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
| [Commands: binds](#commands-binds) | 41 | 22 | 26 | 23 | 18 |
| [Commands: draws and dispatches](#commands-draws-and-dispatches) | 26 | 3 | 6 | 6 | 20 |
| [Commands: clears and copies](#commands-clears-and-copies) | 25 | 18 | 18 | 18 | 7 |
| [Commands: render state](#commands-render-state) | 36 | 23 | 23 | 23 | 13 |
| [DLSS](#dlss) | 58 | 0 | 0 | 0 | 58 |
| [Ray tracing](#ray-tracing) | 15 | 0 | 6 | 6 | 9 |
| [Debug](#debug) | 44 | 36 | 42 | 42 | 2 |
| **All** | **677** | **430** | **470** | **465** | **212** |

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
| nvn-11 | nvn | the samples counter reports a quad's 256 pixels and none after a reset, through the layer's own NVN functions | the NRO |
| nvn-12 | nvn | two values a list reports are in memory once it has run, through the layer's own NVN functions | the NRO |
| nvn-13 | nvn | two timestamp reports hold no count and times between the device's clock before and after, through the layer's own NVN functions | the NRO |
| nvn-14 | nvn | a timestamp report's word is the graphics unit's ticks, 384 every 625 nanoseconds, through the layer's own NVN functions | the NRO |
| nvn-15 | nvn | the render enable draws under on and under a condition that holds and skips the rest, through the layer's own NVN functions | the NRO |
| nvn-16 | nvn | a command buffer's memory used and free come to what it was given, and draws take some, through the layer's own NVN functions | the NRO |
| nvn-17 | nvn | a list out of command memory asks the program's callback and draws on, through the layer's own NVN functions | the NRO |
| nvn-18 | nvn | a list called from another and a list copied into it draw their cells, through the layer's own NVN functions | the NRO |
| nvn-19 | nvn | clears of an unsigned and a signed integer target hold the integers given, through the layer's own NVN functions | the NRO |
| nvn-20 | nvn | a clear of a texture that is no target reaches its region and its mask's channels, through the layer's own NVN functions | the NRO |
| nvn-21 | nvn | a range of a buffer cleared to a word holds it, and nothing beside the range, through the layer's own NVN functions | the NRO |
| nvn-22 | nvn | a command buffer gives back the copy strides it was set, through the layer's own NVN functions | the NRO |
| nvn-23 | nvn | targets discarded, then cleared and drawn, hold the clear and the draw, through the layer's own NVN functions | the NRO |
| nvn-24 | nvn | a scissor and a viewport set by number bound and place a quad, through the layer's own NVN functions | the NRO |
| nvn-25 | nvn | depth ranges move a quad across the depth test and depth bounds test the target's depth, through the layer's own NVN functions | the NRO |
| nvn-26 | nvn | the rasterizer discards, clamps depth and takes a flat colour from the provoking vertex, through the layer's own NVN functions | the NRO |
| nvn-27 | nvn | a point of sixteen pixels fills its cell, through the layer's own NVN functions | the NRO |
| nvn-28 | nvn | a strip with a restart index is two quads, through the layer's own NVN functions | the NRO |
| nvn-29 | nvn | a viewport swizzle of x and y turns a quad about the diagonal, through the layer's own NVN functions | the NRO |
| nvn-30 | nvn | draws around the tiled cache's actions are as without them, through the layer's own NVN functions | the NRO |
| nvn-31 | nvn | conservative rasterization draws every pixel a quad touches, through the layer's own NVN functions | the NRO |
| nvn-32 | nvn | a conservative quad dilated by half a pixel reaches the pixels a third of one away, through the layer's own NVN functions | the NRO |
| nvn-33 | nvn | a line of width four is four rows of pixels, through the layer's own NVN functions | the NRO |
| nvn-34 | nvn | patches of the size and the outer levels a list sets fill their cells, through the layer's own NVN functions | the NRO |
| nvn-35 | nvn | draws between debug markers, groups and checkpoints are as without them, through the layer's own NVN functions | the NRO |
| nvn-36 | nvn | the device says its modes and its builder its flags, and clear values register, through the layer's own NVN functions | the NRO |
| nvn-37 | nvn | a program set from the longer shader data draws, through the layer's own NVN functions | the NRO |
| nvn-38 | nvn | a texture's handle and a sampler's put together sample, and a texel fetch handle fetches, through the layer's own NVN functions | the NRO |
| nvn-39 | nvn | the program's NVN functions resolve | the NRO, the Switch 2 program |
| nvn-40 | nvn | the device initializes | the NRO, the Switch 2 program |
| nvn-41 | nvn | the device answers the layer's API version | the NRO, the Switch 2 program |
| nvn-42 | nvn | a queue, its pools and a 64x64 target come up | the NRO, the Switch 2 program |
| nvn-43 | nvn | clear to opaque red | the NRO, the Switch 2 program |
| nvn-44 | nvn | clear to a mid channel value | the NRO, the Switch 2 program |
| nvn-45 | nvn | two copy textures and a staging pool come up | the NRO, the Switch 2 program |
| nvn-46 | nvn | a picture copied into a texture and back keeps every texel | the NRO, the Switch 2 program |
| nvn-47 | nvn | a texture copied into another keeps every texel | the NRO, the Switch 2 program |
| nvn-48 | nvn | a buffer copied into another keeps every byte | the NRO, the Switch 2 program |
| nvn-49 | nvn | a texture copied to a buffer with MIRROR_Y lands upside down | the NRO, the Switch 2 program |
| nvn-50 | nvn | a texture copied into another with MIRROR_X lands mirrored | the NRO, the Switch 2 program |
| nvn-51 | nvn | a copy to a buffer with a row stride of 288 keeps the bytes between rows | the NRO, the Switch 2 program |
| nvn-52 | nvn | a texture builder gives back each number it was set | the NRO, the Switch 2 program |
| nvn-53 | nvn | a texture view says which of its parts were set, and with what | the NRO, the Switch 2 program |
| nvn-54 | nvn | a texture of three levels comes up and says what its builder had | the NRO, the Switch 2 program |
| nvn-55 | nvn | a picture copied into each level of a texture through a view comes back from that level | the NRO, the Switch 2 program |
| nvn-56 | nvn | a memory pool and its builder give back what they were set, and the pool maps its storage | the NRO, the Switch 2 program |
| nvn-57 | nvn | a page flushed from a cached pool is what a copy reads, and the copy is there after an invalidate | the NRO, the Switch 2 program |
| nvn-58 | nvn | a sync fenced on the queue behind a clear signals, and the clear is there with no finish | the NRO, the Switch 2 program |
| nvn-59 | nvn | a sync fenced inside the list behind a clear signals, and the clear is there with no finish | the NRO, the Switch 2 program |
| nvn-60 | nvn | a queue and a list told to wait for a sync that signalled run their clear | the NRO, the Switch 2 program |
| nvn-61 | nvn | a wait on a sync nothing fenced fails, with a timeout or without | the NRO, the Switch 2 program |
| nvn-62 | nvn | the queue has no error after work that was valid | the NRO, the Switch 2 program |
| nvn-63 | nvn | a queue builder gives back its flags and says which of its sizes were set | the NRO, the Switch 2 program |
| nvn-64 | nvn | an event keeps what the processor signals: a write, an add and an add that wraps | the NRO, the Switch 2 program |
| nvn-65 | nvn | a sampler builder gives back each number it was set | the NRO, the Switch 2 program |
| nvn-66 | nvn | a sampler says what its builder had, and is the same as another of that state alone | the NRO, the Switch 2 program |
| nvn-67 | nvn | the texture and sampler pools say where they are, and a builder registers as its sampler | the NRO, the Switch 2 program |
| nvn-68 | nvn | a buffer is a range of its pool: its address, its mapping, and what it and its builder say | the NRO, the Switch 2 program |
| nvn-69 | nvn | bytes flushed from one buffer and copied to another are there after an invalidate | the NRO, the Switch 2 program |
| nvn-70 | nvn | a blend state gives back its target, functions, equations and advanced mode | the NRO, the Switch 2 program |
| nvn-71 | nvn | a channel mask and a colour state give back a target's channels, blending, the logic operation and the alpha test | the NRO, the Switch 2 program |
| nvn-72 | nvn | a depth and stencil state gives back its depth test and each face's stencil | the NRO, the Switch 2 program |
| nvn-73 | nvn | a multisample state gives back its samples, its switches and its sample locations' grid | the NRO, the Switch 2 program |
| nvn-74 | nvn | a polygon state, a vertex attribute and a vertex stream give back what they were set | the NRO, the Switch 2 program |
| nvn-75 | nvn | the debug layer's calls are answered as a release driver answers them, and a label changes no object | the NRO, the Switch 2 program |
| nvn-76 | nvn | a window builder gives back what it was set | the NRO, the Switch 2 program |
| nvn-77 | nvn | a window acquires and presents both its textures | the NRO, the Switch 2 program |
| nvn-78 | nvn | a window says what it is of, and keeps the active textures, the interval and the crop it is set | the NRO, the Switch 2 program |
| nvn-79 | nvn | a texture the window hands with a sync is free once the sync signals, and presents | the NRO, the Switch 2 program |
| nvn-80 | nvn | the same scene through the NVN the program looks up holds what the processor works out | the NRO |
| nvn-81 | nvn | the same texture past its edges through the NVN the program looks up does the same | the NRO |
| nvn-82 | nvn | the same four indexed quads through the NVN the program looks up are each in its quarter | the NRO |
| nvn-83 | nvn | the scene of five draws bound through the looked up calls that take several holds what the processor works out | the NRO |
| nvn-84 | nvn | the scene of five draws bound through the looked up deferred calls holds what the processor works out | the NRO |
| nvn-85 | nvn | the same four quads of an updated uniform buffer through the NVN the program looks up are each its own colour | the NRO |
| nvn-86 | nvn | the samples counter reports a quad's 256 pixels and none after a reset, through the NVN the program looks up | the NRO |
| nvn-87 | nvn | two values a list reports are in memory once it has run, through the NVN the program looks up | the NRO |
| nvn-88 | nvn | two timestamp reports hold no count and times between the device's clock before and after, through the NVN the program looks up | the NRO |
| nvn-89 | nvn | a timestamp report's word is the graphics unit's ticks, 384 every 625 nanoseconds, through the NVN the program looks up | the NRO |
| nvn-90 | nvn | the render enable draws under on and under a condition that holds and skips the rest, through the NVN the program looks up | the NRO |
| nvn-91 | nvn | a command buffer's memory used and free come to what it was given, and draws take some, through the NVN the program looks up | the NRO |
| nvn-92 | nvn | a list out of command memory asks the program's callback and draws on, through the NVN the program looks up | the NRO |
| nvn-93 | nvn | a list called from another and a list copied into it draw their cells, through the NVN the program looks up | the NRO |
| nvn-94 | nvn | clears of an unsigned and a signed integer target hold the integers given, through the NVN the program looks up | the NRO |
| nvn-95 | nvn | a clear of a texture that is no target reaches its region and its mask's channels, through the NVN the program looks up | the NRO |
| nvn-96 | nvn | a range of a buffer cleared to a word holds it, and nothing beside the range, through the NVN the program looks up | the NRO |
| nvn-97 | nvn | a command buffer gives back the copy strides it was set, through the NVN the program looks up | the NRO |
| nvn-98 | nvn | targets discarded, then cleared and drawn, hold the clear and the draw, through the NVN the program looks up | the NRO |
| nvn-99 | nvn | a scissor and a viewport set by number bound and place a quad, through the NVN the program looks up | the NRO |
| nvn-100 | nvn | depth ranges move a quad across the depth test and depth bounds test the target's depth, through the NVN the program looks up | the NRO |
| nvn-101 | nvn | the rasterizer discards, clamps depth and takes a flat colour from the provoking vertex, through the NVN the program looks up | the NRO |
| nvn-102 | nvn | a point of sixteen pixels fills its cell, through the NVN the program looks up | the NRO |
| nvn-103 | nvn | a strip with a restart index is two quads, through the NVN the program looks up | the NRO |
| nvn-104 | nvn | a viewport swizzle of x and y turns a quad about the diagonal, through the NVN the program looks up | the NRO |
| nvn-105 | nvn | draws around the tiled cache's actions are as without them, through the NVN the program looks up | the NRO |
| nvn-106 | nvn | conservative rasterization draws every pixel a quad touches, through the NVN the program looks up | the NRO |
| nvn-107 | nvn | a conservative quad dilated by half a pixel reaches the pixels a third of one away, through the NVN the program looks up | the NRO |
| nvn-108 | nvn | a line of width four is four rows of pixels, through the NVN the program looks up | the NRO |
| nvn-109 | nvn | patches of the size and the outer levels a list sets fill their cells, through the NVN the program looks up | the NRO |
| nvn-110 | nvn | draws between debug markers, groups and checkpoints are as without them, through the NVN the program looks up | the NRO |
| nvn-111 | nvn | the device says its modes and its builder its flags, and clear values register, through the NVN the program looks up | the NRO |
| nvn-112 | nvn | a program set from the longer shader data draws, through the NVN the program looks up | the NRO |
| nvn-113 | nvn | a texture's handle and a sampler's put together sample, and a texel fetch handle fetches, through the NVN the program looks up | the NRO |
| nvn-114 | nvn | a clear between the Ounce's graphics and compute markers and groups is the clear | the Switch 2 program |
| nvn-115 | nvn | an Ounce program builder gives back what it was set, a ray tracing program's numbers too | the Switch 2 program |
| nvn-116 | nvn | an Ounce window builder starts with the driver's colour space and HDR metadata, and gives back what it is set | the Switch 2 program |
| nvn-117 | nvn | an Ounce window has its builder's HDR metadata, and gives back what it is set | the Switch 2 program |
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
| `nvnBootstrapLoader` | 55, 231 | nvn-39, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn2-1 |
| `nvnDeviceGetProcAddress` | 55, 231 | nvn-39, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn2-1 |
| `nvnDeviceApplyDeferredFinalizes` |  |  |
| `nvnDeviceBuilderGetFlags` | 55, 231 | nvn-111 |
| `nvnDeviceFinalize` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnDeviceRegisterFastClearColori` | 55, 231 | nvn-111 |
| `nvnDeviceRegisterFastClearColorui` | 55, 231 | nvn-111 |
| `nvnDeviceSetIntermediateShaderCache` |  |  |
| `nvnDeviceBuilderSetDefaults` | 55, 231 | nvn-40, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-111, nvn2-1 |
| `nvnDeviceBuilderSetFlags` | 55, 231 | nvn-40, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-111, nvn2-1 |
| `nvnDeviceGetImageHandle` | 231 |  |
| `nvnDeviceFinalizeCommandHandle` |  |  |
| `nvnDeviceGetCurrentTimestampInNanoseconds` | 55, 231 | nvn-13, nvn-14, nvn-88, nvn-89 |
| `nvnDeviceGetDepthMode` | 55, 231 | nvn-111 |
| `nvnDeviceGetInteger` | 55, 231 | nvn-41, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn2-1 |
| `nvnDeviceGetWindowOriginMode` | 55, 231 | nvn-111 |
| `nvnDeviceGetSeparateSamplerHandle` | 55, 231 | nvn-113 |
| `nvnDeviceGetSeparateTextureHandle` | 55, 231 | nvn-113 |
| `nvnDeviceGetTexelFetchHandle` | 55, 231 | nvn-113 |
| `nvnDeviceGetTextureHandle` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnDeviceInitialize` | 55, 231 | nvn-40, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn2-1 |
| `nvnDeviceRegisterFastClearColor` | 55, 231 | nvn-111 |
| `nvnDeviceRegisterFastClearDepth` | 55, 231 | nvn-111 |
| `nvnDeviceRegisterFastClearStencil` | 55, 231 | nvn-111 |
| `nvnDeviceSetDepthMode` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85 |
| `nvnDeviceSetWindowOriginMode` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85 |
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
| `nvnMemoryPoolBuilderGetDevice` | 55, 231 | nvn-56 |
| `nvnMemoryPoolBuilderGetFlags` | 55, 231 | nvn-56 |
| `nvnMemoryPoolBuilderGetMemory` | 55, 231 | nvn-56 |
| `nvnMemoryPoolBuilderGetSize` | 55, 231 | nvn-56 |
| `nvnMemoryPoolBuilderSetDefaults` | 55, 231 | nvn-19, nvn-20, nvn-42, nvn-45, nvn-54, nvn-56, nvn-77, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-94, nvn-95, nvn2-1 |
| `nvnMemoryPoolBuilderSetDevice` | 55, 231 | nvn-19, nvn-20, nvn-42, nvn-45, nvn-54, nvn-56, nvn-77, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-94, nvn-95, nvn2-1 |
| `nvnMemoryPoolBuilderSetFlags` | 55, 231 | nvn-19, nvn-20, nvn-42, nvn-45, nvn-54, nvn-56, nvn-77, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-94, nvn-95, nvn2-1 |
| `nvnMemoryPoolBuilderSetStorage` | 55, 231 | nvn-19, nvn-20, nvn-42, nvn-45, nvn-54, nvn-56, nvn-77, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-94, nvn-95, nvn2-1 |
| `nvnMemoryPoolBuilderSetNativeHandleNVX` |  |  |
| `nvnMemoryPoolFinalize` | 55, 231 | nvn-19, nvn-20, nvn-77, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-94, nvn-95, nvn-109, nvn-113 |
| `nvnMemoryPoolFlushMappedRange` | 55, 231 | nvn-57, nvn-69 |
| `nvnMemoryPoolGetBufferAddress` | 55, 231 | nvn-43, nvn-44, nvn-46, nvn-47, nvn-48, nvn-49, nvn-50, nvn-51, nvn-55, nvn-57, nvn-58, nvn-59, nvn-60, nvn-68, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113, nvn-114, nvn2-2, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-8 |
| `nvnMemoryPoolGetFlags` | 55, 231 | nvn-56 |
| `nvnMemoryPoolGetSize` | 55, 231 | nvn-56, nvn-75 |
| `nvnMemoryPoolInitialize` | 55, 231 | nvn-19, nvn-20, nvn-42, nvn-45, nvn-54, nvn-56, nvn-77, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-94, nvn-95, nvn2-1 |
| `nvnMemoryPoolInvalidateMappedRange` | 55, 231 | nvn-57 |
| `nvnMemoryPoolMap` | 55, 231 | nvn-43, nvn-44, nvn-45, nvn-46, nvn-47, nvn-48, nvn-49, nvn-50, nvn-51, nvn-55, nvn-56, nvn-57, nvn-58, nvn-59, nvn-60, nvn-64, nvn-67, nvn-68, nvn-69, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113, nvn-114, nvn2-2, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-8 |
| `nvnMemoryPoolMapVirtual` |  |  |

## Buffers

16 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnBufferBuilderGetDevice` | 55, 231 | nvn-68 |
| `nvnBufferBuilderGetMemoryOffset` | 55, 231 | nvn-68 |
| `nvnBufferBuilderGetMemoryPool` | 55, 231 | nvn-68 |
| `nvnBufferBuilderGetSize` | 55, 231 | nvn-68 |
| `nvnBufferBuilderSetDefaults` | 55, 231 | nvn-68, nvn-69, nvn-75 |
| `nvnBufferBuilderSetDevice` | 55, 231 | nvn-68, nvn-69, nvn-75 |
| `nvnBufferBuilderSetStorage` | 55, 231 | nvn-68, nvn-69, nvn-75 |
| `nvnBufferFinalize` | 55, 231 | nvn-68, nvn-69, nvn-75 |
| `nvnBufferFlushMappedRange` | 55, 231 | nvn-69 |
| `nvnBufferInvalidateMappedRange` | 55, 231 | nvn-69 |
| `nvnBufferGetAddress` | 55, 231 | nvn-68, nvn-69 |
| `nvnBufferGetMemoryOffset` | 55, 231 | nvn-68 |
| `nvnBufferGetMemoryPool` | 55, 231 | nvn-68 |
| `nvnBufferGetSize` | 55, 231 | nvn-68, nvn-75 |
| `nvnBufferInitialize` | 55, 231 | nvn-68, nvn-69, nvn-75 |
| `nvnBufferMap` | 55, 231 | nvn-68, nvn-69 |

## Textures

95 functions, 42 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnTextureBuilderGetGLTextureName` |  |  |
| `nvnTextureBuilderGetMemoryOffset` | 55, 231 | nvn-52 |
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
| `nvnTextureViewCompare` | 55, 231 | nvn-53 |
| `nvnTextureBuilderGetDepthStencilMode` |  |  |
| `nvnTextureBuilderGetDevice` | 55, 231 | nvn-52 |
| `nvnTextureBuilderGetMemoryPool` | 55, 231 | nvn-52 |
| `nvnTextureBuilderGetPackagedTextureData` |  |  |
| `nvnTextureBuilderGetSwizzle` |  |  |
| `nvnTextureBuilderGetDepth` | 55, 231 | nvn-52 |
| `nvnTextureBuilderGetFlags` | 55, 231 | nvn-52 |
| `nvnTextureBuilderGetFormat` | 55, 231 | nvn-52 |
| `nvnTextureBuilderGetHeight` | 55, 231 | nvn-52 |
| `nvnTextureBuilderGetLevels` | 55, 231 | nvn-52 |
| `nvnTextureBuilderGetSamples` | 55, 231 | nvn-52 |
| `nvnTextureBuilderGetStorageAlignment` | 55, 231 | nvn-19, nvn-20, nvn-42, nvn-54, nvn-77, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-94, nvn-95, nvn-109, nvn-113, nvn2-1 |
| `nvnTextureBuilderGetStorageClass` |  |  |
| `nvnTextureBuilderGetStorageSize` | 55, 231 | nvn-19, nvn-20, nvn-42, nvn-45, nvn-54, nvn-77, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-94, nvn-95, nvn-109, nvn-113, nvn2-1 |
| `nvnTextureBuilderGetStride` |  |  |
| `nvnTextureBuilderGetTarget` | 55, 231 | nvn-52 |
| `nvnTextureBuilderGetWidth` | 55, 231 | nvn-52 |
| `nvnTextureBuilderGetZCullStorageSize` |  |  |
| `nvnTextureBuilderSetDefaults` | 55, 231 | nvn-19, nvn-20, nvn-42, nvn-45, nvn-52, nvn-54, nvn-77, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-94, nvn-95, nvn-109, nvn-113, nvn2-1 |
| `nvnTextureBuilderSetDepth` | 55, 231 | nvn-52, nvn-54 |
| `nvnTextureBuilderSetDepthStencilMode` |  |  |
| `nvnTextureBuilderSetDevice` | 55, 231 | nvn-19, nvn-20, nvn-42, nvn-45, nvn-52, nvn-54, nvn-77, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-94, nvn-95, nvn-109, nvn-113, nvn2-1 |
| `nvnTextureBuilderSetFlags` | 55, 231 | nvn-42, nvn-45, nvn-52, nvn-54, nvn-77, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113, nvn2-1 |
| `nvnTextureBuilderSetFormat` | 55, 231 | nvn-19, nvn-20, nvn-42, nvn-45, nvn-52, nvn-54, nvn-77, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-94, nvn-95, nvn-109, nvn-113, nvn2-1 |
| `nvnTextureBuilderSetHeight` | 55, 231 | nvn-52 |
| `nvnTextureBuilderSetLevels` | 55, 231 | nvn-52, nvn-54 |
| `nvnTextureBuilderSetPackagedTextureData` |  |  |
| `nvnTextureBuilderSetPackagedTextureLayout` |  |  |
| `nvnTextureBuilderSetSamples` | 55, 231 | nvn-52, nvn-54 |
| `nvnTextureBuilderSetSize1D` |  |  |
| `nvnTextureBuilderSetSize2D` | 55, 231 | nvn-19, nvn-20, nvn-42, nvn-45, nvn-52, nvn-54, nvn-77, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-94, nvn-95, nvn-109, nvn-113, nvn2-1 |
| `nvnTextureBuilderSetSize3D` |  |  |
| `nvnTextureBuilderSetStorage` | 55, 231 | nvn-19, nvn-20, nvn-42, nvn-45, nvn-52, nvn-54, nvn-77, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-94, nvn-95, nvn-109, nvn-113, nvn2-1 |
| `nvnTextureBuilderSetStride` |  |  |
| `nvnTextureBuilderSetSwizzle` |  |  |
| `nvnTextureBuilderSetTarget` | 55, 231 | nvn-19, nvn-20, nvn-42, nvn-45, nvn-52, nvn-54, nvn-77, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-94, nvn-95, nvn-109, nvn-113, nvn2-1 |
| `nvnTextureBuilderSetWidth` | 55, 231 | nvn-52 |
| `nvnTextureCompare` |  |  |
| `nvnTextureFinalize` | 55, 231 | nvn-19, nvn-20, nvn-77, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-94, nvn-95, nvn-109, nvn-113 |
| `nvnTextureFlushTexels` |  |  |
| `nvnTextureGetDepth` | 55, 231 | nvn-54 |
| `nvnTextureGetDepthStencilMode` |  |  |
| `nvnTextureGetFlags` | 55, 231 | nvn-54 |
| `nvnTextureGetFormat` | 55, 231 | nvn-54 |
| `nvnTextureGetHeight` | 55, 231 | nvn-54 |
| `nvnTextureGetLevels` | 55, 231 | nvn-54 |
| `nvnTextureGetMemoryOffset` | 55, 231 | nvn-54 |
| `nvnTextureGetMemoryPool` | 55, 231 | nvn-54 |
| `nvnTextureGetSamples` | 55, 231 | nvn-54 |
| `nvnTextureGetStorageClass` |  |  |
| `nvnTextureGetStorageSize` |  |  |
| `nvnTextureGetStride` |  |  |
| `nvnTextureGetSwizzle` |  |  |
| `nvnTextureGetTarget` | 55, 231 | nvn-54 |
| `nvnTextureGetTextureAddress` |  |  |
| `nvnTextureGetViewOffset` |  |  |
| `nvnTextureGetWidth` | 55, 231 | nvn-54, nvn-75 |
| `nvnTextureGetZCullStorageSize` |  |  |
| `nvnTextureInitialize` | 55, 231 | nvn-19, nvn-20, nvn-42, nvn-45, nvn-54, nvn-77, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-94, nvn-95, nvn-109, nvn-113, nvn2-1 |
| `nvnTexturePoolFinalize` | 55, 231 | nvn-67, nvn-75, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnTexturePoolGetMemoryOffset` | 55, 231 | nvn-67 |
| `nvnTexturePoolGetMemoryPool` | 55, 231 | nvn-67 |
| `nvnTexturePoolGetSize` | 55, 231 | nvn-67, nvn-75 |
| `nvnTexturePoolInitialize` | 55, 231 | nvn-67, nvn-75, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85 |
| `nvnTexturePoolRegisterImage` | 231 |  |
| `nvnTexturePoolRegisterTexture` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnTextureReadTexels` |  |  |
| `nvnTextureViewGetDepthStencilMode` |  |  |
| `nvnTextureViewGetFormat` | 55, 231 | nvn-53 |
| `nvnTextureViewGetLayers` | 55, 231 | nvn-53 |
| `nvnTextureViewGetLevels` | 55, 231 | nvn-53 |
| `nvnTextureViewGetSwizzle` |  |  |
| `nvnTextureViewGetTarget` | 55, 231 | nvn-53 |
| `nvnTextureViewSetDefaults` | 55, 231 | nvn-53, nvn-55 |
| `nvnTextureViewSetDepthStencilMode` |  |  |
| `nvnTextureViewSetFormat` | 55, 231 | nvn-53 |
| `nvnTextureViewSetLayers` | 55, 231 | nvn-53 |
| `nvnTextureViewSetLevels` | 55, 231 | nvn-53, nvn-55 |
| `nvnTextureViewSetSwizzle` |  |  |
| `nvnTextureViewSetTarget` | 55, 231 | nvn-53 |
| `nvnTextureWriteTexels` |  |  |
| `nvnTextureWriteTexelsStrided` |  |  |

## Samplers

45 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnSamplerBuilderGetBorderColor` | 55, 231 | nvn-65, nvn-66 |
| `nvnSamplerBuilderGetBorderColori` | 55, 231 | nvn-65 |
| `nvnSamplerBuilderGetBorderColorui` | 55, 231 | nvn-65 |
| `nvnSamplerBuilderGetCompare` | 55, 231 | nvn-65, nvn-66 |
| `nvnSamplerBuilderGetDevice` | 55, 231 | nvn-65 |
| `nvnSamplerBuilderGetLodBias` | 55, 231 | nvn-65, nvn-66 |
| `nvnSamplerBuilderGetLodClamp` | 55, 231 | nvn-65, nvn-66 |
| `nvnSamplerBuilderGetMaxAnisotropy` | 55, 231 | nvn-65, nvn-66 |
| `nvnSamplerBuilderGetMinMagFilter` | 55, 231 | nvn-65, nvn-66 |
| `nvnSamplerBuilderGetReductionFilter` | 55, 231 | nvn-65, nvn-66 |
| `nvnSamplerBuilderGetWrapMode` | 55, 231 | nvn-65, nvn-66 |
| `nvnSamplerBuilderSetBorderColori` | 55, 231 | nvn-65 |
| `nvnSamplerBuilderSetBorderColorui` | 55, 231 | nvn-65 |
| `nvnSamplerGetBorderColori` | 55, 231 | nvn-66 |
| `nvnSamplerGetBorderColorui` | 55, 231 | nvn-66 |
| `nvnSamplerPoolRegisterSamplerBuilder` | 55, 231 | nvn-67 |
| `nvnSamplerBuilderSetBorderColor` | 55, 231 | nvn-65, nvn-66, nvn-67, nvn-75, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnSamplerBuilderSetCompare` | 55, 231 | nvn-65, nvn-66, nvn-67, nvn-75 |
| `nvnSamplerBuilderSetDefaults` | 55, 231 | nvn-65, nvn-66, nvn-67, nvn-75, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnSamplerBuilderSetDevice` | 55, 231 | nvn-65, nvn-66, nvn-67, nvn-75, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnSamplerBuilderSetLodBias` | 55, 231 | nvn-65, nvn-66, nvn-67, nvn-75 |
| `nvnSamplerBuilderSetLodClamp` | 55, 231 | nvn-65, nvn-66, nvn-67, nvn-75 |
| `nvnSamplerBuilderGetLodSnap` | 55, 231 | nvn-65 |
| `nvnSamplerBuilderSetLodSnap` | 55, 231 | nvn-65, nvn-66, nvn-67, nvn-75 |
| `nvnSamplerBuilderSetMaxAnisotropy` | 55, 231 | nvn-65, nvn-66, nvn-67, nvn-75 |
| `nvnSamplerBuilderSetMinMagFilter` | 55, 231 | nvn-65, nvn-66, nvn-67, nvn-75, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnSamplerBuilderSetReductionFilter` | 55, 231 | nvn-65, nvn-66, nvn-67, nvn-75 |
| `nvnSamplerBuilderSetWrapMode` | 55, 231 | nvn-65, nvn-66, nvn-67, nvn-75, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnSamplerCompare` | 55, 231 | nvn-66 |
| `nvnSamplerFinalize` | 55, 231 | nvn-66, nvn-67, nvn-75, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnSamplerGetBorderColor` | 55, 231 | nvn-66 |
| `nvnSamplerGetCompare` | 55, 231 | nvn-66 |
| `nvnSamplerGetLodBias` | 55, 231 | nvn-66 |
| `nvnSamplerGetLodClamp` | 55, 231 | nvn-66 |
| `nvnSamplerGetMaxAnisotropy` | 55, 231 | nvn-66 |
| `nvnSamplerGetMinMagFilter` | 55, 231 | nvn-66 |
| `nvnSamplerGetReductionFilter` | 55, 231 | nvn-66 |
| `nvnSamplerGetWrapMode` | 55, 231 | nvn-66 |
| `nvnSamplerInitialize` | 55, 231 | nvn-66, nvn-67, nvn-75, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnSamplerPoolFinalize` | 55, 231 | nvn-67, nvn-75, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnSamplerPoolGetMemoryOffset` | 55, 231 | nvn-67 |
| `nvnSamplerPoolGetMemoryPool` | 55, 231 | nvn-67 |
| `nvnSamplerPoolGetSize` | 55, 231 | nvn-67, nvn-75 |
| `nvnSamplerPoolInitialize` | 55, 231 | nvn-67, nvn-75, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85 |
| `nvnSamplerPoolRegisterSampler` | 55, 231 | nvn-67, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |

## Programs

20 functions, 7 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnProgramGetSubtileSize` |  |  |
| `nvnProgramGetScratchBytesPerWarp` |  |  |
| `nvnProgramSetSampleShading` |  |  |
| `nvnProgramSetSubroutineLinkage` |  |  |
| `nvnProgramFinalize` | 55, 231 | nvn-34, nvn-37, nvn-38, nvn-75, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-112, nvn-113 |
| `nvnProgramInitialize` | 55, 231 | nvn-75, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-112, nvn-113 |
| `nvnProgramSetShaders` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnProgramSetShadersExt` | 55, 231 | nvn-112 |
| `nvnProgramBuilderSetDefaults` | 231 | nvn-115, nvn2-2, nvn2-5, nvn2-8 |
| `nvnProgramBuilderSetDevice` | 231 | nvn-115, nvn2-2, nvn2-5, nvn2-8 |
| `nvnProgramBuilderGetDevice` | 231 | nvn-115 |
| `nvnProgramBuilderSetSampleShading` | 231 | nvn-115 |
| `nvnProgramBuilderGetSampleShading` | 231 | nvn-115 |
| `nvnProgramBuilderSetShaders` | 231 | nvn-115, nvn2-2, nvn2-5, nvn2-8 |
| `nvnProgramBuilderGetShaders` | 231 | nvn-115 |
| `nvnProgramBuilderGetShaderCount` | 231 | nvn-115 |
| `nvnProgramBuilderSetSubroutineLinkage` |  |  |
| `nvnProgramBuilderGetSubroutineLinkage` |  |  |
| `nvnProgramBuilderGetSubroutineLinkageCount` |  |  |
| `nvnProgramInitializeFromBuilder` | 231 | nvn2-2, nvn2-5, nvn2-8 |

## State objects

82 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnBlendStateGetAdvancedMode` | 55, 231 | nvn-70 |
| `nvnBlendStateGetAdvancedNormalizedDst` | 55, 231 | nvn-70 |
| `nvnBlendStateGetAdvancedOverlap` | 55, 231 | nvn-70 |
| `nvnBlendStateGetAdvancedPremultipliedSrc` | 55, 231 | nvn-70 |
| `nvnBlendStateSetAdvancedMode` | 55, 231 | nvn-70 |
| `nvnBlendStateSetAdvancedNormalizedDst` | 55, 231 | nvn-70 |
| `nvnBlendStateSetAdvancedOverlap` | 55, 231 | nvn-70 |
| `nvnBlendStateSetAdvancedPremultipliedSrc` | 55, 231 | nvn-70 |
| `nvnMultisampleStateGetSampleLocationsGrid` | 55, 231 | nvn-73 |
| `nvnMultisampleStateSetSampleLocations` | 55, 231 | nvn-73 |
| `nvnBlendStateSetBlendEquation` | 55, 231 | nvn-70 |
| `nvnBlendStateSetBlendFunc` | 55, 231 | nvn-70 |
| `nvnBlendStateSetBlendTarget` | 55, 231 | nvn-70 |
| `nvnBlendStateSetDefaults` | 55, 231 | nvn-70, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113, nvn2-7 |
| `nvnChannelMaskStateSetChannelMask` | 55, 231 | nvn-71 |
| `nvnChannelMaskStateSetDefaults` | 55, 231 | nvn-71, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113, nvn2-7 |
| `nvnColorStateGetBlendEnable` | 55, 231 | nvn-71 |
| `nvnColorStateSetAlphaTest` | 55, 231 | nvn-71 |
| `nvnColorStateSetBlendEnable` | 55, 231 | nvn-71 |
| `nvnColorStateSetDefaults` | 55, 231 | nvn-71, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113, nvn2-7 |
| `nvnColorStateSetLogicOp` | 55, 231 | nvn-71 |
| `nvnDepthStencilStateSetDefaults` | 55, 231 | nvn-72, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113, nvn2-7 |
| `nvnDepthStencilStateSetDepthFunc` | 55, 231 | nvn-72, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113, nvn2-7 |
| `nvnDepthStencilStateSetDepthTestEnable` | 55, 231 | nvn-72, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113, nvn2-7 |
| `nvnDepthStencilStateSetDepthWriteEnable` | 55, 231 | nvn-72, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113, nvn2-7 |
| `nvnDepthStencilStateSetStencilFunc` | 55, 231 | nvn-72 |
| `nvnDepthStencilStateSetStencilOp` | 55, 231 | nvn-72 |
| `nvnDepthStencilStateSetStencilTestEnable` | 55, 231 | nvn-72 |
| `nvnMultisampleStateSetAlphaToCoverageEnable` | 55, 231 | nvn-73 |
| `nvnMultisampleStateSetDefaults` | 55, 231 | nvn-73, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113, nvn2-7 |
| `nvnMultisampleStateSetMultisampleEnable` | 55, 231 | nvn-73 |
| `nvnMultisampleStateSetSamples` | 55, 231 | nvn-73 |
| `nvnPolygonStateSetCullFace` | 55, 231 | nvn-74, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113, nvn2-7 |
| `nvnPolygonStateSetDefaults` | 55, 231 | nvn-74, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113, nvn2-7 |
| `nvnPolygonStateSetFrontFace` | 55, 231 | nvn-74, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnPolygonStateSetPolygonMode` | 55, 231 | nvn-74 |
| `nvnPolygonStateSetPolygonOffsetEnables` | 55, 231 | nvn-74 |
| `nvnVertexAttribStateSetDefaults` | 55, 231 | nvn-74, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnVertexAttribStateSetFormat` | 55, 231 | nvn-74, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnVertexAttribStateSetStreamIndex` | 55, 231 | nvn-74, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnVertexStreamStateSetDefaults` | 55, 231 | nvn-74, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnVertexStreamStateSetDivisor` | 55, 231 | nvn-74 |
| `nvnVertexStreamStateSetStride` | 55, 231 | nvn-74, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnBlendStateGetBlendEquation` | 55, 231 | nvn-70 |
| `nvnBlendStateGetBlendFunc` | 55, 231 | nvn-70 |
| `nvnBlendStateGetBlendTarget` | 55, 231 | nvn-70 |
| `nvnChannelMaskStateGetChannelMask` | 55, 231 | nvn-71 |
| `nvnColorStateGetAlphaTest` | 55, 231 | nvn-71 |
| `nvnColorStateGetLogicOp` | 55, 231 | nvn-71 |
| `nvnDepthStencilStateGetDepthFunc` | 55, 231 | nvn-72 |
| `nvnDepthStencilStateGetDepthTestEnable` | 55, 231 | nvn-72 |
| `nvnDepthStencilStateGetDepthWriteEnable` | 55, 231 | nvn-72 |
| `nvnDepthStencilStateGetStencilFunc` | 55, 231 | nvn-72 |
| `nvnDepthStencilStateGetStencilOp` | 55, 231 | nvn-72 |
| `nvnDepthStencilStateGetStencilTestEnable` | 55, 231 | nvn-72 |
| `nvnMultisampleStateGetAlphaToCoverageDither` | 55, 231 | nvn-73 |
| `nvnMultisampleStateGetAlphaToCoverageEnable` | 55, 231 | nvn-73 |
| `nvnMultisampleStateGetCoverageModulationMode` | 55, 231 | nvn-73 |
| `nvnMultisampleStateGetCoverageToColorEnable` | 55, 231 | nvn-73 |
| `nvnMultisampleStateGetCoverageToColorOutput` | 55, 231 | nvn-73 |
| `nvnMultisampleStateGetMultisampleEnable` | 55, 231 | nvn-73 |
| `nvnMultisampleStateGetRasterSamples` | 55, 231 | nvn-73 |
| `nvnMultisampleStateGetSampleLocationsEnable` | 55, 231 | nvn-73 |
| `nvnMultisampleStateGetSampleLocationsGridEnable` | 55, 231 | nvn-73 |
| `nvnMultisampleStateGetSamples` | 55, 231 | nvn-73 |
| `nvnMultisampleStateSetAlphaToCoverageDither` | 55, 231 | nvn-73 |
| `nvnMultisampleStateSetCoverageModulationMode` | 55, 231 | nvn-73 |
| `nvnMultisampleStateSetCoverageToColorEnable` | 55, 231 | nvn-73 |
| `nvnMultisampleStateSetCoverageToColorOutput` | 55, 231 | nvn-73 |
| `nvnMultisampleStateSetRasterSamples` | 55, 231 | nvn-73 |
| `nvnMultisampleStateSetCoverageTruncationEnable` | 55, 231 | nvn-73 |
| `nvnMultisampleStateGetCoverageTruncationEnable` | 55, 231 | nvn-73 |
| `nvnMultisampleStateSetSampleLocationsEnable` | 55, 231 | nvn-73 |
| `nvnMultisampleStateSetSampleLocationsGridEnable` | 55, 231 | nvn-73 |
| `nvnPolygonStateGetCullFace` | 55, 231 | nvn-74 |
| `nvnPolygonStateGetFrontFace` | 55, 231 | nvn-74 |
| `nvnPolygonStateGetPolygonMode` | 55, 231 | nvn-74 |
| `nvnPolygonStateGetPolygonOffsetEnables` | 55, 231 | nvn-74 |
| `nvnVertexAttribStateGetFormat` | 55, 231 | nvn-74 |
| `nvnVertexAttribStateGetStreamIndex` | 55, 231 | nvn-74 |
| `nvnVertexStreamStateGetDivisor` | 55, 231 | nvn-74 |
| `nvnVertexStreamStateGetStride` | 55, 231 | nvn-74 |

## Queues

30 functions, 11 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnQueueBuilderGetCommandFlushThreshold` | 55, 231 | nvn-63 |
| `nvnQueueBuilderGetCommandMemorySize` | 55, 231 | nvn-63 |
| `nvnQueueBuilderGetComputeMemorySize` | 55, 231 | nvn-63 |
| `nvnQueueBuilderGetControlMemorySize` | 55, 231 | nvn-63 |
| `nvnQueueBuilderGetDevice` | 55, 231 | nvn-63 |
| `nvnQueueBuilderGetFlags` | 55, 231 | nvn-63 |
| `nvnQueueBuilderGetMemory` |  |  |
| `nvnQueueBuilderGetMemorySize` |  |  |
| `nvnQueueGetTotalCommandMemoryUsed` |  |  |
| `nvnQueueGetTotalComputeMemoryUsed` |  |  |
| `nvnQueueGetTotalControlMemoryUsed` |  |  |
| `nvnQueueResetMemoryUsageCounts` |  |  |
| `nvnQueueBuilderGetQueueMemorySize` |  |  |
| `nvnQueueBuilderSetCommandFlushThreshold` | 55, 231 | nvn-63 |
| `nvnQueueBuilderSetCommandMemorySize` | 55, 231 | nvn-63 |
| `nvnQueueBuilderSetComputeMemorySize` | 55, 231 | nvn-63 |
| `nvnQueueBuilderSetControlMemorySize` | 55, 231 | nvn-63 |
| `nvnQueueBuilderSetDefaults` | 55, 231 | nvn-42, nvn-63, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn2-1 |
| `nvnQueueBuilderSetDevice` | 55, 231 | nvn-42, nvn-63, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn2-1 |
| `nvnQueueBuilderSetFlags` | 55, 231 | nvn-63 |
| `nvnQueueBuilderGetQueuePriority` |  |  |
| `nvnQueueBuilderSetQueuePriority` |  |  |
| `nvnQueueBuilderSetQueueMemory` |  |  |
| `nvnQueueFinalize` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnQueueFinish` | 55, 231 | nvn-43, nvn-44, nvn-46, nvn-47, nvn-48, nvn-49, nvn-50, nvn-51, nvn-55, nvn-57, nvn-58, nvn-59, nvn-60, nvn-62, nvn-69, nvn-77, nvn-78, nvn-79, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113, nvn-114, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnQueueFlush` | 55, 231 | nvn-58, nvn-59, nvn-60 |
| `nvnQueueGetError` | 55, 231 | nvn-62, nvn-75 |
| `nvnQueueInitialize` | 55, 231 | nvn-42, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn2-1 |
| `nvnQueueSubmitCommands` | 55, 231 | nvn-43, nvn-44, nvn-46, nvn-47, nvn-48, nvn-49, nvn-50, nvn-51, nvn-55, nvn-57, nvn-58, nvn-59, nvn-60, nvn-69, nvn-77, nvn-78, nvn-79, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113, nvn-114, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnQueueBuilderSetPrimaryQueue` |  |  |

## Sync and events

24 functions, 6 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnSyncCreateGLSync` |  |  |
| `nvnSyncInitializeFromFencedGLSync` |  |  |
| `nvnSyncFinalize` | 55, 231 | nvn-58, nvn-59, nvn-60, nvn-61, nvn-75, nvn-79 |
| `nvnSyncInitialize` | 55, 231 | nvn-58, nvn-59, nvn-60, nvn-61, nvn-75, nvn-79 |
| `nvnSyncWait` | 55, 231 | nvn-58, nvn-59, nvn-60, nvn-61, nvn-79 |
| `nvnSyncGetNvRmSyncNVX` |  |  |
| `nvnSyncInitializeFromNvRmSyncNVX` |  |  |
| `nvnEventBuilderGetMemoryOffset` | 55, 231 | nvn-64 |
| `nvnEventBuilderGetMemoryPool` | 55, 231 | nvn-64 |
| `nvnEventBuilderGetStorage` | 55, 231 | nvn-64 |
| `nvnEventGetMemoryOffset` | 55, 231 | nvn-64 |
| `nvnEventGetMemoryPool` | 55, 231 | nvn-64 |
| `nvnEventBuilderSetDefaults` | 55, 231 | nvn-64 |
| `nvnEventBuilderSetStorage` | 55, 231 | nvn-64 |
| `nvnEventFinalize` | 55, 231 | nvn-64 |
| `nvnEventGetValue` | 55, 231 | nvn-64 |
| `nvnEventInitialize` | 55, 231 | nvn-64 |
| `nvnEventSignal` | 55, 231 | nvn-64 |
| `nvnQueueFenceSync` | 55, 231 | nvn-58, nvn-60 |
| `nvnQueueWaitSync` | 55, 231 | nvn-58, nvn-59, nvn-60 |
| `nvnCommandBufferFenceSync` | 55, 231 | nvn-59 |
| `nvnCommandBufferSignalEvent` |  |  |
| `nvnCommandBufferWaitEvent` |  |  |
| `nvnCommandBufferWaitSync` | 55, 231 | nvn-60 |

## Windows and present

35 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnQueueAcquireTexture` | 55, 231 | nvn-77, nvn-78 |
| `nvnQueuePresentTexture` | 55, 231 | nvn-77, nvn-78, nvn-79 |
| `nvnWindowBuilderGetDevice` | 55, 231 | nvn-76 |
| `nvnWindowBuilderGetNativeWindow` | 55, 231 | nvn-76 |
| `nvnWindowBuilderGetNumActiveTextures` | 55, 231 | nvn-76 |
| `nvnWindowGetNativeWindow` | 55, 231 | nvn-78 |
| `nvnWindowGetNumActiveTextures` | 55, 231 | nvn-78 |
| `nvnWindowGetNumTextures` | 55, 231 | nvn-78 |
| `nvnWindowSetNumActiveTextures` | 55, 231 | nvn-78 |
| `nvnWindowBuilderGetNumTextures` | 55, 231 | nvn-76 |
| `nvnWindowBuilderGetPresentInterval` | 55, 231 | nvn-76 |
| `nvnWindowBuilderGetTexture` | 55, 231 | nvn-76 |
| `nvnWindowGetCrop` | 55, 231 | nvn-78 |
| `nvnWindowGetPresentInterval` | 55, 231 | nvn-78 |
| `nvnWindowAcquireTexture` | 55, 231 | nvn-79 |
| `nvnWindowBuilderSetDefaults` | 55, 231 | nvn-76, nvn-77, nvn-116 |
| `nvnWindowBuilderSetDevice` | 55, 231 | nvn-76, nvn-77 |
| `nvnWindowBuilderSetNativeWindow` | 55, 231 | nvn-76, nvn-77 |
| `nvnWindowBuilderSetNumActiveTextures` | 55, 231 | nvn-76 |
| `nvnWindowBuilderSetPresentInterval` | 55, 231 | nvn-76 |
| `nvnWindowBuilderSetTextures` | 55, 231 | nvn-76, nvn-77 |
| `nvnWindowBuilderSetColorSpace` | 231 | nvn-116 |
| `nvnWindowBuilderGetColorSpace` | 231 | nvn-116 |
| `nvnWindowBuilderSetHdrDisplayMetadata` | 231 | nvn-116 |
| `nvnWindowBuilderGetHdrDisplayMetadata` | 231 | nvn-116 |
| `nvnWindowBuilderSetHdrContentMetadata` | 231 | nvn-116 |
| `nvnWindowBuilderGetHdrContentMetadata` | 231 | nvn-116 |
| `nvnWindowSetHdrDisplayMetadata` | 231 | nvn-117 |
| `nvnWindowGetHdrDisplayMetadata` | 231 | nvn-117 |
| `nvnWindowSetHdrContentMetadata` | 231 | nvn-117 |
| `nvnWindowGetHdrContentMetadata` | 231 | nvn-117 |
| `nvnWindowFinalize` | 55, 231 | nvn-77 |
| `nvnWindowInitialize` | 55, 231 | nvn-77 |
| `nvnWindowSetCrop` | 55, 231 | nvn-78 |
| `nvnWindowSetPresentInterval` | 55, 231 | nvn-78 |

## Counters and queries

6 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnDeviceGetTimestampInNanoseconds` | 55, 231 | nvn-13, nvn-14, nvn-88, nvn-89 |
| `nvnCommandBufferReportCounter` | 55, 231 | nvn-86, nvn-88, nvn-89, nvn-90 |
| `nvnCommandBufferReportValue` | 55, 231 | nvn-87 |
| `nvnCommandBufferResetCounter` | 55, 231 | nvn-86, nvn-90 |
| `nvnCommandBufferSetRenderEnable` | 55, 231 | nvn-90 |
| `nvnCommandBufferSetRenderEnableConditional` | 55, 231 | nvn-90 |

## Commands: memory and recording

25 functions, 3 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferGetCommandMemoryFree` | 55, 231 | nvn-91 |
| `nvnCommandBufferGetCommandMemorySize` | 55, 231 | nvn-91 |
| `nvnCommandBufferGetControlMemoryFree` | 55, 231 | nvn-91 |
| `nvnCommandBufferGetControlMemorySize` | 55, 231 | nvn-91 |
| `nvnCommandBufferSetControlMemoryCallbackEnabled` | 55, 231 | nvn-92 |
| `nvnCommandBufferAddCommandMemory` | 55, 231 | nvn-42, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-91, nvn-92, nvn2-1 |
| `nvnCommandBufferAddControlMemory` | 55, 231 | nvn-42, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-91, nvn2-1 |
| `nvnCommandBufferBarrier` | 55, 231 | nvn-43, nvn-44, nvn-46, nvn-47, nvn-49, nvn-50, nvn-51, nvn-55, nvn-58, nvn-59, nvn-60, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-94, nvn-95, nvn-109, nvn-113, nvn-114, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferBeginRecording` | 55, 231 | nvn-43, nvn-44, nvn-46, nvn-47, nvn-48, nvn-49, nvn-50, nvn-51, nvn-55, nvn-57, nvn-58, nvn-59, nvn-60, nvn-69, nvn-77, nvn-78, nvn-79, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113, nvn-114, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferSetRecordingMode` |  |  |
| `nvnCommandBufferSetComputeOverlapMode` |  |  |
| `nvnCommandBufferCallCommands` | 55, 231 | nvn-93 |
| `nvnCommandBufferCopyCommands` | 55, 231 | nvn-93 |
| `nvnCommandBufferEndRecording` | 55, 231 | nvn-43, nvn-44, nvn-46, nvn-47, nvn-48, nvn-49, nvn-50, nvn-51, nvn-55, nvn-57, nvn-58, nvn-59, nvn-60, nvn-69, nvn-77, nvn-78, nvn-79, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-93, nvn-109, nvn-113, nvn-114, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferFinalize` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnCommandBufferGetCommandMemoryUsed` | 55, 231 | nvn-91 |
| `nvnCommandBufferGetControlMemoryUsed` | 55, 231 | nvn-91 |
| `nvnCommandBufferGetMemoryCallback` | 55, 231 | nvn-92 |
| `nvnCommandBufferGetMemoryCallbackData` | 55, 231 | nvn-92 |
| `nvnCommandBufferInitialize` | 55, 231 | nvn-42, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn2-1 |
| `nvnCommandBufferIsRecording` | 55, 231 | nvn-91 |
| `nvnCommandBufferSetCommandMemoryCallbackEnabled` | 55, 231 | nvn-92 |
| `nvnCommandBufferSetComputeWorkloadPriority` |  |  |
| `nvnCommandBufferSetMemoryCallback` | 55, 231 | nvn-92 |
| `nvnCommandBufferSetMemoryCallbackData` | 55, 231 | nvn-92 |

## Commands: binds

41 functions, 18 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferBindCoverageModulationTable` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnCommandBufferBindImagesDeferred` |  |  |
| `nvnCommandBufferBindSeparateSamplersDeferred` |  |  |
| `nvnCommandBufferBindSeparateTexturesDeferred` |  |  |
| `nvnCommandBufferBindStorageBuffersDeferred` |  |  |
| `nvnCommandBufferBindUniformBuffersDeferred` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnCommandBufferSetProgramSubroutines` |  |  |
| `nvnCommandBufferBindShadingRateImage` |  |  |
| `nvnCommandBufferShadingRateImagePalette` |  |  |
| `nvnCommandBufferPushGraphicsConstants` | 231 |  |
| `nvnCommandBufferPushComputeConstants` | 231 |  |
| `nvnCommandBufferBindBlendState` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113, nvn2-7 |
| `nvnCommandBufferBindChannelMaskState` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113, nvn2-7 |
| `nvnCommandBufferBindColorState` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113, nvn2-7 |
| `nvnCommandBufferBindDepthStencilState` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113, nvn2-7 |
| `nvnCommandBufferBindImage` | 231 |  |
| `nvnCommandBufferBindImages` |  |  |
| `nvnCommandBufferBindMultisampleState` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113, nvn2-7 |
| `nvnCommandBufferBindPolygonState` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113, nvn2-7 |
| `nvnCommandBufferBindProgram` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-112, nvn-113, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferBindSeparateSampler` |  |  |
| `nvnCommandBufferBindSeparateSamplers` |  |  |
| `nvnCommandBufferBindSeparateTexture` |  |  |
| `nvnCommandBufferBindSeparateTextures` |  |  |
| `nvnCommandBufferBindStorageBuffer` | 231 | nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferBindStorageBuffers` |  |  |
| `nvnCommandBufferBindTexture` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnCommandBufferBindTextures` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnCommandBufferBindTexturesDeferred` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnCommandBufferBindTransformFeedbackBuffer` |  |  |
| `nvnCommandBufferBindTransformFeedbackBuffers` |  |  |
| `nvnCommandBufferBindUniformBuffer` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnCommandBufferBindUniformBuffers` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnCommandBufferBindVertexAttribState` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnCommandBufferBindVertexBuffer` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnCommandBufferBindVertexBuffers` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnCommandBufferBindVertexStreamState` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnCommandBufferSetSamplerPool` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnCommandBufferSetShaderScratchMemory` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnCommandBufferSetTexturePool` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnCommandBufferUpdateUniformBuffer` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |

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
| `nvnCommandBufferDrawArrays` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-102, nvn-108, nvn-109, nvn-113 |
| `nvnCommandBufferDrawArraysIndirect` |  |  |
| `nvnCommandBufferDrawArraysInstanced` |  |  |
| `nvnCommandBufferDrawElements` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-103, nvn-109, nvn-113 |
| `nvnCommandBufferDrawElementsBaseVertex` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
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
| `nvnCommandBufferClearTexturei` | 55, 231 | nvn-95 |
| `nvnCommandBufferGetCopyImageStride` | 55, 231 | nvn-97 |
| `nvnCommandBufferGetCopyRowStride` | 55, 231 | nvn-97 |
| `nvnCommandBufferResolveDepthBuffer` |  |  |
| `nvnCommandBufferTiledDownsample` |  |  |
| `nvnCommandBufferTiledDownsampleTextureView` |  |  |
| `nvnCommandBufferClearBuffer` | 55, 231 | nvn-96 |
| `nvnCommandBufferClearColor` | 55, 231 | nvn-43, nvn-44, nvn-58, nvn-59, nvn-60, nvn-77, nvn-78, nvn-79, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113, nvn-114, nvn2-7 |
| `nvnCommandBufferClearColori` | 55, 231 | nvn-94 |
| `nvnCommandBufferClearColorui` | 55, 231 | nvn-94 |
| `nvnCommandBufferClearDepthStencil` | 55, 231 | nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113, nvn2-7 |
| `nvnCommandBufferClearTexture` | 55, 231 | nvn-95 |
| `nvnCommandBufferClearTextureui` | 55, 231 | nvn-95 |
| `nvnCommandBufferCopyBufferToBuffer` | 55, 231 | nvn-48, nvn-57, nvn-69, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferCopyBufferToTexture` | 55, 231 | nvn-46, nvn-47, nvn-49, nvn-50, nvn-51, nvn-55, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113 |
| `nvnCommandBufferCopyTextureToBuffer` | 55, 231 | nvn-43, nvn-44, nvn-46, nvn-47, nvn-49, nvn-50, nvn-51, nvn-55, nvn-58, nvn-59, nvn-60, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-94, nvn-95, nvn-109, nvn-113, nvn-114, nvn2-7 |
| `nvnCommandBufferCopyTextureToTexture` | 55, 231 | nvn-47, nvn-50 |
| `nvnCommandBufferDiscardColor` | 55, 231 | nvn-98 |
| `nvnCommandBufferDiscardDepthStencil` | 55, 231 | nvn-98 |
| `nvnCommandBufferDownsample` |  |  |
| `nvnCommandBufferDownsampleTextureView` |  |  |
| `nvnCommandBufferRestoreZCullData` |  |  |
| `nvnCommandBufferSaveZCullData` |  |  |
| `nvnCommandBufferSetCopyImageStride` | 55, 231 | nvn-97 |
| `nvnCommandBufferSetCopyRowStride` | 55, 231 | nvn-46, nvn-47, nvn-48, nvn-49, nvn-50, nvn-51, nvn-55, nvn-97 |

## Commands: render state

36 functions, 13 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferOverrideSubtileSize` |  |  |
| `nvnCommandBufferSetColorReductionEnable` |  |  |
| `nvnCommandBufferSetColorReductionThresholds` |  |  |
| `nvnCommandBufferSetConservativeRasterDilate` | 55, 231 | nvn-107 |
| `nvnCommandBufferSetInnerTessellationLevels` | 55, 231 | nvn-109 |
| `nvnCommandBufferSetOuterTessellationLevels` | 55, 231 | nvn-109 |
| `nvnCommandBufferSetProvokingVertexMode` | 55, 231 | nvn-101 |
| `nvnCommandBufferSetSubpixelPrecisionBias` | 55, 231 | nvn-106 |
| `nvnCommandBufferSetZCullZF32CompressionEnable` |  |  |
| `nvnCommandBufferSetShadingRatePaletteMode` |  |  |
| `nvnCommandBufferSetAlphaRef` |  |  |
| `nvnCommandBufferSetBlendColor` |  |  |
| `nvnCommandBufferSetConservativeRasterEnable` | 55, 231 | nvn-106, nvn-107 |
| `nvnCommandBufferSetDepthBounds` | 55, 231 | nvn-100 |
| `nvnCommandBufferSetDepthClamp` | 55, 231 | nvn-101 |
| `nvnCommandBufferSetDepthRange` | 55, 231 | nvn-100 |
| `nvnCommandBufferSetDepthRanges` | 55, 231 | nvn-100 |
| `nvnCommandBufferSetLineWidth` | 55, 231 | nvn-108 |
| `nvnCommandBufferSetPatchSize` | 55, 231 | nvn-109 |
| `nvnCommandBufferSetPointSize` | 55, 231 | nvn-102 |
| `nvnCommandBufferSetPolygonOffsetClamp` |  |  |
| `nvnCommandBufferSetPrimitiveRestart` | 55, 231 | nvn-103 |
| `nvnCommandBufferSetRasterizerDiscard` | 55, 231 | nvn-101 |
| `nvnCommandBufferSetRenderTargets` | 55, 231 | nvn-43, nvn-44, nvn-58, nvn-59, nvn-60, nvn-77, nvn-78, nvn-79, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-94, nvn-109, nvn-113, nvn-114, nvn2-7 |
| `nvnCommandBufferSetSampleMask` |  |  |
| `nvnCommandBufferSetScissor` | 55, 231 | nvn-43, nvn-44, nvn-58, nvn-59, nvn-60, nvn-77, nvn-78, nvn-79, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-94, nvn-109, nvn-113, nvn-114, nvn2-7 |
| `nvnCommandBufferSetScissors` | 55, 231 | nvn-99 |
| `nvnCommandBufferSetStencilCullCriteria` |  |  |
| `nvnCommandBufferSetStencilMask` |  |  |
| `nvnCommandBufferSetStencilRef` |  |  |
| `nvnCommandBufferSetStencilValueMask` |  |  |
| `nvnCommandBufferSetTiledCacheAction` | 55, 231 | nvn-105 |
| `nvnCommandBufferSetTiledCacheTileSize` | 55, 231 | nvn-105 |
| `nvnCommandBufferSetViewport` | 55, 231 | nvn-43, nvn-44, nvn-58, nvn-59, nvn-60, nvn-77, nvn-78, nvn-79, nvn-80, nvn-81, nvn-82, nvn-83, nvn-84, nvn-85, nvn-109, nvn-113, nvn-114, nvn2-7 |
| `nvnCommandBufferSetViewportSwizzles` | 55, 231 | nvn-104 |
| `nvnCommandBufferSetViewports` | 55, 231 | nvn-99 |

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
| `nvnProgramBuilderSetMaxCallableRecursionDepth` | 231 | nvn-115 |
| `nvnProgramBuilderGetMaxCallableRecursionDepth` | 231 | nvn-115 |
| `nvnProgramBuilderSetMaxTraceRecursionDepth` | 231 | nvn-115 |
| `nvnProgramBuilderGetMaxTraceRecursionDepth` | 231 | nvn-115 |
| `nvnProgramBuilderSetProgramRayTracingFlags` | 231 | nvn-115 |
| `nvnProgramBuilderGetProgramRayTracingFlags` | 231 | nvn-115 |
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
| `nvnSetExternalDebuggerEnabledNVX` | 55, 231 | nvn-75 |
| `nvnDeviceGenerateDebugDomainId` | 55, 231 | nvn-75, nvn-110, nvn-114 |
| `nvnDeviceInstallDebugCallback` | 55, 231 | nvn-75 |
| `nvnDeviceIsExternalDebuggerAttached` | 55, 231 | nvn-75 |
| `nvnDeviceSetAutomaticCheckpointsByDebugGroup` | 55, 231 | nvn-75 |
| `nvnDeviceSetAutomaticCheckpointsEnable` | 55, 231 | nvn-75 |
| `nvnDeviceSetAutomaticCheckpointsMask` | 55, 231 | nvn-75 |
| `nvnDeviceSetAutomaticCheckpointsSamplingInterval` | 55, 231 | nvn-75 |
| `nvnDeviceSetCheckpointCallStacksEnable` | 55, 231 | nvn-75 |
| `nvnDeviceSetCheckpointMaxCallStacksDepth` | 55, 231 | nvn-75 |
| `nvnDeviceWalkDebugDatabase` | 55, 231 | nvn-75 |
| `nvnMemoryPoolSetDebugLabel` | 55, 231 | nvn-75 |
| `nvnBufferGetDebugID` | 55, 231 | nvn-75 |
| `nvnBufferSetDebugLabel` | 55, 231 | nvn-75 |
| `nvnTextureGetDebugID` | 55, 231 | nvn-75 |
| `nvnTextureSetDebugLabel` | 55, 231 | nvn-75 |
| `nvnSamplerGetDebugID` | 55, 231 | nvn-75 |
| `nvnSamplerSetDebugLabel` | 55, 231 | nvn-75 |
| `nvnProgramBuilderSetDebugDataHashes` |  |  |
| `nvnProgramBuilderGetDebugDataHashes` |  |  |
| `nvnCommandBufferInsertCheckpointDynamic` | 55, 231 | nvn-110 |
| `nvnCommandBufferInsertCheckpointStatic` | 55, 231 | nvn-110 |
| `nvnCommandBufferInsertDebugMarkerGraphics` | 231 | nvn-114 |
| `nvnCommandBufferInsertDebugMarkerCompute` | 231 | nvn-114 |
| `nvnCommandBufferPushDebugGroupGraphics` | 231 | nvn-114 |
| `nvnCommandBufferPushDebugGroupCompute` | 231 | nvn-114 |
| `nvnCommandBufferPopDebugGroupGraphics` | 231 | nvn-114 |
| `nvnCommandBufferPopDebugGroupCompute` | 231 | nvn-114 |
| `nvnCommandBufferInsertDebugMarker` | 55, 231 | nvn-110 |
| `nvnCommandBufferInsertDebugMarkerDynamic` | 55, 231 | nvn-110 |
| `nvnCommandBufferInsertDebugMarkerStatic` | 55, 231 | nvn-110 |
| `nvnCommandBufferPopDebugGroup` | 55, 231 | nvn-110 |
| `nvnCommandBufferPopDebugGroupId` | 55, 231 | nvn-110 |
| `nvnCommandBufferPushDebugGroup` | 55, 231 | nvn-110 |
| `nvnCommandBufferPushDebugGroupDynamic` | 55, 231 | nvn-110 |
| `nvnCommandBufferPushDebugGroupStatic` | 55, 231 | nvn-110 |
| `nvnCommandBufferSetDebugLabel` | 55, 231 | nvn-75 |
| `nvnDeviceSetDebugLabel` | 55, 231 | nvn-75 |
| `nvnProgramSetDebugLabel` | 55, 231 | nvn-75 |
| `nvnQueueSetDebugLabel` | 55, 231 | nvn-75 |
| `nvnSamplerPoolSetDebugLabel` | 55, 231 | nvn-75 |
| `nvnSyncSetDebugLabel` | 55, 231 | nvn-75 |
| `nvnTexturePoolSetDebugLabel` | 55, 231 | nvn-75 |
| `nvnWindowSetDebugLabel` | 55, 231 | nvn-78 |
