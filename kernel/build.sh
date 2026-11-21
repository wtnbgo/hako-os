#!/bin/bash
# Build a kernel as Debian packages (linux-image / linux-headers / linux-libc-dev).
#
#   kernel/build.sh                 SM8250 (Retroid Pocket Mini): linux-image-<ver>-rpmini
#   SOC=SM4450 kernel/build.sh      SM4450 (Anbernic RG 55G1):   linux-image-<ver>-sm4450
#   kernel/build.sh prepare         stop after the configured source tree is ready
#
# Inputs (pinned in SOURCE.pin and soc/<SOC>.conf):
#   - kernel.org tarball (KERNEL_VERSION / KERNEL_SHA256)
#   - ROCKNIX patches + DTS + config for the SoC (ROCKNIX_COMMIT, ROCKNIX_DEVICE_DIR)
#   - firmware built into the image (firmware-<SOC>.list: linux-firmware at
#     LINUX_FIRMWARE_TAG, "extra:" entries from ROCKNIX extra-firmware)
#   - our own patches/*.patch, dts/, config/*.config, config/soc-<SOC>/*.config
# Output: out/kernel/*.deb
set -euo pipefail

K=$(cd "$(dirname "$0")" && pwd)
TOP=$(cd "$K/.." && pwd)
. "$K/SOURCE.pin"
SOC=${SOC:-SM8250}
[ -f "$K/soc/$SOC.conf" ] || { echo "no kernel/soc/$SOC.conf"; exit 1; }
. "$K/soc/$SOC.conf"

W=$TOP/$WORKDIR
SRC=$W/src
OUT=$TOP/out/kernel
JOBS=${JOBS:-$(nproc)}
MAKEARGS=(ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LOCALVERSION=-$LOCALVER)

log() { printf '\n==> %s\n' "$*"; }

mkdir -p "$W/dl" "$OUT"

# --- 1. ROCKNIX reference tree ---------------------------------------------------
log "$SOC: ROCKNIX $ROCKNIX_COMMIT"
"$K/fetch-rocknix.sh" >/dev/null
RX=$K/rocknix/projects/ROCKNIX
RXDEV=$K/rocknix/$ROCKNIX_DEVICE_DIR

# --- 2. kernel source --------------------------------------------------------------
# the tarball is shared by every SoC
TAR=$TOP/build/kernel/dl/linux-$KERNEL_VERSION.tar.xz
mkdir -p "$(dirname "$TAR")"
if [ ! -f "$TAR" ]; then
  log "download linux-$KERNEL_VERSION"
  wget -nv -O "$TAR.part" \
    "https://cdn.kernel.org/pub/linux/kernel/v${KERNEL_VERSION%%.*}.x/linux-$KERNEL_VERSION.tar.xz"
  mv "$TAR.part" "$TAR"
fi
echo "$KERNEL_SHA256  $TAR" | sha256sum -c --quiet

log "unpack"
rm -rf "$SRC"
mkdir -p "$SRC"
tar -C "$SRC" --strip-components=1 -xf "$TAR"

