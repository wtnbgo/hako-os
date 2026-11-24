/*
 * hako-raylib-classics: the classic games of raysan5/raylib-games in one app.
 *
 * A menu lists the twelve games of classics/ (unmodified sources, zlib license). Each is
 * drawn at its own size into a render texture that is scaled to the screen; the pad and
 * the touch screen work through hk_input.c. START + SELECT, HOME, Escape or the MENU
 * button (touch) go back to the menu. The licenses are shown from the menu.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <string.h>
#include "raylib.h"
#include "hk.h"

extern const HkGame hk_game_arkanoid, hk_game_asteroids, hk_game_asteroids_survival, hk_game_floppy,
	hk_game_gold_fever, hk_game_gorilas, hk_game_missile_commander, hk_game_pang, hk_game_platformer,
	hk_game_snake, hk_game_space_invaders, hk_game_tetris;

static const HkGame *const games[] = {
	&hk_game_arkanoid, &hk_game_asteroids, &hk_game_asteroids_survival, &hk_game_floppy,
	&hk_game_gold_fever, &hk_game_gorilas, &hk_game_missile_commander, &hk_game_pang,
	&hk_game_platformer, &hk_game_snake, &hk_game_space_invaders, &hk_game_tetris,
};
#define GAME_COUNT ((int)(sizeof games / sizeof games[0]))
#define ENTRY_COUNT (GAME_COUNT + 2) /* + Licenses, Quit */

/* --- licenses (licenses/*.txt next to the program, read at start) ------------------- */

static char *licenseText;
static int licenseLines;
static float licenseScroll;

static void load_licenses(void)
{
	const char *dir = GetApplicationDirectory();
	const char *files[] = { "licenses/raylib-classics.txt", "licenses/raylib.txt", "licenses/raylib-external.txt", "licenses/hako-raylib-classics.txt" };
	size_t total = 0;
	char *texts[4] = { 0 };
	for (int i = 0; i < 4; i++) {
		texts[i] = LoadFileText(TextFormat("%s%s", dir, files[i]));
		if (texts[i])
			total += strlen(texts[i]) + 2;
	}
	licenseText = MemAlloc((unsigned int)total + 1);
	for (int i = 0; i < 4; i++) {
		if (!texts[i])
			continue;
		strcat(licenseText, texts[i]);
		strcat(licenseText, "\n\n");
		UnloadFileText(texts[i]);
	}
	licenseLines = 1;
	for (const char *p = licenseText; *p; p++)
		licenseLines += (*p == '\n');
}

/* returns false when closed */
static bool update_licenses(void)
{
	const int sw = GetScreenWidth(), sh = GetScreenHeight();
	const int fs = sh / 40 < 12 ? 12 : sh / 40, lh = fs + 4;
	const float maxScroll = (float)(licenseLines * lh - (sh - 80));

	licenseScroll -= GetMouseWheelMove() * lh * 3;
	if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
		licenseScroll -= GetMouseDelta().y;
	licenseScroll += (IsKeyDown(KEY_DOWN) - IsKeyDown(KEY_UP)) * 600.0f * GetFrameTime();
	for (int p = 0; p < 4; p++)
		if (IsGamepadAvailable(p)) {
			licenseScroll += (IsGamepadButtonDown(p, GAMEPAD_BUTTON_LEFT_FACE_DOWN) - IsGamepadButtonDown(p, GAMEPAD_BUTTON_LEFT_FACE_UP)
				+ GetGamepadAxisMovement(p, GAMEPAD_AXIS_LEFT_Y) + GetGamepadAxisMovement(p, GAMEPAD_AXIS_RIGHT_Y)) * 600.0f * GetFrameTime();
			if (IsGamepadButtonPressed(p, GAMEPAD_BUTTON_RIGHT_TRIGGER_1)) licenseScroll += sh * 0.8f;
			if (IsGamepadButtonPressed(p, GAMEPAD_BUTTON_LEFT_TRIGGER_1)) licenseScroll -= sh * 0.8f;
			break;
		}
	if (licenseScroll > maxScroll) licenseScroll = maxScroll;
	if (licenseScroll < 0) licenseScroll = 0;

	BeginDrawing();
	ClearBackground((Color){ 245, 245, 245, 255 });
	BeginScissorMode(0, 60, sw, sh - 60);
	int y = 70 - (int)licenseScroll;
	const char *p = licenseText;
	while (*p) {
		const char *e = strchr(p, '\n');
		const int len = e ? (int)(e - p) : (int)strlen(p);
		if (y > 40 && y < sh)
			DrawText(TextFormat("%.*s", len, p), 30, y, fs, (Color){ 40, 40, 40, 255 });
		y += lh;
		if (!e)
			break;
		p = e + 1;
	}
	EndScissorMode();
	DrawRectangle(0, 0, sw, 60, (Color){ 30, 32, 40, 255 });
	DrawText("Licenses", 30, 14, 32, RAYWHITE);
	DrawText("up/down, L/R: scroll   B / tap here: close", sw - 30 - MeasureText("up/down, L/R: scroll   B / tap here: close", 20), 20, 20, LIGHTGRAY);
	EndDrawing();

	const bool tapHeader = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && GetMouseY() < 60;
	return !(hk_pad_cancel_pressed() || hk_menu_requested() || IsKeyPressed(KEY_BACKSPACE) || tapHeader);
}

