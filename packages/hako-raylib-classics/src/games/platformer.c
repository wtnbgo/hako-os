/*
 * Wrapper of "Platformer" (classics/platformer.c) for hako-raylib-classics.
 *
 * The game is raysan5/raylib-games classics/src/platformer.c at commit 318688c759ea,
 * included unmodified (zlib license, classics/LICENSE). hk.h (HK_WRAP_GAME) renames its
 * main() and redirects its drawing and input queries; see src/hk.h.
 *
 * SPDX-License-Identifier: MIT
 */
#define HK_WRAP_GAME
#include "../hk.h"
#include "../../classics/platformer.c"

/* platformer: main() of the sample sets these before InitGame() */
static void hk_init(void)
{
	screenScale = 2.0;
	screenWidth = TILE_SIZE*TILE_MAP_WIDTH*(int)screenScale;
	screenHeight = TILE_SIZE*TILE_MAP_HEIGHT*(int)screenScale;
	InitGame();
}

static void hk_size(int *w, int *h)
{
	*w = (int)screenWidth;
	*h = (int)screenHeight;
}

const HkGame hk_game_platformer = {
	"Platformer",
	{ "Left / right: move   A: jump", "Touch: on-screen pad and A", NULL },
	HK_KEYS,
	hk_size,
	hk_init,
	UpdateDrawFrame,
	UnloadGame,
};
