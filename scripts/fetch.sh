#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/src"

mkdir -p "$SRC"

fetch()
{
    local url="$1"
    local file="$2"
    local sum="$3"
    local dir="$4"
    local archive="$SRC/$file"

    if [ -d "$SRC/$dir" ]; then
        return
    fi

    if [ ! -f "$archive" ]; then
        curl -L --fail --retry 3 --output "$archive" "$url"
    fi

    printf '%s  %s\n' "$sum" "$archive" | sha256sum -c -

    case "$file" in
        *.tar.xz) tar -xf "$archive" -C "$SRC" ;;
        *.tar.gz) tar -xzf "$archive" -C "$SRC" ;;
    esac
}

fetch \
    "https://github.com/libarchive/libarchive/releases/download/v3.8.11/libarchive-3.8.11.tar.xz" \
    "libarchive-3.8.11.tar.xz" \
    "a3dc0872b1b39da34dc18ef1b944f0f11ffd37425d70ae04be679279f1cf43d2" \
    "libarchive-3.8.11"

fetch \
    "https://github.com/tukaani-project/xz/releases/download/v5.8.4/xz-5.8.4.tar.gz" \
    "xz-5.8.4.tar.gz" \
    "0014c7886930454fe8bd4228665b51af55eeae560ea135c9c4cd33f55b2591d9" \
    "xz-5.8.4"

fetch \
    "https://github.com/facebook/zstd/archive/refs/tags/v1.5.7.tar.gz" \
    "zstd-1.5.7.tar.gz" \
    "37d7284556b20954e56e1ca85b80226768902e2edabd3b649e9e72c0c9012ee3" \
    "zstd-1.5.7"

fetch \
    "https://github.com/lz4/lz4/releases/download/v1.10.0/lz4-1.10.0.tar.gz" \
    "lz4-1.10.0.tar.gz" \
    "537512904744b35e232912055ccf8ec66d768639ff3abe5788d90d792ec5f48b" \
    "lz4-1.10.0"
