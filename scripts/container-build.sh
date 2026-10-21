#!/usr/bin/env bash
# Build the Pi binary on this Mac in an arm64 Debian container, using Apple's `container` CLI.
# Needs the submodules checked out (git submodule update --init --depth 1 lib/fprime lib/fprime-socketcan).
#
# Result: build-artifacts/Linux/PiCanTest/bin/PiCanTest (aarch64) and its dictionary.
# The Python venv lives in a named volume, so it never mixes with a host fprime-venv.
set -euo pipefail

cd "$(dirname "$0")/.."

IMAGE="picantest-build"
VENV_VOLUME="picantest-venv"
CPUS="${CPUS:-4}"
MEMORY="${MEMORY:-4g}"

# The apiserver has to be running before any other container command
if ! container system status >/dev/null 2>&1; then
    container system start
fi

container build --platform linux/arm64 -t "$IMAGE" -f container/Dockerfile container/

if ! container volume inspect "$VENV_VOLUME" >/dev/null 2>&1; then
    container volume create "$VENV_VOLUME"
fi

container run --rm --platform linux/arm64 \
    --cpus "$CPUS" --memory "$MEMORY" \
    -v "$PWD":/work \
    -v "$VENV_VOLUME":/opt/venv \
    "$IMAGE" bash -euo pipefail -c '
        if [ ! -x /opt/venv/bin/python ]; then
            python -m venv /opt/venv
        fi
        . /opt/venv/bin/activate
        pip install --quiet --upgrade pip
        pip install --quiet -r requirements.txt
        fprime-util generate --build-cache build-container-aarch64
        fprime-util build --build-cache build-container-aarch64 -j "$(nproc)"
    '

file build-artifacts/Linux/PiCanTest/bin/PiCanTest 2>/dev/null || true
echo "Built: build-artifacts/Linux/PiCanTest/bin/PiCanTest"
