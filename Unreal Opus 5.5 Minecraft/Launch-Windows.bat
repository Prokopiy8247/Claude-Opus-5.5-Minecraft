@echo off
setlocal

title Opus 5.5 Minecraft - Windows launcher
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Scripts\Launch-Windows.ps1"
set "LAUNCH_RESULT=%ERRORLEVEL%"

if not "%LAUNCH_RESULT%"=="0" (
  echo.
  echo Launch failed. Read the message above, then press any key to close this window.
  pause >nul
)

exit /b %LAUNCH_RESULT%
