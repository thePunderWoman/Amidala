// xbee_at_command.h
// Builds the XBee "Local AT Command Request" API frame (0x08) and parses
// its "Local AT Command Response" (0x88) -- the first time Amidala has ever
// configured its own XBee module rather than just exchanging data frames
// with it (issue #213: PAN ID / coordinator role read+write over SPI).
// Deliberately free of any Arduino/SPI dependency, same convention as
// xbee_transmit_frame.h/xbee_receive_packet.h, so it's unit-testable
// natively. See src/xbee_at_session.cpp for the stateful request/response
// correlation built on top of this, and src/xbee_spi.cpp for the SPI-driven
// send/receive that wraps these with the 0x7E/length header and trailing
// checksum.
//
// Request frame body layout (frame-type byte through the end, NOT
// including the leading 0x7E/length header or trailing checksum -- same
// "frame data" convention as xbee_transmit_frame.h):
//   frameType(1)=0x08 frameId(1) atCommand(2) parameter(0-N)
//
// Unlike XBeeTransmitFrame::buildTransmitRequest() (frameId always 0,
// fire-and-forget), frameId here must be a real, caller-chosen, non-zero
// value: it's echoed back in the 0x88 response, which is the only way to
// correlate a response to the specific request that caused it when a query
// and a set could both plausibly be in flight in principle. (Digi frame
// convention: 0x00 conventionally means "no response wanted" -- callers
// should never pass 0.)
//
// Response frame body layout (frame type 0x88):
//   frameType(1)=0x88 frameId(1) atCommand(2) status(1) value(0-N)
// status: 0x00 OK, 0x01 ERROR, 0x02 invalid command, 0x03 invalid
// parameter, 0x04 Tx failure. `value` is only present on a successful query
// (a successful set's response has no value bytes at all).
#pragma once

#include <stdint.h>
#include <string.h>

namespace XBeeATCommand {

constexpr uint8_t kRequestFrameType = 0x08;
constexpr uint8_t kResponseFrameType = 0x88;
constexpr uint16_t kRequestHeaderLength = 4;   // type+frameId+2-char command
constexpr uint16_t kResponseHeaderLength = 5;  // type+frameId+2-char command+status

enum Status : uint8_t {
  kOk = 0x00,
  kError = 0x01,
  kInvalidCommand = 0x02,
  kInvalidParameter = 0x03,
  kTxFailure = 0x04,
};

// Returns the number of bytes written, or 0 if outCapacity is too small.
static inline uint16_t buildRequest(uint8_t *outFrameData, uint16_t outCapacity,
                                     uint8_t frameId, const char command[2],
                                     const uint8_t *param, uint8_t paramLength) {
  uint16_t total = (uint16_t)(kRequestHeaderLength + paramLength);
  if (outCapacity < total) return 0;

  outFrameData[0] = kRequestFrameType;
  outFrameData[1] = frameId;
  outFrameData[2] = (uint8_t)command[0];
  outFrameData[3] = (uint8_t)command[1];
  if (paramLength > 0) {
    memcpy(outFrameData + kRequestHeaderLength, param, paramLength);
  }
  return total;
}

// Converts up to 8 raw bytes (big-endian) to a minimal hex string -- the
// leading included byte is NOT zero-padded (so {0x01} -> "1", not "01"),
// every byte after it is (so {0x01,0x02} -> "102", correctly re-parsing as
// 0x102, not "0102"). All-zero input reads as "0". Writes at most 17 bytes
// (16 hex digits + NUL) into `out`; returns the string length written
// (excluding the NUL), or 0 if outCap is too small.
static inline uint8_t valueToHexString(const uint8_t *bytes, uint8_t len, char *out, uint8_t outCap) {
  if (len == 0) return 0;
  uint8_t start = 0;
  while (start + 1 < len && bytes[start] == 0) start++;

  // Compute the exact output length up front and validate it once,
  // including room for the NUL terminator, rather than bounds-checking
  // each character write individually -- easy to get that arithmetic
  // subtly wrong right at the terminator (a prior version of this function
  // did, confirmed by ASan: writing the last hex digit right up to
  // outCap-1 left no room for the '\0' after it).
  uint8_t leadingDigits = (bytes[start] >= 0x10) ? 2 : 1;
  uint8_t totalDigits = (uint8_t)(leadingDigits + (len - 1 - start) * 2);
  if (outCap < (uint8_t)(totalDigits + 1)) return 0;

  static const char kHexDigits[] = "0123456789ABCDEF";
  uint8_t pos = 0;
  if (leadingDigits == 2) out[pos++] = kHexDigits[bytes[start] >> 4];
  out[pos++] = kHexDigits[bytes[start] & 0x0F];
  for (uint8_t i = start + 1; i < len; i++) {
    out[pos++] = kHexDigits[bytes[i] >> 4];
    out[pos++] = kHexDigits[bytes[i] & 0x0F];
  }
  out[pos] = '\0';
  return pos;
}

// Inverse of valueToHexString() -- parses a variable-length hex string (as
// typed by a user, or produced by valueToHexString() above) into the
// minimal big-endian byte encoding an AT command SET accepts. There's no
// need to pad to the register's full native width -- see this header's top
// comment on why a short PAN ID like "4133" is normal, not something that
// needs padding. Non-hex characters are treated as 0 (matches strtoul-style
// permissiveness elsewhere in this codebase). Returns the number of bytes
// written (at most outCap, at most 8 significant bytes from the input).
static inline uint8_t hexStringToValue(const char *hex, uint8_t *out, uint8_t outCap) {
  uint64_t val = 0;
  for (const char *p = hex; *p; p++) {
    char c = *p;
    uint8_t digit = (c >= '0' && c <= '9') ? (uint8_t)(c - '0')
                  : (c >= 'a' && c <= 'f') ? (uint8_t)(c - 'a' + 10)
                  : (c >= 'A' && c <= 'F') ? (uint8_t)(c - 'A' + 10)
                  : 0;
    val = (val << 4) | digit;
  }
  uint8_t bytes[8];
  for (int i = 0; i < 8; i++) bytes[i] = (uint8_t)(val >> (8 * (7 - i)));
  uint8_t start = 0;
  while (start + 1 < 8 && bytes[start] == 0) start++;
  uint8_t len = 8 - start;
  if (len > outCap) len = outCap;
  memcpy(out, bytes + start, len);
  return len;
}

struct Response {
  uint8_t frameId;
  char command[2];
  uint8_t status;
  const uint8_t *value;   // points into the buffer passed to parseResponse(); may be empty
  uint8_t valueLength;
};

// Returns true and fills `out` on success. Returns false if the frame isn't
// a Local AT Command Response (0x88) or is too short for its fixed 5-byte
// header. Length is checked before touching buf[0] -- a 0-length buffer
// must not be dereferenced even speculatively.
static inline bool parseResponse(const uint8_t *buf, uint16_t length, Response *out) {
  if (length < kResponseHeaderLength || buf[0] != kResponseFrameType) {
    return false;
  }
  out->frameId = buf[1];
  out->command[0] = (char)buf[2];
  out->command[1] = (char)buf[3];
  out->status = buf[4];
  out->value = buf + kResponseHeaderLength;
  out->valueLength = (uint8_t)(length - kResponseHeaderLength);
  return true;
}

}  // namespace XBeeATCommand
