#!/usr/bin/env bash
# Build the deployment natively on a Pi (or any Linux machine).
# Needs scripts/pi-bootstrap.sh first. The first build takes a while on a Pi.
#
# Result: build-artifacts/Linux/PiCanTest/bin/PiCanTest
set -euo pipefail

cd "$(dirname "$0")/.."

# Only these two submodules. fprime-socketcan has its own lib/fprime submodule
# that must stay uninitialized; this project supplies F Prime.
git submodule update --init --depth 1 lib/fprime lib/fprime-socketcan

if [ ! -d fprime-venv ]; then
    python3 -m venv fprime-venv
fi
# shellcheck disable=SC1091
. fprime-venv/bin/activate
pip install --quiet --upgrade pip
pip install --quiet -r requirements.txt

fprime-util generate
fprime-util build -j "$(nproc)"

echo "Built: build-artifacts/Linux/PiCanTest/bin/PiCanTest"
