#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${1:-}"

if [ -z "$DEST" ]; then
    printf 'usage: %s <portlibs-prefix>\n' "$0" >&2
    exit 1
fi

if [ ! -f "$ROOT/dist/ps3/lib/libarchive.a" ]; then
    printf 'Missing %s/dist/ps3/lib/libarchive.a\n' "$ROOT" >&2
    exit 1
fi

mkdir -p "$DEST/include" "$DEST/lib/pkgconfig"
cp -a "$ROOT/dist/ps3/include/." "$DEST/include/"
cp -a "$ROOT/dist/ps3/lib/." "$DEST/lib/"

printf 'Installed libarchive into %s\n' "$DEST"
