// xbee_receive_packet.h
// Parsing for the XBee "Receive Packet" API frame body (frame type 0x90,
// ZigBee) -- what carries a Snips controller's custom UplinkPacket payload
// (see snips_packet.h), as distinct from the IO Data Sample frames
// (0x92/0x82, see xbee_io_sample.h) the existing XBee pocket remote uses.
// Deliberately free of any Arduino/SPI dependency, like xbee_io_sample.h, so
// it's unit-testable natively. See src/xbee_spi.cpp for the SPI-driven frame
// reader that calls this.
//
// Frame body layout (frame-type byte through the end, NOT including the
// leading 0x7E/length header or trailing checksum -- i.e. exactly what
// xbeeReadFrame() returns):
//   frameType(1)=0x90 addr64(8) addr16(2) options(1) payload(N)
//
// addr64 is at the same byte offset (1-8) as the IO Data Sample frames --
// see xbee_io_sample.h's header comment -- so the existing
// params.xbr/params.xbl low-address remote-matching convention carries over
// unchanged.
#pragma once

#include <stdint.h>

struct XBeeReceivePacket {
  uint32_t addrLsb;          // low 32 bits of the 64-bit source address
  const uint8_t *payload;    // points into the buffer passed to parse()
  uint16_t payloadLength;
};

// Returns true and fills `out` on success. Returns false if the frame isn't
// a Receive Packet (0x90) or is too short for its fixed 12-byte header.
static inline bool xbeeParseReceivePacket(const uint8_t *buf, uint16_t length,
                                           XBeeReceivePacket *out) {
  static const uint16_t kHeaderLength = 12;  // type(1)+addr64(8)+addr16(2)+options(1)
  // Length checked before touching buf[0] -- a 0-length buffer must not be
  // dereferenced even speculatively.
  if (length < kHeaderLength || buf[0] != 0x90) {
    return false;
  }
  out->addrLsb = ((uint32_t)buf[5] << 24) | ((uint32_t)buf[6] << 16) |
                 ((uint32_t)buf[7] << 8) | (uint32_t)buf[8];
  out->payload = buf + kHeaderLength;
  out->payloadLength = length - kHeaderLength;
  return true;
}
