@echo off
REM One-stop build script. Run it from whatever prompt you have open:
REM   - "x64 Native Tools Command Prompt for VS"  -> builds MSVC x64 table.lib
REM   - "x86 Native Tools Command Prompt for VS"  -> builds MSVC x86 table.lib
REM   - any plain cmd.exe                          -> builds MinGW x64 AND/OR
REM                                                   MinGW x86, using the
REM                                                   fixed install paths below
REM
REM MinGW toolchains are called by FULL PATH, not via PATH/-dumpmachine
REM detection. That detection was the thing that broke: the classic
REM mingw.org gcc at C:\MinGW\bin reports "mingw32" from -dumpmachine
REM instead of "i686-w64-mingw32", so the old prefix-matching logic
REM couldn't tell what architecture it was and skipped it. Since we
REM already know which architecture lives at which path, we just say
REM so directly instead of asking gcc to confirm it.
REM
REM Edit these two lines if your installs live somewhere else:
set MINGW64_BIN=C:\mingw64\bin
set MINGW32_BIN=C:\MinGW\bin
REM
REM It builds table.c for whichever toolchain+architecture is available,
REM copies the result straight into payload\ itself (this is the step
REM that kept getting missed doing it by hand), then builds
REM installer.exe from whatever is currently in payload\.
REM
REM Run it again from a different prompt (e.g. the MSVC one) to add
REM another architecture/toolchain - it only ever ADDS to payload\,
REM never removes what's already there.
setlocal enabledelayedexpansion
cd /d "%~dp0"

REM source\ is the canonical copy of table.h/table.c - this keeps
REM include\ and src\ (what actually gets compiled below) in sync with
REM it automatically, so a fix made in source\table.h can never get
REM silently left out of a build because someone forgot the manual
REM copy step. Same idea as the existing include\table.h -^> payload\
REM sync further down - just extended one hop further back.
if exist source\table.h copy /Y source\table.h include\table.h >nul
if exist source\table.c copy /Y source\table.c src\table.c >nul

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
        REM table.h auto-links via #pragma comment(lib, "table.lib") for
        REM _M_X64 but "table_x86.lib" for _M_IX86 - two different names,
        REM specifically so both architectures' libs can coexist on LIB
        REM without link.exe grabbing the wrong one by name collision.
        REM This step must produce a file matching whichever name the
        REM header will actually ask for on this architecture, or the
        REM auto-link fails at link time with LNK1104 even though the
        REM compile itself succeeds.
        if "%VSCMD_ARG_TGT_ARCH%"=="x86" (
            set MSVC_LIBNAME=table_x86.lib
        ) else (
            set MSVC_LIBNAME=table.lib
        )
        echo [MSVC]  Building !MSVC_LIBNAME! for %VSCMD_ARG_TGT_ARCH% ...
        if not exist "dist\msvc\%VSCMD_ARG_TGT_ARCH%" mkdir "dist\msvc\%VSCMD_ARG_TGT_ARCH%"
        cl /nologo /c /O2 /std:c11 /W4 /I include src\table.c /Fo:"dist\msvc\%VSCMD_ARG_TGT_ARCH%\table.obj"
        if errorlevel 1 (
            echo [MSVC]  Compile failed - see errors above.
        ) else (
            lib /nologo /OUT:"dist\msvc\%VSCMD_ARG_TGT_ARCH%\!MSVC_LIBNAME!" "dist\msvc\%VSCMD_ARG_TGT_ARCH%\table.obj" >nul
            del "dist\msvc\%VSCMD_ARG_TGT_ARCH%\table.obj" >nul 2>nul
            if not exist "payload\lib\msvc\%VSCMD_ARG_TGT_ARCH%" mkdir "payload\lib\msvc\%VSCMD_ARG_TGT_ARCH%"
            copy /Y "dist\msvc\%VSCMD_ARG_TGT_ARCH%\!MSVC_LIBNAME!" "payload\lib\msvc\%VSCMD_ARG_TGT_ARCH%\!MSVC_LIBNAME!" >nul
            echo [MSVC]  Built and staged payload\lib\msvc\%VSCMD_ARG_TGT_ARCH%\!MSVC_LIBNAME!
            set BUILT_MSVC=1
        )
    )
)

