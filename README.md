# Amberlith
<p align="center">
  <img src="./static/icon.svg" alt="Wimsy as Amberlith, in amber with a test probe" width="200"><br>
  <b>Wimsy as Amberlith</b>
</p>

A test program for the console and its emulators. Every case works out the answer it expects, checks the one it
got, and writes it to a report that can be compared between two machines: a console and an emulator, or two
builds of one emulator. No case needs a person to look at the screen.

## Quick start

The newest version is on [its release](https://github.com/scarletnx/amberlith/releases/latest), as
`amberlith-<version>.nro`, and the newest nightly on
[nightly](https://github.com/scarletnx/amberlith/releases/tag/nightly) ([Releases](docs/Releases.md)).

- On a console: put it in the SD card's `switch/` folder and open it from the homebrew menu.
- In an emulator: open the NRO.
- On Android, or ScarletNX's Android layer: the APK, `android/` ([On Android](docs/Android.md)), the same engine
  with Android's own cases beside the ones every machine runs.
- On a Vita, in Vita3K or in ScarletNX's Vita runtime: the VPK, `vita/` ([On the Vita](docs/Vita.md)), the same engine
  with the Vita's GPU cases beside the ones every machine runs.
- On a 3DS, from the Homebrew Launcher, or in a 3DS emulator: the 3DSX, `3ds/` ([On the 3DS](docs/3DS.md)), the same
  engine with the libc family whole.
- On a DS, from a card's menu, or in a DS emulator: the NDS, `nds/` ([On the DS](docs/DS.md)), the same engine with
  the libc family's suites its memory holds.

Without a mode file the start screen asks what to run, and its L and R open the browser, where a person runs a suite,
a single case or a demo at a time ([The browser](docs/UI.md)); `sdmc:/amberlith/mode` chooses instead
([Mode file](docs/ModeFile.md)): `quick all` runs every family that needs nobody, all but pad, nvn, demo, music and pace. Naming a family
runs that family alone. nvn's window case runs only in a quick run from the mode file, never in one begun on the start
screen, `noprogress` or not. The report goes to `sdmc:/amberlith/runs/<build>/report.jsonl`
([Report](docs/Report.md)).

## Build

With devkitPro in `/opt/devkitpro`:

```sh
export DEVKITPRO=/opt/devkitpro
make                                  # amberlith.nro
make -f host.mk host-run              # the judge and the tools' tests, on this machine
gradle -p android assembleDebug       # the APK, with ANDROID_HOME and a JDK 17 or newer
make -f vita.mk                       # the VPK, with VITASDK set (docs/Vita.md)
make -f 3ds.mk                        # the 3DSX, with DEVKITPRO set and devkitARM in it (docs/3DS.md)
make -f nds.mk                        # the NDS, the same way (docs/DS.md)
make run EMULATOR=/path/to/emulator   # a quick run, started again after a crash
```

See [Build](docs/Build.md), [Running it under an emulator](docs/Emulator.md),
[Running it on Android](docs/Android.md) and [Frontends](docs/Frontends.md). Every document: [docs](docs/README.md).

## Credits

Amberlith follows switch-examples and builds on libnx and deko3d; what comes from each, and from the other
projects it uses, is in [Credits](docs/Credits.md). Wimsy, ScarletNX's mascot, was drawn by Liz.

## Licence

GPL-3.0-or-later; the full text is in `LICENSE`.
