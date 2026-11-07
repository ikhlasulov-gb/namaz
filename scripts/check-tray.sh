#!/usr/bin/env bash
# Manual StatusNotifier/tray smoke test on a real KDE session.
#
# Verifies registration, tooltip/metadata, the Mute All toggle and the Quit
# menu action.  Requires a running D-Bus session and StatusNotifierWatcher.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/ui/build/namaz"
HOME_DIR="$(mktemp -d)"
DATA_DIR="$HOME_DIR/.local/share"
mkdir -p "$DATA_DIR/icons/hicolor"
for size in 16 22 24 32 48 64 96 128 192 256 512; do
    mkdir -p "$DATA_DIR/icons/hicolor/${size}x${size}/apps"
    cp "$ROOT/packaging/icons/io.github.ikhlasulov_gb.Namaz-$size.png" \
        "$DATA_DIR/icons/hicolor/${size}x${size}/apps/io.github.ikhlasulov_gb.Namaz.png"
done
mkdir -p "$DATA_DIR/icons/hicolor/scalable/apps"
cp "$ROOT/packaging/icons/io.github.ikhlasulov_gb.Namaz.svg" \
    "$DATA_DIR/icons/hicolor/scalable/apps/io.github.ikhlasulov_gb.Namaz.svg"
export XDG_DATA_HOME="$DATA_DIR"
export XDG_DATA_DIRS="$DATA_DIR:/usr/local/share:/usr/share"
PID=""
cleanup() {
    if [[ -n "$PID" ]] && kill -0 "$PID" 2>/dev/null; then
        kill "$PID" 2>/dev/null || true
    fi
    rm -rf "$HOME_DIR"
}
trap cleanup EXIT

if ! busctl --user introspect org.kde.StatusNotifierWatcher /StatusNotifierWatcher \
        org.kde.StatusNotifierWatcher >/dev/null 2>&1; then
    echo "SKIP no StatusNotifierWatcher in this session"
    exit 0
fi

items() {
    busctl --user get-property org.kde.StatusNotifierWatcher /StatusNotifierWatcher \
        org.kde.StatusNotifierWatcher RegisteredStatusNotifierItems 2>/dev/null \
        | grep -o ':[0-9.]*/StatusNotifierItem' || true
}

menu_id() {
    local label="$1"
    gdbus call --session --dest "$BUS" --object-path /MenuBar \
        --method com.canonical.dbusmenu.GetLayout -- 0 1 "['label']" 2>/dev/null \
        | python3 -c 'import re, sys
data = sys.stdin.read()
for match in re.finditer(r"<\((\d+), \{\x27label\x27: <\x27([^\x27]*)\x27>\}", data):
    if match.group(2) == sys.argv[1]:
        print(match.group(1))
        break' "$label"
}

before="$(items | sort)"
HOME="$HOME_DIR" "$BIN" --tab 0 >/tmp/namaz-tray-check.log 2>&1 &
PID=$!
for _ in $(seq 1 50); do
    after="$(items | sort)"
    new_item="$(comm -13 <(echo "$before") <(echo "$after") | tail -1 || true)"
    [[ -n "$new_item" ]] && break
    sleep 0.2
done

if [[ -z "${new_item:-}" ]]; then
    echo "FAIL tray item was not registered"
    cat /tmp/namaz-tray-check.log 2>/dev/null || true
    exit 1
fi
BUS="${new_item%%/*}"
echo "PASS tray item registered: $new_item"

id="$(busctl --user get-property "$BUS" /StatusNotifierItem \
    org.kde.StatusNotifierItem Id 2>/dev/null || true)"
title="$(busctl --user get-property "$BUS" /StatusNotifierItem \
    org.kde.StatusNotifierItem Title 2>/dev/null || true)"
icon="$(busctl --user get-property "$BUS" /StatusNotifierItem \
    org.kde.StatusNotifierItem IconName 2>/dev/null || true)"
tooltip="$(busctl --user get-property "$BUS" /StatusNotifierItem \
    org.kde.StatusNotifierItem ToolTip 2>/dev/null || true)"
[[ "$id" == 's "Namaz"' ]] || { echo "FAIL unexpected Id: $id"; exit 1; }
[[ "$title" == 's "Namaz"' ]] || { echo "FAIL unexpected Title: $title"; exit 1; }
[[ "$icon" == 's "io.github.ikhlasulov_gb.Namaz"' ]] \
    || { echo "FAIL unexpected IconName: $icon"; exit 1; }
echo "PASS Id/Title/IconName"
echo "INFO ToolTip: $tooltip"

MUTE_ID="$(menu_id "Mute All")"
QUIT_ID="$(menu_id "Quit")"
[[ -n "$MUTE_ID" && -n "$QUIT_ID" ]] \
    || { echo "FAIL could not find Mute All/Quit in the tray menu"; exit 1; }

toggle() {
    gdbus call --session --dest "$BUS" --object-path /MenuBar \
        --method com.canonical.dbusmenu.GetProperty -- "$MUTE_ID" "toggle-state" \
        | sed -E 's/^\(<([0-9]+)>,\)$/\1/'
}
before_toggle="$(toggle)"
gdbus call --session --dest "$BUS" --object-path /MenuBar \
    --method com.canonical.dbusmenu.Event -- "$MUTE_ID" "clicked" "<0>" 0 >/dev/null
sleep 0.5
after_toggle="$(toggle)"
[[ "$before_toggle" != "$after_toggle" ]] \
    || { echo "FAIL Mute All toggle did not change"; exit 1; }
gdbus call --session --dest "$BUS" --object-path /MenuBar \
    --method com.canonical.dbusmenu.Event -- "$MUTE_ID" "clicked" "<0>" 0 >/dev/null
echo "PASS Mute All toggles ($before_toggle -> $after_toggle)"

gdbus call --session --dest "$BUS" --object-path /MenuBar \
    --method com.canonical.dbusmenu.Event -- "$QUIT_ID" "clicked" "<0>" 0 >/dev/null
sleep 1
if kill -0 "$PID" 2>/dev/null; then
    echo "FAIL Quit menu action did not stop the application"
    exit 1
fi
PID=""
echo "PASS Quit menu action stops the application"
