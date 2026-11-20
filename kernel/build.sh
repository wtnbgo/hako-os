#!/bin/bash
# Build the rpmini kernel as Debian packages (linux-image / linux-headers / linux-libc-dev).
#
#   kernel/build.sh            full build (fetch, patch, config, bindeb-pkg)
#   kernel/build.sh prepare    stop after the configured source tree is ready
#
# Inputs (all pinned in SOURCE.pin):
#   - kernel.org tarball (KERNEL_VERSION / KERNEL_SHA256)
#   - ROCKNIX patches + DTS + config for SM8250 (ROCKNIX_COMMIT)
#   - linux-firmware blobs built into the image (LINUX_FIRMWARE_TAG, firmware.list)
#   - our own patches/*.patch, dts/, config/*.config
# Output: out/kernel/*.deb
set -euo pipefail

K=$(cd "$(dirname "$0")" && pwd)
TOP=$(cd "$K/.." && pwd)
. "$K/SOURCE.pin"

W=$TOP/build/kernel
SRC=$W/src
OUT=$TOP/out/kernel
JOBS=${JOBS:-$(nproc)}
MAKEARGS=(ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LOCALVERSION=-rpmini)

log() { printf '\n==> %s\n' "$*"; }

mkdir -p "$W/dl" "$OUT"

# --- 1. ROCKNIX reference tree ---------------------------------------------------
log "ROCKNIX $ROCKNIX_COMMIT"
"$K/fetch-rocknix.sh" >/dev/null
RX=$K/rocknix/projects/ROCKNIX
RXDEV=$K/rocknix/$ROCKNIX_DEVICE_DIR

# --- 2. kernel source --------------------------------------------------------------
TAR=$W/dl/linux-$KERNEL_VERSION.tar.xz
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
    sed -e 's#@TARGET_CPU@#cortex-a76.cortex-a55#g' -e 's#@DEVICE@#SM8250#g' "$p" \
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
FWCACHE=$W/dl/linux-firmware-$LINUX_FIRMWARE_TAG
mkdir -p "$FWDIR" "$FWCACHE"
FWLIST=()
while read -r src dst; do
  [ -z "$src" ] || [ "${src#\#}" != "$src" ] && continue
  dst=${dst:-$src}
  if [ ! -f "$FWCACHE/$src" ]; then
    mkdir -p "$(dirname "$FWCACHE/$src")"
    wget -q -O "$FWCACHE/$src.part" \
      "https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/plain/$src?h=$LINUX_FIRMWARE_TAG"
    mv "$FWCACHE/$src.part" "$FWCACHE/$src"
  fi
  mkdir -p "$(dirname "$FWDIR/$dst")"
  cp "$FWCACHE/$src" "$FWDIR/$dst"
  FWLIST+=("$dst")
done < "$K/firmware.list"
# cfg80211 is built in: it needs regulatory.db before the rootfs is mounted.
# Use the upstream-signed db (the kernel trusts the upstream wireless-regdb key).
cp /usr/lib/firmware/regulatory.db-upstream "$FWDIR/regulatory.db"
cp /usr/lib/firmware/regulatory.db.p7s-upstream "$FWDIR/regulatory.db.p7s"
FWLIST+=(regulatory.db regulatory.db.p7s)
if [ -f "$K/firmware.sha256" ]; then
  (cd "$FWDIR" && sha256sum -c --quiet "$K/firmware.sha256") \
    || { echo "firmware checksum mismatch (see kernel/firmware.sha256)"; exit 1; }
else
  (cd "$FWDIR" && sha256sum "${FWLIST[@]}") > "$K/firmware.sha256"
  echo "NOTE: created kernel/firmware.sha256 - review and commit it"
fi

# --- 6. config ---------------------------------------------------------------------
log "config"
cp "$RXDEV/linux/linux.aarch64.conf" "$SRC/.config"
(cd "$SRC" && ./scripts/kconfig/merge_config.sh -m -O . .config "$K"/config/*.config >/dev/null)
"$SRC/scripts/config" --file "$SRC/.config" \
  --set-str EXTRA_FIRMWARE "${FWLIST[*]}" \
  --set-str EXTRA_FIRMWARE_DIR external-firmware
make -C "$SRC" -s "${MAKEARGS[@]}" olddefconfig
# report requested options that did not survive olddefconfig
for f in "$K"/config/*.config; do
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
  KDEB_SOURCENAME=linux-rpmini \
  KBUILD_BUILD_USER=rpmini KBUILD_BUILD_HOST=hako-os \
  bindeb-pkg
mv "$W"/*.deb "$OUT/"
cp "$W/config-final" "$OUT/config-$KERNEL_VERSION-rpmini"
log "done"
ls -l "$OUT"
