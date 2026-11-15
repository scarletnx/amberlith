# The browser

## Index

* [Opening it](#opening-it)
* [The tabs](#the-tabs)
* [A run begun in the browser](#a-run-begun-in-the-browser)
* [Where the cases come from](#where-the-cases-come-from)
* [Demos](#demos)
* [Options](#options)
* [Music](#music)
* [A run from the mode file](#a-run-from-the-mode-file)
* [Adding a demo](#adding-a-demo)

The browser is where a person runs one thing at a time: a family, a suite, a single case or a demo, then reads its
results and comes back for the next. It is the console's (`source/viewer.c` draws it, `source/ui.c` decides what it
draws and what each press does, `source/browse.c` reads the cases it lists); the APK has a screen of its own. Its look
is the one every screen has ([On the console](Console.md)): ScarletNX's launcher in its YoRHa theme, at the scale of
two that launcher opens in on a frame of 720 lines.

## Opening it

- On the start screen, L or R, or a tap on one of its tabs: the start screen is the browser's first tab.
- The mode file's word `browse`: the browser instead of the start screen, on its suites tab. Only in a detailed run:
  beside `quick` the mode line says `browse refused: the run is quick, a script's, and the browser opens only in a
  detailed one` and the run is a quick one. Nothing pressed on its first screen for ten seconds, the program leaves
  and the launch's report says so.
- After a detailed run, its results are the browser's results tab, and the other tabs are a press away.

The launch's report, open since the program started, says where the browser's runs go and closes; each run begun in
the browser writes a report of its own.

## The tabs

| tab | what it lists | A | X | plus |
|---|---|---|---|---|
| Start | the start screen's rows ([On the console](Console.md#the-start-screen)) | turns the row | the crash list | runs the families on, whole |
| Suites | the families, a family's suites, a suite's cases, each with how its last run went | opens; on a case, runs it alone | marks the row for the next run | runs what is marked, or the row under the cursor |
| Demos | the demos | runs the demo | marks it | runs what is marked |
| Results | the last run's results, as [On the console](Console.md#the-results-screens) has them | opens | the pictures | |
| Options | the session's [options](#options) | changes the value | | |

L and R walk the tabs, round at either end. B goes back a level, and from a tab's top to the start tab; on the results'
overview, to the suites. Y on the results runs the same run again. Minus quits from any tab. Every key the foot names
is a button for a tap, as a tab is; a tap on a row moves the cursor there, and a second tap on it does what A does.

The square at a row's left says what of it the next run holds: hollow nothing, a bar part of it, full all of it. A
suite's first cases, those its last run reported with no tier (the device, the shaders), have a bar of their own: they
run whenever a case of the suite does. The judge's suites have no mark: they run before every run.

## A run begun in the browser

Each is a run of its own, in a folder of its own: `runs/<build>[_<label>]/browse/<n>/`, `n` the first number with no
folder yet. Its report, journal and pictures go there, and `last-run` names it. The run is:

1. the judge (selfcheck, support, reference), as before every run;
2. the steps that hold what was picked, in the order every run has them; cpu only when picked;
3. the demos picked, in the browser's order.

Inside a step, a case that was not picked does not run and leaves no line, count, picture or record; the run's mode line
says what was picked, `browser: run 2, picked in the browser: the judge, then clear whole; coverage: 1 case`. A case of
a picked case's suite with no tier in the last run (its bring-up), and one the catalogue does not hold, run with it. In
a step that holds several suites (net, service) the ones not picked are left out the same way, and a case that needed
one of them skips as `depends`, saying which.

The mode file's other words hold as it says them: `hooks`, `machine=`, `skip=`, `label=`, `unban`, `hang`. The program
ending inside a case of a browser's run puts that case on the crash list at the next launch, with a note, as in any run.

The results open on what was run: one case on its own page, else the overview on the first failure.

## Where the cases come from

The suites are every suite the screens know a family for. Their cases are read, when a launch a person attends begins
and before its report is opened over them, from the last report of this build in the run's folder, then from the
newest of the browser's runs there; each run of the session adds its own. A suite no report holds yet lists `?` cases,
and runs whole. A launch made detailed by X held at launch has read its report over by then, and lists the cases of the
browser's runs and its own.

## Demos

A demo is something to watch that judges what it draws, as its suite does in a run from the mode file: the game scene,
the Wimsy levels, the Mandelbrot and Julia zooms, and the fluid, aliased or not. It shows its frames through the
program's screen, read back and composed by the processor, so its frame times are that way's and its notes say so.

Over its frames, the HUD: its name, frames a second, the last frame's time, the frame of how many, its music's track,
the last 160 frame times against 16.7 and 33.3 ms, and its keys. Its boxes are opaque and nothing is blended.

| button | while it runs | paused |
|---|---|---|
| plus | pauses | resumes |
| B | stops it | back to the browser |
| Y | hides or shows the HUD | |
| up, down | | the row |
| left, right | | the level, the frames, the music, the HUD |
| A | | resume, start the level or the frames chosen, restart, back to the browser |

Paused, the frozen frame is dimmed by drawing every other row of it in the run's page colour, and the time the menu is
up is left out of the frame times. A demo stopped, or left for another level, ends where it is: the frames' case and
the frames it did not reach skip as `asked` (`stopped at frame 312 of 600 by the browser's B`), its times too; a
Wimsy level's judged frames it did draw are judged. The game scene judges every frame, so none of it is judged once
stopped.

## Options

They start as the mode file leaves them and hold for the session.

| option | as the file has it | values |
|---|---|---|
| Timing | on, off with `notime` | on: a drawing suite makes its eleven passes and its lines carry times |
| Pictures | failures, every case with `images` | |
| Crash list | off, on with `retry` | on: the crash list's cases run again |
| Demo frames | 600, or the nearest at or over `frames=` | 120, 600, 3600, 36000 |
| Music | on | the browser's track between runs, a demo's own while it runs |
| Motion | on | a new tab or level comes in from the top over six frames |
| HUD | on | over a demo's frames |

## Music

The browser alone plays music (`source/musicplay.h`). While it is open the interactive mode is on (`MusicSetInteractive`,
`source/music.h`); it plays the track `menu` between runs, stops it while a run's suites run, so nothing it does lands in
a case's time or beside the audio family's own renderer, and switches to a demo's track (`wimsy`, `fractal`, `fluid`)
while the demo runs. The game scene, whose audio thread steps a renderer of its own, plays none. A track that does not
play is said in the report's next line, with the player's reason. A run from the mode file never turns the interactive
mode on; in a browser's run where the music played, the case that holds the demos' music to nought skips as `asked`,
with the frames, threads and voices it made.

## A run from the mode file

A run with `quick`, every runner's and every A/B's, never opens the browser: nothing of it is allocated or read, no
hook is called, no case is left out, and its report has the same lines it had, the support suite's own cases for the
browser beside the others. The support suite holds every mode file a script writes to keeping the browser closed.

## Adding a demo

One line in `DEMOS` in `source/main.c` (a `BrowseDemo`, `source/browse.h`), as a step is one line in `STEPS`: its name
in the list, the suite its lines report under, a sentence or two for the box beside the list, its music's track (NULL
for none), its family and its run. The run takes a `DemoRun` (`source/demo.h`): `interactive`, the `frames` and
`timing` the options give, `stopped()`, looked at once a frame, and `paused()`, true while the pause menu is up. It
shows each frame with `ViewerShowImage`, which draws the HUD and brings up the pause menu; paused, it shows its last
frame again until `paused()` is false, judges nothing, and leaves that time out of its frame times. Demos of one suite
in consecutive lines are the levels the pause menu walks.
