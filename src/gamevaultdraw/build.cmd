@echo off
setlocal

if not defined GV_VS (
  for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "GV_VS=%%I"
)
if not exist "%GV_VS%\Common7\Tools\VsDevCmd.bat" (
  echo ERROR: Visual Studio 2022 C++ Build Tools not found. Set GV_VS to its installation directory.
  exit /b 1
)

call "%GV_VS%\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%

msbuild "%~dp0GameVaultDraw.vcxproj" /m /t:Rebuild /p:Configuration=Release /p:Platform=Win32 /v:minimal
exit /b %errorlevel%
