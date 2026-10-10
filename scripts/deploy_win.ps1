# Pack runnable Windows tree: deploy\win64\
# Called from build_win.ps1 after full build.
# Usage:  .\scripts\deploy_win.ps1
$ErrorActionPreference = "Stop"

$QtPrefix = if ($env:RCS_QT_PREFIX) { $env:RCS_QT_PREFIX } else { "C:\Qt\Qt5.12.8\5.12.8\mingw73_64" }
$MingwBin = if ($env:RCS_MINGW_BIN) { $env:RCS_MINGW_BIN } else { "C:\Qt\Qt5.12.8\Tools\mingw730_64\bin" }
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$Build = if ($env:RCS_BUILD_DIR) { $env:RCS_BUILD_DIR } else { "build_win" }
if (-not [System.IO.Path]::IsPathRooted($Build)) {
  $Build = Join-Path $Root $Build
}

$Out = Join-Path $Root "deploy\win64"
if (Test-Path -LiteralPath $Out) {
  Remove-Item -LiteralPath $Out -Recurse -Force
}
New-Item -ItemType Directory -Path $Out | Out-Null

function Find-File([string[]]$cands) {
  foreach ($c in $cands) {
    if (Test-Path -LiteralPath $c) { return $c }
  }
  return $null
}

$gui = Find-File @(
  (Join-Path $Build "bin\RCS.exe"),
  (Join-Path $Build "RCS.exe")
)
$cli = Find-File @(
  (Join-Path $Build "cli\rcs_cli.exe"),
  (Join-Path $Build "bin\rcs_cli.exe"),
  (Join-Path $Build "rcs_cli.exe")
)
$dll = Find-File @(
  (Join-Path $Build "lib\librcs_proc.dll"),
  (Join-Path $Build "bin\librcs_proc.dll"),
  (Join-Path $Build "lib\rcs_proc.dll"),
  (Join-Path $Build "bin\rcs_proc.dll")
)

if (-not $gui -and -not $cli) {
  throw "deploy: RCS.exe / rcs_cli.exe not found under $Build"
}
if (-not $dll) {
  throw "deploy: librcs_proc.dll not found under $Build"
}

if ($gui) { Copy-Item -LiteralPath $gui -Destination (Join-Path $Out "RCS.exe") -Force }
if ($cli) { Copy-Item -LiteralPath $cli -Destination (Join-Path $Out "rcs_cli.exe") -Force }
Copy-Item -LiteralPath $dll -Destination (Join-Path $Out "librcs_proc.dll") -Force
# alias name some loaders look for
Copy-Item -LiteralPath $dll -Destination (Join-Path $Out "rcs_proc.dll") -Force

# MinGW runtime next to exe
foreach ($name in @("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")) {
  $src = Join-Path $MingwBin $name
  if (Test-Path -LiteralPath $src) {
    Copy-Item -LiteralPath $src -Destination (Join-Path $Out $name) -Force
  } else {
    Write-Host "warn: missing MinGW runtime: $src"
  }
}

# Qt runtime via windeployqt
$windeploy = Join-Path $QtPrefix "bin\windeployqt.exe"
$env:Path = "$MingwBin;$QtPrefix\bin;" + $env:Path

if ($gui -and (Test-Path -LiteralPath $windeploy)) {
  $exe = Join-Path $Out "RCS.exe"
  & $windeploy --release --no-translations --no-system-d3d-compiler --no-angle --no-opengl-sw $exe
  if ($LASTEXITCODE -ne 0) {
    Write-Host "warn: windeployqt exit $LASTEXITCODE"
  }
} elseif ($gui) {
  Write-Host "warn: windeployqt not found: $windeploy"
}

# CLI 若也链了 Qt（一般没有）可再跑一次；本工程 CLI 不链 Qt

$readme = @"
雷达数据处理软件 — Windows 发布目录（win64）

本目录可整夹拷贝到无开发环境的电脑运行。

  RCS.exe          图形界面
  rcs_cli.exe      命令行
  librcs_proc.dll  处理库（另有 rcs_proc.dll 同内容）
  Qt5*.dll / platforms / ...   Qt 运行时（windeployqt）
  libgcc / libstdc++ / libwinpthread   MinGW 运行时

用法:
  双击 RCS.exe
  或命令行: rcs_cli.exe --help

要求: 64-bit Windows。勿单独挪走 exe 而不带同目录 DLL。
生成自: $Build
"@
Set-Content -LiteralPath (Join-Path $Out "README.txt") -Value $readme -Encoding UTF8

Write-Host ""
Write-Host "OK: deploy -> $Out"
Get-ChildItem -LiteralPath $Out | Select-Object -First 25 | ForEach-Object { Write-Host ("  " + $_.Name) }
Write-Host "run:  $Out\RCS.exe"
