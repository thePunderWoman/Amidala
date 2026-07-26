// globals.cpp — hardware-level globals.
//
// ServoDispatchDirect.h is included HERE and nowhere else.
// ServoDispatchPrivate.h (pulled in transitively) defines hardware ISR
// handlers that must live in exactly one translation unit.

#include "ServoDispatchDirect.h"
#include "ServoEasing.h"
#include "controller.h"

// Servo index assignments (issue #133): each of the 11 assignable pool pins
// can be Servo-typed, giving a LIVE count of 0-8 channels (see
// pin_assignment.h's kMaxServoChannels -- the ESP32-S3 LEDC peripheral's
// hardware ceiling). All 8 LEDC channels are always compiled in here; these
// are just placeholders before config.txt loads -- AmidalaController::setup()
// re-applies each channel's actual pin (or disables it, pin=0, for channels
// at/beyond the live servo count) from params.pinRole once config is loaded,
// same double-init pattern used throughout setup().
//   0-3 (SERVO1-4_PIN / GPIO3,4,5,6) — this board's 4 physical servo headers.
//   4-7 — pin 0 (disabled) by default; only active if the user trades a
//     DOUT/Analog/PPM/Hall pin for an additional servo.
//
//   Pin  Group ID,      Min,  Max
const ServoSettings servoSettings[] = {
    {SERVO1_PIN, 1000, 2000, 0},
    {SERVO2_PIN, 1000, 2000, 0},
    {SERVO3_PIN, 1000, 2000, 0},
    {SERVO4_PIN, 1000, 2000, 0},
    {0, 1000, 2000, 0},
    {0, 1000, 2000, 0},
    {0, 1000, 2000, 0},
    {0, 1000, 2000, 0}};

// The concrete instance lives here — the only TU that includes
// ServoDispatchDirect.h (and its ISR handlers via ServoDispatchPrivate.h).
static ServoDispatchDirect<SizeOfArray(servoSettings)> _servoImpl(servoSettings);

// Base-class reference exported to other TUs.  They declare it as
// `extern ServoDispatch& servoDispatch;` so they never need to include
// ServoDispatchDirect.h and trigger duplicate ISR-definition errors.
ServoDispatch& servoDispatch = _servoImpl;
