@echo off
REM Windows：Qt MinGW 7.3 + 工程内 3rdparty libtiff，默认打开大图拼接
REM 用法：在工程根目录双击，或 cmd/PowerShell 里: .\scripts\build_win.bat

setlocal
set QT_PREFIX=C:\Qt\Qt5.12.8\5.12.8\mingw73_64
set MINGW_BIN=C:\Qt\Qt5.12.8\Tools\mingw730_64\bin
set PATH=%MINGW_BIN%;%QT_PREFIX%\bin;%PATH%

cd /d %~dp0\..

if not exist "%MINGW_BIN%\g++.exe" (
  echo [错误] 找不到 MinGW: %MINGW_BIN%\g++.exe
  echo 请确认已安装 Qt 5.12.8 的 Tools\mingw730_64，或改本脚本里的 MINGW_BIN。
  exit /b 1
)
if not exist "%MINGW_BIN%\mingw32-make.exe" (
  echo [错误] 找不到 %MINGW_BIN%\mingw32-make.exe
  exit /b 1
)
if not exist "%QT_PREFIX%\bin\qmake.exe" (
  echo [错误] 找不到 Qt: %QT_PREFIX%
  exit /b 1
)

if not exist "3rdparty\install\mingw73_64\include\tiffio.h" (
  echo [错误] 缺少 3rdparty\install\mingw73_64 （libtiff）
  echo 请从 207_RCS_code\3rdparty\install\mingw73_64 拷贝到本工程。
  exit /b 1
)

echo 使用编译器: %MINGW_BIN%\g++.exe
where g++
where mingw32-make

if exist build_win rmdir /s /q build_win
mkdir build_win

cmake -S . -B build_win -G "MinGW Makefiles" ^
  -DCMAKE_MAKE_PROGRAM=%MINGW_BIN%\mingw32-make.exe ^
  -DCMAKE_C_COMPILER=%MINGW_BIN%\gcc.exe ^
  -DCMAKE_CXX_COMPILER=%MINGW_BIN%\g++.exe ^
  -DCMAKE_PREFIX_PATH=%QT_PREFIX% ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DRCS_ENABLE_MOSAIC=ON ^
  -DRCS_BUILD_GUI=ON
if errorlevel 1 exit /b 1

cmake --build build_win -j
if errorlevel 1 exit /b 1

echo.
echo 完成。可执行文件: build_win\bin\RCS.exe
echo 运行前 PATH 需含: %QT_PREFIX%\bin
endlocal
