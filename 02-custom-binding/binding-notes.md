# Phase 2 — Driver Binding

## How DT Binding Works

When the kernel boots, `of_platform_populate()` walks every node in the live device tree. For each node with a `compatible` string, it creates a `platform_device`. The platform bus then tries to match that device against registered `platform_driver` entries via their `of_match_table`. When a match is found, the driver's `.probe()` function is called with the matched node attached.

```
DT node (compatible = "vendor,mydev")
  → platform_device created by of_platform_populate()
    → platform bus matches against platform_driver.of_match_table
      → driver .probe() called
```

---

## `platform_driver` + `of_match_table`

```c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>

static const struct of_device_id mydev_of_match[] = {
    { .compatible = "vendor,mydev" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, mydev_of_match);

static int mydev_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    dev_info(dev, "probed from DT node: %pOF\n", dev->of_node);
    return 0;
}

static int mydev_remove(struct platform_device *pdev)
{
    return 0;
}

static struct platform_driver mydev_driver = {
    .probe  = mydev_probe,
    .remove = mydev_remove,
    .driver = {
        .name           = "mydev",
        .of_match_table = mydev_of_match,
    },
};
module_platform_driver(mydev_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Example DT-probed driver");
```

The DTS node that triggers this probe:
```dts
mydev@0 {
    compatible = "vendor,mydev";
    status = "okay";
};
```

---

## Reading Properties from DT

### Integers
```c
u32 val;
int ret = of_property_read_u32(dev->of_node, "clock-frequency", &val);
if (ret)
    dev_err(dev, "missing clock-frequency: %d\n", ret);
```

### Strings
```c
const char *name;
of_property_read_string(dev->of_node, "label", &name);
```

### Boolean (property presence = true)
```c
bool active_low = of_property_read_bool(dev->of_node, "active-low");
```

### Arrays
```c
u32 coords[2];
of_property_read_u32_array(dev->of_node, "position", coords, 2);
```

### GPIOs
```c
#include <linux/gpio/consumer.h>

struct gpio_desc *gpio;
gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
if (IS_ERR(gpio))
    return PTR_ERR(gpio);
```

DTS side:
```dts
mydev@0 {
    compatible = "vendor,mydev";
    reset-gpios = <&gpio 17 GPIO_ACTIVE_LOW>;
};
```

### Clocks
```c
#include <linux/clk.h>

struct clk *clk;
clk = devm_clk_get(dev, NULL);   /* NULL = first unnamed clock */
if (IS_ERR(clk))
    return PTR_ERR(clk);
clk_prepare_enable(clk);
```

DTS side:
```dts
mydev@0 {
    compatible = "vendor,mydev";
    clocks = <&clk_uart>;
    clock-names = "apb_pclk";
};
```

---

## `devm_*` Resource Management

`devm_*` functions tie resource lifetime to the device. When the device is removed (`.remove()` called or probe fails), all `devm_`-allocated resources are automatically released — no manual cleanup needed.

| Manual | devm equivalent |
|---|---|
| `kzalloc` / `kfree` | `devm_kzalloc` |
| `ioremap` / `iounmap` | `devm_ioremap` |
| `clk_get` / `clk_put` | `devm_clk_get` |
| `gpiod_get` / `gpiod_put` | `devm_gpiod_get` |
| `request_irq` / `free_irq` | `devm_request_irq` |

Rule: always prefer `devm_*` in new drivers. Only use manual cleanup when you need to release resources at a different point in time than device removal.

---

## Writing a YAML Binding

Modern kernel bindings are documented in YAML schema files under `Documentation/devicetree/bindings/`. The `dtschema` tooling validates actual DTS files against these schemas.

### Minimal YAML binding

