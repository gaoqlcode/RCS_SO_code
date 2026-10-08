@echo off
REM 用 Qt MinGW 7.3 编译 zlib / libtiff（标准开源库）
setlocal
set QT_MINGW=C:\Qt\Qt5.12.8\Tools\mingw730_64
set PATH=%QT_MINGW%\bin;%PATH%
set CMAKE=C:\Program Files\JetBrains\CLion 2026.2.2\bin\cmake\win\x64\bin\cmake.exe
set ROOT=%~dp0..
set SRC=%ROOT%\src
set BUILD=%ROOT%\build\mingw73_64
set INST=%ROOT%\install\mingw73_64

echo gcc:
gcc -dumpversion
mkdir "%INST%\include" 2>nul
mkdir "%INST%\lib" 2>nul
mkdir "%INST%\bin" 2>nul

if exist "%SRC%\zlib-1.3.1\CMakeLists.txt" (
  echo === zlib ===
  "%CMAKE%" -G "MinGW Makefiles" -S "%SRC%\zlib-1.3.1" -B "%BUILD%\zlib" -DCMAKE_INSTALL_PREFIX="%INST%" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
  "%CMAKE%" --build "%BUILD%\zlib" --config Release -j 4
  "%CMAKE%" --install "%BUILD%\zlib"
)

if exist "%SRC%\tiff-4.6.0\CMakeLists.txt" (
  echo === libtiff ===
  "%CMAKE%" -G "MinGW Makefiles" -S "%SRC%\tiff-4.6.0" -B "%BUILD%\tiff" -DCMAKE_INSTALL_PREFIX="%INST%" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DZLIB_INCLUDE_DIR="%INST%\include" -DZLIB_LIBRARY="%INST%\lib\libzlibstatic.a" -Djpeg=OFF -Dlzma=OFF -Dzstd=OFF -Dwebp=OFF -Dlibdeflate=OFF -Dcxx=OFF -Dtiff-tools=OFF -Dtiff-tests=OFF -Dtiff-contrib=OFF -Dtiff-docs=OFF
  "%CMAKE%" --build "%BUILD%\tiff" --target tiff --config Release -j 4
  "%CMAKE%" --install "%BUILD%\tiff"
)

echo.
echo 完成。主工程依赖 Eigen + libtiff + zlib，见 deps.pri
endlocal
