// params.h
// Runtime configuration struct for Amidala Firmware.
//
// AmidalaParameters holds all tunable settings (servo channels, sound banks,
// button actions, dome parameters, XBee addresses, …).  On first access
// init() zeroes the struct, applies compiled-in defaults, then overlays any
// values persisted in EEPROM.
//
// Depends on: EEPROM (Arduino <EEPROM.h> / arduino_mock.h),
//             button_actions.h (ButtonAction, GestureAction, SerialString),
//             core.h           (Gesture),
//             drive_config.h   (DEFAULT_DOME_* / DOME_MAXIMUM_SPEED, DOME_DRIVE),
//             button_actions.h (HAPPY, EMOTE_MODERATE fallback constants),
//             pin_assignment.h (PinRoleType, kMaxServoChannels)
//             <math.h>         (ceil)

#pragma once

#include "core.h"
#include "drive_config.h"
#include "button_actions.h"
#include "pin_assignment.h"
#include "serial_assignment.h"

// ---- Audio hardware selection -----------------------------------------------

#ifndef AUDIO_HW_HCR
#define AUDIO_HW_HCR    1  // Human Cyborg Relations board (default)
#define AUDIO_HW_VMUSIC 2  // VMusic2
#endif

// ---- Auxiliary string count -------------------------------------------------

// ---- Default pin-role mapping (issue #133) ----------------------------------

// Reproduces today's REAL (not just nominal) wiring, in kAssignablePins
// order {1,2,3,4,5,6,39,40,41,42,47}:
//   1,2       -> Analog (today's ANALOG1_PIN/ANALOG2_PIN)
//   3,4,5,6   -> Servo  (today's SERVO1-4_PIN)
//   39        -> Dout   (today's DOUT1_PIN)
//   40        -> Hall if RoboClaw dome drive is compiled in, else Dout.
//                GPIO40 is physically wired to the dome hall sensor;
//                DomeDriveRoboClaw::setup()'s pinMode(INPUT_PULLUP) for it
//                always runs after general pin setup and wins the "last
//                pinMode() call wins" race, so this pin has never actually
//                been a usable digital output on RoboClaw boards -- Hall
//                reflects that honestly instead of nominally calling it
//                Dout. See pin_assignment.h.
//   41,42     -> Dout   (today's DOUT3_PIN/DOUT4_PIN)
//   47        -> Ppm    (today's PPMIN_PIN)
//
// Pulled out of AmidalaParameters::init() into a standalone function so
// AmidalaConfig::validatePinAssignments() (config.cpp) can get at the
// defaults directly -- constructing a second AmidalaParameters and calling
// .init() on it would silently no-op (init()'s sInited/sRAMInited guards
// are function-local statics shared across every instance, not per-object),
// so it needs a default source that doesn't depend on the singleton dance.
inline void defaultPinRoles(PinRoleType out[11]) {
  out[0] = PinRoleType::kAnalog;
  out[1] = PinRoleType::kAnalog;
  out[2] = PinRoleType::kServo;
  out[3] = PinRoleType::kServo;
  out[4] = PinRoleType::kServo;
  out[5] = PinRoleType::kServo;
  out[6] = PinRoleType::kDout;
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
  out[7] = PinRoleType::kHall;
#else
  out[7] = PinRoleType::kDout;
#endif
  out[8]  = PinRoleType::kDout;
  out[9]  = PinRoleType::kDout;
  out[10] = PinRoleType::kPpm;
}

