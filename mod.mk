# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The probe as a mod of a game (docs/Mod.md): the engine and the nvn family built as one more
# module of a game's program, amberlith-mod/exefs/subsdk9, with the main.npdm that gives it the
# memory card. Its NVN is the driver of the game it is loaded in.
#
#   make -f mod.mk          # the two files
#   make -f mod.mk check    # and what tools/mod-imports.py holds the module to

ifeq ($(strip $(DEVKITPRO)),)
$(error Set DEVKITPRO first, e.g. export DEVKITPRO=/opt/devkitpro)
endif

TARGET   := amberlith-mod
BUILD    := build-mod
EXEFS    := $(TARGET)/exefs

PREFIX   := $(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-
CC       := $(PREFIX)gcc
LIBNX    := $(DEVKITPRO)/libnx
TOOLS    := $(DEVKITPRO)/tools/bin

# The layer's types, names and loader, with no body: the functions are the game's driver's.
NVNLAYER_DRIVER := game
include nvnlayer/layer.mk
ifneq ($(NVNLAYER_API),55)
$(error NVNLAYER_API=$(NVNLAYER_API) is not an API version the mod builds for: 55 (unset, it is 55))
endif

APP_VERSION := $(strip $(shell cat VERSION))
APP_BUILD   := $(strip $(shell git describe --tags --match 'v[0-9]*' --always --dirty 2>/dev/null || echo unknown))
APP_HASH    := $(strip $(shell git describe --always --dirty --abbrev=7 --exclude='*' 2>/dev/null || echo unknown))

ARCH     := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIC
# One thread runs the probe, and its thread pointer is the game's SDK's: AMBERLITH_ONE_THREAD keeps
# the engine off thread-locals, which libnx's reader would look up in libnx's thread data.
CFLAGS   := $(ARCH) -g -O2 -ffunction-sections -fdata-sections -MMD -MP \
            -Wall -Wextra -Werror -Wno-unused-parameter \
            -D__SWITCH__ -DAMBERLITH_ONE_THREAD -I$(LIBNX)/include -Isource -Imod -I$(BUILD) \
            $(NVNLAYER_CFLAGS)
# A shared object with libnx's section layout and none of its start: the game's dynamic linker
# relocates the module, binds its imports and calls its DT_INIT. No packed relocations, which a
# linker of an older SDK does not read. Every import is bound at load (-z now), and a weak one
# stays a dynamic symbol, so the game's linker leaves its address nought when the game has none.
LDFLAGS  := $(ARCH) -g -shared -nostartfiles -Wl,-T,$(LIBNX)/switch.ld -Wl,--gc-sections \
            -Wl,-z,text -Wl,-z,now -Wl,--build-id=sha1 -Wl,--nx-module-name \
            -Wl,-init=AmberlithModInit -Wl,--version-script=mod/exports.map \
            -Wl,-Map,$(BUILD)/$(TARGET).map $(NVNLAYER_LDFLAGS)
# libnx for the kernel's calls alone (tools/mod-imports.py reads the map for it).
LIBS     := -L$(LIBNX)/lib -lnx -lm -Wl,--start-group -lc -lsysbase -lgcc -Wl,--end-group

# The engine (source/portable.txt), the card's device over the game's nn::fs, the nvn suite, the
# fractal zooms' cases, the mod's own start, frontend, platform, screens and zooms' way through
# NVN, and the layer's loader. Less the libc family, which no step here runs: its cases ask the C
# library for sockets and thread-local storage, and those are libnx's start's to give.
CFILES   := $(filter-out source/tests/t_libc.c,$(shell grep -v '^\#' source/portable.txt)) \
            source/sdk_card.c source/tests/t_nvn.c source/fractalzoom.c \
            mod/start.c mod/frontend.c mod/host.c mod/platform.c mod/screen.c mod/fractal_nvn.c \
            $(NVNLAYER_SOURCES)
SFILES   := mod/header.s

# The zooms' programs: the plain program's GLSL, and the NVN way's own (source/shaders-nvn), compiled
# for NVN's layout of the driver's banks (uam --nvn, tools/uam/0003-nvn-layout.patch) by the uam the
# plain program's build makes (Makefile, UAM_FIXED), in its tree, and turned into read-only data.
UAM_FIXED  := build/uam/uam
NVN_DKSH   := $(addprefix $(BUILD)/shaders/,fractal.comp.dksh fractal.frag.dksh fullscreen.vert.dksh \
                fractal_show.frag.dksh)
NVN_ASM    := $(BUILD)/shaders/blobs.s
NVN_HDR    := $(BUILD)/shaders/mod_shaders.h
NVN_OBJ    := $(BUILD)/shaders/blobs.o
CFLAGS     += -I$(BUILD)/shaders

OFILES   := $(patsubst %.c,$(BUILD)/%.o,$(CFILES)) $(patsubst %.s,$(BUILD)/%.o,$(SFILES)) $(NVN_OBJ)
DFILES   := $(patsubst %.c,$(BUILD)/%.d,$(CFILES))

.PHONY: all check clean FORCE

all: $(EXEFS)/subsdk9 $(EXEFS)/main.npdm

$(BUILD)/version.h: FORCE
	@mkdir -p $(dir $@)
	@printf '#define AMBERLITH_VERSION "%s"\n#define AMBERLITH_BUILD "%s"\n#define AMBERLITH_HASH "%s"\n' "$(APP_VERSION)" "$(APP_BUILD)" "$(APP_HASH)" > $@.tmp; \
	if cmp -s $@.tmp $@; then rm -f $@.tmp; else mv $@.tmp $@; fi
FORCE:
$(BUILD)/mod/frontend.o: $(BUILD)/version.h

$(UAM_FIXED): tools/uam.sh $(wildcard tools/uam/*.patch)
	tools/uam.sh $(dir $@)

$(BUILD)/shaders/%.comp.dksh: source/shaders/%.comp.glsl $(UAM_FIXED)
	@mkdir -p $(dir $@)
	$(UAM_FIXED) --nvn -s comp -o $@ $<

$(BUILD)/shaders/%.vert.dksh: source/shaders/%.vert.glsl $(UAM_FIXED)
	@mkdir -p $(dir $@)
	$(UAM_FIXED) --nvn -s vert -o $@ $<

$(BUILD)/shaders/%.frag.dksh: source/shaders-nvn/%.frag.glsl $(UAM_FIXED)
	@mkdir -p $(dir $@)
	$(UAM_FIXED) --nvn -s frag -o $@ $<

$(BUILD)/shaders/%.frag.dksh: source/shaders/%.frag.glsl $(UAM_FIXED)
	@mkdir -p $(dir $@)
	$(UAM_FIXED) --nvn -s frag -o $@ $<

# One grouped target, as the NRO's: one run of bin2s makes both files.
$(NVN_ASM) $(NVN_HDR) &: $(NVN_DKSH)
	@mkdir -p $(dir $(NVN_ASM))
	$(TOOLS)/bin2s -a 16 -H $(NVN_HDR) $^ > $(NVN_ASM)
	@echo >> $(NVN_ASM)	# bin2s ends on a comment with no newline, and the assembler says so

$(NVN_OBJ): $(NVN_ASM)
	$(CC) $(ARCH) -c $< -o $@

$(BUILD)/mod/fractal_nvn.o $(BUILD)/source/tests/t_nvn.o: $(NVN_HDR)

# As the NRO's build (Makefile): the fluid reference rounds a multiply and the add after it apart.
$(BUILD)/source/demo_fluidref.o: CFLAGS += -ffp-contract=off

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(ARCH) -c $< -o $@

$(TARGET).elf: $(OFILES) mod/exports.map
	$(CC) $(LDFLAGS) $(OFILES) $(LIBS) -o $@

$(EXEFS)/subsdk9: $(TARGET).elf
	@mkdir -p $(dir $@)
	$(TOOLS)/elf2nso $< $@

# One descriptor for every game: the loader that reads it from the card writes the launched
# program's id into it.
$(EXEFS)/main.npdm: mod/main.json
	@mkdir -p $(dir $@)
	$(TOOLS)/npdmtool $< $@

check: all
	python3 tools/mod-imports.py $(TARGET).elf --map $(BUILD)/$(TARGET).map --imports mod/imports.txt

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET)

-include $(DFILES)
