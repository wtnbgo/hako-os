# 4. Supporting new devices

[日本語](../ja/4-新しい機種への対応.md)

How to run hako on a device of the same family (Qualcomm-based handheld game consoles that can boot via OpenABL).
The device-independent parts (`hako-*`) are used as-is; the device-specific parts are added separately as a `<device>-bsp` package and an image definition.

> Work on a new device involves rewriting the bootloader. First read [1-device-setup.md](1-device-setup.md)
> as written for the Retroid Pocket Mini, and understand what it does before proceeding.

## Checking the prerequisites

| What to check | Where to look |
|---|---|
| Whether OpenABL supports the device (SoC) | The support list and releases of [OpenABL](https://github.com/OpenHandheldCollective/OpenABL) |
| Whether a Linux kernel and DTS exist | Distributions that run on the device, such as ROCKNIX. Mainline support status |
| Where to get firmware (GPU, DSP, Wi-Fi) | linux-firmware, files bundled with distributions |

- **Devices with the same SoC (SM8250)** (Retroid Pocket 5, Mini V2, Flip 2, etc.) can likely use the kernel as-is, with only DTS and bsp differences.
- **Devices with a different SoC** require redoing everything from the kernel build (`kernel/`).

## How to proceed

### 1. Record a working state (baseline data)

Boot a distribution that runs Linux on the device (ROCKNIX, etc.) from SD, and collect and save the following via ssh or similar.
It serves as the reference for comparison when something doesn't work on hako.

```sh
dmesg; lsmod; cat /proc/cmdline; uname -a
ls -R /lib/firmware; zcat /proc/config.gz
cat /proc/bus/input/devices; amixer contents; aplay -l
ls /sys/class/drm; cat /sys/class/drm/*/modes
```

Also note the ESP boot entries (`loader/entries/*.conf`) and the name of the dtb in use.

### 2. Kernel

- `kernel/SOURCE.pin` and `kernel/build.sh` determine which upstream, patches and configuration are used ([kernel/README.md](../../kernel/README.md)).
- For the same SoC, it is often enough to check that the new device's dtb is built and installed into `/usr/lib/linux-image-*/`.
- For a different SoC, add `kernel/soc/<SOC>.conf`, `kernel/firmware-<SOC>.list` and, if needed, `kernel/config/soc-<SOC>/`,
  then build with `SOC=<SOC> kernel/build.sh`. The package name carries the SoC (`linux-image-<ver>-sm4450`).
  [5-example-rg55g1.md](5-example-rg55g1.md) walks through this for SM4450.

### 3. Board package `<device>-bsp`

Copy `packages/rpmini-bsp/`, rename it, and adapt its contents to the new device.

| Contents | What to change |
|---|---|
| Dependencies (`debian/control`) | Kernel package name, required firmware |
| ESP updates (`/usr/sbin/rpmini-update-esp`, `/etc/default/rpmini-esp`) | dtb name, boot entry file name, kernel command line |
| USB gadget (`usb-gadget` and its service, `*-usb0.network`) | USB controller name (`/sys/class/udc`). Keeping the addresses the same makes things easier |
| UCM (`/usr/share/alsa/ucm2/...`) and WirePlumber settings | Audio routing. Base it on `amixer contents` from the baseline data and the distribution's UCM |
| udev rules | Making the gamepad be treated as a joystick, the rotation matrix for touch coordinates |
| Session defaults (`/usr/lib/hako/session.d/10-<device>.sh`) | `HAKO_ROTATE` (if the panel is portrait), `SDL_GAMECONTROLLERTYPE` / `SDL_GAMECONTROLLERCONFIG` (gamepad mapping) |
| Volume fallback (`/usr/lib/hako/hw/volume`) | Mixer name for when PipeWire isn't available |

- Decide the gamepad mapping while watching `evtest` on the device (the session grabs the gamepad, so run `systemctl stop hako-session` first).
  Write a/b/x/y by position (bottom/right/left/top). The arrangement of printed letters is conveyed to SDL with `SDL_GAMECONTROLLERTYPE`.
- cage handles screen rotation. cage doesn't rotate touch, so pass a correction matrix for the same angle to libinput via a udev rule.

### 4. Image definition

- Copy `elbe/rpmini-dev.xml` to create `elbe/<device>-dev.xml`, and fix the kernel and bsp package names, the hostname,
  and the comments describing the ESP contents.
- Build with `BOARD=<device> bash scripts/build-image.sh dev` (uses `elbe/<device>-dev.xml`).
- For the first boot, select the device in the OpenABL menu and boot.

### 5. Android-side scripts

Copy `tools/rpmini-android/` to create `tools/<device>-android/`, and change the device model (`ro.product.model`), SoC
(`ro.board.platform`), and the OpenABL elf and its hash to those of the device. Do not remove the device model and SoC checks.

### 6. Verifying operation

Fill in the table in "Hardware status" in [reference.md](reference.md) for the new device.

| Item | How to check |
|---|---|
| Display, GPU | The launcher appears, `vulkaninfo --summary`, `kmscube` |
| Gamepad, touch | ABXY diagram, sticks and touch circles in the SDL3 sample (operated by a human) |
| Sound, volume buttons | Sample sound, volume buttons (a human listens) |
| USB networking | `ssh root@192.168.7.2` |
| Wi-Fi, Bluetooth | `iwctl`, `bluetoothctl` |
| Boot time | `systemd-analyze` |

A full worked example (research, preparation, EDL backup and flashing, first boot) is [5-example-rg55g1.md](5-example-rg55g1.md).

## Asking an agent (examples)

- "I want to support the Retroid Pocket 5. Start by checking the prerequisites. Investigate the status of OpenABL, the kernel and the DTS, and give me an estimate of the work."
- "I put baseline data taken on ROCKNIX in `baseline/`. Compare it with rpmini and list what should change in the bsp."
- "Create `packages/rp5-bsp/` from rpmini-bsp. Explain separately what you changed and what needs to be checked on the device."
- "I want to decide the gamepad mapping. I'll watch the evtest output, so tell me which buttons to press in what order."

Work that writes the bootloader or touches partitions must be done by a human, not left to the agent's judgment.
