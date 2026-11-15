# The demo family

Demos a person watches, each one a graphics workload a game runs, drawn frame after frame, presented and timed, and
judged like every other case: chosen frames against the answer the processor works out. Only the word `demo` runs it
([The mode file](../ModeFile.md)), and `all` leaves it out: a demo is there to be watched and adds seconds of frames
to a run that times everything else. A run that does not name it makes no device, no frame and no answer for it: not
one of its cases, references or buffers costs a run anything until the word asks for them. The start screen has no
row for it, and a file that names it keeps it whatever the screen changes. `frames=<n>` sets how many frames each
demo draws, as it does the game scene's; 600 without it, ten seconds at 60 a second.

What every demo of the family does:

- **Every frame is a function of its number.** What a frame draws comes from its number and nothing else: no clock,
  no input, no state carried over that a slow frame could change. A machine that runs the frames slowly draws the
  same images as one that keeps up, and gets the same verdicts.
- **Chosen frames are judged** against the processor's answer for that frame, exact wherever the arithmetic the
  demo's programs do has one answer, and the tolerance named where it has more. A demo's section says which frames,
  what is held exact, and the limits of the arithmetic it draws with.
- **Every frame is presented and timed** as the game family's scene is ([What is timed](Game.md#what-is-timed)):
  frame 0 is the warm-up and left out, then present to present (mean, 99th percentile, longest) and the frame's work,
  from the image in hand to the queue standing idle after the frame's submit (mean, 99th percentile). With `notime`
  the frames run and are judged, and no case carries a time.
- **Presented like the game scene**: through a swapchain on the default window in a quick run from the mode file,
  through the program's screen in a run with a screen, the target copied back and scaled up by the processor, where
  present to present is the screen's.
- **A demo is a function of how it is run** (`source/demo.h`, `DemoRun`): the frames, whether it times them, whether
  a person is in front of it, and two questions it asks once a frame, whether that person stopped it and whether
  they hold it paused. A step of the run asks it with the mode file's words, and nobody stops or pauses it; a screen
  that lets a person pick a demo asks it with its own. Paused, a demo shows its last frame again and draws no new
  one, and the time it stays paused is in no frame's time; the frames case says how often and how long. Frames a
  stopped demo did not reach are skipped as `asked`, saying the frame it stopped at.
