"""
coverage_flags.py — PlatformIO pre-build extra script for env:native-coverage.

Adds gcov instrumentation flags to both the compile AND link steps. Doing
this via `build_flags` in platformio.ini isn't enough: SCons's flag parser
(Environment.ParseFlags) routes unrecognized flags like `--coverage` into
CCFLAGS only, never LINKFLAGS (see scons-local/SCons/Environment.py), so the
test binaries would compile instrumented but fail to link against libgcov.
Appending directly to both construction variables here sidesteps that.
"""

Import("env")  # noqa: F821 — SCons Import

env.Append(
    CCFLAGS=["--coverage"],
    LINKFLAGS=["--coverage"],
)
