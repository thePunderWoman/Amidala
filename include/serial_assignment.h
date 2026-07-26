// serial_assignment.h
// Pure validation logic for runtime dome/drive serial port reassignment
// (issue #147).
//
// Model: the two reassignable hardware UARTs on this board -- Serial1
// (GPIO17/18) and Serial2/AUX_SERIAL (GPIO21/38) -- form a 2-member pool.
// Each of the (at most two) serial-consuming subsystems in a given build --
// the dome drive (RoboClaw or Sabertooth) and the drive system (Sabertooth
// or Roboteq-serial/Roboteq-PWM-serial) -- picks one port from that pool.
// The only rule: two active consumers can't both claim the same port, since
// each port is a single physical UART and only one protocol can own it.
//
// Serial0 (the WCB/body-controller "main serial out" path) is fixed and out
// of scope -- it already has its own live destination toggle (see params.h's
// outboundserial). I2C and true software serial are also out of scope: the
// ESP32-S3 has exactly 3 hardware UART peripherals in silicon, and all 3
// are already spoken for (Serial0/Serial1/Serial2), so a genuinely
// independent 4th concurrent serial port would require bit-banged software
// serial -- a different, bigger feature than reassigning the two ports that
// already exist.
//
// Deliberately free of any Arduino/params dependency so it's unit-testable
// natively (same pattern as pin_assignment.h/safety_stop_latch.h/
// bt_scan_policy.h/dome_position_math.h).

#pragma once

#include <stdint.h>
#include <string.h>

enum class SerialPortId : uint8_t { kSerial1, kSerial2, kCount };

// Which subsystem is asking -- used only to know which "other" consumer's
// current port to check against when validating a change.
enum class SerialConsumer : uint8_t { kDome, kDrive };

struct SerialPortValidationResult {
    bool ok;
    const char *reason;  // static string literal; nullptr when ok
};

// String encoding for port ids -- the config.txt/web JSON representation
// (e.g. "domeserialport=serial2"). Shared by config parsing/persistence
// (config.cpp, config_file.h) and the web API (web_api.h) so the scheme
// lives in exactly one place.
inline const char *serialPortToString(SerialPortId p) {
    return (p == SerialPortId::kSerial2) ? "serial2" : "serial1";
}

// Parses a serialPortToString()-encoded string back to a SerialPortId.
// Returns false (leaving *outPort untouched) if `s` doesn't match either
// known port.
inline bool serialPortFromString(const char *s, SerialPortId *outPort) {
    if (strcmp(s, "serial1") == 0) { *outPort = SerialPortId::kSerial1; return true; }
    if (strcmp(s, "serial2") == 0) { *outPort = SerialPortId::kSerial2; return true; }
    return false;
}

// Full check for assigning `requested` to `consumer`. `otherConsumerActive`
// is false when the build's other subsystem (dome vs. drive) doesn't
// consume a serial port at all (e.g. a PWM-only drive) -- in that case
// there's nothing to conflict with and any port is valid. When the other
// consumer IS active, `otherConsumerPort` is its current port; re-selecting
// a consumer's own current port is always valid regardless of what the
// other consumer holds, since this function is never told the requesting
// consumer's own current port -- callers simply don't call it unless the
// value is actually changing.
inline SerialPortValidationResult validateSerialPortChange(
        SerialConsumer consumer, SerialPortId requested,
        bool otherConsumerActive, SerialPortId otherConsumerPort) {
    (void)consumer;
    if (otherConsumerActive && requested == otherConsumerPort) {
        return {false, "port already used by the other serial subsystem"};
    }
    return {true, nullptr};
}
