# CLAUDE.md

Rules for an AI that works in Amberlith, or in ScarletNX on a change Amberlith should check. The docs in `docs/` are
the reference; this file says what to do and when.

## What Amberlith is

A test program for the console and its emulators. Each case works out the answer it expects, checks the one it got
and writes a line to `report.jsonl`; no case needs a person or a screenshot ([Design](docs/Design.md)). ScarletNX's CI
runs a pinned release of it on every pull request, so a case here is a rule ScarletNX is held to.

- `source/`: the engine, every frontend's. `source/tests/t_*.c` run the cases on the device; `source/*ref.[ch]` are
  the judges and their CPU references; `source/origin.c` says where each suite's answers come from; `source/bans.txt`
  is the list of cases a machine is known to end on.
- Frontends ([Frontends](docs/Frontends.md)): the NRO (`source/main.c`, `Makefile`), the Switch 2 ExeFS (`ounce/`,
  `ounce.mk`), a mod of a game (`mod/`, `mod.mk`), Android (`android/`), the Vita (`vita/`, `vita.mk`), the 3DS
  (`3ds/`, `3ds.mk`), the DS (`nds/`, `nds.mk`), and the host build (`host.mk`).
- `docs/families/`: one page a family, its suites and cases. `docs/cases.tsv`: the catalogue of every case.
- `tools/`: the scripts and their tests. `.github/`: the CI and the release scripts. `hooks/`: the commit hook.

## When a change asks for a new case

A ScarletNX change gets an Amberlith case when it adds or changes behaviour a homebrew program can observe:

