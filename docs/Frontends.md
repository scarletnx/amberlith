# Frontends

## Index

* [What the engine is](#what-the-engine-is)
* [What a frontend gives it](#what-a-frontend-gives-it)
* [The frontends there are](#the-frontends-there-are)
* [A new frontend](#a-new-frontend)

One program, built for each machine it asks: the NRO for the console, an ExeFS for the Switch 2, the APK for
Android, the VPK for the Vita, the 3DSX for the 3DS, the NDS for the DS, the ROM for the GBA, the host build for a PC. Each build is a frontend around one engine, and every frontend writes the same report, so a report from a
machine and one from its emulator compare line by line ([The report](Report.md)).

## What the engine is

Portable C, the same sources in every build (`source/portable.txt` lists them):

- **the report** (`report.c`): each case's line, the notes, the summary; every line to a file and to the machine's
  log, and synced before the next case starts;
- **the journal and the crash list** (`journal.c`): where a run is, so a crash costs one case and the next launch
  goes on after it ([When the program ends in the middle](Crashes.md));
- **the mode file** (`mode.c`): the words that choose the families ([The mode file](ModeFile.md));
- **the run** (`runner.c`): the mode file read, the run's folder made, the journal of the run before read and the
  plan made from it, the steps run in their order with the journal written between them, the summary, and
  `AMBERLITH DONE` on the log;
- **the judge** (selfcheck, support, reference) and the references the cases are held to;
- **the cases every frontend runs**: the libc family's suites, the C library and what it asks of the system
  ([libc](families/Libc.md)).

The engine uses no thread, no drawing and no network of its own, and the heap only to keep a detailed run's cases
for its screens. It writes files with stdio; a machine with no file system gets its report on the log alone.

## What a frontend gives it

`source/platform.h` is all the engine asks of a machine beyond its C library:

| What | Required | Console | Android | Vita | 3DS | DS | GBA | Host |
|---|---|---|---|---|---|---|---|---|
| `PlatformLog`, the machine's log | yes | the debug string service | logcat, tag `amberlith` | the kernel's `sceClibPrintf` | the kernel's debug string, `svcOutputDebugString` | no$gba's debug port (`nocashWrite`) | mGBA's debug registers, and the cartridge's SRAM | standard output |
| `PlatformLogIsStandardOutput`, whether a case can read the log back | yes | no | no | no | no | no | no | yes |
| `PlatformSocketsOpen`, for the socket suite | when it runs that suite | bsd (`socketInitializeDefault`) | nothing to bring up | no: not among its steps | soc:U (`socInit`, with a buffer of its own) | no: not among its steps | no: no network | nothing to bring up |
| `PlatformSurface*`, a surface to draw into and read back | when it runs a drawing suite of the engine's | no: its graphics family is deko3d's own | not yet | no: its gxm suite is its own | no | no | no | no |

Files, time and threads come from the C library itself: stdio, `clock_gettime` and pthreads, which newlib on the
consoles, bionic and glibc all have. A suite that needs one of them is a step only a frontend whose machine has it
lists: the system suite needs threads and a clock, the socket suite sockets.

The rest is the frontend's own code, around the runner's calls (`runner.h`):

- a `RunnerFrontend`: the root folder (the mode file, the crash list, `runs/` and `baseline/` go under it), the
  version, the build and the hash, its steps in their order, the families they make up, and of those the ones that
  run only when the mode file names them, and what it writes into the report after each step (the console notes
  what the SDK's own code answered in it, [The hooks mode](Hooks.md));
- its own suites: the console's are its processor, deko3d, its services; Android's its Java framework, JNI, GLES and
  bionic's own;
- its screens, when it has any: the console asks whether to go on with a run left in the middle, shows the start
  screen and the running screen, and the results after the report is closed. A frontend without screens runs as a
  quick run does.

## The frontends there are

| Frontend | Source | Root | Families |
|---|---|---|---|
| The console (NRO) | `source/main.c`, `source/platform_switch.c` | `sdmc:/amberlith` | judge and cpu always; graphics, fs, net when no word names one; pad, kernel, pace, display, video, audio, service, game, libc and music when named |
| Android (APK) | `android/src/main/cpp/frontend.c`, `platform_android.c`, the Java in `android/src/main/java` | the app's external files, `amberlith/` | judge always; libc, graphics, net, jni and framework when no word names one ([On Android](Android.md)) |
| The Switch 2 (ExeFS) | `ounce/frontend.c`, `source/platform_switch.c` | `sdmc:/amberlith` | judge always; nvn (the `nvn` and `nvn2` suites) when no word names one ([The probe as a Switch 2 program](Ounce.md)) |
| A mod of a game (`subsdk9`) | `mod/frontend.c`, `mod/start.c`, `mod/platform.c`, `mod/screen.c` | `sdmc:/amberlith` | the judge's selfcheck and support always; nvn (the `mod` suite and the `nvn` suite's looked up way, on the game's own driver) when no word names one; game and demo when named: the fractal zooms through the game's NVN, the other demos listed as skipped ([The probe as a mod of a game](Mod.md)) |
| The Vita (VPK) | `vita/frontend.c`, `vita/platform_vita.c`, `vita/gxm.c` | `ux0:data/amberlith` | judge always; libc and graphics (the `gxm` suite) when no word names one ([On the Vita](Vita.md)) |
| The 3DS (3DSX) | `3ds/frontend.c`, `3ds/platform_3ds.c` | `sdmc:/amberlith` | judge always; libc (the libc, system and socket suites) when no word names one ([On the 3DS](3DS.md)) |
| The DS (NDS) | `nds/frontend.c`, `nds/platform_nds.c` | `/amberlith` on the card's FAT | judge always, with the libc family's references alone; libc (the libc and system suites) when no word names one ([On the DS](DS.md)) |
| The GBA (ROM) | `gba/frontend.c`, `gba/platform_gba.c` | none: the report on mGBA's log and in the cartridge's SRAM | the report alone, its mode `quick libc` compiled in: the judge's selfcheck and libc references, then the libc suite ([On the GBA](GBA.md)) |
| Host | `source/host_main.c`, `source/platform_host.c` | none: `amberlith-host-report.jsonl` where it runs | the judge and the libc family, every time ([Build](Build.md#the-host-build)) |

The console runs the libc family only when a word names it, so a run that names no family reports what it did
before the family existed.

## A new frontend

1. A build that compiles `source/portable.txt` with its compiler, `-std=c11 -Wall -Wextra -Werror` or a later
   standard (the Vita's `vita.mk` and the 3DS's `3ds.mk` take `-std=c23`, the DS's `nds.mk` and the GBA's `gba.mk`
   `-std=gnu23` for their libraries' headers, the engine unchanged under them, [On the Vita](Vita.md)), and its own
   sources beside them, and names its build as the Makefile does (`AMBERLITH_VERSION`, `AMBERLITH_BUILD`,
   `AMBERLITH_HASH`, [Versions](Versions.md)).
2. `platform_<name>.c` with `PlatformLog`, `PlatformError`, and the sockets and the surface for the suites it runs.
3. A `RunnerFrontend` and an entry that calls the runner in order: `RunnerInit`, `RunnerModeFromFile`,
   `RunnerPlan`, `RunnerModeAtStart`, `RunnerOpen`, `RunnerBeginJournal` for a new run, `RunnerApplyMode`,
   `RunnerSteps`, `RunnerFinish`. Android's `frontend.c` is the shortest one to copy. A machine with no files for
   the journal calls the report alone instead: `ReportOpen`, its steps, `ReportSummary` and `AMBERLITH DONE` on the
   log, as the GBA's `frontend.c` does.
4. Its steps: the judge's three first, then the libc family's suites its machine can run, then its own. A machine
   whose memory cannot hold every family's reference runs those of its own suites as its reference step
   ([On the DS](DS.md#what-fits-in-its-memory)).
5. A job in `.github/workflows/build.yml` that builds it, and its line in this document and in
   [Releases](Releases.md).
