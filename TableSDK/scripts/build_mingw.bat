@echo off
REM Builds dist\mingw\libtable.a from src\table.cpp
REM Requires MinGW-w64 (g++, ar) on PATH.
setlocal

where g++ >nul 2>nul
if errorlevel 1 (
    echo ERROR: g++ not found on PATH. Install MinGW-w64 and add its bin\ directory to PATH.
    exit /b 1
)

set ROOT=%~dp0..
set OUT=%ROOT%\dist\mingw
if not exist "%OUT%" mkdir "%OUT%"

echo Compiling with g++ ...
g++ -c -O2 -std=c++17 -Wall -I "%ROOT%\include" "%ROOT%\src\table.cpp" -o "%OUT%\table.o"
if errorlevel 1 exit /b 1

echo Archiving ...
ar rcs "%OUT%\libtable.a" "%OUT%\table.o"
if errorlevel 1 exit /b 1

del "%OUT%\table.o" >nul 2>nul

echo Built %OUT%\libtable.a
endlocal
