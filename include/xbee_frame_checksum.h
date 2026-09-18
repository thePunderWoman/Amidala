// xbee_frame_checksum.h
// Checksum validation for XBee API frames (AP=1, unescaped).
//
// Deliberately free of any Arduino/SPI dependency -- like
// safety_stop_latch.h -- so it's unit-testable natively. See
// src/xbee_spi.cpp for the SPI-driven frame reader that calls this.
#pragma once

#include <stdint.h>

// XBee API frame checksum: 0xFF minus the low 8 bits of the sum of all
// bytes in the frame (the data after the 0x7E start delimiter and 2-byte
// length header, NOT including the checksum byte itself). Used on both the
// receive side (xbeeChecksumValid(), below) and the transmit side (issue
// #204 phase 2 downlink transmit) -- single implementation for both so a
// future formula change can't land on only one side.
static inline uint8_t xbeeComputeChecksum(const uint8_t *frame, uint16_t length) {
  uint8_t sum = 0;
  for (uint16_t i = 0; i < length; i++)
    sum = (uint8_t)(sum + frame[i]);
  return (uint8_t)(0xFF - sum);
}

// Returns true if `checksum` matches what xbeeComputeChecksum() computes
// for these bytes.
static inline bool xbeeChecksumValid(const uint8_t *frame, uint16_t length,
                                      uint8_t checksum) {
  return xbeeComputeChecksum(frame, length) == checksum;
}
