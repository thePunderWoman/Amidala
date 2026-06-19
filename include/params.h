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
//             drive_config.h   (DEFAULT_DOME_* / DOME_MAXIMUM_SPEED),
//             button_actions.h (HAPPY, EMOTE_MODERATE fallback constants),
//             <math.h>         (ceil)

#pragma once

#include "core.h"
#include "drive_config.h"
#include "button_actions.h"

// ---- Audio hardware selection -----------------------------------------------

#ifndef AUDIO_HW_HCR
#define AUDIO_HW_HCR    1  // Human Cyborg Relations board (default)
#define AUDIO_HW_VMUSIC 2  // VMusic2
#endif

// ---- Auxiliary string count -------------------------------------------------

#ifndef MAX_SERIAL_STRINGS
#define MAX_SERIAL_STRINGS 40
#endif

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

  SoundBank SB[20];
  Channel S[4];
  ButtonAction B[9];
  ButtonAction LB[9];
  ButtonAction AB[9];   // Alt-button layer (dispatched when altbtn is held)
  GestureAction G[10];
  DigitalOut D[4];
  SerialString Str[MAX_SERIAL_STRINGS];
  uint8_t serialcount;
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
  unsigned mindelay;
  unsigned maxdelay;
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

  // ---- WiFi access point -----------------------------------------------
  // wifion: enable the on-board WiFi soft-AP (default true).
  // wifiSSID: network name broadcast by the AP (max 32 chars).
  // wifiPassword: WPA2 passphrase (min 8, max 64 chars).
  bool    wifion;
  char    wifiSSID[33];
  char    wifiPassword[65];

  constexpr unsigned getSoundBankCount() {
    return sizeof(SB) / sizeof(SB[0]);
  }

  constexpr unsigned getServoCount() { return sizeof(S) / sizeof(S[0]); }

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
      wifion = true;
      strncpy(wifiSSID, "amidala", sizeof(wifiSSID));
      strncpy(wifiPassword, "Astromech", sizeof(wifiPassword));
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
