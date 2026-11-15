# The demo family

Demos a person watches, each one a graphics workload a game runs, drawn frame after frame, presented and timed, and
judged like every other case: chosen frames against the answer the processor works out. Only the word `demo` runs it
([The mode file](../ModeFile.md)), and `all` leaves it out: a demo is there to be watched and adds seconds of frames
to a run that times everything else. A run that does not name it makes no device, no frame and no answer for it: not
one of its cases, references or buffers costs a run anything until the word asks for them. The start screen has no
row for it, and a file that names it keeps it whatever the screen changes.

What every demo of the family does:

- **Every frame is a function of its number.** What a frame draws comes from its number and nothing else: no clock,
  no input, no state carried over that a slow frame could change. A machine that runs the frames slowly draws the
  same images as one that keeps up, and gets the same verdicts.
- **Chosen frames are judged** against the processor's answer for that frame, exact wherever the arithmetic the
  demo's programs do has one answer, and the tolerance named where it has more. A demo's page says which frames, what
  is held exact, and the limits of the arithmetic it draws with.
- **Every frame is presented and timed** as the game family's scene is ([What is timed](Game.md#what-is-timed)):
  frame 0 is the warm-up and left out, then present to present (mean, 99th percentile, longest) and the part of the
  frame spent recording, submitting and waiting for the graphics unit (mean, 99th percentile). With `notime` the
  frames run and are judged, and no case carries a time.
- **Presented like the game scene**: through a swapchain on the default window in a quick run from the mode file,
  through the program's screen in a run with a screen.
