#!/usr/bin/env bash
# Add the Waveshare 2-CH CAN HAT (2x MCP2515) device-tree overlays to the Pi's
# config.txt. Idempotent, and keeps a backup. Reboot afterward.
#
# The lines come from Waveshare's wiki (https://waveshare.com/wiki/2-CH_CAN_HAT).
# The wiki says INT_0 is wired to GPIO23 and INT_1 to GPIO25, and 16 MHz crystals.
# It does not give separate Pi 5 settings, only an FAQ answer that the HAT works on a Pi 5.
#
# After the reboot, check what actually came up:
#   dmesg | grep -i mcp251
#   ip -br link show type can
# Interface names (can0/can1) are assigned in probe order, so confirm which
# physical channel is which by sending a frame and watching the other side.
set -euo pipefail

OSCILLATOR="${OSCILLATOR:-16000000}"
INT_CAN0="${INT_CAN0:-23}"
INT_CAN1="${INT_CAN1:-25}"

CONFIG=""
for candidate in /boot/firmware/config.txt /boot/config.txt; do
    if [ -f "$candidate" ]; then
        CONFIG="$candidate"
        break
    fi
done
if [ -z "$CONFIG" ]; then
    echo "No config.txt found under /boot/firmware or /boot" >&2
    exit 1
fi

SUDO=""
if [ "$(id -u)" -ne 0 ]; then
    SUDO="sudo"
fi

MARKER="# picantest: Waveshare 2-CH CAN HAT"
if grep -qF "$MARKER" "$CONFIG"; then
    echo "$CONFIG already has the CAN overlay block; edit it by hand to change it."
    exit 0
fi

BACKUP="$CONFIG.picantest.bak"
$SUDO cp -n "$CONFIG" "$BACKUP"
echo "Backed up $CONFIG to $BACKUP"

$SUDO tee -a "$CONFIG" >/dev/null <<EOF

$MARKER
dtparam=spi=on
dtoverlay=mcp2515-can0,oscillator=$OSCILLATOR,interrupt=$INT_CAN0
dtoverlay=mcp2515-can1,oscillator=$OSCILLATOR,interrupt=$INT_CAN1
EOF

echo "Added the overlay block to $CONFIG. Reboot to apply: sudo reboot"
