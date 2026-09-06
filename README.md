# Table SDK

A generic, type-tagged container library for C/C++ — dynamic array, fixed-capacity stack, doubly linked list (heap and fixed-pool variants), and a hash map — all supporting the same core value types (`int`, `float`, `double`, `char`, `long`, `short`) plus pointers, structs, unions, function pointers, and file pointers through a single tagged-slot design.

Ships as a one-file Windows installer (`Table.exe`) and a Linux install script, so consumers just write:

```c
#include <table.h>
```

and compile normally — no manual include/lib paths, no linker flags to remember.

## Install

**Windows:** download `Table.exe` and run it. Pick an install location (defaults to `%LOCALAPPDATA%\TableSDK`), leave the toolchain checkbox(es) checked, click **Install**, then restart your terminal.

**Linux:**
```
./scripts/build_linux.sh
sudo ./linux/install.sh          # installs to /usr/local by default
```

Full build-from-source instructions (MSVC, MinGW-w64, Linux) and complete API reference are in [`MANUAL.docx`](MANUAL.docx).

## Quick start

```c
// hello_table.c
#include <table.h>

int main(void) {
    Table t;
    vGetTable(&t, 8, 1024);        // 8 slots, 1KB arena

    PUSH_INT(&t, 42);
    PUSH_FLOAT(&t, 3.14f);

    TableSlot_DA slot = sGetSlotAtTable(&t, 0);
    printf("first = %d\n", GET_STRUCT_MEMBER(&slot, 0, int));

    vDropTable(&t);
    return 0;
}
```

```
# MSVC
cl hello_table.c

# GCC / Clang (MinGW-w64 or Linux)
gcc hello_table.c -ltable -o hello_table
```

Both work with zero extra include/link flags — the installer wires up `INCLUDE`/`LIB` (MSVC) and `C_INCLUDE_PATH`/`CPLUS_INCLUDE_PATH`/`LIBRARY_PATH` (MinGW/GCC), and `table.h` carries its own `#pragma comment(lib, "table.lib")` for MSVC auto-linking.

## Containers at a glance

| Container | Backing storage | Fixed size? | Use when |
|---|---|---|---|
| `Table` | Heap array + arena allocator | No, grows | General-purpose dynamic array |
| `TableStack` | Fixed array | Yes, capacity set at init | Predictable, bounded LIFO storage |
| `TableList` | Heap doubly linked list | No, grows | Frequent insert/remove at both ends or middle |
| `StackTableList` | Fixed node pool, linked via indices | Yes | Linked-list semantics without heap allocation |
| `TableMap` | Fixed pool hash table w/ chaining | Yes, pool size set at compile time | Key → value lookup |

See `MANUAL.docx` for the complete API reference, per-type macro tables, and troubleshooting notes for common toolchain issues (32-bit vs 64-bit MinGW, static linking, manifest conflicts, `PATH` quirks with spaces in usernames).

## Repository layout

```
Inside TableSDK folder
|
include/table.h         Public header (single file)
src/table.cpp           Implementation
scripts/                build_msvc.bat, build_mingw.bat, build_linux.sh
linux/install.sh        System-wide Linux installer
installer/              Win32 GUI installer source (Table.exe)
dist/                   Build output per toolchain
MANUAL.docx             Full API reference and troubleshooting guide
```
