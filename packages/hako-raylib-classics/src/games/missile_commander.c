/*
 * Wrapper of "Missile Commander" (classics/missile_commander.c) for hako-raylib-classics.
 *
 * The game is raysan5/raylib-games classics/src/missile_commander.c at commit 318688c759ea,
 * included unmodified (zlib license, classics/LICENSE). hk.h (HK_WRAP_GAME) renames its
 * main() and redirects its drawing and input queries; see src/hk.h.
 *
 * SPDX-License-Identifier: MIT
 */
#define HK_WRAP_GAME
#include "../hk.h"
#include "../../classics/missile_commander.c"

static void hk_size(int *w, int *h)
{
	*w = (int)screenWidth;
	*h = (int)screenHeight;
}

const HkGame hk_game_missile_commander = {
	"Missile Commander",
	{ "Stick: aim with the pointer", "Left / top / right face button: fire from the left / middle / right base", "Touch: tap to fire" },
	HK_MOUSE3,
	hk_size,
	InitGame,
	UpdateDrawFrame,
	UnloadGame,
};
