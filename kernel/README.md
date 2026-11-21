# kernel

Builds the kernel as Debian packages, one per SoC.

```
kernel/build.sh             # SM8250 (Retroid Pocket Mini) -> out/kernel/linux-image-<ver>-rpmini_*.deb etc.
SOC=SM4450 kernel/build.sh  # SM4450 (Anbernic RG 55G1)    -> out/kernel/linux-image-<ver>-sm4450_*.deb
kernel/build.sh prepare     # stop once the source tree is patched and configured
JOBS=8 kernel/build.sh      # override parallelism (default: nproc)
```

## Per-SoC settings

| file | what |
|---|---|
| `soc/<SOC>.conf` | ROCKNIX device dir, package suffix (`LOCALVER`), `KERNEL_PKGREV`, `TARGET_CPU`, firmware list, work dir (`build/kernel` for SM8250, `build/kernel-<localver>` otherwise), optional `EXTRA_FW_REPO`/`EXTRA_FW_COMMIT` |
| `firmware-<SOC>.list` / `.sha256` | built-in firmware; `extra:<path>` entries come from `EXTRA_FW_REPO` at `EXTRA_FW_COMMIT` |
| `config/soc-<SOC>/*.config` | config fragments merged after the common `config/*.config` |

Every SoC builds the same `linux-libc-dev`; only the SM8250 one is copied to `out/kernel/`.
The kernel tarball is shared (`build/kernel/dl/`).

## Sources (pinned in `SOURCE.pin`)

| what | where |
|---|---|
| kernel | kernel.org `linux-$KERNEL_VERSION.tar.xz` (sha256 checked) |
| patches, DTS, base config | ROCKNIX `distribution` at `$ROCKNIX_COMMIT`, fetched sparse into `kernel/rocknix/` |
| built-in firmware | linux-firmware.git at `$LINUX_FIRMWARE_TAG` (plus ROCKNIX extra-firmware for SM4450), list in `firmware-<SOC>.list`, hashes in `firmware-<SOC>.sha256` |
| our additions | `patches/*.patch`, `dts/qcom/*.dts`, `config/*.config` |

Patch order follows ROCKNIX `scripts/unpack` for the device (SM8250 shown):

1. `packages/linux/patches/mainline/*.patch`
2. `packages/linux/patches/$KERNEL_VERSION/*.patch`
3. `devices/SM8250/patches/linux/*.patch`
4. `kernel/patches/*.patch` (ours)

The applied list is written to `<work dir>/series.txt`.

## Differences from ROCKNIX

- External initramfs (Debian initramfs-tools) instead of a built-in one.
- Uncompressed `Image` with EFI stub is packaged (`CONFIG_EFI_ZBOOT` off), like ROCKNIX's `KERNEL` file.
- overlayfs / erofs / dm (verity) / squashfs-zstd built in for the read-only A/B release image.
- DTBs are registered in `arch/arm64/boot/dts/qcom/Makefile` so `bindeb-pkg` installs them
  to `/usr/lib/linux-image-<ver>/qcom/`.

The device tree for the Mini V1 is `qcom/sm8250-retroidpocket-rpmini.dtb`, for the RG 55G1
`qcom/sm4450-anbernic-rg55g1.dtb`.

SM4450 differs from ROCKNIX in one more point: debug info is off (`config/soc-SM4450/20-no-debug-info.config`),
so no `-dbg` package is built.

## Updating

1. Change `ROCKNIX_COMMIT` (and `KERNEL_VERSION`/`KERNEL_SHA256` if ROCKNIX moved) in `SOURCE.pin`.
2. `kernel/build.sh prepare` - fix any patch that no longer applies.
3. Bump `KERNEL_PKGREV` in each `soc/<SOC>.conf`, run the full build for each SoC.
