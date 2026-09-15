// Table SDK Installer
//
// A small, single-file Win32 GUI installer. table.h and the prebuilt
// static libs (table.lib for MSVC, libtable.a for MinGW) are embedded
// directly into this exe as RT_RCDATA resources at build time (see
// installer.rc + installer/payload/) - nothing needs to travel alongside
// installer.exe at distribution time.
//
// Both x64 and x86 builds of table.lib / libtable.a are embedded and, when
// present (non-zero size - a 0-byte resource means that architecture was
// never built), installed side by side:
//
//   <install>\lib\msvc\x64\table.lib      <install>\lib\msvc\x86\table.lib
//   <install>\lib\mingw\x64\libtable.a    <install>\lib\mingw\x86\libtable.a
//
// It updates HKEY_CURRENT_USER\Environment so a plain `cl file.c` /
// `gcc file.c -ltable` / `g++ file.cpp -ltable` just works with no extra
// flags, for all three compilers, on whichever architecture you actually
// have installed:
//
//   INCLUDE               += <install>\include                       (MSVC)
//   LIB                    += <install>\lib\msvc\x64                  (MSVC x64)
//                          += <install>\lib\msvc\x86                  (MSVC x86)
//   C_INCLUDE_PATH         += <install>\include                       (MinGW, gcc)
//   CPLUS_INCLUDE_PATH     += <install>\include                       (MinGW, g++)
//   LIBRARY_PATH           += <install>\lib\mingw\x64  OR  \x86        (MinGW - ONE, detected)
//
// MSVC gets BOTH x64 and x86 on LIB safely, because the x86 lib is staged
// as table_x86.lib (not table.lib) - see the writeLibVariant call below -
// and table.h's #pragma comment(lib, ...) picks the matching filename via
// _M_X64/_M_IX86, which cl.exe always sets correctly for whatever it's
// actually compiling. No name collision is possible, so link.exe can't
// grab the wrong architecture's table.lib the way it used to (the old
// LNK4272/LNK2019 errors this comment used to warn about).
//
// MinGW is different: GCC has no header-level autolink mechanism (no
// equivalent to #pragma comment(lib,...) - that's an MSVC-only feature),
// and GNU ld does NOT skip an incompatible-architecture archive and keep
// searching subsequent -L/LIBRARY_PATH entries the way you might expect -
// verified directly with ld -Wl,--verbose: it opens the first same-named
// libtable.a it finds by directory order and stops, then fails with plain
// "undefined reference" errors if that one is the wrong architecture.
// Both mingw\x64 and mingw\x86 use the same filename (libtable.a) since
// there's no header-level trick to disambiguate them for GCC, so only
// ONE of the two directories can safely go on LIBRARY_PATH. DetectMingwArch()
// runs `gcc -dumpmachine` at install time (mirroring build.bat's own
// detection) to pick the one that actually matches this machine's gcc.exe.
// If gcc.exe isn't on PATH yet at install time, it defaults to x64 and
// says so in the post-install message, with instructions to re-run the
// installer (or pass one explicit -L) once MinGW is actually on PATH.
//
// No admin rights are required: everything happens under the current
// user's profile and HKCU.

#define WIN32_LEAN_AND_MEAN
#undef UNICODE
#undef _UNICODE

#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <string>
#include <vector>

#include "resource.h"

// Note: no need for a #pragma comment(linker, "/manifestdependency:...") here -
// the manifest is embedded via the RT_MANIFEST resource in installer.rc
// (see "1 24 app.manifest"), which both MSVC (rc.exe) and MinGW (windres)
// understand identically.

// ---------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------

// Creates every directory component of path (like `mkdir -p`).
static bool EnsureDirectoryRecursive(const std::string& path)
{
    int rc = SHCreateDirectoryExA(NULL, path.c_str(), NULL);
    return rc == ERROR_SUCCESS || rc == ERROR_ALREADY_EXISTS || rc == ERROR_FILE_EXISTS;
}

static std::string ParentDir(const std::string& path)
{
    size_t pos = path.find_last_of("\\/");
    return (pos == std::string::npos) ? "" : path.substr(0, pos);
}

