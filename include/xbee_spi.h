// xbee_spi.h
// Receive ZigBee IO Sample frames (API type 0x92) from the XBee 3 via SPI
// and update pocket remote state.
//
// Replaces the old andrewrapp XBee UART path.  The hardware coordinator is
// the same XBee 3 module; only the transport changed (Serial1 → FSPI).

#pragma once
#include "xbee_remote.h"

class SnipsRemote;
class XBeeATSession;

// Registers the session that receives every Local AT Command Response
// (0x88) read off the SPI link, regardless of controllertype (see
// xbeeSPIReceiveAll()/xbeeSPIReceiveAllSnips()'s shared frame pump in
// xbee_spi.cpp) -- issue #213. Call once from AmidalaController::setup().
// Pass nullptr to stop routing (not expected outside tests).
void xbeeSPISetATSession(XBeeATSession* session);

// Drains queued frames looking only for Local AT Command Responses (0x88),
// routing them to whatever session xbeeSPISetATSession() registered and
// silently ignoring anything else. For boot-time use (see
// AmidalaController::setup()'s coordinator-role check) before the normal
// per-controllertype receive path is running every animate() cycle.
void xbeeSPIPumpATResponsesOnly();

// Sends a Local AT Command Request (0x08) frame -- see xbee_at_command.h
// for the frame layout and xbee_at_session.h for the request/response
// correlation built on top of this. Its own SPI transaction, independent
// of the receive pump, same as xbee_spi.cpp's file-local
// xbeeSPISendPacket() (Snips downlink).
void xbeeSPISendATCommand(uint8_t frameId, const char command[2],
                           const uint8_t *param, uint8_t paramLength);

// Poll XBEE_ATTN_PIN and drain all queued IO Sample frames into `remotes`.
// Each frame's source address LSB is matched against remote[i]->addr.
// Call from the main animate() loop once per cycle.
void xbeeSPIReceiveAll(XBeePocketRemote** remotes, unsigned count);

// Poll XBEE_ATTN_PIN and drain all queued Receive Packet frames (0x90) into
// `remotes`, decoding each payload as a Snips::UplinkPacket (see
// snips_packet.h) and matching its source address LSB against
// remotes[i]->addr. Sibling to xbeeSPIReceiveAll() above -- see
// params.controllertype for which one the caller should use; the two are
// never called in the same animate() cycle. Call from the main animate()
// loop once per cycle when params.controllertype == CONTROLLER_TYPE_SNIPS.
void xbeeSPIReceiveAllSnips(SnipsRemote** remotes, unsigned count);
