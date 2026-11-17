# The probe as a mod of a game

## Index

* [What a result says](#what-a-result-says)
* [Building it](#building-it)
* [What it asks of the game](#what-it-asks-of-the-game)
* [The start](#the-start)
* [What it runs](#what-it-runs)
* [The report](#the-report)
* [On a console](#on-a-console)
* [Under ScarletNX](#under-scarletnx)

The same engine and the nvn family's cases, built as one more module of a game's program: a `subsdk9` the console's
custom firmware loads into the game's process beside the game's own modules. A homebrew program has no NVN, each
game carries its own driver; loaded into a game, the probe looks that driver up by name, as the game does, and runs
its cases through it before the game's own code starts.

## What a result says

- **On a console**, the answers are the console's own NVN on its graphics unit, of the SDK that game was built
  with. A case that fails there expects something the driver does not do: the expectation is the wrong one.
- **Under ScarletNX**, which runs a game's NVN calls itself, the answers are ScarletNX's at the game's API version:
  the same cases as the NRO's, which reach ScarletNX's NVN through [the layer](NvnLayer.md).

The cases are the `nvn` suite's by the looked up way: the device, pools, textures, copies, state objects, queues,
syncs and events. None of them has a shader program. The command and scene cases stay out: their programs are
uam's code with a control section the layer writes as ScarletNX reads one, and a game's own driver reads the
console's compiler's.

## Building it

```sh
make -f mod.mk          # DEVKITPRO set, as for the NRO
make -f mod.mk check    # the same, then tools/mod-imports.py on the module
```

It leaves `edenprobe-mod/exefs/subsdk9` and `main.npdm`, and `edenprobe-mod.elf` beside them. The sources are the
engine's (`source/portable.txt`, less the libc family), the card's device over nn::fs (`source/sdk_card.c`), the
nvn suite, `mod/` and the layer built for [a game's own driver](NvnLayer.md#a-games-own-driver). The toolchain is
the NRO's: devkitA64 and its C library, libnx's headers and of its code the kernel's calls alone, `elf2nso` and
`npdmtool`. No framework for mods is linked: what those give a mod is a hook into the game's code, and the probe
places none.

## What it asks of the game

Names, and nothing else: no address, no offset and no program ID is in the module, so one `subsdk9` goes into any
game. `mod/imports.txt` lists every symbol, and the game's own dynamic linker binds each to the module of the game
that exports it:

- `nvnBootstrapLoader`, the driver;
- nn::fs's file functions and one of its two mounts of the memory card, as [the hooks mode](Hooks.md) asks them,
  here answered by the game's SDK;
- nn::oe::Initialize, nv::SetGraphicsAllocator and nv::InitializeGraphics, what a game's start calls before its
  first NVN device.

The mounts and the last three are weak imports: in a game whose SDK has none of a name the address is nought, the
mod does not call it, and the report says which it went without. `tools/mod-imports.py` holds the built module to
the list, to exporting nothing (a name a game's module exports is found before the SDK's own, and the module's C
library has the SDK's names), to plain relocations and to nothing of libnx's start; `make -f host.mk host-run` runs
its tests.

## The start

The game's dynamic linker calls the module's `DT_INIT` (`mod/start.c`) on the game's main thread, after the SDK's
own start and before the game's constructors and its main. From there:

- the C library's heap is 64 MiB of the module's own zeroed data, so nothing comes out of the heap the game sized;
- the card is mounted as `sdmc` through the game's nn::fs and becomes the C library's `sdmc:` device;
- the run goes as on every frontend ([Frontends](Frontends.md)): the mode file, the journal, the steps, the
  report;
- the process ends (`svcExitProcess`). The game never reaches its main: no memory of its own is in use, no device
  or queue of its own exists, and nothing has to be paused.

One thread runs it all, so the C library's locks do nothing here.

A card that cannot be mounted leaves no place for a report: the mod says so on the debug output, with each mount's
result, and ends the process.

## What it runs

The judge's selfcheck and support steps, then the `nvn` family, which runs when the mode file names no family. The
judge's reference step is left out: it holds the drawing suites' references, which no case here is judged by, and
under ScarletNX with the game recompiled unoptimized it alone took over ten minutes.

- the `mod` suite, three cases of the mod's own: the card came through the game's SDK; the driver answers its API
  version with no device, and it is one whose objects this build has (53 to 55: `nvn/nvn.h`'s types were taken
  from a 53.105 driver); the driver was given its allocator and 32 MiB of the module's data before the first
  device;
- the `nvn` suite's looked up way. Its window cases are skipped as unavailable: a native window is the game's to
  make, and the mod makes none.

A driver of another API major fails the second case with both numbers, and the nvn suite is not run: its objects
would be made in storage of another size.

A driver has the names of its own API version, and the suite looks up every version's. The names a game's driver
has no function for are a note of the report, and the suite runs: a case that calls one of them ends the launch
there, its last line naming the function, and the next launch goes on after that case, which the report keeps as
crashed ([When the program ends in the middle](Crashes.md)).

## The report

Where the NRO's is, `sdmc:/edenprobe/runs/<build>/report.jsonl`, with the same mode file and the same journal: a
launch that ends inside a case goes on after it at the next launch. A `label=` in the mode file keeps each game's
runs in a folder of their own. Each report says what it was made on:

```
frontend: a module of a game, run before the game's main; the card by nn::fs::MountSdCardForDebug
host: the NVN driver is the game's own, in a module of SDK 10.4.0 and build id 0123456789abcdef0123456789abcdef01234567
host: the game's NVN driver answers API 55.12
```

The SDK version is the label in the read-only data of the module that holds `nvnBootstrapLoader`, and the build id
that module's build id note. They name the driver, which is what two reports are compared by; nothing names the
game.

## On a console

With Atmosphere, whose loader takes a program's modules and its `main.npdm` from the card before the game's own
(`atmosphere/contents/<program id>/exefs/`):

1. `make -f mod.mk`.
2. Copy `edenprobe-mod/exefs/subsdk9` and `edenprobe-mod/exefs/main.npdm` to
   `atmosphere/contents/<program id>/exefs/` on the card, `<program id>` the game's sixteen hexadecimal digits.
   A game that has a `subsdk9` of its own, or a folder that holds another mod's, takes the module under another
   free name from `subsdk0` to `subsdk8`.
3. Write `edenprobe/mode` on the card: `quick nvn label=<a name for this game's driver>`.
4. Start the game. The screen stays black while the cases run, and the console goes back to its menu when the
   report is closed.
5. The report is `edenprobe/runs/<build>_<label>/report.jsonl` on the card. Take the two files out of the folder to
   play the game again.

`main.npdm` is the same file for every game: Atmosphere's loader writes the launched program's ID into the one it
reads from the card. It is there for the memory card, which a game's own descriptor gives no access to.

Not run on a console yet. What only a console can say: whether the driver makes a device before the game's main
(the mod calls nn::oe::Initialize and gives the driver its memory first, as a game's start does), and what the menu
shows when a game's process ends this way.

## Under ScarletNX

A mod of a game there is a folder of the game's mods folder ([ScarletNX's usage](https://github.com/scarletnx/ScarletNX/blob/main/docs/user/usage.md#mods)):
`<game>_data/mods/edenprobe/exefs/subsdk9` and `main.npdm`, and the mode file in the game's card,
`<game>_data/sdmc/edenprobe/mode`. The driver recompiles the game with the module the next time it plays it, and the
run ends by itself with the report in `<game>_data/sdmc/edenprobe/runs/`.
