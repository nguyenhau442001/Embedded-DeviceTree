# Raspberry Pi 4 — Pin Numbering Reference

## Three Numbering Systems

The Pi has three different ways to refer to a pin, which causes endless confusion:

| System | What it is | Used in |
|---|---|---|
| **Physical** | The actual pin position on the 40-pin header (1–40) | Hardware wiring, datasheets |
| **BCM** | Broadcom chip GPIO number | Device Tree, kernel driver, `libgpiod` |
| **WiringPi** | A third-party library's own numbering | `wiringPi` library only — ignore this |

**The Device Tree always uses BCM numbers.** When you write `brcm,pins = <17>`, that is BCM17.

---

## 40-Pin Header Map

```
                    3V3  (1) (2)  5V
          BCM2  SDA1 I2C (3) (4)  5V
          BCM3  SCL1 I2C (5) (6)  GND
              BCM4 GPCLK (7) (8)  BCM14 UART TX
                    GND  (9) (10) BCM15 UART RX
               BCM17    (11) (12) BCM18 PCM CLK
               BCM27    (13) (14) GND
               BCM22    (15) (16) BCM23
                    3V3 (17) (18) BCM24
          BCM10  SPI0 MOSI (19) (20) GND
           BCM9  SPI0 MISO (21) (22) BCM25
          BCM11  SPI0 SCLK (23) (24) BCM8  SPI0 CE0
                    GND (25) (26) BCM7  SPI0 CE1
           BCM0  I2C ID EEPROM (27) (28) BCM1  I2C ID EEPROM
               BCM5    (29) (30) GND
               BCM6    (31) (32) BCM12
              BCM13    (33) (34) GND
              BCM19    (35) (36) BCM16
              BCM26    (37) (38) BCM20
                    GND (39) (40) BCM21
```

### Readable table — Physical pin → BCM number

| Physical | BCM | Notes |
|---|---|---|
| 1 | — | 3.3V power |
| 2 | — | 5V power |
| 3 | 2 | I2C1 SDA |
| 4 | — | 5V power |
| 5 | 3 | I2C1 SCL |
| 6 | — | GND |
| 7 | 4 | GPCLK0 |
| 8 | 14 | UART TX |
| 9 | — | GND |
| 10 | 15 | UART RX |
| **11** | **17** | **General use ← good for LED** |
| 12 | 18 | PCM CLK |
| **13** | **27** | **General use ← good for LED** |
| 14 | — | GND |
| **15** | **22** | **General use ← good for LED** |
| 16 | 23 | General use |
| 17 | — | 3.3V power |
| 18 | 24 | General use |
| 19 | 10 | SPI0 MOSI |
| 20 | — | GND |
| 21 | 9 | SPI0 MISO |
| 22 | 25 | General use |
| 23 | 11 | SPI0 SCLK |
| 24 | 8 | SPI0 CE0 |
| 25 | — | GND |
| 26 | 7 | SPI0 CE1 |
| 27 | 0 | I2C ID EEPROM (avoid) |
| 28 | 1 | I2C ID EEPROM (avoid) |
| 29 | 5 | General use |
| 30 | — | GND |
| 31 | 6 | General use |
| 32 | 12 | PWM0 |
| 33 | 13 | PWM1 |
| 34 | — | GND |
| 35 | 19 | SPI1 MISO |
| 36 | 16 | SPI1 CE0 |
| 37 | 26 | General use |
| 38 | 20 | SPI1 MOSI |
| 39 | — | GND |
| 40 | 21 | SPI1 SCLK |

---

## How to Read the Header Physically

Pin 1 is identified by a **square solder pad** on the PCB (all others are round), and is closest to the SD card slot on Pi 4.

```
SD card side
┌─────────────────────────────────────────────┐
│  [1][3][5][7][9][11][13][15][17]...[39]     │  ← odd pins (left column)
│  [2][4][6][8][10][12][14][16][18]...[40]    │  ← even pins (right column)
└─────────────────────────────────────────────┘
                                       USB/Ethernet side
```

Pin 1 (top-left) = 3.3V. Pin 2 (top-right) = 5V.

---

## Recommended Pins for Beginners

These BCM pins are safe to use as general GPIO — they have no special boot function and won't conflict with UART, I2C, SPI, or the camera:

| BCM | Physical | Why safe |
|---|---|---|
| 17 | 11 | No ALT function conflicts |
| 27 | 13 | No ALT function conflicts |
| 22 | 15 | No ALT function conflicts |
| 23 | 16 | No ALT function conflicts |
| 24 | 18 | No ALT function conflicts |
| 25 | 22 | No ALT function conflicts |

Pins to **avoid** for beginners:

| BCM | Physical | Why avoid |
|---|---|---|
| 14, 15 | 8, 10 | UART TX/RX — used by serial console |
| 2, 3 | 3, 5 | I2C — has on-board pull-ups |
| 0, 1 | 27, 28 | ID EEPROM — reserved |
| 4 | 7 | GPCLK — used by some overlays |

---

## Verify on the Pi Itself

```bash
# Install pinout tool
pip3 install gpiozero

# Show the full pinout in the terminal
pinout
```

Or check online: https://pinout.xyz — interactive, shows BCM / physical / WiringPi for every pin.

---

## In the Device Tree

When you write a DT node referencing a GPIO, always use the **BCM number**:

```dts
/* LED on physical pin 11 = BCM17 */
ctrl-gpios = <&gpio 17 0>;
/*                   ^^
/*                   BCM number, not physical pin number */
```

The `&gpio` phandle points to the Pi's GPIO controller node which owns BCM0–BCM57.
