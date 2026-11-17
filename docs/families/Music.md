# The music family

## Index

* [The synthesizer](#the-synthesizer)
* [The composer](#the-composer)
* [The tracks](#the-tracks)
* [What is judged](#what-is-judged)
* [When it plays](#when-it-plays)
* [Listening to it](#listening-to-it)

<!-- counts: family music -->
| Suite | Cases | With a judge | Without one | Not yet known |
|---|--:|--:|--:|--:|
| music | 12 | 12 | 0 | 0 |
| **All** | **12** | **12** | **0** | **0** |
<!-- /counts -->

The demos' music: AdLib tracker songs, composed afresh from a seed out of material each track's
score holds, and played by an FM synthesizer in the manner of the OPL2 an AdLib card carried, all
of it the probe's own (`source/musicscore.txt`, `source/musiccompose.c`, `source/music.c`), with
no sample, score or instrument from anywhere else. Every sample is worked out in whole numbers,
from the composer's cells to the last one, so a track and a seed render to the same samples on any
machine; the family holds a machine's render to the host build's, then what the audio renderer
plays of it to that render. Only the word `music` runs it ([the mode file](../ModeFile.md)), and
`all` leaves it out: outside its cases and the interactive mode the music makes no thread, no voice
and no sample ([When it plays](#when-it-plays)).

- **music**: four hold the first pass the fixed seed composes of each track,
  rendered on this machine, to the host's samples, one a switch between two tracks to the tracks
  rendered alone, and one a switch and a stop inside the silence (the host build runs these six
  too: their hashes are its own). Four play an excerpt of each track through the renderer and one
  the interactive mode's own stream across a switch, each held sample for sample to what the
  synthesizer rendered; the last starts the browser's player, switches it and stops it. If the
  renderer does not open, those six are skipped as `unavailable`, with what it answered.

## The synthesizer

Nine channels of two operators each, the OPL2's kind of voice. An operator is a phase stepped at its
note's frequency times its multiple (a half, or 1 to 15 as the chip's), read through one of the
chip's eight waveforms (sine, half sine, absolute sine, quarter sine, alternating sine, camel sine,
square and log saw). The reading is in the log domain, as the chip computes: a quarter sine as
attenuation in 256ths of an octave, the operator's attenuation added, then an exponential table back
to an amplitude of 13 bits.

The attenuation is the operator's envelope, its level and the tremolo. The envelope keeps the
chip's sixteen rates: an attack at rate r reaches its top in 2.826 s over 2^(r - 1), exponentially,
a decay or a release at rate r falls 96 dB in 39.28 s over 2^(r - 1), and the sustain level is in
3 dB steps; an operator that does not sustain goes on falling at its release rate. The carrier's
level is the channel's volume, 0 to 63 in 0.75 dB steps, as a tracker sets it; the modulator's is
its total level. The modulator's output moves the carrier's phase, or the two are added; the
modulator can feed its last two outputs back into itself, and at the chip's strongest feedback with
a high multiple it turns to noise: the snare's, the hat's and the cymbal's.

The player reads a row of cells at a time, one a channel: a note, a patch, a volume and an effect
that moves on every tick of the row but its first (a slide up or down, a tone slide toward the
note, a volume slide), as a tracker does. A row is a sixteenth at the track's tempo, split into its
ticks. Two chips play every channel, the right one 4.7 cents higher: the surround an AdLib player
gives a mono card, which spreads the sound across both sides as a recording of a RAD tune played
that way does (its left and right correlate at 0.43, a RAD tune through this synthesizer at 0.39).
Each side takes the track's gain and goes through the card's coupling, a high pass near 6 Hz, so no
constant part of a half sine thumps.

The instruments, `source/music.c`'s patches, built as an AdLib tracker's are: sines, half and
absolute sines, no square or saw lead; the kick and the bass with both operators at half the note,
so the low end sits an octave under the key; vibrato and tremolo on what sustains.

| patch | what it is |
|---|---|
| kick | both operators at half the note, the modulator fed back, the carrier dying in a fifth of a second |
| snare | a modulator at eight times the note on an absolute sine, fed back to noise |
| hat | a modulator at fifteen times the note at the chip's strongest feedback: noise |
| crash | the hat's noise on an absolute sine, held, the tracker fading it by volume |
| riser | a modulator at three times the note at the strongest feedback, sliding up |
| sub, rubber | basses: half the note and a little feedback, held; a plucked one an octave up |
| strings, warm | pads with slow attacks: a half-sine carrier an octave up; both operators at the note, fed back |
| bright, horn, flute | leads: a modulator at four times the note fed back hard over a half sine; a softer attack; a breath of modulation |
| bell, glass, epiano | the ostinatos' bells and keys, the bell's modulator swelling in |

## The composer

A track's score (`source/musicscore.txt`) says what each of its nine channels plays and holds its
material; a seed picks from it and composes the rest (`source/musiccompose.c`). The channels, as an
AdLib tracker uses them: the whole drum kit on one channel, kick, snare and hat taking turns, their
loudness making the accents; the bass on its own; up to three pads; the lead; an echo, the lead some
rows late and softer; a double, the lead again drifting sharp, at the climax; a harmony a third
under the lead in the B section and the climax; an ostinato; and the cymbal and the riser.

A pass is a form: an intro, then sections of eight bars, A, B, a breakdown and a climax, in an order
the seed picks from the score's. The seed picks for the pass the patches where the score offers two,
the progressions (four bars of scale degrees) of the home sections, the B section and the breakdown,
and the lead's rhythms; for each part the drum groove, the bass line, the pads' rhythm and the
ostinato; for each section its fill. The lead's melody is composed: two bars on one rhythm, chord
tones on the first and third beats and steps between them, turning back at the range's edges; its
answer moved onto the next chords; a phrase on the other rhythm a third higher; the first again,
ending on the last chord's root. The climax plays the A section's melody with the double and the
harmony. The pads voice each chord nearest the one before and glide into it; at the climax they
cascade, each in turn taking the next chord tone and holding it. The cymbal strikes every fourth
bar and fades by volume; a riser climbs through the last bar of the intro and the breakdown.

What the seed changes: the form, the progressions, the patterns, the patches where there are two,
and every note of the melodies. What it keeps: the track's key, tempo, channels, ranges and the
material it picks from. When a pass ends the next one starts from a seed of its own, without the
intro, so the music never plays the same pass twice. In the interactive mode each start takes the
console's tick as its seed (`MusicSeedNow`); everything else plays `MUSIC_SEED_FIXED`, the seed the
family's hashes pin.

`tools/music-score.py` turns the score into `source/musicscore.c`, refusing a track out of order, a
field missing or out of range, a part played by too many channels, an echo with no lead, a
diminished triad in a progression, a pattern of the wrong length or with a mark its kind has not,
with the line and what it holds; a host test holds the two files to each other.

## The tracks

| track | for | key | tempo | channels | bass, pads, lead, ostinato |
|---|---|---|---|---|---|
| menu | the demo browser | A aeolian | 132 | drums, bass, three pads, lead, echo, ostinato, cymbal | sub; strings or warm; bright or horn; bell or epiano |
| wimsy | Wimsy's levels and the game scene | F ionian | 150 | drums, bass, two pads, lead, double, echo, ostinato, cymbal | rubber or sub; warm; horn or flute; bell or glass |
| fractal | the Mandelbrot and Julia zooms | E phrygian | 112 | drums, bass, two pads, lead, echo, harmony, ostinato, cymbal | sub; strings; flute or bright; glass or bell |
| fluid | the fluid simulation | D dorian | 124 | drums, bass, three pads, lead, harmony, ostinato, cymbal | sub or rubber; warm or strings; horn or flute; epiano or bell |

A first pass is five or six sections of eight bars: about a minute to a minute and three quarters.

## What is judged

| case | passes when | tier |
|---|---|---|
| the *track*, its first pass from the fixed seed, renders to the host's samples (four) | the first pass `MUSIC_SEED_FIXED` composes, its intro and its form, renders to the frames and the FNV-1a of its samples, left then right, low byte first, that the host build rendered (`source/tests/t_musicrender.c`), with none clamped to 16 bits | exact |
| a switch fades the track out over 50 ms, keeps a second of silence, then starts the next one from its first bar | half a second of the browser's track, a switch to Wimsy's: the 2400 frames after it are the browser's own scaled by the fade, the 48000 after those are nought, and the half second after them is Wimsy's first, each as the synthesizer renders it alone | exact |
| a switch inside the silence keeps it and starts the newer track after it, and a stop there stays silent | a switch to Wimsy's track, then 1000 frames into the barrier one to the fluid's: the rest of the barrier is nought and the fluid's track follows it, as it renders alone; the same with a stop instead, and two seconds of nought after it | exact |
| the *track*, looped on two voices at the output rate, comes back from the renderer sample for sample (four) | 9600 frames of the track from its first second, left and right on two voices looping into the audio family's submix at a gain of nought: once they have played 100 renderer frames, both channels the submix's aux effect recorded are that loop, one for one, from some sample of it | exact |
| the player's stream through the renderer's wave buffers comes back sample for sample, past a switch and its silence | the browser's track through the stream the interactive mode plays (below), switched to Wimsy's after 0.4 s and played to 0.15 s past the barrier: the aux's last 9600 frames, read from wherever its ring was last written, are a stretch of the stream as the synthesizer renders it alone, one for one, and not silent | exact |
| the browser's player starts on a renderer of its own, takes a switch and fades out, the renderer closed after it | `MusicPlayStart`'s thread and renderer, into the silent submix: half a second of the browser's track, a switch to Wimsy's that its thread takes, a name no track has refused with that name, then a stop that fades out, joins the thread and closes the renderer; one thread and two voices counted, the stream never dry | exact |

The renderer converts a voice to its output rate even when the rates are one, and at default and
high quality that is a filter of four or eight taps ([the audio family](Audio.md)). Low quality
takes the nearest sample, which at a ratio of one is the sample itself, and the renderer's own
volume of 1 changes nothing, so the recording is the voice's samples, moved by the few the renderer
holds back. That is why the cases look for the loop at every sample rather than at a frame's edge:
under ScarletNX a voice comes back four samples late. There the family takes about eighteen seconds:
thirteen rendering the tracks and the switches, five of sound through the renderer, silent (the last
note says which part took what). The synthesizer's arithmetic and what the composer writes for many
seeds are pinned in [the reference suite](Judge.md), with no sample rendered there.

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

`make -f host.mk music-wav MUSIC_WAV=folder` writes the first pass the fixed seed composes of each
track as a WAV, 48 kHz, 16 bits, stereo, and one of eight seconds of the browser's track, the switch
and eight of Wimsy's: the samples the family holds a machine to, with each file's frames and hash.
`MUSIC_SEEDS="2 3"` adds each track's first pass from those seeds, `menu-seed2.wav` and on.
