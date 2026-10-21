#!/usr/bin/env bash
# Install what a Pi needs to configure CAN and, optionally, build natively.
# Run once per Pi, with network access.
set -euo pipefail

SUDO=""
if [ "$(id -u)" -ne 0 ]; then
    SUDO="sudo"
fi

$SUDO apt-get update
$SUDO apt-get install -y --no-install-recommends \
    can-utils iproute2 \
    git build-essential ninja-build python3-venv python3-pip
