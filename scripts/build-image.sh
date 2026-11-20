#!/bin/bash
# Build an image with ELBE.
#
#   scripts/build-image.sh dev                -> out/images/dev/rpmini-dev.img
#   BOARD=<board> scripts/build-image.sh dev  -> uses elbe/<board>-dev.xml
#
# Steps: build own packages (only the ones whose version changed), publish
# kernel + packages to repo/, check for mistakes that would only show up at
# the end of the ELBE build, prepare the overlay (ssh key of this build host
# for root), submit to the ELBE initvm.
set -euo pipefail
TOP=$(cd "$(dirname "$0")/.." && pwd)
FLAVOR=${1:-dev}
BOARD=${BOARD:-rpmini}
XML=$TOP/elbe/$BOARD-$FLAVOR.xml
[ -f "$XML" ] || { echo "no $XML"; exit 1; }

bash "$TOP/scripts/build-packages.sh"
bash "$TOP/scripts/publish-repo.sh" >/dev/null

# --- checks before the (long) ELBE build ---------------------------------
fail=0
# 1. the XML must be well-formed (elbe validate cannot be used here: it does
#    not know <repodir>/<archivedir>, which only `initvm submit` expands, and
#    it exits 0 on errors anyway)
xmllint --noout "$XML" || { echo "check: $XML is not well-formed XML"; fail=1; }
# 2. no package may ship an apt source on localhost: it is unreachable in the
#    ELBE build and makes it fail at the very end (AptCacheUpdateError)
for deb in "$TOP"/out/packages/*.deb; do
	srcs=$(dpkg-deb --fsys-tarfile "$deb" | tar -t 2>/dev/null | grep -E '^\./etc/apt/sources\.list(\.d/[^/]+)?$' || true)
	for s in $srcs; do
		if dpkg-deb --fsys-tarfile "$deb" | tar -xO "$s" 2>/dev/null | grep -vE '^\s*#' | grep -qE '127\.0\.0\.1|localhost'; then
			echo "check: $(basename "$deb") ships ${s#.} with a localhost apt source"
			fail=1
		fi
	done
done
# 3. every package named in the XML pkg-list must be in repo/ or come from Debian
#    (only our own names are checked here: hako-*, <board>-*, linux-image-*)
for p in $(grep -oE '<pkg>[^<]+</pkg>' "$XML" | sed -E 's/<\/?pkg>//g' | grep -E "^(hako-|$BOARD-|linux-image-)"); do
	reprepro -b "$TOP/repo" list trixie "$p" | grep -q . || { echo "check: $p is in the pkg-list but not in repo/"; fail=1; }
done
[ $fail = 0 ] || { echo "stopped before the ELBE build"; exit 1; }
# -------------------------------------------------------------------------

# overlay copied into the image (<archivedir> in the XML); only dev gets a key
OV=$TOP/build/elbe/overlay
rm -rf "$OV"
mkdir -p "$OV"
if [ "$FLAVOR" = dev ]; then
	install -d -m 700 "$OV/root/.ssh"
	cat "$HOME"/.ssh/id_*.pub > "$OV/root/.ssh/authorized_keys"
	chmod 600 "$OV/root/.ssh/authorized_keys"
fi

OUT=$TOP/out/images/$FLAVOR
mkdir -p "$OUT"
elbe initvm ensure
elbe initvm submit --skip-build-sources --output "$OUT" "$XML"
ls -l "$OUT"
