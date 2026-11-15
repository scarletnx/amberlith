#!/bin/sh
# SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# tools/uam-source.sh SRC REPO COMMIT: SRC as a clone of REPO checked out at COMMIT, for tools/uam.sh. A clone already
# there is pointed at REPO and fetched when it lacks COMMIT: one made before the pin moved to scarletnx/uam is the
# toolchain's, devkitPro/uam, which has none of the fork's commits.
set -eu
[ $# -eq 3 ] || { echo "usage: tools/uam-source.sh SRC REPO COMMIT" >&2; exit 2; }
src=$1
repo=$2
commit=$3
[ -d "$src/.git" ] || git clone --quiet "$repo" "$src"
git -C "$src" remote set-url origin "$repo"
git -C "$src" cat-file -e "$commit^{commit}" 2>/dev/null || git -C "$src" fetch --quiet origin
git -C "$src" checkout --quiet --force --detach "$commit"
