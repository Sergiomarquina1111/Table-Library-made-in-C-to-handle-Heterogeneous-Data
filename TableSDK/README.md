# Table SDK

Packaging for the `table.h` / `table.cpp` generic-container library (dynamic
array, fixed stack, linked list, hash map) as a distributable SDK for
Windows (MSVC + MinGW-w64) and Linux.

## Layout

```
TableSDK/
├── include/table.h        # public header (now wrapped in extern "C" for C/C++ interop)
├── src/table.cpp          # implementation
├── scripts/
│   ├── build_msvc.bat     # -> dist/msvc/table.lib
│   ├── build_mingw.bat    # -> dist/mingw/libtable.a
│   └── build_linux.sh     # -> dist/linux/libtable.a
├── linux/
│   └── install.sh         # copies header+lib to /usr/local (or custom prefix)
├── installer/
│   ├── installer.cpp      # Win32 GUI installer (dialog + Win32 API only, no toolkit deps)
│   ├── installer.rc       # dialog resource + embedded manifest (ComCtl32 v6)
│   ├── app.manifest
│   ├── resource.h
│   ├── build_installer_msvc.bat
│   ├── build_installer_mingw.bat
│   └── payload/           # what installer.exe copies onto the user's machine
│       ├── include/table.h
│       └── lib/
│           ├── msvc/table.lib     (build on Windows with MSVC, see below)
│           └── mingw/libtable.a   (already built here, cross-compiled)
└── dist/                  # build output per toolchain
```

## 1. Build the library

Each toolchain produces its own static archive - static libs aren't binary
compatible across MSVC / MinGW / Linux, so you build once per target you
want to ship.

| Toolchain | Command                          | Output                  |
|-----------|-----------------------------------|--------------------------|
| MSVC      | `scripts\build_msvc.bat` (from a *Developer Command Prompt*) | `dist\msvc\table.lib` |
| MinGW-w64 | `scripts\build_mingw.bat`        | `dist\mingw\libtable.a` |
| Linux     | `scripts/build_linux.sh`         | `dist/linux/libtable.a` |

This repo already has a MinGW-w64 build done via cross-compilation and
copied into `installer/payload/lib/mingw/libtable.a` — verified to compile
and archive cleanly. **`table.lib` for MSVC still needs to be built on an
actual Windows machine with Visual Studio installed** (there's no way to
produce a real MSVC-ABI `.lib` from Linux) — run `build_msvc.bat`, then copy
the result into `installer/payload/lib/msvc/table.lib`.

## 2. Build the Windows installer (single self-contained .exe)

`table.h` and both static libs are now embedded **inside** `installer.exe`
as resources at compile time - the final artifact really is one file, no
folder needs to travel with it. This means `installer/payload/` is only
needed at *build* time by you; end users only ever see `installer.exe`.

From Windows, with the payload libs in place under `installer/payload/lib/`:

```
installer\build_installer_msvc.bat      (or build_installer_mingw.bat)
```

This produces a single `installer.exe` with `table.h`, `table.lib`, and
`libtable.a` baked directly into its resource section. Verified here (via
MinGW-w64 cross-compilation): the binary contains the real header text and
the real `libtable.a` archive data, compiles with zero warnings, and has no
dependency on any MinGW runtime DLL (`-static -static-libgcc
-static-libstdc++` are already in `build_installer_mingw.bat`) - just stock
Windows DLLs (`KERNEL32`, `USER32`, `SHELL32`, `COMCTL32`, `ADVAPI32`,
`ole32`, `msvcrt`).

**To distribute:** just hand out `installer.exe`. That's it - nothing else
needs to travel with it.

**Important - rebuild the installer whenever the payload changes.** Since
the files are now baked in at compile time instead of read from disk at
runtime, dropping a new `table.lib` into `payload/lib/msvc/` does nothing
on its own anymore - you must rerun `build_installer_*.bat` afterward so
the new bytes get embedded. (This is the trade-off versus the old
folder-based approach: one file to distribute, but a rebuild step whenever
the payload changes.)

If a payload file doesn't exist yet (e.g. no MSVC build available), the
`.rc` still needs *something* to embed at that resource id - use a 0-byte
placeholder file (already set up at `payload/lib/msvc/table.lib` until a
real MSVC build replaces it). The installer treats a 0-byte embedded
resource as "not built for this toolchain" and greys out that checkbox at
runtime.

## 3. What the installer does

- Asks for an install directory (defaults to `%LOCALAPPDATA%\TableSDK` -
  no admin rights needed).
- Auto-detects which lib(s) are embedded (checking for a non-zero-size
  resource) and pre-checks/enables the matching checkbox (MSVC / MinGW),
  disabling whichever one wasn't built in.
