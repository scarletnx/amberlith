# On the Vita

## Index

* [The VPK](#the-vpk)
  * [Build it](#build-it)
  * [Run it](#run-it)
  * [What it runs](#what-it-runs)
* [Where it leaves its report](#where-it-leaves-its-report)

## The VPK

`vita/` is Amberlith's Vita frontend ([Frontends](Frontends.md)): one VPK, title `AMBR00001`, whose program holds the
engine and the libc family's cases, the same C the NRO runs (`source/portable.txt`), beside the Vita's own suite of
its GPU, gxm. It runs on a console, in Vita3K and in ScarletNX's Vita runtime (`scarletnx-vita`), and all of them
write the same report, so a case that passes on the console and fails in an emulator names something the emulator does
differently.

### Build it

```sh
export VITASDK=$HOME/vitasdk          # where the SDK's installer, vdpm, put it
make -f vita.mk                       # build-vita/amberlith.vpk
```

VitaSDK installs in a folder of the user's, no root; the CI builds with the SDK's build of 2026-10-15
(`sdk-snapshot-20261015.850.1`), held to its release's hash. The SDK has no shader compiler, so the gxm suite draws with
vita2d's clear and texture programs as their author compiled them: `vita.mk` fetches the four objects at a fixed commit
and checks each against `vita/shaders.sha256` before it links them ([Credits](Credits.md)). C only, in C23 (`-std=c23
-Wall -Wextra -Werror`), the engine's C11 compiling unchanged under it. Two things the Vita's C library and kernel do
not give the engine as the other machines do, which the frontend gives itself:

- **C99's size formats.** VitaSDK's newlib is built without them (its `newlib.h` leaves `_WANT_IO_C99_FORMATS`
  undefined): it prints `%zu`, `%jd` and `%td` as their letters and leaves the arguments out of step, so a `%s` after
  one reads a number as a pointer. The engine prints sizes with `%zu`, so `vita/printf.c` wraps the printf family
  (`--wrap` in `vita.mk`): `z` becomes `l`, `j` `ll`, and `t` goes, which are the same widths there. `%a`, C99's too,
  stays as the library has it; the engine prints none.
- **A larger main thread's stack.** The kernel gives 256 KiB when the program names none, and the judge's support
  cases keep larger records on it: `sceUserMainThreadStackSize` asks for 4 MiB.

The build names itself from `VERSION` and git as the NRO's does
([Versions](Versions.md)), so a report's folder is the commit's.

### Run it

- On a console: install the VPK with VitaShell, and open Amberlith from the home screen.
- In ScarletNX: `scarletnx-vita build-vita/amberlith.vpk`, which installs it in its game folder and runs it. Its
  `--resolution=N` draws every scene N times larger; the gxm suite's answers are the same at every N, which is how
  that setting is checked.
- In Vita3K: install the VPK, and run it from its list.

No screens: a run leaves once its report is closed, whatever the mode file says, as Android's does.

### What it runs

The judge first, always; then the libc suite, the C library alone (VitaSDK's newlib); then gxm, the Vita's GPU through
sceGxm ([Graphics](families/Graphics.md#gxm)). The system and socket suites are not among its steps yet: the first
needs VitaSDK's threads held to what the suite asks, the second the network brought up.

## Where it leaves its report

`ux0:data/amberlith/`, laid out as `sdmc:/amberlith/` is on the Switch ([The report](Report.md)): the mode file, the
crash list and `runs/<commit>/report.jsonl`. Every line also goes to the kernel's log (`sceClibPrintf`), which an
emulator prints.
