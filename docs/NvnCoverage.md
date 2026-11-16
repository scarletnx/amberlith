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
names 677; the layer has 527 of them in its API 55 build and 662 in its API 231 build; a case calls 662. The
other 15 have no call at all: a runtime could answer any of them wrongly and every case would still pass.

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
| [Commands: binds](#commands-binds) | 41 | 24 | 39 | 39 | 2 |
| [Commands: draws and dispatches](#commands-draws-and-dispatches) | 26 | 15 | 26 | 26 | 0 |
| [Commands: clears and copies](#commands-clears-and-copies) | 25 | 25 | 25 | 25 | 0 |
| [Commands: render state](#commands-render-state) | 36 | 35 | 36 | 36 | 0 |
| [DLSS](#dlss) | 58 | 0 | 58 | 58 | 0 |
| [Ray tracing](#ray-tracing) | 15 | 0 | 6 | 6 | 9 |
| [Debug](#debug) | 44 | 36 | 42 | 42 | 2 |
| **All** | **677** | **527** | **662** | **662** | **15** |

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
| nvn-38 | nvn | a cleared target of four samples resolves into its colour by each of the four downsamples, through the layer's own NVN functions | the NRO |
| nvn-39 | nvn | depth tested draws around a depth target's resolve and its culling data's save and restore are as without them, through the layer's own NVN functions | the NRO |
| nvn-40 | nvn | conservative rasterization draws every pixel a quad touches, through the layer's own NVN functions | the NRO |
| nvn-41 | nvn | a conservative quad dilated by half a pixel reaches the pixels a third of one away, through the layer's own NVN functions | the NRO |
| nvn-42 | nvn | a line of width four is four rows of pixels, through the layer's own NVN functions | the NRO |
| nvn-43 | nvn | patches of the size and the outer levels a list sets fill their cells, through the layer's own NVN functions | the NRO |
| nvn-44 | nvn | draws between debug markers, groups and checkpoints are as without them, through the layer's own NVN functions | the NRO |
| nvn-45 | nvn | the device says its modes and its builder its flags, and clear values register, through the layer's own NVN functions | the NRO |
| nvn-46 | nvn | a program set from the longer shader data draws, through the layer's own NVN functions | the NRO |
| nvn-47 | nvn | a texture's handle and a sampler's put together sample, and a texel fetch handle fetches, through the layer's own NVN functions | the NRO |
| nvn-48 | nvn | the layer's own: the program's NVN functions resolve | the NRO |
| nvn-49 | nvn | the layer's own: the device initializes | the NRO |
| nvn-50 | nvn | the layer's own: the device answers the layer's API version | the NRO |
| nvn-51 | nvn | the layer's own: a queue, its pools and a 64x64 target come up | the NRO |
| nvn-52 | nvn | the layer's own: clear to opaque red | the NRO |
| nvn-53 | nvn | the layer's own: clear to a mid channel value | the NRO |
| nvn-54 | nvn | the layer's own: clears before and after deferred finalizes, the shader cache switch and a list's handle given up are there | the NRO |
| nvn-55 | nvn | the layer's own: two copy textures and a staging pool come up | the NRO |
| nvn-56 | nvn | the layer's own: a picture copied into a texture and back keeps every texel | the NRO |
| nvn-57 | nvn | the layer's own: a texture copied into another keeps every texel | the NRO |
| nvn-58 | nvn | the layer's own: a buffer copied into another keeps every byte | the NRO |
| nvn-59 | nvn | the layer's own: a texture copied to a buffer with MIRROR_Y lands upside down | the NRO |
| nvn-60 | nvn | the layer's own: a texture copied into another with MIRROR_X lands mirrored | the NRO |
| nvn-61 | nvn | the layer's own: a copy to a buffer with a row stride of 288 keeps the bytes between rows | the NRO |
| nvn-62 | nvn | the layer's own: a texture builder gives back each number it was set | the NRO |
| nvn-63 | nvn | the layer's own: a texture view says which of its parts were set, and with what | the NRO |
| nvn-64 | nvn | the layer's own: a texture of three levels comes up and says what its builder had | the NRO |
| nvn-65 | nvn | the layer's own: a picture copied into each level of a texture through a view comes back from that level | the NRO |
| nvn-66 | nvn | the layer's own: a memory pool and its builder give back what they were set, and the pool maps its storage | the NRO |
| nvn-67 | nvn | the layer's own: a page flushed from a cached pool is what a copy reads, and the copy is there after an invalidate | the NRO |
| nvn-68 | nvn | the layer's own: a sync fenced on the queue behind a clear signals, and the clear is there with no finish | the NRO |
| nvn-69 | nvn | the layer's own: a sync fenced inside the list behind a clear signals, and the clear is there with no finish | the NRO |
| nvn-70 | nvn | the layer's own: a queue and a list told to wait for a sync that signalled run their clear | the NRO |
| nvn-71 | nvn | the layer's own: a wait on a sync nothing fenced fails, with a timeout or without | the NRO |
| nvn-72 | nvn | the layer's own: with no OpenGL, a sync from an OpenGL sync and an OpenGL sync from a sync both fail | the NRO |
| nvn-73 | nvn | the layer's own: a pool builder given a native handle forgets its storage, and takes storage again | the NRO |
| nvn-74 | nvn | the layer's own: the queue has no error after work that was valid | the NRO |
| nvn-75 | nvn | the layer's own: a queue builder gives back its flags and says which of its sizes were set | the NRO |
| nvn-76 | nvn | the layer's own: an event keeps what the processor signals: a write, an add and an add that wraps | the NRO |
| nvn-77 | nvn | the layer's own: a sampler builder gives back each number it was set | the NRO |
| nvn-78 | nvn | the layer's own: a sampler says what its builder had, and is the same as another of that state alone | the NRO |
| nvn-79 | nvn | the layer's own: the texture and sampler pools say where they are, and a builder registers as its sampler | the NRO |
| nvn-80 | nvn | the layer's own: a buffer is a range of its pool: its address, its mapping, and what it and its builder say | the NRO |
| nvn-81 | nvn | the layer's own: bytes flushed from one buffer and copied to another are there after an invalidate | the NRO |
| nvn-82 | nvn | the layer's own: a blend state gives back its target, functions, equations and advanced mode | the NRO |
| nvn-83 | nvn | the layer's own: a channel mask and a colour state give back channels, blending, the logic operation and the alpha test | the NRO |
| nvn-84 | nvn | the layer's own: a depth and stencil state gives back its depth test and each face's stencil | the NRO |
| nvn-85 | nvn | the layer's own: a multisample state gives back its samples, its switches and its sample locations' grid | the NRO |
| nvn-86 | nvn | the layer's own: a polygon state, a vertex attribute and a vertex stream give back what they were set | the NRO |
| nvn-87 | nvn | the layer's own: a texture builder and a view give back their swizzle, depth and stencil mode and the rest | the NRO |
| nvn-88 | nvn | the layer's own: a texture says its swizzle, storage and address, and is the same as its twin alone | the NRO |
| nvn-89 | nvn | the layer's own: texels the processor writes into a texture are what it and the graphics unit read back | the NRO |
| nvn-90 | nvn | the layer's own: a part of a texture written and read with row strides keeps the bytes between rows | the NRO |
| nvn-91 | nvn | the layer's own: a 1D texture, each layer of an array and each slice of a 3D texture keep a picture of their own | the NRO |
| nvn-92 | nvn | the layer's own: a queue builder gives back its memory and priority, and a queue's memory counts are nought once reset | the NRO |
| nvn-93 | nvn | the layer's own: a list writes an event's value over the one the processor signalled | the NRO |
| nvn-94 | nvn | the layer's own: a list adds to an event's value | the NRO |
| nvn-95 | nvn | the layer's own: a list that waits for an event's value runs when the processor signals it, and not before | the NRO |
| nvn-96 | nvn | the layer's own: a virtual pool's page is the physical pool's page mapped into it | the NRO |
| nvn-97 | nvn | the layer's own: the debug layer's calls are answered as a release driver answers them, and a label changes no object | the NRO |
| nvn-98 | nvn | the layer's own: a program with no subroutine takes no linkage map, one offered or none | the NRO |
| nvn-99 | nvn | the layer's own: a window builder gives back what it was set | the NRO |
| nvn-100 | nvn | the program's NVN functions resolve | the NRO, the Switch 2 program |
| nvn-101 | nvn | the device initializes | the NRO, the Switch 2 program |
| nvn-102 | nvn | the device answers the layer's API version | the NRO, the Switch 2 program |
| nvn-103 | nvn | a queue, its pools and a 64x64 target come up | the NRO, the Switch 2 program |
| nvn-104 | nvn | clear to opaque red | the NRO, the Switch 2 program |
| nvn-105 | nvn | clear to a mid channel value | the NRO, the Switch 2 program |
| nvn-106 | nvn | clears before and after deferred finalizes, the shader cache switch and a list's handle given up are there | the NRO, the Switch 2 program |
| nvn-107 | nvn | two copy textures and a staging pool come up | the NRO, the Switch 2 program |
| nvn-108 | nvn | a picture copied into a texture and back keeps every texel | the NRO, the Switch 2 program |
| nvn-109 | nvn | a texture copied into another keeps every texel | the NRO, the Switch 2 program |
| nvn-110 | nvn | a buffer copied into another keeps every byte | the NRO, the Switch 2 program |
| nvn-111 | nvn | a texture copied to a buffer with MIRROR_Y lands upside down | the NRO, the Switch 2 program |
| nvn-112 | nvn | a texture copied into another with MIRROR_X lands mirrored | the NRO, the Switch 2 program |
| nvn-113 | nvn | a copy to a buffer with a row stride of 288 keeps the bytes between rows | the NRO, the Switch 2 program |
| nvn-114 | nvn | a texture builder gives back each number it was set | the NRO, the Switch 2 program |
| nvn-115 | nvn | a texture view says which of its parts were set, and with what | the NRO, the Switch 2 program |
| nvn-116 | nvn | a texture of three levels comes up and says what its builder had | the NRO, the Switch 2 program |
| nvn-117 | nvn | a picture copied into each level of a texture through a view comes back from that level | the NRO, the Switch 2 program |
| nvn-118 | nvn | a memory pool and its builder give back what they were set, and the pool maps its storage | the NRO, the Switch 2 program |
| nvn-119 | nvn | a page flushed from a cached pool is what a copy reads, and the copy is there after an invalidate | the NRO, the Switch 2 program |
| nvn-120 | nvn | a sync fenced on the queue behind a clear signals, and the clear is there with no finish | the NRO, the Switch 2 program |
| nvn-121 | nvn | a sync fenced inside the list behind a clear signals, and the clear is there with no finish | the NRO, the Switch 2 program |
| nvn-122 | nvn | a queue and a list told to wait for a sync that signalled run their clear | the NRO, the Switch 2 program |
| nvn-123 | nvn | a wait on a sync nothing fenced fails, with a timeout or without | the NRO, the Switch 2 program |
| nvn-124 | nvn | with no OpenGL, a sync from an OpenGL sync and an OpenGL sync from a sync both fail | the NRO, the Switch 2 program |
| nvn-125 | nvn | a pool builder given a native handle forgets its storage, and takes storage again | the NRO, the Switch 2 program |
| nvn-126 | nvn | the queue has no error after work that was valid | the NRO, the Switch 2 program |
| nvn-127 | nvn | a queue builder gives back its flags and says which of its sizes were set | the NRO, the Switch 2 program |
| nvn-128 | nvn | an event keeps what the processor signals: a write, an add and an add that wraps | the NRO, the Switch 2 program |
| nvn-129 | nvn | a sampler builder gives back each number it was set | the NRO, the Switch 2 program |
| nvn-130 | nvn | a sampler says what its builder had, and is the same as another of that state alone | the NRO, the Switch 2 program |
| nvn-131 | nvn | the texture and sampler pools say where they are, and a builder registers as its sampler | the NRO, the Switch 2 program |
| nvn-132 | nvn | a buffer is a range of its pool: its address, its mapping, and what it and its builder say | the NRO, the Switch 2 program |
| nvn-133 | nvn | bytes flushed from one buffer and copied to another are there after an invalidate | the NRO, the Switch 2 program |
| nvn-134 | nvn | a blend state gives back its target, functions, equations and advanced mode | the NRO, the Switch 2 program |
| nvn-135 | nvn | a channel mask and a colour state give back channels, blending, the logic operation and the alpha test | the NRO, the Switch 2 program |
| nvn-136 | nvn | a depth and stencil state gives back its depth test and each face's stencil | the NRO, the Switch 2 program |
| nvn-137 | nvn | a multisample state gives back its samples, its switches and its sample locations' grid | the NRO, the Switch 2 program |
| nvn-138 | nvn | a polygon state, a vertex attribute and a vertex stream give back what they were set | the NRO, the Switch 2 program |
| nvn-139 | nvn | a texture builder and a view give back their swizzle, depth and stencil mode and the rest | the NRO, the Switch 2 program |
| nvn-140 | nvn | a texture says its swizzle, storage and address, and is the same as its twin alone | the NRO, the Switch 2 program |
| nvn-141 | nvn | texels the processor writes into a texture are what it and the graphics unit read back | the NRO, the Switch 2 program |
| nvn-142 | nvn | a part of a texture written and read with row strides keeps the bytes between rows | the NRO, the Switch 2 program |
| nvn-143 | nvn | a 1D texture, each layer of an array and each slice of a 3D texture keep a picture of their own | the NRO, the Switch 2 program |
| nvn-144 | nvn | a queue builder gives back its memory and priority, and a queue's memory counts are nought once reset | the NRO, the Switch 2 program |
| nvn-145 | nvn | a list writes an event's value over the one the processor signalled | the NRO, the Switch 2 program |
| nvn-146 | nvn | a list adds to an event's value | the NRO, the Switch 2 program |
| nvn-147 | nvn | a list that waits for an event's value runs when the processor signals it, and not before | the NRO, the Switch 2 program |
| nvn-148 | nvn | a virtual pool's page is the physical pool's page mapped into it | the NRO, the Switch 2 program |
| nvn-149 | nvn | the debug layer's calls are answered as a release driver answers them, and a label changes no object | the NRO, the Switch 2 program |
| nvn-150 | nvn | a program with no subroutine takes no linkage map, one offered or none | the NRO, the Switch 2 program |
| nvn-151 | nvn | a window builder gives back what it was set | the NRO, the Switch 2 program |
| nvn-152 | nvn | a window acquires and presents both its textures | the NRO, the Switch 2 program |
| nvn-153 | nvn | a window says what it is of, and keeps the active textures, the interval and the crop it is set | the NRO, the Switch 2 program |
| nvn-154 | nvn | a texture the window hands with a sync is free once the sync signals, and presents | the NRO, the Switch 2 program |
| nvn-155 | nvn | the same scene through the NVN the program looks up holds what the processor works out | the NRO |
| nvn-156 | nvn | the same texture past its edges through the NVN the program looks up does the same | the NRO |
| nvn-157 | nvn | the same four indexed quads through the NVN the program looks up are each in its quarter | the NRO |
| nvn-158 | nvn | the scene of five draws bound through the looked up calls that take several holds what the processor works out | the NRO |
| nvn-159 | nvn | the scene of five draws bound through the looked up deferred calls holds what the processor works out | the NRO |
| nvn-160 | nvn | the same four quads of an updated uniform buffer through the NVN the program looks up are each its own colour | the NRO |
| nvn-161 | nvn | the same eight instances through the NVN the program looks up are each in its cell | the NRO |
| nvn-162 | nvn | a texture drawn with no program and as many draws as a word says, through the NVN the program looks up, are each in its place | the NRO |
| nvn-163 | nvn | what a capture took of two draws, paused for a third between them, is drawn again a row down through the NVN the program looks up | the NRO |
| nvn-164 | nvn | the same capture into a buffer bound by the call that takes several is drawn again a row down | the NRO |
| nvn-165 | nvn | the samples counter reports a quad's 256 pixels and none after a reset, through the NVN the program looks up | the NRO |
| nvn-166 | nvn | two values a list reports are in memory once it has run, through the NVN the program looks up | the NRO |
| nvn-167 | nvn | two timestamp reports hold no count and times between the device's clock before and after, through the NVN the program looks up | the NRO |
| nvn-168 | nvn | a timestamp report's word is the graphics unit's ticks, 384 every 625 nanoseconds, through the NVN the program looks up | the NRO |
| nvn-169 | nvn | the render enable draws under on and under a condition that holds and skips the rest, through the NVN the program looks up | the NRO |
| nvn-170 | nvn | a command buffer's memory used and free come to what it was given, and draws take some, through the NVN the program looks up | the NRO |
| nvn-171 | nvn | a list out of command memory asks the program's callback and draws on, through the NVN the program looks up | the NRO |
| nvn-172 | nvn | a list called from another and a list copied into it draw their cells, through the NVN the program looks up | the NRO |
| nvn-173 | nvn | clears of an unsigned and a signed integer target hold the integers given, through the NVN the program looks up | the NRO |
| nvn-174 | nvn | a clear of a texture that is no target reaches its region and its mask's channels, through the NVN the program looks up | the NRO |
| nvn-175 | nvn | a range of a buffer cleared to a word holds it, and nothing beside the range, through the NVN the program looks up | the NRO |
| nvn-176 | nvn | a command buffer gives back the copy strides it was set, through the NVN the program looks up | the NRO |
| nvn-177 | nvn | targets discarded, then cleared and drawn, hold the clear and the draw, through the NVN the program looks up | the NRO |
| nvn-178 | nvn | a scissor and a viewport set by number bound and place a quad, through the NVN the program looks up | the NRO |
| nvn-179 | nvn | depth ranges move a quad across the depth test and depth bounds test the target's depth, through the NVN the program looks up | the NRO |
| nvn-180 | nvn | the rasterizer discards, clamps depth and takes a flat colour from the provoking vertex, through the NVN the program looks up | the NRO |
| nvn-181 | nvn | a point of sixteen pixels fills its cell, through the NVN the program looks up | the NRO |
| nvn-182 | nvn | a strip with a restart index is two quads, through the NVN the program looks up | the NRO |
| nvn-183 | nvn | a viewport swizzle of x and y turns a quad about the diagonal, through the NVN the program looks up | the NRO |
| nvn-184 | nvn | draws around the tiled cache's actions are as without them, through the NVN the program looks up | the NRO |
| nvn-185 | nvn | the alpha test's reference and the blend colour a list sets decide two draws each, through the NVN the program looks up | the NRO |
| nvn-186 | nvn | the stencil's reference, compare mask and write mask a list sets decide what is drawn, through the NVN the program looks up | the NRO |
| nvn-187 | nvn | a sample mask without the first sample draws nothing, and a clamp holds a polygon offset, through the NVN the program looks up | the NRO |
| nvn-188 | nvn | draws under five hints to the graphics unit are as without them, through the NVN the program looks up | the NRO |
| nvn-189 | nvn | a program whose sample shading is its code's draws and asks for no subtile, through the NVN the program looks up | the NRO |
| nvn-190 | nvn | a cleared target of four samples resolves into its colour by each of the four downsamples, through the NVN the program looks up | the NRO |
| nvn-191 | nvn | depth tested draws around a depth target's resolve and its culling data's save and restore are as without them, through the NVN the program looks up | the NRO |
| nvn-192 | nvn | conservative rasterization draws every pixel a quad touches, through the NVN the program looks up | the NRO |
| nvn-193 | nvn | a conservative quad dilated by half a pixel reaches the pixels a third of one away, through the NVN the program looks up | the NRO |
| nvn-194 | nvn | a line of width four is four rows of pixels, through the NVN the program looks up | the NRO |
| nvn-195 | nvn | patches of the size and the outer levels a list sets fill their cells, through the NVN the program looks up | the NRO |
| nvn-196 | nvn | draws between debug markers, groups and checkpoints are as without them, through the NVN the program looks up | the NRO |
| nvn-197 | nvn | the device says its modes and its builder its flags, and clear values register, through the NVN the program looks up | the NRO |
| nvn-198 | nvn | a program set from the longer shader data draws, through the NVN the program looks up | the NRO |
| nvn-199 | nvn | a texture's handle and a sampler's put together sample, and a texel fetch handle fetches, through the NVN the program looks up | the NRO |
| nvn-200 | nvn | a clear between the Ounce's graphics and compute markers and groups is the clear | the Switch 2 program |
| nvn-201 | nvn | an Ounce program builder gives back what it was set, a ray tracing program's numbers too | the Switch 2 program |
| nvn-202 | nvn | an Ounce program builder gives back the subroutine linkage it was set, and none before | the Switch 2 program |
| nvn-203 | nvn | an Ounce program with no ray tracing asks for no scratch memory per warp | the Switch 2 program |
| nvn-204 | nvn | an Ounce window builder starts with the driver's colour space and HDR metadata, and gives back what it is set | the Switch 2 program |
| nvn-205 | nvn | an Ounce window has its builder's HDR metadata, and gives back what it is set | the Switch 2 program |
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
| nvn2-23 | nvn2 | a dispatch loads each texel of one image and stores three times it plus its column into another | the Switch 2 program |
| nvn2-24 | nvn2 | the same images bound by the call that takes several | the Switch 2 program |
| nvn2-25 | nvn2 | the same images bound by the deferred call | the Switch 2 program |
| nvn2-26 | nvn2 | a dispatch samples a texture bound apart from its sampler, slot 1 of each, past the texture's edge | the Switch 2 program |
| nvn2-27 | nvn2 | the same texture and sampler bound by the calls that take several | the Switch 2 program |
| nvn2-28 | nvn2 | the same texture and sampler bound by the deferred calls | the Switch 2 program |
| nvn2-29 | nvn2 | a uniform buffer's handle is its address and size, and the device's two buffer builders are filled | the Switch 2 program |
| nvn2-30 | nvn2 | a DLSS network builder gives back what it was set | the Switch 2 program |
| nvn2-31 | nvn2 | the parameters of a DLSS evaluation give back what they were set | the Switch 2 program |
| nvn2-32 | nvn2 | a DLSS builder's render sizes are in order within its output, and a network is resized within it alone | the Switch 2 program |
| nvn2-33 | nvn2 | a DLSS evaluation of a smooth picture into four times its side is within a tenth of a plain filter | the Switch 2 program |

## Device

36 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnBootstrapLoader` | 55, 231 | nvn-100, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn2-1 |
| `nvnDeviceGetProcAddress` | 55, 231 | nvn-100, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn2-1 |
| `nvnDeviceApplyDeferredFinalizes` | 55, 231 | nvn-106 |
| `nvnDeviceBuilderGetFlags` | 55, 231 | nvn-197 |
| `nvnDeviceFinalize` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnDeviceRegisterFastClearColori` | 55, 231 | nvn-197 |
| `nvnDeviceRegisterFastClearColorui` | 55, 231 | nvn-197 |
| `nvnDeviceSetIntermediateShaderCache` | 55, 231 | nvn-106 |
| `nvnDeviceBuilderSetDefaults` | 55, 231 | nvn-101, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-197, nvn2-1 |
| `nvnDeviceBuilderSetFlags` | 55, 231 | nvn-101, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-197, nvn2-1 |
| `nvnDeviceGetImageHandle` | 231 | nvn2-23, nvn2-24, nvn2-25 |
| `nvnDeviceFinalizeCommandHandle` | 55, 231 | nvn-106 |
| `nvnDeviceGetCurrentTimestampInNanoseconds` | 55, 231 | nvn-15, nvn-16, nvn-167, nvn-168 |
| `nvnDeviceGetDepthMode` | 55, 231 | nvn-197 |
| `nvnDeviceGetInteger` | 55, 231 | nvn-102, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn2-1, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29 |
| `nvnDeviceGetWindowOriginMode` | 55, 231 | nvn-197 |
| `nvnDeviceGetSeparateSamplerHandle` | 55, 231 | nvn-199, nvn2-26, nvn2-27, nvn2-28 |
| `nvnDeviceGetSeparateTextureHandle` | 55, 231 | nvn-199, nvn2-26, nvn2-27, nvn2-28 |
| `nvnDeviceGetTexelFetchHandle` | 55, 231 | nvn-199 |
| `nvnDeviceGetTextureHandle` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnDeviceInitialize` | 55, 231 | nvn-101, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn2-1 |
| `nvnDeviceRegisterFastClearColor` | 55, 231 | nvn-197 |
| `nvnDeviceRegisterFastClearDepth` | 55, 231 | nvn-197 |
| `nvnDeviceRegisterFastClearStencil` | 55, 231 | nvn-197 |
| `nvnDeviceSetDepthMode` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164 |
| `nvnDeviceSetWindowOriginMode` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164 |
| `nvnDeviceGetUniformBufferHandle` | 231 | nvn2-29 |
| `nvnDeviceCommandBufferSetDefaults` | 231 | nvn2-29 |
| `nvnDeviceCommandBufferSetDevice` | 231 | nvn2-29 |
| `nvnDeviceCommandBufferSetStorage` | 231 | nvn2-29 |
| `nvnDeviceCommandBufferSetCommandMemorySize` | 231 | nvn2-29 |
| `nvnDeviceCommandBufferSetComputeMemorySize` | 231 | nvn2-29 |
| `nvnDeviceMessageBufferSetDefaults` | 231 | nvn2-29 |
| `nvnDeviceMessageBufferSetDevice` | 231 | nvn2-29 |
| `nvnDeviceMessageBufferSetStorage` | 231 | nvn2-29 |
| `nvnDeviceMessageBufferEndMessages` | 231 | nvn2-29 |

## Memory pools

18 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnMemoryPoolBuilderGetDevice` | 55, 231 | nvn-118 |
| `nvnMemoryPoolBuilderGetFlags` | 55, 231 | nvn-118 |
| `nvnMemoryPoolBuilderGetMemory` | 55, 231 | nvn-118, nvn-125 |
| `nvnMemoryPoolBuilderGetSize` | 55, 231 | nvn-118, nvn-125 |
| `nvnMemoryPoolBuilderSetDefaults` | 55, 231 | nvn-21, nvn-22, nvn-38, nvn-103, nvn-107, nvn-116, nvn-118, nvn-125, nvn-143, nvn-148, nvn-152, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-173, nvn-174, nvn-190, nvn2-1, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28 |
| `nvnMemoryPoolBuilderSetDevice` | 55, 231 | nvn-21, nvn-22, nvn-38, nvn-103, nvn-107, nvn-116, nvn-118, nvn-125, nvn-143, nvn-148, nvn-152, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-173, nvn-174, nvn-190, nvn2-1, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28 |
| `nvnMemoryPoolBuilderSetFlags` | 55, 231 | nvn-21, nvn-22, nvn-38, nvn-103, nvn-107, nvn-116, nvn-118, nvn-143, nvn-148, nvn-152, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-173, nvn-174, nvn-190, nvn2-1, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28 |
| `nvnMemoryPoolBuilderSetStorage` | 55, 231 | nvn-21, nvn-22, nvn-38, nvn-103, nvn-107, nvn-116, nvn-118, nvn-125, nvn-143, nvn-148, nvn-152, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-173, nvn-174, nvn-190, nvn2-1, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28 |
| `nvnMemoryPoolBuilderSetNativeHandleNVX` | 55, 231 | nvn-125 |
| `nvnMemoryPoolFinalize` | 55, 231 | nvn-21, nvn-22, nvn-38, nvn-143, nvn-148, nvn-152, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-173, nvn-174, nvn-190, nvn-195, nvn-199 |
| `nvnMemoryPoolFlushMappedRange` | 55, 231 | nvn-119, nvn-133 |
| `nvnMemoryPoolGetBufferAddress` | 55, 231 | nvn-104, nvn-105, nvn-106, nvn-108, nvn-109, nvn-110, nvn-111, nvn-112, nvn-113, nvn-117, nvn-119, nvn-120, nvn-121, nvn-122, nvn-132, nvn-140, nvn-141, nvn-143, nvn-147, nvn-148, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199, nvn-200, nvn2-2, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-8, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-29, nvn2-33 |
| `nvnMemoryPoolGetFlags` | 55, 231 | nvn-118 |
| `nvnMemoryPoolGetSize` | 55, 231 | nvn-118, nvn-149 |
| `nvnMemoryPoolInitialize` | 55, 231 | nvn-21, nvn-22, nvn-38, nvn-103, nvn-107, nvn-116, nvn-118, nvn-143, nvn-148, nvn-152, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-173, nvn-174, nvn-190, nvn2-1, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28 |
| `nvnMemoryPoolInvalidateMappedRange` | 55, 231 | nvn-119 |
| `nvnMemoryPoolMap` | 55, 231 | nvn-104, nvn-105, nvn-106, nvn-107, nvn-108, nvn-109, nvn-110, nvn-111, nvn-112, nvn-113, nvn-117, nvn-118, nvn-119, nvn-120, nvn-121, nvn-122, nvn-128, nvn-131, nvn-132, nvn-133, nvn-141, nvn-143, nvn-147, nvn-148, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199, nvn-200, nvn2-2, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-8, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-33 |
| `nvnMemoryPoolMapVirtual` | 55, 231 | nvn-148 |

## Buffers

16 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnBufferBuilderGetDevice` | 55, 231 | nvn-132 |
| `nvnBufferBuilderGetMemoryOffset` | 55, 231 | nvn-132 |
| `nvnBufferBuilderGetMemoryPool` | 55, 231 | nvn-132 |
| `nvnBufferBuilderGetSize` | 55, 231 | nvn-132 |
| `nvnBufferBuilderSetDefaults` | 55, 231 | nvn-132, nvn-133, nvn-149 |
| `nvnBufferBuilderSetDevice` | 55, 231 | nvn-132, nvn-133, nvn-149 |
| `nvnBufferBuilderSetStorage` | 55, 231 | nvn-132, nvn-133, nvn-149 |
| `nvnBufferFinalize` | 55, 231 | nvn-132, nvn-133, nvn-149 |
| `nvnBufferFlushMappedRange` | 55, 231 | nvn-133 |
| `nvnBufferInvalidateMappedRange` | 55, 231 | nvn-133 |
| `nvnBufferGetAddress` | 55, 231 | nvn-132, nvn-133 |
| `nvnBufferGetMemoryOffset` | 55, 231 | nvn-132 |
| `nvnBufferGetMemoryPool` | 55, 231 | nvn-132 |
| `nvnBufferGetSize` | 55, 231 | nvn-132, nvn-149 |
| `nvnBufferInitialize` | 55, 231 | nvn-132, nvn-133, nvn-149 |
| `nvnBufferMap` | 55, 231 | nvn-132, nvn-133 |

## Textures

95 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnTextureBuilderGetGLTextureName` | 55, 231 | nvn-139 |
| `nvnTextureBuilderGetMemoryOffset` | 55, 231 | nvn-114 |
| `nvnTextureBuilderGetPackagedTextureLayout` | 55, 231 | nvn-139 |
| `nvnTextureBuilderGetRawStorageClass` | 55, 231 | nvn-140 |
| `nvnTextureBuilderGetSparseTileLayout` | 55, 231 | nvn-140 |
| `nvnTextureBuilderGetViewOffset` | 55, 231 | nvn-140 |
| `nvnTextureBuilderSetGLTextureName` | 55, 231 | nvn-139 |
| `nvnTextureGetNvRmSurfaceNVX` | 55, 231 | nvn-140 |
| `nvnTextureGetRawStorageClass` | 55, 231 | nvn-140 |
| `nvnTextureGetSparseTileLayout` | 55, 231 | nvn-140 |
| `nvnTextureInvalidateTexels` | 55, 231 | nvn-141 |
| `nvnTextureReadTexelsStrided` | 55, 231 | nvn-142 |
| `nvnTextureViewCompare` | 55, 231 | nvn-115 |
| `nvnTextureBuilderGetDepthStencilMode` | 55, 231 | nvn-139 |
| `nvnTextureBuilderGetDevice` | 55, 231 | nvn-114 |
| `nvnTextureBuilderGetMemoryPool` | 55, 231 | nvn-114 |
| `nvnTextureBuilderGetPackagedTextureData` | 55, 231 | nvn-139 |
| `nvnTextureBuilderGetSwizzle` | 55, 231 | nvn-139 |
| `nvnTextureBuilderGetDepth` | 55, 231 | nvn-114 |
| `nvnTextureBuilderGetFlags` | 55, 231 | nvn-114 |
| `nvnTextureBuilderGetFormat` | 55, 231 | nvn-114 |
| `nvnTextureBuilderGetHeight` | 55, 231 | nvn-114 |
| `nvnTextureBuilderGetLevels` | 55, 231 | nvn-114 |
| `nvnTextureBuilderGetSamples` | 55, 231 | nvn-114 |
| `nvnTextureBuilderGetStorageAlignment` | 55, 231 | nvn-21, nvn-22, nvn-38, nvn-103, nvn-116, nvn-143, nvn-152, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-173, nvn-174, nvn-190, nvn-195, nvn-199, nvn2-1, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-33 |
| `nvnTextureBuilderGetStorageClass` | 55, 231 | nvn-140 |
| `nvnTextureBuilderGetStorageSize` | 55, 231 | nvn-21, nvn-22, nvn-38, nvn-103, nvn-107, nvn-116, nvn-140, nvn-143, nvn-152, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-173, nvn-174, nvn-190, nvn-195, nvn-199, nvn2-1, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-33 |
| `nvnTextureBuilderGetStride` | 55, 231 | nvn-139 |
| `nvnTextureBuilderGetTarget` | 55, 231 | nvn-114 |
| `nvnTextureBuilderGetWidth` | 55, 231 | nvn-114 |
| `nvnTextureBuilderGetZCullStorageSize` | 55, 231 | nvn-140 |
| `nvnTextureBuilderSetDefaults` | 55, 231 | nvn-21, nvn-22, nvn-38, nvn-103, nvn-107, nvn-114, nvn-116, nvn-139, nvn-140, nvn-143, nvn-152, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-173, nvn-174, nvn-190, nvn-195, nvn-199, nvn2-1, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-33 |
| `nvnTextureBuilderSetDepth` | 55, 231 | nvn-114, nvn-116, nvn-143 |
| `nvnTextureBuilderSetDepthStencilMode` | 55, 231 | nvn-139 |
| `nvnTextureBuilderSetDevice` | 55, 231 | nvn-21, nvn-22, nvn-38, nvn-103, nvn-107, nvn-114, nvn-116, nvn-139, nvn-140, nvn-143, nvn-152, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-173, nvn-174, nvn-190, nvn-195, nvn-199, nvn2-1, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-33 |
| `nvnTextureBuilderSetFlags` | 55, 231 | nvn-103, nvn-107, nvn-114, nvn-116, nvn-140, nvn-143, nvn-152, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199, nvn2-1, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-33 |
| `nvnTextureBuilderSetFormat` | 55, 231 | nvn-21, nvn-22, nvn-38, nvn-103, nvn-107, nvn-114, nvn-116, nvn-140, nvn-143, nvn-152, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-173, nvn-174, nvn-190, nvn-195, nvn-199, nvn2-1, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-33 |
| `nvnTextureBuilderSetHeight` | 55, 231 | nvn-114 |
| `nvnTextureBuilderSetLevels` | 55, 231 | nvn-114, nvn-116 |
| `nvnTextureBuilderSetPackagedTextureData` | 55, 231 | nvn-139 |
| `nvnTextureBuilderSetPackagedTextureLayout` | 55, 231 | nvn-139 |
| `nvnTextureBuilderSetSamples` | 55, 231 | nvn-21, nvn-22, nvn-38, nvn-114, nvn-116, nvn-173, nvn-174, nvn-190 |
| `nvnTextureBuilderSetSize1D` | 55, 231 | nvn-143 |
| `nvnTextureBuilderSetSize2D` | 55, 231 | nvn-21, nvn-22, nvn-38, nvn-103, nvn-107, nvn-114, nvn-116, nvn-140, nvn-143, nvn-152, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-173, nvn-174, nvn-190, nvn-195, nvn-199, nvn2-1, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-33 |
| `nvnTextureBuilderSetSize3D` | 55, 231 | nvn-143 |
| `nvnTextureBuilderSetStorage` | 55, 231 | nvn-21, nvn-22, nvn-38, nvn-103, nvn-107, nvn-114, nvn-116, nvn-140, nvn-143, nvn-152, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-173, nvn-174, nvn-190, nvn-195, nvn-199, nvn2-1, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-33 |
| `nvnTextureBuilderSetStride` | 55, 231 | nvn-139 |
| `nvnTextureBuilderSetSwizzle` | 55, 231 | nvn-139, nvn-140 |
| `nvnTextureBuilderSetTarget` | 55, 231 | nvn-21, nvn-22, nvn-38, nvn-103, nvn-107, nvn-114, nvn-116, nvn-140, nvn-143, nvn-152, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-173, nvn-174, nvn-190, nvn-195, nvn-199, nvn2-1, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-33 |
| `nvnTextureBuilderSetWidth` | 55, 231 | nvn-114 |
| `nvnTextureCompare` | 55, 231 | nvn-140 |
| `nvnTextureFinalize` | 55, 231 | nvn-21, nvn-22, nvn-38, nvn-140, nvn-143, nvn-152, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-173, nvn-174, nvn-190, nvn-195, nvn-199 |
| `nvnTextureFlushTexels` | 55, 231 | nvn-141 |
| `nvnTextureGetDepth` | 55, 231 | nvn-116 |
| `nvnTextureGetDepthStencilMode` | 55, 231 | nvn-140 |
| `nvnTextureGetFlags` | 55, 231 | nvn-116 |
| `nvnTextureGetFormat` | 55, 231 | nvn-116 |
| `nvnTextureGetHeight` | 55, 231 | nvn-116 |
| `nvnTextureGetLevels` | 55, 231 | nvn-116 |
| `nvnTextureGetMemoryOffset` | 55, 231 | nvn-116 |
| `nvnTextureGetMemoryPool` | 55, 231 | nvn-116 |
| `nvnTextureGetSamples` | 55, 231 | nvn-116 |
| `nvnTextureGetStorageClass` | 55, 231 | nvn-140 |
| `nvnTextureGetStorageSize` | 55, 231 | nvn-140 |
| `nvnTextureGetStride` | 55, 231 | nvn-140 |
| `nvnTextureGetSwizzle` | 55, 231 | nvn-140 |
| `nvnTextureGetTarget` | 55, 231 | nvn-116 |
| `nvnTextureGetTextureAddress` | 55, 231 | nvn-140 |
| `nvnTextureGetViewOffset` | 55, 231 | nvn-140 |
| `nvnTextureGetWidth` | 55, 231 | nvn-116, nvn-149 |
| `nvnTextureGetZCullStorageSize` | 55, 231 | nvn-140 |
| `nvnTextureInitialize` | 55, 231 | nvn-21, nvn-22, nvn-38, nvn-103, nvn-107, nvn-116, nvn-140, nvn-143, nvn-152, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-173, nvn-174, nvn-190, nvn-195, nvn-199, nvn2-1, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-33 |
| `nvnTexturePoolFinalize` | 55, 231 | nvn-131, nvn-149, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnTexturePoolGetMemoryOffset` | 55, 231 | nvn-131 |
| `nvnTexturePoolGetMemoryPool` | 55, 231 | nvn-131 |
| `nvnTexturePoolGetSize` | 55, 231 | nvn-131, nvn-149 |
| `nvnTexturePoolInitialize` | 55, 231 | nvn-131, nvn-149, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28 |
| `nvnTexturePoolRegisterImage` | 231 | nvn2-23, nvn2-24, nvn2-25 |
| `nvnTexturePoolRegisterTexture` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199, nvn2-26, nvn2-27, nvn2-28 |
| `nvnTextureReadTexels` | 55, 231 | nvn-141 |
| `nvnTextureViewGetDepthStencilMode` | 55, 231 | nvn-139 |
| `nvnTextureViewGetFormat` | 55, 231 | nvn-115 |
| `nvnTextureViewGetLayers` | 55, 231 | nvn-115 |
| `nvnTextureViewGetLevels` | 55, 231 | nvn-115 |
| `nvnTextureViewGetSwizzle` | 55, 231 | nvn-139 |
| `nvnTextureViewGetTarget` | 55, 231 | nvn-115 |
| `nvnTextureViewSetDefaults` | 55, 231 | nvn-115, nvn-117, nvn-139, nvn-190 |
| `nvnTextureViewSetDepthStencilMode` | 55, 231 | nvn-139 |
| `nvnTextureViewSetFormat` | 55, 231 | nvn-115 |
| `nvnTextureViewSetLayers` | 55, 231 | nvn-115 |
| `nvnTextureViewSetLevels` | 55, 231 | nvn-115, nvn-117 |
| `nvnTextureViewSetSwizzle` | 55, 231 | nvn-139 |
| `nvnTextureViewSetTarget` | 55, 231 | nvn-115 |
| `nvnTextureWriteTexels` | 55, 231 | nvn-141 |
| `nvnTextureWriteTexelsStrided` | 55, 231 | nvn-142 |

## Samplers

45 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnSamplerBuilderGetBorderColor` | 55, 231 | nvn-129, nvn-130 |
| `nvnSamplerBuilderGetBorderColori` | 55, 231 | nvn-129 |
| `nvnSamplerBuilderGetBorderColorui` | 55, 231 | nvn-129 |
| `nvnSamplerBuilderGetCompare` | 55, 231 | nvn-129, nvn-130 |
| `nvnSamplerBuilderGetDevice` | 55, 231 | nvn-129 |
| `nvnSamplerBuilderGetLodBias` | 55, 231 | nvn-129, nvn-130 |
| `nvnSamplerBuilderGetLodClamp` | 55, 231 | nvn-129, nvn-130 |
| `nvnSamplerBuilderGetMaxAnisotropy` | 55, 231 | nvn-129, nvn-130 |
| `nvnSamplerBuilderGetMinMagFilter` | 55, 231 | nvn-129, nvn-130 |
| `nvnSamplerBuilderGetReductionFilter` | 55, 231 | nvn-129, nvn-130 |
| `nvnSamplerBuilderGetWrapMode` | 55, 231 | nvn-129, nvn-130 |
| `nvnSamplerBuilderSetBorderColori` | 55, 231 | nvn-129 |
| `nvnSamplerBuilderSetBorderColorui` | 55, 231 | nvn-129 |
| `nvnSamplerGetBorderColori` | 55, 231 | nvn-130 |
| `nvnSamplerGetBorderColorui` | 55, 231 | nvn-130 |
| `nvnSamplerPoolRegisterSamplerBuilder` | 55, 231 | nvn-131 |
| `nvnSamplerBuilderSetBorderColor` | 55, 231 | nvn-129, nvn-130, nvn-131, nvn-149, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnSamplerBuilderSetCompare` | 55, 231 | nvn-129, nvn-130, nvn-131, nvn-149 |
| `nvnSamplerBuilderSetDefaults` | 55, 231 | nvn-129, nvn-130, nvn-131, nvn-149, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199, nvn2-26, nvn2-27, nvn2-28 |
| `nvnSamplerBuilderSetDevice` | 55, 231 | nvn-129, nvn-130, nvn-131, nvn-149, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199, nvn2-26, nvn2-27, nvn2-28 |
| `nvnSamplerBuilderSetLodBias` | 55, 231 | nvn-129, nvn-130, nvn-131, nvn-149 |
| `nvnSamplerBuilderSetLodClamp` | 55, 231 | nvn-129, nvn-130, nvn-131, nvn-149 |
| `nvnSamplerBuilderGetLodSnap` | 55, 231 | nvn-129 |
| `nvnSamplerBuilderSetLodSnap` | 55, 231 | nvn-129, nvn-130, nvn-131, nvn-149 |
| `nvnSamplerBuilderSetMaxAnisotropy` | 55, 231 | nvn-129, nvn-130, nvn-131, nvn-149 |
| `nvnSamplerBuilderSetMinMagFilter` | 55, 231 | nvn-129, nvn-130, nvn-131, nvn-149, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199, nvn2-26, nvn2-27, nvn2-28 |
| `nvnSamplerBuilderSetReductionFilter` | 55, 231 | nvn-129, nvn-130, nvn-131, nvn-149 |
| `nvnSamplerBuilderSetWrapMode` | 55, 231 | nvn-129, nvn-130, nvn-131, nvn-149, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199, nvn2-26, nvn2-27, nvn2-28 |
| `nvnSamplerCompare` | 55, 231 | nvn-130 |
| `nvnSamplerFinalize` | 55, 231 | nvn-130, nvn-131, nvn-149, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnSamplerGetBorderColor` | 55, 231 | nvn-130 |
| `nvnSamplerGetCompare` | 55, 231 | nvn-130 |
| `nvnSamplerGetLodBias` | 55, 231 | nvn-130 |
| `nvnSamplerGetLodClamp` | 55, 231 | nvn-130 |
| `nvnSamplerGetMaxAnisotropy` | 55, 231 | nvn-130 |
| `nvnSamplerGetMinMagFilter` | 55, 231 | nvn-130 |
| `nvnSamplerGetReductionFilter` | 55, 231 | nvn-130 |
| `nvnSamplerGetWrapMode` | 55, 231 | nvn-130 |
| `nvnSamplerInitialize` | 55, 231 | nvn-130, nvn-131, nvn-149, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199, nvn2-26, nvn2-27, nvn2-28 |
| `nvnSamplerPoolFinalize` | 55, 231 | nvn-131, nvn-149, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnSamplerPoolGetMemoryOffset` | 55, 231 | nvn-131 |
| `nvnSamplerPoolGetMemoryPool` | 55, 231 | nvn-131 |
| `nvnSamplerPoolGetSize` | 55, 231 | nvn-131, nvn-149 |
| `nvnSamplerPoolInitialize` | 55, 231 | nvn-131, nvn-149, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28 |
| `nvnSamplerPoolRegisterSampler` | 55, 231 | nvn-131, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199, nvn2-26, nvn2-27, nvn2-28 |

## Programs

20 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnProgramGetSubtileSize` | 55, 231 | nvn-189 |
| `nvnProgramGetScratchBytesPerWarp` | 231 | nvn-203 |
| `nvnProgramSetSampleShading` | 55, 231 | nvn-189 |
| `nvnProgramSetSubroutineLinkage` | 55, 231 | nvn-150 |
| `nvnProgramFinalize` | 55, 231 | nvn-37, nvn-43, nvn-46, nvn-47, nvn-149, nvn-150, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-189, nvn-195, nvn-198, nvn-199, nvn-203 |
| `nvnProgramInitialize` | 55, 231 | nvn-149, nvn-150, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-189, nvn-195, nvn-198, nvn-199, nvn-203 |
| `nvnProgramSetShaders` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-189, nvn-195, nvn-199 |
| `nvnProgramSetShadersExt` | 55, 231 | nvn-198 |
| `nvnProgramBuilderSetDefaults` | 231 | nvn-201, nvn-202, nvn2-2, nvn2-8, nvn2-18, nvn2-19, nvn2-21, nvn2-23, nvn2-26 |
| `nvnProgramBuilderSetDevice` | 231 | nvn-201, nvn2-2, nvn2-8, nvn2-18, nvn2-19, nvn2-21, nvn2-23, nvn2-26 |
| `nvnProgramBuilderGetDevice` | 231 | nvn-201 |
| `nvnProgramBuilderSetSampleShading` | 231 | nvn-201 |
| `nvnProgramBuilderGetSampleShading` | 231 | nvn-201 |
| `nvnProgramBuilderSetShaders` | 231 | nvn-201, nvn2-2, nvn2-8, nvn2-18, nvn2-19, nvn2-21, nvn2-23, nvn2-26 |
| `nvnProgramBuilderGetShaders` | 231 | nvn-201 |
| `nvnProgramBuilderGetShaderCount` | 231 | nvn-201 |
| `nvnProgramBuilderSetSubroutineLinkage` | 231 | nvn-202 |
| `nvnProgramBuilderGetSubroutineLinkage` | 231 | nvn-202 |
| `nvnProgramBuilderGetSubroutineLinkageCount` | 231 | nvn-202 |
| `nvnProgramInitializeFromBuilder` | 231 | nvn2-2, nvn2-8, nvn2-18, nvn2-19, nvn2-21, nvn2-23, nvn2-26 |

## State objects

82 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnBlendStateGetAdvancedMode` | 55, 231 | nvn-134 |
| `nvnBlendStateGetAdvancedNormalizedDst` | 55, 231 | nvn-134 |
| `nvnBlendStateGetAdvancedOverlap` | 55, 231 | nvn-134 |
| `nvnBlendStateGetAdvancedPremultipliedSrc` | 55, 231 | nvn-134 |
| `nvnBlendStateSetAdvancedMode` | 55, 231 | nvn-134 |
| `nvnBlendStateSetAdvancedNormalizedDst` | 55, 231 | nvn-134 |
| `nvnBlendStateSetAdvancedOverlap` | 55, 231 | nvn-134 |
| `nvnBlendStateSetAdvancedPremultipliedSrc` | 55, 231 | nvn-134 |
| `nvnMultisampleStateGetSampleLocationsGrid` | 55, 231 | nvn-137 |
| `nvnMultisampleStateSetSampleLocations` | 55, 231 | nvn-137 |
| `nvnBlendStateSetBlendEquation` | 55, 231 | nvn-134 |
| `nvnBlendStateSetBlendFunc` | 55, 231 | nvn-134, nvn-185 |
| `nvnBlendStateSetBlendTarget` | 55, 231 | nvn-134, nvn-185 |
| `nvnBlendStateSetDefaults` | 55, 231 | nvn-134, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-185, nvn-195, nvn-199, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22 |
| `nvnChannelMaskStateSetChannelMask` | 55, 231 | nvn-135 |
| `nvnChannelMaskStateSetDefaults` | 55, 231 | nvn-135, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-33 |
| `nvnColorStateGetBlendEnable` | 55, 231 | nvn-135 |
| `nvnColorStateSetAlphaTest` | 55, 231 | nvn-135, nvn-185 |
| `nvnColorStateSetBlendEnable` | 55, 231 | nvn-135, nvn-185 |
| `nvnColorStateSetDefaults` | 55, 231 | nvn-135, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-185, nvn-195, nvn-199, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22 |
| `nvnColorStateSetLogicOp` | 55, 231 | nvn-135 |
| `nvnDepthStencilStateSetDefaults` | 55, 231 | nvn-136, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-186, nvn-195, nvn-199, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22 |
| `nvnDepthStencilStateSetDepthFunc` | 55, 231 | nvn-136, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22 |
| `nvnDepthStencilStateSetDepthTestEnable` | 55, 231 | nvn-136, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-186, nvn-195, nvn-199, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22 |
| `nvnDepthStencilStateSetDepthWriteEnable` | 55, 231 | nvn-136, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-186, nvn-195, nvn-199, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22 |
| `nvnDepthStencilStateSetStencilFunc` | 55, 231 | nvn-136, nvn-186 |
| `nvnDepthStencilStateSetStencilOp` | 55, 231 | nvn-136, nvn-186 |
| `nvnDepthStencilStateSetStencilTestEnable` | 55, 231 | nvn-136, nvn-186 |
| `nvnMultisampleStateSetAlphaToCoverageEnable` | 55, 231 | nvn-137 |
| `nvnMultisampleStateSetDefaults` | 55, 231 | nvn-137, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-190, nvn-195, nvn-199, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22 |
| `nvnMultisampleStateSetMultisampleEnable` | 55, 231 | nvn-137, nvn-190 |
| `nvnMultisampleStateSetSamples` | 55, 231 | nvn-137, nvn-190 |
| `nvnPolygonStateSetCullFace` | 55, 231 | nvn-138, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-187, nvn-195, nvn-199, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22 |
| `nvnPolygonStateSetDefaults` | 55, 231 | nvn-138, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-187, nvn-195, nvn-199, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22 |
| `nvnPolygonStateSetFrontFace` | 55, 231 | nvn-138, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnPolygonStateSetPolygonMode` | 55, 231 | nvn-138 |
| `nvnPolygonStateSetPolygonOffsetEnables` | 55, 231 | nvn-138, nvn-187 |
| `nvnVertexAttribStateSetDefaults` | 55, 231 | nvn-138, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnVertexAttribStateSetFormat` | 55, 231 | nvn-138, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnVertexAttribStateSetStreamIndex` | 55, 231 | nvn-138, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnVertexStreamStateSetDefaults` | 55, 231 | nvn-138, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnVertexStreamStateSetDivisor` | 55, 231 | nvn-138, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnVertexStreamStateSetStride` | 55, 231 | nvn-138, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnBlendStateGetBlendEquation` | 55, 231 | nvn-134 |
| `nvnBlendStateGetBlendFunc` | 55, 231 | nvn-134 |
| `nvnBlendStateGetBlendTarget` | 55, 231 | nvn-134 |
| `nvnChannelMaskStateGetChannelMask` | 55, 231 | nvn-135 |
| `nvnColorStateGetAlphaTest` | 55, 231 | nvn-135 |
| `nvnColorStateGetLogicOp` | 55, 231 | nvn-135 |
| `nvnDepthStencilStateGetDepthFunc` | 55, 231 | nvn-136 |
| `nvnDepthStencilStateGetDepthTestEnable` | 55, 231 | nvn-136 |
| `nvnDepthStencilStateGetDepthWriteEnable` | 55, 231 | nvn-136 |
| `nvnDepthStencilStateGetStencilFunc` | 55, 231 | nvn-136 |
| `nvnDepthStencilStateGetStencilOp` | 55, 231 | nvn-136 |
| `nvnDepthStencilStateGetStencilTestEnable` | 55, 231 | nvn-136 |
| `nvnMultisampleStateGetAlphaToCoverageDither` | 55, 231 | nvn-137 |
| `nvnMultisampleStateGetAlphaToCoverageEnable` | 55, 231 | nvn-137 |
| `nvnMultisampleStateGetCoverageModulationMode` | 55, 231 | nvn-137 |
| `nvnMultisampleStateGetCoverageToColorEnable` | 55, 231 | nvn-137 |
| `nvnMultisampleStateGetCoverageToColorOutput` | 55, 231 | nvn-137 |
| `nvnMultisampleStateGetMultisampleEnable` | 55, 231 | nvn-137 |
| `nvnMultisampleStateGetRasterSamples` | 55, 231 | nvn-137 |
| `nvnMultisampleStateGetSampleLocationsEnable` | 55, 231 | nvn-137 |
| `nvnMultisampleStateGetSampleLocationsGridEnable` | 55, 231 | nvn-137 |
| `nvnMultisampleStateGetSamples` | 55, 231 | nvn-137 |
| `nvnMultisampleStateSetAlphaToCoverageDither` | 55, 231 | nvn-137 |
| `nvnMultisampleStateSetCoverageModulationMode` | 55, 231 | nvn-137 |
| `nvnMultisampleStateSetCoverageToColorEnable` | 55, 231 | nvn-137 |
| `nvnMultisampleStateSetCoverageToColorOutput` | 55, 231 | nvn-137 |
| `nvnMultisampleStateSetRasterSamples` | 55, 231 | nvn-137 |
| `nvnMultisampleStateSetCoverageTruncationEnable` | 55, 231 | nvn-137 |
| `nvnMultisampleStateGetCoverageTruncationEnable` | 55, 231 | nvn-137 |
| `nvnMultisampleStateSetSampleLocationsEnable` | 55, 231 | nvn-137 |
| `nvnMultisampleStateSetSampleLocationsGridEnable` | 55, 231 | nvn-137 |
| `nvnPolygonStateGetCullFace` | 55, 231 | nvn-138 |
| `nvnPolygonStateGetFrontFace` | 55, 231 | nvn-138 |
| `nvnPolygonStateGetPolygonMode` | 55, 231 | nvn-138 |
| `nvnPolygonStateGetPolygonOffsetEnables` | 55, 231 | nvn-138 |
| `nvnVertexAttribStateGetFormat` | 55, 231 | nvn-138 |
| `nvnVertexAttribStateGetStreamIndex` | 55, 231 | nvn-138 |
| `nvnVertexStreamStateGetDivisor` | 55, 231 | nvn-138 |
| `nvnVertexStreamStateGetStride` | 55, 231 | nvn-138 |

## Queues

30 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnQueueBuilderGetCommandFlushThreshold` | 55, 231 | nvn-127 |
| `nvnQueueBuilderGetCommandMemorySize` | 55, 231 | nvn-127 |
| `nvnQueueBuilderGetComputeMemorySize` | 55, 231 | nvn-127 |
| `nvnQueueBuilderGetControlMemorySize` | 55, 231 | nvn-127 |
| `nvnQueueBuilderGetDevice` | 55, 231 | nvn-127 |
| `nvnQueueBuilderGetFlags` | 55, 231 | nvn-127 |
| `nvnQueueBuilderGetMemory` | 55, 231 | nvn-144 |
| `nvnQueueBuilderGetMemorySize` | 55, 231 | nvn-144 |
| `nvnQueueGetTotalCommandMemoryUsed` | 55, 231 | nvn-144 |
| `nvnQueueGetTotalComputeMemoryUsed` | 55, 231 | nvn-144 |
| `nvnQueueGetTotalControlMemoryUsed` | 55, 231 | nvn-144 |
| `nvnQueueResetMemoryUsageCounts` | 55, 231 | nvn-144 |
| `nvnQueueBuilderGetQueueMemorySize` | 55, 231 | nvn-144 |
| `nvnQueueBuilderSetCommandFlushThreshold` | 55, 231 | nvn-127 |
| `nvnQueueBuilderSetCommandMemorySize` | 55, 231 | nvn-127 |
| `nvnQueueBuilderSetComputeMemorySize` | 55, 231 | nvn-127 |
| `nvnQueueBuilderSetControlMemorySize` | 55, 231 | nvn-127 |
| `nvnQueueBuilderSetDefaults` | 55, 231 | nvn-103, nvn-127, nvn-144, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn2-1 |
| `nvnQueueBuilderSetDevice` | 55, 231 | nvn-103, nvn-127, nvn-144, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn2-1 |
| `nvnQueueBuilderSetFlags` | 55, 231 | nvn-127 |
| `nvnQueueBuilderGetQueuePriority` | 55, 231 | nvn-144 |
| `nvnQueueBuilderSetQueuePriority` | 55, 231 | nvn-144 |
| `nvnQueueBuilderSetQueueMemory` | 55, 231 | nvn-144 |
| `nvnQueueFinalize` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnQueueFinish` | 55, 231 | nvn-104, nvn-105, nvn-106, nvn-108, nvn-109, nvn-110, nvn-111, nvn-112, nvn-113, nvn-117, nvn-119, nvn-120, nvn-121, nvn-122, nvn-126, nvn-133, nvn-141, nvn-142, nvn-143, nvn-144, nvn-145, nvn-146, nvn-147, nvn-148, nvn-152, nvn-153, nvn-154, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199, nvn-200, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-33 |
| `nvnQueueFlush` | 55, 231 | nvn-120, nvn-121, nvn-122, nvn-147 |
| `nvnQueueGetError` | 55, 231 | nvn-126, nvn-149 |
| `nvnQueueInitialize` | 55, 231 | nvn-103, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn2-1 |
| `nvnQueueSubmitCommands` | 55, 231 | nvn-104, nvn-105, nvn-106, nvn-108, nvn-109, nvn-110, nvn-111, nvn-112, nvn-113, nvn-117, nvn-119, nvn-120, nvn-121, nvn-122, nvn-133, nvn-141, nvn-143, nvn-145, nvn-146, nvn-147, nvn-148, nvn-152, nvn-153, nvn-154, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199, nvn-200, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-33 |
| `nvnQueueBuilderSetPrimaryQueue` | 55, 231 | nvn-144 |

## Sync and events

24 functions, 2 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnSyncCreateGLSync` | 55, 231 | nvn-124 |
| `nvnSyncInitializeFromFencedGLSync` | 55, 231 | nvn-124 |
| `nvnSyncFinalize` | 55, 231 | nvn-120, nvn-121, nvn-122, nvn-123, nvn-124, nvn-147, nvn-149, nvn-154 |
| `nvnSyncInitialize` | 55, 231 | nvn-120, nvn-121, nvn-122, nvn-123, nvn-124, nvn-147, nvn-149, nvn-154 |
| `nvnSyncWait` | 55, 231 | nvn-120, nvn-121, nvn-122, nvn-123, nvn-147, nvn-154 |
| `nvnSyncGetNvRmSyncNVX` |  |  |
| `nvnSyncInitializeFromNvRmSyncNVX` |  |  |
| `nvnEventBuilderGetMemoryOffset` | 55, 231 | nvn-128 |
| `nvnEventBuilderGetMemoryPool` | 55, 231 | nvn-128 |
| `nvnEventBuilderGetStorage` | 55, 231 | nvn-128 |
| `nvnEventGetMemoryOffset` | 55, 231 | nvn-128 |
| `nvnEventGetMemoryPool` | 55, 231 | nvn-128 |
| `nvnEventBuilderSetDefaults` | 55, 231 | nvn-128, nvn-145, nvn-146, nvn-147 |
| `nvnEventBuilderSetStorage` | 55, 231 | nvn-128, nvn-145, nvn-146, nvn-147 |
| `nvnEventFinalize` | 55, 231 | nvn-128, nvn-145, nvn-146, nvn-147 |
| `nvnEventGetValue` | 55, 231 | nvn-128, nvn-145, nvn-146 |
| `nvnEventInitialize` | 55, 231 | nvn-128, nvn-145, nvn-146, nvn-147 |
| `nvnEventSignal` | 55, 231 | nvn-128, nvn-145, nvn-146, nvn-147 |
| `nvnQueueFenceSync` | 55, 231 | nvn-120, nvn-122, nvn-147 |
| `nvnQueueWaitSync` | 55, 231 | nvn-120, nvn-121, nvn-122 |
| `nvnCommandBufferFenceSync` | 55, 231 | nvn-121 |
| `nvnCommandBufferSignalEvent` | 55, 231 | nvn-145, nvn-146 |
| `nvnCommandBufferWaitEvent` | 55, 231 | nvn-147 |
| `nvnCommandBufferWaitSync` | 55, 231 | nvn-122 |

## Windows and present

35 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnQueueAcquireTexture` | 55, 231 | nvn-152, nvn-153 |
| `nvnQueuePresentTexture` | 55, 231 | nvn-152, nvn-153, nvn-154 |
| `nvnWindowBuilderGetDevice` | 55, 231 | nvn-151 |
| `nvnWindowBuilderGetNativeWindow` | 55, 231 | nvn-151 |
| `nvnWindowBuilderGetNumActiveTextures` | 55, 231 | nvn-151 |
| `nvnWindowGetNativeWindow` | 55, 231 | nvn-153 |
| `nvnWindowGetNumActiveTextures` | 55, 231 | nvn-153 |
| `nvnWindowGetNumTextures` | 55, 231 | nvn-153 |
| `nvnWindowSetNumActiveTextures` | 55, 231 | nvn-153 |
| `nvnWindowBuilderGetNumTextures` | 55, 231 | nvn-151 |
| `nvnWindowBuilderGetPresentInterval` | 55, 231 | nvn-151 |
| `nvnWindowBuilderGetTexture` | 55, 231 | nvn-151 |
| `nvnWindowGetCrop` | 55, 231 | nvn-153 |
| `nvnWindowGetPresentInterval` | 55, 231 | nvn-153 |
| `nvnWindowAcquireTexture` | 55, 231 | nvn-154 |
| `nvnWindowBuilderSetDefaults` | 55, 231 | nvn-151, nvn-152, nvn-204 |
| `nvnWindowBuilderSetDevice` | 55, 231 | nvn-151, nvn-152 |
| `nvnWindowBuilderSetNativeWindow` | 55, 231 | nvn-151, nvn-152 |
| `nvnWindowBuilderSetNumActiveTextures` | 55, 231 | nvn-151 |
| `nvnWindowBuilderSetPresentInterval` | 55, 231 | nvn-151 |
| `nvnWindowBuilderSetTextures` | 55, 231 | nvn-151, nvn-152 |
| `nvnWindowBuilderSetColorSpace` | 231 | nvn-204 |
| `nvnWindowBuilderGetColorSpace` | 231 | nvn-204 |
| `nvnWindowBuilderSetHdrDisplayMetadata` | 231 | nvn-204 |
| `nvnWindowBuilderGetHdrDisplayMetadata` | 231 | nvn-204 |
| `nvnWindowBuilderSetHdrContentMetadata` | 231 | nvn-204 |
| `nvnWindowBuilderGetHdrContentMetadata` | 231 | nvn-204 |
| `nvnWindowSetHdrDisplayMetadata` | 231 | nvn-205 |
| `nvnWindowGetHdrDisplayMetadata` | 231 | nvn-205 |
| `nvnWindowSetHdrContentMetadata` | 231 | nvn-205 |
| `nvnWindowGetHdrContentMetadata` | 231 | nvn-205 |
| `nvnWindowFinalize` | 55, 231 | nvn-152 |
| `nvnWindowInitialize` | 55, 231 | nvn-152 |
| `nvnWindowSetCrop` | 55, 231 | nvn-153 |
| `nvnWindowSetPresentInterval` | 55, 231 | nvn-153 |

## Counters and queries

6 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnDeviceGetTimestampInNanoseconds` | 55, 231 | nvn-15, nvn-16, nvn-167, nvn-168 |
| `nvnCommandBufferReportCounter` | 55, 231 | nvn-165, nvn-167, nvn-168, nvn-169 |
| `nvnCommandBufferReportValue` | 55, 231 | nvn-166 |
| `nvnCommandBufferResetCounter` | 55, 231 | nvn-165, nvn-169 |
| `nvnCommandBufferSetRenderEnable` | 55, 231 | nvn-169 |
| `nvnCommandBufferSetRenderEnableConditional` | 55, 231 | nvn-169 |

## Commands: memory and recording

25 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferGetCommandMemoryFree` | 55, 231 | nvn-170 |
| `nvnCommandBufferGetCommandMemorySize` | 55, 231 | nvn-170 |
| `nvnCommandBufferGetControlMemoryFree` | 55, 231 | nvn-170 |
| `nvnCommandBufferGetControlMemorySize` | 55, 231 | nvn-170 |
| `nvnCommandBufferSetControlMemoryCallbackEnabled` | 55, 231 | nvn-171 |
| `nvnCommandBufferAddCommandMemory` | 55, 231 | nvn-103, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-170, nvn-171, nvn2-1 |
| `nvnCommandBufferAddControlMemory` | 55, 231 | nvn-103, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-170, nvn2-1 |
| `nvnCommandBufferBarrier` | 55, 231 | nvn-104, nvn-105, nvn-106, nvn-108, nvn-109, nvn-111, nvn-112, nvn-113, nvn-117, nvn-120, nvn-121, nvn-122, nvn-141, nvn-143, nvn-147, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-173, nvn-174, nvn-190, nvn-195, nvn-199, nvn-200, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-33 |
| `nvnCommandBufferBeginRecording` | 55, 231 | nvn-104, nvn-105, nvn-106, nvn-108, nvn-109, nvn-110, nvn-111, nvn-112, nvn-113, nvn-117, nvn-119, nvn-120, nvn-121, nvn-122, nvn-133, nvn-141, nvn-143, nvn-145, nvn-146, nvn-147, nvn-148, nvn-152, nvn-153, nvn-154, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199, nvn-200, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-33 |
| `nvnCommandBufferSetRecordingMode` | 231 | nvn2-20 |
| `nvnCommandBufferSetComputeOverlapMode` | 231 | nvn2-20 |
| `nvnCommandBufferCallCommands` | 55, 231 | nvn-172 |
| `nvnCommandBufferCopyCommands` | 55, 231 | nvn-172 |
| `nvnCommandBufferEndRecording` | 55, 231 | nvn-104, nvn-105, nvn-106, nvn-108, nvn-109, nvn-110, nvn-111, nvn-112, nvn-113, nvn-117, nvn-119, nvn-120, nvn-121, nvn-122, nvn-133, nvn-141, nvn-143, nvn-145, nvn-146, nvn-147, nvn-148, nvn-152, nvn-153, nvn-154, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-172, nvn-195, nvn-199, nvn-200, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-33 |
| `nvnCommandBufferFinalize` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferGetCommandMemoryUsed` | 55, 231 | nvn-170 |
| `nvnCommandBufferGetControlMemoryUsed` | 55, 231 | nvn-170 |
| `nvnCommandBufferGetMemoryCallback` | 55, 231 | nvn-171 |
| `nvnCommandBufferGetMemoryCallbackData` | 55, 231 | nvn-171 |
| `nvnCommandBufferInitialize` | 55, 231 | nvn-103, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn2-1 |
| `nvnCommandBufferIsRecording` | 55, 231 | nvn-170 |
| `nvnCommandBufferSetCommandMemoryCallbackEnabled` | 55, 231 | nvn-171 |
| `nvnCommandBufferSetComputeWorkloadPriority` | 231 | nvn2-20 |
| `nvnCommandBufferSetMemoryCallback` | 55, 231 | nvn-171 |
| `nvnCommandBufferSetMemoryCallbackData` | 55, 231 | nvn-171 |

## Commands: binds

41 functions, 2 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferBindCoverageModulationTable` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferBindImagesDeferred` | 231 | nvn2-25 |
| `nvnCommandBufferBindSeparateSamplersDeferred` | 231 | nvn2-28 |
| `nvnCommandBufferBindSeparateTexturesDeferred` | 231 | nvn2-28 |
| `nvnCommandBufferBindStorageBuffersDeferred` | 231 | nvn2-6 |
| `nvnCommandBufferBindUniformBuffersDeferred` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferSetProgramSubroutines` |  |  |
| `nvnCommandBufferBindShadingRateImage` | 231 | nvn2-22 |
| `nvnCommandBufferShadingRateImagePalette` |  |  |
| `nvnCommandBufferPushGraphicsConstants` | 231 | nvn2-21 |
| `nvnCommandBufferPushComputeConstants` | 231 | nvn2-19 |
| `nvnCommandBufferBindBlendState` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-185, nvn-195, nvn-199, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22 |
| `nvnCommandBufferBindChannelMaskState` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-33 |
| `nvnCommandBufferBindColorState` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-185, nvn-195, nvn-199, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22 |
| `nvnCommandBufferBindDepthStencilState` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-186, nvn-195, nvn-199, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22 |
| `nvnCommandBufferBindImage` | 231 | nvn2-23 |
| `nvnCommandBufferBindImages` | 231 | nvn2-24 |
| `nvnCommandBufferBindMultisampleState` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-190, nvn-195, nvn-199, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22 |
| `nvnCommandBufferBindPolygonState` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-187, nvn-195, nvn-199, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22 |
| `nvnCommandBufferBindProgram` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-189, nvn-195, nvn-198, nvn-199, nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28 |
| `nvnCommandBufferBindSeparateSampler` | 231 | nvn2-26 |
| `nvnCommandBufferBindSeparateSamplers` | 231 | nvn2-27 |
| `nvnCommandBufferBindSeparateTexture` | 231 | nvn2-26 |
| `nvnCommandBufferBindSeparateTextures` | 231 | nvn2-27 |
| `nvnCommandBufferBindStorageBuffer` | 231 | nvn2-3, nvn2-4, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-26, nvn2-27, nvn2-28 |
| `nvnCommandBufferBindStorageBuffers` | 231 | nvn2-5 |
| `nvnCommandBufferBindTexture` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferBindTextures` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferBindTexturesDeferred` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferBindTransformFeedbackBuffer` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferBindTransformFeedbackBuffers` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferBindUniformBuffer` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferBindUniformBuffers` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferBindVertexAttribState` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferBindVertexBuffer` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferBindVertexBuffers` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferBindVertexStreamState` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferSetSamplerPool` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199, nvn2-26, nvn2-27, nvn2-28 |
| `nvnCommandBufferSetShaderScratchMemory` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferSetTexturePool` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28 |
| `nvnCommandBufferUpdateUniformBuffer` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |

## Commands: draws and dispatches

26 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferDrawTexture` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferBeginTransformFeedback` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferSetGeometryProcessingMode` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22 |
| `nvnCommandBufferDrawMeshTasks` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22 |
| `nvnCommandBufferDrawMeshTasks3D` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22 |
| `nvnCommandBufferDrawMeshTasksIndirect` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22 |
| `nvnCommandBufferDrawMeshTasks3DIndirect` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22 |
| `nvnCommandBufferMultiDrawMeshTasksIndirect` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22 |
| `nvnCommandBufferMultiDrawMeshTasks3DIndirect` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22 |
| `nvnCommandBufferMultiDrawMeshTasksIndirectCount` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22 |
| `nvnCommandBufferMultiDrawMeshTasks3DIndirectCount` | 231 | nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22 |
| `nvnCommandBufferDispatchCompute` | 231 | nvn2-3, nvn2-4, nvn2-5, nvn2-6, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28 |
| `nvnCommandBufferDispatchComputeIndirect` | 231 | nvn2-7 |
| `nvnCommandBufferDrawArrays` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-181, nvn-194, nvn-195, nvn-199 |
| `nvnCommandBufferDrawArraysIndirect` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferDrawArraysInstanced` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferDrawElements` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-182, nvn-195, nvn-199 |
| `nvnCommandBufferDrawElementsBaseVertex` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferDrawElementsIndirect` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferDrawElementsInstanced` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferDrawTransformFeedback` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferEndTransformFeedback` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferMultiDrawArraysIndirectCount` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferMultiDrawElementsIndirectCount` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferPauseTransformFeedback` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |
| `nvnCommandBufferResumeTransformFeedback` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199 |

## Commands: clears and copies

25 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferClearTexturei` | 55, 231 | nvn-174 |
| `nvnCommandBufferGetCopyImageStride` | 55, 231 | nvn-176 |
| `nvnCommandBufferGetCopyRowStride` | 55, 231 | nvn-176 |
| `nvnCommandBufferResolveDepthBuffer` | 55, 231 | nvn-191 |
| `nvnCommandBufferTiledDownsample` | 55, 231 | nvn-190 |
| `nvnCommandBufferTiledDownsampleTextureView` | 55, 231 | nvn-190 |
| `nvnCommandBufferClearBuffer` | 55, 231 | nvn-175 |
| `nvnCommandBufferClearColor` | 55, 231 | nvn-104, nvn-105, nvn-106, nvn-120, nvn-121, nvn-122, nvn-147, nvn-152, nvn-153, nvn-154, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-190, nvn-195, nvn-199, nvn-200, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-33 |
| `nvnCommandBufferClearColori` | 55, 231 | nvn-173 |
| `nvnCommandBufferClearColorui` | 55, 231 | nvn-173 |
| `nvnCommandBufferClearDepthStencil` | 55, 231 | nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-186, nvn-195, nvn-199, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-33 |
| `nvnCommandBufferClearTexture` | 55, 231 | nvn-174 |
| `nvnCommandBufferClearTextureui` | 55, 231 | nvn-174 |
| `nvnCommandBufferCopyBufferToBuffer` | 55, 231 | nvn-110, nvn-119, nvn-133, nvn-148, nvn2-4, nvn2-5, nvn2-6, nvn2-7, nvn2-9, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-18, nvn2-19, nvn2-20, nvn2-21, nvn2-22, nvn2-26, nvn2-27, nvn2-28 |
| `nvnCommandBufferCopyBufferToTexture` | 55, 231 | nvn-108, nvn-109, nvn-111, nvn-112, nvn-113, nvn-117, nvn-143, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199, nvn2-23, nvn2-24, nvn2-25, nvn2-26, nvn2-27, nvn2-28, nvn2-33 |
| `nvnCommandBufferCopyTextureToBuffer` | 55, 231 | nvn-104, nvn-105, nvn-106, nvn-108, nvn-109, nvn-111, nvn-112, nvn-113, nvn-117, nvn-120, nvn-121, nvn-122, nvn-141, nvn-143, nvn-147, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-173, nvn-174, nvn-190, nvn-195, nvn-199, nvn-200, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-23, nvn2-24, nvn2-25, nvn2-33 |
| `nvnCommandBufferCopyTextureToTexture` | 55, 231 | nvn-109, nvn-112 |
| `nvnCommandBufferDiscardColor` | 55, 231 | nvn-177 |
| `nvnCommandBufferDiscardDepthStencil` | 55, 231 | nvn-177 |
| `nvnCommandBufferDownsample` | 55, 231 | nvn-190 |
| `nvnCommandBufferDownsampleTextureView` | 55, 231 | nvn-190 |
| `nvnCommandBufferRestoreZCullData` | 55, 231 | nvn-191 |
| `nvnCommandBufferSaveZCullData` | 55, 231 | nvn-191 |
| `nvnCommandBufferSetCopyImageStride` | 55, 231 | nvn-176 |
| `nvnCommandBufferSetCopyRowStride` | 55, 231 | nvn-108, nvn-109, nvn-110, nvn-111, nvn-112, nvn-113, nvn-117, nvn-141, nvn-143, nvn-176 |

## Commands: render state

36 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnCommandBufferOverrideSubtileSize` | 55, 231 | nvn-188 |
| `nvnCommandBufferSetColorReductionEnable` | 55, 231 | nvn-188 |
| `nvnCommandBufferSetColorReductionThresholds` | 55, 231 | nvn-188 |
| `nvnCommandBufferSetConservativeRasterDilate` | 55, 231 | nvn-193 |
| `nvnCommandBufferSetInnerTessellationLevels` | 55, 231 | nvn-195 |
| `nvnCommandBufferSetOuterTessellationLevels` | 55, 231 | nvn-195 |
| `nvnCommandBufferSetProvokingVertexMode` | 55, 231 | nvn-180 |
| `nvnCommandBufferSetSubpixelPrecisionBias` | 55, 231 | nvn-192 |
| `nvnCommandBufferSetZCullZF32CompressionEnable` | 55, 231 | nvn-188 |
| `nvnCommandBufferSetShadingRatePaletteMode` | 231 | nvn2-22 |
| `nvnCommandBufferSetAlphaRef` | 55, 231 | nvn-185 |
| `nvnCommandBufferSetBlendColor` | 55, 231 | nvn-185 |
| `nvnCommandBufferSetConservativeRasterEnable` | 55, 231 | nvn-192, nvn-193 |
| `nvnCommandBufferSetDepthBounds` | 55, 231 | nvn-179 |
| `nvnCommandBufferSetDepthClamp` | 55, 231 | nvn-180 |
| `nvnCommandBufferSetDepthRange` | 55, 231 | nvn-179 |
| `nvnCommandBufferSetDepthRanges` | 55, 231 | nvn-179 |
| `nvnCommandBufferSetLineWidth` | 55, 231 | nvn-194 |
| `nvnCommandBufferSetPatchSize` | 55, 231 | nvn-195 |
| `nvnCommandBufferSetPointSize` | 55, 231 | nvn-181 |
| `nvnCommandBufferSetPolygonOffsetClamp` | 55, 231 | nvn-187 |
| `nvnCommandBufferSetPrimitiveRestart` | 55, 231 | nvn-182 |
| `nvnCommandBufferSetRasterizerDiscard` | 55, 231 | nvn-180 |
| `nvnCommandBufferSetRenderTargets` | 55, 231 | nvn-104, nvn-105, nvn-106, nvn-120, nvn-121, nvn-122, nvn-147, nvn-152, nvn-153, nvn-154, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-173, nvn-190, nvn-195, nvn-199, nvn-200, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-33 |
| `nvnCommandBufferSetSampleMask` | 55, 231 | nvn-187 |
| `nvnCommandBufferSetScissor` | 55, 231 | nvn-104, nvn-105, nvn-106, nvn-120, nvn-121, nvn-122, nvn-147, nvn-152, nvn-153, nvn-154, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-173, nvn-190, nvn-195, nvn-199, nvn-200, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-33 |
| `nvnCommandBufferSetScissors` | 55, 231 | nvn-178 |
| `nvnCommandBufferSetStencilCullCriteria` | 55, 231 | nvn-188 |
| `nvnCommandBufferSetStencilMask` | 55, 231 | nvn-186 |
| `nvnCommandBufferSetStencilRef` | 55, 231 | nvn-186 |
| `nvnCommandBufferSetStencilValueMask` | 55, 231 | nvn-186 |
| `nvnCommandBufferSetTiledCacheAction` | 55, 231 | nvn-184 |
| `nvnCommandBufferSetTiledCacheTileSize` | 55, 231 | nvn-184 |
| `nvnCommandBufferSetViewport` | 55, 231 | nvn-104, nvn-105, nvn-106, nvn-120, nvn-121, nvn-122, nvn-147, nvn-152, nvn-153, nvn-154, nvn-155, nvn-156, nvn-157, nvn-158, nvn-159, nvn-160, nvn-161, nvn-162, nvn-163, nvn-164, nvn-195, nvn-199, nvn-200, nvn2-10, nvn2-11, nvn2-12, nvn2-13, nvn2-14, nvn2-15, nvn2-16, nvn2-17, nvn2-21, nvn2-22, nvn2-33 |
| `nvnCommandBufferSetViewportSwizzles` | 55, 231 | nvn-183 |
| `nvnCommandBufferSetViewports` | 55, 231 | nvn-178 |

## DLSS

58 functions, 0 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnDlssNetworkBuilderSetDefaults` | 231 | nvn2-30, nvn2-32, nvn2-33 |
| `nvnDlssNetworkBuilderSetDevice` | 231 | nvn2-30, nvn2-32, nvn2-33 |
| `nvnDlssNetworkBuilderGetDevice` | 231 | nvn2-30 |
| `nvnDlssNetworkBuilderSetFlags` | 231 | nvn2-30 |
| `nvnDlssNetworkBuilderGetFlags` | 231 | nvn2-30 |
| `nvnDlssNetworkBuilderSetMethod` | 231 | nvn2-30 |
| `nvnDlssNetworkBuilderGetMethod` | 231 | nvn2-30 |
| `nvnDlssNetworkBuilderSetPreset` | 231 | nvn2-30 |
| `nvnDlssNetworkBuilderGetPreset` | 231 | nvn2-30 |
| `nvnDlssNetworkBuilderSetOutputResolution` | 231 | nvn2-30, nvn2-32, nvn2-33 |
| `nvnDlssNetworkBuilderGetOutputResolution` | 231 | nvn2-30 |
| `nvnDlssNetworkBuilderSetMaxEvaluationEntries` | 231 | nvn2-30 |
| `nvnDlssNetworkBuilderGetMaxEvaluationEntries` | 231 | nvn2-30 |
| `nvnDlssNetworkBuilderSetStorage` | 231 | nvn2-30, nvn2-32, nvn2-33 |
| `nvnDlssNetworkBuilderGetMemory` | 231 | nvn2-30 |
| `nvnDlssNetworkBuilderGetMemorySize` | 231 | nvn2-30 |
| `nvnDlssNetworkBuilderSetTextureIDs` | 231 | nvn2-30 |
| `nvnDlssNetworkBuilderGetTexturePool` | 231 | nvn2-30 |
| `nvnDlssNetworkBuilderGetTextureIDs` | 231 | nvn2-30 |
| `nvnDlssNetworkBuilderGetOptimalSettings` | 231 | nvn2-32 |
| `nvnDlssNetworkBuilderGetStorageSize` | 231 | nvn2-32, nvn2-33 |
| `nvnDlssNetworkInitialize` | 231 | nvn2-32, nvn2-33 |
| `nvnDlssNetworkFinalize` | 231 | nvn2-32, nvn2-33 |
| `nvnDlssNetworkResize` | 231 | nvn2-32 |
| `nvnCommandBufferEvaluateDlss` | 231 | nvn2-33 |
| `nvnDlssParametersSetDefaults` | 231 | nvn2-31, nvn2-33 |
| `nvnDlssParametersSetInputColor` | 231 | nvn2-31, nvn2-33 |
| `nvnDlssParametersSetOutputColor` | 231 | nvn2-31, nvn2-33 |
| `nvnDlssParametersSetMotionVectors` | 231 | nvn2-31, nvn2-33 |
| `nvnDlssParametersSetDepth` | 231 | nvn2-31, nvn2-33 |
| `nvnDlssParametersSetBiasCurrentColorMask` | 231 | nvn2-31 |
| `nvnDlssParametersSetExposureTexture` | 231 | nvn2-31 |
| `nvnDlssParametersSetJitterOffset` | 231 | nvn2-31 |
| `nvnDlssParametersGetJitterOffset` | 231 | nvn2-31 |
| `nvnDlssParametersSetMotionVectorScale` | 231 | nvn2-31 |
| `nvnDlssParametersGetMotionVectorScale` | 231 | nvn2-31 |
| `nvnDlssParametersSetPreExposure` | 231 | nvn2-31 |
| `nvnDlssParametersGetPreExposure` | 231 | nvn2-31 |
| `nvnDlssParametersSetHistoryReset` | 231 | nvn2-31, nvn2-33 |
| `nvnDlssParametersGetHistoryReset` | 231 | nvn2-31 |
| `nvnDlssParametersSetEvaluationIndex` | 231 | nvn2-31 |
| `nvnDlssParametersGetEvaluationIndex` | 231 | nvn2-31 |
| `nvnDlssParametersSetInputSubrectangleSize` | 231 | nvn2-31, nvn2-33 |
| `nvnDlssParametersGetInputSubrectangleSize` | 231 | nvn2-31 |
| `nvnDlssParametersSetInputColorSubrectangleBase` | 231 | nvn2-31 |
| `nvnDlssParametersGetInputColorSubrectangleBase` | 231 | nvn2-31 |
| `nvnDlssParametersSetDepthSubrectangleBase` | 231 | nvn2-31 |
| `nvnDlssParametersGetDepthSubrectangleBase` | 231 | nvn2-31 |
| `nvnDlssParametersSetMotionVectorSubrectangleBase` | 231 | nvn2-31 |
| `nvnDlssParametersGetMotionVectorSubrectangleBase` | 231 | nvn2-31 |
| `nvnDlssParametersSetBiasCurrentColorSubrectangleBase` | 231 | nvn2-31 |
| `nvnDlssParametersGetBiasCurrentColorSubrectangleBase` | 231 | nvn2-31 |
| `nvnDlssParametersSetOutputSubrectangleBase` | 231 | nvn2-31 |
| `nvnDlssParametersGetOutputSubrectangleBase` | 231 | nvn2-31 |
| `nvnDlssParametersSetOverlayDebugLayer` | 231 | nvn2-31 |
| `nvnDlssParametersSetOverlayFullScreen` | 231 | nvn2-31 |
| `nvnDlssParametersSetOverlayShowNans` | 231 | nvn2-31 |
| `nvnDlssParametersSetOverlayJitterDebug` | 231 | nvn2-31 |

## Ray tracing

15 functions, 9 with no case.

| Function | Layer | Cases |
| --- | --- | --- |
| `nvnDeviceCalculateAccelerationStructureMemoryRequirements` |  |  |
| `nvnProgramGetShaderGroupHandle` |  |  |
| `nvnProgramBuilderSetRayTracingModules` |  |  |
| `nvnProgramBuilderSetMaxCallableRecursionDepth` | 231 | nvn-201 |
| `nvnProgramBuilderGetMaxCallableRecursionDepth` | 231 | nvn-201 |
| `nvnProgramBuilderSetMaxTraceRecursionDepth` | 231 | nvn-201 |
| `nvnProgramBuilderGetMaxTraceRecursionDepth` | 231 | nvn-201 |
| `nvnProgramBuilderSetProgramRayTracingFlags` | 231 | nvn-201 |
| `nvnProgramBuilderGetProgramRayTracingFlags` | 231 | nvn-201 |
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
| `nvnSetExternalDebuggerEnabledNVX` | 55, 231 | nvn-149 |
| `nvnDeviceGenerateDebugDomainId` | 55, 231 | nvn-149, nvn-196, nvn-200 |
| `nvnDeviceInstallDebugCallback` | 55, 231 | nvn-149 |
| `nvnDeviceIsExternalDebuggerAttached` | 55, 231 | nvn-149 |
| `nvnDeviceSetAutomaticCheckpointsByDebugGroup` | 55, 231 | nvn-149 |
| `nvnDeviceSetAutomaticCheckpointsEnable` | 55, 231 | nvn-149 |
| `nvnDeviceSetAutomaticCheckpointsMask` | 55, 231 | nvn-149 |
| `nvnDeviceSetAutomaticCheckpointsSamplingInterval` | 55, 231 | nvn-149 |
| `nvnDeviceSetCheckpointCallStacksEnable` | 55, 231 | nvn-149 |
| `nvnDeviceSetCheckpointMaxCallStacksDepth` | 55, 231 | nvn-149 |
| `nvnDeviceWalkDebugDatabase` | 55, 231 | nvn-149 |
| `nvnMemoryPoolSetDebugLabel` | 55, 231 | nvn-149 |
| `nvnBufferGetDebugID` | 55, 231 | nvn-149 |
| `nvnBufferSetDebugLabel` | 55, 231 | nvn-149 |
| `nvnTextureGetDebugID` | 55, 231 | nvn-149 |
| `nvnTextureSetDebugLabel` | 55, 231 | nvn-149 |
| `nvnSamplerGetDebugID` | 55, 231 | nvn-149 |
| `nvnSamplerSetDebugLabel` | 55, 231 | nvn-149 |
| `nvnProgramBuilderSetDebugDataHashes` |  |  |
| `nvnProgramBuilderGetDebugDataHashes` |  |  |
| `nvnCommandBufferInsertCheckpointDynamic` | 55, 231 | nvn-196 |
| `nvnCommandBufferInsertCheckpointStatic` | 55, 231 | nvn-196 |
| `nvnCommandBufferInsertDebugMarkerGraphics` | 231 | nvn-200 |
| `nvnCommandBufferInsertDebugMarkerCompute` | 231 | nvn-200 |
| `nvnCommandBufferPushDebugGroupGraphics` | 231 | nvn-200 |
| `nvnCommandBufferPushDebugGroupCompute` | 231 | nvn-200 |
| `nvnCommandBufferPopDebugGroupGraphics` | 231 | nvn-200 |
| `nvnCommandBufferPopDebugGroupCompute` | 231 | nvn-200 |
| `nvnCommandBufferInsertDebugMarker` | 55, 231 | nvn-196 |
| `nvnCommandBufferInsertDebugMarkerDynamic` | 55, 231 | nvn-196 |
| `nvnCommandBufferInsertDebugMarkerStatic` | 55, 231 | nvn-196 |
| `nvnCommandBufferPopDebugGroup` | 55, 231 | nvn-196 |
| `nvnCommandBufferPopDebugGroupId` | 55, 231 | nvn-196 |
| `nvnCommandBufferPushDebugGroup` | 55, 231 | nvn-196 |
| `nvnCommandBufferPushDebugGroupDynamic` | 55, 231 | nvn-196 |
| `nvnCommandBufferPushDebugGroupStatic` | 55, 231 | nvn-196 |
| `nvnCommandBufferSetDebugLabel` | 55, 231 | nvn-149 |
| `nvnDeviceSetDebugLabel` | 55, 231 | nvn-149 |
| `nvnProgramSetDebugLabel` | 55, 231 | nvn-149 |
| `nvnQueueSetDebugLabel` | 55, 231 | nvn-149 |
| `nvnSamplerPoolSetDebugLabel` | 55, 231 | nvn-149 |
| `nvnSyncSetDebugLabel` | 55, 231 | nvn-149 |
| `nvnTexturePoolSetDebugLabel` | 55, 231 | nvn-149 |
| `nvnWindowSetDebugLabel` | 55, 231 | nvn-153 |
