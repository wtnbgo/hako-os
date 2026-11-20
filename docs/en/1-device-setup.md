# 1. Device setup (Retroid Pocket Mini only)

[日本語](../ja/1-実機の準備.md)

> **Warning: these steps are for the Retroid Pocket Mini (V1, SM8250) only.** The scripts check the device model and SoC,
> and do nothing if they don't match. Doing the same on another device may leave it unable to boot.
> For other devices, see [4-new-boards.md](4-new-boards.md).
>
> This rewrites the bootloader (ABL). It is not covered by the manufacturer's warranty, and a failure can leave even Android unable to boot.
> Proceed at your own risk, and only after you understand the steps.

## Overall flow

1. On Android, back up the device-specific partitions (read only)
2. On Android, replace ABL with OpenABL (verify before writing, read back and verify after writing)
3. On the PC, build (or obtain) the dev image and write it to a microSD card
4. Insert the SD card and boot, connect to the PC over USB, and log in with ssh

The built-in Android is not erased. Holding **Volume +** during boot starts Android.

## What you need

- A Retroid Pocket Mini (V1), on which you can run scripts with root privileges on Android
  (Settings → Handheld Settings → Advanced → Run Script as Root)
- A microSD card (8GB or larger. The dev image is 7GiB and expands to fill the card on first boot)
- PC: Debian trixie (WSL2 also works). If you build the image yourself, see "Build environment" in [reference.md](reference.md)
- A USB-C cable (connect **directly** to the PC. Through a hub, USB device mode may not work)
- `abl_signed-SM8250.elf` from OpenABL v1.2 (from the [OpenABL](https://github.com/OpenHandheldCollective/OpenABL) releases;
  sha256 `628dbff74f5b570b897c14725f2c36ff2ca5fe018998e2c248a3d15d906ccef9`)

## 1. Backup

Copy the contents of `tools/rpmini-android/` and the OpenABL elf to the `hako/` folder in the device's internal storage
(`/data/media/0/hako/`, via MTP or adb from the PC).

1. Run `hako/run_backup.sh` with Run Script as Root.
2. `*.img` files, the GPT and `SHA256SUMS` are created in `hako/backup/`. Check `backup.log` for no FAIL.
3. **Copy the whole `hako/backup/` folder to the PC and keep it.** It contains values unique to each unit (calibration data, etc.) that cannot be restored from another unit.

"Run Script as Root" sends each line as a separate command, so run the `run_*.sh` scripts (one-line wrappers).
If files written by root are not visible from the PC, reboot the device and reconnect.

## 2. Write OpenABL

1. Run `hako/run_check.sh`. It writes nothing; it checks the device model, SoC, the elf hash, and whether the current ABL matches the backup.
   The last line of `hako/flash.log` should be `RESULT=CHECK_OK`.
2. Run `hako/run_flash.sh`. It writes abl_a then abl_b, reads them back and verifies. Confirm `RESULT=SUCCESS`.
   - If it is not SUCCESS, **do not reboot**; restore the stock ABL with `hako/run_restore.sh`.

## 3. Prepare the image and write it to SD

```bash
git clone --recursive https://github.com/wtnbgo/hako-os && cd hako-os
kernel/build.sh                        # kernel .deb (once; it goes into the image)
bash scripts/build-image.sh dev        # → out/images/dev/rpmini-dev.img (7GiB, 15–20 minutes with KVM)
```

- The first time, you need to set up the build environment (creating ELBE's initvm takes about 30 minutes). See "Build environment" in [reference.md](reference.md).
- The dev image includes the ssh public key (`~/.ssh/id_*.pub`) of the PC that built it, for root.
- Write it with balenaEtcher or `dd`. Everything on the SD card is erased.

## 4. Boot and connect

1. Power off, insert the SD card and boot. The first time, hold **Volume -** while booting to bring up the OpenABL menu, and choose
   - Device model → **Retroid Pocket Mini** (not V2. Choosing the wrong one gives no display)
   - Boot mode → Linux, Boot source → Auto (or SD card)
   - Start
2. The launcher appears within a few seconds. On first boot, `/data` automatically expands to the end of the SD card.
3. Connect directly to the PC with USB-C and run `ssh root@192.168.7.2`. The PC side gets a 192.168.7.x address automatically.

| Action at boot | Result |
|---|---|
| Press nothing | Linux from the SD card's boot entry |
| Volume - | OpenABL menu |
| Volume + | Built-in Android |

## Reverting

- Android temporarily: Volume + at boot.
- Fully back to stock: `hako/run_restore.sh` on Android (writes back abl_a/abl_b from the backup and verifies).
- If Android won't boot either: in EDL mode (Power + Volume + + Volume -), write back the manufacturer's Android image.
  Restore the device-specific partitions from the backup.

## Asking an agent (examples)

- "Go ahead with step 3 of `docs/en/1-device-setup.md`. If there is no build environment, show me first what you will install."
- "I've written the SD card and booted. Log in with ssh and check hako-session, the launcher, the size of `/data`, and the network."
- "I'll paste the OpenABL flashing log (flash.log); check whether there are any problems." (The flashing itself is done by a human on Android.)

Do not let the agent write the bootloader directly. A human makes the decisions and carries them out; ask the agent for checks and explanations.
