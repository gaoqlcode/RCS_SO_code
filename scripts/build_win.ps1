# Qt MinGW 7.3 + bundled libtiff. Run from repo root: .\scripts\build_win.ps1
$ErrorActionPreference = "Stop"

$QtPrefix = "C:\Qt\Qt5.12.8\5.12.8\mingw73_64"
$MingwBin = "C:\Qt\Qt5.12.8\Tools\mingw730_64\bin"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$env:Path = "$MingwBin;$QtPrefix\bin;" + $env:Path

$Gpp = Join-Path $MingwBin "g++.exe"
$Gcc = Join-Path $MingwBin "gcc.exe"
$Make = Join-Path $MingwBin "mingw32-make.exe"
$TiffHdr = Join-Path $Root "3rdparty\install\mingw73_64\include\tiffio.h"

if (-not (Test-Path -LiteralPath $Gpp)) {
  throw "MinGW not found: $Gpp"
}
if (-not (Test-Path -LiteralPath $TiffHdr)) {
  throw "Missing 3rdparty libtiff. Copy from 207_RCS_code\3rdparty\install\mingw73_64"
}

Write-Host "g++  = $Gpp"
Write-Host "make = $Make"

if (Test-Path -LiteralPath "build_win") {
  Remove-Item -Recurse -Force "build_win"
}

& cmake -S . -B build_win -G "MinGW Makefiles" `
  "-DCMAKE_MAKE_PROGRAM=$Make" `
  "-DCMAKE_C_COMPILER=$Gcc" `
  "-DCMAKE_CXX_COMPILER=$Gpp" `
  "-DCMAKE_PREFIX_PATH=$QtPrefix" `
  "-DCMAKE_BUILD_TYPE=Release" `
  "-DRCS_ENABLE_MOSAIC=ON" `
  "-DRCS_BUILD_GUI=ON"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& cmake --build build_win -j
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "OK: build_win\bin\RCS.exe"
