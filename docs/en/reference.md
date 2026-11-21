# hako reference

[日本語](../ja/リファレンス.md)

**hako** is an environment for game handhelds that runs Debian trixie (arm64) on Qualcomm devices that can boot with OpenABL, and deploys your own apps on it.
The current target device is the Retroid Pocket Mini (V1, SM8250).
Device-independent parts live in the `hako-*` packages, and device-specific parts in a `<device>-bsp` package (currently `rpmini-bsp`).
This document collects the details of the layout, packages, configuration and troubleshooting. For the workflow, read documents 1-4 listed in [README.md](README.md) first.

Status as of 2026-11-20.

---

## 1. Overall layout

```
 Windows PC                                               Retroid Pocket Mini (V1)
 ┌──────────────────────────────────────────┐            ┌────────────────────────────────────┐
 │ Build Linux (Debian trixie; WSL2 is OK)  │            │ Internal UFS: Android (still kept) │
 │  ~/hako-os  … this git repository        │            │   abl_a/b = OpenABL v1.2           │
 │   kernel/   → linux-image .deb           │            │                                    │
 │   packages/ → hako-* / rpmini-bsp        │            │ microSD: dev image                 │
 │   repo/     … reprepro (apt repository)  │USB-C direct│   ESP   (boot entries, Image)      │
 │   elbe/     … image definition XML       │◀──────────▶│   rootA (Debian, rw)               │
 │                                          │ NCM + ACM  │   data                             │
 │  ELBE initvm (a libvirt/KVM VM)          │            │                                    │
 │   └ builds the arm64 image from the XML  │            │ 192.168.7.2 (usb0)                 │
 │                                          │            │                                    │
 └──────────────────────────────────────────┘            └────────────────────────────────────┘
```

- **Policy**: everything that goes onto the device is a .deb. The image is built from a minimal configuration with ELBE every time.
  During development, the device is updated with apt instead of rebuilding the image.
- **Boot**: power on → Qualcomm XBL → **OpenABL** (a replacement for ABL) → boot entry on the ESP
  (`loader/entries/*.conf`, the same BLS format as systemd-boot) → Linux.
  - Holding **Volume -** at boot shows the OpenABL menu (choose boot source or device). **Volume +** boots Android.
  - If nothing is held, it boots automatically from the boot entry on the SD card.

## 2. Build environment

| Item | Details | Notes |
|---|---|---|
| Build Linux | Debian trixie (x86_64). WSL2 is fine (KVM must be available) | Cross-builds with arm64 development packages installed via multiarch (`dpkg --add-architecture arm64`) |
| Required packages | `build-essential crossbuild-essential-arm64 debhelper cmake pkgconf reprepro libxml2-utils elbe`, plus the arm64 versions of the Build-Depends in each `debian/control` (`libsdl3-dev:arm64` etc.) | |
| ELBE | 15.9. initvm is at `~/elbe/initvm` (first time: `cd ~/elbe && elbe initvm create`, about 30 minutes) | VM name `initvm` (libvirt). It must run with KVM (`virsh -c qemu:///system dumpxml initvm` shows `<domain type='kvm'>`); on WSL2 set `nestedVirtualization=true` in `.wslconfig` |
| Key for logging in to the device | `~/.ssh/id_*.pub` on the build machine (create one with `ssh-keygen -t ed25519` if there is none) | Registered automatically for root in the dev image |
| Submodules | `git submodule update --init` | Upstream sources of LÖVE and raylib |

## 3. Repository contents

| Path | Role |
|---|---|
| `kernel/` | Packages the kernel as a .deb. kernel.org 7.2 + ROCKNIX's SM8250 patches and DTS. Details in `kernel/README.md` |
| `kernel/SOURCE.pin` | Source versions used (ROCKNIX commit, kernel version and sha256, linux-firmware tag, package version) |
| `kernel/config/*.config` | Fragments layered on top of ROCKNIX's config (for Debian's initramfs, USB gadget, etc.) |
| `kernel/firmware.list` | Firmware built into the kernel (GPU, DSP, etc.) |
| `packages/` | Sources of our own packages (tables below). `hako-base` (hako-base / hako-session / hako-dev), `hako-launcher`, `hako-sdl-hello`, `hako-love`, `hako-raylib-hello`, `rpmini-bsp` |
| `packages/hako-love/love`, `packages/hako-raylib-hello/raylib` | Upstream sources of LÖVE and raylib (git submodules, pinned versions). Fetch with `git submodule update --init` |
| `samples/` | Samples that are not packaged (placed in `/data/dev/apps/` to run). `love-hello/` checks that LÖVE works |
| `repo/` | reprepro apt repository. Only the configuration (`conf/`) is under git; the contents are generated |
| `elbe/rpmini-dev.xml` | Definition of the dev image (package list, partitions, fstab, finishing commands) |
| `scripts/` | Build and deployment scripts (chapter 5) |
| `out/` | Build outputs (`out/kernel/`, `out/packages/`, `out/images/<kind>/`). Not under git |
| `build/` | Working area (kernel source extraction, logs). Not under git |

