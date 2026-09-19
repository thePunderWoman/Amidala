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
#include "debug_monitor_tee.h"  // must follow ReelTwo.h -- see its own header comment
#include "xbee_spi.h"
#include "xbee_frame_checksum.h"
#include "xbee_io_sample.h"
#include "xbee_receive_packet.h"
#include "xbee_transmit_frame.h"
#include "xbee_at_command.h"
#include "xbee_at_session.h"
#include "snips_packet.h"
#include "snips_remote.h"
#include "pin_config.h"
#include <SPI.h>

static const SPISettings kXBeeSettings(3000000, MSBFIRST, SPI_MODE0);

// Registered once from AmidalaController::setup() (issue #213) -- every
// Local AT Command Response (0x88) read off the SPI link gets routed here
// regardless of controllertype, since PAN ID/coordinator-role config isn't
// tied to which remote type is active. See xbeeSPIPumpFrames() below.
static XBeeATSession* sATSession = nullptr;

void xbeeSPISetATSession(XBeeATSession* session) {
    sATSession = session;
}

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
        DEBUG_PRINT("XBee: bad frame length ");
        DEBUG_PRINTLN(length);
        xbeeDrain(length + 1);  // drain data + checksum
        return 0;
    }
    for (uint16_t i = 0; i < length; i++)
        buf[i] = xbeeTransfer();
    uint8_t checksum = xbeeTransfer();
    if (!xbeeChecksumValid(buf, length, checksum)) {
        DEBUG_PRINTLN("XBee: bad checksum");
        return 0;
    }
    return length;
}

// Drains all queued frames (cap 8 per call) into `handler(buf, length)`,
// shared by xbeeSPIReceiveAll()/xbeeSPIReceiveAllSnips() below -- the two
// differ only in how they parse/dispatch a frame's body, not in how frames
// are pumped off the SPI link.
//
// Cap at 8 frames per animate() cycle. If xbeeReadFrame() returns -1 (no
// start delimiter found at all — ATTN may be stuck low with nothing
// actually queued), break immediately — looping forever would block
// fDomeDrive->animate() and prevent the RoboClaw homing timeout from
// firing. A 0 return (delimiter found, but the frame itself was bad — see
// xbeeReadFrame()'s comment) is different: the SPI stream is still in
// sync, so keep draining — more valid frames may be queued right behind a
// single corrupt one (issue #185).
template <typename FrameHandler>
static void xbeeSPIPumpFrames(FrameHandler handler) {
    uint8_t buf[64];
    for (int limit = 8; limit > 0 && digitalRead(XBEE_ATTN_PIN) == LOW; limit--) {
        SPI.beginTransaction(kXBeeSettings);
        digitalWrite(XBEE_CS_PIN, LOW);
        int32_t length = xbeeReadFrame(buf, sizeof(buf));
        digitalWrite(XBEE_CS_PIN, HIGH);
        SPI.endTransaction();

        if (length < 0) break;
        if (length == 0) continue;

        // Local AT Command Responses (0x88, issue #213) can arrive
        // regardless of controllertype -- intercepted here, before either
        // per-controllertype handler below, so neither one logs it as an
        // unrecognized frame type.
        if (buf[0] == XBeeATCommand::kResponseFrameType) {
            if (sATSession) sATSession->handleResponse(buf, (uint16_t)length);
            continue;
        }

        handler(buf, (uint16_t)length);
    }
}

void xbeeSPIPumpATResponsesOnly() {
    // The 0x88 interception already happens inside xbeeSPIPumpFrames()
    // above, before the handler runs -- an empty handler here means
    // everything else (no configured remote should even be sending data
    // yet at boot) is silently drained and ignored, not logged as
    // unrecognized.
    xbeeSPIPumpFrames([](const uint8_t*, uint16_t) {});
}

// Writes one complete API frame (0x7E start delimiter, 2-byte length,
// frame data, trailing checksum) in its own SPI transaction, independent
// of xbeeSPIPumpFrames() above, since ATTN only signals inbound data and
// has no bearing on write-readiness. Shared by xbeeSPISendPacket() (Snips
// downlink) and xbeeSPISendATCommand() (issue #213) below.
static void xbeeSPISendFrame(const uint8_t *frameData, uint16_t frameLength) {
    uint8_t checksum = xbeeComputeChecksum(frameData, frameLength);

    SPI.beginTransaction(kXBeeSettings);
    digitalWrite(XBEE_CS_PIN, LOW);
    SPI.transfer(0x7E);
    SPI.transfer((uint8_t)(frameLength >> 8));
    SPI.transfer((uint8_t)(frameLength));
    for (uint16_t i = 0; i < frameLength; i++) SPI.transfer(frameData[i]);
    SPI.transfer(checksum);
    digitalWrite(XBEE_CS_PIN, HIGH);
    SPI.endTransaction();
}

