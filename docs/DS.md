# On the DS

## Index

* [The NDS](#the-nds)
  * [Build it](#build-it)
  * [Run it](#run-it)
  * [What it runs](#what-it-runs)
  * [What fits in its memory](#what-fits-in-its-memory)
* [Where it leaves its report](#where-it-leaves-its-report)

## The NDS

`nds/` is Amberlith's DS frontend ([Frontends](Frontends.md)): one NDS, a card image whose ARM9 program holds the
engine and the libc family's cases, the same C the NRO runs (`source/portable.txt`), with calico's own ARM7 program
beside it. Every machine that runs it writes the same report, so a case that passes on the console and fails in an
emulator names something the emulator does differently.

### Build it

```sh
export DEVKITPRO=/opt/devkitpro       # where devkitPro's installer, or its container, puts it
make -f nds.mk                        # build-nds/amberlith.nds
```

It needs devkitARM, libnds and calico: devkitPro's `nds-dev` packages, or its `devkitpro/devkitarm` container, which the
CI builds in, pinned by tag and digest (`20260610`: devkitARM r68 with GCC 16.1.0, libnds 2.0.2, calico 1.2.0, ndstool
2.3.1). C only, in C23 with GNU's `asm` (`-std=gnu23 -Wall -Wextra -Werror`): libnds's and calico's headers write
inline assembly with the `asm` keyword, which strict C23 does not have; this program's own code holds to
[Code style](CodeStyle.md) all the same. The headers are taken as the system's (`-isystem`). The code is Thumb, each
function and object in a section of its own, so the link keeps only what the DS's steps reach. The header names the
program (`AMBERLITH`) and carries homebrew's code, `####`, by which an emulator (melonDS) gives a program whose ARM9
starts at `0x4000`, as calico's layout puts it, its DLDI card; a menu shows `nds/icon.bmp`, the cover drawn at 32 by
32 in 16 colours by `make icon`.

The build names itself from `VERSION` and git as the NRO's does ([Versions](Versions.md)), so a report's folder is the
commit's.

### Run it

- On a console: from a card's menu with the NDS on its FAT, or a DSi's SD card. The card's DLDI driver, or the DSi's
  SD, is where the report goes.
- In a DS emulator: open the NDS. With a FAT image and DLDI the report lands in it; without one, on the log alone.

No screens to choose from: the top screen names each step as it starts, and a run leaves once its report is closed,
whatever the mode file says, as the Vita's and the 3DS's do.

### What it runs

The judge first, always, with the references of the suites the DS runs, the libc family's; then libc, the C library
alone (devkitARM's newlib), and system, calico's threads and the C library's clock ([libc](families/Libc.md)). No
socket suite: the DS's network is a Wi-Fi access point, with no loopback to talk to. The DS's own suites come after
these as they are written.

### What fits in its memory

The DS has 4 MiB of main memory, 3.5 MiB of it for the ARM9's program and its data, and its main thread's stack is
the 16 KiB of data TCM. So the frontend differs from the others in two ways:

- **The judge's other references stay out.** `RunReference` holds every family's reference, and the Wimsy scene's
  two alone keep 13.6 MiB of tables; the DS's reference step runs `RunReferenceLibc`, and the link leaves the rest
  out. The program comes to about 2.7 MiB: 0.6 MiB of code, 2.1 MiB of zeroed data.
- **The run has a thread of its own.** The judge's journal checks keep up to 296 KiB in one frame
  (`t_journal.c`'s `CheckReading`), so `main` runs the engine on a thread with a 512 KiB stack.

## Where it leaves its report

`/amberlith/` on the card's FAT, laid out as `sdmc:/amberlith/` is on the Switch ([The report](Report.md)): the mode
file, the crash list and `runs/<commit>/report.jsonl`. Every line also goes to no$gba's debug port (`nocashWrite`, a
character at a time at `0x04FFFA1C`), which no$gba prints and a console ignores. Without a FAT the C library can
write, the run's report is on that log alone: the frontend makes a device with no operations the default one, so
every file call fails with an error, where with no device at all devkitARM's `mkdir` would end the program.
