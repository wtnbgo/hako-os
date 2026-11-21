/*
 * hako-launcher: home screen of the hako game console.
 *
 * Shows the registered applications (see apps.c), starts the selected one
 * and comes back when it exits. While an app runs the launcher closes its
 * window and display (the app gets the screen and the GPU to itself) and only
 * waits; holding HOME sends the launcher SIGUSR1 (see home-button), which
 * stops the app (SIGTERM, SIGKILL after 3 s) and brings the launcher back.
 * SIGUSR2 asks to start the app named in $XDG_RUNTIME_DIR/hako-launch
 * (see launch-app; used by development tools after deploying an app).
 * The second tab holds the system settings: volume, brightness, language,
 * time zone, external monitor audio, Bluetooth (its own screen, see bt.c),
 * system information, reboot and power off.
 *
 * Drawn on a landscape 1280x960 canvas. Under the cage compositor of
 * hako-session the output is already landscape (HAKO_ROTATE=0); straight
 * on KMS/DRM the canvas is rotated onto the portrait panel. HAKO_ROTATE is
 * the output transform in wlr-randr terms (counter-clockwise), so the canvas
 * turns by 360 - HAKO_ROTATE degrees clockwise.
 *
 * Input (any connected gamepad, e.g. the built-in pad and a Bluetooth one):
 * D-pad / left stick / arrow keys move, A / Enter selects, B / Esc goes back,
 * X / Delete is the extra action of a screen, L / R / Tab switch tabs, touch taps.
 */
#include "launcher.h"

#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* colours */
#define C_BG       0x14161c
#define C_BAR      0x1d2029
#define C_TILE     0x232733
#define C_TILE_SEL 0x2e3445
#define C_ACCENT   0xffc83c
#define C_TEXT     0xeeeeee
#define C_DIM      0x9098a8
#define C_WARN     0xff7a6a
#define C_DEV      0xe07b2a

/* layout */
#define HEADER_H 88
#define FOOTER_Y 892
#define GRID_COLS 3
#define GRID_ROWS 3
#define TILE_W 380
#define TILE_H 232
#define TILE_GAP 30
#define GRID_X ((CW - GRID_COLS * TILE_W - (GRID_COLS - 1) * TILE_GAP) / 2)
#define GRID_Y 116
#define SET_X 140
#define SET_W 1000
#define SET_Y 120
#define SET_H 80
#define SET_TY ((SET_H - 10) / 2 - 21) /* text offset in a row */
#define BAR_X (SET_X + 380)
#define BAR_W 440

enum { TAB_APPS, TAB_SETTINGS, TAB_COUNT };
enum { S_VOLUME, S_BRIGHT, S_LANGUAGE, S_TIMEZONE, S_EXTAUDIO, S_BLUETOOTH, S_INFO, S_REBOOT, S_POWEROFF, S_COUNT };
/* rows of the Bluetooth screen before the devices */
enum { BT_ROW_POWER, BT_ROW_SCAN, BT_ROWS_FIXED };
#define BT_VISIBLE 8
#define BT_SCAN_MS 30000

/* time zones offered in the settings (names: ja, en) */
static const struct { const char *tz, *name[LANG_COUNT]; } TZ[] = {
	{ "Asia/Tokyo",          { "東京 (UTC+9)", "Tokyo (UTC+9)" } },
	{ "Asia/Seoul",          { "ソウル (UTC+9)", "Seoul (UTC+9)" } },
	{ "Asia/Shanghai",       { "上海 (UTC+8)", "Shanghai (UTC+8)" } },
	{ "Asia/Taipei",         { "台北 (UTC+8)", "Taipei (UTC+8)" } },
	{ "Asia/Singapore",      { "シンガポール (UTC+8)", "Singapore (UTC+8)" } },
	{ "Asia/Bangkok",        { "バンコク (UTC+7)", "Bangkok (UTC+7)" } },
	{ "Asia/Kolkata",        { "インド (UTC+5:30)", "India (UTC+5:30)" } },
	{ "Europe/Moscow",       { "モスクワ (UTC+3)", "Moscow (UTC+3)" } },
	{ "Europe/Berlin",       { "中央ヨーロッパ (UTC+1)", "Central Europe (UTC+1)" } },
	{ "Europe/London",       { "ロンドン (UTC+0)", "London (UTC+0)" } },
	{ "Etc/UTC",             { "UTC", "UTC" } },
	{ "America/Sao_Paulo",   { "サンパウロ (UTC-3)", "São Paulo (UTC-3)" } },
	{ "America/New_York",    { "米国東部 (UTC-5)", "US Eastern (UTC-5)" } },
	{ "America/Chicago",     { "米国中部 (UTC-6)", "US Central (UTC-6)" } },
	{ "America/Denver",      { "米国山岳部 (UTC-7)", "US Mountain (UTC-7)" } },
	{ "America/Los_Angeles", { "米国太平洋 (UTC-8)", "US Pacific (UTC-8)" } },
	{ "Pacific/Honolulu",    { "ハワイ (UTC-10)", "Hawaii (UTC-10)" } },
	{ "Australia/Sydney",    { "シドニー (UTC+10)", "Sydney (UTC+10)" } },
	{ "Pacific/Auckland",    { "ニュージーランド (UTC+12)", "New Zealand (UTC+12)" } },
};
#define TZ_COUNT ((int)SDL_arraysize(TZ))
enum { DLG_NONE, DLG_REBOOT, DLG_POWEROFF, DLG_BT_FORGET };
enum { A_NONE, A_UP, A_DOWN, A_LEFT, A_RIGHT, A_OK, A_BACK, A_TAB_L, A_TAB_R, A_OPT };

static const float TAB_X[TAB_COUNT] = { 330, 500 };

static volatile sig_atomic_t g_home, g_term, g_launch;
static void on_usr1(int sig) { (void)sig; g_home = 1; }
static void on_usr2(int sig) { (void)sig; g_launch = 1; }

/* Launch request from outside (see launch-app): the app id is in
 * $XDG_RUNTIME_DIR/hako-launch and SIGUSR2 tells us to look at it. */
static bool take_launch_request(char *id, size_t size)
{
	const char *rt = getenv("XDG_RUNTIME_DIR");
	char path[512];
	snprintf(path, sizeof path, "%s/hako-launch", rt ? rt : "/tmp");
	FILE *f = fopen(path, "r");
	if (!f)
		return false;
	bool ok = fgets(id, (int)size, f) != NULL;
	fclose(f);
	unlink(path);
	if (ok)
		id[strcspn(id, "\r\n")] = 0;
	return ok && id[0];
}
static void on_term(int sig) { (void)sig; g_term = 1; }

typedef struct {
	SDL_Window *win;
	SDL_Renderer *ren;
	SDL_Texture *canvas;
	int ow, oh, rot; /* output size, canvas rotation (clockwise) */
	float fit;
	SDL_FRect dst;
} Screen;

static void screen_fit(Screen *s);

