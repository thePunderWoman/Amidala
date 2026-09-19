// xbee_at_session.cpp
// See include/xbee_at_session.h for the design rationale.

#ifndef UNIT_TEST
#include "ReelTwo.h"   // must precede xbee_spi.h/xbee_remote.h — provides Arduino type definitions
#endif
#include "xbee_at_session.h"
#include "xbee_at_command.h"
#include "xbee_spi.h"
#include <string.h>

bool XBeeATSession::startQuery(const char command[2]) {
  if (isBusy()) return false;
  fIsSet = false;
  sendCommand(command, nullptr, 0, kAwaitingValue);
  return true;
}

bool XBeeATSession::startSetAndPersist(const char command[2], const uint8_t *value,
                                        uint8_t valueLength) {
  if (isBusy()) return false;
  fIsSet = true;
  sendCommand(command, value, valueLength, kAwaitingValue);
  return true;
}

void XBeeATSession::checkTimeout(uint32_t now) {
  if (!isBusy()) return;
  if (now - fSentAt > kTimeoutMs) {
    fState = kTimedOut;
  }
}

void XBeeATSession::handleResponse(const uint8_t *buf, uint16_t length) {
  if (!isBusy()) return;

  XBeeATCommand::Response resp;
  if (!XBeeATCommand::parseResponse(buf, length, &resp)) return;
  if (resp.frameId != fFrameId) return;  // not the response we're waiting for

  if (resp.status != XBeeATCommand::kOk) {
    fStatus = resp.status;
    fState = kFailed;
    return;
  }

  if (fState == kAwaitingValue && fIsSet) {
    // Set succeeded -- persist it before reporting done, so a value that
    // "saved" in the UI actually survives a power cycle.
    sendCommand("WR", nullptr, 0, kAwaitingWrite);
    return;
  }

  // Either a plain query completing, or the WR follow-up completing.
  fStatus = resp.status;
  fValueLength = resp.valueLength < kMaxValueLength ? resp.valueLength : kMaxValueLength;
  memcpy(fValue, resp.value, fValueLength);
  fState = kDone;
}

uint8_t XBeeATSession::nextFrameId() {
  fFrameId++;
  if (fFrameId == 0) fFrameId = 1;  // 0 is reserved for "nothing pending"
  return fFrameId;
}

void XBeeATSession::sendCommand(const char command[2], const uint8_t *value,
                                 uint8_t valueLength, State awaiting) {
  nextFrameId();  // advances and stores the new pending fFrameId
  xbeeSPISendATCommand(fFrameId, command, value, valueLength);
  fState = awaiting;
  fSentAt = millis();
}
