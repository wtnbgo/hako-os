/*
 * Registered applications: one small "key=value" file per app.
 *
 *   Name=SDL3 サンプル            shown on the tile
 *   Name[en]=SDL3 Sample           (optional) for one UI language (ja / en)
 *   Description=パッドとタッチの確認  (optional) second line on the tile
 *   Description[en]=...            (optional) for one UI language
 *   Exec=/usr/bin/hako-sdl-hello  the program (relative: to the .app file)
 *   Args=--foo "two words"          (optional) arguments, "..." keeps spaces
 *   Dir=/path                        (optional) working directory, default the
 *                                    directory of Exec (engines load assets
 *                                    relative to the current directory)
 *   Icon=icon.png                    (optional) PNG, relative: to the .app file
 *   Env=KEY=VALUE                    (optional, repeatable) extra environment
 *   Order=50                         (optional) sort key, then Name
 *
 * Packages put theirs in /usr/share/hako/apps/, development builds in
 * /data/dev/apps/ (shown with a "開発" badge).
 */
#define _GNU_SOURCE
#include "launcher.h"

#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const struct { const char *path; bool dev; } APP_DIRS[] = {
	{ "/usr/share/hako/apps", false },
	{ "/data/dev/apps", true },
};

static char *trim(char *s)
{
	while (*s == ' ' || *s == '\t')
		s++;
	char *e = s + strlen(s);
	while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n'))
		*--e = 0;
	return s;
}

static void copy(char *dst, size_t n, const char *src)
{
	snprintf(dst, n, "%s", src);
}

/* make a path relative to the .app file's directory absolute */
static void resolve(char *path, size_t n, const char *base)
{
	if (!path[0] || path[0] == '/')
		return;
	char tmp[1024];
	snprintf(tmp, sizeof tmp, "%s/%s", base, path);
	copy(path, n, tmp);
}

static bool load_one(App *a, const char *dir, const char *file, bool dev)
{
	char path[1024];
	snprintf(path, sizeof path, "%s/%s", dir, file);
	FILE *f = fopen(path, "r");
	if (!f)
		return false;
	memset(a, 0, sizeof *a);
	a->order = 50;
	a->dev = dev;
	copy(a->id, sizeof a->id, file);
	char *dot = strrchr(a->id, '.');
	if (dot)
		*dot = 0;

	char line[1400];
	bool name_local = false, desc_local = false;
	while (fgets(line, sizeof line, f)) {
		char *s = trim(line);
		if (!*s || *s == '#')
			continue;
		char *eq = strchr(s, '=');
		if (!eq)
			continue;
		*eq = 0;
		char *key = trim(s), *val = trim(eq + 1);
		/* Name[en]=... : for that UI language only, wins over the plain key */
		bool local = false;
		char *br = strchr(key, '[');
		if (br) {
			char *end = strchr(br, ']');
			if (!end)
				continue;
			*br = 0;
			*end = 0;
			if (strcmp(br + 1, lang_code(g_lang)))
				continue;
			local = true;
		}
		if (!strcmp(key, "Name")) {
			if (local || !name_local)
				copy(a->name, sizeof a->name, val);
			name_local |= local;
		} else if (!strcmp(key, "Description")) {
			if (local || !desc_local)
				copy(a->desc, sizeof a->desc, val);
			desc_local |= local;
		} else if (local)
			continue;
		else if (!strcmp(key, "Exec"))
			copy(a->exec, sizeof a->exec, val);
		else if (!strcmp(key, "Args"))
			copy(a->args, sizeof a->args, val);
		else if (!strcmp(key, "Dir"))
			copy(a->dir, sizeof a->dir, val);
		else if (!strcmp(key, "Icon"))
			copy(a->icon, sizeof a->icon, val);
		else if (!strcmp(key, "Order"))
			a->order = atoi(val);
		else if (!strcmp(key, "Env") && a->nenv < MAX_ENV && strchr(val, '='))
			copy(a->env[a->nenv++], sizeof a->env[0], val);
	}
	fclose(f);

	if (!a->exec[0]) {
		SDL_Log("%s: no Exec=, skipped", path);
		return false;
	}
	if (!a->name[0])
		copy(a->name, sizeof a->name, a->id);
	resolve(a->exec, sizeof a->exec, dir);
	resolve(a->dir, sizeof a->dir, dir);
	resolve(a->icon, sizeof a->icon, dir);
	if (access(a->exec, X_OK) != 0) {
		SDL_Log("%s: %s is not executable, skipped", path, a->exec);
		return false;
	}
	return true;
}

static int cmp_app(const void *pa, const void *pb)
{
	const App *a = pa, *b = pb;
	if (a->order != b->order)
		return a->order < b->order ? -1 : 1;
	return strcmp(a->name, b->name);
}

int apps_load(App *apps, int max)
{
	int n = 0;
	for (size_t d = 0; d < SDL_arraysize(APP_DIRS); d++) {
		DIR *dp = opendir(APP_DIRS[d].path);
		if (!dp)
			continue;
		struct dirent *de;
		while ((de = readdir(dp)) && n < max) {
			size_t len = strlen(de->d_name);
			if (len < 5 || strcmp(de->d_name + len - 4, ".app"))
				continue;
			if (load_one(&apps[n], APP_DIRS[d].path, de->d_name, APP_DIRS[d].dev))
				n++;
		}
		closedir(dp);
	}
	qsort(apps, n, sizeof *apps, cmp_app);
	return n;
}

/* split Args on spaces; "..." groups words */
static int split_args(char *s, char **argv, int max)
{
	int n = 0;
	while (*s && n < max) {
		while (*s == ' ' || *s == '\t')
			s++;
		if (!*s)
			break;
		char *out = s;
		argv[n++] = out;
		bool quoted = false;
		while (*s && (quoted || (*s != ' ' && *s != '\t'))) {
			if (*s == '"')
				quoted = !quoted;
			else
				*out++ = *s;
			s++;
		}
		if (*s)
			s++;
		*out = 0;
	}
	return n;
}

pid_t app_spawn(const App *a)
{
	char args[sizeof a->args];
	copy(args, sizeof args, a->args);
	char *argv[64];
	argv[0] = (char *)a->exec;
	int argc = 1 + split_args(args, argv + 1, 62);
	argv[argc] = NULL;

	pid_t pid = fork();
	if (pid < 0) {
		SDL_Log("fork: %s", strerror(errno));
		return -1;
	}
	if (pid == 0) {
		/* own process group: HOME can stop the app and everything it started */
		setpgid(0, 0);
		signal(SIGTERM, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		signal(SIGUSR1, SIG_DFL);
		sigset_t none;
		sigemptyset(&none);
		sigprocmask(SIG_SETMASK, &none, NULL);

		char dir[sizeof a->exec];
		if (a->dir[0])
			copy(dir, sizeof dir, a->dir);
		else {
			char tmp[sizeof a->exec];
			copy(tmp, sizeof tmp, a->exec);
			copy(dir, sizeof dir, dirname(tmp));
		}
		if (chdir(dir) != 0)
			fprintf(stderr, "hako-launcher: chdir %s: %s\n", dir, strerror(errno));
		for (int i = 0; i < a->nenv; i++)
			putenv(strdup(a->env[i]));
		close_range(3, ~0U, 0);
		execv(a->exec, argv);
		fprintf(stderr, "hako-launcher: exec %s: %s\n", a->exec, strerror(errno));
		_exit(127);
	}
	setpgid(pid, pid); /* also from the parent, so kill(-pid) works right away */
	return pid;
}
