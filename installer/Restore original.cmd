@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Restore original.ps1" -GameDirectory "%~dp0."
set "result=%errorlevel%"
echo.
pause
exit /b %result%
