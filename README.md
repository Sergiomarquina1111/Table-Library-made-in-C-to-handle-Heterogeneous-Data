# Table SDK

A generic, type-tagged container library for C. It gives you three collection
types — a dynamic array/stack (`Table`), a fixed-capacity zero-malloc stack
(`TableStack`), and a hash map (`TableMap`) — that can each hold heterogeneous
C values (ints, floats, structs, unions, pointers, function pointers, file
handles) side by side, tagged so the library always knows what it's holding.
Underneath all three sits a custom boundary-tag memory arena with free-list
splitting and coalescing, plus a small standard library of sort/filter/
transform/foreach helpers layered on top.

- **Language:** C (compiles clean under both `gcc`/`clang` and MSVC)
- **Size:** ~1,180 lines of header, ~6,500 lines of implementation
- **License:** add yours here

## Why

C has no built-in way to say "a list of mixed types" without either giving up
type safety entirely (`void*` everywhere, hope for the best) or hand-rolling a
tagged union for every combination of types you need. Table SDK does the
tagging once, generically, so a single `Table` can hold an `int`, a `double`,
a `struct Point`, and a `FILE*` in adjacent slots, and code that walks the
table can always ask "what's actually in this slot" before touching it.

## The three container types

| Type | Backing | Use it when |
|---|---|---|
| `Table` | Heap slots, grows via the arena | You need a dynamic array/stack and don't know the size up front |
| `TableStack` | Fixed-size array, no heap allocation for primitives | You want a stack with predictable, zero-malloc behavior and a known upper bound |
| `TableMap` | Fixed-capacity open hash table with chaining | You need key-style lookup by pointer identity, with pluggable hashing |

All three share the same type-tagging scheme (see below), and a parallel set
of function names: `...Table`, `...Stack`, `...Map` suffixes on otherwise
identical operations (`bPushInt` / `bPushIntStack` / `tMapInsertInt`, etc).

## Type tagging

Every value pushed into a container is stored next to a `short` type tag, so
the container knows how to print it, free it, and validate access to it. Tags
are grouped into five bands:

| Band | Examples | Meaning |
|---|---|---|
| `DATA_TYPE` (0–9) | `TYPE_INT`, `TYPE_FLOAT`, `TYPE_DOUBLE`, `TYPE_CHAR`, `TYPE_LONG`, `TYPE_SHORT` | Plain scalar values |
| `POINTERS` (10–99) | `TYPE_INT_STAR`, `TYPE_CHAR_STAR`, `TYPE_VOID_STAR`, ... | Single-level pointers |
| `DOUBLE_POINTERS` (100–999) | `TYPE_INT_STAR_DOUBLE`, ... | Pointer-to-pointer |
| `TRIPLE_POINTERS` (1000–1999) | `TYPE_INT_STAR_TRIPLE`, ... | Pointer-to-pointer-to-pointer |
| `USER_DEFINED` (2000+) | `TYPE_STRUCT`, `TYPE_UNION`, `TYPE_STRUCT_PTR`, `TYPE_UNION_PTR`, `TYPE_FUNC_PTR`, `TYPE_FILE_PTR` | Caller-defined aggregate types |

`bIsOwnedTypeTable()` / `bIsOwnedTypeStack()` / `bIsOwnedTypeMap()` tell you,
for any tag, whether the container heap-owns that slot's memory (and will
free it on drop/remove) or whether it's a raw pointer the library never
touches.

## Quick start

```c
#include <table.h>

int main(void) {
    Table t;
    vGetTable(&t, /*total_slots hint*/ 16, /*arena_bytes*/ 1 << 16);

    bPushInt(&t, 42);
    bPushDouble(&t, 3.14);
    bPushChar(&t, 'x');

    vPrintTable(&t);          // dumps every slot with its type name and value
    printf("sum of ints: %d\n", iSumIntTable(&t));

    vDropTable(&t);           // frees every owned slot back to the arena
    vDropArena();             // release the arena's backing memory (once
                               // every Table/TableStack/TableMap is dropped)
    return 0;
}
```

Fixed-capacity, zero-malloc stack variant:

```c
vGetTableStackDefault(s);     // declares `s` with TABLE_STACK_DEFAULT_CAPACITY
PUSH_INT_STACK(&s, 7);
PUSH_FLOAT_STACK(&s, 2.5f);
vPrintTableStack(&s);
```

Hash map:

```c
TableMap map;
vFormHashMap(&map);

void* key;
tMapInsertInt(&map, 100, &key);

TableSlot_HM slot;
if (bTryMapGetTyped(&map, key, TYPE_INT, &slot))
    printf("found: %d\n", *(int*)slot.ptr);

vDropHashMap(&map);
```

## Building

Prebuilt static libraries and the matching public header are produced by the
scripts in `scripts/` and staged under `dist/` and `payload/`:

| Platform / toolchain | Artifact | Status in this build |
|---|---|---|
| Linux (gcc) | `dist/linux/libtable.a` | ✅ present, valid archive |
| Windows, MinGW x64 | `payload/lib/mingw/x64/libtable.a` | ✅ present, valid archive |
| Windows, MinGW x86 | `payload/lib/mingw/x86/libtable.a` | ✅ present, valid archive |
| Windows, MSVC x64 | `payload/lib/msvc/x64/table.lib` | ❌ **empty file — needs rebuilding** |
| Windows, MSVC x86 | `payload/lib/msvc/x86/table.lib` | ❌ **empty file — needs rebuilding** |
| macOS | `dist/macos/` | ⚠️ build script exists, never run/verified |

If you're consuming this package as-is, don't assume the MSVC libraries
link correctly — rebuild them from `source/table.c` with `scripts/build_msvc.bat`
before shipping to MSVC users.

To build from source directly:

```sh
# gcc / clang
cc -c source/table.c -o table.o
ar rcs libtable.a table.o

# MSVC (from a Developer Command Prompt)
cl /c source\table.c
lib /OUT:table.lib table.obj
```

Include `source/table.h` (or `payload/include/table.h`, `include/table.h` —
these are duplicates of the same header) in your project and link the
resulting library.

## Known limitations

Being upfront about what hasn't been fully hardened yet:

- **Strict-aliasing violation in `GET_STACK_VALUE`.** The macro
  (`*(T*)&elem.ptr`) reads a `void*` cell through a differently-typed lvalue,
  which is undefined behavior under strict aliasing. It works today under
  `-O2` with mainstream compilers, but is not guaranteed to keep working under
  a different optimizer, LTO, or a future compiler version. Not yet fixed.
- **No thread safety.** `g_MasterArena` is a global, unsynchronized singleton.
  Fine single-threaded; will corrupt under concurrent use from multiple
  threads without external locking.
- **`TYPE_STRUCT` / `TYPE_UNION` size isn't tracked on the tag.** `bCloneTable`
  / `bFilterTable` and similar deep-copy operations can't correctly clone
  struct/union slots without the caller separately supplying the size.
  Struct/union values also can't be safely rehydrated on the receiving end
  of `vForEachSlotTable` without external knowledge of the payload size.
- **No automated test suite.** What's been verified is smoke-level: push a
  few values, sort, sum, check the result. The arena's edge cases (coalescing
  at the first/last chunk, overflow paths), the hash map's collision
  handling, and sort/filter/transform under adversarial input haven't been
  exercised under a sanitizer (ASan/UBSan).
- **macOS is unverified.** `scripts/build_macos.sh` exists but has never been
  run against a real macOS toolchain.

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