### Our own packages

**rpmini-bsp**: device support for the Retroid Pocket Mini (`packages/rpmini-bsp/`). Also used in releases. To port to another device, create the corresponding `<device>-bsp`.

| Contents | Role |
|---|---|
| Dependencies | Kernel, initramfs-tools, firmware-atheros / firmware-qcom-soc, alsa-ucm-conf |
| `/usr/sbin/rpmini-update-esp` | When the kernel or initrd is updated, copies Image, initrd and dtb to the ESP and rewrites the boot entry `loader/entries/rpmini.conf` (called automatically from kernel and initramfs hooks) |
| `/etc/default/rpmini-esp` | Settings for the above (ESP location, dtb, kernel command line) |
| `/usr/lib/rpmini/usb-gadget` and `rpmini-usb-gadget.service` | Puts USB-C into device mode and exposes networking (NCM) and serial (ACM) |
| `50-rpmini-usb0.network` | usb0 = 192.168.7.2/24. Hands out an address to the connected PC via DHCP |
| UCM (`ucm2/Qualcomm/sm8250/`) | Audio path configuration. Derived from ROCKNIX. The speaker's hardware volume (digital -6dB) is set here |
| `51-rpmini.conf` (WirePlumber) | Uses software volume, sets output to S16LE (24-bit comes out quiet through the q6 DSP), initial volume |
| `/usr/lib/hako/hw/volume` | Volume buttons when PipeWire is absent (raises/lowers hardware volume). Called from `/usr/lib/hako/volume` |
| `70-rpmini-gamepad.rules` | Makes the built-in pad be treated as a joystick (udev misdetects it as a mouse) |
| `71-rpmini-wcd938x-nosleep.rules` | Disables power saving on the headphone chip (WCD938x). Without this the jack is not detected |
| `72-rpmini-touch-rotate.rules` | Rotates touch coordinates to match cage's rotation (libinput calibration matrix) |
| `/usr/lib/hako/session.d/10-rpmini.sh` | Device defaults for the session: `HAKO_ROTATE=270` (portrait panel), `SDL_GAMECONTROLLERTYPE` / `SDL_GAMECONTROLLERCONFIG` for the built-in pad |

**hako-base**: device-independent foundation (`packages/hako-base/`). Also used in releases. hako-session depends on it.

| Contents | Role |
|---|---|
| `/usr/lib/hako/grow-data` and `hako-grow-data.service` | On first boot, grows the partition labeled `data` to the end of the disk (`sgdisk -e` → `growpart` → `resize2fs`). When done, creates `/var/lib/hako-base/grow-data.done` and never runs again. Does not run at install time |

**hako-dev**: for development (`packages/hako-base/`). Not included in releases.

| Contents | Role |
|---|---|
| Dependencies | ssh server, gdbserver, strace, evtest, alsa-utils, iwd, mesa (GL/Vulkan), kmscube, vulkan-tools, libsdl3, grim, etc. |
| Autologin on ttyGS0 | Immediate root login over USB serial |
| `60-hako-wlan.network` | Uses Wi-Fi with DHCP |
| `/etc/hako/session.d/90-dev-override.sh` | Loads `/data/dev/session.env` if it exists (for replacing the app that is started; not in releases) |
| `hako-dev.conf` (tmpfiles) | Creates `/data/dev` and `/data/dev/apps` |

**hako-session**: automatically starts an app full-screen at boot (`packages/hako-base/`). Also used in releases.

