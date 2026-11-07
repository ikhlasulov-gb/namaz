#!/usr/bin/env bash
# Full check: Rust/Qt tests, native layout invariants and PDF export.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CONTAINER="${NAMAZ_CONTAINER:-salaat-dev}"
cd "$ROOT"

BUILD_COMMANDS='cargo test -p prayer-core -p namaz-core && \
     cmake -S ui -B ui/build -DCMAKE_BUILD_TYPE=Debug && \
     cmake --build ui/build -j4 && \
     QT_QPA_PLATFORM=offscreen ctest --test-dir ui/build --output-on-failure'

if command -v podman >/dev/null 2>&1 \
    && podman container exists "$CONTAINER" >/dev/null 2>&1; then
    podman exec -u "$(id -u)" -w "$ROOT" "$CONTAINER" bash -lc "$BUILD_COMMANDS"
else
    bash -lc "$BUILD_COMMANDS"
fi

OUT="${1:-/tmp/namaz-ui-geometry.json}"
PREFS="${2:-/tmp/namaz-ui-preferences.json}"
SETTINGS="${3:-/tmp/namaz-ui-settings.json}"

# Isolate the checks from the user's real configuration and require no display.
CHECK_HOME="$(mktemp -d)"
trap 'rm -rf "$CHECK_HOME"' EXIT
export HOME="$CHECK_HOME"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"

./ui/build/namaz --window-size 920x660 --dump-geometry "$OUT"
python3 scripts/check_native_layout.py --actual "$OUT"

NARROW="${OUT%.json}-narrow.json"
./ui/build/namaz --window-size 640x1100 --dump-geometry "$NARROW"
python3 scripts/check_native_layout.py --actual "$NARROW" --narrow

./ui/build/namaz --preferences --dump-geometry "$PREFS"
python3 scripts/check_dialog_layout.py --actual "$PREFS"

./ui/build/namaz --settings --dump-geometry "$SETTINGS"
python3 scripts/check_dialog_layout.py --actual "$SETTINGS"

./ui/build/namaz --print-pdf "${OUT%.json}.pdf" --print-calendar 0
./ui/build/namaz --print-pdf "${OUT%.json}-hijri.pdf" --print-calendar 1
test -s "${OUT%.json}.pdf"
test -s "${OUT%.json}-hijri.pdf"
