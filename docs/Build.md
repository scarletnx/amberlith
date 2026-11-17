# Build

## The NRO

```sh
export DEVKITPRO=/opt/devkitpro
make
```

`amberlith.nro` runs unchanged on a console and in an emulator; two reports compare only when the NRO is the same.

One shader, the bindless suite's four sample fetch, is built by uam with Mesa's f9d385d77716 on it: the toolchain's uam
1.1.0 puts a multisample fetch's sample in the register the console reads the handle from. `tools/uam.sh` builds that
uam into `build/uam` the first time make needs it (and after `make clean`): it clones scarletnx/uam at the commit it
names (a clone already there, the toolchain's from before, is pointed at it and fetched), the toolchain's uam with the
fix of a uniform block's vector read by a component worked out at run time (which uam 1.1.0 reads from the vector's
first word), applies `tools/uam/*.patch`, and builds it with meson, so it needs the network then, git, meson, ninja,
bison, flex, Python 3 with mako, and a C and a C++ compiler for the machine. `tools/dksh-handles.py
build/shaders/*.dksh` says where each multisample fetch reads its handle; the CI runs it after every build.

## The Switch 2 ExeFS

`make -f ounce.mk`, with `DEVKITPRO` set as for the NRO, builds the probe as a Switch 2 program:
`amberlith-ounce/exefs/main` and `main.npdm` ([The probe as a Switch 2 program](Ounce.md)). It needs no uam: its
shader programs are assembled by `tools/sm86asm.py`. No release carries it.

## The mod of a game

`make -f mod.mk`, with `DEVKITPRO` set as for the NRO, builds the probe as one more module of a game's program:
`amberlith-mod/exefs/subsdk9` and `main.npdm` ([The probe as a mod of a game](Mod.md)). `make -f mod.mk check` then
holds the module to `mod/imports.txt` (`tools/mod-imports.py`). It needs no uam, having no shader. No release
carries it.

## The APK

```sh
gradle -p android assembleDebug -Pamberlith.out=/tmp/amberlith-apk
```

With the Android SDK in `ANDROID_HOME` and a JDK 17 or newer in `JAVA_HOME`; what it needs and where the APK lands is
in [On Android](Android.md#build-it).

## The host build

```sh
make -f host.mk host-run
```

It builds what touches no console and runs it here: the three judge suites, the screens and their navigation (on
frames composed in memory from a fixture), the picture writer, the mode file's words and the time a case reports,
and the [libc](families/Libc.md) family's cases on this machine's C library. Then [the NVN layer](NvnLayer.md)'s
own test, built for each of its API versions and for a game's own driver, the test of what the mod reads of a
game's SDK module (`mod/tests/host_test.c`), and the tools' tests (`tools/perf.py`,
`tools/run.py`, `tools/validate.py`, `tools/libc-math.py`, `tools/dksh-handles.py`, `tools/astc-blocks.py`,
`tools/astc-images.py`, `tools/wimsy-mesh.py`, `tools/cover.py`, `tools/vic-expect.py`, `tools/blit2d-expect.py`,
`tools/vp9-jobs.py`, `tools/nvn-lookups.py`, `tools/nvn-coverage.py`, `tools/npdm-ounce.py`, `tools/mod-imports.py`, `tools/mod-lacked.py` and
`tools/sm86asm.py`), which need Python 3, the CI scripts' tests in `.github/scripts`, against a fake GitHub and a fake Discord, which also need
git (and bash and jq for the pull request comment's tests, skipped and said so without them), and the APK's runners'
(`android/tests`), against a fake adb and a fake ScarletNX driver, in bash. Writing the nvdec
suite's jobs again needs FFmpeg and libvpx's vectors: see [nvdec](families/Nvdec.md).

To see the screens without a console, name an existing folder:

```sh
AMBERLITH_FRAMES=/tmp/frames make -f host.mk host-run
```

The screen cases then write each frame they walk through there as a PAM file (`magick 1-overview.pam
1-overview.png`). Nothing reads them back, so they change no verdict.

## Commit messages

`amberlith: what changed`, one to three lines of intent, a `Change-Id` (the hook writes one), one Signed-off-by
that is also the author and the committer, ASCII, no merges. `git config core.hooksPath hooks` turns the check on;
`sh hooks/test-commit-msg.sh` shows what it accepts, and `--history <range>` checks existing commits.
