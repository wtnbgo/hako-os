/*
 * hako-raylib-hello: raylib (SDL3 platform) sample for hako.
 *
 * A small 3D scene: move the cube with the left stick / d-pad, jump with the
 * bottom face button, orbit the camera with the right stick, zoom with L2/R2.
 * The overlay shows the face buttons, sticks, touch points and FPS, and every
 * face button plays a tone. START + SELECT (or Escape) quits.
 *
 * raylib's GAMEPAD_BUTTON_RIGHT_FACE_* are positions (SDL convention). With
 * HAKO_PAD_LAYOUT=label hako-session swaps a<->b and x<->y, so the button
 * raylib calls "down" is the one printed A (on the right) and so on; the
 * overlay follows that to keep drawing each button where it physically is.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "raymath.h"

#define PAD 0
#define TONES 4

static Sound make_tone(float freq)
{
	const int rate = 44100, frames = rate / 8;
	short *pcm = MemAlloc(frames * sizeof(short));
	for (int i = 0; i < frames; i++) {
		float env = 1.0f - (float)i / frames;
		pcm[i] = (short)(sinf(2 * PI * freq * i / rate) * 9000 * env);
	}
	Wave w = { .frameCount = frames, .sampleRate = rate, .sampleSize = 16, .channels = 1, .data = pcm };
	Sound s = LoadSoundFromWave(w);
	UnloadWave(w);
	return s;
}

/* one face button of the overlay: lit while held */
static void face(int button, Vector2 at, const char *label)
{
	bool down = IsGamepadButtonDown(PAD, button);
	DrawCircleV(at, 34, down ? ORANGE : (Color){ 60, 64, 80, 255 });
	DrawCircleLinesV(at, 34, RAYWHITE);
	int w = MeasureText(label, 30);
	DrawText(label, (int)at.x - w / 2, (int)at.y - 15, 30, RAYWHITE);
}

static void stick(Vector2 c, float x, float y, bool pressed)
{
	DrawCircleLinesV(c, 50, GRAY);
	DrawCircleV(Vector2Add(c, (Vector2){ x * 40, y * 40 }), 16, pressed ? ORANGE : SKYBLUE);
}

