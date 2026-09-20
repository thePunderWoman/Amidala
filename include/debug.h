// debug.h — user-editable debug switches.
//
// Uncomment any of these before building to enable the corresponding
// debug output over CONSOLE_SERIAL.  Must be included before ReelTwo.h
// (i.e. before controller.h) so the macros are defined in time.

#pragma once

// #define USE_DEBUG
// #define USE_DOME_SENSOR_SERIAL_DEBUG
// #define USE_POCKET_REMOTE_DEBUG
// #define USE_PPM_DEBUG
// #define USE_MOTOR_DEBUG
// #define USE_DOME_DEBUG
// #define USE_VOLUME_WHEEL_DEBUG
// #define USE_SERVO_DEBUG
// #define USE_VERBOSE_SERVO_DEBUG

// Uncomment to print a line to serial every time the hall sensor fires during
// normal operation (homing, calibration, running).  Works independently of
// USE_DEBUG.
// #define USE_HALL_DEBUG

// Uncomment to enter hall-sensor test mode at boot.
// Loads config.txt (so it knows which pin you assigned Hall Sensor to on the
// Pins page; if none is assigned it falls back to DOME_HALL_PIN), then skips
// the rest of normal init and loops forever printing HIGH/LOW transitions on
// that pin. Reflash without the define to return to normal operation.
// #define HALL_SENSOR_TEST
