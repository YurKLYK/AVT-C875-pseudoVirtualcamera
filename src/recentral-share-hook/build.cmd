@echo off
setlocal

set "ROOT=%~dp0"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if not exist "%VSWHERE%" (
  echo Visual Studio Build Tools not found.
  exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
if not defined VSROOT (
  echo MSVC x86/x64 tools not found.
  exit /b 1
)

if not exist "%ROOT%.deps\minhook\include\MinHook.h" (
  echo Fetching MinHook...
  git clone --depth 1 https://github.com/TsudaKageyu/minhook.git "%ROOT%.deps\minhook"
  if errorlevel 1 exit /b 1
)

call "%VSROOT%\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64
if errorlevel 1 exit /b 1

if not exist "%ROOT%build" mkdir "%ROOT%build"

cl /nologo /O2 /W4 /EHsc /MT /DUNICODE /D_UNICODE /I"%ROOT%.deps\minhook\include" /LD ^
  "%ROOT%src\hook_dll.cpp" ^
  "%ROOT%.deps\minhook\src\buffer.c" ^
  "%ROOT%.deps\minhook\src\hook.c" ^
  "%ROOT%.deps\minhook\src\trampoline.c" ^
  "%ROOT%.deps\minhook\src\hde\hde32.c" ^
  /link /MACHINE:X86 user32.lib /OUT:"%ROOT%build\recentral_share_hook.dll"
if errorlevel 1 exit /b 1

cl /nologo /O2 /W4 /EHsc /MT /DUNICODE /D_UNICODE ^
  "%ROOT%src\injector.cpp" ^
  /link /MACHINE:X86 /OUT:"%ROOT%build\recentral_share_injector.exe"
if errorlevel 1 exit /b 1

cl /nologo /O2 /W4 /EHsc /MT /DUNICODE /D_UNICODE ^
  "%ROOT%src\hook_test.cpp" ^
  /link /MACHINE:X86 /OUT:"%ROOT%build\hook_test.exe"
if errorlevel 1 exit /b 1

echo.
echo Build completed: %ROOT%build