// Sweeps all 11 pins, reverting any whose role is no longer valid (out of
// the assignable pool, electrically invalid, or over a hardware ceiling --
// e.g. from config.txt lines parsing in file order without full-array
// context) back to its default. If `requireHall` is true and no pin ends
// up with role Hall (its sole occupant got reassigned away), forces
// kAssignablePins[7] (GPIO40) to Hall -- a RoboClaw dome drive needs
// exactly one hall input; DomeDriveRoboClaw::setup() would otherwise be
// handed kNoPin and call pinMode()/attachInterrupt() on it unconditionally,
// not a graceful "no sensor" no-op. validateRoleChange() only ever enforces
// a ceiling, never a floor, so this is the one role that needs a second,
// explicit check.
//
// Pure function (no I/O, no AmidalaController dependency) so it's natively
// testable -- callers that want to log what changed diff the array
// themselves before/after calling this (see
// AmidalaConfig::validatePinAssignments() in config.cpp).
inline void sanitizePinRoles(PinRoleType allRoles[11], bool requireHall) {
  PinRoleType defaults[11];
  defaultPinRoles(defaults);
  for (uint8_t i = 0; i < 11; i++) {
    PinRoleValidationResult r =
        validateRoleChange(kAssignablePins[i], allRoles[i], allRoles);
    if (!r.ok) allRoles[i] = defaults[i];
  }
  if (requireHall && countPinsWithRole(allRoles, PinRoleType::kHall) == 0) {
    allRoles[7] = PinRoleType::kHall;  // kAssignablePins[7] == GPIO40
  }
}

// ---- Default dome/drive serial port mapping (issue #147) --------------------
//
// Reproduces today's REAL wiring: RoboClaw dome defaults to Serial1 (its
// only-ever port today, ROBOCLAW_SERIAL); everything else that consumes a
// serial port (Sabertooth dome, Sabertooth/Roboteq-serial/Roboteq-PWM-serial
// drive) defaults to Serial2/AUX_SERIAL (today's DOME_DRIVE_SERIAL/
// DRIVE_SERIAL, both always AUX_SERIAL). Builds whose dome/drive doesn't
// consume a serial port at all (PWM) still get a deterministic default here
// -- it's simply inert, same as domercaddr etc. being harmless on non-
// RoboClaw builds. Pulled out of AmidalaParameters::init() for the same
// reason as defaultPinRoles() above -- AmidalaConfig::validateSerialPortAssignments()
// (config.cpp) needs a defaults source that doesn't depend on the init()
// singleton-guard dance.
inline void defaultSerialPorts(SerialPortId &domePort, SerialPortId &drivePort) {
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
  domePort = SerialPortId::kSerial1;
#else
  domePort = SerialPortId::kSerial2;  // Sabertooth dome (only other serial-consuming dome)
#endif
  drivePort = SerialPortId::kSerial2; // Sabertooth/Roboteq-serial/Roboteq-PWM-serial drive
}

// ---- AmidalaParameters ------------------------------------------------------

struct AmidalaParameters {
  unsigned getRadioChannelCount() {
    init();
    return rcchn;
  }

  struct SoundBank {
    char dir[9];
    uint8_t numfiles;
    uint8_t playindex;
    bool random;
  };

  struct Channel {
    uint8_t min; /* Min position you want the servo to reach. Always has to be
                    less than servo max. */
    uint8_t max; /* Max position you want the servo to reach. Always has to be
                    greater than servo min. */
    uint8_t n;   /* Center or Neutral position of the servo */
    uint8_t d; /* Number of degrees around center that will still register as
                  center */
    int16_t t; /* Logically shift the center position of the servo/joystick
                  either left or right */
    bool r;    /* Flip the direction of the servo */
    uint8_t s; /* How fast the servo will move or accelerate, 1 = Slowest,
                  100=Fastest */
    uint16_t minpulse; /* Optional value. Sets min pulse for servo channel on
                          startup */
    uint16_t maxpulse; /* Optional value. Sets max pulse for servo channel on
                          startup */
  };

  struct DigitalOut {
    bool state;
  };

  char serial[5];
  uint32_t xbr;      /* Right XBEE's unique serial number (lower address) */
  uint32_t xbl;      /* Left XBEE's unique serial number (lower address) */
  uint8_t rcchn;     /* How many channels does the RC radio have */
  uint16_t minpulse; /* Minimum pulse width for all servo outs (internal
                        default 1000) */
  uint16_t maxpulse; /* Maximum pulse width for all servo outs (internal
                        default 2000) */
  uint16_t rvrmin; /* Adjust Right Joystick Analog MIN "Reference Voltage" */
  uint16_t rvrmax; /* Adjust Right Joystick Analog MAX "Reference Voltage" */
  uint16_t rvlmin; /* Adjust Left Joystick Analog MIN "Reference Voltage" */
  uint16_t rvlmax; /* Adjust Left Joystick Analog MAX "Reference Voltage" */
  uint16_t fst; /* Adjust the failsafe timeout. You wouldn't normally adjust
                   this */
  uint8_t rcd;
  uint8_t rcj;

