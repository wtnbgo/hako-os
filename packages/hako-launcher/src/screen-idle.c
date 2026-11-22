/*
 * hako-screen-idle: turn the built-in panel's backlight off when nothing has
 * been touched for a while, and back on at the next input.
 *
 * Runs as root (hako-screen-idle.service). It watches every /dev/input/event*
 * device itself instead of relying on the compositor's idle notification:
 * cage does not count gamepads as activity, and SDL apps (the launcher too)
 * inhibit the compositor's idle state while they run.
 *
 * While the screen is off, all input devices are grabbed (EVIOCGRAB), so the
 * input that wakes the screen does not reach the app, logind (power key) or
 * triggerhappy (volume keys).
 *
 * Timeout: minutes in ~hako/.config/hako/screen-off (written by the launcher's
 * settings), 0 = never. Missing file: 5 minutes. Re-read when it changes.
 * The backlight is switched with bl_power, so its brightness is left alone.
 */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MAX_DEV 32
#define DEFAULT_MIN 5
#define BITS_PER_LONG (sizeof(long) * 8)
#define NLONGS(x) (((x) + BITS_PER_LONG - 1) / BITS_PER_LONG)
#define TEST_BIT(b, a) (((a)[(b) / BITS_PER_LONG] >> ((b) % BITS_PER_LONG)) & 1)

typedef struct {
	int fd;
	char node[32];
	int abs_last[ABS_CNT];  /* last value that counted as activity */
	int abs_step[ABS_CNT];  /* change needed to count (0: axis ignored) */
} Dev;

static Dev devs[MAX_DEV];
static int ndev;
static bool blanked, grabbed;
static char cfg_path[512];
static time_t cfg_mtime = -1;
static int timeout_min = DEFAULT_MIN;
static volatile sig_atomic_t quit;

static double now(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void load_config(void)
{
	struct stat st;
	if (stat(cfg_path, &st) < 0) {
		if (cfg_mtime != 0)
			timeout_min = DEFAULT_MIN;
		cfg_mtime = 0;
		return;
	}
	if (st.st_mtime == cfg_mtime)
		return;
	cfg_mtime = st.st_mtime;
	FILE *f = fopen(cfg_path, "r");
	int v;
	if (f && fscanf(f, "%d", &v) == 1 && v >= 0)
		timeout_min = v;
	else
		timeout_min = DEFAULT_MIN;
	if (f)
		fclose(f);
	fprintf(stderr, "screen-idle: timeout %d min\n", timeout_min);
}

static void backlight(bool on)
{
	DIR *d = opendir("/sys/class/backlight");
	if (!d)
		return;
	struct dirent *de;
	while ((de = readdir(d))) {
		if (de->d_name[0] == '.')
			continue;
		char p[300];
		snprintf(p, sizeof p, "/sys/class/backlight/%s/bl_power", de->d_name);
		int fd = open(p, O_WRONLY);
		if (fd < 0)
			continue;
		/* FB_BLANK_UNBLANK = 0, FB_BLANK_POWERDOWN = 4 */
		if (write(fd, on ? "0" : "4", 1) < 0)
			fprintf(stderr, "screen-idle: %s: %s\n", p, strerror(errno));
		close(fd);
	}
	closedir(d);
}

static void grab_all(bool on)
{
	for (int i = 0; i < ndev; i++)
		ioctl(devs[i].fd, EVIOCGRAB, on ? 1 : 0);
	grabbed = on;
}

static void set_blank(bool b)
{
	if (b == blanked)
		return;
	blanked = b;
	if (b) {
		grab_all(true);
		backlight(false);
	} else {
		backlight(true);
		grab_all(false);
	}
	fprintf(stderr, "screen-idle: screen %s\n", b ? "off" : "on");
}

static bool have_dev(const char *node)
{
	for (int i = 0; i < ndev; i++)
		if (!strcmp(devs[i].node, node))
			return true;
	return false;
}

static void add_dev(const char *name)
{
	if (strncmp(name, "event", 5) || ndev >= MAX_DEV || have_dev(name))
		return;
	char path[64];
	snprintf(path, sizeof path, "/dev/input/%s", name);
	int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0)
		return;
	unsigned long ev[NLONGS(EV_CNT)] = { 0 };
	char dname[128] = "";
	ioctl(fd, EVIOCGBIT(0, sizeof ev), ev);
	ioctl(fd, EVIOCGNAME(sizeof dname), dname);
	/* jacks (switch events) and the haptics motor are not user input */
	if (!(TEST_BIT(EV_KEY, ev) || TEST_BIT(EV_REL, ev) || TEST_BIT(EV_ABS, ev)) ||
	    strstr(dname, "Jack") || strstr(dname, "haptics")) {
		close(fd);
		return;
	}
	Dev *d = &devs[ndev++];
	memset(d, 0, sizeof *d);
	d->fd = fd;
	snprintf(d->node, sizeof d->node, "%s", name);
	if (TEST_BIT(EV_ABS, ev)) {
		unsigned long ab[NLONGS(ABS_CNT)] = { 0 };
		ioctl(fd, EVIOCGBIT(EV_ABS, sizeof ab), ab);
		for (int a = 0; a < ABS_CNT; a++) {
			struct input_absinfo ai;
			/* touch positions count through BTN_TOUCH; sticks only when
			 * they move by an eighth of their range (ADC noise) */
			if (!TEST_BIT(a, ab) || (a >= ABS_MT_SLOT && a <= ABS_MT_TOOL_Y) ||
			    ioctl(fd, EVIOCGABS(a), &ai) < 0)
				continue;
			int range = ai.maximum - ai.minimum;
			d->abs_step[a] = range <= 2 ? 1 : range / 8;
			d->abs_last[a] = ai.value;
		}
	}
	if (grabbed)
		ioctl(fd, EVIOCGRAB, 1);
	fprintf(stderr, "screen-idle: watching %s (%s)\n", name, dname);
}

