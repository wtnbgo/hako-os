/*
 * hako-raylib-classics: keyboard / mouse queries of the classic games, extended with the
 * pad and the touch screen.
 *
 *   arrow keys   d-pad, left stick, on-screen pad (touch)
 *   SPACE        A (as printed), on-screen A
 *   ENTER        A or START, on-screen A
 *   P (pause)    START, on-screen START
 *   mouse        touch (as the mouse), or a pointer moved by the stick; A clicks.
 *                Missile Commander: the left / top (or bottom) / right face button fire
 *                from the left / middle / right base.
 *
 * SPDX-License-Identifier: MIT
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "hk.h"

#if __has_include(<SDL3/SDL_gamepad.h>)
#include <SDL3/SDL_gamepad.h>
#define HK_HAVE_SDL3 1
#else
#define HK_HAVE_SDL3 0
#endif

enum { K_LEFT, K_RIGHT, K_UP, K_DOWN, K_SPACE, K_ENTER, K_P, K_COUNT };

static struct {
	int mode;
	Rectangle view;        /* where the game texture is drawn on the screen */
	Vector2 game;          /* game screen size */
	int pad;               /* raylib pad index, -1: none */
	bool nintendo;         /* A on the right */
	bool layoutChecked;
	bool now[K_COUNT], prev[K_COUNT];
	bool touchUsed;        /* show the on-screen pad */
	bool menuTouch;
	/* mouse games */
	Vector2 pointer;       /* game coordinates */
	bool pointerFromPad;
	Vector2 lastMouse;
	/* menu key repeat */
	float repeatTime;
	int repeatDir;
	/* on-screen pad layout (screen coordinates) */
	Vector2 dpad, aButton;
	float dpadR, aR;
	Rectangle startRect, menuRect;
} g = { .pad = -1 };

static RenderTexture2D gTarget;

RenderTexture2D *hk_game_target(void) { return &gTarget; }

static bool nintendo_layout(void)
{
	/* HAKO_PAD_LAYOUT=label: hako-session already swapped the mapping to follow the labels */
	const char *layout = getenv("HAKO_PAD_LAYOUT");
	if (layout && !strcmp(layout, "label"))
		return false;
#if HK_HAVE_SDL3
	int count = 0;
	SDL_JoystickID *ids = SDL_GetGamepads(&count);
	bool n = false;
	if (ids && count > 0)
		n = (SDL_GetGamepadButtonLabelForType(SDL_GetGamepadTypeForID(ids[0]), SDL_GAMEPAD_BUTTON_EAST) == SDL_GAMEPAD_BUTTON_LABEL_A);
	SDL_free(ids);
	return n;
#else
	return false;
#endif
}

static int btn_confirm(void) { return g.nintendo ? GAMEPAD_BUTTON_RIGHT_FACE_RIGHT : GAMEPAD_BUTTON_RIGHT_FACE_DOWN; }
static int btn_cancel(void) { return g.nintendo ? GAMEPAD_BUTTON_RIGHT_FACE_DOWN : GAMEPAD_BUTTON_RIGHT_FACE_RIGHT; }

static bool pad_down(int b) { return (g.pad >= 0) && IsGamepadButtonDown(g.pad, b); }
static bool pad_pressed(int b) { return (g.pad >= 0) && IsGamepadButtonPressed(g.pad, b); }

static float pad_axis(int a)
{
	return (g.pad >= 0) ? GetGamepadAxisMovement(g.pad, a) : 0.0f;
}

static Vector2 to_game(Vector2 screen)
{
	return (Vector2){ (screen.x - g.view.x) * g.game.x / g.view.width, (screen.y - g.view.y) * g.game.y / g.view.height };
}

static void layout_touch_pad(void)
{
	const float w = (float)GetScreenWidth(), h = (float)GetScreenHeight();
	g.dpadR = h * 0.13f;
	g.aR = h * 0.09f;
	g.dpad = (Vector2){ g.dpadR * 1.35f, h - g.dpadR * 1.45f };
	g.aButton = (Vector2){ w - g.aR * 1.9f, h - g.dpadR * 1.45f };
	g.startRect = (Rectangle){ w - h * 0.2f, h * 0.03f, h * 0.17f, h * 0.08f };
	g.menuRect = (Rectangle){ h * 0.03f, h * 0.03f, h * 0.17f, h * 0.08f };
}

