// hcr_link.h
// How HCR commands are framed for the link they travel over (params.hcrlink).
//
//   HCR_LINK_SERIAL      (default) the HCR board -- or a WCB acting as a dumb
//                        serial pipe to one -- gets the library's bare
//                        "<...>" frames, exactly as HCRVocalizer builds them.
//   HCR_LINK_WCB_NATIVE  a Wireless Communication Board (firmware 6.1.0+) owns
//                        the HCR via ?HCR,PORT,Sx:baud. That port is removed
//                        from the WCB's broadcast loop, so a bare "<...>" frame
//                        is never written to it. The WCB's raw-passthrough verb
//                        ";H,RAW,<frame>" is, and puts the identical bytes on
//                        the HCR's wire.
//
// Pure logic, no Arduino/WCB_Client/hcr.h dependencies, so it is unit tested
// natively; hcr_link_transport.h adapts it to HCRVocalizer's transport hook.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define HCR_LINK_SERIAL      0
#define HCR_LINK_WCB_NATIVE  1

// Longest single mesh line: WCB_Client::broadcast() does not fragment, and
// its single-packet ceiling is 199 chars, 187 once the checksum suffix is on.
#define HCR_LINK_MAX_LINE    187
// Line buffer, and the same line framed for a serial link ("\n" + line + "\n").
#define HCR_LINK_LINE_BUF    (HCR_LINK_MAX_LINE + 1)
#define HCR_LINK_BYTES_BUF   (HCR_LINK_MAX_LINE + 3)

// WCB runtime command: command character ';' + HCR device 'H' + RAW verb.
// (';' is the WCB's default command character; ?CMDCHAR can change it.)
#define HCR_LINK_NATIVE_PREFIX ";H,RAW,"

// Wrap one HCR frame for a WCB with native HCR. False if the result would not
// fit `out` or a single mesh packet (nothing usable is written then).
inline bool hcrWrapNative(const char *frame, char *out, size_t outLen) {
  size_t plen = strlen(HCR_LINK_NATIVE_PREFIX);
  size_t flen = frame ? strlen(frame) : 0;
  if (flen == 0) return false;
  if (plen + flen > HCR_LINK_MAX_LINE || plen + flen >= outLen) return false;
  memcpy(out, HCR_LINK_NATIVE_PREFIX, plen);
  memcpy(out + plen, frame, flen + 1);
  return true;
}

// Where routed HCR traffic actually goes. The firmware implements this over
// WCB_Client and the UART0 handle; tests use a recording fake.
class HcrLinkSink {
public:
  virtual ~HcrLinkSink() {}
  // One complete line broadcast to every WCB on the mesh. False if the mesh
  // isn't live or the send was refused, so the caller can fall back to the
  // wired link.
  virtual bool sendMesh(const char *line) = 0;
  // Same, but for a WCB-native line: aim it at the WCB that hosts the HCR when
  // that's known (so the others don't log "HCR not configured" per command),
  // else broadcast. Only native lines may narrow the audience -- a bare frame
  // must reach every WCB, since any of them might have the HCR on a plain port.
  virtual bool sendMeshToHost(const char *line) = 0;
  // Exact bytes for the wired link (UART0), written in a single call.
  virtual void writeSerial(const char *bytes) = 0;
  // Record one outbound HCR line in the monitor's TX log: `text` is what went
  // on the wire (no priming/trailing newlines), viaMesh says which link. Called
  // exactly once per frame that is actually sent. Needed because nothing else
  // sees HCR traffic leave: HCRVocalizer writes UART0 directly, and the
  // monitor's S0 tap only drains what Serial0 RECEIVES.
  virtual void logTx(const char *text, bool viaMesh) = 0;
};

// Route one HCR frame (as built by HCRVocalizer, "<...>" included).
// Returns true if handled here, so HCRVocalizer must skip its own local write;
// false to let it write the bare frame to its serial port as it always has.
//
// wantMesh is params.outboundserial == 1.
//
// Invariant: in WCB-native mode an unwrapped frame is never sent anywhere --
// a WCB would treat it as a broadcast string and drop it at the HCR port.
// Invariant: every frame that is sent is logged once via sink.logTx(), on the
// link it actually took (mesh, or UART0 -- including the case where this
// returns false and HCRVocalizer does the UART0 write itself).
inline bool hcrLinkRoute(uint8_t link, bool wantMesh, const char *frame, HcrLinkSink &sink) {
  if (link != HCR_LINK_WCB_NATIVE) {
    if (wantMesh && sink.sendMesh(frame)) {
      sink.logTx(frame, true);
      return true;
    }
    // Not handled: HCRVocalizer writes this frame to UART0 itself, which
    // bypasses us, so log it here.
    sink.logTx(frame, false);
    return false;
  }

  char line[HCR_LINK_LINE_BUF];
  if (!hcrWrapNative(frame, line, sizeof(line)))
    return true;  // unsendable (empty/oversize): swallow rather than leak it bare

  if (wantMesh && sink.sendMeshToHost(line)) {
    sink.logTx(line, true);
    return true;
  }

  // Wired to a WCB (or the mesh isn't live). Same idle-line priming newline
  // HCRVocalizer prepends on its own serial path; the WCB ignores empty lines.
  char bytes[HCR_LINK_BYTES_BUF];
  snprintf(bytes, sizeof(bytes), "\n%s\n", line);
  sink.writeSerial(bytes);
  sink.logTx(line, false);
  return true;
}
