#!/bin/sh
# SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# tools/uam.sh DIR: uam 1.1.0 with the fixes of scarletnx/uam (the commit below) and tools/uam/*.patch on it, built
# into DIR/uam. The toolchain's uam puts a multisample fetch's sample in the register the console reads the handle from
# (tools/uam/0001-*.patch), and reads a uniform block's vector indexed by a component worked out at run time from the
# vector's first word (the fork's commits); the shaders that do either are built by this one (Makefile, UAM_FIXED).
#
# The first run clones the source (tools/uam-source.sh, which also moves an older clone to this commit), which needs the
# network, and it needs git, meson, ninja, bison, flex, Python 3 with mako, and a C and a C++ compiler for this machine.
# A run that finds DIR/uam built from this commit, these patches and this script only marks it current, so make does not
# ask again.
set -eu
[ $# -eq 1 ] || { echo "usage: tools/uam.sh DIR" >&2; exit 2; }
dir=$1
repo=https://github.com/scarletnx/uam.git
commit=e56cb016ca2a39fc055b851deea7b688b64dbb37
here=$(cd "$(dirname "$0")" && pwd)
stamp="$commit $(cat "$here/uam.sh" "$here"/uam/*.patch | sha256sum | cut -c1-16)"
if [ -x "$dir/uam" ] && [ "$(cat "$dir/uam.stamp" 2>/dev/null)" = "$stamp" ]; then
    touch "$dir/uam"
    exit 0
fi
mkdir -p "$dir"
src=$dir/src
sh "$here/uam-source.sh" "$src" "$repo" "$commit"
for patch in "$here"/uam/*.patch; do
    git -C "$src" apply "$patch"
done
build=$dir/build-$(echo "$stamp" | cut -d' ' -f2)
[ -d "$build" ] || meson setup "$build" "$src" --buildtype=release
ninja -C "$build"
cp "$build/uam" "$dir/uam"
echo "$stamp" > "$dir/uam.stamp"
