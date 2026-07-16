// wcb_client_controller.h
// Owns the (possibly nonexistent) live WCB_Client mesh connection and everything
// that depends on it: boot-time validation, outbound routing (UART0 vs mesh),
// the HCR transport wiring, inbound-message monitor visibility, and the
// /api/wcb/status JSON.
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
#include "wcb_hcr_transport.h"
#include "wcb_rx_queue.h"
#include "wcb_status_json.h"

class HCRVocalizer;
class WCB_Client;

class WCBClientController {
public:
    WCBClientController() {}

    // Called once from AmidalaController::setup(), after config load/
    // self-heal. No-op if params.wcbenable is off. If it's on but the
    // identity fields don't validate, logs a warning to `out` and skips
    // attempting to join -- never attempts a connection with an incomplete
    // identity (item 4's exact requirement).
    void begin(const AmidalaParameters &params, HCRVocalizer &hcr, Print &out);

    // Called every animate() tick. No-op (cheap) if begin() never
    // constructed a live client. Drains the RX queue into the monitor
    // (tagged "MESH: ", gated on params.wcbenable so mesh chatter doesn't
    // scroll the ring buffer when nobody asked to see it), pumps
    // WCB_Client::update() (required every loop iteration per the library),
    // and reacts to a live params.outboundserial change by wiring/unwiring
    // the HCR transport -- unlike the identity fields, outboundserial is
    // safe to apply without a reboot since it doesn't touch WCB_Client's
    // construction at all, only which path outbound commands take.
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
    WCBHcrTransport  *fHcrTransport  = nullptr;
    HCRVocalizer     *fHcr           = nullptr;
    WCBRxQueue        fRxQueue;
    bool              fHcrTransportActive = false; // mirrors the last-applied outboundserial state
    bool              fRebootRequired     = false;

    // WCB_Client's onCommand callback is a plain C function pointer (no
    // captures, no member-function pointers) -- bridge via a static
    // instance pointer, same pattern as BTGamepad::sInstance.
    static WCBClientController *sInstance;
    static void onCommandBridge(uint8_t senderID, const char *command);
};
