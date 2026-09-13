@echo off
REM Builds dist\mingw\libtable.a from src\table.c
REM Requires MinGW-w64 (gcc, ar) on PATH.
setlocal

where gcc >nul 2>nul
if errorlevel 1 (
    echo ERROR: gcc not found on PATH. Install MinGW-w64 and add its bin\ directory to PATH.
    exit /b 1
)

REM Ask gcc what it actually targets instead of assuming - a machine with
REM only a 32-bit MinGW-w64 toolchain installed would otherwise silently
REM produce an x86 libtable.a that this script filed under dist\mingw\ as
REM if it were x64, which is exactly the kind of mismatch that broke the
REM MSVC side earlier.
for /f "delims=" %%A in ('gcc -dumpmachine') do set TRIPLE=%%A
set ARCH=
echo %TRIPLE% | findstr /b "x86_64" >nul && set ARCH=x64
echo %TRIPLE% | findstr /b "i686"   >nul && set ARCH=x86
if not defined ARCH (
    echo ERROR: Could not determine architecture from gcc -dumpmachine ^(%TRIPLE%^).
    echo Expected an x86_64-w64-mingw32-* or i686-w64-mingw32-* toolchain.
    exit /b 1
)

set ROOT=%~dp0..
set OUT=%ROOT%\dist\mingw\%ARCH%
if not exist "%OUT%" mkdir "%OUT%"

REM table.c is plain C, so this uses gcc (the C frontend), not g++. g++ would
REM technically still link since every declaration is wrapped in
REM extern "C", but compiling a C library as C++ is a fragile habit to get
REM into - gcc is the compiler that actually matches what this file is.
echo Compiling with gcc ...
gcc -c -O2 -std=c11 -Wall -Wextra -I "%ROOT%\include" "%ROOT%\src\table.c" -o "%OUT%\table.o"
if errorlevel 1 exit /b 1

echo Archiving ...
ar rcs "%OUT%\libtable.a" "%OUT%\table.o"
if errorlevel 1 exit /b 1

del "%OUT%\table.o" >nul 2>nul

echo Built %OUT%\libtable.a
endlocal
