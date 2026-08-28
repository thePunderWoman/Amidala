// xbee_spi.cpp
// IO Sample receive path for the XBee 3 SPI interface. Recognizes both the
// ZigBee frame type (0x92, what this firmware has always used) and the
// 802.15.4 frame type (0x82) it's migrating toward -- see issue #187. Which
// one arrives depends only on what's flashed on the transmitting pocket
// remote, so both are simply accepted side by side; there's no local
// runtime toggle for this. See include/xbee_io_sample.h for the frame body
// layouts and parsing.
//
// Outer frame format parsed here (XBee API, AP=1):
//   [0x7E][len_hi][len_lo][frame data...][checksum]
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
#include "xbee_io_sample.h"
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
    // stuck, and looping forever would block fDomeDrive->animate() and prevent
    // the RoboClaw homing timeout from firing.
    for (int limit = 8; limit > 0 && digitalRead(XBEE_ATTN_PIN) == LOW; limit--) {
        SPI.beginTransaction(kXBeeSettings);
        digitalWrite(XBEE_CS_PIN, LOW);
        uint16_t length = xbeeReadFrame(buf, sizeof(buf));
        digitalWrite(XBEE_CS_PIN, HIGH);
        SPI.endTransaction();

        if (length == 0) break;  // no valid frame — stop draining to avoid blocking

        XBeeIOSample sample;
        if (!xbeeParseIOSample(buf, (uint16_t)length, &sample))
            continue;

        for (unsigned i = 0; i < count; i++) {
            auto r = remotes[i];
            if (sample.addrLsb != r->addr) continue;
            // Only update analog channels that are present in this packet.
            // A button-only packet (analogMask==0) must not clobber the last
            // known stick position with the {512,512,0,0} defaults — that
            // would reset lx/ly to zero and prevent gesture direction detection.
            if (sample.analogMask & (1 << 0)) r->y  = sample.analog[0];
            if (sample.analogMask & (1 << 1)) r->x  = sample.analog[1];
            if (sample.analogMask & (1 << 2)) r->w1 = sample.analog[2];
            if (sample.analogMask & (1 << 3)) r->w2 = sample.analog[3];
            // Active-LOW buttons: a DIO bit of 0 means the button is pressed.
            // If a DIO pin isn't in digitalMask (not wired), its bit is 0 in
            // digitalSamples regardless of physical state — guard with the mask
            // so an unwired button reads as not-pressed rather than always-pressed.
            r->button[0] = (sample.digitalMask & (1 << 5))  && !(sample.digitalSamples & (1 << 5));   // triangle
            r->button[1] = (sample.digitalMask & (1 << 6))  && !(sample.digitalSamples & (1 << 6));   // circle
            r->button[2] = (sample.digitalMask & (1 << 10)) && !(sample.digitalSamples & (1 << 10));  // cross
            r->button[3] = (sample.digitalMask & (1 << 11)) && !(sample.digitalSamples & (1 << 11));  // square
            r->button[4] = (sample.digitalMask & (1 << 4))  && !(sample.digitalSamples & (1 << 4));   // l3
            r->lastPacket = millis();
            if (r->type != r->kXBee) r->type = r->kXBee;
            DEBUG_PRINT("XBee J");
            DEBUG_PRINT(i + 1);
            DEBUG_PRINTLN(" packet");
            break;
        }
    }
}
