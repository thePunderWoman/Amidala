# Amidala Firmware — Claude Notes

## Design principles

This firmware is intended to run on many different R2 builds with varying hardware configurations. Keep these principles in mind for all new features, integrations, and refactoring work.

**Modularity and flexibility**
- Features should be independently enable/disable-able via runtime configuration (stored settings) rather than requiring source edits. Reserve `#define` compile-time flags for major component-level choices only — e.g. swapping in a different drive system or audio system.
- Avoid hard-coding assumptions about which hardware is present. New integrations should degrade gracefully when a feature is disabled.
- Prefer small, focused classes and functions with clear responsibilities over monolithic ones.

**Test coverage**
- All new logic should have unit tests in `test/` to prevent regressions.
- Tests run on the native PlatformIO environment (no hardware required) — keep them that way. Do not introduce test dependencies that require Arduino or physical hardware.
- When fixing a bug, add a test that would have caught it.

**Bug fixes and regression test**
- Any time you fix a bug, that bugfix should be covered by a new regression test.

**Restart-required settings**
- Most subsystems are constructed once at boot from `params` and never look at it again — writing a new value via `/api/config` silently does nothing until the device reboots. Before adding a new setting, check whether its consumer actually re-reads `params` live (e.g. every `animate()` tick, or via a live setter called from `processConfig()`) or only at `setup()`/construction time.
- Prefer making a setting apply live over flagging it as needing a restart — it's usually a small addition (add a setter to the subsystem, call it from both `setup()` and the relevant `processConfig()` branch; see `domercaddr`/`domercchan`/`domespeed` in `src/config.cpp` and `include/dome_drive_roboclaw.h` for the pattern). Don't leave a setting silently inert just because flagging it is easier.
- If a setting genuinely can't apply until reboot (e.g. it's baked into an object's constructor), mark its SCHEMA row with `restart:true`:
  ```js
  {key:'wifissid', label:'SSID', type:'text', restart:true},
  ```
  `buildRow()`/`doSave()` in `web/assets/edit.js` pick this up automatically — saving that field shows the shared "restart required" banner with a "Restart Now" button (`POST /api/reboot`), on every page, no extra wiring needed. This also works for one-off `buildRow(s, val)` calls outside a `SCHEMA`/`buildPage()` page (see `servos.html`'s global pulse-limit rows).
  - If a field's "needs restart" status is *stateful* rather than a fixed property of the key (e.g. only true once a subsystem is already running — see `WCBClientController::rebootRequired()` and `connectivity.html`'s WCB panel), don't use `restart:true`; instead compute it server-side and call the shared `_flagRestartRequired()` JS function directly when it's true.

## Git workflow

**All changes must go through pull requests — never push directly to `main`.**

1. Create a feature branch, make your changes, then open a PR.
2. Push branches and create PRs with:
   ```
   git push thePunderWoman <branch>
   gh pr create ...
   ```

## Web UI development

**Never edit `include/web_pages.h` directly** — it is generated from the source files in `web/`. Edits to the header will be overwritten the next time the script runs.

The correct workflow for any UI change:
1. Edit the source files in `web/` (HTML, CSS, JS).
2. Regenerate the header: `python3 scripts/embed_web.py`
3. Preview in a browser: `python3 scripts/web_dev.py` → http://localhost:8080

`web_dev.py` serves `web/` directly and mocks the firmware REST API, so no hardware is needed to iterate on the UI.

## Build verification

CI only runs the native unit tests (`pio test -e native`).
**Always verify the firmware build locally before pushing:**

```
pio run
```

If the firmware build is broken, fix it before opening or merging a PR.

## Running tests

```
pio test -e native
```

Tests live in `test/` and use the native PlatformIO environment (no hardware required).
