// wcb_client_controller.h
// Owns the (possibly nonexistent) live WCB_Client mesh connection and everything
// that depends on it: boot-time validation, outbound routing (UART0 vs mesh),
// inbound-message monitor visibility, and the /api/wcb/status JSON. (How HCR
// frames are framed for the link is hcr_link.h; sendHcr() below is just its
// mesh leg.)
//
// WCB_Client's constructor needs runtime config (octets/password/quantity/id)
// that's only known after loadConfig() runs inside setup() -- but
// AmidalaController's member sub-objects are constructed via the constructor's
// member-initializer list, which runs BEFORE setup()'s body. So WCB_Client
// can't be a plain member like fHCR/fDomeDrive; this wrapper defers its
// construction to begin(), called explicitly from setup() once config is
// loaded and validated. Never freed once constructed -- WCB_Client has no
// teardown API, so "disabling" WCB Client after it's been constructed just
// stops routing traffic through it (see routeOutbound()); the underlying
// radio/heartbeat machinery keeps running, same as it must per the library's
// own "call update() every loop iteration, forever" contract.
#pragma once

// Must come before params.h -- Arduino.h defines the min/max macros that
// core.h (pulled in by params.h) uses, and this header has no other include
// that would bring those in first (controller.h normally gets away with
// omitting this because ReelTwo.h, included earlier there, does it
// transitively; this header has no such neighbor).
#include <Arduino.h>
#include <EEPROM.h> // params.h uses the global EEPROM object in AmidalaParameters::init()

#include <Print.h>
#include <WString.h>

#include "params.h"
#include "serial_output.h"
#include "wcb_config_validator.h"
#include "wcb_rx_queue.h"
#include "wcb_status_json.h"

class WCB_Client;

class WCBClientController {
public:
    WCBClientController() {}

    // Called once from AmidalaController::setup(), after config load/
    // self-heal. No-op if params.wcbenable is off. If it's on but the
    // identity fields don't validate, logs a warning to `out` and skips
    // attempting to join -- never attempts a connection with an incomplete
    // identity (item 4's exact requirement).
    void begin(const AmidalaParameters &params, Print &out);

    // Called every animate() tick. No-op (cheap) if begin() never
    // constructed a live client. Drains the RX queue into the monitor
    // (tagged "MESH: ", gated on params.wcbenable so mesh chatter doesn't
    // scroll the ring buffer when nobody asked to see it), and pumps
    // WCB_Client::update() (required every loop iteration per the library).
    void poll(const AmidalaParameters &params);

    // Called from sendSerialString() instead of writing to UART0 directly.
    // Returns true if the command was broadcast over the mesh (caller must
    // skip the UART0 write); false if the caller should write to UART0
    // itself. Covers both "UART0 is the selected destination" and the
    // fail-safe fallback: if WCB is selected but isn't actually live
    // (disabled, failed validation, or the broadcast call itself failed),
    // this returns false so the command is never silently dropped.
    //
    // cmd may be a compound, delim-delimited string (e.g. a single sstr/
    // gadget entry like "DM:ALARM|:PL4:PP100:PR30:PW20:PH", the same
    // convention sendSerialStringTo() splits into separate UART0 lines) --
    // WCB_Client::broadcast() has no delimiter handling of its own and
    // sends one complete command per call, so each delimited piece is
    // broadcast individually rather than shipping the whole compound
    // string as one unsplit blob (which would desync both halves' intended
    // receivers -- see the regression this fixed).
    bool routeOutbound(const char *cmd, bool wantMesh, uint8_t delim);

    // One HCR line (already framed for the link, see hcr_link.h) to the mesh.
    // Both return false if the mesh isn't live or the send was refused, so the
    // caller can fall back to the wired link.
    //
    // broadcastHcr: every WCB gets it (bare frames -- any WCB might have the
    // HCR on a plain serial port).
    bool broadcastHcr(const char *line);
    // sendHcrToHost: unicast to the WCB advertising native HCR hosting (WDP
    // capability bit) when one is online, else broadcast. Aiming it matters
    // because a WCB never forwards a mesh-origin ;H command (its "one-hop
    // cap") -- the receiver runs it locally, so a broadcast makes every WCB
    // but the host print "HCR not configured" per command. (A command that
    // arrives over a WCB's serial port IS forwarded to the host, which is why
    // the wired path needs no WCB number or port.)
    bool sendHcrToHost(const char *line);

    // Live status for /api/wcb/status.
    String statusJson(const AmidalaParameters &params) const;

    // Sticky flag: an identity field (octets/password/quantity/id) was
    // edited via the web UI after the live client already exists. There is
    // no way to reconfigure a constructed WCB_Client, so the edit is
    // persisted to params/config.txt but not applied until reboot.
    bool rebootRequired() const { return fRebootRequired; }
    void flagRebootRequired() { fRebootRequired = true; }

    bool isRunning() const { return fClient != nullptr; }

private:
    WCB_Client       *fClient        = nullptr;
    WCBRxQueue        fRxQueue;
    bool              fRebootRequired     = false;

    // WCB_Client's onCommand callback is a plain C function pointer (no
    // captures, no member-function pointers) -- bridge via a static
    // instance pointer, same pattern as BTGamepad::sInstance.
    static WCBClientController *sInstance;
    static void onCommandBridge(uint8_t senderID, const char *command);
};
