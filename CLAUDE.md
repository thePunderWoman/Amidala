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
- CI enforces a minimum aggregate line coverage (`scripts/coverage.sh --fail-under 80`, scoped to `src/`/`include/`) — see [Code coverage](#code-coverage). Run it locally before opening a PR that adds untested logic.

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
3. Once a PR lands, delete the local feature branch. This repo always squash or rebase merges — `main` never gets a merge commit for the PR, so `git branch -d` (and `--merged` checks) won't recognize the branch as merged even though its content has landed. Confirm via `git log --oneline` (look for the PR's commit/title on `main`) or `gh pr view <branch> --json state`, then use `git branch -D <branch>` to remove it.

## Web UI development

**Never edit `include/web_pages.h` directly** — it is generated from the source files in `web/`. Edits to the header will be overwritten the next time the script runs.

The correct workflow for any UI change:
1. Edit the source files in `web/` (HTML, CSS, JS).
2. Regenerate the header: `python3 scripts/embed_web.py`
3. Preview in a browser: `python3 scripts/web_dev.py` → http://localhost:8080

`web_dev.py` serves `web/` directly and mocks the firmware REST API, so no hardware is needed to iterate on the UI.

## Build verification

CI (`ci.yml`) runs the native unit tests with coverage instrumentation (`scripts/coverage.sh --fail-under 80`, see [Code coverage](#code-coverage)) and compiles the real firmware for both board environments (default `DRIVE_SYSTEM`/`DOME_DRIVE` config only — not the full release matrix) on every push/PR. **Still always verify locally before pushing too** — CI catching it is a safety net, not a substitute for finding out before you open the PR:

```
pio run
```

If the firmware build is broken, fix it before opening or merging a PR.

The full 30-combination `DRIVE_SYSTEM` x `DOME_DRIVE` x board matrix (`build-firmware-matrix.yml`) only runs on release/manual dispatch, not every push — it's too expensive for routine CI. `HumanCyborgRelationsAPI` is a normal `lib_deps` URL entry in `esp32s3_base` (pinned to the same tag as `env:native`'s own entry) — no manual install step, no local `lib/` copy, and nothing to keep in sync by hand. This replaced an older setup where the `esp32s3-*` environments relied on a manually-cloned local `lib/` copy that had to be kept in sync by hand with `env:native`'s `lib_deps` pin; a mismatch there previously shipped all the way to a published release (HCR pinned to 1.0.3 in `platformio.ini` but the release workflow still cloning 1.0.2) before anyone noticed, which is why `ci.yml`'s `build-firmware` job exists as a routine real-firmware sanity check. If you ever reintroduce a local `lib/` copy of a dependency that also has a `lib_deps` entry, remember PlatformIO's LDF prefers the local copy silently — a stale clone left in `lib/` would keep building against the wrong version without any error.

## Running tests

```
pio test -e native
```

Tests live in `test/` and use the native PlatformIO environment (no hardware required).

## Code coverage

```
scripts/coverage.sh --fail-under 80
```

Runs the suite under `env:native-coverage` (`env:native` plus gcov instrumentation, see `scripts/coverage_flags.py`) and generates an HTML/XML report in `coverage/`, scoped to `src/` and `include/` (excludes `test/`, Unity, and vendored libs). Requires `gcovr` (`pip install gcovr`).

CI runs this same script instead of a plain test pass and fails the build if aggregate line coverage drops below 80%. That threshold exists to catch regressions, not to be a target — ratchet it up as coverage improves rather than treating it as a permanent ceiling.
