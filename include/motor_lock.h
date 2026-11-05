// motor_lock.h
// Whether the foot drive and the dome may be re-enabled. Three independent
// reasons keep them off:
//   - an e-stop (web UI E-STOP, the OTA upload's automatic one, or a
//     ButtonAction::kEstop button) -- blocks both;
//   - the foot motors being switched off (ButtonAction::kDriveMotorsToggle,
//     or ButtonAction::kMotorsToggle) -- blocks the drive only;
//   - the dome motor being switched off (ButtonAction::kDomeMotorToggle, or
//     ButtonAction::kMotorsToggle) -- blocks the dome only.
// These gate AmidalaController::enableController()/enableDomeController(),
// so nothing else that re-enables the motors -- a remote reconnecting, the
// lag-based safety stop recovering, a gesture ending -- can bring them back
// while one is set.
//
// Deliberately free of any Arduino/controller dependency so it's unit-testable
// natively, like safety_stop_latch.h.
#pragma once

class MotorLock {
public:
  bool estopActive() const { return fEstop; }
  bool driveMotorsOff() const { return fDriveOff; }
  bool domeMotorOff() const { return fDomeOff; }
  // Either one switched off. This is what Snips controllers' status
  // screens report as "motors disabled".
  bool anyMotorsOff() const { return fDriveOff || fDomeOff; }

  void setEstop(bool active) { fEstop = active; }
  void setDriveMotorsOff(bool off) { fDriveOff = off; }
  void setDomeMotorOff(bool off) { fDomeOff = off; }

  // What the combined "Motors On/Off" toggle should set both to: back on
  // only when both are off, otherwise off. A mixed state (one already off)
  // goes all off, so pressing it never starts a motor you meant to stop.
  bool nextAllMotorsOff() const { return !(fDriveOff && fDomeOff); }

  bool allowsDriveEnable() const { return !fEstop && !fDriveOff; }
  bool allowsDomeEnable() const { return !fEstop && !fDomeOff; }

private:
  bool fEstop = false;
  bool fDriveOff = false;
  bool fDomeOff = false;
};
