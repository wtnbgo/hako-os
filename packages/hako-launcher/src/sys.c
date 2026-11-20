/*
 * System settings and information, as the unprivileged session user:
 *   volume      PipeWire default sink (wpctl)
 *   brightness  logind Session.SetBrightness (no root needed for the session owner)
 *   power       logind PowerOff / Reboot (allowed by the polkit rule of this package)
 *   time zone   timedated SetTimezone (likewise)
 */
#define _GNU_SOURCE
#include "launcher.h"

#include <arpa/inet.h>
#include <dirent.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

static bool read_line(const char *path, char *buf, size_t n)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return false;
	bool ok = fgets(buf, (int)n, f) != NULL;
	fclose(f);
	if (ok)
		buf[strcspn(buf, "\r\n")] = 0;
	return ok;
}

static int read_int(const char *path, int def)
{
	char buf[64];
	return read_line(path, buf, sizeof buf) ? atoi(buf) : def;
}

/* run a command and keep the first line of its output */
static bool run_line(const char *cmd, char *buf, size_t n)
{
	FILE *p = popen(cmd, "r");
	if (!p)
		return false;
	bool ok = fgets(buf, (int)n, p) != NULL;
	pclose(p);
	if (ok)
		buf[strcspn(buf, "\r\n")] = 0;
	return ok;
}

void sys_refresh(SysState *s)
{
	char buf[256];
	/* "Volume: 0.70" or "Volume: 0.70 [MUTED]" */
	s->volume = -1;
	s->muted = false;
	if (run_line("wpctl get-volume @DEFAULT_AUDIO_SINK@ 2>/dev/null", buf, sizeof buf) &&
	    !strncmp(buf, "Volume:", 7)) {
		s->volume = (float)atof(buf + 7);
		s->muted = strstr(buf, "MUTED") != NULL;
	}

	if (!s->backlight[0]) {
		DIR *d = opendir("/sys/class/backlight");
		struct dirent *de;
		while (d && (de = readdir(d)))
			if (de->d_name[0] != '.') {
				snprintf(s->backlight, sizeof s->backlight, "%s", de->d_name);
				break;
			}
		if (d)
			closedir(d);
	}
	s->bright_max = 0;
	if (s->backlight[0]) {
		char path[256];
		snprintf(path, sizeof path, "/sys/class/backlight/%s/max_brightness", s->backlight);
		s->bright_max = read_int(path, 0);
		snprintf(path, sizeof path, "/sys/class/backlight/%s/brightness", s->backlight);
		s->bright = read_int(path, 0);
	}

	s->battery = read_int("/sys/class/power_supply/battery/capacity", -1);
	if (!read_line("/sys/class/power_supply/battery/status", s->battery_status, sizeof s->battery_status))
		s->battery_status[0] = 0;
}

void sys_set_volume(SysState *s, float v)
{
	v = SDL_clamp(v, 0.0f, 1.0f);
	char cmd[128];
	snprintf(cmd, sizeof cmd, "wpctl set-volume @DEFAULT_AUDIO_SINK@ %.2f >/dev/null 2>&1", v);
	if (system(cmd) == 0)
		s->volume = v;
}

/* D-Bus object path of our logind session: /org/freedesktop/login1/session/<escaped id> */
static void session_path(char *buf, size_t n)
{
	const char *id = getenv("XDG_SESSION_ID");
	if (!id || !*id) {
		snprintf(buf, n, "/org/freedesktop/login1/session/auto");
		return;
	}
	size_t o = (size_t)snprintf(buf, n, "/org/freedesktop/login1/session/");
	for (size_t i = 0; id[i] && o + 4 < n; i++) {
		unsigned char c = (unsigned char)id[i];
		bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (i > 0 && c >= '0' && c <= '9');
		if (plain)
			buf[o++] = (char)c;
		else
			o += (size_t)snprintf(buf + o, n - o, "_%02x", c);
	}
	buf[o] = 0;
}

void sys_set_brightness(SysState *s, int v)
{
	if (!s->bright_max)
		return;
	int lo = SDL_max(1, s->bright_max / 20); /* never fully dark */
	v = SDL_clamp(v, lo, s->bright_max);
	char path[160], cmd[512];
	session_path(path, sizeof path);
	snprintf(cmd, sizeof cmd,
	         "busctl call org.freedesktop.login1 %s org.freedesktop.login1.Session "
	         "SetBrightness ssu backlight %s %d >/dev/null 2>&1",
	         path, s->backlight, v);
	if (system(cmd) == 0)
		s->bright = v;
	else
		SDL_Log("SetBrightness failed: %s", cmd);
}

bool sys_power(bool reboot)
{
	char cmd[256];
	snprintf(cmd, sizeof cmd,
	         "busctl call org.freedesktop.login1 /org/freedesktop/login1 "
	         "org.freedesktop.login1.Manager %s b false 2>&1",
	         reboot ? "Reboot" : "PowerOff");
	int r = system(cmd);
	if (r != 0)
		SDL_Log("%s failed (%d)", reboot ? "Reboot" : "PowerOff", r);
	return r == 0;
}

