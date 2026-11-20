# hako-os

hako: a Debian trixie (arm64) based environment for game consoles with Qualcomm
SoCs booted by OpenABL, for deploying our own applications. Current target:
Retroid Pocket Mini (V1, SM8250). Images are built with ELBE. Every component is a .deb.

| dir | content |
|---|---|
| kernel/   | kernel build: pinned upstream + ROCKNIX SM8250 patches/DTS, config fragments -> linux-image .deb |
| packages/ | own Debian packages (below) and board support (rpmini-bsp) |
| samples/  | samples run from /data/dev/apps without packaging (LÖVE) |
| repo/     | reprepro config; built .debs are published here (repo/dists, repo/pool are not tracked) |
| elbe/     | ELBE project XMLs (per board: rpmini-dev.xml) |
| scripts/  | build / deploy / flash helpers |
| tools/    | Android-side scripts for the device setup (backup, OpenABL flash/restore) |
| docs/     | documentation (en / ja) |

## Packages

| package | role |
|---|---|
| hako-base, hako-session | first-boot data partition growing; the game session (cage kiosk compositor, user `hako`, PipeWire, volume keys) |
| hako-launcher | app launcher and system settings (volume, brightness, language, time zone, power), SDL3 |
| hako-dev | development image extras (ssh, debugging tools, apt over the USB link) |
| hako-sdl-hello | SDL3 sample / template |
| hako-love | LÖVE 12 (SDL3) from a pinned upstream snapshot |
| hako-raylib-hello | raylib (SDL3 platform) sample |
| rpmini-bsp | Retroid Pocket Mini board support: ESP/boot entry updates, USB gadget, UCM, pad mapping, panel rotation |

Apps are registered with small `.app` files (`/usr/share/hako/apps`, `/data/dev/apps`) and started from hako-launcher.

## Building

Build host: Debian trixie (x86_64; WSL2 works) with arm64 multiarch development packages and ELBE 15.9.

```sh
git submodule update --init
kernel/build.sh                         # kernel .deb -> out/kernel/ (once; needed by the image)
bash scripts/build-packages.sh          # changed packages -> out/packages/
bash scripts/build-image.sh dev         # dev image -> out/images/dev/rpmini-dev.img
bash scripts/dev-update.sh <pkg>...     # update a device on the USB link (192.168.7.2)
```

Documentation: [English](docs/en/README.md) / [日本語](docs/ja/README.md)

1. preparing a Retroid Pocket Mini (backup, OpenABL, writing the image) — this part is specific to that device
2. the daily development loop and the samples (SDL3, LÖVE, raylib, Siv3D, Kirikiri Z)
3. packaging an app and building an image with it
4. bringing up another OpenABL + Qualcomm handheld

## Working with an AI agent

The workflow is designed to be driven together with an AI coding agent: the agent builds, deploys,
reads logs and takes screenshots; the human handles the device and makes the irreversible decisions.
[AGENTS.md](AGENTS.md) (also read through CLAUDE.md) is the entry point for agents.

## Status

Experimental. Booting needs [OpenABL](https://github.com/OpenHandheldCollective/OpenABL) on the device, which replaces the stock
bootloader (ABL); back up the device-specific partitions before trying anything.

## License

hako-os itself is MIT licensed (see LICENSE). Third-party parts keep their own licenses:

- [Linux kernel](https://www.kernel.org/) and the [ROCKNIX](https://rocknix.org/) SM8250 patches/DTS it is built with
  (from [ROCKNIX/distribution](https://github.com/ROCKNIX/distribution), fetched at build time): GPL-2.0
- UCM configuration in rpmini-bsp (from ROCKNIX's patches to [alsa-ucm-conf](https://github.com/alsa-project/alsa-ucm-conf)): BSD-3-Clause
- [LÖVE](https://love2d.org/) (submodule, packages/hako-love/love): zlib license
- [raylib](https://www.raylib.com/) (submodule, packages/hako-raylib-hello/raylib): zlib license
- Firmware listed in kernel/firmware.list (fetched at build time from
  [linux-firmware](https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git)): see its WHENCE file
