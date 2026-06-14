// globals.cpp — hardware-level globals.
//
// ServoDispatchDirect.h is included HERE and nowhere else.
// ServoDispatchPrivate.h (pulled in transitively) defines hardware ISR
// handlers that must live in exactly one translation unit.

#include "ServoDispatchDirect.h"
#include "ServoEasing.h"
#include "controller.h"

// Servo index assignments:
//   0 (SERVO1_PIN / GPIO3) — Drive Right PWM → Roboteq
//   1 (SERVO2_PIN / GPIO4) — Drive Left  PWM → Roboteq
//   2 (SERVO3_PIN / GPIO5) — Throttle / speed-control PWM → Roboteq script
//
//   Pin  Group ID,      Min,  Max
const ServoSettings servoSettings[] = {
    {SERVO1_PIN, 1000, 2000, 0},
    {SERVO2_PIN, 1000, 2000, 0},
    {SERVO3_PIN, 1000, 2000, 0}};

// The concrete instance lives here — the only TU that includes
// ServoDispatchDirect.h (and its ISR handlers via ServoDispatchPrivate.h).
static ServoDispatchDirect<SizeOfArray(servoSettings)> _servoImpl(servoSettings);

// Base-class reference exported to other TUs.  They declare it as
// `extern ServoDispatch& servoDispatch;` so they never need to include
// ServoDispatchDirect.h and trigger duplicate ISR-definition errors.
ServoDispatch& servoDispatch = _servoImpl;
