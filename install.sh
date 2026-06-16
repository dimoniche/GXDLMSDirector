#!/usr/bin/env bash
# Install GXDLMSDirector from a local build or package in dist/.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
VERSION="$(tr -d '[:space:]' < "$ROOT/VERSION")"

usage() {
    cat <<EOF
GXDLMSDirector ${VERSION}

Usage:
  $0                 install from default build output
  $0 /path/to/app    install from explicit .app (macOS) or AppImage (Linux)

macOS default:  build-release/GXDLMSDirector.app or dist/*.dmg (mount + copy)
Linux default:  dist/GXDLMSDirector-${VERSION}-x86_64.AppImage -> ~/.local/bin
EOF
}

install_macos() {
    local src="${1:-}"
    local tmp_mount=""

    if [[ -z "$src" ]]; then
        if [[ -d "$ROOT/build-release/GXDLMSDirector.app" ]]; then
            src="$ROOT/build-release/GXDLMSDirector.app"
        else
            local dmg
            dmg="$(ls -1 "$ROOT/dist"/GXDLMSDirector-"${VERSION}"-macOS.dmg 2>/dev/null | head -1 || true)"
            if [[ -z "$dmg" ]]; then
                echo "Build the app first: ./packaging/build-dmg.sh" >&2
                exit 1
            fi
            tmp_mount="$(hdiutil attach "$dmg" -nobrowse -quiet | awk '/\/Volumes\// {print $3; exit}')"
            src="$tmp_mount/GXDLMSDirector.app"
        fi
    fi

    if [[ ! -d "$src" ]]; then
        echo "App bundle not found: $src" >&2
        exit 1
    fi

    echo "Installing GXDLMSDirector ${VERSION} to /Applications ..."
    rm -rf "/Applications/GXDLMSDirector.app"
    cp -R "$src" /Applications/

    if [[ -n "$tmp_mount" ]]; then
        hdiutil detach "$tmp_mount" -quiet || true
    fi

    echo "Installed: /Applications/GXDLMSDirector.app"
}

install_linux() {
    local src="${1:-}"
    local bindir="${HOME}/.local/bin"

    if [[ -z "$src" ]]; then
        src="$ROOT/dist/GXDLMSDirector-${VERSION}-x86_64.AppImage"
    fi

    if [[ ! -f "$src" ]]; then
        echo "AppImage not found: $src" >&2
        echo "Build it first: ./packaging/build-appimage.sh" >&2
        exit 1
    fi

    mkdir -p "$bindir"
    local target="$bindir/GXDLMSDirector-${VERSION}-x86_64.AppImage"
    install -m 755 "$src" "$target"
    ln -sf "$(basename "$target")" "$bindir/GXDLMSDirector"

    echo "Installed: $target"
    echo "Run: GXDLMSDirector"
}

case "${1:-}" in
    -h|--help|help)
        usage
        exit 0
        ;;
esac

case "$(uname -s)" in
    Darwin) install_macos "${1:-}" ;;
    Linux) install_linux "${1:-}" ;;
    *)
        echo "Unsupported OS. Use macOS or Linux." >&2
        exit 1
        ;;
esac