| Contents | Role |
|---|---|
| `hako-session.service` | Starts `session-run` on tty1 as the regular user `hako`. Restarts 1 second after exit (up to 10 times in 60 seconds). Mutually exclusive with the getty on tty1 |
| `/usr/lib/hako/session-run` | Loads the environment settings and starts the app inside cage (if `HAKO_COMPOSITOR` is empty, `exec`s it directly) |
| `/usr/lib/hako/session-app` | Inside cage, rotates the output with `wlr-randr` and starts the app with `SDL_VIDEO_DRIVER=wayland` |
| Environment settings | Loaded in order: `/usr/lib/hako/session.d/*.sh` (device defaults, installed by the bsp) → `/etc/default/hako-session` (`HAKO_COMPOSITOR=cage`, `SIV3D_FULLSCREEN`, `HAKO_APP`, `HAKO_APP_ARGS`, etc.) → `/etc/hako/session.d/*.sh` (additions) |
| `/usr/lib/hako/volume` and triggerhappy configuration | Volume buttons → raise/lower PipeWire volume by 5% (if PipeWire is absent, the device's `/usr/lib/hako/hw/volume`). triggerhappy runs as root (`triggerhappy.service.d/hako.conf`) |
| sysusers / tmpfiles | User `hako` (groups video/render/input/audio, home `/var/lib/hako`) |

- The app that is started is `/usr/bin/hako-app`. Using Debian alternatives, each app package registers itself in its postinst
  (`update-alternatives --install /usr/bin/hako-app hako-app /usr/bin/<app> <priority>`). The one with the highest priority is chosen.
- A logind session is created through PAM (`PAMName=login`), so the display and input devices are usable without root (uaccess). `XDG_RUNTIME_DIR` is also set.
- By default `hako-launcher` (below) is `hako-app`, and each app is started from the launcher.
  If the launcher itself exits, it is restarted immediately.

Versions are determined by the top entry of each source's `debian/changelog` (hako-session and hako-dev share the same version in `packages/hako-base/`).
The old names (rpmini-session / rpmini-dev / rpmini-launcher / rpmini-sdl-hello) are replaced via Conflicts / Replaces.

**hako-sdl-hello**: SDL3 sample (`packages/hako-sdl-hello/`). A template for writing apps.

- A single C file (`src/main.c`) + Makefile + debian/. Cross-built for arm64 on the PC (`build-packages.sh` builds every package with `-a arm64`).
- Draws on a 1280x960 landscape canvas. By default (inside cage) it is shown as-is on the already-rotated landscape screen. When drawing directly to KMSDRM without cage, it rotates its output 90 degrees itself onto the portrait panel (`HAKO_ROTATE` / `--rotate`).
- Shows the pad (Switch-layout button names), touch and FPS. Long-press Start + Select to quit.
  The ABXY diagram is drawn by position (with `HAKO_PAD_LAYOUT=label` it is rearranged to match the printed letters; see chapter 6, "SDL3 and the gamepad").
- `hako-sdl-hello [--rotate 0|90|180|270] [--seconds N] [--shot file.bmp]`. `--shot` saves the screen as a BMP (for checking rendering remotely).
- When running from ssh, use `systemd-run --unit=sdl-hello --collect hako-sdl-hello` so it keeps running after ssh disconnects. Logs are in `journalctl -u sdl-hello`.
- Installs `/usr/share/hako/apps/hako-sdl-hello.app` for the launcher (an example for app packages).

**hako-love**: LÖVE 12 (a Lua game framework, SDL3 version) (`packages/hako-love/`).

- Debian's `love` is 11.x (SDL2), so we build the upstream development version (main, 12.0) pinned as a submodule.
  We have not modified it, so there is no fork (if changes become necessary, fork it and change the submodule's remote).
- Dependency libraries (SDL3, LuaJIT, freetype, harfbuzz, OpenAL, modplug, theora, vorbis) come from Debian.
  The build is the same arm64 cross-build as the others (cmake, about 7 minutes). Installs `/usr/bin/love` and `liblove-12.0.so`. Conflicts with Debian's `love`.
- To move to a newer upstream, change the submodule commit and bump the version in `debian/changelog` (`12.0~git<date>.<commit>-1`).
- Vulkan (Turnip) is chosen for rendering. This can be changed with `LOVE_GRAPHICS_USE_OPENGLES=1` or `t.graphics.renderers` in `love.conf`.
- Register games with a `.app` file using `Exec=/usr/bin/love` and `Args=<game folder or .love>` (`samples/love-hello/love-hello.app` is an example).

**hako-raylib-hello**: SDL3-based sample for raylib (a C game library) (`packages/hako-raylib-hello/`).

- raylib is pinned to an upstream release (6.2) as the submodule `raylib/` and linked statically into the sample (`PLATFORM=SDL`; SDL3 is Debian's,
  found via `find_package`). The only runtime dependency is libsdl3. Sound goes through raylib's own miniaudio to ALSA → PipeWire.
- Rendering is OpenGL 4.6 (freedreno). 1280x960, 60fps.
- 3D scene: move with the left stick / D-pad, jump with the bottom button, camera with the right stick, zoom with L2/R2. Shows the ABXY diagram (by position,
  also supports `HAKO_PAD_LAYOUT=label`), sticks, touch and FPS. ABXY play sounds. START+SELECT quits.
- raylib's button names (`GAMEPAD_BUTTON_RIGHT_FACE_DOWN` etc.) are positions, the same as SDL.
- Verified on the device (2026-11-20): movement, jump, camera, zoom, sound, touch, quit.

**hako-launcher**: a launcher that starts apps from a list, doubling as system settings (`packages/hako-launcher/`). Also used in releases.

- C + SDL3 (SDL3_ttf / SDL3_image). `src/` contains main (screen and input), apps (loading and starting `.app` files), sys (volume, brightness, power, info), i18n (Japanese/English), ui (text rendering).
- Registers itself in postinst as the `hako-app` alternative with priority 50.
- Apps tab: shows registered apps as 3x3 tiles. A (or a tap) starts one; the launcher closes its display and audio and waits for it to exit.
  When the app exits, it returns to the list (an error exit or crash shows a message).
- **Long-press the HOME button (1 second)** to terminate the running app and return. triggerhappy (`/etc/triggerhappy/triggers.d/hako-home.conf`)
  calls `/usr/lib/hako/home-button`, which sends SIGUSR1 to the launcher. The launcher sends SIGTERM to the app's process group, then SIGKILL after 3 seconds.
- Settings tab: volume (a test sound plays when changed), screen brightness (logind's `SetBrightness`), language, time zone (systemd-timedated), external monitor audio (`~/.config/hako/external-audio`, applied by `/usr/lib/hako/audio-route`),
  system info, reboot, power off (power and time zone are allowed for user `hako` via the polkit rule `50-hako-launcher.rules`).
  The time zone is passed to started apps via `TZ`.
- Starting from outside: `/usr/lib/hako/launch-app <id>` (run as root) makes the launcher start a registered app.
  If an app is running, it is closed first. Development tools use this after transferring files (internally it writes the id to `$XDG_RUNTIME_DIR/hako-launch` and sends SIGUSR2).
- Controls: D-pad / left stick to select, A to confirm, B to go back, L/R to switch tabs. Touch also works.
- Languages: Japanese and English. The value chosen in settings is saved to `/var/lib/hako/.config/hako/language`. If unset,
  `HAKO_LANGUAGE` in `/etc/default/hako-session` is used, and if that is also absent, Japanese. Started apps receive `HAKO_LANGUAGE=ja|en`, `LANGUAGE` and `LANG`.

**`.app` file** (app registration): reads `/usr/share/hako/apps/*.app` (installed by packages) and `/data/dev/apps/*.app` (for development; the tile shows "開発" in Japanese, "DEV" in English).

```ini
# Display name (required)
Name=SDL3 サンプル
# Per-language display name (same for Description)
Name[en]=SDL3 Sample
Description=パッド・タッチ・音の確認
# Executable (required; may be relative to the .app; not listed if it cannot be executed)
Exec=/usr/bin/hako-sdl-hello
# Arguments (space-separated)
Args=/data/dev/x/data -fullscreen=yes
# Working directory (defaults to the Exec's directory)
Dir=/data/dev/x
# Tile icon (PNG etc.)
Icon=icon.png
# Environment variable (may appear on multiple lines)
Env=FOO=1
# Sort order (smaller comes first; ties are sorted by name)
Order=10
```

`#` comments must start the line (after a value they become part of the value).

## 4. Device layout (dev image)

### Partitions (SD, GPT)

| # | Label | Size | Mount point | Contents |
|---|---|---|---|---|
| 1 | ESP | 256MiB | `/boot/efi` (vfat) | `loader/entries/rpmini.conf`, `rpmini/Image`, `rpmini/initrd.img`, `dtbs/qcom/*.dtb` |
| 2 | rootA | 6GiB | `/` (ext4, read-write) | The Debian system |
| 3 | data | Remainder | `/data` (ext4) | Free space for your own use |

The image is built at 7GiB. On first boot, hako-base grows data to the end of the card.

### Boot entry

```
title      rpmini (Debian)
linux      /rpmini/Image
initrd     /rpmini/initrd.img
devicetree /dtbs/qcom/sm8250-retroidpocket-rpmini.dtb
options    root=PARTLABEL=rootA rw rootwait console=tty0
```

### Services started automatically

- `rpmini-usb-gadget` (USB networking + serial), `systemd-networkd`, `ssh`
- `serial-getty@ttyGS0` (autologin over USB serial)
- `iwd` (Wi-Fi), `triggerhappy` (volume buttons)
- `hako-session` (app autostart; replaces the getty on tty1)

The app appears about 6.5 seconds after power-on (kernel 2.1 s + userspace 1.8 s + app initialization; bootloader time not included).

### Hardware status

| Item | Status | Notes |
|---|---|---|
| Display | OK | DSI, 960x1280 (portrait is the native orientation), `/dev/dri/card0` |
| GPU | OK | Vulkan = Turnip, GL ES = freedreno (Adreno 650) |
| Gamepad | OK | `Retroid Pocket Gamepad` (evdev). udev misdetects it as a mouse, so `70-rpmini-gamepad.rules` (rpmini-bsp) makes it a joystick. Switch layout: bottom = B = `BTN_SOUTH`, right = A = `BTN_EAST`. L2/R2 are analog (`ABS_HAT2X/Y`) |
| Speaker | OK | PipeWire "Speaker" output. Hardware is `hw:0,1` (MultiMedia2) |
| Headphones | OK | PipeWire "Headphones" output (`hw:0,0`). Switches automatically when plugged in |
| Volume buttons | OK | PipeWire volume in 5% steps. 100% is speaker +6dB |
| Wi-Fi | OK | QCA6390 (ath11k). Connected with iwd |
| DSP | OK | adsp / cdsp / slpi start up |
| Touch panel | OK | `generic ft5x06`. Available as SDL3 finger events (coordinates are relative to the panel's native portrait orientation; conversion for a 90-degree landscape rotation verified with `hako-sdl-hello`) |
| External monitor (USB-C DisplayPort Alt Mode) | Works | Plugged in while running, the screen moves to the monitor (cage `-m last`, `HAKO_CAGE_MODE`). The built-in panel goes dark; 4:3 apps are scaled to the centre, raylib and similar use the whole 16:9. The monitor's touch (USB) and the built-in touch both map to the whole monitor. Sound moves to the monitor too (the launcher setting "External monitor audio" can keep it on the device). The DSP does not answer the first start of DP audio, so a moment of silence is played right after the switch (`/usr/lib/hako/hw/display-audio-prime`). (The earlier stall when the session stopped after unplugging the monitor came from the terminal reset of the USB serial getty; fixed in hako-dev 0.2.11) |
| Bluetooth, headset microphone | Unverified | |

## 5. Common operations

Run all commands in `~/hako-os` in WSL.

### Connecting to the device

1. Connect the device **directly to the PC** with USB-C. Through a hub, USB device mode sometimes did not work.
2. ssh from WSL (the key is on the WSL side, so Windows' ssh cannot log in).

```bash
ssh root@192.168.7.2
```

- The PC side automatically gets a 192.168.7.x address (recognized by Windows' standard driver).
- Serial is also exposed (root autologin on ttyGS0 on the device). Windows shows it with its standard driver as
  "USB Serial Device (COMn)" (Device Manager → Ports; hardware ID `VID_1D6B&PID_0104&MI_02`).
  Open it at 115200 bps (Tera Term, PuTTY, ...) to get a root shell. A fallback for when networking does not work.
- To use Wi-Fi, run `iwctl station wlan0 connect <SSID>` on the device.

### Updating the device's software (everyday development)

```bash
bash scripts/dev-update.sh                     # Update everything installed on the device to the latest of our packages
bash scripts/dev-update.sh --build             # Build packages, then update
bash scripts/dev-update.sh <pkg>...            # Install/update the given packages
bash scripts/dev-update.sh --reinstall <pkg>   # Reinstall even if the version is the same
HAKO_DEV=root@<IP> bash scripts/dev-update.sh   # Change the target (e.g. over Wi-Fi)
```

- How it works: `repo/` is served on 127.0.0.1:8780 in WSL and exposed to the device's 127.0.0.1:8780 with `ssh -R`; the apt source for it (`/etc/apt/sources.list.d/hako-dev-tunnel.list`) exists only while the script runs.
  The same ssh also sets up a reverse SOCKS proxy (`-R 1080`), so official Debian dependencies are fetched through the PC too (no Wi-Fi needed on the device).
- apt **does not reinstall the same version**. When you change a package, bump the version in the changelog (use `--reinstall` if you are just trying something).

### Managing the autostarted app

```bash
systemctl stop hako-session            # Stop it (when testing by running things manually). A reboot restores it
systemctl restart hako-session         # Restart it
journalctl -u hako-session -f          # Watch the app's output
update-alternatives --config hako-app  # Choose from the registered apps
```

To try an app without packaging it (fast iteration with rsync). Registering it with the launcher is the standard way:

```bash
# From the PC
rsync -av build/mygame root@192.168.7.2:/data/dev/
# On the device (the launcher re-reads .app files at startup and when an app exits, not every time the Apps tab is opened)
mkdir -p /data/dev/apps
printf 'Name=mygame\nExec=/data/dev/mygame/mygame\n' > /data/dev/apps/mygame.app
/usr/lib/hako/launch-app mygame        # Start it right away (closes any running app)
```

To replace the session's app itself without going through the launcher, write
`HAKO_APP=/data/dev/mygame/mygame` (`HAKO_APP_ARGS=...` can also be set) to `/data/dev/session.env` and run `systemctl restart hako-session`.
Delete it to go back to the original `/usr/bin/hako-app`. The app runs as the regular user `hako`, so
make sure files under `/data/dev/` are readable by that user.

To view the launcher's screen from the PC: `runuser -u hako -- env XDG_RUNTIME_DIR=/run/user/$(id -u hako) WAYLAND_DISPLAY=wayland-0 grim -s 0.5 /tmp/x.png`
Logs of the launcher and apps: `journalctl _UID=$(id -u hako)`.

### Building our own packages

```bash
bash scripts/build-packages.sh     # packages/*/ whose changelog version is not built yet → out/packages/
bash scripts/build-packages.sh hako-love   # Always build the given ones
FORCE=1 bash scripts/build-packages.sh     # Rebuild everything
bash scripts/publish-repo.sh       # Add out/kernel and out/packages to repo/
```

A package is identified by its changelog version: without a bump, the previous build is reused. Older .debs of a rebuilt package are removed.
`dev-update.sh` and `build-image.sh` call `publish-repo.sh` automatically.

### Building the kernel

```bash
kernel/build.sh            # → out/kernel/linux-image-7.2.0-rpmini_<version>_arm64.deb etc.
kernel/build.sh prepare    # Only apply patches and configure (to check when patches do not apply)
```

- When rebuilding with the same kernel version, bump `KERNEL_PKGREV` in `kernel/SOURCE.pin` (otherwise apt will not update).
- To deploy to the device: `bash scripts/dev-update.sh linux-image-7.2.0-rpmini`. `rpmini-update-esp` rewrites the ESP automatically, so a reboot boots the new kernel.
- If the new kernel does not boot, read the SD card on the PC and restore `rpmini/Image` on the ESP (or rewrite the dev image).

### Building an image and writing it to SD

```bash
bash scripts/build-image.sh dev    # → out/images/dev/rpmini-dev.img (7GiB). About 15–20 minutes with KVM
```

- Does everything in one go: building our changed packages, adding them to the repo, checks, embedding the ssh key, and building in the initvm.
- Before the ELBE build it stops on mistakes that would otherwise fail only at the end: malformed XML, a package that ships an apt source on
  localhost, and a hako/board package in the pkg-list that is not in `repo/`.
- The initvm must run with KVM (`virsh -c qemu:///system dumpxml initvm` shows `<domain type='kvm'>`). Without it the build takes hours.
- Write it with balenaEtcher, `dd` or similar. On WSL, the image is visible at `\\wsl.localhost\<distro name>\...`, but
  copying it to a Windows drive first and writing from there is faster and more reliable.
- **Do not run `build-packages.sh` / `publish-repo.sh` / `dev-update.sh` during the build.** ELBE keeps reading
  `repo/` throughout the build, so if an older .deb disappears midway, it fails with `AptCacheUpdateError`.
- **Rebuild the image only when the base configuration changes (package list or partitions in the XML) or when finalizing a distribution build.** Normally `dev-update.sh` is enough.

## 6. Checking commands on the device

```bash
# Display and GPU
kmscube                                   # Rotating cube (Ctrl+C to quit)
vulkaninfo --summary

# Audio (PipeWire runs as the session user hako; call it from root like this)
U="runuser -u hako -- env XDG_RUNTIME_DIR=/run/user/$(id -u hako)"
$U wpctl status                           # Output (Speaker / Headphones) and volume
$U wpctl set-volume @DEFAULT_AUDIO_SINK@ 0.7
$U speaker-test -D default -c 2 -t sine -f 440 -l 1

# Pad and buttons (the session's app holds the pad, so stop it first)
systemctl stop hako-session; evtest; systemctl start hako-session

# Wi-Fi
iwctl station wlan0 scan; iwctl station wlan0 get-networks

# Overall status
systemctl --failed; dmesg --level=err,warn
```

### Audio setup

- PipeWire + WirePlumber run inside the session (user hako). Apps output to PipeWire through SDL3
  (ALSA's `default` also goes to PipeWire via pipewire-alsa).
- There are two outputs, "Speaker" and "Headphones". Plugging in headphones switches automatically, and volume is remembered per output.
- Key points of the WirePlumber config `/usr/share/wireplumber/wireplumber.conf.d/51-rpmini.conf` (rpmini-bsp):
  - **Fixed at 16-bit (S16LE)**. This DSP accepts 24-bit (S24_LE), but it comes out at about -48dB, nearly silent.
  - **Hardware volume is fixed** (soft-mixer). The speaker is at digital -6dB via UCM, and the default settings set the amp to +12dB each time it is enabled, so
    100% = +6dB. Anything below that is PipeWire's software volume. Initial value 70%.
- When outputting directly to ALSA (with PipeWire stopped), enable the path with `alsaucm -c RetroidPocket set _verb HiFi set _enadev Speaker`
  and output 16-bit to `plughw:0,1`.

### SDL3 and the gamepad

The device defaults (`/usr/lib/hako/session.d/10-rpmini.sh`, rpmini-bsp) pass the following two settings to SDL. Apps need no configuration.

- `SDL_GAMECONTROLLERCONFIG`: button mapping for the built-in pad. SDL's automatic mapping treats the kernel's `BTN_NORTH` (top) under its old name
  `BTN_X` as "X = left", swapping top and left, so the correct mapping is given explicitly.
- `SDL_GAMECONTROLLERTYPE=0x2020/0x3001=SwitchPro`: tells SDL that the printed letters follow the Switch layout (bottom = B, right = A, left = Y, top = X)
  (`SDL_GetGamepadButtonLabel` returns the printed letters).

**Is ABXY positions or printed letters?** (`HAKO_PAD_LAYOUT`, `/etc/default/hako-session`):

- SDL3's a/b/x/y (`SDL_GAMEPAD_BUTTON_SOUTH/EAST/WEST/NORTH`) are positions (bottom/right/left/top; Xbox naming), not printed letters.
  On this device, which has the Nintendo layout, the printed letters and the names are swapped (the bottom button is a, its printed letter is B). This is how SDL is specified.
- `position` (default): SDL's behavior as is. Games that assume Xbox (bottom button = confirm) and apps that draw a pad diagram work correctly as they are.
- `label`: session-run swaps a↔b and x↔y in the mapping and adds `face:abxy`. The a/b/x/y names then match the printed letters
  (Nintendo-style feel). In exchange, SOUTH etc. no longer represent positions, so apps that assume positions (drawing a diagram, bottom button = confirm) are off.
  Only the mapping in `SDL_GAMECONTROLLERCONFIG` (the built-in pad) is swapped. External pads keep SDL's built-in mappings.
- cage does not handle the pad (the app's SDL reads input devices directly), so the swap cannot be done on the compositor side.
- The launcher decides by printed letters (A = confirm, B = back), so it behaves the same either way. To show printed letters in an app,
  use `SDL_GetGamepadButtonLabel` (LÖVE cannot call it from Lua, so check `getGamepadType()` and `HAKO_PAD_LAYOUT` and remap; see `samples/love-hello`).
- Verified on the device (2026-11-20): with both position and label, the printed letters and positions behave as expected in the launcher, the SDL3 sample (including the diagram) and the LÖVE sample.

When trying an app outside the session (started manually from ssh), run `set -a; for f in /usr/lib/hako/session.d/*.sh /etc/default/hako-session; do . $f; done; set +a`
before starting it to get nearly the same conditions (the `HAKO_PAD_LAYOUT` swap is done by session-run, so it does not take effect this way).

## 7. Troubleshooting

### The WSL disk got corrupted

- Symptoms: builds hang, `database disk image is malformed`, Python's `bad marshal data`, etc.
- Check: see whether `sudo dmesg | grep EXT4-fs` shows `Remounting filesystem read-only`.
- Recovery steps:
  1. Stop the broken distro, attach its ext4.vhdx from another distro and run `e2fsck`.
  2. Find broken packages with `debsums -a -c` and `dpkg --audit`, and reinstall them with `apt-get install --reinstall`.
  3. If the initvm is broken, recreate it (below).
- The cause is probably WSL being forcibly terminated. Do not run `wsl --shutdown` or put the PC to sleep during a build.

### ELBE

- Recreating the initvm: `virsh -c qemu:///system destroy initvm; virsh -c qemu:///system undefine initvm`,
  move `~/elbe/initvm` aside, then `cd ~/elbe && elbe initvm create` (about 30 minutes).
- If failed projects remain, list them with `elbe control list_projects` and delete them with `elbe control del_project <path>`.
- `elbe validate` reports `<repodir>` and `<archivedir>` as errors, but they are expanded at submit time, so this is fine.
- `no libvirt domain "initvm"`: libvirt has forgotten the definition (after a WSL restart, etc.). The initvm itself is still there, so
  just re-register it with `virsh -c qemu:///system define ~/elbe/initvm/libvirt.xml`.
- `revision` in the XML is the version of ELBE's schema (max 6), not the image version. Raising it makes validation fail.

### Device

- **Cannot ssh in**: check that USB is connected directly. If that fails, log in over serial (COM port). If that also fails, read the SD card on the PC and look at the logs.
- **Connection dropped during a package update**: run `dpkg --configure -a` on the device, then redo `dev-update.sh`.
  Note that the USB gadget service is configured not to restart on update (restarting it would drop the USB connection).
- **Going back to Android**: holding Volume + at boot starts Android (the internal Android is still there).
  To restore stock ABL in place of OpenABL itself, write back the stock `abl_a` / `abl_b` you dumped before installing it
  (`dd` with Android root. Before installing OpenABL, always back these up together with the device-specific partitions).

### Quirks of "Run Script as Root" (Android)

- It sends the script line by line as separate commands, so make it **a single line, `sh /data/media/0/hako/<script>.sh [args] [> log]`**, that calls the main script.
- Files written by root may appear with stale contents from the PC (adb / MTP). Write to a newly named folder, or output to logcat with `log -t rpmini`.

## 7.5 Running game engines (Kirikiri Z / Siv3D)

Both are built separately as SDL3 versions for arm64 Linux, placed in `/data/dev/<app>/` on the device and registered with a `.app` file
(development mode, before packaging). Prepare a build environment per engine (e.g. a container with Debian trixie + an aarch64 cross compiler +
arm64 development libraries).

### Running a Kirikiri Z title on the device (development mode)

Runs from unpacked data. Put the game data next to the executable.

```bash
rsync -a <build output>/ root@192.168.7.2:/data/dev/<title>/          # krkrz itself and the bundled libSDL3
rsync -a <title>/data/ root@192.168.7.2:/data/dev/<title>/data/
# On the device: register with the launcher (shows up in the list when an app exits; to show it right away, systemctl restart hako-session)
printf 'Name=<title>\nExec=/data/dev/<title>/<executable>\nArgs=/data/dev/<title>/data -fullscreen=yes\n' > /data/dev/apps/<title>.app
```

### Running a Siv3D app on the device

Transfer the built app to `/data/dev/<exeName>/`, write `/data/dev/apps/<exeName>.app`, and
start it with `/usr/lib/hako/launch-app <exeName>`. Siv3D loads assets relative to the working directory, so
set `Dir=` in the `.app` (defaults to the Exec's directory) to the app's folder.

### Screen orientation (portrait panel) and the compositor

The panel is natively portrait (960x1280). When drawing directly to KMS/DRM, apps get a portrait screen and landscape games are shown
small (neither krkrz nor Siv3D has a rotation mechanism).

- **The compositor approach is the default** (since rpmini-bsp 0.1.11; `HAKO_COMPOSITOR=cage` in `/etc/default/hako-session`):
  inside the kiosk compositor cage, `wlr-randr` rotates the output by `HAKO_ROTATE` degrees (270 on the Retroid Pocket Mini; 90 is upside down.
  The device default is in the bsp's `/usr/lib/hako/session.d/10-rpmini.sh`; to change it, use `/etc/default/hako-session`), so apps see a 1280x960 landscape screen.
  cage does not transform touch coordinates, so they are rotated with a libinput calibration matrix (`/usr/lib/udev/rules.d/72-rpmini-touch-rotate.rules`.
  When changing `HAKO_ROTATE`, adjust this matrix too. Verified on the device that the four corners match finger positions at 270 degrees). Apps run on Wayland (SDL picks Wayland when `WAYLAND_DISPLAY` is set).
  Apps receive `HAKO_ROTATE=0` (the output is already rotated, so do not rotate again in the app).
- To draw directly to KMS/DRM without cage, set `HAKO_COMPOSITOR=` (empty) in `/data/dev/session.env` or similar.
- Results: the SDL sample, krkrz and Siv3D all display in landscape. sdl-hello runs at 60fps. Audio (PipeWire) is OK too.
- Full screen: if the app goes full-screen at startup, the engine scales the game screen to 1280x960 keeping the aspect ratio (16:9 gets black bars top and bottom).
  - Kirikiri Z: startup option `-fullscreen=yes` (makes the first window shown full-screen).
    Example: `HAKO_APP_ARGS="/data/dev/<title>/data -fullscreen=yes"`.
  - Siv3D: environment variable `SIV3D_FULLSCREEN=1` (supported by the SDL3 version of Siv3D). Enabled by default in `/etc/default/hako-session`.
  - Verified: krkrz's core demo and the Siv3D template app. KAG-based game data has not been tested on Linux at all yet, so that is separate.
- Checking the screen: `runuser -u hako -- env XDG_RUNTIME_DIR=/run/user/$(id -u hako) WAYLAND_DISPLAY=wayland-0 grim /tmp/s.png`

### Development loop and debugging

- Fast loop: build → `rsync` to `/data/dev/<app>/` → register with `/data/dev/apps/<app>.app` → start from the launcher.
  Long-press HOME to quit and return to the list. When driving this from development tools, combining transfer → writing the `.app` → `launch-app <id>`
  into one command makes it fast.
- Logs: app output does not appear in `journalctl -u hako-session` (it is treated as a user session).
  Use `journalctl -b SYSLOG_IDENTIFIER=session-run` or `journalctl _PID=<pid>`.
- Remote debugging: on the device `gdbserver :2345 ./MyApp`; in WSL `gdb-multiarch` → `set sysroot target:` →
  `target remote 192.168.7.2:2345`. For source-level debugging, build as Debug / RelWithDebInfo.
- Automating krkrz and checking its screen: send TJS through the REPL channel with `-replfile=<dir>` and save a PNG with `Agent.captureScreen()`
  (capture works even when drawing directly to KMS/DRM).

## 8. Future work

- Investigating features (items marked "Unverified" in the table in chapter 4)
  - Headset microphone (UCM Headset input) and headphone buttons.
  - Bluetooth: firmware loading, pairing (controllers and earphones), audio output.
  - External monitor (USB-C DisplayPort Alt Mode): a real fix for the DSP not answering the first DP audio start; the first `typec_displayport` probe fails with `-71` (not yet host) and binds a little later.

- A mechanism to version each app build automatically (since apt does not reinstall the same version).
- Release image: read-only root, A/B slots, rollback based on a boot counter, an update mechanism (our own script at first, later RAUC).
- Moving to the internal UFS (erase Android and write a GPT image to userdata; note that sectors are 4096 bytes).
