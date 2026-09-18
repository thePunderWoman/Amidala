// xbee_transmit_frame.h
// Builds the XBee "Transmit Request" API frame body (frame type 0x10) --
// what Amidala (the coordinator) sends to reply to a Snips controller with
// a Snips::DownlinkPacket (issue #204 phase 2). The write-side counterpart
// of xbee_receive_packet.h. Deliberately free of any Arduino/SPI
// dependency, like the rest of the xbee_*.h parsers, so it's unit-testable
// natively. See src/xbee_spi.cpp for the SPI-driven transmit function that
// wraps this with the 0x7E/length header and trailing checksum
// (xbeeComputeChecksum(), xbee_frame_checksum.h).
//
// Frame body layout built here (frame-type byte through the end, NOT
// including the leading 0x7E/length header or trailing checksum -- i.e.
// the same "frame data" convention xbeeReadFrame()/
// xbeeParseReceivePacket() use):
//   frameType(1)=0x10 frameId(1) dest64(8) dest16(2) broadcastRadius(1)
//   options(1) payload(N)
#pragma once

#include <stdint.h>
#include <string.h>

namespace XBeeTransmitFrame {

constexpr uint8_t kFrameType = 0x10;

// Digi's "unknown 64-bit, route by 16-bit" addressing convention -- lets a
// reply be addressed purely by the sender's 16-bit network address
// (captured in XBeeReceivePacket::addr16, see xbee_receive_packet.h)
// without needing to know or store its full 64-bit address.
constexpr uint64_t kUnknownAddress64 = 0xFFFFFFFFFFFFFFFFULL;

constexpr uint16_t kHeaderLength = 14;  // type+frameId+dest64+dest16+radius+options

// frameId is fixed at 0 (no Transmit Status/0x8B response requested) --
// keeps downlink sends fire-and-forget; a send that's dropped is naturally
// retried by the next uplink cycle ~50ms later (see SnipsController.ino's
// kUplinkSendIntervalMs), so there's no need to track delivery.
// Returns the number of bytes written, or 0 if outCapacity is too small.
static inline uint16_t buildTransmitRequest(uint8_t *outFrameData, uint16_t outCapacity,
                                             const uint8_t *payload, uint16_t payloadLength,
                                             uint16_t dest16) {
  uint16_t total = (uint16_t)(kHeaderLength + payloadLength);
  if (outCapacity < total) return 0;

  outFrameData[0] = kFrameType;
  outFrameData[1] = 0;  // frameId
  for (int i = 0; i < 8; i++) {
    outFrameData[2 + i] = (uint8_t)(kUnknownAddress64 >> (8 * (7 - i)));
  }
  outFrameData[10] = (uint8_t)(dest16 >> 8);
  outFrameData[11] = (uint8_t)(dest16);
  outFrameData[12] = 0;  // broadcast radius (0 = max hops)
  outFrameData[13] = 0;  // options
  if (payloadLength > 0) {
    memcpy(outFrameData + kHeaderLength, payload, payloadLength);
  }
  return total;
}

}  // namespace XBeeTransmitFrame
