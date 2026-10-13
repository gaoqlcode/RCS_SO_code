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

HDRS=(MainWindow.h CalMosaicWorker.h RectPickDialog.h)
SRCS=(main.cpp MainWindow.cpp CalMosaicWorker.cpp RectPickDialog.cpp)
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
g++ -o "$OUT" "${OBJS[@]}" \
  -L"$SDK/lib" -lrcs_proc \
  -L"$QT_LIB" -lQt5Widgets -lQt5Gui -lQt5Core \
  -Wl,-rpath,"$SDK/lib" -Wl,-rpath,"$QT_LIB" \
  -pthread

echo "已生成: $OUT"
echo "运行:   bash $HERE/run_gui.sh"
