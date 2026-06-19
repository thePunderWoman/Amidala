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
- [x] Build-info strip: foot drive, dome drive, audio system, WiFi SSID — fetched live from `/api/info`
- [x] Navigation grid linking to all config sub-pages and tools
- [x] Firmware version and build date in header

## Config sub-pages
- [x] General settings (`/config/general`) — edit-in-place, saves to SD
  - [x] Sound settings (volume, startup sound, random sounds, ack sounds, delay range)
  - [x] Drive settings (slow mode, channel mixing, autocorrect)
  - [x] Serial settings (baud, delimiter, EOL)
  - [x] I²C address
- [ ] Audio (`/config/audio`)
  - [ ] Audio hardware display (compile-time, read-only)
  - [ ] Per-channel volume (volumeChA, volumeChB)
  - [ ] Volume wheel assignment (volumewheel, altvolumewheel)
  - [ ] Startup emote (startupem, startuplvl)
  - [ ] Ack emote (ackem, acklvl)
  - [ ] Sound banks list
- [ ] Dome drive (`/config/dome`)
  - [ ] Dome drive type display (compile-time, read-only)
  - [ ] Speed settings (domespeed, domespeedhome, domespeedseek, domespeedmin)
  - [ ] Position settings (domehome, domefudge, domemode, domeflip)
  - [ ] Seek range (domeseekmin, domeseekmax, domeseekl, domeseekr)
  - [ ] RoboClaw-specific (domercaddr, domercchan, domercqpps, domefront, domestall)
  - [ ] Dome commands (home, calibrate, stop, front, random toggle)
- [ ] Buttons & Gestures (`/config/buttons`)
  - [ ] Button mapping table (b=, lb=, ab=)
  - [ ] Alt button config (altbtn, altdomestick)
  - [ ] Mute button config (mutebutton)
  - [ ] Gesture mapping table (g=)
  - [ ] Special gestures (rnd, ackgest, slowgest, domegest)
- [ ] Servos (`/config/servos`)
  - [ ] Per-channel min/max/neutral/deadzone/trim/speed/reversed
  - [ ] Global pulse limits (minpulse, maxpulse)
- [ ] XBee (`/config/xbee`)
  - [ ] Drive remote address (xbr)
  - [ ] Dome remote address (xbl)
- [ ] Serial strings (`/config/serial-strings`)
  - [ ] Numbered list of sstr= entries, editable and reorderable
- [ ] RC Radio (`/config/rc-radio`)
  - [ ] Channel count (rcchn), deadzone (rcd), jitter (rcj)
  - [ ] Joystick calibration (rvrmin, rvrmax, rvlmin, rvlmax, j1adjv, j1adjh)
  - [ ] Failsafe timeout (fst)
- [ ] WiFi (`/config/wifi`)
  - [ ] SSID (wifiSSID), password (wifiPassword), enable (wifion)
  - [ ] Show current IP address

## Tools
- [ ] Sequences (`/sequences`) — mobile-friendly touch-triggered list of serial strings
- [ ] Serial monitor (`/monitor`) — WebSocket or SSE stream of serial output
- [ ] Firmware update (`/update`) — OTA flash over WiFi

## Design / UX
- [x] Responsive grid layout (works on phone)
- [x] Touch-friendly tap targets
- [x] Star Wars color theme (black + #ffe81f gold)
- [x] Edit-in-place pattern: display → edit icon → input + save/cancel
- [ ] Consistent page header with back-navigation on all sub-pages (only general done)
- [ ] Toast / status feedback after save (currently uses default browser alert on error)

## Testing
- [x] Unit tests for JSON API builders (`buildGeneralConfigJson`, `buildInfoJson`)
- [x] Unit tests for HTML page content (API endpoint URLs, nav links, SCHEMA keys)
- [ ] Unit tests for `updateConfigFile` SD write-back logic (needs SD mock extension)
