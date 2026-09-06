#pragma once

#define IDD_MAIN        101
#define IDI_APPICON     201

#define IDC_PATH_EDIT   1001
#define IDC_BROWSE      1002
#define IDC_CHK_MSVC    1003
#define IDC_CHK_MINGW   1004
#define IDC_INSTALL     1005
#define IDC_PROGRESS    1006
#define IDC_STATUS      1007
#define IDC_TITLE       1008

// Embedded payload resources (RT_RCDATA). Baked into the exe at build time
// from installer/payload/ - see installer.rc. A resource with size 0 means
// "not built for this toolchain" (see payload/lib/msvc/table.lib, which is
// a 0-byte placeholder until a real MSVC build is dropped in).
#define ID_RES_HEADER    3001
#define ID_RES_MSVC_LIB  3002
#define ID_RES_MINGW_LIB 3003
