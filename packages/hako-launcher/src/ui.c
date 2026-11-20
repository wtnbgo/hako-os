/*
 * Drawing helpers: filled boxes, frames and UTF-8 text (SDL3_ttf, Noto Sans CJK).
 */
#include "launcher.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *FONT_REGULAR[] = {
	"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
	"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
	NULL,
};
static const char *FONT_BOLD[] = {
	"/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc",
	"/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
	NULL,
};

static TTF_Font *open_font(const char **paths, float size)
{
	for (int i = 0; paths[i]; i++) {
		TTF_Font *f = TTF_OpenFont(paths[i], size);
		if (f)
			return f;
	}
	SDL_Log("font: %s", SDL_GetError());
	return NULL;
}

bool ui_fonts_open(Ui *u)
{
	if (!TTF_Init()) {
		SDL_Log("TTF_Init: %s", SDL_GetError());
		return false;
	}
	u->title = open_font(FONT_BOLD, 40);
	u->big = open_font(FONT_BOLD, 34);
	u->mid = open_font(FONT_REGULAR, 28);
	u->small = open_font(FONT_REGULAR, 22);
	return u->title && u->big && u->mid && u->small;
}

void ui_fonts_close(Ui *u)
{
	TTF_Font **fs[] = { &u->title, &u->big, &u->mid, &u->small };
	for (size_t i = 0; i < SDL_arraysize(fs); i++)
		if (*fs[i]) {
			TTF_CloseFont(*fs[i]);
			*fs[i] = NULL;
		}
	TTF_Quit();
}

bool ui_attach(Ui *u, SDL_Renderer *ren)
{
	u->ren = ren;
	u->eng = TTF_CreateRendererTextEngine(ren);
	if (!u->eng)
		SDL_Log("text engine: %s", SDL_GetError());
	return u->eng != NULL;
}

void ui_detach(Ui *u)
{
	if (u->eng)
		TTF_DestroyRendererTextEngine(u->eng);
	u->eng = NULL;
	u->ren = NULL;
}

static void set_color(Ui *u, Uint32 rgb)
{
	SDL_SetRenderDrawColor(u->ren, rgb >> 16 & 0xff, rgb >> 8 & 0xff, rgb & 0xff, 255);
}

void ui_fill(Ui *u, float x, float y, float w, float h, Uint32 rgb)
{
	SDL_FRect r = { x, y, w, h };
	set_color(u, rgb);
	SDL_RenderFillRect(u->ren, &r);
}

void ui_frame(Ui *u, float x, float y, float w, float h, float t, Uint32 rgb)
{
	ui_fill(u, x, y, w, t, rgb);
	ui_fill(u, x, y + h - t, w, t, rgb);
	ui_fill(u, x, y, t, h, rgb);
	ui_fill(u, x + w - t, y, t, h, rgb);
}

float ui_text_width(Ui *u, TTF_Font *f, const char *s)
{
	int w = 0, h = 0;
	(void)u;
	TTF_GetStringSize(f, s, 0, &w, &h);
	return (float)w;
}

/* cut s (UTF-8) so that s + "…" fits in maxw */
static void ellipsize(Ui *u, TTF_Font *f, char *s, float maxw)
{
	if (ui_text_width(u, f, s) <= maxw)
		return;
	size_t len = strlen(s);
	while (len > 0) {
		/* drop one UTF-8 character */
		do
			len--;
		while (len > 0 && ((unsigned char)s[len] & 0xc0) == 0x80);
		char tmp[512];
		snprintf(tmp, sizeof tmp, "%.*s…", (int)len, s);
		if (ui_text_width(u, f, tmp) <= maxw) {
			snprintf(s, 512, "%s", tmp);
			return;
		}
	}
	s[0] = 0;
}

float ui_text(Ui *u, TTF_Font *f, float x, float y, Uint32 rgb, int align, float maxw, const char *fmt, ...)
{
	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	if (!buf[0] || !f || !u->eng)
		return 0;
	if (maxw > 0)
		ellipsize(u, f, buf, maxw);

	TTF_Text *t = TTF_CreateText(u->eng, f, buf, 0);
	if (!t)
		return 0;
	int w = 0, h = 0;
	TTF_GetTextSize(t, &w, &h);
	if (align == ALIGN_CENTER)
		x -= w / 2.0f;
	else if (align == ALIGN_RIGHT)
		x -= w;
	TTF_SetTextColor(t, rgb >> 16 & 0xff, rgb >> 8 & 0xff, rgb & 0xff, 255);
	TTF_DrawRendererText(t, x, y);
	TTF_DestroyText(t);
	return (float)w;
}
