# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds the parts that do not touch the console for the machine you are sitting at, so what
# decides pass and fail can be exercised with no emulator and no device.

HOSTCC   ?= cc
HOSTOUT  := build-host/amberlith-host
HOSTSRC  := source/platform_host.c $(shell grep -v '^\#' source/portable.txt) source/host_main.c

# One object per source, so the -MMD below has somewhere to put its dependency files. Without
# them an edited header rebuilds nothing and the run reports the previous binary's answer.
# AMBERLITH_HOST: the few judge cases that hold this build to what host_main.c sets up
HOSTFLAGS := -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -O1 -g -MMD -MP -Isource -DAMBERLITH_HOST
HOSTOBJ  := $(patsubst source/%.c,build-host/%.o,$(HOSTSRC))
HOSTDEP  := $(HOSTOBJ:.o=.d)

# The NVN layer's own test (nvnlayer/tests), once for each API version the layer builds for: its
# shared part, the API 231 build's bodies and the API 55 build's control sections, none of which
# touches the console.
LAYERTEST := build-host/nvnlayer-test-55 build-host/nvnlayer-test-231 \
             build-host/nvnlayer-test-game-55 build-host/nvnlayer-test-game-231
LAYERFLAGS := -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -O1 -g -Invnlayer/include

# The host build again with the processor reference's 128-bit integers in two 64-bit words, as a
# machine with no __int128 builds it (the Vita's ARM32): its reference suite gives the same answers.
HOSTWORDS := build-host/amberlith-host-words

# What the mod reads of the SDK module of the game it is loaded in (mod/host.c), on bytes the
# test makes.
MODTEST := build-host/mod-host-test

.PHONY: host host-run host-clean music-wav

# Where the fluid demo's budgets come from, measured on the processor (tools/fluid-envelope.c).
FLUIDTOOL := build-host/fluid-envelope

host: $(HOSTOUT) $(HOSTWORDS) $(FLUIDTOOL) $(LAYERTEST) $(MODTEST)

build-host/nvnlayer-test-55: nvnlayer/tests/layer_test.c nvnlayer/source/layer.c \
                             nvnlayer/source/api55_shader.c $(wildcard nvnlayer/source/*.h) \
                             $(wildcard nvnlayer/include/*.h nvnlayer/include/nvn/*.h)
	@mkdir -p $(dir $@)
	$(HOSTCC) $(LAYERFLAGS) -DNVN_LAYER_API=55 $(filter %.c,$^) -o $@

build-host/nvnlayer-test-231: nvnlayer/tests/layer_test.c nvnlayer/source/layer.c \
                              nvnlayer/source/api231_surface.c $(wildcard nvnlayer/source/*.h) \
                              $(wildcard nvnlayer/include/*.h nvnlayer/include/nvn/*.h)
	@mkdir -p $(dir $@)
	$(HOSTCC) $(LAYERFLAGS) -DNVN_LAYER_API=231 $(filter %.c,$^) -o $@

# And the build for a game's own driver (NVNLAYER_DRIVER := game), which is the shared part with
# no lookup of its own and the bodies of the names a game's driver lacks: the test's
# nvnBootstrapLoader stands for the game's.
build-host/nvnlayer-test-game-%: nvnlayer/tests/game_test.c nvnlayer/source/layer.c \
                                 nvnlayer/source/game_driver.c $(wildcard nvnlayer/source/*.h) \
                                 $(wildcard nvnlayer/include/*.h nvnlayer/include/nvn/*.h)
	@mkdir -p $(dir $@)
	$(HOSTCC) $(LAYERFLAGS) -DNVN_LAYER_API=$* -DNVN_LAYER_GAME_DRIVER $(filter %.c,$^) -o $@

build-host/mod-host-test: mod/tests/host_test.c mod/host.c mod/host.h
	@mkdir -p $(dir $@)
	$(HOSTCC) $(LAYERFLAGS) $(filter %.c,$^) -o $@

build-host/%.o: source/%.c
	@mkdir -p $(dir $@)
	$(HOSTCC) $(HOSTFLAGS) -c $< -o $@

$(HOSTOUT): $(HOSTOBJ)
	$(HOSTCC) $(HOSTFLAGS) $(HOSTOBJ) -lm -pthread -o $@

build-host/cpuref-words.o: source/cpuref.c
	@mkdir -p $(dir $@)
	$(HOSTCC) $(HOSTFLAGS) -DCPUREF_NO_INT128 -c $< -o $@

$(HOSTWORDS): $(filter-out build-host/cpuref.o,$(HOSTOBJ)) build-host/cpuref-words.o
	$(HOSTCC) $(HOSTFLAGS) $^ -lm -pthread -o $@

$(FLUIDTOOL): tools/fluid-envelope.c build-host/demo_fluidref.o
	$(HOSTCC) $(HOSTFLAGS) $^ -lm -o $@

# Twice, the second from somewhere else on purpose: a path built relative to the working
# directory works from here and nowhere else, and the suite has to be able to notice that. Then
# the NVN layer's test in both of its API versions, the report tools, which read what the binary
# wrote and have tests of their own, the CI's
# scripts in .github/scripts, against a fake GitHub and a fake Discord, and the APK's runners,
# against a fake adb and a fake ScarletNX driver. The fluid demo's budgets are measured between
# them, on the early frames and a default run's 600.
# a TMPDIR as long as an app's cache folder on a desktop (Android's layer of ScarletNX), where the
# support suite's scratch corner goes past 100 characters
LONGTMP := /tmp/amberlith-a-tmpdir-as-long-as-an-app-cache-folder-on-a-desktop-runner

host-run: $(HOSTOUT) $(HOSTWORDS) $(FLUIDTOOL) $(LAYERTEST) $(MODTEST)
	./$(HOSTOUT)
	cd / && exec $(CURDIR)/$(HOSTOUT)
	mkdir -p $(LONGTMP) && TMPDIR=$(LONGTMP) ./$(HOSTOUT) support
	./$(HOSTWORDS) reference
	./build-host/nvnlayer-test-55
	./build-host/nvnlayer-test-231
	./build-host/nvnlayer-test-game-55
	./build-host/nvnlayer-test-game-231
	./$(MODTEST)
	./$(FLUIDTOOL) 64 600
	PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tools
	PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s .github/scripts
	bash android/tests/test-run-adb.sh
	bash android/tests/test-run-scarletnx.sh

# The demos' tracks as WAVs to listen to (tools/music-wav.c): the first pass of each track the fixed
# seed composes, and of each seed in MUSIC_SEEDS, and a switch across the silence barrier, the
# samples the music family holds a machine to.
MUSIC_WAV ?= build-host/music
MUSIC_SEEDS ?=
MUSICFLAGS := -std=c11 -Wall -Wextra -Werror -O2 -Isource

music-wav: build-host/music-wav
	@mkdir -p $(MUSIC_WAV)
	./build-host/music-wav $(MUSIC_WAV) $(MUSIC_SEEDS)

build-host/music-wav: tools/music-wav.c source/music.c source/music.h source/musiccompose.c \
		source/musiccompose.h source/musicscore.c source/musicscore.h
	@mkdir -p build-host
	$(HOSTCC) $(MUSICFLAGS) tools/music-wav.c source/music.c source/musiccompose.c \
		source/musicscore.c -o $@

host-clean:
	rm -rf build-host

# Last, and with a leading dash: on the first build there are no .d files yet and that is not an
# error. They live under build-host, so host-clean already takes them with it.
-include $(HOSTDEP)
