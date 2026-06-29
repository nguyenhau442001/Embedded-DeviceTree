# Cross-Compile Setup on macOS

## Why Cross-Compile

Building on the Raspberry Pi is possible but slow — a full kernel + DTB build takes 30–60 minutes on Pi 4 hardware. Cross-compiling on macOS finishes the same build in 3–5 minutes.

The cross-compiler runs on macOS (x86_64 or Apple Silicon) and produces binaries for the Pi's architecture (AArch64 / ARM64).

---

## Install the Toolchain

```bash
brew install aarch64-elf-gcc
```

This installs:
- `aarch64-elf-gcc` — C compiler targeting AArch64 bare-metal
- `aarch64-elf-ld` — linker
- `aarch64-elf-objcopy`, `aarch64-elf-objdump` — binary utilities
- `aarch64-elf-as` — assembler

Also install `dtc` for standalone DTS/DTB work:

```bash
brew install dtc
```

Verify:

```bash
aarch64-elf-gcc --version
# aarch64-elf-gcc (GCC) 14.x.x ...

dtc --version
# Version DTC 1.7.x
```

---

## Environment Variables

The kernel build system uses two variables to select the cross-compiler:

```bash
export ARCH=arm64
export CROSS_COMPILE=aarch64-elf-
```

`CROSS_COMPILE` is a prefix — the build system appends `gcc`, `ld`, `objcopy`, etc. to form the full tool name.

Add these to your shell profile so they persist:

```bash
# ~/.zshrc
export ARCH=arm64
export CROSS_COMPILE=aarch64-elf-
```

Then reload:

```bash
source ~/.zshrc
```

---

## Get the Raspberry Pi Kernel Source

Use the Raspberry Pi Foundation's fork — it carries Pi-specific DT patches not yet in mainline:

```bash
git clone --depth=1 \
    https://github.com/raspberrypi/linux.git \
    --branch rpi-6.6.y \
    ~/rpi-linux
cd ~/rpi-linux
```

`--depth=1` skips the full git history — saves ~2 GB and several minutes of clone time.

---

## Build DTBs Only (Fast Path)

For Phase 1 and Phase 3 work you only need DTBs, not a full kernel image. This takes under a minute.

```bash
cd ~/rpi-linux

# Apply the default Pi 4 config
make ARCH=arm64 CROSS_COMPILE=aarch64-elf- bcm2711_defconfig

# Build only the DTBs
make ARCH=arm64 CROSS_COMPILE=aarch64-elf- dtbs -j$(sysctl -n hw.logicalcpu)
```

Output DTBs land in:
```
arch/arm64/boot/dts/broadcom/bcm2711-rpi-4-b.dtb
arch/arm64/boot/dts/broadcom/bcm2711-rpi-400.dtb
# ... etc
```

---

## Build Full Kernel + DTBs

Needed when your driver changes require a new kernel module or Image:

```bash
cd ~/rpi-linux

make ARCH=arm64 CROSS_COMPILE=aarch64-elf- bcm2711_defconfig

make ARCH=arm64 CROSS_COMPILE=aarch64-elf- -j$(sysctl -n hw.logicalcpu) \
    Image modules dtbs
```

Outputs:
```
arch/arm64/boot/Image           ← kernel binary
arch/arm64/boot/dts/broadcom/   ← DTBs
drivers/**/*.ko                 ← kernel modules
```

Typical build time on Apple Silicon: ~4 minutes.

---

## Deploy DTB to the Pi

Copy the freshly built DTB to the Pi's boot partition over SSH:

```bash
# Replace <pi-ip> with your Pi's IP address
scp arch/arm64/boot/dts/broadcom/bcm2711-rpi-4-b.dtb \
    pi@<pi-ip>:/boot/firmware/bcm2711-rpi-4-b.dtb
```

Reboot the Pi to load the new DTB:

```bash
ssh pi@<pi-ip> sudo reboot
```

Verify the new DTB is running after reboot:

```bash
ssh pi@<pi-ip> "cat /proc/device-tree/model && echo"
```

---

## Deploy Full Kernel + Modules (When Needed)

```bash
# Copy kernel image
scp arch/arm64/boot/Image pi@<pi-ip>:/boot/firmware/kernel8.img

# Install modules directly onto the Pi
make ARCH=arm64 CROSS_COMPILE=aarch64-elf- \
    INSTALL_MOD_PATH=/tmp/rpi-modules \
    modules_install

scp -r /tmp/rpi-modules/lib/modules/ pi@<pi-ip>:/lib/modules/

ssh pi@<pi-ip> sudo reboot
```

---

## Compile a Single DTS File (No Full Kernel Build)

For quick iteration on a custom DTS during Phase 1/2 work:

```bash
# Compile DTS → DTB
dtc -I dts -O dtb -o my-board.dtb my-board.dts

# Decompile a DTB back to readable DTS
dtc -I dtb -O dts -o my-board-decoded.dts my-board.dtb

# Compile with symbol export (needed for overlays)
dtc -@ -I dts -O dtb -o my-board.dtb my-board.dts
```

For DTS files that use `#include` directives (like real kernel DTS files), run the C preprocessor first:

```bash
aarch64-elf-gcc -E -x assembler-with-cpp \
    -I arch/arm64/boot/dts/broadcom \
    -I include \
    arch/arm64/boot/dts/broadcom/bcm2711-rpi-4-b.dts \
    | dtc -I dts -O dtb -o bcm2711-rpi-4-b.dtb -
```

---

## Troubleshooting

### `make: aarch64-elf-gcc: No such file or directory`

The toolchain isn't on `$PATH`. Homebrew installs to a versioned prefix:

```bash
# Find the actual path
ls $(brew --prefix)/bin/aarch64-elf-gcc*

# Add Homebrew bin to PATH if missing
export PATH="$(brew --prefix)/bin:$PATH"
```

### `fatal error: openssl/bio.h: No such file or directory`

The kernel build needs OpenSSL headers:

```bash
brew install openssl
export LIBRARY_PATH="$(brew --prefix openssl)/lib:$LIBRARY_PATH"
export CPATH="$(brew --prefix openssl)/include:$CPATH"
```

### `make[1]: *** No rule to make target 'scripts/basic/fixdep'`

Run the config step first before building:

```bash
make ARCH=arm64 CROSS_COMPILE=aarch64-elf- bcm2711_defconfig
```

---

## Quick Reference

```bash
# One-time setup
brew install aarch64-elf-gcc dtc
export ARCH=arm64 CROSS_COMPILE=aarch64-elf-

# Clone Pi kernel
git clone --depth=1 https://github.com/raspberrypi/linux --branch rpi-6.6.y ~/rpi-linux

# Build DTBs only
cd ~/rpi-linux
make bcm2711_defconfig
make dtbs -j$(sysctl -n hw.logicalcpu)

# Deploy to Pi
scp arch/arm64/boot/dts/broadcom/bcm2711-rpi-4-b.dtb pi@<pi-ip>:/boot/firmware/
```
