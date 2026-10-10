#!/usr/bin/env bash
# 把当前 build 产物打成可拷贝运行的目录：deploy/linux_<arch>/
# 用法（一般由 build_linux.sh 末尾调用）：
#   ./scripts/deploy_linux.sh
#   RCS_BUILD_DIR=build_linux ./scripts/deploy_linux.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

BUILD_DIR="${RCS_BUILD_DIR:-build_linux}"
[[ "$BUILD_DIR" != /* ]] && BUILD_DIR="$ROOT/$BUILD_DIR"

arch="$(uname -m | tr 'A-Z' 'a-z')"
case "$arch" in
  amd64|x86_64|x64) la=x86_64 ;;
  aarch64|arm64) la=aarch64 ;;
  armv7*|armhf) la=armv7l ;;
  *) la="$arch" ;;
esac

OUT="$ROOT/deploy/linux_${la}"
rm -rf "$OUT"
mkdir -p "$OUT/bin" "$OUT/lib" "$OUT/plugins/platforms"

copy_if() {
  local src="$1" dst="$2"
  if [[ -e "$src" ]]; then
    cp -a "$src" "$dst"
    return 0
  fi
  return 1
}

# ---- 可执行文件 ----
GUI=""
for cand in "$BUILD_DIR/bin/RCS" "$BUILD_DIR/RCS"; do
  [[ -x "$cand" ]] && GUI="$cand" && break
done
CLI=""
for cand in "$BUILD_DIR/cli/rcs_cli" "$BUILD_DIR/bin/rcs_cli" "$BUILD_DIR/rcs_cli"; do
  [[ -x "$cand" ]] && CLI="$cand" && break
done

if [[ -z "$GUI" && -z "$CLI" ]]; then
  echo "deploy: 未找到 RCS 或 rcs_cli（BUILD=$BUILD_DIR）" >&2
  exit 1
fi

[[ -n "$GUI" ]] && cp -a "$GUI" "$OUT/bin/RCS"
[[ -n "$CLI" ]] && cp -a "$CLI" "$OUT/bin/rcs_cli"

# ---- 本工程动态库 ----
shopt -s nullglob
rcs_sos=("$BUILD_DIR"/lib/librcs_proc.so*)
if ((${#rcs_sos[@]} == 0)); then
  echo "deploy: 未找到 $BUILD_DIR/lib/librcs_proc.so*" >&2
  exit 1
fi
cp -a "${rcs_sos[@]}" "$OUT/lib/"
shopt -u nullglob

# ---- 收集 ldd 依赖（跳过系统 libc / 显卡驱动等）----
is_skip_lib() {
  local p="$1" b
  b="$(basename "$p")"
  case "$b" in
    linux-vdso.so*|ld-linux*.so*|libc.so*|libm.so*|libdl.so*|libpthread.so*| \
    librt.so*|libresolv.so*|libnss_*.so*|libgcc_s.so*|libstdc++.so*| \
    libGL.so*|libGLdispatch.so*|libGLX.so*|libOpenGL.so*|libEGL.so*| \
    libdrm.so*|libX*.so*|libxcb*.so*|libxkbcommon*.so*|libfontconfig.so*| \
    libfreetype.so*|libz.so*|libpng*.so*|libjpeg*.so*|libharfbuzz.so*| \
    libglib*.so*|libgobject*.so*|libgio*.so*|libpcre*.so*|libuuid.so*| \
    libdbus*.so*|libsystemd.so*|libselinux.so*|libmount.so*|libblkid.so*| \
    libexpat.so*|libffi.so*|liblzma.so*|libbz2.so*|libbrotli*.so*| \
    libgraphite2.so*|libmd.so*|libbsd.so*|libcap.so*)
      return 0
      ;;
  esac
  # 打进包：Qt、ICU（Qt 常依赖）、本库
  case "$b" in
    libQt5*.so*|librcs_proc.so*|libicu*.so*) return 1 ;;
  esac
  if [[ "$p" == *"/qt5/"* || "$p" == *"/Qt/"* || "$p" == *"/Qt5/"* ]]; then
    return 1
  fi
  return 0
}

collect_deps() {
  local bin="$1"
  [[ -e "$bin" ]] || return 0
  local line path
  while IFS= read -r line; do
    path="$(echo "$line" | awk '/=>/ {print $3} /^[[:space:]]*\// {print $1}')"
    [[ -n "$path" && -f "$path" ]] || continue
    if is_skip_lib "$path"; then
      continue
    fi
    local base
    base="$(basename "$path")"
    if [[ ! -e "$OUT/lib/$base" ]]; then
      cp -aL "$path" "$OUT/lib/$base"
    fi
  done < <(ldd "$bin" 2>/dev/null || true)
}

collect_deps "$OUT/bin/RCS"
collect_deps "$OUT/bin/rcs_cli"
collect_deps "$OUT/lib/librcs_proc.so"

# Qt 平台插件（缺了 GUI 起不来）
QT_PLUGIN=""
for d in \
  /usr/lib/x86_64-linux-gnu/qt5/plugins/platforms \
  /usr/lib/aarch64-linux-gnu/qt5/plugins/platforms \
  /usr/lib/arm-linux-gnueabihf/qt5/plugins/platforms \
  /usr/lib/qt5/plugins/platforms \
  "${QTDIR:-}/plugins/platforms" \
  "${CMAKE_PREFIX_PATH:-}/plugins/platforms"
do
  if [[ -f "$d/libqxcb.so" ]]; then
    QT_PLUGIN="$d"
    break
  fi
done

if [[ -n "$GUI" ]]; then
  if [[ -n "$QT_PLUGIN" ]]; then
    cp -a "$QT_PLUGIN/libqxcb.so" "$OUT/plugins/platforms/"
    # xcb 插件自身依赖
    collect_deps "$OUT/plugins/platforms/libqxcb.so"
  else
    echo "warn: 未找到 libqxcb.so，GUI 在无 Qt 的机器上可能无法启动" >&2
  fi
fi

# RPATH：$ORIGIN/../lib
if command -v patchelf >/dev/null 2>&1; then
  for b in "$OUT/bin/RCS" "$OUT/bin/rcs_cli"; do
    [[ -f "$b" ]] || continue
    patchelf --set-rpath '$ORIGIN/../lib' "$b" 2>/dev/null || true
  done
  for so in "$OUT/lib"/librcs_proc.so*; do
    [[ -f "$so" && ! -L "$so" ]] || continue
    patchelf --set-rpath '$ORIGIN' "$so" 2>/dev/null || true
  done
fi

# 启动脚本（无 patchelf 时仍可用）
cat > "$OUT/run_RCS.sh" <<'EOF'
#!/usr/bin/env bash
HERE="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="$HERE/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$HERE/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
export QT_QPA_PLATFORM_PLUGIN_PATH="$HERE/plugins/platforms"
exec "$HERE/bin/RCS" "$@"
EOF
chmod +x "$OUT/run_RCS.sh"

cat > "$OUT/run_cli.sh" <<'EOF'
#!/usr/bin/env bash
HERE="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="$HERE/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$HERE/bin/rcs_cli" "$@"
EOF
chmod +x "$OUT/run_cli.sh"

cat > "$OUT/README.txt" <<EOF
雷达数据处理软件 — Linux 发布目录（linux_${la}）

结构:
  bin/RCS          GUI
  bin/rcs_cli      命令行
  lib/             librcs_proc + Qt 等运行时库
  plugins/         Qt 平台插件
  run_RCS.sh       推荐用此启动 GUI
  run_cli.sh       启动 CLI

用法:
  ./run_RCS.sh
  ./run_cli.sh --help

整目录拷到目标机即可；目标机需有基本 X11/Wayland 与字体（中文建议 fonts-noto-cjk）。
生成自: $BUILD_DIR
EOF

echo
echo "OK: deploy -> $OUT"
ls -la "$OUT/bin" 2>/dev/null || true
ls "$OUT/lib" | head -20
echo "... (lib 共 $(ls -1 "$OUT/lib" | wc -l) 个文件)"
echo "run:  $OUT/run_RCS.sh"
