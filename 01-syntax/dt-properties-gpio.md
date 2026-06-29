# Device Tree Properties — GPIO & LED Walkthrough

## What Is a GPIO in Device Tree Terms

A GPIO (General Purpose Input/Output) is a physical pin on the SoC that software can control — set high (3.3V) or low (0V), or read as input.

In the device tree there are two roles:

| Role | What it is | Example |
|---|---|---|
| **GPIO controller** | The hardware block that owns the pins | `gpio@7e200000` on the Pi |
| **GPIO consumer** | A device that uses one of those pins | An LED, a reset line, a chip-select |

The controller is described once in the SoC `.dtsi`. Consumers reference it by phandle.

---

## Step 1 — The GPIO Controller Node

This is already written for you in the Pi's upstream DTS. Let's read it to understand each property.

From `arch/arm64/boot/dts/broadcom/bcm2711.dtsi`:

```dts
gpio: gpio@7e200000 {
    compatible = "brcm,bcm2711-gpio";
    reg = <0x7e200000 0xb4>;

    gpio-controller;
    #gpio-cells = <2>;

    interrupt-controller;
    #interrupt-cells = <2>;
};
```

### `compatible = "brcm,bcm2711-gpio"`
Tells the kernel which driver to load. The driver that matches this string is `drivers/pinctrl/bcm/pinctrl-bcm2835.c`. Without this, the kernel doesn't know what this hardware block is.

