# The probe as a mod of a game

## Index

* [What a result says](#what-a-result-says)
* [Building it](#building-it)
* [What it asks of the game](#what-it-asks-of-the-game)
* [The start](#the-start)
  * [The screens](#the-screens)
* [What it runs](#what-it-runs)
* [The report](#the-report)
* [On a console](#on-a-console)
* [Under an emulator](#under-an-emulator)
* [Under ScarletNX](#under-scarletnx)

The same engine and the nvn family's cases, built as one more module of a game's program: a `subsdk9` the console's
custom firmware loads into the game's process beside the game's own modules. A homebrew program has no NVN, each
game carries its own driver; loaded into a game, the probe looks that driver up by name, as the game does, and runs
its cases through it before the game's own code starts.

## What a result says

- **On a console**, the answers are the console's own NVN on its graphics unit, of the SDK that game was built
  with. A case that fails there expects something the driver does not do: the expectation is the wrong one.
- **Under an emulator** that runs the game's own code and emulates the graphics unit, as Eden does, the answers are
  the game's driver's, on the emulator's graphics unit: a case that passes on a console and fails there is the
  emulator's to answer.
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

It leaves `amberlith-mod/exefs/subsdk9` and `main.npdm`, and `amberlith-mod.elf` beside them. A release's
`amberlith-mod-<version>.zip` ([Releases](Releases.md)) holds the same two files in `exefs/`. The sources are the
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
  first NVN device;
- nn::vi's Initialize, OpenDefaultDisplay, CreateLayer, GetNativeWindow, DestroyLayer, CloseDisplay and Finalize,
  for the mod's own window.

The mounts and all after them are weak imports: in a game whose SDK has none of a name the address is nought, the
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
- the mod's window: nn::vi is initialized, a layer of the mod's own is made on the default display, and an NVN
  window is made on its native window as a game makes its own. It closes as the run ends. The mode file's
  `nowindow` keeps it closed and the game keeps its screen; a game whose SDK lacks one of the names, or a call that
  fails, leaves no window, the report naming the name or the result;
- a detailed run draws [the screens](#the-screens) there; a quick one, a script's, draws nothing, and the first of
  the nvn suite's window cases opens the window for its own frames;
- the process ends (`svcExitProcess`). The game never reaches its main: no memory of its own is in use, no device
  or queue of its own exists, and nothing has to be paused.

One thread runs it all, so the C library's locks do nothing here.

### The screens

A detailed run shows the plain program's screens on the mod's window ([The screens](UI.md)), composed by the same
code on the processor and copied into the window's texture through the game's NVN, on a device of their own:

- the start screen, the rows as the mode file leaves them, for ten seconds;
- the running screen, the step, the case and the counts as the cases go;
- the results, the overview of the run, for ten seconds once the report is closed.

A game's process gives the mod no pad, so no screen offers a key or a tab: the start screen says the run begins by
itself, and the results that they close by themselves. The one thread that runs the cases draws the running screen,
between cases only, so a long case leaves it still (the report says so). While the screens hold the window the nvn
suite's window cases are skipped, as under the plain program's screen; `quick` in the mode file runs them, and
`noprogress` keeps the running screen off with the window still held.

Before the run the driver is asked its API version and given its memory, as the `mod` suite's cases below then say,
and the screens come up only on a driver of this build's API that has every function they call. When they do not
come up, a note says why and the run goes on without them, the window cases on the window:

```
screens: none, since the game's driver has no nvnWindowAcquireTexture, which the screens call; the nvn suite's window cases have the window
```

A game whose driver ends the process while the screens come up ends every launch there: `quick` or `nowindow` in the
mode file runs without them.

A card that cannot be mounted leaves no place for a report: the mod says so on the debug output, with each mount's
result, and ends the process.

## What it runs

The judge's selfcheck and support steps, then the `nvn` family, which runs when the mode file names no family, then
the `game` and `demo` families when the mode file names them. The judge's reference step is left out: it holds the drawing suites' references, which no case here is judged by, and
under ScarletNX with the game recompiled unoptimized it alone took over ten minutes.

- the `mod` suite, three cases of the mod's own: the card came through the game's SDK; the driver answers its API
  version with no device, and it is one whose objects this build has (53 to 55: `nvn/nvn.h`'s types were taken
  from a 53.105 driver); the driver was given its allocator and 32 MiB of the module's data before the first
  device;
- the `nvn` suite's looked up way, its window cases on the mod's own window (above);
- the game scene, the Wimsy levels the mode file names, and the demos, each listed as skipped, `unavailable`, on
  its first case, as the plain program names it: their frames are deko3d's and their programs uam's, and the mod has
  neither, only the game's NVN.

```
{"type":"case","suite":"mandelbrot","name":"a graphics device with compute, the programs and a way to the display come up","result":"skip","reason":"unavailable","detail":"the demos draw through deko3d, which the mod has not: in a game's process it draws only through the game's NVN"}
```

A driver of another API major fails the second case with both numbers, and the nvn suite is not run: its objects
would be made in storage of another size.

A driver has the names of its own API version, and the suite looks up every version's. A call of a name the game's
driver has no function for does nothing and returns nought ([the layer](NvnLayer.md#a-games-own-driver)), so the
case goes on to its end and finalizes what it made, and the run goes on in the same launch. What the case came to
then judged no answer of the driver's, so its line is a skip, `unavailable`, with the functions it called:

```
{"type":"case","suite":"nvn","name":"a queue builder gives back its memory and priority, and a queue's memory counts are nought once reset","tier":"none","result":"skip","reason":"unavailable","detail":"the game's driver has no nvnQueueBuilderSetPrimaryQueue, which the case called: nothing was done and nought came back, so the verdict the case came to judged no answer (pass)"}
```

The case is asked as it ends (the report's `ReportSetUnasked`), and each lacked function is said on the debug
output as it is first called.

## The report

Where the NRO's is, `sdmc:/amberlith/runs/<build>/report.jsonl`, with the same mode file and the same journal: a
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

Then what that driver lacks, every name, over as many notes as they take, and as the nvn step ends, each lacked
function a case called with the cases it made unavailable:

```
host: the game's driver has no function for 11 of the 523 names this build looks up (1 of 1): nvnQueueAcquireTexture, nvnCommandBufferSetControlMemoryCallbackEnabled, ...
host: nvnQueueBuilderSetPrimaryQueue, which the game's driver has not, made 1 case unavailable: "a queue builder gives back its memory and priority, and a queue's memory counts are nought once reset"
```

`tools/mod-lacked.py REPORT...` reads those notes of one report or several: for each driver its lacked names, the
cases each took and the names no case calls, then each name with the drivers that lack it, and with `--csv FILE` a
row a name and case. It refuses a report whose notes and case lines do not agree, with the numbers.

## On a console

With Atmosphere, whose loader takes a program's modules and its `main.npdm` from the card before the game's own
(`atmosphere/contents/<program id>/exefs/`):

1. `make -f mod.mk`, or unzip a release's `amberlith-mod-<version>.zip`.
2. Copy `amberlith-mod/exefs/subsdk9` and `amberlith-mod/exefs/main.npdm` to
   `atmosphere/contents/<program id>/exefs/` on the card, `<program id>` the game's sixteen hexadecimal digits.
   A game that has a `subsdk9` of its own, or a folder that holds another mod's, takes the module under another
   free name from `subsdk0` to `subsdk8`.
3. Write `amberlith/mode` on the card: `quick nvn label=<a name for this game's driver>`.
4. Start the game, once. The screen stays black while the cases run, and the console goes back to its menu when the
   report is closed. A case that calls a function the game's driver has not ends unavailable and the run goes on,
   so one launch is the whole run; only a case that ends the program (the console's error screen) asks for another
   launch, which goes on after it.
5. The report is `amberlith/runs/<build>_<label>/report.jsonl` on the card, whole once its last line is the
   summary. `tools/mod-lacked.py` on it says what the driver lacks and which cases that took. Take the two files
   out of the folder to play the game again.

`main.npdm` is the same file for every game: Atmosphere's loader writes the launched program's ID into the one it
reads from the card. It is there for the memory card, which a game's own descriptor gives no access to.

Not run on a console yet. What only a console can say: whether the driver makes a device before the game's main
(the mod calls nn::oe::Initialize and gives the driver its memory first, as a game's start does), and what the menu
shows when a game's process ends this way.

## Under an emulator

Eden takes a game's mods from `load/<program id>/<mod name>/exefs/` in its data folder: copy `subsdk9` and
`main.npdm` to `load/<program id>/amberlith/exefs/`, write the mode file in its card, `sdmc/amberlith/mode`, and
start the game. Its loader reads the module beside the game's own and takes the mod's `main.npdm` for the game's, so
the process runs under the descriptor's program ID; the run ends by itself, the report in `sdmc/amberlith/runs/`.
A case that never returns there is ended by closing the emulator, and the next launch goes on after it.

## Under ScarletNX

A mod of a game there is a folder of the game's mods folder ([ScarletNX's usage](https://github.com/scarletnx/ScarletNX/blob/main/docs/user/usage.md#mods)):
`<game>_data/mods/amberlith/exefs/subsdk9` and `main.npdm`, and the mode file in the game's card,
`<game>_data/sdmc/amberlith/mode`. The driver recompiles the game with the module the next time it plays it, and the
run ends by itself with the report in `<game>_data/sdmc/amberlith/runs/`.