REM ------------------------------------------------------- MinGW x64 -----
REM Output is named libtable64.a (not libtable.a) so that building both
REM architectures in the same run can never have one overwrite the other
REM in a shared folder, and so a lib picked up from the wrong folder by
REM accident is obviously wrong by its filename alone.
if exist "%MINGW64_BIN%\gcc.exe" (
    echo [MinGW] Found gcc at %MINGW64_BIN% - treating as x64.
    if not exist "dist\mingw\x64" mkdir "dist\mingw\x64"
    "%MINGW64_BIN%\gcc.exe" -c -O2 -std=c11 -Wall -Wextra -I include src\table.c -o "dist\mingw\x64\table64.o"
    if errorlevel 1 (
        echo [MinGW] x64 compile failed - see errors above.
    ) else (
        "%MINGW64_BIN%\ar.exe" rcs "dist\mingw\x64\libtable64.a" "dist\mingw\x64\table64.o"
        del "dist\mingw\x64\table64.o" >nul 2>nul
        if not exist "payload\lib\mingw\x64" mkdir "payload\lib\mingw\x64"
        copy /Y "dist\mingw\x64\libtable64.a" "payload\lib\mingw\x64\libtable64.a" >nul
        echo [MinGW] Built and staged payload\lib\mingw\x64\libtable64.a
        set BUILT_MINGW=1
    )
) else (
    echo [MinGW] No gcc.exe at %MINGW64_BIN% - skipping x64.
)

REM ------------------------------------------------------- MinGW x86 -----
REM Same reasoning: libtable32.a, distinct from the x64 output above.
if exist "%MINGW32_BIN%\gcc.exe" (
    echo [MinGW] Found gcc at %MINGW32_BIN% - treating as x86.
    if not exist "dist\mingw\x86" mkdir "dist\mingw\x86"
    "%MINGW32_BIN%\gcc.exe" -c -O2 -std=c11 -Wall -Wextra -I include src\table.c -o "dist\mingw\x86\table32.o"
    if errorlevel 1 (
        echo [MinGW] x86 compile failed - see errors above.
    ) else (
        "%MINGW32_BIN%\ar.exe" rcs "dist\mingw\x86\libtable32.a" "dist\mingw\x86\table32.o"
        del "dist\mingw\x86\table32.o" >nul 2>nul
        if not exist "payload\lib\mingw\x86" mkdir "payload\lib\mingw\x86"
        copy /Y "dist\mingw\x86\libtable32.a" "payload\lib\mingw\x86\libtable32.a" >nul
        echo [MinGW] Built and staged payload\lib\mingw\x86\libtable32.a
        set BUILT_MINGW=1
    )
) else (
    echo [MinGW] No gcc.exe at %MINGW32_BIN% - skipping x86.
)

if "%BUILT_MSVC%%BUILT_MINGW%"=="00" (
    echo.
    echo Nothing built - no cl.exe usable on PATH and no gcc.exe found at
    echo %MINGW64_BIN% or %MINGW32_BIN%. Nothing to embed into installer.exe,
    echo so stopping here rather than producing a build with no new content.
    exit /b 1
)

REM ------------------------------------------------------- wrapper scripts ---
REM These let a consumer pick an architecture by COMMAND NAME instead of by
REM checking LIBRARY_PATH/PATH order or running `gcc -dumpmachine` - each
REM wrapper hardcodes its own compiler's full path plus explicit -I/-L/-l
REM flags pointed at that one architecture's install folder, so it's
REM unambiguous no matter what else is on PATH or LIBRARY_PATH, and no
REM matter which/how many MinGW installs are present. They target the
REM install location the installer actually deploys to
REM (%%LOCALAPPDATA%%\TableSDK by default) - that env var is expanded at
REM the time the wrapper itself runs, not now, so it correctly resolves
REM per-user on whatever machine ends up running it.
REM
REM Always (re)generated here, independent of what this particular build.bat
REM run found/built - a wrapper for a toolchain the end user doesn't have
REM installed simply won't be invoked; that's fine and expected.
echo.
echo Generating architecture-specific wrapper scripts (gcc32/gcc64/g++32/g++64) ...
if not exist "payload\bin" mkdir "payload\bin"

