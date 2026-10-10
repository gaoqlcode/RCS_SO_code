@echo off
REM Copy Windows rcs_proc DLL into sdk\lib. Headers stay in sdk\include (no mosaic).
REM MinGW usually names it librcs_proc.dll (lib prefix).
setlocal
cd /d %~dp0\..

set BUILD=build_win
if not "%RCS_BUILD_DIR%"=="" set BUILD=%RCS_BUILD_DIR%

set DLL=
for %%P in (
  "%BUILD%\lib\librcs_proc.dll"
  "%BUILD%\bin\librcs_proc.dll"
  "%BUILD%\lib\rcs_proc.dll"
  "%BUILD%\bin\rcs_proc.dll"
) do if exist %%~P if "%DLL%"=="" set DLL=%%~P

if "%DLL%"=="" (
  echo [ERROR] librcs_proc.dll / rcs_proc.dll not found under %BUILD%
  echo Looked in: %BUILD%\lib  and  %BUILD%\bin
  dir /s /b "%BUILD%\*rcs_proc*.dll" 2>nul
  exit /b 1
)

if not exist sdk\include\rcs\rcs_api.h (
  echo [ERROR] missing sdk\include\rcs\rcs_api.h
  exit /b 1
)

if not exist sdk\lib mkdir sdk\lib

REM keep MinGW name; also drop a rcs_proc.dll copy for find_library
for %%F in ("%DLL%") do (
  copy /Y "%DLL%" "sdk\lib\%%~nxF" >nul
  copy /Y "%DLL%" "sdk\lib\rcs_proc.dll" >nul
  copy /Y "%DLL%" "sdk\lib\librcs_proc.dll" >nul
)

for %%P in (
  "%BUILD%\lib\librcs_proc.dll.a"
  "%BUILD%\bin\librcs_proc.dll.a"
  "%BUILD%\lib\rcs_proc.dll.a"
  "%BUILD%\bin\rcs_proc.dll.a"
) do if exist %%~P copy /Y %%~P sdk\lib\ >nul

echo packed: %DLL% -^> sdk\lib
dir /b sdk\lib
endlocal
