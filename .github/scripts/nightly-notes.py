# SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""A release's title and notes: the NRO, the APK and the two ExeFS folders with their sizes and SHA-256, where to read
how to run them, what changed since the previous release, and where the source is, which the GPL asks for beside the
binary. publish-builds.py beside it takes them for each nightly and each version.

The changes are the checkout's history from the commit the previous release was built from to the one built, left out
without it or when it is not an ancestor. The notes link the built commit first, which is where the next nightly finds
it.
"""

import collections
import re
import subprocess

SERVER = "https://github.com"
# What a release holds: the NRO, the APK and two ExeFS folders zipped, each with its checksum in the form sha256sum -c
# reads
NRO, SUM = "edenprobe.nro", "edenprobe.nro.sha256"
APK, APK_SUM = "edenprobe.apk", "edenprobe.apk.sha256"
OUNCE, OUNCE_SUM = "edenprobe-ounce.zip", "edenprobe-ounce.zip.sha256"
MOD, MOD_SUM = "edenprobe-mod.zip", "edenprobe-mod.zip.sha256"
# what runs each ExeFS, after its name in the notes
RUNS = {
    OUNCE: "the probe as a Switch 2 program, which ScarletNX runs",
    MOD: (
        "the probe as a mod of a game, in the game's mods folder on a console with Atmosphere, in an emulator or in "
        "ScarletNX"
    ),
}
MERGE = re.compile(r"Merge pull request #(\d+) from \S+")
# what text from a commit could make of the markdown around it: a link, a tag, a table cell
MARKDOWN = re.compile(r"([\\`*_\[\]<>|~])")
MOST_CHANGES = 30

# An ExeFS folder as the release holds it: its zip's name, its checksum file's, its bytes and its SHA-256
Packed = collections.namedtuple("Packed", "name sum count digest")

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
    return f"edenprobe nightly {git(checkout, 'log', '-1', '--format=%cs', sha).strip()} ({sha[:7]})"


def size(count):
    return f"{count / 1e6:.1f} MB"


def download(url, sha, where, count, digest, apk, exefs=()):
    """the NRO, the APK (apk: its bytes and SHA-256, or None for a release without one) and the ExeFS folders (exefs:
    Packed), linked from the release they are in, with their sizes and SHA-256; their checksum files; the documents"""
    rows = [f"| [{NRO}]({where}/{NRO}) | {size(count)} | `{digest}` |"]
    sums = [SUM]
    if apk:
        rows.append(f"| [{APK}]({where}/{APK}) | {size(apk[0])} | `{apk[1]}` |")
        sums.append(APK_SUM)
    for packed in exefs:
        rows.append(f"| [{packed.name}]({where}/{packed.name}) | {size(packed.count)} | `{packed.digest}` |")
        sums.append(packed.sum)
    runs = "The same NRO runs on the console and under an emulator"
    if apk:
        runs += ", the APK on Android and on ScarletNX's Android layer"
    for packed in exefs:
        runs += f"; {packed.name} holds an ExeFS folder, {RUNS[packed.name]}"
    linked = [f"[{name}]({where}/{name})" for name in sums]
    if len(sums) == 1:
        said = f"{runs}. {linked[0]} holds its checksum: `sha256sum -c {SUM}` beside the NRO checks it."
    else:
        listing = ", ".join(linked[:-1]) + f" and {linked[-1]}"
        said = f"{runs}. {listing} hold their checksums: `sha256sum -c {' '.join(sums)}` beside them checks them all."
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
            f"How to run it: [Emulator.md]({url}/blob/{sha}/docs/Emulator.md), "
            f"[Console.md]({url}/blob/{sha}/docs/Console.md) and [Android.md]({url}/blob/{sha}/docs/Android.md)"
            + (
                f", [Ounce.md]({url}/blob/{sha}/docs/Ounce.md) and [Mod.md]({url}/blob/{sha}/docs/Mod.md)"
                if exefs
                else ""
            )
            + f"; how the releases are made: [Releases.md]({url}/blob/{sha}/docs/Releases.md)."
        ),
    ]


def source(tree, apk, exefs=()):
    """Where a build's source is, which the GPL asks for beside the binary, and what it links, folded"""
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
            if apk
            else []
        ),
        *(
            [
                f"- {' and '.join(packed.name for packed in exefs)} were built in the same container as the NRO, and "
                "link libnx (ISC) from it."
            ]
            if exefs
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
    count,
    digest,
    kept,
    since="the previous nightly",
    lead="Built from main at",
    apk=None,
    exefs=(),
):
    """where: the release the files are in; count and digest: the NRO's bytes and SHA-256; kept: what becomes of the
    release; since: what previous was; lead: what the first line calls the build; apk: the APK's bytes and SHA-256;
    exefs: the ExeFS folders, Packed"""
    url = f"{SERVER}/{repo}"
    date = git(checkout, "log", "-1", "--format=%cs", sha).strip()
    lines = [f"{lead} [`{sha[:7]}`]({url}/commit/{sha}), committed {date}." + (f" {kept}" if kept else ""), ""]
    lines += download(url, sha, where, count, digest, apk, exefs)
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
    lines += ["", *source(f"{url}/tree/{sha}", apk, exefs)]
    return "\n".join(lines) + "\n"
