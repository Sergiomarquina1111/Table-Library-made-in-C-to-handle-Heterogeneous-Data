@echo off
REM One-stop build script. Run it from whatever prompt you have open:
REM   - "x64 Native Tools Command Prompt for VS"  -> builds MSVC x64 table.lib
REM   - "x86 Native Tools Command Prompt for VS"  -> builds MSVC x86 table.lib
REM   - a shell with MinGW-w64 gcc on PATH        -> builds MinGW (x64 or x86,
REM                                                   whichever that gcc targets)
REM
REM It auto-detects whichever of cl.exe / gcc.exe is available, builds
REM table.c for that toolchain+architecture, copies the result straight into
REM payload\ itself (this is the step that kept getting missed doing it by
REM hand), then builds installer.exe from whatever is currently in payload\.
REM
REM Run it again from a different prompt to add another architecture/
REM toolchain - it only ever ADDS to payload\, never removes what's already
REM there, so re-running from the x86 prompt after the x64 one just fills in
REM the 32-bit variant alongside the 64-bit one you already built.
setlocal enabledelayedexpansion
cd /d "%~dp0"

set BUILT_MSVC=0
set BUILT_MINGW=0

REM ---------------------------------------------------------------- MSVC ---
where cl >nul 2>nul
if errorlevel 1 (
    echo [MSVC]  cl.exe not found on PATH - skipping.
) else (
    if not defined VSCMD_ARG_TGT_ARCH (
        echo [MSVC]  cl.exe found, but this isn't a proper Developer Command
        echo         Prompt ^(VSCMD_ARG_TGT_ARCH isn't set^) - skipping.
        echo         Run from "x64/x86 Native Tools Command Prompt for VS".
    ) else (
        echo [MSVC]  Building table.lib for %VSCMD_ARG_TGT_ARCH% ...
        if not exist "dist\msvc\%VSCMD_ARG_TGT_ARCH%" mkdir "dist\msvc\%VSCMD_ARG_TGT_ARCH%"
        cl /nologo /c /O2 /std:c11 /W4 /I include src\table.c /Fo:"dist\msvc\%VSCMD_ARG_TGT_ARCH%\table.obj"
        if errorlevel 1 (
            echo [MSVC]  Compile failed - see errors above.
        ) else (
            lib /nologo /OUT:"dist\msvc\%VSCMD_ARG_TGT_ARCH%\table.lib" "dist\msvc\%VSCMD_ARG_TGT_ARCH%\table.obj" >nul
            del "dist\msvc\%VSCMD_ARG_TGT_ARCH%\table.obj" >nul 2>nul
            if not exist "payload\lib\msvc\%VSCMD_ARG_TGT_ARCH%" mkdir "payload\lib\msvc\%VSCMD_ARG_TGT_ARCH%"
            copy /Y "dist\msvc\%VSCMD_ARG_TGT_ARCH%\table.lib" "payload\lib\msvc\%VSCMD_ARG_TGT_ARCH%\table.lib" >nul
            echo [MSVC]  Built and staged payload\lib\msvc\%VSCMD_ARG_TGT_ARCH%\table.lib
            set BUILT_MSVC=1
        )
    )
)

REM --------------------------------------------------------------- MinGW ---
where gcc >nul 2>nul
if errorlevel 1 (
    echo [MinGW] gcc not found on PATH - skipping.
) else (
    set MTRIPLE=
    for /f "delims=" %%A in ('gcc -dumpmachine') do set MTRIPLE=%%A
    set MARCH=
    echo !MTRIPLE! | findstr /b "x86_64" >nul && set MARCH=x64
    echo !MTRIPLE! | findstr /b "i686"   >nul && set MARCH=x86
    if not defined MARCH (
        echo [MinGW] Could not tell architecture from gcc -dumpmachine ^(!MTRIPLE!^) - skipping.
    ) else (
        echo [MinGW] Building libtable.a for !MARCH! ...
        if not exist "dist\mingw\!MARCH!" mkdir "dist\mingw\!MARCH!"
        gcc -c -O2 -std=c11 -Wall -Wextra -I include src\table.c -o "dist\mingw\!MARCH!\table.o"
        if errorlevel 1 (
            echo [MinGW] Compile failed - see errors above.
        ) else (
            ar rcs "dist\mingw\!MARCH!\libtable.a" "dist\mingw\!MARCH!\table.o"
            del "dist\mingw\!MARCH!\table.o" >nul 2>nul
            if not exist "payload\lib\mingw\!MARCH!" mkdir "payload\lib\mingw\!MARCH!"
            copy /Y "dist\mingw\!MARCH!\libtable.a" "payload\lib\mingw\!MARCH!\libtable.a" >nul
            echo [MinGW] Built and staged payload\lib\mingw\!MARCH!\libtable.a
            set BUILT_MINGW=1
        )
    )
)

if "%BUILT_MSVC%%BUILT_MINGW%"=="00" (
    echo.
    echo Nothing built - no cl.exe or gcc.exe usable on PATH. Nothing to embed
    echo into installer.exe, so stopping here rather than producing a build
    echo with no new content.
    exit /b 1
)

REM ------------------------------------------------------- installer.exe ---
echo.
echo Building installer.exe from whatever is currently staged in payload\ ...
copy /Y include\table.h payload\include\table.h >nul

where cl >nul 2>nul
if not errorlevel 1 (
    rc /nologo /fo installer.res installer.rc
    if errorlevel 1 exit /b 1
    cl /nologo /EHsc /O2 /std:c++17 installer.cpp installer.res ^
       /Fe:installer.exe ^
       /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib comctl32.lib shell32.lib advapi32.lib ole32.lib
    if errorlevel 1 exit /b 1
    del installer.obj installer.res >nul 2>nul
) else (
    where g++ >nul 2>nul
    if not errorlevel 1 (
        windres installer.rc -O coff -o installer_res.o
        if errorlevel 1 exit /b 1
        g++ -O2 -std=c++17 -static -static-libgcc -static-libstdc++ ^
            installer.cpp installer_res.o -o installer.exe ^
            -mwindows -lcomctl32 -lshell32 -ladvapi32 -lole32
        if errorlevel 1 exit /b 1
        del installer_res.o >nul 2>nul
    ) else (
        echo ERROR: Neither cl.exe nor g++ available to build installer.exe itself.
        exit /b 1
    )
)

echo.
echo Built installer.exe
echo.
echo Currently embedded in it:
call :ReportLib "payload\lib\msvc\x64\table.lib"     "MSVC  x64"
call :ReportLib "payload\lib\msvc\x86\table.lib"     "MSVC  x86"
call :ReportLib "payload\lib\mingw\x64\libtable.a"   "MinGW x64"
call :ReportLib "payload\lib\mingw\x86\libtable.a"   "MinGW x86"
echo.
echo To add another architecture: open a different prompt (e.g. the other
echo Native Tools Command Prompt, or a shell with a different-arch MinGW on
echo PATH) and run this same script again - it only adds to payload\, it
echo never removes what's already staged there.

endlocal
exit /b 0

:ReportLib
if exist %1 (
    for %%F in (%1) do (
        if %%~zF gtr 0 (echo   [x] %~2) else (echo   [ ] %~2  ^(0 bytes - not built yet^))
    )
) else (
    echo   [ ] %~2  ^(missing^)
)
exit /b 0
