#!/bin/sh
# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# tools/deko3d.sh DIR: deko3d from scarletnx/deko3d (the commit below) built into DIR/lib/libdeko3d.a, its headers in
# DIR/include, which the NRO's build reads and links before the toolchain's deko3d 0.5.0 (Makefile, DEKO3D). The fork
# has the blend's alpha destination factor, the image copies, the command and code memory given back and the zcull
# invalidation right where 0.5.0 has them wrong, and the calls the NVN layer and the deko3d way draw with where 0.5.0
# has none: the render enable, a texture drawn with no program and transform feedback.
#
# deko3d's build turns its method and macro files into headers with dekodef and dekomme (fincs/dekotools, the commit
# below, GPL-3.0), which run on this machine and leave nothing of themselves in the library; they are built here
# first. The first run clones both (tools/uam-source.sh, which also moves an older clone to its commit), which needs
# the network, and it needs git, meson, ninja, bison, flex, a C++ compiler for this machine and DEVKITPRO's
# devkitA64. A run that finds DIR built from this commit and this script, which names both commits, only renews
# DIR/deko3d.stamp, so make does not ask again; a new build copies the library and the headers over the old only
# where they differ, so an object rebuilds only when the header it read changed.
set -eu
[ $# -eq 1 ] || { echo "usage: tools/deko3d.sh DIR" >&2; exit 2; }
[ -n "${DEVKITPRO:-}" ] ||
    { echo "tools/deko3d.sh: DEVKITPRO is not set, and deko3d builds with its devkitA64" >&2; exit 2; }
# Absolute: deko3d's make runs from its own directory and finds the tools by a PATH under DIR.
dir=$(mkdir -p "$1" && cd "$1" && pwd)
repo=https://github.com/scarletnx/deko3d.git
commit=d0e324354d27dafaf699e73d102acc5f31fe35c5
tools_repo=https://github.com/fincs/dekotools.git
tools_commit=c15523cea963c04fad77a46f23c3a96d26a86ab9
here=$(cd "$(dirname "$0")" && pwd)
stamp="$commit $(sha256sum < "$here/deko3d.sh" | cut -c1-16)"
if [ -f "$dir/lib/libdeko3d.a" ] && [ -f "$dir/include/deko3d.h" ] &&
    [ "$(cat "$dir/deko3d.stamp" 2>/dev/null)" = "$stamp" ]; then
    touch "$dir/deko3d.stamp"
    exit 0
fi

# A copy that leaves FILE as it was when it already holds what SOURCE does.
copy_changed() {
    cmp -s "$1" "$2" 2>/dev/null || cp "$1" "$2"
}

mkdir -p "$dir/lib" "$dir/include"
sh "$here/uam-source.sh" "$dir/dekotools-src" "$tools_repo" "$tools_commit"
tools_build=$dir/dekotools-$(echo "$tools_commit" | cut -c1-12)
[ -d "$tools_build" ] || meson setup "$tools_build" "$dir/dekotools-src" --buildtype=release
ninja -C "$tools_build"

sh "$here/uam-source.sh" "$dir/src" "$repo" "$commit"
# The library alone, not deko3d's debug build beside it, by a make of its own: a make running this script passes its
# command line's variables on, and the probe's CFLAGS or CC would build deko3d with the probe's flags.
env -u MAKEFLAGS -u MFLAGS -u MAKEOVERRIDES -u MAKELEVEL PATH="$tools_build:$PATH" \
    make -C "$dir/src" -j"$(getconf _NPROCESSORS_ONLN)" lib/libdeko3d.a
copy_changed "$dir/src/lib/libdeko3d.a" "$dir/lib/libdeko3d.a"
for header in "$dir"/src/include/*; do
    copy_changed "$header" "$dir/include/$(basename "$header")"
done
echo "$stamp" > "$dir/deko3d.stamp"
