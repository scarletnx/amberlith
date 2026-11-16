# The NVN layer

## Index

* [What it is](#what-it-is)
* [One layer, two builds](#one-layer-two-builds)
* [Using it in a program](#using-it-in-a-program)
* [The API version](#the-api-version)
* [API 55: NVN on deko3d](#api-55-nvn-on-deko3d)
  * [What it takes](#what-it-takes)
  * [Programs](#programs)
  * [What a shader may use](#what-a-shader-may-use)
  * [Where the layer makes its own choices](#where-the-layer-makes-its-own-choices)
* [API 231: the names alone](#api-231-the-names-alone)
* [What a pass proves](#what-a-pass-proves)
* [Tests](#tests)
* [Where its numbers come from](#where-its-numbers-come-from)

## What it is

NVN is the API a game draws with, and each game carries its own driver for it; a homebrew program has none.
`nvnlayer/` stands where that driver does: the functions a game gets by name from `nvnBootstrapLoader` and the
`nvnDeviceGetProcAddress` it hands back. A program written against it looks every function up by name
(`NvnLoadProcs`) and calls nothing of the layer's by name, as a game calls its driver.

It is a module of its own: a folder, a public header (`nvnlayer/include/nvnlayer.h`), a make fragment
(`nvnlayer/layer.mk`) and a test. Nothing in it reads edenprobe's sources, so a demo or another homebrew program
links it without the rest of the probe. It is no development kit and takes nothing from one: the names and argument
types are ScarletNX's `gpu/nvn/nvn.hook`, the types h1k421's reconstruction (see
[Where its numbers come from](#where-its-numbers-come-from)).

What it is for: a runtime that runs a game's NVN calls itself. ScarletNX finds `nvnBootstrapLoader` among a
program's exported functions, as it finds a game's, and puts its own NVN in place of every function the program
looks up; none of the layer's bodies runs past the name lookup. The program's draws then go down the path every
game takes, where a homebrew program that calls deko3d reaches ScarletNX's engine methods path instead.

## One layer, two builds

The layer is built for one NVN API version, `NVNLAYER_API` in the make fragment. ScarletNX carries both versions
in one `nvn.hook`; the layer mirrors that: what is shared is written once, and a version's own part is one file
set behind the version.

| Part | Files | In which build |
| --- | --- | --- |
| the entry points: one list of every function with its prototype | `include/nvnlayer_procs.h` | both; the API 231 build has forty names more (compute, storage buffers and images, the program builder and its getters, push constants, mesh draws, a window's colour space and HDR metadata, the markers and groups that name graphics or compute work) |
| the lookup: `nvnBootstrapLoader`, `nvnDeviceGetProcAddress`, `NvnLoadProcs` | `source/layer.c` | both |
| `nvnDeviceGetInteger`'s switch, in assembly, with the version's two numbers | `source/layer.c` | both |
| the failure a program reads, the abort guard, the fatal hook | `source/layer.c` | both |
| the types and the object sizes | `include/nvn/nvn.h` | both; the API 231 build adds the Ounce's objects and its larger window builder |
| NVN on deko3d: every function's body | `source/api55_deko3d.c`, `api55_draw.c`, `api55_command.c`, `api55_debug.c`, `api55_object.c` | API 55 |
| a DKSH as an NVN program's code and control section | `source/api55_shader.c` | API 55 |
| bodies that stop the program, naming the function | `source/api231_surface.c` | API 231 |

- **API 55, the Switch's.** A working NVN on deko3d. It runs on the console and in an emulator that runs the
  graphics unit (Eden); under ScarletNX its entry points are taken over. Its programs are Maxwell code, what uam
  compiles.
- **API 231, the Switch 2's.** The names, and the object sizes ScarletNX's API 231 implementation takes, and
  nothing under them. No machine but ScarletNX runs such a program, so there is nothing for a body to record: a
  body that is reached means ScarletNX did not take that function, and it ends the run with a FATAL that names
  the function instead of going on with nothing drawn.

Both builds are tested (see [Tests](#tests)): games use both versions.

## Using it in a program

In the makefile:

```make
NVNLAYER_DIR := nvnlayer
NVNLAYER_API := 55            # or 231
include nvnlayer/layer.mk

CFLAGS  += $(NVNLAYER_CFLAGS)
LDFLAGS += $(NVNLAYER_LDFLAGS)
CFILES  += $(NVNLAYER_SOURCES)
```

`NVNLAYER_CFLAGS` gives the include path and `-DNVN_LAYER_API`, which the program's own files need too, since the
header's list follows the version. `NVNLAYER_LDFLAGS` exports `nvnBootstrapLoader` from the program, which is how
ScarletNX finds it. An `NVNLAYER_API` that is neither 55 nor 231 stops the build with its value. The API 55 build
links deko3d and libnx, as any deko3d program does.

In the program:

```c
#include <nvnlayer.h>

NvnProcs nvn;
char missing[64];
if (!NvnLoadProcs(&nvn, missing, sizeof missing)) { /* the lookup gave no <missing> */ }

_Alignas(8) NVNdevice device;
_Alignas(8) NVNdeviceBuilder builder;
nvn.nvnDeviceBuilderSetDefaults(&builder);
nvn.nvnDeviceBuilderSetFlags(&builder, 0);
if (!nvn.nvnDeviceInitialize(&device, &builder)) { /* NvnLayerError() says why, under deko3d */ }
```

- **Objects** are storage the program owns, of the sizes `nvn/nvn.h` gives, on an 8-byte boundary (the header
  declares them as bytes, so the program asks for the alignment), the state objects on a 4-byte one. The layer
  keeps its record there and ScarletNX keeps its own, so a program reads no byte of an object and asks every
  question through a function.
- **`NvnLayerError()`** is the first failure the API 55 build met since `NvnLayerClearError()`, a sentence with
  the value refused, or `""`. NVN's own results have no room for a reason. Under ScarletNX it is always `""`.
- **`NvnLayerSetGuard(guard)`**: deko3d ends the program when it cannot make an object. A program that can turn
  that abort into a return (edenprobe's `AbortGuarded`, a linker wrap of `diagAbortWithResult`) gives the layer its
  guard, and a bring-up that fails is then a false NVN result with its reason. Without one the abort stands.
- **`NvnLayerSetFatal(hook)`**, for the API 231 build: called with the function's name before the layer writes
  `nvnlayer: FATAL: <name> was reached: no NVN implementation took it` to the debug output and breaks.
- **`NvnLayerOwnProcs(&nvn)`** fills the structure with the layer's own functions, through no lookup: the layer's
  bodies on every machine, ScarletNX too, where they record deko3d commands like any deko3d program's. It is how
  the layer's translation is checked on a machine that otherwise never runs it. ScarletNX takes a function over
  by its address when the program first looks its name up, so a program that uses both ways is done with these
  before its first `NvnLoadProcs`. After one it returns false on every machine, with the reason.

## The API version

ScarletNX reads a game's NVN API version out of the code of its driver's `nvnDeviceGetInteger`, before the game
asks: a bounded switch on the item through a byte table scaled by four from an ADR, its cases setting w0. A C
switch compiles to another shape, so the layer writes that function in assembly in that shape, with the version's
two numbers as its first two cases; every other item goes to the build's own answer, which ScarletNX's reader sees
as an item it cannot read from code and answers from its own table.

- The API 55 build answers **55.8**, a version ScarletNX has measured a game's driver for, and not the 55.15
  ScarletNX answers when it cannot read a program's version, so a program can tell that its code was read.
- The API 231 build answers **231.16**, the version ScarletNX has an Ounce game's layouts for, which is what turns
  on its API 231 handlers and its reading of Ounce control sections.

## API 55: NVN on deko3d

### What it takes

Four hundred and twenty one functions, each recording the deko3d commands that do what NVN says it does:

- **the device**, its two modes among them. `nvnDeviceSetWindowOriginMode` and `nvnDeviceSetDepthMode` are NVN's
  to set on a device that exists, and deko3d's only as it makes one: the layer makes deko3d's device again with the
  mode, until the first object is made from the device. A mode set after that is refused, saying so.
- **queues**, **memory pools** (shader code pools among them), **2D textures** of RGBA8, BGRA8, RGBA8UI, RGBA8I, 32-bit float
  depth and 24-bit depth with stencil, of one level or more, **command buffers** and **windows**;
- **buffers**: a range of a memory pool, with the range's own address and mapping, what the buffer and its
  builder say of it, and the two cache calls, each deko3d's one call over the buffer's part of the pool;
- **the rest of a window**: what it and its builder answer of themselves (the device, the native window, the
  textures, which a builder keeps by copy); the present interval, deko3d's swap interval, on a builder and on a
  window; the crop, deko3d's crop from NVN's corner and size; the number of active textures, which the layer only
  keeps, refusing an acquire while it is fewer than the window has, since deko3d takes whichever texture the
  display hands back; and `nvnWindowAcquireTexture`, deko3d's acquire that hands the display's fence, which becomes
  the sync's;
- **what a list reports**: a counter with its time (`nvnCommandBufferReportCounter`, each of NVN's seventeen
  counters as deko3d's), a value, the counter's reset, and the device's two readings of the graphics unit's clock,
  a report's ticks turned into nanoseconds as deko3d turns them (625 over 384);
- **the render enable**, which deko3d has no command for: the 3D engine's method itself (0x554: an address and a
  mode), its words and the control list that names them written into the part of the command memory the layer
  keeps, a unit of 0x200 bytes each, so they live as long as the list;
- **what a command buffer says of its memory**: the sizes it was given; the command memory used, which is the
  layer's own part and what deko3d has written of the rest, found by the byte the layer fills that memory with
  as it hands it over (so the pool has to be one the processor reads), and the free as the rest; no control
  memory used, since deko3d keeps a list's control words itself; whether it is recording; **the memory
  callback**, which deko3d's own call for more command memory reaches while the program left it on, with its
  getters; and **lists in a list**, called or copied, both deko3d's call of a list where it lies;
- **the other clears**: of an integer target, signed and unsigned; of a region of a colour texture that is no
  target, for which the layer binds the texture alone, puts the scissor over the region, clears, and binds back
  the targets and the scissor the program last set (no view, one layer); and of a range of a buffer, whole words,
  which the graphics unit copies from a unit of the layer's memory filled with the word;
- **the copy strides' getters**, the image stride kept for its getter alone, and **the discards** of a colour
  target and of the depth target, deko3d's own;
- **the render state a list sets by itself**: viewports, scissors and viewport swizzles by number, sixteen of
  each; the depth range, which deko3d takes with a viewport's rectangle, so the layer keeps both and records the
  viewport whole on either call; the rasterizer's discard, depth clamp and provoking vertex, which deko3d keeps in
  the state it takes for NVN's polygon state, kept and recorded together the same way; the depth bounds, the
  point size, the primitive restart, the line width, conservative rasterization with its dilation and its
  subpixel bias, a patch's size and its tessellation levels, and the tiled cache's tile size and actions, each
  deko3d's own;
- **the debug layer's calls**, answered as a console's release driver answers them, the driver a game ships
  with: no callback is installed and no object database walked, no debugger is attached, no object is numbered,
  every checkpoint switch and checkpoint answers that it was taken and records nothing, a debug domain is the next
  number from 0, and a label, a marker or a group goes nowhere;
- **what a device and its builder answer of themselves** (the flags, the two modes), **the clear values a
  program registers**, taken and kept nowhere, deko3d having no such hint, **a program set from NVN 55's longer
  shader data**, whose first sixteen bytes a stage are the shader data the layer draws with, and **the handles of a
  texture alone, of a sampler alone and of a texel fetch**: the descriptor's number where deko3d's one handle has
  it, so a shader that puts the low words of a texture's and a sampler's one over the other has deko3d's handle;
- **syncs**: `nvnQueueFenceSync` and `nvnCommandBufferFenceSync` are deko3d's fence signalled by the queue or by
  the list, for the one condition deko3d has (every command done) and no flag; `nvnSyncWait` polls the fence and
  then waits, so it can tell a sync that had signalled from one it waited for, and fails at once for a sync nothing
  fenced, as NVN does; `nvnQueueWaitSync` and `nvnCommandBufferWaitSync` are deko3d's waits. An NVN sync is as
  large as deko3d's fence, so the layer keeps the fences itself, sixteen at a time;
- **events** as the processor signals them: a word in a pool the processor reaches, written or added to, with
  the builder's and the event's getters. A list that signals or waits on one is not there yet;
- **the queue's error** (`nvnQueueGetError`: none, or NVN's unknown error for a queue deko3d says faulted) and
  **what a queue builder answers of itself**: the flags, and four sizes each set or left to the driver. A queue is
  made with no flags, and deko3d's queue keeps its own sizes;
- **what a memory pool and its builder answer of themselves** (the device, the storage, the size, the flags), and
  the processor's cache of a pool: `nvnMemoryPoolFlushMappedRange` and `nvnMemoryPoolInvalidateMappedRange` are
  both deko3d's one call, which writes the range back and drops it; a range past the pool is refused;
- **what a texture and its builder answer of themselves**: the size, the levels, the samples, the format, the
  target, the flags, the pool and the offset, each as it was set. A builder keeps any number, and one the layer
  cannot make a texture of (a depth, more than one sample, a level too many) is refused when a size is asked or
  the texture is made;
- **texture views**: the levels, the layers, the format and the target, each set or left to the texture, with the
  getters that say which, and the comparison;
- **copies** between textures and buffers, with the mirror flags (between two textures a mirror is deko3d's
  blit, colour formats only) and a row stride, and through a view of levels,
  whose first level is the one copied; a view with another part set is refused in a copy;
- **programs** of the graphics stages, from DKSH modules (below);
- **samplers**: the filters, the wrap modes, the border colour (its four words, whichever of the float, signed
  and unsigned setters gave them), the compare mode and function, the level clamp, bias and snap, the greatest
  anisotropy and the reduction filter, each deko3d's own sampler field; the getters of a builder and of a sampler,
  and `nvnSamplerCompare`, which holds for two samplers of one state;
- the **texture pool** and the **sampler pool**, with where each is and how many descriptors it holds, and
  `nvnSamplerPoolRegisterSamplerBuilder`, the descriptor a sampler made from the builder would get;
- **the state objects**, every setter and getter of each: blend (the target, the functions, the equations, the
  advanced mode's four fields), channel mask, colour (a target's blending, the logic operation, the alpha test),
  depth and stencil (the depth test, write and function, and each face's stencil function and operations),
  multisample (the samples, the raster samples, alpha to coverage, coverage to colour, coverage modulation, the
  sample locations and their grid), polygon (the cull face, the front face, the mode, the offset enables), vertex
  attributes (float formats of one to four components, and RGBA8) and vertex streams (the stride and the divisor).
  A setter keeps one of NVN's numbers and complains of any other; a bind hands deko3d the same numbers, which are
  the graphics unit's in both (the layer's build asserts it), and refuses what deko3d has no state for: an advanced
  blend mode, sixteen samples, the program's own sample locations, coverage truncation. No case draws with the
  blend, stencil, logic, alpha, multisample and polygon mode fields yet: the cases read them back;
- **the binds**: a program, each state, vertex buffers, uniform buffers, textures by handle; and **the draw**,
  `nvnCommandBufferDrawArrays`, with the colour and depth clears.

A choice NVN has and the layer has not made yet (a format, a target, a flag, a view, a setter) is refused with its
value in `NvnLayerError`, or is a name the lookup does not have; none is filled in with a guess. Compute, storage
buffers, images, and instanced and indirect draws are not there yet.

`nvnDeviceGetInteger` answers the items deko3d fixes (alignments, counts, the code pool's padding) and NVN's own
count of reserved descriptors, 256 in each pool, so a program numbers its textures and samplers from the same
place on every machine. A program asks, and uses what it is told: ScarletNX answers those from a driver's table.

### Programs

uam is the only compiler a homebrew program has, and it writes DKSH modules. NVN takes a stage as two things
(`NVNshaderData`): the GPU address of its code in a `SHADER_CODE` pool, and a control section in the program's
memory. `NvnLayerShaderControl` makes both from a DKSH:

- **The code** is the DKSH's code section as it is, which the program copies into the pool on a 0x100 boundary.
  uam lays it out as a console's compiler does: 0x30 bytes, the program header, the instructions, and the
  compiler's constants on a 0x100 boundary after them.
- **The control section**, 0x900 bytes the program keeps for as long as the NVN program lives. Its first 0x800
  are what ScarletNX's `nvnProgramSetShaders` reads of a game's section, written from the DKSH's header: the magic
  `0x98761234` and a one, format 8, at 0x700 the constants' offset in the code area, at 0x704 the code area's
  bytes, at 0x708 the program's offset (0x30), at 0x714 the stage, and for a fragment stage at 0x718 whether it
  asked for the early tests, and for a tessellation evaluation stage its four declarations (the domain, the
  spacing, points or not, the winding), each in GLSL's own word for it, from the DKSH's tessellation mode word.
  Every other byte is nought. The last 0x100 are the layer's own: the DKSH's header
  with its one program's, which deko3d takes as a control section apart from the code, and the `DkShader` the API
  55 build's `nvnProgramSetShaders` makes there, an NVN program object being too small for five of them.

So the graphics unit runs what uam compiled and ScarletNX translates the same bytes. A DKSH of more than one
program, a compute program and a vertex program with a second entry point are refused.

### What a shader may use

A program compiled by uam reads the constant buffers deko3d binds, and a program compiled for NVN reads the ones a
console's NVN binds. The two differ, and ScarletNX's NVN binds NVN's. The layer binds NVN's too, which leaves a
shader two rules, and a shader written to them is one binary that draws the same through the layer, through deko3d
alone and through ScarletNX's NVN:

| Constant buffer | deko3d | NVN | The rule |
| --- | --- | --- | --- |
| 0 | the library's own: base vertex, texture and image handles, storage buffers | the driver's own, laid out otherwise | nothing of it: no `gl_BaseVertex`, `gl_BaseInstance` or `gl_DrawID`, no storage buffer, no image |
| 1 | the compiler's constants | the compiler's constants | free: both bind it from the code area |
| 2 | uniform block `binding = 0` | the stage's texture handles, slot r's in the word at 0x20 + 8 r | **textures by handle**: a block at `binding = 0`, and each sampler made from its handle there (`GL_ARB_bindless_texture`), never a `uniform sampler` of its own |
| 3 + r | uniform block `binding = r + 1` | uniform buffer slot r | **uniform blocks from `binding = 1`**: block `binding = r + 1` is NVN's slot r |

In GLSL, for a fragment stage that samples texture slot 1 and reads uniform buffer slot 0:

```glsl
#extension GL_ARB_bindless_texture : require

layout (std140, binding = 0) uniform Handles {
    uvec4 lead[2];   // 0x00 to 0x1f
    uvec4 pair[8];   // slot 2 k is pair[k].xy, slot 2 k + 1 is pair[k].zw
} handles;

layout (std140, binding = 1) uniform Paint { vec4 color; } paint;   // NVN's slot 0

void main() { outColor = texture(sampler2D(handles.pair[0].zw), inUv) * paint.color; }
```

A program that draws through deko3d alone with such a shader binds the handles itself, as a uniform buffer at id 0
of the stage (`source/drawpath.c`'s deko3d way does); through the layer, `nvnCommandBufferBindTexture` is that
bind.

A slot's handle is a constant component of a constant element, as above. uam 1.1.0 compiles a vector's component
chosen at run time (`pair[i >> 1][2 * (i & 1)]`) to a read of the vector's first word, and the console runs that
code as written, so a slot chosen at run time reads slot 2 k for both of its pair.

### Where the layer makes its own choices

- **The texture handles' memory.** A draw has to find the handles it was recorded with when the list runs, so
  each change costs a copy, 0x200 bytes a stage. A driver keeps such words in command memory, and so does the
  layer: of the memory `nvnCommandBufferAddCommandMemory` gives, deko3d gets the first three quarters and the
  layer keeps the last for these copies, which then live exactly as long as the list. When the quarter is full
  the draw is refused, saying how many copies it held; a list that changes its textures that often needs more
  command memory.
- **Orders.** Every order bit of `nvnCommandBufferBarrier` is deko3d's full barrier.
- **Defaults.** The builders' and state objects' defaults are the layer's guess where nothing public says what a
  driver's are (a sampler: nearest, clamped to its edge, every level, nothing compared; depth: no test, written, less; polygons: nothing culled,
  counterclockwise in front). A program sets every state it relies on, which is also what makes it mean the same
  under ScarletNX.
- **Storage.** A texture's storage size and alignment, a pool's page and the command memory a call takes are
  deko3d's rules, where ScarletNX answers a driver's.

## API 231: the names alone

The API 231 build gives every name of the list a body of one line, `LayerFatal("<name>")`: the program's hook,
then `nvnlayer: FATAL: <name> was reached: no NVN implementation took it` on the debug output, then a break. Each
body has an address of its own, since a runtime that takes functions by address cannot tell two names at one
address apart. `nvnDeviceGetProcAddress` and the lookup are whole, as in every build, and `nvnDeviceGetInteger`
answers the version from its switch and stops on any other item.

The objects keep the sizes of `nvn/nvn.h`, with the Ounce's own (the program builder, DLSS's three, the device's
command and message buffers) at the sizes ScarletNX's implementation keeps of each.

The list is the Switch's functions and the ones a program of this API needs first: compute dispatches, storage
buffers and images, the program builder, push constants and `nvnCommandBufferDrawMeshTasks`. A name is one line
of `NVN_LAYER_PROCS_231`.

## What a pass proves

The layer's reference is the graphics unit running what the layer recorded, never an official driver.

- **On the console and in Eden** (API 55), a case that passes says deko3d and the graphics unit did what the case
  expects for the commands the layer chose. Where the layer and a driver would choose the same commands, which is
  wherever NVN defines the result (a clear's bytes, a copy's texels, a draw's pixels), that is the console's
  answer.
- **Under ScarletNX**, the same case asks ScarletNX's NVN, and a pass says its NVN gives the pixels the console's
  graphics unit gives for those calls. It says nothing of what a driver alone decides and a program can see
  without the graphics unit: storage sizes, command memory, object layouts, the device's answers past the
  version.
- **The layer's own functions under ScarletNX** (`NvnLayerOwnProcs`) go down ScarletNX's engine methods path, as
  deko3d's own calls do: a pass there says the layer chose commands that draw what the case expects, on
  ScarletNX's reading of those commands. No console and no emulator of the graphics unit has run the API 55
  build's draws yet; the nvn family's three scene cases are what would say so there.
- **API 231** has no machine that runs its graphics unit from homebrew, so a case there has no console answer at
  all: its reference is what the program works out on the processor, and a pass says ScarletNX agrees with that.

[The nvn family](families/Nvn.md) has the cases.

## Tests

- `nvnlayer/tests/layer_test.c`, built once for each API version by `host.mk` and run by `make -f host.mk
  host-run`, with nothing of the console: every name of the list resolves through both lookups to a function of
  its own, `NvnLoadProcs` fills every field with its name's function, the version's two numbers, the failure
  keeps the first complaint, the guard. For API 231, every body stops with its own name and calls the hook once.
  For API 55, a DKSH's control section holds each field at the offset ScarletNX reads it at and meets the
  conditions ScarletNX asks of a section, and ten kinds of module are refused, each naming the value.
- The nvn family, in the NRO: the layer's functions on the console and in Eden, ScarletNX's NVN under ScarletNX,
  and one scene down three ways, the layer's own functions among them, which run its bodies under ScarletNX too.
- `tools/nvn-lookups.py edenprobe.elf`, which the CI runs on the NRO: `nvnBootstrapLoader` is exported, every
  lookup in `NvnLoadProcs` is a call through a pointer, and nothing reaches an entry point by name. ScarletNX
  takes a function when a call of the lookup returns its address, and a compiler that can tell which function a
  pointer holds calls it by name instead, guarded by a compare: GCC 15 did so for the lookups of a build that
  named the lookup's function in the same file, and the functions looked up that way kept the layer's bodies
  beside ScarletNX's own. The pointers in `NvnLoadProcs` are volatile for that, and the check reads the result.

## Where its numbers come from

- `include/nvn/nvn.h` is h1k421's reconstruction of NVN's types from one game's debug information (MIT, 2020), as
  ScarletNX carries it (`src/nx/gpu/nvn/nvn.h`, from nxnative's `dm/gfx/nvn/include/nvn.h`), with the format
  numbers written out from ScarletNX's `nvn_tic_format.inc` and, for API 231, the Ounce's object sizes from the
  same header.
- The prototypes of `include/nvnlayer_procs.h` follow ScarletNX's `gpu/nvn/nvn.hook`.
- The control section's offsets are the ones ScarletNX's `nvn_object.c` reads (`stage_of`), and the constant
  buffers' layout is its `docs/layers/gpu/nvn.md`, "Constant buffer banks".
- The DKSH's layout and the constant buffers deko3d binds are deko3d's `source/dksh.h` and
  `source/cmd_bind_common.cpp`.

None of them holds driver code: names, numbers, sizes and argument types.
