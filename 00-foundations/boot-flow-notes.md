# Phase 0 — ARM Boot Flow & DT Fundamentals

## Boot Chain Overview

```
BootROM → SPL → U-Boot → Linux Kernel
```

### BootROM
- Burned into SoC at manufacture, immutable
- Knows how to read from a fixed boot medium (eMMC, SD, SPI NOR)
- Loads the next stage (SPL) into on-chip SRAM (DRAM not yet initialized)

### SPL (Secondary Program Loader)
- Tiny loader that fits in SRAM (~256 KB budget)
- Initializes DRAM, clocks, pinmux
- Loads full U-Boot into DRAM and jumps to it

### U-Boot
- Full bootloader: filesystem drivers, networking, scripting, environment variables
- Loads the kernel Image and DTB from storage into DRAM
- Passes the DTB physical address to the kernel via register `x0` (ARM64) / `r2` (ARM32)

### Linux Kernel
- Receives the flat device tree blob (FDT) address at entry
- `start_kernel()` → `setup_arch()` → `unflatten_device_tree()` converts the binary blob into the live `device_node` tree in memory
- Drivers query the live tree via `of_*` APIs at probe time

---

## Where the DTB Physically Enters the Chain

```
Storage (SD / eMMC)
  └── U-Boot loads:
        ├── Image (kernel binary)  → DRAM @ ${kernel_addr_r}
        └── bcm2711-rpi-4-b.dtb   → DRAM @ ${fdt_addr_r}

U-Boot → booti ${kernel_addr_r} - ${fdt_addr_r}
  passes fdt_addr_r in register x0 to kernel entry point
```

Key point: the DTB address is **not** embedded in the kernel Image. This is what allows one kernel binary to boot many different boards — U-Boot selects and loads the right `.dtb` at runtime.

### Verifying on Raspberry Pi

```bash
# U-Boot prints the FDT address during boot — visible on serial console:
# "Flattened Device Tree blob at 2eff6200"

# After kernel boots, confirm the DTB was consumed:
cat /proc/device-tree/model && echo
# Raspberry Pi 4 Model B Rev 1.4
```

---

## ARM32 ATAGS vs Device Tree

| | ATAGS (pre-2011) | Device Tree |
|---|---|---|
| Format | Linked list of tagged C structs in DRAM | Flattened binary blob (FDT spec) |
| Board info location | Hardcoded in bootloader | Described in `.dts`, compiled to `.dtb` |
| Kernel per-board code | Required (`mach_desc`, board init functions) | Not required — generic kernel + board DTB |
| Extensibility | Fixed struct layout, kernel patch needed | Open-ended property/node schema |

ATAGS could only describe a narrow set of parameters: memory layout, kernel cmdline, initrd address. Any new peripheral required both a bootloader change and a kernel patch.

---

## Board File vs DT-Described Platform

### Pre-DT board file (pre-2011)

```
arch/arm/mach-omap2/board-omap3beagle.c
arch/arm/mach-bcm2708/bcm2708.c          # original Raspberry Pi
```

Each file hardcoded clock rates, GPIO assignments, `platform_device` registrations, IRQ numbers in C. Adding a new board variant = new `.c` file + full kernel rebuild.

Example — registering an I2C device in a board file:
```c
static struct i2c_board_info __initdata beagle_i2c1_boardinfo[] = {
    { I2C_BOARD_INFO("tps65950", 0x48), .irq = INT_34XX_SYS_NIRQ },
};
```

### DT era

```
arch/arm/boot/dts/ti/omap/omap3-beagle.dts
arch/arm64/boot/dts/broadcom/bcm2711-rpi-4-b.dts   # Pi 4
```

The same I2C device described declaratively:
```dts
&i2c1 {
    clock-frequency = <2600000>;
    twl: twl@48 {
        compatible = "ti,twl4030";
        reg = <0x48>;
        interrupts = <7>;
    };
};
```

The driver matches on `compatible`, not on a C struct registered at boot. No board-specific kernel code needed.

### Raspberry Pi specifically

The Pi 4 DTS lives at:
```
arch/arm64/boot/dts/broadcom/bcm2711-rpi-4-b.dts
  └── includes bcm2711.dtsi        (SoC-level nodes)
        └── includes bcm2711-rpi.dtsi  (Pi-family shared)
```

You can read it on the Pi itself:
```bash
# Decompile the running DTB back to source
dtc -I fs /proc/device-tree 2>/dev/null | less
```

---

## Why the Kernel Forced the Switch

By ~2010, `arch/arm/` had over 700 board files — more than any other architecture, and growing faster than the rest of the kernel combined. Russell King (ARM maintainer) declared he would stop accepting new board files unless the submitter migrated to DT.

Root problems with board files:
- Hardware description was mixed with kernel policy in C code
- Rebuilding the kernel was required to support a new board variant
- One kernel Image could not boot multiple boards
- Board files duplicated each other with minor register-value changes

The DT migration started in 2011. By 2013, new ARM SoC submissions without DT support were rejected upstream. The Raspberry Pi Foundation upstreamed Pi DT support starting with the Pi 2.

---

## Key Kernel Entry Points

| Function | File | What it does |
|---|---|---|
| `start_kernel()` | `init/main.c` | Top-level kernel init |
| `setup_arch()` | `arch/arm64/kernel/setup.c` | Arch setup, scans FDT for memory/cmdline |
| `unflatten_device_tree()` | `drivers/of/fdt.c` | Converts FDT blob → live `device_node` tree |
| `of_platform_populate()` | `drivers/of/platform.c` | Walks tree, creates `platform_device` per node |

---

## Hands-On: Observe the Boot Chain on Pi

### 1. Watch U-Boot on serial console

Connect via serial (see [rpi-setup.md](rpi-setup.md)) and power on. Look for:
```
U-Boot 2023.xx
...
Loading Device Tree to 000000002eff6200, end 000000002efff177 ... OK
...
Starting kernel ...
```

### 2. Confirm FDT was consumed by kernel

```bash
# Compatible string — set from DT root node
cat /proc/device-tree/compatible | tr '\0' '\n'

# Memory layout — parsed from DT memory node
cat /proc/device-tree/memory@0/reg | xxd

# Kernel cmdline — passed via DT /chosen node
cat /proc/device-tree/chosen/bootargs
```

### 3. Find the Pi 4 DTB on the SD card

The firmware partition contains the compiled DTBs:
```bash
ls /boot/firmware/*.dtb
# bcm2711-rpi-4-b.dtb   bcm2711-rpi-cm4.dtb  ...

# Decompile one to see the source
dtc -I dtb -O dts /boot/firmware/bcm2711-rpi-4-b.dtb 2>/dev/null | head -60
```

---

## References

- [ARM64 Booting Requirements](https://www.kernel.org/doc/html/latest/arm64/booting.html)
- [Devicetree Specification](https://github.com/devicetree-org/devicetree-specification)
- [drivers/of/fdt.c](https://elixir.bootlin.com/linux/latest/source/drivers/of/fdt.c)
- [bcm2711-rpi-4-b.dts](https://elixir.bootlin.com/linux/latest/source/arch/arm64/boot/dts/broadcom/bcm2711-rpi-4-b.dts)
