# SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The probe as a Switch 2 program (docs/Ounce.md): the engine and the cases a program of that
# console can run, built into an ExeFS folder, edenprobe-ounce/exefs, of main (an NSO) and
# main.npdm, which is what a runtime recompiles and runs. No console starts it.
#
#   make -f ounce.mk

ifeq ($(strip $(DEVKITPRO)),)
$(error Set DEVKITPRO first, e.g. export DEVKITPRO=/opt/devkitpro)
endif

TARGET   := edenprobe-ounce
BUILD    := build-ounce
EXEFS    := $(TARGET)/exefs

PREFIX   := $(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-
CC       := $(PREFIX)gcc
LIBNX    := $(DEVKITPRO)/libnx
TOOLS    := $(DEVKITPRO)/tools/bin

# The layer in its other version: the names ScarletNX's API 231 NVN takes, and nothing under them.
NVNLAYER_API := 231
include nvnlayer/layer.mk

APP_VERSION := $(strip $(shell cat VERSION))
APP_BUILD   := $(strip $(shell git describe --tags --match 'v[0-9]*' --always --dirty 2>/dev/null || echo unknown))
APP_HASH    := $(strip $(shell git describe --always --dirty --abbrev=7 --exclude='*' 2>/dev/null || echo unknown))

# The Switch's processor flags: a Switch 2 program's code is the same instruction set, and what
# its processor adds is the runtime's to give, not this build's to ask for.
ARCH     := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS   := $(ARCH) -g -O2 -ffunction-sections -fdata-sections -MMD -MP \
            -Wall -Wextra -Werror -Wno-unused-parameter \
            -D__SWITCH__ -I$(LIBNX)/include -Isource -I$(BUILD) -I$(BUILD)/shaders $(NVNLAYER_CFLAGS)
# As the NRO's link, less deko3d's abort wrap: nothing here makes a deko3d object.
LDFLAGS  := $(ARCH) -specs=$(LIBNX)/switch.specs -Wl,--gc-sections -g $(NVNLAYER_LDFLAGS) \
            -Wl,--export-dynamic-symbol='_ZN2nn*' -Wl,--export-dynamic-symbol='_ZNK2nn*' \
            -Wl,--undefined=edenprobe_sdk_label
LIBS     := -L$(LIBNX)/lib -lnx -lm

# The engine (source/portable.txt), the console's side of platform.h, the start through the SDK's
# functions (a runtime that answers them itself refuses a program that opens the services), the
# nvn suite, this frontend and the layer.
CFILES   := $(shell grep -v '^\#' source/portable.txt) source/platform_switch.c source/hooks.c \
            source/sdk_card.c source/sdk_fs.c source/sdk_time.c source/sdk_other.c \
            source/sdk_audio.c \
            source/tests/t_nvn.c ounce/frontend.c ounce/t_nvn2.c $(NVNLAYER_SOURCES)

# The shader programs, of the Switch 2's own code: each source assembled into its code and its
# control section (docs/Sm86.md), and bin2s turns the two into read-only data.
SHADER_SRC  := $(wildcard ounce/shaders/*.sm86)
SHADER_BIN  := $(patsubst ounce/shaders/%.sm86,$(BUILD)/shaders/%.bin,$(SHADER_SRC))
SHADER_CTL  := $(SHADER_BIN:.bin=.ctl)
SHADER_ASM  := $(BUILD)/shaders/blobs.s
SHADER_HDR  := $(BUILD)/shaders/ounce_shaders.h
SHADER_OBJ  := $(BUILD)/shaders/blobs.o

OFILES   := $(patsubst %.c,$(BUILD)/%.o,$(CFILES)) $(SHADER_OBJ)
DFILES   := $(patsubst %.c,$(BUILD)/%.d,$(CFILES))

.PHONY: all clean FORCE

all: $(EXEFS)/main $(EXEFS)/main.npdm

$(BUILD)/version.h: FORCE
	@mkdir -p $(dir $@)
	@printf '#define EDENPROBE_VERSION "%s"\n#define EDENPROBE_BUILD "%s"\n#define EDENPROBE_HASH "%s"\n' "$(APP_VERSION)" "$(APP_BUILD)" "$(APP_HASH)" > $@.tmp; \
	if cmp -s $@.tmp $@; then rm -f $@.tmp; else mv $@.tmp $@; fi
FORCE:
$(BUILD)/ounce/frontend.o: $(BUILD)/version.h

# One run of the assembler writes both files of a program.
$(BUILD)/shaders/%.bin $(BUILD)/shaders/%.ctl &: ounce/shaders/%.sm86 tools/sm86asm.py tools/sm86/sm86.decode
	@mkdir -p $(dir $@)
	python3 tools/sm86asm.py $< --code $(BUILD)/shaders/$*.bin --control $(BUILD)/shaders/$*.ctl

# And one run of bin2s both of these.
$(SHADER_ASM) $(SHADER_HDR) &: $(SHADER_BIN) $(SHADER_CTL)
	$(TOOLS)/bin2s -a 16 -H $(SHADER_HDR) $^ > $(SHADER_ASM)
	@echo >> $(SHADER_ASM)	# bin2s ends on a comment with no newline, and the assembler says so

$(SHADER_OBJ): $(SHADER_ASM)
	$(CC) $(ARCH) -c $< -o $@

$(BUILD)/ounce/t_nvn2.o: $(SHADER_HDR)

# As the NRO's build (Makefile): the fluid reference rounds a multiply and the add after it apart.
$(BUILD)/source/demo_fluidref.o: CFLAGS += -ffp-contract=off

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OFILES)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

$(EXEFS)/main: $(TARGET).elf
	@mkdir -p $(dir $@)
	$(TOOLS)/elf2nso $< $@

# The descriptor of a Switch 2 program, which the toolchain's npdmtool does not write.
$(EXEFS)/main.npdm: tools/npdm-ounce.py
	@mkdir -p $(dir $@)
	python3 tools/npdm-ounce.py write $@

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET)

-include $(DFILES)
