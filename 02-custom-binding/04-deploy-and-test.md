# Deploy and Test — mydev-led

## Overview

| Task | Where |
|---|---|
| Compile the DTB overlay (`.dtbo`) | macOS |
| Copy overlay + driver source to Pi | macOS → Pi via scp |
| Build the kernel module (`.ko`) | Raspberry Pi |
| Load overlay + module, test | Raspberry Pi |

---

## Step 1 — Compile the DTB Overlay on macOS

```bash
cd 02-custom-binding

dtc -@ -I dts -O dtb -o mydev-led.dtbo 01-dt-overlay.dts
```

`-@` exports symbols so the overlay can resolve phandles (like `&gpio`) from the base DTB.

Verify the output exists:
```bash
ls -lh mydev-led.dtbo
# -rw-r--r-- 1 ... 512 mydev-led.dtbo
```

---

## Step 2 — Copy Files to the Pi

```bash
# Replace <pi-ip> with your Pi's IP address

# Copy the overlay to the Pi's overlays directory
scp mydev-led.dtbo pi@<pi-ip>:/boot/firmware/overlays/

# Copy the driver source + Makefile to the Pi
scp mydev-led-driver.c Makefile pi@<pi-ip>:~/mydev-led/
```

---

## Step 3 — Enable the Overlay on the Pi

SSH into the Pi:
```bash
ssh pi@<pi-ip>
```

Add the overlay to `config.txt`:
```bash
echo "dtoverlay=mydev-led" | sudo tee -a /boot/firmware/config.txt
```

Reboot:
```bash
sudo reboot
```

After reboot, verify the DT node is live:
```bash
# The node should appear in the live tree
find /proc/device-tree -name compatible | xargs grep -l "myvendor,mydev" 2>/dev/null
# /proc/device-tree/mydev@0/compatible

# Read the label property back
cat /proc/device-tree/mydev@0/label && echo
# mydev-led
```

---

## Step 4 — Build the Kernel Module on the Pi

```bash
# Install kernel headers (one-time)
sudo apt update
sudo apt install -y linux-headers-$(uname -r)

# Build the module
cd ~/mydev-led
make

# Expected output:
# make -C /lib/modules/6.6.x-rpi.../build M=... modules
#   CC [M]  mydev-led-driver.o
#   MODPOST modules.list
#   LD [M]  mydev-led-driver.ko
```

Verify:
```bash
ls -lh mydev-led-driver.ko
# -rw-r--r-- 1 pi pi 5.x mydev-led-driver.ko
```

---

## Step 5 — Load the Module

```bash
sudo insmod mydev-led-driver.ko
```

Check dmesg for the probe message:
```bash
dmesg | tail -5
# [  42.123] mydev mydev@0: mydev-led probed! label=mydev-led gpio=17
```

If you see that line — the driver found the DT node, read the label, and claimed GPIO17.

---

## Step 6 — Toggle the LED via Sysfs

```bash
# Find the sysfs path
ls /sys/bus/platform/devices/ | grep mydev
# mydev@0

# Turn LED on
echo 1 | sudo tee /sys/bus/platform/devices/mydev@0/led

# Turn LED off
echo 0 | sudo tee /sys/bus/platform/devices/mydev@0/led

# Read current state
cat /sys/bus/platform/devices/mydev@0/led
# 0 or 1
```

---

## Step 7 — Unload the Module

```bash
sudo rmmod mydev_led_driver

dmesg | tail -3
# [  65.456] mydev mydev@0: mydev-led removed
```

---

## Wiring Reminder

```
Pi 40-pin header

Pin 11 (BCM17) ──── 330Ω resistor ──── LED (+) anode
                                        LED (-) cathode ──── Pin 9 (GND)
```

Use a 330Ω resistor to limit current. The Pi's GPIO outputs 3.3V max — without the resistor you risk damaging the GPIO pin.

---

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `find /proc/device-tree` returns nothing | Overlay not applied | Check `/boot/firmware/config.txt` has `dtoverlay=mydev-led`, reboot |
| `insmod: ERROR: could not insert module` | Kernel version mismatch | Rebuild `.ko` on the Pi after `sudo apt upgrade` |
| `failed to get ctrl-gpios` in dmesg | GPIO17 in use by another driver | Check `dtoverlay -l`, remove conflicting overlay |
| LED doesn't light up | Wiring issue | Check resistor, LED polarity, correct pin (physical pin 11) |
| `dmesg` shows probe but no sysfs file | `device_create_file` failed | Check dmesg for the error code |
