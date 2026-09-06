# VOIDSTAR / TABLE

A tagged, type-erased container library for C/C++, offering five container
backends — a growable array, a fixed stack array, a heap-based doubly-linked
list, a pool-backed doubly-linked list, and a hash map — built on a
consistent naming convention and ownership model.

VOIDSTAR/TABLE exists to answer a simple question: how far can a
hand-written, allocation-disciplined C container library go if you actually
take memory layout and ownership seriously, instead of reaching for the
first `void*` + `switch` that compiles? It isn't a drop-in replacement for
`std::vector`, Python's `list`, or Java's collections — it solves a
different problem (predictable, low-overhead storage in systems and
performance-sensitive code) and makes different tradeoffs (manual teardown,
fixed capacities in some variants, convention-enforced type safety instead
of compiler-enforced).

## Why five containers?

Different jobs call for different memory strategies:

| Container | Storage | Grows? | Heap allocation per value? |
|---|---|---|---|
| `Table` | Dynamic array | Yes | Yes (owned types) |
| `TableStack` | Fixed array on the caller's stack | No | Never |
| `TableList` | Doubly-linked list | Yes | Yes (one heap node per value) |
| `StackTableList` | Doubly-linked list, backed by a fixed internal pool | No (bounded by pool size) | Never |
| `TableMap` | Bucketed hash map | No (fixed pool capacity) | Yes (owned types) |

Reach for `Table` by default. Reach for `TableStack` or `StackTableList` when
you want zero heap allocation and know your upper bound ahead of time.
Reach for `TableMap` when you need keyed lookup and can afford its fixed
~16.5 KB footprint (see [MANUAL.md](./MANUAL.md) for exact numbers).

## Every value knows its own type

Every slot, in every container, is a small struct pairing a payload pointer
with a `short` type tag (`TYPE_INT`, `TYPE_FLOAT`, `TYPE_STRUCT`, ...).
There's no `union` involved — type erasure here is convention-based: the tag
tells you how to interpret the payload, and every `Push`/`Get` function
either enforces the tag itself or offers a `_Try*` variant that validates it
for you before handing back a value. See `bTryGetSlotAtTable`,
`bTryGetSlotAtStackTyped`, `bTryGetIntAt`, `bTryMapGetTyped`, and friends.

## Quick example

```c
#include "table.h"

Table myTable;
vGetTable(&myTable, 8, 0);

bPushInt(&myTable, 42);
bPushFloat(&myTable, 3.14f);

TableSlot_DA slot = sGetSlotAtTable(&myTable, 0);
if (slot.type == TYPE_INT)
    printf("%d\n", *(int*)slot.ptr);

vDropTable(&myTable);
```

For a short tour of all five containers, see `tests/hello_table.cpp`.
For the full, exhaustive function-coverage test suite, see `tests/test1.cpp`.

## Building

The library is two files: `include/table.h` and `src/table.cpp`. Compile
them together with anything that uses the API:

```
# MSVC
cl.exe your_program.cpp src/table.cpp

# GCC / Clang
g++ -std=c++11 your_program.cpp src/table.cpp -o your_program
```

`table.cpp` currently relies on a small number of C++-only constructs (bare
brace-init returns, e.g. `return { NULL, TYPE_EMPTY };`), so build it as
C++ rather than strict C99/C11 unless those sites are first converted to
compound literals.

## Project layout

```
Table/
├── include/
│   └── table.h          — all type, macro, and function declarations
├── src/
│   └── table.cpp         — implementation
└── tests/
    ├── test1.cpp          — exhaustive function-coverage test suite
    └── hello_table.cpp    — minimal introductory tour of all five containers
```

## Documentation

- **README.md** (this file) — what the project is and why it exists.
- **[MANUAL.md](./MANUAL.md)** — the full reference: every container's API,
  ownership rules, memory-layout numbers, known limitations, and worked
  examples.

## Known limitations

- `TYPE_STRUCT`/`TYPE_UNION` payload size is never recorded on the slot, so
  operations that need to know a value's size to copy it safely
  (`bDupTopTable`, `bCloneTable`, `bCloneTableMap`, `bMapMergeInto`) skip
  these types with a warning rather than guessing.
- `TableStack`, `StackTableList`, and `TableMap` all have fixed capacities
  set at compile time (`TABLE_STACK_DEFAULT_CAPACITY`,
  `STACK_TABLE_CAPACITY`, `HASH_POOL_CAPACITY`). Exceeding them fails the
  operation rather than growing.
- `bMapMergeInto` deduplicates non-owned types (function/file pointers) by
  pointer identity and owned types (ints, floats, structs, ...) by value
  equality — see MANUAL.md for why these need different rules.

## License

_(add your license here)_
