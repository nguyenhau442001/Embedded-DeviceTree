# Phase 1 — DT Syntax & Tooling

## DTS File Types

| Extension | Purpose |
|---|---|
| `.dts` | Top-level board file — one per board variant |
| `.dtsi` | Include file — shared SoC or family definitions |
| `.dtb` | Compiled binary blob — what the bootloader loads |
| `.dtbo` | Compiled overlay blob — applied at runtime on top of a base DTB |

A `.dts` file pulls in `.dtsi` files via `#include`, layering board-specific nodes on top of SoC defaults.

---

## Node Syntax

```dts
/ {                              /* root node */
    model = "My Board";
    compatible = "vendor,my-board", "vendor,soc1";

    cpus {
        #address-cells = <1>;
        #size-cells = <0>;

        cpu@0 {
            compatible = "arm,cortex-a72";
            reg = <0>;           /* unit address matches node name suffix */
        };
    };

    memory@0 {
        device_type = "memory";
        reg = <0x0 0x0 0x0 0x40000000>;  /* base + size, 64-bit each */
    };
};
```

Rules:
- Node name format: `name@unit-address` — `unit-address` must match the first value of the `reg` property
- Every node that has `reg` needs `#address-cells` and `#size-cells` on its parent to define how many 32-bit cells encode an address and size

---

## Core Properties

### `compatible`
String list, most-specific first. The kernel walks this list to find a matching driver.
```dts
compatible = "raspberrypi,4-model-b", "brcm,bcm2711";
```

### `reg`
Physical address + size of the device's register block. Cell count set by parent's `#address-cells` / `#size-cells`.
```dts
uart0: serial@fe201400 {
    reg = <0x0 0xfe201400 0x0 0x200>;  /* 2-cell addr, 2-cell size */
};
```

### `#address-cells` / `#size-cells`
Tells children how many 32-bit cells to use in `reg`.
```dts
soc {
    #address-cells = <2>;   /* 64-bit address = 2 x u32 */
    #size-cells = <2>;
};
```

### `status`
```dts
status = "okay";     /* device is enabled */
status = "disabled"; /* driver won't probe */
```

### `interrupts` and `interrupt-parent`
```dts
uart0: serial@fe201400 {
    interrupt-parent = <&gic>;
    interrupts = <GIC_SPI 125 IRQ_TYPE_LEVEL_HIGH>;
};
```

---

## Phandles

A phandle is a unique integer that lets one node reference another — the DT equivalent of a pointer.

```dts
clk_uart: clock@7ef00000 {
    #clock-cells = <0>;
    compatible = "fixed-clock";
    clock-frequency = <48000000>;
};

uart0: serial@fe201400 {
    clocks = <&clk_uart>;    /* & dereferences the label as a phandle */
};
```

The compiler assigns phandle values automatically. You reference nodes by label (`&label`), never by raw integer.

---

## `aliases` and `/chosen`

```dts
/ {
    aliases {
        serial0 = &uart0;    /* /dev/ttyS0 → uart0 node */
    };

    chosen {
        bootargs = "console=serial0,115200 rootwait";
        stdout-path = "serial0:115200n8";
    };
};
```

- `aliases` — assigns short names to nodes; used by drivers and U-Boot to find devices by role
- `/chosen` — communication channel between bootloader and kernel; kernel reads `bootargs` from here

---

## Node Overriding with `&label`

`.dtsi` files define base nodes; `.dts` files override them using the label reference syntax:

```dts
/* bcm2711.dtsi defines: */
uart0: serial@fe201400 {
    status = "disabled";
};

/* bcm2711-rpi-4-b.dts enables it: */
&uart0 {
    status = "okay";
};
```

Properties in the override are merged in — only changed properties need to be listed.

---

## Tooling: `dtc`

### Install on Raspberry Pi
```bash
sudo apt install device-tree-compiler
dtc --version
```

### Compile DTS → DTB
```bash
dtc -I dts -O dtb -o output.dtb input.dts
```

### Decompile DTB → DTS
```bash
dtc -I dtb -O dts -o output.dts input.dtb
```

### Decompile the live running tree
```bash
dtc -I fs /proc/device-tree 2>/dev/null | less
```

### Dump a DTB in readable form (no dtc needed)
```bash
fdtdump /boot/firmware/bcm2711-rpi-4-b.dtb | less
```

---

## Device Tree Overlays

Overlays let you modify the live DT at runtime without recompiling the base DTB. Used heavily on Raspberry Pi to enable/configure optional hardware (I2C, SPI, UART, HATs).

### Structure of an overlay
```dts
/dts-v1/;
/plugin/;

/ {
    compatible = "brcm,bcm2711";

    fragment@0 {
        target = <&i2c1>;
        __overlay__ {
            status = "okay";
            clock-frequency = <400000>;
        };
    };
};
```

### Compile an overlay
```bash
dtc -I dts -O dtb -o myoverlay.dtbo myoverlay.dts
```

### Apply at runtime (Pi-specific)
Add to `/boot/firmware/config.txt`:
```ini
dtoverlay=myoverlay
```

Or apply manually after boot:
```bash
sudo dtoverlay myoverlay.dtbo
dtoverlay -l    # list active overlays
dtoverlay -r myoverlay   # remove
```

### Symbol mode for overlay phandle resolution
When an overlay references a phandle from the base DTB (e.g. `&i2c1`), `dtc` needs the base DTB compiled with symbol export enabled:
```bash
dtc -@ -I dts -O dtb -o base.dtb base.dts   # -@ exports symbols
```

---

## Hands-On: Decompile the Pi 4 DTB

On the Raspberry Pi:

```bash
# Decompile the shipped DTB
dtc -I dtb -O dts /boot/firmware/bcm2711-rpi-4-b.dtb 2>/dev/null > rpi4-decompiled.dts

# Count nodes
grep -c "^[[:space:]]*[a-z].*{" rpi4-decompiled.dts

# Find all compatible strings
grep "compatible" rpi4-decompiled.dts | sort -u

# Diff against upstream source (if kernel source is available)
wget -q https://raw.githubusercontent.com/raspberrypi/linux/rpi-6.6.y/arch/arm64/boot/dts/broadcom/bcm2711-rpi-4-b.dts
diff bcm2711-rpi-4-b.dts rpi4-decompiled.dts | head -60
```

Things to notice in the diff:
- Phandle values get assigned concrete integers by the compiler
- Labels disappear from the binary (but can be preserved with `-@`)
- Some properties get normalized (string encoding, cell ordering)

---

## References

- [Devicetree Specification v0.4](https://github.com/devicetree-org/devicetree-specification/releases)
- [Kernel DT usage docs](https://www.kernel.org/doc/html/latest/devicetree/usage-model.html)
- [bcm2711-rpi-4-b.dts upstream](https://elixir.bootlin.com/linux/latest/source/arch/arm64/boot/dts/broadcom/bcm2711-rpi-4-b.dts)
- [Raspberry Pi overlay docs](https://www.raspberrypi.com/documentation/computers/configuration.html#device-trees-overlays-and-parameters)
