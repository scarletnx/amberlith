# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The DS's frontend (docs/Frontends.md, docs/DS.md): the engine and the libc family's cases, as
# every frontend compiles them (source/portable.txt), with nds/'s own, into amberlith.nds, in C23
# with GNU's asm (libnds's headers use it), for the ARM9; calico's own ARM7 program beside it.
#   make -f nds.mk             with DEVKITPRO set, as devkitPro's installer and its container set it

ifeq ($(strip $(DEVKITPRO)),)
$(error Set DEVKITPRO first, e.g. export DEVKITPRO=/opt/devkitpro)
endif

BUILD     := build-nds
TARGET    := $(BUILD)/amberlith.nds
DEVKITARM ?= $(DEVKITPRO)/devkitARM
CC        := $(DEVKITARM)/bin/arm-none-eabi-gcc
CALICO    := $(DEVKITPRO)/calico
LIBNDS    := $(DEVKITPRO)/libnds
TOOLS     := $(DEVKITPRO)/tools/bin
# The three lines a DS menu shows under the icon
APP_TITLE  := Amberlith
APP_DESC   := A test program for the console
APP_AUTHOR := Amberlith contributors

# The version and the build as the console's Makefile names them (docs/Versions.md)
APP_VERSION := $(strip $(shell cat VERSION))
APP_BUILD   := $(strip $(shell git describe --tags --match 'v[0-9]*' --always --dirty 2>/dev/null || echo unknown))
APP_HASH    := $(strip $(shell git describe --always --dirty --abbrev=7 --exclude='*' 2>/dev/null || echo unknown))

# The DS's ARM946E-S, in Thumb: the program and its data share 4 MiB of main memory
ARCH    := -march=armv5te -mtune=arm946e-s -mthumb
SOURCES := $(shell grep -v '^\#' source/portable.txt) nds/platform_nds.c nds/frontend.c
OBJECTS := $(patsubst %.c,$(BUILD)/%.o,$(SOURCES))
# libnds's and calico's headers as the system's: C23 holds this program's code, not theirs. Each
# function and object in a section of its own, so the link keeps only what the DS's steps reach.
CFLAGS  := -std=gnu23 -O2 -g -Wall -Wextra -Werror -Wno-unused-parameter -MMD -MP $(ARCH) \
           -ffunction-sections -fdata-sections -DARM9 -D__NDS__ \
           -Isource -Inds -isystem $(LIBNDS)/include -isystem $(CALICO)/include \
           -DAMBERLITH_VERSION='"$(APP_VERSION)"' -DAMBERLITH_BUILD='"$(APP_BUILD)"' \
           -DAMBERLITH_HASH='"$(APP_HASH)"'
LDFLAGS := -specs=$(CALICO)/share/ds9.specs -g $(ARCH) -Wl,-Map,$(BUILD)/amberlith.map \
           -L$(LIBNDS)/lib -L$(CALICO)/lib
LIBS    := -lfat -lnds9 -lcalico_ds9 -lm

.PHONY: all clean
all: $(TARGET)

# An object again when this file changes its flags
$(BUILD)/%.o: %.c nds.mk
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/amberlith.elf: $(OBJECTS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

# The header's title names the program, AMBERLITH, which the release checks it by; its code is
# homebrew's, ####, by which an emulator gives a program whose ARM9 starts at 0x4000 its DLDI card;
# the banner gives a menu its icon and three lines
$(TARGET): $(BUILD)/amberlith.elf nds/icon.bmp VERSION
	$(TOOLS)/ndstool -c $@ -9 $< -7 $(CALICO)/bin/ds7_maine.elf -g '####' 00 AMBERLITH \
		-b nds/icon.bmp "$(APP_TITLE);$(APP_DESC) ($(APP_VERSION));$(APP_AUTHOR)"

clean:
	rm -rf $(BUILD)

-include $(OBJECTS:.o=.d)
