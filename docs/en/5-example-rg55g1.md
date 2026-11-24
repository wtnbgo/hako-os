# 5. Worked example: Anbernic RG 55G1

[日本語](../ja/5-実例-RG55G1.md)

This is a record of following [4-new-boards.md](4-new-boards.md) for a real device.
The SoC is different (the Retroid Pocket Mini is SM8250, the RG 55G1 is SM4450), so the kernel is built separately.
It is kept as a model for the next device: **what was looked up, what each decision rests on, and what was checked on the device**.

- The first half (research, preparation) was done in the repository before the device arrived.
- The second half (after arrival) is the on-device procedure and its log.
  - Writing the bootloader worked neither with the planned Android root scripts nor with fastboot; it was done over **EDL** (3.2, 3.3).

## 0. Overview

| Stage | What | Device | Time |
|---|---|---|---|
| Research | Where OpenABL, kernel, DTS, firmware and audio config come from | no | 1-2 h |
| Kernel | Per-SoC kernel builds, build the SM4450 one | no | 1 h + 8 min build |
| bsp | `rg55g1-bsp` from `rpmini-bsp` | no | 1 h |
| Image, Android | `elbe/rg55g1-dev.xml`, `tools/rg55g1-android/` | no | 30 min |
| Backup, OpenABL | Back up over EDL, write OpenABL over EDL | yes | 1-2 h (half a day the first time, finding out how) |
| First boot, tuning | Rotation, touch, pad, audio, USB, Wi-Fi, GPU, clock | yes | half a day+ |

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

- The OpenABL v1.2 release has `abl_signed-SM4450.elf` for SM4450.
  - sha256 `8d73232614bdf50d602dfb934157328b95fc90f1ea22ba71dbc8836bc7aa8a3b`, 532480 bytes.
- ROCKNIX's procedure: install an updated Anbernic app (AnbernicControl.apk) on Android, then `dd` to `abl_a` / `abl_b`
  from "Handheld Settings > Advanced > Root Scripts".
- **On the unit that arrived (stock firmware V3.0.9) neither that nor fastboot worked.**
  - Root Scripts run as root (uid 0) but stay in the SELinux domain `system_app`, which cannot read any partition under
    `/dev/block` (the same after installing the updated app and rebooting).
  - The bootloader is unlocked from the factory (`ro.boot.flash.locked=0`, `verifiedbootstate=orange`), but `fastboot flash`
    fails with `unknown command` (the transfer goes through, "Writing" fails). `fastboot oem edl` too.
  - `adb root` is not available (production build).
- So backup and writing were done over **EDL** (Qualcomm 9008 mode) (3.2, 3.3).
  - `adb reboot edl` enters it. EDL runs before the ABL, so a bad ABL can be put back from EDL.
- The values read on Android are in 4.1.

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

Made from `packages/rpmini-bsp/` with these changes. The state column is after the checks on the device.

| File | Content | State |
|---|---|---|
| `etc/default/rg55g1-esp` | boot entry `rg55g1`, dtb `qcom/sm4450-anbernic-rg55g1.dtb`, kernel options | final |
| `usr/sbin/rg55g1-update-esp` | puts kernel, initrd and dtb on the ESP (looks for the `*-sm4450` kernel) | final |
| `usr/lib/rg55g1/usb-gadget` etc. | USB network and serial; the UDC is the first in `/sys/class/udc` | final (the PC sees NCM and a COM port) |
| `usr/share/alsa/ucm2/...` | ROCKNIX's UCM as is | speakers final; switching to headphones unsolved (4.3) |
| `usr/share/wireplumber/.../51-rg55g1.conf` | S16LE, initial volume | plays; whether S24_LE is quiet is TO CHECK |
| `usr/lib/hako/hw/volume` | volume keys without PipeWire | the mixer controls exist; not tried without PipeWire |
| `usr/lib/hako/session.d/10-rg55g1.sh` | rotation (`HAKO_ROTATE=270`), pad layout and mapping, GPU (hako-mesa) | final |
| `usr/lib/udev/rules.d/72-rg55g1-touch-rotate.rules` | touch rotation matrix (for 270) | final |
| `usr/lib/udev/rules.d/71-rg55g1-wcd937x-nosleep.rules` | keep the WCD937x awake (added after arrival) | in, but does not fix the headphones on its own |