  // ---- Runtime-reassignable GPIO pin roles (issue #133) ----------------------
  // Each of the 11 pins already physically broken out on the board's headers
  // (see pin_assignment.h's kAssignablePins) gets assigned a role type
  // (DOUT/Analog/PPM/Servo/Hall) -- the count in each category is DERIVED
  // from however many pins currently have that type, e.g. trading a DOUT
  // pin for a 5th servo. pinRole[i] is the role for kAssignablePins[i].
  // Defaults reproduce today's real (not just nominal) wiring -- see
  // init() below and pin_assignment.h's header comment for the hall-sensor
  // special case. Reassigning any of these requires a reboot (they're baked
  // into pinMode()/constructor calls at boot) -- see controller.cpp setup().
  PinRoleType pinRole[11];

  SoundBank SB[20];
  Channel S[kMaxServoChannels];  // storage capacity, not live count -- see getServoCount()
  ButtonAction B[9];
  ButtonAction LB[9];
  ButtonAction AB[9];   // Alt-button layer (dispatched when altbtn is held)
  ButtonAction DB[9];   // Double-press layer
  GestureAction G[MAX_GESTURES];
  DigitalOut D[11];  // matches the assignable pool size -- no hardware ceiling on DOUT
                     // count below that, unlike Servo's LEDC-channel limit
  SerialString Str[MAX_SERIAL_STRINGS];
  uint8_t serialcount;
  uint16_t nextSstrId; // monotonic counter; set to max(all IDs)+1 after config load
  uint8_t gcount;
  uint8_t sbcount;
  uint8_t volume;
  uint8_t volumeChA;      // HCR channel A initial volume (0–100, default matches volume)
  uint8_t volumeChB;      // HCR channel B initial volume (0–100, default matches volume)
  // Which channel the volume wheel controls:
  //   0 = global (all three channels, current behaviour)
  //   1 = voice only (CH_V)
  //   2 = channel A only (CH_A)
  //   3 = channel B only (CH_B)
  uint8_t volumewheel;
  // Which channel alt+wheel controls (same enum as volumewheel).
  // 0 = fall through to volumewheel behaviour (no separate alt channel).
  uint8_t altvolumewheel;
  bool startup;
  bool rndon;
  bool ackon;
  char acktype;
  uint32_t mindelay;
  uint32_t maxdelay;
  bool mix12;
  uint8_t myi2c;
  uint32_t serialbaud;
  char serialinit[16];
  uint8_t serialdelim;
  uint8_t serialeol;
  bool autocorrect;
  char b9;
  bool goslow;
  uint8_t j1adjv;
  uint8_t j1adjh;
  uint8_t audiohw;     // AUDIO_HW_HCR (default) or AUDIO_HW_VMUSIC
  uint8_t startupem;   // Startup emote emotion (HAPPY=0..OVERLOAD=4)
  uint8_t startuplvl;  // Startup emote level (EMOTE_MODERATE=0, EMOTE_STRONG=1)
  uint8_t ackem;       // Ack emote emotion
  uint8_t acklvl;      // Ack emote level

  Gesture rnd;
  Gesture ackgest;
  Gesture slowgest;
  Gesture domegest;

  // ---- RoboClaw dome drive parameters ---------------------------------------
  // Only active when DOME_DRIVE == DOME_DRIVE_ROBOCLAW, but the fields are
  // present unconditionally so config parsing and EEPROM layout are the same
  // regardless of which dome drive is compiled in.
  uint8_t  domercaddr;    // RoboClaw packet-serial address (default 128 = 0x80)
  uint8_t  domercchan;    // Motor channel: 1 = M1, 2 = M2
  uint16_t domercqpps;    // Encoder pulses per second at maximum commanded speed
  uint16_t domefront;     // Degrees from hall sensor to dome "front" (0–359).
                          // e.g. if the radar eye is 88° past the hall trigger,
                          // set domefront=88 so dome=0 means "look forward".
  uint16_t domestall;     // Stall timeout in ms before obstruction is declared

