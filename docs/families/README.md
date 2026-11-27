# What it checks

The families run in this order: the judge, the processor, pad (when a word names it), graphics, nvn (when its word
names it), fs, then kernel, pace,
display, video, audio and service (each when a word names it), net, game, demo and music (each when its word names
it); libc, on the console, when a word names it. Android's frontend has the judge, libc, graphics, net, jni and framework
([On Android](../Android.md)). The
[mode file](../ModeFile.md) says which words name what, and [the roadmap](../Roadmap.md) what a failure in each suite
points at. A case that ends the program costs that case, not the rest of the run
([When the program ends in the middle](../Crashes.md)).

Each family's suites and cases, written by `tools/case-counts.py` from [the catalogue](../cases.tsv): a case with a
judge compares what the machine produced with an expected answer, at one of [the tiers](../Design.md#the-tiers); one
without compares nothing (a device that opens, a time it measures); one not yet known only failed or skipped, with no
tier, in the reports the catalogue was made from. The judge's cases hold the probe's own code to its references and
judge no machine, so the last line leaves them out.

<!-- counts: families -->
| Category | Family | Suites | Cases | With a judge | Console | Driver | Reference | Emulator | Spec | Without one | Not yet known |
|---|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| CPU and kernel | [The processor](Cpu.md) | 1 | 38 | 36 | 0 | 0 | 25 | 0 | 11 | 0 | 2 |
| CPU and kernel | [Kernel](Kernel.md) | 2 | 30 | 28 | 0 | 0 | 21 | 0 | 7 | 2 | 0 |
| GPU and NVN | [Graphics](Graphics.md) | 44 | 866 | 749 | 0 | 0 | 158 | 0 | 591 | 117 | 0 |
| GPU and NVN | [nvn](Nvn.md) | 1 | 218 | 203 | 0 | 11 | 0 | 0 | 192 | 9 | 6 |
| GPU and NVN | [nvn2](Nvn2.md) | 1 | 34 | 31 | 0 | 5 | 0 | 0 | 26 | 3 | 0 |
| GPU and NVN | [Game](Game.md) | 2 | 87 | 34 | 0 | 0 | 0 | 0 | 34 | 53 | 0 |
| GPU and NVN | [Demo](Demo.md) | 4 | 67 | 41 | 0 | 0 | 0 | 0 | 41 | 26 | 0 |
| Video and audio | [Video](Video.md) | 2 | 27 | 25 | 0 | 0 | 6 | 0 | 19 | 2 | 0 |
| Video and audio | [Audio](Audio.md) | 1 | 12 | 12 | 0 | 0 | 0 | 6 | 6 | 0 | 0 |
| Video and audio | [Music](Music.md) | 1 | 12 | 12 | 0 | 0 | 0 | 0 | 12 | 0 | 0 |
| System and services | [fs](Fs.md) | 1 | 15 | 13 | 0 | 0 | 0 | 0 | 13 | 2 | 0 |
| System and services | [Display](Display.md) | 1 | 4 | 3 | 0 | 0 | 0 | 3 | 0 | 1 | 0 |
| System and services | [Service](Service.md) | 40 | 125 | 109 | 0 | 0 | 0 | 108 | 1 | 14 | 2 |
| System and services | [Net](Net.md) | 6 | 53 | 18 | 0 | 0 | 0 | 0 | 18 | 5 | 30 |
| System and services | [libc](Libc.md) | 3 | 15 | 15 | 0 | 0 | 0 | 0 | 15 | 0 | 0 |
| Input | [Pad](Pad.md) | 2 | 35 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 35 |
| Speed | [Pace](Pace.md) | 6 | 36 | 0 | 0 | 0 | 0 | 0 | 0 | 36 | 0 |
| The judge and the tools | [The judge](Judge.md) | 3 | 823 | - | - | - | - | - | - | - | - |
| **On a machine** | | **118** | **1674** | **1329** | **0** | **16** | **210** | **117** | **986** | **270** | **75** |
<!-- /counts -->

- **[The judge](./Judge.md)** - selfcheck, support and reference, no hardware involved
- **[The processor](./Cpu.md)** - cpu
- **[Pad](./Pad.md)** - a controller in a person's hands, and the touch screen ([touch](./Touch.md))
- **[Graphics](./Graphics.md)** - the drawing suites, clear to copy ([blit2d](./Blit2d.md)), and tex8k
- **[nvn](./Nvn.md)** - the clear suite's question through the program's NVN, which ScarletNX takes as a game's
- **[nvn2](./Nvn2.md)** - what a Switch 2 program asks of NVN: programs of its own shader code assembled by hand, a
  compute dispatch, and a Mandelbrot whose bars a mesh program draws; only the Switch 2 build runs it
- **[fs](./Fs.md)** - reads through the file system service
- **[Kernel](./Kernel.md)** - the scheduler: priority inheritance, preemption and migration; and the kernel's
  answers to waits, events, handles and memory calls ([svc](./Kernel.md#svc))
- **[Pace](./Pace.md)** - how fast the machine goes: the processor's loops on each core, memory, a thread woken
  and a mutex passed, the counter and sleeps, the card and the graphics unit, each timed several times over, none
  judged until a console's run is the reference
- **[Display](./Display.md)** - the buffer producer's history, on a layer of its own
- **[Video](./Video.md)** - the video image compositor, and the decoder on VP9 jobs ([nvdec](./Nvdec.md))
- **[Audio](./Audio.md)** - the audio renderer, and the listening sequence
- **[Service](./Service.md)** - commands a game sends to the system's services, held to reden's answers
- **[Net](./Net.md)** - nifm, dns, bsd, ssl, acc and friend, the net file and the fixture
- **[Game](./Game.md)** - one scene that runs everything at once, as a game does, and the levels that draw Wimsy
  as a game draws a character
- **[Demo](./Demo.md)** - graphics workloads a game runs, drawn for a person to watch, judged and timed,
  among them the [fluid](./Fluid.md), a 2D Navier-Stokes fluid in ping-pong fragment passes
- **[libc](./Libc.md)** - the C library and what it asks of the system, the same cases on every frontend
- **[Music](./Music.md)** - the demos' music, four tracks an AdLib tracker would play, composed from a seed and
  played by an FM synthesizer in the OPL2's manner, each held to the host's samples and to what the audio renderer
  plays of it

Every drawing suite draws offscreen and reads its target back, so no case depends on the compositor, the swapchain
or vertical sync. The game family is the one exception: its scene and its levels present every frame and judge
offscreen copies of them. A run with a screen shows the running screen between cases, never inside a timed interval; a quick
run from the mode file draws nothing. The pad family and the listening sequence put up screens for a person, and
neither is an input to a verdict.