What was **not** taken over from the Retroid Pocket Mini, and why:

| rpmini-bsp file | Why |
|---|---|
| `70-rpmini-gamepad.rules` | works around the Retroid pad being taken for a mouse (no buttons); the RG 55G1 pad reports normal buttons (checked) |
| `71-rpmini-wcd938x-nosleep.rules` | for WCD938x; the headphone jack was detected late on the device, so the same rule was added for WCD937x (table above) |
| `display-audio-prime`, `HDMI-RP.conf` etc. | audio over the video output (DP); the RG 55G1 has none |

### 2.3 Image description

`elbe/rg55g1-dev.xml` was made from `elbe/rpmini-dev.xml`, changing the name, hostname (`rg55g1`),
kernel and bsp package names and the enabled services.

```sh
SOC=SM4450 kernel/build.sh
bash scripts/build-packages.sh                 # into the repository, rg55g1-bsp included
BOARD=rg55g1 bash scripts/build-image.sh dev   # -> rg55g1-dev.img
```

### 2.4 Android and EDL scripts

- `tools/rg55g1-android/`: made from `tools/rpmini-android/`, for the Android root script runner.
  - `rg55g1_backup.sh`: also saves `dtbo_a` and `vendor_boot_a` (to look at the stock DTS later).
  - `openabl_flash.sh` / `openabl_restore.sh`: stop while the device values (`PLAT_EXPECT` / `MODEL_EXPECT`) are empty; they were filled in after arrival.
  - **On V3.0.9 they cannot read the partitions, so they were used neither for the backup nor for writing** (1.2). `run_check.sh` still shows the device values.
- `tools/rg55g1-edl/edl_xml.py`: makes the fh_loader XML files for EDL (added after arrival).
  - `gpt`: read the GPT of each LUN. `backup`: from those GPTs, read the given partitions (and list all in `parts.txt`).
    `flash`: write a file to the given partitions, and read the written range back.
  - Partition locations always come from the GPT, never typed by hand.

## 3. After arrival

> Checks that need eyes, ears or buttons: the agent explains the steps and waits until the person says "ready".

### 3.1 Prepare on Android

1. Finish the first-run setup and apply system updates (it went to V3.0.9).
2. Enable USB debugging in the developer options and check that `adb devices` shows it.
   With other Android devices connected, point at the RG 55G1 with `adb -s <serial>`.
3. Read the device values and the bootloader state (results in 4.1).

```sh
adb shell getprop ro.board.platform      # parrot
adb shell getprop ro.product.model       # RG 55G1
adb shell getprop ro.boot.flash.locked   # 0 (unlocked)
```

### 3.2 Back up over EDL

What is needed (Windows):

| Item | Notes |
|---|---|
| QPST | `QSaharaServer.exe` (sends the loader) and `fh_loader.exe` (reads / writes). The ones from QFIL 2.0.3.5 worked |
| Qualcomm USB driver | the device in EDL shows up as `Qualcomm HS-USB QDLoader 9008 (COMn)` |
| firehose loader `xbl_s_devprg_ns.melf` | the program that reads and writes on the device side. The one in `stock/firmware/` of TheGammaSqueeze/sd-tools was used (signed with Qualcomm test keys, 1.7 MB). Third-party, so it is not in the repository |

Steps (`<dir>` is where the backup goes, `COMn` the COM port that appears in EDL):

```powershell
adb reboot edl                                   # the screen stays black; a new COM port means it is in EDL
QSaharaServer.exe -p \\.\COMn -s 13:xbl_s_devprg_ns.melf
python3 tools/rg55g1-edl/edl_xml.py gpt <dir>
fh_loader.exe --port=\\.\COMn --memoryname=UFS --sendxml=read_gpt.xml --convertprogram2read --search_path=<dir> --mainoutputdir=<dir> --noprompt --zlpawarehost=1
python3 tools/rg55g1-edl/edl_xml.py backup <dir> abl_a,abl_b,persist,devinfo,misc,modemst1,modemst2,fsg,fsc,keystore,frp,uefivarstore,secdata,storsec,ssd,cdt,ddr,splash,logfs,limits,limits-cdsp,apdp,dip,spunvm,dtbo_a,vendor_boot_a
fh_loader.exe --port=\\.\COMn --memoryname=UFS --sendxml=read_parts.xml --convertprogram2read --search_path=<dir> --mainoutputdir=<dir> --noprompt --zlpawarehost=1
fh_loader.exe --port=\\.\COMn --reset --noprompt --zlpawarehost=1   # boot Android again
```