static void drop_dev(int i)
{
	close(devs[i].fd);
	devs[i] = devs[--ndev];
}

static void scan_devs(void)
{
	DIR *d = opendir("/dev/input");
	if (!d)
		return;
	struct dirent *de;
	while ((de = readdir(d)))
		add_dev(de->d_name);
	closedir(d);
}

/* drain a device; true if any event counts as activity */
static bool read_dev(int i)
{
	Dev *d = &devs[i];
	struct input_event ev[64];
	bool act = false;
	for (;;) {
		ssize_t n = read(d->fd, ev, sizeof ev);
		if (n < 0) {
			if (errno == ENODEV)
				return drop_dev(i), act;
			break;
		}
		for (size_t k = 0; k < n / sizeof ev[0]; k++) {
			struct input_event *e = &ev[k];
			if (e->type == EV_KEY && e->value == 1)
				act = true;
			else if (e->type == EV_REL)
				act = true;
			else if (e->type == EV_ABS && e->code < ABS_CNT && d->abs_step[e->code] &&
			         abs(e->value - d->abs_last[e->code]) >= d->abs_step[e->code]) {
				d->abs_last[e->code] = e->value;
				act = true;
			}
		}
	}
	return act;
}

static void on_signal(int s)
{
	(void)s;
	quit = 1;
}

int main(void)
{
	const char *user = getenv("HAKO_USER") ? getenv("HAKO_USER") : "hako";
	struct passwd *pw = getpwnam(user);
	snprintf(cfg_path, sizeof cfg_path, "%s/.config/hako/screen-off", pw ? pw->pw_dir : "/var/lib/hako");
	signal(SIGTERM, on_signal);
	signal(SIGINT, on_signal);

	int ino = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
	inotify_add_watch(ino, "/dev/input", IN_CREATE | IN_ATTRIB);
	load_config();
	scan_devs();
	backlight(true);  /* in case an earlier run died while the screen was off */

	double last = now();
	while (!quit) {
		load_config();
		double idle = now() - last;
		int wait_ms = 30000;
		if (timeout_min > 0 && !blanked) {
			double left = timeout_min * 60.0 - idle;
			if (left <= 0) {
				set_blank(true);
			} else if (left * 1000 < wait_ms)
				wait_ms = (int)(left * 1000) + 50;
		}
		if (timeout_min == 0 && blanked)
			set_blank(false);

		struct pollfd p[MAX_DEV + 1];
		p[0] = (struct pollfd){ .fd = ino, .events = POLLIN };
		for (int i = 0; i < ndev; i++)
			p[i + 1] = (struct pollfd){ .fd = devs[i].fd, .events = POLLIN };
		int nd = ndev;
		if (poll(p, nd + 1, wait_ms) < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (p[0].revents & POLLIN) {
			char buf[4096];
			while (read(ino, buf, sizeof buf) > 0)
				;
			scan_devs();  /* new devices (also when udev fixes up a node) */
		}
		bool act = false;
		/* backwards: read_dev may drop a device (swaps in the last one) */
		for (int i = nd - 1; i >= 0; i--)
			if (p[i + 1].revents & (POLLIN | POLLERR | POLLHUP))
				act |= read_dev(i);
		if (act) {
			last = now();
			set_blank(false);
		}
	}
	set_blank(false);
	return 0;
}
