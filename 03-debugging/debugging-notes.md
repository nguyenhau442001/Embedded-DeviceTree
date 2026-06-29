# Phase 3 — Real Hardware & Debugging

## Where to Look on a Live System

### `/proc/device-tree`
The live device tree mounted as a pseudo-filesystem. Each node is a directory, each property is a file.

```bash
# List top-level nodes
ls /proc/device-tree/

# Read a string property
cat /proc/device-tree/model && echo

# Read a binary property (reg, ranges, etc.)
xxd /proc/device-tree/memory@0/reg

# Navigate into a node
ls /proc/device-tree/soc/

# Find a node by compatible string
find /proc/device-tree -name compatible | xargs grep -l "brcm,bcm2711-gpio" 2>/dev/null
```

### `/sys/firmware/devicetree/base`
Identical content to `/proc/device-tree` — both are mounts of the same in-kernel OF tree. Use whichever path feels cleaner.

```bash
ls /sys/firmware/devicetree/base/
```

### Dump the full live tree as text
```bash
dtc -I fs /proc/device-tree 2>/dev/null | less
```

---

## 5 Deliberate Breaks — Symptoms & Fixes

Each of the following is a real class of DT error. The boot-log signature tells you exactly what went wrong.

---

### Break 1 — Wrong `reg` Value

**What you break:**
```dts
uart0: serial@fe201400 {
    reg = <0x0 0xfe201000 0x0 0x200>;  /* wrong address — should be 0xfe201400 */
};
```

**Boot-log signature:**
```
[    1.243] serial fe201000.serial: could not get clocks
[    1.244] serial fe201000.serial: probe with driver serial8250 failed with error -2
```
The node name suffix (`@fe201400`) no longer matches `reg`, so `dtc` warns at compile time:
```
Warning (unit_address_vs_reg): /soc/serial@fe201400: unit address and first address in reg (fe201000) are not equal
```

**Fix:** Make the first cell of `reg` match the `@unit-address` exactly.

---

### Break 2 — Missing Required Clock

**What you break:** Remove the `clocks` property from a node whose driver calls `devm_clk_get()`.

```dts
uart0: serial@fe201400 {
    /* clocks = <&clk_uart>; */   /* removed */
    status = "okay";
};
```

**Boot-log signature:**
```
[    1.301] fe201400.serial: could not get clocks: -2
[    1.302] fe201400.serial: probe with driver serial8250 failed with error -2
```
`-2` = `ENOENT` — the driver asked for a clock that doesn't exist in the DT node.

**Fix:** Restore the `clocks` (and `clock-names` if the driver uses named clocks) property.

---

### Break 3 — Wrong `compatible` String

**What you break:**
```dts
uart0: serial@fe201400 {
    compatible = "brcm,bcm2835-aux-uart";  /* wrong — Pi 4 uses bcm2711 PL011 */
};
```

**Boot-log signature:**
No probe message at all. The device appears in `/sys/bus/platform/devices/` but has no driver symlink:
```bash
ls -la /sys/bus/platform/devices/fe201400.serial/driver
# ls: cannot access ... No such file or directory
```

Check registered drivers vs device compatible:
```bash
cat /sys/bus/platform/devices/fe201400.serial/of_node/compatible | tr '\0' '\n'
# brcm,bcm2835-aux-uart    ← no driver matches this

ls /sys/bus/platform/drivers/ | grep serial
# amba-pl011   serial8250   ...    ← neither matches
```

**Fix:** Use the correct `compatible` string that a loaded driver declares in its `of_match_table`.

---

### Break 4 — Overlay Phandle Conflict

**What you break:** Compile an overlay without `-@` on the base DTB, then try to reference a base phandle.

```bash
# Base compiled WITHOUT symbol export
dtc -I dts -O dtb -o base.dtb base.dts   # missing -@

# Overlay references &i2c1 from base
dtc -I dts -O dtb -o myoverlay.dtbo myoverlay.dts
sudo dtoverlay myoverlay.dtbo
```

**Error signature:**
```
[ 10.442] OF: overlay: unflatten failed -22
[ 10.443] OF: overlay: overlay changeset entry notifier reported an error: -22
```
`-22` = `EINVAL` — the overlay's phandle reference couldn't be resolved because the base DTB has no symbol table.

**Fix:** Recompile the base DTB with symbol export:
```bash
dtc -@ -I dts -O dtb -o base.dtb base.dts
```

On Raspberry Pi, the firmware-shipped DTB already includes symbols — this issue appears when you build a custom base DTB without `-@`.

---

### Break 5 — Phandle Reference Loop