// Checks whether a file exists on THIS machine (the one running the
// installer) - used so wrapper-script generation reflects what's actually
// installed here, not just which architectures the build machine happened
// to embed libs for.
static bool FileExists(const std::string& path)
{
    DWORD attrs = GetFileAttributesA(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static std::string DefaultInstallPath()
{
    char buf[MAX_PATH] = { 0 };
    if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, buf) == S_OK)
        return std::string(buf) + "\\TableSDK";
    return "C:\\TableSDK";
}

static std::string GetDlgText(HWND hDlg, int id)
{
    char buf[MAX_PATH] = { 0 };
    GetDlgItemTextA(hDlg, id, buf, MAX_PATH);
    return std::string(buf);
}

static bool BrowseForFolder(HWND owner, std::string& outPath)
{
    BROWSEINFOA bi = { 0 };
    bi.hwndOwner = owner;
    bi.lpszTitle = "Choose install location";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderA(&bi);
    if (!pidl)
        return false;

    char path[MAX_PATH] = { 0 };
    bool ok = SHGetPathFromIDListA(pidl, path) != 0;
    CoTaskMemFree(pidl);

    if (ok)
        outPath = path;
    return ok;
}

// ---------------------------------------------------------------------
// Environment variable handling (HKEY_CURRENT_USER\Environment)
// ---------------------------------------------------------------------

// Reads a REG_(EXPAND_)SZ value; returns "" if it doesn't exist yet.
static std::string ReadUserEnvVar(HKEY hEnv, const char* name)
{
    DWORD type = 0, size = 0;
    if (RegQueryValueExA(hEnv, name, NULL, &type, NULL, &size) != ERROR_SUCCESS)
        return "";
    if (type != REG_SZ && type != REG_EXPAND_SZ)
        return "";

    std::vector<char> buf(size + 1, 0);
    if (RegQueryValueExA(hEnv, name, NULL, &type, (LPBYTE)buf.data(), &size) != ERROR_SUCCESS)
        return "";
    return std::string(buf.data());
}

static bool PathTokenPresent(const std::string& pathList, const std::string& token)
{
    size_t start = 0;
    while (start <= pathList.size())
    {
        size_t sep = pathList.find(';', start);
        std::string part = (sep == std::string::npos) ? pathList.substr(start) : pathList.substr(start, sep - start);
        if (_stricmp(part.c_str(), token.c_str()) == 0)
            return true;
        if (sep == std::string::npos) break;
        start = sep + 1;
    }
    return false;
}

// Appends newPath to a user env var (creating it if absent), skipping the
// append if it's already there. Writes back as REG_EXPAND_SZ so any
// pre-existing %VAR% references in the value keep working.
static bool AppendUserEnvPath(const char* varName, const std::string& newPath)
{
    HKEY hEnv = NULL;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Environment", 0, KEY_READ | KEY_WRITE, &hEnv) != ERROR_SUCCESS)
        return false;

    std::string current = ReadUserEnvVar(hEnv, varName);
    bool changed = false;

    if (current.empty())
    {
        current = newPath;
        changed = true;
    }
    else if (!PathTokenPresent(current, newPath))
    {
        current += ";" + newPath;
        changed = true;
    }

    bool ok = true;
    if (changed)
    {
        ok = RegSetValueExA(hEnv, varName, 0, REG_EXPAND_SZ,
            (const BYTE*)current.c_str(),
            (DWORD)current.size() + 1) == ERROR_SUCCESS;
    }

    RegCloseKey(hEnv);
    return ok;
}

