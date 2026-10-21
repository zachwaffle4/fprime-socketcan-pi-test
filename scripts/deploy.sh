#!/usr/bin/env bash
# Copy a Linux build and the helper scripts to a Pi.
#
# Usage: scripts/deploy.sh user@pi-address
# Puts everything in ~/picantest on the Pi.
set -euo pipefail

if [ $# -ne 1 ]; then
    echo "Usage: $0 user@pi-address" >&2
    exit 1
fi
TARGET="$1"

cd "$(dirname "$0")/.."

BINARY="build-artifacts/Linux/PiCanTest/bin/PiCanTest"
if [ ! -f "$BINARY" ]; then
    echo "$BINARY not found. Run scripts/container-build.sh first." >&2
    exit 1
fi

ssh "$TARGET" mkdir -p picantest/scripts
rsync -av "$BINARY" "$TARGET":picantest/
rsync -av scripts/setup-can.sh scripts/can-sanity.sh "$TARGET":picantest/scripts/
