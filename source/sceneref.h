// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// The scene the nvn suite draws down both of the program's ways to the graphics unit
/// (drawpath.h), and what each of its pixels has to hold: one answer for both ways, worked out
/// here with no graphics library, so the host build checks it.
///
/// A 64 by 64 target in four quarters of 32 by 32:
/// - top left: an 8 by 8 texture drawn four pixels a texel, sampled nearest through texture
///   slot 1, every pixel centre an eighth of a texel or more from a texel's edge. Slot 0 holds
///   a decoy of other texels;
/// - top right: a quad of one flat colour drawn with the depth test and write on, then a
///   farther quad of another colour over it, which the depth test has to hide;
/// - bottom left: a quad the fragment stage's uniform buffer colours;
/// - bottom right: the clear colour, and in it a 16 by 16 square whose vertices are the target's
///   top left corner, moved here by the vertex stage's uniform buffer. The three draws before it
///   have a buffer that moves nothing.
#define SCENE_SIZE 64u
#define SCENE_QUARTER 32u
#define SCENE_TEXTURE_SIZE 8u
/// The moved square: its side, and the pixel its top left corner lands on.
#define SCENE_SQUARE_SIDE 16u
#define SCENE_SQUARE_AT 40u

/// Texel @p x, @p y of the texture the top left quarter shows: four bytes no other texel has.
void SceneTexel(uint32_t x, uint32_t y, uint8_t out[4]);

/// Texel @p x, @p y of the decoy in texture slot 0: no byte triple the texture has.
void SceneDecoyTexel(uint32_t x, uint32_t y, uint8_t out[4]);

/// The scene's colours as bytes; a way gives the graphics unit each as byte / 255.
extern const uint8_t SCENE_CLEAR[4];
extern const uint8_t SCENE_NEAR[4];   ///< the top right quad the depth test keeps
extern const uint8_t SCENE_FAR[4];    ///< the one it hides
extern const uint8_t SCENE_PAINT[4];  ///< the fragment uniform buffer's
extern const uint8_t SCENE_SQUARE[4]; ///< the moved square's

/// The four bytes pixel @p x, @p y holds, the top row first.
void SceneExpected(uint32_t x, uint32_t y, uint8_t out[4]);

/// What a wrong pixel looks like, for a failing case's sentence: the part of the scene whose
/// colour @p pixel is, or "nothing the scene draws".
const char *SceneLooksLike(const uint8_t pixel[4]);

#ifdef __cplusplus
}
#endif
