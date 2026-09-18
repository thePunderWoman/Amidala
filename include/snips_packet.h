// snips_packet.h
// Amidala's own encode/decode for the Snips Controllers wire protocol -- the
// application-level payload carried inside an XBee Transmit Request (0x10) /
// Receive Packet (0x90), see xbee_receive_packet.h for the outer framing.
//
// This is a fresh wire format defined by the SnipsControllers firmware
// rewrite (thePunderWoman/SnipsControllers, see include/packet.h and
// src/packet.cpp there), not dictated by any pre-existing spec. Field
// widths/order/endianness here are written to match that implementation
// exactly (verified against its actual encode/decode bodies, not just its
// header) so the two sides agree on the wire -- see
// thePunderWoman/Amidala#204.
//
// Deliberately free of any Arduino/SPI dependency, like
// xbee_frame_checksum.h/xbee_io_sample.h, so it's unit-testable natively.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace Snips {

// bq25185 charge status, mirrors SnipsControllers/include/battery.h's
// ChargeState -- Amidala only needs to carry the value through, not
// interpret it (that's the controller's own OLED to render).
enum class ChargeState : uint8_t {
  kDone = 0,
  kCharging = 1,
  kRecoverableFault = 2,
  kLatchedFault = 3,
};

// Controller -> Amidala, sent periodically. Raw per-button state (bit i =
// SnipsButtons::Index i, see snips_buttons.h) plus calibrated analog and
// battery/charge status.
struct UplinkPacket {
  static constexpr uint8_t kFlagShuttingDown = 0x01;

  uint16_t buttonMask = 0;
  uint8_t triggerPercent = 0;   // 0-100
  int8_t stickXPercent = 0;     // -100..100
  int8_t stickYPercent = 0;     // -100..100
  uint8_t batteryPercent = 0;   // 0-100
  ChargeState chargeState = ChargeState::kDone;
  uint8_t flags = 0;            // bit0 = kFlagShuttingDown
};

// Amidala -> controller. Handedness is sent once at connect and is static
// for the session; the Left/Right slot label+value are generic -- Amidala
// assigns their meaning (volume, throttle, or anything else) and echoes
// back whatever the current label/value should read. Not yet sent anywhere
// in Phase 1 (no downlink transmit wiring exists yet) -- encode/decode are
// implemented now anyway since they're cheap and keep the packet layer
// self-contained for that follow-up.
struct DownlinkPacket {
  enum class Handedness : uint8_t { kUnknown = 0, kLeft = 1, kRight = 2 };

  static constexpr size_t kFieldLength = 8;

  Handedness handedness = Handedness::kUnknown;
  char leftLabel[kFieldLength + 1] = {};
  char leftValue[kFieldLength + 1] = {};
  char rightLabel[kFieldLength + 1] = {};
  char rightValue[kFieldLength + 1] = {};
};

constexpr size_t kUplinkEncodedSize = 8;
constexpr size_t kDownlinkEncodedSize = 1 + 4 * DownlinkPacket::kFieldLength;  // 33

// Returns the number of bytes written (always kUplinkEncodedSize), or 0 if
// outCapacity is too small.
size_t encodeUplink(const UplinkPacket &packet, uint8_t *outBuf, size_t outCapacity);

// Returns false if length is too short to hold a full uplink packet.
bool decodeUplink(const uint8_t *buf, size_t length, UplinkPacket *out);

// Returns the number of bytes written (always kDownlinkEncodedSize), or 0 if
// outCapacity is too small.
size_t encodeDownlink(const DownlinkPacket &packet, uint8_t *outBuf, size_t outCapacity);

// Returns false if length is too short to hold a full downlink packet.
bool decodeDownlink(const uint8_t *buf, size_t length, DownlinkPacket *out);

}  // namespace Snips
