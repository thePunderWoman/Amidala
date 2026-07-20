// safety_stop_latch.h
// Tracks whether DriveController/DomeController's lag-based safety stop
// (see notify() in src/drive_controllers.cpp) is currently tripped, so the
// caller knows whether it owes a re-enable once packets resume.
//
// Bug this fixes: a packet gap just over notify()'s own 500ms threshold but
// under the real failsafe timeout (params.fst, 1000-3000ms) tripped
// emergencyStop()/domeEmergencyStop() (which disables the drive/dome output)
// but never re-enabled it -- only a full failsafe/reconnect cycle
// (XBeePocketRemote::update() -> onConnect() -> enableController()) did that.
// So a brief drop left the drive silently disabled -- stick input looked
// normal (buttons still worked) -- until the remote was fully power-cycled.
//
// Deliberately free of any Arduino/controller dependency so it's unit-testable
// natively -- AmidalaController itself can't be constructed in the native
// test environment (see test/test_xbee_remote/test_xbee_remote.cpp's
// comment on why DriveController/DomeController::notify() are stubbed there).
#pragma once

class SafetyStopLatch {
public:
    SafetyStopLatch() : fTripped(false) {}

    // Call whenever the lag-based safety stop fires.
    void trip() { fTripped = true; }

    // Call once packets are flowing normally again. Returns true (and
    // clears the latch) the first time this runs after trip() -- signals
    // the caller owes a re-enable. Returns false on every other call so the
    // caller doesn't re-enable every single tick.
    bool recover() {
        if (!fTripped) return false;
        fTripped = false;
        return true;
    }

    bool tripped() const { return fTripped; }

private:
    bool fTripped;
};