```yaml
# Documentation/devicetree/bindings/misc/vendor,mydev.yaml
%YAML 1.2
---
$id: http://devicetree.org/schemas/misc/vendor,mydev.yaml#
$schema: http://devicetree.org/meta-schemas/core.yaml#

title: My Example Peripheral

maintainers:
  - Your Name <you@example.com>

properties:
  compatible:
    const: vendor,mydev

  reg:
    maxItems: 1

  clock-frequency:
    $ref: /schemas/types.yaml#/definitions/uint32
    description: Operating clock in Hz

  reset-gpios:
    maxItems: 1
    description: GPIO connected to the reset pin, active low

required:
  - compatible
  - reg

additionalProperties: false

examples:
  - |
    mydev@fe300000 {
        compatible = "vendor,mydev";
        reg = <0x0 0xfe300000 0x0 0x100>;
        clock-frequency = <48000000>;
        reset-gpios = <&gpio 17 1>;
    };
```

### Validating with `dtschema`

Install on the Pi or host machine:
```bash
pip3 install dtschema
```

Validate a single DTS against a schema:
```bash
dt-validate -s Documentation/devicetree/bindings/misc/vendor,mydev.yaml \
    arch/arm64/boot/dts/broadcom/bcm2711-rpi-4-b.dts
```

Validate all DTS files against all schemas (run from kernel source root):
```bash
make dtbs_check DT_SCHEMA_FILES=Documentation/devicetree/bindings/misc/vendor,mydev.yaml
```

---

## Pinctrl Bindings

Pinctrl lets a driver declare which pin states it needs; the pinctrl subsystem applies the right mux at probe time.

DTS side:
```dts
&uart0 {
    status = "okay";
    pinctrl-names = "default";
    pinctrl-0 = <&uart0_pins>;
};

&gpio {
    uart0_pins: uart0_pins {
        brcm,pins = <14 15>;
        brcm,function = <BCM2835_FSEL_ALT0>;
    };
};
```

Driver side — nothing needed. The platform bus applies the `"default"` pinctrl state automatically before `.probe()` is called.

---

## Regulator Consumer Bindings

```dts
mydev@0 {
    compatible = "vendor,mydev";
    vdd-supply = <&reg_3v3>;   /* phandle to a regulator node */
};
```

```c
#include <linux/regulator/consumer.h>

struct regulator *vdd;
vdd = devm_regulator_get(dev, "vdd");
if (IS_ERR(vdd))
    return PTR_ERR(vdd);
regulator_enable(vdd);
```

The `"vdd"` string is a prefix — the framework looks up `vdd-supply` in the DT node automatically.

---

## Hands-On: Probe a Fake Peripheral on QEMU

If you don't have a spare peripheral, you can test driver binding on QEMU's `virt` machine:

```bash
# On macOS, install QEMU
brew install qemu

# Boot a minimal ARM64 image
qemu-system-aarch64 \
  -machine virt \
  -cpu cortex-a57 \
  -m 512M \
  -kernel Image \
  -append "console=ttyAMA0" \
  -nographic
```

Add your fake node to `arch/arm64/boot/dts/qemu/qemu-virt.dts`:
```dts
mydev@9000000 {
    compatible = "vendor,mydev";
    reg = <0x0 0x09000000 0x0 0x1000>;
    status = "okay";
};
```

Rebuild DTB, relaunch QEMU, check `dmesg | grep mydev`.

---

## Debugging Probe Failures

```bash
# Check if the device was created from DT
ls /sys/bus/platform/devices/ | grep mydev

# Check if the driver is loaded
ls /sys/bus/platform/drivers/ | grep mydev

# Check probe result
cat /sys/bus/platform/devices/mydev.0/driver 2>/dev/null || echo "no driver bound"

# Kernel messages
dmesg | grep -i "mydev\|platform\|of:"
```

Common failures:
| Symptom | Cause |
|---|---|
| Device not in `/sys/bus/platform/devices/` | Node has `status = "disabled"` or missing `compatible` |
| Device present, no driver | `compatible` string mismatch between DTS and `of_match_table` |
| Probe called but returns error | Missing required property, clock/GPIO not available |

---

## References

- [Kernel driver model — platform devices](https://www.kernel.org/doc/html/latest/driver-api/driver-model/platform.html)
- [Writing devicetree bindings in YAML](https://www.kernel.org/doc/html/latest/devicetree/bindings/writing-schema.html)
- [devm resource management](https://www.kernel.org/doc/html/latest/driver-api/driver-model/devres.html)
- [dtschema tools](https://github.com/devicetree-org/dt-schema)
