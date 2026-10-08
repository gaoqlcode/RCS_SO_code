#!/usr/bin/env bash
# 编译 Linux 静态 zlib + libtiff（带 -fPIC），打进 librcs_proc 后客户无需 apt 装 libtiff
# 用法:
#   ./build_linux_static.sh                 # 本机架构 (x86_64 / aarch64)
#   ./build_linux_static.sh aarch64         # 交叉到 aarch64（需 aarch64-linux-gnu-g++）
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

if [[ ! -f "$SRC/zlib-1.3.1/CMakeLists.txt" ]]; then
  for cand in \
    "$ROOT/../../207_RCS_code/3rdparty/src" \
    "/mnt/e/project/SAR-RCS/207_RCS/207_RCS_code/3rdparty/src"
  do
    if [[ -f "$cand/zlib-1.3.1/CMakeLists.txt" ]]; then
      SRC="$cand"
      break
    fi
  done
fi

if [[ ! -f "$SRC/zlib-1.3.1/CMakeLists.txt" || ! -f "$SRC/tiff-4.6.0/CMakeLists.txt" ]]; then
  echo "缺少源码。请先: ./3rdparty/scripts/fetch_sources.sh" >&2
  echo "需要: $SRC/zlib-1.3.1  与  $SRC/tiff-4.6.0" >&2
  exit 1
fi

CMAKE_EXTRA=()
if [[ "$TARGET_ARCH" != "$HOST_ARCH" ]]; then
  case "$TARGET_ARCH" in
    aarch64)
      PROJ="$(cd "$ROOT/.." && pwd)"
      TC="${RCS_CMAKE_TOOLCHAIN:-$PROJ/cmake/toolchain-aarch64-linux-gnu.cmake}"
      [[ -f "$TC" ]] || { echo "缺少 $TC" >&2; exit 1; }
      command -v aarch64-linux-gnu-g++ >/dev/null || {
        echo "请先: sudo apt install g++-aarch64-linux-gnu" >&2; exit 1; }
      CMAKE_EXTRA+=(-DCMAKE_TOOLCHAIN_FILE="$TC")
      ;;
    *)
      echo "交叉到 $TARGET_ARCH 请设 RCS_CMAKE_TOOLCHAIN" >&2
      exit 1
      ;;
  esac
fi

echo "HOST=$HOST_ARCH  TARGET=$TARGET_ARCH"
echo "SRC = $SRC"
echo "INST= $INST"
mkdir -p "$INST"

# 若上次只编了 zlib、没有 libtiff.a，清掉 tiff 构建缓存再来
if [[ ! -f "$INST/lib/libtiff.a" ]]; then
  echo "未找到 $INST/lib/libtiff.a ，将重新编译 libtiff…"
  rm -rf "$BUILD/tiff"
fi

echo "=== zlib (static, PIC) ==="
cmake -S "$SRC/zlib-1.3.1" -B "$BUILD/zlib" \
  "${CMAKE_EXTRA[@]}" \
  -DCMAKE_INSTALL_PREFIX="$INST" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DBUILD_SHARED_LIBS=OFF
cmake --build "$BUILD/zlib" -j"$(nproc)"
cmake --install "$BUILD/zlib"

ZLIB_A=""
for cand in "$INST/lib/libz.a" "$INST/lib64/libz.a" "$INST/lib/libzlibstatic.a"; do
  if [[ -f "$cand" ]]; then ZLIB_A="$cand"; break; fi
done
if [[ -z "$ZLIB_A" ]]; then
  echo "zlib 安装失败，找不到 libz.a" >&2
  exit 1
fi
echo "ZLIB_A=$ZLIB_A"

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

# 有的环境 install 到 lib64；或只编出在 build 树里
TIFF_A=""
for cand in \
  "$INST/lib/libtiff.a" \
  "$INST/lib64/libtiff.a" \
  "$BUILD/tiff/libtiff/libtiff.a" \
  "$BUILD/tiff/lib/libtiff.a"
do
  if [[ -f "$cand" ]]; then TIFF_A="$cand"; break; fi
done

if [[ -z "$TIFF_A" ]]; then
  echo "libtiff 编译/安装失败：找不到 libtiff.a" >&2
  echo "请把上面 cmake/build 的报错完整贴出。" >&2
  find "$BUILD/tiff" -name 'libtiff.a' 2>/dev/null | head || true
  exit 1
fi

mkdir -p "$INST/lib" "$INST/include"
if [[ "$TIFF_A" != "$INST/lib/libtiff.a" ]]; then
  cp -a "$TIFF_A" "$INST/lib/libtiff.a"
  echo "已复制 libtiff.a → $INST/lib/libtiff.a"
fi
# 头文件
for h in tiff.h tiffio.h tiffvers.h tiffconf.h; do
  if [[ ! -f "$INST/include/$h" ]]; then
    find "$BUILD/tiff" "$SRC/tiff-4.6.0" -name "$h" 2>/dev/null | head -1 | while read -r p; do
      cp -a "$p" "$INST/include/"
    done
  fi
done

echo
echo "完成: $INST"
ls -la "$INST/lib/libtiff.a" "$INST/lib/libz.a" "$INST/include/tiffio.h"
file "$INST/lib/libtiff.a"
echo
echo "下一步:"
echo "  rm -rf ../../build   # 或工程根目录的 build"
echo "  cd ../.. && cmake -S . -B build -DRCS_ENABLE_MOSAIC=ON -DRCS_TIFF_PREFER_STATIC=ON -DRCS_BUILD_GUI=OFF"
echo "  cmake --build build -j && ./sdk/pack_sdk.sh"
