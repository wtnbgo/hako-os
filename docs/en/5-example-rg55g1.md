# 5. Worked example: Anbernic RG 55G1

[日本語](../ja/5-実例-RG55G1.md)

This is a record of following [4-new-boards.md](4-new-boards.md) for a real device.
The SoC is different (the Retroid Pocket Mini is SM8250, the RG 55G1 is SM4450), so the kernel is built separately.
It is kept as a model for the next device: **what was looked up, what each decision rests on, and what was checked on the device**.

- The first half (research, preparation) was done in the repository before the device arrived.
- The second half (after arrival) is the on-device procedure and a log to fill in during the work.

## 0. Overview

| Stage | What | Device | Time |
|---|---|---|---|
| Research | Where OpenABL, kernel, DTS, firmware and audio config come from | no | 1-2 h |
| Kernel | Per-SoC kernel builds, build the SM4450 one | no | 1 h + 8 min build |
| bsp | `rg55g1-bsp` from `rpmini-bsp` | no | 1 h |
| Image, Android | `elbe/rg55g1-dev.xml`, `tools/rg55g1-android/` | no | 30 min |
| Backup, OpenABL | Back up from Android, write OpenABL | yes | 30 min |
| First boot, tuning | Rotation, touch, pad, audio, USB, Wi-Fi | yes | half a day+ |

## 1. Research

### 1.1 The hardware

The main sources were ROCKNIX (`projects/ROCKNIX/devices/SM4450/`) and its DTS.
A DTS reads as "the list of this device's hardware".

| Item | Value | Source |
|---|---|---|
| SoC | Qualcomm SM4450 (Snapdragon G1 Gen 2), Cortex-A76 + A55, Adreno 613 | ROCKNIX `options`, DTS `compatible = "anbernic,rg55g1", "qcom,sm4450"` |
| Display | FocalTech FT7131M portrait panel (1080x1920), `rotation = <90>` in the DTS | DTS panel node |
| Touch | FocalTech FT5452 (I2C), `edt-ft5x06` driver | DTS, `CONFIG_TOUCHSCREEN_EDT_FT5X06=y` |
| Pad | `singleadc-joypad` (ADC sticks + GPIO buttons). ABXY are `BTN_SOUTH/EAST/WEST/NORTH`, the d-pad is `ABS_HAT0X/Y`, plus `KEY_BACK` and `KEY_F10` buttons | DTS joypad node, ROCKNIX inputplumber map (`KEY_F10` = Guide) |
| Audio | q6 DSP (AudioReach) -> two AW88166 speaker amps, WCD9370 (WCD937x) for headphones | DTS, UCM `Anbernic/RG55G1` |
| Wi-Fi / BT | WCN3990 (ath10k over SNOC) / WCN3950 | DTS, ROCKNIX `kernel-firmware.dat` |
| Storage | UFS, microSD | DTS |
| Video out | none (no DP alt mode on USB-C) | no DP output in the DTS |
| Fan | `pwm-fan`, not tied to any thermal zone | DTS `fan: pwm-fan` and `cooling-maps` |

> The kernel does not drive the fan by temperature (ROCKNIX controls it from userspace).
> How hako handles it is decided on the device by listening and watching temperatures.

### 1.2 Bootloader (OpenABL)

- The OpenABL v1.2 release has `abl_signed-SM4450.elf`.
  - sha256 `8d73232614bdf50d602dfb934157328b95fc90f1ea22ba71dbc8836bc7aa8a3b`, 532480 bytes.
- Flashing works like on the Retroid Pocket Mini: a root script on Android dd's it to `abl_a` / `abl_b`.
- **The difference is the device check values**.
  - Android's `getprop ro.board.platform` and `ro.product.model` are unknown until read on the device.
  - The flash script therefore always stops while they are empty.

### 1.3 Kernel

- ROCKNIX builds SM4450 from the same 7.2 kernel at the same commit.
  - Device patches are in `devices/SM4450/patches/linux/`. The patch order rule is the same as SM8250 (64 patches in total with the common ones).
  - The config is `devices/SM4450/linux/linux.aarch64.conf`.
