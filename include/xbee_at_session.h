// xbee_at_session.h
// Tracks one local AT-command round-trip to the XBee module at a time (a
// query, or a set-then-persist), correlating the eventual 0x88 response
// back to the request that caused it via frame ID (see xbee_at_command.h).
//
// Only one action is ever in flight at once -- the same simplifying
// assumption the gesture-capture feature (issue #138) already makes for
// its own shared session state -- so this is a single owned instance
// (AmidalaController::fXBeeAT), not something constructed per-request.
// Web handlers in src/wifi_ap.cpp start an action and poll this for
// completion; src/xbee_spi.cpp feeds it every 0x88 frame it sees
// (regardless of controllertype -- see xbee_spi.cpp's xbeeSPIPumpFrames())
// and ticks its timeout check once per animate() cycle.
//
// Body is in src/xbee_at_session.cpp, not inline here: it calls
// xbeeSPISendATCommand() (src/xbee_spi.cpp), which pulls in SPI.h --
// keeping that out of this header lets other Arduino-light headers that
// might someday need XBeeATSession::State avoid it.
#pragma once

#include <stdint.h>

class XBeeATSession {
public:
  static constexpr uint8_t kMaxValueLength = 8;  // widest AT parameter used here (ID, 64-bit)

  enum State {
    kIdle,           // nothing in flight
    kAwaitingValue,  // sent the query/set command, waiting for its 0x88
    kAwaitingWrite,  // set succeeded, sent WR, waiting for its 0x88
    kDone,           // completed successfully (query: value() holds the result)
    kFailed,         // a 0x88 came back with a non-OK status
    kTimedOut,       // no 0x88 arrived within the timeout window
  };

  // Starts a read-only query (no parameter bytes). Returns false (does
  // nothing) if an action is already in flight.
  bool startQuery(const char command[2]);

  // Starts a set: sends `command`=`value`, and -- only if that succeeds
  // (status 0x00) -- follows up with WR to persist it across a power
  // cycle. A failed set never sends WR. Returns false if already busy.
  bool startSetAndPersist(const char command[2], const uint8_t *value, uint8_t valueLength);

  // Call once per animate() tick: moves kAwaitingValue/kAwaitingWrite to
  // kTimedOut if too long has passed since the last frame was sent.
  void checkTimeout(uint32_t now);

  // Unconditionally abandons whatever's in flight and returns to kIdle,
  // regardless of how much time has actually passed -- for a caller with
  // its own, shorter deadline than checkTimeout()'s kTimeoutMs (e.g. the
  // boot-time coordinator-role check, which gives up after ~1s so a
  // non-responding XBee doesn't leave the session looking busy to the web
  // UI for the rest of kTimeoutMs's window into normal operation).
  void abort() { fState = kIdle; }

  // Call for every 0x88 frame read off the SPI link, regardless of
  // controllertype -- a no-op if nothing is pending or the frame ID
  // doesn't match the current request.
  void handleResponse(const uint8_t *buf, uint16_t length);

  State state() const { return fState; }
  bool isBusy() const { return fState == kAwaitingValue || fState == kAwaitingWrite; }
  uint8_t status() const { return fStatus; }
  const uint8_t *value() const { return fValue; }
  uint8_t valueLength() const { return fValueLength; }

private:
  static constexpr uint32_t kTimeoutMs = 2000;

  uint8_t nextFrameId();
  void sendCommand(const char command[2], const uint8_t *value, uint8_t valueLength, State awaiting);

  State fState = kIdle;
  uint8_t fFrameId = 0;   // 0 reserved for "no request outstanding"
  bool fIsSet = false;    // true if the in-flight action needs a WR follow-up on success
  uint32_t fSentAt = 0;

  uint8_t fStatus = 0;
  uint8_t fValue[kMaxValueLength] = {0};
  uint8_t fValueLength = 0;
};
