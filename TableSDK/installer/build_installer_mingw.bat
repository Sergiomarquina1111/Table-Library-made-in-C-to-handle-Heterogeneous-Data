@echo off
REM Builds installer.exe using MinGW-w64 (g++ + windres).
setlocal
cd /d "%~dp0"

where g++ >nul 2>nul
if errorlevel 1 (
    echo ERROR: g++ not found on PATH. Install MinGW-w64 and add its bin\ directory to PATH.
    exit /b 1
)

echo Compiling resources ...
windres installer.rc -O coff -o installer_res.o
if errorlevel 1 exit /b 1

echo Compiling and linking installer.exe ...
REM -static / -static-libgcc / -static-libstdc++ avoid depending on
REM libgcc_s_seh-1.dll / libstdc++-6.dll / libwinpthread-1.dll at runtime -
REM without these, installer.exe fails on a machine without MinGW installed
REM with "libgcc_s_seh-1.dll was not found".
g++ -O2 -std=c++17 -static -static-libgcc -static-libstdc++ ^
    installer.cpp installer_res.o -o installer.exe ^
    -mwindows -lcomctl32 -lshell32 -ladvapi32 -lole32
if errorlevel 1 exit /b 1

del installer_res.o >nul 2>nul
echo Built installer.exe
endlocal
