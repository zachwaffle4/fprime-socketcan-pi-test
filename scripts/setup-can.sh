#!/usr/bin/env bash
# Configure and bring up a SocketCAN interface the way the deployment expects:
# the driver never sets bitrate or restart itself, it only checks the interface is up.
#
# Usage: scripts/setup-can.sh [interface]      (default can0)
# Environment overrides: BITRATE (500000), RESTART_MS (100), TXQUEUELEN (1000)
#
# Every node on the bus must use the same BITRATE.
set -euo pipefail

IFACE="${1:-can0}"
BITRATE="${BITRATE:-500000}"
RESTART_MS="${RESTART_MS:-100}"
TXQUEUELEN="${TXQUEUELEN:-1000}"

SUDO=""
if [ "$(id -u)" -ne 0 ]; then
    SUDO="sudo"
fi

if ! ip link show "$IFACE" >/dev/null 2>&1; then
    echo "Interface $IFACE does not exist. Check the overlay with: dmesg | grep -i mcp251" >&2
    exit 1
fi

$SUDO ip link set "$IFACE" down 2>/dev/null || true
$SUDO ip link set "$IFACE" type can bitrate "$BITRATE" restart-ms "$RESTART_MS"
$SUDO ip link set "$IFACE" txqueuelen "$TXQUEUELEN"
$SUDO ip link set "$IFACE" up

# Worth saving with your test results: shows the real bit timing and clock
ip -details -statistics link show "$IFACE"