- `--convertprogram2read` turns the `<program>` (write) entries of the XML into reads. Always pass it for a backup.
- Record the sha256 of every file and keep a second copy elsewhere on the PC (the only way back for this unit).
- `abl_a` / `abl_b` are on LUN 4 (4.1): the same data ROCKNIX's script would `dd`, read over EDL.

### 3.3 Write OpenABL over EDL (after the person agrees)

```powershell
python3 tools/rg55g1-edl/edl_xml.py flash <dir> abl_signed-SM4450.elf abl_a,abl_b
adb reboot edl
QSaharaServer.exe -p \\.\COMn -s 13:xbl_s_devprg_ns.melf
fh_loader.exe --port=\\.\COMn --memoryname=UFS --sendxml=write_abl_signed-SM4450.xml --search_path=<dir> --mainoutputdir=<dir> --noprompt --zlpawarehost=1
fh_loader.exe --port=\\.\COMn --memoryname=UFS --sendxml=readback_abl_signed-SM4450.xml --convertprogram2read --search_path=<dir> --mainoutputdir=<dir> --noprompt --zlpawarehost=1
```

1. Check that the sha256 of `readback_abl_a.bin` and `readback_abl_b.bin` is OpenABL's (`8d7323…`).
2. Hold volume down, reset with `fh_loader.exe --port=\\.\COMn --reset ...`, and check that the OpenABL menu appears.
3. To go back to stock, write the backup's `abl_a.img` / `abl_b.img` the same way
   (`edl_xml.py flash <dir> abl_a.img abl_a`, likewise for `abl_b`).

> OpenABL's "Device model" list comes from the dtbs on the inserted microSD card. Without a card it says `no device trees found`; that is not an error.

### 3.4 (optional) Baseline with ROCKNIX

Write ROCKNIX for SM4450 to another microSD card, boot it and record with the commands under "baseline" in [chapter 4](4-new-boards.md).
It is what to compare against when something does not work on hako (skipped this time).

### 3.5 First boot of hako

1. Write `rg55g1-dev.img` to a microSD card; in the OpenABL menu pick **Anbernic RG 55G1** as the device model, then Start.
2. Watch whether it gets from the splash to the launcher.
3. Connect USB and `ssh root@192.168.7.2`.
   - The Retroid Pocket Mini uses the same address, so the ssh host keys clash. Use another name for the RG 55G1:
     `ssh -o HostKeyAlias=rg55g1 root@192.168.7.2`, and `HAKO_SSH_OPTS='-o HostKeyAlias=rg55g1' scripts/dev-update.sh` for updates.
4. If it neither reaches the launcher nor answers over USB:
   - see whether a long press on the power key runs the shutdown (then Linux is up);
   - put the microSD card into a Linux PC, mount rootA read-only and read the log of the last boot:
     `journalctl -D <mnt>/var/log/journal -b -1 -p warning`.
     WSL's `wsl --mount` does not take USB SD card readers, so another Linux machine was used.

### 3.6 Checks and where to fix

| Item | How to check | Where to fix |
|---|---|---|
| Rotation | is the launcher landscape and the right way up | `HAKO_ROTATE` (`10-rg55g1.sh`) |
| Touch | does the SDL3 sample draw the circle under the finger | `72-rg55g1-touch-rotate.rules` |
| Pad | `evtest` for each button; ABXY position vs. print | `SDL_GAMECONTROLLERTYPE` / `SDL_GAMECONTROLLERCONFIG` (`10-rg55g1.sh`) |
| The two buttons below the screen | `evtest` | `SDL_GAMECONTROLLERCONFIG` |
| Audio (speakers, headphones) | sample sound, `aplay -l`, `amixer contents`, `wpctl status` | UCM, `51-rg55g1.conf` |
| Volume keys | does the volume change (`volume up/down` in `journalctl -u triggerhappy`) | `hw/volume` |
| Headphone plug / unplug | jack events (`EV_SW` in `evtest`) and the output | WCD937x, kernel |
| USB network, serial | `ssh root@192.168.7.2`, COM port | `usb-gadget` |
| Wi-Fi / Bluetooth | `iwctl station wlan0 scan`, `bluetoothctl list` | firmware |
| Fan | speed (`/sys/class/hwmon/*/fan1_input`), noise, temperature | decide how to control it |
| Battery | `/sys/class/power_supply` | - |
| GPU | `vulkaninfo --summary`, `eglinfo -p surfaceless`, raylib / LÖVE samples | Mesa (hako-mesa) |
| Clock | `timedatectl`, the time after a reboot | hako-rtc-offset |
| Boot time | `systemd-analyze` | - |