# --- 3. patches (same order as ROCKNIX scripts/unpack, then ours) -------------------
log "patch"
SERIES=(
  "$RX"/packages/linux/patches/mainline/*.patch
  "$RX"/packages/linux/patches/"$KERNEL_VERSION"/*.patch
  "$RXDEV"/patches/linux/*.patch
  "$K"/patches/*.patch
)
: > "$W/series.txt"
for p in "${SERIES[@]}"; do
  [ -f "$p" ] || continue
  echo "${p#$K/}" >> "$W/series.txt"
  if grep -qE '^GIT binary patch$|^rename from|^rename to' "$p"; then
    git apply --directory="$SRC" -p1 --whitespace=nowarn --unsafe-paths < "$p" \
      || { echo "FAILED: $p"; exit 1; }
  else
    sed -e "s#@TARGET_CPU@#$TARGET_CPU#g" -e "s#@DEVICE@#$SOC#g" "$p" \
      | patch -d "$SRC" -p1 -s --no-backup-if-mismatch \
      || { echo "FAILED: $p"; exit 1; }
  fi
done
echo "applied $(wc -l < "$W/series.txt") patches"

# --- 4. device trees -------------------------------------------------------------
log "dts"
rsync -a "$RXDEV/linux/dts/" "$SRC/arch/arm64/boot/dts/"
[ -d "$K/dts" ] && rsync -a "$K/dts/" "$SRC/arch/arm64/boot/dts/"
# ROCKNIX names the dtbs on the make command line; register them in the Makefile
# so that dtbs / dtbs_install (and therefore bindeb-pkg) pick them up.
QMK=$SRC/arch/arm64/boot/dts/qcom/Makefile
for d in "$RXDEV"/linux/dts/qcom/*.dts "$K"/dts/qcom/*.dts; do
  [ -f "$d" ] || continue
  b=$(basename "$d" .dts)
  grep -q "$b\.dtb" "$QMK" || echo "dtb-\$(CONFIG_ARCH_QCOM)	+= $b.dtb" >> "$QMK"
done

# --- 5. firmware built into the image ----------------------------------------------
log "firmware ($LINUX_FIRMWARE_TAG)"
FWDIR=$SRC/external-firmware
FWCACHE=$TOP/build/kernel/dl/linux-firmware-$LINUX_FIRMWARE_TAG
XFWCACHE=$TOP/build/kernel/dl/rocknix-extra-firmware-${EXTRA_FW_COMMIT:-none}
mkdir -p "$FWDIR" "$FWCACHE"
FWLIST=()
while read -r src dst; do
  [ -z "$src" ] || [ "${src#\#}" != "$src" ] && continue
  case "$src" in
  extra:*)
    src=${src#extra:}; dst=${dst:-$src}; cache=$XFWCACHE
    url="$EXTRA_FW_REPO/$EXTRA_FW_COMMIT/$src" ;;
  *)
    dst=${dst:-$src}; cache=$FWCACHE
    url="https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/plain/$src?h=$LINUX_FIRMWARE_TAG" ;;
  esac
  if [ ! -f "$cache/$src" ]; then
    mkdir -p "$(dirname "$cache/$src")"
    wget -q -O "$cache/$src.part" "$url" || { echo "firmware download failed: $url"; exit 1; }
    mv "$cache/$src.part" "$cache/$src"
  fi
  mkdir -p "$(dirname "$FWDIR/$dst")"
  cp "$cache/$src" "$FWDIR/$dst"
  FWLIST+=("$dst")
done < "$K/$FW_LIST"
# cfg80211 is built in: it needs regulatory.db before the rootfs is mounted.
# Use the upstream-signed db (the kernel trusts the upstream wireless-regdb key).
cp /usr/lib/firmware/regulatory.db-upstream "$FWDIR/regulatory.db"
cp /usr/lib/firmware/regulatory.db.p7s-upstream "$FWDIR/regulatory.db.p7s"
FWLIST+=(regulatory.db regulatory.db.p7s)
FWSUM=$K/${FW_LIST%.list}.sha256
if [ -f "$FWSUM" ]; then
  (cd "$FWDIR" && sha256sum -c --quiet "$FWSUM") \
    || { echo "firmware checksum mismatch (see kernel/${FWSUM##*/})"; exit 1; }
else
  (cd "$FWDIR" && sha256sum "${FWLIST[@]}") > "$FWSUM"
  echo "NOTE: created kernel/${FWSUM##*/} - review and commit it"
fi

# --- 6. config ---------------------------------------------------------------------
log "config"
cp "$RXDEV/linux/linux.aarch64.conf" "$SRC/.config"
(cd "$SRC" && ./scripts/kconfig/merge_config.sh -m -O . .config "$K"/config/*.config "$K"/config/soc-"$SOC"/*.config >/dev/null)
"$SRC/scripts/config" --file "$SRC/.config" \
  --set-str EXTRA_FIRMWARE "${FWLIST[*]}" \
  --set-str EXTRA_FIRMWARE_DIR external-firmware
make -C "$SRC" -s "${MAKEARGS[@]}" olddefconfig
# report requested options that did not survive olddefconfig
for f in "$K"/config/*.config "$K"/config/soc-"$SOC"/*.config; do
  grep -E '^CONFIG_|^# CONFIG_.* is not set' "$f" | while read -r line; do
    opt=$(echo "$line" | sed -E 's/^# (CONFIG_[A-Za-z0-9_]+) is not set/\1=n/')
    name=${opt%%=*}; want=${opt#*=}
    have=$(grep -E "^$name=" "$SRC/.config" | cut -d= -f2- || true)
    [ -z "$have" ] && have=n
    [ "$have" = "$want" ] || echo "WARN: $name wanted $want, got $have"
  done
done
cp "$SRC/.config" "$W/config-final"

[ "${1:-}" = prepare ] && { log "prepared: $SRC"; exit 0; }

# --- 7. build packages -------------------------------------------------------------
log "bindeb-pkg (-j$JOBS)"
rm -f "$W"/*.deb "$W"/*.buildinfo "$W"/*.changes
make -C "$SRC" -j"$JOBS" "${MAKEARGS[@]}" \
  KBUILD_IMAGE=arch/arm64/boot/Image \
  DTC_FLAGS=-@ \
  KDEB_PKGVERSION="$KERNEL_VERSION-$KERNEL_PKGREV" \
  KDEB_SOURCENAME=linux-$LOCALVER \
  KBUILD_BUILD_USER=$LOCALVER KBUILD_BUILD_HOST=hako-os \
  bindeb-pkg
# every SoC builds the same linux-libc-dev (userspace headers); only the main
# board's (SM8250) goes to the repository so the versions do not clash
for f in "$W"/*.deb; do
  case "$f" in */linux-libc-dev_*) [ "$SOC" = SM8250 ] || continue ;; esac
  mv "$f" "$OUT/"
done
cp "$W/config-final" "$OUT/config-$KERNEL_VERSION-$LOCALVER"
log "done"
ls -l "$OUT"
