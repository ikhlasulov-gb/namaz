#!/usr/bin/env bash
# Install Namaz into the current user's ~/.local prefix.
#
# Usage: scripts/install-user.sh [binary] [core-library]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${1:-$ROOT/ui/build/namaz}"
CORE="${2:-$ROOT/ui/build/cargo/debug/libnamaz_core.so}"
PREFIX="${HOME}/.local"
BIN_DIR="$PREFIX/bin"
APP_LIB_DIR="$PREFIX/lib/namaz"
DATA_DIR="$PREFIX/share/namaz"
ICON_DIR="$PREFIX/share/icons/hicolor"
APP_DIR="$PREFIX/share/applications"
METAINFO_DIR="$PREFIX/share/metainfo"

if [[ ! -x "$BIN" ]]; then
  echo "binary not found: $BIN" >&2
  echo "build first: cmake --build ui/build" >&2
  exit 1
fi
if [[ ! -f "$CORE" ]]; then
  echo "core library not found: $CORE" >&2
  exit 1
fi

install -Dm755 "$BIN" "$APP_LIB_DIR/namaz"
install -Dm755 "$CORE" "$APP_LIB_DIR/libnamaz_core.so"
if command -v chrpath >/dev/null 2>&1; then
  chrpath -r '$ORIGIN' "$APP_LIB_DIR/namaz" >/dev/null 2>&1 || true
fi
install -d "$DATA_DIR/wave"
cp "$ROOT"/assets/audio/*.ogg "$DATA_DIR/wave/"
install -Dm644 "$ROOT/assets/audio/audio.LICENSE" "$DATA_DIR/audio.LICENSE"
install -Dm644 "$ROOT/assets/cities.LICENSE" "$DATA_DIR/cities.LICENSE"
install -Dm644 "$ROOT/packaging/io.github.ikhlasulov_gb.Namaz.desktop" \
  "$APP_DIR/io.github.ikhlasulov_gb.Namaz.desktop"
install -Dm644 "$ROOT/packaging/io.github.ikhlasulov_gb.Namaz.metainfo.xml" \
  "$METAINFO_DIR/io.github.ikhlasulov_gb.Namaz.metainfo.xml"
for size in 16 22 24 32 48 64 96 128 192 256 512; do
  install -Dm644 "$ROOT/packaging/icons/io.github.ikhlasulov_gb.Namaz-$size.png" \
    "$ICON_DIR/${size}x${size}/apps/io.github.ikhlasulov_gb.Namaz.png"
done
install -Dm644 "$ROOT/packaging/icons/io.github.ikhlasulov_gb.Namaz.svg" \
  "$ICON_DIR/scalable/apps/io.github.ikhlasulov_gb.Namaz.svg"
if [[ -f /usr/share/icons/hicolor/index.theme ]]; then
  install -Dm644 /usr/share/icons/hicolor/index.theme "$ICON_DIR/index.theme"
fi

install -d "$BIN_DIR"
cat > "$BIN_DIR/namaz" <<EOF
#!/bin/sh
export LD_LIBRARY_PATH="$APP_LIB_DIR\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
export NAMAZ_WAVE_DIR="$DATA_DIR/wave"
exec "$APP_LIB_DIR/namaz" "\$@"
EOF
chmod 755 "$BIN_DIR/namaz"

# Make Exec work even when ~/.local/bin is not in PATH.
sed -i "s|^Exec=namaz$|Exec=$BIN_DIR/namaz|" \
  "$APP_DIR/io.github.ikhlasulov_gb.Namaz.desktop"

command -v update-desktop-database >/dev/null 2>&1 && \
  update-desktop-database "$APP_DIR" || true
command -v gtk-update-icon-cache >/dev/null 2>&1 && \
  gtk-update-icon-cache -f -t "$ICON_DIR" >/dev/null 2>&1 || true

echo "installed: $BIN_DIR/namaz"
echo "application: $APP_LIB_DIR/namaz"
echo "desktop entry: $APP_DIR/io.github.ikhlasulov_gb.Namaz.desktop"
