@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install patch.ps1" -GameDirectory "%~dp0"
set "result=%errorlevel%"
echo.
if not "%result%"=="0" echo Installation did not complete. No unknown executable was accepted.
pause
exit /b %result%
