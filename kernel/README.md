# kernel

Builds the Retroid Pocket Mini kernel as Debian packages.

```
kernel/build.sh           # -> out/kernel/linux-image-<ver>-rpmini_*.deb etc.
kernel/build.sh prepare   # stop once build/kernel/src is patched and configured
JOBS=8 kernel/build.sh    # override parallelism (default: nproc)
```

## Sources (pinned in `SOURCE.pin`)

| what | where |
|---|---|
| kernel | kernel.org `linux-$KERNEL_VERSION.tar.xz` (sha256 checked) |
| patches, DTS, base config | ROCKNIX `distribution` at `$ROCKNIX_COMMIT`, fetched sparse into `kernel/rocknix/` |
| built-in firmware | linux-firmware.git at `$LINUX_FIRMWARE_TAG`, list in `firmware.list`, hashes in `firmware.sha256` |
| our additions | `patches/*.patch`, `dts/qcom/*.dts`, `config/*.config` |

Patch order follows ROCKNIX `scripts/unpack` for DEVICE=SM8250:

1. `packages/linux/patches/mainline/*.patch`
2. `packages/linux/patches/$KERNEL_VERSION/*.patch`
3. `devices/SM8250/patches/linux/*.patch`
4. `kernel/patches/*.patch` (ours)

The applied list is written to `build/kernel/series.txt`.

## Differences from ROCKNIX

- External initramfs (Debian initramfs-tools) instead of a built-in one.
- Uncompressed `Image` with EFI stub is packaged (`CONFIG_EFI_ZBOOT` off), like ROCKNIX's `KERNEL` file.
- overlayfs / erofs / dm (verity) / squashfs-zstd built in for the read-only A/B release image.
- DTBs are registered in `arch/arm64/boot/dts/qcom/Makefile` so `bindeb-pkg` installs them
  to `/usr/lib/linux-image-<ver>/qcom/`.

The device tree for the Mini V1 is `qcom/sm8250-retroidpocket-rpmini.dtb`.

## Updating

1. Change `ROCKNIX_COMMIT` (and `KERNEL_VERSION`/`KERNEL_SHA256` if ROCKNIX moved) in `SOURCE.pin`.
2. `kernel/build.sh prepare` - fix any patch that no longer applies.
3. Bump `KERNEL_PKGREV`, run the full build.
