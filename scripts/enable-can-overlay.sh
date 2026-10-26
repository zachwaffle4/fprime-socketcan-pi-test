#!/usr/bin/env bash
# Add the Waveshare 2-CH CAN HAT+ (2x MCP2515 on SPI1) device-tree overlays to the Pi's
# config.txt. Idempotent, and keeps a backup. Reboot afterward.
#
# The lines come from Waveshare's wiki (https://www.waveshare.com/wiki/2-CH_CAN_HAT+).
# The wiki says INT_0 is wired to GPIO22 and INT_1 to GPIO13 by default, and 16 MHz crystals.
# Both controllers are on SPI1 (chip selects 1 and 2), hence the spi1-3cs overlay.
# If you move the interrupt jumpers on the HAT, set INT_CAN0/INT_CAN1 to match.
#
# After the reboot, check what actually came up:
#   dmesg | grep -i mcp251
#   ip -br link show type can
# Interface names (can0/can1) are assigned in probe order, so confirm which
# physical channel is which by sending a frame and watching the other side.
set -euo pipefail

OSCILLATOR="${OSCILLATOR:-16000000}"
INT_CAN0="${INT_CAN0:-22}"
INT_CAN1="${INT_CAN1:-13}"

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

MARKER="# picantest: Waveshare 2-CH CAN HAT+"
if grep -qF "$MARKER" "$CONFIG"; then
    echo "$CONFIG already has the CAN overlay block; edit it by hand to change it."
    exit 0
fi

if grep -qF "# picantest: Waveshare 2-CH CAN HAT" "$CONFIG"; then
    echo "$CONFIG has the older (non-HAT+) overlay block. Remove it by hand, or restore" >&2
    echo "$CONFIG.picantest.bak, then re-run this script." >&2
    exit 1
fi

BACKUP="$CONFIG.picantest.bak"
$SUDO cp -n "$CONFIG" "$BACKUP"
echo "Backed up $CONFIG to $BACKUP"

$SUDO tee -a "$CONFIG" >/dev/null <<EOF

$MARKER
dtparam=spi=on
dtoverlay=i2c0
dtoverlay=spi1-3cs
dtoverlay=mcp2515,spi1-1,oscillator=$OSCILLATOR,interrupt=$INT_CAN0
dtoverlay=mcp2515,spi1-2,oscillator=$OSCILLATOR,interrupt=$INT_CAN1
EOF

echo "Added the overlay block to $CONFIG. Reboot to apply: sudo reboot"
