// pin_config.h
// Hardware pin assignments and serial port mappings for Amidala Firmware.
// All physical I/O pin numbers and serial port aliases are defined here so
// that wiring changes require edits in exactly one place.
//
// Target: ESP32-S3 WROOM1 N16R8 — Amidala custom PCB v1.1.
//
// Note: drive-system serial symbols (DOME_DRIVE_SERIAL, RDH_SERIAL) come from
// drive_config.h and must be resolved before this header is included.

#pragma once

// ---- Servo output pins (LEDC PWM via ServoDispatchESP32) --------------------
// 3 active channels: right drive, left drive, throttle. SERVO4 defined but unassigned.

#define SERVO1_PIN  3   // Drive Right
#define SERVO2_PIN  4   // Drive Left
#define SERVO3_PIN  5   // Throttle / speed-control
#define SERVO4_PIN  6

// ---- Digital output pins ----------------------------------------------------
// 4 channels on this PCB (v1.1).
// DRIVE_ACTIVE / DOME_ACTIVE are not wired; setDigitalPin() treats a 0 entry
// as absent.

#define DOUT1_PIN   39
#define DOUT2_PIN   40
#define DOUT3_PIN   41
#define DOUT4_PIN   42

// ---- Hall-effect sensor (RoboClaw dome drive) --------------------------------
// Wired to the Digital 2 header (GPIO40).  All ESP32-S3 GPIOs support
// external interrupts, so any digital pin would work here.

#define DOME_HALL_PIN  40

// ---- PPM RC input -----------------------------------------------------------

#define PPMIN_PIN   47

// ---- Analog inputs (ADC1, WiFi-safe) ----------------------------------------

#define ANALOG1_PIN  1   // ADC1_0
#define ANALOG2_PIN  2   // ADC1_1

// ---- SD card chip-select pin ------------------------------------------------

#define SD_CS_PIN   10

// ---- Mode select pin --------------------------------------------------------
// SEL2 A: available on GPIO21 unless AUX serial is enabled.
//   Define ESP32_AUX_SERIAL to use GPIO21/38 as UART2; SEL2 is then unavailable.

#ifndef ESP32_AUX_SERIAL
#define SEL2_PIN    21   // SEL2 A; SEL2 B (GPIO38) reserved for future 2-bit expansion
#endif

// ---- SPI bus pins -----------------------------------------------------------

#define SPI_MOSI_PIN      11
#define SPI_MISO_PIN      13
#define SPI_SCK_PIN       12
#define XBEE_CS_PIN        7
#define XBEE_ATTN_PIN     16
#define XBEE_SLEEP_PIN    15
#define SPI_SPARE_CS_PIN  14

// ---- I2C bus pins -----------------------------------------------------------

#define I2C_SDA_PIN  8
#define I2C_SCL_PIN  9

// ---- AUX serial (UART2 on SW-UART header) -----------------------------------
// Mutually exclusive with SEL2 A/B.  Enable with -DESP32_AUX_SERIAL.

#ifdef ESP32_AUX_SERIAL
#define AUX_SERIAL         Serial2
#define AUX_SERIAL_TX_PIN  21
#define AUX_SERIAL_RX_PIN  38
#endif

// ---- Serial port assignments ------------------------------------------------
// Serial  = USB-CDC console  (ARDUINO_USB_CDC_ON_BOOT=1)
// Serial0 = UART0 GPIO43/44  WCB / body controller out  (was Serial3 on Mega)
// Serial1 = UART1 GPIO17/18  RoboClaw dome drive         (see drive_config.h)
//
// Always pass explicit pin args to Serial0/Serial1.begin(): on ESP32-S3 the
// GPIO matrix routes UARTs to whatever pins you specify, and board-variant
// defaults may not match the Amidala PCB wiring.

#define SERIAL0_RX_PIN  44   // UART0 RX — WCB / body controller out
#define SERIAL0_TX_PIN  43   // UART0 TX

#define CONSOLE_SERIAL  Serial   // USB-CDC

// SERIAL (downstream WCB output) — ESP32 Arduino.h defines SERIAL as 0x0
// (a UART index constant).  Undefine it so we can use SERIAL as a Stream alias.
#if !defined(DOME_DRIVE_SERIAL) && !defined(RDH_SERIAL)
#undef  SERIAL
#define SERIAL  Serial0          // UART0, GPIO43/44 — WCB serial out
#endif