// Removes every entry in a user env var that starts with the given prefix
// (case-insensitive), EXCEPT keepPath itself if provided (so a caller can
// clean up everything else under this install's tree before re-adding the
// one correct entry, in a single pass, without a remove-then-immediately-
// readd race).
//
// This is what makes re-running the installer actually self-healing rather
// than purely additive. Without it, every past install (an older installer
// version that staged libs without an arch subfolder, or a previous run
// where DetectMingwArch() guessed a different architecture than it does
// now) leaves its entry in place forever - AppendUserEnvPath only skips
// adding an *exact duplicate* string, it has no way to know an existing
// entry is stale or simply wrong for this machine. That's exactly how
// LIBRARY_PATH and LIB ended up with duplicate/conflicting entries even
// though every individual install "succeeded" - each run only ever added,
// never corrected. Scrubbing our own install tree's prior entries before
// re-adding the current correct one closes that gap for good.
static bool RemoveUserEnvPathsWithPrefix(const char* varName, const std::string& prefix, const std::string& keepPath = "")
{
    HKEY hEnv = NULL;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Environment", 0, KEY_READ | KEY_WRITE, &hEnv) != ERROR_SUCCESS)
        return false;

    std::string current = ReadUserEnvVar(hEnv, varName);
    if (current.empty()) { RegCloseKey(hEnv); return true; }

    std::vector<std::string> kept;
    size_t start = 0;
    while (start <= current.size())
    {
        size_t sep = current.find(';', start);
        std::string part = (sep == std::string::npos) ? current.substr(start) : current.substr(start, sep - start);
        if (!part.empty())
        {
            bool hasPrefix = (_strnicmp(part.c_str(), prefix.c_str(), prefix.size()) == 0);
            bool isKeep = (!keepPath.empty() && _stricmp(part.c_str(), keepPath.c_str()) == 0);
            if (!hasPrefix || isKeep)
                kept.push_back(part);
        }
        if (sep == std::string::npos) break;
        start = sep + 1;
    }

    std::string rebuilt;
    for (size_t i = 0; i < kept.size(); ++i)
    {
        if (i) rebuilt += ";";
        rebuilt += kept[i];
    }

    bool ok = true;
    if (rebuilt != current)
    {
        ok = RegSetValueExA(hEnv, varName, 0, REG_EXPAND_SZ,
            (const BYTE*)rebuilt.c_str(),
            (DWORD)rebuilt.size() + 1) == ERROR_SUCCESS;
    }

    RegCloseKey(hEnv);
    return ok;
}

static void BroadcastEnvironmentChange()
{
    DWORD_PTR result = 0;
    SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)"Environment",
        SMTO_ABORTIFHUNG, 5000, &result);
}

// ---------------------------------------------------------------------
// Embedded resource access (RT_RCDATA payload baked into this exe)
// ---------------------------------------------------------------------

// Locates an embedded resource by id. Returns false (data=NULL, size=0) if
// the resource doesn't exist at all - a 0-byte resource that DOES exist
// (our MSVC-not-built placeholder) still returns true with size 0.
static bool GetEmbeddedResource(int resId, const BYTE** outData, DWORD* outSize)
{
    HMODULE hSelf = GetModuleHandleA(NULL);
    HRSRC hRes = FindResourceA(hSelf, MAKEINTRESOURCEA(resId), RT_RCDATA);
    if (!hRes) { *outData = NULL; *outSize = 0; return false; }

    HGLOBAL hMem = LoadResource(hSelf, hRes);
    if (!hMem) { *outData = NULL; *outSize = 0; return false; }

    *outSize = SizeofResource(hSelf, hRes);
    *outData = (const BYTE*)LockResource(hMem);
    return (*outData != NULL || *outSize == 0);
}

static DWORD EmbeddedResourceSize(int resId)
{
    const BYTE* data = NULL;
    DWORD size = 0;
    GetEmbeddedResource(resId, &data, &size);
    return size;
}