typedef struct {
	App apps[MAX_APPS];
	SDL_Texture *icon[MAX_APPS];
	int napps;
	int tab, sel_app, sel_set, top_row;
	char tz[64];      /* current time zone */
	bool ext_audio_device; /* external monitor audio stays on the device */
	bool info;
	bool bt_screen;       /* the Bluetooth screen is open */
	BtState bt;
	int bt_sel, bt_top;
	Uint64 bt_next;       /* next refresh of the device list */
	Uint64 scan_until;    /* searching stops then */
	BtDevice bt_target;   /* device of the "forget" dialog */
	char info_lines[40][160];
	int ninfo;
	int dialog;
	char toast[256];
	Uint64 toast_until;
	SysState sys;
	Uint64 sys_next;
	SDL_Gamepad *gp[8];   /* every connected pad drives the launcher */
	int rep_act;          /* held direction, for auto repeat */
	Uint64 rep_next;
	int stick_x, stick_y; /* left stick as a D-pad: -1, 0, 1 */
	bool touch_down;
	SDL_FingerID touch_id;
	float touch_x, touch_y;
	bool dirty;
} Launcher;

static Ui ui;

/* ---------------------------------------------------------------- sound */

/* Opened on the first beep and closed while an app runs. */
static SDL_AudioStream *audio;

static void sound_close(void)
{
	if (!audio)
		return;
	SDL_DestroyAudioStream(audio);
	audio = NULL;
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

/* short tone at the current output volume, so the volume setting can be heard */
static void beep(void)
{
	enum { RATE = 48000, LEN = RATE * 3 / 20 };
	static float buf[LEN];
	if (!audio) {
		SDL_AudioSpec spec = { SDL_AUDIO_F32, 1, RATE };
		if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
			return;
		audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
		if (!audio) {
			SDL_Log("audio: %s", SDL_GetError());
			SDL_QuitSubSystem(SDL_INIT_AUDIO);
			return;
		}
		SDL_ResumeAudioStreamDevice(audio);
	}
	for (int i = 0; i < LEN; i++) {
		float env = SDL_min(1.0f, i / 240.0f) * (1.0f - (float)i / LEN);
		buf[i] = 0.4f * env * SDL_sinf(2 * SDL_PI_F * 880.0f * i / RATE);
	}
	SDL_ClearAudioStream(audio);
	SDL_PutAudioStreamData(audio, buf, sizeof buf);
}

/* ---------------------------------------------------------------- screen */

static bool screen_open(Screen *s, int rot)
{
	memset(s, 0, sizeof *s);
	s->rot = rot;
	if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
		SDL_Log("SDL video: %s", SDL_GetError());
		return false;
	}
	if (!SDL_CreateWindowAndRenderer("hako-launcher", CW, CH, SDL_WINDOW_FULLSCREEN, &s->win, &s->ren)) {
		SDL_Log("window: %s", SDL_GetError());
		return false;
	}
	SDL_SetRenderVSync(s->ren, 1);
	SDL_HideCursor();
	s->canvas = SDL_CreateTexture(s->ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, CW, CH);
	if (!s->canvas) {
		SDL_Log("canvas: %s", SDL_GetError());
		return false;
	}
	screen_fit(s);
	SDL_Log("video=%s renderer=%s output=%dx%d rotate=%d", SDL_GetCurrentVideoDriver(),
	        SDL_GetRendererName(s->ren), s->ow, s->oh, rot);
	return ui_attach(&ui, s->ren);
}

