// xbee_spi.cpp
// ZigBee IO Sample (0x92) receive path for the XBee 3 SPI interface.
//
// Frame format parsed here (XBee API, AP=1):
//   [0x7E][len_hi][len_lo][0x92][8-byte addr64][2-byte addr16][options]
//   [num_samples][digital_mask_hi][digital_mask_lo][analog_mask]
//   [digital_samples (2 bytes, if mask!=0)]
//   [analog_0 (2 bytes), ..., analog_3 (2 bytes), per analogMask bits]
//   [checksum]
//
// Pocket remote wire format (unchanged from Mega / andrewrapp era):
//   Analog 0 → y (vertical stick)
//   Analog 1 → x (horizontal stick)
//   Analog 2 → w1 (wheel)
//   Analog 3 → w2 (wheel)
//   DIO4  → button[4] l3      (active-LOW)
//   DIO5  → button[0] triangle(active-LOW)
//   DIO6  → button[1] circle  (active-LOW)
//   DIO10 → button[2] cross   (active-LOW)
//   DIO11 → button[3] square  (active-LOW)

#include "debug.h"
#include "ReelTwo.h"   // must precede xbee_remote.h — provides Arduino type definitions
#include "xbee_spi.h"
#include "xbee_frame_checksum.h"
#include "pin_config.h"
#include <SPI.h>

static const SPISettings kXBeeSettings(3000000, MSBFIRST, SPI_MODE0);

static inline uint8_t xbeeTransfer() { return SPI.transfer(0xFF); }

static void xbeeDrain(uint16_t n) { while (n--) SPI.transfer(0xFF); }

// Read one complete API frame into buf (frame-type byte + data, excludes the
// 0x7E/length header and trailing checksum). Returns the frame data length
// on success.
//
// Returns -1 if no start delimiter (0x7E) was found at all within the idle
// scan -- ATTN may be stuck low with nothing actually queued, and the caller
// should stop draining rather than keep burning SPI cycles.
//
// Returns 0 if a delimiter WAS found but the frame itself is unusable (bad
// length, or a checksum mismatch -- see xbeeChecksumValid() -- which most
// often means the frame got mis-framed because the module isn't actually in
// AP=1/unescaped API mode, see issue #185). Unlike the -1 case, the SPI
// stream is still in sync here (the bad frame was fully drained), so the
// caller should keep going: ATTN may still be low with more, valid, frames
// queued right behind this one.
static int32_t xbeeReadFrame(uint8_t* buf, uint16_t maxLen) {
    // Skip idle 0xFF bytes (XBee pads before the start delimiter)
    uint8_t b = 0xFF;
    for (int i = 0; i < 32 && b != 0x7E; i++)
        b = xbeeTransfer();
    if (b != 0x7E) return -1;

    uint16_t length = ((uint16_t)xbeeTransfer() << 8) | xbeeTransfer();
    if (length == 0 || length > maxLen) {
        xbeeDrain(length + 1);  // drain data + checksum
        return 0;
    }
    for (uint16_t i = 0; i < length; i++)
        buf[i] = xbeeTransfer();
    uint8_t checksum = xbeeTransfer();
    if (!xbeeChecksumValid(buf, length, checksum))
        return 0;
    return length;
}

void xbeeSPIReceiveAll(XBeePocketRemote** remotes, unsigned count) {
    uint8_t buf[64];

    // Cap at 8 frames per animate() cycle. If xbeeReadFrame() returns -1 (no
    // start delimiter found at all — ATTN may be stuck low with nothing
    // actually queued), break immediately — looping forever would block
    // fDomeDrive->animate() and prevent the RoboClaw homing timeout from
    // firing. A 0 return (delimiter found, but the frame itself was bad —
    // see xbeeReadFrame()'s comment) is different: the SPI stream is still
    // in sync, so keep draining — more valid frames may be queued right
    // behind a single corrupt one (issue #185).
    for (int limit = 8; limit > 0 && digitalRead(XBEE_ATTN_PIN) == LOW; limit--) {
        SPI.beginTransaction(kXBeeSettings);
        digitalWrite(XBEE_CS_PIN, LOW);
        int32_t length = xbeeReadFrame(buf, sizeof(buf));
        digitalWrite(XBEE_CS_PIN, HIGH);
        SPI.endTransaction();

        if (length < 0) break;
        if (length == 0) continue;

        if (length < 16 || buf[0] != 0x92)
            continue;

        uint32_t addrLsb = ((uint32_t)buf[5] << 24) | ((uint32_t)buf[6] << 16) |
                           ((uint32_t)buf[7] << 8)  |  (uint32_t)buf[8];

        uint16_t digitalMask = ((uint16_t)buf[13] << 8) | buf[14];
        uint8_t  analogMask  = buf[15];

        uint16_t dataOff = 16;
        uint16_t digitalSamples = 0;
        if (digitalMask != 0) {
            if (dataOff + 2 > length) continue;
            digitalSamples = ((uint16_t)buf[dataOff] << 8) | buf[dataOff + 1];
            dataOff += 2;
        }

        uint16_t analog[4] = {512, 512, 0, 0};
        for (int i = 0; i < 4; i++) {
            if (analogMask & (1 << i)) {
                if (dataOff + 2 > length) break;
                analog[i] = ((uint16_t)buf[dataOff] << 8) | buf[dataOff + 1];
                dataOff += 2;
            }
        }

        for (unsigned i = 0; i < count; i++) {
            auto r = remotes[i];
            if (addrLsb != r->addr) continue;
            // Only update analog channels that are present in this packet.
            // A button-only packet (analogMask==0) must not clobber the last
            // known stick position with the {512,512,0,0} defaults — that
            // would reset lx/ly to zero and prevent gesture direction detection.
            if (analogMask & (1 << 0)) r->y  = analog[0];
            if (analogMask & (1 << 1)) r->x  = analog[1];
            if (analogMask & (1 << 2)) r->w1 = analog[2];
            if (analogMask & (1 << 3)) r->w2 = analog[3];
            // Active-LOW buttons: a DIO bit of 0 means the button is pressed.
            // If a DIO pin isn't in digitalMask (not wired), its bit is 0 in
            // digitalSamples regardless of physical state — guard with the mask
            // so an unwired button reads as not-pressed rather than always-pressed.
            r->button[0] = (digitalMask & (1 << 5))  && !(digitalSamples & (1 << 5));   // triangle
            r->button[1] = (digitalMask & (1 << 6))  && !(digitalSamples & (1 << 6));   // circle
            r->button[2] = (digitalMask & (1 << 10)) && !(digitalSamples & (1 << 10));  // cross
            r->button[3] = (digitalMask & (1 << 11)) && !(digitalSamples & (1 << 11));  // square
            r->button[4] = (digitalMask & (1 << 4))  && !(digitalSamples & (1 << 4));   // l3
            r->lastPacket = millis();
            if (r->type != r->kXBee) r->type = r->kXBee;
            DEBUG_PRINT("XBee J");
            DEBUG_PRINT(i + 1);
            DEBUG_PRINTLN(" packet");
            break;
        }
    }
}
