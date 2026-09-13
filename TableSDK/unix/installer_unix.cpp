// Table SDK Installer (Unix: Linux + macOS)
//
// GUI counterpart to installer.cpp (Windows). Same job, different mechanics:
//
//   - table.h and table.c are embedded as byte arrays in embedded_payload.h
//     (see generate_embedded_payload.py) instead of RCDATA resources - FLTK
//     has no resource compiler, so we just compile the bytes straight in.
//   - Rather than shipping a prebuilt .a (which risks a glibc/libc++ ABI
//     mismatch against whatever machine actually runs it), this installer
//     writes table.c to a temp dir and compiles libtable.a locally with the
//     host's own cc/clang. That always matches the target machine exactly.
//   - There's no per-user registry. If we install somewhere already on the
//     compiler's default search path (/usr/local, /usr), nothing else is
//     needed. Otherwise we offer to append CPATH / LIBRARY_PATH exports to
//     the user's shell rc file - the closest Unix equivalent of the
//     Windows installer's HKCU\Environment updates.
//
// No root required: default install location is under $HOME unless
// /usr/local is writable by the current user.

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Progress.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/fl_ask.H>

#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>

#include "embedded_payload.h"

// ---------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------

static std::string HomeDir()
{
    const char* h = getenv("HOME");
    if (h && *h) return h;
    struct passwd* pw = getpwuid(getuid());
    return (pw && pw->pw_dir) ? pw->pw_dir : "/tmp";
}

static bool PathExists(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

static bool IsWritableDir(const std::string& path)
{
    return access(path.c_str(), W_OK) == 0;
}

// mkdir -p
static bool EnsureDirectoryRecursive(const std::string& path)
{
    if (path.empty() || path == "/") return true;
    if (PathExists(path)) return true;

    size_t pos = path.find_last_of('/');
    if (pos != std::string::npos && pos > 0)
    {
        if (!EnsureDirectoryRecursive(path.substr(0, pos)))
            return false;
    }
    if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST)
        return false;
    return true;
}

static std::string ParentDir(const std::string& path)
{
    size_t pos = path.find_last_of('/');
    return (pos == std::string::npos) ? "" : path.substr(0, pos);
}

static std::string DefaultInstallPrefix()
{
    if (IsWritableDir("/usr/local"))
        return "/usr/local";
    return HomeDir() + "/.local";
}

static bool WriteBytesToFile(const std::string& destPath, const unsigned char* data, unsigned long size)
{
    std::string parent = ParentDir(destPath);
    if (!parent.empty() && !EnsureDirectoryRecursive(parent))
        return false;

    FILE* f = fopen(destPath.c_str(), "wb");
    if (!f) return false;
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    return written == size;
}

// Runs a shell command, returns its exit code. Captures nothing - status
// text in the UI is set explicitly around each call instead.
static int RunCommand(const std::string& cmd)
{
    return WEXITSTATUS(system((cmd + " >/tmp/tablesdk_install.log 2>&1").c_str()));
}

// Finds the first working C compiler on PATH, preferring $CC if set.
static std::string DetectCompiler()
{
    const char* envCC = getenv("CC");
    if (envCC && *envCC && RunCommand(std::string(envCC) + " --version") == 0)
        return envCC;

    for (const char* candidate : {"cc", "clang", "gcc"})
    {
        std::string check = std::string("command -v ") + candidate;
        if (RunCommand(check) == 0)
            return candidate;
    }
    return "";
}

static std::string DetectArchiver()
{
    if (RunCommand("command -v ar") == 0)
        return "ar";
    return "";
}

// Best-effort shell rc file for the user's current shell.
static std::string DetectShellRcFile()
{
    std::string home = HomeDir();
    const char* shell = getenv("SHELL");
    std::string shellName = shell ? shell : "";

    if (shellName.find("zsh") != std::string::npos)
    {
        std::string p = home + "/.zshrc";
        return p;
    }
    if (shellName.find("bash") != std::string::npos)
    {
        std::string bashrc = home + "/.bashrc";
        if (PathExists(bashrc)) return bashrc;
        return home + "/.bash_profile";
    }
    // Fallback understood by any POSIX-ish shell.
    return home + "/.profile";
}

