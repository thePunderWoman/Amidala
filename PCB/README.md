# AmidalaBoard (AmidalaShield)

A custom KiCad PCB shield designed specifically for the [Amidala Firmware](https://github.com/thePunderWoman/AmidalaFirmware) — a control system for astromech and similar droid builds. This board provides all the I/O the firmware needs in a single, purpose-built package.

**Designed by [thePunderWoman](https://github.com/thePunderWoman)**

---

## Overview

The AmidalaShield is a carrier/breakout board for the **ESP32-S3 N16R8 DevKit**. It breaks out and conditions the ESP32-S3's peripherals for the specific needs of the Amidala Firmware: wireless XBee communication, multi-channel servo control, RC receiver input, SD card storage, and expansion headers for additional peripherals.

---

## Requirements

- **Microcontroller:** ESP32-S3 N16R8 DevKit (16 MB flash, 8 MB PSRAM)
- **Wireless modules:** XBee3 modules (x2 recommended — one for drive controller, one for dome controller)

---

## Hardware Features

### Wireless Communication
- **XBee3 module socket** for dual-controller operation (drive + dome)

### Servo Control
- **4× PWM servo outputs** — 3-pin headers (signal, power, ground)

### Serial Ports
- **3× hardware serial ports** — UART0 (shared with USB/programming, silkscreen "Serial 0"), UART1 ("Serial 1"), and UART2 ("Serial 2"). None of these are bit-banged/software serial — the ESP32-S3's UART peripherals route through its GPIO matrix onto whichever pins are needed, unlike chips with a single hardware UART. Firmware reassigns the dome drive and drive system's serial link between Serial 1/Serial 2 at runtime — see the Amidala firmware wiki's [Serial Ports](https://github.com/thePunderWoman/Amidala/wiki/Serial-Ports) page.

### I/O
- **4× digital I/O pins**
- **2× analog input pins**
- **1× Aux I2C header** — for external I2C devices

### RC Control
- **PPM input (ppmin)** — for analog RC receiver

### Storage
- **Built-in microSD card reader** — used for `config.txt` at startup (matches Amidala Firmware config system)

### Expansion
- **Dedicated SPI header** — for external SPI peripherals

---

## Repository Contents

| File/Folder | Description |
|---|---|
| `AmidalaShield.kicad_sch` | Top-level schematic |
| `comms.kicad_sch` | XBee communications sub-sheet |
| `io.kicad_sch` | Digital/analog I/O sub-sheet |
| `power.kicad_sch` | Power distribution sub-sheet |
| `rc_control.kicad_sch` | RC input sub-sheet |
| `servos.kicad_sch` | Servo output sub-sheet |
| `spi.kicad_sch` | SPI header sub-sheet |
| `gerbers/` | Production-ready Gerber files |
| `libraries/` | Project-specific KiCad symbol/footprint libraries |
| `images/` | Board renders and screenshots |

---

## Getting Started

1. Clone this repo and open `AmidalaShield.kicad_pro` in KiCad 8+.
2. Review the schematics for pin assignments before assembling.
3. Use the Gerber files in `gerbers/` to order PCBs from your preferred fab.
4. Flash your ESP32-S3 with the [Amidala Firmware](https://github.com/thePunderWoman/AmidalaFirmware).
5. Place a `config.txt` on the microSD card — see the firmware repo for a full annotated example.

---

## GPIO Table

> **V1.1** — Updated GPIO assignments for ESP32-S3 WROOM-1 N8R2/N16R2 (Quad flash/PSRAM).
> Avoid GPIO35–37 on R8 (Octal) variants. GPIO48 reserved for onboard RGB LED.

| Function | GPIO | DevKit Side | Notes |
|---|---|---|---|
| **SPI MOSI** | GPIO11 | Left | Native FSPI, shared bus |
| **SPI MISO** | GPIO13 | Left | Native FSPI, shared bus |
| **SPI SCK** | GPIO12 | Left | Native FSPI, shared bus |
| **SD Card CS** | GPIO10 | Left | |
| **XBee CS** | GPIO7 | Left | |
| **SPI Spare CS** | GPIO14 | Left | Exposed header |
| **XBee ATTN** | GPIO16 | Left | |
| **XBee SLEEP** | GPIO15 | Left | |
| **I2C SDA** | GPIO8 | Left | Native I2C default |
| **I2C SCL** | GPIO9 | Left | Native I2C default |
| **UART0 TX** | GPIO43 | Right | Shared w/ USB — do not use simultaneously. Silkscreen "Serial 0"; fixed, carries the main serial-out/WCB mesh path |
| **UART0 RX** | GPIO44 | Right | Shared w/ USB — do not use simultaneously |
| **UART1 TX** | GPIO17 | Right | Silkscreen "Serial 1". Reassignable between the dome/drive serial link (default: dome) |
| **UART1 RX** | GPIO18 | Right | |
| **UART2 TX** | GPIO21 | Right | Silkscreen "Serial 2". Real hardware UART, not software serial. Reassignable between the dome/drive serial link (default: drive) |
| **UART2 RX** | GPIO38 | Right | |
| **PPMIN** | GPIO47 | Right | |
| **Analog 1** | GPIO1 | Right | ADC1_0, WiFi-safe |
| **Analog 2** | GPIO2 | Right | ADC1_1, WiFi-safe |
| **Servo 1** | GPIO3 | Left | Strapping pin (JTAG source), safe after boot |
| **Servo 2** | GPIO4 | Left | |
| **Servo 3** | GPIO5 | Left | |
| **Servo 4** | GPIO6 | Left | |
| **Digital 1** | GPIO39 | Right | Goes HIGH after reset (not LOW like most GPIOs) |
| **Digital 2** | GPIO40 | Right | |
| **Digital 3** | GPIO41 | Right | |
| **Digital 4** | GPIO42 | Right | Transitions low on reset slightly slower |
| **RGB LED** | GPIO48 | Right | Reserved — onboard DevKit RGB LED |
| **NC** | GPIO8 | Left | Reserved |

### Pins Intentionally Avoided

| GPIO | Reason |
|---|---|
| GPIO0 | Strapping pin — boot mode |
| GPIO3 | Strapping pin — JTAG source (used for Servo 1, safe after boot) |
| GPIO19/20 | USB D-/D+ |
| GPIO26–32 | Internal SPI flash/PSRAM — never use |
| GPIO35–37 | Internal on Octal (R8) variants — safe on R2 only |
| GPIO45 | Strapping pin — VDD_SPI voltage |
| GPIO46 | Strapping pin |

---

## License

This hardware design is licensed under the [Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License](LICENSE.md) (CC BY-NC-SA 4.0).

You are free to share and adapt this design for non-commercial purposes, as long as you give appropriate credit and distribute any adaptations under the same license.