> When recording buttons with `evtest`, stop the launcher first (`systemctl stop hako-session`);
> otherwise pressing A starts an app.

## 4. Log

### 4.1 Values read on Android and over EDL

| Value | Result |
|---|---|
| `ro.board.platform` | `parrot` |
| `ro.product.model` | `RG 55G1` |
| `ro.soc.model` | `SG4250P` |
| Stock firmware | V3.0.9 (system update after arrival) |
| Bootloader | unlocked from the factory (`ro.boot.flash.locked=0`, `verifiedbootstate=orange`); `fastboot flash` not available |
| `abl_a` / `abl_b` | LUN 4, start sector 6 / 262, 256 sectors each (1 MB, 4096-byte sectors); both stock copies identical |
| Backup | over EDL: 26 partitions (243 MB) and the GPTs of 7 LUNs; `msadp` is not in the GPT; `modemst1` is all zeros (Wi-Fi only device) |

### 4.2 Results

| Item | Result | Action |
|---|---|---|
| First boot | black screen after the splash (GPU not supported) | 4.3 item 4; boots in 7.4 s after the fix |
| Rotation | right with `HAKO_ROTATE=270` | - |
| Touch | right position | - |
| Pad | Switch-style print; the left face button is `BTN_NORTH`, the top one `BTN_WEST` (the reverse of the Mini) | explicit mapping, matches the print (rg55g1-bsp 0.1.5) |
| Two buttons below the screen | left `KEY_F10`, right `KEY_BACK` | passed to apps as `guide` (HOME) and `misc1`; the launcher does not use them |
| Audio | speakers play; volume keys work | - |
| Headphones | the output stays on the speakers (unsolved) | 4.3 item 7 |
| USB | NCM and ACM seen by the PC; USB role is device; did not come back after a replug | 4.3 item 8 (rg55g1-bsp 0.1.9) |
| Wi-Fi / BT | scanning works, the BT controller is there; connecting not tried | - |
| Fan | always running (3200 rpm, pwm 102); 34-35 °C | no temperature-based control yet |
| Battery | level and state readable through pmic-glink | - |
| GPU | Mesa 26.2.6 (hako-mesa): `Turnip Adreno (TM) 613`, GL `FD613` | 4.3 item 4 |
| Clock | came up with an April date | 4.3 item 6 (hako-base 0.2.16) |

### 4.3 Problems hit

1. **Root Scripts cannot touch the partitions.** Root, but in SELinux `system_app`: listing and reading `/dev/block` is refused
   through every path (`by-name`, `bootdevice`, `platform/...`, `sde1`, whole disks). → switched to EDL.
2. **`fastboot flash` says `unknown command`.** Unlocked, but Anbernic's ABL has no write command. → wrote over EDL.
3. **OpenABL's device model says `no device trees found`.** Only because no microSD card was inserted.
4. **Black screen after boot** (backlight on, no reaction to buttons).
   - Log: cage `Unable to create the wlroots renderer`, after Mesa's `unsupported GPU id 0x0 / chip id 0xd506010300`.
   - Neither trixie's Mesa 25.0 nor 26.1 in backports knows the Adreno 613; Mesa 26.2 added one line to its GPU list
     (`GPUId(613)` in `freedreno_devices.py`). ROCKNIX ships Mesa 26.2.4, so it works there as is.
   - First the launcher came up with software rendering (`WLR_RENDERER=pixman`, `SDL_RENDER_DRIVER=software`) to go on with the other checks.
   - Then **hako-mesa** was made: Debian's Mesa 26.2.6 orig tarball (version and sha256 pinned), cross-built with only freedreno and
     turnip, without LLVM (about 2 minutes). It lives in `/usr/lib/hako/mesa` and does not replace Debian's Mesa; only the session uses it,
     through `/usr/lib/hako/mesa/env.sh` (LD_LIBRARY_PATH, the glvnd EGL vendor file, the Vulkan ICD). Drop it once Debian's Mesa is 26.2 or later.
   - Build note: by default meson downloads missing parts (libarchive, libxml2) from the net; use `--wrap-mode=nodownload` and Debian's packages.