static bool AppendShellExports(const std::string& rcFile, const std::string& includeDir, const std::string& libDir)
{
    FILE* f = fopen(rcFile.c_str(), "a");
    if (!f) return false;
    fprintf(f, "\n# Added by Table SDK installer\n");
    fprintf(f, "export CPATH=\"%s:$CPATH\"\n", includeDir.c_str());
    fprintf(f, "export LIBRARY_PATH=\"%s:$LIBRARY_PATH\"\n", libDir.c_str());
    fclose(f);
    return true;
}

// ---------------------------------------------------------------------
// Install logic
// ---------------------------------------------------------------------

struct InstallPlan
{
    std::string prefix;      // e.g. /usr/local or ~/.local
    bool updateShellProfile = true;
};

// UI handles the install step touches, passed down so status/progress can update live.
struct InstallUi
{
    Fl_Box* status = nullptr;
    Fl_Progress* progress = nullptr;
};

static void SetStatus(InstallUi& ui, const char* text)
{
    ui.status->copy_label(text);
    ui.status->redraw();
    Fl::check(); // pump the event loop so the label actually repaints mid-install
}

static void SetProgress(InstallUi& ui, int percent)
{
    ui.progress->value((float)percent);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", percent);
    ui.progress->copy_label(buf);
    ui.progress->redraw();
    Fl::check();
}

// Returns "" on success, or a human-readable error.
static std::string RunInstall(InstallUi& ui, const InstallPlan& plan)
{
    std::string includeDir = plan.prefix + "/include";
    std::string libDir     = plan.prefix + "/lib";
    std::string tmpDir     = "/tmp/tablesdk_build_" + std::to_string(getpid());

    SetProgress(ui, 0);
    SetStatus(ui, "Checking for a C compiler...");
    std::string cc = DetectCompiler();
    if (cc.empty())
        return "No C compiler (cc/clang/gcc) found on PATH.\n"
               "Install one (Xcode Command Line Tools on macOS, build-essential on Linux) and try again.";
    std::string ar = DetectArchiver();
    if (ar.empty())
        return "No 'ar' archiver found on PATH - it normally ships with the compiler toolchain.";
    SetProgress(ui, 10);

    SetStatus(ui, "Creating install directories...");
    if (!EnsureDirectoryRecursive(includeDir) || !EnsureDirectoryRecursive(libDir))
        return "Could not create install directories under:\n" + plan.prefix +
               "\n\nTry a location you have write access to.";
    if (!EnsureDirectoryRecursive(tmpDir))
        return "Could not create a temp build directory:\n" + tmpDir;
    SetProgress(ui, 20);

    SetStatus(ui, "Writing table.h...");
    if (!WriteBytesToFile(includeDir + "/table.h", g_TableH, g_TableH_len))
        return "Failed to write table.h to:\n" + includeDir;
    SetProgress(ui, 35);

    SetStatus(ui, "Writing table.c (temporary build copy)...");
    if (!WriteBytesToFile(tmpDir + "/table.c", g_TableC, g_TableC_len))
        return "Failed to write a temporary table.c to:\n" + tmpDir;
    // The temp build also needs table.h alongside table.c to compile.
    if (!WriteBytesToFile(tmpDir + "/table.h", g_TableH, g_TableH_len))
        return "Failed to write a temporary table.h to:\n" + tmpDir;
    SetProgress(ui, 45);

    SetStatus(ui, ("Compiling libtable.a with " + cc + "...").c_str());
    std::string compileCmd = cc + " -O2 -std=c11 -c \"" + tmpDir + "/table.c\" -o \"" + tmpDir + "/table.o\"";
    if (RunCommand(compileCmd) != 0)
        return "Compilation failed. See /tmp/tablesdk_install.log for details.\nCommand was:\n" + compileCmd;
    SetProgress(ui, 70);

    SetStatus(ui, "Archiving libtable.a...");
    std::string archiveCmd = ar + " rcs \"" + libDir + "/libtable.a\" \"" + tmpDir + "/table.o\"";
    if (RunCommand(archiveCmd) != 0)
        return "Archiving failed. See /tmp/tablesdk_install.log for details.\nCommand was:\n" + archiveCmd;
    SetProgress(ui, 85);

    SetStatus(ui, "Cleaning up temporary files...");
    RunCommand("rm -rf \"" + tmpDir + "\"");
    SetProgress(ui, 90);

    bool onDefaultSearchPath = (plan.prefix == "/usr/local" || plan.prefix == "/usr");
    if (plan.updateShellProfile && !onDefaultSearchPath)
    {
        SetStatus(ui, "Updating shell profile...");
        std::string rc = DetectShellRcFile();
        if (!AppendShellExports(rc, includeDir, libDir))
            return "Built and installed libtable.a, but failed to update:\n" + rc +
                   "\n\nAdd these manually:\n"
                   "  export CPATH=\"" + includeDir + ":$CPATH\"\n"
                   "  export LIBRARY_PATH=\"" + libDir + ":$LIBRARY_PATH\"\n";
    }

    SetProgress(ui, 100);
    SetStatus(ui, "Done.");
    return "";
}

