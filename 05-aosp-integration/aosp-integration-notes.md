# Phase 5 — AOSP Integration

## How Android's Kernel Build Integrates DT

### DTB/DTBO in the Android Boot Image

Android devices carry DT blobs in two places:

| Location | Content | Built by |
|---|---|---|
| `boot.img` (GKI kernel) | Base DTB for the SoC | Kernel build (`make dtbs`) |
| `vendor_boot.img` or separate `dtbo.img` | DT overlays (board-specific) | Device vendor |

At boot, the bootloader (typically ABoot/LK or U-Boot):
1. Loads the base DTB from `boot.img`
2. Selects and applies the matching DTBO from `dtbo.img` based on board ID / SKU
3. Passes the merged DTB to the kernel

### `ANDROIDBOOT.*` params via `/chosen`

The bootloader populates `/chosen/bootargs` with Android-specific parameters:

```dts
chosen {
    bootargs = "androidboot.hardware=bcm2711 \
                androidboot.serialno=XXXX \
                androidboot.verifiedbootstate=green \
                androidboot.slot_suffix=_a";
};
```

The kernel exposes these as `/proc/cmdline`. Android's `init` process reads them to set system properties:

```
androidboot.hardware  →  ro.hardware
androidboot.serialno  →  ro.serialno
```

This is the bridge between DT/bootloader and the Android property system.

---

## Vendor DLKM and DT

**Generic Kernel Image (GKI)** separates the kernel from vendor modules:

```
GKI kernel (google-maintained)
  └── vendor_dlkm partition
        └── vendor kernel modules (.ko)
              └── each module probes devices via DT compatible strings
```

A vendor DLKM driver works exactly like any other `platform_driver` — it registers an `of_match_table`, and the kernel's platform bus binds it to DT nodes. The DT node itself lives in the vendor DTBO.

### Typical DTBO structure for a vendor peripheral

```dts
/* vendor/device/boardname/boardname.dts */
/dts-v1/;
/plugin/;

/ {
    fragment@0 {
        target-path = "/";
        __overlay__ {
            /* Vendor-specific peripheral */
            nfc: nfc@29 {
                compatible = "nxp,pn553";
                reg = <0x29>;
                interrupt-parent = <&tlmm>;
                interrupts = <37 IRQ_TYPE_LEVEL_HIGH>;
                enable-gpios = <&tlmm 12 GPIO_ACTIVE_HIGH>;
                status = "okay";
            };
        };
    };
};
```

---

## Soong/Bazel DTB Build Targets

### Soong (Android.bp)

```bp
// In device/<vendor>/<board>/Android.bp
dtb {
    name: "bcm2711-rpi-4-b",
    srcs: ["arch/arm64/boot/dts/broadcom/bcm2711-rpi-4-b.dts"],
}

dtbo {
    name: "bcm2711-rpi-4-b-overlay",
    srcs: ["arch/arm64/boot/dts/overlays/bcm2711-rpi-4-b-overlay.dts"],
    base_dtb: ":bcm2711-rpi-4-b",
}
```

### How `dtbo.img` is assembled

Multiple DTBOs are packed into a single `dtbo.img` using `mkdtimg`:

```bash
mkdtimg create dtbo.img \
    --page_size=4096 \
    overlay1.dtbo \
    overlay2.dtbo \
    overlay3.dtbo
```

The bootloader selects the right DTBO at runtime using the `board_id` / `rev` fields embedded in each DTBO header, matched against hardware strapping or PMIC registers.

---

## End-to-End Trace: DT Node → HAL

Using NFC as a concrete example (`nxp,pn553`):

### Step 1 — DT node in DTBO
```dts
nfc: nfc@29 {
    compatible = "nxp,pn553";
    reg = <0x29>;
    interrupts = <37 IRQ_TYPE_LEVEL_HIGH>;
    enable-gpios = <&tlmm 12 GPIO_ACTIVE_HIGH>;
    status = "okay";
};
```

### Step 2 — Kernel driver probes from DT
```c
/* drivers/nfc/pn553.c */
static const struct of_device_id pn553_of_match[] = {
    { .compatible = "nxp,pn553" },
    {}
};

static int pn553_probe(struct i2c_client *client,
                       const struct i2c_device_id *id)
{
    struct gpio_desc *enable = devm_gpiod_get(&client->dev, "enable", GPIOD_OUT_LOW);
    /* ... */
}
```