5. **X and Y swapped.** SDL's automatic mapping does not match how this pad reports the left and top buttons; recorded the presses with `evtest` and wrote an explicit mapping.
6. **The clock said April.** The PMIC RTC (rtc-pm8xxx) keeps counting while the device is off, but Linux cannot set it
   (Android keeps its own offset), and there was no NTP.
   → `hako-rtc-offset` (save the difference between the clock and the RTC, restore it early at boot) and `systemd-timesyncd` in hako-base.
   `dev-update.sh` sets the clock from the PC (apt rejects Release files "from the future" otherwise).
7. **Headphones (unsolved).** A plug is taken for a line out (`Impedance detect ramp error`), so `Headphone Jack` stays `off`;
   the plug is detected late and the unplug not at all. Keeping the WCD937x awake did not help. The kernel's MBHC (DTS thresholds etc.) needs a look.
8. **After unplugging and plugging the cable in again, the PC no longer sees the USB device.** On unplugging, the USB controller (dwc3)
   suspends (`dwc3-qcom a600000.usb: remote wakeup not configured`) and does not come back: the Type-C port sees the partner and the role
   stays device, but the UDC stays `not attached`. Toggling the role (none -> device) or re-binding the gadget does not help; unbinding and
   binding the `dwc3-qcom` driver does.
   -> rg55g1-bsp 0.1.9: when a Type-C partner appears, udev starts `rg55g1-usb-replug.service`, which re-binds the driver if the UDC is not
   `configured` 5 seconds later (`/usr/lib/rg55g1/usb-replug`). About 8 seconds from plugging in until the PC sees it. The first boot without USB
   probably had the same cause.
   How it was found: a script started with `systemd-run` wrote the state (`/sys/class/usb_role/*/role`, `/sys/class/udc/*/state`,
   `/sys/class/typec/port0-partner`) to the journal every second; unplug, plug, reboot, then `journalctl -b -1` (no ssh while USB is gone).

## 5. Lessons for the next device

- **If the same distribution (ROCKNIX) handles the device at the same commit, the kernel only needs one more SoC config file.**
  Create `kernel/soc/<SOC>.conf` and `firmware-<SOC>.list`, then `SOC=<SOC> kernel/build.sh`.
- **Have more than one way to write.** Which of Android root, fastboot and EDL works depends on the stock firmware version.
  EDL runs before the ABL, so a bad ABL can be put back, and the backup can be taken over EDL too.
- **The GPU depends on the Mesa version.** Debian stable's Mesa may not know a new SoC. Go on with the other checks under software rendering
  and provide Mesa separately (hako-mesa).
- **Some boards have an RTC that cannot be set.** Keep the clock with NTP and a saved offset.
- Always mark guessed values TO CHECK. `grep -rn "TO CHECK" packages/<board>-bsp` is the arrival-day work list.
- Make writing scripts **stop while their check values are empty**, so they cannot write to the wrong device by accident.
- When copying the bsp of the previous device, ask for each file why it was needed.
  Workarounds for the previous device (pad taken for a mouse, codec power saving) may be unnecessary, or needed in another form.
- Stop the apps before checking input; otherwise the launcher starts an app while you record buttons.

## Asking an agent (examples)

- "The RG 55G1 arrived. Let's start at 3.1. It is connected over adb; read the device values and the bootloader state."
- "I want a backup over EDL. Follow 3.2; decide the partitions and the destination with me before starting."
- "First boot done and ssh works. Go through the table in 3.6 from the top; for checks that need me, explain first and wait."
- "Write the results into the log in chapter 4, and fix the comments where a TO CHECK can go."
