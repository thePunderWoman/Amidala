#include "wcb_client_controller.h"

#include <WCB_Client.h>
#include <hcr.h>

#include "monitor_buf.h"
#include "version.h"

WCBClientController *WCBClientController::sInstance = nullptr;

void WCBClientController::onCommandBridge(uint8_t senderID, const char *command) {
    // Runs on the ESP-NOW/WiFi receive task -- do the absolute minimum here
    // (library docs are explicit about this) and defer everything else to
    // poll(), called from the main loop task.
    if (sInstance) sInstance->fRxQueue.push(senderID, command);
}

void WCBClientController::begin(const AmidalaParameters &params, HCRVocalizer &hcr, Print &out) {
    fHcr = &hcr;

    if (!params.wcbenable) return;

    WCBConfigValidation v = WCBConfigValidator::validate(params);
    if (!v.ok) {
        out.print(F("WCB Client enabled but not fully configured ("));
        out.print(v.reason);
        out.println(F(") -- skipping mesh join."));
        return;
    }

    sInstance = this;
    fClient = new WCB_Client(params.wcboct2, params.wcboct3, params.wcbpassword,
                              params.wcbquantity, params.wcbid,
                              &WCBClientController::onCommandBridge);
    // Tell WCB_Client what channel the mesh should be on -- since Amidala's
    // SoftAP always comes up before this (see AmidalaController::setup()),
    // pinning the shared radio to params.wifichannel, this is the exact
    // channel the AP already established. With a SoftAP active WCB_Client
    // never force-moves the radio (the AP owns the channel); it only warns
    // (rate-limited, on begin() and every heartbeat) if the two ever
    // disagree -- turning today's implicit "both happen to default the
    // same way" into an explicit, continuously-verified match. Range is
    // 1-11 (WCB_MESH_CHANNEL); out-of-range values are ignored by the
    // library with just a log line, not a hard error.
    fClient->setMeshChannel(params.wifichannel);
    if (!fClient->begin()) {
        out.println(F("WCB Client: begin() failed -- mesh join unsuccessful."));
        delete fClient;
        fClient = nullptr;
        sInstance = nullptr;
        return;
    }

    // Human-readable device name broadcast for WDP neighbor discovery --
    // separate from wcbid (the required numeric device_id used for MAC
    // addressing, passed to the constructor above). Hardcoded, not a config
    // setting: every Amidala board on a given mesh identifies the same way.
    fClient->setIdentity("Amidala", FIRMWARE_VERSION);

    fHcrTransport = new WCBHcrTransport(fClient);
}

void WCBClientController::poll(const AmidalaParameters &params) {
    // Drain the RX queue unconditionally -- even with monitoring off there's
    // no reason to let messages back up in the (small, fixed-size) buffer.
    // Only actually append to the monitor when wcbenable is on: "the WCB can
    // be used to monitor traffic" regardless of which outbound destination
    // is selected, but there's no point tagging inbound mesh traffic into
    // the ring buffer while the feature itself is off.
    WCBRxMessage msg;
    while (fRxQueue.pop(&msg)) {
        if (params.wcbenable) {
            char line[WCB_RX_CMD_LEN + 16];
            snprintf(line, sizeof(line), "MESH: [%u] %s", msg.senderID, msg.command);
            monAppend(line, 'r');
        }
    }

    if (!fClient) return;
    fClient->update(); // required every loop iteration per the library's own contract

    // outboundserial is safe to apply live (unlike the identity fields,
    // it never touches WCB_Client's construction) -- react to a change
    // by wiring/unwiring HCR's transport to match.
    bool wantMesh = (params.outboundserial == 1);
    if (wantMesh != fHcrTransportActive) {
        if (wantMesh) fHcr->setExternalTransport(fHcrTransport);
        else          fHcr->clearExternalTransport();
        fHcrTransportActive = wantMesh;
    }
}

bool WCBClientController::routeOutbound(const char *cmd, bool wantMesh, uint8_t delim) {
    if (!wantMesh) return false;
    if (!fClient) return false; // not live -- fail-safe fallback to UART0

    // Segment size/count mirror WCB_RX_CMD_LEN (wcb_rx_queue.h) and
    // WCB_Client's own ~199-char single-broadcast-command ceiling -- any
    // one delimited piece longer than that would already be rejected by
    // the library itself, so there's no point sizing larger.
    static const uint8_t kMaxSegments = 4;
    static const uint8_t kSegLen      = 200;
    char segments[kMaxSegments][kSegLen];
    uint8_t n = splitOnDelimiter(cmd, delim, &segments[0][0], kSegLen, kMaxSegments);
    if (n == 0) return false; // empty/all-delimiter input -- nothing sent, fall back to UART0

    bool ok = true;
    for (uint8_t i = 0; i < n; i++)
        ok = fClient->broadcast(segments[i]) && ok;
    return ok;
}

String WCBClientController::statusJson(const AmidalaParameters &params) const {
    bool configured = WCBConfigValidator::validate(params).ok;
    bool running     = (fClient != nullptr);
    uint8_t neighbors = running ? fClient->neighborCount() : 0;
    bool joined       = running && neighbors > 0;
    return buildWCBStatusJson(params.wcbenable, configured, running, joined,
                               neighbors, params.wcbid, params.wcbquantity,
                               fRebootRequired);
}
