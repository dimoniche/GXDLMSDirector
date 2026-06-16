#!/usr/bin/env bash
# Build GXDLMSDirector.app, bundle Qt with macdeployqt, create .dmg (macOS).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build-release}"
DIST="${DIST_DIR:-$ROOT/dist}"
QT_PREFIX="${CMAKE_PREFIX_PATH:-/opt/homebrew/opt/qt@6}"
VERSION="$(tr -d '[:space:]' < "$ROOT/VERSION")"
DMG_NAME="GXDLMSDirector-${VERSION}-macOS.dmg"
STAGE="$BUILD/dmg-stage"

if [[ "$(uname -s)" != Darwin ]]; then
    echo "build-dmg.sh requires macOS" >&2
    exit 1
fi

MACDEPLOYQT="$QT_PREFIX/bin/macdeployqt"
if [[ ! -x "$MACDEPLOYQT" ]]; then
    echo "macdeployqt not found at $MACDEPLOYQT (set CMAKE_PREFIX_PATH)" >&2
    exit 1
fi

HOMEBREW_PREFIX="${HOMEBREW_PREFIX:-/opt/homebrew}"
DEPLOY_LIBPATHS=("$QT_PREFIX/lib")
for candidate in \
    "$HOMEBREW_PREFIX/lib" \
    "$HOMEBREW_PREFIX/opt/qtbase/lib" \
    "$HOMEBREW_PREFIX/opt/qtsvg/lib" \
    "$HOMEBREW_PREFIX/opt/qtpdf/lib" \
    "$HOMEBREW_PREFIX/opt/qtvirtualkeyboard/lib" \
    "$HOMEBREW_PREFIX/opt/brotli/lib" \
    "$HOMEBREW_PREFIX/opt/webp/lib"; do
    if [[ -d "$candidate" ]]; then
        DEPLOY_LIBPATHS+=("$candidate")
    fi
done

DEPLOY_ARGS=(-always-overwrite -codesign=-)
for libpath in "${DEPLOY_LIBPATHS[@]}"; do
    DEPLOY_ARGS+=(-libpath="$libpath")
done

cmake -S "$ROOT" -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_PREFIX"
cmake --build "$BUILD" -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

APP="$BUILD/GXDLMSDirector.app"
test -d "$APP"

"$MACDEPLOYQT" "$APP" "${DEPLOY_ARGS[@]}" 2>&1 || {
    echo "macdeployqt reported warnings; continuing if the bundle exists" >&2
}

test -x "$APP/Contents/MacOS/GXDLMSDirector"

if command -v codesign >/dev/null 2>&1; then
    codesign --force --deep --sign - "$APP" >/dev/null 2>&1 || true
fi

rm -rf "$STAGE"
mkdir -p "$STAGE" "$DIST"
cp -R "$APP" "$STAGE/"
ln -sf /Applications "$STAGE/Applications"

hdiutil create -volname "GXDLMSDirector ${VERSION}" -srcfolder "$STAGE" -ov -format UDZO \
    "$DIST/$DMG_NAME"

echo "Created $DIST/$DMG_NAME"
echo "App bundle: $APP"