- SM8250 and SM4450 share the CPU layout (`cortex-a76.cortex-a55`), but the patches and config differ.
  - So kernel packages are per SoC (`linux-image-7.2.0-rpmini` and `linux-image-7.2.0-sm4450`).
- The built config was checked for the drivers this device needs:

| Option | Value |
|---|---|
| `CONFIG_JOYSTICK_SINGLEADC_JOYPAD` | y |
| `CONFIG_DRM_PANEL_FOCALTECH_FT7131M` | y |
| `CONFIG_SND_SOC_AW88166` | y |
| `CONFIG_SND_SOC_WCD937X` | m |
| `CONFIG_ATH10K_SNOC` | m |
| `CONFIG_SENSORS_PWM_FAN` | m |
| `CONFIG_BATTERY_QCOM_BATTMGR` | m |

### 1.4 Firmware

Built-in firmware comes from two places:

| Source | Files |
|---|---|
| linux-firmware (same tag as SM8250) | `qcom/a630_sqe.fw`, `ath10k/WCN3990/hw1.0/` board-2.bin, firmware-5.bin, wlanmdsp.mbn |
| ROCKNIX extra-firmware repository (pinned commit) | GPU zap (`a613_zap.mbn`), BT (`qca/cmbtfw13.tlv`, `cmnv13*.bin`), DSP (`qcom/sm4450/RG55G1/adsp.mbn`), Wi-Fi controller (`wpss.mbn`), amp tuning (`aw88166_acf.bin`), audio topology (`qcom/sm8450/AnbernicRG55G1-tplg.bin`) |

- Device-specific blobs such as the DSP are not in linux-firmware; ROCKNIX also takes them from extra-firmware.
- `firmware-SM4450.list` marks them with `extra:`, and the source commit is pinned in `kernel/soc/SM4450.conf`.
- The first build records the hashes in `firmware-SM4450.sha256`; later builds check against it.

### 1.5 Audio

- ROCKNIX's UCM (`Anbernic/RG55G1/` and `conf.d/sm8450/AnbernicRG55G1.conf`) is used as is.
  - The card name matches the DTS `model = "AnbernicRG55G1"`.
- The volume-key fallback (no PipeWire) uses the mixer names from that UCM.
  - Speakers: `Left/Right PCM Playback Volume` (0-511). Headphones: `HPHL/HPHR Volume` (0-20).
  - To be checked with `amixer contents` on the device.
- The WirePlumber config starts from the Retroid Pocket Mini one (forced S16LE).
  - It is the same q6 DSP, so check whether the same problem (S24_LE too quiet) appears.

### 1.6 Tips

- **Read the DTS first**. The `compatible` strings tell you most of the drivers, firmware and options needed.
- **Look at the distribution's whole device folder**. ROCKNIX's `devices/<SoC>/` has kernel patches, config,
  firmware list, UCM and pad maps (inputplumber).
- **Compare with a sibling device**. Diffing against another device on the same SoC (here the Retroid Pocket Classic)
  shows what is device specific.
- Fill unknown values with a guess and **write "TO CHECK"**. The on-device checklist is then one grep away.

## 2. Preparation (no device needed)

### 2.1 Per-SoC kernel builds

`kernel/build.sh` selects the target with `SOC=`. Per-SoC differences live in `kernel/soc/<SOC>.conf`.

```sh
kernel/build.sh                 # SM8250 (Retroid Pocket Mini): linux-image-<ver>-rpmini
SOC=SM4450 kernel/build.sh      # SM4450 (Anbernic RG 55G1):    linux-image-<ver>-sm4450
```

| File | Role |
|---|---|
| `kernel/soc/SM8250.conf`, `SM4450.conf` | ROCKNIX device folder, package suffix (`LOCALVER`), package revision (`KERNEL_PKGREV`), firmware list, work folder |
| `kernel/firmware-<SOC>.list` / `.sha256` | built-in firmware and its hashes |
| `kernel/config/soc-<SOC>/*.config` | per-SoC config additions (default hostname, no debug info, ...) |
| `kernel/SOURCE.pin` | shared pins (kernel version, ROCKNIX commit, linux-firmware tag) |

