# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The GBA's frontend (docs/Frontends.md, docs/GBA.md): the report alone, with the judge's selfcheck, the
# libc family's references and the libc suite, as every frontend compiles them (source/portable.txt),
# with gba/'s own, into amberlith.gba, in C23 with GNU's asm (libgba's headers use it).
#   make -f gba.mk             with DEVKITPRO set, as devkitPro's installer and its container set it

ifeq ($(strip $(DEVKITPRO)),)
$(error Set DEVKITPRO first, e.g. export DEVKITPRO=/opt/devkitpro)
endif

BUILD     := build-gba
TARGET    := $(BUILD)/amberlith.gba
DEVKITARM ?= $(DEVKITPRO)/devkitARM
CC        := $(DEVKITARM)/bin/arm-none-eabi-gcc
OBJCOPY   := $(DEVKITARM)/bin/arm-none-eabi-objcopy
OBJDUMP   := $(DEVKITARM)/bin/arm-none-eabi-objdump
NM        := $(DEVKITARM)/bin/arm-none-eabi-nm
LIBGBA    := $(DEVKITPRO)/libgba
TOOLS     := $(DEVKITPRO)/tools/bin

# The version and the build as the console's Makefile names them (docs/Versions.md)
APP_VERSION := $(strip $(shell cat VERSION))
APP_BUILD   := $(strip $(shell git describe --tags --match 'v[0-9]*' --always --dirty 2>/dev/null || echo unknown))
APP_HASH    := $(strip $(shell git describe --always --dirty --abbrev=7 --exclude='*' 2>/dev/null || echo unknown))

# The GBA's ARM7TDMI, in Thumb: the code runs from the cartridge's ROM
ARCH    := -mcpu=arm7tdmi -mtune=arm7tdmi -mthumb -mthumb-interwork
SOURCES := $(shell grep -v '^\#' source/portable.txt) gba/platform_gba.c gba/frontend.c gba/video.c
OBJECTS := $(patsubst %.c,$(BUILD)/%.o,$(SOURCES))
# libgba's headers as the system's: C23 holds this program's code, not theirs. Each function and
# object in a section of its own, so the link keeps only what the GBA's steps reach. The baseline
# keeps 16 timed cases, 8 KiB, where the others keep 2048 in 1 MiB: the GBA writes no baseline file,
# and its steps time nothing.
CFLAGS  := -std=gnu23 -O2 -g -Wall -Wextra -Werror -Wno-unused-parameter -MMD -MP $(ARCH) \
           -ffunction-sections -fdata-sections -DBASELINE_CASES_MAX=16u \
           -Isource -Igba -isystem $(LIBGBA)/include \
           -DAMBERLITH_VERSION='"$(APP_VERSION)"' -DAMBERLITH_BUILD='"$(APP_BUILD)"' \
           -DAMBERLITH_HASH='"$(APP_HASH)"'
# GBA_SAVE_TYPE, the string an emulator finds the cartridge's SRAM by, kept though no code reads it
LDFLAGS := -specs=gba.specs -g $(ARCH) -Wl,-Map,$(BUILD)/amberlith.map -Wl,--require-defined=GBA_SAVE_TYPE \
           -L$(LIBGBA)/lib
LIBS    := -lgba -lm

.PHONY: all clean
all: $(TARGET)
# A recipe that fails leaves no target behind: an ELF past the GBA's memory is not kept
.DELETE_ON_ERROR:

# An object again when this file changes its flags. Its zeroed data goes to the 256 KiB of EWRAM,
# not the 32 KiB of IWRAM, which holds the stack: devkitARM's cartridge layout puts .bss in IWRAM
# and .sbss in EWRAM, so each .bss section is renamed .sbss.
$(BUILD)/%.o: %.c gba.mk
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
	$(OBJCOPY) $$($(OBJDUMP) -h $@ | awk '$$2 ~ /^\.bss/ { printf " --rename-section %s=.s%s", $$2, substr($$2, 2) }') $@

# devkitARM's cartridge layout checks neither EWRAM's zeroed data nor IWRAM's data against their
# ends: the link is held to leave the heap at least FITS bytes of EWRAM, and the stack as many of
# IWRAM below its top
FITS := 16384
$(BUILD)/amberlith.elf: $(OBJECTS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@
	@$(NM) $@ > $(BUILD)/amberlith.symbols; \
	at() { echo $$((0x$$(awk -v s=$$1 '$$3 == s { print $$1 }' $(BUILD)/amberlith.symbols))); }; \
	heap=$$(( $$(at __eheap_end) - $$(at __eheap_start) )); \
	stack=$$(( $$(at __sp_usr) - $$(at __iheap_start) )); \
	echo "gba.mk: EWRAM leaves the heap $$heap bytes, IWRAM the stack $$stack, of the $(FITS) each needs"; \
	[ $$heap -ge $(FITS) ] && [ $$stack -ge $(FITS) ] || \
		{ echo "gba.mk: the heap's $$heap bytes or the stack's $$stack are under $(FITS)" >&2; exit 1; }

# The header names the program (AMBERLITH, code AMBR), which the release checks it by
$(TARGET): $(BUILD)/amberlith.elf
	$(OBJCOPY) -O binary $< $@
	$(TOOLS)/gbafix $@ -tAMBERLITH -cAMBR -m00

clean:
	rm -rf $(BUILD)

-include $(OBJECTS:.o=.d)
