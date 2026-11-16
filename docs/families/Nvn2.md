# The nvn2 suite

## Index

* [The cases](#the-cases)
* [The Mandelbrot, in three stages](#the-mandelbrot-in-three-stages)
* [The matrix instruction](#the-matrix-instruction)

What a Switch 2 program asks of NVN that a Switch's does not: programs made through the builder, of the Switch
2's own shader code, a compute dispatch that writes a storage buffer, and a draw whose geometry a mesh program
writes. Only the probe's Switch 2 build runs it ([The probe as a Switch 2 program](../Ounce.md)), in the nvn
family, after the `nvn` suite. Eighteen cases.

Every shader program is assembled by hand from its instructions ([Switch 2 shader programs](../Sm86.md)), and
every answer is worked out on the processor with integers alone, so a case has one right answer on every machine.
What a pass says, and what it cannot, is in [The probe as a Switch 2 program](../Ounce.md#what-a-pass-says).

## The cases

| Case | What it asks | Held to |
| --- | --- | --- |
| a device, a queue, a command buffer, their pools and a 256x256 target with its depth come up | the objects every case below uses, the program's NVN looked up at API 231 | each call's answer |
| a compute program assembled by hand makes a program through the builder | `nvnProgramBuilderSetShaders` and `nvnProgramInitializeFromBuilder` with one stage, its code in a code pool and its control section as the assembler wrote it | the call's answer |
| a dispatch's writes to a storage buffer, as the processor reads them once the queue finishes | `ounce/shaders/scale.comp.sm86`: 128 invocations in two blocks of 64, each reading its word of storage buffer 0, bound with `nvnCommandBufferBindStorageBuffer`, and writing three times it plus its index 0x200 bytes on; the program reads the buffer's memory once `nvnQueueFinish` returns | every word, exact; the output starts as 0xdeadbeef, so a dispatch that wrote nothing is told from one that wrote wrong |
| a dispatch's writes to a storage buffer, as the graphics unit copies them to another buffer | the same output, copied with `nvnCommandBufferCopyBufferToBuffer` and read there: what the buffer holds on the graphics unit's side, whatever the processor saw | every word, exact |
| the same dispatch, its storage buffer bound by the call that takes several; ... by the deferred call; ... its counts of blocks read by the graphics unit from a buffer (three cases) | `nvnCommandBufferBindStorageBuffers`, `BindStorageBuffersDeferred` (its range left where it is until the list has run) and `nvnCommandBufferDispatchComputeIndirect` | the same words, copied out by the graphics unit |
| the Mandelbrot's compute, mesh and fragment programs make two programs | one program of the compute stage, one of the mesh and fragment stages | the calls' answers |
| a compute program counts the Mandelbrot set's steps for every cell of a grid | `mandel.comp.sm86`, a block a row; the counts copied out by the graphics unit | every cell's count, exact (`source/mandelref.c`, `MandelCount`) |
| a mesh program draws the counts as bars, a fragment program colours them and the depth test keeps the nearest | `mandel.mesh.sm86` and `mandel.frag.sm86`, drawn with `nvnCommandBufferDrawMeshTasks`, a workgroup a row | every pixel, exact (`MandelExpected`) |
| the same picture by a mesh draw of a grid of workgroups; ... that reads its numbers; ... of a grid that reads its numbers; ... by two mesh draws read from one buffer, a half of the rows each; ... by the mesh draws of a grid read from one buffer; ... by as many mesh draws as a count in a buffer says, a half each; ... by as many mesh draws of a grid as a count in a buffer says (seven cases) | the same draw through `nvnCommandBufferDrawMeshTasks3D`, `DrawMeshTasksIndirect`, `DrawMeshTasks3DIndirect`, `MultiDrawMeshTasksIndirect`, `MultiDrawMeshTasks3DIndirect`, `MultiDrawMeshTasksIndirectCount` and `MultiDrawMeshTasks3DIndirectCount`: a count and a first workgroup, or three counts for a grid, read by the graphics unit from the data pool; the draws of several take the rows in two halves, the second from workgroup 14 | the same picture, every pixel |
| one warp's matrix instruction multiplies a 16 by 16 matrix of halves by a 16 by 8 one into floats | `hmma.comp.sm86`: `HMMA.16816.F32` over a block of 32 invocations, each lane holding its part of the matrices | the product's 128 floats, bit for bit, against the integers' sums |

## The Mandelbrot, in three stages

One picture made by the three stages a Switch 2 game's 3D scene uses together.

- **Compute.** A grid of 32 columns by 28 rows over the plane, c from -2.25 - 1.5i in steps of 3/32 and
  439/4096. For each cell, how many of sixteen steps of z = z * z + c keep z within the circle of radius 2: fixed
  point with twelve fraction bits, the sixteen steps written out with no branch, a cell that left neither counting
  nor moving again, so no product passes 31 bits. A block of 32 invocations is a row; the count goes to the cell's
  word of storage buffer 0, whose address the program takes from the driver's table (`c[0][0x310]`).
- **Mesh.** A workgroup is a row and each of its 32 invocations a column. It reads the cell's count from the same
  buffer, bound to the mesh stage (`c[0][0x770]`), and writes one rectangle, four vertices and two triangles: 8
  pixels wide, from 2 pixels a count above the row's line to the line below, at a depth that is less the nearer
  the row. The depth test (less) then leaves each bar's top and what no nearer bar covers: the counts as a relief
  seen from the front and above.
- **Fragment.** Red is the count over 17, which eight bits hold as 15 a count with nothing near a half to round
  either way; green a quarter; no blue. Where no bar is, the clear colour, blue.

The device keeps the modes it starts with: clip space's y of -1 is the first row of the target's memory, the
picture's top as the reference counts rows, and a lesser z is the nearer whichever way depth is mapped.

Every edge is on a pixel's edge of the 256 by 256 target and every vertex of a rectangle carries the same count, so
neither the rasteriser's rule for a shared edge nor the interpolation can move a byte. The reference
(`source/mandelref.c`) works the counts out with the same integer steps and looks, for each pixel, for the nearest
row whose bar reaches it; the reference suite holds both to numbers worked out apart, the counts by unbounded
integers and the picture by painting the bars from the furthest row to the nearest ([The judge](Judge.md)).

The picture is drawn from the counts the dispatch left in the buffer, and the count case reads them through a
copy the graphics unit makes: neither leans on what the processor reads of a buffer a dispatch wrote, which the
scale program's first case asks alone.

## The matrix instruction

`HMMA` multiplies two matrices held across a warp's 32 lanes and adds a third. The case gives it A, 16 by 16, and B,
16 by 8, of half floats, and takes the product in floats. Their elements are quarters from -6/4 to 6/4, never
nought, drawn from a generator: every product and every sum of sixteen is exact in a float, so the order the
graphics unit adds them in cannot show, and a sum of nought is +0. The reference is the sum in integers.

What the case holds beyond the arithmetic is where each lane keeps its part of the matrices, which is the
instruction's public description (PTX's `mma.sync` for m16n8k16): lane l is thread t = l % 4 of group g = l / 4;
its four words of A are rows g and g + 8 at columns 2t and 2t + 1, then the same rows at columns 2t + 8 and
2t + 9; its two words of B are rows 2t and 2t + 1, then 2t + 8 and 2t + 9, of column g; its four floats of the
product are rows g and g + 8 at columns 2t and 2t + 1. ScarletNX's translator is the only reading of the
instruction's arithmetic there is to hold it to ([Switch 2 shader programs](../Sm86.md)).

ScarletNX draws mesh draws only with its `mesh_draws` setting on (`--mesh_draws=1`), which is experimental and off
unless set: the suite is run with it.
