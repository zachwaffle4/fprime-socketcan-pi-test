#!/usr/bin/env bash
# Start a GDS that connects to one Pi's deployment.
# Run this once per Pi, each in its own terminal, with a different GUI port.
#
# Usage: scripts/run-gds.sh <pi-address> [gui-port]
#   scripts/run-gds.sh pi-a.local 5000
#   scripts/run-gds.sh pi-b.local 5001
#
# Needs the fprime venv active (. fprime-venv/bin/activate) and a build on this machine
# for the dictionary (fprime-util build, or scripts/container-build.sh).
set -euo pipefail

if [ $# -lt 1 ]; then
    echo "Usage: $0 <pi-address> [gui-port]" >&2
    exit 1
fi
HOST="$1"
GUI_PORT="${2:-5000}"
FSW_PORT="${FSW_PORT:-50000}"

cd "$(dirname "$0")/.."

if ! command -v fprime-gds >/dev/null; then
    echo "fprime-gds not found. Activate the venv first: . fprime-venv/bin/activate" >&2
    exit 1
fi

# The dictionary does not depend on the platform, so any build's will do
DICTIONARY="$(ls build-artifacts/*/PiCanTest/dict/PiCanTestTopologyDictionary.json 2>/dev/null | head -n 1 || true)"
if [ -z "$DICTIONARY" ]; then
    echo "No dictionary found under build-artifacts. Build first." >&2
    exit 1
fi

# Each GDS needs its own GUI port, ZMQ sockets and log directory
exec fprime-gds -n \
    --dictionary "$DICTIONARY" \
    --ip-client --ip-address "$HOST" --ip-port "$FSW_PORT" \
    --gui-port "$GUI_PORT" \
    --zmq-transport "ipc:///tmp/picantest-$GUI_PORT-in" "ipc:///tmp/picantest-$GUI_PORT-out" \
    --logs "logs/$HOST"
