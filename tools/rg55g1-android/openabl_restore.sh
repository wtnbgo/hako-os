#!/system/bin/sh
# Put the stock ABL (saved by rg55g1_backup.sh) back into abl_a and abl_b.
# Run via run_restore.sh. Use it from Android, before rebooting, if anything goes wrong.
# (If Android no longer boots at all: EDL mode and the vendor's Android flat build.)

D=/data/media/0/hako
BK=$D/backup
LOG=$D/restore.log
exec > "$LOG" 2>&1

fail() { echo "ABORT: $*"; echo "RESULT=ABORTED"; exit 1; }
sha() { sha256sum "$1" | cut -d' ' -f1; }

echo "start $(date)"
PLAT_EXPECT="parrot"     # same value as in openabl_flash.sh (getprop ro.board.platform)
[ -n "$PLAT_EXPECT" ] || fail "PLAT_EXPECT not set yet (platform=$(getprop ro.board.platform))"
[ "$(getprop ro.board.platform)" = "$PLAT_EXPECT" ] || fail "not the expected platform ($PLAT_EXPECT)"

for s in a b; do
  img=$BK/abl_$s.img
  [ -f "$img" ] || fail "missing $img"
  want=$(sha "$img")
  blocks=$(( $(stat -c %s "$img") / 4096 ))
  echo "restoring abl_$s from $img ($want)"
  dd if="$img" of=/dev/block/by-name/abl_$s bs=4096 conv=fsync || fail "dd abl_$s failed"
  rb=$(dd if=/dev/block/by-name/abl_$s bs=4096 count=$blocks 2>/dev/null | sha256sum | cut -d' ' -f1)
  echo "readback abl_$s=$rb"
  [ "$rb" = "$want" ] || fail "verify abl_$s failed"
done
sync
echo "RESULT=SUCCESS $(date)"