// ---------------------------------------------------------------------
// GUI
// ---------------------------------------------------------------------

struct AppWidgets
{
    Fl_Window* window = nullptr;
    Fl_Input* pathInput = nullptr;
    Fl_Check_Button* shellChk = nullptr;
    Fl_Box* status = nullptr;
    Fl_Progress* progress = nullptr;
    Fl_Button* installBtn = nullptr;
    Fl_Button* browseBtn = nullptr;
    Fl_Button* cancelBtn = nullptr;
};

static AppWidgets g_ui;

static void BrowseCallback(Fl_Widget*, void*)
{
    Fl_Native_File_Chooser chooser;
    chooser.title("Choose install location");
    chooser.type(Fl_Native_File_Chooser::BROWSE_DIRECTORY);
    chooser.directory(g_ui.pathInput->value());

    if (chooser.show() == 0 && chooser.filename())
        g_ui.pathInput->value(chooser.filename());
}

static void InstallCallback(Fl_Widget*, void*)
{
    InstallPlan plan;
    plan.prefix = g_ui.pathInput->value();
    plan.updateShellProfile = g_ui.shellChk->value() != 0;

    if (plan.prefix.empty())
    {
        fl_alert("Please choose an install location.");
        return;
    }

    g_ui.installBtn->deactivate();
    g_ui.browseBtn->deactivate();

    InstallUi ui{ g_ui.status, g_ui.progress };
    std::string err = RunInstall(ui, plan);

    if (err.empty())
    {
        std::string msg = "Table SDK installed to:\n" + plan.prefix +
            "\n\nUse:\n\n"
            "  #include <table.h>\n"
            "  cc yourprog.c -I" + plan.prefix + "/include -L" + plan.prefix + "/lib -ltable\n";
        if (plan.updateShellProfile && plan.prefix != "/usr/local" && plan.prefix != "/usr")
            msg += "\nOpen a new terminal (or re-source your shell rc file) to pick up the updated CPATH/LIBRARY_PATH.\n";
        fl_message("%s", msg.c_str());
        g_ui.window->hide();
    }
    else
    {
        fl_alert("%s", err.c_str());
        g_ui.installBtn->activate();
        g_ui.browseBtn->activate();
        g_ui.progress->value(0);
        g_ui.status->copy_label("");
    }
}

static void CancelCallback(Fl_Widget*, void*)
{
    g_ui.window->hide();
}

int main(int, char**)
{
    Fl_Window win(420, 260, "Table SDK Setup");
    g_ui.window = &win;

    Fl_Box title(10, 8, 400, 20, "Table SDK");
    title.labelfont(FL_BOLD);
    title.labelsize(16);
    title.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    Fl_Box pathLabel(10, 40, 400, 16, "Install location:");
    pathLabel.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    Fl_Input pathInput(10, 60, 300, 26);
    pathInput.value(DefaultInstallPrefix().c_str());
    g_ui.pathInput = &pathInput;

    Fl_Button browseBtn(320, 60, 90, 26, "Browse...");
    browseBtn.callback(BrowseCallback);
    g_ui.browseBtn = &browseBtn;

    Fl_Check_Button shellChk(10, 96, 400, 22, "Update shell profile (CPATH / LIBRARY_PATH) if needed");
    shellChk.value(1);
    g_ui.shellChk = &shellChk;

    Fl_Progress progress(10, 130, 400, 20);
    progress.minimum(0);
    progress.maximum(100);
    progress.selection_color(FL_BLUE);
    g_ui.progress = &progress;

    Fl_Box status(10, 156, 400, 20, "");
    status.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    g_ui.status = &status;

    Fl_Return_Button installBtn(230, 220, 90, 28, "Install");
    installBtn.callback(InstallCallback);
    g_ui.installBtn = &installBtn;

    Fl_Button cancelBtn(330, 220, 80, 28, "Cancel");
    cancelBtn.callback(CancelCallback);
    g_ui.cancelBtn = &cancelBtn;

    win.end();
    win.show();
    return Fl::run();
}
