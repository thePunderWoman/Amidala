// xbee_io_sample.h
// Parsing for XBee "IO Data Sample" API frame bodies -- both the ZigBee
// variant (frame type 0x92) this firmware has always used, and the
// 802.15.4 variant (frame type 0x82) it's migrating toward (issue #187,
// see #185). Deliberately free of any Arduino/SPI dependency -- like
// xbee_frame_checksum.h -- so it's unit-testable natively. See
// src/xbee_spi.cpp for the SPI-driven frame reader that calls this.
//
// Both frame types carry the 64-bit source address at the same offset
// (bytes 1-8, right after the frame-type byte), so the existing
// params.xbr/params.xbl low-address remote matching in xbee_spi.cpp needs
// no changes -- only the header fields between the address and the IO
// sample data differ:
//
//   0x92 (ZigBee):   ...addr64(8) addr16(2) options(1) numSamples(1)
//                     digitalMask(2) analogMask(1) [samples...]
//   0x82 (802.15.4): ...addr64(8) rssi(1)   options(1) numSamples(1)
//                     digitalMask(2) analogMask(1) [samples...]
//
// 0x83 (802.15.4, 16-bit short address) is intentionally not handled --
// this firmware's remote matching is keyed entirely off the 64-bit long
// address, so transmitting 802.15.4 modules need MY=0xFFFF (short
// addressing disabled) so the coordinator only ever sees 0x82.
#pragma once

#include <stdint.h>

struct XBeeIOSample {
  uint32_t addrLsb;         // low 32 bits of the 64-bit source address
  uint16_t digitalMask;
  uint8_t analogMask;
  uint16_t digitalSamples;
  uint16_t analog[4];       // per analogMask bit; {512,512,0,0} if unset
};

// Parses an IO Data Sample frame body -- the frame-type byte through the
// end of the frame, NOT including the leading 0x7E/length header or
// trailing checksum (i.e. exactly what xbeeReadFrame() returns). Returns
// true and fills `out` on success. Returns false if the frame type isn't
// recognized, or the frame is too short for its type's fixed header --
// callers should silently skip the frame in either case, the same way an
// unrecognized frame type always has been.
static inline bool xbeeParseIOSample(const uint8_t *buf, uint16_t length,
                                      XBeeIOSample *out) {
  uint16_t maskOff;  // offset of the 2-byte digital channel mask
  switch (buf[0]) {
    case 0x92: maskOff = 13; break;  // ZigBee
    case 0x82: maskOff = 12; break;  // 802.15.4, 64-bit address
    default: return false;
  }
  if (length < (uint16_t)(maskOff + 3))  // mask(2) + analogMask(1)
    return false;

  out->addrLsb = ((uint32_t)buf[5] << 24) | ((uint32_t)buf[6] << 16) |
                 ((uint32_t)buf[7] << 8) | (uint32_t)buf[8];
  out->digitalMask = ((uint16_t)buf[maskOff] << 8) | buf[maskOff + 1];
  out->analogMask = buf[maskOff + 2];

  uint16_t dataOff = maskOff + 3;
  out->digitalSamples = 0;
  if (out->digitalMask != 0) {
    if (dataOff + 2 > length) return false;
    out->digitalSamples = ((uint16_t)buf[dataOff] << 8) | buf[dataOff + 1];
    dataOff += 2;
  }

  out->analog[0] = 512;
  out->analog[1] = 512;
  out->analog[2] = 0;
  out->analog[3] = 0;
  for (int i = 0; i < 4; i++) {
    if (out->analogMask & (1 << i)) {
      if (dataOff + 2 > length) break;
      out->analog[i] = ((uint16_t)buf[dataOff] << 8) | buf[dataOff + 1];
      dataOff += 2;
    }
  }
  return true;
}
