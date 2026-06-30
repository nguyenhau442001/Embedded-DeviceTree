# Build environment for mydev-led kernel module
#
# Uses Debian Trixie to match Raspberry Pi OS (latest).
# We use Docker because macOS lacks Linux host headers (elf.h etc.)
# that the kernel build system needs to compile host-side tools.
# Running a Linux container on macOS gives us those headers.
#
# Build the image:
#   docker build -t mydev-led-builder .
#
# Build the module:
#   docker run --rm -v $(pwd):/build mydev-led-builder \
#     make -C /rpi-linux ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- \
#     M=/build modules

FROM debian:trixie-slim

ENV DEBIAN_FRONTEND=noninteractive

# Install cross-compiler and kernel build dependencies
RUN apt-get update && apt-get install -y \
    gcc-aarch64-linux-gnu \
    make \
    bc \
    bison \
    flex \
    libssl-dev \
    libelf-dev \
    git \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Clone the Raspberry Pi kernel source (depth=1 to keep image small)
RUN git clone --depth=1 \
    https://github.com/raspberrypi/linux.git \
    --branch rpi-6.6.y \
    /rpi-linux

# Apply bcm2711 (Pi 4) config and prepare headers for out-of-tree module builds
RUN make -C /rpi-linux \
    ARCH=arm64 \
    CROSS_COMPILE=aarch64-linux-gnu- \
    bcm2711_defconfig

RUN make -C /rpi-linux \
    ARCH=arm64 \
    CROSS_COMPILE=aarch64-linux-gnu- \
    -j$(nproc) \
    modules_prepare

WORKDIR /build
