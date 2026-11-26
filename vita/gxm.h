// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

/// The gxm suite (vita/gxm.c): the Vita's GPU through sceGxm, a render target drawn into, read
/// back, sampled as a texture onto the display's buffer and shown.
void RunGxm(void);
