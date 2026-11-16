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
names 677; the layer has 288 of them in its API 55 build and 311 in its API 231 build; a case calls 306. The
other 371 have no call at all: a runtime could answer any of them wrongly and every case would still pass.

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
| [Device](#device) | 36 | 11 | 12 | 11 | 25 |
| [Memory pools](#memory-pools) | 18 | 16 | 16 | 16 | 2 |
| [Buffers](#buffers) | 16 | 16 | 16 | 16 | 0 |
| [Textures](#textures) | 95 | 53 | 54 | 53 | 42 |
| [Samplers](#samplers) | 45 | 45 | 45 | 45 | 0 |
| [Programs](#programs) | 20 | 3 | 7 | 7 | 13 |
| [State objects](#state-objects) | 82 | 16 | 16 | 16 | 66 |
| [Queues](#queues) | 30 | 19 | 19 | 19 | 11 |
| [Sync and events](#sync-and-events) | 24 | 18 | 18 | 18 | 6 |
| [Windows and present](#windows-and-present) | 35 | 25 | 35 | 35 | 0 |
| [Counters and queries](#counters-and-queries) | 6 | 6 | 6 | 6 | 0 |
| [Commands: memory and recording](#commands-memory-and-recording) | 25 | 22 | 22 | 22 | 3 |
| [Commands: binds](#commands-binds) | 41 | 14 | 18 | 15 | 26 |
| [Commands: draws and dispatches](#commands-draws-and-dispatches) | 26 | 3 | 6 | 6 | 20 |
| [Commands: clears and copies](#commands-clears-and-copies) | 25 | 18 | 18 | 18 | 7 |
| [Commands: render state](#commands-render-state) | 36 | 3 | 3 | 3 | 33 |
| [DLSS](#dlss) | 58 | 0 | 0 | 0 | 58 |
| [Ray tracing](#ray-tracing) | 15 | 0 | 0 | 0 | 15 |
| [Debug](#debug) | 44 | 0 | 0 | 0 | 44 |
| **All** | **677** | **288** | **311** | **306** | **371** |

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
| nvn-20 | nvn | the program's NVN functions resolve | the NRO, the Switch 2 program |
| nvn-21 | nvn | the device initializes | the NRO, the Switch 2 program |
| nvn-22 | nvn | the device answers the layer's API version | the NRO, the Switch 2 program |
| nvn-23 | nvn | a queue, its pools and a 64x64 target come up | the NRO, the Switch 2 program |
| nvn-24 | nvn | clear to opaque red | the NRO, the Switch 2 program |
| nvn-25 | nvn | clear to a mid channel value | the NRO, the Switch 2 program |
| nvn-26 | nvn | two copy textures and a staging pool come up | the NRO, the Switch 2 program |
| nvn-27 | nvn | a picture copied into a texture and back keeps every texel | the NRO, the Switch 2 program |
| nvn-28 | nvn | a texture copied into another keeps every texel | the NRO, the Switch 2 program |
| nvn-29 | nvn | a buffer copied into another keeps every byte | the NRO, the Switch 2 program |
| nvn-30 | nvn | a texture copied to a buffer with MIRROR_Y lands upside down | the NRO, the Switch 2 program |
| nvn-31 | nvn | a texture copied into another with MIRROR_X lands mirrored | the NRO, the Switch 2 program |
| nvn-32 | nvn | a copy to a buffer with a row stride of 288 keeps the bytes between rows | the NRO, the Switch 2 program |
| nvn-33 | nvn | a texture builder gives back each number it was set | the NRO, the Switch 2 program |
| nvn-34 | nvn | a texture view says which of its parts were set, and with what | the NRO, the Switch 2 program |
| nvn-35 | nvn | a texture of three levels comes up and says what its builder had | the NRO, the Switch 2 program |
| nvn-36 | nvn | a picture copied into each level of a texture through a view comes back from that level | the NRO, the Switch 2 program |
| nvn-37 | nvn | a memory pool and its builder give back what they were set, and the pool maps its storage | the NRO, the Switch 2 program |
| nvn-38 | nvn | a page flushed from a cached pool is what a copy reads, and the copy is there after an invalidate | the NRO, the Switch 2 program |
| nvn-39 | nvn | a sync fenced on the queue behind a clear signals, and the clear is there with no finish | the NRO, the Switch 2 program |
| nvn-40 | nvn | a sync fenced inside the list behind a clear signals, and the clear is there with no finish | the NRO, the Switch 2 program |
| nvn-41 | nvn | a queue and a list told to wait for a sync that signalled run their clear | the NRO, the Switch 2 program |
| nvn-42 | nvn | a wait on a sync nothing fenced fails, with a timeout or without | the NRO, the Switch 2 program |
| nvn-43 | nvn | the queue has no error after work that was valid | the NRO, the Switch 2 program |
| nvn-44 | nvn | a queue builder gives back its flags and says which of its sizes were set | the NRO, the Switch 2 program |
| nvn-45 | nvn | an event keeps what the processor signals: a write, an add and an add that wraps | the NRO, the Switch 2 program |
| nvn-46 | nvn | a sampler builder gives back each number it was set | the NRO, the Switch 2 program |
| nvn-47 | nvn | a sampler says what its builder had, and is the same as another of that state alone | the NRO, the Switch 2 program |
| nvn-48 | nvn | the texture and sampler pools say where they are, and a builder registers as its sampler | the NRO, the Switch 2 program |
| nvn-49 | nvn | a buffer is a range of its pool: its address, its mapping, and what it and its builder say | the NRO, the Switch 2 program |
| nvn-50 | nvn | bytes flushed from one buffer and copied to another are there after an invalidate | the NRO, the Switch 2 program |
| nvn-51 | nvn | a window builder gives back what it was set | the NRO, the Switch 2 program |
| nvn-52 | nvn | a window acquires and presents both its textures | the NRO, the Switch 2 program |
| nvn-53 | nvn | a window says what it is of, and keeps the active textures, the interval and the crop it is set | the NRO, the Switch 2 program |
| nvn-54 | nvn | a texture the window hands with a sync is free once the sync signals, and presents | the NRO, the Switch 2 program |
| nvn-55 | nvn | the same scene through the NVN the program looks up holds what the processor works out | the NRO |
| nvn-56 | nvn | the same texture past its edges through the NVN the program looks up does the same | the NRO |
| nvn-57 | nvn | the same four indexed quads through the NVN the program looks up are each in its quarter | the NRO |
| nvn-58 | nvn | the samples counter reports a quad's 256 pixels and none after a reset, through the NVN the program looks up | the NRO |
| nvn-59 | nvn | two values a list reports are in memory once it has run, through the NVN the program looks up | the NRO |
| nvn-60 | nvn | two timestamp reports hold no count and times between the device's clock before and after, through the NVN the program looks up | the NRO |
| nvn-61 | nvn | a timestamp report's word is the graphics unit's ticks, 384 every 625 nanoseconds, through the NVN the program looks up | the NRO |
| nvn-62 | nvn | the render enable draws under on and under a condition that holds and skips the rest, through the NVN the program looks up | the NRO |
| nvn-63 | nvn | a command buffer's memory used and free come to what it was given, and draws take some, through the NVN the program looks up | the NRO |
| nvn-64 | nvn | a list out of command memory asks the program's callback and draws on, through the NVN the program looks up | the NRO |
| nvn-65 | nvn | a list called from another and a list copied into it draw their cells, through the NVN the program looks up | the NRO |
| nvn-66 | nvn | clears of an unsigned and a signed integer target hold the integers given, through the NVN the program looks up | the NRO |
| nvn-67 | nvn | a clear of a texture that is no target reaches its region and its mask's channels, through the NVN the program looks up | the NRO |
| nvn-68 | nvn | a range of a buffer cleared to a word holds it, and nothing beside the range, through the NVN the program looks up | the NRO |
| nvn-69 | nvn | a command buffer gives back the copy strides it was set, through the NVN the program looks up | the NRO |
| nvn-70 | nvn | targets discarded, then cleared and drawn, hold the clear and the draw, through the NVN the program looks up | the NRO |
| nvn-71 | nvn | an Ounce window builder starts with the driver's colour space and HDR metadata, and gives back what it is set | the Switch 2 program |
| nvn-72 | nvn | an Ounce window has its builder's HDR metadata, and gives back what it is set | the Switch 2 program |
| nvn2-1 | nvn2 | a device, a queue, a command buffer, their pools and a 256x256 target with its depth come up | the Switch 2 program |
| nvn2-2 | nvn2 | a compute program assembled by hand makes a program through the builder | the Switch 2 program |
| nvn2-3 | nvn2 | a dispatch's writes to a storage buffer, as the processor reads them once the queue finishes | the Switch 2 program |
| nvn2-4 | nvn2 | a dispatch's writes to a storage buffer, as the graphics unit copies them to another buffer | the Switch 2 program |
| nvn2-5 | nvn2 | the Mandelbrot's compute, mesh and fragment programs make two programs | the Switch 2 program |
| nvn2-6 | nvn2 | a compute program counts the Mandelbrot set's steps for every cell of a grid | the Switch 2 program |
| nvn2-7 | nvn2 | a mesh program draws the counts as bars, a fragment program colours them and the depth test keeps the nearest | the Switch 2 program |
| nvn2-8 | nvn2 | one warp's matrix instruction multiplies a 16 by 16 matrix of halves by a 16 by 8 one into floats | the Switch 2 program |

## Device

36 functions, 25 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnBootstrapLoader` | 55, 231 | nvn-20, nvn-55, nvn-56, nvn-57, nvn2-1 |
| `nvnDeviceGetProcAddress` | 55, 231 | nvn-20, nvn-55, nvn-56, nvn-57, nvn2-1 |
| `nvnDeviceApplyDeferredFinalizes` |  |  |
| `nvnDeviceBuilderGetFlags` |  |  |
| `nvnDeviceFinalize` | 55, 231 | nvn-55, nvn-56, nvn-57 |
| `nvnDeviceRegisterFastClearColori` |  |  |
| `nvnDeviceRegisterFastClearColorui` |  |  |
| `nvnDeviceSetIntermediateShaderCache` |  |  |
| `nvnDeviceBuilderSetDefaults` | 55, 231 | nvn-21, nvn-55, nvn-56, nvn-57, nvn2-1 |
| `nvnDeviceBuilderSetFlags` | 55, 231 | nvn-21, nvn-55, nvn-56, nvn-57, nvn2-1 |
| `nvnDeviceGetImageHandle` | 231 |  |
| `nvnDeviceFinalizeCommandHandle` |  |  |
| `nvnDeviceGetCurrentTimestampInNanoseconds` | 55, 231 | nvn-9, nvn-10, nvn-60, nvn-61 |
| `nvnDeviceGetDepthMode` |  |  |
| `nvnDeviceGetInteger` | 55, 231 | nvn-22, nvn-55, nvn-56, nvn-57, nvn2-1 |
| `nvnDeviceGetWindowOriginMode` |  |  |
| `nvnDeviceGetSeparateSamplerHandle` |  |  |
| `nvnDeviceGetSeparateTextureHandle` |  |  |
| `nvnDeviceGetTexelFetchHandle` |  |  |
| `nvnDeviceGetTextureHandle` | 55, 231 | nvn-55, nvn-56, nvn-57 |
| `nvnDeviceInitialize` | 55, 231 | nvn-21, nvn-55, nvn-56, nvn-57, nvn2-1 |
| `nvnDeviceRegisterFastClearColor` |  |  |
| `nvnDeviceRegisterFastClearDepth` |  |  |
| `nvnDeviceRegisterFastClearStencil` |  |  |
| `nvnDeviceSetDepthMode` | 55, 231 | nvn-55, nvn-56, nvn-57 |
| `nvnDeviceSetWindowOriginMode` | 55, 231 | nvn-55, nvn-56, nvn-57 |
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
| `nvnMemoryPoolBuilderGetDevice` | 55, 231 | nvn-37 |
| `nvnMemoryPoolBuilderGetFlags` | 55, 231 | nvn-37 |
| `nvnMemoryPoolBuilderGetMemory` | 55, 231 | nvn-37 |
| `nvnMemoryPoolBuilderGetSize` | 55, 231 | nvn-37 |
| `nvnMemoryPoolBuilderSetDefaults` | 55, 231 | nvn-15, nvn-16, nvn-23, nvn-26, nvn-35, nvn-37, nvn-52, nvn-55, nvn-56, nvn-57, nvn-66, nvn-67, nvn2-1 |
| `nvnMemoryPoolBuilderSetDevice` | 55, 231 | nvn-15, nvn-16, nvn-23, nvn-26, nvn-35, nvn-37, nvn-52, nvn-55, nvn-56, nvn-57, nvn-66, nvn-67, nvn2-1 |
| `nvnMemoryPoolBuilderSetFlags` | 55, 231 | nvn-15, nvn-16, nvn-23, nvn-26, nvn-35, nvn-37, nvn-52, nvn-55, nvn-56, nvn-57, nvn-66, nvn-67, nvn2-1 |
| `nvnMemoryPoolBuilderSetStorage` | 55, 231 | nvn-15, nvn-16, nvn-23, nvn-26, nvn-35, nvn-37, nvn-52, nvn-55, nvn-56, nvn-57, nvn-66, nvn-67, nvn2-1 |
| `nvnMemoryPoolBuilderSetNativeHandleNVX` |  |  |
| `nvnMemoryPoolFinalize` | 55, 231 | nvn-15, nvn-16, nvn-52, nvn-55, nvn-56, nvn-57, nvn-66, nvn-67 |
| `nvnMemoryPoolFlushMappedRange` | 55, 231 | nvn-38, nvn-50 |
| `nvnMemoryPoolGetBufferAddress` | 55, 231 | nvn-24, nvn-25, nvn-27, nvn-28, nvn-29, nvn-30, nvn-31, nvn-32, nvn-36, nvn-38, nvn-39, nvn-40, nvn-41, nvn-49, nvn-55, nvn-56, nvn-57, nvn2-2, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-8 |
| `nvnMemoryPoolGetFlags` | 55, 231 | nvn-37 |
| `nvnMemoryPoolGetSize` | 55, 231 | nvn-37 |
| `nvnMemoryPoolInitialize` | 55, 231 | nvn-15, nvn-16, nvn-23, nvn-26, nvn-35, nvn-37, nvn-52, nvn-55, nvn-56, nvn-57, nvn-66, nvn-67, nvn2-1 |
| `nvnMemoryPoolInvalidateMappedRange` | 55, 231 | nvn-38 |
| `nvnMemoryPoolMap` | 55, 231 | nvn-24, nvn-25, nvn-26, nvn-27, nvn-28, nvn-29, nvn-30, nvn-31, nvn-32, nvn-36, nvn-37, nvn-38, nvn-39, nvn-40, nvn-41, nvn-45, nvn-48, nvn-49, nvn-50, nvn-55, nvn-56, nvn-57, nvn2-2, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-8 |
| `nvnMemoryPoolMapVirtual` |  |  |

## Buffers

16 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnBufferBuilderGetDevice` | 55, 231 | nvn-49 |
| `nvnBufferBuilderGetMemoryOffset` | 55, 231 | nvn-49 |
| `nvnBufferBuilderGetMemoryPool` | 55, 231 | nvn-49 |
| `nvnBufferBuilderGetSize` | 55, 231 | nvn-49 |
| `nvnBufferBuilderSetDefaults` | 55, 231 | nvn-49, nvn-50 |
| `nvnBufferBuilderSetDevice` | 55, 231 | nvn-49, nvn-50 |
| `nvnBufferBuilderSetStorage` | 55, 231 | nvn-49, nvn-50 |
| `nvnBufferFinalize` | 55, 231 | nvn-49, nvn-50 |
| `nvnBufferFlushMappedRange` | 55, 231 | nvn-50 |
| `nvnBufferInvalidateMappedRange` | 55, 231 | nvn-50 |
| `nvnBufferGetAddress` | 55, 231 | nvn-49, nvn-50 |
| `nvnBufferGetMemoryOffset` | 55, 231 | nvn-49 |
| `nvnBufferGetMemoryPool` | 55, 231 | nvn-49 |
| `nvnBufferGetSize` | 55, 231 | nvn-49 |
| `nvnBufferInitialize` | 55, 231 | nvn-49, nvn-50 |
| `nvnBufferMap` | 55, 231 | nvn-49, nvn-50 |

## Textures

95 functions, 42 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnTextureBuilderGetGLTextureName` |  |  |
| `nvnTextureBuilderGetMemoryOffset` | 55, 231 | nvn-33 |
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
| `nvnTextureViewCompare` | 55, 231 | nvn-34 |
| `nvnTextureBuilderGetDepthStencilMode` |  |  |
| `nvnTextureBuilderGetDevice` | 55, 231 | nvn-33 |
| `nvnTextureBuilderGetMemoryPool` | 55, 231 | nvn-33 |
| `nvnTextureBuilderGetPackagedTextureData` |  |  |
| `nvnTextureBuilderGetSwizzle` |  |  |
| `nvnTextureBuilderGetDepth` | 55, 231 | nvn-33 |
| `nvnTextureBuilderGetFlags` | 55, 231 | nvn-33 |
| `nvnTextureBuilderGetFormat` | 55, 231 | nvn-33 |
| `nvnTextureBuilderGetHeight` | 55, 231 | nvn-33 |
| `nvnTextureBuilderGetLevels` | 55, 231 | nvn-33 |
| `nvnTextureBuilderGetSamples` | 55, 231 | nvn-33 |
| `nvnTextureBuilderGetStorageAlignment` | 55, 231 | nvn-15, nvn-16, nvn-23, nvn-35, nvn-52, nvn-55, nvn-56, nvn-57, nvn-66, nvn-67, nvn2-1 |
| `nvnTextureBuilderGetStorageClass` |  |  |
| `nvnTextureBuilderGetStorageSize` | 55, 231 | nvn-15, nvn-16, nvn-23, nvn-26, nvn-35, nvn-52, nvn-55, nvn-56, nvn-57, nvn-66, nvn-67, nvn2-1 |
| `nvnTextureBuilderGetStride` |  |  |
| `nvnTextureBuilderGetTarget` | 55, 231 | nvn-33 |
| `nvnTextureBuilderGetWidth` | 55, 231 | nvn-33 |
| `nvnTextureBuilderGetZCullStorageSize` |  |  |
| `nvnTextureBuilderSetDefaults` | 55, 231 | nvn-15, nvn-16, nvn-23, nvn-26, nvn-33, nvn-35, nvn-52, nvn-55, nvn-56, nvn-57, nvn-66, nvn-67, nvn2-1 |
| `nvnTextureBuilderSetDepth` | 55, 231 | nvn-33, nvn-35 |
| `nvnTextureBuilderSetDepthStencilMode` |  |  |
| `nvnTextureBuilderSetDevice` | 55, 231 | nvn-15, nvn-16, nvn-23, nvn-26, nvn-33, nvn-35, nvn-52, nvn-55, nvn-56, nvn-57, nvn-66, nvn-67, nvn2-1 |
| `nvnTextureBuilderSetFlags` | 55, 231 | nvn-23, nvn-26, nvn-33, nvn-35, nvn-52, nvn-55, nvn-56, nvn-57, nvn2-1 |
| `nvnTextureBuilderSetFormat` | 55, 231 | nvn-15, nvn-16, nvn-23, nvn-26, nvn-33, nvn-35, nvn-52, nvn-55, nvn-56, nvn-57, nvn-66, nvn-67, nvn2-1 |
| `nvnTextureBuilderSetHeight` | 55, 231 | nvn-33 |
| `nvnTextureBuilderSetLevels` | 55, 231 | nvn-33, nvn-35 |
| `nvnTextureBuilderSetPackagedTextureData` |  |  |
| `nvnTextureBuilderSetPackagedTextureLayout` |  |  |
| `nvnTextureBuilderSetSamples` | 55, 231 | nvn-33, nvn-35 |
| `nvnTextureBuilderSetSize1D` |  |  |
| `nvnTextureBuilderSetSize2D` | 55, 231 | nvn-15, nvn-16, nvn-23, nvn-26, nvn-33, nvn-35, nvn-52, nvn-55, nvn-56, nvn-57, nvn-66, nvn-67, nvn2-1 |
| `nvnTextureBuilderSetSize3D` |  |  |
| `nvnTextureBuilderSetStorage` | 55, 231 | nvn-15, nvn-16, nvn-23, nvn-26, nvn-33, nvn-35, nvn-52, nvn-55, nvn-56, nvn-57, nvn-66, nvn-67, nvn2-1 |
| `nvnTextureBuilderSetStride` |  |  |
| `nvnTextureBuilderSetSwizzle` |  |  |
| `nvnTextureBuilderSetTarget` | 55, 231 | nvn-15, nvn-16, nvn-23, nvn-26, nvn-33, nvn-35, nvn-52, nvn-55, nvn-56, nvn-57, nvn-66, nvn-67, nvn2-1 |
| `nvnTextureBuilderSetWidth` | 55, 231 | nvn-33 |
| `nvnTextureCompare` |  |  |
| `nvnTextureFinalize` | 55, 231 | nvn-15, nvn-16, nvn-52, nvn-55, nvn-56, nvn-57, nvn-66, nvn-67 |
| `nvnTextureFlushTexels` |  |  |
| `nvnTextureGetDepth` | 55, 231 | nvn-35 |
| `nvnTextureGetDepthStencilMode` |  |  |
| `nvnTextureGetFlags` | 55, 231 | nvn-35 |
| `nvnTextureGetFormat` | 55, 231 | nvn-35 |
| `nvnTextureGetHeight` | 55, 231 | nvn-35 |
| `nvnTextureGetLevels` | 55, 231 | nvn-35 |
| `nvnTextureGetMemoryOffset` | 55, 231 | nvn-35 |
| `nvnTextureGetMemoryPool` | 55, 231 | nvn-35 |
| `nvnTextureGetSamples` | 55, 231 | nvn-35 |
| `nvnTextureGetStorageClass` |  |  |
| `nvnTextureGetStorageSize` |  |  |
| `nvnTextureGetStride` |  |  |
| `nvnTextureGetSwizzle` |  |  |
| `nvnTextureGetTarget` | 55, 231 | nvn-35 |
| `nvnTextureGetTextureAddress` |  |  |
| `nvnTextureGetViewOffset` |  |  |
| `nvnTextureGetWidth` | 55, 231 | nvn-35 |
| `nvnTextureGetZCullStorageSize` |  |  |
| `nvnTextureInitialize` | 55, 231 | nvn-15, nvn-16, nvn-23, nvn-26, nvn-35, nvn-52, nvn-55, nvn-56, nvn-57, nvn-66, nvn-67, nvn2-1 |
| `nvnTexturePoolFinalize` | 55, 231 | nvn-48, nvn-55, nvn-56, nvn-57 |
| `nvnTexturePoolGetMemoryOffset` | 55, 231 | nvn-48 |
| `nvnTexturePoolGetMemoryPool` | 55, 231 | nvn-48 |
| `nvnTexturePoolGetSize` | 55, 231 | nvn-48 |
| `nvnTexturePoolInitialize` | 55, 231 | nvn-48, nvn-55, nvn-56, nvn-57 |
| `nvnTexturePoolRegisterImage` | 231 |  |
| `nvnTexturePoolRegisterTexture` | 55, 231 | nvn-55, nvn-56, nvn-57 |
| `nvnTextureReadTexels` |  |  |
| `nvnTextureViewGetDepthStencilMode` |  |  |
| `nvnTextureViewGetFormat` | 55, 231 | nvn-34 |
| `nvnTextureViewGetLayers` | 55, 231 | nvn-34 |
| `nvnTextureViewGetLevels` | 55, 231 | nvn-34 |
| `nvnTextureViewGetSwizzle` |  |  |
| `nvnTextureViewGetTarget` | 55, 231 | nvn-34 |
| `nvnTextureViewSetDefaults` | 55, 231 | nvn-34, nvn-36 |
| `nvnTextureViewSetDepthStencilMode` |  |  |
| `nvnTextureViewSetFormat` | 55, 231 | nvn-34 |
| `nvnTextureViewSetLayers` | 55, 231 | nvn-34 |
| `nvnTextureViewSetLevels` | 55, 231 | nvn-34, nvn-36 |
| `nvnTextureViewSetSwizzle` |  |  |
| `nvnTextureViewSetTarget` | 55, 231 | nvn-34 |
| `nvnTextureWriteTexels` |  |  |
| `nvnTextureWriteTexelsStrided` |  |  |

## Samplers

45 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnSamplerBuilderGetBorderColor` | 55, 231 | nvn-46, nvn-47 |
| `nvnSamplerBuilderGetBorderColori` | 55, 231 | nvn-46 |
| `nvnSamplerBuilderGetBorderColorui` | 55, 231 | nvn-46 |
| `nvnSamplerBuilderGetCompare` | 55, 231 | nvn-46, nvn-47 |
| `nvnSamplerBuilderGetDevice` | 55, 231 | nvn-46 |
| `nvnSamplerBuilderGetLodBias` | 55, 231 | nvn-46, nvn-47 |
| `nvnSamplerBuilderGetLodClamp` | 55, 231 | nvn-46, nvn-47 |
| `nvnSamplerBuilderGetMaxAnisotropy` | 55, 231 | nvn-46, nvn-47 |
| `nvnSamplerBuilderGetMinMagFilter` | 55, 231 | nvn-46, nvn-47 |
| `nvnSamplerBuilderGetReductionFilter` | 55, 231 | nvn-46, nvn-47 |
| `nvnSamplerBuilderGetWrapMode` | 55, 231 | nvn-46, nvn-47 |
| `nvnSamplerBuilderSetBorderColori` | 55, 231 | nvn-46 |
| `nvnSamplerBuilderSetBorderColorui` | 55, 231 | nvn-46 |
| `nvnSamplerGetBorderColori` | 55, 231 | nvn-47 |
| `nvnSamplerGetBorderColorui` | 55, 231 | nvn-47 |
| `nvnSamplerPoolRegisterSamplerBuilder` | 55, 231 | nvn-48 |
| `nvnSamplerBuilderSetBorderColor` | 55, 231 | nvn-46, nvn-47, nvn-48, nvn-55, nvn-56, nvn-57 |
| `nvnSamplerBuilderSetCompare` | 55, 231 | nvn-46, nvn-47, nvn-48 |
| `nvnSamplerBuilderSetDefaults` | 55, 231 | nvn-46, nvn-47, nvn-48, nvn-55, nvn-56, nvn-57 |
| `nvnSamplerBuilderSetDevice` | 55, 231 | nvn-46, nvn-47, nvn-48, nvn-55, nvn-56, nvn-57 |
| `nvnSamplerBuilderSetLodBias` | 55, 231 | nvn-46, nvn-47, nvn-48 |
| `nvnSamplerBuilderSetLodClamp` | 55, 231 | nvn-46, nvn-47, nvn-48 |
| `nvnSamplerBuilderGetLodSnap` | 55, 231 | nvn-46 |
| `nvnSamplerBuilderSetLodSnap` | 55, 231 | nvn-46, nvn-47, nvn-48 |
| `nvnSamplerBuilderSetMaxAnisotropy` | 55, 231 | nvn-46, nvn-47, nvn-48 |
| `nvnSamplerBuilderSetMinMagFilter` | 55, 231 | nvn-46, nvn-47, nvn-48, nvn-55, nvn-56, nvn-57 |
| `nvnSamplerBuilderSetReductionFilter` | 55, 231 | nvn-46, nvn-47, nvn-48 |
| `nvnSamplerBuilderSetWrapMode` | 55, 231 | nvn-46, nvn-47, nvn-48, nvn-55, nvn-56, nvn-57 |
| `nvnSamplerCompare` | 55, 231 | nvn-47 |
| `nvnSamplerFinalize` | 55, 231 | nvn-47, nvn-48, nvn-55, nvn-56, nvn-57 |
| `nvnSamplerGetBorderColor` | 55, 231 | nvn-47 |
| `nvnSamplerGetCompare` | 55, 231 | nvn-47 |
| `nvnSamplerGetLodBias` | 55, 231 | nvn-47 |
| `nvnSamplerGetLodClamp` | 55, 231 | nvn-47 |
| `nvnSamplerGetMaxAnisotropy` | 55, 231 | nvn-47 |
| `nvnSamplerGetMinMagFilter` | 55, 231 | nvn-47 |
| `nvnSamplerGetReductionFilter` | 55, 231 | nvn-47 |
| `nvnSamplerGetWrapMode` | 55, 231 | nvn-47 |
| `nvnSamplerInitialize` | 55, 231 | nvn-47, nvn-48, nvn-55, nvn-56, nvn-57 |
| `nvnSamplerPoolFinalize` | 55, 231 | nvn-48, nvn-55, nvn-56, nvn-57 |
| `nvnSamplerPoolGetMemoryOffset` | 55, 231 | nvn-48 |
| `nvnSamplerPoolGetMemoryPool` | 55, 231 | nvn-48 |
| `nvnSamplerPoolGetSize` | 55, 231 | nvn-48 |
| `nvnSamplerPoolInitialize` | 55, 231 | nvn-48, nvn-55, nvn-56, nvn-57 |
| `nvnSamplerPoolRegisterSampler` | 55, 231 | nvn-48, nvn-55, nvn-56, nvn-57 |

## Programs

20 functions, 13 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnProgramGetSubtileSize` |  |  |
| `nvnProgramGetScratchBytesPerWarp` |  |  |
| `nvnProgramSetSampleShading` |  |  |
| `nvnProgramSetSubroutineLinkage` |  |  |
| `nvnProgramFinalize` | 55, 231 | nvn-55, nvn-56, nvn-57 |
| `nvnProgramInitialize` | 55, 231 | nvn-55, nvn-56, nvn-57 |
| `nvnProgramSetShaders` | 55, 231 | nvn-55, nvn-56, nvn-57 |
| `nvnProgramSetShadersExt` |  |  |
| `nvnProgramBuilderSetDefaults` | 231 | nvn2-2, nvn2-5, nvn2-8 |
| `nvnProgramBuilderSetDevice` | 231 | nvn2-2, nvn2-5, nvn2-8 |
| `nvnProgramBuilderGetDevice` |  |  |
| `nvnProgramBuilderSetSampleShading` |  |  |
| `nvnProgramBuilderGetSampleShading` |  |  |
| `nvnProgramBuilderSetShaders` | 231 | nvn2-2, nvn2-5, nvn2-8 |
| `nvnProgramBuilderGetShaders` |  |  |
| `nvnProgramBuilderGetShaderCount` |  |  |
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
| `nvnBlendStateSetDefaults` | 55, 231 | nvn-55, nvn-56, nvn-57, nvn2-7 |
| `nvnChannelMaskStateSetChannelMask` |  |  |
| `nvnChannelMaskStateSetDefaults` | 55, 231 | nvn-55, nvn-56, nvn-57, nvn2-7 |
| `nvnColorStateGetBlendEnable` |  |  |
| `nvnColorStateSetAlphaTest` |  |  |
| `nvnColorStateSetBlendEnable` |  |  |
| `nvnColorStateSetDefaults` | 55, 231 | nvn-55, nvn-56, nvn-57, nvn2-7 |
| `nvnColorStateSetLogicOp` |  |  |
| `nvnDepthStencilStateSetDefaults` | 55, 231 | nvn-55, nvn-56, nvn-57, nvn2-7 |
| `nvnDepthStencilStateSetDepthFunc` | 55, 231 | nvn-55, nvn-56, nvn-57, nvn2-7 |
| `nvnDepthStencilStateSetDepthTestEnable` | 55, 231 | nvn-55, nvn-56, nvn-57, nvn2-7 |
| `nvnDepthStencilStateSetDepthWriteEnable` | 55, 231 | nvn-55, nvn-56, nvn-57, nvn2-7 |
| `nvnDepthStencilStateSetStencilFunc` |  |  |
| `nvnDepthStencilStateSetStencilOp` |  |  |
| `nvnDepthStencilStateSetStencilTestEnable` |  |  |
| `nvnMultisampleStateSetAlphaToCoverageEnable` |  |  |
| `nvnMultisampleStateSetDefaults` | 55, 231 | nvn-55, nvn-56, nvn-57, nvn2-7 |
| `nvnMultisampleStateSetMultisampleEnable` |  |  |
| `nvnMultisampleStateSetSamples` |  |  |
| `nvnPolygonStateSetCullFace` | 55, 231 | nvn-55, nvn-56, nvn-57, nvn2-7 |
| `nvnPolygonStateSetDefaults` | 55, 231 | nvn-55, nvn-56, nvn-57, nvn2-7 |
| `nvnPolygonStateSetFrontFace` | 55, 231 | nvn-55, nvn-56, nvn-57 |
| `nvnPolygonStateSetPolygonMode` |  |  |
| `nvnPolygonStateSetPolygonOffsetEnables` |  |  |
| `nvnVertexAttribStateSetDefaults` | 55, 231 | nvn-55, nvn-56, nvn-57 |
| `nvnVertexAttribStateSetFormat` | 55, 231 | nvn-55, nvn-56, nvn-57 |
| `nvnVertexAttribStateSetStreamIndex` | 55, 231 | nvn-55, nvn-56, nvn-57 |
| `nvnVertexStreamStateSetDefaults` | 55, 231 | nvn-55, nvn-56, nvn-57 |
| `nvnVertexStreamStateSetDivisor` |  |  |
| `nvnVertexStreamStateSetStride` | 55, 231 | nvn-55, nvn-56, nvn-57 |
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
| `nvnQueueBuilderGetCommandFlushThreshold` | 55, 231 | nvn-44 |
| `nvnQueueBuilderGetCommandMemorySize` | 55, 231 | nvn-44 |
| `nvnQueueBuilderGetComputeMemorySize` | 55, 231 | nvn-44 |
| `nvnQueueBuilderGetControlMemorySize` | 55, 231 | nvn-44 |
| `nvnQueueBuilderGetDevice` | 55, 231 | nvn-44 |
| `nvnQueueBuilderGetFlags` | 55, 231 | nvn-44 |
| `nvnQueueBuilderGetMemory` |  |  |
| `nvnQueueBuilderGetMemorySize` |  |  |
| `nvnQueueGetTotalCommandMemoryUsed` |  |  |
| `nvnQueueGetTotalComputeMemoryUsed` |  |  |
| `nvnQueueGetTotalControlMemoryUsed` |  |  |
| `nvnQueueResetMemoryUsageCounts` |  |  |
| `nvnQueueBuilderGetQueueMemorySize` |  |  |
| `nvnQueueBuilderSetCommandFlushThreshold` | 55, 231 | nvn-44 |
| `nvnQueueBuilderSetCommandMemorySize` | 55, 231 | nvn-44 |
| `nvnQueueBuilderSetComputeMemorySize` | 55, 231 | nvn-44 |
| `nvnQueueBuilderSetControlMemorySize` | 55, 231 | nvn-44 |
| `nvnQueueBuilderSetDefaults` | 55, 231 | nvn-23, nvn-44, nvn-55, nvn-56, nvn-57, nvn2-1 |
| `nvnQueueBuilderSetDevice` | 55, 231 | nvn-23, nvn-44, nvn-55, nvn-56, nvn-57, nvn2-1 |
| `nvnQueueBuilderSetFlags` | 55, 231 | nvn-44 |
| `nvnQueueBuilderGetQueuePriority` |  |  |
| `nvnQueueBuilderSetQueuePriority` |  |  |
| `nvnQueueBuilderSetQueueMemory` |  |  |
| `nvnQueueFinalize` | 55, 231 | nvn-55, nvn-56, nvn-57 |
| `nvnQueueFinish` | 55, 231 | nvn-24, nvn-25, nvn-27, nvn-28, nvn-29, nvn-30, nvn-31, nvn-32, nvn-36, nvn-38, nvn-39, nvn-40, nvn-41, nvn-43, nvn-50, nvn-52, nvn-53, nvn-54, nvn-55, nvn-56, nvn-57, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnQueueFlush` | 55, 231 | nvn-39, nvn-40, nvn-41 |
| `nvnQueueGetError` | 55, 231 | nvn-43 |
| `nvnQueueInitialize` | 55, 231 | nvn-23, nvn-55, nvn-56, nvn-57, nvn2-1 |
| `nvnQueueSubmitCommands` | 55, 231 | nvn-24, nvn-25, nvn-27, nvn-28, nvn-29, nvn-30, nvn-31, nvn-32, nvn-36, nvn-38, nvn-39, nvn-40, nvn-41, nvn-50, nvn-52, nvn-53, nvn-54, nvn-55, nvn-56, nvn-57, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnQueueBuilderSetPrimaryQueue` |  |  |

## Sync and events

24 functions, 6 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnSyncCreateGLSync` |  |  |
| `nvnSyncInitializeFromFencedGLSync` |  |  |
| `nvnSyncFinalize` | 55, 231 | nvn-39, nvn-40, nvn-41, nvn-42, nvn-54 |
| `nvnSyncInitialize` | 55, 231 | nvn-39, nvn-40, nvn-41, nvn-42, nvn-54 |
| `nvnSyncWait` | 55, 231 | nvn-39, nvn-40, nvn-41, nvn-42, nvn-54 |
| `nvnSyncGetNvRmSyncNVX` |  |  |
| `nvnSyncInitializeFromNvRmSyncNVX` |  |  |
| `nvnEventBuilderGetMemoryOffset` | 55, 231 | nvn-45 |
| `nvnEventBuilderGetMemoryPool` | 55, 231 | nvn-45 |
| `nvnEventBuilderGetStorage` | 55, 231 | nvn-45 |
| `nvnEventGetMemoryOffset` | 55, 231 | nvn-45 |
| `nvnEventGetMemoryPool` | 55, 231 | nvn-45 |
| `nvnEventBuilderSetDefaults` | 55, 231 | nvn-45 |
| `nvnEventBuilderSetStorage` | 55, 231 | nvn-45 |
| `nvnEventFinalize` | 55, 231 | nvn-45 |
| `nvnEventGetValue` | 55, 231 | nvn-45 |
| `nvnEventInitialize` | 55, 231 | nvn-45 |
| `nvnEventSignal` | 55, 231 | nvn-45 |
| `nvnQueueFenceSync` | 55, 231 | nvn-39, nvn-41 |
| `nvnQueueWaitSync` | 55, 231 | nvn-39, nvn-40, nvn-41 |
| `nvnCommandBufferFenceSync` | 55, 231 | nvn-40 |
| `nvnCommandBufferSignalEvent` |  |  |
| `nvnCommandBufferWaitEvent` |  |  |
| `nvnCommandBufferWaitSync` | 55, 231 | nvn-41 |

## Windows and present

35 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnQueueAcquireTexture` | 55, 231 | nvn-52, nvn-53 |
| `nvnQueuePresentTexture` | 55, 231 | nvn-52, nvn-53, nvn-54 |
| `nvnWindowBuilderGetDevice` | 55, 231 | nvn-51 |
| `nvnWindowBuilderGetNativeWindow` | 55, 231 | nvn-51 |
| `nvnWindowBuilderGetNumActiveTextures` | 55, 231 | nvn-51 |
| `nvnWindowGetNativeWindow` | 55, 231 | nvn-53 |
| `nvnWindowGetNumActiveTextures` | 55, 231 | nvn-53 |
| `nvnWindowGetNumTextures` | 55, 231 | nvn-53 |
| `nvnWindowSetNumActiveTextures` | 55, 231 | nvn-53 |
| `nvnWindowBuilderGetNumTextures` | 55, 231 | nvn-51 |
| `nvnWindowBuilderGetPresentInterval` | 55, 231 | nvn-51 |
| `nvnWindowBuilderGetTexture` | 55, 231 | nvn-51 |
| `nvnWindowGetCrop` | 55, 231 | nvn-53 |
| `nvnWindowGetPresentInterval` | 55, 231 | nvn-53 |
| `nvnWindowAcquireTexture` | 55, 231 | nvn-54 |
| `nvnWindowBuilderSetDefaults` | 55, 231 | nvn-51, nvn-52, nvn-71 |
| `nvnWindowBuilderSetDevice` | 55, 231 | nvn-51, nvn-52 |
| `nvnWindowBuilderSetNativeWindow` | 55, 231 | nvn-51, nvn-52 |
| `nvnWindowBuilderSetNumActiveTextures` | 55, 231 | nvn-51 |
| `nvnWindowBuilderSetPresentInterval` | 55, 231 | nvn-51 |
| `nvnWindowBuilderSetTextures` | 55, 231 | nvn-51, nvn-52 |
| `nvnWindowBuilderSetColorSpace` | 231 | nvn-71 |
| `nvnWindowBuilderGetColorSpace` | 231 | nvn-71 |
| `nvnWindowBuilderSetHdrDisplayMetadata` | 231 | nvn-71 |
| `nvnWindowBuilderGetHdrDisplayMetadata` | 231 | nvn-71 |
| `nvnWindowBuilderSetHdrContentMetadata` | 231 | nvn-71 |
| `nvnWindowBuilderGetHdrContentMetadata` | 231 | nvn-71 |
| `nvnWindowSetHdrDisplayMetadata` | 231 | nvn-72 |
| `nvnWindowGetHdrDisplayMetadata` | 231 | nvn-72 |
| `nvnWindowSetHdrContentMetadata` | 231 | nvn-72 |
| `nvnWindowGetHdrContentMetadata` | 231 | nvn-72 |
| `nvnWindowFinalize` | 55, 231 | nvn-52 |
| `nvnWindowInitialize` | 55, 231 | nvn-52 |
| `nvnWindowSetCrop` | 55, 231 | nvn-53 |
| `nvnWindowSetPresentInterval` | 55, 231 | nvn-53 |

## Counters and queries

6 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnDeviceGetTimestampInNanoseconds` | 55, 231 | nvn-9, nvn-10, nvn-60, nvn-61 |
| `nvnCommandBufferReportCounter` | 55, 231 | nvn-58, nvn-60, nvn-61, nvn-62 |
| `nvnCommandBufferReportValue` | 55, 231 | nvn-59 |
| `nvnCommandBufferResetCounter` | 55, 231 | nvn-58, nvn-62 |
| `nvnCommandBufferSetRenderEnable` | 55, 231 | nvn-62 |
| `nvnCommandBufferSetRenderEnableConditional` | 55, 231 | nvn-62 |

## Commands: memory and recording

25 functions, 3 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferGetCommandMemoryFree` | 55, 231 | nvn-63 |
| `nvnCommandBufferGetCommandMemorySize` | 55, 231 | nvn-63 |
| `nvnCommandBufferGetControlMemoryFree` | 55, 231 | nvn-63 |
| `nvnCommandBufferGetControlMemorySize` | 55, 231 | nvn-63 |
| `nvnCommandBufferSetControlMemoryCallbackEnabled` | 55, 231 | nvn-64 |
| `nvnCommandBufferAddCommandMemory` | 55, 231 | nvn-23, nvn-55, nvn-56, nvn-57, nvn-63, nvn-64, nvn2-1 |
| `nvnCommandBufferAddControlMemory` | 55, 231 | nvn-23, nvn-55, nvn-56, nvn-57, nvn-63, nvn2-1 |
| `nvnCommandBufferBarrier` | 55, 231 | nvn-24, nvn-25, nvn-27, nvn-28, nvn-30, nvn-31, nvn-32, nvn-36, nvn-39, nvn-40, nvn-41, nvn-55, nvn-56, nvn-57, nvn-66, nvn-67, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferBeginRecording` | 55, 231 | nvn-24, nvn-25, nvn-27, nvn-28, nvn-29, nvn-30, nvn-31, nvn-32, nvn-36, nvn-38, nvn-39, nvn-40, nvn-41, nvn-50, nvn-52, nvn-53, nvn-54, nvn-55, nvn-56, nvn-57, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferSetRecordingMode` |  |  |
| `nvnCommandBufferSetComputeOverlapMode` |  |  |
| `nvnCommandBufferCallCommands` | 55, 231 | nvn-65 |
| `nvnCommandBufferCopyCommands` | 55, 231 | nvn-65 |
| `nvnCommandBufferEndRecording` | 55, 231 | nvn-24, nvn-25, nvn-27, nvn-28, nvn-29, nvn-30, nvn-31, nvn-32, nvn-36, nvn-38, nvn-39, nvn-40, nvn-41, nvn-50, nvn-52, nvn-53, nvn-54, nvn-55, nvn-56, nvn-57, nvn-65, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferFinalize` | 55, 231 | nvn-55, nvn-56, nvn-57 |
| `nvnCommandBufferGetCommandMemoryUsed` | 55, 231 | nvn-63 |
| `nvnCommandBufferGetControlMemoryUsed` | 55, 231 | nvn-63 |
| `nvnCommandBufferGetMemoryCallback` | 55, 231 | nvn-64 |
| `nvnCommandBufferGetMemoryCallbackData` | 55, 231 | nvn-64 |
| `nvnCommandBufferInitialize` | 55, 231 | nvn-23, nvn-55, nvn-56, nvn-57, nvn2-1 |
| `nvnCommandBufferIsRecording` | 55, 231 | nvn-63 |
| `nvnCommandBufferSetCommandMemoryCallbackEnabled` | 55, 231 | nvn-64 |
| `nvnCommandBufferSetComputeWorkloadPriority` |  |  |
| `nvnCommandBufferSetMemoryCallback` | 55, 231 | nvn-64 |
| `nvnCommandBufferSetMemoryCallbackData` | 55, 231 | nvn-64 |

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
| `nvnCommandBufferBindBlendState` | 55, 231 | nvn-55, nvn-56, nvn-57, nvn2-7 |
| `nvnCommandBufferBindChannelMaskState` | 55, 231 | nvn-55, nvn-56, nvn-57, nvn2-7 |
| `nvnCommandBufferBindColorState` | 55, 231 | nvn-55, nvn-56, nvn-57, nvn2-7 |
| `nvnCommandBufferBindDepthStencilState` | 55, 231 | nvn-55, nvn-56, nvn-57, nvn2-7 |
| `nvnCommandBufferBindImage` | 231 |  |
| `nvnCommandBufferBindImages` |  |  |
| `nvnCommandBufferBindMultisampleState` | 55, 231 | nvn-55, nvn-56, nvn-57, nvn2-7 |
| `nvnCommandBufferBindPolygonState` | 55, 231 | nvn-55, nvn-56, nvn-57, nvn2-7 |
| `nvnCommandBufferBindProgram` | 55, 231 | nvn-55, nvn-56, nvn-57, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferBindSeparateSampler` |  |  |
| `nvnCommandBufferBindSeparateSamplers` |  |  |
| `nvnCommandBufferBindSeparateTexture` |  |  |
| `nvnCommandBufferBindSeparateTextures` |  |  |
| `nvnCommandBufferBindStorageBuffer` | 231 | nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferBindStorageBuffers` |  |  |
| `nvnCommandBufferBindTexture` | 55, 231 | nvn-55, nvn-56, nvn-57 |
| `nvnCommandBufferBindTextures` |  |  |
| `nvnCommandBufferBindTexturesDeferred` |  |  |
| `nvnCommandBufferBindTransformFeedbackBuffer` |  |  |
| `nvnCommandBufferBindTransformFeedbackBuffers` |  |  |
| `nvnCommandBufferBindUniformBuffer` | 55, 231 | nvn-55, nvn-56, nvn-57 |
| `nvnCommandBufferBindUniformBuffers` |  |  |
| `nvnCommandBufferBindVertexAttribState` | 55, 231 | nvn-55, nvn-56, nvn-57 |
| `nvnCommandBufferBindVertexBuffer` | 55, 231 | nvn-55, nvn-56, nvn-57 |
| `nvnCommandBufferBindVertexBuffers` |  |  |
| `nvnCommandBufferBindVertexStreamState` | 55, 231 | nvn-55, nvn-56, nvn-57 |
| `nvnCommandBufferSetSamplerPool` | 55, 231 | nvn-55, nvn-56, nvn-57 |
| `nvnCommandBufferSetShaderScratchMemory` |  |  |
| `nvnCommandBufferSetTexturePool` | 55, 231 | nvn-55, nvn-56, nvn-57 |
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
| `nvnCommandBufferDrawArrays` | 55, 231 | nvn-55, nvn-56, nvn-57 |
| `nvnCommandBufferDrawArraysIndirect` |  |  |
| `nvnCommandBufferDrawArraysInstanced` |  |  |
| `nvnCommandBufferDrawElements` | 55, 231 | nvn-55, nvn-56, nvn-57 |
| `nvnCommandBufferDrawElementsBaseVertex` | 55, 231 | nvn-55, nvn-56, nvn-57 |
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
| `nvnCommandBufferClearTexturei` | 55, 231 | nvn-67 |
| `nvnCommandBufferGetCopyImageStride` | 55, 231 | nvn-69 |
| `nvnCommandBufferGetCopyRowStride` | 55, 231 | nvn-69 |
| `nvnCommandBufferResolveDepthBuffer` |  |  |
| `nvnCommandBufferTiledDownsample` |  |  |
| `nvnCommandBufferTiledDownsampleTextureView` |  |  |
| `nvnCommandBufferClearBuffer` | 55, 231 | nvn-68 |
| `nvnCommandBufferClearColor` | 55, 231 | nvn-24, nvn-25, nvn-39, nvn-40, nvn-41, nvn-52, nvn-53, nvn-54, nvn-55, nvn-56, nvn-57, nvn2-7 |
| `nvnCommandBufferClearColori` | 55, 231 | nvn-66 |
| `nvnCommandBufferClearColorui` | 55, 231 | nvn-66 |
| `nvnCommandBufferClearDepthStencil` | 55, 231 | nvn-55, nvn-56, nvn-57, nvn2-7 |
| `nvnCommandBufferClearTexture` | 55, 231 | nvn-67 |
| `nvnCommandBufferClearTextureui` | 55, 231 | nvn-67 |
| `nvnCommandBufferCopyBufferToBuffer` | 55, 231 | nvn-29, nvn-38, nvn-50, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferCopyBufferToTexture` | 55, 231 | nvn-27, nvn-28, nvn-30, nvn-31, nvn-32, nvn-36, nvn-55, nvn-56, nvn-57 |
| `nvnCommandBufferCopyTextureToBuffer` | 55, 231 | nvn-24, nvn-25, nvn-27, nvn-28, nvn-30, nvn-31, nvn-32, nvn-36, nvn-39, nvn-40, nvn-41, nvn-55, nvn-56, nvn-57, nvn-66, nvn-67, nvn2-7 |
| `nvnCommandBufferCopyTextureToTexture` | 55, 231 | nvn-28, nvn-31 |
| `nvnCommandBufferDiscardColor` | 55, 231 | nvn-70 |
| `nvnCommandBufferDiscardDepthStencil` | 55, 231 | nvn-70 |
| `nvnCommandBufferDownsample` |  |  |
| `nvnCommandBufferDownsampleTextureView` |  |  |
| `nvnCommandBufferRestoreZCullData` |  |  |
| `nvnCommandBufferSaveZCullData` |  |  |
| `nvnCommandBufferSetCopyImageStride` | 55, 231 | nvn-69 |
| `nvnCommandBufferSetCopyRowStride` | 55, 231 | nvn-27, nvn-28, nvn-29, nvn-30, nvn-31, nvn-32, nvn-36, nvn-69 |

## Commands: render state

36 functions, 33 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferOverrideSubtileSize` |  |  |
| `nvnCommandBufferSetColorReductionEnable` |  |  |
| `nvnCommandBufferSetColorReductionThresholds` |  |  |
| `nvnCommandBufferSetConservativeRasterDilate` |  |  |
| `nvnCommandBufferSetInnerTessellationLevels` |  |  |
| `nvnCommandBufferSetOuterTessellationLevels` |  |  |
| `nvnCommandBufferSetProvokingVertexMode` |  |  |
| `nvnCommandBufferSetSubpixelPrecisionBias` |  |  |
| `nvnCommandBufferSetZCullZF32CompressionEnable` |  |  |
| `nvnCommandBufferSetShadingRatePaletteMode` |  |  |
| `nvnCommandBufferSetAlphaRef` |  |  |
| `nvnCommandBufferSetBlendColor` |  |  |
| `nvnCommandBufferSetConservativeRasterEnable` |  |  |
| `nvnCommandBufferSetDepthBounds` |  |  |
| `nvnCommandBufferSetDepthClamp` |  |  |
| `nvnCommandBufferSetDepthRange` |  |  |
| `nvnCommandBufferSetDepthRanges` |  |  |
| `nvnCommandBufferSetLineWidth` |  |  |
| `nvnCommandBufferSetPatchSize` |  |  |
| `nvnCommandBufferSetPointSize` |  |  |
| `nvnCommandBufferSetPolygonOffsetClamp` |  |  |
| `nvnCommandBufferSetPrimitiveRestart` |  |  |
| `nvnCommandBufferSetRasterizerDiscard` |  |  |
| `nvnCommandBufferSetRenderTargets` | 55, 231 | nvn-24, nvn-25, nvn-39, nvn-40, nvn-41, nvn-52, nvn-53, nvn-54, nvn-55, nvn-56, nvn-57, nvn-66, nvn2-7 |
| `nvnCommandBufferSetSampleMask` |  |  |
| `nvnCommandBufferSetScissor` | 55, 231 | nvn-24, nvn-25, nvn-39, nvn-40, nvn-41, nvn-52, nvn-53, nvn-54, nvn-55, nvn-56, nvn-57, nvn-66, nvn2-7 |
| `nvnCommandBufferSetScissors` |  |  |
| `nvnCommandBufferSetStencilCullCriteria` |  |  |
| `nvnCommandBufferSetStencilMask` |  |  |
| `nvnCommandBufferSetStencilRef` |  |  |
| `nvnCommandBufferSetStencilValueMask` |  |  |
| `nvnCommandBufferSetTiledCacheAction` |  |  |
| `nvnCommandBufferSetTiledCacheTileSize` |  |  |
| `nvnCommandBufferSetViewport` | 55, 231 | nvn-24, nvn-25, nvn-39, nvn-40, nvn-41, nvn-52, nvn-53, nvn-54, nvn-55, nvn-56, nvn-57, nvn2-7 |
| `nvnCommandBufferSetViewportSwizzles` |  |  |
| `nvnCommandBufferSetViewports` |  |  |

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

15 functions, 15 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnDeviceCalculateAccelerationStructureMemoryRequirements` |  |  |
| `nvnProgramGetShaderGroupHandle` |  |  |
| `nvnProgramBuilderSetRayTracingModules` |  |  |
| `nvnProgramBuilderSetMaxCallableRecursionDepth` |  |  |
| `nvnProgramBuilderGetMaxCallableRecursionDepth` |  |  |
| `nvnProgramBuilderSetMaxTraceRecursionDepth` |  |  |
| `nvnProgramBuilderGetMaxTraceRecursionDepth` |  |  |
| `nvnProgramBuilderSetProgramRayTracingFlags` |  |  |
| `nvnProgramBuilderGetProgramRayTracingFlags` |  |  |
| `nvnQueueBuilderGetRayTracingMemorySize` |  |  |
| `nvnQueueBuilderSetRayTracingMemorySize` |  |  |
| `nvnQueueGetTotalRayTracingMemoryUsed` |  |  |
| `nvnCommandBufferBindAccelerationStructure` |  |  |
| `nvnCommandBufferBuildAccelerationStructures` |  |  |
| `nvnCommandBufferCompactAccelerationStructure` |  |  |

## Debug

44 functions, 44 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnSetExternalDebuggerEnabledNVX` |  |  |
| `nvnDeviceGenerateDebugDomainId` |  |  |
| `nvnDeviceInstallDebugCallback` |  |  |
| `nvnDeviceIsExternalDebuggerAttached` |  |  |
| `nvnDeviceSetAutomaticCheckpointsByDebugGroup` |  |  |
| `nvnDeviceSetAutomaticCheckpointsEnable` |  |  |
| `nvnDeviceSetAutomaticCheckpointsMask` |  |  |
| `nvnDeviceSetAutomaticCheckpointsSamplingInterval` |  |  |
| `nvnDeviceSetCheckpointCallStacksEnable` |  |  |
| `nvnDeviceSetCheckpointMaxCallStacksDepth` |  |  |
| `nvnDeviceWalkDebugDatabase` |  |  |
| `nvnMemoryPoolSetDebugLabel` |  |  |
| `nvnBufferGetDebugID` |  |  |
| `nvnBufferSetDebugLabel` |  |  |
| `nvnTextureGetDebugID` |  |  |
| `nvnTextureSetDebugLabel` |  |  |
| `nvnSamplerGetDebugID` |  |  |
| `nvnSamplerSetDebugLabel` |  |  |
| `nvnProgramBuilderSetDebugDataHashes` |  |  |
| `nvnProgramBuilderGetDebugDataHashes` |  |  |
| `nvnCommandBufferInsertCheckpointDynamic` |  |  |
| `nvnCommandBufferInsertCheckpointStatic` |  |  |
| `nvnCommandBufferInsertDebugMarkerGraphics` |  |  |
| `nvnCommandBufferInsertDebugMarkerCompute` |  |  |
| `nvnCommandBufferPushDebugGroupGraphics` |  |  |
| `nvnCommandBufferPushDebugGroupCompute` |  |  |
| `nvnCommandBufferPopDebugGroupGraphics` |  |  |
| `nvnCommandBufferPopDebugGroupCompute` |  |  |
| `nvnCommandBufferInsertDebugMarker` |  |  |
| `nvnCommandBufferInsertDebugMarkerDynamic` |  |  |
| `nvnCommandBufferInsertDebugMarkerStatic` |  |  |
| `nvnCommandBufferPopDebugGroup` |  |  |
| `nvnCommandBufferPopDebugGroupId` |  |  |
| `nvnCommandBufferPushDebugGroup` |  |  |
| `nvnCommandBufferPushDebugGroupDynamic` |  |  |
| `nvnCommandBufferPushDebugGroupStatic` |  |  |
| `nvnCommandBufferSetDebugLabel` |  |  |
| `nvnDeviceSetDebugLabel` |  |  |
| `nvnProgramSetDebugLabel` |  |  |
| `nvnQueueSetDebugLabel` |  |  |
| `nvnSamplerPoolSetDebugLabel` |  |  |
| `nvnSyncSetDebugLabel` |  |  |
| `nvnTexturePoolSetDebugLabel` |  |  |
| `nvnWindowSetDebugLabel` |  |  |