**What you break:** Create a circular phandle reference (node A's property points to node B, node B points back to A).

```dts
clk_a: clock@0 {
    clocks = <&clk_b>;   /* A depends on B */
};

clk_b: clock@1 {
    clocks = <&clk_a>;   /* B depends on A — loop */
};
```

**Compile-time warning:**
```
Warning (clocks_property): /clocks@0: cell 0 is not a phandle
```

**Runtime signature:** The clock framework detects the cycle during `clk_prepare_enable()`:
```
[    2.101] clk: detected cycle in clock tree, refusing to prepare
[    2.102] mydev fe300000.mydev: failed to enable clock: -22
```

**Fix:** Restructure the clock hierarchy so there are no cycles. Clocks must form a DAG (directed acyclic graph) — each clock has exactly one parent, no loops.

---

## `of_node` Refcounting

Every `device_node` is reference-counted. If you hold a reference beyond the natural lifetime of a DT lookup, you must release it manually.

```c
/* of_find_node_by_name increments refcount — must put */
struct device_node *np = of_find_node_by_name(NULL, "mydev");
if (np) {
    /* use np */
    of_node_put(np);   /* decrement refcount */
}

/* of_get_child_by_name also increments refcount */
struct device_node *child = of_get_child_by_name(parent, "port");
if (child) {
    of_node_put(child);
}
```

Functions that do **not** require `of_node_put`:
- `dev->of_node` — owned by the platform device, do not put
- `of_match_node()` result — does not increment

Leak symptom: `WARNING: CPU: 0 PID: 1 at drivers/of/dynamic.c:... kref_put` on device removal, or `of_node` refcount != 0 at shutdown.

---

## `dtc -@` Symbol Mode for Overlays

The `-@` flag instructs `dtc` to emit a `__symbols__` node listing all labeled nodes and their paths:

```bash
dtc -@ -I dts -O dtb -o base.dtb base.dts
```

Inside the DTB, this adds:
```dts
__symbols__ {
    uart0 = "/soc/serial@fe201400";
    i2c1  = "/soc/i2c@fe804000";
    gpio  = "/soc/gpio@7e200000";
    /* ... */
};
```

When you `dtoverlay` an overlay that references `&uart0`, the overlay loader resolves the label to the path via this symbol table, then finds the phandle. Without `__symbols__`, the resolution fails with `EINVAL`.

Check if a DTB has symbols:
```bash
fdtdump /boot/firmware/bcm2711-rpi-4-b.dtb | grep -A5 "__symbols__"
```

---

## Systematic Debugging Workflow

```
1. Check dmesg for OF/DT messages at boot
   dmesg | grep -E "OF:|of_|fdt|dtb|device-tree"

2. Confirm the node exists in the live tree
   find /proc/device-tree -name compatible | xargs grep -l "vendor,mydev"

3. Confirm the platform_device was created
   ls /sys/bus/platform/devices/ | grep mydev

4. Confirm a driver is bound
   ls -la /sys/bus/platform/devices/mydev.0/driver

5. If no driver bound — check compatible string match
   cat /sys/bus/platform/devices/mydev.0/of_node/compatible | tr '\0' '\n'
   ls /sys/bus/platform/drivers/

6. If probe failed — check probe return value
   dmesg | grep "mydev\|probe"

7. If property missing — inspect the node directly
   ls /proc/device-tree/soc/mydev@fe300000/
```

---

## Useful One-Liners

```bash
# All nodes that failed to probe
dmesg | grep "probe with driver.*failed"

# All compatible strings in the live tree
find /proc/device-tree -name compatible -exec sh -c 'printf "%s: " "$1"; cat "$1" | tr "\0" "\n"' _ {} \;

# All bound drivers
for d in /sys/bus/platform/devices/*/driver; do
    echo "$(basename $(dirname $d)) → $(basename $(readlink $d))";
done 2>/dev/null

# Show reg address of every node
find /proc/device-tree -name reg | while read f; do
    echo "$(dirname $f | sed 's|/proc/device-tree||'): $(xxd $f | head -1)"
done
```

---

## References

- [Kernel OF debugging docs](https://www.kernel.org/doc/html/latest/devicetree/of_unittest.html)
- [dtc man page](https://manpages.debian.org/unstable/device-tree-compiler/dtc.1.en.html)
- [Raspberry Pi overlay README](https://github.com/raspberrypi/linux/blob/rpi-6.6.y/arch/arm/boot/dts/overlays/README)
- [of_node refcount rules — drivers/of/dynamic.c](https://elixir.bootlin.com/linux/latest/source/drivers/of/dynamic.c)
