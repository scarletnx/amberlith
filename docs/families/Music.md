# The music family

## Index

* [The synthesizer](#the-synthesizer)
* [The tracks](#the-tracks)
* [The scores](#the-scores)
* [What is judged](#what-is-judged)
* [When it plays](#when-it-plays)
* [Listening to it](#listening-to-it)

The demos' music: four tracks written note by note, played by an FM synthesizer in the manner of
the OPL3 an AdLib-era sound card carried, both the probe's own (`source/musicscore.txt`,
`source/music.c`), with no sample or score from anywhere else. Every sample is worked out in whole
numbers, from the score to the last one, so a track renders to the same samples on any machine;
the family holds a machine's render to the host build's, then what the audio renderer plays of it
to that render. Only the word `music` runs it ([the mode file](../ModeFile.md)), and `all` leaves
it out: outside its cases and the interactive mode the music makes no thread, no voice and no
sample ([When it plays](#when-it-plays)).

- **music**, twelve cases. Four hold each track, its intro and its form, rendered on this machine,
  to the host's samples, one a switch between two tracks to the tracks rendered alone, and one a
  switch and a stop inside the silence (the host build runs these six too: their hashes are its
  own). Four play an excerpt of each track through the renderer and one the interactive mode's own
  stream across a switch, each held sample for sample to what the synthesizer rendered; the last
  starts the browser's player, switches it and stops it. If the renderer does not open, those six
  are skipped as `unavailable`, with what it answered.

## The synthesizer

Ten channels of two operators each, the OPL3's kind of voice: bass, three chord voices, lead,
counter-melody, arpeggio, kick, snare and hats. An operator is a phase stepped at its note's
frequency times its multiple (a half, or 1 to 15 as the chip's), read through one of the chip's
eight waveforms (sine, half sine, absolute sine, quarter sine, alternating sine, camel sine,
square and log saw) or, for the drums, noise, and a noise whose sign a square of the operator's own
phase flips, as the chip's hats sound. The reading is in the log domain, as the chip computes: a
quarter sine as attenuation in 256ths of an octave, the operator's attenuation added, then an
exponential table back to an amplitude of 13 bits.

The attenuation is the operator's envelope, its total level (0.75 dB steps) and the tremolo. The
envelope keeps the chip's sixteen rates: an attack at rate r reaches its top in 2.826 s over
2^(r - 1), exponentially, a decay or a release at rate r falls 96 dB in 39.28 s over 2^(r - 1), and
the sustain level is in 3 dB steps; an operator that does not sustain goes on falling at its
release rate. The modulator's output moves the carrier's phase, or the two are added; the
modulator can feed its last two outputs back into itself, from none to the chip's most. A note can
glide into the next, the lead's long notes grow a vibrato after half a second, and a drum's pitch
falls from above to its note.

The instruments, `source/music.c`'s patches:

| patch | what it is |
|---|---|
| slap | a bass whose bright modulator dies faster than the note |
| deep | a round bass, little modulation |
| epiano | an electric piano: a half-sine modulator, a bell's attack and a soft tail |
| organ | two operators added, the second an octave up, swelling in |
| brass | a modulator fed back, a slower attack |
| chip | the chip's square carrier under a modulator an octave up |
| flute | a breath of modulation, a soft attack, vibrato |
| saw | the modulator fed back hard |
| bell | a modulator at 3.5 times the carrier, both dying away |
| pluck | a short, bright modulator over a half sine |
| kick, snare, hat, hatopen | a sine falling two and a half octaves to A1; noise and a falling tone; the metallic noise, closed and open |

Each channel sits hard to one side or in the middle, as the OPL3 sets them: the bass, kick and
snare in the middle, the chord across both sides, the lead to the right and the counter-melody,
arpeggio and hats on the left. The sum takes the track's gain and goes through the card's
coupling, a high pass near 6 Hz, so no constant part of a half sine thumps.

## The tracks

| track | for | key | tempo | lead, chords, bass, arpeggio | drums |
|---|---|---|---|---|---|
| menu | the demo browser | C mixolydian | 100 | brass, epiano, slap, pluck | straight sixteenths |
| wimsy | Wimsy's levels and the game scene | F lydian | 132, shuffled | chip, epiano, slap, bell | a shuffle |
| fractal | the Mandelbrot and Julia zooms | E phrygian | 84 | flute, organ, deep, bell | sparse |
| fluid | the fluid simulation | D dorian | 120 in twelve eight | saw, epiano, slap, pluck | twelve eight |

Each is a short intro of arpeggiated chords, then sections of eight bars: A, A again closing on the
tonic, a bridge where the drums thin out and the counter-melody takes over from the lead on chords
of its own, and A once more; then it loops to the first A. About a minute to a minute and three
quarters each.

## The scores

`source/musicscore.txt` is the music as written, a track at a time: its key, tempo, swing, the
patches of its parts, its form, each section's chords a bar, its lead and counter-melody note by
note (`E5:4` is E in octave 5 for four steps, `r:4` a rest, a `~` glides into the note), and the
patterns its bass, drums, chords and arpeggio play, written as tracker grids, a character a step.
The comment at its top says what each character means. `tools/music-score.py` turns it into
`source/musicscore.c`, refusing a bar that does not fill its steps, a note or chord that is none, a
pattern that is missing or of the wrong length, with the line and what it holds; a host test holds
the two files to each other.

## What is judged

| case | passes when | tier |
|---|---|---|
| the *track*, its intro and its form, renders to the host's samples (four) | one pass of its intro and its form renders to the frames and the FNV-1a of its samples, left then right, low byte first, that the host build rendered (`source/tests/t_musicrender.c`), with none clamped to 16 bits | exact |
| a switch fades the track out over 50 ms, keeps a second of silence, then starts the next one from its first bar | half a second of the browser's track, a switch to Wimsy's: the 2400 frames after it are the browser's own scaled by the fade, the 48000 after those are nought, and the half second after them is Wimsy's first, each as the synthesizer renders it alone | exact |
| a switch inside the silence keeps it and starts the newer track after it, and a stop there stays silent | a switch to Wimsy's track, then 1000 frames into the barrier one to the fluid's: the rest of the barrier is nought and the fluid's track follows it, as it renders alone; the same with a stop instead, and two seconds of nought after it | exact |
| the *track*, looped on two voices at the output rate, comes back from the renderer sample for sample (four) | 9600 frames of the track from its first second, left and right on two voices looping into the audio family's submix at a gain of nought: once they have played 100 renderer frames, both channels the submix's aux effect recorded are that loop, one for one, from some sample of it | exact |
| the player's stream through the renderer's wave buffers comes back sample for sample, past a switch and its silence | the browser's track through the stream the interactive mode plays (below), switched to Wimsy's after 0.4 s and played to 0.15 s past the barrier: the aux's last 9600 frames, read from wherever its ring was last written, are a stretch of the stream as the synthesizer renders it alone, one for one, and not silent | exact |
| the browser's player starts on a renderer of its own, takes a switch and fades out, the renderer closed after it | `MusicPlayStart`'s thread and renderer, into the silent submix: half a second of the browser's track, a switch to Wimsy's that its thread takes, a name no track has refused with that name, then a stop that fades out, joins the thread and closes the renderer; one thread and two voices counted, the stream never dry | exact |

The renderer converts a voice to its output rate even when the rates are one, and at default and
high quality that is a filter of four or eight taps ([the audio family](Audio.md)). Low quality
takes the nearest sample, which at a ratio of one is the sample itself, and the renderer's own
volume of 1 changes nothing, so the recording is the voice's samples, moved by the few the
renderer holds back. That is why the cases look for the loop at every sample rather than at a
frame's edge: under ScarletNX a voice comes back four samples late. There the family takes about
five seconds: one rendering the tracks and the switch, four of sound through the renderer, silent
(the last note says which part took what). The synthesizer's arithmetic and what the scores keep
are pinned in [the reference suite](Judge.md), with no sample rendered there.

The game family's scene and its levels, and each of the demo family's demos run from a script, end
on a case that the music made nothing while they ran: no frame rendered, no thread, no voice ([the
game family](Game.md), [the demo family](Demo.md)). Run from the browser with its music on, the
case skips as `asked`, with what the music made: the person chose it ([the browser](../UI.md#music)).

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

`make -f host.mk music-wav MUSIC_WAV=folder` writes each track, its intro and its form, as a WAV,
48 kHz, 16 bits, stereo, and one of eight seconds of the browser's track, the switch and eight of
Wimsy's: the samples the family holds a machine to, with each file's frames and hash.
