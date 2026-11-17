# The probe as a Switch 2 program

## Index

* [What makes it a Switch 2 program](#what-makes-it-a-switch-2-program)
* [Building it](#building-it)
* [Running it](#running-it)
* [What a pass says](#what-a-pass-says)

The same engine, built as a program of the Switch 2: an ExeFS folder of `main` (an NSO) and `main.npdm`, which
ScarletNX recompiles and runs as it does a game's. It asks what a Switch 2 program asks of a runtime that a
Switch's does not, first of all NVN at API 231 with shader programs of the Switch 2's own code.

No console runs it. A homebrew program does not start on a Switch 2, and no other emulator runs one of its
programs, so every case here is held by ScarletNX alone, against what the processor works out
([what a pass says](#what-a-pass-says)).

## What makes it a Switch 2 program

- **The NPDM.** A machine tells the console from the descriptor (ACID) in `main.npdm`: a Switch 2's has its magic
  0x300 bytes in, behind a longer signature and key, where a Switch's has it at 0x200. ScarletNX reads that and
  nothing else to decide; an NRO always gets the Switch's. `tools/npdm-ounce.py` writes one (`write OUT`, with the
  name, the title, the main thread's stack, priority and core as options) and reads one back (`read FILE`); the
  toolchain's npdmtool writes the Switch's layout only. The signature and key are nought: nothing here is signed.
- **The NVN version.** The layer is built for API 231 ([The NVN layer](NvnLayer.md)): the names a Switch 2 game
  looks up, its `nvnDeviceGetInteger` answering 231.16, and under each name a body that ends the run saying which
  function was reached, because no library drives the Switch 2's graphics unit. ScarletNX takes every one of them.
- **The shader programs** are the Switch 2's code, assembled by hand ([Switch 2 shader programs](Sm86.md)).

The rest is the Switch's: the same compiler and libnx, the same instruction set, the same start through the SDK's
functions ([The hooks mode](Hooks.md)). What a Switch 2's processor adds is for a later case to ask.

## Building it

```sh
make -f ounce.mk          # DEVKITPRO set, as for the NRO
```

It leaves `edenprobe-ounce/exefs/main` and `main.npdm`, and `edenprobe-ounce.elf` beside them. The sources are the
engine's (`source/portable.txt`), the console's platform file and its start through the SDK's functions, the nvn
suite, `ounce/frontend.c`, `ounce/t_nvn2.c` and the layer at API 231. The programs under `ounce/shaders/*.sm86`
are assembled by `tools/sm86asm.py` into the build.

The heap is 256 MiB, set by the frontend: libnx's own start takes all the memory the kernel offers a program, which
for a Switch 2 program is gigabytes.

## Running it

In ScarletNX, as an extracted game's ExeFS is run: recompile the folder, then start the program it wrote. A release's
`edenprobe-ounce-<version>.zip` ([Releases](Releases.md)) holds the same folder, `exefs/`: unzipped in
`edenprobe-ounce/`, it is the one the commands below name.

```sh
scarletnx -r edenprobe-ounce/exefs -o edenprobe-ounce --outdir GAMES
(cd GAMES/edenprobe-ounce && ./edenprobe-ounce -H)        # -H: no window
```

The mode file and the report are where the console's are, `edenprobe/mode` and `edenprobe/runs/` on the program's
SD card ([The mode file](ModeFile.md)). The frontend has no screens: every run leaves once its report is closed,
and a file with no `quick` says so in the report. Its steps are the judge's three and the nvn family, which runs
when the file names no family: the `nvn` suite, less the scene (a Switch program's shaders, and deko3d), and
`nvn2` ([The nvn2 suite](families/Nvn2.md)).

## What a pass says

That ScarletNX, given the program's bytes, does what the probe's reading of them says. The reading is ScarletNX's
own documentation and decode table, so a pass is the runtime agreeing with itself across two programs written
apart, the assembler and the processor's arithmetic on one side, the decoder, the translator and the host's
graphics unit on the other. It catches a translator that computes something else, a buffer bound to the wrong
place, a stage read as another. It cannot catch a row of the table that is wrong about the hardware: only a game
can, by drawing wrong.