### `reg = <0x7e200000 0xb4>`
The physical address and size of the GPIO controller's register block in memory.
- `0x7e200000` — base address (bus address on the Pi's peripheral bus)
- `0xb4` — size in bytes (the register map is 180 bytes)

The driver calls `devm_ioremap_resource()` with these values to map the registers into kernel virtual address space.

### `gpio-controller`
A boolean flag (presence = true). Declares that this node **is** a GPIO controller — it provides GPIO lines to other nodes. No value needed.

### `#gpio-cells = <2>`
Tells consumers how many cells (32-bit integers) they need to reference one GPIO from this controller.

`<2>` means every GPIO reference looks like: `<&gpio  PIN  FLAGS>`
- Cell 1: pin number (0–57 on the Pi)
- Cell 2: flags (active high / active low)

You'll see this used in consumer nodes below.

### `interrupt-controller` + `#interrupt-cells = <2>`
Same pattern as GPIO but for interrupts — this controller can also route GPIO lines as interrupt sources. Not needed for basic LED usage.

---

## Step 2 — Pinctrl: Telling the Pin What Function to Use

Each GPIO pin can be muxed to different functions (UART TX, SPI CLK, plain GPIO, etc.). Before using a pin as GPIO output, you declare a pinctrl state.

```dts
&gpio {
    led_pin: led_pin {
        brcm,pins = <17>;           /* GPIO17 = physical pin 11 on the header */
        brcm,function = <1>;        /* 1 = output */
    };
};
```

### `brcm,pins = <17>`
Pi-specific property. Selects GPIO17. This is the **BCM number** (Broadcom chip pin number), not the physical header pin number.

Common BCM numbers for easy LED testing:

| BCM | Header pin | Notes |
|---|---|---|
| 17 | Pin 11 | General use |
| 27 | Pin 13 | General use |
| 22 | Pin 15 | General use |

### `brcm,function = <1>`
Sets the pin mux function:

| Value | Function |
|---|---|
| 0 | Input |
| 1 | Output |
| 4 | ALT0 (e.g. SPI, I2C, UART depending on pin) |
| 5 | ALT1 |

For an LED we want `1` (output).

---

## Step 3 — The LED Consumer Node

Now we describe the LED itself. The kernel has a generic `leds-gpio` driver that creates a `/sys/class/leds/` entry for any GPIO-connected LED.

```dts
/ {
    leds {
        compatible = "gpio-leds";

        led_green: led-green {
            label = "green";
            gpios = <&gpio 17 GPIO_ACTIVE_HIGH>;
            default-state = "off";
        };
    };
};
```

### `compatible = "gpio-leds"`
Matches the driver `drivers/leds/leds-gpio.c`. This driver reads the child nodes to find LEDs and registers each one with the LED subsystem.

### `label = "green"`
The name that appears in `/sys/class/leds/`. After booting:
```bash
ls /sys/class/leds/
# green  mmc0  ...
```

### `gpios = <&gpio 17 GPIO_ACTIVE_HIGH>`
This is the GPIO reference. Breaking it down cell by cell:

| Part | Meaning |
|---|---|
| `&gpio` | Phandle — points to the GPIO controller node labeled `gpio` |
| `17` | Pin number (cell 1, as defined by `#gpio-cells = <2>`) |
| `GPIO_ACTIVE_HIGH` | Flags (cell 2) — `0` means high = LED on |

`GPIO_ACTIVE_LOW` (`1`) would invert the logic — the LED turns on when the pin is driven low. Use this when your LED is wired between the GPIO pin and VCC instead of GND.

### `default-state = "off"`
Initial state at boot. Options: `"on"`, `"off"`, `"keep"` (keep whatever state the bootloader left it in).

### `pinctrl-names` + `pinctrl-0`
Links the LED node to the pin mux state we defined in Step 2:

```dts
led_green: led-green {
    label = "green";
    gpios = <&gpio 17 GPIO_ACTIVE_HIGH>;
    default-state = "off";
    pinctrl-names = "default";
    pinctrl-0 = <&led_pin>;       /* apply led_pin mux state at probe */
};
```

`pinctrl-names = "default"` names the state. `pinctrl-0` is the phandle to the pinctrl node for that state. The platform bus applies the `"default"` state automatically before the driver's `.probe()` runs.

---

## Step 4 — Full Example DTS Fragment

Putting it all together as a complete overlay you can apply on the Pi:

```dts
/dts-v1/;
/plugin/;

/ {
    compatible = "brcm,bcm2711";

    /* Fragment 0: set GPIO17 as output via pinctrl */
    fragment@0 {
        target = <&gpio>;
        __overlay__ {
            led_pin: led_pin {
                brcm,pins = <17>;
                brcm,function = <1>;    /* output */
                brcm,pull = <0>;        /* no pull resistor */
            };
        };
    };

    /* Fragment 1: register the LED device */
    fragment@1 {
        target-path = "/";
        __overlay__ {
            leds {
                compatible = "gpio-leds";

                led_green: led-green {
                    label = "green";
                    gpios = <&gpio 17 0>;   /* 0 = GPIO_ACTIVE_HIGH */
                    default-state = "off";
                    pinctrl-names = "default";
                    pinctrl-0 = <&led_pin>;
                };
            };
        };
    };
};
```

### `brcm,pull = <0>`
Sets the internal pull resistor on the pin:

| Value | Meaning |
|---|---|
| 0 | No pull (floating) |
| 1 | Pull down |
| 2 | Pull up |

For an output driving an LED, pull resistors don't matter much — use `0`.

---

## Step 5 — Build, Deploy, Test

### Compile the overlay on macOS

```bash
dtc -@ -I dts -O dtb -o led-green.dtbo led-green.dts
```

### Copy to the Pi

```bash
scp led-green.dtbo pi@<pi-ip>:/boot/firmware/overlays/
```

### Enable in `config.txt`

```bash
ssh pi@<pi-ip>
echo "dtoverlay=led-green" | sudo tee -a /boot/firmware/config.txt
sudo reboot
```

### Test after reboot

```bash
ssh pi@<pi-ip>

# Check the LED entry exists
ls /sys/class/leds/
# green

# Turn LED on
echo 1 | sudo tee /sys/class/leds/green/brightness

# Turn LED off
echo 0 | sudo tee /sys/class/leds/green/brightness

# Blink using the kernel timer trigger
echo timer | sudo tee /sys/class/leds/green/trigger
echo 500 | sudo tee /sys/class/leds/green/delay_on   # ms on
echo 500 | sudo tee /sys/class/leds/green/delay_off  # ms off
```

---

## Summary — Property Cheat Sheet

| Property | Where | What it does |
|---|---|---|
| `compatible` | Any node | Selects the driver |
| `reg` | Controller nodes | Physical address + size of register block |
| `gpio-controller` | GPIO controller | Declares this node provides GPIO lines |
| `#gpio-cells = <2>` | GPIO controller | Consumers need 2 cells: pin + flags |
| `brcm,pins` | Pinctrl node | Which GPIO pin numbers to configure |
| `brcm,function` | Pinctrl node | Pin mux function (0=in, 1=out, 4=ALT0…) |
| `brcm,pull` | Pinctrl node | Pull resistor (0=none, 1=down, 2=up) |
| `gpios` | Consumer node | `<&controller pin flags>` |
| `pinctrl-names` | Consumer node | Names the pinctrl states ("default") |
| `pinctrl-0` | Consumer node | Phandle to the pinctrl node for "default" state |
| `label` | LED node | Name in `/sys/class/leds/` |
| `default-state` | LED node | Initial state: "on", "off", "keep" |

---

## Next Step

Once you can control an LED via sysfs, the next natural step is [binding-notes.md](../02-custom-binding/binding-notes.md) — writing your own driver that probes from a DT node the same way `leds-gpio` does.