/* --- menu ---------------------------------------------------------------------------- */

static int update_menu(int *selected)
{
	const int sw = GetScreenWidth(), sh = GetScreenHeight();
	const float unit = sh / 600.0f;
	const int rowH = (int)(36 * unit), top = (int)(110 * unit), colW = (int)(sw / 2 - 40 * unit);
	const int rows = (ENTRY_COUNT + 1) / 2;
	int chosen = -1;

	const int v = hk_pad_vertical_repeat();
	if (v) *selected = (*selected + v + ENTRY_COUNT) % ENTRY_COUNT;
	if (IsKeyPressed(KEY_RIGHT) || (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)))
		*selected = (*selected + rows < ENTRY_COUNT) ? *selected + rows : *selected;
	if (IsKeyPressed(KEY_LEFT) || (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT)))
		*selected = (*selected - rows >= 0) ? *selected - rows : *selected;
	if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || hk_pad_confirm_pressed())
		chosen = *selected;

	BeginDrawing();
	ClearBackground((Color){ 30, 32, 40, 255 });
	DrawText("raylib classics", (int)(30 * unit), (int)(24 * unit), (int)(44 * unit), RAYWHITE);
	DrawText("the classic games of raysan5/raylib-games, for pad and touch", (int)(34 * unit), (int)(72 * unit), (int)(18 * unit), GRAY);

	for (int i = 0; i < ENTRY_COUNT; i++) {
		const Rectangle r = { 30 * unit + (i / rows) * (colW + 20 * unit), (float)(top + (i % rows) * rowH), (float)colW, rowH - 4 * unit };
		if (CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
			*selected = i;
			chosen = i;
		}
		DrawRectangleRounded(r, 0.25f, 6, (i == *selected) ? (Color){ 64, 115, 205, 255 } : (Color){ 52, 54, 64, 255 });
		const char *label = (i < GAME_COUNT) ? TextFormat("%2d. %s", i + 1, games[i]->title) : (i == GAME_COUNT) ? "Licenses" : "Quit";
		DrawText(label, (int)(r.x + 14 * unit), (int)(r.y + (r.height - 22 * unit) / 2), (int)(22 * unit), RAYWHITE);
	}
	DrawText("up/down/left/right: choose   A: start   (touch or mouse also)", (int)(30 * unit), sh - (int)(34 * unit), (int)(18 * unit), GRAY);
	EndDrawing();
	return chosen;
}

/* --- a running game ------------------------------------------------------------------ */

static Rectangle letterbox(int w, int h)
{
	const float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
	const float s = (sw / w < sh / h) ? sw / w : sh / h;
	return (Rectangle){ (sw - w * s) / 2, (sh - h * s) / 2, w * s, h * s };
}

int main(void)
{
	SetConfigFlags(FLAG_FULLSCREEN_MODE | FLAG_VSYNC_HINT);
	InitWindow(0, 0, "raylib classics");   /* 0x0: the display size */
	SetExitKey(KEY_NULL);                  /* Escape goes back to the menu */
	SetTargetFPS(60);
	load_licenses();

	int selected = 0;
	const HkGame *game = NULL;
	bool licenses = false;
	double helpUntil = 0;
	int w = 0, h = 0;

	while (!WindowShouldClose()) {
		if (game) {
			const Rectangle view = letterbox(w, h);
			hk_input_update(game->mode, view, (Vector2){ (float)w, (float)h });
			if (hk_menu_requested()) {
				game->unload();
				UnloadRenderTexture(*hk_game_target());
				game = NULL;
				continue;
			}
			game->frame();   /* draws into the game's render texture */

			BeginDrawing();
			ClearBackground(BLACK);
			const RenderTexture2D *t = hk_game_target();
			DrawTexturePro(t->texture, (Rectangle){ 0, 0, (float)w, (float)-h }, view, (Vector2){ 0, 0 }, 0.0f, WHITE);
			hk_input_draw_overlay();
			if (GetTime() < helpUntil) {
				const int sh = GetScreenHeight(), fs = sh / 28;
				DrawRectangle(0, sh - fs * 5, GetScreenWidth(), fs * 5, (Color){ 0, 0, 0, 170 });
				DrawText(game->title, fs, sh - fs * 5 + fs / 2, fs + 4, RAYWHITE);
				for (int i = 0; i < 3 && game->help[i]; i++)
					DrawText(game->help[i], fs, sh - fs * 5 + fs * 2 + i * (fs + 4), fs, LIGHTGRAY);
			}
			EndDrawing();
			continue;
		}

		hk_input_update(HK_KEYS, (Rectangle){ 0 }, (Vector2){ 1, 1 });

		if (licenses) {
			licenses = update_licenses();
			continue;
		}

		const int chosen = update_menu(&selected);
		if (chosen < 0)
			continue;
		if (chosen < GAME_COUNT) {
			game = games[chosen];
			game->init();
			game->size(&w, &h);
			*hk_game_target() = LoadRenderTexture(w, h);
			SetTextureFilter(hk_game_target()->texture, TEXTURE_FILTER_BILINEAR);
			helpUntil = GetTime() + 5.0;
		} else if (chosen == GAME_COUNT) {
			licenses = true;
			licenseScroll = 0;
		} else {
			break;
		}
	}

	CloseWindow();
	return 0;
}
