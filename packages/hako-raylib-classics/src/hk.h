/*
 * hako-raylib-classics: shared declarations.
 *
 * The classic games (classics/, unmodified sources of raysan5/raylib-games) are each
 * compiled through a small wrapper in src/games/. The wrapper includes this header
 * with HK_WRAP_GAME defined before the game's source, so that:
 *
 *   - the game's main() is renamed and never called (the collection owns the window),
 *   - BeginDrawing() / EndDrawing() draw into the game's own render texture, which the
 *     collection scales to the screen (letterbox),
 *   - keyboard and mouse queries also see the pad and the touch screen
 *     (see hk_input.c: arrows = d-pad / left stick / touch pad, SPACE and ENTER = A,
 *     P = START; the mouse = touch, or a pointer moved by the stick with A to click).
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef HK_H
#define HK_H

#include <stdbool.h>
#include <stddef.h>
#include "raylib.h"

/* how the game is played: changes what the pad and the touch screen do */
enum {
	HK_KEYS = 0,     /* keyboard game: touch shows an on-screen pad */
	HK_MOUSE = 1,    /* mouse game: touch is the mouse, the stick moves a pointer, A clicks */
	HK_MOUSE3 = 2,   /* like HK_MOUSE, the three face buttons by position are the three mouse buttons */
};

typedef struct HkGame {
	const char *title;
	const char *help[3];          /* controls on the pad / touch, shown at the start */
	int mode;                     /* HK_KEYS / HK_MOUSE */
	void (*size)(int *w, int *h); /* the game's screen size (after init) */
	void (*init)(void);
	void (*frame)(void);          /* update and draw one frame */
	void (*unload)(void);
} HkGame;

/* --- used by the collection (main.c) ----------------------------------------------- */

void hk_input_update(int mode, Rectangle view, Vector2 gameSize);
void hk_input_draw_overlay(void);          /* on-screen pad / pad pointer, screen coordinates */
bool hk_menu_requested(void);              /* START+SELECT, HOME or Escape */
bool hk_pad_confirm_pressed(void);
bool hk_pad_cancel_pressed(void);
int hk_pad_vertical_repeat(void);          /* -1 / +1 with key repeat */
RenderTexture2D *hk_game_target(void);

/* --- used by the wrapped games (through the macros below) --------------------------- */

bool hk_IsKeyDown(int key);
bool hk_IsKeyPressed(int key);
bool hk_IsKeyReleased(int key);
bool hk_IsMouseButtonPressed(int button);
Vector2 hk_GetMousePosition(void);
void hk_BeginGameDrawing(void);
void hk_EndGameDrawing(void);

#ifdef HK_WRAP_GAME
/* "int main(void)" becomes "int static hk_unused_game_main(void)": one per game, never called */
#define main static hk_unused_game_main
#define IsKeyDown(k) hk_IsKeyDown(k)
#define IsKeyPressed(k) hk_IsKeyPressed(k)
#define IsKeyReleased(k) hk_IsKeyReleased(k)
#define IsMouseButtonPressed(b) hk_IsMouseButtonPressed(b)
#define GetMousePosition() hk_GetMousePosition()
#define BeginDrawing() hk_BeginGameDrawing()
#define EndDrawing() hk_EndGameDrawing()
#endif

#endif
