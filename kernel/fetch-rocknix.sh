#!/bin/bash
# Fetch the pinned ROCKNIX tree (sparse) into kernel/rocknix/ for reference and patch import.
set -euo pipefail
cd "$(dirname "$0")"
. ./SOURCE.pin
if [ ! -d rocknix/.git ]; then
  git clone --filter=blob:none --no-checkout "$ROCKNIX_REPO" rocknix
fi
cd rocknix
git sparse-checkout set "$ROCKNIX_DEVICE_DIR" projects/ROCKNIX/packages/linux projects/ROCKNIX/packages/tools/openabl
git fetch --depth 1 origin "$ROCKNIX_COMMIT"
git checkout -q "$ROCKNIX_COMMIT"
grep -m1 '^PKG_VERSION' projects/ROCKNIX/packages/linux/package.mk || true
ls "$ROCKNIX_DEVICE_DIR"/patches/linux | wc -l | xargs echo "linux patches:"
