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

// Amidala -> controller, sent after every processed uplink (see
// xbee_spi.cpp). Carries what the controller's status screen shows about
// the droid -- built by SnipsDownlink::build() (snips_downlink.h). A
// percentage of kUnknown means "nothing to report", shown as "--" (or, for
// droid battery, left off the screen).
//
// Wire format (23 bytes): version, handedness, volume, throttle, flags,
// droid battery, sequence id, sequence progress, then the sequence name as
// a fixed 15-byte, zero-padded field. The controller rejects any downlink
// whose first byte isn't kVersion; kVersion is deliberately not 0-2 so the
// previous 33-byte format (which started with a handedness byte) never
// matches it.
struct DownlinkPacket {
  enum class Handedness : uint8_t { kUnknown = 0, kLeft = 1, kRight = 2 };

  static constexpr uint8_t kVersion = 0x10;
  static constexpr uint8_t kUnknown = 0xFF;

  static constexpr uint8_t kFlagEstop = 0x01;
  static constexpr uint8_t kFlagMotorsDisabled = 0x02;
  static constexpr uint8_t kFlagMuse = 0x04;
  static constexpr uint8_t kFlagAutoDome = 0x08;

  static constexpr size_t kSequenceNameLength = 15;

  Handedness handedness = Handedness::kUnknown;
  uint8_t volumePercent = kUnknown;
  uint8_t throttlePercent = kUnknown;  // the drive speed cap, not live speed
  uint8_t flags = 0;
  uint8_t droidBatteryPercent = kUnknown;
  // Incremented each time a sequence starts (wrapping at 255), so the
  // controller can tell the same sequence started twice from one that's
  // still being reported.
  uint8_t sequenceId = 0;
  uint8_t sequenceProgressPercent = kUnknown;
  char sequenceName[kSequenceNameLength + 1] = {};  // empty = none
};

constexpr size_t kUplinkEncodedSize = 8;
constexpr size_t kDownlinkEncodedSize = 8 + DownlinkPacket::kSequenceNameLength;  // 23

// Returns the number of bytes written (always kUplinkEncodedSize), or 0 if
// outCapacity is too small.
size_t encodeUplink(const UplinkPacket &packet, uint8_t *outBuf, size_t outCapacity);

// Returns false if length is too short to hold a full uplink packet.
bool decodeUplink(const uint8_t *buf, size_t length, UplinkPacket *out);

// Returns the number of bytes written (always kDownlinkEncodedSize), or 0 if
// outCapacity is too small.
size_t encodeDownlink(const DownlinkPacket &packet, uint8_t *outBuf, size_t outCapacity);

// Returns false if length is too short to hold a full downlink packet, or
// its version byte isn't DownlinkPacket::kVersion.
bool decodeDownlink(const uint8_t *buf, size_t length, DownlinkPacket *out);

}  // namespace Snips
