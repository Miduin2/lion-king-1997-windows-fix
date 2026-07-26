@echo off
setlocal

if not defined GV_VS (
  for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "GV_VS=%%I"
)
set "GV_TEST=%~dp0build\integration-current"
if not exist "%GV_TEST%" mkdir "%GV_TEST%"

if not exist "%GV_VS%\Common7\Tools\VsDevCmd.bat" (
  echo ERROR: Visual Studio 2022 C++ Build Tools not found. Set GV_VS to its installation directory.
  exit /b 1
)

call "%GV_VS%\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%

cl /nologo /W4 /WX /O2 /MT /DUNICODE /D_UNICODE "%~dp0tests\stub_game.cpp" /link /SUBSYSTEM:WINDOWS /OUT:"%GV_TEST%\LIONW.EXE"
if errorlevel 1 exit /b %errorlevel%

copy /y "%~dp0build\Win32\Release\Play Lion King.exe" "%GV_TEST%\Play Lion King.exe" >nul
copy /y "%~dp0..\gamevaultdraw\build\Win32\Release\ddraw.dll" "%GV_TEST%\ddraw.dll" >nul
del /q "%GV_TEST%\launcher-stub-result.txt" "%GV_TEST%\GameVaultLauncher.log" 2>nul

start "" /wait "%GV_TEST%\Play Lion King.exe"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoProfile -Command "$result = Get-Content -LiteralPath '%GV_TEST%\launcher-stub-result.txt' -Encoding Unicode; if ($result[0] -notmatch '^module=([A-Z]):\\LIONW\.EXE$') { throw 'The stub was not launched through a short drive path.' }; $drive = $Matches[1] + ':\'; if (Test-Path -LiteralPath $drive) { throw ('The temporary drive still exists: ' + $drive) }; $log = (Get-Content -LiteralPath '%GV_TEST%\GameVaultLauncher.log') -join [Environment]::NewLine; if ($log -notmatch 'Game exited code=0' -or $log -notmatch 'Cleanup complete') { throw 'The launcher did not finish its cleanup sequence.' }"
if errorlevel 1 exit /b %errorlevel%

echo GameVault Launcher integration test OK
exit /b 0
