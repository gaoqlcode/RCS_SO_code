#!/usr/bin/env bash
# 下载 zlib / libtiff 源码到 3rdparty/src（无 207 工程时用）
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/src"
mkdir -p "$SRC"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

need_zlib=0
need_tiff=0
[[ -f "$SRC/zlib-1.3.1/CMakeLists.txt" ]] || need_zlib=1
[[ -f "$SRC/tiff-4.6.0/CMakeLists.txt" ]] || need_tiff=1

if [[ "$need_zlib" -eq 0 && "$need_tiff" -eq 0 ]]; then
  echo "已有源码: $SRC/zlib-1.3.1  $SRC/tiff-4.6.0"
  exit 0
fi

if ! command -v curl >/dev/null 2>&1 && ! command -v wget >/dev/null 2>&1; then
  echo "需要 curl 或 wget" >&2
  exit 1
fi

dl() {
  local url="$1" out="$2"
  if command -v curl >/dev/null 2>&1; then
    curl -L --fail -o "$out" "$url"
  else
    wget -O "$out" "$url"
  fi
}

if [[ "$need_zlib" -eq 1 ]]; then
  echo "下载 zlib-1.3.1 …"
  dl "https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz" "$TMP/zlib.tgz"
  tar -xzf "$TMP/zlib.tgz" -C "$TMP"
  rm -rf "$SRC/zlib-1.3.1"
  mv "$TMP/zlib-1.3.1" "$SRC/"
fi

if [[ "$need_tiff" -eq 1 ]]; then
  echo "下载 tiff-4.6.0 …"
  dl "https://download.osgeo.org/libtiff/tiff-4.6.0.tar.gz" "$TMP/tiff.tgz"
  tar -xzf "$TMP/tiff.tgz" -C "$TMP"
  rm -rf "$SRC/tiff-4.6.0"
  mv "$TMP/tiff-4.6.0" "$SRC/"
fi

echo "完成: $SRC"
ls -la "$SRC"
