# Qt MinGW 7.3 + bundled libtiff.
# Flow: build librcs_proc -> pack sdk/lib -> build full project (GUI/CLI).
# Run from repo root:  .\scripts\build_win.ps1
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
  throw "Missing 3rdparty libtiff (3rdparty\install\mingw73_64)"
}

Write-Host "g++  = $Gpp"
Write-Host "make = $Make"

function Stop-RcsProcs {
  foreach ($name in @("RCS", "rcs_cli", "rcs_gui_demo", "rcs_cli_demo")) {
    Get-Process -Name $name -ErrorAction SilentlyContinue | ForEach-Object {
      Write-Host "stop $($_.ProcessName) pid=$($_.Id)"
      Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue
    }
  }
}

function Remove-BuildWin {
  if (-not (Test-Path -LiteralPath "build_win")) { return }
  Stop-RcsProcs
  Start-Sleep -Milliseconds 500
  for ($i = 1; $i -le 5; $i++) {
    try {
      Remove-Item -LiteralPath "build_win" -Recurse -Force -ErrorAction Stop
      return
    } catch {
      Write-Host "remove build_win failed ($i/5): $($_.Exception.Message)"
      Stop-RcsProcs
      Start-Sleep -Seconds 1
    }
  }
  throw "Cannot remove build_win. Close RCS.exe / Explorer on bin, then retry."
}

Stop-RcsProcs
Remove-BuildWin

# Source build (mosaic ON). Do NOT use packaged SDK for this Windows product build.
& cmake -S . -B build_win -G "MinGW Makefiles" `
  "-DCMAKE_MAKE_PROGRAM=$Make" `
  "-DCMAKE_C_COMPILER=$Gcc" `
  "-DCMAKE_CXX_COMPILER=$Gpp" `
  "-DCMAKE_PREFIX_PATH=$QtPrefix" `
  "-DCMAKE_BUILD_TYPE=Release" `
  "-DRCS_USE_SDK=OFF" `
  "-DRCS_ENABLE_MOSAIC=ON" `
  "-DRCS_BUILD_GUI=ON" `
  "-DRCS_BUILD_CLI=ON"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "==== 1/2 build SDK lib (rcs_proc) ===="
& cmake --build build_win --target rcs_proc -j
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "==== pack sdk\lib ===="
$env:RCS_BUILD_DIR = "build_win"
& cmd /c "sdk\pack_sdk.bat"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "==== 2/2 build full project ===="
& cmake --build build_win -j
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "==== deploy (可发布运行包) ===="
$env:RCS_BUILD_DIR = "build_win"
& "$PSScriptRoot\deploy_win.ps1"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "OK: build_win\bin\RCS.exe"
Write-Host "OK: sdk\lib (packed)"
Write-Host "OK: deploy\win64\  (整目录可拷贝运行)"
Write-Host "run:  deploy\win64\RCS.exe"