- Copies `table.h` and the selected `.lib`/`.a` file(s) into the chosen
  directory.
- Updates the **current user's** environment variables in the registry
  (`HKEY_CURRENT_USER\Environment`) so a fresh terminal picks things up
  with no manual `-I`/`-L` flags:
  - MSVC: appends to `INCLUDE` and `LIB`
  - MinGW: appends to `C_INCLUDE_PATH`, `CPLUS_INCLUDE_PATH`, and `LIBRARY_PATH`
  - Broadcasts `WM_SETTINGCHANGE` so new processes see the change without
    a reboot (existing open terminals still need to be restarted).
- After install, a user's own code just does:
  ```c
  #include <table.h>
  ```
  and links with `table.lib` (MSVC) or `-ltable` (MinGW), no extra include
  path flags required.

## 4. Linux install

```
./scripts/build_linux.sh        # builds dist/linux/libtable.a
sudo ./linux/install.sh          # installs to /usr/local by default
# or: sudo ./linux/install.sh /opt/tablesdk
```

Then anywhere:
```c
#include <table.h>
```
```
g++ your_program.cpp -ltable -o your_program
```

## Notes / things worth knowing

- The header was changed to wrap all declarations in
  `#ifdef __cplusplus extern "C" { ... } #endif` so the library links
  correctly whether the *caller's* translation unit is compiled as C or
  C++. This was the one source change made for SDK purposes - nothing
  else in `table.h`/`table.cpp` was touched.
- The installer uses only ANSI (`...A`) Win32 APIs for simplicity -
  it won't correctly handle non-ASCII characters in install paths. Flag
  if you need full Unicode path support and I'll switch it to the `W`
  APIs + wide strings.
- The installer requests `asInvoker` execution (no UAC prompt) since it
  only writes under the user's own profile and `HKCU`. If you'd rather
  install to `C:\Program Files\...` for all users, that needs
  `HKEY_LOCAL_MACHINE` writes and admin elevation - let me know if you
  want that variant instead.
- If you build the installer with MinGW yourself, use
  `build_installer_mingw.bat` as-is rather than a bare `g++` command - it
  passes `-static -static-libgcc -static-libstdc++` so `installer.exe`
  doesn't depend on `libgcc_s_seh-1.dll` / `libstdc++-6.dll` at runtime.
  Without those flags the installer fails on any machine that doesn't
  happen to have MinGW's runtime DLLs on `PATH`, with an error like
  *"libgcc_s_seh-1.dll was not found"*. Verified fixed: `objdump -p
  installer.exe` now shows only system DLLs (`KERNEL32`, `USER32`,
  `SHELL32`, `COMCTL32`, `ADVAPI32`, `ole32`, `msvcrt`) and nothing from
  MinGW.
- A few pre-existing `-Wstrict-aliasing` warnings show up when compiling
  `table.cpp` (in `iSumIntStack`, `dSumFloatStack`, `bMinIntStack`,
  `bMaxIntStack`, and the `GET_STACK_VALUE` macro) - these aren't new,
  they're in the original code, and didn't block the build, but worth a
  look separately if you want it fully clean.

## 5. Zero-friction usage: no manual link flags, no pragma in user code

The goal is that an end user, after running the installer, can do exactly:

```
# MSVC
cl HelloTable.cpp

# GCC / Clang (MinGW)
g++ HelloTable.cpp -ltable -o HelloTable.exe
```

with **no** `#pragma comment(lib, "table.lib")` in their own source and no
manual `/link` or `-L` flags. Two things make that work:

- **MSVC:** `table.h` itself now contains
  ```c
  #ifdef _MSC_VER
  #pragma comment(lib, "table.lib")
  #endif
  ```
  right after the include guard. This is a standard MSVC auto-link
  mechanism (the same trick headers like `<winsock2.h>` use for
  `ws2_32.lib`) - it embeds a linker directive into the user's own `.obj`
  file saying "link against table.lib", and `link.exe` resolves that name
  by searching the paths in the `LIB` environment variable - which the
  installer already sets. So the user never writes the pragma themselves;
  `#include <table.h>` carries it in for them. Verified: compiling
  `table.cpp` itself under GCC (native and MinGW-target) with this change
  produces zero warnings and zero pragma-related output - `_MSC_VER` is
  undefined there, so GCC/Clang never even see the line.
- **GCC/Clang:** `#pragma comment(lib,...)` isn't a GCC feature at all
  (there's no portable equivalent that auto-links a static lib from inside
  a header), so `-ltable` on the command line is the normal, expected way -
  which matches what you wrote above. The reason it doesn't also need
  `-L<path>` is `LIBRARY_PATH`, which the installer already sets to
  `<install>\lib\mingw` - GCC searches every directory listed there
  automatically when resolving `-ltable`.
