# TODO list

## Infrastructure
- [x] Add WiFi soft-AP support (SSID / password configurable via config.txt)
- [x] DNS server — wildcard redirect so any URL on the AP resolves to the board
- [x] mDNS — advertise `<ssid>.local` for macOS / iOS / Windows 10+ clients
- [x] Web server (WebServer.h) wired into the main animate() loop
- [x] SD card write-back — saving a setting via the web UI updates config.txt
- [x] JSON API infrastructure (`web_api.h`) with unit tests

## Landing page  (`/`)
- [x] Star Wars–themed responsive layout
- [x] Build-info strip: drive, dome, audio, WiFi SSID, free heap — fetched live from `/api/info`
- [x] Navigation grid linking to all config sub-pages and tools
- [x] Firmware version and build date in header
- [x] Emergency stop (E-Stop) button fixed to top-right on every page

## Config sub-pages
- [x] General settings (`/config/general`) — edit-in-place, saves to SD
  - [x] Drive settings (slow mode, channel mixing, autocorrect)
  - [x] Serial settings (baud, delimiter, EOL)
  - [x] I²C address
- [x] Audio (`/config/audio`)
  - [x] Audio hardware display (compile-time, read-only)
  - [x] R2 Sounds volume (volume)
  - [x] Per-channel volume (volumeChA, volumeChB) — HCR only (WAV file channels)
  - [x] Volume wheel assignment (volumewheel, altvolumewheel) — HCR only
  - [x] Startup emote (startupem, startuplvl) — HCR only
  - [x] Ack emote (ackem, acklvl) — HCR only
  - [x] Sound playback settings (startup sound, random sounds, ack sounds, delay range) — moved from General
  - [x] Sound banks list (read-only) — VMusic only
  - [x] HCR/VMusic sections gated by `audiohw` config value
- [x] Dome drive (`/config/dome`)
  - [x] Speed settings (domespeed, domespeedhome, domespeedseek, domespeedmin)
  - [x] Position settings (domehome, domefudge, domeflip, domeimu, domech6)
  - [x] Seek range (domeseekl, domeseekr, domefudge)
  - [x] RoboClaw-specific (domercaddr, domercchan, domercqpps, domefront, domestall) — gated by `domehw === 'roboclaw'`
  - [x] Dome commands (home, calibrate, stop, front, random) — action buttons via `/api/dome`
- [x] Controllers (`/config/controllers`)
  - [x] Button mapping table (b=, lb=, ab=) — 9 buttons × press/long/alt layers
  - [x] Alt button config (altbtn, altdomestick)
  - [x] Mute button config (mutebutton)
  - [x] Gesture mapping table (g=) — add/delete/edit gesture sequences
  - [ ] Special gestures (rnd, ackgest, slowgest, domegest)
- [x] Servos (`/config/servos`)
  - [x] Per-channel min/max/neutral/deadzone/trim/speed/reversed
  - [x] Global pulse limits (minpulse, maxpulse)
- [x] XBee (`/config/xbee`)
  - [x] Drive remote address (xbr)
  - [x] Dome remote address (xbl)
- [x] Serial Commands (`/config/serial-strings`)
  - [x] Numbered list of sstr= entries, editable and reorderable
- [x] RC Radio (`/config/rc-radio`)
  - [x] Channel count (rcchn), deadzone (rcd), jitter (rcj)
  - [x] Joystick calibration (rvrmin, rvrmax, rvlmin, rvlmax, j1adjv, j1adjh)
  - [x] Failsafe timeout (fst)
  - [x] Informational banner clarifying RC-only applicability
- [x] WiFi (`/config/wifi`)
  - [x] SSID (wifiSSID), password (wifiPassword), enable (wifion)

## Droid Control
- [x] Dome (`/droid-control#dome`) — STOP, Go Home, angle presets, relative moves (±45°/±90°), Random/Abs-Stick toggles
- [x] Sequences (`/droid-control#sequences`) — grid of serial command buttons + HCR audio (musing toggle, 4 emotions × 2 levels); `/api/serial` and `/api/hcr` endpoints
- [ ] Gadgets (`/droid-control#gadgets`) — coming soon placeholder; needs gadget configuration page first

## Tools
- [x] Serial monitor (`/monitor`) — polls `/api/monitor`, color-coded TX/RX, pause/clear
- [x] Firmware update (`/update`) — OTA flash over WiFi, progress bar, version confirmation after restart
- [ ] Safety (`/safety`) - safety settings

## Design / UX
- [x] Responsive grid layout (works on phone)
- [x] Touch-friendly tap targets
- [x] Star Wars color theme (black + #ffe81f gold)
- [x] Edit-in-place pattern: display → edit icon → input + save/cancel
- [x] Cancel restores field to original value without saving
- [x] Consistent page header with back-navigation on all sub-pages
- [x] Toast / status feedback after save (replaces browser alert)

## Testing
- [x] Unit tests for JSON API builders (`buildGeneralConfigJson`, `buildInfoJson`)
- [x] Unit tests for HTML page content (API endpoint URLs, nav links, SCHEMA keys)
- [x] Unit tests for `updateConfigFile` SD write-back logic (SD mock extended with write support)
