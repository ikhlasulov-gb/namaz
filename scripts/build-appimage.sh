#!/usr/bin/env bash
# Build a portable AppImage with linuxdeploy + the Qt plugin.
#
# Usage: scripts/build-appimage.sh [binary] [core-library]
# Environment:
#   LINUXDEPLOY_CACHE  download cache (default: .cache/linuxdeploy)
#   QMAKE              qmake executable (auto-detected)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="$(tr -d '[:space:]' < "$ROOT/VERSION")"
ARCH="$(uname -m)"
BIN="${1:-$ROOT/ui/build-release/namaz}"
CORE="${2:-$ROOT/ui/build-release/cargo/release/libnamaz_core.so}"
CACHE="${LINUXDEPLOY_CACHE:-$ROOT/.cache/linuxdeploy}"
WORK="$ROOT/build-appimage"
APPDIR="$WORK/AppDir"

if [[ ! -x "$BIN" ]]; then
    echo "binary not found: $BIN" >&2
    echo "build first: cmake -S ui -B ui/build-release -DCMAKE_BUILD_TYPE=Release && cmake --build ui/build-release" >&2
    exit 1
fi
if [[ ! -f "$CORE" ]]; then
    echo "core library not found: $CORE" >&2
    exit 1
fi

if [[ -n "${QMAKE:-}" ]]; then
    :
elif command -v qmake6 >/dev/null 2>&1; then
    QMAKE="$(command -v qmake6)"
elif command -v qmake >/dev/null 2>&1; then
    QMAKE="$(command -v qmake)"
elif [[ -x /usr/lib64/qt6/bin/qmake ]]; then
    QMAKE=/usr/lib64/qt6/bin/qmake
elif [[ -x /usr/lib/qt6/bin/qmake ]]; then
    QMAKE=/usr/lib/qt6/bin/qmake
else
    echo "qmake not found; install Qt 6 development tools" >&2
    exit 1
fi
export QMAKE

case "$ARCH" in
    x86_64) TOOLS_ARCH=x86_64 ;;
    aarch64|arm64) TOOLS_ARCH=aarch64 ;;
    *) echo "unsupported architecture for AppImage: $ARCH" >&2; exit 1 ;;
esac

mkdir -p "$CACHE"
if [[ ! -x "$CACHE/linuxdeploy-$TOOLS_ARCH.AppImage" ]]; then
    curl -fsSL -o "$CACHE/linuxdeploy-$TOOLS_ARCH.AppImage" \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$TOOLS_ARCH.AppImage"
    chmod +x "$CACHE/linuxdeploy-$TOOLS_ARCH.AppImage"
fi
if [[ ! -x "$CACHE/linuxdeploy-plugin-qt-$TOOLS_ARCH.AppImage" ]]; then
    curl -fsSL -o "$CACHE/linuxdeploy-plugin-qt-$TOOLS_ARCH.AppImage" \
        "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-$TOOLS_ARCH.AppImage"
    chmod +x "$CACHE/linuxdeploy-plugin-qt-$TOOLS_ARCH.AppImage"
fi
if [[ ! -x "$CACHE/linuxdeploy-root/squashfs-root/AppRun" ]]; then
    mkdir -p "$CACHE/linuxdeploy-root"
    (cd "$CACHE/linuxdeploy-root" && "$CACHE/linuxdeploy-$TOOLS_ARCH.AppImage" --appimage-extract >/dev/null)
fi
if [[ ! -x "$CACHE/plugin-root/squashfs-root/usr/bin/linuxdeploy-plugin-qt" ]]; then
    mkdir -p "$CACHE/plugin-root"
    (cd "$CACHE/plugin-root" && "$CACHE/linuxdeploy-plugin-qt-$TOOLS_ARCH.AppImage" --appimage-extract >/dev/null)
fi

rm -rf "$WORK"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/lib" \
    "$APPDIR/usr/share/applications" \
    "$APPDIR/usr/share/icons/hicolor/256x256/apps" \
    "$APPDIR/usr/share/namaz/wave"
install -m755 "$BIN" "$APPDIR/usr/bin/namaz"
install -m755 "$CORE" "$APPDIR/usr/lib/libnamaz_core.so"
install -m644 "$ROOT/packaging/io.github.ikhlasulov_gb.Namaz.desktop" \
    "$APPDIR/usr/share/applications/"
install -m644 "$ROOT/packaging/icons/io.github.ikhlasulov_gb.Namaz-256.png" \
    "$APPDIR/usr/share/icons/hicolor/256x256/apps/io.github.ikhlasulov_gb.Namaz.png"
cp "$ROOT"/assets/audio/*.ogg "$APPDIR/usr/share/namaz/wave/"
install -m644 "$ROOT/assets/audio/LICENSE" "$APPDIR/usr/share/namaz/audio.LICENSE"
install -m644 "$ROOT/assets/cities.LICENSE" "$APPDIR/usr/share/namaz/cities.LICENSE"

# linuxdeploy ships a very old strip that cannot read RELR sections produced by
# newer toolchains.  The binaries are already stripped by Cargo/CMake.
export NO_STRIP=1
export PATH="$CACHE/plugin-root/squashfs-root/usr/bin:$CACHE/linuxdeploy-root/squashfs-root/usr/bin:$PATH"

cd "$WORK"
"$CACHE/linuxdeploy-root/squashfs-root/AppRun" \
    --appdir "$APPDIR" \
    --plugin qt \
    --desktop-file "$APPDIR/usr/share/applications/io.github.ikhlasulov_gb.Namaz.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/256x256/apps/io.github.ikhlasulov_gb.Namaz.png" \
    --executable "$APPDIR/usr/bin/namaz" \
    --output appimage

mkdir -p "$ROOT/dist"
OUTPUT="$ROOT/dist/Namaz-${VERSION}-${ARCH}.AppImage"
mv "$WORK/Namaz-${ARCH}.AppImage" "$OUTPUT"
chmod 755 "$OUTPUT"
echo "AppImage: $OUTPUT"
