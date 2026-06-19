// core.h
// Pure-logic classes and helper functions for Amidala Firmware.
// Included by controller.h (where Arduino types are already available)
// and by unit tests (where test/arduino_mock.h provides the necessary stubs).

#pragma once

#include <stdint.h>
#include <string.h>
#include <ctype.h>

#ifndef MAX_GESTURE_LENGTH
#define MAX_GESTURE_LENGTH 8
#endif

#ifndef MAX_SERIAL_STRINGS
#define MAX_SERIAL_STRINGS 500
#endif

#ifndef MAX_GESTURES
#define MAX_GESTURES 100
#endif

// Portable min/max/constrain for non-Arduino builds (e.g. native unit tests).
// Arduino.h already defines these for embedded builds; the guards prevent
// redefinition conflicts.
#ifndef ARDUINO
#ifndef constrain
#define constrain(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#endif
#ifndef _AMIDALA_MINMAX_DEFINED
#define _AMIDALA_MINMAX_DEFINED
template <typename T> static inline T min(T a, T b) { return a < b ? a : b; }
template <typename T> static inline T max(T a, T b) { return a > b ? a : b; }
#endif
#endif // !ARDUINO

////////////////////////////////

class Gesture {
public:
  Gesture(const char *gestureStr = nullptr) {
    fGesture = 0;
    if (gestureStr != nullptr)
      setGesture(gestureStr);
  }

  bool isEmpty() { return fGesture == 0; }

  uint8_t getGestureType(char ch) {
    switch (ch) {
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      return ch - '0';
    case 'A':
      return 10;
    case 'B':
      return 11;
    case 'C':
      return 12;
    case 'D':
      return 13;
    default:
      return 0;
    }
  }

  bool matches(const char *str) {
    if (fGesture == 0 && *str == '\0')
      return true;
    if (fGesture == 0)
      return false;
    const char *s = str;
    unsigned cnt = 0;
    uint32_t quotient = fGesture;
    while (quotient != 0 && cnt < MAX_GESTURE_LENGTH) {
      uint8_t val[] = {'\0', '1', '2', '3', '4', '5', '6',
                       '7',  '8', '9', 'A', 'B', 'C', 'D'};
      uint32_t remainder = quotient % 14;
      if (*s++ != val[remainder])
        return false;
      quotient = quotient / 14;
    }
    return (*s == '\0');
  }

  char *getGestureString(char *str) {
    char *s = str;
    unsigned cnt = 0;
    uint32_t quotient = fGesture;
    while (quotient != 0 && cnt < MAX_GESTURE_LENGTH) {
      uint8_t val[] = {'\0', '1', '2', '3', '4', '5', '6',
                       '7',  '8', '9', 'A', 'B', 'C', 'D'};
      uint32_t remainder = quotient % 14;
      *s++ = val[remainder];
      *s = '\0';
      quotient = quotient / 14;
    }
    return str;
  }

  void setGesture(const char *str) {
    fGesture = 0;
    if (str == nullptr)
      return;
    const char *strend = str + strlen(str);
    while (str != strend) {
      // Base 14
      fGesture = fGesture * 14 + getGestureType(*--strend);
    }
  }

private:
  uint32_t fGesture;
};

////////////////////////////////

class ServoPD {
public:
  ServoPD(long Kp, long Kd, long zero, long range, long scalebits = 8)
      : fKp(Kp), fKd(Kd), fPos(zero << scalebits),
        fPrevTarget(zero << scalebits), fZero(zero << scalebits),
        fRange(range << scalebits), fScaleBits(scalebits) {}

  long get() const { return (fPos >> fScaleBits); }

  void update(long targetpos) {
    targetpos <<= fScaleBits;
    long diff = fKp * targetpos + fKd * (targetpos - fPrevTarget);
    fPos += (diff >> 16);
    fPos = constrain(fPos, fZero - fRange, fZero + fRange);
    fPrevTarget = targetpos;
  }

  void reset(long targetpos) {
    targetpos <<= fScaleBits;
    fPos = constrain(fPos, fZero - fRange, fZero + fRange);
    fPrevTarget = targetpos;
  }

  long rawget() { return fPos >> fScaleBits; }

  void rawset(long rawval) {
    fPos = rawval << fScaleBits;
  }

private:
  long fKp, fKd, fPos, fPrevTarget, fZero, fRange, fScaleBits;
};

