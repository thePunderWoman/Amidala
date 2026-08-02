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

  // config.txt key dispatch (issue #171): processConfig() used to be one
  // large if/else-if chain matching every key, which grew large enough on
  // the ESP32-S3/Xtensa toolchain to trip a link-time "dangerous relocation:
  // windowed longcall crosses 1GB boundary" error -- a hard limit on a
  // single function's compiled body size, not a logic bug (compiler-flag
  // workarounds were tried and ruled out; only the function's own size
  // matters). A prior split of dome/RoboClaw keys into their own
  // processDomeConfig() sub-dispatch bought headroom once, but was always a
  // stopgap -- nothing stopped the same error from resurfacing once either
  // function grew again.
  //
  // The real fix: one handler function per key (or per key-variant, e.g.
  // domepos='s two comma-count overloads), referenced from kConfigHandlers
  // below. processConfig() is now a fixed-size loop over that table -- its
  // compiled size no longer grows with the number of config keys, so this
  // class of link error can't recur no matter how many settings get added
  // later. See src/config.cpp for the handler definitions and the table.
  using ConfigHandler = bool (AmidalaConfig::*)(const char *cmd);
  static const ConfigHandler kConfigHandlers[];

  bool cfg_sb(const char *cmd);
  bool cfg_s(const char *cmd);
  bool cfg_b(const char *cmd);
  bool cfg_lb(const char *cmd);
  bool cfg_ab(const char *cmd);
  bool cfg_db(const char *cmd);
  bool cfg_sstr(const char *cmd);
  bool cfg_fav(const char *cmd);
  bool cfg_hidden(const char *cmd);
  bool cfg_cat(const char *cmd);
  bool cfg_estopstr(const char *cmd);
  bool cfg_resumestr(const char *cmd);
  bool cfg_gesture(const char *cmd);
  bool cfg_audiohw(const char *cmd);

  // ---- Simple scalar settings (single key -> single params field) --------
  bool cfg_acktype(const char *cmd);
  bool cfg_b9(const char *cmd);
  bool cfg_volume(const char *cmd);
  bool cfg_volumeChA(const char *cmd);
  bool cfg_volumeChB(const char *cmd);
  bool cfg_volumewheel(const char *cmd);
  bool cfg_altvolumewheel(const char *cmd);
  bool cfg_startupem(const char *cmd);
  bool cfg_startuplvl(const char *cmd);
  bool cfg_ackem(const char *cmd);
  bool cfg_acklvl(const char *cmd);
  bool cfg_mindelay(const char *cmd);
  bool cfg_maxdelay(const char *cmd);
  bool cfg_rvrmin(const char *cmd);
  bool cfg_rvrmax(const char *cmd);
  bool cfg_rvlmin(const char *cmd);
  bool cfg_rvlmax(const char *cmd);
  bool cfg_minpulse(const char *cmd);
  bool cfg_maxpulse(const char *cmd);
  bool cfg_rcchn(const char *cmd);
  bool cfg_rcd(const char *cmd);
  bool cfg_rcj(const char *cmd);
  bool cfg_myi2c(const char *cmd);
  bool cfg_serialbaud(const char *cmd);
  bool cfg_serialdelim(const char *cmd);
  bool cfg_serialeol(const char *cmd);
  bool cfg_fst(const char *cmd);
  bool cfg_j1adjv(const char *cmd);
  bool cfg_j1adjh(const char *cmd);
  bool cfg_rnd(const char *cmd);
  bool cfg_ackgest(const char *cmd);
  bool cfg_slowgest(const char *cmd);
  bool cfg_domegest(const char *cmd);
  bool cfg_startup(const char *cmd);
  bool cfg_rndon(const char *cmd);
  bool cfg_ackon(const char *cmd);
  bool cfg_mix12(const char *cmd);
  bool cfg_autocorrect(const char *cmd);
  bool cfg_goslow(const char *cmd);
  bool cfg_domech6(const char *cmd);

  bool cfg_xbr(const char *cmd);
  bool cfg_xbl(const char *cmd);

  bool cfg_altbtn(const char *cmd);
  bool cfg_altdomestick(const char *cmd);
  bool cfg_mutebutton(const char *cmd);
  bool cfg_dbtimeout(const char *cmd);
  bool cfg_gesturetimeout(const char *cmd);
  bool cfg_auxserial3(const char *cmd);
  bool cfg_btcontrolleron(const char *cmd);
  bool cfg_btaddr(const char *cmd);
  bool cfg_wcbenable(const char *cmd);
  bool cfg_wcboct2(const char *cmd);
  bool cfg_wcboct3(const char *cmd);
  bool cfg_wcbpassword(const char *cmd);
  bool cfg_wcbquantity(const char *cmd);
  bool cfg_wcbid(const char *cmd);
  bool cfg_outboundserial(const char *cmd);
  bool cfg_wifion(const char *cmd);
  bool cfg_wifissid(const char *cmd);
  bool cfg_wifipassword(const char *cmd);
  bool cfg_wifichannel(const char *cmd);

  // ---- Reassignable GPIO pin roles (issue #133) ---------------------------
  bool cfg_pin1role(const char *cmd);
  bool cfg_pin2role(const char *cmd);
  bool cfg_pin3role(const char *cmd);
  bool cfg_pin4role(const char *cmd);
  bool cfg_pin5role(const char *cmd);
  bool cfg_pin6role(const char *cmd);
  bool cfg_pin39role(const char *cmd);
  bool cfg_pin40role(const char *cmd);
  bool cfg_pin41role(const char *cmd);
  bool cfg_pin42role(const char *cmd);
  bool cfg_pin47role(const char *cmd);

  bool cfg_domeserialport(const char *cmd);
  bool cfg_driveserialport(const char *cmd);

  bool cfg_reboot(const char *cmd);

  // ---- Dome/RoboClaw settings (formerly processDomeConfig()) -------------
  bool cfg_domeimu(const char *cmd);
  bool cfg_domeflip(const char *cmd);
  bool cfg_domespeed(const char *cmd);
  bool cfg_domepos(const char *cmd);
  bool cfg_domepos2(const char *cmd);
  bool cfg_domerpos(const char *cmd);
  bool cfg_domerpos2(const char *cmd);
  bool cfg_domehome(const char *cmd);
  bool cfg_domemode(const char *cmd);
  bool cfg_domeseekr(const char *cmd);
  bool cfg_domeseekl(const char *cmd);
  bool cfg_domefudge(const char *cmd);
  bool cfg_domespeedhome(const char *cmd);
  bool cfg_domespeedseek(const char *cmd);
  bool cfg_domespeedmin(const char *cmd);
  bool cfg_domedecelzone(const char *cmd);
  bool cfg_domercaddr(const char *cmd);
  bool cfg_domercchan(const char *cmd);
  bool cfg_domercqpps(const char *cmd);
  bool cfg_domefront(const char *cmd);
  bool cfg_domestall(const char *cmd);
  bool cfg_domeerrlog(const char *cmd);
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
