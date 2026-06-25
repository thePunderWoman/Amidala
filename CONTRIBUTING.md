# Contributing to Amidala Firmware

Thanks for your interest in contributing! Here's how to get started.

## Reporting bugs and requesting features

Please open an issue in the [issue tracker](https://github.com/thePunderWoman/Amidala/issues). Include:

- A clear description of the problem or request
- Your hardware setup (PCB revision, module variant, drive system, dome drive)
- Steps to reproduce for bugs, or the use case for feature requests

## Making changes

1. **Fork** the repo and create a feature branch from `main`.
2. Make your changes. See the guidelines below.
3. **Open a pull request** targeting `main` on `thePunderWoman/Amidala`.

All changes go through PRs — direct pushes to `main` are not accepted.

## Development setup

You'll need [PlatformIO](https://platformio.org/) (CLI or IDE extension).

```bash
# Build the firmware
pio run

# Run unit tests (no hardware required)
pio test -e native
```

## Guidelines

**Tests.** All new logic should have unit tests in `test/`. Tests run on the native PlatformIO environment — keep them free of Arduino or hardware dependencies. If you're fixing a bug, add a test that would have caught it.

**Build.** Always verify `pio run` succeeds before opening a PR. CI only runs the native unit tests, so a broken firmware build won't be caught automatically.

**Modularity.** Features should be enable/disable-able via runtime configuration where possible. Avoid hard-coding assumptions about which hardware is present — new integrations should degrade gracefully when a feature is disabled. Reserve `#define` compile-time flags for major component-level choices (drive system, audio system, etc.).

**Scope.** Keep changes focused. A bug fix doesn't need surrounding cleanup; a feature doesn't need to redesign adjacent code. If you spot something unrelated worth fixing, open a separate issue or PR.

**Commit style.** Write commit messages that explain *why*, not just what. Keep the subject line under 72 characters; add a body when context helps.

**Thank you!** We appreciate you giving this project your valuable time and effort.