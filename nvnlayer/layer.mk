# SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The NVN layer for a program's makefile (docs/NvnLayer.md): include this file, compile
# NVNLAYER_SOURCES with NVNLAYER_CFLAGS beside the program's own sources, which take the same
# flags, and link with NVNLAYER_LDFLAGS. The API 55 build also needs deko3d and libnx in the link
# (-ldeko3d -lnx), as any deko3d program does.
#
#   NVNLAYER_DIR  where this file is, from the program's makefile (nvnlayer)
#   NVNLAYER_API  the NVN API version the layer is built for: 55, NVN on deko3d, for the Switch;
#                 231, the names alone for ScarletNX to take, for the Switch 2. Another value
#                 stops the build.

NVNLAYER_DIR ?= nvnlayer
NVNLAYER_API ?= 55

ifeq ($(NVNLAYER_API),55)
NVNLAYER_SOURCES := $(NVNLAYER_DIR)/source/layer.c $(NVNLAYER_DIR)/source/api55_deko3d.c \
                    $(NVNLAYER_DIR)/source/api55_draw.c $(NVNLAYER_DIR)/source/api55_shader.c \
                    $(NVNLAYER_DIR)/source/api55_command.c \
                    $(NVNLAYER_DIR)/source/api55_debug.c
else ifeq ($(NVNLAYER_API),231)
NVNLAYER_SOURCES := $(NVNLAYER_DIR)/source/layer.c $(NVNLAYER_DIR)/source/api231_surface.c
else
$(error NVNLAYER_API=$(NVNLAYER_API) is not an API version the layer builds for: 55 or 231 (unset, it is 55))
endif

NVNLAYER_CFLAGS  := -I$(NVNLAYER_DIR)/include -DNVN_LAYER_API=$(NVNLAYER_API)
# ScarletNX finds a program's NVN by this name among its exported functions, as it finds a
# game's driver.
NVNLAYER_LDFLAGS := -Wl,--export-dynamic-symbol=nvnBootstrapLoader
