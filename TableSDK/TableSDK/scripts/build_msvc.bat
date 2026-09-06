@echo off
REM Builds dist\msvc\table.lib from src\table.cpp
REM Run this from an "x64 Native Tools Command Prompt for VS" so cl.exe and lib.exe are on PATH.
setlocal

where cl >nul 2>nul
if errorlevel 1 (
    echo ERROR: cl.exe not found on PATH.
    echo Run this script from a "Developer Command Prompt for VS" ^(x64^).
    exit /b 1
)

set ROOT=%~dp0..
set OUT=%ROOT%\dist\msvc
if not exist "%OUT%" mkdir "%OUT%"

echo Compiling with cl.exe ...
cl /nologo /c /EHsc /O2 /std:c++17 /I "%ROOT%\include" "%ROOT%\src\table.cpp" /Fo:"%OUT%\table.obj"
if errorlevel 1 exit /b 1

echo Archiving ...
lib /nologo /OUT:"%OUT%\table.lib" "%OUT%\table.obj"
if errorlevel 1 exit /b 1

del "%OUT%\table.obj" >nul 2>nul

echo Built %OUT%\table.lib
endlocal
