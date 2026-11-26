# 1. Device setup

[日本語](../ja/1-実機の準備.md)

How to back up the device, replace the bootloader (ABL) with OpenABL, write the dev image to a microSD card and boot it.
Two devices are supported:

| Device | SoC | Device model in the OpenABL menu |
|---|---|---|
| Retroid Pocket Mini (V1) | SM8250 | Retroid Pocket Mini (not V2) |
| Anbernic RG 55G1 | SM4450 | Anbernic RG 55G1 |

Backup and flashing are done from a PC over **EDL** (Qualcomm's 9008 mode).
EDL runs before ABL, so it works without Android and even after a bad ABL write. The PC can run Windows or Linux.

> **Warning**: this rewrites the bootloader. It is not covered by the manufacturer's warranty, and a failure can leave even Android
> unable to boot. Proceed at your own risk, and only after you understand the steps. The script checks the per-device loader and
> OpenABL hashes and writes nothing if they don't match. Do not use it on devices not in the table above ([4-new-boards.md](4-new-boards.md)).

## Overall flow

1. Install the EDL tools on the PC (QPST on Windows, qdl on Linux)
2. Put the device into EDL and back up the device-specific partitions (read only)
3. Put the device into EDL and replace ABL with OpenABL (verify before writing, read back and verify after writing)
4. On the PC, build (or obtain) the dev image and write it to a microSD card
5. Insert the SD card and boot, connect to the PC over USB, and log in with ssh

The built-in Android is not erased. Holding **Volume +** during boot starts Android.

## What you need

- A supported device
- A microSD card (8GB or larger. The dev image is 7GiB and expands to fill the card on first boot)
- A USB-C cable (connect **directly** to the PC. Through a hub, EDL and USB device mode may be unreliable)
- PC: Windows 10/11 or Linux (checked on Debian trixie and Ubuntu 26.04). To build the image yourself: Debian trixie (WSL2 also works)
- This repository (`tools/edl/hako_edl.py`; only the Python 3 standard library is needed)
- Per-device files (not in the repository; get them and check their hashes):

| | Retroid Pocket Mini | Anbernic RG 55G1 |
|---|---|---|
| EDL loader | `prog_firehose_ddr.elf` from Retroid's stock firmware recovery package (for QFIL, Android 13 `RPMini_A13_20250121`) | `xbl_s_devprg_ns.melf` from `stock/firmware/` in TheGammaSqueeze/sd-tools |
| its sha256 | `690bd3920e637d66584314929f1198818b8163a1f84a4d3200c60922defb9d16` | `4268562be8c612b3036fde519a64edac3afd65b8b31c483dcd345b24cc5ee19c` |
| OpenABL v1.2 elf | `abl_signed-SM8250.elf` | `abl_signed-SM4450.elf` |
| its sha256 | `628dbff74f5b570b897c14725f2c36ff2ca5fe018998e2c248a3d15d906ccef9` | `8d73232614bdf50d602dfb934157328b95fc90f1ea22ba71dbc8836bc7aa8a3b` |

Get the OpenABL elf from the [OpenABL](https://github.com/OpenHandheldCollective/OpenABL) releases.
The loader is the program that runs on the device and does the reading and writing; it differs per device (SoC).

## 1. Install the EDL tools on the PC

### Windows

| What | How |
|---|---|
| Python 3 | `winget install Python.Python.3.13` (or from python.org). `py -3` should work |
| QPST 2.7 | provides `QSaharaServer.exe` and `fh_loader.exe`. Install `qpst.win.2.7_installer_00495.1.zip` (`QPST.2.7.496.1.exe` inside) from Retroid's recovery package as administrator. It goes to `C:\Program Files (x86)\Qualcomm\QPST\bin` |
| Qualcomm USB driver | install `Qualcomm_USB_Driver_v1.0.10061.1.zip` (or `qualcomm_hsusb_drv_*.exe`) from the same package as administrator |

In EDL, Device Manager shows **Qualcomm HS-USB QDLoader 9008 (COMn)** under "Ports (COM & LPT)".
The script finds the port by itself (or pass `--port COMn`).

### Linux

- **Ubuntu 26.04**: `sudo apt install qdl` (checked with 2.4).
- **Debian trixie**: qdl in trixie itself is old; install 2.8 from backports.

  ```bash
  printf 'Types: deb\nURIs: http://deb.debian.org/debian\nSuites: trixie-backports\nComponents: main\nSigned-By: /usr/share/keyrings/debian-archive-keyring.gpg\n' | sudo tee /etc/apt/sources.list.d/backports.sources
  sudo apt update && sudo apt install -t trixie-backports qdl
  ```

- To use it without sudo, install the udev rule (members of the plugdev group can then open the device in EDL):

  ```bash
  sudo cp tools/edl/51-qcom-edl.rules /etc/udev/rules.d/ && sudo udevadm control --reload
  ```

- Not tried from WSL2 (passing USB through needs usbipd). Use a plain Linux machine.
- qdl reboots the device at the end of each run, so on Linux the device enters EDL again for the check and for the write (the script waits and tells you).

## 2. Enter EDL

| Device | How |
|---|---|
| Retroid Pocket Mini | Power off and unplug the USB cable. **Hold both Volume + and Volume -** while plugging the cable in, release after 2–3 seconds. Combinations with the power key do not work |
| Anbernic RG 55G1 | Enable USB debugging on Android and run `adb reboot edl` |

- The screen stays black. The device is in EDL when a 9008 device appears on the PC (Device Manager on Windows, `lsusb -d 05c6:9008` on Linux).
- To leave EDL, hold the power key for 10–15 seconds (the script reboots the device itself when it finishes).
- A device left in EDL for a while may not answer (`failed to read sahara request` and the like). Leave EDL and enter it again.

## 3. Backup

Create the backup folder (`<dir>`) and put the loader in it. The device is `rpmini` or `rg55g1`.

```bash
mkdir -p ~/hako-backup && cp prog_firehose_ddr.elf ~/hako-backup/     # RG 55G1: xbl_s_devprg_ns.melf
python3 tools/edl/hako_edl.py rpmini backup ~/hako-backup               # on Windows: py -3 tools\edl\hako_edl.py ...
```

1. When the script prints "waiting for the device in EDL", put the device into EDL.
2. It reads the GPT (first 6 sectors of each LUN) and the per-device partitions (ABL, persist, modemst, devinfo and so on, 26–29 of them),
   and writes `SHA256SUMS` and `parts.txt` (the partition list). The last line should be `RESULT=BACKUP_OK`.
3. **Copy the whole `<dir>` to another place on the PC and keep it.** It contains values unique to each unit (calibration data, etc.) that cannot be restored from another unit.

- Take it **before** writing OpenABL. A backup taken afterwards does not contain the stock ABL (the script warns).
- Partitions written on every boot (logfs, persist, uefivarstore, ...) differ slightly between backups.

## 4. Write OpenABL (after a human agrees)

Put the OpenABL elf next to it and check first.

```bash
python3 tools/edl/hako_edl.py rpmini check ~/hako-backup abl_signed-SM8250.elf
```

- Writes nothing. It checks the loader and elf hashes and the backup's `SHA256SUMS`, reads the current abl_a / abl_b and shows whether
  they are `stock` (the stock ABL in the backup) or `OpenABL`. The last line should be `RESULT=CHECK_OK`.
- The device reboots at the end. Put it into EDL again before `flash`.

If all is well, write.

```bash
python3 tools/edl/hako_edl.py rpmini flash ~/hako-backup abl_signed-SM8250.elf
```

- It checks the current ABL again (stops unless it is stock or OpenABL); after you type `yes` it writes abl_a and abl_b, reads them back
  and verifies. Confirm `RESULT=SUCCESS`.
- On Windows one EDL session covers this check and the write. On Linux the device reboots after the check, so put it into EDL again.
- Only the length of the elf (520 KiB) is written. The rest of the partition keeps its old contents, which does not affect booting.
- If you get `RESULT=VERIFY_FAILED`, do not leave it: go back to stock with `restore` (EDL works regardless of ABL).

## 5. Prepare the image and write it to SD

```bash
git clone --recursive https://github.com/wtnbgo/hako-os && cd hako-os
kernel/build.sh                        # kernel .deb (once; for the RG 55G1: SOC=SM4450 kernel/build.sh)
bash scripts/build-image.sh dev        # → out/images/dev/rpmini-dev.img (for the RG 55G1 add BOARD=rg55g1)
```

- The first time, you need to set up the build environment (creating ELBE's initvm takes about 30 minutes). See "Build environment" in [reference.md](reference.md).
- The dev image includes the ssh public key (`~/.ssh/id_*.pub`) of the PC that built it, for root.
- Write it with balenaEtcher or `dd`. Everything on the SD card is erased.

## 6. Boot and connect

1. Power off, insert the SD card and boot. The first time, hold **Volume -** while booting to bring up the OpenABL menu, and choose
   - Device model → the device in the table at the top (choosing the wrong one gives no display; without an SD card it says `no device trees found`)
   - Boot mode → Linux, Boot source → Auto (or SD card)
   - Start
2. The launcher appears within a few seconds. On first boot, `/data` automatically expands to the end of the SD card.
3. Connect directly to the PC with USB-C and run `ssh root@192.168.7.2`. The PC side gets a 192.168.7.x address automatically.
   Both devices use the same address, so log in to the RG 55G1 under another name: `ssh -o HostKeyAlias=rg55g1 root@192.168.7.2`.

| Action at boot | Result |
|---|---|
| Press nothing | Linux from the SD card's boot entry |
| Volume - | OpenABL menu |
| Volume + | Built-in Android |

| Power key after boot | Result |
|---|---|
| Short press | Suspend (press again to wake) |
| Hold 5 seconds | Power off (also in the launcher settings) |

> Suspend is only a stopgap and does not save much battery (the SoC does not reach its deep power state; see chapter 4 of the reference).
> Power off to keep the battery. The screen turns off by itself after a while without input ("Screen off after" in the settings).

## Reverting

- Android temporarily: Volume + at boot.
- Fully back to stock: `restore` over EDL. It writes back abl_a / abl_b from the backup and verifies.

  ```bash
  python3 tools/edl/hako_edl.py rpmini restore ~/hako-backup
  ```

- If the backup has no stock ABL (taken after writing OpenABL), pass a stock ABL file with `--stock`.
  On the Retroid Pocket Mini, `abl.elf` in the recovery package (Android 13) was the same as the stock ABL (`cf5713…`).
  `check` and `flash` can tell the stock ABL with the same `--stock`.
- If Android won't boot either: write back the manufacturer's recovery package with QFIL or similar. Restore the device-specific partitions from the backup.

## Another way: Android root scripts (Retroid Pocket Mini)

For when EDL cannot be used, the scripts that run on Android are kept (`tools/rpmini-android/`).
Put them and the OpenABL elf in `hako/` in the device's internal storage, and run `run_backup.sh` → `run_check.sh` → `run_flash.sh`
from Settings → Handheld Settings → Advanced → Run Script as Root (`hako/flash.log` shows `RESULT=`).

- In a backup taken with these scripts, the GPT of LUN 4 and 5 was all zeros (Android cannot read them). EDL is more reliable.
- On the RG 55G1's stock firmware (V3.0.9) this way did not work ([5-example-rg55g1.md](5-example-rg55g1.md), 1.2).

## Asking an agent (examples)

- "Go ahead with step 5 of `docs/en/1-device-setup.md`. If there is no build environment, show me first what you will install."
- "I want an EDL backup. Run `hako_edl.py backup` and tell me when to put the device into EDL."
- "I've written the SD card and booted. Log in with ssh and check hako-session, the launcher, the size of `/data`, and the network."

Write ABL (`flash` / `restore`) only after a human agrees. When an agent runs it, a human looks at what will be written
(the `check` result) and answers "go ahead" first.