static bool WriteResourceToFile(int resId, const std::string& destPath)
{
    const BYTE* data = NULL;
    DWORD size = 0;
    if (!GetEmbeddedResource(resId, &data, &size) || size == 0)
        return false;

    std::string parent = ParentDir(destPath);
    if (!parent.empty() && !EnsureDirectoryRecursive(parent))
        return false;

    HANDLE hFile = CreateFileA(destPath.c_str(), GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    DWORD written = 0;
    bool ok = WriteFile(hFile, data, size, &written, NULL) && written == size;
    CloseHandle(hFile);
    return ok;
}

// Writes arbitrary text content directly to a file - used for the
// architecture-specific wrapper scripts (gcc32.bat/gcc64.bat/g++32.bat/
// g++64.bat), which don't need the embedded-resource machinery above
// since their content is fully deterministic (fixed compiler paths +
// the actual install dir chosen for this install) rather than payload
// baked into the installer at build time.
static bool WriteTextFile(const std::string& destPath, const std::string& content)
{
    std::string parent = ParentDir(destPath);
    if (!parent.empty() && !EnsureDirectoryRecursive(parent))
        return false;

    HANDLE hFile = CreateFileA(destPath.c_str(), GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    DWORD written = 0;
    bool ok = WriteFile(hFile, content.c_str(), (DWORD)content.size(), &written, NULL)
        && written == (DWORD)content.size();
    CloseHandle(hFile);
    return ok;
}

// ---------------------------------------------------------------------
// Install logic
// ---------------------------------------------------------------------

struct InstallPlan
{
    std::string installDir;
    bool doMsvc = false;
    bool doMingw = false;
};

static void SetStatus(HWND hDlg, const char* text)
{
    SetDlgItemTextA(hDlg, IDC_STATUS, text);
    // Force a repaint so the label updates before the next (possibly slow) step.
    UpdateWindow(GetDlgItem(hDlg, IDC_STATUS));
}

static void SetProgress(HWND hDlg, int percent)
{
    SendDlgItemMessage(hDlg, IDC_PROGRESS, PBM_SETPOS, (WPARAM)percent, 0);
}

// Returns "" on success, or a human-readable error message.
// Runs "gcc -dumpmachine" (hidden, no console flash) and returns "x64",
// "x86", or "" if gcc.exe isn't on PATH yet or its target triple isn't
// one we recognize. Mirrors build.bat's own detection logic exactly, so
// the installer and the build script always agree on which architecture
// a given gcc.exe actually targets.
//
// This exists because GNU ld does NOT skip an incompatible-architecture
// static archive and keep searching subsequent -L/LIBRARY_PATH entries -
// verified directly (ld -Wl,--verbose): it opens the first same-named
// libtable.a it finds and stops looking, then fails with plain
// "undefined reference" errors if that one doesn't match the target.
// So only ONE mingw\<arch> directory can safely go on LIBRARY_PATH -
// whichever one actually matches this machine's gcc.exe.
static std::string DetectMingwArch()
{
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hReadPipe = NULL, hWritePipe = NULL;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
        return "";
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = { sizeof(si) };
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.hStdInput = NULL;

    PROCESS_INFORMATION pi = { 0 };
    char cmdLine[] = "gcc -dumpmachine";
    BOOL ok = CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE, CREATE_NO_WINDOW,
        NULL, NULL, &si, &pi);
    CloseHandle(hWritePipe);
    if (!ok)
    {
        CloseHandle(hReadPipe);
        return "";
    }

    std::string output;
    char buf[256];
    DWORD bytesRead = 0;
    while (ReadFile(hReadPipe, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0)
    {
        buf[bytesRead] = '\0';
        output += buf;
    }
    CloseHandle(hReadPipe);

    WaitForSingleObject(pi.hProcess, 5000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (output.rfind("x86_64", 0) == 0) return "x64";
    if (output.rfind("i686", 0) == 0)   return "x86";
    // Classic mingw.org gcc (as opposed to a mingw-w64-based toolchain)
    // reports plain "mingw32" from -dumpmachine, not an "i686-..." triple -
    // this is the exact same quirk build.bat's own comments describe and
    // route around by calling compilers by known full path instead of
    // trusting -dumpmachine. This detector still needs -dumpmachine (it
    // has no fixed-path shortcut the way build.bat does), so it has to
    // recognize this output shape explicitly rather than falling through
    // to the "unrecognized -> default to x64" branch, which is what was
    // silently sending 32-bit-only machines' LIBRARY_PATH to the x64
    // folder instead of x86.
    if (output.rfind("mingw32", 0) == 0) return "x86";
    return "";
}

static std::string RunInstall(HWND hDlg, const InstallPlan& plan)
{
    SetProgress(hDlg, 0);
    SetStatus(hDlg, "Creating install directory...");
    if (!EnsureDirectoryRecursive(plan.installDir))
        return "Could not create install directory:\n" + plan.installDir;
    SetProgress(hDlg, 15);

    SetStatus(hDlg, "Writing table.h...");
    std::string headerDst = plan.installDir + "\\include\\table.h";
    if (!WriteResourceToFile(ID_RES_HEADER, headerDst))
        return "Failed to write table.h to:\n" + headerDst;
    SetProgress(hDlg, 35);

    // Writes one lib variant if it was actually built (non-zero embedded
    // resource); silently skips it otherwise, exactly like the header/table.lib
    // "0 bytes = not built for this toolchain" convention used elsewhere.
    auto writeLibVariant = [&](int resId, const char* archLabel, const std::string& dst,
        const char* fileLabel) -> std::string
        {
            if (EmbeddedResourceSize(resId) == 0)
                return ""; // not built for this architecture - nothing to do
            SetStatus(hDlg, (std::string("Writing ") + fileLabel + " (" + archLabel + ")...").c_str());
            if (!WriteResourceToFile(resId, dst))
                return "Failed to write " + std::string(fileLabel) + " to:\n" + dst;
            return "";
        };

    if (plan.doMsvc)
    {
        std::string err;
        err = writeLibVariant(ID_RES_MSVC_LIB_X64, "MSVC x64", plan.installDir + "\\lib\\msvc\\x64\\table.lib", "table.lib");
        if (!err.empty()) return err;
        err = writeLibVariant(ID_RES_MSVC_LIB_X86, "MSVC x86", plan.installDir + "\\lib\\msvc\\x86\\table_x86.lib", "table_x86.lib");
        if (!err.empty()) return err;
    }
    SetProgress(hDlg, 55);

    if (plan.doMingw)
    {
        std::string err;
        err = writeLibVariant(ID_RES_MINGW_LIB_X64, "MinGW x64", plan.installDir + "\\lib\\mingw\\x64\\libtable.a", "libtable.a");
        if (!err.empty()) return err;
        err = writeLibVariant(ID_RES_MINGW_LIB_X86, "MinGW x86", plan.installDir + "\\lib\\mingw\\x86\\libtable.a", "libtable.a");
        if (!err.empty()) return err;

        // Architecture-specific wrapper scripts (gcc32.bat/gcc64.bat/
        // g++32.bat/g++64.bat) - these exist so a consumer never has to
        // check LIBRARY_PATH, PATH order, or run `gcc -dumpmachine` to
        // know which toolchain+lib pair they're actually getting: the
        // command name alone picks it, via a hardcoded full compiler
        // path and explicit -I/-L/-ltable baked in at install time.
        // These conventional MinGW install locations mirror build.bat's
        // own MINGW64_BIN/MINGW32_BIN defaults - if a user's install
        // lives elsewhere, the wrapper for that arch just won't resolve
        // and they'll need to edit it, same as build.bat's own comment
        // instructs for that script.
        SetStatus(hDlg, "Writing architecture-specific build wrappers...");
        std::string includeDirForWrapper = plan.installDir + "\\include";

        auto writeWrapper = [&](const char* fileName, const char* compilerFullPath,
            const std::string& libDir) -> std::string
            {
                std::string content =
                    "@echo off\r\n"
                    "REM Auto-generated by the TableSDK installer - always builds with\r\n"
                    "REM this specific compiler and library pair, regardless of\r\n"
                    "REM LIBRARY_PATH/PATH order or which other MinGW installs exist.\r\n"
                    "if not exist \"" + std::string(compilerFullPath) + "\" (\r\n"
                    "    echo ERROR: " + std::string(compilerFullPath) + " not found.\r\n"
                    "    echo This wrapper expects MinGW installed at its conventional path.\r\n"
                    "    echo Edit this file's compiler path if yours lives elsewhere.\r\n"
                    "    exit /b 1\r\n"
                    ")\r\n"
                    "\"" + std::string(compilerFullPath) + "\" %*"
                    " -I\"" + includeDirForWrapper + "\""
                    " -L\"" + libDir + "\""
                    " -ltable\r\n";
                return WriteTextFile(plan.installDir + "\\bin\\" + fileName, content) ? "" :
                    ("Failed to write " + std::string(fileName));
            };

        if (EmbeddedResourceSize(ID_RES_MINGW_LIB_X86) > 0 && FileExists("C:\\MinGW\\bin\\gcc.exe"))
        {
            err = writeWrapper("gcc32.bat", "C:\\MinGW\\bin\\gcc.exe", plan.installDir + "\\lib\\mingw\\x86");
            if (!err.empty()) return err;
            err = writeWrapper("g++32.bat", "C:\\MinGW\\bin\\g++.exe", plan.installDir + "\\lib\\mingw\\x86");
            if (!err.empty()) return err;
        }
        if (EmbeddedResourceSize(ID_RES_MINGW_LIB_X64) > 0 && FileExists("C:\\mingw64\\bin\\gcc.exe"))
        {
            err = writeWrapper("gcc64.bat", "C:\\mingw64\\bin\\gcc.exe", plan.installDir + "\\lib\\mingw\\x64");
            if (!err.empty()) return err;
            err = writeWrapper("g++64.bat", "C:\\mingw64\\bin\\g++.exe", plan.installDir + "\\lib\\mingw\\x64");
            if (!err.empty()) return err;
        }
    }
    SetProgress(hDlg, 75);

    // MinGW's ld skips an incompatible-architecture archive automatically
    // when resolving -ltable and keeps searching subsequent -L paths, so
    // adding BOTH mingw dirs to LIBRARY_PATH is safe and "just works" for
    // whichever architecture gcc.exe on PATH actually targets.
    //
    // MSVC's link.exe does NOT do that - it takes the first table.lib it
    // finds by name regardless of architecture, which is exactly what
    // produced the LNK4272/LNK2019 errors earlier. So only ONE msvc path
    // can go in LIB automatically; x64 is added since that's the default
    // architecture for the vast majority of setups. 32-bit MSVC users
    // (already a deliberate choice - it requires opening the x86 Native
    // Tools prompt specifically) need one explicit /LIBPATH override,
    // which the success message spells out.
    SetStatus(hDlg, "Updating environment variables...");
    std::string includeDir = plan.installDir + "\\include";

    if (!AppendUserEnvPath("INCLUDE", includeDir))
        return "Failed to update INCLUDE.";

    // Put the generated wrapper scripts (gcc32/gcc64/g++32/g++64) on PATH,
    // if any were written above. Scrubbed first for the same reason LIB/
    // LIBRARY_PATH are: a prior install's bin\ entry should be replaced,
    // not just left alongside, on every re-run.
    if (plan.doMingw)
    {
        RemoveUserEnvPathsWithPrefix("PATH", plan.installDir + "\\bin");
        bool anyWrapper =
            FileExists(plan.installDir + "\\bin\\gcc32.bat") ||
            FileExists(plan.installDir + "\\bin\\gcc64.bat");
        if (anyWrapper && !AppendUserEnvPath("PATH", plan.installDir + "\\bin"))
            return "Failed to update PATH.";
    }

    // Scrub any stale LIB entries under this install's msvc\ tree first -
    // e.g. a flat "...\lib\msvc" (no arch subfolder) left by an older
    // installer version - before re-adding the current correct pair. Do
    // this once, ahead of both x64/x86 appends below, so a single install
    // run always leaves exactly the right set behind.
    RemoveUserEnvPathsWithPrefix("LIB", plan.installDir + "\\lib\\msvc");

    if (plan.doMsvc && EmbeddedResourceSize(ID_RES_MSVC_LIB_X64) > 0)
    {
        if (!AppendUserEnvPath("LIB", plan.installDir + "\\lib\\msvc\\x64"))
            return "Failed to update LIB.";
    }
    if (plan.doMsvc && EmbeddedResourceSize(ID_RES_MSVC_LIB_X86) > 0)
    {
        // Safe to add alongside the x64 dir above: this variant is staged
        // as table_x86.lib (not table.lib), so link.exe never has two
        // same-named candidates to pick between. table.h's pragma selects
        // the matching filename via _M_IX86/_M_X64, so a plain `cl file.c`
        // just works from either Native Tools prompt - no /LIBPATH needed.
        if (!AppendUserEnvPath("LIB", plan.installDir + "\\lib\\msvc\\x86"))
            return "Failed to update LIB.";
    }

    if (plan.doMingw)
    {
        if (!AppendUserEnvPath("C_INCLUDE_PATH", includeDir))
            return "Failed to update C_INCLUDE_PATH.";
        if (!AppendUserEnvPath("CPLUS_INCLUDE_PATH", includeDir))
            return "Failed to update CPLUS_INCLUDE_PATH.";

        // Only add the ONE directory matching this machine's actual
        // gcc.exe - see DetectMingwArch() for why both can't go on
        // LIBRARY_PATH together the way the MSVC LIB paths now can.
        //
        // Scrub BOTH this install's mingw\x64 and mingw\x86 entries first,
        // regardless of which one DetectMingwArch() picks below. Without
        // this, switching gcc.exe (or re-running after a previous run
        // guessed differently) just appends the new one alongside the old
        // one instead of replacing it - which is exactly how both ended up
        // on LIBRARY_PATH together in the first place, recreating the
        // wrong-architecture-archive-picked-first bug on every future run.
        RemoveUserEnvPathsWithPrefix("LIBRARY_PATH", plan.installDir + "\\lib\\mingw");

        std::string mingwArch = DetectMingwArch();
        if (mingwArch == "x64" && EmbeddedResourceSize(ID_RES_MINGW_LIB_X64) > 0)
        {
            if (!AppendUserEnvPath("LIBRARY_PATH", plan.installDir + "\\lib\\mingw\\x64"))
                return "Failed to update LIBRARY_PATH.";
        }
        else if (mingwArch == "x86" && EmbeddedResourceSize(ID_RES_MINGW_LIB_X86) > 0)
        {
            if (!AppendUserEnvPath("LIBRARY_PATH", plan.installDir + "\\lib\\mingw\\x86"))
                return "Failed to update LIBRARY_PATH.";
        }
        else
        {
            // gcc.exe isn't on PATH yet (e.g. SDK installed before MinGW
            // itself) or reported an architecture we don't recognize.
            // Best-effort default to x64 (the common case) rather than
            // leaving LIBRARY_PATH empty; the post-install message flags
            // this so the person knows to re-run the installer once
            // their MinGW gcc.exe is actually on PATH, which will then
            // correctly detect and switch to x86 if that's what they have.
            if (EmbeddedResourceSize(ID_RES_MINGW_LIB_X64) > 0)
                if (!AppendUserEnvPath("LIBRARY_PATH", plan.installDir + "\\lib\\mingw\\x64"))
                    return "Failed to update LIBRARY_PATH.";
        }
    }

    BroadcastEnvironmentChange();
    SetProgress(hDlg, 100);
    SetStatus(hDlg, "Done.");
    return "";
}

// ---------------------------------------------------------------------
// Dialog procedure
// ---------------------------------------------------------------------

static INT_PTR CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        SetDlgItemTextA(hDlg, IDC_PATH_EDIT, DefaultInstallPath().c_str());

        bool haveMsvc = EmbeddedResourceSize(ID_RES_MSVC_LIB_X64) > 0 || EmbeddedResourceSize(ID_RES_MSVC_LIB_X86) > 0;
        bool haveMingw = EmbeddedResourceSize(ID_RES_MINGW_LIB_X64) > 0 || EmbeddedResourceSize(ID_RES_MINGW_LIB_X86) > 0;

        HWND chkMsvc = GetDlgItem(hDlg, IDC_CHK_MSVC);
        HWND chkMingw = GetDlgItem(hDlg, IDC_CHK_MINGW);

        EnableWindow(chkMsvc, haveMsvc);
        EnableWindow(chkMingw, haveMingw);
        SendMessage(chkMsvc, BM_SETCHECK, haveMsvc ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessage(chkMingw, BM_SETCHECK, haveMingw ? BST_CHECKED : BST_UNCHECKED, 0);

        SendDlgItemMessage(hDlg, IDC_PROGRESS, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_BROWSE:
        {
            std::string chosen;
            if (BrowseForFolder(hDlg, chosen))
                SetDlgItemTextA(hDlg, IDC_PATH_EDIT, chosen.c_str());
            return TRUE;
        }

        case IDC_INSTALL:
        {
            InstallPlan plan;
            plan.installDir = GetDlgText(hDlg, IDC_PATH_EDIT);
            plan.doMsvc = SendDlgItemMessage(hDlg, IDC_CHK_MSVC, BM_GETCHECK, 0, 0) == BST_CHECKED;
            plan.doMingw = SendDlgItemMessage(hDlg, IDC_CHK_MINGW, BM_GETCHECK, 0, 0) == BST_CHECKED;

            if (plan.installDir.empty())
            {
                MessageBoxA(hDlg, "Please choose an install location.", "Table SDK Setup", MB_ICONWARNING);
                return TRUE;
            }
            if (!plan.doMsvc && !plan.doMingw)
            {
                MessageBoxA(hDlg, "Select at least one toolchain to install for.", "Table SDK Setup", MB_ICONWARNING);
                return TRUE;
            }

            EnableWindow(GetDlgItem(hDlg, IDC_INSTALL), FALSE);
            EnableWindow(GetDlgItem(hDlg, IDC_BROWSE), FALSE);

            std::string err = RunInstall(hDlg, plan);

            if (err.empty())
            {
                std::string msg = "Table SDK installed to:\n" + plan.installDir +
                    "\n\nRestart any open terminal/IDE so it picks up the new "
                    "environment variables, then use:\n\n"
                    "  #include <table.h>\n";
                if (plan.doMsvc && (EmbeddedResourceSize(ID_RES_MSVC_LIB_X64) > 0 || EmbeddedResourceSize(ID_RES_MSVC_LIB_X86) > 0))
                    msg += "  (MSVC, x64 or x86)        cl yourfile.c            -> just works\n";
                if (plan.doMingw && (EmbeddedResourceSize(ID_RES_MINGW_LIB_X64) > 0 || EmbeddedResourceSize(ID_RES_MINGW_LIB_X86) > 0))
                {
                    std::string mingwArch = DetectMingwArch();
                    bool haveMatch =
                        (mingwArch == "x64" && EmbeddedResourceSize(ID_RES_MINGW_LIB_X64) > 0) ||
                        (mingwArch == "x86" && EmbeddedResourceSize(ID_RES_MINGW_LIB_X86) > 0);
                    if (haveMatch)
                    {
                        msg += "  (MinGW, " + mingwArch + ")            gcc yourfile.c -ltable  -> just works\n"
                            "  (MinGW, " + mingwArch + ", C++)       g++ yourfile.cpp -ltable -> just works\n";
                    }
                    else
                    {
                        msg += "\nCouldn't detect your MinGW gcc.exe's architecture during install\n"
                            "(it may not have been on PATH yet). Defaulted to wiring up the\n"
                            "x64 library. If `gcc yourfile.c -ltable` fails with \"undefined\n"
                            "reference\" errors, either re-run this installer now that MinGW\n"
                            "is on PATH (it will auto-detect correctly), or add the matching\n"
                            "path explicitly:\n"
                            "  gcc yourfile.c -L\"" + plan.installDir + "\\lib\\mingw\\x86\" -ltable\n";
                    }
                }
                MessageBoxA(hDlg, msg.c_str(), "Table SDK Setup", MB_ICONINFORMATION);
                EndDialog(hDlg, IDOK);
            }
            else
            {
                MessageBoxA(hDlg, err.c_str(), "Install failed", MB_ICONERROR);
                EnableWindow(GetDlgItem(hDlg, IDC_INSTALL), TRUE);
                EnableWindow(GetDlgItem(hDlg, IDC_BROWSE), TRUE);
                SetProgress(hDlg, 0);
                SetStatus(hDlg, "");
            }
            return TRUE;
        }

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;

    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    CoInitialize(NULL);

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    DialogBoxParamA(hInstance, MAKEINTRESOURCEA(IDD_MAIN), NULL, DlgProc, 0);

    CoUninitialize();
    return 0;
}