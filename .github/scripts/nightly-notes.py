# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""A release's file names, title and notes: the NRO, the APK, the VPK, the 3DSX, the NDS and the two ExeFS folders, named after the version (and a
nightly's after its commit too), with their sizes and SHA-256, where to read how to run them, what changed since the
previous release, and where the source is, which the GPL asks for beside the binary. publish-builds.py beside it takes
them for each nightly and each version.

The changes are the checkout's history from the commit the previous release was built from to the one built, left out
without it or when it is not an ancestor. The notes link the built commit first, which is where the next nightly finds
it.
"""

import collections
import re
import subprocess

SERVER = "https://github.com"
# What a release holds, in this order: the NRO, the APK, the VPK, the 3DSX, the NDS and two ExeFS folders zipped, each
# with its checksum beside it
# in the form sha256sum -c reads; each kind's name before the version, its ending after it
KINDS = {
    "nro": ("amberlith", ".nro"),
    "apk": ("amberlith", ".apk"),
    "vpk": ("amberlith", ".vpk"),
    "3dsx": ("amberlith", ".3dsx"),
    "nds": ("amberlith", ".nds"),
    "ounce": ("amberlith-ounce", ".zip"),
    "mod": ("amberlith-mod", ".zip"),
}
SUM = ".sha256"
# what each file is, after its name in the notes
RUNS = {
    "nro": "runs on the console and under an emulator",
    "apk": "runs on Android and on ScarletNX's Android layer",
    "vpk": "runs on a Vita, in Vita3K and in ScarletNX's Vita runtime",
    "3dsx": "runs on a 3DS from the Homebrew Launcher, and in a 3DS emulator",
    "nds": "runs on a DS from a card's menu, and in a DS emulator",
    "ounce": "holds an ExeFS folder, the probe as a Switch 2 program, which ScarletNX runs",
    "mod": (
        "holds an ExeFS folder, the probe as a mod of a game, in the game's mods folder on a console with Atmosphere, "
        "in an emulator or in ScarletNX"
    ),
}
MERGE = re.compile(r"Merge pull request #(\d+) from \S+")
# what text from a commit could make of the markdown around it: a link, a tag, a table cell
MARKDOWN = re.compile(r"([\\`*_\[\]<>|~])")
MOST_CHANGES = 30

# A file as the release holds it: its kind (a key of KINDS), its name, its bytes and its SHA-256
Built = collections.namedtuple("Built", "kind name count digest")

# A change of main: its subject or its pull request's title, the pull request's number (None for a commit pushed or
# rebased onto main), and the commit
Change = collections.namedtuple("Change", "text pr sha")


def git(checkout, *args):
    return subprocess.run(["git", "-C", checkout, *args], check=True, capture_output=True, text=True).stdout


def escape(text):
    """text shown as it is, in GitHub's markdown and Discord's"""
    return MARKDOWN.sub(r"\\\1", text)


def changes(checkout, previous, sha):
    """What main got from PREVIOUS to SHA, newest first: a merged pull request by its title, any other commit by its
    subject. None when PREVIOUS is missing or not an ancestor of SHA"""
    if not previous:
        return None
    ancestor = subprocess.run(
        ["git", "-C", checkout, "merge-base", "--is-ancestor", previous, sha], capture_output=True, check=False
    )
    if ancestor.returncode:
        return None
    items = []
    log = git(checkout, "log", "--first-parent", "--format=%H%x1f%s%x1f%b%x1e", f"{previous}..{sha}")
    for record in log.split("\x1e"):
        record = record.strip("\n")
        if not record:
            continue
        commit, subject, body = record.split("\x1f", 2)
        merge = MERGE.fullmatch(subject)
        if merge:
            title = next((line.strip() for line in body.splitlines() if line.strip()), subject)
            items.append(Change(title, int(merge.group(1)), commit))
        else:
            items.append(Change(subject, None, commit))
    return items


def change_line(change, url):
    """a change as a line, its pull request or its commit linked in the repository at url"""
    if change.pr:
        return f"{escape(change.text)} ([#{change.pr}]({url}/pull/{change.pr}))"
    return f"{escape(change.text)} ([`{change.sha[:7]}`]({url}/commit/{change.sha}))"


def title(checkout, sha):
    return f"Amberlith nightly {git(checkout, 'log', '-1', '--format=%cs', sha).strip()} ({sha[:7]})"


def size(count):
    return f"{count / 1e6:.1f} MB"


def names(version, commit=""):
    """each kind's file name: amberlith-<version>.nro in a version's release, amberlith-<version>-<commit>.nro in a
    nightly and as a run's artifact"""
    stem = f"{version}-{commit}" if commit else version
    return {kind: f"{before}-{stem}{ending}" for kind, (before, ending) in KINDS.items()}


