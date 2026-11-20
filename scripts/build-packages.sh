#!/bin/bash
# Build packages under packages/ (binary only) into out/packages/.
#
#   scripts/build-packages.sh            build every package whose changelog
#                                        version is not in out/packages/ yet
#   scripts/build-packages.sh pkg...     build the named packages (always)
#   FORCE=1 scripts/build-packages.sh    rebuild everything
#
# A package is identified by the version at the top of debian/changelog, so
# bump the changelog for every change (otherwise the old build is reused).
# Older debs of a rebuilt package are removed, so publish-repo.sh, which takes
# everything in out/packages/, sees one version per package.
set -euo pipefail
TOP=$(cd "$(dirname "$0")/.." && pwd)
OUT=$TOP/out/packages
mkdir -p "$OUT"
explicit=0
if [ $# -eq 0 ]; then
	dirs=("$TOP"/packages/*/)
else
	explicit=1
	dirs=()
	for p in "$@"; do dirs+=("$TOP/packages/$p/"); done
fi

# binary package names of a source (Package: lines of debian/control)
binaries() { awk '/^Package:/ {print $2}' "$1/debian/control"; }

for d in "${dirs[@]}"; do
	[ -f "$d/debian/control" ] || continue
	name=$(basename "$d")
	ver=$(cd "$d" && dpkg-parsechangelog -S Version)
	fver=${ver#*:}   # the epoch is not part of the file name
	if [ "$explicit" = 0 ] && [ "${FORCE:-0}" != 1 ]; then
		have=1
		for b in $(binaries "$d"); do
			ls "$OUT/${b}_${fver}_"*.deb >/dev/null 2>&1 || { have=0; break; }
		done
		if [ $have = 1 ]; then echo "==> $name $ver (up to date)"; continue; fi
	fi
	echo "==> $name $ver"
	(cd "$d" && DEB_BUILD_OPTIONS=nocheck dpkg-buildpackage -b -a arm64 -us -uc -tc --no-sign >/dev/null)
	for b in $(binaries "$d"); do
		for f in "$OUT/${b}_"*.deb; do
			[ -e "$f" ] || continue
			case "$f" in *"_${fver}_"*) ;; *) rm -f "$f" ;; esac
		done
	done
	# dpkg-buildpackage writes results next to the source dir
	mv "$TOP"/packages/*.deb "$OUT/"
	rm -f "$TOP"/packages/*.buildinfo "$TOP"/packages/*.changes
done
ls -l "$OUT"
