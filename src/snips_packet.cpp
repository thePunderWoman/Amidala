#include "snips_packet.h"

namespace Snips {

namespace {

// Copies a null-terminated string into a fixed-width field, truncating if
// too long and zero-padding if shorter (so the decode side's strings come
// back clean, terminated at the first zero either way).
void writeField(const char *text, uint8_t *outField, size_t fieldLength) {
  size_t i = 0;
  for (; i < fieldLength && text[i] != '\0'; i++) {
    outField[i] = static_cast<uint8_t>(text[i]);
  }
  for (; i < fieldLength; i++) {
    outField[i] = 0;
  }
}

// Reads a fixed-width field back into a null-terminated buffer (which must
// be at least fieldLength + 1 bytes).
void readField(const uint8_t *field, size_t fieldLength, char *outText) {
  for (size_t i = 0; i < fieldLength; i++) {
    outText[i] = static_cast<char>(field[i]);
  }
  outText[fieldLength] = '\0';
}

}  // namespace

size_t encodeUplink(const UplinkPacket &packet, uint8_t *outBuf, size_t outCapacity) {
  if (outCapacity < kUplinkEncodedSize) {
    return 0;
  }
  outBuf[0] = static_cast<uint8_t>(packet.buttonMask >> 8);
  outBuf[1] = static_cast<uint8_t>(packet.buttonMask & 0xFF);
  outBuf[2] = packet.triggerPercent;
  outBuf[3] = static_cast<uint8_t>(packet.stickXPercent);
  outBuf[4] = static_cast<uint8_t>(packet.stickYPercent);
  outBuf[5] = packet.batteryPercent;
  outBuf[6] = static_cast<uint8_t>(packet.chargeState);
  outBuf[7] = packet.flags;
  return kUplinkEncodedSize;
}

bool decodeUplink(const uint8_t *buf, size_t length, UplinkPacket *out) {
  if (length < kUplinkEncodedSize) {
    return false;
  }
  out->buttonMask = (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
  out->triggerPercent = buf[2];
  out->stickXPercent = static_cast<int8_t>(buf[3]);
  out->stickYPercent = static_cast<int8_t>(buf[4]);
  out->batteryPercent = buf[5];
  out->chargeState = static_cast<ChargeState>(buf[6]);
  out->flags = buf[7];
  return true;
}

size_t encodeDownlink(const DownlinkPacket &packet, uint8_t *outBuf, size_t outCapacity) {
  if (outCapacity < kDownlinkEncodedSize) {
    return 0;
  }
  constexpr size_t kF = DownlinkPacket::kFieldLength;
  outBuf[0] = static_cast<uint8_t>(packet.handedness);
  writeField(packet.leftLabel, outBuf + 1, kF);
  writeField(packet.leftValue, outBuf + 1 + kF, kF);
  writeField(packet.rightLabel, outBuf + 1 + 2 * kF, kF);
  writeField(packet.rightValue, outBuf + 1 + 3 * kF, kF);
  return kDownlinkEncodedSize;
}

bool decodeDownlink(const uint8_t *buf, size_t length, DownlinkPacket *out) {
  if (length < kDownlinkEncodedSize) {
    return false;
  }
  constexpr size_t kF = DownlinkPacket::kFieldLength;
  out->handedness = static_cast<DownlinkPacket::Handedness>(buf[0]);
  readField(buf + 1, kF, out->leftLabel);
  readField(buf + 1 + kF, kF, out->leftValue);
  readField(buf + 1 + 2 * kF, kF, out->rightLabel);
  readField(buf + 1 + 3 * kF, kF, out->rightValue);
  return true;
}

}  // namespace Snips
