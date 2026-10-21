#!/usr/bin/env bash
# Check the CAN hardware with can-utils alone, before involving F Prime.
# If this does not work, fix wiring, termination, bitrate or the overlay first.
#
# Usage:
#   scripts/can-sanity.sh dump <if>            print frames on <if> (run on one Pi)
#   scripts/can-sanity.sh send <if>            send one frame, id 0x123 (run on the other)
#   scripts/can-sanity.sh flood <if> [count]   send count frames back to back (default 10000)
#   scripts/can-sanity.sh loopback <tx> <rx>   one Pi, both channels wired together
set -euo pipefail

usage() {
    sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//' >&2
    exit 1
}

[ $# -ge 2 ] || usage
MODE="$1"
IFACE="$2"

case "$MODE" in
    dump)
        exec candump -ta "$IFACE"
        ;;
    send)
        cansend "$IFACE" 123#DEADBEEF
        echo "Sent 0x123#DEADBEEF on $IFACE"
        ;;
    flood)
        COUNT="${3:-10000}"
        # -g 0: no gap between frames, -n: frame count, -I: fixed ID, -L: 8 data bytes, -D i: incrementing data
        cangen "$IFACE" -g 0 -n "$COUNT" -I 300 -L 8 -D i
        echo "Sent $COUNT frames on $IFACE; check the receiver's counters"
        ;;
    loopback)
        [ $# -ge 3 ] || usage
        RX="$3"
        DUMP="$(mktemp)"
        trap 'rm -f "$DUMP"' EXIT
        candump -L "$RX" >"$DUMP" &
        DUMP_PID=$!
        sleep 0.5
        cansend "$IFACE" 123#DEADBEEF
        sleep 0.5
        kill "$DUMP_PID" 2>/dev/null || true
        if grep -q "123#DEADBEEF" "$DUMP"; then
            echo "OK: frame sent on $IFACE arrived on $RX"
        else
            echo "FAIL: nothing arrived on $RX. Check wiring, termination and that both are up at the same bitrate." >&2
            exit 1
        fi
        ;;
    *)
        usage
        ;;
esac
