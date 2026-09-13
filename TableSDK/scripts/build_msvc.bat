@echo off
REM Builds dist\msvc\x64\table.lib or dist\msvc\x86\table.lib from src\table.c,
REM depending on which prompt you run it from. Run it TWICE - once from
REM "x64 Native Tools Command Prompt for VS" and once from
REM "x86 Native Tools Command Prompt for VS" - to get both architectures.
setlocal

where cl >nul 2>nul
if errorlevel 1 (
    echo ERROR: cl.exe not found on PATH.
    echo Run this script from a "Developer Command Prompt for VS" ^(x64^).
    exit /b 1
)

REM VSCMD_ARG_TGT_ARCH is set by vcvarsall.bat/the Developer Command Prompt
REM shortcut you launched (x64 Native Tools -> x64, x86 Native Tools -> x86).
REM This is what was missing before: the script always wrote to dist\msvc\
REM regardless of which architecture cl.exe was actually targeting, so an
REM x86 build silently overwrote (or got overwritten by) the x64 one.
if not defined VSCMD_ARG_TGT_ARCH (
    echo ERROR: VSCMD_ARG_TGT_ARCH is not set - run this from a proper
    echo "x64 Native Tools Command Prompt for VS" or
    echo "x86 Native Tools Command Prompt for VS", not a plain cmd/PowerShell
    echo with cl.exe manually added to PATH.
    exit /b 1
)

set ROOT=%~dp0..
set OUT=%ROOT%\dist\msvc\%VSCMD_ARG_TGT_ARCH%
if not exist "%OUT%" mkdir "%OUT%"

REM table.c is plain C - compiled with cl's C mode (/std:c11), not as C++.
REM cl already treats a .c extension as C automatically; /std:c11 just pins
REM the language version explicitly so this doesn't silently drift onto
REM whatever cl's default happens to be on a future toolchain.
echo Compiling with cl.exe ...
cl /nologo /c /O2 /std:c11 /W4 /I "%ROOT%\include" "%ROOT%\src\table.c" /Fo:"%OUT%\table.obj"
if errorlevel 1 exit /b 1

echo Archiving ...
lib /nologo /OUT:"%OUT%\table.lib" "%OUT%\table.obj"
if errorlevel 1 exit /b 1

del "%OUT%\table.obj" >nul 2>nul

echo Built %OUT%\table.lib
endlocal
