# Notes for AI coding agents

This repository is meant to be worked on together with an AI coding agent (Claude Code, Codex, etc.).
The human owns the device and makes the irreversible or physical decisions; the agent builds,
deploys, reads logs and takes screenshots. Docs: `docs/en/` (English) and `docs/ja/` (Japanese originals); start with `docs/en/README.md`.
When you change a document, update both languages.

## What this is

hako: Debian trixie (arm64) for handheld game consoles with Qualcomm SoCs booted by OpenABL.
Everything on the device is a .deb from `packages/` or Debian; images are built with ELBE.
Boards: Retroid Pocket Mini V1 (SM8250, board package `rpmini-bsp`) and Anbernic RG 55G1 (SM4450, `rg55g1-bsp`).

## Map

| path | what |
|---|---|
| `packages/hako-base/` | hako-base (first-boot data growing), hako-session (cage kiosk session, user `hako`), hako-dev |
| `packages/hako-launcher/` | launcher + settings (C, SDL3). `.app` files register apps |
| `packages/hako-sdl-hello/`, `hako-raylib-hello/`, `hako-love/` | samples / engines |
| `packages/hako-siv3d-games/`, `hako-raylib-classics/` | game collections (Siv3D samples, raylib classics), pad + touch |
| `packages/hako-mesa/` | Mesa 26.2 (freedreno / turnip) in /usr/lib/hako/mesa, for the Adreno 613 |
| `packages/rpmini-bsp/`, `rg55g1-bsp/` | everything board specific (ESP update, USB gadget, UCM, udev, pad mapping, rotation) |
| `kernel/` | kernel .deb (pinned upstream + ROCKNIX patches) |
| `elbe/<board>-<flavor>.xml` | image definitions |
| `scripts/` | `build-packages.sh [pkg..]` (skips unchanged versions), `publish-repo.sh`, `dev-update.sh [pkg..]`, `[BOARD=<board>] build-image.sh <flavor>` (needs the kernel .deb from `kernel/build.sh` first) |
| `tools/edl/hako_edl.py` | backup / check / OpenABL flash / restore over EDL (Windows: QPST, Linux: qdl); per-device table `DEVICES` |
| `tools/rpmini-android/` | fallback: Android-side scripts (run on the device as root) |

## Rules that matter

- **Ask the human before anything physical or irreversible**: flashing ABL/partitions, writing an SD card,
  rebooting into another OS, and any test that needs someone to press buttons, touch the screen or listen.
  Say what to do and what to look for, wait for "ready", then run it.
- Never touch the device's stock partitions except through `tools/edl/hako_edl.py` (or the Android fallback scripts) with a verified
  backup, and only after the human agrees to the write (`flash` / `restore`). `backup` and `check` only read.
- Bump `debian/changelog` whenever a package changes, or apt will not reinstall it
  (`dev-update.sh --reinstall` for a quick try).
- Do not run `build-packages.sh` / `publish-repo.sh` / `dev-update.sh` while `build-image.sh` is running:
  ELBE reads `repo/` during the whole build and fails if packages disappear.
- Do not edit a shell script while it is running (bash reads scripts incrementally).
- Keep the public repo free of private information (local paths, private repository names, customer or
  project names). Generalize instead.

## Device access (dev image)

- USB-C direct to the build host: `ssh root@192.168.7.2` (key from the build host, baked into the dev image).
- Session apps run as user `hako`. Logs: `journalctl _UID=$(id -u hako)`.
- Screenshot (cage/Wayland):
  `runuser -u hako -- env XDG_RUNTIME_DIR=/run/user/$(id -u hako) WAYLAND_DISPLAY=wayland-0 grim -s 0.5 /tmp/s.png`
  then copy it back and look at it.
- Start an app: `/usr/lib/hako/launch-app <id>` (id = `.app` file name). Back to the launcher: HOME long press,
  or `kill -USR1 $(pgrep -o -x hako-launcher)`.
- Dev apps: `/data/dev/<app>/` + `/data/dev/apps/<id>.app`. Session overrides: `/data/dev/session.env`.
- Gamepad buttons: SDL3 a/b/x/y are positions; `HAKO_PAD_LAYOUT=label` swaps them to the printed letters.

## Typical loop

```sh
bash scripts/build-packages.sh <pkg>      # cross-build (arm64) -> out/packages
bash scripts/dev-update.sh <pkg>          # publish + install on the device through an ssh tunnel
ssh root@192.168.7.2 /usr/lib/hako/launch-app <id>
# screenshot / logs, then ask the human to check input and sound
```
