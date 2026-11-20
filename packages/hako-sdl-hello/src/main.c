/*
 * hako-sdl-hello: minimal SDL3 sample for hako.
 *
 * Draws a landscape 1280x960 canvas, rotated onto the portrait-native panel,
 * and visualises the gamepad (Switch layout labels), the touch panel and a
 * bit of animation. Runs directly on KMS/DRM (no compositor).
 *
 *   hako-sdl-hello [--rotate 0|90|180|270] [--seconds N] [--shot file.bmp]
 *
 *   --rotate   canvas rotation on the panel, clockwise (default 90, or 360 - $HAKO_ROTATE)
 *   --seconds  quit after N seconds
 *   --shot     save the canvas as BMP after about one second
 *
 * Quit: hold Start + Select, or Ctrl+C.
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CW 1280 /* canvas (landscape) */
#define CH 960
#define MAX_TOUCH 10

typedef struct { float x, y; bool down; SDL_FingerID id; } Touch;

static SDL_Renderer *ren;

static SDL_AudioStream *audio;

static void color(Uint8 r, Uint8 g, Uint8 b) { SDL_SetRenderDrawColor(ren, r, g, b, 255); }

/* short sine blip with a decaying envelope */
static void beep(float freq)
{
	enum { RATE = 48000, LEN = RATE / 8 };
	static float buf[LEN];
	if (!audio)
		return;
	for (int i = 0; i < LEN; i++) {
		float env = 1.0f - (float)i / LEN;
		buf[i] = 0.5f * env * env * SDL_sinf(2 * SDL_PI_F * freq * i / RATE);
	}
	SDL_ClearAudioStream(audio);
	SDL_PutAudioStreamData(audio, buf, sizeof buf);
}

/* 8x8 debug font scaled by s */
static void text(float x, float y, float s, const char *fmt, ...)
{
	char buf[256];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	SDL_SetRenderScale(ren, s, s);
	SDL_RenderDebugText(ren, x / s, y / s, buf);
	SDL_SetRenderScale(ren, 1, 1);
}

static void text_center(float cx, float cy, float s, const char *str)
{
	text(cx - strlen(str) * 4 * s, cy - 4 * s, s, "%s", str);
}

static void circle(float cx, float cy, float r, bool fill)
{
	enum { N = 48 };
	Uint8 cr, cg, cb, ca;
	SDL_GetRenderDrawColor(ren, &cr, &cg, &cb, &ca);
	if (fill) {
		SDL_Vertex v[N + 1];
		int idx[N * 3];
		SDL_FColor fc = { cr / 255.f, cg / 255.f, cb / 255.f, ca / 255.f };
		v[0] = (SDL_Vertex){ { cx, cy }, fc, { 0, 0 } };
		for (int i = 0; i < N; i++) {
			float a = i * 2 * SDL_PI_F / N;
			v[i + 1] = (SDL_Vertex){ { cx + r * SDL_cosf(a), cy + r * SDL_sinf(a) }, fc, { 0, 0 } };
			idx[i * 3] = 0;
			idx[i * 3 + 1] = i + 1;
			idx[i * 3 + 2] = (i + 1) % N + 1;
		}
		SDL_RenderGeometry(ren, NULL, v, N + 1, idx, N * 3);
	} else {
		SDL_FPoint p[N + 1];
		for (int i = 0; i <= N; i++) {
			float a = i * 2 * SDL_PI_F / N;
			p[i] = (SDL_FPoint){ cx + r * SDL_cosf(a), cy + r * SDL_sinf(a) };
		}
		SDL_RenderLines(ren, p, N + 1);
	}
}

static void box(float x, float y, float w, float h, bool fill)
{
	SDL_FRect r = { x, y, w, h };
	if (fill)
		SDL_RenderFillRect(ren, &r);
	else
		SDL_RenderRect(ren, &r);
}

static const char *label_str(SDL_GamepadButtonLabel l)
{
	switch (l) {
	case SDL_GAMEPAD_BUTTON_LABEL_A: return "A";
	case SDL_GAMEPAD_BUTTON_LABEL_B: return "B";
	case SDL_GAMEPAD_BUTTON_LABEL_X: return "X";
	case SDL_GAMEPAD_BUTTON_LABEL_Y: return "Y";
	default: return "?";
	}
}

/* a round face button; pressed = filled */
static void face_button(SDL_Gamepad *gp, SDL_GamepadButton b, float x, float y)
{
	bool on = gp && SDL_GetGamepadButton(gp, b);
	color(on ? 255 : 90, on ? 200 : 90, on ? 60 : 110);
	circle(x, y, 42, on);
	color(on ? 30 : 230, on ? 30 : 230, on ? 30 : 230);
	text_center(x, y, 4, gp ? label_str(SDL_GetGamepadButtonLabel(gp, b)) : "-");
}

