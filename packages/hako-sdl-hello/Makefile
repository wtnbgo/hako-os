PKG_CONFIG ?= pkg-config
CFLAGS ?= -O2 -g
CFLAGS += -Wall -Wextra $(shell $(PKG_CONFIG) --cflags sdl3)
LDLIBS += $(shell $(PKG_CONFIG) --libs sdl3) -lm
PREFIX ?= /usr

all: hako-sdl-hello

hako-sdl-hello: src/main.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

install: hako-sdl-hello
	install -D -m 0755 hako-sdl-hello $(DESTDIR)$(PREFIX)/bin/hako-sdl-hello
	install -D -m 0644 hako-sdl-hello.app $(DESTDIR)$(PREFIX)/share/hako/apps/hako-sdl-hello.app

clean:
	rm -f hako-sdl-hello

.PHONY: all install clean
