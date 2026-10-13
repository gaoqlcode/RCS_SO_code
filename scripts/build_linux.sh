#!/usr/bin/env bash
# Build Linux: 完整 rcs_proc（含大图拼接）-> pack sdk/lib -> GUI/CLI。
# Usage (repo root or anywhere):
#   ./scripts/build_linux.sh
#   ./scripts/build_linux.sh --clean          # wipe build_linux first
#   ./scripts/build_linux.sh --no-gui         # lib + cli only
#   ./scripts/build_linux.sh --no-tozero      # 关闭 ToZeroData
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

BUILD_DIR="${RCS_BUILD_DIR:-build_linux}"
JOBS="${RCS_JOBS:-$(nproc 2>/dev/null || echo 4)}"
BUILD_GUI=ON
ENABLE_TOZERODATA=ON
DO_CLEAN=0

for arg in "$@"; do
  case "$arg" in
    --clean|-c) DO_CLEAN=1 ;;
    --no-gui) BUILD_GUI=OFF ;;
    --no-tozero) ENABLE_TOZERODATA=OFF ;;
    --no-mosaic|--mosaic)
      echo "提示: 已取消拼接编译开关，库始终包含大图拼接。忽略 $arg" >&2
      ;;
    -h|--help)
      sed -n '2,7p' "$0" | sed 's/^# \{0,1\}//'
      exit 0
      ;;
    *)
      echo "unknown arg: $arg" >&2
      exit 2
      ;;
  esac
done

need() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "missing: $1" >&2
    exit 1
  }
}
need cmake
need g++

# arch -> bundled tiff path（完整库强制需要）
arch="$(uname -m | tr 'A-Z' 'a-z')"
case "$arch" in
  amd64|x86_64|x64) la=x86_64 ;;
  aarch64|arm64) la=aarch64 ;;
  armv7*|armhf) la=armv7l ;;
  *) la="$arch" ;;
esac
TIFF_HDR="3rdparty/install/linux_${la}/include/tiffio.h"
if [[ -f "$TIFF_HDR" ]]; then
  echo "tiff: $TIFF_HDR (full lib / mosaic always ON)"
else
  echo "warn: 未找到捆绑 libtiff: $TIFF_HDR" >&2
  echo "  可先: ./3rdparty/scripts/build_linux_static.sh" >&2
  echo "  或 cmake 时 -DRCS_TIFF_PREFER_STATIC=OFF 用系统 libtiff" >&2
fi

CMAKE_ARGS=(
  -S .
  -B "$BUILD_DIR"
  -DCMAKE_BUILD_TYPE=Release
  -DRCS_USE_SDK=OFF
  -DRCS_BUILD_GUI="$BUILD_GUI"
  -DRCS_BUILD_CLI=ON
  -DRCS_BUILD_TESTS=ON
  -DRCS_ENABLE_TOZERODATA="$ENABLE_TOZERODATA"
)

if [[ "$BUILD_GUI" == ON ]]; then
  if ! cmake --find-package -DNAME=Qt5 -DCOMPILER_ID=GNU -DLANGUAGE=CXX -DMODE=EXIST >/dev/null 2>&1; then
    if ! pkg-config --exists Qt5Widgets 2>/dev/null && [[ ! -d /usr/include/x86_64-linux-gnu/qt5 ]]; then
      echo "warn: Qt5 may be missing (apt install qtbase5-dev)" >&2
    fi
  fi
fi

if [[ "$DO_CLEAN" -eq 1 && -d "$BUILD_DIR" ]]; then
  echo "clean $BUILD_DIR"
  rm -rf "$BUILD_DIR"
fi

echo "g++  = $(command -v g++)"
echo "cmake args: ${CMAKE_ARGS[*]}"
cmake "${CMAKE_ARGS[@]}"

echo
echo "==== 1/2 build SDK lib (rcs_proc) ===="
cmake --build "$BUILD_DIR" --target rcs_proc -j"$JOBS"

echo
echo "==== pack sdk/lib ===="
export RCS_BUILD_DIR="$BUILD_DIR"
./sdk/pack_sdk.sh

echo
echo "==== 2/2 build full project ===="
cmake --build "$BUILD_DIR" -j"$JOBS"

echo
echo "==== deploy (可发布运行包) ===="
export RCS_BUILD_DIR="$BUILD_DIR"
./scripts/deploy_linux.sh

echo
echo "OK: $BUILD_DIR/bin/RCS  (if GUI on)"
echo "OK: $BUILD_DIR/cli/rcs_cli"
echo "OK: sdk/lib packed"
echo "OK: deploy/linux_*  (整目录可拷贝运行)"
if [[ -x "$BUILD_DIR/bin/RCS" ]]; then
  echo "dev run:  export LD_LIBRARY_PATH=$ROOT/$BUILD_DIR/lib:\$LD_LIBRARY_PATH"
  echo "          $BUILD_DIR/bin/RCS"
  echo "release:  ./deploy/linux_*/run_RCS.sh"
fi
