#!/usr/bin/env bash
# 刷新 sdk/lib 里的动态库。对外头文件以 sdk/include 为准（不含大图拼接）。
# 客户交付建议：
#   cmake -S . -B build -DRCS_ENABLE_MOSAIC=OFF -DRCS_BUILD_GUI=OFF
#   cmake --build build -j
#   ./sdk/pack_sdk.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${RCS_BUILD_DIR:-$ROOT/build_linux}"
# relative RCS_BUILD_DIR -> under repo root
[[ "$BUILD" != /* ]] && BUILD="$ROOT/$BUILD"
SDK="$ROOT/sdk"

SO=""
for cand in "$BUILD/lib/librcs_proc.so.1.0.0" "$BUILD/lib/librcs_proc.so" \
            "$BUILD/lib/librcs_proc.so.1"; do
  if [[ -f "$cand" ]]; then SO="$cand"; break; fi
done
if [[ -z "$SO" ]]; then
  # fallback old default
  for cand in "$ROOT/build/lib/librcs_proc.so.1.0.0" "$ROOT/build/lib/librcs_proc.so"; do
    if [[ -f "$cand" ]]; then SO="$cand"; BUILD="$(dirname "$(dirname "$cand")")"; break; fi
  done
fi
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
for need in processSigma0 previewL0Folder listL0Files probeL0File loadImagedRawPreview \
            listImagedRawFiles; do
  if ! grep -q "$need" "$SDK/include/rcs/rcs_api.h"; then
    echo "警告: 客户头文件缺少 $need" >&2
  fi
done
if [[ ! -f "$SDK/include/rcs/rcs.hpp" ]]; then
  echo "警告: 缺少 rcs.hpp" >&2
fi
if [[ -f "$SDK/include/rcs/helpers.hpp" ]]; then
  echo "警告: 已废弃的 helpers.hpp 仍存在，请删除" >&2
fi
if grep -qE '\brun(Hrrp|Rcs|CwRcs|Sigma0|L0Preview)\b' "$SDK/include/rcs/rcs_api.h"; then
  echo "警告: 客户头文件仍含已移除的 run* 一键接口" >&2
fi
if grep -q '无回调\|批处理用' "$SDK/include/rcs/rcs_api.h"; then
  echo "警告: 客户头文件仍描述无回调批处理重载" >&2
fi
echo "对外头文件保持: $SDK/include/rcs （交互 process* + 探查，不含拼接）"

if command -v ldd >/dev/null; then
  echo
  echo "运行时依赖:"
  ldd "$SDK/lib/librcs_proc.so" || true
fi

echo
echo "demo: sdk/demo/cli_demo 、 sdk/demo/gui_demo"
