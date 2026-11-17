# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
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
#   NVNLAYER_DRIVER  whose NVN the program's lookup reaches: layer, this layer's, as above; game,
#                 the driver of the game the program is loaded in as one more module, where the
#                 layer is its types, its list of names, the loader, and for a name that driver
#                 lacks a body that keeps the name and returns nought. Another value stops the
#                 build.

NVNLAYER_DIR ?= nvnlayer
NVNLAYER_API ?= 55
NVNLAYER_DRIVER ?= layer

ifeq ($(filter $(NVNLAYER_DRIVER),layer game),)
$(error NVNLAYER_DRIVER=$(NVNLAYER_DRIVER) is not a driver the layer builds for: layer or game (unset, it is layer))
endif

ifeq ($(filter $(NVNLAYER_API),55 231),)
$(error NVNLAYER_API=$(NVNLAYER_API) is not an API version the layer builds for: 55 or 231 (unset, it is 55))
else ifeq ($(NVNLAYER_DRIVER),game)
NVNLAYER_SOURCES := $(NVNLAYER_DIR)/source/layer.c $(NVNLAYER_DIR)/source/game_driver.c
else ifeq ($(NVNLAYER_API),55)
NVNLAYER_SOURCES := $(NVNLAYER_DIR)/source/layer.c $(NVNLAYER_DIR)/source/api55_deko3d.c \
                    $(NVNLAYER_DIR)/source/api55_draw.c $(NVNLAYER_DIR)/source/api55_shader.c \
                    $(NVNLAYER_DIR)/source/api55_command.c \
                    $(NVNLAYER_DIR)/source/api55_debug.c \
                    $(NVNLAYER_DIR)/source/api55_object.c
else
NVNLAYER_SOURCES := $(NVNLAYER_DIR)/source/layer.c $(NVNLAYER_DIR)/source/api231_surface.c
endif

NVNLAYER_CFLAGS  := -I$(NVNLAYER_DIR)/include -DNVN_LAYER_API=$(NVNLAYER_API)
ifeq ($(NVNLAYER_DRIVER),game)
# The game exports the driver's nvnBootstrapLoader, and a second one of that name in the process
# would be found in its place.
NVNLAYER_CFLAGS  += -DNVN_LAYER_GAME_DRIVER
NVNLAYER_LDFLAGS :=
else
# ScarletNX finds a program's NVN by this name among its exported functions, as it finds a
# game's driver.
NVNLAYER_LDFLAGS := -Wl,--export-dynamic-symbol=nvnBootstrapLoader
endif
