#!/bin/bash
# Build both the DT overlay (.dtbo) and kernel module (.ko) on macOS.
#
# Usage (from 02-custom-binding/):
#   ./build.sh          — build overlay + kernel module
#   ./build.sh clean    — clean all build artifacts

set -e

IMAGE=mydev-led-builder
KDIR=/rpi-linux
ARCH=arm64
CROSS_COMPILE=aarch64-linux-gnu-
DTS=01-dt-overlay.dts
DTBO=mydev-led.dtbo

if [ "$1" = "clean" ]; then
    echo "==> Cleaning build artifacts..."
    rm -f "$DTBO"
    docker run --rm \
        -v "$(pwd)":/build \
        "$IMAGE" \
        make -C "$KDIR" \
            ARCH="$ARCH" \
            CROSS_COMPILE="$CROSS_COMPILE" \
            M=/build \
            clean
    echo "==> Done."
    exit 0
fi

# ── Step 1: compile the DT overlay (native dtc on macOS) ──────────────────
echo "==> Compiling DT overlay: $DTS → $DTBO"
dtc -@ -I dts -O dtb -o "$DTBO" "$DTS"
echo "    Output: $(pwd)/$DTBO"

# ── Step 2: build the kernel module via Docker ────────────────────────────
if ! docker image inspect "$IMAGE" > /dev/null 2>&1; then
    echo "==> Docker image '$IMAGE' not found, building it (one-time, ~5-10 min)..."
    docker build -t "$IMAGE" .
fi

echo "==> Building kernel module: mydev-led-driver.ko"
docker run --rm \
    -v "$(pwd)":/build \
    "$IMAGE" \
    make -C "$KDIR" \
        ARCH="$ARCH" \
        CROSS_COMPILE="$CROSS_COMPILE" \
        M=/build \
        -j$(sysctl -n hw.logicalcpu) \
        modules
echo "    Output: $(pwd)/mydev-led-driver.ko"

echo ""
echo "==> Build complete. Files ready to deploy:"
echo "    $DTBO          → scp to Pi's /boot/firmware/overlays/"
echo "    mydev-led-driver.ko  → scp to Pi's ~/"