def download(url, sha, where, built):
    """the files (Built, the NRO first), linked from the release they are in, with their sizes and SHA-256; their
    checksum files; the documents"""
    rows = [f"| [{made.name}]({where}/{made.name}) | {size(made.count)} | `{made.digest}` |" for made in built]
    sums = [made.name + SUM for made in built]
    runs = "; ".join(f"{made.name} {RUNS[made.kind]}" for made in built)
    linked = [f"[{name}]({where}/{name})" for name in sums]
    if len(sums) == 1:
        said = f"{runs}. {linked[0]} holds its checksum: `sha256sum -c {sums[0]}` beside it checks it."
    else:
        listing = ", ".join(linked[:-1]) + f" and {linked[-1]}"
        said = f"{runs}. {listing} hold their checksums: `sha256sum -c {' '.join(sums)}` beside them checks them all."
    kinds = {made.kind for made in built}
    documents = [
        "Emulator",
        "Console",
        "Android",
        *(
            name
            for kind, name in (("vpk", "Vita"), ("3dsx", "3DS"), ("nds", "DS"), ("ounce", "Ounce"), ("mod", "Mod"))
            if kind in kinds
        ),
    ]
    linked_documents = [f"[{name}.md]({url}/blob/{sha}/docs/{name}.md)" for name in documents]
    return [
        "## Download",
        "",
        "| File | Size | SHA-256 |",
        "|---|---|---|",
        *rows,
        "",
        said,
        "",
        (
            f"How to run it: {', '.join(linked_documents[:-1])} and {linked_documents[-1]}; how the releases are "
            f"made: [Releases.md]({url}/blob/{sha}/docs/Releases.md)."
        ),
    ]


def source(tree, built):
    """Where a build's source is, which the GPL asks for beside the binary, and what it links, folded"""
    kinds = {made.kind: made.name for made in built}
    packed = [kinds[kind] for kind in ("ounce", "mod") if kind in kinds]
    return [
        "<details>",
        "<summary>Source and licences</summary>",
        "",
        f"The source of this build is at {tree}.",
        "",
        (
            "- The NRO was built in the devkitPro container .github/workflows/build.yml names, with devkitA64, "
            "and links libnx (ISC) and deko3d (zlib) from it."
        ),
        *(
            [
                "- The APK was built with the Android SDK and NDK .github/workflows/build.yml names, and links only "
                "the libraries every Android device has."
            ]
            if "apk" in kinds
            else []
        ),
        *(
            [
                "- The VPK was built with the VitaSDK .github/workflows/build.yml names, and links its C library and "
                "the system's stubs; the gxm suite draws with vita2d's compiled shaders (MIT)."
            ]
            if "vpk" in kinds
            else []
        ),
        *(
            [
                "- The 3DSX was built in the devkitARM container .github/workflows/build.yml names, and links "
                "libctru (zlib) from it."
            ]
            if "3dsx" in kinds
            else []
        ),
        *(
            [
                "- The NDS was built in the same devkitARM container, and links libnds (zlib) and calico (ZPL 2.1) "
                "from it, with calico's ARM7 program."
            ]
            if "nds" in kinds
            else []
        ),
        *(
            [
                f"- {' and '.join(packed)} {'were' if len(packed) > 1 else 'was'} built in the same container as "
                f"the NRO, and {'link' if len(packed) > 1 else 'links'} libnx (ISC) from it."
            ]
            if packed
            else []
        ),
        "- What else comes from elsewhere, and under which licence, is in docs/Credits.md.",
        "",
        "</details>",
    ]


def notes(
    checkout,
    sha,
    previous,
    repo,
    where,
    built,
    kept,
    since="the previous nightly",
    lead="Built from main at",
):
    """where: the release the files are in; built: its files (Built), the NRO first; kept: what becomes of the
    release; since: what previous was; lead: what the first line calls the build"""
    url = f"{SERVER}/{repo}"
    date = git(checkout, "log", "-1", "--format=%cs", sha).strip()
    lines = [f"{lead} [`{sha[:7]}`]({url}/commit/{sha}), committed {date}." + (f" {kept}" if kept else ""), ""]
    lines += download(url, sha, where, built)
    items = changes(checkout, previous, sha)
    if items is not None:
        compare = f"{url}/compare/{previous}...{sha}"
        counted = f"{len(items)} change{'' if len(items) == 1 else 's'}, the newest first" if items else "no change"
        lines += ["", f"## Changes since {since}", "", f"[`{previous[:7]}...{sha[:7]}`]({compare}): {counted}.", ""]
        lines += [f"- {change_line(item, url)}" for item in items[:MOST_CHANGES]] or [
            "None: the same commit, built again."
        ]
        if len(items) > MOST_CHANGES:
            lines.append(f"- and [{len(items) - MOST_CHANGES} more]({compare})")
    lines += ["", *source(f"{url}/tree/{sha}", built)]
    return "\n".join(lines) + "\n"
