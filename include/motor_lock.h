// motor_lock.h
// Whether the drive and dome may be re-enabled. Two independent reasons
// keep them off: an e-stop (web UI E-STOP, the OTA upload's automatic one,
// or a ButtonAction::kEstop button) and the "Motors on/off" button action
// (ButtonAction::kMotorsToggle). Either one blocks AmidalaController::
// enableController()/enableDomeController(), so nothing else that
// re-enables the motors -- a remote reconnecting, the lag-based safety stop
// recovering, a gesture ending -- can bring them back while it's set.
//
// Deliberately free of any Arduino/controller dependency so it's unit-testable
// natively, like safety_stop_latch.h.
#pragma once

class MotorLock {
public:
  bool estopActive() const { return fEstop; }
  bool motorsDisabled() const { return fMotorsDisabled; }

  void setEstop(bool active) { fEstop = active; }
  void setMotorsDisabled(bool disabled) { fMotorsDisabled = disabled; }

  bool allowsEnable() const { return !fEstop && !fMotorsDisabled; }

private:
  bool fEstop = false;
  bool fMotorsDisabled = false;
};