Decisions:

- **Package names include the SoC**, so both kernels can sit in one apt repository.
  - `linux-libc-dev` is identical for every SoC, so only the SM8250 one goes to the repository.
- **ROCKNIX builds SM4450 with debug info**.
  - As is, that produces a 68MB `-dbg` package and a slower build.
  - hako turns it off in `config/soc-SM4450/20-no-debug-info.config`.
- Work folders are per SoC (`build/kernel-sm4450/`); only the kernel tarball is shared.

### 2.2 bsp package `rg55g1-bsp`

Made from `packages/rpmini-bsp/` with these changes:

| File | Content | State |
|---|---|---|
| `etc/default/rg55g1-esp` | boot entry `rg55g1`, dtb `qcom/sm4450-anbernic-rg55g1.dtb`, command line | final |
| `usr/sbin/rg55g1-update-esp` | puts kernel, initrd and dtb on the ESP (looks for `*-sm4450` kernels) | final |
| `usr/lib/rg55g1/usb-gadget` etc. | USB network and serial, binds to the first `/sys/class/udc` entry | TO CHECK (UDC present) |
| `usr/share/alsa/ucm2/...` | ROCKNIX UCM as is | TO CHECK (sound) |
| `usr/share/wireplumber/.../51-rg55g1.conf` | forced S16LE, initial volume | TO CHECK |
| `usr/lib/hako/hw/volume` | volume keys without PipeWire | TO CHECK (mixer names) |
| `usr/lib/hako/session.d/10-rg55g1.sh` | `HAKO_ROTATE=270` | TO CHECK (orientation) |
| `usr/lib/udev/rules.d/72-rg55g1-touch-rotate.rules` | touch rotation matrix (for 270) | TO CHECK |

**Not taken over** from the Retroid Pocket Mini, and why:

| rpmini-bsp file | Why |
|---|---|
| `70-rpmini-gamepad.rules` | Retroid's pad reports no buttons and is taken for a mouse. The RG 55G1 pad reports normal buttons, so it should not be needed |
| `71-rpmini-wcd938x-nosleep.rules` | for WCD938x. The RG 55G1 has a WCD937x; add a similar rule if headphone detection stops working |
| `display-audio-prime`, `HDMI-RP.conf`, ... | DP audio. The RG 55G1 has no video output |

### 2.3 Image description

`elbe/rg55g1-dev.xml` was made from `elbe/rpmini-dev.xml`, changing the name, hostname (`rg55g1`),
kernel and bsp package names and the enabled services.

```sh
SOC=SM4450 kernel/build.sh
bash scripts/build-packages.sh                 # into the repository, rg55g1-bsp included
BOARD=rg55g1 bash scripts/build-image.sh dev   # -> rg55g1-dev.img
```

### 2.4 Android scripts

`tools/rg55g1-android/` was made from `tools/rpmini-android/`.

- `rg55g1_backup.sh`: also saves `dtbo_a` and `vendor_boot_a` (to read the stock DTS later; never written back).
- `openabl_flash.sh`: SM4450 elf and hash. Stops without writing while `PLAT_EXPECT` / `MODEL_EXPECT` are empty.
- `openabl_restore.sh`: likewise stops while `PLAT_EXPECT` is empty.

## 3. After arrival

> Checks that need eyes or ears (buttons, sound, screen) start only after the agent has given the steps
> and the person has answered "ready".

### 3.1 Record and back up from Android

1. Enable USB debugging in developer options and connect to the PC.
2. Copy `tools/rg55g1-android/` to internal storage and run `run_check.sh` with Anbernic's "run script as root".
   Note `platform=... model=...` in `flash.log`.
