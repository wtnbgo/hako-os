/*
 * UI language (Japanese / English).
 *
 * The choice is kept in $XDG_CONFIG_HOME/hako/language (default
 * ~/.config/hako/language, i.e. /var/lib/hako/.config/... for the session
 * user). Without it: HAKO_LANGUAGE (set in /etc/default/hako-session),
 * then LANG ("en*" -> English); otherwise Japanese.
 *
 * Started apps get the language as HAKO_LANGUAGE=ja|en, LANGUAGE and LANG
 * (LANG falls back to C.UTF-8 when that locale is not installed).
 */
#include "launcher.h"

#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

Lang g_lang = LANG_JA;

static const char *CODE[LANG_COUNT] = { "ja", "en" };
static const char *LOCALE[LANG_COUNT] = { "ja_JP.UTF-8", "en_US.UTF-8" };
static const char *NAME[LANG_COUNT] = { "日本語", "English" };

const char *lang_code(Lang l) { return CODE[l]; }
const char *lang_name(Lang l) { return NAME[l]; }

static bool parse(const char *s, Lang *out)
{
	if (!s)
		return false;
	for (int i = 0; i < LANG_COUNT; i++)
		if (!strncmp(s, CODE[i], 2)) {
			*out = (Lang)i;
			return true;
		}
	return false;
}

void hako_config_path(const char *name, char *buf, size_t n, bool mkdirs)
{
	const char *xdg = getenv("XDG_CONFIG_HOME");
	char dir[512];
	if (xdg && *xdg)
		snprintf(dir, sizeof dir, "%s/hako", xdg);
	else
		snprintf(dir, sizeof dir, "%s/.config/hako", getenv("HOME") ? getenv("HOME") : "/tmp");
	if (mkdirs) {
		char parent[512];
		snprintf(parent, sizeof parent, "%s", dir);
		char *slash = strrchr(parent, '/');
		if (slash) {
			*slash = 0;
			mkdir(parent, 0755);
		}
		mkdir(dir, 0755);
	}
	snprintf(buf, n, "%s/%s", dir, name);
}

static void config_path(char *buf, size_t n, bool mkdirs)
{
	hako_config_path("language", buf, n, mkdirs);
}

void lang_load(void)
{
	char path[600], buf[32] = "";
	config_path(path, sizeof path, false);
	FILE *f = fopen(path, "r");
	if (f) {
		if (fgets(buf, sizeof buf, f) && parse(buf, &g_lang)) {
			fclose(f);
			return;
		}
		fclose(f);
	}
	if (parse(getenv("HAKO_LANGUAGE"), &g_lang))
		return;
	const char *lang = getenv("LANG");
	g_lang = (lang && !strncmp(lang, "en", 2)) ? LANG_EN : LANG_JA;
}

void lang_save(void)
{
	char path[600];
	config_path(path, sizeof path, true);
	FILE *f = fopen(path, "w");
	if (!f) {
		SDL_Log("save %s: %s", path, strerror(errno));
		return;
	}
	fprintf(f, "%s\n", CODE[g_lang]);
	fclose(f);
}

void lang_export(void)
{
	setenv("HAKO_LANGUAGE", CODE[g_lang], 1);
	setenv("LANGUAGE", CODE[g_lang], 1);
	/* The image has no generated locales (only the built-in C.UTF-8); an
	 * unavailable LANG makes setlocale() fail in apps (Python, Qt, ...).
	 * Keep UTF-8 then: the language still reaches apps via LANGUAGE (SDL's
	 * SDL_GetPreferredLocales reads it first) and HAKO_LANGUAGE. */
	locale_t l = newlocale(LC_ALL_MASK, LOCALE[g_lang], (locale_t)0);
	if (l) {
		freelocale(l);
		setenv("LANG", LOCALE[g_lang], 1);
	} else {
		setenv("LANG", "C.UTF-8", 1);
	}
}

