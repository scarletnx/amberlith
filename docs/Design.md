# Design

## Index

* [No screenshots compared](#no-screenshots-compared)
* [The tiers](#the-tiers)
* [Where an answer comes from](#where-an-answer-comes-from)

## No screenshots compared

Two correct implementations may disagree, and the specification allows it: division within 2.5 units in the last
place, the logarithm within 3 outside a half to two, the power of two within 3 plus twice its argument's magnitude,
the sine and cosine within an absolute 2^-11 between minus pi and pi, and sub-pixel precision a limit each
implementation reports. A hash of a shaded frame breaks between two machines that are both right. So each case
works out what it expects and decides inside the program, with the same code on every machine.

## The tiers

A case's tier is a claim about the hardware, and the case states it.

- **Exact.** Bit for bit, where the result is defined exactly: integer targets, integer arithmetic, bit patterns
  moved around. Never a float shader.
- **Units in the last place.** For float work, with the budget stated per case.
- **Position.** The right answer up to a pixel or two away, for rasterisation coverage; and the order threads ran
  in, where the order is the verdict and the time is not.
- **Level.** A level measured over a stretch of sound, within a fraction of the expected one stated per case: the
  audio family, where two correct renderers differ in rounding and in their filters' taps.
- **Bound.** Within a distance a channel of the exact answer, for texels a machine may keep in a lossy format of its
  own: tex8k's ASTC textures on an emulator whose host samples no ASTC and encodes them again as BC7. The bound is
  that format's loss measured on the same texels. And for texels a format lets a machine round its own way: the
  bcnimage suite's BC1 to BC5 textures on a host GPU whose interpolants are not the console's, the bound the span
  of the decodes known GPUs and decoders give. A case that came back exact says exact, so this tier on a pass says the texels
  moved, and by how much is in its detail.

## Where an answer comes from

A tier says how close a machine has to come; `expected_from` says to what. Each line names the strongest source of
its expected answer: a console run, the console's driver, an independent reference program, an emulator's code, or
this program's reading of the specification ([The report](Report.md)). Until a console has run a case, its answer
is a claim about the console, and a failure is where to look, not a verdict.
