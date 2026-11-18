#!/bin/sh
# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# tools/uam.sh DIR: scarletnx/uam at the commit below, built into DIR/uam. The toolchain's uam 1.1.0 puts a multisample
# fetch's sample in the register the console reads the handle from, and reads a uniform block's vector indexed by a
# component worked out at run time from the vector's first word; the fork fixes both, and its --nvn lays a program's
# buffers out as NVN's driver keeps them, for the mod's zooms (mod.mk). The shaders that need any of it are built by
# this one (Makefile and mod.mk, UAM_FIXED).
#
# The first run clones the source (tools/uam-source.sh, which also moves an older clone to this commit), which needs the
# network, and it needs git, meson, ninja, bison, flex, Python 3 with mako, and a C and a C++ compiler for this machine.
# A run that finds DIR/uam built from this commit and this script only marks it current, so make does not ask again.
set -eu
[ $# -eq 1 ] || { echo "usage: tools/uam.sh DIR" >&2; exit 2; }
dir=$1
repo=https://github.com/scarletnx/uam.git
commit=019b6939c8961dd6a72aeee0deb8efefac787fc2
here=$(cd "$(dirname "$0")" && pwd)
stamp="$commit $(sha256sum < "$here/uam.sh" | cut -c1-16)"
if [ -x "$dir/uam" ] && [ "$(cat "$dir/uam.stamp" 2>/dev/null)" = "$stamp" ]; then
    touch "$dir/uam"
    exit 0
fi
mkdir -p "$dir"
src=$dir/src
sh "$here/uam-source.sh" "$src" "$repo" "$commit"
build=$dir/build-$(echo "$stamp" | cut -d' ' -f2)
[ -d "$build" ] || meson setup "$build" "$src" --buildtype=release
ninja -C "$build"
cp "$build/uam" "$dir/uam"
echo "$stamp" > "$dir/uam.stamp"