static void rect_button(SDL_Gamepad *gp, SDL_GamepadButton b, float x, float y, float w, float h, const char *name)
{
	bool on = gp && SDL_GetGamepadButton(gp, b);
	color(on ? 255 : 90, on ? 200 : 90, on ? 60 : 110);
	box(x, y, w, h, on);
	color(on ? 30 : 200, on ? 30 : 200, on ? 30 : 200);
	text_center(x + w / 2, y + h / 2, 2, name);
}

static void stick(SDL_Gamepad *gp, SDL_GamepadAxis ax, SDL_GamepadAxis ay, SDL_GamepadButton click, float cx, float cy)
{
	float r = 90;
	float x = gp ? SDL_GetGamepadAxis(gp, ax) / 32767.f : 0;
	float y = gp ? SDL_GetGamepadAxis(gp, ay) / 32767.f : 0;
	bool on = gp && SDL_GetGamepadButton(gp, click);
	color(90, 90, 110);
	circle(cx, cy, r, false);
	color(60, 60, 75);
	SDL_RenderLine(ren, cx - r, cy, cx + r, cy);
	SDL_RenderLine(ren, cx, cy - r, cx, cy + r);
	color(on ? 255 : 80, on ? 200 : 200, on ? 60 : 255);
	circle(cx + x * r, cy + y * r, 26, true);
	color(180, 180, 190);
	text(cx - r, cy + r + 14, 2, "%+.2f %+.2f", x, y);
}

static void trigger(SDL_Gamepad *gp, SDL_GamepadAxis a, float x, float y, const char *name)
{
	float v = gp ? SDL_GetGamepadAxis(gp, a) / 32767.f : 0;
	if (v < 0)
		v = 0;
	color(90, 90, 110);
	box(x, y, 200, 36, false);
	color(255, 200, 60);
	box(x + 2, y + 2, 196 * v, 32, true);
	color(230, 230, 230);
	text(x + 8, y + 10, 2, "%s %3.0f%%", name, v * 100);
}

