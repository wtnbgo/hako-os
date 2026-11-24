# 2. Development basics and samples

[日本語](../ja/2-開発の基本とサンプル.md)

How to install and run apps from the PC on a device booted from the dev image. Prerequisites: [1-device-setup.md](1-device-setup.md) is done,
and you can log in with `ssh root@192.168.7.2`. Run commands from the repository root on the PC.

## How the device works (the minimum)

- At boot, `hako-session` starts the kiosk compositor **cage** as user `hako`, and inside it runs
  the **launcher** (`hako-launcher`) full-screen. cage rotates the portrait panel, so apps see a 1280x960 landscape screen.
- The launcher starts apps from the list of `.app` files and waits for them to finish. **Long-pressing HOME** quits the running app and returns to the launcher.
- Where `.app` files go: `/usr/share/hako/apps/` for packages, `/data/dev/apps/` during development (the tile shows "開発" in Japanese, "DEV" in English).
- Apps are built with SDL3 (or an engine that uses SDL3). Display is Wayland, sound is PipeWire, input is the gamepad and touch.

```ini
# /data/dev/apps/mygame.app
Name=マイゲーム
Name[en]=My Game
# executable
Exec=/data/dev/mygame/mygame
# arguments (space-separated; group with "...")
Args=--fullscreen
# working directory (defaults to the directory of Exec)
Dir=/data/dev/mygame
# tile icon
Icon=icon.png
# environment variable (multiple lines allowed)
Env=FOO=1
# sort order
Order=10
```

`#` comments must start the line (after a value they become part of the value).

## Two development loops

### A. Transfer files and try (fast; use this during development)

```bash
rsync -a build/mygame/ root@192.168.7.2:/data/dev/mygame/
scp mygame.app root@192.168.7.2:/data/dev/apps/
ssh root@192.168.7.2 /usr/lib/hako/launch-app mygame     # start immediately (closes any running app)
```

Apps run as user `hako`, so make sure everything under `/data/dev/` is readable by that user.

### B. Build a package and install it (the proper way)

```bash
bash scripts/build-packages.sh mypkg     # cross-build packages/mypkg for arm64 → out/packages/
bash scripts/dev-update.sh mypkg         # add to the PC's apt repository and install on the device via an ssh tunnel
```

