# The music family

## Index

* [The tracker](#the-tracker)
* [The tracks](#the-tracks)
* [The form](#the-form)
* [What is judged](#what-is-judged)
* [When it plays](#when-it-plays)
* [Listening to it](#listening-to-it)

The demos' music: a tracker of four channels in the Amiga's manner and a composer that writes its
patterns, both the probe's own (`source/music.c`), with no sample or score from anywhere else.
Every sample is worked out in whole numbers, from the composer's choices to the last one, so a
track renders to the same samples on any machine; the family holds a machine's render to the host
build's, then what the audio renderer plays of it to that render. Only the word `music` runs it
([the mode file](../ModeFile.md)), and `all` leaves it out: outside its cases and the interactive
mode the music makes no thread, no voice and no sample ([When it plays](#when-it-plays)).

- **music**, twelve cases. Four hold each track's 32 bars, rendered on this machine, to the host's
  samples, one a switch between two tracks to the tracks rendered alone, and one a switch and a
  stop inside the silence (the host build runs these six too: their hashes are its own). Four play
  an excerpt of each track through the renderer and one the interactive mode's own stream across a
  switch, each held sample for sample to what the tracker rendered; the last starts the browser's
  player, switches it and stops it. If the renderer does not open, those six are skipped as
  `unavailable`, with what it answered.

## The tracker

Four channels, each playing one of the probe's samples: single cycles of a square (32 and 64
samples), pulses a quarter high (16 and 32), a thin pulse an eighth high, triangles (32 and 64),
saws (16 and 32), a sine and an organ of three harmonics, all looped; a kick, a snare and a hat,
played once. Eight bits a sample, the looped ones about nought, so a note starts and stops without
a thump.

A note is a period, as the Amiga's sound chip reads one: a channel takes its next sample every
period ticks of the chip's 3546895 Hz clock, and holds it until then. Three octaves, 856 for the
lowest note and each semitone 2 to the -1/12 of the one below, rounded, which keeps every note
within 6 cents of equal temperament. The length of a wave sets its octave: the same note on a 16
sample cycle sounds two octaves above a 64 sample one. The output is 48 kHz; a channel steps
through its sample by the chip's rate over that, in 16.16 fixed point.

A row of a pattern lasts a number of ticks, and a tick 2.5 seconds over the tempo, its fraction
of a frame carried to the next so a track keeps its tempo exactly. A shuffle moves a tick from the
second half of each beat to the first. On a row's first tick each channel reads its cell: a note,
an instrument and an effect, the ones a tracker of that era has:

| effect | what it does |
|---|---|
| `0xy` | arpeggio: the note, then x and y semitones above it, a tick each |
| `1xx`, `2xx` | the period slides up or down by xx a tick |
| `3xx` | tone portamento: the period glides to the note's by xx a tick, not struck again |
| `4xy` | vibrato: speed x, depth y, half a sine |
| `Axy` | the volume slides up by x or down by y a tick |
| `Cxx` | the volume, 0 to 64 |
| `E9x`, `ECx`, `EDx` | strike again every x ticks, cut at tick x, strike at tick x |

Each instrument has a volume and an envelope in ticks: attack to full, decay to a sustain, release
once the note is let go. A change of gain moves over 32 samples rather than in one, so it does not
click. Channels 0 and 3 go left and 1 and 2 right, each with three eighths of the other side's, as
a player softens the chip's hard stereo; the mix takes 11/8, which keeps four channels at their
peak inside 16 bits; then the A500's fixed low pass, one pole near 4.4 kHz, and for a track that
asks for it the one its power light switches on, two more near 3.3 kHz.

## The tracks

One for each demo, named as the program asks for it. A track's colour is everything about it
before a note is chosen: its mode on its tonic, its tempo and groove, its sounds and registers,
how its chords are voiced.

| track | for | mode | tempo | groove | lead, chords, bass | voicing | drums |
|---|---|---|---|---|---|---|---|
| menu | the demo browser | C mixolydian | 112 | straight eighths | quarter pulse, thin pulse plucked, triangle | fifth and ninth | a kit on the beat |
| wimsy | Wimsy's levels | F lydian | 140 | shuffled | high quarter pulse, square stabs on the offbeats, square | triad | a kit, shuffled |
| fractal | the Mandelbrot and Julia zooms | E phrygian | 84 | slow, long notes | soft triangle that swells, organ held, sine | third and seventh | none: the lead's echo, three quarters of a beat later, across the stereo |
| fluid | the fluid simulation | D dorian | 125 in twelve eight | rolling triplets | saw that glides into some notes, pulse swells, triangle | second and fifth | a kit in twelve eight |

Fractal's goes through the power light's filter as well. Each mode is chosen for the colour it
gives: lydian's raised fourth is bright and a little unreal, phrygian's lowered second dark,
dorian's raised sixth a minor that moves, mixolydian's lowered seventh a major at ease.

## The form

Every track is A A B A, 32 bars, about a minute, and loops. A is eight bars of a progression the
seed picks among its mode's own, played twice: the first time it ends away from the tonic, the
second time it closes on it, through the chord that leads home in that mode (the dominant in ionian,
the lowered seventh in mixolydian and aeolian, the second in lydian, major there since its third is
the raised fourth, the subdominant in dorian, the lowered second in phrygian). B, the bridge, is
eight bars on a progression of its own that starts away from the tonic and ends on that chord, so A
comes back as an arrival. No chord whose triad is diminished in its mode is used.

Channel 0 is the bass on each chord's root on the bar's first row, with the fifth, the octave and
a step into the next bar's root; 1 the lead; 2 the chord, an arpeggio on its root in the track's
voicing; 3 the drums, a fill at the end of each eight bars and half time in the bridge, or the
lead's echo. The lead is a two bar motif from the seed, a rhythm and the steps between its notes,
played twice, the first time again and an answer: a chord tone on every beat, the steps of the
mode between them, inside a twelfth. The bridge has a motif of its own, higher. The last bar of an
eight holds one note and lets it go before the next phrase: the tonic at a close, a chord's other
tone where the music goes on. A long note gets the track's vibrato from its second beat.

A switch between tracks fades the one playing over 50 ms, then a second of silence, the barrier
between two colours, then the next track from its first row. A switch inside the barrier changes
only what follows it, and a stop there ends it in silence; from silence a track starts at once.

## What is judged

| case | passes when | tier |
|---|---|---|
| the *track*'s 32 bars render to the host's samples (four) | one pass of its form renders to the frames and the FNV-1a of its samples, left then right, low byte first, that the host build rendered (`source/tests/t_musicrender.c`), with none clamped to 16 bits | exact |
| a switch fades the track out over 50 ms, keeps a second of silence, then starts the next one from its first row | half a second of the browser's track, a switch to Wimsy's: the 2400 frames after it are the browser's own scaled by the fade, the 48000 after those are nought, and the half second after them is Wimsy's first, each as the tracker renders it alone | exact |
| a switch inside the silence keeps it and starts the newer track after it, and a stop there stays silent | a switch to Wimsy's track, then 1000 frames into the barrier one to the fluid's: the rest of the barrier is nought and the fluid's track follows it, as it renders alone; the same with a stop instead, and two seconds of nought after it | exact |
| the *track*, looped on two voices at the output rate, comes back from the renderer sample for sample (four) | 9600 frames of the track from its first second, left and right on two voices looping into the audio family's submix at a gain of nought: once they have played 100 renderer frames, both channels the submix's aux effect recorded are that loop, one for one, from some sample of it | exact |
| the player's stream through the renderer's wave buffers comes back sample for sample, past a switch and its silence | the browser's track through the stream the interactive mode plays (below), switched to Wimsy's after 0.4 s and played to 0.15 s past the barrier: the aux's last 9600 frames, read from wherever its ring was last written, are a stretch of the stream as the tracker renders it alone, one for one, and not silent | exact |
| the browser's player starts on a renderer of its own, takes a switch and fades out, the renderer closed after it | `MusicPlayStart`'s thread and renderer, into the silent submix: half a second of the browser's track, a switch to Wimsy's that its thread takes, a name no track has refused with that name, then a stop that fades out, joins the thread and closes the renderer; one thread and two voices counted, the stream never dry | exact |

The renderer converts a voice to its output rate even when the rates are one, and at default and
high quality that is a filter of four or eight taps ([the audio family](Audio.md)). Low quality
takes the nearest sample, which at a ratio of one is the sample itself, and the renderer's own
volume of 1 changes nothing, so the recording is the voice's samples, moved by the few the
renderer holds back. That is why the cases look for the loop at every sample rather than at a
frame's edge: under ScarletNX a voice comes back four samples late. There the family takes about
five seconds: one rendering the tracks and the switch, four of sound through the renderer, silent
(the last note says which part took what). The rules the tracker and the
composer keep are pinned in [the reference suite](Judge.md), with no sample rendered there.

The game family's scene and its levels, and each of the demo family's demos run from a script, end
on a case that the music made nothing while they ran: no frame rendered, no thread, no voice ([the
game family](Game.md), [the demo family](Demo.md)).

## When it plays

Nowhere, unless a person or a run asks: in the interactive mode, which the browser's screens turn
on (`MusicSetInteractive`), and inside the music family's cases. Outside both every call that would
play is refused with what is off, and `MusicMadeSoFar` counts the frames rendered and the threads
and voices started, which stay nought: what the game family's last cases hold.

The music goes to the renderer the game family uses, nn::audio's in [the hooks mode](../Hooks.md):
two voices of its own (6 and 7), left and right, at low quality and the output rate, each fed four
wave buffers of 1920 frames, 40 ms, rendered as the renderer ends one. Two ways in, `source/
musicplay.h`:

- `MusicPlayStart("menu")`: the renderer opened, the stream into the final mix at half its
  level, and a thread on core 2, below the game scene's audio and file threads, feeding and
  stepping it; `MusicPlaySwitch("fractal")` for the next track, through the fade and the barrier;
  `MusicPlayStop()` fades it out and closes it all. For the browser, and a demo that has no sound
  of its own.
- A demo that steps the renderer itself, as the game scene's audio thread does, opens the stream
  on its own renderer (`MusicStreamOpen`, from the barrier when it takes over from the browser's
  music) and calls `MusicStreamFeed()` before each step.

A feed that finds every buffer played counts the stream as dry: the voices ran out and a person
heard a gap.

## Listening to it

`make -f host.mk music-wav MUSIC_WAV=folder` writes each track's 32 bars as a WAV, 48 kHz, 16
bits, stereo, and one of eight seconds of the browser's track, the switch and eight of Wimsy's:
the samples the family holds a machine to, with each file's frames and hash.
