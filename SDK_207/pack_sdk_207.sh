#!/usr/bin/env bash
# 刷新 SDK_207/lib（仅 librcs_proc；ToZeroData 已静态链入）
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
BUILD="${RCS_BUILD_DIR:-$ROOT/build_linux}"
[[ "$BUILD" != /* ]] && BUILD="$ROOT/$BUILD"

SO=""
for cand in "$BUILD/lib/librcs_proc.so.1.0.0" "$BUILD/lib/librcs_proc.so" \
            "$BUILD/lib/librcs_proc.so.1"; do
  if [[ -f "$cand" ]]; then SO="$cand"; break; fi
done
if [[ -z "$SO" ]]; then
  echo "未找到 librcs_proc.so，请先编译完整库（BUILD=$BUILD）" >&2
  exit 1
fi

mkdir -p "$HERE/lib"
rm -f "$HERE/lib"/librcs_proc.so* "$HERE/lib"/libToZeroDataCpp11.so*
cp -a "$BUILD/lib"/librcs_proc.so* "$HERE/lib/"

API="$HERE/include/rcs/rcs_api.h"
for need in rcsVersion processRcsCalTif processMosaic flattenMosaic \
            listMosaicTiffs loadMosaicGrayPreview prepareMosaicInputs \
            processToZeroData toZeroDataOutputPaths toZeroDataRecoveryPath; do
  if ! grep -q "$need" "$API"; then
    echo "错误: SDK_207 头文件缺少 $need" >&2
    exit 1
  fi
done
# 主库须已静态合入 ToZero（打包前请用 Linux + RCS_ENABLE_TOZERODATA=ON 编译）
if ! nm -D "$HERE/lib/librcs_proc.so" 2>/dev/null | grep -q processToZeroData; then
  echo "错误: librcs_proc.so 无 processToZeroData，请以 -DRCS_ENABLE_TOZERODATA=ON 重新编译后再 pack" >&2
  exit 1
fi
for ban in processHrrp 'processRcs([^C]|$)' processCwRcs processSigma0 previewL0Folder; do
  if grep -qE "$ban" "$API"; then
    echo "警告: SDK_207 头文件不应声明 ${ban%%(*}" >&2
  fi
done

echo "已更新: $HERE/lib"
ls -la "$HERE/lib"
echo "对外头文件: $HERE/include/rcs （定标 + 拼接 + ToZeroData；ToZero 已静态合入主库）"
if command -v ldd >/dev/null; then
  echo
  echo "运行时依赖 (librcs_proc):"
  ldd "$HERE/lib/librcs_proc.so" || true
  if ldd "$HERE/lib/librcs_proc.so" 2>/dev/null | grep -q ToZeroData; then
    echo "错误: 仍依赖 libToZeroDataCpp11.so，静态合入失败" >&2
    exit 1
  fi
fi