- When you change a package, bump the version in `debian/changelog` (otherwise apt won't reinstall it; for a quick try, use `--reinstall`).
- `dev-update.sh` works even without Wi-Fi on the device (Debian dependencies are also fetched via the PC).
- For how to make packages, see [3-packaging-an-app.md](3-packaging-an-app.md).

## Tools for checking

```bash
# take a screenshot and view it on the PC
ssh root@192.168.7.2 'runuser -u hako -- env XDG_RUNTIME_DIR=/run/user/$(id -u hako) WAYLAND_DISPLAY=wayland-0 grim -s 0.5 /tmp/s.png'
scp root@192.168.7.2:/tmp/s.png .

# app and launcher logs
ssh root@192.168.7.2 'journalctl _UID=$(id -u hako) -f'

# return to the launcher (same as long-pressing HOME)
ssh root@192.168.7.2 'kill -USR1 $(pgrep -o -x hako-launcher)'

# temporarily change session settings (e.g. buttons by printed letters). Delete the file to revert
ssh root@192.168.7.2 'echo HAKO_PAD_LAYOUT=label >> /data/dev/session.env; systemctl restart hako-session'
```

For remote debugging, run `gdbserver :2345 <exe>` on the device, and on the PC `gdb-multiarch` → `target remote 192.168.7.2:2345`.

## Gamepad buttons

- SDL3's a/b/x/y are **positions** (bottom/right/left/top), not printed letters. This device has the Nintendo layout, so the bottom button (printed B) is a.
- To display the printed letters, use `SDL_GetGamepadButtonLabel` (this device is reported to SDL as a Switch layout).
- With `HAKO_PAD_LAYOUT=label`, a/b/x/y follow the printed letters. In exchange, apps that assume positions (drawing a button diagram, bottom button = confirm) will be off.
  The default is `position`. For details, see "SDL3 and the gamepad" in [reference.md](reference.md).
- The samples share the convention that Start + Select quits. Long-pressing HOME always works.

## Samples

Each one lets you check the gamepad, touch, sound and FPS. Use them as templates when starting something new.

| Sample | Language / rendering | Location | How to install |
|---|---|---|---|
| SDL3 sample | C / SDL_Renderer | `packages/hako-sdl-hello/` | Included in the image (`hako-sdl-hello`) |
| raylib sample | C / OpenGL | `packages/hako-raylib-hello/` | Included in the image (`hako-raylib-hello`) |
| raylib classics | C / OpenGL | `packages/hako-raylib-classics/` | Included in the image (`hako-raylib-classics`) |
| LÖVE sample | Lua / Vulkan | `samples/love-hello/` | The runtime `hako-love` is in the image. Transfer the sample (see below) |
| Siv3D game collection | C++ / OpenGL | `packages/hako-siv3d-games/` | Included in the image (`hako-siv3d-games`) |
| Siv3D | C++ / OpenGL | (start from siv3d_app; links below) | Transfer and register with a `.app` |
| Kirikiri Z | TJS / OpenGL | (SDL3 build from krkrz_dev; links below) | Transfer and register with a `.app` |

### SDL3 sample (`hako-sdl-hello`)

Links: [SDL](https://www.libsdl.org/) / [SDL3 Wiki](https://wiki.libsdl.org/SDL3/) / [GitHub](https://github.com/libsdl-org/SDL)

- One C file + Makefile + debian/. The smallest template. ABXY diagram, touch, sound, FPS.
- Also includes rotation handling (`--rotate`, `HAKO_ROTATE`) for drawing directly to KMS/DRM without cage.

### raylib sample (`hako-raylib-hello`)

Links: [raylib](https://www.raylib.com/) / [cheatsheet](https://www.raylib.com/cheatsheet/cheatsheet.html) / [GitHub](https://github.com/raysan5/raylib)

- Pins raylib 6.2 as a submodule and links it statically, configured to run on SDL3 (`PLATFORM=SDL` in `CMakeLists.txt`).
- 3D scene: left stick to move, bottom button to jump, right stick for the camera, L2/R2 to zoom.
- To make your own game with raylib, the quickest way is to copy this whole directory and rename it.


#### raylib classics (`hako-raylib-classics`)

The twelve classic games by raylib's author ([classics of raysan5/raylib-games](https://github.com/raysan5/raylib-games/tree/master/classics), zlib)
in one app: chosen from a menu and played with the pad and touch.

- The original sources are in `classics/` unmodified and are compiled through the wrappers in `src/games/`. The macros in
  `src/hk.h` rename their `main()`, send `BeginDrawing()` / `EndDrawing()` to a render texture per game and the keyboard /
  mouse queries to `src/hk_input.c`.
- Input: arrow keys = d-pad / stick, Space and Enter = A (as printed), P (pause) = START. Touching the screen shows an
  on-screen d-pad and A. In the mouse games (Gorilas, Missile Commander) touch is the mouse, and the stick moves a pointer that A clicks.
- START+SELECT / HOME / Esc / MENU (top left) return to the menu. The licenses (the games, raylib, the notices of the
  third-party code raylib compiles in) are shown from the menu; `licenses/make-licenses.py` regenerates the notices after a raylib update.
- raylib is the submodule of `hako-raylib-hello`, built in statically. Its default font has no Japanese, so the texts are English.

### LÖVE sample (`samples/love-hello`)

Links: [LÖVE](https://love2d.org/) / [Wiki](https://love2d.org/wiki/Main_Page) / [GitHub](https://github.com/love2d/love)

- The LÖVE runtime is `hako-love` (12.0 development version, SDL3). Games run with `love <folder or .love>`.

```bash
rsync -a samples/love-hello/ root@192.168.7.2:/data/dev/apps/love-hello/
scp samples/love-hello/love-hello.app root@192.168.7.2:/data/dev/apps/
ssh root@192.168.7.2 /usr/lib/hako/launch-app love-hello
```

- Lua can't get the printed letters, so it looks at `joystick:getGamepadType()` and `HAKO_PAD_LAYOUT` and remaps accordingly (see main.lua).
- Vulkan is chosen for rendering. You can switch to GLES with `LOVE_GRAPHICS_USE_OPENGLES=1`.

### Siv3D

Links: **starting point [wtnbgo/siv3d_app](https://github.com/wtnbgo/siv3d_app)** (app template for the SDL3 version of Siv3D; `siv3d-app.json` has an `rpmini` target) /
SDL3 version [wtnbgo/OpenSiv3D `develop_sdl3`](https://github.com/wtnbgo/OpenSiv3D/tree/develop_sdl3) /
arm64 build container [wamsoft/steamdev `deckbuild/`](https://github.com/wamsoft/steamdev/tree/master/deckbuild) /
[Siv3D](https://siv3d.github.io/) / [OpenSiv3D (upstream)](https://github.com/Siv3D/OpenSiv3D)

- Build the SDL3 version of OpenSiv3D for arm64 Linux, transfer the app files to `/data/dev/<exeName>/`,
  register it with `/data/dev/apps/<exeName>.app`, and start it with `launch-app`.
- Assets are read by paths relative to the working directory, so set `Dir=` in the `.app` to the app's folder.
- Full screen is enabled by the environment variable `SIV3D_FULLSCREEN=1` (on by default in the session).
- Start from siv3d_app, build with `python3 <OpenSiv3D>/tools/siv3d_app.py --project <app> --target rpmini all`, and
  `... --target rpmini run` transfers the app, writes the `.app` and starts it with `launch-app` in one step
  (the build runs in the rpmini container from steamdev's `deckbuild/`; the device address is `rpmini.device` in `siv3d-app.json`).

#### Siv3D game collection (`hako-siv3d-games`)

The 22 [game samples of Siv3D](https://siv3d.github.io/ja-jp/samples/games/) in one app: chosen from a menu and played
with the pad and touch. It is also an example of a Siv3D app for hako.

- Layout (`packages/hako-siv3d-games/src/`):
  - `Input.hpp`: pad, keyboard, mouse and touch merged into confirm / cancel / directions / a pointer.
    A / B follow the printed labels (SDL tells whether the pad has a Nintendo layout). In games played with the mouse
    the stick moves the pointer and A clicks.
  - `Games.cpp`: the list of games (number, title, controls, scene size); `games/Gnn_*.cpp` are the games.
  - Each game keeps the scene size it was written for (800x600 etc.), scaled with letterboxing (`ResizeMode::Keep`).
  - START+SELECT / HOME / Esc returns to the menu. The licenses are shown from the menu (`assets/licenses/`).
- Where each game comes from and what was changed is at the top of its file. Main changes:
  - 15 puzzle: the picture is made from an emoji instead of a file dialog. Typing: an on-screen keyboard. Rhythm game: the chart is built in.
  - Slot machine: the built-in Heavy typeface instead of an extra font. 2D RPG: the third-party map chip set cannot be shipped,
    so the map is drawn with shapes and emoji (the map layout is the sample's).
- Licenses: 1-12, 21 and 22 are samples of the Siv3D documentation (MIT; the copyright notice and the license text are in
  `assets/licenses/siv3d-docs.txt`); 13-20 come from [Siv3D-Samples](https://github.com/Siv3D/Siv3D-Samples) and
  [Reputeless/games](https://github.com/Reputeless/games) (CC0). The changes for hako are MIT.
- Build: `scripts/build-packages.sh hako-siv3d-games` (it runs `siv3d_app.py --target rpmini stage`, so docker and the
  deckbuild-rpmini image are needed). OpenSiv3D is pinned to a commit by `siv3d.ref` in `siv3d-app.json`; to use a local
  working copy, set `SIV3D_BASE=<OpenSiv3D>`.
- While developing, `SIV3D_BASE=<OpenSiv3D> python3 <OpenSiv3D>/tools/siv3d_app.py --project packages/hako-siv3d-games --target rpmini run`
  transfers and starts it.

### Kirikiri Z

Links: **starting point [wamsoft/krkrz_dev](https://github.com/wamsoft/krkrz_dev)** (the Wamsoft engine and plugins in one repository; SDL3 build and docs) /
[wamsoft/krkrz_linux](https://github.com/wamsoft/krkrz_linux) (wrapper for Linux distribution packages; the x86_64 Steam Linux Runtime, and hako with `ARCH=arm64`) /
arm64 build container [wamsoft/steamdev `deckbuild/`](https://github.com/wamsoft/steamdev/tree/master/deckbuild) /
engine [wamsoft/krkrz](https://github.com/wamsoft/krkrz) / [Kirikiri Z (krkrz/krkrz)](https://github.com/krkrz/krkrz)

With krkrz_linux, one make command builds for arm64, copies to the device, writes the `.app` and starts it.
Clone krkrz_dev and krkrz_linux into the same parent directory.

```sh
# Once: create the rpmini build container (where you cloned steamdev)
bash <steamdev>/deckbuild/deckbuild.sh -T rpmini image

export KRKRZ_BASE=~/work/kirikiri       # parent of krkrz_dev and krkrz_linux
make -C $KRKRZ_BASE/krkrz_linux all ARCH=arm64      # build (about 30 minutes the first time)
make -C $KRKRZ_BASE/krkrz_linux deploy ARCH=arm64   # copy to the device and start (bundled core demo)

# Your own title
PROJECT_DIR=/path/to/title make -C $KRKRZ_BASE/krkrz_linux all deploy ARCH=arm64
```

- Set the target and launch arguments in the title's `linux-config.json` as `"device": {host, baseDir, title, args}`,
  or with `KRKRZ_LINUX_DEVICE` / `KRKRZ_LINUX_DEVICE_DIR` / `KRKRZ_LINUX_SSH_OPTS`
  (defaults: `root@192.168.7.2`, `/data/dev/<title>/`). To copy without starting, use
  `python3 $KRKRZ_BASE/krkrz_linux/tools/krkrz_linux.py deploy --arch arm64 --no-run`.
- deploy writes `/data/dev/apps/<title>.app`, e.g. `Exec=/data/dev/krkrz-sample/krkrz-sample`,
  `Args=/data/dev/krkrz-sample/data -fullscreen=yes`.
- For a quick display check, the bundled demo can tour itself: add `-demotest -demotestcap=/tmp/kzdemo` to `Args`;
  it opens every scene, saves `/tmp/kzdemo/sceneNN.png`, and exits when done.

## Asking an agent (examples)

- "Based on the raylib sample, create a template for a new game in `packages/hako-mygame/`. Install it on the device and show me a screenshot."
- "I want to make a 2D action game with LÖVE. Put it in `samples/` and go as far as transferring and starting it. I'll check the buttons, so tell me the steps."
- "Starting with this .app crashes immediately. Look at the logs and find the cause."
- "Take a screenshot and check that no text is cut off."

For checks that need human operation (buttons, sound, touch), ask the agent to "show the steps and wait until I say I'm ready".
