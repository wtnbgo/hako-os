/*
 * Wrapper of "Arkanoid" (classics/arkanoid.c) for hako-raylib-classics.
 *
 * The game is raysan5/raylib-games classics/src/arkanoid.c at commit 318688c759ea,
 * included unmodified (zlib license, classics/LICENSE). hk.h (HK_WRAP_GAME) renames its
 * main() and redirects its drawing and input queries; see src/hk.h.
 *
 * SPDX-License-Identifier: MIT
 */
#define HK_WRAP_GAME
#include "../hk.h"
#include "../../classics/arkanoid.c"

static void hk_size(int *w, int *h)
{
	*w = (int)screenWidth;
	*h = (int)screenHeight;
}

const HkGame hk_game_arkanoid = {
	"Arkanoid",
	{ "Left / right: move the paddle", "A: launch the ball   START: pause", "Touch: on-screen pad and A" },
	HK_KEYS,
	hk_size,
	InitGame,
	UpdateDrawFrame,
	UnloadGame,
};
