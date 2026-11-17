# The pace family

## Index

* [How a case measures](#how-a-case-measures)
* [The reference to come](#the-reference-to-come)
* [pace-cpu](#pace-cpu)
* [pace-mem](#pace-mem)
* [pace-thread](#pace-thread)
* [pace-timer](#pace-timer)
* [pace-fs](#pace-fs)
* [pace-gpu](#pace-gpu)
* [Under ScarletNX](#under-scarletnx)
* [What it does not measure](#what-it-does-not-measure)

How fast the machine goes. A console always goes at the same speed, and a game is written against that speed: an
emulator far slower drops frames, and one far faster reaches code in an order the console never did, which is where
the races a game never met on hardware show. Six suites, thirty six cases, each a part of the machine timed several
times over: the processor's loops on each core, memory, a thread woken and a mutex passed, the counter and sleeps,
the card, and the graphics unit. Only the word `pace` runs it ([The mode file](../ModeFile.md)), and `all` leaves it
out: its numbers want a machine doing nothing else, and it takes about two minutes, ninety seconds of them in
pace-cpu. Console build only.

No case judges a speed yet. There is no reference: the console's own run is the one to come, and every line says
so.

<!-- counts: family pace -->
| Suite | Cases | With a judge | Console | Driver | Reference | Emulator | Spec | Without one | Not yet known |
|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| pace-cpu | 10 | 0 | 0 | 0 | 0 | 0 | 0 | 10 | 0 |
| pace-mem | 4 | 0 | 0 | 0 | 0 | 0 | 0 | 4 | 0 |
| pace-thread | 5 | 0 | 0 | 0 | 0 | 0 | 0 | 5 | 0 |
| pace-timer | 7 | 0 | 0 | 0 | 0 | 0 | 0 | 7 | 0 |
| pace-fs | 4 | 0 | 0 | 0 | 0 | 0 | 0 | 4 | 0 |
| pace-gpu | 6 | 0 | 0 | 0 | 0 | 0 | 0 | 6 | 0 |
| **All** | **36** | **0** | **0** | **0** | **0** | **0** | **0** | **36** | **0** |
<!-- /counts -->

## How a case measures

A case does a fixed piece of work several times over and reads the system counter (CNTPCT_EL0, 19.2 MHz) around
each. Each time is a sample; the line carries the middle one as its time, `us`, the place every timed case's number
goes ([Time per case](../TimePerCase.md)), and the detail says what the samples came to, here under ScarletNX:

```json
{"type":"case","suite":"pace-cpu","name":"core 0 runs the integer loop","tier":"none","result":"pass","detail":"1027.4 million iterations of 9 instructions a second on core 0, in 39129 runs of 262144; 10 windows of 1 s: the time of 100 million iterations, the median 97.3 ms, 97.3 to 98.0 (0.8% apart); no reference yet: a console's run gives it","us":97331}
```

- **the median**, the lower of the two middle samples, and the line's `us`: it, in microseconds, never nought;
- **the least and the most**, and how far apart they are in percent of the least;
- with a hundred samples or more, **where 98 in 100 lie** (the 1st and the 99th percentile, by nearest rank) and
  **the scatter**, how far a sample lies from the median on average;
- what the case says of its own before them: a rate, the core it ran on, what a kernel call did.

The times are all in the unit the median reads best in: ms from 10 ms, us from 10 us, ns under it. Where one piece of
work is too short for a number in microseconds (a wake, a sleep's overrun), the line's `us` is 1000 of the middle
one, and the detail says so.

The tier is `none`: a line compares nothing a console gave. A case **fails** only when the work's own answer is wrong,
so a time is never a wrong answer's: a loop that left other sums, a copy that differs from its source, a byte read
that was not written, a pixel that is not the last quad's, a counter that went back. It is a `skip`, `depends`, when
its suite's bring-up left it nothing to run on, and `asked` under `notime`, since a time is all it has.

The arithmetic is `source/paceref.c`, which the reference suite holds to numbers worked out apart
([the judge](Judge.md)). The work is `source/pacework.c`, written in the processor's own instructions so that every
build times the same machine code, whatever its compiler.

## The reference to come

A [baseline](../Baseline.md) is how a console's run becomes the reference, with nothing new in the program:

```sh
quick pace baseline=console     # on the console, three times or more: each run a sample of every case's us
quick pace compare=console      # on an emulator, with the console's file copied to its card
```

A compared run gets a `baseline` line a case, `same`, `slower` or `faster`, held to the console's median with the
larger of 20 percent and one and a half times the console's own spread as the limit, a move of under 100
microseconds never counted. Both ways count here: `faster` is as far from the console as `slower`, and is where a
game's races show. Until a console's file exists, compare a machine with itself: `baseline=<name>` on one build,
`compare=<name>` on the next.

Under `make run` the family needs a longer launch than the default: `make run MODE="quick pace" TIMEOUT=600`.

## pace-cpu

Ten cases: a thread started on each of the three cores (none tier; when it fails, the nine after it are skipped as
`depends`), then three loops on each core, one thread at a time at the main thread's priority while the main thread
sleeps on core 0. A loop runs in turns of a fixed count of iterations from its start, each turn's sums held to
`source/paceref.c`'s, and the counter is read after every turn; ten windows of a second each count the turns that
fit, and a window's sample is the time its pace gives the loop's unit of work.

| loop | an iteration, in order | turn | the line's time is of |
|---|---|---|---|
| integer | `madd` (x = x * A + C), `eor` with a shifted register, `add`, `ror`, `cmp`, `csel`, `add`, `subs`, `b.ne`: nine instructions | 262144 iterations | 100 million iterations |
| floating point | on doubles: `fmadd`, `fmul`, `frintz`, `fmsub` (x = (x * A + C) mod 2^24), `fsub`, `fmax`, `fadd`, `subs`, `b.ne`: nine | 65536 | 10 million iterations |
| vector | four lanes of singles and four of 32-bit integers: `mov`, `fmla`, `fmul`, `frintz`, `fmls`, `mov`, `fcvtzu`, `mul`, `add`, `ushr`, `eor`, `add`, `subs`, `b.ne`: fourteen | 65536 | 10 million iterations |

Every value the floating point and the vector loop hold is a whole number its format keeps exactly, so no rounding
rule decides a sum, and a machine that fuses a multiply and an add agrees with one that does not: the sums are
right or wrong, never close. `tools/pace-expect.py` restates the three loops in Python, the floating point one in
doubles beside whole numbers, and the reference suite holds `source/paceref.c` to its table.

Each loop is one chain: an iteration's multiply waits for the one before it. So the number is how fast the core
takes a dependent chain of those instructions, not how many it could run side by side.

A counter that has not moved after 64 turns fails the case, which then does not wait for a window that would never
end.

## pace-mem

Four cases over one block of 64 MiB, on the main thread. A machine with no 64 MiB to give skips them as
`unavailable`.

| case | the work | a sample | checked |
|---|---|---|---|
| a walk through 64 MiB reads a slot at a time | the block as 8388608 slots, each holding the slot to read next: `ldr x, [table, x, lsl #3]`, so a load's address is known only when the load before it ends | 262144 reads; sixteen samples, each going on from where the last ended, so none reads a slot twice | each sample ends on the slot the rule gives |
| 32 MiB are filled with one word, eight times | `stp q, q` twice an iteration, 64 bytes | eight passes, 256 MiB; sixteen samples | every word holds the last word written |
| 32 MiB are read and added up, eight times | `ldp q, q` twice and four vector adds into two sums | eight passes; sixteen | each pass's sum is the words' |
| 32 MiB are copied, eight times | `ldp q, q` twice and `stp q, q` twice | eight passes; sixteen | the copy is its source, byte for byte |

The walk's next slot is (slot * A + C) mod 8388608, which goes through every slot once before it is back at its
start and lands far from where it was each time: nothing can fetch ahead of it, and 64 MiB is far past the console's
caches and what its address translation keeps at hand. Its number is what a read costs when the memory has to be
asked; the other three are what memory gives in a row. On a machine whose caches hold 32 MiB, as a desktop's may,
those three run inside them, and their number there is the cache's.

## pace-thread

Five cases: the bring-up of pace-cpu, then two threads at the main thread's priority that hand something to each
other 2000 times, on one core (both on core 1) and across two (cores 1 and 2). A sample is one handoff, and the
line's `us` is 1000 of the middle one.

- **A thread asleep on an event wakes at a signal.** Each thread sleeps in `svcWaitSynchronization` on an event of
  its own; woken, it resets it, and signals the other's. A sample is the time from just before `svcSignalEvent` to
  just after the other's wait returned.
- **A mutex passes to a thread waiting for it.** libnx's `Mutex`, the kernel's: the holder unlocks while the other
  thread waits in `svcArbitrateLock`, so the kernel hands the mutex over. A sample is the time from just before the
  unlock to just after the other's lock returned.

A handoff is timed only once the other thread is asleep in the kernel, so every sample is a wake and never a wait
that found its event already signaled: the other thread says it is about to sleep (a flag, or the mark the kernel's
mutex word carries while a thread waits for it), and the one with the turn waits for that and 50 us more, yielding
while it waits, since on one core the other cannot get to sleep otherwise.

On one core the time holds the signaler's own way back to sleep and the switch between the two; across two it is
the signal reaching another core. A thread that waits two seconds for the other ends the case as a failure, and one
that never exits keeps its stack and its handle while the rest of the suite is skipped.

## pace-timer

Seven cases, on the main thread.

- **The system counter is read a million times**, sixteen times over: `mrs cntpct_el0` in a loop that also keeps the
  smallest step between two reads that was not nought, the largest, and how many reads were under the read before
  them. The line's time is of a million reads; the detail gives the time of one and the two steps in the counter's
  ticks. A read under the one before it, or a counter that never moved, fails the case.
- **Sleeps of 100 us, 1 ms and 16 ms**, 2000, 1000 and 100 of them, each `svcSleepThread` timed by the counter
  around it, two cases a length from the same sleeps:
  - **by how much each runs over**: a sample is the time a sleep lasted past the time asked, nought for one that
    ended early; the detail says how many ended before their time and the earliest. The precision.
  - **how far apart the longest and the shortest are**: the sleeps cut into ten groups in the order they were made,
    200, 100 and 10 to a group, a sample is the time between a group's longest sleep and its shortest; the detail
    gives the shortest and the longest of them all. The jitter: a late wake now and then moves the group it falls
    in, where the middle of every sleep's distance from the rest would not see it.

  Both lines' `us` is 1000 of the middle sample. A sleep that ends before its time does not fail the case today: the
  detail says it, and the console's run settles what the family then holds a machine to.

## pace-fs

Four cases, through the same file system layer as the [fs](Fs.md) family: `fsp-srv`'s commands, or `nn::fs`'s
functions in [the hooks mode](../Hooks.md). The card opens (none tier), then:

| case | a sample | samples | checked |
|---|---|---|---|
| a 4 MiB file is made and written 64 KiB at a time | the file made at its size, opened, sixty four writes, the flush, the close | eight, each writing other bytes | the last file read back, untimed, holds the bytes written |
| the 4 MiB file is opened and read 64 KiB at a time | the open, sixty four reads, the close | eight | every byte of every read |
| 64 files of 4 KiB are made, written, read back and removed | each made, opened, written and closed; then each opened, read, closed and removed | four | every file's bytes |

Only the calls are timed, each on its own: making the bytes to write and checking the ones read happen between
them. The files live in `sdmc:/edenprobe/pace` and are removed as the suite ends. The fs family times reads in the
patterns a game makes; this suite adds what it does not have, writes and the cost of making and removing a file.

## pace-gpu

Six cases on a device of its own with a 1024 by 1024 RGBA8 target, through deko3d. The device opens and the flat
shaders load (none tier); then four timed cases, eight samples each after one that is not counted and takes the
first use of the target or the texture.

| case | a sample, from the submit to the last byte read | checked |
|---|---|---|
| 200 quads cover a 1024 by 1024 target, each over the last | one draw of 200 quads over the whole target, 210 million pixels written; then a corner of the target, 16 pixels a side, copied to a buffer and read | the corner, and after the sample every pixel: the last quad's colour |
| 200 quads are added to a 1024 by 1024 target by the blender | the same with the blender adding source to destination, each quad a 255th in every channel, so the target is read and written 200 times over | the corner, then every pixel: 200 in every channel |
| a 1024 by 1024 texture is uploaded through the copy engine | one copy of 4 MiB from memory the processor wrote to a texture; then a corner of the texture copied to a buffer and read | the corner, then every texel |
| the 1024 by 1024 target is copied to memory the processor reads | one copy of the target, 4 MiB, to a buffer, and a byte of every page of it read | those bytes, then every pixel: the colour the target was cleared to |

A sample is timed by the counter, as the rest of the family, and ends only once the processor has read pixels the
work made: a machine may put a list's work off until something reads its result, and a time that ended at the
queue standing idle would there be the time of recording the list. A corner is enough to have the work done, and
costs next to nothing to copy and read; the whole target or texture is read back after the sample, untimed, for the
check. A corner that does not hold what the whole readback then holds fails the case: the machine handed back
pixels from before the work.

The detail gives the list or the copy on its own too, from the submit to the queue standing idle, so a machine
that puts work off shows as one whose two times are far apart. The first two cases are the fill, with and without
the blender's read of the target; the last two are what the graphics unit moves from and to memory, its band.

## Under ScarletNX

ScarletNX 75b49fb, built and recompiling the program at its second optimization level, in the hooks mode and with
no window, on a Ryzen 5 7500F with an RX 6700 under RADV: three runs one after another with the machine to itself
(`quick hooks pace`), every case passing in each. The Eden column is one run of Eden 6a9235c22d on the same
machine, not alone on it, where every case passes too: a second machine's answer to the loops' sums, and a number
to read ScarletNX's against, no more.

| suite | case | us, the middle of three runs | runs apart | which is | Eden, one run, us |
|---|---|---:|---:|---|---:|
| pace-cpu | core 0 runs the integer loop | 98078 | 0.9% | 1019.6 million iterations a second | 235587 |
| pace-cpu | core 0 runs the floating point loop | 162560 | 1.7% | 61.5 million iterations a second | 112604 |
| pace-cpu | core 0 runs the vector loop | 81870 | 1.3% | 122.1 million iterations a second | 67890 |
| pace-cpu | core 1 runs the integer loop | 97324 | 0.2% | 1027.5 million iterations a second | 235662 |
| pace-cpu | core 1 runs the floating point loop | 161204 | 2.4% | 62.0 million iterations a second | 112280 |
| pace-cpu | core 1 runs the vector loop | 81598 | 0.1% | 122.6 million iterations a second | 67857 |
| pace-cpu | core 2 runs the integer loop | 97446 | 0.0% | 1026.2 million iterations a second | 234896 |
| pace-cpu | core 2 runs the floating point loop | 163569 | 2.1% | 61.1 million iterations a second | 112289 |
| pace-cpu | core 2 runs the vector loop | 81755 | 0.1% | 122.3 million iterations a second | 67853 |
| pace-mem | a walk through 64 MiB reads a slot at a time | 20172 | 5.4% | 77.0 ns a read | 19796 |
| pace-mem | 32 MiB are filled with one word, eight times | 4377 | 1.1% | 57.1 GiB a second | 9814 |
| pace-mem | 32 MiB are read and added up, eight times | 4171 | 1.8% | 59.9 GiB a second | 11650 |
| pace-mem | 32 MiB are copied, eight times | 11789 | 4.7% | 21.2 GiB a second | 17312 |
| pace-thread | a thread asleep on an event wakes at a signal from its own core | 3020 | 0.0% | 3.02 us a handoff | 937 |
| pace-thread | a thread asleep on an event wakes at a signal from another core | 2968 | 1.8% | 2.97 us a handoff | 3958 |
| pace-thread | a mutex passes to a thread waiting for it on the same core | 2968 | 0.0% | 2.97 us a handoff | 833 |
| pace-thread | a mutex passes to a thread waiting for it on another core | 2968 | 1.8% | 2.97 us a handoff | 3906 |
| pace-timer | the system counter is read a million times | 23829 | 0.6% | 23.8 ns a read | 23374 |
| pace-timer | sleeps of 100 us, by how much each runs over | 52604 | 0.0% | 52.6 us | 56510 |
| pace-timer | sleeps of 100 us, how far apart the longest and the shortest of 200 are | 37136 | 47.0% | 37.1 us | 58802 |
| pace-timer | sleeps of 1 ms, by how much each runs over | 52552 | 0.1% | 52.6 us | 57291 |
| pace-timer | sleeps of 1 ms, how far apart the longest and the shortest of 100 are | 10938 | 554.5% | 10.9 us | 49791 |
| pace-timer | sleeps of 16 ms, by how much each runs over | 68854 | 58.8% | 68.9 us | 68958 |
| pace-timer | sleeps of 16 ms, how far apart the longest and the shortest of 10 are | 98542 | 979.9% | 98.5 us | 14583 |
| pace-fs | a 4 MiB file is made and written 64 KiB at a time | 488 | 0.4% | 8.0 GiB a second | 1101 |
| pace-fs | the 4 MiB file is opened and read 64 KiB at a time | 223 | 0.9% | 17.5 GiB a second | 1014 |
| pace-fs | 64 files of 4 KiB are made, written, read back and removed | 3833 | 0.4% | 59.9 us a file | 13168 |
| pace-gpu | 200 quads cover a 1024 by 1024 target, each over the last | 2953 | 5.2% | 71.0 thousand million pixels a second | 6987 |
| pace-gpu | 200 quads are added to a 1024 by 1024 target by the blender | 7752 | 7.4% | 27.1 thousand million pixels a second | 10407 |
| pace-gpu | a 1024 by 1024 texture is uploaded through the copy engine | 789 | 2.4% | 4.95 GiB a second | 228 |
| pace-gpu | the 1024 by 1024 target is copied to memory the processor reads | 5381 | 0.4% | 0.73 GiB a second | 5109 |

- Two runs of a case are within 2.5 percent of each other for the loops, 8 percent for memory and the graphics
  unit, 2 percent for the handoffs, the counter and the card. A sleep's overrun is the same to the microsecond for
  100 us and 1 ms, and 59 percent apart for 16 ms. The sleeps' spread is what no two runs agree on: the 1 ms one
  came to 11.3, 10.9 and 1.7 ms for 1000, the 16 ms one to 100.7, 98.5 and 9.3. A baseline's limit follows that
  spread case by case, so those cases are held loosely, as they should be.
- A sleep ends 52.6 us after its time whatever its length: the 50 us of slack Linux gives a thread's timers by
  default, which the runtime leaves as it is. None ended before its time.
- The floating point loop is the slow one: 61.5 million iterations a second against the integer loop's 1020, and
  Eden's 88.8.
- The submit of the 200 quads alone comes back in 0.2 ms: the work is put off until the corner is read, which is
  why a sample ends there.

## What it does not measure

- the three cores at once: every loop runs alone, so a machine whose cores slow each other down shows nothing here;
- the counter against another clock: a counter that runs at another rate than it says is the cpu suite's case
  (`cntfrq_el0 says 19.2 MHz and 100 ms asleep counts at that rate`), and nothing here sees a rate that drifts;
- a timed wait's end (`svcWaitSynchronization` with a timeout, a condition variable's), which the svc suite judges
  for its answer and nothing times;
- the card's raw speed apart from the file system's, and reads in a game's patterns, which the fs family has.
