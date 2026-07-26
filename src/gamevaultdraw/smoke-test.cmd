@echo off
setlocal

if not defined GV_VS (
  for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "GV_VS=%%I"
)
set "GV_OUT=%~dp0build\Win32\Release"

if not exist "%GV_VS%\Common7\Tools\VsDevCmd.bat" (
  echo ERROR: Visual Studio 2022 C++ Build Tools not found. Set GV_VS to its installation directory.
  exit /b 1
)

if not exist "%GV_OUT%\ddraw.dll" (
  echo ERROR: Build ddraw.dll before running the smoke test.
  exit /b 1
)

call "%GV_VS%\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%

cl /nologo /W4 /WX /EHsc /std:c++17 /MT "%~dp0tests\smoke_test.cpp" /Fe:"%GV_OUT%\smoke_test.exe" /link user32.lib
if errorlevel 1 exit /b %errorlevel%

pushd "%GV_OUT%"
smoke_test.exe
set "GV_RESULT=%ERRORLEVEL%"
popd
exit /b %GV_RESULT%
