# Amberlith documentation

## Index

* [What it checks](#what-it-checks)
  * [CPU and kernel](#cpu-and-kernel)
  * [GPU and NVN](#gpu-and-nvn)
  * [Video and audio](#video-and-audio)
  * [System and services](#system-and-services)
  * [Input](#input)
  * [Speed](#speed)
  * [The judge and the tools](#the-judge-and-the-tools)

New here? Start with [Build](./Build.md), then [the mode file](./ModeFile.md) and [the report](./Report.md).

- **[Design](./Design.md)** - the tiers a case judges by, and where its answer comes from
- **[Roadmap](./Roadmap.md)** - the suites in the order they run, and what a failure in each points at
- Running it:
  - **[Build](./Build.md)** - the NRO, the host build and the commit hook
  - **[Releases](./Releases.md)** - the nightly, the versions and the CI
  - **[Under an emulator](./Emulator.md)** - `make run`
  - **[On Android](./Android.md)** - Amberlith's APK, and the NRO in an emulator's Android build
  - **[On the Vita](./Vita.md)** - Amberlith's VPK, on a console and in an emulator
  - **[Frontends](./Frontends.md)** - one engine for the console, Android, the Vita and a PC, and how another fits
  - **[On the console](./Console.md)** - the two modes, the screens and the buttons
  - **[The browser](./UI.md)** - a family, a suite, a case or a demo at a time, and the demos' HUD
  - **[The mode file](./ModeFile.md)** - the words that choose what runs
  - **[The hooks mode](./Hooks.md)** - the services asked through the SDK's functions the program exports
  - **[The validation layer](./Validation.md)** - `make validate`
  - **[An older driver](./OlderDriver.md)** - the masquerade layer
- **[The NVN layer](./NvnLayer.md)** - NVN for a homebrew program, a module of its own: on deko3d for the Switch,
  the names alone for the Switch 2
- **[The probe as a Switch 2 program](./Ounce.md)** - the ExeFS build ScarletNX runs, and what a pass there says
- **[The probe as a mod of a game](./Mod.md)** - the module a game's process loads, its NVN the game's own driver: on a console and under ScarletNX
- Reading a run:
  - **[The report](./Report.md)** - its lines, and where a run leaves them
  - **[Time per case](./TimePerCase.md)** - what a case's time measures, and comparing two builds
  - **[Baselines](./Baseline.md)** - a machine's own times, and a run held to them
  - **[Failure pictures](./FailurePictures.md)** - the pictures a failing case leaves
  - **[When the program ends in the middle](./Crashes.md)** - the journal, the crash list, the tables
  - **[Versions](./Versions.md)** - the version, the build and the cover
- **[Credits](./Credits.md)**

## What it checks

[The families](./families/README.md), grouped by what they ask of a machine. Every count here and on their pages is
written by `tools/case-counts.py` from [the catalogue of cases](./cases.tsv), never by hand. A case has a judge when
its line compares what the machine produced with an expected answer, at one of [the tiers](./Design.md#the-tiers);
one without compares nothing: a device that opens, a shader that loads, a time it measures.

### CPU and kernel

<!-- counts: category cpu -->
_2 families, 3 suites, 68 cases: 64 with a judge (exact 56, position 8; expected from another reference 46, the specification 18), 2 without one, 2 not yet seen judged._
<!-- /counts -->

- **[The processor](./families/Cpu.md)** - cpu, the processor's instructions held to the architecture's answers
- **[Kernel](./families/Kernel.md)** - the scheduler, and the kernel's answers to waits, events, handles and memory
  calls

### GPU and NVN

<!-- counts: category gpu -->
_5 families, 52 suites, 1272 cases: 1058 with a judge (exact 966, ulp 56, position 23, level 2, bound 11; expected from the driver 16, astcenc 43, another reference 115, the specification 884), 208 without one, 6 not yet seen judged._
<!-- /counts -->

- **[Graphics](./families/Graphics.md)** - the drawing suites, clear to tex8k, and
  **[blit2d](./families/Blit2d.md)**
- **[nvn](./families/Nvn.md)** - clears, copies and a scene through the program's NVN, a layer on deko3d
- **[nvn2](./families/Nvn2.md)** - what a Switch 2 program asks of NVN, with shader programs assembled by hand
- **[Game](./families/Game.md)** - one scene that runs everything at once, as a game does, and the levels that draw
  Wimsy as a game draws a character
- **[Demo](./families/Demo.md)** - graphics workloads a game runs, drawn for a person to watch, judged and timed,
  among them the **[fluid](./families/Fluid.md)**, a 2D Navier-Stokes fluid in ping-pong fragment passes

### Video and audio

<!-- counts: category media -->
_3 families, 4 suites, 51 cases: 49 with a judge (exact 37, level 12; expected from another reference 6, an emulator 6, the specification 37), 2 without one._
<!-- /counts -->

- **[Video](./families/Video.md)** - the VIC, and **[nvdec](./families/Nvdec.md)**, VP9 through the decode engine
- **[Audio](./families/Audio.md)** - the audio renderer, and the listening sequence
- **[Music](./families/Music.md)** - the demos' music, AdLib tracker songs composed from a seed on an FM
  synthesizer in the OPL2's manner, held to the host's samples

### System and services

<!-- counts: category system -->
_5 families, 51 suites, 212 cases: 158 with a judge (exact 156, ulp 1, position 1; expected from an emulator 111, the specification 47), 22 without one, 32 not yet seen judged._
<!-- /counts -->

- **[fs](./families/Fs.md)** - reads through the file system service
- **[Display](./families/Display.md)** - the buffer producer's history, on a layer of its own
- **[Service](./families/Service.md)** - commands a game sends to the system's services
- **[Net](./families/Net.md)** - nifm, dns, bsd, ssl, acc and friend
- **[libc](./families/Libc.md)** - the C library under every frontend

### Input

<!-- counts: category input -->
_1 family, 2 suites, 35 cases: 0 with a judge, 0 without one, 35 not yet seen judged._
<!-- /counts -->

- **[Pad](./families/Pad.md)** and **[touch](./families/Touch.md)** - a controller in a person's hands, and the
  touch screen

### Speed

<!-- counts: category speed -->
_1 family, 6 suites, 36 cases: 0 with a judge, 36 without one._
<!-- /counts -->

- **[Pace](./families/Pace.md)** - how fast the machine goes: the processor's loops on each core, memory, a thread
  woken and a mutex passed, the counter and sleeps, the card and the graphics unit, each timed several times over and
  none judged until a console's run is the reference

### The judge and the tools

<!-- counts: category tools -->
_3 suites, 823 cases: the probe's own code held to its references, before every run._
<!-- /counts -->

- **[The judge](./families/Judge.md)** - selfcheck, support and reference: the probe's own code held to its
  references, before every run
- **[NVN coverage](./NvnCoverage.md)** - which NVN functions a case calls, of the ones ScarletNX answers, by group
- **[Switch 2 shader programs](./Sm86.md)** - `tools/sm86asm.py`, which assembles them from their instructions