void hk_input_update(int mode, Rectangle view, Vector2 gameSize)
{
	g.mode = mode;
	g.view = view;
	g.game = gameSize;
	memcpy(g.prev, g.now, sizeof g.now);
	memset(g.now, 0, sizeof g.now);
	g.menuTouch = false;

	/* the first connected pad */
	int pad = -1;
	for (int p = 0; p < 4; p++)
		if (IsGamepadAvailable(p)) {
			pad = p;
			break;
		}
	if (pad != g.pad)
		g.layoutChecked = false;
	g.pad = pad;
	if (g.pad >= 0 && !g.layoutChecked) {
		g.nintendo = nintendo_layout();
		g.layoutChecked = true;
	}

	/* pad */
	float lx = pad_axis(GAMEPAD_AXIS_LEFT_X), ly = pad_axis(GAMEPAD_AXIS_LEFT_Y);
	if (sqrtf(lx * lx + ly * ly) < 0.35f)
		lx = ly = 0.0f;
	g.now[K_LEFT] |= pad_down(GAMEPAD_BUTTON_LEFT_FACE_LEFT) || (lx < -0.5f);
	g.now[K_RIGHT] |= pad_down(GAMEPAD_BUTTON_LEFT_FACE_RIGHT) || (lx > 0.5f);
	g.now[K_UP] |= pad_down(GAMEPAD_BUTTON_LEFT_FACE_UP) || (ly < -0.5f);
	g.now[K_DOWN] |= pad_down(GAMEPAD_BUTTON_LEFT_FACE_DOWN) || (ly > 0.5f);
	if (g.mode == HK_KEYS)
		g.now[K_SPACE] |= pad_down(btn_confirm());
	g.now[K_ENTER] |= pad_down(btn_confirm()) || pad_down(GAMEPAD_BUTTON_MIDDLE_RIGHT);
	g.now[K_P] |= pad_down(GAMEPAD_BUTTON_MIDDLE_RIGHT);

	/* touch: the on-screen pad of keyboard games */
	layout_touch_pad();
	const int touches = GetTouchPointCount();
	if (touches > 0)
		g.touchUsed = true;
	if (g.pad >= 0 && (pad_down(btn_confirm()) || lx != 0.0f || ly != 0.0f))
		g.touchUsed = false;
	for (int i = 0; i < touches; i++) {
		const Vector2 t = GetTouchPosition(i);
		if (CheckCollisionPointRec(t, g.menuRect)) {
			g.menuTouch = true;
			continue;
		}
		if (g.mode != HK_KEYS)
			continue;
		const Vector2 d = { t.x - g.dpad.x, t.y - g.dpad.y };
		const float r = sqrtf(d.x * d.x + d.y * d.y);
		if (r < g.dpadR * 1.6f && r > g.dpadR * 0.2f) {
			g.now[K_LEFT] |= d.x < -fabsf(d.y) * 0.5f;
			g.now[K_RIGHT] |= d.x > fabsf(d.y) * 0.5f;
			g.now[K_UP] |= d.y < -fabsf(d.x) * 0.5f;
			g.now[K_DOWN] |= d.y > fabsf(d.x) * 0.5f;
		} else if (CheckCollisionPointCircle(t, g.aButton, g.aR * 1.6f)) {
			g.now[K_SPACE] = g.now[K_ENTER] = true;
		} else if (CheckCollisionPointRec(t, g.startRect)) {
			g.now[K_P] = g.now[K_ENTER] = true;
		}
	}

	/* mouse games: the pointer */
	if (g.mode != HK_KEYS) {
		const Vector2 m = GetMousePosition();
		if (m.x != g.lastMouse.x || m.y != g.lastMouse.y || IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
			g.pointerFromPad = false;
			g.pointer = to_game(m);
		} else if (lx != 0.0f || ly != 0.0f || g.now[K_LEFT] || g.now[K_RIGHT] || g.now[K_UP] || g.now[K_DOWN]) {
			const float dx = (lx != 0.0f) ? lx : (float)(g.now[K_RIGHT] - g.now[K_LEFT]);
			const float dy = (ly != 0.0f) ? ly : (float)(g.now[K_DOWN] - g.now[K_UP]);
			g.pointerFromPad = true;
			g.pointer.x = fminf(fmaxf(g.pointer.x + dx * 500.0f * GetFrameTime(), 0.0f), g.game.x - 1.0f);
			g.pointer.y = fminf(fmaxf(g.pointer.y + dy * 500.0f * GetFrameTime(), 0.0f), g.game.y - 1.0f);
		}
		g.lastMouse = m;
	}

	/* menu repeat */
	const int dir = (g.now[K_DOWN] ? 1 : 0) - (g.now[K_UP] ? 1 : 0) + (IsKeyDown(KEY_DOWN) ? 1 : 0) - (IsKeyDown(KEY_UP) ? 1 : 0);
	if (dir != g.repeatDir) {
		g.repeatDir = dir;
		g.repeatTime = 0.0f;
	} else {
		g.repeatTime += GetFrameTime();
	}
}

