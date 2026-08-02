#!/usr/bin/env bash
# coverage.sh — run the native unit test suite instrumented for gcov and
# generate a line/function/branch coverage report scoped to src/ and
# include/ (test/ itself, Unity, and vendored libraries are excluded).
#
# Usage:
#   scripts/coverage.sh                  # HTML report + terminal summary in coverage/
#   scripts/coverage.sh --fail-under 80  # also exit non-zero if line coverage drops below 80%
#
# Requires gcovr (pip install gcovr). Uses the env:native-coverage
# PlatformIO environment (see platformio.ini / scripts/coverage_flags.py),
# which is env:native plus --coverage instrumentation.
set -euo pipefail

cd "$(dirname "$0")/.."

fail_under=""
if [[ "${1:-}" == "--fail-under" ]]; then
    fail_under="$2"
fi

if ! command -v gcovr >/dev/null 2>&1; then
    echo "gcovr not found — install with: pip install gcovr" >&2
    exit 1
fi

rm -rf .pio/build/native-coverage coverage
pio test -e native-coverage

mkdir -p coverage
gcovr_args=(
    --root .
    --object-directory .pio/build/native-coverage
    --filter 'src/'
    --filter 'include/'
    --print-summary
    --html-details coverage/index.html
    --xml coverage/coverage.xml
)
if [[ -n "$fail_under" ]]; then
    gcovr_args+=(--fail-under-line "$fail_under")
fi

gcovr "${gcovr_args[@]}"
echo "HTML report: coverage/index.html"
