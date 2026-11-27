# Versions

`VERSION` holds the version (semantic), and a tag `v<version>` on `main` marks each. Every report opens with a note
naming it and the build as git describes the tree, `Amberlith 1.0.0 (v1.0.0) starting` (`-dirty` when uncommitted
changes were built), and goes in a folder named after that commit
([Where a run leaves its report](Report.md#where-a-run-leaves-its-report)). Compare two reports only when that line
agrees, or when the difference between the builds is what is measured. A new suite or case is a minor version; a
changed verdict of an existing case at least that.

The cover a loader shows is `icon.jpg`, and the 3DS's launcher `3ds/icon.png`, both made by `make icon` and
committed, so a build needs neither drawing tool:
Wimsy as Amberlith (`static/wimsy-amberlith.svg`), in amber with a test probe, over a dark amber sun cut by bands,
with the program's name. `tools/cover.py` writes the cover's drawing, `static/icon.svg`, from hers, and `make
wimsy-picture` the picture of her the start screen draws under its rows (`source/wimsypicture.c`, also committed).

## Before 1.0.0

Amberlith was edenprobe up to 0.18.1. What an edenprobe build or its tools left is still read:

- **The card's folder.** A card where an edenprobe build ran (`sdmc:/edenprobe` holds a mode file or `runs/`) and
  no Amberlith build has (`sdmc:/amberlith` holds neither) runs from `sdmc:/edenprobe` whole: its mode file, net
  file, crash list, baselines and runs, so a launcher that still writes the old folder's mode file finds its report
  there. The report's notes say which folder the launch ran from and why; renaming the folder `sdmc:/amberlith` moves
  the card over. With both, `sdmc:/amberlith` alone is read, and a note says the old one is there.
- **Reports.** `tools/case-counts.py` takes an edenprobe build's report as it takes Amberlith's (its first note
  `edenprobe <version> ... starting`, the host build's `edenprobe host build`), so the catalogue's earlier counts
  stand.
- **Variables.** `EDENPROBE_FRAMES` and the masquerade layer's `EDENPROBE_MASQUERADE_API`, `_HIDE` and
  `_NO_HOST_QUERY_RESET` are read when the `AMBERLITH_` one is not set, with a line on stderr naming both.
- **Commits and pull requests.** The commit hook takes `edenprobe:` as it takes `gfxprobe:`, for history; a pull
  request's builds comment from before is rewritten in place.

What changed and is not read under its old name: the program's files (`amberlith.nro`, `amberlith-ounce/`,
`amberlith-mod/`, the APK `org.scarletnx.amberlith`, a new app beside the old one), the debug channel's markers
(`AMBERLITH DONE`, `AMBERLITH TIMING`), the masquerade layer's name (`VK_LAYER_AMBERLITH_masquerade`) and the net
family's `.invalid` names (`*.amberlith.invalid`), which `tools/net-backend.py` publishes, and the repository,
`github.com/scarletnx/amberlith`, to which GitHub sends the old address.
