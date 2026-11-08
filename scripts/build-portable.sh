#!/usr/bin/env bash
# Build a self-contained portable tree and tar.gz archive.
#
# Usage: scripts/build-portable.sh [binary] [core-library]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="$(tr -d '[:space:]' < "$ROOT/VERSION")"
ARCH="$(uname -m)"
DIST="$ROOT/dist/namaz"
BIN="${1:-$ROOT/ui/build/namaz}"
CORE="${2:-$ROOT/ui/build/cargo/debug/libnamaz_core.so}"

if [[ ! -x "$BIN" ]]; then
  echo "binary not found: $BIN" >&2
  echo "build first: cmake --build ui/build" >&2
  exit 1
fi
if [[ ! -f "$CORE" ]]; then
  echo "core library not found: $CORE" >&2
  exit 1
fi

rm -rf "$DIST"
mkdir -p "$DIST/bin" "$DIST/lib" "$DIST/share/namaz/wave" \
  "$DIST/share/applications" "$DIST/share/metainfo"
install -m755 "$BIN" "$DIST/bin/namaz"
install -m755 "$CORE" "$DIST/lib/libnamaz_core.so"
if command -v chrpath >/dev/null 2>&1; then
  chrpath -r '$ORIGIN/../lib' "$DIST/bin/namaz" >/dev/null 2>&1 || true
fi

cp "$ROOT"/assets/audio/*.ogg "$DIST/share/namaz/wave/"
install -m644 "$ROOT/assets/audio/LICENSE" "$DIST/share/namaz/audio.LICENSE"
install -m644 "$ROOT/assets/cities.LICENSE" "$DIST/share/namaz/cities.LICENSE"
install -m644 "$ROOT/packaging/io.github.ikhlasulov_gb.Namaz.desktop" \
  "$DIST/share/applications/"
install -m644 "$ROOT/packaging/io.github.ikhlasulov_gb.Namaz.metainfo.xml" \
  "$DIST/share/metainfo/"
for size in 16 22 24 32 48 64 96 128 192 256 512; do
  install -Dm644 "$ROOT/packaging/icons/io.github.ikhlasulov_gb.Namaz-$size.png" \
    "$DIST/share/icons/hicolor/${size}x${size}/apps/io.github.ikhlasulov_gb.Namaz.png"
done
install -Dm644 "$ROOT/packaging/icons/io.github.ikhlasulov_gb.Namaz.svg" \
  "$DIST/share/icons/hicolor/scalable/apps/io.github.ikhlasulov_gb.Namaz.svg"
if [[ -f /usr/share/icons/hicolor/index.theme ]]; then
  install -Dm644 /usr/share/icons/hicolor/index.theme \
    "$DIST/share/icons/hicolor/index.theme"
fi

cat > "$DIST/run.sh" <<'EOF'
#!/usr/bin/env bash
set -e
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export NAMAZ_WAVE_DIR="$HERE/share/namaz/wave"
export LD_LIBRARY_PATH="$HERE/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export XDG_DATA_DIRS="$HERE/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
exec "$HERE/bin/namaz" "$@"
EOF
chmod +x "$DIST/run.sh"

mkdir -p "$ROOT/dist"
tar -C "$ROOT/dist" -czf "$ROOT/dist/namaz-${VERSION}-linux-${ARCH}.tar.gz" namaz
echo "portable tree: $DIST"
echo "tarball:       $ROOT/dist/namaz-${VERSION}-linux-${ARCH}.tar.gz"