int main(void)
{
	SetConfigFlags(FLAG_FULLSCREEN_MODE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
	InitWindow(0, 0, "hako raylib sample");   /* 0x0: the display size */
	InitAudioDevice();
	SetExitKey(KEY_ESCAPE);

	const char *layout = getenv("HAKO_PAD_LAYOUT");
	bool by_label = layout && !strcmp(layout, "label");

	Sound tone[TONES] = { make_tone(523.25f), make_tone(659.25f), make_tone(783.99f), make_tone(1046.5f) };
	const int face_buttons[TONES] = { GAMEPAD_BUTTON_RIGHT_FACE_DOWN, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT,
	                                  GAMEPAD_BUTTON_RIGHT_FACE_LEFT, GAMEPAD_BUTTON_RIGHT_FACE_UP };

	Vector3 pos = { 0, 0.5f, 0 };
	float vy = 0, yaw = 0.8f, pitch = 0.45f, dist = 12, t = 0;
	Camera3D cam = { .up = { 0, 1, 0 }, .fovy = 50, .projection = CAMERA_PERSPECTIVE };

	while (!WindowShouldClose()) {
		float dt = GetFrameTime();
		t += dt;
		bool pad = IsGamepadAvailable(PAD);

		for (int i = 0; i < TONES; i++)
			if (IsGamepadButtonPressed(PAD, face_buttons[i]))
				PlaySound(tone[i]);
		if (IsGamepadButtonDown(PAD, GAMEPAD_BUTTON_MIDDLE_LEFT) && IsGamepadButtonDown(PAD, GAMEPAD_BUTTON_MIDDLE_RIGHT))
			break;

		/* move relative to the camera direction */
		float lx = 0, ly = 0;
		if (pad) {
			lx = GetGamepadAxisMovement(PAD, GAMEPAD_AXIS_LEFT_X);
			ly = GetGamepadAxisMovement(PAD, GAMEPAD_AXIS_LEFT_Y);
		}
		lx += IsGamepadButtonDown(PAD, GAMEPAD_BUTTON_LEFT_FACE_RIGHT) - IsGamepadButtonDown(PAD, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
		ly += IsGamepadButtonDown(PAD, GAMEPAD_BUTTON_LEFT_FACE_DOWN) - IsGamepadButtonDown(PAD, GAMEPAD_BUTTON_LEFT_FACE_UP);
		lx += IsKeyDown(KEY_RIGHT) - IsKeyDown(KEY_LEFT);
		ly += IsKeyDown(KEY_DOWN) - IsKeyDown(KEY_UP);
		if (fabsf(lx) < 0.15f) lx = 0;
		if (fabsf(ly) < 0.15f) ly = 0;
		Vector3 fwd = { -cosf(yaw), 0, -sinf(yaw) }, right = { -fwd.z, 0, fwd.x };
		pos = Vector3Add(pos, Vector3Scale(Vector3Add(Vector3Scale(right, lx), Vector3Scale(fwd, -ly)), 6 * dt));
		pos.x = Clamp(pos.x, -9.5f, 9.5f);
		pos.z = Clamp(pos.z, -9.5f, 9.5f);

		if ((IsGamepadButtonPressed(PAD, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || IsKeyPressed(KEY_SPACE)) && pos.y <= 0.5f)
			vy = 7;
		vy -= 20 * dt;
		pos.y += vy * dt;
		if (pos.y < 0.5f) { pos.y = 0.5f; vy = 0; }

		if (pad) {
			float rx = GetGamepadAxisMovement(PAD, GAMEPAD_AXIS_RIGHT_X);
			float ry = GetGamepadAxisMovement(PAD, GAMEPAD_AXIS_RIGHT_Y);
			if (fabsf(rx) > 0.15f) yaw += rx * 2 * dt;
			if (fabsf(ry) > 0.15f) pitch = Clamp(pitch + ry * 1.5f * dt, 0.1f, 1.3f);
			float lt = (GetGamepadAxisMovement(PAD, GAMEPAD_AXIS_LEFT_TRIGGER) + 1) / 2;
			float rt = (GetGamepadAxisMovement(PAD, GAMEPAD_AXIS_RIGHT_TRIGGER) + 1) / 2;
			dist = Clamp(dist + (lt - rt) * 10 * dt, 5, 25);
		}
		cam.target = pos;
		cam.position = Vector3Add(pos, (Vector3){ cosf(yaw) * cosf(pitch) * dist, sinf(pitch) * dist,
		                                          sinf(yaw) * cosf(pitch) * dist });

		int w = GetScreenWidth(), h = GetScreenHeight();
		BeginDrawing();
		ClearBackground((Color){ 20, 23, 36, 255 });

		BeginMode3D(cam);
		DrawPlane((Vector3){ 0, 0, 0 }, (Vector2){ 20, 20 }, (Color){ 36, 42, 60, 255 });
		DrawGrid(20, 1);
		for (int i = 0; i < 8; i++) {
			float a = t * 0.5f + i * PI / 4;
			Vector3 p = { cosf(a) * 7, 1 + sinf(t * 2 + i) * 0.4f, sinf(a) * 7 };
			DrawCube(p, 0.8f, 0.8f, 0.8f, ColorFromHSV(i * 45.0f, 0.6f, 0.95f));
			DrawCubeWires(p, 0.8f, 0.8f, 0.8f, RAYWHITE);
		}
		DrawCube(pos, 1, 1, 1, ORANGE);
		DrawCubeWires(pos, 1, 1, 1, MAROON);
		DrawCylinder((Vector3){ pos.x, 0.01f, pos.z }, 0.6f, 0.6f, 0.01f, 16, Fade(BLACK, 0.4f));
		EndMode3D();

		DrawText(TextFormat("raylib %s  SDL3  %dx%d  FPS %d", RAYLIB_VERSION, w, h, GetFPS()), 24, 20, 28, RAYWHITE);
		DrawText(pad ? GetGamepadName(PAD) : "no gamepad", 24, 56, 24, LIGHTGRAY);
		DrawText(by_label ? "HAKO_PAD_LAYOUT=label" : "HAKO_PAD_LAYOUT=position", 24, 86, 20, GRAY);
		DrawText(TextFormat("L stick: move  %s: jump  R stick: camera  L2/R2: zoom  START+SELECT: quit",
		                    by_label ? "A" : "bottom (B)"),
		         24, h - 40, 22, LIGHTGRAY);

		/* face buttons where they physically are, with the printed letters
		 * of a Nintendo-layout pad (A right, B bottom, X top, Y left) */
		Vector2 fc = { w - 170.0f, h - 230.0f };
		if (by_label) {
			face(GAMEPAD_BUTTON_RIGHT_FACE_DOWN, (Vector2){ fc.x + 75, fc.y }, "A");
			face(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, (Vector2){ fc.x, fc.y + 75 }, "B");
			face(GAMEPAD_BUTTON_RIGHT_FACE_LEFT, (Vector2){ fc.x, fc.y - 75 }, "X");
			face(GAMEPAD_BUTTON_RIGHT_FACE_UP, (Vector2){ fc.x - 75, fc.y }, "Y");
		} else {
			face(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, (Vector2){ fc.x + 75, fc.y }, "A");
			face(GAMEPAD_BUTTON_RIGHT_FACE_DOWN, (Vector2){ fc.x, fc.y + 75 }, "B");
			face(GAMEPAD_BUTTON_RIGHT_FACE_UP, (Vector2){ fc.x, fc.y - 75 }, "X");
			face(GAMEPAD_BUTTON_RIGHT_FACE_LEFT, (Vector2){ fc.x - 75, fc.y }, "Y");
		}
		if (pad) {
			stick((Vector2){ 110, h - 200.0f }, GetGamepadAxisMovement(PAD, GAMEPAD_AXIS_LEFT_X),
			      GetGamepadAxisMovement(PAD, GAMEPAD_AXIS_LEFT_Y), IsGamepadButtonDown(PAD, GAMEPAD_BUTTON_LEFT_THUMB));
			stick((Vector2){ 250, h - 200.0f }, GetGamepadAxisMovement(PAD, GAMEPAD_AXIS_RIGHT_X),
			      GetGamepadAxisMovement(PAD, GAMEPAD_AXIS_RIGHT_Y), IsGamepadButtonDown(PAD, GAMEPAD_BUTTON_RIGHT_THUMB));
		}

		for (int i = 0; i < GetTouchPointCount(); i++) {
			Vector2 p = GetTouchPosition(i);
			DrawCircleLinesV(p, 50, ORANGE);
			DrawCircleLinesV(p, 52, ORANGE);
		}
		EndDrawing();
	}

	for (int i = 0; i < TONES; i++)
		UnloadSound(tone[i]);
	CloseAudioDevice();
	CloseWindow();
	return 0;
}
