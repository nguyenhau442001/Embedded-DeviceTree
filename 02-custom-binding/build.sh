#!/bin/bash
# Build mydev-led-driver.ko on macOS using Docker.
#
# Usage (from 02-custom-binding/):
#   ./build.sh          — build the .ko
#   ./build.sh clean    — clean build artifacts

set -e

IMAGE=mydev-led-builder
KDIR=/rpi-linux
ARCH=arm64
CROSS_COMPILE=aarch64-linux-gnu-

# Build the Docker image if it doesn't exist yet
if ! docker image inspect "$IMAGE" > /dev/null 2>&1; then
    echo "==> Docker image '$IMAGE' not found, building it (one-time, takes ~5 min)..."
    docker build -t "$IMAGE" .
fi

if [ "$1" = "clean" ]; then
    echo "==> Cleaning build artifacts..."
    docker run --rm \
        -v "$(pwd)":/build \
        "$IMAGE" \
        make -C "$KDIR" \
            ARCH="$ARCH" \
            CROSS_COMPILE="$CROSS_COMPILE" \
            M=/build \
            clean
    echo "==> Done."
else
    echo "==> Building mydev-led-driver.ko..."
    docker run --rm \
        -v "$(pwd)":/build \
        "$IMAGE" \
        make -C "$KDIR" \
            ARCH="$ARCH" \
            CROSS_COMPILE="$CROSS_COMPILE" \
            M=/build \
            -j$(sysctl -n hw.logicalcpu) \
            modules
    echo "==> Done. Output: $(pwd)/mydev-led-driver.ko"
fi
