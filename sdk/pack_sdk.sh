#!/usr/bin/env bash
# 刷新 sdk/lib 里的动态库。对外头文件以 sdk/include 为准（完整接口，含大图拼接）。
# 完整交付（含拼接）建议：
#   cmake -S . -B build_linux -DRCS_BUILD_GUI=OFF
#   cmake --build build_linux -j && ./sdk/pack_sdk.sh
# 不含拼接算法时仍可打包装库，但 processMosaic 等会返回「已禁用」。
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
  for cand in "$ROOT/build/lib/librcs_proc.so.1.0.0" "$ROOT/build/lib/librcs_proc.so"; do
    if [[ -f "$cand" ]]; then SO="$cand"; BUILD="$(dirname "$(dirname "$cand")")"; break; fi
  done
fi
if [[ -z "$SO" ]]; then
  echo "未找到 librcs_proc.so，请先编译（BUILD=$BUILD）" >&2
  exit 1
fi

mkdir -p "$SDK/include/rcs" "$SDK/lib"
if [[ ! -f "$SDK/include/rcs/rcs_api.h" ]]; then
  echo "缺少 $SDK/include/rcs/rcs_api.h" >&2
  exit 1
fi
if ! grep -q 'processMosaic' "$SDK/include/rcs/rcs_api.h"; then
  echo "警告: 客户头文件缺少 processMosaic（完整版应含 [8] 大图拼接）" >&2
fi
if ! grep -q 'struct MosaicRequest' "$SDK/include/rcs/types.hpp"; then
  echo "警告: types.hpp 缺少 MosaicRequest" >&2
fi

rm -f "$SDK/lib"/librcs_proc.so*
cp -a "$BUILD/lib"/librcs_proc.so* "$SDK/lib/"

echo "已更新动态库: $SDK/lib"
ls -la "$SDK/lib"
for need in processHrrp processRcs processCwRcs processSigma0 previewL0Folder \
            listL0Files probeL0File loadImagedRawPreview listImagedRawFiles \
            processMosaic flattenMosaic listMosaicTiffs prepareMosaicInputs; do
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
echo "对外头文件: $SDK/include/rcs （完整分组 [0]~[9]，含大图拼接与 RCS 像定标）"

if command -v ldd >/dev/null; then
  echo
  echo "运行时依赖:"
  ldd "$SDK/lib/librcs_proc.so" || true
fi

echo
echo "demo: sdk/demo/cli_demo 、 sdk/demo/gui_demo"
