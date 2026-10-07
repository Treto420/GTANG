@echo off
rem Rebuilds GtaBeamD1.asi with Visual Studio 2022. Only needed if you want to build it yourself;
rem the kit already has a ready GtaBeamD1.asi.
rem Needs ScriptHookV.lib from the official Script Hook V SDK (dev-c.com, "Script Hook V SDK"):
rem put it at sdk\lib\ScriptHookV.lib next to this file.
setlocal
cd /d "%~dp0"
if not exist "sdk\lib\ScriptHookV.lib" (
  echo Missing sdk\lib\ScriptHookV.lib - get it from the Script Hook V SDK at dev-c.com
  exit /b 1
)
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH (
  echo Visual Studio C++ tools not found
  exit /b 1
)
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist build mkdir build
cl /nologo /std:c++17 /O2 /EHsc /MT /W3 /LD /D_CRT_SECURE_NO_WARNINGS GtaBeamD1.cpp carmodel.cpp /Fo:build\ /Fe:build\GtaBeamD1.asi /link /LIBPATH:sdk\lib user32.lib ws2_32.lib
if errorlevel 1 (
  echo BUILD FAILED
  exit /b 1
)
echo Built build\GtaBeamD1.asi
