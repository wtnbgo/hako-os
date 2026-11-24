/*
 * Wrapper of "Tetris" (classics/tetris.c) for hako-raylib-classics.
 *
 * The game is raysan5/raylib-games classics/src/tetris.c at commit 318688c759ea,
 * included unmodified (zlib license, classics/LICENSE). hk.h (HK_WRAP_GAME) renames its
 * main() and redirects its drawing and input queries; see src/hk.h.
 *
 * SPDX-License-Identifier: MIT
 */
#define HK_WRAP_GAME
#include "../hk.h"
#include "../../classics/tetris.c"

static void hk_size(int *w, int *h)
{
	*w = (int)screenWidth;
	*h = (int)screenHeight;
}

const HkGame hk_game_tetris = {
	"Tetris",
	{ "Left / right: move   Up: rotate   Down: drop faster", "START: pause", "Touch: on-screen pad" },
	HK_KEYS,
	hk_size,
	InitGame,
	UpdateDrawFrame,
	UnloadGame,
};
