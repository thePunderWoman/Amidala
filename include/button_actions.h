// button_actions.h
// Data types for button and gesture action mappings in Amidala Firmware.
//
// ButtonAction  — what happens when a button is pressed or a gesture fires.
// GestureAction — a (Gesture, ButtonAction) pair stored in config.
// SerialString  — a short string sent to the primary serial port (SERIAL / Serial0).
//
// Depends on: Gesture (core.h), Print (Arduino / arduino_mock.h)
// In embedded builds hcr.h is included before this header and supplies the
// HCR constants. The #ifndef guards below provide fallbacks for native tests.

#pragma once

#include "core.h"

// ---- HCR emotion / level constants (fallbacks for non-embedded builds) ------

#ifndef HAPPY
#define HAPPY    0
#define SAD      1
#define MAD      2
#define SCARED   3
#define OVERLOAD 4
#endif

#ifndef EMOTE_MODERATE
#define EMOTE_MODERATE 0
#define EMOTE_STRONG   1
#endif

// ---- ButtonAction -----------------------------------------------------------

struct ButtonAction {
  enum {
    kNone      = 0,
    kSound     = 1,
    kServo     = 2,
    kDigitalOut = 3,
    kI2CCmd    = 4,
    kSerialStr = 5,
    kI2CStr    = 6,
    kHCREmote  = 7,  // Trigger an HCR emotion (emotion, level)
    kHCRMuse   = 8,  // Toggle HCR musing on/off
    kDomeCmd   = 9   // Dome drive command (subcmd, arg)
  };

  // Sub-commands for kDomeCmd.  Stored in dome.subcmd.
  enum DomeCmdType {
    kDomeRand      = 0,  // Toggle random wander mode
    kDomeStop      = 1,  // Emergency stop
    kDomeFront     = 2,  // Go to front (0°)
    kDomeHome      = 3,  // Re-run homing sweep
    kDomeCalibrate = 4,  // Run 10-revolution calibration
    kDomeGotoAbs   = 5,  // Go to absolute angle (arg = degrees, 0–255)
    kDomeRelPos    = 6,  // Relative move +N degrees (arg = N)
    kDomeRelNeg    = 7,  // Relative move -N degrees (arg = N)
    kDomeAbsStick  = 8   // Toggle absolute-stick mode (joystick angle = dome heading)
  };

  uint8_t action;
  union {
    struct {
      uint8_t soundbank;
      uint8_t sound;
    } sound;
    struct {
      uint8_t num;
      uint8_t pos;
    } servo;
    struct {
      uint8_t num;
      uint8_t state;
    } dout;
    struct {
      uint8_t target;
      uint8_t cmd;
    } i2ccmd;
    struct {
      uint8_t unused1;
      uint8_t unused2;
    } serial;
    struct {
      uint8_t target;   // I2C address; string looked up by serialid
    } i2cstr;
    struct {
      uint8_t emotion;   // HAPPY=0, SAD=1, MAD=2, SCARED=3, OVERLOAD=4
      uint8_t level;     // EMOTE_MODERATE=0, EMOTE_STRONG=1
    } emote;
    struct {
      uint8_t subcmd;    // DomeCmdType
      uint8_t arg;       // Angle (0–255°) for kDomeGotoAbs; delta for kDomeRelPos/kDomeRelNeg
    } dome;
  };
  // Stable ID of the serial string to send.  0 = none.
  // For kSerialStr: the string to send (primary ref).
  // For kI2CStr: the string to send via I2C to i2cstr.target.
  // For all other actions: optional side-effect string fired alongside the action.
  uint16_t serialid;

  void printDescription(Print *stream) {
    switch (action) {
    case kNone:
      break;
    case kSound:
      stream->print(F("Sound Bank #"));
      stream->print(sound.soundbank);
      if (sound.sound != 0) {
        stream->print(F(","));
        stream->print(sound.sound);
      }
      break;
    case kServo:
      stream->print(F("Servo #"));
      stream->print(servo.num);
      stream->print(F(", On Pos="));
      stream->print(servo.pos);
      break;
    case kDigitalOut:
      stream->print(F("DOut #"));
      stream->print(dout.num);
      stream->print(F(", Type="));
      switch (dout.state) {
      case 0:
        stream->print(F("NO"));
        break;
      case 1:
        stream->print(F("NC"));
        break;
      case 2:
        stream->print(F("MON"));
        break;
      }
      break;
    case kI2CCmd:
      stream->print(F("i2c Dest #"));
      stream->print(i2ccmd.target);
      stream->print(F(", Cmd="));
      stream->print(i2ccmd.cmd);
      break;
    case kSerialStr:
      stream->print(F("Serial Str #"));
      stream->print(serialid);
      break;
    case kI2CStr:
      stream->print(F("Aux I2C Str #"));
      stream->print(serialid);
      stream->print(F(", Dest "));
      stream->print(i2cstr.target);
      break;
    case kHCREmote:
      stream->print(F("HCR "));
      switch (emote.emotion) {
        case HAPPY:    stream->print(F("Happy"));    break;
        case SAD:      stream->print(F("Sad"));      break;
        case MAD:      stream->print(F("Mad"));      break;
        case SCARED:   stream->print(F("Scared"));   break;
        case OVERLOAD: stream->print(F("Overload")); break;
        default:       stream->print(emote.emotion); break;
      }
      if (emote.emotion != OVERLOAD)
        stream->print(emote.level ? F(" Strong") : F(" Moderate"));
      break;
    case kHCRMuse:
      stream->print(F("HCR Muse Toggle"));
      break;
    case kDomeCmd:
      stream->print(F("Dome: "));
      switch (dome.subcmd) {
        case kDomeRand:      stream->print(F("Random Mode"));            break;
        case kDomeStop:      stream->print(F("Stop"));                   break;
        case kDomeFront:     stream->print(F("Front"));                  break;
        case kDomeHome:      stream->print(F("Home"));                   break;
        case kDomeCalibrate: stream->print(F("Calibrate"));              break;
        case kDomeGotoAbs:   stream->print(F("Goto "));
                             stream->print(dome.arg);
                             stream->print(F("\xb0")); break;
        case kDomeRelPos:    stream->print(F("Relative +"));
                             stream->print(dome.arg);
                             stream->print(F("\xb0")); break;
        case kDomeRelNeg:    stream->print(F("Relative -"));
                             stream->print(dome.arg);
                             stream->print(F("\xb0")); break;
        case kDomeAbsStick:  stream->print(F("Absolute-Stick Toggle"));   break;
        default:             stream->print(dome.subcmd);                 break;
      }
      break;
    }
    if (action != kSerialStr && action != kI2CStr && serialid != 0) {
      stream->print(F(", Serial #"));
      stream->print(serialid);
    }
    stream->println();
  }
};

// ---- GestureAction ----------------------------------------------------------

struct GestureAction {
  Gesture gesture;
  ButtonAction action;

  void printDescription(Print *stream) {
    char buf[MAX_GESTURE_LENGTH + 1];
    if (!gesture.isEmpty()) {
      stream->print(gesture.getGestureString(buf));
      stream->print(F(": "));
      action.printDescription(stream);
    }
  }
};

// ---- SerialString -----------------------------------------------------------

struct SerialString {
  uint16_t id;    // stable unique ID (assigned at creation; never changes)
  char name[32];  // human-readable label (e.g. "Leia Sequence")
  char str[100];  // serial string to send (e.g. ":LD00")
};
