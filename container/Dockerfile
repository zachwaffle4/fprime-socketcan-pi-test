# Build environment for the Pi binaries. On an Apple Silicon Mac this runs as
# native arm64 Linux, so it compiles the real SocketCAN driver for the Pi 5
# without a cross toolchain. Built with Apple's `container` CLI (scripts/container-build.sh).
#
# Debian trixie matches the current Raspberry Pi OS (trixie). A binary links
# against the build image's glibc, so it runs on trixie and newer but not on
# bookworm Pis; use scripts/build-on-pi.sh for those.
FROM --platform=linux/arm64 python:3.12-slim-trixie

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    git \
    build-essential \
    ninja-build \
    && rm -rf /var/lib/apt/lists/* \
    && git config --system --add safe.directory '*'

WORKDIR /work
