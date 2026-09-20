// button_names.h
// Human-readable button labels for the serial monitor/console log, so
// "Processing Button 7" reads the same way the Controllers page
// (web/config/controllers.html) labels that button -- e.g. "Left Controller
// Trigger Left (7)" -- instead of a bare number that only means something
// once you know which controllertype is active (issue #227).
//
// The tables below MUST stay in sync with BTN_NAMES / SNIPS_BTN_NAMES /
// BT_BTN_NAMES and the right/left button groupings in controllers.html's
// renderButtons(); test_button_names covers the mapping.
//
// Depends only on plain integer types and params.h's CONTROLLER_TYPE_*
// defines -- no Arduino dependency, so it's unit-testable natively.

#pragma once

#include <stdint.h>
#include <stdio.h>
#include "params.h"

namespace ButtonNames {

// XBee pocket remote (also the fallback for RC, matching the web UI, which
// uses these names for any controllertype that isn't Snips/Bluetooth).
// Buttons 1-5 are the Right Controller, 6-9 the Left Controller.
inline const char *xbeeName(unsigned num, bool &right) {
  static const char *const kNames[] = {
      nullptr,
      "Trigger Right", "Trigger Left", "Top", "Bottom", "Stick Press",
      "Trigger Right", "Trigger Left", "Top", "Bottom"};
  if (num < 1 || num > 9) return nullptr;
  right = num <= 5;
  return kNames[num];
}

// Snips Controllers -- numbering per SnipsRemote::buttonNumberFor()
// (include/snips_remote.h): Right is 1-8 plus 21-24, Left is 9-20.
inline const char *snipsName(unsigned num, bool &right) {
  static const char *const kNames[] = {
      nullptr,
      "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Macro 5", "Macro 6", "Bumper", "Stick Click",
      "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Macro 5", "Macro 6", "Bumper", "Stick Click",
      "Left Up", "Left Down", "Right Up", "Right Down",
      "Left Up", "Left Down", "Right Up", "Right Down"};
  if (num < 1 || num > 24) return nullptr;
  right = num <= 8 || num >= 21;
  return kNames[num];
}

// Bluetooth gamepad -- one physical controller, so no Right/Left side.
// Only the configurable slots are named; anything else (e.g. 6-9, reserved
// for a future dome-stick feature) returns nullptr.
inline const char *btName(unsigned num) {
  switch (num) {
    case 1:  return "A / Cross";
    case 2:  return "B / Circle";
    case 3:  return "X / Square";
    case 4:  return "Y / Triangle";
    case 5:  return "Left Stick Click (L3)";
    case 10: return "Left Bumper";
    case 11: return "Left Trigger";
    case 12: return "Right Bumper";
    case 13: return "Right Trigger";
    default: return nullptr;
  }
}

// Writes the label for button `num` under `controllerType` into `out`, as
// "<Right|Left> Controller <name> (<num>)" ("Snips Controller" for Snips,
// no side for Bluetooth). Buttons the Controllers page has no name for fall
// back to "Button <num>". Returns `out`.
inline const char *label(uint8_t controllerType, unsigned num, char *out, size_t len) {
  bool right = true;
  const char *name = nullptr;
  const char *family = "Controller";
  bool sided = true;
  switch (controllerType) {
    case CONTROLLER_TYPE_SNIPS:
      name = snipsName(num, right);
      family = "Snips Controller";
      break;
    case CONTROLLER_TYPE_BLUETOOTH:
      name = btName(num);
      sided = false;
      break;
    default:
      name = xbeeName(num, right);
      break;
  }
  if (!name)
    snprintf(out, len, "Button %u", num);
  else if (sided)
    snprintf(out, len, "%s %s %s (%u)", right ? "Right" : "Left", family, name, num);
  else
    snprintf(out, len, "%s (%u)", name, num);
  return out;
}

}  // namespace ButtonNames