  uint16_t domepos; // current dome position (used only in RDH_SERIAL builds)
  uint16_t domehome;
  uint8_t domemode;
  uint8_t domeseekmin;
  uint8_t domeseekmax;
  uint8_t domeseekr;
  uint8_t domeseekl;
  uint8_t domefudge;
  uint8_t domespeed;
  uint8_t domespeedhome;
  uint8_t domespeedseek;
  uint8_t domespeedmin;
  uint8_t domedecelzone;
  uint16_t domespmin; // only for analog
  uint16_t domespmax; // only for analog
  bool domech6;  // dome channel-6 mode flag (configurable but effect unimplemented)
  bool domeimu;  // dome IMU flag (configurable; read by getDomeIMU())
  bool domeflip;

  // ---- Alt-button modifier ---------------------------------------------------
  // altbtn: which button (1–9) acts as the modifier held to activate alt layer.
  //   0 = disabled (default).  Button numbering: drive stick triangle=1,
  //   circle=2, cross=3, square=4, l3=5; dome stick triangle=6, circle=7,
  //   cross=8, square=9.  (Dome l3 is reserved for gesture input.)
  // altdomestick: what happens to the dome stick while alt is held.
  //   0 = no change (default), 1 = abs-stick mode (RoboClaw only).
  uint8_t altbtn;
  uint8_t altdomestick;
  // mutebutton: which button (1–9, same numbering as altbtn) toggles HCR mute
  // when double-pressed.  0 = disabled (default).  May be the same button as
  // altbtn — a quick double-tap fires mute while a held press is the alt modifier.
  uint8_t mutebutton;
  // dbtimeout: window (ms) within which a second press counts as a double-press.
  // Only matters when a DB[] action is configured for a button.  0 disables
  // double-press detection entirely.  Default 300.
  uint16_t dbtimeout;

  // ---- Aux serial (Serial2/AUX_SERIAL header, GPIO21 TX / GPIO38 RX) ------
  // auxserial3: enable UART2 at startup even when nothing in this build
  // claims it via domeSerialPort/driveSerialPort below (default false).
  // Real hardware UART2, not software/bit-banged serial -- see AUX_SERIAL in
  // pin_config.h.
  bool    auxserial3;

  // ---- Reassignable dome/drive serial ports (issue #147) ------------------
  // Which physical UART (Serial1 or Serial2/AUX_SERIAL) the compiled-in
  // dome/drive subsystem's serial link uses. Only meaningful (and only shown
  // in the web UI) when the compiled DOME_DRIVE/DRIVE_SYSTEM actually needs
  // a serial port at all (RoboClaw or Sabertooth dome; Sabertooth or
  // Roboteq-serial/Roboteq-PWM-serial drive) -- inert otherwise, same as
  // domercaddr etc. on non-RoboClaw builds. Defaults reproduce today's real
  // wiring -- see defaultSerialPorts() above. Reassigning either requires a
  // reboot -- baked into a constructor call in AmidalaController::setup().
  SerialPortId domeSerialPort;
  SerialPortId driveSerialPort;

  // ---- Serial string metadata (favorites, hidden, categories) ---------------
  // Stored as separate config blocks; sstr= entries are unchanged.
  //   f=1,3,5          → sstr_favs / sstr_fav_cnt
  //   hidden=2,4       → sstr_hidden / sstr_hidden_cnt
  //   cat=Dome|1,3     → sstr_cats / sstr_cat_count (one line per category)
#define MAX_SSTR_FAVS        64
#define MAX_SSTR_HIDDEN      64
#define MAX_SSTR_CATS        16
#define MAX_SSTR_CAT_ENTRIES 64
  struct SstrCat {
    char     name[24];
    uint16_t idx[MAX_SSTR_CAT_ENTRIES]; // 1-based sstr indices
    uint8_t  cnt;
  };
  uint16_t sstr_favs[MAX_SSTR_FAVS];
  uint8_t  sstr_fav_cnt;
  uint16_t sstr_hidden[MAX_SSTR_HIDDEN];
  uint8_t  sstr_hidden_cnt;
  SstrCat  sstr_cats[MAX_SSTR_CATS];
  uint8_t  sstr_cat_count;

