# VOIDSTAR / TABLE

**Version 2.1** &nbsp;·&nbsp; A tagged, type-erased container library for C

TABLE gives you a small set of heterogeneous container types — a dynamic
array, a zero-malloc fixed-capacity stack, a doubly-linked list, a
stack-backed (zero-malloc) doubly-linked list, and an open-addressing-free
hash map — that can each hold a mix of value types (`int`, `float`,
`double`, `char`, `long`, `short`), pointer/double-pointer/triple-pointer
wrappers, structs, unions, function pointers, and `FILE*` streams, side by
side in the same container, with a type tag carried alongside every slot.

Pure C (C11), no C++ required. Ships prebuilt for MSVC and MinGW, both
x86 and x64.

---

## Why

C has no generics and no `std::any`. TABLE is a from-scratch answer to
"I want one container that can hold different types safely," built around
a tagged slot (`{ void* ptr; short type; }`) instead of templates —
every push records what it stored, every read can check it back.

---

## Features

- **Table** — dynamic array variant, heap-backed, grows on demand.
- **TableStack** — fixed-capacity, zero-malloc variant. Primitives are
  packed directly into the slot's pointer-sized cell instead of being
  heap-allocated, so pushing an `int` never touches `malloc`.
- **TableList** — heap-backed doubly-linked list.
- **StackTableList** — zero-malloc doubly-linked list, backed by a fixed
  internal node pool with its own free list (no `malloc` per node).
- **TableMap** — a hash map with no per-insert heap allocation: an
  index-based arena of `TableSlot_HM` nodes, a 16-bit free list, and
  circular doubly-linked bucket chains for collision handling.
- Type-checked accessors (`bTryGetSlotAtTable`, `bTryMapGetTyped`, …)
  that validate an index *and* a type tag before handing back data,
  instead of trusting the caller.
- Sort / filter / transform layered on top of the core array API
  (`vSortTable`, `bFilterTable`, `vTransformTable`).
- A large `PUSH_*` / `GET_*` macro layer so call sites read like
  `PUSH_INT(&t, 5)` instead of `bPushInt(&t, 5)`.

See **MANUAL.md** for the full API reference.

---

## Supported toolchains

| Toolchain      | Architectures  | Output                              |
|----------------|----------------|--------------------------------------|
| MSVC (`cl.exe`)| x64, x86       | `table.lib`, `table_x86.lib`         |
| MinGW (`gcc`)  | x64, x86       | `libtable.a` (one per arch folder)   |

Both architectures of both toolchains can be built and installed side by
side without conflict — see **Multi-architecture support** below.

---

## Installation

1. Run `installer.exe`. It installs to `%LOCALAPPDATA%\TableSDK` by
   default (no admin rights required) and updates your **user**
   environment variables (`INCLUDE`, `LIB`, `C_INCLUDE_PATH`,
   `CPLUS_INCLUDE_PATH`, `LIBRARY_PATH`, `PATH`) so the compiler can
   find `table.h` and the correct prebuilt library automatically.
2. **Open a new terminal** after installing — environment variable
   changes only apply to terminals opened *after* the change; a
   terminal that was already open when you ran the installer won't
   see the update.
3. Re-running the installer is safe and self-correcting: it removes
   its own previous entries before re-adding the current ones, so
   re-installing (e.g. after installing a second MinGW toolchain)
   never leaves stale or conflicting paths behind.

### MSVC

```
cl.exe HelloTable.c
```

That's it — `table.h` auto-links the correct `.lib` for you via
`#pragma comment(lib, ...)`, selecting `table.lib` or `table_x86.lib`
based on which architecture `cl.exe` is actually compiling for
(`_M_X64` / `_M_IX86`), not which Native Tools prompt happens to be
open.

### MinGW

```
gcc HelloTable.c -ltable
```

`-ltable` is required — MSVC's auto-link `#pragma` is a Microsoft
extension that GCC/Clang ignore. `LIBRARY_PATH` (set by the installer)
points at the one MinGW library folder that matches your `gcc.exe`'s
actual architecture, detected via `gcc -dumpmachine` at install time.

### Multi-architecture support (both 32-bit and 64-bit MinGW installed)

If your machine has **both** a 32-bit MinGW (e.g. classic
`C:\MinGW\bin`) and a 64-bit MinGW (e.g. `C:\mingw64\bin`) installed,
a bare `gcc -ltable` is inherently ambiguous — GNU `ld` resolves
`-ltable` to the first `libtable.a` it finds on `LIBRARY_PATH` and
does **not** skip a wrong-architecture archive the way some assume;
picking the wrong one produces a wall of `undefined reference` errors
that all trace back to this.

To make the choice unambiguous, the installer generates four
architecture-pinned wrapper commands (only for the toolchains it
actually finds installed on your machine):

```
gcc32  HelloTable.c -o HelloTable.exe     # always 32-bit MinGW + x86 lib
gcc64  HelloTable.c -o HelloTable.exe     # always 64-bit MinGW + x64 lib
g++32  HelloTable.cpp -o HelloTable.exe   # 32-bit MinGW g++ variant
g++64  HelloTable.cpp -o HelloTable.exe   # 64-bit MinGW g++ variant
```

Each wrapper hardcodes its own compiler's full path plus explicit
`-I`/`-L`/`-ltable` flags, so it's correct regardless of `PATH` order,
`LIBRARY_PATH` contents, or which toolchain a plain `gcc` currently
resolves to. Use these instead of a bare `gcc`/`g++` whenever both
architectures are installed and you need to pick one deliberately.

---

## Quick example

```c
#include <table.h>

int main(void)
{
    Table t;
    vGetTable(&t, 8, 0);       // 8 initial slots, no arena

    PUSH_INT(&t, 42);
    PUSH_FLOAT(&t, 3.14f);
    PUSH_CHAR(&t, 'x');

    TableSlot_DA slot;
    TABLE_FOREACH(&t, i, slot)
    {
        printf("[%d] type=%s\n", i, sTypeNameTable(slot.type));
    }

    vDropTable(&t);
    return 0;
}
```

---

## Building from source

`build.bat` builds whichever toolchain matches the prompt it's run
from:

```
build.bat        (from x86 Native Tools Command Prompt for VS)  -> MSVC x86
build.bat        (from x64 Native Tools Command Prompt for VS)  -> MSVC x64
build.bat        (from a plain cmd.exe)                         -> MinGW x64 and/or x86
```

Run it from all three to build every combination — it only ever adds
to `payload\`, never removes what a previous run already staged, so
order doesn't matter. Once at least one library exists, it builds
`installer.exe` from whatever is currently staged.

MinGW paths are configured at the top of `build.bat`:

```bat
set MINGW64_BIN=C:\mingw64\bin
set MINGW32_BIN=C:\MinGW\bin
```

Edit these if your installs live elsewhere.

---

## Project layout

```
TableSDK/
├── source/            canonical table.h / table.c (edit these)
├── include/            synced copy used by the build
├── src/                 synced copy used by the build
├── dist/                intermediate per-toolchain build output
├── payload/              staged files that get embedded into installer.exe
├── build.bat
├── installer.cpp / installer.rc
└── README.md / MANUAL.md
```

---

## License

Not yet specified.

---

## More

Full function-by-function reference: **MANUAL.md**
