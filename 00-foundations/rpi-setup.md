# Raspberry Pi 4/5 — Setup Guide (macOS)

## Hardware Requirements

| Item | Notes |
|---|---|
| Raspberry Pi 4B or 5 | 2 GB RAM minimum; 4 GB+ recommended |
| microSD card | 16 GB+ Class 10 / A1 rated |
| USB-A to USB-C power supply | 5V/3A for Pi 4, 5V/5A (USB-C PD) for Pi 5 |
| USB-to-TTL serial adapter | CP2102 or CH340 — 3.3V logic level only |
| Jumper wires (3x) | TX, RX, GND |

---

## Flashing Raspberry Pi OS (64-bit Lite)

```bash
brew install --cask raspberry-pi-imager
```

Steps in Imager:
1. **Choose OS** → *Raspberry Pi OS (other)* → **Raspberry Pi OS Lite (64-bit)**
2. **Choose Storage** → select your SD card
3. Click the gear icon (**Advanced options**) before writing:
   - Enable SSH → use password authentication
   - Set hostname: `raspberrypi.local`
   - Set username + password
   - Configure WiFi SSID + password
4. **Write** → wait for flash + verify

---

## Enabling UART Console

Mount the `bootfs` FAT partition (it auto-mounts on macOS after flashing) and edit two files:

**`/Volumes/bootfs/config.txt`** — add at the bottom:
```ini
# Free primary UART from Bluetooth, route it to GPIO pins
dtoverlay=disable-bt
enable_uart=1
```

**`/Volumes/bootfs/cmdline.txt`** — verify `console=serial0,115200` is present (it should be by default).

Eject the SD card before inserting into the Pi.

---

## Wiring the Serial Adapter

GPIO header (40-pin), looking from above with pin 1 at top-left:

```
Pin 6  → GND   → GND on adapter
Pin 8  → TX    → RX  on adapter
Pin 10 → RX    → TX  on adapter
```

**Never connect the adapter's 5V pin to the Pi.**

---

## Connecting via Serial Console (macOS)

```bash
# Find the device node after plugging in the adapter
ls /dev/cu.usbserial-*    # CP2102
ls /dev/cu.wchusbserial*  # CH340

# Connect at 115200 baud
screen /dev/cu.usbserial-XXXX 115200
```

Power on the Pi — you should see U-Boot output then the kernel boot log within ~10 seconds.

Exit `screen`: `Ctrl-A` then `K`, confirm `y`.

---

## SSH Access

```bash
ssh <your-username>@raspberrypi.local
# or by IP if mDNS doesn't resolve
ssh <your-username>@<ip-address>
```

Find the IP from the serial console boot log or your router's DHCP table.

---

## First Boot Verification

Once logged in, confirm the device tree is live:

```bash
# Kernel version (should be 6.x)
uname -r

# Root compatible string — identifies the board
cat /proc/device-tree/compatible | tr '\0' '\n'
# Pi 4 expected output:
#   raspberrypi,4-model-b
#   brcm,bcm2711

# Human-readable model string
cat /proc/device-tree/model && echo
# Raspberry Pi 4 Model B Rev 1.x

# Top-level DT nodes
ls /proc/device-tree/
# aliases  chosen  cpus  memory  reserved-memory  soc  ...
```

---

## Installing Development Tools on the Pi

```bash
sudo apt update
sudo apt install -y device-tree-compiler build-essential git
```

Verify:
```bash
dtc --version
# dtc: Version DTC 1.6.x
```

---

## Useful Commands

```bash
# Dump the full live device tree as readable text
dtc -I fs /proc/device-tree 2>/dev/null | less

# DT-related kernel boot messages
dmesg | grep -i "device tree\|fdt\|dtb"

# Platform devices probed from DT
ls /sys/bus/platform/devices/ | head -20
```

---

## Next Step

With the Pi booted and serial + SSH confirmed, move on to [boot-flow-notes.md](boot-flow-notes.md) to understand exactly how U-Boot handed the DTB to the kernel you just inspected.
