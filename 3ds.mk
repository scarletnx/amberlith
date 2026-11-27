# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The 3DS's frontend (docs/Frontends.md, docs/3DS.md): the engine and the libc family's cases, as
# every frontend compiles them (source/portable.txt), with 3ds/'s own, into amberlith.3dsx, in C23.
#   make -f 3ds.mk             with DEVKITPRO set, as devkitPro's installer and its container set it

ifeq ($(strip $(DEVKITPRO)),)
$(error Set DEVKITPRO first, e.g. export DEVKITPRO=/opt/devkitpro)
endif

BUILD     := build-3ds
TARGET    := $(BUILD)/amberlith.3dsx
DEVKITARM ?= $(DEVKITPRO)/devkitARM
CC        := $(DEVKITARM)/bin/arm-none-eabi-gcc
CTRULIB   := $(DEVKITPRO)/libctru
TOOLS     := $(DEVKITPRO)/tools/bin
# The name, the line under it and the author the Homebrew Launcher shows
APP_TITLE  := Amberlith
APP_DESC   := A test program for the console and its emulators
APP_AUTHOR := Amberlith contributors

# The version and the build as the console's Makefile names them (docs/Versions.md)
APP_VERSION := $(strip $(shell cat VERSION))
APP_BUILD   := $(strip $(shell git describe --tags --match 'v[0-9]*' --always --dirty 2>/dev/null || echo unknown))
APP_HASH    := $(strip $(shell git describe --always --dirty --abbrev=7 --exclude='*' 2>/dev/null || echo unknown))

# The 3DS's ARM11 MPCore, with its VFPv2 and the thread pointer read through the kernel
ARCH    := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
SOURCES := $(shell grep -v '^\#' source/portable.txt) 3ds/platform_3ds.c 3ds/frontend.c
OBJECTS := $(patsubst %.c,$(BUILD)/%.o,$(SOURCES))
# libctru's headers as the system's: C23 holds this program's code, not theirs
CFLAGS  := -std=c23 -O2 -g -Wall -Wextra -Werror -Wno-unused-parameter -MMD -MP $(ARCH) -D__3DS__ \
           -Isource -I3ds -isystem $(CTRULIB)/include \
           -DAMBERLITH_VERSION='"$(APP_VERSION)"' -DAMBERLITH_BUILD='"$(APP_BUILD)"' \
           -DAMBERLITH_HASH='"$(APP_HASH)"'
LDFLAGS := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(BUILD)/amberlith.map -L$(CTRULIB)/lib
LIBS    := -lctru -lm

.PHONY: all clean
all: $(TARGET)

# An object again when this file changes its flags
$(BUILD)/%.o: %.c 3ds.mk
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/amberlith.elf: $(OBJECTS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD)/amberlith.smdh: 3ds/icon.png VERSION
	$(TOOLS)/smdhtool --create "$(APP_TITLE)" "$(APP_DESC) ($(APP_VERSION))" "$(APP_AUTHOR)" $< $@

$(TARGET): $(BUILD)/amberlith.elf $(BUILD)/amberlith.smdh
	$(TOOLS)/3dsxtool $< $@ --smdh=$(BUILD)/amberlith.smdh

clean:
	rm -rf $(BUILD)

-include $(OBJECTS:.o=.d)
