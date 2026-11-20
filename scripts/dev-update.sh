#!/bin/bash
# Update the development device over USB (or any ssh link) with apt.
#
#   scripts/dev-update.sh [--build] [--reinstall] [pkg...]
#
#   --build      run scripts/build-packages.sh first
#   --reinstall  reinstall the given packages even if the version is unchanged
#   pkg...       install/upgrade these; default: upgrade every installed package
#                that comes from this repository
#
# The repo/ directory is served on 127.0.0.1:8780 here and forwarded to the
# device with ssh -R, so nothing on the PC is exposed to the network. A reverse
# SOCKS proxy (ssh -R 1080) lets apt reach the Debian mirrors through the PC,
# so new Debian dependencies install even without Wi-Fi on the device.
# The apt source for the tunnel is written on the device for the duration of
# the run only (it is unreachable otherwise).
# Device: HAKO_DEV (default root@192.168.7.2).
set -euo pipefail
TOP=$(cd "$(dirname "$0")/.." && pwd)
DEV=${HAKO_DEV:-root@192.168.7.2}
PORT=8780
SOCKS=1080

build=0 reinstall=0
while [ $# -gt 0 ]; do
	case "$1" in
	--build) build=1 ;;
	--reinstall) reinstall=1 ;;
	-*) echo "unknown option $1" >&2; exit 2 ;;
	*) break ;;
	esac
	shift
done

[ $build = 1 ] && bash "$TOP/scripts/build-packages.sh"
bash "$TOP/scripts/publish-repo.sh" >/dev/null

python3 -m http.server "$PORT" --bind 127.0.0.1 --directory "$TOP/repo" >/dev/null 2>&1 &
srv=$!
trap 'kill $srv 2>/dev/null || true' EXIT
for _ in 1 2 3 4 5 6 7 8 9 10; do
	(exec 3<>/dev/tcp/127.0.0.1/$PORT) 2>/dev/null && break
	sleep 0.2
done

APT="apt-get -o Acquire::http::Proxy=socks5h://127.0.0.1:$SOCKS -o Acquire::http::Proxy::127.0.0.1=DIRECT"
if [ $# -gt 0 ]; then
	pkgs="$*"
	if [ $reinstall = 1 ]; then act="$APT install -y --reinstall $pkgs"; else act="$APT install -y $pkgs"; fi
else
	# every package of this repository; --only-upgrade skips the ones not installed
	pkgs=$(reprepro -b "$TOP/repo" list trixie | awk '{print $2}' | sort -u | tr '\n' ' ')
	act="$APT install -y --only-upgrade $pkgs"
fi

echo "== $DEV: $act"
ssh -o ExitOnForwardFailure=yes -R "$PORT:127.0.0.1:$PORT" -R "$SOCKS" "$DEV" \
	"set -e; export DEBIAN_FRONTEND=noninteractive
	L=/etc/apt/sources.list.d/hako-dev-tunnel.list
	echo 'deb [trusted=yes] http://127.0.0.1:$PORT trixie main' > \$L
	trap 'rm -f \$L' EXIT
	$APT update -q; $APT install -y -f; $act"
