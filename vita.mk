# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The Vita's frontend (docs/Frontends.md, docs/Vita.md): the engine and the libc family's cases, as
# every frontend compiles them (source/portable.txt), with vita/'s own, into amberlith.vpk, in C23.
#   make -f vita.mk            with VITASDK set, as the SDK's installer sets it

ifeq ($(strip $(VITASDK)),)
$(error Set VITASDK first, e.g. export VITASDK=$$HOME/vitasdk)
endif

BUILD     := build-vita
VPK       := $(BUILD)/amberlith.vpk
PREFIX    := $(VITASDK)/bin/arm-vita-eabi-
CC        := $(PREFIX)gcc
# The title it installs as, and its name on the home screen
TITLE_ID  := AMBR00001
APP_TITLE := Amberlith

# The version and the build as the console's Makefile names them (docs/Versions.md)
APP_VERSION := $(strip $(shell cat VERSION))
APP_BUILD   := $(strip $(shell git describe --tags --match 'v[0-9]*' --always --dirty 2>/dev/null || echo unknown))
APP_HASH    := $(strip $(shell git describe --always --dirty --abbrev=7 --exclude='*' 2>/dev/null || echo unknown))

# vita2d's clear and texture programs as its author compiled them, fetched at a fixed commit and
# held to vita/shaders.sha256: the SDK has no shader compiler (docs/Credits.md)
VITA2D     := a8f15ab09d5233f0a4e4ad0e8f6ade0da888cbed
VITA2D_URL := https://raw.githubusercontent.com/xerpi/libvita2d/$(VITA2D)/libvita2d/shader/compiled
SHADERS    := clear_v clear_f texture_v texture_f
SHADER_OBJ := $(SHADERS:%=$(BUILD)/shaders/%_gxp.o)

SOURCES := $(shell grep -v '^\#' source/portable.txt) vita/platform_vita.c vita/frontend.c vita/gxm.c vita/printf.c
OBJECTS := $(patsubst %.c,$(BUILD)/%.o,$(SOURCES))
CFLAGS  := -std=c23 -O2 -g -Wall -Wextra -Werror -Wno-unused-parameter -MMD -MP -Isource -Ivita \
           -DAMBERLITH_VERSION='"$(APP_VERSION)"' -DAMBERLITH_BUILD='"$(APP_BUILD)"' \
           -DAMBERLITH_HASH='"$(APP_HASH)"'
LIBS    := -lSceGxm_stub -lSceDisplay_stub -lSceSysmodule_stub -lSceLibKernel_stub -lSceKernelThreadMgr_stub \
           -lSceIofilemgr_stub -lSceRtc_stub -lpthread -lm

.PHONY: all clean
all: $(VPK)

$(BUILD)/shaders/%_gxp.o: vita/shaders.sha256
	@mkdir -p $(dir $@)
	curl -sSfL -o $@.part $(VITA2D_URL)/$*_gxp.o
	@grep -qx "$$(sha256sum $@.part | cut -c1-64)  $*_gxp.o" vita/shaders.sha256 || \
		{ echo "vita.mk: $*_gxp.o is not the one vita/shaders.sha256 holds" >&2; exit 1; }
	mv $@.part $@

# An object again when this file changes its flags
$(BUILD)/%.o: %.c vita.mk
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# The printf family through vita/printf.c, which gives newlib the C99 size formats it lacks
WRAPS   := -Wl,--wrap=snprintf,--wrap=vsnprintf,--wrap=fprintf,--wrap=vfprintf,--wrap=printf

$(BUILD)/amberlith.elf: $(OBJECTS) $(SHADER_OBJ)
	$(CC) -Wl,-q $(WRAPS) $^ $(LIBS) -o $@

$(BUILD)/eboot.bin: $(BUILD)/amberlith.elf
	$(VITASDK)/bin/vita-elf-create $< $(BUILD)/amberlith.velf
	$(VITASDK)/bin/vita-make-fself -s $(BUILD)/amberlith.velf $@

$(BUILD)/param.sfo: VERSION
	$(VITASDK)/bin/vita-mksfoex -s TITLE_ID=$(TITLE_ID) -s APP_VER=01.00 "$(APP_TITLE)" $@

$(VPK): $(BUILD)/eboot.bin $(BUILD)/param.sfo
	$(VITASDK)/bin/vita-pack-vpk -s $(BUILD)/param.sfo -b $(BUILD)/eboot.bin $@

clean:
	rm -rf $(BUILD)

-include $(OBJECTS:.o=.d)
