# On the 3DS

## Index

* [The 3DSX](#the-3dsx)
  * [Build it](#build-it)
  * [Run it](#run-it)
  * [What it runs](#what-it-runs)
* [Where it leaves its report](#where-it-leaves-its-report)

## The 3DSX

`3ds/` is Amberlith's 3DS frontend ([Frontends](Frontends.md)): one 3DSX, a homebrew program the Homebrew Launcher
opens, whose program holds the engine and the libc family's cases, the same C the NRO runs (`source/portable.txt`). Every
machine that runs it writes the same report, so a case that passes on the console and fails in an emulator names
something the emulator does differently.

### Build it

```sh
export DEVKITPRO=/opt/devkitpro       # where devkitPro's installer, or its container, puts it
make -f 3ds.mk                        # build-3ds/amberlith.3dsx
```

It needs devkitARM and libctru: devkitPro's `3ds-dev` packages, or its `devkitpro/devkitarm` container, which the CI
builds in, pinned by tag and digest (`20260610`: devkitARM r68 with GCC 16.1.0, libctru 2.7.0, 3dstools 1.3.1). C only,
in C23 (`-std=c23 -Wall -Wextra -Werror`), the engine's C11 compiling unchanged under it, libctru's headers taken as the
system's (`-isystem`). The Homebrew Launcher shows `3ds/icon.png`, the cover drawn at 48 by 48 by `make icon`. Two
things the 3DS gives a program differently from the other machines, which the frontend sets itself:

- **A larger main thread's stack.** A 3DSX gets 32 KiB when the program names none, and the judge's support cases keep
  larger records on it: `__stacksize__` asks for 4 MiB.
- **A folder for temporary files.** The support suite writes its checks under `TMPDIR`; the 3DS has no `/tmp`, so the
  frontend makes `sdmc:/amberlith/tmp` and names it there.

devkitARM's C library has `int32_t` as a `long`, where every other machine Amberlith builds for has an `int`, so a
32-bit value goes to `printf` through a cast or a `PRIu32`, never as `%u` alone: the 3DSX's build, with `-Werror`,
refuses one that does not.

The build names itself from `VERSION` and git as the NRO's does ([Versions](Versions.md)), so a report's folder is the
commit's.

### Run it

- On a console: copy the 3DSX to `sdmc:/3ds/`, and open Amberlith from the Homebrew Launcher.
- In a 3DS emulator that opens a 3DSX: open the file. The report lands in the emulator's own SD card folder.

ScarletNX's 3DS driver runs applications (a CIA, a cartridge image, an NCCH), not a 3DSX yet.

No screens to choose from: the top screen names each step as it starts, and a run leaves once its report is closed,
whatever the mode file says, as the Vita's does.

### What it runs

The judge first, always; then the libc family whole: libc, the C library alone (devkitARM's newlib); system, libctru's
threads and `clock_gettime`; socket, soc:U on loopback, brought up with a buffer of the frontend's own
([libc](families/Libc.md)). The 3DS's own suites come after these as they are written.

## Where it leaves its report

`sdmc:/amberlith/`, laid out as on the Switch ([The report](Report.md)): the mode file, the crash list and
`runs/<commit>/report.jsonl`. Every line also goes to the kernel's debug string (`svcOutputDebugString`), which an
emulator logs and a console without a debugger drops.
