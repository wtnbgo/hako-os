PKG_CONFIG ?= pkg-config
CFLAGS ?= -O2 -g
CFLAGS += -Wall -Wextra -Wno-format-truncation $(shell $(PKG_CONFIG) --cflags sdl3 sdl3-ttf sdl3-image libsystemd)
LDLIBS += $(shell $(PKG_CONFIG) --libs sdl3 sdl3-ttf sdl3-image libsystemd) -lm
PREFIX ?= /usr

SRC = src/main.c src/apps.c src/i18n.c src/sys.c src/ui.c src/bt.c

all: hako-launcher

hako-launcher: $(SRC) src/launcher.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(SRC) $(LDLIBS)

install: hako-launcher
	install -D -m 0755 hako-launcher $(DESTDIR)$(PREFIX)/bin/hako-launcher
	install -D -m 0755 data/home-button $(DESTDIR)$(PREFIX)/lib/hako/home-button
	install -D -m 0755 data/launch-app $(DESTDIR)$(PREFIX)/lib/hako/launch-app
	install -D -m 0644 data/hako-home.conf $(DESTDIR)/etc/triggerhappy/triggers.d/hako-home.conf
	install -D -m 0644 data/50-hako-launcher.rules $(DESTDIR)$(PREFIX)/share/polkit-1/rules.d/50-hako-launcher.rules
	install -d $(DESTDIR)$(PREFIX)/share/hako/apps

clean:
	rm -f hako-launcher

.PHONY: all install clean
