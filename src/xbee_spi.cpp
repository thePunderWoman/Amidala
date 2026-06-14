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
#include "pin_config.h"
#include <SPI.h>

static const SPISettings kXBeeSettings(3000000, MSBFIRST, SPI_MODE0);

static inline uint8_t xbeeTransfer() { return SPI.transfer(0xFF); }

static void xbeeDrain(uint16_t n) { while (n--) SPI.transfer(0xFF); }

// Read one complete API frame into buf (frame-type byte + data, excludes the
// 0x7E/length header and trailing checksum).  Returns the frame data length,
// or 0 if no valid frame was found.
static uint16_t xbeeReadFrame(uint8_t* buf, uint16_t maxLen) {
    // Skip idle 0xFF bytes (XBee pads before the start delimiter)
    uint8_t b = 0xFF;
    for (int i = 0; i < 32 && b != 0x7E; i++)
        b = xbeeTransfer();
    if (b != 0x7E) return 0;

    uint16_t length = ((uint16_t)xbeeTransfer() << 8) | xbeeTransfer();
    if (length == 0 || length > maxLen) {
        xbeeDrain(length + 1);  // drain data + checksum
        return 0;
    }
    for (uint16_t i = 0; i < length; i++)
        buf[i] = xbeeTransfer();
    xbeeTransfer();  // checksum — read and discard
    return length;
}

void xbeeSPIReceiveAll(XBeePocketRemote** remotes, unsigned count) {
    uint8_t buf[64];

    // Cap at 8 frames per animate() cycle. If xbeeReadFrame() returns 0
    // (ATTN LOW but no valid frame found), break immediately — the pin may be
    // stuck, and looping forever would block fDomeDrive.animate() and prevent
    // the RoboClaw homing timeout from firing.
    for (int limit = 8; limit > 0 && digitalRead(XBEE_ATTN_PIN) == LOW; limit--) {
        SPI.beginTransaction(kXBeeSettings);
        digitalWrite(XBEE_CS_PIN, LOW);
        uint16_t length = xbeeReadFrame(buf, sizeof(buf));
        digitalWrite(XBEE_CS_PIN, HIGH);
        SPI.endTransaction();

        if (length == 0) break;  // no valid frame — stop draining to avoid blocking

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
            r->y  = analog[0];
            r->x  = analog[1];
            r->w1 = analog[2];
            r->w2 = analog[3];
            r->button[0] = !(digitalSamples & (1 << 5));   // triangle
            r->button[1] = !(digitalSamples & (1 << 6));   // circle
            r->button[2] = !(digitalSamples & (1 << 10));  // cross
            r->button[3] = !(digitalSamples & (1 << 11));  // square
            r->button[4] = !(digitalSamples & (1 << 4));   // l3
            r->lastPacket = millis();
            if (r->type != r->kXBee) r->type = r->kXBee;
            DEBUG_PRINT("XBee J");
            DEBUG_PRINT(i + 1);
            DEBUG_PRINTLN(" packet");
            break;
        }
    }
}