(
    echo @echo off
    echo REM Always builds with the 32-bit MinGW toolchain and the matching
    echo REM TableSDK x86 library - no LIBRARY_PATH/PATH dependency.
    echo "%MINGW32_BIN%\gcc.exe" %%* -I"%%LOCALAPPDATA%%\TableSDK\include" -L"%%LOCALAPPDATA%%\TableSDK\lib\mingw\x86" -ltable
) > "payload\bin\gcc32.bat"

(
    echo @echo off
    echo REM Always builds with the 64-bit MinGW toolchain and the matching
    echo REM TableSDK x64 library - no LIBRARY_PATH/PATH dependency.
    echo "%MINGW64_BIN%\gcc.exe" %%* -I"%%LOCALAPPDATA%%\TableSDK\include" -L"%%LOCALAPPDATA%%\TableSDK\lib\mingw\x64" -ltable
) > "payload\bin\gcc64.bat"

(
    echo @echo off
    echo REM Always builds with the 32-bit MinGW g++ and the matching
    echo REM TableSDK x86 library - no LIBRARY_PATH/PATH dependency.
    echo "%MINGW32_BIN%\g++.exe" %%* -I"%%LOCALAPPDATA%%\TableSDK\include" -L"%%LOCALAPPDATA%%\TableSDK\lib\mingw\x86" -ltable
) > "payload\bin\g++32.bat"

(
    echo @echo off
    echo REM Always builds with the 64-bit MinGW g++ and the matching
    echo REM TableSDK x64 library - no LIBRARY_PATH/PATH dependency.
    echo "%MINGW64_BIN%\g++.exe" %%* -I"%%LOCALAPPDATA%%\TableSDK\include" -L"%%LOCALAPPDATA%%\TableSDK\lib\mingw\x64" -ltable
) > "payload\bin\g++64.bat"

echo Staged payload\bin\gcc32.bat, gcc64.bat, g++32.bat, g++64.bat
echo NOTE: installer.cpp does not yet deploy payload\bin\ or add it to PATH -
echo       that still needs to be wired up there before these are usable
echo       from an install, not just from payload\bin\ directly.

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
    REM Prefer mingw64's g++ for building the installer itself if present,
    REM otherwise fall back to the classic MinGW's g++.
    set GPP=
    if exist "%MINGW64_BIN%\g++.exe" set GPP=%MINGW64_BIN%\g++.exe
    if not defined GPP if exist "%MINGW32_BIN%\g++.exe" set GPP=%MINGW32_BIN%\g++.exe

    if defined GPP (
        set WINDRES=%MINGW64_BIN%\windres.exe
        if not exist "!WINDRES!" set WINDRES=%MINGW32_BIN%\windres.exe
        "!WINDRES!" installer.rc -O coff -o installer_res.o
        if errorlevel 1 exit /b 1
        "!GPP!" -O2 -std=c++17 -static -static-libgcc -static-libstdc++ ^
            installer.cpp installer_res.o -o installer.exe ^
            -mwindows -lcomctl32 -lshell32 -ladvapi32 -lole32
        if errorlevel 1 exit /b 1
        del installer_res.o >nul 2>nul
    ) else (
        echo ERROR: Neither cl.exe nor g++.exe ^(at %MINGW64_BIN% or %MINGW32_BIN%^) available to build installer.exe itself.
        exit /b 1
    )
)

echo.
echo Built installer.exe
echo.
echo Currently embedded in it:
call :ReportLib "payload\lib\msvc\x64\table.lib"     "MSVC  x64"
call :ReportLib "payload\lib\msvc\x86\table_x86.lib" "MSVC  x86"
call :ReportLib "payload\lib\mingw\x64\libtable64.a" "MinGW x64"
call :ReportLib "payload\lib\mingw\x86\libtable32.a" "MinGW x86"
echo.
echo To add another architecture: open a different prompt (e.g. the MSVC
echo Native Tools Command Prompt) and run this same script again - it only
echo adds to payload\, it never removes what's already staged there.

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