/* System time zone: /etc/localtime -> ../usr/share/zoneinfo/Asia/Tokyo */
void sys_timezone(char *buf, size_t n)
{
	char link[512];
	ssize_t r = readlink("/etc/localtime", link, sizeof link - 1);
	if (r > 0) {
		link[r] = 0;
		char *p = strstr(link, "zoneinfo/");
		if (p) {
			snprintf(buf, n, "%s", p + 9);
			return;
		}
	}
	if (!read_line("/etc/timezone", buf, n))
		snprintf(buf, n, "Etc/UTC");
}

/* Set it with systemd-timedated (allowed by the polkit rule of this package)
 * and use it in this process and the apps we start (glibc does not notice a
 * changed /etc/localtime by itself). tz comes from our own table. */
bool sys_set_timezone(const char *tz)
{
	char cmd[256];
	snprintf(cmd, sizeof cmd,
	         "busctl call org.freedesktop.timedate1 /org/freedesktop/timedate1 "
	         "org.freedesktop.timedate1 SetTimezone sb '%s' false 2>&1", tz);
	int r = system(cmd);
	if (r != 0) {
		SDL_Log("SetTimezone %s failed (%d)", tz, r);
		return false;
	}
	sys_apply_timezone(tz);
	return true;
}

void sys_apply_timezone(const char *tz)
{
	setenv("TZ", tz, 1);
	tzset();
}

static void human(char *buf, size_t n, double bytes)
{
	const char *unit[] = { "B", "KB", "MB", "GB", "TB" };
	int i = 0;
	while (bytes >= 1024 && i < 4) {
		bytes /= 1024;
		i++;
	}
	snprintf(buf, n, i >= 3 ? "%.1f %s" : "%.0f %s", bytes, unit[i]);
}

/* sysfs battery status in the UI language */
const char *battery_status_text(const char *st)
{
	if (!strcmp(st, "Charging"))
		return T(T_ST_CHARGING);
	if (!strcmp(st, "Discharging"))
		return T(T_ST_DISCHARGING);
	if (!strcmp(st, "Full"))
		return T(T_ST_FULL);
	if (!strcmp(st, "Not charging"))
		return T(T_ST_NOT_CHARGING);
	return st;
}

int sys_info(char lines[][160], int max)
{
	int n = 0;
	char buf[256];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wformat-security"
#define ADD(...) do { if (n < max) snprintf(lines[n++], 160, __VA_ARGS__); } while (0)

	/* OS */
	FILE *f = fopen("/etc/os-release", "r");
	if (f) {
		while (fgets(buf, sizeof buf, f))
			if (!strncmp(buf, "PRETTY_NAME=", 12)) {
				char *v = buf + 12;
				v[strcspn(v, "\r\n")] = 0;
				if (*v == '"') {
					v++;
					v[strcspn(v, "\"")] = 0;
				}
				ADD("OS: %s", v);
			}
		fclose(f);
	}
	struct utsname u;
	if (uname(&u) == 0)
		ADD("%s: %s (%s)", T(T_KERNEL), u.release, u.machine);

	/* our packages */
	ADD("%s:", T(T_PACKAGES));
	FILE *p = popen("dpkg-query -W -f '${db:Status-Abbrev}${Package} ${Version}\\n' 'hako-*' '*-bsp' 2>/dev/null", "r");
	if (p) {
		while (fgets(buf, sizeof buf, p))
			if (!strncmp(buf, "ii ", 3)) {
				buf[strcspn(buf, "\r\n")] = 0;
				ADD("  %s", buf + 3);
			}
		pclose(p);
	}

	/* network */
	struct ifaddrs *ifa0;
	if (getifaddrs(&ifa0) == 0) {
		for (struct ifaddrs *ifa = ifa0; ifa; ifa = ifa->ifa_next) {
			if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET || (ifa->ifa_flags & IFF_LOOPBACK))
				continue;
			char addr[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr, addr, sizeof addr);
			ADD("%s: %s %s", T(T_NETWORK), ifa->ifa_name, addr);
		}
		freeifaddrs(ifa0);
	}

	/* storage */
	const char *mounts[] = { "/", "/data" };
	for (size_t i = 0; i < SDL_arraysize(mounts); i++) {
		struct statvfs st;
		if (statvfs(mounts[i], &st) != 0)
			continue;
		char used[32], total[32];
		human(used, sizeof used, (double)(st.f_blocks - st.f_bfree) * st.f_frsize);
		human(total, sizeof total, (double)st.f_blocks * st.f_frsize);
		ADD(T(T_STORAGE), mounts[i], used, total);
	}

	/* battery, uptime */
	SysState s = { 0 };
	sys_refresh(&s);
	if (s.battery >= 0)
		ADD(T(T_BATTERY_INFO), s.battery, battery_status_text(s.battery_status));
	double up = 0;
	if (read_line("/proc/uptime", buf, sizeof buf))
		up = atof(buf);
	ADD(T(T_UPTIME), (int)up / 3600, (int)up / 60 % 60);
	ADD("%s: %s", T(T_DISPLAY), SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "-");
	ADD("%s: %s", T(T_LANGUAGE_INFO), lang_name(g_lang));
	sys_timezone(buf, sizeof buf);
	ADD("%s: %s", T(T_TIMEZONE_INFO), buf);
#undef ADD
#pragma GCC diagnostic pop
	return n;
}
