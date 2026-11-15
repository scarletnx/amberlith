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
names 677; the layer has 151 of them in its API 55 build and 164 in its API 231 build; a case calls 158. The
other 519 have no call at all: a runtime could answer any of them wrongly and every case would still pass.

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

- **the NRO** (API 55): `source/tests/t_nvn.c`, `source/tests/t_nvnscene.c`, `source/drawpath.c`, `source/drawpath_nvn.c`, from `RunNvn`.
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
| [Device](#device) | 36 | 10 | 11 | 10 | 26 |
| [Memory pools](#memory-pools) | 18 | 16 | 16 | 16 | 2 |
| [Buffers](#buffers) | 16 | 0 | 0 | 0 | 16 |
| [Textures](#textures) | 95 | 50 | 51 | 50 | 45 |
| [Samplers](#samplers) | 45 | 9 | 9 | 9 | 36 |
| [Programs](#programs) | 20 | 3 | 7 | 7 | 13 |
| [State objects](#state-objects) | 82 | 16 | 16 | 16 | 66 |
| [Queues](#queues) | 30 | 7 | 7 | 6 | 24 |
| [Sync and events](#sync-and-events) | 24 | 0 | 0 | 0 | 24 |
| [Windows and present](#windows-and-present) | 35 | 8 | 8 | 8 | 27 |
| [Counters and queries](#counters-and-queries) | 6 | 0 | 0 | 0 | 6 |
| [Commands: memory and recording](#commands-memory-and-recording) | 25 | 7 | 7 | 7 | 18 |
| [Commands: binds](#commands-binds) | 41 | 14 | 18 | 15 | 26 |
| [Commands: draws and dispatches](#commands-draws-and-dispatches) | 26 | 1 | 4 | 4 | 22 |
| [Commands: clears and copies](#commands-clears-and-copies) | 25 | 7 | 7 | 7 | 18 |
| [Commands: render state](#commands-render-state) | 36 | 3 | 3 | 3 | 33 |
| [DLSS](#dlss) | 58 | 0 | 0 | 0 | 58 |
| [Ray tracing](#ray-tracing) | 15 | 0 | 0 | 0 | 15 |
| [Debug](#debug) | 44 | 0 | 0 | 0 | 44 |
| **All** | **677** | **151** | **164** | **158** | **519** |

## The cases

| Label | Suite | Case | In |
| --- | --- | --- | --- |
| nvn-1 | nvn | a scene of five draws through deko3d holds what the processor works out | the NRO |
| nvn-2 | nvn | the same scene through the layer's own NVN functions holds what the processor works out | the NRO |
| nvn-3 | nvn | the program's NVN functions resolve | the NRO, the Switch 2 program |
| nvn-4 | nvn | the device initializes | the NRO, the Switch 2 program |
| nvn-5 | nvn | the device answers the layer's API version | the NRO, the Switch 2 program |
| nvn-6 | nvn | a queue, its pools and a 64x64 target come up | the NRO, the Switch 2 program |
| nvn-7 | nvn | clear to opaque red | the NRO, the Switch 2 program |
| nvn-8 | nvn | clear to a mid channel value | the NRO, the Switch 2 program |
| nvn-9 | nvn | two copy textures and a staging pool come up | the NRO, the Switch 2 program |
| nvn-10 | nvn | a picture copied into a texture and back keeps every texel | the NRO, the Switch 2 program |
| nvn-11 | nvn | a texture copied into another keeps every texel | the NRO, the Switch 2 program |
| nvn-12 | nvn | a buffer copied into another keeps every byte | the NRO, the Switch 2 program |
| nvn-13 | nvn | a texture copied to a buffer with MIRROR_Y lands upside down | the NRO, the Switch 2 program |
| nvn-14 | nvn | a texture copied into another with MIRROR_X lands mirrored | the NRO, the Switch 2 program |
| nvn-15 | nvn | a copy to a buffer with a row stride of 288 keeps the bytes between rows | the NRO, the Switch 2 program |
| nvn-16 | nvn | a texture builder gives back each number it was set | the NRO, the Switch 2 program |
| nvn-17 | nvn | a texture view says which of its parts were set, and with what | the NRO, the Switch 2 program |
| nvn-18 | nvn | a texture of three levels comes up and says what its builder had | the NRO, the Switch 2 program |
| nvn-19 | nvn | a picture copied into each level of a texture through a view comes back from that level | the NRO, the Switch 2 program |
| nvn-20 | nvn | a memory pool and its builder give back what they were set, and the pool maps its storage | the NRO, the Switch 2 program |
| nvn-21 | nvn | a page flushed from a cached pool is what a copy reads, and the copy is there after an invalidate | the NRO, the Switch 2 program |
| nvn-22 | nvn | a window acquires and presents both its textures | the NRO, the Switch 2 program |
| nvn-23 | nvn | the same scene through the NVN the program looks up holds what the processor works out | the NRO |
| nvn2-1 | nvn2 | a device, a queue, a command buffer, their pools and a 256x256 target with its depth come up | the Switch 2 program |
| nvn2-2 | nvn2 | a compute program assembled by hand makes a program through the builder | the Switch 2 program |
| nvn2-3 | nvn2 | a dispatch's writes to a storage buffer, as the processor reads them once the queue finishes | the Switch 2 program |
| nvn2-4 | nvn2 | a dispatch's writes to a storage buffer, as the graphics unit copies them to another buffer | the Switch 2 program |
| nvn2-5 | nvn2 | the Mandelbrot's compute, mesh and fragment programs make two programs | the Switch 2 program |
| nvn2-6 | nvn2 | a compute program counts the Mandelbrot set's steps for every cell of a grid | the Switch 2 program |
| nvn2-7 | nvn2 | a mesh program draws the counts as bars, a fragment program colours them and the depth test keeps the nearest | the Switch 2 program |
| nvn2-8 | nvn2 | one warp's matrix instruction multiplies a 16 by 16 matrix of halves by a 16 by 8 one into floats | the Switch 2 program |

## Device

36 functions, 26 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnBootstrapLoader` | 55, 231 | nvn-3, nvn-23, nvn2-1 |
| `nvnDeviceGetProcAddress` | 55, 231 | nvn-3, nvn-23, nvn2-1 |
| `nvnDeviceApplyDeferredFinalizes` |  |  |
| `nvnDeviceBuilderGetFlags` |  |  |
| `nvnDeviceFinalize` | 55, 231 | nvn-23 |
| `nvnDeviceRegisterFastClearColori` |  |  |
| `nvnDeviceRegisterFastClearColorui` |  |  |
| `nvnDeviceSetIntermediateShaderCache` |  |  |
| `nvnDeviceBuilderSetDefaults` | 55, 231 | nvn-4, nvn-23, nvn2-1 |
| `nvnDeviceBuilderSetFlags` | 55, 231 | nvn-4, nvn-23, nvn2-1 |
| `nvnDeviceGetImageHandle` | 231 |  |
| `nvnDeviceFinalizeCommandHandle` |  |  |
| `nvnDeviceGetCurrentTimestampInNanoseconds` |  |  |
| `nvnDeviceGetDepthMode` |  |  |
| `nvnDeviceGetInteger` | 55, 231 | nvn-5, nvn-23, nvn2-1 |
| `nvnDeviceGetWindowOriginMode` |  |  |
| `nvnDeviceGetSeparateSamplerHandle` |  |  |
| `nvnDeviceGetSeparateTextureHandle` |  |  |
| `nvnDeviceGetTexelFetchHandle` |  |  |
| `nvnDeviceGetTextureHandle` | 55, 231 | nvn-23 |
| `nvnDeviceInitialize` | 55, 231 | nvn-4, nvn-23, nvn2-1 |
| `nvnDeviceRegisterFastClearColor` |  |  |
| `nvnDeviceRegisterFastClearDepth` |  |  |
| `nvnDeviceRegisterFastClearStencil` |  |  |
| `nvnDeviceSetDepthMode` | 55, 231 | nvn-23 |
| `nvnDeviceSetWindowOriginMode` | 55, 231 | nvn-23 |
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
| `nvnMemoryPoolBuilderGetDevice` | 55, 231 | nvn-20 |
| `nvnMemoryPoolBuilderGetFlags` | 55, 231 | nvn-20 |
| `nvnMemoryPoolBuilderGetMemory` | 55, 231 | nvn-20 |
| `nvnMemoryPoolBuilderGetSize` | 55, 231 | nvn-20 |
| `nvnMemoryPoolBuilderSetDefaults` | 55, 231 | nvn-6, nvn-9, nvn-18, nvn-20, nvn-22, nvn-23, nvn2-1 |
| `nvnMemoryPoolBuilderSetDevice` | 55, 231 | nvn-6, nvn-9, nvn-18, nvn-20, nvn-22, nvn-23, nvn2-1 |
| `nvnMemoryPoolBuilderSetFlags` | 55, 231 | nvn-6, nvn-9, nvn-18, nvn-20, nvn-22, nvn-23, nvn2-1 |
| `nvnMemoryPoolBuilderSetStorage` | 55, 231 | nvn-6, nvn-9, nvn-18, nvn-20, nvn-22, nvn-23, nvn2-1 |
| `nvnMemoryPoolBuilderSetNativeHandleNVX` |  |  |
| `nvnMemoryPoolFinalize` | 55, 231 | nvn-22, nvn-23 |
| `nvnMemoryPoolFlushMappedRange` | 55, 231 | nvn-21 |
| `nvnMemoryPoolGetBufferAddress` | 55, 231 | nvn-7, nvn-8, nvn-10, nvn-11, nvn-12, nvn-13, nvn-14, nvn-15, nvn-19, nvn-21, nvn-23, nvn2-2, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-8 |
| `nvnMemoryPoolGetFlags` | 55, 231 | nvn-20 |
| `nvnMemoryPoolGetSize` | 55, 231 | nvn-20 |
| `nvnMemoryPoolInitialize` | 55, 231 | nvn-6, nvn-9, nvn-18, nvn-20, nvn-22, nvn-23, nvn2-1 |
| `nvnMemoryPoolInvalidateMappedRange` | 55, 231 | nvn-21 |
| `nvnMemoryPoolMap` | 55, 231 | nvn-7, nvn-8, nvn-9, nvn-10, nvn-11, nvn-12, nvn-13, nvn-14, nvn-15, nvn-19, nvn-20, nvn-21, nvn-23, nvn2-2, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-8 |
| `nvnMemoryPoolMapVirtual` |  |  |

## Buffers

16 functions, 16 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnBufferBuilderGetDevice` |  |  |
| `nvnBufferBuilderGetMemoryOffset` |  |  |
| `nvnBufferBuilderGetMemoryPool` |  |  |
| `nvnBufferBuilderGetSize` |  |  |
| `nvnBufferBuilderSetDefaults` |  |  |
| `nvnBufferBuilderSetDevice` |  |  |
| `nvnBufferBuilderSetStorage` |  |  |
| `nvnBufferFinalize` |  |  |
| `nvnBufferFlushMappedRange` |  |  |
| `nvnBufferInvalidateMappedRange` |  |  |
| `nvnBufferGetAddress` |  |  |
| `nvnBufferGetMemoryOffset` |  |  |
| `nvnBufferGetMemoryPool` |  |  |
| `nvnBufferGetSize` |  |  |
| `nvnBufferInitialize` |  |  |
| `nvnBufferMap` |  |  |

## Textures

95 functions, 45 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnTextureBuilderGetGLTextureName` |  |  |
| `nvnTextureBuilderGetMemoryOffset` | 55, 231 | nvn-16 |
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
| `nvnTextureViewCompare` | 55, 231 | nvn-17 |
| `nvnTextureBuilderGetDepthStencilMode` |  |  |
| `nvnTextureBuilderGetDevice` | 55, 231 | nvn-16 |
| `nvnTextureBuilderGetMemoryPool` | 55, 231 | nvn-16 |
| `nvnTextureBuilderGetPackagedTextureData` |  |  |
| `nvnTextureBuilderGetSwizzle` |  |  |
| `nvnTextureBuilderGetDepth` | 55, 231 | nvn-16 |
| `nvnTextureBuilderGetFlags` | 55, 231 | nvn-16 |
| `nvnTextureBuilderGetFormat` | 55, 231 | nvn-16 |
| `nvnTextureBuilderGetHeight` | 55, 231 | nvn-16 |
| `nvnTextureBuilderGetLevels` | 55, 231 | nvn-16 |
| `nvnTextureBuilderGetSamples` | 55, 231 | nvn-16 |
| `nvnTextureBuilderGetStorageAlignment` | 55, 231 | nvn-6, nvn-18, nvn-22, nvn-23, nvn2-1 |
| `nvnTextureBuilderGetStorageClass` |  |  |
| `nvnTextureBuilderGetStorageSize` | 55, 231 | nvn-6, nvn-9, nvn-18, nvn-22, nvn-23, nvn2-1 |
| `nvnTextureBuilderGetStride` |  |  |
| `nvnTextureBuilderGetTarget` | 55, 231 | nvn-16 |
| `nvnTextureBuilderGetWidth` | 55, 231 | nvn-16 |
| `nvnTextureBuilderGetZCullStorageSize` |  |  |
| `nvnTextureBuilderSetDefaults` | 55, 231 | nvn-6, nvn-9, nvn-16, nvn-18, nvn-22, nvn-23, nvn2-1 |
| `nvnTextureBuilderSetDepth` | 55, 231 | nvn-16, nvn-18 |
| `nvnTextureBuilderSetDepthStencilMode` |  |  |
| `nvnTextureBuilderSetDevice` | 55, 231 | nvn-6, nvn-9, nvn-16, nvn-18, nvn-22, nvn-23, nvn2-1 |
| `nvnTextureBuilderSetFlags` | 55, 231 | nvn-6, nvn-9, nvn-16, nvn-18, nvn-22, nvn-23, nvn2-1 |
| `nvnTextureBuilderSetFormat` | 55, 231 | nvn-6, nvn-9, nvn-16, nvn-18, nvn-22, nvn-23, nvn2-1 |
| `nvnTextureBuilderSetHeight` | 55, 231 | nvn-16 |
| `nvnTextureBuilderSetLevels` | 55, 231 | nvn-16, nvn-18 |
| `nvnTextureBuilderSetPackagedTextureData` |  |  |
| `nvnTextureBuilderSetPackagedTextureLayout` |  |  |
| `nvnTextureBuilderSetSamples` | 55, 231 | nvn-16, nvn-18 |
| `nvnTextureBuilderSetSize1D` |  |  |
| `nvnTextureBuilderSetSize2D` | 55, 231 | nvn-6, nvn-9, nvn-16, nvn-18, nvn-22, nvn-23, nvn2-1 |
| `nvnTextureBuilderSetSize3D` |  |  |
| `nvnTextureBuilderSetStorage` | 55, 231 | nvn-6, nvn-9, nvn-16, nvn-18, nvn-22, nvn-23, nvn2-1 |
| `nvnTextureBuilderSetStride` |  |  |
| `nvnTextureBuilderSetSwizzle` |  |  |
| `nvnTextureBuilderSetTarget` | 55, 231 | nvn-6, nvn-9, nvn-16, nvn-18, nvn-22, nvn-23, nvn2-1 |
| `nvnTextureBuilderSetWidth` | 55, 231 | nvn-16 |
| `nvnTextureCompare` |  |  |
| `nvnTextureFinalize` | 55, 231 | nvn-22, nvn-23 |
| `nvnTextureFlushTexels` |  |  |
| `nvnTextureGetDepth` | 55, 231 | nvn-18 |
| `nvnTextureGetDepthStencilMode` |  |  |
| `nvnTextureGetFlags` | 55, 231 | nvn-18 |
| `nvnTextureGetFormat` | 55, 231 | nvn-18 |
| `nvnTextureGetHeight` | 55, 231 | nvn-18 |
| `nvnTextureGetLevels` | 55, 231 | nvn-18 |
| `nvnTextureGetMemoryOffset` | 55, 231 | nvn-18 |
| `nvnTextureGetMemoryPool` | 55, 231 | nvn-18 |
| `nvnTextureGetSamples` | 55, 231 | nvn-18 |
| `nvnTextureGetStorageClass` |  |  |
| `nvnTextureGetStorageSize` |  |  |
| `nvnTextureGetStride` |  |  |
| `nvnTextureGetSwizzle` |  |  |
| `nvnTextureGetTarget` | 55, 231 | nvn-18 |
| `nvnTextureGetTextureAddress` |  |  |
| `nvnTextureGetViewOffset` |  |  |
| `nvnTextureGetWidth` | 55, 231 | nvn-18 |
| `nvnTextureGetZCullStorageSize` |  |  |
| `nvnTextureInitialize` | 55, 231 | nvn-6, nvn-9, nvn-18, nvn-22, nvn-23, nvn2-1 |
| `nvnTexturePoolFinalize` | 55, 231 | nvn-23 |
| `nvnTexturePoolGetMemoryOffset` |  |  |
| `nvnTexturePoolGetMemoryPool` |  |  |
| `nvnTexturePoolGetSize` |  |  |
| `nvnTexturePoolInitialize` | 55, 231 | nvn-23 |
| `nvnTexturePoolRegisterImage` | 231 |  |
| `nvnTexturePoolRegisterTexture` | 55, 231 | nvn-23 |
| `nvnTextureReadTexels` |  |  |
| `nvnTextureViewGetDepthStencilMode` |  |  |
| `nvnTextureViewGetFormat` | 55, 231 | nvn-17 |
| `nvnTextureViewGetLayers` | 55, 231 | nvn-17 |
| `nvnTextureViewGetLevels` | 55, 231 | nvn-17 |
| `nvnTextureViewGetSwizzle` |  |  |
| `nvnTextureViewGetTarget` | 55, 231 | nvn-17 |
| `nvnTextureViewSetDefaults` | 55, 231 | nvn-17, nvn-19 |
| `nvnTextureViewSetDepthStencilMode` |  |  |
| `nvnTextureViewSetFormat` | 55, 231 | nvn-17 |
| `nvnTextureViewSetLayers` | 55, 231 | nvn-17 |
| `nvnTextureViewSetLevels` | 55, 231 | nvn-17, nvn-19 |
| `nvnTextureViewSetSwizzle` |  |  |
| `nvnTextureViewSetTarget` | 55, 231 | nvn-17 |
| `nvnTextureWriteTexels` |  |  |
| `nvnTextureWriteTexelsStrided` |  |  |

## Samplers

45 functions, 36 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnSamplerBuilderGetBorderColor` |  |  |
| `nvnSamplerBuilderGetBorderColori` |  |  |
| `nvnSamplerBuilderGetBorderColorui` |  |  |
| `nvnSamplerBuilderGetCompare` |  |  |
| `nvnSamplerBuilderGetDevice` |  |  |
| `nvnSamplerBuilderGetLodBias` |  |  |
| `nvnSamplerBuilderGetLodClamp` |  |  |
| `nvnSamplerBuilderGetMaxAnisotropy` |  |  |
| `nvnSamplerBuilderGetMinMagFilter` |  |  |
| `nvnSamplerBuilderGetReductionFilter` |  |  |
| `nvnSamplerBuilderGetWrapMode` |  |  |
| `nvnSamplerBuilderSetBorderColori` |  |  |
| `nvnSamplerBuilderSetBorderColorui` |  |  |
| `nvnSamplerGetBorderColori` |  |  |
| `nvnSamplerGetBorderColorui` |  |  |
| `nvnSamplerPoolRegisterSamplerBuilder` |  |  |
| `nvnSamplerBuilderSetBorderColor` |  |  |
| `nvnSamplerBuilderSetCompare` |  |  |
| `nvnSamplerBuilderSetDefaults` | 55, 231 | nvn-23 |
| `nvnSamplerBuilderSetDevice` | 55, 231 | nvn-23 |
| `nvnSamplerBuilderSetLodBias` |  |  |
| `nvnSamplerBuilderSetLodClamp` |  |  |
| `nvnSamplerBuilderGetLodSnap` |  |  |
| `nvnSamplerBuilderSetLodSnap` |  |  |
| `nvnSamplerBuilderSetMaxAnisotropy` |  |  |
| `nvnSamplerBuilderSetMinMagFilter` | 55, 231 | nvn-23 |
| `nvnSamplerBuilderSetReductionFilter` |  |  |
| `nvnSamplerBuilderSetWrapMode` | 55, 231 | nvn-23 |
| `nvnSamplerCompare` |  |  |
| `nvnSamplerFinalize` | 55, 231 | nvn-23 |
| `nvnSamplerGetBorderColor` |  |  |
| `nvnSamplerGetCompare` |  |  |
| `nvnSamplerGetLodBias` |  |  |
| `nvnSamplerGetLodClamp` |  |  |
| `nvnSamplerGetMaxAnisotropy` |  |  |
| `nvnSamplerGetMinMagFilter` |  |  |
| `nvnSamplerGetReductionFilter` |  |  |
| `nvnSamplerGetWrapMode` |  |  |
| `nvnSamplerInitialize` | 55, 231 | nvn-23 |
| `nvnSamplerPoolFinalize` | 55, 231 | nvn-23 |
| `nvnSamplerPoolGetMemoryOffset` |  |  |
| `nvnSamplerPoolGetMemoryPool` |  |  |
| `nvnSamplerPoolGetSize` |  |  |
| `nvnSamplerPoolInitialize` | 55, 231 | nvn-23 |
| `nvnSamplerPoolRegisterSampler` | 55, 231 | nvn-23 |

## Programs

20 functions, 13 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnProgramGetSubtileSize` |  |  |
| `nvnProgramGetScratchBytesPerWarp` |  |  |
| `nvnProgramSetSampleShading` |  |  |
| `nvnProgramSetSubroutineLinkage` |  |  |
| `nvnProgramFinalize` | 55, 231 | nvn-23 |
| `nvnProgramInitialize` | 55, 231 | nvn-23 |
| `nvnProgramSetShaders` | 55, 231 | nvn-23 |
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
| `nvnBlendStateSetDefaults` | 55, 231 | nvn-23, nvn2-7 |
| `nvnChannelMaskStateSetChannelMask` |  |  |
| `nvnChannelMaskStateSetDefaults` | 55, 231 | nvn-23, nvn2-7 |
| `nvnColorStateGetBlendEnable` |  |  |
| `nvnColorStateSetAlphaTest` |  |  |
| `nvnColorStateSetBlendEnable` |  |  |
| `nvnColorStateSetDefaults` | 55, 231 | nvn-23, nvn2-7 |
| `nvnColorStateSetLogicOp` |  |  |
| `nvnDepthStencilStateSetDefaults` | 55, 231 | nvn-23, nvn2-7 |
| `nvnDepthStencilStateSetDepthFunc` | 55, 231 | nvn-23, nvn2-7 |
| `nvnDepthStencilStateSetDepthTestEnable` | 55, 231 | nvn-23, nvn2-7 |
| `nvnDepthStencilStateSetDepthWriteEnable` | 55, 231 | nvn-23, nvn2-7 |
| `nvnDepthStencilStateSetStencilFunc` |  |  |
| `nvnDepthStencilStateSetStencilOp` |  |  |
| `nvnDepthStencilStateSetStencilTestEnable` |  |  |
| `nvnMultisampleStateSetAlphaToCoverageEnable` |  |  |
| `nvnMultisampleStateSetDefaults` | 55, 231 | nvn-23, nvn2-7 |
| `nvnMultisampleStateSetMultisampleEnable` |  |  |
| `nvnMultisampleStateSetSamples` |  |  |
| `nvnPolygonStateSetCullFace` | 55, 231 | nvn-23, nvn2-7 |
| `nvnPolygonStateSetDefaults` | 55, 231 | nvn-23, nvn2-7 |
| `nvnPolygonStateSetFrontFace` | 55, 231 | nvn-23 |
| `nvnPolygonStateSetPolygonMode` |  |  |
| `nvnPolygonStateSetPolygonOffsetEnables` |  |  |
| `nvnVertexAttribStateSetDefaults` | 55, 231 | nvn-23 |
| `nvnVertexAttribStateSetFormat` | 55, 231 | nvn-23 |
| `nvnVertexAttribStateSetStreamIndex` | 55, 231 | nvn-23 |
| `nvnVertexStreamStateSetDefaults` | 55, 231 | nvn-23 |
| `nvnVertexStreamStateSetDivisor` |  |  |
| `nvnVertexStreamStateSetStride` | 55, 231 | nvn-23 |
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

30 functions, 24 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnQueueBuilderGetCommandFlushThreshold` |  |  |
| `nvnQueueBuilderGetCommandMemorySize` |  |  |
| `nvnQueueBuilderGetComputeMemorySize` |  |  |
| `nvnQueueBuilderGetControlMemorySize` |  |  |
| `nvnQueueBuilderGetDevice` |  |  |
| `nvnQueueBuilderGetFlags` |  |  |
| `nvnQueueBuilderGetMemory` |  |  |
| `nvnQueueBuilderGetMemorySize` |  |  |
| `nvnQueueGetTotalCommandMemoryUsed` |  |  |
| `nvnQueueGetTotalComputeMemoryUsed` |  |  |
| `nvnQueueGetTotalControlMemoryUsed` |  |  |
| `nvnQueueResetMemoryUsageCounts` |  |  |
| `nvnQueueBuilderGetQueueMemorySize` |  |  |
| `nvnQueueBuilderSetCommandFlushThreshold` |  |  |
| `nvnQueueBuilderSetCommandMemorySize` |  |  |
| `nvnQueueBuilderSetComputeMemorySize` |  |  |
| `nvnQueueBuilderSetControlMemorySize` |  |  |
| `nvnQueueBuilderSetDefaults` | 55, 231 | nvn-6, nvn-23, nvn2-1 |
| `nvnQueueBuilderSetDevice` | 55, 231 | nvn-6, nvn-23, nvn2-1 |
| `nvnQueueBuilderSetFlags` |  |  |
| `nvnQueueBuilderGetQueuePriority` |  |  |
| `nvnQueueBuilderSetQueuePriority` |  |  |
| `nvnQueueBuilderSetQueueMemory` |  |  |
| `nvnQueueFinalize` | 55, 231 | nvn-23 |
| `nvnQueueFinish` | 55, 231 | nvn-7, nvn-8, nvn-10, nvn-11, nvn-12, nvn-13, nvn-14, nvn-15, nvn-19, nvn-21, nvn-22, nvn-23, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnQueueFlush` | 55, 231 |  |
| `nvnQueueGetError` |  |  |
| `nvnQueueInitialize` | 55, 231 | nvn-6, nvn-23, nvn2-1 |
| `nvnQueueSubmitCommands` | 55, 231 | nvn-7, nvn-8, nvn-10, nvn-11, nvn-12, nvn-13, nvn-14, nvn-15, nvn-19, nvn-21, nvn-22, nvn-23, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnQueueBuilderSetPrimaryQueue` |  |  |

## Sync and events

24 functions, 24 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnSyncCreateGLSync` |  |  |
| `nvnSyncInitializeFromFencedGLSync` |  |  |
| `nvnSyncFinalize` |  |  |
| `nvnSyncInitialize` |  |  |
| `nvnSyncWait` |  |  |
| `nvnSyncGetNvRmSyncNVX` |  |  |
| `nvnSyncInitializeFromNvRmSyncNVX` |  |  |
| `nvnEventBuilderGetMemoryOffset` |  |  |
| `nvnEventBuilderGetMemoryPool` |  |  |
| `nvnEventBuilderGetStorage` |  |  |
| `nvnEventGetMemoryOffset` |  |  |
| `nvnEventGetMemoryPool` |  |  |
| `nvnEventBuilderSetDefaults` |  |  |
| `nvnEventBuilderSetStorage` |  |  |
| `nvnEventFinalize` |  |  |
| `nvnEventGetValue` |  |  |
| `nvnEventInitialize` |  |  |
| `nvnEventSignal` |  |  |
| `nvnQueueFenceSync` |  |  |
| `nvnQueueWaitSync` |  |  |
| `nvnCommandBufferFenceSync` |  |  |
| `nvnCommandBufferSignalEvent` |  |  |
| `nvnCommandBufferWaitEvent` |  |  |
| `nvnCommandBufferWaitSync` |  |  |

## Windows and present

35 functions, 27 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnQueueAcquireTexture` | 55, 231 | nvn-22 |
| `nvnQueuePresentTexture` | 55, 231 | nvn-22 |
| `nvnWindowBuilderGetDevice` |  |  |
| `nvnWindowBuilderGetNativeWindow` |  |  |
| `nvnWindowBuilderGetNumActiveTextures` |  |  |
| `nvnWindowGetNativeWindow` |  |  |
| `nvnWindowGetNumActiveTextures` |  |  |
| `nvnWindowGetNumTextures` |  |  |
| `nvnWindowSetNumActiveTextures` |  |  |
| `nvnWindowBuilderGetNumTextures` |  |  |
| `nvnWindowBuilderGetPresentInterval` |  |  |
| `nvnWindowBuilderGetTexture` |  |  |
| `nvnWindowGetCrop` |  |  |
| `nvnWindowGetPresentInterval` |  |  |
| `nvnWindowAcquireTexture` |  |  |
| `nvnWindowBuilderSetDefaults` | 55, 231 | nvn-22 |
| `nvnWindowBuilderSetDevice` | 55, 231 | nvn-22 |
| `nvnWindowBuilderSetNativeWindow` | 55, 231 | nvn-22 |
| `nvnWindowBuilderSetNumActiveTextures` |  |  |
| `nvnWindowBuilderSetPresentInterval` |  |  |
| `nvnWindowBuilderSetTextures` | 55, 231 | nvn-22 |
| `nvnWindowBuilderSetColorSpace` |  |  |
| `nvnWindowBuilderGetColorSpace` |  |  |
| `nvnWindowBuilderSetHdrDisplayMetadata` |  |  |
| `nvnWindowBuilderGetHdrDisplayMetadata` |  |  |
| `nvnWindowBuilderSetHdrContentMetadata` |  |  |
| `nvnWindowBuilderGetHdrContentMetadata` |  |  |
| `nvnWindowSetHdrDisplayMetadata` |  |  |
| `nvnWindowGetHdrDisplayMetadata` |  |  |
| `nvnWindowSetHdrContentMetadata` |  |  |
| `nvnWindowGetHdrContentMetadata` |  |  |
| `nvnWindowFinalize` | 55, 231 | nvn-22 |
| `nvnWindowInitialize` | 55, 231 | nvn-22 |
| `nvnWindowSetCrop` |  |  |
| `nvnWindowSetPresentInterval` |  |  |

## Counters and queries

6 functions, 6 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnDeviceGetTimestampInNanoseconds` |  |  |
| `nvnCommandBufferReportCounter` |  |  |
| `nvnCommandBufferReportValue` |  |  |
| `nvnCommandBufferResetCounter` |  |  |
| `nvnCommandBufferSetRenderEnable` |  |  |
| `nvnCommandBufferSetRenderEnableConditional` |  |  |

## Commands: memory and recording

25 functions, 18 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferGetCommandMemoryFree` |  |  |
| `nvnCommandBufferGetCommandMemorySize` |  |  |
| `nvnCommandBufferGetControlMemoryFree` |  |  |
| `nvnCommandBufferGetControlMemorySize` |  |  |
| `nvnCommandBufferSetControlMemoryCallbackEnabled` |  |  |
| `nvnCommandBufferAddCommandMemory` | 55, 231 | nvn-6, nvn-23, nvn2-1 |
| `nvnCommandBufferAddControlMemory` | 55, 231 | nvn-6, nvn-23, nvn2-1 |
| `nvnCommandBufferBarrier` | 55, 231 | nvn-7, nvn-8, nvn-10, nvn-11, nvn-13, nvn-14, nvn-15, nvn-19, nvn-23, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferBeginRecording` | 55, 231 | nvn-7, nvn-8, nvn-10, nvn-11, nvn-12, nvn-13, nvn-14, nvn-15, nvn-19, nvn-21, nvn-22, nvn-23, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferSetRecordingMode` |  |  |
| `nvnCommandBufferSetComputeOverlapMode` |  |  |
| `nvnCommandBufferCallCommands` |  |  |
| `nvnCommandBufferCopyCommands` |  |  |
| `nvnCommandBufferEndRecording` | 55, 231 | nvn-7, nvn-8, nvn-10, nvn-11, nvn-12, nvn-13, nvn-14, nvn-15, nvn-19, nvn-21, nvn-22, nvn-23, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferFinalize` | 55, 231 | nvn-23 |
| `nvnCommandBufferGetCommandMemoryUsed` |  |  |
| `nvnCommandBufferGetControlMemoryUsed` |  |  |
| `nvnCommandBufferGetMemoryCallback` |  |  |
| `nvnCommandBufferGetMemoryCallbackData` |  |  |
| `nvnCommandBufferInitialize` | 55, 231 | nvn-6, nvn-23, nvn2-1 |
| `nvnCommandBufferIsRecording` |  |  |
| `nvnCommandBufferSetCommandMemoryCallbackEnabled` |  |  |
| `nvnCommandBufferSetComputeWorkloadPriority` |  |  |
| `nvnCommandBufferSetMemoryCallback` |  |  |
| `nvnCommandBufferSetMemoryCallbackData` |  |  |

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
| `nvnCommandBufferBindBlendState` | 55, 231 | nvn-23, nvn2-7 |
| `nvnCommandBufferBindChannelMaskState` | 55, 231 | nvn-23, nvn2-7 |
| `nvnCommandBufferBindColorState` | 55, 231 | nvn-23, nvn2-7 |
| `nvnCommandBufferBindDepthStencilState` | 55, 231 | nvn-23, nvn2-7 |
| `nvnCommandBufferBindImage` | 231 |  |
| `nvnCommandBufferBindImages` |  |  |
| `nvnCommandBufferBindMultisampleState` | 55, 231 | nvn-23, nvn2-7 |
| `nvnCommandBufferBindPolygonState` | 55, 231 | nvn-23, nvn2-7 |
| `nvnCommandBufferBindProgram` | 55, 231 | nvn-23, nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferBindSeparateSampler` |  |  |
| `nvnCommandBufferBindSeparateSamplers` |  |  |
| `nvnCommandBufferBindSeparateTexture` |  |  |
| `nvnCommandBufferBindSeparateTextures` |  |  |
| `nvnCommandBufferBindStorageBuffer` | 231 | nvn2-3, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferBindStorageBuffers` |  |  |
| `nvnCommandBufferBindTexture` | 55, 231 | nvn-23 |
| `nvnCommandBufferBindTextures` |  |  |
| `nvnCommandBufferBindTexturesDeferred` |  |  |
| `nvnCommandBufferBindTransformFeedbackBuffer` |  |  |
| `nvnCommandBufferBindTransformFeedbackBuffers` |  |  |
| `nvnCommandBufferBindUniformBuffer` | 55, 231 | nvn-23 |
| `nvnCommandBufferBindUniformBuffers` |  |  |
| `nvnCommandBufferBindVertexAttribState` | 55, 231 | nvn-23 |
| `nvnCommandBufferBindVertexBuffer` | 55, 231 | nvn-23 |
| `nvnCommandBufferBindVertexBuffers` |  |  |
| `nvnCommandBufferBindVertexStreamState` | 55, 231 | nvn-23 |
| `nvnCommandBufferSetSamplerPool` | 55, 231 | nvn-23 |
| `nvnCommandBufferSetShaderScratchMemory` |  |  |
| `nvnCommandBufferSetTexturePool` | 55, 231 | nvn-23 |
| `nvnCommandBufferUpdateUniformBuffer` |  |  |

## Commands: draws and dispatches

26 functions, 22 with no case.

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
| `nvnCommandBufferDrawArrays` | 55, 231 | nvn-23 |
| `nvnCommandBufferDrawArraysIndirect` |  |  |
| `nvnCommandBufferDrawArraysInstanced` |  |  |
| `nvnCommandBufferDrawElements` |  |  |
| `nvnCommandBufferDrawElementsBaseVertex` |  |  |
| `nvnCommandBufferDrawElementsIndirect` |  |  |
| `nvnCommandBufferDrawElementsInstanced` |  |  |
| `nvnCommandBufferDrawTransformFeedback` |  |  |
| `nvnCommandBufferEndTransformFeedback` |  |  |
| `nvnCommandBufferMultiDrawArraysIndirectCount` |  |  |
| `nvnCommandBufferMultiDrawElementsIndirectCount` |  |  |
| `nvnCommandBufferPauseTransformFeedback` |  |  |
| `nvnCommandBufferResumeTransformFeedback` |  |  |

## Commands: clears and copies

25 functions, 18 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferClearTexturei` |  |  |
| `nvnCommandBufferGetCopyImageStride` |  |  |
| `nvnCommandBufferGetCopyRowStride` |  |  |
| `nvnCommandBufferResolveDepthBuffer` |  |  |
| `nvnCommandBufferTiledDownsample` |  |  |
| `nvnCommandBufferTiledDownsampleTextureView` |  |  |
| `nvnCommandBufferClearBuffer` |  |  |
| `nvnCommandBufferClearColor` | 55, 231 | nvn-7, nvn-8, nvn-22, nvn-23, nvn2-7 |
| `nvnCommandBufferClearColori` |  |  |
| `nvnCommandBufferClearColorui` |  |  |
| `nvnCommandBufferClearDepthStencil` | 55, 231 | nvn-23, nvn2-7 |
| `nvnCommandBufferClearTexture` |  |  |
| `nvnCommandBufferClearTextureui` |  |  |
| `nvnCommandBufferCopyBufferToBuffer` | 55, 231 | nvn-12, nvn-21, nvn2-4, nvn2-6, nvn2-7, nvn2-8 |
| `nvnCommandBufferCopyBufferToTexture` | 55, 231 | nvn-10, nvn-11, nvn-13, nvn-14, nvn-15, nvn-19, nvn-23 |
| `nvnCommandBufferCopyTextureToBuffer` | 55, 231 | nvn-7, nvn-8, nvn-10, nvn-11, nvn-13, nvn-14, nvn-15, nvn-19, nvn-23, nvn2-7 |
| `nvnCommandBufferCopyTextureToTexture` | 55, 231 | nvn-11, nvn-14 |
| `nvnCommandBufferDiscardColor` |  |  |
| `nvnCommandBufferDiscardDepthStencil` |  |  |
| `nvnCommandBufferDownsample` |  |  |
| `nvnCommandBufferDownsampleTextureView` |  |  |
| `nvnCommandBufferRestoreZCullData` |  |  |
| `nvnCommandBufferSaveZCullData` |  |  |
| `nvnCommandBufferSetCopyImageStride` |  |  |
| `nvnCommandBufferSetCopyRowStride` | 55, 231 | nvn-10, nvn-11, nvn-12, nvn-13, nvn-14, nvn-15, nvn-19 |

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
| `nvnCommandBufferSetRenderTargets` | 55, 231 | nvn-7, nvn-8, nvn-22, nvn-23, nvn2-7 |
| `nvnCommandBufferSetSampleMask` |  |  |
| `nvnCommandBufferSetScissor` | 55, 231 | nvn-7, nvn-8, nvn-22, nvn-23, nvn2-7 |
| `nvnCommandBufferSetScissors` |  |  |
| `nvnCommandBufferSetStencilCullCriteria` |  |  |
| `nvnCommandBufferSetStencilMask` |  |  |
| `nvnCommandBufferSetStencilRef` |  |  |
| `nvnCommandBufferSetStencilValueMask` |  |  |
| `nvnCommandBufferSetTiledCacheAction` |  |  |
| `nvnCommandBufferSetTiledCacheTileSize` |  |  |
| `nvnCommandBufferSetViewport` | 55, 231 | nvn-7, nvn-8, nvn-22, nvn-23, nvn2-7 |
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
