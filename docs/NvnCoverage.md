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
names 677; the layer has 528 of them in its API 55 build and 671 in its API 231 build; a case calls 671. The
other 6 have no call at all: a runtime could answer any of them wrongly and every case would still pass.

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
own bodies, which no runtime's NVN takes. The same goes for the cases of the suite's own device
(`LayerCases`), which run twice in the NRO: down the layer's own functions first, each named
`the layer's own: ...` and counted for nothing, then down the ones the program looks up.
`#if NVN_LAYER_API` chooses a build's lines.

`tools/nvn-coverage.py --check` says whether this file is what the tree gives, and the host build's
tests run it; `--gaps GROUP` lists a group's functions with no case.

## By group

| Group | Functions | Layer, API 55 | Layer, API 231 | Called by a case | No case |
| --- | ---: | ---: | ---: | ---: | ---: |
| [Device](#device) | 36 | 25 | 36 | 36 | 0 |
| [Memory pools](#memory-pools) | 18 | 18 | 18 | 18 | 0 |
| [Buffers](#buffers) | 16 | 16 | 16 | 16 | 0 |
| [Textures](#textures) | 95 | 94 | 95 | 95 | 0 |
| [Samplers](#samplers) | 45 | 45 | 45 | 45 | 0 |
| [Programs](#programs) | 20 | 7 | 20 | 20 | 0 |
| [State objects](#state-objects) | 82 | 82 | 82 | 82 | 0 |
| [Queues](#queues) | 30 | 30 | 30 | 30 | 0 |
| [Sync and events](#sync-and-events) | 24 | 22 | 22 | 22 | 2 |
| [Windows and present](#windows-and-present) | 35 | 25 | 35 | 35 | 0 |
| [Counters and queries](#counters-and-queries) | 6 | 6 | 6 | 6 | 0 |
| [Commands: memory and recording](#commands-memory-and-recording) | 25 | 22 | 25 | 25 | 0 |
| [Commands: binds](#commands-binds) | 41 | 25 | 41 | 41 | 0 |
| [Commands: draws and dispatches](#commands-draws-and-dispatches) | 26 | 15 | 26 | 26 | 0 |
| [Commands: clears and copies](#commands-clears-and-copies) | 25 | 25 | 25 | 25 | 0 |
| [Commands: render state](#commands-render-state) | 36 | 35 | 36 | 36 | 0 |
| [DLSS](#dlss) | 58 | 0 | 58 | 58 | 0 |
| [Ray tracing](#ray-tracing) | 15 | 0 | 11 | 11 | 4 |
| [Debug](#debug) | 44 | 36 | 44 | 44 | 0 |
| **All** | **677** | **528** | **671** | **671** | **6** |

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
| nvn-33 | nvn | the alpha test's reference and the blend colour a list sets decide two draws each, through the layer's own NVN functions | the NRO |
| nvn-34 | nvn | the stencil's reference, compare mask and write mask a list sets decide what is drawn, through the layer's own NVN functions | the NRO |
| nvn-35 | nvn | a sample mask without the first sample draws nothing, and a clamp holds a polygon offset, through the layer's own NVN functions | the NRO |
| nvn-36 | nvn | draws under five hints to the graphics unit are as without them, through the layer's own NVN functions | the NRO |
| nvn-37 | nvn | a program whose sample shading is its code's draws and asks for no subtile, through the layer's own NVN functions | the NRO |
| nvn-38 | nvn | draws after subroutines chosen for a program with no linkage map are as without them, through the layer's own NVN functions | the NRO |
| nvn-39 | nvn | a cleared target of four samples resolves into its colour by each of the four downsamples, through the layer's own NVN functions | the NRO |
| nvn-40 | nvn | depth tested draws around a depth target's resolve and its culling data's save and restore are as without them, through the layer's own NVN functions | the NRO |
| nvn-41 | nvn | conservative rasterization draws every pixel a quad touches, through the layer's own NVN functions | the NRO |
| nvn-42 | nvn | a conservative quad dilated by half a pixel reaches the pixels a third of one away, through the layer's own NVN functions | the NRO |
| nvn-43 | nvn | a line of width four is four rows of pixels, through the layer's own NVN functions | the NRO |
| nvn-44 | nvn | patches of the size and the outer levels a list sets fill their cells, through the layer's own NVN functions | the NRO |
| nvn-45 | nvn | draws between debug markers, groups and checkpoints are as without them, through the layer's own NVN functions | the NRO |
| nvn-46 | nvn | the device says its modes and its builder its flags, and clear values register, through the layer's own NVN functions | the NRO |
| nvn-47 | nvn | a program set from the longer shader data draws, through the layer's own NVN functions | the NRO |
| nvn-48 | nvn | a texture's handle and a sampler's put together sample, and a texel fetch handle fetches, through the layer's own NVN functions | the NRO |
| nvn-49 | nvn | the layer's own: the program's NVN functions resolve | the NRO |
| nvn-50 | nvn | the layer's own: the device initializes | the NRO |
| nvn-51 | nvn | the layer's own: the device answers the layer's API version | the NRO |
| nvn-52 | nvn | the layer's own: a queue, its pools and a 64x64 target come up | the NRO |
| nvn-53 | nvn | the layer's own: clear to opaque red | the NRO |
| nvn-54 | nvn | the layer's own: clear to a mid channel value | the NRO |
| nvn-55 | nvn | the layer's own: clears before and after deferred finalizes, the shader cache switch and a list's handle given up are there | the NRO |
| nvn-56 | nvn | the layer's own: two copy textures and a staging pool come up | the NRO |
| nvn-57 | nvn | the layer's own: a picture copied into a texture and back keeps every texel | the NRO |
| nvn-58 | nvn | the layer's own: a texture copied into another keeps every texel | the NRO |
| nvn-59 | nvn | the layer's own: a buffer copied into another keeps every byte | the NRO |
| nvn-60 | nvn | the layer's own: a texture copied to a buffer with MIRROR_Y lands upside down | the NRO |
| nvn-61 | nvn | the layer's own: a texture copied into another with MIRROR_X lands mirrored | the NRO |
| nvn-62 | nvn | the layer's own: a copy to a buffer with a row stride of 288 keeps the bytes between rows | the NRO |
| nvn-63 | nvn | the layer's own: a texture builder gives back each number it was set | the NRO |
| nvn-64 | nvn | the layer's own: a texture view says which of its parts were set, and with what | the NRO |
| nvn-65 | nvn | the layer's own: a texture of three levels comes up and says what its builder had | the NRO |
| nvn-66 | nvn | the layer's own: a picture copied into each level of a texture through a view comes back from that level | the NRO |
| nvn-67 | nvn | the layer's own: a memory pool and its builder give back what they were set, and the pool maps its storage | the NRO |
| nvn-68 | nvn | the layer's own: a page flushed from a cached pool is what a copy reads, and the copy is there after an invalidate | the NRO |
| nvn-69 | nvn | the layer's own: a sync fenced on the queue behind a clear signals, and the clear is there with no finish | the NRO |
| nvn-70 | nvn | the layer's own: a sync fenced inside the list behind a clear signals, and the clear is there with no finish | the NRO |
| nvn-71 | nvn | the layer's own: a queue and a list told to wait for a sync that signalled run their clear | the NRO |
| nvn-72 | nvn | the layer's own: a wait on a sync nothing fenced fails, with a timeout or without | the NRO |
| nvn-73 | nvn | the layer's own: with no OpenGL, a sync from an OpenGL sync and an OpenGL sync from a sync both fail | the NRO |
| nvn-74 | nvn | the layer's own: a pool builder given a native handle forgets its storage, and takes storage again | the NRO |
| nvn-75 | nvn | the layer's own: the queue has no error after work that was valid | the NRO |
| nvn-76 | nvn | the layer's own: a queue builder gives back its flags and says which of its sizes were set | the NRO |
| nvn-77 | nvn | the layer's own: an event keeps what the processor signals: a write, an add and an add that wraps | the NRO |
| nvn-78 | nvn | the layer's own: a sampler builder gives back each number it was set | the NRO |
| nvn-79 | nvn | the layer's own: a sampler says what its builder had, and is the same as another of that state alone | the NRO |
| nvn-80 | nvn | the layer's own: the texture and sampler pools say where they are, and a builder registers as its sampler | the NRO |
| nvn-81 | nvn | the layer's own: a buffer is a range of its pool: its address, its mapping, and what it and its builder say | the NRO |
| nvn-82 | nvn | the layer's own: bytes flushed from one buffer and copied to another are there after an invalidate | the NRO |
| nvn-83 | nvn | the layer's own: a blend state gives back its target, functions, equations and advanced mode | the NRO |
| nvn-84 | nvn | the layer's own: a channel mask and a colour state give back channels, blending, the logic operation and the alpha test | the NRO |
| nvn-85 | nvn | the layer's own: a depth and stencil state gives back its depth test and each face's stencil | the NRO |
| nvn-86 | nvn | the layer's own: a multisample state gives back its samples, its switches and its sample locations' grid | the NRO |
| nvn-87 | nvn | the layer's own: a polygon state, a vertex attribute and a vertex stream give back what they were set | the NRO |
| nvn-88 | nvn | the layer's own: a texture builder and a view give back their swizzle, depth and stencil mode and the rest | the NRO |
| nvn-89 | nvn | the layer's own: a texture says its swizzle, storage and address, and is the same as its twin alone | the NRO |
| nvn-90 | nvn | the layer's own: texels the processor writes into a texture are what it and the graphics unit read back | the NRO |
| nvn-91 | nvn | the layer's own: a part of a texture written and read with row strides keeps the bytes between rows | the NRO |
| nvn-92 | nvn | the layer's own: a 1D texture, each layer of an array and each slice of a 3D texture keep a picture of their own | the NRO |
| nvn-93 | nvn | the layer's own: a queue builder gives back its memory and priority, and a queue's memory counts are nought once reset | the NRO |
| nvn-94 | nvn | the layer's own: a list writes an event's value over the one the processor signalled | the NRO |
| nvn-95 | nvn | the layer's own: a list adds to an event's value | the NRO |
| nvn-96 | nvn | the layer's own: a list that waits for an event's value runs when the processor signals it, and not before | the NRO |
| nvn-97 | nvn | the layer's own: a virtual pool's page is the physical pool's page mapped into it | the NRO |
| nvn-98 | nvn | the layer's own: the debug layer's calls are answered as a release driver answers them, and a label changes no object | the NRO |
| nvn-99 | nvn | the layer's own: a program with no subroutine takes no linkage map, one offered or none | the NRO |
| nvn-100 | nvn | the layer's own: a window builder gives back what it was set | the NRO |
| nvn-101 | nvn | the program's NVN functions resolve | the NRO, the Switch 2 program |
| nvn-102 | nvn | the device initializes | the NRO, the Switch 2 program |
| nvn-103 | nvn | the device answers the layer's API version | the NRO, the Switch 2 program |
| nvn-104 | nvn | a queue, its pools and a 64x64 target come up | the NRO, the Switch 2 program |
| nvn-105 | nvn | clear to opaque red | the NRO, the Switch 2 program |
| nvn-106 | nvn | clear to a mid channel value | the NRO, the Switch 2 program |
| nvn-107 | nvn | clears before and after deferred finalizes, the shader cache switch and a list's handle given up are there | the NRO, the Switch 2 program |
| nvn-108 | nvn | two copy textures and a staging pool come up | the NRO, the Switch 2 program |
| nvn-109 | nvn | a picture copied into a texture and back keeps every texel | the NRO, the Switch 2 program |
| nvn-110 | nvn | a texture copied into another keeps every texel | the NRO, the Switch 2 program |
| nvn-111 | nvn | a buffer copied into another keeps every byte | the NRO, the Switch 2 program |
| nvn-112 | nvn | a texture copied to a buffer with MIRROR_Y lands upside down | the NRO, the Switch 2 program |
| nvn-113 | nvn | a texture copied into another with MIRROR_X lands mirrored | the NRO, the Switch 2 program |
| nvn-114 | nvn | a copy to a buffer with a row stride of 288 keeps the bytes between rows | the NRO, the Switch 2 program |
| nvn-115 | nvn | a texture builder gives back each number it was set | the NRO, the Switch 2 program |
| nvn-116 | nvn | a texture view says which of its parts were set, and with what | the NRO, the Switch 2 program |
| nvn-117 | nvn | a texture of three levels comes up and says what its builder had | the NRO, the Switch 2 program |
| nvn-118 | nvn | a picture copied into each level of a texture through a view comes back from that level | the NRO, the Switch 2 program |
| nvn-119 | nvn | a memory pool and its builder give back what they were set, and the pool maps its storage | the NRO, the Switch 2 program |
| nvn-120 | nvn | a page flushed from a cached pool is what a copy reads, and the copy is there after an invalidate | the NRO, the Switch 2 program |
| nvn-121 | nvn | a sync fenced on the queue behind a clear signals, and the clear is there with no finish | the NRO, the Switch 2 program |
| nvn-122 | nvn | a sync fenced inside the list behind a clear signals, and the clear is there with no finish | the NRO, the Switch 2 program |
| nvn-123 | nvn | a queue and a list told to wait for a sync that signalled run their clear | the NRO, the Switch 2 program |
| nvn-124 | nvn | a wait on a sync nothing fenced fails, with a timeout or without | the NRO, the Switch 2 program |
| nvn-125 | nvn | with no OpenGL, a sync from an OpenGL sync and an OpenGL sync from a sync both fail | the NRO, the Switch 2 program |
| nvn-126 | nvn | a pool builder given a native handle forgets its storage, and takes storage again | the NRO, the Switch 2 program |
| nvn-127 | nvn | the queue has no error after work that was valid | the NRO, the Switch 2 program |
| nvn-128 | nvn | a queue builder gives back its flags and says which of its sizes were set | the NRO, the Switch 2 program |
| nvn-129 | nvn | an event keeps what the processor signals: a write, an add and an add that wraps | the NRO, the Switch 2 program |
| nvn-130 | nvn | a sampler builder gives back each number it was set | the NRO, the Switch 2 program |
| nvn-131 | nvn | a sampler says what its builder had, and is the same as another of that state alone | the NRO, the Switch 2 program |
| nvn-132 | nvn | the texture and sampler pools say where they are, and a builder registers as its sampler | the NRO, the Switch 2 program |
| nvn-133 | nvn | a buffer is a range of its pool: its address, its mapping, and what it and its builder say | the NRO, the Switch 2 program |
| nvn-134 | nvn | bytes flushed from one buffer and copied to another are there after an invalidate | the NRO, the Switch 2 program |
| nvn-135 | nvn | a blend state gives back its target, functions, equations and advanced mode | the NRO, the Switch 2 program |
| nvn-136 | nvn | a channel mask and a colour state give back channels, blending, the logic operation and the alpha test | the NRO, the Switch 2 program |
| nvn-137 | nvn | a depth and stencil state gives back its depth test and each face's stencil | the NRO, the Switch 2 program |
| nvn-138 | nvn | a multisample state gives back its samples, its switches and its sample locations' grid | the NRO, the Switch 2 program |
| nvn-139 | nvn | a polygon state, a vertex attribute and a vertex stream give back what they were set | the NRO, the Switch 2 program |
| nvn-140 | nvn | a texture builder and a view give back their swizzle, depth and stencil mode and the rest | the NRO, the Switch 2 program |
| nvn-141 | nvn | a texture says its swizzle, storage and address, and is the same as its twin alone | the NRO, the Switch 2 program |
| nvn-142 | nvn | texels the processor writes into a texture are what it and the graphics unit read back | the NRO, the Switch 2 program |
| nvn-143 | nvn | a part of a texture written and read with row strides keeps the bytes between rows | the NRO, the Switch 2 program |
| nvn-144 | nvn | a 1D texture, each layer of an array and each slice of a 3D texture keep a picture of their own | the NRO, the Switch 2 program |
| nvn-145 | nvn | a queue builder gives back its memory and priority, and a queue's memory counts are nought once reset | the NRO, the Switch 2 program |
| nvn-146 | nvn | a list writes an event's value over the one the processor signalled | the NRO, the Switch 2 program |
| nvn-147 | nvn | a list adds to an event's value | the NRO, the Switch 2 program |
| nvn-148 | nvn | a list that waits for an event's value runs when the processor signals it, and not before | the NRO, the Switch 2 program |
| nvn-149 | nvn | a virtual pool's page is the physical pool's page mapped into it | the NRO, the Switch 2 program |
| nvn-150 | nvn | the debug layer's calls are answered as a release driver answers them, and a label changes no object | the NRO, the Switch 2 program |
| nvn-151 | nvn | a program with no subroutine takes no linkage map, one offered or none | the NRO, the Switch 2 program |
| nvn-152 | nvn | a window builder gives back what it was set | the NRO, the Switch 2 program |
| nvn-153 | nvn | a window acquires and presents both its textures | the NRO, the Switch 2 program |
| nvn-154 | nvn | a window says what it is of, and keeps the active textures, the interval and the crop it is set | the NRO, the Switch 2 program |
| nvn-155 | nvn | a texture the window hands with a sync is free once the sync signals, and presents | the NRO, the Switch 2 program |
| nvn-156 | nvn | the same scene through the NVN the program looks up holds what the processor works out | the NRO |
| nvn-157 | nvn | the same texture past its edges through the NVN the program looks up does the same | the NRO |
| nvn-158 | nvn | the same four indexed quads through the NVN the program looks up are each in its quarter | the NRO |
| nvn-159 | nvn | the scene of five draws bound through the looked up calls that take several holds what the processor works out | the NRO |
| nvn-160 | nvn | the scene of five draws bound through the looked up deferred calls holds what the processor works out | the NRO |
| nvn-161 | nvn | the same four quads of an updated uniform buffer through the NVN the program looks up are each its own colour | the NRO |
| nvn-162 | nvn | the same eight instances through the NVN the program looks up are each in its cell | the NRO |
| nvn-163 | nvn | a texture drawn with no program and as many draws as a word says, through the NVN the program looks up, are each in its place | the NRO |
| nvn-164 | nvn | what a capture took of two draws, paused for a third between them, is drawn again a row down through the NVN the program looks up | the NRO |
| nvn-165 | nvn | the same capture into a buffer bound by the call that takes several is drawn again a row down | the NRO |
| nvn-166 | nvn | the samples counter reports a quad's 256 pixels and none after a reset, through the NVN the program looks up | the NRO |
| nvn-167 | nvn | two values a list reports are in memory once it has run, through the NVN the program looks up | the NRO |
| nvn-168 | nvn | two timestamp reports hold no count and times between the device's clock before and after, through the NVN the program looks up | the NRO |
| nvn-169 | nvn | a timestamp report's word is the graphics unit's ticks, 384 every 625 nanoseconds, through the NVN the program looks up | the NRO |
| nvn-170 | nvn | the render enable draws under on and under a condition that holds and skips the rest, through the NVN the program looks up | the NRO |
| nvn-171 | nvn | a command buffer's memory used and free come to what it was given, and draws take some, through the NVN the program looks up | the NRO |
| nvn-172 | nvn | a list out of command memory asks the program's callback and draws on, through the NVN the program looks up | the NRO |
| nvn-173 | nvn | a list called from another and a list copied into it draw their cells, through the NVN the program looks up | the NRO |
| nvn-174 | nvn | clears of an unsigned and a signed integer target hold the integers given, through the NVN the program looks up | the NRO |
| nvn-175 | nvn | a clear of a texture that is no target reaches its region and its mask's channels, through the NVN the program looks up | the NRO |
| nvn-176 | nvn | a range of a buffer cleared to a word holds it, and nothing beside the range, through the NVN the program looks up | the NRO |
| nvn-177 | nvn | a command buffer gives back the copy strides it was set, through the NVN the program looks up | the NRO |
| nvn-178 | nvn | targets discarded, then cleared and drawn, hold the clear and the draw, through the NVN the program looks up | the NRO |
| nvn-179 | nvn | a scissor and a viewport set by number bound and place a quad, through the NVN the program looks up | the NRO |
| nvn-180 | nvn | depth ranges move a quad across the depth test and depth bounds test the target's depth, through the NVN the program looks up | the NRO |
| nvn-181 | nvn | the rasterizer discards, clamps depth and takes a flat colour from the provoking vertex, through the NVN the program looks up | the NRO |
| nvn-182 | nvn | a point of sixteen pixels fills its cell, through the NVN the program looks up | the NRO |
| nvn-183 | nvn | a strip with a restart index is two quads, through the NVN the program looks up | the NRO |
| nvn-184 | nvn | a viewport swizzle of x and y turns a quad about the diagonal, through the NVN the program looks up | the NRO |
| nvn-185 | nvn | draws around the tiled cache's actions are as without them, through the NVN the program looks up | the NRO |
| nvn-186 | nvn | the alpha test's reference and the blend colour a list sets decide two draws each, through the NVN the program looks up | the NRO |
| nvn-187 | nvn | the stencil's reference, compare mask and write mask a list sets decide what is drawn, through the NVN the program looks up | the NRO |
| nvn-188 | nvn | a sample mask without the first sample draws nothing, and a clamp holds a polygon offset, through the NVN the program looks up | the NRO |
| nvn-189 | nvn | draws under five hints to the graphics unit are as without them, through the NVN the program looks up | the NRO |
| nvn-190 | nvn | a program whose sample shading is its code's draws and asks for no subtile, through the NVN the program looks up | the NRO |
| nvn-191 | nvn | draws after subroutines chosen for a program with no linkage map are as without them, through the NVN the program looks up | the NRO |
| nvn-192 | nvn | a cleared target of four samples resolves into its colour by each of the four downsamples, through the NVN the program looks up | the NRO |
| nvn-193 | nvn | depth tested draws around a depth target's resolve and its culling data's save and restore are as without them, through the NVN the program looks up | the NRO |
| nvn-194 | nvn | conservative rasterization draws every pixel a quad touches, through the NVN the program looks up | the NRO |
| nvn-195 | nvn | a conservative quad dilated by half a pixel reaches the pixels a third of one away, through the NVN the program looks up | the NRO |
| nvn-196 | nvn | a line of width four is four rows of pixels, through the NVN the program looks up | the NRO |
| nvn-197 | nvn | patches of the size and the outer levels a list sets fill their cells, through the NVN the program looks up | the NRO |
| nvn-198 | nvn | draws between debug markers, groups and checkpoints are as without them, through the NVN the program looks up | the NRO |
| nvn-199 | nvn | the device says its modes and its builder its flags, and clear values register, through the NVN the program looks up | the NRO |
| nvn-200 | nvn | a program set from the longer shader data draws, through the NVN the program looks up | the NRO |
| nvn-201 | nvn | a texture's handle and a sampler's put together sample, and a texel fetch handle fetches, through the NVN the program looks up | the NRO |
| nvn-202 | nvn | a clear between the Ounce's graphics and compute markers and groups is the clear | the Switch 2 program |
| nvn-203 | nvn | an Ounce program builder gives back what it was set, a ray tracing program's numbers too | the Switch 2 program |
| nvn-204 | nvn | an Ounce program builder gives back the subroutine linkage it was set, and none before | the Switch 2 program |
| nvn-205 | nvn | an Ounce program builder gives back the debug data hashes it was set, and none before | the Switch 2 program |
| nvn-206 | nvn | an Ounce program with no ray tracing asks for no scratch memory per warp | the Switch 2 program |
| nvn-207 | nvn | an Ounce program with no ray tracing leaves a shader group's handle as it was | the Switch 2 program |
| nvn-208 | nvn | ray tracing modules given to an Ounce program builder leave what it answers as set | the Switch 2 program |
| nvn-209 | nvn | an Ounce queue builder says whether its ray tracing memory size was set, and gives it back | the Switch 2 program |
| nvn-210 | nvn | an Ounce queue's ray tracing memory count is nought once its counts are reset | the Switch 2 program |
| nvn-211 | nvn | an Ounce window builder starts with the driver's colour space and HDR metadata, and gives back what it is set | the Switch 2 program |
| nvn-212 | nvn | an Ounce window has its builder's HDR metadata, and gives back what it is set | the Switch 2 program |
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
| nvn2-20 | nvn2 | the same dispatch after the list's recording mode, compute overlap mode and workload priority are set | the Switch 2 program |
| nvn2-21 | nvn2 | the same bars with the green and blue a fragment program reads from a root table pushed | the Switch 2 program |
| nvn2-22 | nvn2 | the same picture with no texture bound as the shading rate image, and the palette's mode set | the Switch 2 program |
| nvn2-23 | nvn2 | the same picture under a shading rate palette of the full rate in every entry, and no palette for another viewport | the Switch 2 program |
| nvn2-24 | nvn2 | a dispatch loads each texel of one image and stores three times it plus its column into another | the Switch 2 program |
| nvn2-25 | nvn2 | the same images bound by the call that takes several | the Switch 2 program |
| nvn2-26 | nvn2 | the same images bound by the deferred call | the Switch 2 program |
| nvn2-27 | nvn2 | a dispatch samples a texture bound apart from its sampler, slot 1 of each, past the texture's edge | the Switch 2 program |
| nvn2-28 | nvn2 | the same texture and sampler bound by the calls that take several | the Switch 2 program |
| nvn2-29 | nvn2 | the same texture and sampler bound by the deferred calls | the Switch 2 program |
| nvn2-30 | nvn2 | a uniform buffer's handle is its address and size, and the device's two buffer builders are filled | the Switch 2 program |
| nvn2-31 | nvn2 | a DLSS network builder gives back what it was set | the Switch 2 program |
| nvn2-32 | nvn2 | the parameters of a DLSS evaluation give back what they were set | the Switch 2 program |
| nvn2-33 | nvn2 | a DLSS builder's render sizes are in order within its output, and a network is resized within it alone | the Switch 2 program |
| nvn2-34 | nvn2 | a DLSS evaluation of a smooth picture into four times its side is within a tenth of a plain filter | the Switch 2 program |

## Device

36 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnBootstrapLoader` | 55, 231 | nvn-101, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn2-1 |
| `nvnDeviceGetProcAddress` | 55, 231 | nvn-101, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn2-1 |
| `nvnDeviceApplyDeferredFinalizes` | 55, 231 | nvn-107 |
| `nvnDeviceBuilderGetFlags` | 55, 231 | nvn-199 |
| `nvnDeviceFinalize` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnDeviceRegisterFastClearColori` | 55, 231 | nvn-199 |
| `nvnDeviceRegisterFastClearColorui` | 55, 231 | nvn-199 |
| `nvnDeviceSetIntermediateShaderCache` | 55, 231 | nvn-107 |
| `nvnDeviceBuilderSetDefaults` | 55, 231 | nvn-102, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-199, nvn2-1 |
| `nvnDeviceBuilderSetFlags` | 55, 231 | nvn-102, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-199, nvn2-1 |
| `nvnDeviceGetImageHandle` | 231 | nvn2-24, nvn2-25, nvn2-26 |
| `nvnDeviceFinalizeCommandHandle` | 55, 231 | nvn-107 |
| `nvnDeviceGetCurrentTimestampInNanoseconds` | 55, 231 | nvn-15, nvn-16, nvn-168, nvn-169 |
| `nvnDeviceGetDepthMode` | 55, 231 | nvn-199 |
| `nvnDeviceGetInteger` | 55, 231 | nvn-103, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn2-1, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29, nvn2-30 |
| `nvnDeviceGetWindowOriginMode` | 55, 231 | nvn-199 |
| `nvnDeviceGetSeparateSamplerHandle` | 55, 231 | nvn-201, nvn2-27, nvn2-28, nvn2-29 |
| `nvnDeviceGetSeparateTextureHandle` | 55, 231 | nvn-201, nvn2-27, nvn2-28, nvn2-29 |
| `nvnDeviceGetTexelFetchHandle` | 55, 231 | nvn-201 |
| `nvnDeviceGetTextureHandle` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnDeviceInitialize` | 55, 231 | nvn-102, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn2-1 |
| `nvnDeviceRegisterFastClearColor` | 55, 231 | nvn-199 |
| `nvnDeviceRegisterFastClearDepth` | 55, 231 | nvn-199 |
| `nvnDeviceRegisterFastClearStencil` | 55, 231 | nvn-199 |
| `nvnDeviceSetDepthMode` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165 |
| `nvnDeviceSetWindowOriginMode` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165 |
| `nvnDeviceGetUniformBufferHandle` | 231 | nvn2-30 |
| `nvnDeviceCommandBufferSetDefaults` | 231 | nvn2-30 |
| `nvnDeviceCommandBufferSetDevice` | 231 | nvn2-30 |
| `nvnDeviceCommandBufferSetStorage` | 231 | nvn2-30 |
| `nvnDeviceCommandBufferSetCommandMemorySize` | 231 | nvn2-30 |
| `nvnDeviceCommandBufferSetComputeMemorySize` | 231 | nvn2-30 |
| `nvnDeviceMessageBufferSetDefaults` | 231 | nvn2-30 |
| `nvnDeviceMessageBufferSetDevice` | 231 | nvn2-30 |
| `nvnDeviceMessageBufferSetStorage` | 231 | nvn2-30 |
| `nvnDeviceMessageBufferEndMessages` | 231 | nvn2-30 |

## Memory pools

18 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnMemoryPoolBuilderGetDevice` | 55, 231 | nvn-119 |
| `nvnMemoryPoolBuilderGetFlags` | 55, 231 | nvn-119 |
| `nvnMemoryPoolBuilderGetMemory` | 55, 231 | nvn-119, nvn-126 |
| `nvnMemoryPoolBuilderGetSize` | 55, 231 | nvn-119, nvn-126 |
| `nvnMemoryPoolBuilderSetDefaults` | 55, 231 | nvn-21, nvn-22, nvn-39, nvn-104, nvn-108, nvn-117, nvn-119, nvn-126, nvn-144, nvn-149, nvn-153, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-174, nvn-175, nvn-192, nvn2-1, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29 |
| `nvnMemoryPoolBuilderSetDevice` | 55, 231 | nvn-21, nvn-22, nvn-39, nvn-104, nvn-108, nvn-117, nvn-119, nvn-126, nvn-144, nvn-149, nvn-153, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-174, nvn-175, nvn-192, nvn2-1, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29 |
| `nvnMemoryPoolBuilderSetFlags` | 55, 231 | nvn-21, nvn-22, nvn-39, nvn-104, nvn-108, nvn-117, nvn-119, nvn-144, nvn-149, nvn-153, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-174, nvn-175, nvn-192, nvn2-1, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29 |
| `nvnMemoryPoolBuilderSetStorage` | 55, 231 | nvn-21, nvn-22, nvn-39, nvn-104, nvn-108, nvn-117, nvn-119, nvn-126, nvn-144, nvn-149, nvn-153, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-174, nvn-175, nvn-192, nvn2-1, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29 |
| `nvnMemoryPoolBuilderSetNativeHandleNVX` | 55, 231 | nvn-126 |
| `nvnMemoryPoolFinalize` | 55, 231 | nvn-21, nvn-22, nvn-39, nvn-144, nvn-149, nvn-153, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-174, nvn-175, nvn-192, nvn-197, nvn-201 |
| `nvnMemoryPoolFlushMappedRange` | 55, 231 | nvn-120, nvn-134 |
| `nvnMemoryPoolGetBufferAddress` | 55, 231 | nvn-105, nvn-106, nvn-107, nvn-109, nvn-110, nvn-111, nvn-112, nvn-113, nvn-114, nvn-118, nvn-120, nvn-121, nvn-122, nvn-123, nvn-133, nvn-141, nvn-142, nvn-144, nvn-148, nvn-149, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201, nvn-202, nvn2-2, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-8, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29, nvn2-30, nvn2-34 |
| `nvnMemoryPoolGetFlags` | 55, 231 | nvn-119 |
| `nvnMemoryPoolGetSize` | 55, 231 | nvn-119, nvn-150 |
| `nvnMemoryPoolInitialize` | 55, 231 | nvn-21, nvn-22, nvn-39, nvn-104, nvn-108, nvn-117, nvn-119, nvn-144, nvn-149, nvn-153, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-174, nvn-175, nvn-192, nvn2-1, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29 |
| `nvnMemoryPoolInvalidateMappedRange` | 55, 231 | nvn-120 |
| `nvnMemoryPoolMap` | 55, 231 | nvn-105, nvn-106, nvn-107, nvn-108, nvn-109, nvn-110, nvn-111, nvn-112, nvn-113, nvn-114, nvn-118, nvn-119, nvn-120, nvn-121, nvn-122, nvn-123, nvn-129, nvn-132, nvn-133, nvn-134, nvn-142, nvn-144, nvn-148, nvn-149, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201, nvn-202, nvn2-2, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-8, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29, nvn2-34 |
| `nvnMemoryPoolMapVirtual` | 55, 231 | nvn-149 |

## Buffers

16 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnBufferBuilderGetDevice` | 55, 231 | nvn-133 |
| `nvnBufferBuilderGetMemoryOffset` | 55, 231 | nvn-133 |
| `nvnBufferBuilderGetMemoryPool` | 55, 231 | nvn-133 |
| `nvnBufferBuilderGetSize` | 55, 231 | nvn-133 |
| `nvnBufferBuilderSetDefaults` | 55, 231 | nvn-133, nvn-134, nvn-150 |
| `nvnBufferBuilderSetDevice` | 55, 231 | nvn-133, nvn-134, nvn-150 |
| `nvnBufferBuilderSetStorage` | 55, 231 | nvn-133, nvn-134, nvn-150 |
| `nvnBufferFinalize` | 55, 231 | nvn-133, nvn-134, nvn-150 |
| `nvnBufferFlushMappedRange` | 55, 231 | nvn-134 |
| `nvnBufferInvalidateMappedRange` | 55, 231 | nvn-134 |
| `nvnBufferGetAddress` | 55, 231 | nvn-133, nvn-134 |
| `nvnBufferGetMemoryOffset` | 55, 231 | nvn-133 |
| `nvnBufferGetMemoryPool` | 55, 231 | nvn-133 |
| `nvnBufferGetSize` | 55, 231 | nvn-133, nvn-150 |
| `nvnBufferInitialize` | 55, 231 | nvn-133, nvn-134, nvn-150 |
| `nvnBufferMap` | 55, 231 | nvn-133, nvn-134 |

## Textures

95 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnTextureBuilderGetGLTextureName` | 55, 231 | nvn-140 |
| `nvnTextureBuilderGetMemoryOffset` | 55, 231 | nvn-115 |
| `nvnTextureBuilderGetPackagedTextureLayout` | 55, 231 | nvn-140 |
| `nvnTextureBuilderGetRawStorageClass` | 55, 231 | nvn-141 |
| `nvnTextureBuilderGetSparseTileLayout` | 55, 231 | nvn-141 |
| `nvnTextureBuilderGetViewOffset` | 55, 231 | nvn-141 |
| `nvnTextureBuilderSetGLTextureName` | 55, 231 | nvn-140 |
| `nvnTextureGetNvRmSurfaceNVX` | 55, 231 | nvn-141 |
| `nvnTextureGetRawStorageClass` | 55, 231 | nvn-141 |
| `nvnTextureGetSparseTileLayout` | 55, 231 | nvn-141 |
| `nvnTextureInvalidateTexels` | 55, 231 | nvn-142 |
| `nvnTextureReadTexelsStrided` | 55, 231 | nvn-143 |
| `nvnTextureViewCompare` | 55, 231 | nvn-116 |
| `nvnTextureBuilderGetDepthStencilMode` | 55, 231 | nvn-140 |
| `nvnTextureBuilderGetDevice` | 55, 231 | nvn-115 |
| `nvnTextureBuilderGetMemoryPool` | 55, 231 | nvn-115 |
| `nvnTextureBuilderGetPackagedTextureData` | 55, 231 | nvn-140 |
| `nvnTextureBuilderGetSwizzle` | 55, 231 | nvn-140 |
| `nvnTextureBuilderGetDepth` | 55, 231 | nvn-115 |
| `nvnTextureBuilderGetFlags` | 55, 231 | nvn-115 |
| `nvnTextureBuilderGetFormat` | 55, 231 | nvn-115 |
| `nvnTextureBuilderGetHeight` | 55, 231 | nvn-115 |
| `nvnTextureBuilderGetLevels` | 55, 231 | nvn-115 |
| `nvnTextureBuilderGetSamples` | 55, 231 | nvn-115 |
| `nvnTextureBuilderGetStorageAlignment` | 55, 231 | nvn-21, nvn-22, nvn-39, nvn-104, nvn-117, nvn-144, nvn-153, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-174, nvn-175, nvn-192, nvn-197, nvn-201, nvn2-1, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29, nvn2-34 |
| `nvnTextureBuilderGetStorageClass` | 55, 231 | nvn-141 |
| `nvnTextureBuilderGetStorageSize` | 55, 231 | nvn-21, nvn-22, nvn-39, nvn-104, nvn-108, nvn-117, nvn-141, nvn-144, nvn-153, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-174, nvn-175, nvn-192, nvn-197, nvn-201, nvn2-1, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29, nvn2-34 |
| `nvnTextureBuilderGetStride` | 55, 231 | nvn-140 |
| `nvnTextureBuilderGetTarget` | 55, 231 | nvn-115 |
| `nvnTextureBuilderGetWidth` | 55, 231 | nvn-115 |
| `nvnTextureBuilderGetZCullStorageSize` | 55, 231 | nvn-141 |
| `nvnTextureBuilderSetDefaults` | 55, 231 | nvn-21, nvn-22, nvn-39, nvn-104, nvn-108, nvn-115, nvn-117, nvn-140, nvn-141, nvn-144, nvn-153, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-174, nvn-175, nvn-192, nvn-197, nvn-201, nvn2-1, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29, nvn2-34 |
| `nvnTextureBuilderSetDepth` | 55, 231 | nvn-115, nvn-117, nvn-144 |
| `nvnTextureBuilderSetDepthStencilMode` | 55, 231 | nvn-140 |
| `nvnTextureBuilderSetDevice` | 55, 231 | nvn-21, nvn-22, nvn-39, nvn-104, nvn-108, nvn-115, nvn-117, nvn-140, nvn-141, nvn-144, nvn-153, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-174, nvn-175, nvn-192, nvn-197, nvn-201, nvn2-1, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29, nvn2-34 |
| `nvnTextureBuilderSetFlags` | 55, 231 | nvn-104, nvn-108, nvn-115, nvn-117, nvn-141, nvn-144, nvn-153, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201, nvn2-1, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29, nvn2-34 |
| `nvnTextureBuilderSetFormat` | 55, 231 | nvn-21, nvn-22, nvn-39, nvn-104, nvn-108, nvn-115, nvn-117, nvn-141, nvn-144, nvn-153, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-174, nvn-175, nvn-192, nvn-197, nvn-201, nvn2-1, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29, nvn2-34 |
| `nvnTextureBuilderSetHeight` | 55, 231 | nvn-115 |
| `nvnTextureBuilderSetLevels` | 55, 231 | nvn-115, nvn-117 |
| `nvnTextureBuilderSetPackagedTextureData` | 55, 231 | nvn-140 |
| `nvnTextureBuilderSetPackagedTextureLayout` | 55, 231 | nvn-140 |
| `nvnTextureBuilderSetSamples` | 55, 231 | nvn-21, nvn-22, nvn-39, nvn-115, nvn-117, nvn-174, nvn-175, nvn-192 |
| `nvnTextureBuilderSetSize1D` | 55, 231 | nvn-144 |
| `nvnTextureBuilderSetSize2D` | 55, 231 | nvn-21, nvn-22, nvn-39, nvn-104, nvn-108, nvn-115, nvn-117, nvn-141, nvn-144, nvn-153, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-174, nvn-175, nvn-192, nvn-197, nvn-201, nvn2-1, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29, nvn2-34 |
| `nvnTextureBuilderSetSize3D` | 55, 231 | nvn-144 |
| `nvnTextureBuilderSetStorage` | 55, 231 | nvn-21, nvn-22, nvn-39, nvn-104, nvn-108, nvn-115, nvn-117, nvn-141, nvn-144, nvn-153, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-174, nvn-175, nvn-192, nvn-197, nvn-201, nvn2-1, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29, nvn2-34 |
| `nvnTextureBuilderSetStride` | 55, 231 | nvn-140 |
| `nvnTextureBuilderSetSwizzle` | 55, 231 | nvn-140, nvn-141 |
| `nvnTextureBuilderSetTarget` | 55, 231 | nvn-21, nvn-22, nvn-39, nvn-104, nvn-108, nvn-115, nvn-117, nvn-141, nvn-144, nvn-153, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-174, nvn-175, nvn-192, nvn-197, nvn-201, nvn2-1, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29, nvn2-34 |
| `nvnTextureBuilderSetWidth` | 55, 231 | nvn-115 |
| `nvnTextureCompare` | 55, 231 | nvn-141 |
| `nvnTextureFinalize` | 55, 231 | nvn-21, nvn-22, nvn-39, nvn-141, nvn-144, nvn-153, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-174, nvn-175, nvn-192, nvn-197, nvn-201 |
| `nvnTextureFlushTexels` | 55, 231 | nvn-142 |
| `nvnTextureGetDepth` | 55, 231 | nvn-117 |
| `nvnTextureGetDepthStencilMode` | 55, 231 | nvn-141 |
| `nvnTextureGetFlags` | 55, 231 | nvn-117 |
| `nvnTextureGetFormat` | 55, 231 | nvn-117 |
| `nvnTextureGetHeight` | 55, 231 | nvn-117 |
| `nvnTextureGetLevels` | 55, 231 | nvn-117 |
| `nvnTextureGetMemoryOffset` | 55, 231 | nvn-117 |
| `nvnTextureGetMemoryPool` | 55, 231 | nvn-117 |
| `nvnTextureGetSamples` | 55, 231 | nvn-117 |
| `nvnTextureGetStorageClass` | 55, 231 | nvn-141 |
| `nvnTextureGetStorageSize` | 55, 231 | nvn-141 |
| `nvnTextureGetStride` | 55, 231 | nvn-141 |
| `nvnTextureGetSwizzle` | 55, 231 | nvn-141 |
| `nvnTextureGetTarget` | 55, 231 | nvn-117 |
| `nvnTextureGetTextureAddress` | 55, 231 | nvn-141 |
| `nvnTextureGetViewOffset` | 55, 231 | nvn-141 |
| `nvnTextureGetWidth` | 55, 231 | nvn-117, nvn-150 |
| `nvnTextureGetZCullStorageSize` | 55, 231 | nvn-141 |
| `nvnTextureInitialize` | 55, 231 | nvn-21, nvn-22, nvn-39, nvn-104, nvn-108, nvn-117, nvn-141, nvn-144, nvn-153, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-174, nvn-175, nvn-192, nvn-197, nvn-201, nvn2-1, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29, nvn2-34 |
| `nvnTexturePoolFinalize` | 55, 231 | nvn-132, nvn-150, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnTexturePoolGetMemoryOffset` | 55, 231 | nvn-132 |
| `nvnTexturePoolGetMemoryPool` | 55, 231 | nvn-132 |
| `nvnTexturePoolGetSize` | 55, 231 | nvn-132, nvn-150 |
| `nvnTexturePoolInitialize` | 55, 231 | nvn-132, nvn-150, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29 |
| `nvnTexturePoolRegisterImage` | 231 | nvn2-24, nvn2-25, nvn2-26 |
| `nvnTexturePoolRegisterTexture` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201, nvn2-27, nvn2-28, nvn2-29 |
| `nvnTextureReadTexels` | 55, 231 | nvn-142 |
| `nvnTextureViewGetDepthStencilMode` | 55, 231 | nvn-140 |
| `nvnTextureViewGetFormat` | 55, 231 | nvn-116 |
| `nvnTextureViewGetLayers` | 55, 231 | nvn-116 |
| `nvnTextureViewGetLevels` | 55, 231 | nvn-116 |
| `nvnTextureViewGetSwizzle` | 55, 231 | nvn-140 |
| `nvnTextureViewGetTarget` | 55, 231 | nvn-116 |
| `nvnTextureViewSetDefaults` | 55, 231 | nvn-116, nvn-118, nvn-140, nvn-192 |
| `nvnTextureViewSetDepthStencilMode` | 55, 231 | nvn-140 |
| `nvnTextureViewSetFormat` | 55, 231 | nvn-116 |
| `nvnTextureViewSetLayers` | 55, 231 | nvn-116 |
| `nvnTextureViewSetLevels` | 55, 231 | nvn-116, nvn-118 |
| `nvnTextureViewSetSwizzle` | 55, 231 | nvn-140 |
| `nvnTextureViewSetTarget` | 55, 231 | nvn-116 |
| `nvnTextureWriteTexels` | 55, 231 | nvn-142 |
| `nvnTextureWriteTexelsStrided` | 55, 231 | nvn-143 |

## Samplers

45 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnSamplerBuilderGetBorderColor` | 55, 231 | nvn-130, nvn-131 |
| `nvnSamplerBuilderGetBorderColori` | 55, 231 | nvn-130 |
| `nvnSamplerBuilderGetBorderColorui` | 55, 231 | nvn-130 |
| `nvnSamplerBuilderGetCompare` | 55, 231 | nvn-130, nvn-131 |
| `nvnSamplerBuilderGetDevice` | 55, 231 | nvn-130 |
| `nvnSamplerBuilderGetLodBias` | 55, 231 | nvn-130, nvn-131 |
| `nvnSamplerBuilderGetLodClamp` | 55, 231 | nvn-130, nvn-131 |
| `nvnSamplerBuilderGetMaxAnisotropy` | 55, 231 | nvn-130, nvn-131 |
| `nvnSamplerBuilderGetMinMagFilter` | 55, 231 | nvn-130, nvn-131 |
| `nvnSamplerBuilderGetReductionFilter` | 55, 231 | nvn-130, nvn-131 |
| `nvnSamplerBuilderGetWrapMode` | 55, 231 | nvn-130, nvn-131 |
| `nvnSamplerBuilderSetBorderColori` | 55, 231 | nvn-130 |
| `nvnSamplerBuilderSetBorderColorui` | 55, 231 | nvn-130 |
| `nvnSamplerGetBorderColori` | 55, 231 | nvn-131 |
| `nvnSamplerGetBorderColorui` | 55, 231 | nvn-131 |
| `nvnSamplerPoolRegisterSamplerBuilder` | 55, 231 | nvn-132 |
| `nvnSamplerBuilderSetBorderColor` | 55, 231 | nvn-130, nvn-131, nvn-132, nvn-150, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnSamplerBuilderSetCompare` | 55, 231 | nvn-130, nvn-131, nvn-132, nvn-150 |
| `nvnSamplerBuilderSetDefaults` | 55, 231 | nvn-130, nvn-131, nvn-132, nvn-150, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201, nvn2-27, nvn2-28, nvn2-29 |
| `nvnSamplerBuilderSetDevice` | 55, 231 | nvn-130, nvn-131, nvn-132, nvn-150, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201, nvn2-27, nvn2-28, nvn2-29 |
| `nvnSamplerBuilderSetLodBias` | 55, 231 | nvn-130, nvn-131, nvn-132, nvn-150 |
| `nvnSamplerBuilderSetLodClamp` | 55, 231 | nvn-130, nvn-131, nvn-132, nvn-150 |
| `nvnSamplerBuilderGetLodSnap` | 55, 231 | nvn-130 |
| `nvnSamplerBuilderSetLodSnap` | 55, 231 | nvn-130, nvn-131, nvn-132, nvn-150 |
| `nvnSamplerBuilderSetMaxAnisotropy` | 55, 231 | nvn-130, nvn-131, nvn-132, nvn-150 |
| `nvnSamplerBuilderSetMinMagFilter` | 55, 231 | nvn-130, nvn-131, nvn-132, nvn-150, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201, nvn2-27, nvn2-28, nvn2-29 |
| `nvnSamplerBuilderSetReductionFilter` | 55, 231 | nvn-130, nvn-131, nvn-132, nvn-150 |
| `nvnSamplerBuilderSetWrapMode` | 55, 231 | nvn-130, nvn-131, nvn-132, nvn-150, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201, nvn2-27, nvn2-28, nvn2-29 |
| `nvnSamplerCompare` | 55, 231 | nvn-131 |
| `nvnSamplerFinalize` | 55, 231 | nvn-131, nvn-132, nvn-150, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnSamplerGetBorderColor` | 55, 231 | nvn-131 |
| `nvnSamplerGetCompare` | 55, 231 | nvn-131 |
| `nvnSamplerGetLodBias` | 55, 231 | nvn-131 |
| `nvnSamplerGetLodClamp` | 55, 231 | nvn-131 |
| `nvnSamplerGetMaxAnisotropy` | 55, 231 | nvn-131 |
| `nvnSamplerGetMinMagFilter` | 55, 231 | nvn-131 |
| `nvnSamplerGetReductionFilter` | 55, 231 | nvn-131 |
| `nvnSamplerGetWrapMode` | 55, 231 | nvn-131 |
| `nvnSamplerInitialize` | 55, 231 | nvn-131, nvn-132, nvn-150, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201, nvn2-27, nvn2-28, nvn2-29 |
| `nvnSamplerPoolFinalize` | 55, 231 | nvn-132, nvn-150, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnSamplerPoolGetMemoryOffset` | 55, 231 | nvn-132 |
| `nvnSamplerPoolGetMemoryPool` | 55, 231 | nvn-132 |
| `nvnSamplerPoolGetSize` | 55, 231 | nvn-132, nvn-150 |
| `nvnSamplerPoolInitialize` | 55, 231 | nvn-132, nvn-150, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29 |
| `nvnSamplerPoolRegisterSampler` | 55, 231 | nvn-132, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201, nvn2-27, nvn2-28, nvn2-29 |

## Programs

20 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnProgramGetSubtileSize` | 55, 231 | nvn-190 |
| `nvnProgramGetScratchBytesPerWarp` | 231 | nvn-206 |
| `nvnProgramSetSampleShading` | 55, 231 | nvn-190 |
| `nvnProgramSetSubroutineLinkage` | 55, 231 | nvn-151, nvn-191 |
| `nvnProgramFinalize` | 55, 231 | nvn-37, nvn-38, nvn-44, nvn-47, nvn-48, nvn-150, nvn-151, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-190, nvn-191, nvn-197, nvn-200, nvn-201, nvn-206, nvn-207 |
| `nvnProgramInitialize` | 55, 231 | nvn-150, nvn-151, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-190, nvn-191, nvn-197, nvn-200, nvn-201, nvn-206, nvn-207 |
| `nvnProgramSetShaders` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-190, nvn-191, nvn-197, nvn-201 |
| `nvnProgramSetShadersExt` | 55, 231 | nvn-200 |
| `nvnProgramBuilderSetDefaults` | 231 | nvn-203, nvn-204, nvn-205, nvn-208, nvn2-2, nvn2-8, nvn2-18, nvn2-19, nvn2-21, nvn2-24, nvn2-27 |
| `nvnProgramBuilderSetDevice` | 231 | nvn-203, nvn-208, nvn2-2, nvn2-8, nvn2-18, nvn2-19, nvn2-21, nvn2-24, nvn2-27 |
| `nvnProgramBuilderGetDevice` | 231 | nvn-203, nvn-208 |
| `nvnProgramBuilderSetSampleShading` | 231 | nvn-203, nvn-208 |
| `nvnProgramBuilderGetSampleShading` | 231 | nvn-203, nvn-208 |
| `nvnProgramBuilderSetShaders` | 231 | nvn-203, nvn-208, nvn2-2, nvn2-8, nvn2-18, nvn2-19, nvn2-21, nvn2-24, nvn2-27 |
| `nvnProgramBuilderGetShaders` | 231 | nvn-203, nvn-208 |
| `nvnProgramBuilderGetShaderCount` | 231 | nvn-203, nvn-208 |
| `nvnProgramBuilderSetSubroutineLinkage` | 231 | nvn-204 |
| `nvnProgramBuilderGetSubroutineLinkage` | 231 | nvn-204 |
| `nvnProgramBuilderGetSubroutineLinkageCount` | 231 | nvn-204 |
| `nvnProgramInitializeFromBuilder` | 231 | nvn2-2, nvn2-8, nvn2-18, nvn2-19, nvn2-21, nvn2-24, nvn2-27 |

## State objects

82 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnBlendStateGetAdvancedMode` | 55, 231 | nvn-135 |
| `nvnBlendStateGetAdvancedNormalizedDst` | 55, 231 | nvn-135 |
| `nvnBlendStateGetAdvancedOverlap` | 55, 231 | nvn-135 |
| `nvnBlendStateGetAdvancedPremultipliedSrc` | 55, 231 | nvn-135 |
| `nvnBlendStateSetAdvancedMode` | 55, 231 | nvn-135 |
| `nvnBlendStateSetAdvancedNormalizedDst` | 55, 231 | nvn-135 |
| `nvnBlendStateSetAdvancedOverlap` | 55, 231 | nvn-135 |
| `nvnBlendStateSetAdvancedPremultipliedSrc` | 55, 231 | nvn-135 |
| `nvnMultisampleStateGetSampleLocationsGrid` | 55, 231 | nvn-138 |
| `nvnMultisampleStateSetSampleLocations` | 55, 231 | nvn-138 |
| `nvnBlendStateSetBlendEquation` | 55, 231 | nvn-135 |
| `nvnBlendStateSetBlendFunc` | 55, 231 | nvn-135, nvn-186 |
| `nvnBlendStateSetBlendTarget` | 55, 231 | nvn-135, nvn-186 |
| `nvnBlendStateSetDefaults` | 55, 231 | nvn-135, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-186, nvn-197, nvn-201, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23 |
| `nvnChannelMaskStateSetChannelMask` | 55, 231 | nvn-136 |
| `nvnChannelMaskStateSetDefaults` | 55, 231 | nvn-136, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23, nvn2-34 |
| `nvnColorStateGetBlendEnable` | 55, 231 | nvn-136 |
| `nvnColorStateSetAlphaTest` | 55, 231 | nvn-136, nvn-186 |
| `nvnColorStateSetBlendEnable` | 55, 231 | nvn-136, nvn-186 |
| `nvnColorStateSetDefaults` | 55, 231 | nvn-136, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-186, nvn-197, nvn-201, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23 |
| `nvnColorStateSetLogicOp` | 55, 231 | nvn-136 |
| `nvnDepthStencilStateSetDefaults` | 55, 231 | nvn-137, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-187, nvn-197, nvn-201, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23 |
| `nvnDepthStencilStateSetDepthFunc` | 55, 231 | nvn-137, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23 |
| `nvnDepthStencilStateSetDepthTestEnable` | 55, 231 | nvn-137, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-187, nvn-197, nvn-201, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23 |
| `nvnDepthStencilStateSetDepthWriteEnable` | 55, 231 | nvn-137, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-187, nvn-197, nvn-201, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23 |
| `nvnDepthStencilStateSetStencilFunc` | 55, 231 | nvn-137, nvn-187 |
| `nvnDepthStencilStateSetStencilOp` | 55, 231 | nvn-137, nvn-187 |
| `nvnDepthStencilStateSetStencilTestEnable` | 55, 231 | nvn-137, nvn-187 |
| `nvnMultisampleStateSetAlphaToCoverageEnable` | 55, 231 | nvn-138 |
| `nvnMultisampleStateSetDefaults` | 55, 231 | nvn-138, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-192, nvn-197, nvn-201, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23 |
| `nvnMultisampleStateSetMultisampleEnable` | 55, 231 | nvn-138, nvn-192 |
| `nvnMultisampleStateSetSamples` | 55, 231 | nvn-138, nvn-192 |
| `nvnPolygonStateSetCullFace` | 55, 231 | nvn-139, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-188, nvn-197, nvn-201, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23 |
| `nvnPolygonStateSetDefaults` | 55, 231 | nvn-139, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-188, nvn-197, nvn-201, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23 |
| `nvnPolygonStateSetFrontFace` | 55, 231 | nvn-139, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnPolygonStateSetPolygonMode` | 55, 231 | nvn-139 |
| `nvnPolygonStateSetPolygonOffsetEnables` | 55, 231 | nvn-139, nvn-188 |
| `nvnVertexAttribStateSetDefaults` | 55, 231 | nvn-139, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnVertexAttribStateSetFormat` | 55, 231 | nvn-139, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnVertexAttribStateSetStreamIndex` | 55, 231 | nvn-139, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnVertexStreamStateSetDefaults` | 55, 231 | nvn-139, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnVertexStreamStateSetDivisor` | 55, 231 | nvn-139, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnVertexStreamStateSetStride` | 55, 231 | nvn-139, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnBlendStateGetBlendEquation` | 55, 231 | nvn-135 |
| `nvnBlendStateGetBlendFunc` | 55, 231 | nvn-135 |
| `nvnBlendStateGetBlendTarget` | 55, 231 | nvn-135 |
| `nvnChannelMaskStateGetChannelMask` | 55, 231 | nvn-136 |
| `nvnColorStateGetAlphaTest` | 55, 231 | nvn-136 |
| `nvnColorStateGetLogicOp` | 55, 231 | nvn-136 |
| `nvnDepthStencilStateGetDepthFunc` | 55, 231 | nvn-137 |
| `nvnDepthStencilStateGetDepthTestEnable` | 55, 231 | nvn-137 |
| `nvnDepthStencilStateGetDepthWriteEnable` | 55, 231 | nvn-137 |
| `nvnDepthStencilStateGetStencilFunc` | 55, 231 | nvn-137 |
| `nvnDepthStencilStateGetStencilOp` | 55, 231 | nvn-137 |
| `nvnDepthStencilStateGetStencilTestEnable` | 55, 231 | nvn-137 |
| `nvnMultisampleStateGetAlphaToCoverageDither` | 55, 231 | nvn-138 |
| `nvnMultisampleStateGetAlphaToCoverageEnable` | 55, 231 | nvn-138 |
| `nvnMultisampleStateGetCoverageModulationMode` | 55, 231 | nvn-138 |
| `nvnMultisampleStateGetCoverageToColorEnable` | 55, 231 | nvn-138 |
| `nvnMultisampleStateGetCoverageToColorOutput` | 55, 231 | nvn-138 |
| `nvnMultisampleStateGetMultisampleEnable` | 55, 231 | nvn-138 |
| `nvnMultisampleStateGetRasterSamples` | 55, 231 | nvn-138 |
| `nvnMultisampleStateGetSampleLocationsEnable` | 55, 231 | nvn-138 |
| `nvnMultisampleStateGetSampleLocationsGridEnable` | 55, 231 | nvn-138 |
| `nvnMultisampleStateGetSamples` | 55, 231 | nvn-138 |
| `nvnMultisampleStateSetAlphaToCoverageDither` | 55, 231 | nvn-138 |
| `nvnMultisampleStateSetCoverageModulationMode` | 55, 231 | nvn-138 |
| `nvnMultisampleStateSetCoverageToColorEnable` | 55, 231 | nvn-138 |
| `nvnMultisampleStateSetCoverageToColorOutput` | 55, 231 | nvn-138 |
| `nvnMultisampleStateSetRasterSamples` | 55, 231 | nvn-138 |
| `nvnMultisampleStateSetCoverageTruncationEnable` | 55, 231 | nvn-138 |
| `nvnMultisampleStateGetCoverageTruncationEnable` | 55, 231 | nvn-138 |
| `nvnMultisampleStateSetSampleLocationsEnable` | 55, 231 | nvn-138 |
| `nvnMultisampleStateSetSampleLocationsGridEnable` | 55, 231 | nvn-138 |
| `nvnPolygonStateGetCullFace` | 55, 231 | nvn-139 |
| `nvnPolygonStateGetFrontFace` | 55, 231 | nvn-139 |
| `nvnPolygonStateGetPolygonMode` | 55, 231 | nvn-139 |
| `nvnPolygonStateGetPolygonOffsetEnables` | 55, 231 | nvn-139 |
| `nvnVertexAttribStateGetFormat` | 55, 231 | nvn-139 |
| `nvnVertexAttribStateGetStreamIndex` | 55, 231 | nvn-139 |
| `nvnVertexStreamStateGetDivisor` | 55, 231 | nvn-139 |
| `nvnVertexStreamStateGetStride` | 55, 231 | nvn-139 |

## Queues

30 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnQueueBuilderGetCommandFlushThreshold` | 55, 231 | nvn-128 |
| `nvnQueueBuilderGetCommandMemorySize` | 55, 231 | nvn-128 |
| `nvnQueueBuilderGetComputeMemorySize` | 55, 231 | nvn-128 |
| `nvnQueueBuilderGetControlMemorySize` | 55, 231 | nvn-128 |
| `nvnQueueBuilderGetDevice` | 55, 231 | nvn-128 |
| `nvnQueueBuilderGetFlags` | 55, 231 | nvn-128 |
| `nvnQueueBuilderGetMemory` | 55, 231 | nvn-145 |
| `nvnQueueBuilderGetMemorySize` | 55, 231 | nvn-145 |
| `nvnQueueGetTotalCommandMemoryUsed` | 55, 231 | nvn-145 |
| `nvnQueueGetTotalComputeMemoryUsed` | 55, 231 | nvn-145 |
| `nvnQueueGetTotalControlMemoryUsed` | 55, 231 | nvn-145 |
| `nvnQueueResetMemoryUsageCounts` | 55, 231 | nvn-145, nvn-210 |
| `nvnQueueBuilderGetQueueMemorySize` | 55, 231 | nvn-145 |
| `nvnQueueBuilderSetCommandFlushThreshold` | 55, 231 | nvn-128 |
| `nvnQueueBuilderSetCommandMemorySize` | 55, 231 | nvn-128 |
| `nvnQueueBuilderSetComputeMemorySize` | 55, 231 | nvn-128 |
| `nvnQueueBuilderSetControlMemorySize` | 55, 231 | nvn-128 |
| `nvnQueueBuilderSetDefaults` | 55, 231 | nvn-104, nvn-128, nvn-145, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-209, nvn2-1 |
| `nvnQueueBuilderSetDevice` | 55, 231 | nvn-104, nvn-128, nvn-145, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-209, nvn2-1 |
| `nvnQueueBuilderSetFlags` | 55, 231 | nvn-128 |
| `nvnQueueBuilderGetQueuePriority` | 55, 231 | nvn-145 |
| `nvnQueueBuilderSetQueuePriority` | 55, 231 | nvn-145 |
| `nvnQueueBuilderSetQueueMemory` | 55, 231 | nvn-145 |
| `nvnQueueFinalize` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnQueueFinish` | 55, 231 | nvn-105, nvn-106, nvn-107, nvn-109, nvn-110, nvn-111, nvn-112, nvn-113, nvn-114, nvn-118, nvn-120, nvn-121, nvn-122, nvn-123, nvn-127, nvn-134, nvn-142, nvn-143, nvn-144, nvn-145, nvn-146, nvn-147, nvn-148, nvn-149, nvn-153, nvn-154, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201, nvn-202, nvn-210, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29, nvn2-34 |
| `nvnQueueFlush` | 55, 231 | nvn-121, nvn-122, nvn-123, nvn-148 |
| `nvnQueueGetError` | 55, 231 | nvn-127, nvn-150 |
| `nvnQueueInitialize` | 55, 231 | nvn-104, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn2-1 |
| `nvnQueueSubmitCommands` | 55, 231 | nvn-105, nvn-106, nvn-107, nvn-109, nvn-110, nvn-111, nvn-112, nvn-113, nvn-114, nvn-118, nvn-120, nvn-121, nvn-122, nvn-123, nvn-134, nvn-142, nvn-144, nvn-146, nvn-147, nvn-148, nvn-149, nvn-153, nvn-154, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201, nvn-202, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29, nvn2-34 |
| `nvnQueueBuilderSetPrimaryQueue` | 55, 231 | nvn-145 |

## Sync and events

24 functions, 2 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnSyncCreateGLSync` | 55, 231 | nvn-125 |
| `nvnSyncInitializeFromFencedGLSync` | 55, 231 | nvn-125 |
| `nvnSyncFinalize` | 55, 231 | nvn-121, nvn-122, nvn-123, nvn-124, nvn-125, nvn-148, nvn-150, nvn-155 |
| `nvnSyncInitialize` | 55, 231 | nvn-121, nvn-122, nvn-123, nvn-124, nvn-125, nvn-148, nvn-150, nvn-155 |
| `nvnSyncWait` | 55, 231 | nvn-121, nvn-122, nvn-123, nvn-124, nvn-148, nvn-155 |
| `nvnSyncGetNvRmSyncNVX` |  |  |
| `nvnSyncInitializeFromNvRmSyncNVX` |  |  |
| `nvnEventBuilderGetMemoryOffset` | 55, 231 | nvn-129 |
| `nvnEventBuilderGetMemoryPool` | 55, 231 | nvn-129 |
| `nvnEventBuilderGetStorage` | 55, 231 | nvn-129 |
| `nvnEventGetMemoryOffset` | 55, 231 | nvn-129 |
| `nvnEventGetMemoryPool` | 55, 231 | nvn-129 |
| `nvnEventBuilderSetDefaults` | 55, 231 | nvn-129, nvn-146, nvn-147, nvn-148 |
| `nvnEventBuilderSetStorage` | 55, 231 | nvn-129, nvn-146, nvn-147, nvn-148 |
| `nvnEventFinalize` | 55, 231 | nvn-129, nvn-146, nvn-147, nvn-148 |
| `nvnEventGetValue` | 55, 231 | nvn-129, nvn-146, nvn-147 |
| `nvnEventInitialize` | 55, 231 | nvn-129, nvn-146, nvn-147, nvn-148 |
| `nvnEventSignal` | 55, 231 | nvn-129, nvn-146, nvn-147, nvn-148 |
| `nvnQueueFenceSync` | 55, 231 | nvn-121, nvn-123, nvn-148 |
| `nvnQueueWaitSync` | 55, 231 | nvn-121, nvn-122, nvn-123 |
| `nvnCommandBufferFenceSync` | 55, 231 | nvn-122 |
| `nvnCommandBufferSignalEvent` | 55, 231 | nvn-146, nvn-147 |
| `nvnCommandBufferWaitEvent` | 55, 231 | nvn-148 |
| `nvnCommandBufferWaitSync` | 55, 231 | nvn-123 |

## Windows and present

35 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnQueueAcquireTexture` | 55, 231 | nvn-153, nvn-154 |
| `nvnQueuePresentTexture` | 55, 231 | nvn-153, nvn-154, nvn-155 |
| `nvnWindowBuilderGetDevice` | 55, 231 | nvn-152 |
| `nvnWindowBuilderGetNativeWindow` | 55, 231 | nvn-152 |
| `nvnWindowBuilderGetNumActiveTextures` | 55, 231 | nvn-152 |
| `nvnWindowGetNativeWindow` | 55, 231 | nvn-154 |
| `nvnWindowGetNumActiveTextures` | 55, 231 | nvn-154 |
| `nvnWindowGetNumTextures` | 55, 231 | nvn-154 |
| `nvnWindowSetNumActiveTextures` | 55, 231 | nvn-154 |
| `nvnWindowBuilderGetNumTextures` | 55, 231 | nvn-152 |
| `nvnWindowBuilderGetPresentInterval` | 55, 231 | nvn-152 |
| `nvnWindowBuilderGetTexture` | 55, 231 | nvn-152 |
| `nvnWindowGetCrop` | 55, 231 | nvn-154 |
| `nvnWindowGetPresentInterval` | 55, 231 | nvn-154 |
| `nvnWindowAcquireTexture` | 55, 231 | nvn-155 |
| `nvnWindowBuilderSetDefaults` | 55, 231 | nvn-152, nvn-153, nvn-211 |
| `nvnWindowBuilderSetDevice` | 55, 231 | nvn-152, nvn-153 |
| `nvnWindowBuilderSetNativeWindow` | 55, 231 | nvn-152, nvn-153 |
| `nvnWindowBuilderSetNumActiveTextures` | 55, 231 | nvn-152 |
| `nvnWindowBuilderSetPresentInterval` | 55, 231 | nvn-152 |
| `nvnWindowBuilderSetTextures` | 55, 231 | nvn-152, nvn-153 |
| `nvnWindowBuilderSetColorSpace` | 231 | nvn-211 |
| `nvnWindowBuilderGetColorSpace` | 231 | nvn-211 |
| `nvnWindowBuilderSetHdrDisplayMetadata` | 231 | nvn-211 |
| `nvnWindowBuilderGetHdrDisplayMetadata` | 231 | nvn-211 |
| `nvnWindowBuilderSetHdrContentMetadata` | 231 | nvn-211 |
| `nvnWindowBuilderGetHdrContentMetadata` | 231 | nvn-211 |
| `nvnWindowSetHdrDisplayMetadata` | 231 | nvn-212 |
| `nvnWindowGetHdrDisplayMetadata` | 231 | nvn-212 |
| `nvnWindowSetHdrContentMetadata` | 231 | nvn-212 |
| `nvnWindowGetHdrContentMetadata` | 231 | nvn-212 |
| `nvnWindowFinalize` | 55, 231 | nvn-153 |
| `nvnWindowInitialize` | 55, 231 | nvn-153 |
| `nvnWindowSetCrop` | 55, 231 | nvn-154 |
| `nvnWindowSetPresentInterval` | 55, 231 | nvn-154 |

## Counters and queries

6 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnDeviceGetTimestampInNanoseconds` | 55, 231 | nvn-15, nvn-16, nvn-168, nvn-169 |
| `nvnCommandBufferReportCounter` | 55, 231 | nvn-166, nvn-168, nvn-169, nvn-170 |
| `nvnCommandBufferReportValue` | 55, 231 | nvn-167 |
| `nvnCommandBufferResetCounter` | 55, 231 | nvn-166, nvn-170 |
| `nvnCommandBufferSetRenderEnable` | 55, 231 | nvn-170 |
| `nvnCommandBufferSetRenderEnableConditional` | 55, 231 | nvn-170 |

## Commands: memory and recording

25 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferGetCommandMemoryFree` | 55, 231 | nvn-171 |
| `nvnCommandBufferGetCommandMemorySize` | 55, 231 | nvn-171 |
| `nvnCommandBufferGetControlMemoryFree` | 55, 231 | nvn-171 |
| `nvnCommandBufferGetControlMemorySize` | 55, 231 | nvn-171 |
| `nvnCommandBufferSetControlMemoryCallbackEnabled` | 55, 231 | nvn-172 |
| `nvnCommandBufferAddCommandMemory` | 55, 231 | nvn-104, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-171, nvn-172, nvn2-1 |
| `nvnCommandBufferAddControlMemory` | 55, 231 | nvn-104, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-171, nvn2-1 |
| `nvnCommandBufferBarrier` | 55, 231 | nvn-105, nvn-106, nvn-107, nvn-109, nvn-110, nvn-112, nvn-113, nvn-114, nvn-118, nvn-121, nvn-122, nvn-123, nvn-142, nvn-144, nvn-148, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-174, nvn-175, nvn-192, nvn-197, nvn-201, nvn-202, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29, nvn2-34 |
| `nvnCommandBufferBeginRecording` | 55, 231 | nvn-105, nvn-106, nvn-107, nvn-109, nvn-110, nvn-111, nvn-112, nvn-113, nvn-114, nvn-118, nvn-120, nvn-121, nvn-122, nvn-123, nvn-134, nvn-142, nvn-144, nvn-146, nvn-147, nvn-148, nvn-149, nvn-153, nvn-154, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201, nvn-202, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29, nvn2-34 |
| `nvnCommandBufferSetRecordingMode` | 231 | nvn2-20 |
| `nvnCommandBufferSetComputeOverlapMode` | 231 | nvn2-20 |
| `nvnCommandBufferCallCommands` | 55, 231 | nvn-173 |
| `nvnCommandBufferCopyCommands` | 55, 231 | nvn-173 |
| `nvnCommandBufferEndRecording` | 55, 231 | nvn-105, nvn-106, nvn-107, nvn-109, nvn-110, nvn-111, nvn-112, nvn-113, nvn-114, nvn-118, nvn-120, nvn-121, nvn-122, nvn-123, nvn-134, nvn-142, nvn-144, nvn-146, nvn-147, nvn-148, nvn-149, nvn-153, nvn-154, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-173, nvn-197, nvn-201, nvn-202, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29, nvn2-34 |
| `nvnCommandBufferFinalize` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferGetCommandMemoryUsed` | 55, 231 | nvn-171 |
| `nvnCommandBufferGetControlMemoryUsed` | 55, 231 | nvn-171 |
| `nvnCommandBufferGetMemoryCallback` | 55, 231 | nvn-172 |
| `nvnCommandBufferGetMemoryCallbackData` | 55, 231 | nvn-172 |
| `nvnCommandBufferInitialize` | 55, 231 | nvn-104, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn2-1 |
| `nvnCommandBufferIsRecording` | 55, 231 | nvn-171 |
| `nvnCommandBufferSetCommandMemoryCallbackEnabled` | 55, 231 | nvn-172 |
| `nvnCommandBufferSetComputeWorkloadPriority` | 231 | nvn2-20 |
| `nvnCommandBufferSetMemoryCallback` | 55, 231 | nvn-172 |
| `nvnCommandBufferSetMemoryCallbackData` | 55, 231 | nvn-172 |

## Commands: binds

41 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferBindCoverageModulationTable` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferBindImagesDeferred` | 231 | nvn2-26 |
| `nvnCommandBufferBindSeparateSamplersDeferred` | 231 | nvn2-29 |
| `nvnCommandBufferBindSeparateTexturesDeferred` | 231 | nvn2-29 |
| `nvnCommandBufferBindStorageBuffersDeferred` | 231 | nvn2-6 |
| `nvnCommandBufferBindUniformBuffersDeferred` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferSetProgramSubroutines` | 55, 231 | nvn-38, nvn-191 |
| `nvnCommandBufferBindShadingRateImage` | 231 | nvn2-22 |
| `nvnCommandBufferShadingRateImagePalette` | 231 | nvn2-23 |
| `nvnCommandBufferPushGraphicsConstants` | 231 | nvn2-21 |
| `nvnCommandBufferPushComputeConstants` | 231 | nvn2-19 |
| `nvnCommandBufferBindBlendState` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-186, nvn-197, nvn-201, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23 |
| `nvnCommandBufferBindChannelMaskState` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23, nvn2-34 |
| `nvnCommandBufferBindColorState` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-186, nvn-197, nvn-201, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23 |
| `nvnCommandBufferBindDepthStencilState` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-187, nvn-197, nvn-201, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23 |
| `nvnCommandBufferBindImage` | 231 | nvn2-24 |
| `nvnCommandBufferBindImages` | 231 | nvn2-25 |
| `nvnCommandBufferBindMultisampleState` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-192, nvn-197, nvn-201, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23 |
| `nvnCommandBufferBindPolygonState` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-188, nvn-197, nvn-201, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23 |
| `nvnCommandBufferBindProgram` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-190, nvn-191, nvn-197, nvn-200, nvn-201, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29 |
| `nvnCommandBufferBindSeparateSampler` | 231 | nvn2-27 |
| `nvnCommandBufferBindSeparateSamplers` | 231 | nvn2-28 |
| `nvnCommandBufferBindSeparateTexture` | 231 | nvn2-27 |
| `nvnCommandBufferBindSeparateTextures` | 231 | nvn2-28 |
| `nvnCommandBufferBindStorageBuffer` | 231 | nvn2-3, nvn2-4, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-27, nvn2-28, nvn2-29 |
| `nvnCommandBufferBindStorageBuffers` | 231 | nvn2-5 |
| `nvnCommandBufferBindTexture` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferBindTextures` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferBindTexturesDeferred` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferBindTransformFeedbackBuffer` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferBindTransformFeedbackBuffers` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferBindUniformBuffer` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferBindUniformBuffers` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferBindVertexAttribState` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferBindVertexBuffer` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferBindVertexBuffers` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferBindVertexStreamState` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferSetSamplerPool` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201, nvn2-27, nvn2-28, nvn2-29 |
| `nvnCommandBufferSetShaderScratchMemory` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferSetTexturePool` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29 |
| `nvnCommandBufferUpdateUniformBuffer` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |

## Commands: draws and dispatches

26 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferDrawTexture` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferBeginTransformFeedback` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferSetGeometryProcessingMode` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23 |
| `nvnCommandBufferDrawMeshTasks` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23 |
| `nvnCommandBufferDrawMeshTasks3D` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23 |
| `nvnCommandBufferDrawMeshTasksIndirect` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23 |
| `nvnCommandBufferDrawMeshTasks3DIndirect` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23 |
| `nvnCommandBufferMultiDrawMeshTasksIndirect` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23 |
| `nvnCommandBufferMultiDrawMeshTasks3DIndirect` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23 |
| `nvnCommandBufferMultiDrawMeshTasksIndirectCount` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23 |
| `nvnCommandBufferMultiDrawMeshTasks3DIndirectCount` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23 |
| `nvnCommandBufferDispatchCompute` | 231 | nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29 |
| `nvnCommandBufferDispatchComputeIndirect` | 231 | nvn2-7 |
| `nvnCommandBufferDrawArrays` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-182, nvn-196, nvn-197, nvn-201 |
| `nvnCommandBufferDrawArraysIndirect` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferDrawArraysInstanced` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferDrawElements` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-183, nvn-197, nvn-201 |
| `nvnCommandBufferDrawElementsBaseVertex` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferDrawElementsIndirect` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferDrawElementsInstanced` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferDrawTransformFeedback` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferEndTransformFeedback` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferMultiDrawArraysIndirectCount` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferMultiDrawElementsIndirectCount` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferPauseTransformFeedback` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |
| `nvnCommandBufferResumeTransformFeedback` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201 |

## Commands: clears and copies

25 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferClearTexturei` | 55, 231 | nvn-175 |
| `nvnCommandBufferGetCopyImageStride` | 55, 231 | nvn-177 |
| `nvnCommandBufferGetCopyRowStride` | 55, 231 | nvn-177 |
| `nvnCommandBufferResolveDepthBuffer` | 55, 231 | nvn-193 |
| `nvnCommandBufferTiledDownsample` | 55, 231 | nvn-192 |
| `nvnCommandBufferTiledDownsampleTextureView` | 55, 231 | nvn-192 |
| `nvnCommandBufferClearBuffer` | 55, 231 | nvn-176 |
| `nvnCommandBufferClearColor` | 55, 231 | nvn-105, nvn-106, nvn-107, nvn-121, nvn-122, nvn-123, nvn-148, nvn-153, nvn-154, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-192, nvn-197, nvn-201, nvn-202, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23, nvn2-34 |
| `nvnCommandBufferClearColori` | 55, 231 | nvn-174 |
| `nvnCommandBufferClearColorui` | 55, 231 | nvn-174 |
| `nvnCommandBufferClearDepthStencil` | 55, 231 | nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-187, nvn-197, nvn-201, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23, nvn2-34 |
| `nvnCommandBufferClearTexture` | 55, 231 | nvn-175 |
| `nvnCommandBufferClearTextureui` | 55, 231 | nvn-175 |
| `nvnCommandBufferCopyBufferToBuffer` | 55, 231 | nvn-111, nvn-120, nvn-134, nvn-149, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-27, nvn2-28, nvn2-29 |
| `nvnCommandBufferCopyBufferToTexture` | 55, 231 | nvn-109, nvn-110, nvn-112, nvn-113, nvn-114, nvn-118, nvn-144, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29, nvn2-34 |
| `nvnCommandBufferCopyTextureToBuffer` | 55, 231 | nvn-105, nvn-106, nvn-107, nvn-109, nvn-110, nvn-112, nvn-113, nvn-114, nvn-118, nvn-121, nvn-122, nvn-123, nvn-142, nvn-144, nvn-148, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-174, nvn-175, nvn-192, nvn-197, nvn-201, nvn-202, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-34 |
| `nvnCommandBufferCopyTextureToTexture` | 55, 231 | nvn-110, nvn-113 |
| `nvnCommandBufferDiscardColor` | 55, 231 | nvn-178 |
| `nvnCommandBufferDiscardDepthStencil` | 55, 231 | nvn-178 |
| `nvnCommandBufferDownsample` | 55, 231 | nvn-192 |
| `nvnCommandBufferDownsampleTextureView` | 55, 231 | nvn-192 |
| `nvnCommandBufferRestoreZCullData` | 55, 231 | nvn-193 |
| `nvnCommandBufferSaveZCullData` | 55, 231 | nvn-193 |
| `nvnCommandBufferSetCopyImageStride` | 55, 231 | nvn-177 |
| `nvnCommandBufferSetCopyRowStride` | 55, 231 | nvn-109, nvn-110, nvn-111, nvn-112, nvn-113, nvn-114, nvn-118, nvn-142, nvn-144, nvn-177 |

## Commands: render state

36 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferOverrideSubtileSize` | 55, 231 | nvn-189 |
| `nvnCommandBufferSetColorReductionEnable` | 55, 231 | nvn-189 |
| `nvnCommandBufferSetColorReductionThresholds` | 55, 231 | nvn-189 |
| `nvnCommandBufferSetConservativeRasterDilate` | 55, 231 | nvn-195 |
| `nvnCommandBufferSetInnerTessellationLevels` | 55, 231 | nvn-197 |
| `nvnCommandBufferSetOuterTessellationLevels` | 55, 231 | nvn-197 |
| `nvnCommandBufferSetProvokingVertexMode` | 55, 231 | nvn-181 |
| `nvnCommandBufferSetSubpixelPrecisionBias` | 55, 231 | nvn-194 |
| `nvnCommandBufferSetZCullZF32CompressionEnable` | 55, 231 | nvn-189 |
| `nvnCommandBufferSetShadingRatePaletteMode` | 231 | nvn2-22 |
| `nvnCommandBufferSetAlphaRef` | 55, 231 | nvn-186 |
| `nvnCommandBufferSetBlendColor` | 55, 231 | nvn-186 |
| `nvnCommandBufferSetConservativeRasterEnable` | 55, 231 | nvn-194, nvn-195 |
| `nvnCommandBufferSetDepthBounds` | 55, 231 | nvn-180 |
| `nvnCommandBufferSetDepthClamp` | 55, 231 | nvn-181 |
| `nvnCommandBufferSetDepthRange` | 55, 231 | nvn-180 |
| `nvnCommandBufferSetDepthRanges` | 55, 231 | nvn-180 |
| `nvnCommandBufferSetLineWidth` | 55, 231 | nvn-196 |
| `nvnCommandBufferSetPatchSize` | 55, 231 | nvn-197 |
| `nvnCommandBufferSetPointSize` | 55, 231 | nvn-182 |
| `nvnCommandBufferSetPolygonOffsetClamp` | 55, 231 | nvn-188 |
| `nvnCommandBufferSetPrimitiveRestart` | 55, 231 | nvn-183 |
| `nvnCommandBufferSetRasterizerDiscard` | 55, 231 | nvn-181 |
| `nvnCommandBufferSetRenderTargets` | 55, 231 | nvn-105, nvn-106, nvn-107, nvn-121, nvn-122, nvn-123, nvn-148, nvn-153, nvn-154, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-174, nvn-192, nvn-197, nvn-201, nvn-202, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23, nvn2-34 |
| `nvnCommandBufferSetSampleMask` | 55, 231 | nvn-188 |
| `nvnCommandBufferSetScissor` | 55, 231 | nvn-105, nvn-106, nvn-107, nvn-121, nvn-122, nvn-123, nvn-148, nvn-153, nvn-154, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-174, nvn-192, nvn-197, nvn-201, nvn-202, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23, nvn2-34 |
| `nvnCommandBufferSetScissors` | 55, 231 | nvn-179 |
| `nvnCommandBufferSetStencilCullCriteria` | 55, 231 | nvn-189 |
| `nvnCommandBufferSetStencilMask` | 55, 231 | nvn-187 |
| `nvnCommandBufferSetStencilRef` | 55, 231 | nvn-187 |
| `nvnCommandBufferSetStencilValueMask` | 55, 231 | nvn-187 |
| `nvnCommandBufferSetTiledCacheAction` | 55, 231 | nvn-185 |
| `nvnCommandBufferSetTiledCacheTileSize` | 55, 231 | nvn-185 |
| `nvnCommandBufferSetViewport` | 55, 231 | nvn-105, nvn-106, nvn-107, nvn-121, nvn-122, nvn-123, nvn-148, nvn-153, nvn-154, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-165, nvn-197, nvn-201, nvn-202, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23, nvn2-34 |
| `nvnCommandBufferSetViewportSwizzles` | 55, 231 | nvn-184 |
| `nvnCommandBufferSetViewports` | 55, 231 | nvn-179 |

## DLSS

58 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnDlssNetworkBuilderSetDefaults` | 231 | nvn2-31, nvn2-33, nvn2-34 |
| `nvnDlssNetworkBuilderSetDevice` | 231 | nvn2-31, nvn2-33, nvn2-34 |
| `nvnDlssNetworkBuilderGetDevice` | 231 | nvn2-31 |
| `nvnDlssNetworkBuilderSetFlags` | 231 | nvn2-31 |
| `nvnDlssNetworkBuilderGetFlags` | 231 | nvn2-31 |
| `nvnDlssNetworkBuilderSetMethod` | 231 | nvn2-31 |
| `nvnDlssNetworkBuilderGetMethod` | 231 | nvn2-31 |
| `nvnDlssNetworkBuilderSetPreset` | 231 | nvn2-31 |
| `nvnDlssNetworkBuilderGetPreset` | 231 | nvn2-31 |
| `nvnDlssNetworkBuilderSetOutputResolution` | 231 | nvn2-31, nvn2-33, nvn2-34 |
| `nvnDlssNetworkBuilderGetOutputResolution` | 231 | nvn2-31 |
| `nvnDlssNetworkBuilderSetMaxEvaluationEntries` | 231 | nvn2-31 |
| `nvnDlssNetworkBuilderGetMaxEvaluationEntries` | 231 | nvn2-31 |
| `nvnDlssNetworkBuilderSetStorage` | 231 | nvn2-31, nvn2-33, nvn2-34 |
| `nvnDlssNetworkBuilderGetMemory` | 231 | nvn2-31 |
| `nvnDlssNetworkBuilderGetMemorySize` | 231 | nvn2-31 |
| `nvnDlssNetworkBuilderSetTextureIDs` | 231 | nvn2-31 |
| `nvnDlssNetworkBuilderGetTexturePool` | 231 | nvn2-31 |
| `nvnDlssNetworkBuilderGetTextureIDs` | 231 | nvn2-31 |
| `nvnDlssNetworkBuilderGetOptimalSettings` | 231 | nvn2-33 |
| `nvnDlssNetworkBuilderGetStorageSize` | 231 | nvn2-33, nvn2-34 |
| `nvnDlssNetworkInitialize` | 231 | nvn2-33, nvn2-34 |
| `nvnDlssNetworkFinalize` | 231 | nvn2-33, nvn2-34 |
| `nvnDlssNetworkResize` | 231 | nvn2-33 |
| `nvnCommandBufferEvaluateDlss` | 231 | nvn2-34 |
| `nvnDlssParametersSetDefaults` | 231 | nvn2-32, nvn2-34 |
| `nvnDlssParametersSetInputColor` | 231 | nvn2-32, nvn2-34 |
| `nvnDlssParametersSetOutputColor` | 231 | nvn2-32, nvn2-34 |
| `nvnDlssParametersSetMotionVectors` | 231 | nvn2-32, nvn2-34 |
| `nvnDlssParametersSetDepth` | 231 | nvn2-32, nvn2-34 |
| `nvnDlssParametersSetBiasCurrentColorMask` | 231 | nvn2-32 |
| `nvnDlssParametersSetExposureTexture` | 231 | nvn2-32 |
| `nvnDlssParametersSetJitterOffset` | 231 | nvn2-32 |
| `nvnDlssParametersGetJitterOffset` | 231 | nvn2-32 |
| `nvnDlssParametersSetMotionVectorScale` | 231 | nvn2-32 |
| `nvnDlssParametersGetMotionVectorScale` | 231 | nvn2-32 |
| `nvnDlssParametersSetPreExposure` | 231 | nvn2-32 |
| `nvnDlssParametersGetPreExposure` | 231 | nvn2-32 |
| `nvnDlssParametersSetHistoryReset` | 231 | nvn2-32, nvn2-34 |
| `nvnDlssParametersGetHistoryReset` | 231 | nvn2-32 |
| `nvnDlssParametersSetEvaluationIndex` | 231 | nvn2-32 |
| `nvnDlssParametersGetEvaluationIndex` | 231 | nvn2-32 |
| `nvnDlssParametersSetInputSubrectangleSize` | 231 | nvn2-32, nvn2-34 |
| `nvnDlssParametersGetInputSubrectangleSize` | 231 | nvn2-32 |
| `nvnDlssParametersSetInputColorSubrectangleBase` | 231 | nvn2-32 |
| `nvnDlssParametersGetInputColorSubrectangleBase` | 231 | nvn2-32 |
| `nvnDlssParametersSetDepthSubrectangleBase` | 231 | nvn2-32 |
| `nvnDlssParametersGetDepthSubrectangleBase` | 231 | nvn2-32 |
| `nvnDlssParametersSetMotionVectorSubrectangleBase` | 231 | nvn2-32 |
| `nvnDlssParametersGetMotionVectorSubrectangleBase` | 231 | nvn2-32 |
| `nvnDlssParametersSetBiasCurrentColorSubrectangleBase` | 231 | nvn2-32 |
| `nvnDlssParametersGetBiasCurrentColorSubrectangleBase` | 231 | nvn2-32 |
| `nvnDlssParametersSetOutputSubrectangleBase` | 231 | nvn2-32 |
| `nvnDlssParametersGetOutputSubrectangleBase` | 231 | nvn2-32 |
| `nvnDlssParametersSetOverlayDebugLayer` | 231 | nvn2-32 |
| `nvnDlssParametersSetOverlayFullScreen` | 231 | nvn2-32 |
| `nvnDlssParametersSetOverlayShowNans` | 231 | nvn2-32 |
| `nvnDlssParametersSetOverlayJitterDebug` | 231 | nvn2-32 |

## Ray tracing

15 functions, 4 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnDeviceCalculateAccelerationStructureMemoryRequirements` |  |  |
| `nvnProgramGetShaderGroupHandle` | 231 | nvn-207 |
| `nvnProgramBuilderSetRayTracingModules` | 231 | nvn-208 |
| `nvnProgramBuilderSetMaxCallableRecursionDepth` | 231 | nvn-203, nvn-208 |
| `nvnProgramBuilderGetMaxCallableRecursionDepth` | 231 | nvn-203, nvn-208 |
| `nvnProgramBuilderSetMaxTraceRecursionDepth` | 231 | nvn-203, nvn-208 |
| `nvnProgramBuilderGetMaxTraceRecursionDepth` | 231 | nvn-203, nvn-208 |
| `nvnProgramBuilderSetProgramRayTracingFlags` | 231 | nvn-203, nvn-208 |
| `nvnProgramBuilderGetProgramRayTracingFlags` | 231 | nvn-203, nvn-208 |
| `nvnQueueBuilderGetRayTracingMemorySize` | 231 | nvn-209 |
| `nvnQueueBuilderSetRayTracingMemorySize` | 231 | nvn-209 |
| `nvnQueueGetTotalRayTracingMemoryUsed` | 231 | nvn-210 |
| `nvnCommandBufferBindAccelerationStructure` |  |  |
| `nvnCommandBufferBuildAccelerationStructures` |  |  |
| `nvnCommandBufferCompactAccelerationStructure` |  |  |

## Debug

44 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnSetExternalDebuggerEnabledNVX` | 55, 231 | nvn-150 |
| `nvnDeviceGenerateDebugDomainId` | 55, 231 | nvn-150, nvn-198, nvn-202 |
| `nvnDeviceInstallDebugCallback` | 55, 231 | nvn-150 |
| `nvnDeviceIsExternalDebuggerAttached` | 55, 231 | nvn-150 |
| `nvnDeviceSetAutomaticCheckpointsByDebugGroup` | 55, 231 | nvn-150 |
| `nvnDeviceSetAutomaticCheckpointsEnable` | 55, 231 | nvn-150 |
| `nvnDeviceSetAutomaticCheckpointsMask` | 55, 231 | nvn-150 |
| `nvnDeviceSetAutomaticCheckpointsSamplingInterval` | 55, 231 | nvn-150 |
| `nvnDeviceSetCheckpointCallStacksEnable` | 55, 231 | nvn-150 |
| `nvnDeviceSetCheckpointMaxCallStacksDepth` | 55, 231 | nvn-150 |
| `nvnDeviceWalkDebugDatabase` | 55, 231 | nvn-150 |
| `nvnMemoryPoolSetDebugLabel` | 55, 231 | nvn-150 |
| `nvnBufferGetDebugID` | 55, 231 | nvn-150 |
| `nvnBufferSetDebugLabel` | 55, 231 | nvn-150 |
| `nvnTextureGetDebugID` | 55, 231 | nvn-150 |
| `nvnTextureSetDebugLabel` | 55, 231 | nvn-150 |
| `nvnSamplerGetDebugID` | 55, 231 | nvn-150 |
| `nvnSamplerSetDebugLabel` | 55, 231 | nvn-150 |
| `nvnProgramBuilderSetDebugDataHashes` | 231 | nvn-205 |
| `nvnProgramBuilderGetDebugDataHashes` | 231 | nvn-205 |
| `nvnCommandBufferInsertCheckpointDynamic` | 55, 231 | nvn-198 |
| `nvnCommandBufferInsertCheckpointStatic` | 55, 231 | nvn-198 |
| `nvnCommandBufferInsertDebugMarkerGraphics` | 231 | nvn-202 |
| `nvnCommandBufferInsertDebugMarkerCompute` | 231 | nvn-202 |
| `nvnCommandBufferPushDebugGroupGraphics` | 231 | nvn-202 |
| `nvnCommandBufferPushDebugGroupCompute` | 231 | nvn-202 |
| `nvnCommandBufferPopDebugGroupGraphics` | 231 | nvn-202 |
| `nvnCommandBufferPopDebugGroupCompute` | 231 | nvn-202 |
| `nvnCommandBufferInsertDebugMarker` | 55, 231 | nvn-198 |
| `nvnCommandBufferInsertDebugMarkerDynamic` | 55, 231 | nvn-198 |
| `nvnCommandBufferInsertDebugMarkerStatic` | 55, 231 | nvn-198 |
| `nvnCommandBufferPopDebugGroup` | 55, 231 | nvn-198 |
| `nvnCommandBufferPopDebugGroupId` | 55, 231 | nvn-198 |
| `nvnCommandBufferPushDebugGroup` | 55, 231 | nvn-198 |
| `nvnCommandBufferPushDebugGroupDynamic` | 55, 231 | nvn-198 |
| `nvnCommandBufferPushDebugGroupStatic` | 55, 231 | nvn-198 |
| `nvnCommandBufferSetDebugLabel` | 55, 231 | nvn-150 |
| `nvnDeviceSetDebugLabel` | 55, 231 | nvn-150 |
| `nvnProgramSetDebugLabel` | 55, 231 | nvn-150 |
| `nvnQueueSetDebugLabel` | 55, 231 | nvn-150 |
| `nvnSamplerPoolSetDebugLabel` | 55, 231 | nvn-150 |
| `nvnSyncSetDebugLabel` | 55, 231 | nvn-150 |
| `nvnTexturePoolSetDebugLabel` | 55, 231 | nvn-150 |
| `nvnWindowSetDebugLabel` | 55, 231 | nvn-154 |
