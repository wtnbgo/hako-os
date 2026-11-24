/*
 * Wrapper of "Floppy" (classics/floppy.c) for hako-raylib-classics.
 *
 * The game is raysan5/raylib-games classics/src/floppy.c at commit 318688c759ea,
 * included unmodified (zlib license, classics/LICENSE). hk.h (HK_WRAP_GAME) renames its
 * main() and redirects its drawing and input queries; see src/hk.h.
 *
 * SPDX-License-Identifier: MIT
 */
#define HK_WRAP_GAME
#include "../hk.h"
#include "../../classics/floppy.c"

static void hk_size(int *w, int *h)
{
	*w = (int)screenWidth;
	*h = (int)screenHeight;
}

const HkGame hk_game_floppy = {
	"Floppy",
	{ "A (hold): fly up", "START: pause", "Touch: on-screen A" },
	HK_KEYS,
	hk_size,
	InitGame,
	UpdateDrawFrame,
	UnloadGame,
};
