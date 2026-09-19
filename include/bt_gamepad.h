#pragma once

#include "ReelTwo.h"
#include "core/SetupEvent.h"
#include "core/AnimatedEvent.h"
#include "JoystickController.h"
#include "bt_scan_policy.h"
#include "button_dispatch.h"

// Maximum number of devices returned by a BLE scan.
#define BT_SCAN_MAX_RESULTS 10

struct BTScanResult {
    char addr[18];
    char name[32];
    int  rssi;
};

class AmidalaController;

// BLE HID gamepad that presents as a JoystickController.
//
// Reconnects only to an already-paired device (btaddr); never scans for an
// unspecified HID device on its own. Pairing with a new device only happens
// via an explicit requestPairing() call (see the web UI's Connectivity page).
//
// Left analog stick → state.analog.stick.lx / .ly   (drive)
// Right analog stick → state.analog.stick.rx / .ry  (dome via alt stick)
// Face buttons (A/B/X/Y or ×/○/□/△) → button.cross/circle/square/triangle
// Shoulder (LB/RB, L1/R1) → button.l1 / button.r1
// Trigger (LT/RT, L2/R2) → button.l2 / button.r2
// D-pad → button.up/down/left/right
// Start/Menu → button.start   Select/Back → button.select
//
// Buttons: face buttons + L3 (triangle/circle/cross/square/l3) are dispatched
// through AmidalaController's drive-side button slots 1-5, and bumpers/
// triggers (l1/l2/r1/r2) through slots 10-13 -- the same slots XBee's
// DriveController uses for 1-5 (see src/drive_controllers.cpp), so B[]/LB[]/
// AB[]/DB[]/altbtn/mutebutton configured there apply identically whether an
// XBee drive remote or this gamepad triggers them. Slots 6-9 are reserved for
// a future dome-stick feature shared with XBee's own numbering and are
// intentionally skipped here. Triggers have no separate digital press bit on
// this HID report -- l2/r2 are "pressed" once their analog travel crosses
// kTriggerPressThreshold (see _dispatchButtons()).
//
// R3 (right stick click) starts/ends gesture drawing exactly like a physical
// dome remote's stick click (BT's right stick drives dome, see
// controller.cpp's setAltDomeStick(&gBTGamepad)) -- it feeds
// DomeController::feedGestureInput() directly rather than being a
// configurable ButtonAction, matching how the real dome remote's own stick
// click is reserved for gesture input and can't be assigned elsewhere. It
// does not support folding face-button taps into the stroke the way a
// physical dome remote can, since BT has only one shared set of face buttons
// (already independently dispatched as drive-side macros) rather than a
// separate dome-side set to redirect.
//
// D-pad, Start/Select/PS, and R3's regular macro use remain unwired (issue
// #203 follow-up).
class BTGamepad : public JoystickController, public SetupEvent, public AnimatedEvent
{
public:
    BTGamepad();

    // --- Configuration -------------------------------------------------------

    // Set target device MAC (AA:BB:CC:DD:EE:FF).  Empty string = auto (first HID).
    void setTargetAddr(const char* addr);

    // Wire up the button-dispatch target. Called once from AmidalaController's
    // setup(), mirroring DriveController(AmidalaController*) in xbee_remote.h --
    // gBTGamepad is a global constructed before AmidalaController exists, so
    // this can't be a constructor argument.
    void setDriver(AmidalaController* driver) { fDriver = driver; }

    // --- Scanning ------------------------------------------------------------

    // Non-blocking: start a 5-second passive scan.  Results are available via
    // getScanResults() after scanComplete() returns true.  Only called
    // internally (during an explicit pairing request) or directly by the web
    // UI's "scan for devices" action -- never triggered automatically for an
    // unpaired gamepad, see requestPairing().
    void startScan();
    bool isScanRunning() const { return fScanning; }
    bool scanComplete()  const { return fScanDone; }
    int  getScanResultCount() const { return fScanResultCount; }
    const BTScanResult* getScanResults() const { return fScanResults; }

    // --- Pairing -------------------------------------------------------------

    // Explicit, user-initiated discovery: scan for any HID device for up to
    // BTScanPolicy::kPairingTimeoutMs, connecting to (and persisting as the
    // new target) the first one found. This is the ONLY path that scans for
    // an unspecified device -- animate() never does this on its own.
    void requestPairing();

    // Persist a new target address and immediately attempt to connect.
    void pairWith(const char* addr);

    // Clear pairing — will scan and connect to the first HID device seen.
    void forget();

    // --- Runtime enable/disable ----------------------------------------------

    // Stop scanning/connecting and force isConnected() false immediately, so
    // drive/dome stop trusting BT stick input the instant the feature is
    // turned off — regardless of whether the underlying BLE link is still up.
    void disable();

    // --- Status --------------------------------------------------------------

    const char* connectedAddr() const { return fConnectedAddr; }

    // --- Framework -----------------------------------------------------------

    virtual void setup()   override;
    virtual void animate() override;

    // Internal: called from BLE task/callback context (must be public).
    void _onConnected(const char* addr);
    void _onDisconnected();
    void _onReport(const uint8_t* data, size_t len);
    void _onScanResult(const char* addr, const char* name, int rssi);
    void _onScanDone();

private:
    char fTargetAddr[18];     // desired device addr ("" = any)
    char fConnectedAddr[18];  // addr of currently connected device
    bool fScanning;
    bool fScanDone;
    bool fDoConnect;          // set from scan callback, consumed in animate()
    bool fConnecting;

    int          fScanResultCount;
    BTScanResult fScanResults[BT_SCAN_MAX_RESULTS];

    BTScanPolicy fScanPolicy;

    AmidalaController* fDriver;
    struct {
        ButtonLongPress l3, triangle, circle, cross, square, l1, r1, l2, r2;
    } fLongPress;

    // Analog trigger travel (0-255) at or above which l2/r2 count as
    // "pressed" for dispatch purposes -- half pull, chosen since there's no
    // existing digital press bit or threshold convention anywhere in this
    // codebase or the vendored Reeltwo library to reuse.
    static constexpr uint8_t kTriggerPressThreshold = 128;

    void _attemptConnect();
    void _parseReport(const uint8_t* data, size_t len);
    void _dispatchButtons(const State& prev);
};

// Global instance — declared here, defined in bt_gamepad.cpp.
extern BTGamepad gBTGamepad;
