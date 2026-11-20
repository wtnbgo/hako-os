#!/bin/bash
# Publish the built .debs (kernel + own packages) into the local apt repository repo/.
# ELBE picks it up through <repodir> in the project XML.
set -euo pipefail
TOP=$(cd "$(dirname "$0")/.." && pwd)
R=$TOP/repo
shopt -s nullglob
debs=("$TOP"/out/kernel/*.deb "$TOP"/out/packages/*.deb)
[ ${#debs[@]} -gt 0 ] || { echo "nothing to publish"; exit 1; }
for d in "${debs[@]}"; do
	pkg=$(dpkg-deb -f "$d" Package)
	reprepro -b "$R" remove trixie "$pkg" >/dev/null 2>&1 || true
	reprepro -b "$R" includedeb trixie "$d"
done
reprepro -b "$R" list trixie
