# edenprobe documentation

New here? Start with [Build](./Build.md), then [the mode file](./ModeFile.md) and [the report](./Report.md).

- **[Design](./Design.md)** - the tiers a case judges by
- **[Roadmap](./Roadmap.md)** - the suites in the order they run, and what a failure in each points at
- Running it:
  - **[Build](./Build.md)** - the NRO, the host build and the commit hook
  - **[Releases](./Releases.md)** - the nightly, the versions and the CI
  - **[Under an emulator](./Emulator.md)** - `make run`
  - **[On Android](./Android.md)** - edenprobe's APK, and the NRO in an emulator's Android build
  - **[Frontends](./Frontends.md)** - one engine for the console, Android and a PC, and how another fits
  - **[On the console](./Console.md)** - the two modes, the screens and the buttons
  - **[The browser](./UI.md)** - a family, a suite, a case or a demo at a time, and the demos' HUD
  - **[The mode file](./ModeFile.md)** - the words that choose what runs
  - **[The hooks mode](./Hooks.md)** - the services asked through the SDK's functions the program exports
  - **[The validation layer](./Validation.md)** - `make validate`
  - **[An older driver](./OlderDriver.md)** - the masquerade layer
- **[The NVN layer](./NvnLayer.md)** - NVN for a homebrew program, a module of its own: on deko3d for the Switch,
  the names alone for the Switch 2
- **[NVN coverage](./NvnCoverage.md)** - which NVN functions a case calls, of the ones ScarletNX answers, by group
- **[The probe as a Switch 2 program](./Ounce.md)** - the ExeFS build ScarletNX runs, and what a pass there says
- **[Switch 2 shader programs](./Sm86.md)** - `tools/sm86asm.py`, which assembles them from their instructions
- Reading a run:
  - **[The report](./Report.md)** - its lines, and where a run leaves them
  - **[Time per case](./TimePerCase.md)** - what a case's time measures, and comparing two builds
  - **[Baselines](./Baseline.md)** - a machine's own times, and a run held to them
  - **[Failure pictures](./FailurePictures.md)** - the pictures a failing case leaves
  - **[When the program ends in the middle](./Crashes.md)** - the journal, the crash list, the tables
  - **[Versions](./Versions.md)** - the version, the build and the cover
- **[What it checks](./families/README.md)**:
  - **[The judge](./families/Judge.md)** - selfcheck, support and reference
  - **[Processor](./families/Cpu.md)**, **[pad](./families/Pad.md)** and **[touch](./families/Touch.md)**
  - **[Graphics](./families/Graphics.md)** - the drawing suites, clear to tex8k, and
    **[blit2d](./families/Blit2d.md)**
  - **[nvn](./families/Nvn.md)** - clears, copies and a scene through the program's NVN, a layer on deko3d
  - **[nvn2](./families/Nvn2.md)** - what a Switch 2 program asks of NVN, with shader programs assembled by hand
  - **[fs](./families/Fs.md)**, **[kernel](./families/Kernel.md)** and **[display](./families/Display.md)**
  - **[Video](./families/Video.md)** - the VIC, and **[nvdec](./families/Nvdec.md)**, VP9 through the decode engine
  - **[Audio](./families/Audio.md)**, **[service](./families/Service.md)** and **[net](./families/Net.md)**
  - **[Game](./families/Game.md)** - one scene that runs everything at once, as a game does, and the levels that
    draw Wimsy as a game draws a character
  - **[Demo](./families/Demo.md)** - graphics workloads a game runs, drawn for a person to watch, judged and timed,
    among them the **[fluid](./families/Fluid.md)**, a 2D Navier-Stokes fluid in ping-pong fragment passes
  - **[libc](./families/Libc.md)** - the C library under every frontend
  - **[Music](./families/Music.md)** - the demos' music, AdLib tracker songs composed from a seed on an FM
    synthesizer in the OPL2's manner, held to the host's samples
- **[Credits](./Credits.md)**
