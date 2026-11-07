#!/usr/bin/env bash
# Capture the Widgets frontend with the built-in screenshot hook.
#
# Usage:
#   ./scripts/ui-shot.sh [output.png]
#   NAMAZ_UI_TAB=1 ./scripts/ui-shot.sh month.png
#   NAMAZ_UI_TAB=settings ./scripts/ui-shot.sh settings.png
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-/tmp/opencode/namaz-ui.png}"
TAB="${NAMAZ_UI_TAB:-0}"

case "$TAB" in
    settings)
        ARGS=(--settings)
        ;;
    preferences)
        ARGS=(--preferences)
        ;;
    masjid)
        ARGS=(--masjid)
        ;;
    *)
        ARGS=(--tab "$TAB")
        ;;
esac

"$ROOT/ui/build/namaz" "${ARGS[@]}" --screenshot "$OUT"
if [[ -f "$OUT" ]]; then
    echo "wrote $OUT"
else
    echo "screenshot failed" >&2
    exit 1
fi
