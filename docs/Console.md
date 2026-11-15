# Looking at it on the console

## Index

* [The results screens](#the-results-screens)
* [The running screen](#the-running-screen)
* [The start screen](#the-start-screen)
* [Nobody there](#nobody-there)
* [Buttons](#buttons)
* [Touch](#touch)

One binary, two modes, chosen by the start screen or the mode file.

- **Detailed** (the default): every suite runs, the report is closed, and then the results go on the screen, a group
  at a time.
- **Quick**: every suite runs and the program exits when the report closes. For scripts.

Both leave the same verdicts and the same images, line for line: the viewer refuses to start while a report is open.
The report differs only in the mode note near its top and in the line a detailed run adds when nobody is there. After a
detailed run its results are the browser's results tab, a press away from its other tabs ([The browser](UI.md)).

## The results screens

A group is a suite, gathered under its family: the judge (selfcheck, support, reference), cpu, pad (pad, touch),
graphics (clear to tex8k), fs, kernel (sched), display (binder), video (vic, nvdec), audio, service (hwopus to
acc-app) and net (nifm to friend). Three screens, one inside the other:

- **The overview** opens on the first group that failed. Its heading has the run's counts and the failed groups' names.
  Below, every group with its passes, failures and skips in columns and a bar in proportion, a failed group in red;
  under the list, the selected group's failures, or its skips when nothing failed. Cases the program ended inside are
  counted in a fourth column, in violet, that only such a run has.
- **A group** lists its cases marked `PASS`, `FAIL` or `SKIP`, with `pic` when the case kept pictures, and opens on its
  first failure. Under the list: the selected case's name, its detail wrapped, and what opening it shows.
- **A case** that compared pixels opens on its three pictures, `drawn`, `expected` and `judged`, each 64 by 64 scaled up
  with nearest neighbour, a key for `judged`'s four colours, and the detail. Any other case opens on its name and its
  whole detail.

The screens hold every case the report recorded, kept in memory as its line went out, and the pictures the judge
looked at; nothing is read back from the card, and `images` changes only what goes on the card. The store holds 4096
cases; a run past that, or one that ran out of memory, says on the overview how many the screens hold. A quick run
keeps nothing.

The look is ScarletNX's launcher in its YoRHa theme, at the scale of two it opens in on a frame of 720 lines: its 8 by
16 VGA font at twice its size, the title (`EDENPROBE  /  RESULTS`) at four times it with its shadow in the dim colour,
the tabs as buttons, a frieze along the top and the bottom, and the theme's paper page with its grid, thin diagonals,
corners a little darker and a grain. A row is a box, the one under the cursor inverted with a rule over and under it
and the cursor in the margin. Lists read down a column, the results' and a case's detail, keep the font's own size.
The screens drawn while the run goes (running, listening) are on the run screen's dark page with its scan lines. The
foot is a box: the sentence of what is under the cursor on its first row, which the countdown takes while it runs,
and on its second the keys, each inverted and what it does after it, each a button for a tap. The resume, pad and
touch screens keep the font's own size and their band of keys. See [Credits](Credits.md).

## The running screen

A run with a screen (every detailed run, and a quick one the start screen chose) shows how far it is: the step and its
number (`step 17 of 34: net`, with a bar), the pass of a timed suite (`pass 3 of 10: a rehearsal, timed`), the case and
its number in the pass (`case 12 of 40`, the total from the second pass on), the net suite a case belongs to, the
counts so far, the time the run has taken and its folder. It is drawn when a case starts or ends, at most twice a
second, and when a step begins; never inside a timed interval, nor while the listening sequence has its own screen. A
case that waits out a timeout holds it still. A quick run from the mode file draws nothing; `noprogress` turns it off
in any run.

## The start screen

Every new detailed run opens on it: no mode file, a file without `quick`, or `quick` with X held at launch. It is the
browser's first tab: L, R or a tap on another tab opens the browser there instead of the run ([The browser](UI.md)).
Each row starts as the mode file has it:

| row | what it turns |
|---|---|
| begin the run | the cursor starts here: A begins the run, as plus or minus does from any row, or the begin box |
| graphics, fs, audio, net, pad | the families; the judge and cpu always run, and pad (the controller, then the touch screen, which waits for a person) right after cpu ([The pad family](families/Pad.md)) |
| listen | the listening sequence before the audio suite ([The audio family](families/Audio.md)); on turns audio on, audio off turns it off |
| quick | leave when the report closes, with no results screens |
| notime | each drawing suite once, with no timing passes |
| crash list | the crash list's cases again (the word `retry`); X from any row |
| save | write the choice into `sdmc:/edenprobe/mode` for the next launch |

Beside the rows, what the row under the cursor does, and the cases on the crash list, by name; while that choice is on
with a case to run, a warning there says the program may end and, started again, goes on after that case. Nothing pressed for ten seconds begins the run as the
file had it. What was chosen goes in the mode line (`detailed, chosen on the start screen`, `the crash list's cases
again, chosen on the start screen`, `saved to the mode file by the start screen`) and in the journal, so a launch that
resumes after a crash keeps it whether or not it was saved. Saving writes the words chosen, keeps the file's label and
drops words nobody understood; saved with quick on, the next launch leaves without the screen, and X held at launch
brings it back.

**Holding X** at launch is the other way into the detailed mode, for a card that says `quick`. The pad is asked once it
turns up, twelve frames at most. The report is opened first, so a machine that cannot read the file or reach the pad
still leaves a report.

## Nobody there

A detailed run that sees no button pressed for ten seconds after the report closed leaves by itself and appends one
line after the summary:

```json
{"type":"note","text":"detailed mode: nothing pressed for 10 seconds after the report closed, left by itself"}
```

A press counts, not a connected controller: an emulator presents one whether or not anyone holds it. The countdown
shows on the foot's first row, and any button stops it for the rest of the run.

## Buttons

| button | overview | group | case |
|---|---|---|---|
| up, down | the group before or after | the case before or after | the case before or after |
| left, right | the first group of the family before or after | the group before or after, on its first failure | the case before or after |
| A | opens the group | opens the case | |
| B | the browser's suites | back to the overview | back to the group |
| ZR, ZL | the next or previous group that failed | the next or previous failure in the group | the next or previous failure in the group |
| X | | | the pictures: all three, then each alone |
| Y | the same run again, when the browser began it | the same | the same |
| L, R | the browser's tab before or after | the same | the same |
| minus | quit | quit | quit |

- **Start screen:** up and down move the cursor, A turns the row or begins the run, plus or minus begins from any row,
  X turns the crash list's cases from any row, L and R open the browser on the tab before or after.
- **Resume screen** ([When the program ends in the middle](Crashes.md)): up and down choose, A confirms.
- **Listening screen:** Y repeats the take that is playing, X skips to the next part; nothing else does anything, so
  nothing quits the sequence.
- **Pad screen:** every button is what a prompt asks about. A button prompt takes whatever arrives, a question takes up
  (yes) or down (no) on the d-pad or a tap on its box, and each prompt runs out by itself. The touch suite's screen
  takes only the taps it measures.

Rules every screen shares:

- The first eight frames are neither a press nor a tap: a controller arriving with the window reports its whole mask as
  newly pressed.
- An edge that holds both Y and X turns nothing on the start screen, where Y alone does nothing either; in an edge
  that also moves the cursor, A turns nothing (on begin the run it still begins). An edge with A and a direction does
  nothing on the resume screen.
- Every list comes round at either end. A held direction repeats after twenty frames, then every four:
  the reference suite's five hundred and forty one cases do not take a press each.
- Going back lands on the group or case that was open; the picture view chosen with X stays from one case to the next.
- A frame is composed only when something moved or the countdown ticked.

## Touch

Every screen takes the touch screen as well as the buttons. A tap is one finger down and up on the same row or box,
counted when it lifts; a finger that slides off chooses nothing, a second finger makes it no tap, and a finger already
down when a screen opens counts only once lifted and put down again. A tap on a row moves the cursor there; a tap on the
cursor's row does what A does. Each key the foot names is a button for what it does, and each tab for its tab; the
resume, pad and touch screens keep a bar of boxes at the right of their band. A list longer than the screen keeps the
cursor in its middle. A finger down keeps a screen as a press does.

The decisions are `source/ui.c`'s (`UiTouchStep`, `UiStartTap`, `UiResumeTap`, `UiTapStep`, `UiBrowseTap`,
`UiPadTap`), which the support suite checks; `source/viewer.c` only reads the touch screen. A docked console's touch screen is off.
