@echo off
REM Qt MinGW 7.3 + bundled libtiff
REM Flow: build rcs_proc -> pack sdk\lib -> build full project
REM Usage: from repo root  scripts\build_win.bat

setlocal
set QT_PREFIX=C:\Qt\Qt5.12.8\5.12.8\mingw73_64
set MINGW_BIN=C:\Qt\Qt5.12.8\Tools\mingw730_64\bin
set PATH=%MINGW_BIN%;%QT_PREFIX%\bin;%PATH%

cd /d %~dp0\..

if not exist "%MINGW_BIN%\g++.exe" (
  echo [ERROR] MinGW not found: %MINGW_BIN%\g++.exe
  exit /b 1
)
if not exist "%MINGW_BIN%\mingw32-make.exe" (
  echo [ERROR] mingw32-make not found
  exit /b 1
)
if not exist "%QT_PREFIX%\bin\qmake.exe" (
  echo [ERROR] Qt not found: %QT_PREFIX%
  exit /b 1
)
if not exist "3rdparty\install\mingw73_64\include\tiffio.h" (
  echo [ERROR] missing 3rdparty\install\mingw73_64
  exit /b 1
)

echo g++ = %MINGW_BIN%\g++.exe

taskkill /F /IM RCS.exe >nul 2>&1
taskkill /F /IM rcs_cli.exe >nul 2>&1
timeout /t 1 /nobreak >nul

if exist build_win rmdir /s /q build_win
if exist build_win (
  echo [ERROR] cannot remove build_win - close RCS.exe first
  exit /b 1
)

cmake -S . -B build_win -G "MinGW Makefiles" ^
  -DCMAKE_MAKE_PROGRAM=%MINGW_BIN%\mingw32-make.exe ^
  -DCMAKE_C_COMPILER=%MINGW_BIN%\gcc.exe ^
  -DCMAKE_CXX_COMPILER=%MINGW_BIN%\g++.exe ^
  -DCMAKE_PREFIX_PATH=%QT_PREFIX% ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DRCS_USE_SDK=OFF ^
  -DRCS_ENABLE_MOSAIC=ON ^
  -DRCS_BUILD_GUI=ON ^
  -DRCS_BUILD_CLI=ON
if errorlevel 1 exit /b 1

echo.
echo ==== 1/2 build SDK lib (rcs_proc) ====
cmake --build build_win --target rcs_proc -j
if errorlevel 1 exit /b 1

echo.
echo ==== pack sdk\lib ====
set RCS_BUILD_DIR=build_win
call sdk\pack_sdk.bat
if errorlevel 1 exit /b 1

echo.
echo ==== 2/2 build full project ====
cmake --build build_win -j
if errorlevel 1 exit /b 1

echo.
echo ==== deploy (可发布运行包) ====
set RCS_BUILD_DIR=build_win
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0deploy_win.ps1"
if errorlevel 1 exit /b 1

echo.
echo OK: build_win\bin\RCS.exe
echo OK: sdk\lib packed
echo OK: deploy\win64\  (整目录可拷贝运行)
echo run: deploy\win64\RCS.exe
endlocal
