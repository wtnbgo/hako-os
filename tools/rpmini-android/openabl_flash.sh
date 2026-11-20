#!/system/bin/sh
# Write OpenABL v1.2 (SM8250) to abl_a and abl_b, with checks before and after.
# Run via run_check.sh (checks only) / run_flash.sh: Retroid's "Run Script as Root"
# sends one line per command, so the wrappers are one line each.
#
# Refuses to write unless:
#   - the device is a Retroid Pocket Mini on SM8250 (kona)
#   - the elf matches the OpenABL v1.2 sha256
#   - the current abl_a/abl_b match the backups in hako/backup/ (= the backup is valid)
# After each write the partition is read back and compared.

D=/data/media/0/hako
ELF=$D/abl_signed-SM8250.elf
EXP=628dbff74f5b570b897c14725f2c36ff2ca5fe018998e2c248a3d15d906ccef9
BK=$D/backup
LOG=$D/flash.log
exec > "$LOG" 2>&1

fail() { echo "ABORT: $*"; echo "RESULT=ABORTED (nothing more was written)"; exit 1; }
sha() { sha256sum "$1" | cut -d' ' -f1; }

echo "start $(date)"
plat=$(getprop ro.board.platform); model=$(getprop ro.product.model)
echo "platform=$plat model=$model"
[ "$plat" = kona ] || fail "not SM8250 (kona)"
[ "$model" = "Retroid Pocket Mini" ] || fail "not a Retroid Pocket Mini"

[ -f "$ELF" ] || fail "missing $ELF"
[ "$(sha "$ELF")" = "$EXP" ] || fail "elf checksum mismatch"
size=$(stat -c %s "$ELF")
blocks=$((size / 4096))
[ $((blocks * 4096)) -eq "$size" ] || fail "unexpected elf size $size"
echo "elf ok ($size bytes)"

for s in a b; do
  cur=$(sha /dev/block/by-name/abl_$s)
  bk=$(sha "$BK/abl_$s.img")
  echo "abl_$s current=$cur backup=$bk"
  [ "$cur" = "$bk" ] || fail "abl_$s does not match the backup (already flashed, or backup is stale)"
done

if [ "$1" = check ]; then
  echo "RESULT=CHECK_OK (check only, nothing written)"
  exit 0
fi

for s in a b; do
  echo "writing abl_$s"
  dd if="$ELF" of=/dev/block/by-name/abl_$s bs=4096 conv=fsync || fail "dd abl_$s failed"
  rb=$(dd if=/dev/block/by-name/abl_$s bs=4096 count=$blocks 2>/dev/null | sha256sum | cut -d' ' -f1)
  echo "readback abl_$s=$rb"
  [ "$rb" = "$EXP" ] || fail "verify abl_$s failed - restore with run_restore.sh before rebooting"
done
sync
echo "RESULT=SUCCESS $(date)"
