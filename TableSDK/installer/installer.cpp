// Table SDK Installer
//
// A small, single-file Win32 GUI installer. table.h and the prebuilt
// static libs (table.lib for MSVC, libtable.a for MinGW) are embedded
// directly into this exe as RT_RCDATA resources at build time (see
// installer.rc + installer/payload/) - nothing needs to travel alongside
// installer.exe at distribution time.
//
// It writes the requested files into the chosen install directory and
// updates HKEY_CURRENT_USER\Environment so a new cl.exe / g++ invocation
// picks up the header and lib automatically:
//
//   INCLUDE               += <install>\include              (MSVC)
//   LIB                    += <install>\lib\msvc              (MSVC)
//   C_INCLUDE_PATH         += <install>\include              (MinGW)
//   CPLUS_INCLUDE_PATH     += <install>\include              (MinGW)
//   LIBRARY_PATH           += <install>\lib\mingw             (MinGW)
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

    if (plan.doMsvc)
    {
        SetStatus(hDlg, "Writing table.lib (MSVC)...");
        std::string dst = plan.installDir + "\\lib\\msvc\\table.lib";
        if (!WriteResourceToFile(ID_RES_MSVC_LIB, dst))
            return "Failed to write table.lib to:\n" + dst;
    }
    SetProgress(hDlg, 55);

    if (plan.doMingw)
    {
        SetStatus(hDlg, "Writing libtable.a (MinGW)...");
        std::string dst = plan.installDir + "\\lib\\mingw\\libtable.a";
        if (!WriteResourceToFile(ID_RES_MINGW_LIB, dst))
            return "Failed to write libtable.a to:\n" + dst;
    }
    SetProgress(hDlg, 75);

    SetStatus(hDlg, "Updating environment variables...");
    std::string includeDir = plan.installDir + "\\include";

    if (!AppendUserEnvPath("INCLUDE", includeDir))
        return "Failed to update INCLUDE.";

    if (plan.doMsvc)
    {
        std::string libDir = plan.installDir + "\\lib\\msvc";
        if (!AppendUserEnvPath("LIB", libDir))
            return "Failed to update LIB.";
    }

    if (plan.doMingw)
    {
        std::string libDir = plan.installDir + "\\lib\\mingw";
        if (!AppendUserEnvPath("C_INCLUDE_PATH", includeDir))
            return "Failed to update C_INCLUDE_PATH.";
        if (!AppendUserEnvPath("CPLUS_INCLUDE_PATH", includeDir))
            return "Failed to update CPLUS_INCLUDE_PATH.";
        if (!AppendUserEnvPath("LIBRARY_PATH", libDir))
            return "Failed to update LIBRARY_PATH.";
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

        bool haveMsvc  = EmbeddedResourceSize(ID_RES_MSVC_LIB) > 0;
        bool haveMingw = EmbeddedResourceSize(ID_RES_MINGW_LIB) > 0;

        HWND chkMsvc  = GetDlgItem(hDlg, IDC_CHK_MSVC);
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
            plan.doMsvc  = SendDlgItemMessage(hDlg, IDC_CHK_MSVC,  BM_GETCHECK, 0, 0) == BST_CHECKED;
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
                if (plan.doMsvc)  msg += "  (MSVC)  link table.lib\n";
                if (plan.doMingw) msg += "  (MinGW) link with -ltable\n";
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
