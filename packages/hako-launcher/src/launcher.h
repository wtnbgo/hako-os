/*
 * hako-launcher: home screen of the hako game console.
 */
#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdbool.h>
#include <sys/types.h>

#define CW 1280 /* canvas (landscape) */
#define CH 960

/* ---- i18n.c: UI language ---- */

typedef enum { LANG_JA, LANG_EN, LANG_COUNT } Lang;

typedef enum {
	T_TAB_APPS, T_TAB_SETTINGS,
	T_START_FAILED, T_CLOSED, T_EXIT_ERROR, T_CRASHED, T_REBOOT_FAILED, T_POWER_FAILED,
	T_BATTERY, T_CHARGING, T_FULL, T_VOLUME_HDR, T_MUTED,
	T_NO_APPS, T_NO_APPS_HINT, T_DEV,
	T_S_VOLUME, T_S_BRIGHT, T_S_SCREENOFF, T_S_LANGUAGE, T_S_TIMEZONE, T_S_EXTAUDIO, T_S_BLUETOOTH, T_S_INFO, T_S_REBOOT, T_S_POWEROFF,
	T_EXT_MONITOR, T_EXT_DEVICE, T_SCREENOFF_NEVER, T_MINUTES,
	T_BT_ON, T_BT_OFF, T_BT_SCAN, T_BT_SCANNING, T_BT_PAIRING_HINT, T_BT_STATE_CONNECTED, T_BT_STATE_PAIRED,
	T_BT_STATE_NEW, T_BT_STATE_PAIRING, T_BT_STATE_CONNECTING, T_BT_STATE_DISCONNECTING, T_BT_UNAVAILABLE,
	T_BT_NONE, T_BT_FORGET, T_ASK_BT_FORGET, T_BT_FAILED, T_BT_CONNECTED_MSG, T_BT_DISCONNECTED_MSG,
	T_BT_FORGOTTEN_MSG, T_BT_CONNECT_OR_PAIR,
	T_NO_SUCH_APP, T_TZ_FAILED, T_TIMEZONE_INFO,
	T_UNAVAILABLE, T_NOT_ADJUSTABLE, T_VOLUME_NOTE,
	T_ASK_REBOOT, T_ASK_POWEROFF, T_YES, T_NO, T_BACK,
	T_LAUNCH, T_SELECT, T_ADJUST, T_SWITCH_TAB, T_HOLD_HOME,
	T_KERNEL, T_PACKAGES, T_NETWORK, T_STORAGE, T_BATTERY_INFO, T_UPTIME, T_DISPLAY, T_LANGUAGE_INFO,
	T_ST_CHARGING, T_ST_DISCHARGING, T_ST_FULL, T_ST_NOT_CHARGING,
	T_COUNT
} TextId;

extern Lang g_lang;
void lang_load(void);
void lang_save(void);
/* Path of a file in ~/.config/hako/ (directories created if mkdirs). */
void hako_config_path(const char *name, char *buf, size_t n, bool mkdirs);
/* HAKO_LANGUAGE / LANGUAGE / LANG for the apps we start */
void lang_export(void);
const char *lang_code(Lang l);
const char *lang_name(Lang l);   /* in its own language: 日本語, English */
const char *T(TextId id);

/* ---- apps.c: registered applications ---- */

#define MAX_APPS 64
#define MAX_ENV 16

typedef struct {
	char id[64];             /* file name without .app */
	char name[128];
	char desc[256];
	char exec[512];
	char args[1024];
	char dir[512];           /* working directory (default: the directory of exec) */
	char icon[512];
	char env[MAX_ENV][256];  /* KEY=VALUE */
	int nenv;
	int order;
	bool dev;                /* from the development directory */
} App;

/* Read /usr/share/hako/apps/ *.app and /data/dev/apps/ *.app, sorted. */
int apps_load(App *apps, int max);
/* Start the app in its own process group; returns the pid or -1. */
pid_t app_spawn(const App *a);

/* ---- ui.c: drawing helpers ---- */

enum { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };

typedef struct {
	TTF_Font *title, *big, *mid, *small;
	TTF_TextEngine *eng;
	SDL_Renderer *ren;
} Ui;

bool ui_fonts_open(Ui *u);
void ui_fonts_close(Ui *u);
bool ui_attach(Ui *u, SDL_Renderer *ren);
void ui_detach(Ui *u);
void ui_fill(Ui *u, float x, float y, float w, float h, Uint32 rgb);
void ui_frame(Ui *u, float x, float y, float w, float h, float t, Uint32 rgb);
/* draw text; returns its width. maxw > 0 shortens it with an ellipsis */
float ui_text(Ui *u, TTF_Font *f, float x, float y, Uint32 rgb, int align, float maxw, const char *fmt, ...)
	SDL_PRINTF_VARARG_FUNC(8);
float ui_text_width(Ui *u, TTF_Font *f, const char *s);

/* ---- sys.c: system settings and information ---- */

typedef struct {
	float volume;        /* 0..1, < 0 when unknown */
	bool muted;
	int bright, bright_max;  /* bright_max 0 when there is no backlight */
	char backlight[64];
	int battery;         /* %, < 0 when unknown */
	char battery_status[32];
} SysState;

void sys_refresh(SysState *s);
void sys_set_volume(SysState *s, float v);
void sys_set_brightness(SysState *s, int v);
int sys_info(char lines[][160], int max);
const char *battery_status_text(const char *st);
bool sys_power(bool reboot);
void sys_timezone(char *buf, size_t n);      /* e.g. Asia/Tokyo */
bool sys_set_timezone(const char *tz);
void sys_apply_timezone(const char *tz);     /* TZ for us and the apps */

/* ---- bt.c: Bluetooth (BlueZ over D-Bus) ---- */

#define BT_MAX_DEVICES 32
#define BT_PATH_MAX 96
#define BT_NAME_MAX 64

typedef enum { BT_IDLE, BT_PAIRING, BT_CONNECTING, BT_DISCONNECTING } BtBusy;

typedef struct {
	char path[BT_PATH_MAX];
	char addr[18];
	char name[BT_NAME_MAX];
	char icon[32];           /* input-gaming, audio-headphones, ... */
	bool has_name, paired, connected, trusted, has_rssi;
	int rssi;
	BtBusy busy;
} BtDevice;

typedef struct {
	bool available;          /* BlueZ and the adapter are there */
	bool powered, discovering;
	BtDevice dev[BT_MAX_DEVICES];  /* connected, paired, then found (nearest first) */
	int ndev;
} BtState;

bool bt_open(void);          /* connect to the bus and register the pairing agent */
void bt_close(void);
bool bt_poll(void);          /* run pending D-Bus work; true when something changed */
bool bt_refresh(BtState *st);
void bt_set_powered(bool on);
void bt_scan(bool on);
void bt_activate(const BtDevice *d);  /* pair+trust+connect, connect or disconnect */
void bt_forget(const BtDevice *d);
bool bt_take_message(char *buf, size_t n);  /* result of an operation, for a toast */
