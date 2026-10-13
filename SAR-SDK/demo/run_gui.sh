#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
SDK="$(cd "$HERE/.." && pwd)"
BIN="$HERE/build_gui/sar_sdk_demo_gui"
[[ -x "$BIN" ]] || {
  echo "未找到 $BIN，请先: bash $HERE/build_gui.sh" >&2
  exit 1
}
export LD_LIBRARY_PATH="$SDK/lib:${LD_LIBRARY_PATH:-}"
exec "$BIN" "$@"