static int key_index(int key)
{
	switch (key) {
	case KEY_LEFT: return K_LEFT;
	case KEY_RIGHT: return K_RIGHT;
	case KEY_UP: return K_UP;
	case KEY_DOWN: return K_DOWN;
	case KEY_SPACE: return K_SPACE;
	case KEY_ENTER: return K_ENTER;
	case 'P': return K_P;
	default: return -1;
	}
}

bool hk_IsKeyDown(int key)
{
	const int i = key_index(key);
	return IsKeyDown(key) || (i >= 0 && g.now[i]);
}

bool hk_IsKeyPressed(int key)
{
	const int i = key_index(key);
	return IsKeyPressed(key) || (i >= 0 && g.now[i] && !g.prev[i]);
}

bool hk_IsKeyReleased(int key)
{
	const int i = key_index(key);
	return IsKeyReleased(key) || (i >= 0 && !g.now[i] && g.prev[i]);
}

bool hk_IsMouseButtonPressed(int button)
{
	if (IsMouseButtonPressed(button))
		return true;
	if (g.pad < 0)
		return false;
	if (g.mode == HK_MOUSE3) {
		/* three mouse buttons (Missile Commander: the left / middle / right base):
		   the face buttons by position, left / top or bottom / right */
		switch (button) {
		case MOUSE_BUTTON_LEFT: return pad_pressed(GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
		case MOUSE_BUTTON_MIDDLE: return pad_pressed(GAMEPAD_BUTTON_RIGHT_FACE_UP) || pad_pressed(GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
		case MOUSE_BUTTON_RIGHT: return pad_pressed(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
		default: return false;
		}
	}
	return (button == MOUSE_BUTTON_LEFT) && pad_pressed(btn_confirm());
}

Vector2 hk_GetMousePosition(void)
{
	return g.pointer;
}

void hk_BeginGameDrawing(void)
{
	BeginTextureMode(gTarget);
}

void hk_EndGameDrawing(void)
{
	EndTextureMode();
}

bool hk_menu_requested(void)
{
	const bool start = pad_down(GAMEPAD_BUTTON_MIDDLE_RIGHT), select = pad_down(GAMEPAD_BUTTON_MIDDLE_LEFT);
	const bool combo = start && select && (pad_pressed(GAMEPAD_BUTTON_MIDDLE_RIGHT) || pad_pressed(GAMEPAD_BUTTON_MIDDLE_LEFT));
	static bool menuTouchPrev = false;
	const bool touch = g.menuTouch && !menuTouchPrev;
	menuTouchPrev = g.menuTouch;
	return IsKeyPressed(KEY_ESCAPE) || combo || pad_pressed(GAMEPAD_BUTTON_MIDDLE) || touch;
}

bool hk_pad_confirm_pressed(void)
{
	return pad_pressed(btn_confirm()) || pad_pressed(GAMEPAD_BUTTON_MIDDLE_RIGHT);
}

bool hk_pad_cancel_pressed(void)
{
	return pad_pressed(btn_cancel());
}

int hk_pad_vertical_repeat(void)
{
	if (g.repeatDir == 0)
		return 0;
	static float next = 0.0f;
	if (g.repeatTime == 0.0f) {
		next = 0.4f;
		return g.repeatDir;
	}
	if (g.repeatTime >= next) {
		next += 0.08f;
		return g.repeatDir;
	}
	return 0;
}

static void draw_button(Vector2 c, float r, const char *label, bool on)
{
	DrawCircleV(c, r, on ? (Color){ 255, 170, 60, 150 } : (Color){ 255, 255, 255, 60 });
	DrawCircleLinesV(c, r, (Color){ 255, 255, 255, 140 });
	const int fs = (int)(r * 0.8f);
	DrawText(label, (int)(c.x - MeasureText(label, fs) / 2), (int)(c.y - fs / 2), fs, (Color){ 255, 255, 255, 200 });
}

void hk_input_draw_overlay(void)
{
	const float h = (float)GetScreenHeight();
	/* the menu button is always there for touch */
	DrawRectangleRounded(g.menuRect, 0.3f, 6, (Color){ 0, 0, 0, 90 });
	DrawText("MENU", (int)(g.menuRect.x + g.menuRect.width * 0.18f), (int)(g.menuRect.y + g.menuRect.height * 0.25f), (int)(h * 0.04f), (Color){ 255, 255, 255, 170 });

	if (g.mode == HK_KEYS && g.touchUsed) {
		DrawCircleV(g.dpad, g.dpadR, (Color){ 255, 255, 255, 40 });
		DrawCircleLinesV(g.dpad, g.dpadR, (Color){ 255, 255, 255, 140 });
		const float a = g.dpadR * 0.6f;
		DrawTriangle((Vector2){ g.dpad.x, g.dpad.y - a - 14 }, (Vector2){ g.dpad.x - 14, g.dpad.y - a + 6 }, (Vector2){ g.dpad.x + 14, g.dpad.y - a + 6 }, g.now[K_UP] ? ORANGE : (Color){ 255, 255, 255, 150 });
		DrawTriangle((Vector2){ g.dpad.x, g.dpad.y + a + 14 }, (Vector2){ g.dpad.x + 14, g.dpad.y + a - 6 }, (Vector2){ g.dpad.x - 14, g.dpad.y + a - 6 }, g.now[K_DOWN] ? ORANGE : (Color){ 255, 255, 255, 150 });
		DrawTriangle((Vector2){ g.dpad.x - a - 14, g.dpad.y }, (Vector2){ g.dpad.x - a + 6, g.dpad.y + 14 }, (Vector2){ g.dpad.x - a + 6, g.dpad.y - 14 }, g.now[K_LEFT] ? ORANGE : (Color){ 255, 255, 255, 150 });
		DrawTriangle((Vector2){ g.dpad.x + a + 14, g.dpad.y }, (Vector2){ g.dpad.x + a - 6, g.dpad.y - 14 }, (Vector2){ g.dpad.x + a - 6, g.dpad.y + 14 }, g.now[K_RIGHT] ? ORANGE : (Color){ 255, 255, 255, 150 });
		draw_button(g.aButton, g.aR, "A", g.now[K_SPACE]);
		DrawRectangleRounded(g.startRect, 0.3f, 6, (Color){ 0, 0, 0, 90 });
		DrawText("START", (int)(g.startRect.x + g.startRect.width * 0.12f), (int)(g.startRect.y + g.startRect.height * 0.25f), (int)(h * 0.04f), (Color){ 255, 255, 255, 170 });
	}

	if (g.mode != HK_KEYS && g.pointerFromPad) {
		const Vector2 p = { g.view.x + g.pointer.x * g.view.width / g.game.x, g.view.y + g.pointer.y * g.view.height / g.game.y };
		DrawTriangle(p, (Vector2){ p.x, p.y + 26 }, (Vector2){ p.x + 18, p.y + 20 }, RAYWHITE);
		DrawTriangleLines(p, (Vector2){ p.x, p.y + 26 }, (Vector2){ p.x + 18, p.y + 20 }, BLACK);
	}
}
