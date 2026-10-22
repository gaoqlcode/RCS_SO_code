#!/usr/bin/env bash
# 编译 SAR-SDK Qt GUI demo（moc + g++，不依赖完整 qmake mkspecs）
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
SDK="$(cd "$HERE/.." && pwd)"
BUILD="$HERE/build_gui"

[[ -f "$SDK/lib/librcs_proc.so" || -f "$SDK/lib/librcs_proc.so.1.0.0" ]] || {
  echo "缺少 $SDK/lib/librcs_proc.so* ，请先在工程根目录编译并拷入 SAR-SDK/lib" >&2
  exit 1
}
if [[ ! -e "$SDK/lib/librcs_proc.so" && -f "$SDK/lib/librcs_proc.so.1.0.0" ]]; then
  ln -sfn librcs_proc.so.1.0.0 "$SDK/lib/librcs_proc.so"
fi

# Qt5：优先 sysroot，其次系统路径
SYSROOT="${RCS_SYSROOT:-/home/gaoql/.local/rcs-sysroot}"
QT_INC=""
QT_LIB=""
MOC=""
for base in "$SYSROOT/usr" /usr; do
  cand_inc="$base/include/x86_64-linux-gnu/qt5"
  cand_lib="$base/lib/x86_64-linux-gnu"
  cand_moc="$base/lib/x86_64-linux-gnu/qt5/bin/moc"
  [[ -x "$cand_moc" ]] || cand_moc="$base/lib/qt5/bin/moc"
  if [[ -d "$cand_inc/QtWidgets" && -e "$cand_lib/libQt5Widgets.so" && -x "$cand_moc" ]]; then
    QT_INC="$cand_inc"
    QT_LIB="$cand_lib"
    MOC="$cand_moc"
    break
  fi
done

if [[ -z "$MOC" ]]; then
  echo "未找到 Qt5 Widgets / moc" >&2
  exit 1
fi
echo "Qt include: $QT_INC"
echo "Qt lib:     $QT_LIB"
echo "moc:        $MOC"

rm -rf "$BUILD"
mkdir -p "$BUILD"
cd "$BUILD"

HDRS=(MainWindow.h CalMosaicWorker.h RectPickDialog.h ManualAlignDialog.h)
SRCS=(main.cpp MainWindow.cpp CalMosaicWorker.cpp RectPickDialog.cpp ManualAlignDialog.cpp)
OBJS=()

CXXFLAGS=(
  -O2 -std=c++11 -fPIC
  -DQT_NO_DEBUG -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_CORE_LIB
  -I"$SDK/include"
  -I"$QT_INC" -I"$QT_INC/QtWidgets" -I"$QT_INC/QtGui" -I"$QT_INC/QtCore"
  -I"$HERE"
)

for h in "${HDRS[@]}"; do
  base="${h%.h}"
  "$MOC" "$HERE/$h" -o "moc_${base}.cpp"
  g++ "${CXXFLAGS[@]}" -c "moc_${base}.cpp" -o "moc_${base}.o"
  OBJS+=("moc_${base}.o")
done

for s in "${SRCS[@]}"; do
  o="${s%.cpp}.o"
  g++ "${CXXFLAGS[@]}" -c "$HERE/$s" -o "$o"
  OBJS+=("$o")
done

OUT="$BUILD/sar_sdk_demo_gui"
# RPATH（不是 RUNPATH）对后续 dlopen 的 Qt 插件也生效，客户机不必再装 Qt
g++ -o "$OUT" "${OBJS[@]}" \
  -L"$SDK/lib" -lrcs_proc \
  -L"$QT_LIB" -lQt5Widgets -lQt5Gui -lQt5Core \
  -Wl,--disable-new-dtags -Wl,-rpath,'$ORIGIN/lib' \
  -pthread

# 运行时库放进 build_gui，整目录拷走即可运行
LIBDIR="$BUILD/lib"
mkdir -p "$LIBDIR" "$BUILD/plugins/platforms"
cp -a "$SDK/lib"/librcs_proc.so* "$LIBDIR/"
if [[ -f "$SDK/lib/libToZeroDataCpp11.so" ]]; then
  cp -a "$SDK/lib/libToZeroDataCpp11.so" "$LIBDIR/"
fi

is_system_lib() {
  local base
  base="$(basename "$1")"
  case "$base" in
    linux-vdso.so*|ld-linux*.so*|libc.so*|libm.so*|libdl.so*|libpthread.so*| \
    librt.so*|libresolv.so*|libgcc_s.so*|libstdc++.so*| \
    libGL.so*|libGLdispatch.so*|libGLX.so*|libOpenGL.so*|libEGL.so*| \
    libX*.so*|libxcb*.so*|libxkbcommon*.so*)
      return 0
      ;;
  esac
  return 1
}

copy_needed() {
  local bin="$1" line path base
  [[ -e "$bin" ]] || return 0
  while IFS= read -r line; do
    path="$(echo "$line" | awk '/=>/ {print $3} /^[[:space:]]*\// {print $1}')"
    [[ -n "$path" && -f "$path" ]] || continue
    is_system_lib "$path" && continue
    base="$(basename "$path")"
    if [[ ! -e "$LIBDIR/$base" ]]; then
      cp -aL "$path" "$LIBDIR/$base"
    fi
  done < <(ldd "$bin" 2>/dev/null || true)
}

copy_needed "$OUT"
copy_needed "$LIBDIR/librcs_proc.so"

QXCB="$QT_LIB/qt5/plugins/platforms/libqxcb.so"
if [[ ! -f "$QXCB" ]]; then
  QXCB="$(dirname "$QT_LIB")/qt5/plugins/platforms/libqxcb.so"
fi
if [[ -f "$QXCB" ]]; then
  cp -aL "$QXCB" "$BUILD/plugins/platforms/libqxcb.so"
  copy_needed "$BUILD/plugins/platforms/libqxcb.so"
else
  echo "warn: 未找到 libqxcb.so，无 Qt 的机器上界面可能起不来" >&2
fi

IMGDIR="$QT_LIB/qt5/plugins/imageformats"
if [[ -d "$IMGDIR" ]]; then
  mkdir -p "$BUILD/plugins/imageformats"
  cp -a "$IMGDIR"/*.so "$BUILD/plugins/imageformats/" 2>/dev/null || true
  for p in "$BUILD/plugins/imageformats"/*.so; do
    [[ -f "$p" ]] && copy_needed "$p"
  done
fi

if [[ -x "$SDK/engine/bin/sarIsarPostProcess" ]]; then
  rm -rf "$BUILD/engine"
  mkdir -p "$BUILD/engine"
  cp -a "$SDK/engine/bin" "$SDK/engine/lib" "$BUILD/engine/"
fi

cat > "$BUILD/qt.conf" <<'EOF'
[Paths]
Prefix = .
Plugins = plugins
Libraries = lib
EOF

cat > "$BUILD/run.sh" <<'EOF'
#!/usr/bin/env bash
HERE="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="$HERE/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$HERE/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
export QT_QPA_PLATFORM_PLUGIN_PATH="$HERE/plugins/platforms"
exec "$HERE/sar_sdk_demo_gui" "$@"
EOF
chmod +x "$BUILD/run.sh"

echo "已生成: $OUT"
echo "运行时库: $LIBDIR"
echo "运行:   bash $BUILD/run.sh"
