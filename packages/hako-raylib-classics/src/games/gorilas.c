/*
 * Wrapper of "Gorilas" (classics/gorilas.c) for hako-raylib-classics.
 *
 * The game is raysan5/raylib-games classics/src/gorilas.c at commit 318688c759ea,
 * included unmodified (zlib license, classics/LICENSE). hk.h (HK_WRAP_GAME) renames its
 * main() and redirects its drawing and input queries; see src/hk.h.
 *
 * SPDX-License-Identifier: MIT
 */
#define HK_WRAP_GAME
#include "../hk.h"
#include "../../classics/gorilas.c"

static void hk_size(int *w, int *h)
{
	*w = (int)screenWidth;
	*h = (int)screenHeight;
}

const HkGame hk_game_gorilas = {
	"Gorilas",
	{ "Stick: aim with the pointer   A: throw", "Touch: tap where to throw (power by distance)", "START: pause" },
	HK_MOUSE,
	hk_size,
	InitGame,
	UpdateDrawFrame,
	UnloadGame,
};
