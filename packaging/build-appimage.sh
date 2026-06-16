#!/usr/bin/env bash
# Build GXDLMSDirector and bundle as AppImage (Linux).
# Requires: linuxdeploy + linuxdeploy-plugin-qt on PATH.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build-appimage}"
DIST="${DIST_DIR:-$ROOT/dist}"
APPDIR="$BUILD/AppDir"
VERSION="$(tr -d '[:space:]' < "$ROOT/VERSION")"
APPIMAGE_NAME="GXDLMSDirector-${VERSION}-x86_64.AppImage"

if [[ "$(uname -s)" != Linux ]]; then
    echo "build-appimage.sh requires Linux" >&2
    exit 1
fi

for tool in linuxdeploy linuxdeploy-plugin-qt; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Missing $tool on PATH" >&2
        exit 1
    fi
done

cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" -j"$(nproc)"

rm -rf "$APPDIR"
mkdir -p "$DIST"
DESTDIR="$APPDIR" cmake --install "$BUILD" --prefix /usr

EXEC="$APPDIR/usr/bin/GXDLMSDirector"
DESKTOP="$APPDIR/usr/share/applications/GXDLMSDirector.desktop"
test -x "$EXEC"
test -f "$DESKTOP"

linuxdeploy --appdir "$APPDIR" \
    --executable "$EXEC" \
    --desktop-file "$DESKTOP" \
    --plugin qt \
    --output appimage

shopt -s nullglob
for candidate in "$BUILD"/GXDLMSDirector*.AppImage "$BUILD"/*.AppImage; do
    mv -f "$candidate" "$DIST/$APPIMAGE_NAME"
    echo "Created $DIST/$APPIMAGE_NAME"
    exit 0
done

echo "linuxdeploy did not produce an AppImage in $BUILD" >&2
exit 1
