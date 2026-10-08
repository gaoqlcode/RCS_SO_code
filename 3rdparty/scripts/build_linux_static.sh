#!/usr/bin/env bash
# 编译 Linux 静态 zlib + libtiff，打进 librcs_proc 后客户无需 apt 装 libtiff
# 用法:
#   ./build_linux_static.sh                 # 本机架构 (x86_64 / aarch64)
#   ./build_linux_static.sh aarch64         # 交叉到 aarch64（需 aarch64-linux-gnu-g++）
#   RCS_3P_SRC=/path/to/src ./build_linux_static.sh aarch64
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="${RCS_3P_SRC:-$ROOT/src}"

HOST_ARCH="$(uname -m)"
case "$HOST_ARCH" in
  arm64) HOST_ARCH=aarch64 ;;
esac

TARGET_ARCH="${1:-$HOST_ARCH}"
case "$TARGET_ARCH" in
  x86_64|amd64) TARGET_ARCH=x86_64 ;;
  aarch64|arm64) TARGET_ARCH=aarch64 ;;
  armv7l|armhf|arm) TARGET_ARCH=armv7l ;;
  *)
    echo "不支持的架构: $TARGET_ARCH（支持 x86_64 / aarch64 / armv7l）" >&2
    exit 1
    ;;
esac

BUILD="$ROOT/build/linux_${TARGET_ARCH}"
INST="$ROOT/install/linux_${TARGET_ARCH}"

# 源码旁路
if [[ ! -f "$SRC/zlib-1.3.1/CMakeLists.txt" ]]; then
  for cand in \
    "$ROOT/../../207_RCS_code/3rdparty/src" \
    "/mnt/e/project/SAR-RCS/207_RCS/207_RCS_code/3rdparty/src" \
    "/home/gaoql/RCS/207_RCS_code/3rdparty/src"
  do
    if [[ -f "$cand/zlib-1.3.1/CMakeLists.txt" ]]; then
      SRC="$cand"
      break
    fi
  done
fi

if [[ ! -f "$SRC/zlib-1.3.1/CMakeLists.txt" ]]; then
  echo "找不到 zlib 源码: $SRC/zlib-1.3.1" >&2
  echo "任选其一：" >&2
  echo "  1) 联网下载: ./3rdparty/scripts/fetch_sources.sh" >&2
  echo "  2) 从 207_RCS_code/3rdparty/src 拷贝 zlib-1.3.1 与 tiff-4.6.0 到本工程 3rdparty/src/" >&2
  echo "  3) RCS_3P_SRC=/含上述目录的路径 ./3rdparty/scripts/build_linux_static.sh" >&2
  exit 1
fi

CMAKE_EXTRA=()
if [[ "$TARGET_ARCH" != "$HOST_ARCH" ]]; then
  # 交叉编译
  case "$TARGET_ARCH" in
    aarch64)
      PROJ="$(cd "$ROOT/.." && pwd)"
      TC="${RCS_CMAKE_TOOLCHAIN:-$PROJ/cmake/toolchain-aarch64-linux-gnu.cmake}"
      if [[ ! -f "$TC" ]]; then
        echo "缺少工具链文件: $TC" >&2
        exit 1
      fi
      if ! command -v aarch64-linux-gnu-g++ >/dev/null 2>&1; then
        echo "未找到 aarch64-linux-gnu-g++，请先: sudo apt install g++-aarch64-linux-gnu" >&2
        exit 1
      fi
      CMAKE_EXTRA+=(-DCMAKE_TOOLCHAIN_FILE="$TC")
      ;;
    armv7l)
      echo "armv7l 交叉请自备工具链并设 RCS_CMAKE_TOOLCHAIN" >&2
      if [[ -z "${RCS_CMAKE_TOOLCHAIN:-}" ]]; then exit 1; fi
      CMAKE_EXTRA+=(-DCMAKE_TOOLCHAIN_FILE="$RCS_CMAKE_TOOLCHAIN")
      ;;
    *)
      echo "本机是 $HOST_ARCH，无法交叉到 $TARGET_ARCH（请在目标机本机编译）" >&2
      exit 1
      ;;
  esac
fi

echo "HOST=$HOST_ARCH  TARGET=$TARGET_ARCH"
echo "SRC = $SRC"
echo "INST= $INST"
mkdir -p "$INST"

echo "=== zlib (static, PIC) ==="
cmake -S "$SRC/zlib-1.3.1" -B "$BUILD/zlib" \
  "${CMAKE_EXTRA[@]}" \
  -DCMAKE_INSTALL_PREFIX="$INST" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DBUILD_SHARED_LIBS=OFF
cmake --build "$BUILD/zlib" -j"$(nproc)"
cmake --install "$BUILD/zlib"

ZLIB_A="$INST/lib/libz.a"
[[ -f "$ZLIB_A" ]] || ZLIB_A="$INST/lib/libzlibstatic.a"

echo "=== libtiff (static, PIC, zlib only) ==="
cmake -S "$SRC/tiff-4.6.0" -B "$BUILD/tiff" \
  "${CMAKE_EXTRA[@]}" \
  -DCMAKE_INSTALL_PREFIX="$INST" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DBUILD_SHARED_LIBS=OFF \
  -DZLIB_INCLUDE_DIR="$INST/include" \
  -DZLIB_LIBRARY="$ZLIB_A" \
  -Djpeg=OFF -Dlzma=OFF -Dzstd=OFF -Dwebp=OFF -Dlibdeflate=OFF \
  -Djbig=OFF -Dlerc=OFF -Dcxx=OFF \
  -Dtiff-tools=OFF -Dtiff-tests=OFF -Dtiff-contrib=OFF -Dtiff-docs=OFF
cmake --build "$BUILD/tiff" --target tiff -j"$(nproc)"
cmake --install "$BUILD/tiff"

echo
echo "完成: $INST"
file "$INST/lib/libtiff.a" 2>/dev/null || true
ls -la "$INST/lib"/libtiff.a "$INST/lib"/libz.a "$INST/include"/tiffio.h 2>/dev/null || true
