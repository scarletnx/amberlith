# On the GBA

## Index

* [The GBA ROM](#the-gba-rom)
  * [Build it](#build-it)
  * [Run it](#run-it)
  * [What it runs](#what-it-runs)
  * [What fits in its memory](#what-fits-in-its-memory)
* [Where it leaves its report](#where-it-leaves-its-report)

## The GBA ROM

`gba/` is Amberlith's GBA frontend ([Frontends](Frontends.md)): one ROM whose program holds the engine's report, the
judge's selfcheck and the libc suite, the same C the NRO runs (`source/portable.txt`). The GBA is an ARM7TDMI with no
operating system, no threads, no file system and no network, so the run is the report alone, with its mode compiled
in (`quick libc`), and its lines go to an emulator's log and the cartridge's SRAM. Every machine that runs it writes the
same lines, so a case that passes on the console and fails in an emulator names something the emulator does
differently.

### Build it

```sh
export DEVKITPRO=/opt/devkitpro       # where devkitPro's installer, or its container, puts it
make -f gba.mk                        # build-gba/amberlith.gba
```

It needs devkitARM and libgba: devkitPro's `gba-dev` packages, or its `devkitpro/devkitarm` container, which the CI
builds in, pinned by tag and digest (`20260610`: devkitARM r68 with GCC 16.1.0, libgba 0.5.4, gbafix from gba-tools
1.2.0). C only, in C23 with GNU's `asm` (`-std=gnu23 -Wall -Wextra -Werror`): libgba's headers write inline assembly
with the `asm` keyword; this program's own code holds to [Code style](CodeStyle.md) all the same. The code is Thumb,
each function and object in a section of its own, so the link keeps only what the GBA's steps reach. `gbafix` gives
the header its title, `AMBERLITH`, and its code, `AMBR`.

The build names itself from `VERSION` and git as the NRO's does ([Versions](Versions.md)).

### Run it

- On a console: from a flash cartridge, with its save type SRAM. The ROM carries `SRAM_V113`, which a cartridge's
  menu and an emulator read the save type from.
- In mGBA: `mgba -l 8 build-gba/amberlith.gba > mgba.log` with its SDL build, whose log at the info level (`-l 8`)
  holds the program's lines; the Qt build shows the same lines in its log view.

The screen names each step as it starts and the counts at the end; the program then waits there, as a GBA program has
nowhere to return to.

### What it runs

The judge's selfcheck, then its references for the libc family (`RunReferenceLibc`), then the libc suite: the C
library alone (devkitARM's newlib), its floats in software, so libm's cases take longer than elsewhere
([libc](families/Libc.md)). No system suite (no threads, no clock to ask) and no socket suite (no network, and no
`<sys/socket.h>` in the GBA's C library, which the socket suite's code is built without). The GBA's own suites come
after these as they are written.

### What fits in its memory

The code and its constants run from the cartridge's ROM: 199 KB, 153 KB of it code. The data has 256 KiB of EWRAM and
32 KiB of IWRAM, which also holds the stack, so the frontend differs from the others in three ways:

- **The judge's support step and the other references stay out.** The support step's journal checks keep up to 296
  KiB in one frame (`t_journal.c`'s `CheckReading`), and the references of the other families hold their tables in
  megabytes; the GBA's reference step runs `RunReferenceLibc`, and the link leaves the rest out.
- **The baseline keeps 16 timed cases**, 8 KiB, where the others keep 2048 in 1 MiB (`BASELINE_CASES_MAX`): the GBA
  writes no baseline file, and its steps time nothing.
- **Zeroed data goes to EWRAM.** devkitARM's cartridge layout puts `.bss` in IWRAM and `.sbss` in EWRAM, so `gba.mk`
  renames each object's `.bss` sections: 173 KB of EWRAM, which leaves the heap 88 KB. IWRAM holds 7 KB of data and
  the stack the other 25 KB.

The layout checks neither part against its end, so `gba.mk` does after the link: the heap and the stack each get at
least 16 KiB, or the build stops and says how many bytes each was left.

## Where it leaves its report

The GBA has no file system: `ReportOpen` says so in a note, and the run goes on with every line on the log.

- **mGBA's debug registers.** `0xC0DE` written at `0x04FFF780` turns them on, and it reads back `0x1DEA` when the
  emulator has them; a line goes into the 256 bytes at `0x04FFF600`, and its level with the send bit at `0x04FFF700`
  writes it to mGBA's log after `GBA Debug: `. A line longer than 256 bytes goes in pieces of 256 and a shorter last
  one (empty for a line whose length is a multiple of 256). A console, and an emulator without them, ignores the
  writes.
- **The cartridge's SRAM.** Every line, and a newline after it, goes into the 32 KiB SRAM, written a byte at a time: a
  head of 16 bytes, `AMBRLOG1`, where the next byte goes (4 bytes, little-endian) and whether the ring wrapped, then
  the log's last 32752 bytes as a ring. An emulator saves it as the ROM's `.sav`, so a run whose log nobody read
  still leaves its end, the summary among it.

`tools/gba-report.py` reads either back into the report: `python3 tools/gba-report.py mgba.log -o report.jsonl`, or the
`.sav` in its place. It joins a line's pieces, leaves out mGBA's own lines, and for a ring that wrapped the line it cut
at its start, saying so; it exits 1 when the report does not end with its summary.