static const char *TEXT[T_COUNT][LANG_COUNT] = {
	[T_TAB_APPS]       = { "アプリ", "Apps" },
	[T_TAB_SETTINGS]   = { "設定", "Settings" },
	[T_START_FAILED]   = { "「%s」を起動できませんでした", "Could not start \"%s\"" },
	[T_CLOSED]         = { "「%s」を終了しました", "Closed \"%s\"" },
	[T_EXIT_ERROR]     = { "「%s」がエラーで終了しました (終了コード %d)", "\"%s\" exited with an error (code %d)" },
	[T_CRASHED]        = { "「%s」が異常終了しました (シグナル %d)", "\"%s\" crashed (signal %d)" },
	[T_REBOOT_FAILED]  = { "再起動できませんでした", "Could not reboot" },
	[T_POWER_FAILED]   = { "電源を切ることができませんでした", "Could not power off" },
	[T_BATTERY]        = { "電池 %d%%", "Battery %d%%" },
	[T_CHARGING]       = { " 充電中", " charging" },
	[T_VOLUME_HDR]     = { "音量 %d%%", "Volume %d%%" },
	[T_MUTED]          = { " (ミュート)", " (muted)" },
	[T_NO_APPS]        = { "アプリが登録されていません", "No applications registered" },
	[T_NO_APPS_HINT]   = { "/usr/share/hako/apps/ か /data/dev/apps/ に .app ファイルを置いてください",
	                       "Put .app files in /usr/share/hako/apps/ or /data/dev/apps/" },
	[T_DEV]            = { "開発", "DEV" },
	[T_S_VOLUME]       = { "音量", "Volume" },
	[T_S_BRIGHT]       = { "画面の明るさ", "Brightness" },
	[T_S_LANGUAGE]     = { "言語 / Language", "Language / 言語" },
	[T_S_TIMEZONE]     = { "タイムゾーン", "Time zone" },
	[T_S_INFO]         = { "システム情報", "System information" },
	[T_NO_SUCH_APP]    = { "アプリ「%s」が見つかりません", "No application \"%s\"" },
	[T_S_EXTAUDIO]     = { "外部モニターの音声", "External monitor audio" },
	[T_S_BLUETOOTH]    = { "Bluetooth", "Bluetooth" },
	[T_BT_ON]          = { "オン", "On" },
	[T_BT_OFF]         = { "オフ", "Off" },
	[T_BT_SCAN]        = { "機器を探す", "Search for devices" },
	[T_BT_SCANNING]    = { "探しています…", "Searching…" },
	[T_BT_PAIRING_HINT] = { "つなぎたい機器をペアリング状態にしてから探してください。",
	                        "Put the device into pairing mode, then search." },
	[T_BT_STATE_CONNECTED]     = { "接続済み", "Connected" },
	[T_BT_STATE_PAIRED]        = { "登録済み", "Paired" },
	[T_BT_STATE_NEW]           = { "未登録", "New" },
	[T_BT_STATE_PAIRING]       = { "ペアリング中…", "Pairing…" },
	[T_BT_STATE_CONNECTING]    = { "つないでいます…", "Connecting…" },
	[T_BT_STATE_DISCONNECTING] = { "切断しています…", "Disconnecting…" },
	[T_BT_UNAVAILABLE] = { "Bluetooth を使えません", "Bluetooth is not available" },
	[T_BT_NONE]        = { "機器はまだありません", "No devices yet" },
	[T_BT_FORGET]      = { "登録を解除", "Forget" },
	[T_ASK_BT_FORGET]  = { "%s の登録を解除しますか？", "Forget %s?" },
	[T_BT_FAILED]      = { "%s: うまくいきませんでした (%s)", "%s: failed (%s)" },
	[T_BT_CONNECTED_MSG]    = { "%s をつなぎました", "Connected %s" },
	[T_BT_DISCONNECTED_MSG] = { "%s を切断しました", "Disconnected %s" },
	[T_BT_FORGOTTEN_MSG]    = { "%s の登録を解除しました", "Forgot %s" },
	[T_BT_CONNECT_OR_PAIR]  = { "つなぐ / 切断", "Connect / disconnect" },
	[T_EXT_MONITOR]    = { "モニターから", "From the monitor" },
	[T_EXT_DEVICE]     = { "本体から", "From the device" },
	[T_TZ_FAILED]      = { "タイムゾーンを変更できませんでした", "Could not change the time zone" },
	[T_TIMEZONE_INFO]  = { "タイムゾーン", "Time zone" },
	[T_S_REBOOT]       = { "再起動", "Reboot" },
	[T_S_POWEROFF]     = { "電源を切る", "Power off" },
	[T_UNAVAILABLE]    = { "取得できません", "Unavailable" },
	[T_NOT_ADJUSTABLE] = { "調整できません", "Not adjustable" },
	[T_VOLUME_NOTE]    = { "音量は本体の音量ボタンでも変えられます。", "The volume buttons on the side also change the volume." },
	[T_ASK_REBOOT]     = { "再起動しますか？", "Reboot now?" },
	[T_ASK_POWEROFF]   = { "電源を切りますか？", "Power off now?" },
	[T_YES]            = { "はい", "Yes" },
	[T_NO]             = { "いいえ", "No" },
	[T_BACK]           = { "戻る", "Back" },
	[T_LAUNCH]         = { "起動", "Start" },
	[T_SELECT]         = { "決定", "Select" },
	[T_ADJUST]         = { "調整", "Adjust" },
	[T_SWITCH_TAB]     = { "タブ切り替え", "Switch tab" },
	[T_HOLD_HOME]      = { "長押しでアプリを終了", "Hold to close the app" },
	[T_KERNEL]         = { "カーネル", "Kernel" },
	[T_PACKAGES]       = { "パッケージ", "Packages" },
	[T_NETWORK]        = { "ネットワーク", "Network" },
	[T_STORAGE]        = { "ストレージ %s: %s / %s 使用", "Storage %s: %s of %s used" },
	[T_BATTERY_INFO]   = { "電池: %d%% (%s)", "Battery: %d%% (%s)" },
	[T_UPTIME]         = { "起動からの時間: %d 時間 %d 分", "Uptime: %d h %d min" },
	[T_DISPLAY]        = { "画面", "Display" },
	[T_LANGUAGE_INFO]  = { "言語", "Language" },
	[T_ST_CHARGING]    = { "充電中", "charging" },
	[T_ST_DISCHARGING] = { "放電中", "discharging" },
	[T_ST_FULL]        = { "満充電", "full" },
	[T_ST_NOT_CHARGING]= { "充電停止中", "not charging" },
};

const char *T(TextId id)
{
	const char *s = TEXT[id][g_lang];
	return s ? s : TEXT[id][LANG_EN];
}
