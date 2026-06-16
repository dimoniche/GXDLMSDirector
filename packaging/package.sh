#!/usr/bin/env bash
# Build a release package for the current OS.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"

case "$(uname -s)" in
    Darwin)
        exec "$ROOT/build-dmg.sh" "$@"
        ;;
    Linux)
        exec "$ROOT/build-appimage.sh" "$@"
        ;;
    *)
        echo "Packaging is supported on macOS (.dmg) and Linux (AppImage) only." >&2
        exit 1
        ;;
esac