  // ---- E-stop / Resume broadcast commands ----------------------------------
  // Serial strings sent to the WCB whenever an emergency-stop or resume is
  // triggered from the web UI.  Allows per-build customisation of which child
  // devices receive stop/restart signals.
  // Config keys: estopstr=<cmd>  resumestr=<cmd>  (one entry per line)
#define MAX_SAFETY_CMDS 16
  struct SafetyCmd { char str[48]; };
  SafetyCmd EstopCmds[MAX_SAFETY_CMDS];
  SafetyCmd ResumeCmds[MAX_SAFETY_CMDS];
  uint8_t   estopCmdCount;
  uint8_t   resumeCmdCount;

  // ---- Bluetooth controller --------------------------------------------------
  // btcontrolleron: enable the BLE HID gamepad at runtime (default false —
  // opt-in since it requires the user to pair a device).
  // btaddr: MAC address of a paired BLE HID gamepad (AA:BB:CC:DD:EE:FF format).
  // Empty string means "connect to any BLE HID device found during scan."
  bool btcontrolleron;
  char btaddr[18];

  // ---- WCB Client (ESP-NOW mesh) --------------------------------------------
  // wcbenable: join the WCB mesh network at runtime (default false — opt-in,
  // requires the identity fields below to be fully configured).
  // wcboct2/wcboct3: 2nd/3rd octet of the shared WCB MAC addressing scheme.
  // Stored as a plain byte, but parsed/displayed as 2-digit hex (config.txt,
  // web UI, JSON API) to match the WCB configuration wizard's own convention
  // -- same treatment as xbr/xbl below.
  // wcbpassword: mesh password (max 39 chars, per the WCB_Client library's
  // own constructor contract).
  // wcbquantity: total number of WCBs in the mesh.
  // wcbid: this device's ID, 1-19, or 20 for the special/out-of-band slot.
  // outboundserial: where outbound serial-string/HCR commands go —
  // 0 = uart0 (wired, default), 1 = wcb (mesh-only; the UART0 write is
  // skipped). Only takes effect when wcbenable is also on and the mesh is
  // actually joined — otherwise outbound always falls back to uart0.
  bool    wcbenable;
  uint8_t wcboct2;
  uint8_t wcboct3;
  char    wcbpassword[40];
  uint8_t wcbquantity;
  uint8_t wcbid;
  uint8_t outboundserial;

  // ---- WiFi access point -----------------------------------------------
  // wifion: enable the on-board WiFi soft-AP (default true).
  // wifiSSID: network name broadcast by the AP (max 32 chars).
  // wifiPassword: WPA2 passphrase (min 8, max 64 chars).
  // wifichannel: 2.4GHz channel 1-13 (default 1). AP and STA share a single
  // radio channel on the ESP32, so this is also the channel WCB Client's
  // ESP-NOW mesh rides when it's enabled -- must match whatever channel the
  // rest of the WCB mesh is actually on (default 1, or whatever's set via
  // ?WCBCH on the WCBs themselves) or this board won't hear it. Passed to
  // WCB_Client::setMeshChannel() (WCBClientController::begin()), which only
  // accepts 1-11 -- 12/13 are valid WiFi channels but not valid mesh
  // channels, so using either while WCB Client is enabled logs a mismatch
  // warning and the mesh won't be reachable. Only change this if you know
  // what you're doing.
  bool    wifion;
  char    wifiSSID[33];
  char    wifiPassword[65];
  uint8_t wifichannel;

  constexpr unsigned getSoundBankCount() {
    return sizeof(SB) / sizeof(SB[0]);
  }

  // Live count of servo-typed pins (0-kMaxServoChannels), NOT S[]'s fixed
  // storage capacity -- issue #133, servo count is now derived from
  // pinRole[] rather than compiled in. No longer constexpr: depends on
  // runtime config, not just the type.
  unsigned getServoCount() const { return countPinsWithRole(pinRole, PinRoleType::kServo); }

  constexpr unsigned getButtonCount() { return sizeof(B) / sizeof(B[0]); }

  constexpr unsigned getGestureCount() { return sizeof(G) / sizeof(G[0]); }

  constexpr unsigned getSerialStringCount() { return sizeof(Str) / sizeof(Str[0]); }