- the GPU or Vulkan path (a format, a blend, a copy, coherence between the processor and the GPU, a draw's state);
- the HLE of the kernel or of a service (an answer, an error code, a timing rule);
- a defect found in a game, once the probe can make the same thing happen without the game.

Internal checks of ScarletNX's own functions stay in ScarletNX's tests. A defect gets both: the unit test in
ScarletNX that fails without the fix, and the Amberlith case. When the probe cannot express it, the ScarletNX pull
request says why.

Whoever writes the ScarletNX change writes the case. It goes in its own pull request to scarletnx/amberlith, named in
the ScarletNX pull request's body. The case binds ScarletNX's CI once a release that has it is pinned there (below).

## Writing a case

[Adding a case](docs/AddingACase.md) gives the files in order, the check macros, the judge and a model case.

- Put it in the family and suite where its subject is ([Roadmap](docs/Roadmap.md) gives the order suites run in).
  A new suite takes its row in `source/origin.c`, the start screen's row in `source/ui.c`, and its family page.
- The expected answer comes from the console: a console run, the console's driver, the hardware's documentation, or
  this program's reading of the specification, named in the case's `expected_from` ([Report](docs/Report.md)). Never
  from an emulator, its output or its code. Until a console has run it, the answer is a claim about the console.
- Pick the tier the hardware allows (exact, units in the last place, position, level, bound;
  [Design](docs/Design.md#the-tiers)). A float shader is never exact.
- A timed case still has a judge for its answer; its time is a measurement, held only to a baseline
  ([Time per case](docs/TimePerCase.md)). Every run for a time or a verdict has no FPS limit: `--fps-limit=0` in
  ScarletNX.
- The judge has a reference check that holds it to the right answer and to the wrong ones a machine gives, ScarletNX's
  among them; it runs in the host build, without a console.
- Update the catalogue and the docs' counts: `python3 tools/case-counts.py write`, from the runs that
  [Releases](docs/Releases.md#counting-the-cases) lists. The CI fails on a count that is not the catalogue's.

## Running it

| Build | Command | Notes |
|---|---|---|
| Host | `make -f host.mk host-run` | the judge, screens, libc family and tools' tests; before each push |
| NRO | `make` | devkitPro in `/opt/devkitpro`; `make run EMULATOR=...` for a quick run under an emulator |
| APK | `gradle -p android assembleDebug` | `ANDROID_HOME` and a JDK 17 or newer |
| VPK | `make -f vita.mk` | `VITASDK` set ([On the Vita](docs/Vita.md)) |
| 3DSX | `make -f 3ds.mk` | `DEVKITPRO` set, with devkitARM and libctru in it ([On the 3DS](docs/3DS.md)) |
| NDS | `make -f nds.mk` | `DEVKITPRO` set, with devkitARM, libnds and calico in it ([On the DS](docs/DS.md)) |

The mode file chooses what runs ([Mode file](docs/ModeFile.md)); `quick all` runs every family that needs nobody.
In ScarletNX: `scripts/amberlith.py run --runtime <build>/scarletnx --nro amberlith-<version>.nro --out <folder>
--mode "<words>"`, headless, in its own folders.

The CI (`.github/workflows/build.yml`) builds the NRO, the ExeFS and the mod, the APK, the VPK, the 3DSX and the NDS,
runs the host build, and links each build from the pull request. `publish.yml` makes a push to `main` a nightly and a
`v<version>` tag a release ([Releases](docs/Releases.md)). A pull request merges on green, by rebase.

## Baselines

ScarletNX holds each run of a pinned release to a baseline: in scarletnx/test-references, folder `amberlith/`
(`linux-x86_64.txt`, `android-x86_64.txt`, `compute-readback.txt`; its README). Write it again from CI's own runs, never
a desktop GPU's: download the artifact of two or three runs of the job and keep what ended the same way in each, with
ScarletNX's script, in a clone of test-references:

```sh
python3 <scarletnx>/scripts/amberlith.py baseline run1/baseline.txt run2/baseline.txt -o amberlith/linux-x86_64.txt
```

A machine's timing baselines are Amberlith's own `baseline=` and `compare=` words ([Baselines](docs/Baseline.md)).

## Versions and the pin

- `VERSION` is semantic: a new case or suite is a minor version, a changed verdict at least that
  ([Versions](docs/Versions.md)).
- The bump is the last commit of the pull request that releases it, never a pull request of its own. After the merge,
  once `main`'s Build passed, tag that commit `v<version>` (annotated).
- ScarletNX takes the release in one pull request: the pin in `.github/workflows/build.yml` (`AMBERLITH_VERSION`, the
  NRO's or the APK's name and SHA-256), and the baseline in scarletnx/test-references written again from that pin's
  runs.

## Commits

`amberlith: what changed`, at most 72 characters; one to three lines of intent, each at most 90; a `Change-Id`, which
the hook writes; one `Signed-off-by` that is also the author and the committer. ASCII only, no merge commits.
`git config core.hooksPath hooks` turns the check on ([Build](docs/Build.md#commit-messages)). One commit for each
change; a fix to a commit already pushed is a commit of its own.

## C by target

The Vita frontend (`vita/`) and the 3DS and DS frontends are C23 (`-std=c23`; the DS `-std=gnu23` for its library's
headers), written by [Code style](docs/CodeStyle.md): `static_assert`, `[[...]]`, `alignas`, `nullptr`, `= {0}` over
`memset`, no undefined or implementation-defined behaviour, one `g_<file>` global a file, no pointer kept in data, one
allocation an owner. Everything else is C11: `host.mk`, the NRO, the ExeFS and the mod, Android and `tools/vklayer`. Do not move
the rest of the repo to C23 until that is asked.

## Never

- A game's name, title ID or nickname in code, cases, commits, pull requests or docs; nor its code, disassembly,
  assets or a value only that game uses. A case asks the console the question a game's failure comes down to.
- A screenshot or a hash of a frame as a verdict.
- A case judged by what an emulator answers.

## Not covered yet (TODO)

- The 3DS's and the DS's frontends run the judge and the libc family, none of their own suites yet: a ScarletNX
  change to their drivers gets a case there only when the engine's suites can ask it. The DS runs the judge's libc
  references alone ([On the DS](docs/DS.md#what-fits-in-its-memory)).
