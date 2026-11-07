#!/usr/bin/env bash
# Manual autostart smoke test in an isolated HOME.
#
# Uses the real application: enables launch_on_startup, starts the app so it
# writes the XDG autostart file, validates the desktop entry, runs its Exec
# line with --version, then disables the option and verifies removal.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HOME_DIR="$(mktemp -d)"
export HOME="$HOME_DIR"
export QT_QPA_PLATFORM=offscreen
BIN="$ROOT/ui/build/namaz"
LIB="$ROOT/ui/build/cargo/debug/libnamaz_core.so"
DESKTOP="$HOME/.config/autostart/io.github.ikhlasulov_gb.Namaz.desktop"

cleanup() { rm -rf "$HOME_DIR"; }
trap cleanup EXIT

apply() {
    local value="$1"
    HOME="$HOME" python3 - "$LIB" "$value" <<'PY'
import ctypes, json, sys
lib = ctypes.CDLL(sys.argv[1])
lib.namaz_core_new.restype = ctypes.c_void_p
lib.namaz_core_apply_preferences_json.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
lib.namaz_core_free.argtypes = [ctypes.c_void_p]
core = lib.namaz_core_new()
ok = lib.namaz_core_apply_preferences_json(
    core, json.dumps({"launch_on_startup": sys.argv[2] == "true"}).encode())
lib.namaz_core_free(core)
raise SystemExit(0 if ok else 1)
PY
}

apply true
timeout 3 "$BIN" >/dev/null 2>&1 || true
test -f "$DESKTOP"
desktop-file-validate "$DESKTOP"
echo "PASS autostart desktop entry is valid"

exec_line="$(grep '^Exec=' "$DESKTOP" | cut -d= -f2-)"
/bin/sh -c "$exec_line --version" >/dev/null
echo "PASS autostart Exec launches the application"

apply false
timeout 3 "$BIN" >/dev/null 2>&1 || true
test ! -e "$DESKTOP"
echo "PASS autostart entry is removed when disabled"
