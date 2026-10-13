#!/usr/bin/env bash
# 安装随包的 MATLAB Runtime v9.3（R2017b），供零级成像引擎使用。
# 默认装到 /usr/local/MATLAB/MATLAB_Runtime ，完成后库使用 v93 子目录。
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
INSTALLER="$HERE/MyAppInstaller_mcr.install"
DEST="${MCR_DEST:-/usr/local/MATLAB/MATLAB_Runtime}"
MODE="gui"

usage() {
  cat <<EOF
用法: bash runtime/install_mcr.sh [--silent] [--dest <目录>]

  --silent     无界面安装（需同意 MathWorks 许可）
  --dest DIR   安装根目录，默认 /usr/local/MATLAB/MATLAB_Runtime
               实际运行库在 DIR/v93

安装到默认目录需要管理员权限。已安装则可跳过，成像参数里的 MCR 目录填:
  $DEST/v93
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --silent) MODE="silent"; shift ;;
    --dest)
      [[ $# -ge 2 ]] || { echo "缺少 --dest 目录" >&2; exit 2; }
      DEST="$2"
      shift 2
      ;;
    -h|--help) usage; exit 0 ;;
    *) echo "未知参数: $1" >&2; usage; exit 2 ;;
  esac
done

if [[ ! -x "$INSTALLER" ]]; then
  echo "找不到安装器: $INSTALLER" >&2
  exit 1
fi

if [[ -d "$DEST/v93" ]]; then
  echo "已存在 $DEST/v93 ，无需重复安装。"
  exit 0
fi

if [[ "$(id -u)" -ne 0 ]]; then
  echo "安装到 $DEST 需要管理员权限。"
  args=(--dest "$DEST")
  if [[ "$MODE" == "silent" ]]; then
    args=(--silent "${args[@]}")
  fi
  exec sudo "$HERE/install_mcr.sh" "${args[@]}"
fi

echo "安装 MATLAB Runtime v9.3 到 $DEST"
if [[ "$MODE" == "silent" ]]; then
  "$INSTALLER" -mode silent -agreeToLicense yes -destinationFolder "$DEST"
else
  "$INSTALLER" -destinationFolder "$DEST"
fi

if [[ -d "$DEST/v93" ]]; then
  echo "安装完成: $DEST/v93"
else
  echo "安装程序已结束。请确认 $DEST/v93 是否存在；若装到了其他位置，请把该 v93 目录填到 MCR 路径。" >&2
  exit 1
fi
