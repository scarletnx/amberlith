# Versions

`VERSION` holds the version (semantic), and a tag `v<version>` on `main` marks each. Every report opens with a note
naming it and the build as git describes the tree, `Amberlith 1.0.0 (v1.0.0) starting` (`-dirty` when uncommitted
changes were built), and goes in a folder named after that commit
([Where a run leaves its report](Report.md#where-a-run-leaves-its-report)). Compare two reports only when that line
agrees, or when the difference between the builds is what is measured. A new suite or case is a minor version; a
changed verdict of an existing case at least that.

The cover a loader shows is `icon.jpg`, made by `make icon` and committed, so a build needs neither drawing tool:
Wimsy as Amberlith (`static/wimsy-amberlith.svg`), in amber with a test probe, over a dark amber sun cut by bands,
with the program's name. `tools/cover.py` writes the cover's drawing, `static/icon.svg`, from hers, and `make
wimsy-picture` the picture of her the start screen draws under its rows (`source/wimsypicture.c`, also committed).
