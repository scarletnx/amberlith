# The report

One JSON object per line, to two channels: the debug string service, which an emulator logs and a console ignores
without a debugger, and a file on the SD card.

```json
{"type":"case","suite":"clear","name":"clear to opaque red","tier":"exact","result":"pass","detail":""}
```

Each case line carries its tier ([Design](Design.md#the-tiers)), since a pass compares only with another of the
same tolerance: `exact`, `ulp`, `position`, `level`, or `none` for a line that compares nothing the hardware
produced (the judge's own arithmetic, a precondition such as the device opening, a drawing case stopped before it
compared, which gives the reason in its detail).

Every line that compares carries `expected_from` after its tier: where the answer it held the machine to comes from,
the strongest source that gives it. A failure against anything but `console` is a question about the machine, not a
settled bug.

| word | the expected answer comes from |
|---|---|
| `console` | a console run |
| `driver` | the console's own driver, read from a game's copy: what a builder holds when new, what it answers for a part a program lacks ([nvn](families/Nvn.md), [nvn2](families/Nvn2.md)) |
| `astcenc` | ARM's reference ASTC codec, which the reference suite holds this program's decoder to ([Graphics](families/Graphics.md#astc)) |
| `reference` | another program, independent of this one, that gives the same answer: Unicorn's AArch64 for the processor, Mesa's ETC2 and EAC decoders, Pillow's and bcdec's BCn decodes and an NVIDIA card's interpolants, libvpx's hash lists, Atmosphere's mesosphere for the kernel's results; each family's page says which |
| `emulator` | an emulator's code, read at the commit the family's page names: the services' and the display's answers, and the audio renderer's resampling tables, are reden's |
| `spec` | the specification alone: this program's reading of it, which nothing independent gives |

```json
{"type":"case","suite":"astc","name":"an HDR partition is the error colour and the LDR one after it decodes","tier":"exact","expected_from":"astcenc","result":"fail","detail":"..."}
```

A case says its source when it differs from its suite's; every other line takes its suite's, from `source/origin.c`.
A line that compares nothing (tier `none`) has no field, and neither do the judge's own suites, which hold this
program's code to its references rather than judge a machine. The docs' counts give each family's cases by source,
and `tools/case-counts.py unsaid [FAMILY]` lists the cases with a judge whose lines said none.

A case that could not ask its question is a `skip`, tier `none`, with a one-word `reason` and a sentence in
`detail`, counted apart:

```json
{"type":"case","suite":"acc","name":"the id token refresh completes and succeeds","tier":"none","result":"skip","reason":"not configured","detail":"needs an online session: ..."}
{"type":"summary","passed":424,"failed":9,"skipped":27,"crashed":2}
```

| reason | why the case did not ask |
|---|---|
| `crashed` | the program ended inside it on this machine, in this run (the journal) or an earlier one (the crash list) |
| `hangs` | it is in the hang table, `hang` did not ask for it, and the run's machine is not one a run showed it return on |
| `not configured` | the net file does not name what it needs: a session, a server, a rewrite, an account, a certificate |
| `unavailable` | a service or a resource the program cannot have here |
| `depends` | a case before it did not leave what it needs |
| `unanswered` | it asked a person, and nothing was pressed in time |
| `banned` | the ban list bans it on the machine `machine=` names, and `unban` did not ask for it; the detail names the line of `source/bans.txt` and what breaks |
| `asked` | the mode file's `skip=<suite>` leaves its suite out ([Leaving a suite out](Crashes.md#leaving-a-suite-out)), or its `notime` a case of the [pace](families/Pace.md) family, which has nothing but a time |
| `timeout` | a runner stopped the program inside it at a launch's time limit, earlier in this run; the detail gives the limit and how long the case had run ([a launch stopped at its time limit](Crashes.md#a-launch-stopped-at-its-time-limit)) |

The summary's four counts add up to the case lines; `skipped` leaves out the `crashed` ones, which have their own
count. The debug channel's `AMBERLITH DONE` line carries the same four. With `compare=<name>`, `baseline` lines and a
`baseline_summary` line come just before the summary ([Baselines](Baseline.md)). A run that dies halfway leaves every
line up to its last finished case. The summary is the last line, but for a detailed run nobody attended, which adds
one note after it ([On the console](Console.md)).

## Where a run leaves its report

```
sdmc:/amberlith/
  mode              the mode file, read by every build
  net               what the net family is pointed at
  crashes           the crash list: this machine's, shared by every build
  last-run          the folder the latest launch reported in, e.g. runs/5fd6ec1_phone
  fs/               the fs family's files, written once
  pace/             the pace family's files, made and removed in every run of it
  service/          what the service family leaves for its next launch
  baseline/         the baselines, one file each
  runs/
    5fd6ec1/        a build, by its commit
      report.jsonl
      images/       the failing cases' pictures
      journal       where the run is, for the launch after a crash
      emulator.log  under make run: everything the emulator printed
      logs/         under make run: what it said during each case that did not pass
      rounds/       under make run ROUNDS=<n>: each round's report, and times.tsv
    5fd6ec1_phone/  the same build with label=phone
    9e1c2a4-dirty/  a build of a tree with uncommitted changes
```

The folder is the commit as `git describe --always --dirty --exclude='*'` gives it (`unknown` outside git); the
report's first note names the build and the second the folder. `label=<name>` adds `_<name>`, for one build under two
emulators that share a card: 1 to 32 letters, digits, dots, dashes and underscores, not starting with a dot; anything
else is refused in the mode line and the run goes in the folder with no label. Each folder has its own journal, so a
run one build left in the middle waits while another build runs. The crash list is shared by every build. Every
launch rewrites `last-run` before it opens the report, which is how `make run` finds it. The last three are the
runner's, not the program's ([Under an emulator](Emulator.md)).

A card where only an edenprobe build has run keeps running from `sdmc:/edenprobe`, the folder Amberlith had before
1.0.0, whole, and a note after the folder's says so ([Before 1.0.0](Versions.md#before-100)):

```json
{"type":"note","text":"card: sdmc:/edenprobe, edenprobe's, holds a mode file or runs and sdmc:/amberlith neither, so this launch runs from sdmc:/edenprobe whole; renamed sdmc:/amberlith, it is read from there"}
```
