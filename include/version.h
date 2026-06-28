// version.h
// Firmware identity and communication-baud constants.
//
// Included by controller.h so every .cpp that pulls in the controller
// header automatically gets these definitions.  FIRMWARE_NAME, VERSION_NUM,
// BUILD_NUM, and BUILD_DATE are used by AmidalaConsole; RDH_BAUD_RATE is used
// by AmidalaController::setup() and the .ino entry point.

#pragma once

// Plain C string literals — usable in web_api.h and anywhere F() can't be used
#define FIRMWARE_VERSION   "1.0.1"
#define BOARD_REV          "1.1"
#define MCU_VARIANT        "ESP32-S3 N16R8"

// Flash-string versions for Arduino serial output (saves RAM)
#define FIRMWARE_NAME   F("Amidala")
#define VERSION_NUM     F(FIRMWARE_VERSION)
#define BUILD_NUM       F("1")
#define BUILD_DATE      F(__DATE__)

#define RDH_BAUD_RATE   9600
