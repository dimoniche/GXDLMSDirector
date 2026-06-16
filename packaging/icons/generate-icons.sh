#!/usr/bin/env bash
# Regenerate GXDLMSDirector.icns and GXDLMSDirector.png from the original .ico.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
ICO="$ROOT/GXDLMSDirector.ico"
PNG="$ROOT/GXDLMSDirector.png"
ICNS="$ROOT/GXDLMSDirector.icns"
ICONSET="$ROOT/GXDLMSDirector.iconset"

if [[ ! -f "$ICO" ]]; then
    echo "Missing $ICO" >&2
    exit 1
fi

if [[ "$(uname -s)" != Darwin ]]; then
    echo "icns generation requires macOS (iconutil). Keep committed GXDLMSDirector.icns." >&2
    if command -v magick >/dev/null 2>&1; then
        magick "$ICO" -resize 256x256 "$PNG"
        echo "Updated $PNG"
    fi
    exit 0
fi

sips -s format png "$ICO" --out /tmp/gxdlms-icon-src.png >/dev/null
sips -z 256 256 /tmp/gxdlms-icon-src.png --out "$PNG" >/dev/null

rm -rf "$ICONSET"
mkdir "$ICONSET"
SRC="$PNG"
sips -z 16 16 $SRC --out "$ICONSET/icon_16x16.png" >/dev/null
sips -z 32 32 $SRC --out "$ICONSET/icon_16x16@2x.png" >/dev/null
sips -z 32 32 $SRC --out "$ICONSET/icon_32x32.png" >/dev/null
sips -z 64 64 $SRC --out "$ICONSET/icon_32x32@2x.png" >/dev/null
sips -z 128 128 $SRC --out "$ICONSET/icon_128x128.png" >/dev/null
sips -z 256 256 $SRC --out "$ICONSET/icon_128x128@2x.png" >/dev/null
sips -z 256 256 $SRC --out "$ICONSET/icon_256x256.png" >/dev/null
sips -z 512 512 $SRC --out "$ICONSET/icon_256x256@2x.png" >/dev/null
sips -z 512 512 $SRC --out "$ICONSET/icon_512x512.png" >/dev/null
sips -z 1024 1024 $SRC --out "$ICONSET/icon_512x512@2x.png" >/dev/null
xattr -cr "$ICONSET" 2>/dev/null || true
iconutil -c icns "$ICONSET" -o "$ICNS"
rm -rf "$ICONSET"

echo "Updated $PNG and $ICNS"