static void screen_close(Screen *s)
{
	ui_detach(&ui);
	if (s->canvas)
		SDL_DestroyTexture(s->canvas);
	if (s->ren)
		SDL_DestroyRenderer(s->ren);
	if (s->win)
		SDL_DestroyWindow(s->win);
	memset(s, 0, sizeof *s);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

/* normalised window coordinates -> canvas */
/* Fit the (rotated) canvas into the output. Called again when the output
 * changes size, e.g. when an external monitor takes over (cage -m last). */
static void screen_fit(Screen *s)
{
	SDL_GetRenderOutputSize(s->ren, &s->ow, &s->oh);
	bool sideways = s->rot == 90 || s->rot == 270;
	float cw = sideways ? CH : CW, ch = sideways ? CW : CH;
	s->fit = SDL_min(s->ow / cw, s->oh / ch);
	s->dst = (SDL_FRect){ (s->ow - CW * s->fit) / 2, (s->oh - CH * s->fit) / 2, CW * s->fit, CH * s->fit };
}

static void to_canvas(const Screen *s, float nx, float ny, float *cx, float *cy)
{
	float sx = nx * s->ow - (s->dst.x + s->dst.w / 2);
	float sy = ny * s->oh - (s->dst.y + s->dst.h / 2);
	double th = s->rot * SDL_PI_D / 180;
	*cx = (float)(sx * cos(th) + sy * sin(th)) / s->fit + CW / 2.0f;
	*cy = (float)(-sx * sin(th) + sy * cos(th)) / s->fit + CH / 2.0f;
}

/* ---------------------------------------------------------------- apps */

static void icons_free(Launcher *l)
{
	for (int i = 0; i < MAX_APPS; i++)
		if (l->icon[i]) {
			SDL_DestroyTexture(l->icon[i]);
			l->icon[i] = NULL;
		}
}

static void icons_load(Launcher *l, SDL_Renderer *ren)
{
	icons_free(l);
	for (int i = 0; i < l->napps; i++)
		if (l->apps[i].icon[0] && !(l->icon[i] = IMG_LoadTexture(ren, l->apps[i].icon)))
			SDL_Log("icon %s: %s", l->apps[i].icon, SDL_GetError());
}

/* reload the app list, keeping the selection on the same app */
static void apps_reload(Launcher *l)
{
	char cur[64] = "";
	if (l->sel_app < l->napps)
		snprintf(cur, sizeof cur, "%s", l->apps[l->sel_app].id);
	l->napps = apps_load(l->apps, MAX_APPS);
	l->sel_app = 0;
	for (int i = 0; i < l->napps; i++)
		if (!strcmp(l->apps[i].id, cur))
			l->sel_app = i;
}

static void toast(Launcher *l, const char *fmt, ...) SDL_PRINTF_VARARG_FUNC(2);
static void toast(Launcher *l, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(l->toast, sizeof l->toast, fmt, ap);
	va_end(ap);
	l->toast_until = SDL_GetTicks() + 4000;
	l->dirty = true;
}

/* Run the app and wait for it. The launcher gives up the display meanwhile. */
static void run_app(Launcher *l, Screen *s, int idx)
{
	App a = l->apps[idx];
	int rot = s->rot;
	SDL_Log("start %s: %s %s", a.id, a.exec, a.args);

	icons_free(l);
	sound_close();
	screen_close(s);
	Uint64 t0 = SDL_GetTicks();
	pid_t pid = app_spawn(&a);
	int status = 0;
	bool stopped = false;
	if (pid > 0) {
		Uint64 kill_at = 0;
		g_home = 0;
		for (;;) {
			pid_t r = waitpid(pid, &status, WNOHANG);
			if (r == pid || (r < 0 && errno != EINTR))
				break;
			if ((g_home || g_term || g_launch) && !stopped) {
				SDL_Log("stopping %s", a.id);
				kill(-pid, SIGTERM);
				stopped = true;
				kill_at = SDL_GetTicks() + 3000;
			} else if (kill_at && SDL_GetTicks() > kill_at) {
				SDL_Log("killing %s", a.id);
				kill(-pid, SIGKILL);
				kill_at = 0;
			}
			g_home = 0;
			SDL_PumpEvents();
			SDL_Delay(50);
		}
	}
	if (g_term)
		return;

	if (!screen_open(s, rot)) {
		SDL_Log("cannot reopen the display");
		g_term = 1;
		return;
	}
	apps_reload(l);
	icons_load(l, s->ren);
	SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
	l->rep_act = A_NONE;
	l->stick_x = l->stick_y = 0;
	l->touch_down = false;
	l->sys_next = 0;
	l->dirty = true;

	Uint64 secs = (SDL_GetTicks() - t0) / 1000;
	if (pid < 0 || (WIFEXITED(status) && WEXITSTATUS(status) == 127 && secs < 3))
		toast(l, T(T_START_FAILED), a.name);
	else if (stopped)
		toast(l, T(T_CLOSED), a.name);
	else if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
		toast(l, T(T_EXIT_ERROR), a.name, WEXITSTATUS(status));
	else if (WIFSIGNALED(status))
		toast(l, T(T_CRASHED), a.name, WTERMSIG(status));
	SDL_Log("%s ended after %llu s (status 0x%x)", a.id, (unsigned long long)secs, status);
}

/* ---------------------------------------------------------------- actions */

static void set_language(Launcher *l, Lang lang)
{
	g_lang = lang;
	lang_save();
	lang_export();
	apps_reload(l); /* names in the new language (the order may change) */
	if (ui.ren)
		icons_load(l, ui.ren);
	l->dirty = true;
}

/* "External monitor audio": ~/.config/hako/external-audio = monitor | device,
 * applied by /usr/lib/hako/audio-route (also run after a display hotplug). */
static void ext_audio_load(Launcher *l)
{
	char path[600], buf[16] = "";
	hako_config_path("external-audio", path, sizeof path, false);
	FILE *f = fopen(path, "r");
	if (f) {
		if (!fgets(buf, sizeof buf, f))
			buf[0] = 0;
		fclose(f);
	}
	l->ext_audio_device = !strncmp(buf, "device", 6);
}

static void set_ext_audio(Launcher *l, bool device)
{
	char path[600];
	hako_config_path("external-audio", path, sizeof path, true);
	FILE *f = fopen(path, "w");
	if (!f) {
		SDL_Log("save %s: %s", path, strerror(errno));
		return;
	}
	fprintf(f, "%s\n", device ? "device" : "monitor");
	fclose(f);
	l->ext_audio_device = device;
	/* apply now, without waiting: double fork so nothing is left to reap */
	pid_t pid = fork();
	if (pid == 0) {
		if (fork() == 0) {
			execl("/usr/lib/hako/audio-route", "audio-route", (char *)NULL);
			_exit(127);
		}
		_exit(0);
	}
	if (pid > 0)
		waitpid(pid, NULL, 0);
	l->dirty = true;
}

static int tz_index(const char *tz)
{
	for (int i = 0; i < TZ_COUNT; i++)
		if (!strcmp(TZ[i].tz, tz))
			return i;
	return -1;
}

static void step_timezone(Launcher *l, int dir)
{
	int i = tz_index(l->tz);
	i = i < 0 ? 0 : (i + TZ_COUNT + dir) % TZ_COUNT;
	if (sys_set_timezone(TZ[i].tz))
		snprintf(l->tz, sizeof l->tz, "%s", TZ[i].tz);
	else
		toast(l, "%s", T(T_TZ_FAILED));
	l->dirty = true;
}

static void settings_adjust(Launcher *l, int dir)
{
	SysState *s = &l->sys;
	if (l->sel_set == S_LANGUAGE)
		set_language(l, (Lang)((g_lang + LANG_COUNT + dir) % LANG_COUNT));
	if (l->sel_set == S_TIMEZONE)
		step_timezone(l, dir);
	if (l->sel_set == S_EXTAUDIO)
		set_ext_audio(l, !l->ext_audio_device);
	if (l->sel_set == S_VOLUME && s->volume >= 0) {
		sys_set_volume(s, roundf((s->volume + dir * 0.05f) * 20) / 20);
		beep();
	}
	else if (l->sel_set == S_BRIGHT && s->bright_max)
		sys_set_brightness(s, s->bright + dir * SDL_max(1, s->bright_max / 20));
	l->dirty = true;
}

static void settings_activate(Launcher *l)
{
	switch (l->sel_set) {
	case S_LANGUAGE:
		set_language(l, (Lang)((g_lang + 1) % LANG_COUNT));
		break;
	case S_TIMEZONE:
		step_timezone(l, 1);
		break;
	case S_EXTAUDIO:
		set_ext_audio(l, !l->ext_audio_device);
		break;
	case S_BLUETOOTH:
		l->bt_screen = true;
		l->bt_sel = l->bt_top = 0;
		bt_refresh(&l->bt);
		break;
	case S_INFO:
		l->ninfo = sys_info(l->info_lines, SDL_arraysize(l->info_lines));
		l->info = true;
		break;
	case S_REBOOT:
		l->dialog = DLG_REBOOT;
		break;
	case S_POWEROFF:
		l->dialog = DLG_POWEROFF;
		break;
	}
}

/* ---------------------------------------------------------------- bluetooth */

static int bt_rows(const Launcher *l)
{
	return BT_ROWS_FIXED + l->bt.ndev;
}

static void bt_leave(Launcher *l)
{
	if (l->bt.discovering || l->scan_until)
		bt_scan(false);
	l->scan_until = 0;
	l->bt_screen = false;
}

static void bt_select_row(Launcher *l, int row)
{
	l->bt_sel = SDL_clamp(row, 0, bt_rows(l) - 1);
	if (l->bt_sel < l->bt_top)
		l->bt_top = l->bt_sel;
	else if (l->bt_sel >= l->bt_top + BT_VISIBLE)
		l->bt_top = l->bt_sel - BT_VISIBLE + 1;
}

static void bt_activate_row(Launcher *l)
{
	if (!l->bt.available)
		return;
	if (l->bt_sel == BT_ROW_POWER)
		bt_set_powered(!l->bt.powered);
	else if (l->bt_sel == BT_ROW_SCAN) {
		if (!l->bt.powered)
			bt_set_powered(true);
		bool on = !(l->bt.discovering || l->scan_until);
		bt_scan(on);
		l->scan_until = on ? SDL_GetTicks() + BT_SCAN_MS : 0;
	} else {
		int i = l->bt_sel - BT_ROWS_FIXED;
		if (i >= 0 && i < l->bt.ndev && l->bt.dev[i].busy == BT_IDLE)
			bt_activate(&l->bt.dev[i]);
	}
	l->bt_next = 0; /* refresh now */
}

static void bt_act(Launcher *l, int a)
{
	switch (a) {
	case A_UP:   bt_select_row(l, l->bt_sel - 1); break;
	case A_DOWN: bt_select_row(l, l->bt_sel + 1); break;
	case A_OK:   bt_activate_row(l); break;
	case A_BACK: bt_leave(l); break;
	case A_OPT: {
		int i = l->bt_sel - BT_ROWS_FIXED;
		if (i >= 0 && i < l->bt.ndev && l->bt.dev[i].paired) {
			l->bt_target = l->bt.dev[i];
			l->dialog = DLG_BT_FORGET;
		}
		break;
	}
	}
}

static void act(Launcher *l, Screen *s, int a)
{
	l->dirty = true;
	if (l->dialog) {
		if (a == A_OK && l->dialog == DLG_BT_FORGET) {
			l->dialog = DLG_NONE;
			bt_forget(&l->bt_target);
			l->bt_next = 0;
		} else if (a == A_OK) {
			bool reboot = l->dialog == DLG_REBOOT;
			l->dialog = DLG_NONE;
			if (!sys_power(reboot))
				toast(l, "%s", T(reboot ? T_REBOOT_FAILED : T_POWER_FAILED));
		} else if (a == A_BACK)
			l->dialog = DLG_NONE;
		return;
	}
	if (l->bt_screen) {
		if (a == A_TAB_L || a == A_TAB_R)
			bt_leave(l);
		else {
			bt_act(l, a);
			return;
		}
	}
	if (l->info) {
		if (a == A_BACK || a == A_OK)
			l->info = false;
		return;
	}
	if (a == A_TAB_L || a == A_TAB_R) {
		l->tab = (l->tab + (a == A_TAB_R ? 1 : TAB_COUNT - 1)) % TAB_COUNT;
		return;
	}
	if (l->tab == TAB_APPS) {
		int n = l->napps, i = l->sel_app;
		switch (a) {
		case A_LEFT:  if (i > 0) i--; break;
		case A_RIGHT: if (i + 1 < n) i++; break;
		case A_UP:    if (i >= GRID_COLS) i -= GRID_COLS; break;
		case A_DOWN:  if (i + GRID_COLS < n) i += GRID_COLS; else if (i / GRID_COLS < (n - 1) / GRID_COLS) i = n - 1; break;
		case A_OK:    if (n) run_app(l, s, i); return;
		}
		l->sel_app = i;
		int row = i / GRID_COLS;
		if (row < l->top_row)
			l->top_row = row;
		else if (row >= l->top_row + GRID_ROWS)
			l->top_row = row - GRID_ROWS + 1;
	} else {
		switch (a) {
		case A_UP:    if (l->sel_set > 0) l->sel_set--; break;
		case A_DOWN:  if (l->sel_set + 1 < S_COUNT) l->sel_set++; break;
		case A_LEFT:  settings_adjust(l, -1); break;
		case A_RIGHT: settings_adjust(l, +1); break;
		case A_OK:    settings_activate(l); break;
		}
	}
}

static bool hit(float x, float y, float rx, float ry, float rw, float rh)
{
	return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

/* dialog buttons (also drawn in draw_dialog) */
#define DLG_YES_X (CW / 2 - 260)
#define DLG_NO_X  (CW / 2 + 20)
#define DLG_BTN_Y 520
#define DLG_BTN_W 240
#define DLG_BTN_H 80
#define BT_LIST_Y (HEADER_H + 90)

static void tap(Launcher *l, Screen *s, float x, float y)
{
	l->dirty = true;
	if (l->dialog) {
		if (hit(x, y, DLG_YES_X, DLG_BTN_Y, DLG_BTN_W, DLG_BTN_H))
			act(l, s, A_OK);
		else if (hit(x, y, DLG_NO_X, DLG_BTN_Y, DLG_BTN_W, DLG_BTN_H))
			act(l, s, A_BACK);
		return;
	}
	if (l->info) {
		l->info = false;
		return;
	}
	if (l->bt_screen) {
		if (y < HEADER_H) {
			bt_leave(l);
			return;
		}
		for (int r = 0; r < BT_VISIBLE && l->bt_top + r < bt_rows(l); r++)
			if (hit(x, y, SET_X, BT_LIST_Y + r * SET_H, SET_W, SET_H - 10)) {
				bt_select_row(l, l->bt_top + r);
				bt_activate_row(l);
				return;
			}
		return;
	}
	for (int t = 0; t < TAB_COUNT; t++)
		if (hit(x, y, TAB_X[t] - 20, 0, 170, HEADER_H)) {
			l->tab = t;
			return;
		}
	if (l->tab == TAB_APPS) {
		for (int i = 0; i < l->napps; i++) {
			int r = i / GRID_COLS - l->top_row, c = i % GRID_COLS;
			if (r < 0 || r >= GRID_ROWS)
				continue;
			if (hit(x, y, GRID_X + c * (TILE_W + TILE_GAP), GRID_Y + r * (TILE_H + TILE_GAP), TILE_W, TILE_H)) {
				l->sel_app = i;
				run_app(l, s, i);
				return;
			}
		}
	} else {
		for (int i = 0; i < S_COUNT; i++) {
			if (!hit(x, y, SET_X, SET_Y + i * SET_H, SET_W, SET_H - 10))
				continue;
			l->sel_set = i;
			float f = (x - BAR_X) / BAR_W;
			if (i == S_VOLUME && f >= -0.05f && f <= 1.05f && l->sys.volume >= 0) {
				sys_set_volume(&l->sys, roundf(SDL_clamp(f, 0, 1) * 20) / 20);
				beep();
			} else if (i == S_BRIGHT && f >= -0.05f && f <= 1.05f && l->sys.bright_max)
				sys_set_brightness(&l->sys, (int)(SDL_clamp(f, 0, 1) * l->sys.bright_max + 0.5f));
			else if (i >= S_LANGUAGE)
				settings_activate(l);
			return;
		}
	}
}

/* ---------------------------------------------------------------- drawing */

static void hint(float *x, const char *key, const char *text)
{
	float kw = SDL_max(40, ui_text_width(&ui, ui.small, key) + 16);
	ui_fill(&ui, *x, FOOTER_Y + 18, kw, 40, 0x3a4152);
	ui_text(&ui, ui.small, *x + kw / 2, FOOTER_Y + 22, C_TEXT, ALIGN_CENTER, 0, "%s", key);
	*x += kw + 12;
	*x += ui_text(&ui, ui.small, *x, FOOTER_Y + 22, C_DIM, ALIGN_LEFT, 0, "%s", text) + 36;
}

static void draw_header(Launcher *l)
{
	ui_fill(&ui, 0, 0, CW, HEADER_H, C_BAR);
	ui_text(&ui, ui.title, 40, 16, C_ACCENT, ALIGN_LEFT, 0, "hako");
	for (int t = 0; t < TAB_COUNT; t++) {
		bool on = l->tab == t && !l->info && !l->bt_screen;
		float w = ui_text(&ui, ui.big, TAB_X[t], 20, on ? C_TEXT : C_DIM, ALIGN_LEFT, 0, "%s",
		                  T(t == TAB_APPS ? T_TAB_APPS : T_TAB_SETTINGS));
		if (on)
			ui_fill(&ui, TAB_X[t], HEADER_H - 8, w, 5, C_ACCENT);
	}
	time_t now = time(NULL);
	struct tm tm;
	localtime_r(&now, &tm);
	float x = CW - 40;
	x -= ui_text(&ui, ui.big, x, 20, C_TEXT, ALIGN_RIGHT, 0, "%02d:%02d", tm.tm_hour, tm.tm_min) + 30;
	char buf[96];
	if (l->sys.battery >= 0) {
		snprintf(buf, sizeof buf, T(T_BATTERY), l->sys.battery);
		x -= ui_text(&ui, ui.mid, x, 26, l->sys.battery <= 15 ? C_WARN : C_DIM, ALIGN_RIGHT, 0, "%s%s", buf,
		             !strcmp(l->sys.battery_status, "Charging") ? T(T_CHARGING) : "") + 30;
	}
	if (l->sys.volume >= 0) {
		snprintf(buf, sizeof buf, T(T_VOLUME_HDR), (int)roundf(l->sys.volume * 100));
		ui_text(&ui, ui.mid, x, 26, C_DIM, ALIGN_RIGHT, 0, "%s%s", buf, l->sys.muted ? T(T_MUTED) : "");
	}
}

static Uint32 tile_color(const char *id)
{
	static const Uint32 pal[] = { 0x3d6fb4, 0x3f9b6b, 0xb4583d, 0x8a4fb0, 0xb08a2e, 0x2e8fa0 };
	Uint32 h = 5381;
	for (const char *p = id; *p; p++)
		h = h * 33 + (unsigned char)*p;
	return pal[h % SDL_arraysize(pal)];
}

static void draw_apps(Launcher *l)
{
	if (!l->napps) {
		ui_text(&ui, ui.big, CW / 2, 360, C_TEXT, ALIGN_CENTER, 0, "%s", T(T_NO_APPS));
		ui_text(&ui, ui.small, CW / 2, 430, C_DIM, ALIGN_CENTER, 0, "%s", T(T_NO_APPS_HINT));
		return;
	}
	for (int i = 0; i < l->napps; i++) {
		int r = i / GRID_COLS - l->top_row, c = i % GRID_COLS;
		if (r < 0 || r >= GRID_ROWS)
			continue;
		const App *a = &l->apps[i];
		float x = GRID_X + c * (TILE_W + TILE_GAP), y = GRID_Y + r * (TILE_H + TILE_GAP);
		bool sel = i == l->sel_app;
		ui_fill(&ui, x, y, TILE_W, TILE_H, sel ? C_TILE_SEL : C_TILE);
		if (sel)
			ui_frame(&ui, x - 4, y - 4, TILE_W + 8, TILE_H + 8, 5, C_ACCENT);

		SDL_FRect ic = { x + (TILE_W - 112) / 2.0f, y + 18, 112, 112 };
		if (l->icon[i])
			SDL_RenderTexture(ui.ren, l->icon[i], NULL, &ic);
		else {
			ui_fill(&ui, ic.x, ic.y, ic.w, ic.h, tile_color(a->id));
			/* first character of the name */
			char first[8] = "";
			size_t n = 1;
			while (a->name[n] && ((unsigned char)a->name[n] & 0xc0) == 0x80 && n < 6)
				n++;
			memcpy(first, a->name, n);
			first[n] = 0;
			ui_text(&ui, ui.title, ic.x + ic.w / 2, ic.y + 28, 0xffffff, ALIGN_CENTER, 0, "%s", first);
		}
		ui_text(&ui, ui.mid, x + TILE_W / 2.0f, y + 140, C_TEXT, ALIGN_CENTER, TILE_W - 30, "%s", a->name);
		if (a->desc[0])
			ui_text(&ui, ui.small, x + TILE_W / 2.0f, y + 184, C_DIM, ALIGN_CENTER, TILE_W - 30, "%s", a->desc);
		if (a->dev) {
			ui_fill(&ui, x + TILE_W - 74, y + 10, 64, 32, C_DEV);
			ui_text(&ui, ui.small, x + TILE_W - 42, y + 12, 0xffffff, ALIGN_CENTER, 0, "%s", T(T_DEV));
		}
	}
	int rows = (l->napps + GRID_COLS - 1) / GRID_COLS;
	if (rows > GRID_ROWS)
		ui_text(&ui, ui.small, CW - 40, FOOTER_Y - 34, C_DIM, ALIGN_RIGHT, 0, "%d / %d%s%s", l->sel_app + 1,
		        l->napps, l->top_row > 0 ? "  ▲" : "", l->top_row + GRID_ROWS < rows ? "  ▼" : "");
}

static void slider(float y, float v, const char *value)
{
	ui_fill(&ui, BAR_X, y + SET_TY + 14, BAR_W, 14, 0x3a4152);
	ui_fill(&ui, BAR_X, y + SET_TY + 14, BAR_W * SDL_clamp(v, 0, 1), 14, C_ACCENT);
	ui_text(&ui, ui.mid, SET_X + SET_W - 30, y + SET_TY, C_TEXT, ALIGN_RIGHT, 0, "%s", value);
}

static void draw_settings(Launcher *l)
{
	static const TextId name[S_COUNT] = { T_S_VOLUME, T_S_BRIGHT, T_S_LANGUAGE, T_S_TIMEZONE,
	                                      T_S_EXTAUDIO, T_S_BLUETOOTH, T_S_INFO, T_S_REBOOT, T_S_POWEROFF };
	for (int i = 0; i < S_COUNT; i++) {
		float y = SET_Y + i * SET_H;
		bool sel = i == l->sel_set;
		ui_fill(&ui, SET_X, y, SET_W, SET_H - 10, sel ? C_TILE_SEL : C_TILE);
		if (sel)
			ui_fill(&ui, SET_X, y, 8, SET_H - 10, C_ACCENT);
		ui_text(&ui, ui.mid, SET_X + 40, y + SET_TY, i == S_POWEROFF ? C_WARN : C_TEXT, ALIGN_LEFT, 0, "%s", T(name[i]));
		char buf[64];
		if (i == S_VOLUME) {
			if (l->sys.volume < 0)
				ui_text(&ui, ui.mid, SET_X + SET_W - 30, y + SET_TY, C_DIM, ALIGN_RIGHT, 0, "%s", T(T_UNAVAILABLE));
			else {
				snprintf(buf, sizeof buf, "%d%%%s", (int)roundf(l->sys.volume * 100), l->sys.muted ? T(T_MUTED) : "");
				slider(y, l->sys.volume, buf);
			}
		} else if (i == S_BRIGHT) {
			if (!l->sys.bright_max)
				ui_text(&ui, ui.mid, SET_X + SET_W - 30, y + SET_TY, C_DIM, ALIGN_RIGHT, 0, "%s", T(T_NOT_ADJUSTABLE));
			else {
				float f = (float)l->sys.bright / l->sys.bright_max;
				snprintf(buf, sizeof buf, "%d%%", (int)roundf(f * 100));
				slider(y, f, buf);
			}
		} else if (i == S_LANGUAGE)
			ui_text(&ui, ui.mid, SET_X + SET_W - 30, y + SET_TY, C_TEXT, ALIGN_RIGHT, 0, "‹  %s  ›", lang_name(g_lang));
		else if (i == S_TIMEZONE) {
			int t = tz_index(l->tz);
			ui_text(&ui, ui.mid, SET_X + SET_W - 30, y + SET_TY, C_TEXT, ALIGN_RIGHT, 0, "‹  %s  ›",
			        t >= 0 ? TZ[t].name[g_lang] : l->tz);
		} else if (i == S_EXTAUDIO)
			ui_text(&ui, ui.mid, SET_X + SET_W - 30, y + SET_TY, C_TEXT, ALIGN_RIGHT, 0, "‹  %s  ›",
			        T(l->ext_audio_device ? T_EXT_DEVICE : T_EXT_MONITOR));
		else if (i == S_BLUETOOTH)
			ui_text(&ui, ui.mid, SET_X + SET_W - 40, y + SET_TY, C_DIM, ALIGN_RIGHT, 0, "%s   ›",
			        !l->bt.available ? "" : T(l->bt.powered ? T_BT_ON : T_BT_OFF));
		else
			ui_text(&ui, ui.mid, SET_X + SET_W - 40, y + SET_TY, C_DIM, ALIGN_RIGHT, 0, "›");
	}
	ui_text(&ui, ui.small, SET_X, SET_Y + S_COUNT * SET_H + 10, C_DIM, ALIGN_LEFT, 0, "%s", T(T_VOLUME_NOTE));
}

static const char *bt_state_text(const BtDevice *d)
{
	switch (d->busy) {
	case BT_PAIRING:       return T(T_BT_STATE_PAIRING);
	case BT_CONNECTING:    return T(T_BT_STATE_CONNECTING);
	case BT_DISCONNECTING: return T(T_BT_STATE_DISCONNECTING);
	default: break;
	}
	return T(d->connected ? T_BT_STATE_CONNECTED : d->paired ? T_BT_STATE_PAIRED : T_BT_STATE_NEW);
}

static void draw_bt(Launcher *l)
{
	ui_text(&ui, ui.big, SET_X, HEADER_H + 22, C_TEXT, ALIGN_LEFT, 0, "%s", T(T_S_BLUETOOTH));
	if (!l->bt.available) {
		ui_text(&ui, ui.mid, SET_X, BT_LIST_Y + 20, C_DIM, ALIGN_LEFT, 0, "%s", T(T_BT_UNAVAILABLE));
		return;
	}
	bool scanning = l->bt.discovering || l->scan_until;
	for (int r = 0; r < BT_VISIBLE && l->bt_top + r < bt_rows(l); r++) {
		int row = l->bt_top + r;
		float y = BT_LIST_Y + r * SET_H;
		bool sel = row == l->bt_sel;
		ui_fill(&ui, SET_X, y, SET_W, SET_H - 10, sel ? C_TILE_SEL : C_TILE);
		if (sel)
			ui_fill(&ui, SET_X, y, 8, SET_H - 10, C_ACCENT);
		if (row == BT_ROW_POWER) {
			ui_text(&ui, ui.mid, SET_X + 40, y + SET_TY, C_TEXT, ALIGN_LEFT, 0, "%s", T(T_S_BLUETOOTH));
			ui_text(&ui, ui.mid, SET_X + SET_W - 30, y + SET_TY, l->bt.powered ? C_ACCENT : C_DIM, ALIGN_RIGHT, 0,
			        "%s", T(l->bt.powered ? T_BT_ON : T_BT_OFF));
		} else if (row == BT_ROW_SCAN) {
			ui_text(&ui, ui.mid, SET_X + 40, y + SET_TY, C_TEXT, ALIGN_LEFT, 0, "%s", T(T_BT_SCAN));
			if (scanning)
				ui_text(&ui, ui.mid, SET_X + SET_W - 30, y + SET_TY, C_ACCENT, ALIGN_RIGHT, 0, "%s", T(T_BT_SCANNING));
		} else {
			const BtDevice *d = &l->bt.dev[row - BT_ROWS_FIXED];
			float w = ui_text(&ui, ui.mid, SET_X + SET_W - 30, y + SET_TY, d->connected ? C_ACCENT : C_DIM,
			                  ALIGN_RIGHT, 0, "%s", bt_state_text(d));
			ui_text(&ui, ui.mid, SET_X + 40, y + SET_TY, C_TEXT, ALIGN_LEFT, SET_W - w - 110, "%s", d->name);
		}
	}
	int rows = bt_rows(l);
	float note_y = BT_LIST_Y + SDL_min(rows - l->bt_top, BT_VISIBLE) * SET_H + 6;
	if (l->bt.ndev == 0)
		ui_text(&ui, ui.small, SET_X, note_y, C_DIM, ALIGN_LEFT, 0, "%s  %s", T(T_BT_NONE), T(T_BT_PAIRING_HINT));
	else if (rows > BT_VISIBLE)
		ui_text(&ui, ui.small, SET_X + SET_W, HEADER_H + 34, C_DIM, ALIGN_RIGHT, 0, "%s%s",
		        l->bt_top > 0 ? "▲ " : "", l->bt_top + BT_VISIBLE < rows ? "▼" : "");
	else
		ui_text(&ui, ui.small, SET_X, note_y, C_DIM, ALIGN_LEFT, 0, "%s", T(T_BT_PAIRING_HINT));
}

static void draw_info(Launcher *l)
{
	ui_text(&ui, ui.big, SET_X, HEADER_H + 26, C_TEXT, ALIGN_LEFT, 0, "%s", T(T_S_INFO));
	for (int i = 0; i < l->ninfo; i++)
		ui_text(&ui, ui.small, SET_X, HEADER_H + 90 + i * 36, C_TEXT, ALIGN_LEFT, CW - 2 * SET_X, "%s",
		        l->info_lines[i]);
}

static void draw_dialog(Launcher *l)
{
	SDL_SetRenderDrawBlendMode(ui.ren, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(ui.ren, 0, 0, 0, 170);
	SDL_FRect all = { 0, 0, CW, CH };
	SDL_RenderFillRect(ui.ren, &all);
	SDL_SetRenderDrawBlendMode(ui.ren, SDL_BLENDMODE_NONE);
	ui_fill(&ui, CW / 2 - 340, 300, 680, 340, C_BAR);
	ui_frame(&ui, CW / 2 - 340, 300, 680, 340, 3, C_ACCENT);
	if (l->dialog == DLG_BT_FORGET)
		ui_text(&ui, ui.big, CW / 2, 380, C_TEXT, ALIGN_CENTER, 600, T(T_ASK_BT_FORGET), l->bt_target.name);
	else
		ui_text(&ui, ui.big, CW / 2, 380, C_TEXT, ALIGN_CENTER, 0, "%s",
		        T(l->dialog == DLG_REBOOT ? T_ASK_REBOOT : T_ASK_POWEROFF));
	ui_fill(&ui, DLG_YES_X, DLG_BTN_Y, DLG_BTN_W, DLG_BTN_H, C_TILE_SEL);
	ui_fill(&ui, DLG_NO_X, DLG_BTN_Y, DLG_BTN_W, DLG_BTN_H, C_TILE);
	ui_text(&ui, ui.mid, DLG_YES_X + DLG_BTN_W / 2, DLG_BTN_Y + 20, C_TEXT, ALIGN_CENTER, 0, "A  %s", T(T_YES));
	ui_text(&ui, ui.mid, DLG_NO_X + DLG_BTN_W / 2, DLG_BTN_Y + 20, C_TEXT, ALIGN_CENTER, 0, "B  %s", T(T_NO));
}

static void draw(Launcher *l, Screen *s)
{
	SDL_SetRenderTarget(s->ren, s->canvas);
	ui_fill(&ui, 0, 0, CW, CH, C_BG);
	draw_header(l);
	if (l->info)
		draw_info(l);
	else if (l->bt_screen)
		draw_bt(l);
	else if (l->tab == TAB_APPS)
		draw_apps(l);
	else
		draw_settings(l);

	ui_fill(&ui, 0, FOOTER_Y, CW, CH - FOOTER_Y, C_BAR);
	float x = 40;
	if (l->dialog) {
		hint(&x, "A", T(T_YES));
		hint(&x, "B", T(T_NO));
	} else if (l->info)
		hint(&x, "B", T(T_BACK));
	else if (l->bt_screen) {
		hint(&x, "A", T(l->bt_sel >= BT_ROWS_FIXED ? T_BT_CONNECT_OR_PAIR : T_SELECT));
		int i = l->bt_sel - BT_ROWS_FIXED;
		if (i >= 0 && i < l->bt.ndev && l->bt.dev[i].paired)
			hint(&x, "X", T(T_BT_FORGET));
		hint(&x, "B", T(T_BACK));
	} else {
		hint(&x, "A", T(l->tab == TAB_APPS ? T_LAUNCH : T_SELECT));
		if (l->tab == TAB_SETTINGS)
			hint(&x, "←→", T(T_ADJUST));
		hint(&x, "L R", T(T_SWITCH_TAB));
		hint(&x, "HOME", T(T_HOLD_HOME));
	}

	if (SDL_GetTicks() < l->toast_until) {
		float w = ui_text_width(&ui, ui.mid, l->toast) + 60;
		ui_fill(&ui, (CW - w) / 2, FOOTER_Y - 96, w, 70, 0x3a4152);
		ui_text(&ui, ui.mid, CW / 2, FOOTER_Y - 80, C_TEXT, ALIGN_CENTER, 0, "%s", l->toast);
	}
	if (l->dialog)
		draw_dialog(l);

	SDL_SetRenderTarget(s->ren, NULL);
	SDL_SetRenderDrawColor(s->ren, 0, 0, 0, 255);
	SDL_RenderClear(s->ren);
	SDL_RenderTextureRotated(s->ren, s->canvas, NULL, &s->dst, s->rot, NULL, SDL_FLIP_NONE);
	SDL_RenderPresent(s->ren);
}

/* ---------------------------------------------------------------- input */

static int gamepad_action(SDL_Gamepad *gp, SDL_GamepadButton b)
{
	switch (b) {
	case SDL_GAMEPAD_BUTTON_DPAD_UP:        return A_UP;
	case SDL_GAMEPAD_BUTTON_DPAD_DOWN:      return A_DOWN;
	case SDL_GAMEPAD_BUTTON_DPAD_LEFT:      return A_LEFT;
	case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:     return A_RIGHT;
	case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  return A_TAB_L;
	case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return A_TAB_R;
	case SDL_GAMEPAD_BUTTON_START:          return A_OK;
	default: break;
	}
	/* confirm / back follow the printed labels (Nintendo layout: A right, B bottom) */
	switch (SDL_GetGamepadButtonLabel(gp, b)) {
	case SDL_GAMEPAD_BUTTON_LABEL_A: return A_OK;
	case SDL_GAMEPAD_BUTTON_LABEL_B: return A_BACK;
	case SDL_GAMEPAD_BUTTON_LABEL_X: return A_OPT;
	case SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN:
		return b == SDL_GAMEPAD_BUTTON_SOUTH ? A_OK : b == SDL_GAMEPAD_BUTTON_EAST ? A_BACK : A_NONE;
	default: return A_NONE;
	}
}

static int key_action(SDL_Keycode k)
{
	switch (k) {
	case SDLK_UP:        return A_UP;
	case SDLK_DOWN:      return A_DOWN;
	case SDLK_LEFT:      return A_LEFT;
	case SDLK_RIGHT:     return A_RIGHT;
	case SDLK_RETURN:
	case SDLK_SPACE:     return A_OK;
	case SDLK_ESCAPE:
	case SDLK_BACKSPACE: return A_BACK;
	case SDLK_TAB:
	case SDLK_E:         return A_TAB_R;
	case SDLK_Q:         return A_TAB_L;
	case SDLK_X:
	case SDLK_DELETE:    return A_OPT;
	default:             return A_NONE;
	}
}

static bool is_dir(int a) { return a >= A_UP && a <= A_RIGHT; }

static void press(Launcher *l, Screen *s, int a)
{
	if (a == A_NONE)
		return;
	if (is_dir(a)) {
		l->rep_act = a;
		l->rep_next = SDL_GetTicks() + 380;
	}
	act(l, s, a);
}

static void release(Launcher *l, int a)
{
	if (a == l->rep_act)
		l->rep_act = A_NONE;
}

static void stick(Launcher *l, Screen *s, int *state, Sint16 v, int neg, int pos)
{
	int nv = v < -20000 ? -1 : v > 20000 ? 1 : (v > -12000 && v < 12000) ? 0 : *state;
	if (nv == *state)
		return;
	if (*state)
		release(l, *state < 0 ? neg : pos);
	*state = nv;
	if (nv)
		press(l, s, nv < 0 ? neg : pos);
}

static void handle(Launcher *l, Screen *s, const SDL_Event *e)
{
	switch (e->type) {
	case SDL_EVENT_QUIT:
		g_term = 1;
		break;
	case SDL_EVENT_GAMEPAD_ADDED:
		for (int i = 0; i < (int)SDL_arraysize(l->gp); i++)
			if (!l->gp[i]) {
				l->gp[i] = SDL_OpenGamepad(e->gdevice.which);
				SDL_Log("gamepad: %s", l->gp[i] ? SDL_GetGamepadName(l->gp[i]) : SDL_GetError());
				break;
			}
		break;
	case SDL_EVENT_GAMEPAD_REMOVED:
		for (int i = 0; i < (int)SDL_arraysize(l->gp); i++)
			if (l->gp[i] && SDL_GetGamepadID(l->gp[i]) == e->gdevice.which) {
				SDL_CloseGamepad(l->gp[i]);
				l->gp[i] = NULL;
			}
		break;
	case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
	case SDL_EVENT_GAMEPAD_BUTTON_UP: {
		/* the labels (A/B/X) come from the pad that sent the event */
		SDL_Gamepad *gp = SDL_GetGamepadFromID(e->gbutton.which);
		if (!gp)
			break;
		int a = gamepad_action(gp, (SDL_GamepadButton)e->gbutton.button);
		if (e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
			press(l, s, a);
		else
			release(l, a);
		break;
	}
	case SDL_EVENT_GAMEPAD_AXIS_MOTION:
		if (e->gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX)
			stick(l, s, &l->stick_x, e->gaxis.value, A_LEFT, A_RIGHT);
		else if (e->gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY)
			stick(l, s, &l->stick_y, e->gaxis.value, A_UP, A_DOWN);
		break;
	case SDL_EVENT_KEY_DOWN:
		if (!e->key.repeat)
			press(l, s, key_action(e->key.key));
		break;
	case SDL_EVENT_KEY_UP:
		release(l, key_action(e->key.key));
		break;
	case SDL_EVENT_FINGER_DOWN:
		if (!l->touch_down) {
			l->touch_down = true;
			l->touch_id = e->tfinger.fingerID;
			to_canvas(s, e->tfinger.x, e->tfinger.y, &l->touch_x, &l->touch_y);
		}
		break;
	case SDL_EVENT_FINGER_UP:
		if (l->touch_down && e->tfinger.fingerID == l->touch_id) {
			float x, y;
			to_canvas(s, e->tfinger.x, e->tfinger.y, &x, &y);
			l->touch_down = false;
			if (fabsf(x - l->touch_x) < 40 && fabsf(y - l->touch_y) < 40)
				tap(l, s, x, y);
		}
		break;
	case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
		screen_fit(s);
		SDL_Log("output=%dx%d", s->ow, s->oh);
		l->dirty = true;
		break;
	case SDL_EVENT_WINDOW_EXPOSED:
		l->dirty = true;
		break;
	}
}

/* ---------------------------------------------------------------- main */

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	/* the session starts us as /usr/bin/hako-app (an alternative symlink);
	 * home-button finds us by this name */
	prctl(PR_SET_NAME, "hako-launcher");
	/* before SDL_Init: SDL leaves SIGTERM alone when a handler is already set */
	struct sigaction sa = { 0 };
	sa.sa_handler = on_term;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sa.sa_handler = on_usr1;
	sigaction(SIGUSR1, &sa, NULL);
	sa.sa_handler = on_usr2;
	sigaction(SIGUSR2, &sa, NULL);

	const char *r = getenv("HAKO_ROTATE");
	int rot = r ? (360 - atoi(r) % 360) % 360 : 90;

	SDL_SetHint(SDL_HINT_GAMECONTROLLERTYPE, "0x2020/0x3001=SwitchPro");
	if (!SDL_Init(SDL_INIT_GAMEPAD)) {
		SDL_Log("SDL_Init: %s", SDL_GetError());
		return 1;
	}
	if (!ui_fonts_open(&ui))
		return 1;

	lang_load();
	lang_export();

	static Launcher l;
	sys_timezone(l.tz, sizeof l.tz);
	sys_apply_timezone(l.tz);
	ext_audio_load(&l);
	bt_open();
	Screen s;
	if (!screen_open(&s, rot))
		return 1;
	apps_reload(&l);
	icons_load(&l, s.ren);
	l.dirty = true;
	SDL_Log("%d apps", l.napps);

	Uint64 last_minute = 0;
	while (!g_term) {
		Uint64 now = SDL_GetTicks();
		int wait = l.rep_act ? 20 : 250;
		SDL_Event e;
		if (SDL_WaitEventTimeout(&e, wait)) {
			handle(&l, &s, &e);
			while (!g_term && SDL_PollEvent(&e))
				handle(&l, &s, &e);
		}
		if (g_term)
			break;
		g_home = 0; /* HOME with no app running: nothing to close */
		if (g_launch) {
			g_launch = 0;
			char id[64];
			if (take_launch_request(id, sizeof id)) {
				apps_reload(&l);
				icons_load(&l, s.ren);
				int idx = -1;
				for (int i = 0; i < l.napps; i++)
					if (!strcmp(l.apps[i].id, id))
						idx = i;
				if (idx >= 0) {
					SDL_Log("launch request: %s", id);
					l.tab = TAB_APPS;
					l.sel_app = idx;
					l.dialog = DLG_NONE;
					l.info = false;
					if (l.bt_screen)
						bt_leave(&l);
					run_app(&l, &s, idx);
				} else {
					SDL_Log("launch request: no app '%s'", id);
					toast(&l, T(T_NO_SUCH_APP), id);
				}
				continue;
			}
		}

		now = SDL_GetTicks();
		if (l.rep_act && now >= l.rep_next) {
			act(&l, &s, l.rep_act);
			l.rep_next = now + 110;
		}
		if (now >= l.sys_next) {
			sys_refresh(&l.sys);
			if (!l.bt_screen && l.tab == TAB_SETTINGS)
				bt_refresh(&l.bt);  /* on / off in the settings list */
			l.sys_next = now + 2000;
			l.dirty = true;
		}
		/* Bluetooth: agent requests, results and changes */
		if (bt_poll())
			l.bt_next = 0;
		char msg[256];
		if (bt_take_message(msg, sizeof msg))
			toast(&l, "%s", msg);
		if (l.bt_screen) {
			if (l.scan_until && now >= l.scan_until) {
				bt_scan(false);
				l.scan_until = 0;
			}
			if (now >= l.bt_next) {
				bt_refresh(&l.bt);
				bt_select_row(&l, l.bt_sel);
				l.bt_next = now + 1000;
				l.dirty = true;
			}
		}
		Uint64 minute = (Uint64)time(NULL) / 60; /* the clock in the header */
		if (minute != last_minute || (l.toast_until && now >= l.toast_until)) {
			last_minute = minute;
			if (now >= l.toast_until)
				l.toast_until = 0;
			l.dirty = true;
		}
		if (l.dirty && s.ren) {
			l.dirty = false;
			draw(&l, &s);
		}
	}

	SDL_Log("exit");
	bt_close();
	icons_free(&l);
	sound_close();
	for (int i = 0; i < (int)SDL_arraysize(l.gp); i++)
		if (l.gp[i])
			SDL_CloseGamepad(l.gp[i]);
	screen_close(&s);
	ui_fonts_close(&ui);
	SDL_Quit();
	return 0;
}
