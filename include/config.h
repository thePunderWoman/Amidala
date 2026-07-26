// config.h
// Configuration layer: AmidalaConfig class + readConfig() template functions.
//
// AmidalaConfig manages parsing, displaying, and persisting the robot's
// configuration. Method bodies are in src/config.cpp, except for
// applyServoConfig() which lives in src/servo.cpp so all servoDispatch
// access stays in one file.
//
// readConfig() reads config.txt from either a VMusic2 USB board (when
// VMUSIC_SERIAL is defined) or an SD card, feeding each character into any
// object that provides process(char, bool).  It must remain header-only
// because it is a template function.
//
// Depends on: AmidalaController (via forward declaration only)

#pragma once

#include <stdint.h>
#include "pin_assignment.h"
#include "serial_assignment.h"

class AmidalaController;
class Print;

// ---- AmidalaConfig ----------------------------------------------------------

class AmidalaConfig {
public:
  AmidalaConfig() {}
  virtual ~AmidalaConfig() {}

  // Called from AmidalaController::setup() to bind the controller and its
  // console output.
  void init(AmidalaController *controller);

  // Parse a single *key=value config command.  Returns true on success.
  bool processConfig(const char *cmd);

  void showLoadEEPROM(bool load = false);
  void showCurrentConfiguration();
  void writeCurrentConfiguration();

  // Implemented in src/servo.cpp — bridges into servoDispatch so that
  // config.cpp has no direct dependency on the servo global.
  void applyServoConfig(unsigned num, uint16_t minpulse, uint16_t maxpulse,
                        float neutral);

  // Re-applies all DomePosition tuning params (fudge, speeds, seek range)
  // to the active dome drive.  Called whenever any of those config values
  // change so live updates via `*domefudge=...` take effect immediately.
  void applyDomePositionParams();

  // Called once from AmidalaController::setup(), right after
  // ensureConfigDefaults(), to guarantee params' 11 pin roles are mutually
  // consistent (hardware ceilings not exceeded, Analog only on ADC1 pins)
  // before any pinMode()/constructor call uses them. config.txt lines parse
  // in file order, so a line's own conflict check only sees whatever's been
  // parsed so far -- this final sweep re-checks the whole pinRole[] array
  // with full context and resets any pin whose role is still invalid back
  // to its compiled-in default (see params.h's defaultPinRoles()), logging
  // a warning.
  void validatePinAssignments();

  // Called once from AmidalaController::setup(), right after
  // validatePinAssignments(), to guarantee domeSerialPort/driveSerialPort
  // don't both claim the same physical port when both subsystems are active
  // in this build (same "config.txt parses in file order" reasoning as
  // validatePinAssignments() above -- each line's own conflict check only
  // sees whatever's been parsed so far). Resets dome's port back to its
  // default on conflict and logs a warning.
  void validateSerialPortAssignments();

private:
  AmidalaController *fController = nullptr;
  Print *fOutput = nullptr;
};

// ---- readConfig() -----------------------------------------------------------

#ifdef VMUSIC_SERIAL
// ============================================================
// VMusic2 path
// ============================================================

#include <audio/VMusic.h>

/// Bridges any Console's process(char, bool) method to the VMusic::Parser
/// interface so parseTextFile() can feed characters into the config pipeline.
template <typename Console>
class VMusicConfigParser : public VMusic::Parser {
public:
    explicit VMusicConfigParser(Console& console) : fConsole(console) {}

    virtual void process(char ch) override {
        fConsole.process(ch, true);
    }

private:
    Console& fConsole;
};

/// Read config.txt from the USB drive attached to the VMusic2 board.
/// The VMusic object must already be initialised before calling.
/// Returns true if the file was found and fully processed.
template <typename Console>
bool readConfig(VMusic& vm, Console& console) {
    VMusicConfigParser<Console> parser(console);
    return vm.parseTextFile(parser, "config.txt");
}

#else
// ============================================================
// SD card path
// ============================================================

#ifndef UNIT_TEST
#  include "SD.h"
#  include "SPI.h"
#endif

/// Read config.txt from the SD card.
/// Returns true if the SD initialised, the file opened, and all characters
/// were processed.
template <typename Console>
bool readConfig(Console& console) {
    bool sdReady = false;
    for (int attempt = 1; attempt <= 5 && !sdReady; attempt++) {
        if (attempt > 1) {
            Serial.printf("[SD] init attempt %d...\n", attempt);
            delay(500);
        }
        sdReady = SD.begin(SD_CS_PIN, SPI);
    }
    if (!sdReady) {
        Serial.println("initialization failed!");
        return false;
    }
    Serial.println("initialization done.");
    File conf = SD.open("/config.txt");
    if (conf) {
        while (conf.available()) {
            char ch = conf.read();
            console.process(ch, true);
        }
        conf.close();
        return true;
    }
    Serial.println("error opening config.txt");
    return false;
}

#endif  // VMUSIC_SERIAL