  void init(bool forceReload = false) {
    static bool sInited;
    static bool sRAMInited;
    if (sInited && !forceReload)
      return;
    if (!sRAMInited) {
      memset(this, '\0', sizeof(*this));
      volume = 50;
      volumeChA = 50;
      volumeChB = 50;
      volumewheel = 0;
      altvolumewheel = 0;
      startup = true;
      rndon = true;
      rnd.setGesture("3");
      ackon = false;
      ackgest.setGesture("252");
      //                acktype = "ads";
      mindelay = 60;
      maxdelay = 120;
      mix12 = false;
      rcd = 30;
      rcj = 5;
      // Default pin roles (issue #133) -- see defaultPinRoles() above.
      defaultPinRoles(pinRole);
      // Default dome/drive serial ports (issue #147) -- see
      // defaultSerialPorts() above.
      defaultSerialPorts(domeSerialPort, driveSerialPort);
      myi2c = 0;
      serialbaud = 9600;
      serialdelim = ':';
      serialeol = 13;
      autocorrect = false;
      b9 = 'n';
#ifdef VMUSIC_SERIAL
      audiohw = AUDIO_HW_VMUSIC;
#else
      audiohw = AUDIO_HW_HCR;
#endif
      startupem = HAPPY;
      startuplvl = EMOTE_MODERATE;
      ackem = HAPPY;
      acklvl = EMOTE_MODERATE;
      slowgest.setGesture("858");
      goslow = false;
      j1adjv = 0;
      j1adjh = 0;
      domercaddr = DEFAULT_DOME_ROBOCLAW_ADDRESS;
      domercchan = DEFAULT_DOME_ROBOCLAW_CHANNEL;
      domercqpps = DEFAULT_DOME_ROBOCLAW_QPPS;
      domefront = 0;
      domestall = DEFAULT_DOME_STALL_TIMEOUT_MS;
      domehome = DEFAULT_DOME_HOME_POSITION;
      domepos = domehome;
      domemode = 0; // Force manual mode to stop auto-spinning on startup
      domeseekmin = DEFAULT_DOME_SEEK_MIN_DELAY;
      domeseekmax = DEFAULT_DOME_SEEK_MAX_DELAY;
      domeseekl = DEFAULT_DOME_SEEK_LEFT;
      domeseekr = DEFAULT_DOME_SEEK_RIGHT;
      domefudge = DEFAULT_DOME_FUDGE;
      domespeed = DOME_MAXIMUM_SPEED;
      domespeedhome = DEFAULT_DOME_SPEED_HOME;
      domespeedseek = DEFAULT_DOME_SPEED_SEEK;
      domespeedmin  = DEFAULT_DOME_SPEED_MIN;
      domedecelzone = DEFAULT_DOME_DECEL_ZONE;
      domespmin = 42;
      domespmax = 935;
      domech6 = false;
      domeflip = DEFAULT_DOME_INVERTED;
      domeimu = true;
      altbtn = 0;
      altdomestick = 0;
      mutebutton = 0;
      dbtimeout = 300;
      auxserial3 = false;
      btcontrolleron = false;
      wcbenable = false;
      wcboct2 = 0;
      wcboct3 = 0;
      wcbpassword[0] = '\0';
      wcbquantity = 0;
      wcbid = 0;
      outboundserial = 0;
      wifion = true;
      strncpy(wifiSSID, "amidala", sizeof(wifiSSID));
      strncpy(wifiPassword, "Astromech", sizeof(wifiPassword));
      wifichannel = 1;
      minpulse = DEFAULT_DOME_MIN_PULSE;
      maxpulse = DEFAULT_DOME_MAX_PULSE;
      sRAMInited = true;
    }
    size_t offs = 0;
    if (EEPROM.read(offs) == 'D' && EEPROM.read(offs + 1) == 'B' &&
        EEPROM.read(offs + 2) == '0' && EEPROM.read(offs + 3) == '1' &&
        EEPROM.read(offs + 4) == 0) {
      offs += 5;
      for (unsigned i = 0; i < sizeof(serial); i++, offs++)
        serial[i] = EEPROM.read(offs);
      // Ensure last character is zero
      if (serial[sizeof(serial) - 1] != 0)
        serial[sizeof(serial) - 1] = 0;
    }
    offs = 0x64;
    if (EEPROM.read(offs) == 'S' && EEPROM.read(offs + 1) == 'C' &&
        EEPROM.read(offs + 2) == '2' && EEPROM.read(offs + 3) == '3' &&
        EEPROM.read(offs + 4) == 0) {
      offs += 5;
      xbr = ((uint32_t)EEPROM.read(offs + 3) << 24) |
            ((uint32_t)EEPROM.read(offs + 2) << 16) |
            ((uint32_t)EEPROM.read(offs + 1) << 8) |
            ((uint32_t)EEPROM.read(offs + 0) << 0);
      offs += sizeof(uint32_t);

      xbl = ((uint32_t)EEPROM.read(offs + 3) << 24) |
            ((uint32_t)EEPROM.read(offs + 2) << 16) |
            ((uint32_t)EEPROM.read(offs + 1) << 8) |
            ((uint32_t)EEPROM.read(offs + 0) << 0);
      offs += sizeof(uint32_t);

      rcchn = EEPROM.read(offs++);

      unsigned unknown;
      // ?
      unknown = EEPROM.read(offs++);
      // CONSOLE_SERIAL.println("?: "+String(unknown));

      minpulse = ((uint16_t)EEPROM.read(offs + 1) << 8) |
                 ((uint16_t)EEPROM.read(offs + 0) << 0);
      offs += sizeof(uint16_t);

      maxpulse = ((uint16_t)EEPROM.read(offs + 1) << 8) |
                 ((uint16_t)EEPROM.read(offs + 0) << 0);
      offs += sizeof(uint16_t);

      // unknown  6?
      unknown = EEPROM.read(offs++);
      // unknown  1?
      unknown = EEPROM.read(offs++);
      // unknown  7?
      unknown = EEPROM.read(offs++);
      // unknown  2?
      unknown = EEPROM.read(offs++);
      // unknown  0?
      unknown = EEPROM.read(offs++);

      rvrmin = ((uint16_t)EEPROM.read(offs + 1) << 8) |
               ((uint16_t)EEPROM.read(offs + 0) << 0);
      offs += sizeof(uint16_t);

      rvlmin = ((uint16_t)EEPROM.read(offs + 1) << 8) |
               ((uint16_t)EEPROM.read(offs + 0) << 0);
      offs += sizeof(uint16_t);

      rvrmax = ((uint32_t)EEPROM.read(offs + 1) << 8) |
               ((uint16_t)EEPROM.read(offs + 0) << 0);
      offs += sizeof(uint16_t);

      rvlmax = ((uint16_t)EEPROM.read(offs + 1) << 8) |
               ((uint16_t)EEPROM.read(offs + 0) << 0);
      offs += sizeof(uint16_t);

      fst = ((uint16_t)EEPROM.read(offs + 1) << 8) |
            ((uint16_t)EEPROM.read(offs + 0) << 0);
      offs += sizeof(uint16_t);

      offs = 0xea;
      for (unsigned i = 0; i < sizeof(S) / sizeof(S[0]); i++) {
        S[i].min = EEPROM.read(offs++);
        S[i].max = EEPROM.read(offs++);
        S[i].n = EEPROM.read(offs++);
        S[i].d = EEPROM.read(offs++);
        S[i].t = (int16_t)((uint16_t)EEPROM.read(offs + 1) << 8) |
                 ((uint16_t)EEPROM.read(offs + 0) << 0);
        offs += sizeof(uint16_t);
        S[i].r = EEPROM.read(offs++);
        S[i].s = (uint8_t)ceil((float)EEPROM.read(offs++) / 255.0f * 10) * 10;
        S[i].minpulse = ((uint16_t)EEPROM.read(offs + 1) << 8) |
                        ((uint16_t)EEPROM.read(offs + 0) << 0);
        offs += sizeof(uint16_t);
        S[i].maxpulse = ((uint16_t)EEPROM.read(offs + 1) << 8) |
                        ((uint16_t)EEPROM.read(offs + 0) << 0);
        offs += sizeof(uint16_t);
      }
      // disable unused variable warning
      (void)unknown;
    }
    sInited = true;
  }
};
