#pragma once
#ifdef USE_BT_CONTROLLER

#include "ReelTwo.h"
#include "JoystickController.h"

// Maximum number of devices returned by a BLE scan.
#define BT_SCAN_MAX_RESULTS 10

struct BTScanResult {
    char addr[18];
    char name[32];
    int  rssi;
};

// BLE HID gamepad that presents as a JoystickController.
//
// Connects to any BLE HID device (gamepad, joystick).  When btaddr is empty,
// pairs with the first advertising HID device found during a scan.
//
// Left analog stick → state.analog.stick.lx / .ly   (drive)
// Right analog stick → state.analog.stick.rx / .ry  (dome via alt stick)
// Face buttons (A/B/X/Y or ×/○/□/△) → button.cross/circle/square/triangle
// Shoulder (LB/RB, L1/R1) → button.l1 / button.r1
// Trigger (LT/RT, L2/R2) → button.l2 / button.r2
// D-pad → button.up/down/left/right
// Start/Menu → button.start   Select/Back → button.select
class BTGamepad : public JoystickController, public SetupEvent, public AnimatedEvent
{
public:
    BTGamepad();

    // --- Configuration -------------------------------------------------------

    // Set target device MAC (AA:BB:CC:DD:EE:FF).  Empty string = auto (first HID).
    void setTargetAddr(const char* addr);

    // --- Scanning ------------------------------------------------------------

    // Non-blocking: start a 5-second passive scan.  Results are available via
    // getScanResults() after scanComplete() returns true.
    void startScan();
    bool isScanRunning() const { return fScanning; }
    bool scanComplete()  const { return fScanDone; }
    int  getScanResultCount() const { return fScanResultCount; }
    const BTScanResult* getScanResults() const { return fScanResults; }

    // --- Pairing -------------------------------------------------------------

    // Persist a new target address and immediately attempt to connect.
    void pairWith(const char* addr);

    // Clear pairing — will scan and connect to the first HID device seen.
    void forget();

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

    uint32_t fLastConnectAttemptMs;

    void _attemptConnect();
    void _parseReport(const uint8_t* data, size_t len);
};

// Global instance — declared here, defined in bt_gamepad.cpp.
extern BTGamepad gBTGamepad;

#endif // USE_BT_CONTROLLER
