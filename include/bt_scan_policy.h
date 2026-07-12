// bt_scan_policy.h
// Pure decision logic for when BTGamepad should scan/reconnect over BLE.
//
// Deliberately free of any BLE/Arduino dependency so it's unit-testable
// natively (BLEDevice/BLEScan/BLEClient are hardware-only, same constraint
// noted in test/test_bt_gamepad/test_bt_gamepad.cpp).
//
// Rules (same pattern phones use for BLE HID/audio devices):
//   - Never scan for "any" device automatically. Only reconnect to an
//     already-paired target, or scan when the user explicitly asked to pair.
//   - Reconnect attempts to a known target back off exponentially (capped)
//     so a powered-off or dead-battery controller doesn't keep the BLE radio
//     busy indefinitely.
//   - An explicit pairing request is bounded by a timeout -- it will not
//     scan forever looking for a device to pair with.
#pragma once

#include <stdint.h>

class BTScanPolicy {
public:
    static constexpr uint32_t kBaseReconnectIntervalMs = 10000;
    static constexpr uint32_t kMaxReconnectIntervalMs   = 60000;
    static constexpr uint32_t kPairingTimeoutMs         = 30000;

    BTScanPolicy() { reset(); }

    void reset() {
        fReconnectIntervalMs = kBaseReconnectIntervalMs;
        fLastAttemptMs       = 0;
        fHaveAttempted       = false;
        fPairing             = false;
        fPairingStartMs      = 0;
    }

    // Call when the user explicitly asks to discover/pair a new device.
    void beginPairing(uint32_t nowMs) {
        fPairing        = true;
        fPairingStartMs = nowMs;
    }

    // Call once a device is paired (scan found one) or successfully
    // reconnected -- ends pairing mode and resets reconnect backoff.
    void onPaired() {
        fPairing             = false;
        fReconnectIntervalMs = kBaseReconnectIntervalMs;
        fHaveAttempted       = false;
    }

    // Call after a reconnect attempt to a known target failed.
    void onReconnectFailed() {
        uint32_t doubled = fReconnectIntervalMs * 2;
        fReconnectIntervalMs = (doubled > kMaxReconnectIntervalMs) ? kMaxReconnectIntervalMs : doubled;
    }

    bool isPairing() const { return fPairing; }

    // Call every animate() tick while pairing. Returns true if pairing is
    // still within its timeout window (caller should keep/restart the
    // discovery scan); returns false (and ends pairing mode) once the
    // timeout has elapsed.
    bool pairingActive(uint32_t nowMs) {
        if (!fPairing) return false;
        if (nowMs - fPairingStartMs > kPairingTimeoutMs) {
            fPairing = false;
            return false;
        }
        return true;
    }

    // Call every animate() tick when not pairing, not connected, and a
    // target is set. Returns true once enough backoff time has elapsed to
    // attempt a reconnect to the paired target.
    bool reconnectDue(uint32_t nowMs) {
        if (!fHaveAttempted || (nowMs - fLastAttemptMs >= fReconnectIntervalMs)) {
            fHaveAttempted = true;
            fLastAttemptMs = nowMs;
            return true;
        }
        return false;
    }

    uint32_t reconnectIntervalMs() const { return fReconnectIntervalMs; }

private:
    uint32_t fReconnectIntervalMs;
    uint32_t fLastAttemptMs;
    bool     fHaveAttempted;
    bool     fPairing;
    uint32_t fPairingStartMs;
};
