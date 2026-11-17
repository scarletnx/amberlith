# Releases

Every push to `main` that builds becomes a nightly, and every version tag a release, each with these files and a
`.sha256` beside each, named after the version, the commit's `VERSION`, which the report's first line names too. A
nightly's carry the commit after it, `amberlith-0.18.0-0123abc.nro`. Only the CI uploads them.

| File, in version 0.18.0 | What it is |
|---|---|
| `amberlith-0.18.0.nro` | the probe, for a console's homebrew menu and an emulator |
| `amberlith-0.18.0.apk` | the probe on Android ([On Android](Android.md)) |
| `amberlith-ounce-0.18.0.zip` | the probe as a Switch 2 program, which ScarletNX runs ([Ounce](Ounce.md)) |
| `amberlith-mod-0.18.0.zip` | the probe as a mod of a game, the nvn suite on the game's own driver ([Mod](Mod.md)) |

Each zip holds an ExeFS folder, `exefs/` and its files, which goes whole where its program is read from.

| What | Where |
|---|---|
| The newest version | https://github.com/scarletnx/edenprobe/releases/latest |
| One version | `https://github.com/scarletnx/edenprobe/releases/tag/v<version>` |
| The newest nightly | https://github.com/scarletnx/edenprobe/releases/tag/nightly |
| One nightly (the newest 30 are kept) | `https://github.com/scarletnx/edenprobe/releases/tag/nightly-<date>-<commit>` |

A file's own link is `.../releases/download/<tag>/<name>`, `.../download/v0.18.0/amberlith-0.18.0.nro`. The newest
version's NRO and its checksum without knowing its number, with GitHub's CLI (`gh release download nightly ...` takes
the newest nightly's):

```sh
gh release download -R scarletnx/edenprobe -p 'amberlith-*.nro' -p 'amberlith-*.nro.sha256'
sha256sum -c amberlith-*.nro.sha256
```

`nightly`'s files are replaced one after the other, so a check that fails while a nightly lands passes when they are
fetched again; a dated nightly and a version never change. A pull request's builds are its Build run's artifacts,
named as a nightly's, `amberlith-<version>-<commit>.nro`, `.apk`, `amberlith-ounce-<version>-<commit>.zip` and
`amberlith-mod-<version>-<commit>.zip`, linked from a comment on the pull request (a fork's on the run's page).

## The CI

- **`build.yml`**, on each push to `main`, each `v*` tag and each pull request: the NRO in devkitPro's container,
  pinned by tag and digest, checked to name its own commit, clean, and uploaded as the run's artifact, and in the same
  job the Switch 2 program and the mod, each checked to name the NRO's build and uploaded as its ExeFS folder
  zipped; the APK with the runner's Gradle, JDK 21 and Android SDK, its native library checked to name the same
  commit, and uploaded the same way; then `make -f host.mk host-run`. Each artifact is named after `VERSION` and the
  commit, and a `VERSION` that is no version stops the build. A tag must be `v` and `VERSION`, or the build stops. A
  newer push to a pull request cancels its older build.
- **`publish.yml`**, after a Build run of a push passes: for `main`, the prerelease `nightly-<date>-<commit>` (never
  the latest) and the prerelease `nightly`, whose tag and files follow the newest, its files named as the build's
  artifacts; for a tag, its release, the latest (a tag with a `-` is a prerelease), its files named after the version
  alone. Each zip is checked to hold exactly its folder's files (`exefs/main` or
  `exefs/subsdk9`, and `exefs/main.npdm`), each starting as an NSO or an NPDM does. Each release lists the changes
  since the one before.
- A version's release also gets a message on Discord, on the channel of the secret `DISCORD_WEBHOOK_RELEASES`;
  without it, nothing is posted there and the run says so.

To release a version: count the cases (below), change `VERSION` on `main`, tag that commit `v<VERSION>` and push the
tag. The scripts are in
`.github/scripts`; `make -f host.mk host-run` runs their tests against a fake GitHub and a fake Discord.

## Counting the cases

Every count of cases in the docs is a block between `<!-- counts: ... -->` and `<!-- /counts -->`, written by
`tools/case-counts.py` from [the catalogue](cases.tsv), `docs/cases.tsv`: a line a case, its suite, its family, its
judge, where its expected answer comes from ([the report](Report.md)) and the builds that ran it. The CI fails when a
block is not what the catalogue gives, when `source/origin.c` has no row for a suite that judges a machine, and when
the host build ran a case the catalogue does not give it or said another source, so a judge, libc or music case added
shows at once; the cases only a machine runs are counted again before each release, on the commit to be released:

```sh
make run EMULATOR=/path/to/emulator TIMEOUT=3600 MODE="quick hooks all pad nvn demo music pace game stress level=0 level=1 level=2 level=3 level=4"
make -f host.mk host-run
python3 tools/case-counts.py catalogue nro=<that run's report.jsonl> host=amberlith-host-report.jsonl
python3 tools/case-counts.py write
```

and commit `docs/cases.tsv` with the blocks. Those words run every family at the default frames: the game and demo
cases' names carry the number of frames, and a demo's judged frames need 600. The launch's limit is an hour: a launch
stopped at the default minute skips the case it was in, and the pace family alone takes two. Pad waits for a person,
and its cases skipped unanswered keep the judge the catalogue had for them. A build given alone updates only its own
cases (`ounce=` the Switch 2 build's report, for nvn2; `apk=` Android's); `python3 tools/case-counts.py unjudged
[FAMILY]` lists the cases with no judge, or none known yet, and `unsaid [FAMILY]` the cases with a judge whose lines
said no source.