3. Run `run_backup.sh` and copy all of `hako/backup/` to the PC (the only way back for this unit).
4. Put the values into `PLAT_EXPECT` / `MODEL_EXPECT` of `openabl_flash.sh` and `PLAT_EXPECT` of `openabl_restore.sh`, and commit.

### 3.2 Write OpenABL (done by a person)

5. Check that every test in `run_check.sh` passes, then run `run_flash.sh`.
6. Reboot and check that the OpenABL menu appears (select with the volume keys).

### 3.3 (optional) Baseline with ROCKNIX

7. Boot ROCKNIX for SM4450 from a separate microSD and collect the data listed under "baseline" in [chapter 4](4-new-boards.md).
   It is the reference when something does not work on hako.

### 3.4 First boot of hako

8. Write `rg55g1-dev.img` to microSD and boot it from the OpenABL menu.
9. Watch the log scroll and the launcher come up. If not, connect USB and `ssh root@192.168.7.2`
   (if that fails too, use Wi-Fi, or take the ROCKNIX baseline and compare).

### 3.5 Checks and where to fix

| Item | How to check | Where to fix |
|---|---|---|
| Orientation | launcher shows in landscape, right way up | `HAKO_ROTATE` (`10-rg55g1.sh`) |
| Touch | SDL3 sample draws the circle where touched | `72-rg55g1-touch-rotate.rules` |
| Pad | `evtest` per button; ABXY positions and labels | `SDL_GAMECONTROLLERTYPE` (`10-rg55g1.sh`) |
| `KEY_BACK` / `KEY_F10` buttons | `evtest`; map to the launcher menu? | launcher |
| Sound (speakers, headphones) | sample sound, `aplay -l`, `amixer contents` | UCM, `51-rg55g1.conf` |
| Volume keys | volume changes (with and without PipeWire) | `hw/volume` |
| Headphone plug | switches over, still detected after a while | WCD937x nosleep rule |
| USB network, serial | `ssh root@192.168.7.2`, COM port | `usb-gadget` |
| Wi-Fi / Bluetooth | `iwctl`, `bluetoothctl`, launcher BT screen | firmware |
| Fan | spinning? noise, temperatures (`/sys/class/thermal`) | decide how to control it |
| Battery | `/sys/class/power_supply` | - |
| GPU | `vulkaninfo --summary`, raylib / Siv3D samples | - |
| Boot time | `systemd-analyze` | - |

## 4. Log (filled in after arrival)

### 4.1 Android values

| Value | Result |
|---|---|
| `ro.board.platform` | (pending) |
| `ro.product.model` | (pending) |
| backup date, file count | (pending) |

### 4.2 Results

| Item | Result | Action |
|---|---|---|
| First boot | (pending) | |
| Orientation | (pending) | |
| Touch | (pending) | |
| Pad | (pending) | |
| Sound | (pending) | |
| USB | (pending) | |
| Wi-Fi / BT | (pending) | |
| Fan | (pending) | |

### 4.3 Problems hit

(added after arrival)

## 5. Lessons for the next device

- **If the same distribution (ROCKNIX) supports the device at the same commit, the kernel is "add one SoC file"**:
  create `kernel/soc/<SOC>.conf` and `firmware-<SOC>.list`, then `SOC=<SOC> kernel/build.sh`.
- Mark every guessed value TO CHECK. `grep -rn "TO CHECK" packages/<board>-bsp` is the arrival-day work list.
- Make flashing scripts **stop while the check values are empty**, so a forgotten value cannot lead to writing the wrong device.
- When copying the previous bsp, ask for each file why it was needed.
  Workarounds for the previous device (pad taken for a mouse, codec power saving) may be unnecessary on the new one, or needed in another form.

## Asking an agent (examples)

- "The RG 55G1 arrived. Start from 3.1. I will paste the run_check.sh output; fill in PLAT_EXPECT and MODEL_EXPECT."
- "First boot done and ssh works. Go through the 3.5 table from the top. Give me the steps before any check that needs me."
- "Write the results into the chapter 4 log, and fix the comments where a TO CHECK is now settled."
