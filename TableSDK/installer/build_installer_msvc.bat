@echo off
REM Builds installer.exe using MSVC (cl.exe + rc.exe).
REM Run from an "x64 Native Tools Command Prompt for VS".
setlocal
cd /d "%~dp0"

where cl >nul 2>nul
if errorlevel 1 (
    echo ERROR: cl.exe not found on PATH. Run from a Developer Command Prompt.
    exit /b 1
)

echo Compiling resources ...
rc /nologo /fo installer.res installer.rc
if errorlevel 1 exit /b 1

echo Compiling and linking installer.exe ...
cl /nologo /EHsc /O2 /std:c++17 installer.cpp installer.res ^
   /Fe:installer.exe ^
   /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib comctl32.lib shell32.lib advapi32.lib ole32.lib
if errorlevel 1 exit /b 1

del installer.obj installer.res >nul 2>nul
echo Built installer.exe
endlocal