// Sends a Transmit Request (0x10) frame addressed to `dest16` (issue #204
// phase 2 downlink). Wraps XBeeTransmitFrame::buildTransmitRequest() and
// hands the result to xbeeSPISendFrame().
static void xbeeSPISendPacket(uint16_t dest16, const uint8_t *payload, uint16_t payloadLength) {
    // Sized for the current Snips::DownlinkPacket (the only caller, 33
    // bytes + 14-byte Transmit Request header = 47) with headroom -- if the
    // packet format ever grows past this, fail loudly at compile time
    // rather than have buildTransmitRequest() silently drop the frame
    // (returns 0 on an undersized output buffer).
    static_assert(XBeeTransmitFrame::kHeaderLength + Snips::kDownlinkEncodedSize <= 64,
                  "xbeeSPISendPacket's frameData buffer is too small for a downlink packet");
    uint8_t frameData[64];
    uint16_t frameLength = XBeeTransmitFrame::buildTransmitRequest(
        frameData, sizeof(frameData), payload, payloadLength, dest16);
    if (frameLength == 0) return;
    xbeeSPISendFrame(frameData, frameLength);
}

// Sends a Local AT Command Request (0x08) frame -- issue #213. Wraps
// XBeeATCommand::buildRequest() and hands the result to
// xbeeSPISendFrame(). `param`/`paramLength` may be null/0 for a query.
void xbeeSPISendATCommand(uint8_t frameId, const char command[2],
                           const uint8_t *param, uint8_t paramLength) {
    uint8_t frameData[XBeeATCommand::kRequestHeaderLength + XBeeATSession::kMaxValueLength];
    uint16_t frameLength = XBeeATCommand::buildRequest(
        frameData, sizeof(frameData), frameId, command, param, paramLength);
    if (frameLength == 0) return;
    xbeeSPISendFrame(frameData, frameLength);
}

void xbeeSPIReceiveAll(XBeePocketRemote** remotes, unsigned count) {
    xbeeSPIPumpFrames([remotes, count](const uint8_t *buf, uint16_t length) {
        XBeeIOSample sample;
        if (!xbeeParseIOSample(buf, length, &sample)) {
            DEBUG_PRINT("XBee: unrecognized/short frame type=0x");
            DEBUG_PRINT_HEX(buf[0]);
            DEBUG_PRINT(" len=");
            DEBUG_PRINTLN(length);
            return;
        }

        bool matched = false;
        for (unsigned i = 0; i < count; i++) {
            auto r = remotes[i];
            if (sample.addrLsb != r->addr) continue;
            matched = true;
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
        if (!matched) {
            DEBUG_PRINT("XBee: no configured remote for addr 0x");
            DEBUG_PRINT_HEX(sample.addrLsb);
            DEBUG_PRINTLN("");
        }
    });
}

void xbeeSPIReceiveAllSnips(SnipsRemote** remotes, unsigned count) {
    xbeeSPIPumpFrames([remotes, count](const uint8_t *buf, uint16_t length) {
        XBeeReceivePacket rx;
        if (!xbeeParseReceivePacket(buf, length, &rx)) {
            DEBUG_PRINT("XBee: unrecognized/short frame type=0x");
            DEBUG_PRINT_HEX(buf[0]);
            DEBUG_PRINT(" len=");
            DEBUG_PRINTLN(length);
            return;
        }

        Snips::UplinkPacket pkt;
        if (!Snips::decodeUplink(rx.payload, rx.payloadLength, &pkt)) {
            DEBUG_PRINTLN("XBee: short Snips uplink payload");
            return;
        }

        bool matched = false;
        for (unsigned i = 0; i < count; i++) {
            auto r = remotes[i];
            if (rx.addrLsb != r->addr) continue;
            matched = true;
            r->onUplinkReceived(pkt, rx.addr16);
            // Reply after every processed uplink, not just when a value
            // changes -- the controller treats 5s without a downlink as
            // "disconnected" regardless of uplink flow (see
            // SnipsController.ino's kConnectionTimeoutMs), so this also
            // serves as the connection-alive heartbeat it depends on.
            Snips::DownlinkPacket downlink = r->buildDownlinkPacket();
            uint8_t downlinkBuf[Snips::kDownlinkEncodedSize];
            size_t downlinkLength = Snips::encodeDownlink(downlink, downlinkBuf, sizeof(downlinkBuf));
            if (downlinkLength > 0) {
                xbeeSPISendPacket(r->addr16, downlinkBuf, (uint16_t)downlinkLength);
            }
            break;
        }
        if (!matched) {
            DEBUG_PRINT("XBee: no configured Snips controller for addr 0x");
            DEBUG_PRINT_HEX(rx.addrLsb);
            DEBUG_PRINTLN("");
        }
    });
}