### Step 3 — Sysfs / dev node created
After successful probe:
```
/dev/pn553               ← char device for userspace access
/sys/bus/i2c/devices/1-0029/   ← sysfs node
/sys/bus/i2c/devices/1-0029/of_node/compatible  → "nxp,pn553"
```

### Step 4 — uevent triggers HAL loading
`udevd` (or Android's `ueventd`) receives the `KOBJ_ADD` uevent when the device is created:
```
ACTION=add
SUBSYSTEM=i2c
DEVPATH=/bus/i2c/devices/1-0029
```

`ueventd.rc` rules map this to device node creation:
```
/dev/pn553   0660   nfc   nfc
```

### Step 5 — HAL opens the device
```cpp
/* hardware/nxp/nfc/NxpNfc.cpp */
int NxpNfc::open() {
    mFd = ::open("/dev/pn553", O_RDWR);
    /* ... */
}
```

The HAL is declared in `android.hardware.nfc@1.2-service.rc`:
```
service vendor.nfc_hal_service /vendor/bin/hw/android.hardware.nfc@1.2-service
    interface android.hardware.nfc@1.2::INfc default
```

### Step 6 — Framework consumes HAL
```java
// NfcService.java (frameworks/base)
NfcAdapter adapter = NfcAdapter.getDefaultAdapter(context);
// → calls into HAL via HIDL/AIDL → HAL opens /dev/pn553
```

Full chain:
```
DT node (nxp,pn553)
  → kernel driver probe → /dev/pn553 + uevent
    → ueventd creates device node
      → HAL service opens /dev/pn553
        → HIDL/AIDL interface
          → NfcService (Java framework)
            → NfcAdapter (app-facing API)
```

---

## Verifying the Chain on a Device

```bash
# 1. Confirm DT node exists
find /proc/device-tree -name compatible | xargs grep -l "nxp,pn553" 2>/dev/null

# 2. Confirm driver is bound
ls /sys/bus/i2c/drivers/pn553/

# 3. Confirm device node exists
ls -la /dev/pn553

# 4. Confirm HAL service is running
adb shell ps -A | grep nfc

# 5. Check HAL interface registration
adb shell hwservicemanager list | grep nfc
# android.hardware.nfc@1.2::INfc/default

# 6. Check sysfs properties exposed to HAL
adb shell cat /sys/bus/i2c/devices/1-0029/of_node/compatible | tr '\0' '\n'
```

---

## DT Properties That Surface to Android Properties

Some DT properties are read by Android's `init` or HAL layer and converted to system properties:

| DT location | Android property | Set by |
|---|---|---|
| `/chosen/bootargs androidboot.hardware` | `ro.hardware` | `init` |
| `/chosen/bootargs androidboot.revision` | `ro.revision` | `init` |
| `/firmware/android/compatible` | `ro.product.board` | vendor `init.rc` |
| `/model` | sometimes `ro.product.model` | vendor `init.rc` |

Read them on a device:
```bash
adb shell getprop ro.hardware
adb shell cat /proc/device-tree/model && echo
```

---

## Using a Public AOSP Reference Device Tree

The Raspberry Pi community maintains an AOSP tree for Pi 4:

```bash
# Clone the device tree (manifest-based)
repo init -u https://github.com/android-rpi/local_manifests
repo sync -j4

# Device DT lives at:
# device/arpi/rpi4/rpi4.dts
# kernel DTS:
# kernel/arpi/linux/arch/arm64/boot/dts/broadcom/bcm2711-rpi-4-b.dts
```

Key files to study:
```
device/arpi/rpi4/
├── BoardConfig.mk          ← BOARD_KERNEL_CMDLINE, DTB paths
├── rpi4.dts                ← board DTBO
├── ueventd.rpi4.rc         ← device node permissions from DT
└── init.rpi4.rc            ← reads androidboot.* props
```

---

## References

- [Android DTB/DTBO image format](https://source.android.com/docs/core/architecture/dto/partitions)
- [GKI overview](https://source.android.com/docs/core/architecture/kernel/generic-kernel-image)
- [DTO (Dynamic Test Overlays) in Android](https://source.android.com/docs/core/architecture/dto)
- [android-rpi AOSP for Pi 4](https://github.com/android-rpi)
- [HIDL to AIDL migration](https://source.android.com/docs/core/architecture/aidl/aidl-hals)
