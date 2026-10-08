#!/usr/bin/env bash
# 刷新 sdk/lib 里的动态库。对外头文件以 sdk/include 为准（不含大图拼接）。
# 客户交付建议：
#   cmake -S . -B build -DRCS_ENABLE_MOSAIC=OFF -DRCS_BUILD_GUI=OFF
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
  echo "未找到 librcs_proc.so，请先编译（BUILD=$BUILD）" >&2
  exit 1
fi

mkdir -p "$SDK/include/rcs" "$SDK/lib"
# 头文件：只用 sdk/include 里维护的客户版，禁止从 lib/include 整包覆盖（会带上 mosaic）
if [[ ! -f "$SDK/include/rcs/rcs_api.h" ]]; then
  echo "缺少 $SDK/include/rcs/rcs_api.h" >&2
  exit 1
fi
if grep -q 'processMosaic' "$SDK/include/rcs/rcs_api.h"; then
  echo "警告: 客户头文件里仍有 processMosaic，请检查 sdk/include/rcs/rcs_api.h" >&2
fi

rm -f "$SDK/lib"/librcs_proc.so*
cp -a "$BUILD/lib"/librcs_proc.so* "$SDK/lib/"

echo "已更新动态库: $SDK/lib"
ls -la "$SDK/lib"
echo "对外头文件保持: $SDK/include/rcs （含 HRRP/RCS/点频，不含拼接）"

if command -v ldd >/dev/null; then
  echo
  echo "运行时依赖:"
  ldd "$SDK/lib/librcs_proc.so" || true
fi

echo
echo "可到 sdk/demo 编一下例子做冒烟。"
