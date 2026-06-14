// xbee_spi.h
// Receive ZigBee IO Sample frames (API type 0x92) from the XBee 3 via SPI
// and update pocket remote state.
//
// Replaces the old andrewrapp XBee UART path.  The hardware coordinator is
// the same XBee 3 module; only the transport changed (Serial1 → FSPI).

#pragma once
#include "xbee_remote.h"

// Poll XBEE_ATTN_PIN and drain all queued IO Sample frames into `remotes`.
// Each frame's source address LSB is matched against remote[i]->addr.
// Call from the main animate() loop once per cycle.
void xbeeSPIReceiveAll(XBeePocketRemote** remotes, unsigned count);