////////////////////////////////
// Helper functions (used by processConfig / processCommand)
////////////////////////////////

inline bool startswith(const char *&cmd, const char *str) {
  size_t len = strlen(str);
  if (strncmp(cmd, str, strlen(str)) == 0) {
    cmd += len;
    return true;
  }
  return false;
}

inline int32_t strtol(const char *cmd, const char **endptr) {
  bool sign = false;
  int32_t result = 0;
  if (*cmd == '-') {
    cmd++;
    sign = true;
  }
  while (isdigit(*cmd)) {
    result = result * 10L + (*cmd - '0');
    cmd++;
  }
  *endptr = cmd;
  return (sign) ? -result : result;
}

inline uint32_t strtolu(const char *cmd, const char **endptr) {
  uint32_t result = 0;
  while (isdigit(*cmd)) {
    result = result * 10L + (*cmd - '0');
    cmd++;
  }
  *endptr = cmd;
  return result;
}

inline bool boolparam(const char *cmd, const char *match, bool &value) {
  if (startswith(cmd, match)) {
    if (strcmp(cmd, "y") == 0) {
      value = true;
      return true;
    } else if (strcmp(cmd, "n") == 0) {
      value = false;
      return true;
    }
  }
  return false;
}

inline bool charparam(const char *cmd, const char *match, const char *oneof,
               char &value) {
  if (startswith(cmd, match) && cmd[1] == '\0') {
    while (*oneof != '\0') {
      if (*oneof++ == cmd[0]) {
        value = cmd[0];
        return true;
      }
    }
  }
  return false;
}

inline bool sintparam(const char *cmd, const char *match, int32_t &value) {
  if (startswith(cmd, match)) {
    int32_t val = strtol(cmd, &cmd);
    if (*cmd == '\0') {
      value = val;
      return true;
    }
  }
  return false;
}

inline bool sintparam2(const char *cmd, const char *match, int32_t &value1,
                int32_t &value2) {
  if (startswith(cmd, match)) {
    int32_t val = strtol(cmd, &cmd);
    if (*cmd == ',') {
      value1 = val;
      val = strtol(cmd + 1, &cmd);
      if (*cmd == '\0') {
        value2 = val;
        return true;
      }
    }
  }
  return false;
}

inline bool intparam(const char *cmd, const char *match, uint32_t &value,
              uint32_t minval, uint32_t maxval) {
  if (startswith(cmd, match)) {
    uint32_t val = strtolu(cmd, &cmd);
    if (*cmd == '\0') {
      value = min(max(val, minval), maxval);
      return true;
    }
  }
  return false;
}

inline bool intparam(const char *cmd, const char *match, uint16_t &value,
              uint16_t minval, uint16_t maxval) {
  if (startswith(cmd, match)) {
    uint16_t val = (uint16_t)strtolu(cmd, &cmd);
    if (*cmd == '\0') {
      value = min(max(val, minval), maxval);
      return true;
    }
  }
  return false;
}

inline bool intparam(const char *cmd, const char *match, uint8_t &value,
              uint8_t minval, uint8_t maxval) {
  if (startswith(cmd, match)) {
    uint8_t val = (uint8_t)strtolu(cmd, &cmd);
    if (*cmd == '\0') {
      value = min(max(val, minval), maxval);
      return true;
    }
  }
  return false;
}

inline bool gestureparam(const char *cmd, const char *match, Gesture &gesture) {
  if (startswith(cmd, match)) {
    gesture.setGesture(cmd);
    return true;
  }
  return false;
}

inline bool numberparams(const char *cmd, uint8_t &argcount, int *args,
                  uint8_t maxcount) {
  for (argcount = 0; argcount < maxcount; argcount++) {
    args[argcount] = strtol(cmd, &cmd);
    if (*cmd == '\0') {
      argcount++;
      return true;
    } else if (*cmd != ',') {
      return false;
    }
    cmd++;
  }
  return true;
}

inline bool numberparams(const char *cmd, uint8_t &argcount, uint8_t *args,
                  uint8_t maxcount) {
  for (argcount = 0; argcount < maxcount; argcount++) {
    args[argcount] = (uint8_t)strtolu(cmd, &cmd);
    if (*cmd == '\0') {
      argcount++;
      return true;
    } else if (*cmd != ',') {
      return false;
    }
    cmd++;
  }
  return true;
}
