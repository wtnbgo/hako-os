# 3. Packaging an app (.deb → dedicated image)

[日本語](../ja/3-アプリをまとめる.md)

How to turn an app that runs via the transfer method in [2-development-and-samples.md](2-development-and-samples.md) into a distributable form.

1. Make the app a .deb (as a package it can be installed with apt and included in an image)
2. Decide how it starts (picked from the launcher / only that app runs when powered on)
3. Build an image containing the app (a single pack that runs once written to SD)

> The current image is only the "development (dev)" kind. Product-oriented mechanisms such as a read-only root,
> A/B updates and automatic rollback don't exist yet ("Future work" in [reference.md](reference.md)).

## 1. Make the app a .deb

Create `packages/<package name>/` and build it for arm64 with `scripts/build-packages.sh <package name>`.
The quickest way to get a template is to copy a sample.

| Kind of app | Template | Notes |
|---|---|---|
| C / C++ built from source | `packages/hako-sdl-hello/` (Makefile), `packages/hako-raylib-hello/` (CMake + submodule), `packages/hako-raylib-classics/` (upstream sources used unchanged) | debhelper builds it as-is |
| LÖVE game | (example below) | `Depends: hako-love`; just include the game folder or `.love` |
| Prebuilt executable (Siv3D, Kirikiri Z, etc.) | (example below). Siv3D: `packages/hako-siv3d-games/` (debian/rules runs siv3d_app.py) | `Architecture: arm64`; just install the files. Watch out for bundled libraries |

Where things go:

| Item | Location |
|---|---|
| Executable | `/usr/bin/<name>` (if there is only one), or `/usr/lib/<package name>/` |
| Data | `/usr/share/<package name>/` (read-only. Write saves under `$HOME` = `/var/lib/hako`) |
| Launcher registration | `/usr/share/hako/apps/<id>.app` |

A minimal `debian/` (example installing prebuilt files):

```
debian/control
  Source: mygame
  Section: games
  Priority: optional
  Maintainer: you <you@example.com>
  Build-Depends: debhelper-compat (= 13)
  Standards-Version: 4.7.0
  Rules-Requires-Root: no

  Package: mygame
  Architecture: arm64
  Depends: ${shlibs:Depends}, ${misc:Depends}
  Description: My game for hako

debian/rules            (make it executable)
  #!/usr/bin/make -f
  %:
  	dh $@

debian/mygame.install
  dist/*        usr/lib/mygame/
  mygame.app    usr/share/hako/apps/

debian/changelog        (the top version becomes the .deb version. Bump it when you change things)
debian/source/format    → 3.0 (native)
```

- For a LÖVE game, use `Depends: hako-love` and set the `.app` to `Exec=/usr/bin/love`, `Args=/usr/share/mygame`.
- If a prebuilt executable bundles its own `libSDL3.so` or similar, make it use that library via `Env=LD_LIBRARY_PATH=...` in the `.app` or
  the executable's RPATH (the version may differ from Debian's SDL3).
- `dpkg-shlibdeps` warnings (bundled library not found) can be ignored if you use the bundled one.
- After building, install it on the device with `bash scripts/dev-update.sh <package name>` to check it.

## 2. Decide how it starts

| Method | How |
|---|---|
| Picked from the launcher (standard) | Just install the `.app`. The launcher lists it |
| Only that app when powered on | Register the app as a `hako-app` alternative with a higher priority than the launcher (priority 50) |
| Swap temporarily | `HAKO_APP=` and `HAKO_APP_ARGS=` in `/etc/default/hako-session` (`/data/dev/session.env` on dev) |

Example postinst / prerm for running only the app (`packages/hako-sdl-hello/debian/` has the same form, with priority 10):

```sh
# debian/mygame.postinst
#!/bin/sh
set -e
if [ "$1" = configure ]; then
	update-alternatives --install /usr/bin/hako-app hako-app /usr/lib/mygame/mygame 100
fi
#DEBHELPER#

# debian/mygame.prerm
#!/bin/sh
set -e
if [ "$1" = remove ]; then
	update-alternatives --remove hako-app /usr/lib/mygame/mygame
fi
#DEBHELPER#
```

- When the app exits, `hako-session` restarts it after 1 second (up to 10 times in 60 seconds). The app doesn't need a "quit" option.
- The volume buttons, HOME button, screen rotation and gamepad mapping are handled by the session and the board package, so the app doesn't need to do anything.

## 3. Build an image containing the app

1. Copy the dev image definition: `cp elbe/rpmini-dev.xml elbe/rpmini-mygame.xml`
2. Edit the new XML:
   - `<name>` and `<description>`
   - `<pkg-list>`: add your package and remove what you don't need
     (for distribution, drop `hako-dev` and the samples. Without `hako-dev`, ssh and the serial console login are not included either)
   - the image file name: `rpmini-dev.img` in `<gpthd>`'s `<name>` and in `<set_packer>` → `rpmini-mygame.img`
   - `<hostname>` and anything else needed
   - `<finetuning>`: adjust the dev-specific lines to match (the `/root/.ssh` chmod, enabling `iwd`, etc. assume hako-dev)
3. Build: `bash scripts/build-image.sh mygame` → `out/images/mygame/rpmini-mygame.img`
   (the ssh key is only included for `dev`; `<archivedir>` points at `build/elbe/overlay`, which is recreated on every build, so leave it)
4. Write it to SD and check that it boots. To distribute, compress with `xz -T0` and include a `sha256sum`.

- `out/images/<kind>/licence-target.txt` lists the licenses of the Debian packages included in the image. Include it when distributing.
- While the image is building (15–20 minutes with KVM), don't run `build-packages.sh` / `publish-repo.sh` / `dev-update.sh`
  (ELBE reads `repo/` throughout the build, so it fails if packages disappear midway).

## Asking an agent (examples)

- "Turn the app running in `/data/dev/mygame` into a .deb in `packages/mygame/`. Make it show up in the launcher, install it on the device and check it."
- "Create an image XML where only mygame starts. Drop hako-dev and the samples, and show me the diff."
- "Build the image. When it's done, tell me how to write it to SD and what to check on first boot."
