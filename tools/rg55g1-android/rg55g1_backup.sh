#!/system/bin/sh
# Anbernic RG 55G1: save this unit's device-specific partitions and GPTs.
# Run it from Android as root (Settings > Handheld Settings > Advanced > Run Script as Root,
# through run_backup.sh). Output: internal storage hako/backup/ (= /data/media/0/hako/backup).
# Read-only: this script never writes to the device's partitions.
# Copy the whole folder to a PC afterwards and keep it: it is the only way back for this unit.

# /sdcard is internal shared storage (not the microSD card). A root shell may see an
# empty /sdcard because of mount namespaces, so write to the real directory behind it.
if [ -d /data/media/0 ]; then
  BASE=/data/media/0
else
  BASE=/sdcard
fi
OUT=$BASE/hako/backup
mkdir -p "$OUT" || { echo "cannot create $OUT"; exit 1; }
LOG="$OUT/backup.log"
echo "start $(date) uid=$(id -u) out=$OUT" | tee "$LOG"

BYNAME=/dev/block/by-name
[ -d "$BYNAME" ] || BYNAME=/dev/block/bootdevice/by-name
echo "partitions from $BYNAME" | tee -a "$LOG"

# Device-specific partitions, plus partitions OpenABL and the bootloader keep settings in
PARTS="abl_a abl_b persist devinfo misc modemst1 modemst2 fsg fsc keystore frp \
uefivarstore secdata storsec ssd nvdata1 nvdata2 qpdata1 qpdata2 cdt ddr \
splash logfs limits limits-cdsp apdp msadp dip spunvm \
dtbo_a vendor_boot_a"
# (dtbo_a / vendor_boot_a are only for reference: they hold the stock device tree)

for p in $PARTS; do
  src=$BYNAME/$p
  if [ -e "$src" ]; then
    if dd if="$src" of="$OUT/$p.img" bs=1048576 2>>"$LOG"; then
      echo "ok   $p" | tee -a "$LOG"
    else
      echo "FAIL $p" | tee -a "$LOG"
    fi
  else
    echo "skip $p (not found)" | tee -a "$LOG"
  fi
done

# Primary GPT (LBA0..5) and backup GPT (last 5 LBAs) of each LUN
for d in /dev/block/sd[a-z]; do
  [ -b "$d" ] || continue
  n=${d##*/}
  ss=$(cat /sys/block/$n/queue/logical_block_size)
  # /sys/block/*/size is in 512-byte units; divide first (Android sh arithmetic is 32-bit)
  total=$(( $(cat /sys/block/$n/size) / (ss / 512) ))
  dd if="$d" of="$OUT/${n}_gpt_primary.bin" bs="$ss" count=6 2>>"$LOG"
  dd if="$d" of="$OUT/${n}_gpt_backup.bin" bs="$ss" skip=$((total - 5)) count=5 2>>"$LOG"
  echo "gpt  $n sector=$ss total=$total" | tee -a "$LOG"
done

ls -l "$BYNAME/" > "$OUT/by-name.txt" 2>&1
cat /proc/partitions > "$OUT/partitions.txt"
getprop > "$OUT/getprop.txt"
(cd "$OUT" && sha256sum *.img *.bin > SHA256SUMS)
echo "done $(date)" | tee -a "$LOG"

# Files made by root are not visible to apps / MTP until ownership, SELinux label
# and the media index are fixed.
if [ "$BASE" = /data/media/0 ]; then
  chown -R 1023:1023 "$BASE/hako" 2>/dev/null   # media_rw
  restorecon -R "$BASE/hako" 2>/dev/null
fi
for f in "$OUT"/*; do
  am broadcast -a android.intent.action.MEDIA_SCANNER_SCAN_FILE \
    -d "file:///sdcard/hako/backup/${f##*/}" >/dev/null 2>&1
done

echo "Backup finished: $OUT ($(ls "$OUT" | wc -l) files)"
echo "If the folder looks empty from the PC, reboot the device and reconnect USB."
