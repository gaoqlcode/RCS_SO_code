#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
RUN="$HERE/build_gui/run.sh"
[[ -x "$RUN" ]] || {
  echo "未找到 $RUN，请先: bash $HERE/build_gui.sh" >&2
  exit 1
}
exec "$RUN" "$@"
