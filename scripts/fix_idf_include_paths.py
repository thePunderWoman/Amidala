"""
fix_idf_include_paths.py — PlatformIO pre-build extra script for the esp32s3
envs.

espressif32 (pioarduino) 55.03.39's plain `framework = arduino` build (no
`espidf` hybrid) doesn't add every ESP-IDF component include directory that
the framework's own bundled libraries need — unlike a full ESP-IDF/CMake
build, which auto-exposes every enabled component's public headers globally.
Concretely, without these paths:
  - WiFi/AP.cpp fails: esp_wifi_types.h guards the struct definition of
    wifi_sta_list_t (and friends) behind
    `#if __has_include("esp_wifi_types_native.h")` — a silent no-op, not an
    error, if that header can't be found — and it lives in an
    esp_wifi/include/local/ subdirectory that isn't on the include path at
    all, so the type stays incomplete wherever AP.cpp declares one.
  - SD/SD_MMC/FFat fail: they need FatFS's ff.h / esp_vfs_fat.h, whose
    directories also aren't on the include path.
This is a gap in this platform release, not something fixable from a
consuming project's library manifest — add the missing directories
directly. Uses PlatformIO's package/board APIs rather than hardcoded paths
so this works on any machine, not just wherever it was first written.
"""

Import("env")  # noqa: F821 — SCons Import
import os

platform = env.PioPlatform()
mcu = env.BoardConfig().get("build.mcu", "esp32s3")
libs_dir = platform.get_package_dir("framework-arduinoespressif32-libs")

if libs_dir:
    include_root = os.path.join(libs_dir, mcu, "include")
    for rel in (
        os.path.join("esp_wifi", "include"),
        os.path.join("esp_wifi", "include", "local"),
        os.path.join("fatfs", "src"),
        os.path.join("fatfs", "vfs"),
        os.path.join("fatfs", "diskio"),
    ):
        path = os.path.join(include_root, rel)
        if os.path.isdir(path):
            env.Append(CPPPATH=[path])