int main(int argc, char **argv)
{
	/* HAKO_ROTATE is the output transform in wlr-randr terms (counter-clockwise,
	 * 270 on this panel); the canvas turns the other way. Under cage it is 0. */
	int rotate = getenv("HAKO_ROTATE") ? (360 - atoi(getenv("HAKO_ROTATE")) % 360) % 360 : 90;
	double seconds = 0;
	const char *shot = NULL;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--rotate") && i + 1 < argc)
			rotate = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--seconds") && i + 1 < argc)
			seconds = atof(argv[++i]);
		else if (!strcmp(argv[i], "--shot") && i + 1 < argc)
			shot = argv[++i];
		else {
			fprintf(stderr, "usage: %s [--rotate 0|90|180|270] [--seconds N] [--shot file.bmp]\n", argv[0]);
			return 2;
		}
	}

	/* The Retroid pad is unknown to SDL; tell it the face buttons carry Nintendo labels. */
	SDL_SetHint(SDL_HINT_GAMECONTROLLERTYPE, "0x2020/0x3001=SwitchPro");

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
		fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return 1;
	}
	SDL_Window *win;
	if (!SDL_CreateWindowAndRenderer("hako-sdl-hello", CW, CH, SDL_WINDOW_FULLSCREEN, &win, &ren)) {
		fprintf(stderr, "create window: %s\n", SDL_GetError());
		return 1;
	}
	SDL_SetRenderVSync(ren, 1);
	SDL_HideCursor();

	/* sound is optional: keep going without it */
	if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
		SDL_AudioSpec spec = { SDL_AUDIO_F32, 1, 48000 };
		audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
		if (audio)
			SDL_ResumeAudioStreamDevice(audio);
	}
	printf("audio=%s %s\n", SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "(none)",
	       audio ? "ok" : SDL_GetError());

	int ow, oh;
	SDL_GetRenderOutputSize(ren, &ow, &oh);
	SDL_Texture *canvas = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, CW, CH);
	if (!canvas) {
		fprintf(stderr, "canvas: %s\n", SDL_GetError());
		return 1;
	}
	printf("video=%s renderer=%s output=%dx%d rotate=%d\n", SDL_GetCurrentVideoDriver(),
	       SDL_GetRendererName(ren), ow, oh, rotate);
	fflush(stdout);

	/* fit the rotated canvas into the output */
	bool sideways = rotate == 90 || rotate == 270;
	float cw = sideways ? CH : CW, chh = sideways ? CW : CH;
	float fit = SDL_min(ow / cw, oh / chh);
	SDL_FRect dst = { (ow - CW * fit) / 2, (oh - CH * fit) / 2, CW * fit, CH * fit };

	SDL_Gamepad *gp = NULL;
	Touch touch[MAX_TOUCH] = { 0 };
	float bx = 200, by = 200, vx = 6, vy = 4.5f;
	Uint64 t0 = SDL_GetTicks(), fps_t = t0;
	int frames = 0, fps = 0, frame_no = 0;
	Uint64 combo_since = 0;
	bool run = true;

	while (run) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			switch (e.type) {
			case SDL_EVENT_QUIT:
				run = false;
				break;
			case SDL_EVENT_GAMEPAD_ADDED:
				if (!gp) {
					gp = SDL_OpenGamepad(e.gdevice.which);
					char *map = SDL_GetGamepadMapping(gp);
					printf("gamepad: %s (%s)\nmapping: %s\n", SDL_GetGamepadName(gp),
					       SDL_GetGamepadStringForType(SDL_GetGamepadType(gp)), map ? map : "(none)");
					SDL_free(map);
					fflush(stdout);
				}
				break;
			case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
				switch (e.gbutton.button) {
				case SDL_GAMEPAD_BUTTON_SOUTH: beep(523.25f); break; /* C5 */
				case SDL_GAMEPAD_BUTTON_EAST:  beep(659.25f); break; /* E5 */
				case SDL_GAMEPAD_BUTTON_WEST:  beep(783.99f); break; /* G5 */
				case SDL_GAMEPAD_BUTTON_NORTH: beep(1046.5f); break; /* C6 */
				default: break;
				}
				break;
			case SDL_EVENT_GAMEPAD_REMOVED:
				if (gp && SDL_GetGamepadID(gp) == e.gdevice.which) {
					SDL_CloseGamepad(gp);
					gp = NULL;
				}
				break;
			case SDL_EVENT_FINGER_DOWN:
			case SDL_EVENT_FINGER_MOTION:
			case SDL_EVENT_FINGER_UP: {
				/* normalised window coords -> output pixels -> unrotate -> canvas */
				float sx = e.tfinger.x * ow - (dst.x + dst.w / 2);
				float sy = e.tfinger.y * oh - (dst.y + dst.h / 2);
				double th = rotate * SDL_PI_D / 180;
				float cx = (float)(sx * cos(th) + sy * sin(th)) / fit + CW / 2;
				float cy = (float)(-sx * sin(th) + sy * cos(th)) / fit + CH / 2;
				int slot = -1;
				for (int i = 0; i < MAX_TOUCH; i++)
					if (touch[i].down && touch[i].id == e.tfinger.fingerID)
						slot = i;
				if (slot < 0)
					for (int i = 0; i < MAX_TOUCH; i++)
						if (!touch[i].down) {
							slot = i;
							break;
						}
				if (slot >= 0)
					touch[slot] = (Touch){ cx, cy, e.type != SDL_EVENT_FINGER_UP, e.tfinger.fingerID };
				break;
			}
			}
		}

		Uint64 now = SDL_GetTicks();
		if (gp && SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_START) && SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_BACK)) {
			if (!combo_since)
				combo_since = now;
			else if (now - combo_since > 700)
				run = false;
		} else
			combo_since = 0;
		if (seconds > 0 && now - t0 > seconds * 1000)
			run = false;

		/* ---- draw the canvas ---- */
		SDL_SetRenderTarget(ren, canvas);
		color(18, 20, 28);
		SDL_RenderClear(ren);

		/* bouncing square */
		bx += vx;
		by += vy;
		if (bx < 0 || bx > CW - 120) vx = -vx;
		if (by < 120 || by > CH - 120) vy = -vy;
		float hue = (now % 6000) / 6000.f * 6;
		float f = hue - (int)hue;
		Uint8 q = (Uint8)(255 * (1 - f)), t = (Uint8)(255 * f);
		Uint8 rgb[6][3] = { { 255, t, 0 }, { q, 255, 0 }, { 0, 255, t }, { 0, q, 255 }, { t, 0, 255 }, { 255, 0, q } };
		SDL_SetRenderDrawColor(ren, rgb[(int)hue][0] / 3, rgb[(int)hue][1] / 3, rgb[(int)hue][2] / 3, 255);
		box(bx, by, 120, 120, true);

		/* header */
		color(240, 240, 240);
		text(40, 30, 4, "hako SDL3 hello");
		color(150, 150, 170);
		text(40, 80, 2, "%s / %s  %dx%d rot %d  %d fps", SDL_GetCurrentVideoDriver(), SDL_GetRendererName(ren),
		     ow, oh, rotate, fps);
		text(40, 104, 2, "pad: %s   audio: %s", gp ? SDL_GetGamepadName(gp) : "(none)",
		     audio ? SDL_GetCurrentAudioDriver() : "(none)");
		text(40, 128, 2, "ABXY: beep");
		text(CW - 520, 30, 2, "quit: hold START + SELECT");

		/* shoulders and triggers */
		trigger(gp, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 60, 150, "ZL");
		trigger(gp, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, CW - 260, 150, "ZR");
		rect_button(gp, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, 60, 200, 200, 50, "L");
		rect_button(gp, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, CW - 260, 200, 200, 50, "R");

		/* d-pad */
		float dx = 230, dy = 400, s = 64;
		rect_button(gp, SDL_GAMEPAD_BUTTON_DPAD_UP, dx - s / 2, dy - s * 1.5f, s, s, "U");
		rect_button(gp, SDL_GAMEPAD_BUTTON_DPAD_DOWN, dx - s / 2, dy + s / 2, s, s, "D");
		rect_button(gp, SDL_GAMEPAD_BUTTON_DPAD_LEFT, dx - s * 1.5f, dy - s / 2, s, s, "L");
		rect_button(gp, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, dx + s / 2, dy - s / 2, s, s, "R");

		/* face buttons by position (labels come from SDL: Switch layout).
		 * SDL's SOUTH/EAST/WEST/NORTH are positions, except with
		 * HAKO_PAD_LAYOUT=label: hako-session swaps a<->b and x<->y so the
		 * names follow the printed letters, and then SOUTH (A) is on the right,
		 * EAST (B) at the bottom, WEST (X) on top and NORTH (Y) on the left. */
		float fx = CW - 230, fy = 400;
		const char *layout = getenv("HAKO_PAD_LAYOUT");
		bool by_label = layout && !strcmp(layout, "label");
		face_button(gp, by_label ? SDL_GAMEPAD_BUTTON_WEST : SDL_GAMEPAD_BUTTON_NORTH, fx, fy - 95);
		face_button(gp, by_label ? SDL_GAMEPAD_BUTTON_EAST : SDL_GAMEPAD_BUTTON_SOUTH, fx, fy + 95);
		face_button(gp, by_label ? SDL_GAMEPAD_BUTTON_NORTH : SDL_GAMEPAD_BUTTON_WEST, fx - 95, fy);
		face_button(gp, by_label ? SDL_GAMEPAD_BUTTON_SOUTH : SDL_GAMEPAD_BUTTON_EAST, fx + 95, fy);

		/* centre buttons */
		rect_button(gp, SDL_GAMEPAD_BUTTON_BACK, CW / 2 - 230, 420, 140, 50, "SELECT");
		rect_button(gp, SDL_GAMEPAD_BUTTON_GUIDE, CW / 2 - 70, 420, 140, 50, "HOME");
		rect_button(gp, SDL_GAMEPAD_BUTTON_START, CW / 2 + 90, 420, 140, 50, "START");

		/* sticks */
		stick(gp, SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY, SDL_GAMEPAD_BUTTON_LEFT_STICK, 420, 700);
		stick(gp, SDL_GAMEPAD_AXIS_RIGHTX, SDL_GAMEPAD_AXIS_RIGHTY, SDL_GAMEPAD_BUTTON_RIGHT_STICK, CW - 420, 700);

		/* touch points */
		for (int i = 0; i < MAX_TOUCH; i++) {
			if (!touch[i].down)
				continue;
			color(80, 255, 140);
			circle(touch[i].x, touch[i].y, 50, false);
			circle(touch[i].x, touch[i].y, 8, true);
			text(touch[i].x + 56, touch[i].y - 8, 2, "%.0f,%.0f", touch[i].x, touch[i].y);
		}

		if (shot && ++frame_no == 60) {
			SDL_Surface *sf = SDL_RenderReadPixels(ren, NULL);
			if (sf && SDL_SaveBMP(sf, shot))
				printf("saved %s\n", shot);
			else
				printf("shot failed: %s\n", SDL_GetError());
			fflush(stdout);
			SDL_DestroySurface(sf);
		}

		/* ---- present: rotate the canvas onto the panel ---- */
		SDL_SetRenderTarget(ren, NULL);
		color(0, 0, 0);
		SDL_RenderClear(ren);
		SDL_RenderTextureRotated(ren, canvas, NULL, &dst, rotate, NULL, SDL_FLIP_NONE);
		SDL_RenderPresent(ren);

		frames++;
		if (now - fps_t >= 1000) {
			fps = frames;
			frames = 0;
			fps_t = now;
		}
	}

	if (gp)
		SDL_CloseGamepad(gp);
	SDL_DestroyAudioStream(audio);
	SDL_DestroyTexture(canvas);
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	SDL_Quit();
	return 0;
}
