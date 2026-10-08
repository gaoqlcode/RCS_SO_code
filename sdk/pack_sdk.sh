#!/usr/bin/env bash
# 从工程 build 产物刷新 sdk/ 里的库和头文件
# 推荐交付：先静态编进 libtiff 再打包，客户无需 apt install libtiff5
#   ./3rdparty/scripts/build_linux_x86_64.sh   # 一次性
#   cmake -S . -B build -DRCS_ENABLE_MOSAIC=ON -DRCS_TIFF_PREFER_STATIC=ON -DRCS_BUILD_GUI=OFF
#   cmake --build build -j
#   ./sdk/pack_sdk.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${RCS_BUILD_DIR:-$ROOT/build}"
SDK="$ROOT/sdk"

SO=""
for cand in "$BUILD/lib/librcs_proc.so.1.0.0" "$BUILD/lib/librcs_proc.so"; do
  if [[ -f "$cand" ]]; then SO="$cand"; break; fi
done
if [[ -z "$SO" ]]; then
  echo "未找到 librcs_proc.so，请先编译工程（BUILD=$BUILD）" >&2
  exit 1
fi

mkdir -p "$SDK/include/rcs" "$SDK/lib"
cp -a "$ROOT/lib/include/rcs/." "$SDK/include/rcs/"
rm -f "$SDK/lib"/librcs_proc.so*
cp -a "$BUILD/lib"/librcs_proc.so* "$SDK/lib/"

echo "已更新: $SDK/include/rcs  $SDK/lib"
ls -la "$SDK/lib"

if command -v ldd >/dev/null; then
  echo
  echo "运行时依赖 (ldd)："
  ldd "$SDK/lib/librcs_proc.so" || true
  if ldd "$SDK/lib/librcs_proc.so" 2>/dev/null | grep -q 'libtiff\.so'; then
    echo
    echo "[警告] 仍依赖系统 libtiff.so — 客户需 apt install libtiff5" >&2
    echo "        请用静态 libtiff 重编：./3rdparty/scripts/build_linux_x86_64.sh" >&2
  else
    echo
    echo "[OK] 未依赖系统 libtiff — 客户可直接使用（无需 apt install libtiff5）"
  fi
fi
