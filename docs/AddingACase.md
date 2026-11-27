# Adding a case

## Index

* [The files, in order](#the-files-in-order)
* [The case](#the-case)
* [The line](#the-line)
  * [The tier](#the-tier)
  * [Where the answer comes from](#where-the-answer-comes-from)
* [The judge and its reference check](#the-judge-and-its-reference-check)
* [Timed cases](#timed-cases)
* [When a machine ends on it](#when-a-machine-ends-on-it)
* [A new suite](#a-new-suite)
* [Running it](#running-it)
* [The version](#the-version)

A case asks a machine one question, works out on the processor the answer the console gives, and writes one line to
the report. This page says which files a new case touches, in what order, and how to prove it before it reaches a
machine. A model case is used throughout: the libc suite's tied sort, which runs on the host build, so every step
here can be tried with nothing but a C compiler.

| What | Where |
|---|---|
| the case | `Ties` in `source/tests/t_libc.c` |
| its judge | `LibcJudgeTiedSort` in `source/libcref.c`, declared in `source/libcref.h` |
| the judge's reference check | `RunReferenceLibc` in `source/tests/t_libcref.c` |

## The files, in order

1. **The case**, in the suite its subject belongs to ([the roadmap](Roadmap.md) gives the suites in the order they
   run, [the families](families/README.md) what each holds): a function in `source/tests/t_<suite>.c`, called from
   the suite's `Run<Suite>` function at the bottom of that file. A new file in `source/tests/` builds in the NRO by
   itself (the `Makefile` compiles every source there); a file every frontend runs also goes in
   `source/portable.txt`, which the host build, Android, the Vita, the ExeFS and the mod read. The mod leaves the libc
   family's file out of that list (`filter-out` in `mod.mk`), since it runs none of its suites: a new file of that
   family goes in the same `filter-out`.
2. **Its judge**, when the rule is more than one comparison: a function in `source/<name>ref.c`, declared in
   `source/<name>ref.h`, and listed in `source/portable.txt` so every frontend's build compiles it.
3. **The judge's reference check**: a `RunReference<Name>` function in `source/tests/t_<name>ref.c`, declared in
   `source/tests/tests.h`, called from `RunReference` at the end of `source/tests/t_reference.c`, and listed in
   `source/portable.txt`.
4. **A new suite** only: see [A new suite](#a-new-suite).
5. **The family page**, `docs/families/<Family>.md`: the case in the prose that lists the suite's cases. A reference
   check also gets its line in the reference list of [the judge's page](families/Judge.md), where each line's count
   of checks ("two on the libc family's tied sort") is written by hand.
6. **The catalogue and the counts**: `docs/cases.tsv` and every count block in `docs/`, written by
   `tools/case-counts.py` ([Running it](#running-it)).
7. **The ban list**, `source/bans.txt`, only when a run showed a machine end or hang on the case
   ([When a machine ends on it](#when-a-machine-ends-on-it)).
8. **`VERSION`**, one line, the last commit of the pull request ([The version](#the-version)).

## The case

```c
static int CompareKeys(const void *a, const void *b) {
    const uint8_t x = ((const LibcRecord *)a)->key;
    const uint8_t y = ((const LibcRecord *)b)->key;
    return (x > y) - (x < y);
}

static void Ties(void) {
    static const char *const NAME =
        "qsort sorts three-byte records by a key four share, each record whole and kept once";
    if (!ReportStart(LIBC, NAME)) {
        return;
    }
    LibcRecord records[LIBC_TIED_RECORDS];
    LibcTiedRecords(records);
    qsort(records, LIBC_TIED_RECORDS, sizeof(records[0]), CompareKeys);
    char said[160];
    const bool passed = LibcJudgeTiedSort(records, said, sizeof(said));
    CHECK_TRUE(LIBC, NAME, CHECK_EXACT, passed, "%s", said);
}
```

- **The name** is a sentence that says what the console does, unique in its suite: it is the case's key in the
  report, the catalogue, the ban list and every baseline. Renaming a case makes it a new one there. No game's name,
  title ID or nickname, here or anywhere ([CLAUDE.md](../CLAUDE.md#never)).
- **`ReportStart(suite, name)`** comes before any of the case's work, in every family that asks the machine something.
  It writes on the journal that the case started, so a launch after the program ended inside it skips that case alone
  ([When the program ends in the middle](Crashes.md)), and it says false when the case must not run: on the crash list,
  stopped at a launch's time limit, banned on this machine, in the hang table, or in a suite `skip=` leaves out. Its
  skip line is then written already; return and report nothing. The judge's own suites (selfcheck, support, reference)
  ask the machine nothing and do not call it.
- **The question** is the one call the machine answers (here `qsort`); everything around it, the input and the
  verdict, is this program's own code, the same on every machine.
- **The verdict** is one line, through a check macro ([The line](#the-line)). Its detail, written only on a
  failure, names what the machine answered and what was expected, both as values.
- A case that cannot ask its question here is a skip, never a pass or a failure: `ReportSkip(suite, name, reason,
  sentence)`, the reason one of `REPORT_REASON_*` in `source/report.h` and the sentence naming what was missing.

## The line

`source/check.h` and `source/report.h` hold everything a case reports with.

| Call | For |
|---|---|
| `CHECK_TRUE(suite, name, tier, condition, fmt, ...)` | the usual verdict: passes when `condition` holds; `fmt` is printed only on a failure |
| `CHECK_TRUE_FROM(suite, name, tier, from, condition, fmt, ...)` | the same, for a case whose answer comes from another source than its suite's row in `source/origin.c` (`t_cpu.c`: cases of the cpu suite, whose row says `reference`, that only the specification answers) |
| `ReportCase(suite, name, tier, passed, detail)`, `ReportCaseFrom(...)` | the same verdicts with the detail already written |
| `ReportCaseTime(...)`, `ReportCaseTimeFrom(...)` | a case that timed its own work: its line carries the time ([Timed cases](#timed-cases)) |
| `ReportSkip(suite, name, reason, detail)` | a case that could not ask its question; counted apart, tier none |

The comparators for the condition: `CheckFloatUlp(produced, expected, ulps)` and `CheckFloatUlpError` (a distance in
units in the last place at the expected value), `CheckFloatAbs(produced, expected, limit)` (a plain difference, for
the sine and cosine near nought), `CheckChannel(produced, expected, threshold)` (an 8-bit channel). Each one's comment
in `source/check.h` says what it does with NaNs, infinities and noughts of either sign.

### The tier

The tier is a claim about the hardware: how close a correct machine has to come ([Design](Design.md#the-tiers)).
Pick the tightest one the console's definition allows, never the one a machine under test happens to meet.

| Tier | When |
|---|---|
| `CHECK_EXACT` | the answer is defined bit for bit: integers, bit patterns moved, an integer render target, a result code. The model case: the records are compared byte for byte, and only the order of ties is free |
| `CHECK_ULP` | float work, with the budget stated in the case: division within 2.5 units, the transcendentals looser. A float shader is never exact |
| `CHECK_POSITION` | the right answer a pixel or two away (rasterisation), or the order threads ran in |
| `CHECK_LEVEL` | a level measured over a stretch of sound, within a fraction stated in the case |
| `CHECK_BOUND` | texels a machine may keep in a lossy format of its own, within that format's measured loss; a case that came back exact says exact |
| `CHECK_NONE` | the line compares nothing the machine produced: the judge's own cases, a device that opens, a case stopped before it compared. It carries no `expected_from` |

### Where the answer comes from

Every line that compares carries `expected_from` ([The report](Report.md)): its case's when the case says one
(`CHECK_TRUE_FROM`), else its suite's row in `source/origin.c`. The libc suite's row is `REPORT_FROM_SPEC`, so the
model case says nothing and its line carries `spec`.

| Value | The answer comes from |
|---|---|
| `REPORT_FROM_CONSOLE` | a console run |
| `REPORT_FROM_DRIVER` | the console's own driver, read from a game's copy |
| `REPORT_FROM_ASTCENC` | ARM's reference ASTC codec |
| `REPORT_FROM_REFERENCE` | another program independent of this one (a processor model, a reference decoder, a published hash list); the family page names it |
| `REPORT_FROM_EMULATOR` | an emulator's code at a commit the family page names; the service family's answers are reden's |
| `REPORT_FROM_SPEC` | this program's reading of the specification or the hardware's documentation |

A new case's answer comes from the console: a console run, its driver, its documentation, or the specification. Never
from what an emulator answers, its output or its code. Until a console has run the case, its answer is a claim about
the console.

## The judge and its reference check

Every rule a case judges by, past one line of arithmetic, is written once in `source/` and pinned by the reference
suite ([the roadmap's rules](Roadmap.md#rules)), so a failure on a machine is a fact about the machine and not about a
judge nobody checked. The reference suite runs before every family on every build, and in the host build with no
console.

**The judge** is a pure function of what the machine handed back, by this shape:

```c
bool LibcJudgeTiedSort(const LibcRecord sorted[LIBC_TIED_RECORDS], char *out, size_t out_size);
```

It touches no device and no service, returns the verdict, and writes into `out` the sentence the line's detail
carries: on a failure the first thing wrong with its values (`record 3 torn: id 3 with key 1 and mark 0xac, handed
over with key 1 and mark 0xa3`), on a pass what was seen. When the console's definition leaves a choice to the
machine, the judge takes every answer the definition allows: C leaves the order of equal elements to the library, so
this judge passes a stable sort and one that reverses the ties, and holds only what C defines (every record whole,
each once, the keys in order). A judge that wants more than the definition fails a correct machine. What a case
gathers for its judge can be a struct of every answer the machine gave (`ServiceAudinList` and
`ServiceJudgeAudinList` in `source/serviceref.h`). It takes values, never the machine's pointers: the case works out
an index or an offset from a pointer it got (a `bsearch` result, say), with a value of its own for NULL, so the judge
never compares pointers into memory the reference check does not have.

**The reference check** feeds the judge answers worked out by hand and holds its verdict and its sentence to them:

- each right answer a machine gives, the console's first once a console run gave it: here a stable order (glibc
  2.43's) and one with each key's records reversed, both passing, the first with its whole sentence;
- each wrong answer a machine has given or a plausible defect gives, each refused with the start of its sentence,
  and a comment saying who gives it: here a record torn between two by a swap that moved two of its three bytes, an
  id from outside the array, a record lost to its neighbour copied twice, and the records left as they came.

A machine's known wrong answer goes in by name. `CheckAudin` in `source/tests/t_serviceref.c` holds the audin judge to
"No microphone: reden with no capture device, ScarletNX" and to reden with one, and refuses a count past the room
given, an empty name, a name with no end and a refusal; `Rules` in `source/tests/t_bcnref.c` holds the BCn rules to
what an NVIDIA card, RADV under ScarletNX and llvmpipe each decode. When the hand-worked values are too many, a script
in another language that shares no code with the judge works them out: `tools/pace-expect.py`, `tools/vic-expect.py`,
`tools/blit2d-expect.py`.

The reference check's lines are the reference suite's, tier `CHECK_NONE`, named after the family (`libc: the tied
sort judge takes the ties in any order and refuses a torn, lost or unsorted record`), and do not call `ReportStart`.

Before the push, break the judge on purpose and see the reference suite fail: drop one of its conditions (the mark
here), make it demand a stable order, make it miss a lost record. A mutant the suite stays green under is a wrong
answer the check does not hold yet. Each mutant takes seconds, with the file put back after it:

```sh
make -f host.mk build-host/amberlith-host && ./build-host/amberlith-host reference   # reference: passed=N failed=1
```

## Timed cases

A timed case still has a judge for its answer; its time is a measurement, held only to a baseline
([Time per case](TimePerCase.md), [Baselines](Baseline.md)).

- **A drawing suite** whose row in `STEPS` (`source/main.c`) has `timed` true runs ten times: a warm-up, eight
  rehearsals with the report muted and the verdict pass. The case does nothing for it: every submit goes through
  `source/gpu.c`, which times it, and the line carries the smallest of nine samples.
- **A case that times itself** (the pace family, tex8k, the game and the demos) measures with the system tick around
  its work and reports with `ReportCaseTime` or `ReportCaseTimeFrom`. Under the mode word `notime` it reports with
  `ReportCase`, or skips as `REPORT_REASON_ASKED` when a time is all it has (`Begin` in `source/tests/t_pace.c`).

Every run for a time or a verdict has no FPS limit: in ScarletNX, `--fps-limit=0`.

## When a machine ends on it

A case that ends or hangs the program every time on one machine gets a line in `source/bans.txt`, written from the
run that showed it: the suite, the case's name as its line has it, the machine (`console`, `eden`, `ryujinx`,
`scarletnx`, `scarletnx-android`, `android`), its versions, what breaks, that run, the mode words that run the case
again, and the date it was last seen; the file's header says each field. `python3 tools/bans.py` checks every line and
writes `source/banlist.c`, which is committed with it. On that machine, which the mode file's `machine=` word names,
the case is then skipped as `banned`, and `unban` runs it anyway ([The ban list](Crashes.md#the-ban-list)). A case
that never returns on a machine that gets it wrong goes in the hang table, `source/hang.c`, instead
([The hang table](Crashes.md#the-hang-table)). Nothing goes on either list that no run showed.

## A new suite

A suite is one `Run<Suite>` function with no arguments; its lines carry the suite's name. A new one touches:

- `source/tests/tests.h`: its declaration, with a comment that says what it asks and where it runs;
- each frontend that runs its family, in its `STEPS` table, in the place its dependencies put it (the order is the
  roadmap's): `source/main.c` (the NRO), `ounce/frontend.c`, `mod/frontend.c`, `android/src/main/cpp/frontend.c`,
  `vita/frontend.c`; and `source/host_main.c` for a suite the host build runs. The frontends that run a family are
  the ones whose tables list its suites already: `git grep -n RunLibc` finds the libc family's in the NRO, Android,
  the Vita and the host build, and none in the ExeFS or the mod. A step is `{ name, Run<Suite>, family, timed, asks }`
  (`RunnerStep` in `source/runner.h`); `asks` is true when its cases ask the machine something;
- `source/origin.c`: the suite's row, where its answers come from;
- `source/ui.c`, `FAMILIES`, and `source/tests/t_screens.c`, `SUITES`: the same row in both, its family on the
  screens; `tools/test_ui_families.py` fails host-run when a step has no row there. Under `SUITES`, the case "every
  suite a run reports is shown under its own family" holds the table to a count (`count == 120u`): raise it by one.
  The screens' index holds `UI_MAX_GROUPS` suites (`source/ui.h`, 128);
- `docs/Roadmap.md`: its row, with its tier and what a failure points at, in the order it runs. The libc family and
  Android's have no rows there yet, so their suites take none;
- its family page: a section for the suite in the prose. `tools/case-counts.py write` adds the suite's row to the
  family's counts table by itself, and writes a `<!-- counts: suite NAME -->` block only where the page has one;
- every other page that lists the family's suites in prose: `git grep -n -i <family> docs/` finds them (for libc,
  [On the Vita](Vita.md), [On Android](Android.md) and the family page's own count of its suites).

A new suite of an existing family needs no new word in [the mode file](ModeFile.md), since the family's word runs all
its suites, and no line in [Frontends](Frontends.md), which names families.

A new family is rarer: it takes a word in the mode file and a family on the screens. Search for an existing one,
`git grep -n MODE_FAMILY_LIBC` and `git grep -n UI_FAMILY_LIBC`, and add the new one at each place it shows
(`source/mode.h`, `source/mode.c`, `source/ui.h`, `source/ui.c`, `source/tests/t_screens.c`), then its row in
[the mode file](ModeFile.md), its page in `docs/families/` with its counts blocks, and its line in
[the families](families/README.md) and in [the docs' index](README.md).

## Running it

```sh
make -f host.mk host-run                    # the judge, the libc family, the tools' tests: before each push
make -f host.mk build-host/amberlith-host && ./build-host/amberlith-host reference   # the reference suite alone
grep 'tied sort\|three-byte' amberlith-host-report.jsonl
grep '"result":"fail"' amberlith-host-report.jsonl                                  # what failed, if anything
```

`host-run` takes about two minutes; on a machine others build on too, run it under the machine's build lock. Its
output is long: a case that failed is a `"result":"fail"` line of the report, and a tool's test that failed a `FAIL:`
line of the output. The host build writes `amberlith-host-report.jsonl` where it runs; a case's line is found by its
name. No word runs a single case: the narrowest is a family's word, with `skip=<suite>` for the suites left out
([The mode file](ModeFile.md)), or the browser on a machine, which runs a family, a suite or one case
([The browser](UI.md)).

The first `host-run` after a case is added fails, as it should, in `tools/test_case_counts.py` ("cases the host ran
that docs/cases.tsv does not give it"): the catalogue has to have every case the host build ran. Write it from that
run, then the counts, and run `host-run` again:

```sh
python3 tools/case-counts.py catalogue host=amberlith-host-report.jsonl
python3 tools/case-counts.py write
```

and commit `docs/cases.tsv` with the blocks. A case only a machine runs reaches the catalogue from that machine's
report (`nro=<report.jsonl>`), and every case is counted again on the commit to be released
([Counting the cases](Releases.md#counting-the-cases)).

On a machine: `make` builds the NRO; `make run EMULATOR=<path> MODE="quick libc"` runs it under an emulator ([Under an
emulator](Emulator.md)). In ScarletNX: `scripts/amberlith.py run --runtime <build>/scarletnx --nro amberlith.nro --out
<folder> --mode "quick libc"`, headless; it adds the `machine=` word itself. The pull request's CI builds the NRO, the
ExeFS and the mod, the APK and the VPK, and runs `host-run` ([Releases](Releases.md#the-ci)).

## The version

`VERSION` holds one line, the version, which is semantic: a new case or suite is a minor version (1.0.4 to 1.1.0), a
changed verdict of an existing case at least that ([Versions](Versions.md)). A pull request bumps it once, however many
cases it brings. The bump is its last commit, never a pull request of its own; once `main`'s build has passed after the
merge, that commit is tagged `v<version>`, annotated. Each commit's message is `amberlith: what changed` with one to
three lines of why ([Build](Build.md#commit-messages)).

The case binds ScarletNX once ScarletNX pins a release that has it ([CLAUDE.md](../CLAUDE.md#versions-and-the-pin)).
