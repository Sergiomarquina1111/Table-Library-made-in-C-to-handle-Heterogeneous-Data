# VOIDSTAR / TABLE — Manual

This is the full reference for the library: what each container does, how
ownership works, what things cost in memory, and where the sharp edges are.
For a quick "what is this and why" overview, see [README.md](./README.md).

Note:
Run these on your cmd before you ever test and run these after your installation is complete

setx LIB=%LOCALAPPDATA%\TableSDK\lib\msvc;%LIB%
setx INCLUDE=%LOCALAPPDATA%\TableSDK\include;%INCLUDE%

if mingw user

setx CPLUS_INCLUDE_PATH=%LOCALAPPDATA%\TableSDK\include
setx LIBRARY_PATH=%LOCALAPPDATA%\TableSDK\lib\mingw

and use
#pragma comment(lib, "table.lib")

after you include table.h in your code else you can give it in msvc compile command

---

## Table of contents

1. [Core concepts](#core-concepts)
2. [Table (heap dynamic array)](#1-table-heap-dynamic-array)
3. [TableStack (fixed stack array)](#2-tablestack-fixed-stack-array)
4. [TableList (heap doubly-linked list)](#3-tablelist-heap-doubly-linked-list)
5. [StackTableList (pool-backed linked list)](#4-stacktablelist-pool-backed-linked-list)
6. [TableMap (hash map)](#5-tablemap-hash-map)
7. [Memory footprint reference](#memory-footprint-reference)
8. [Known limitations and sharp edges](#known-limitations-and-sharp-edges)
9. [Build notes](#build-notes)

---

## Core concepts

### The slot pattern

Every container stores values as a small struct pairing a payload pointer
with a `short` type tag:

```c
typedef struct { void* ptr; short type; } TableSlot_DA;   // Table
typedef struct { void* ptr; short type; } TableSlot_SA;   // TableStack
typedef struct { void* data; short type; } TableSlot_LL;  // TableList
typedef struct { void* data; short type; } TableSlot_STLL;// StackTableList
typedef struct { void* ptr; uint16_t next, prev; short type; } TableSlot_HM; // TableMap
```

This is **not** a `union` — there's no compiler-enforced overlap between
possible value types. Type erasure here is entirely convention-based: the
`type` field tells you how the payload should be interpreted, and it's the
caller's job (or a `_Try*` helper's job) to check it before casting.

Type tags are grouped by numeric range so a tag's *category* can be checked
with a single comparison:

| Range | Category | Examples |
|---|---|---|
| 0–9 | Primitives | `TYPE_INT`, `TYPE_FLOAT`, `TYPE_DOUBLE`, `TYPE_CHAR`, `TYPE_LONG`, `TYPE_SHORT` |
| 10–19 | Single pointers | `TYPE_INT_STAR`, `TYPE_VOID_STAR`, ... |
| 100–109 | Double pointers | `TYPE_INT_STAR_DOUBLE`, ... |
| 1000–1009 | Triple pointers | `TYPE_INT_STAR_TRIPLE`, ... |
| 2000+ | User-defined / system | `TYPE_STRUCT`, `TYPE_UNION`, `TYPE_FUNC_PTR`, `TYPE_FILE_PTR` |

### Ownership: who frees what

This is the single most important thing to understand before using any
container. Each container has a private "does this type own heap memory"
predicate (`bTypeOwnsHeapPtr`, `bTypeOwnsHeapPtrMap`, ...), exposed publicly
as `bIsOwnedTypeTable()` / `bIsOwnedTypeMap()`:

- **Owned types** (`TYPE_INT`, `TYPE_FLOAT`, `TYPE_STRUCT`, `TYPE_UNION`, and
  all pointer-wrapper types): the container `malloc`'d (or arena-allocated)
  this payload itself, and **will free it** on drop/remove/pop.
- **Non-owned types** (`TYPE_FUNC_PTR`, `TYPE_FILE_PTR`, `TYPE_STRUCT_PTR`,
  `TYPE_UNION_PTR`): the payload is a raw pointer supplied by the caller.
  The container stores the pointer value itself and **never frees it** —
  that memory (or resource, like an open `FILE*`) is the caller's
  responsibility.

Getting this wrong in your own code is the most likely way to crash: don't
insert a stack address as an owned type (it will eventually be `free()`'d),
and don't expect a `FILE*` you pushed to be closed for you.

### Type-checked access

Most containers offer two ways to read a value back:

- **Plain getters** (`sGetSlotAtTable`, `tGetIntAt`, ...) — fast, but if the
  index is out of range or the type doesn't match what you expected, you
  either get an empty/zero sentinel or (for the primitive wrappers) a
  silently wrong reinterpretation.
- **`_Try*` variants** (`bTryGetSlotAtTable`, `bTryGetIntAt`,
  `bTryMapGetTyped`, ...) — validate both the index/key and the type tag
  before writing to your output parameter; return `false` and leave your
  output untouched on any mismatch.

Prefer the `_Try*` versions whenever the expected type isn't already
guaranteed by surrounding logic.

---

## 1. Table (heap dynamic array)

**Header type:** `Table` · **Slot type:** `TableSlot_DA`

The general-purpose container. A growable array of slots, backed by
`realloc()`-based geometric growth (1.5×, starting from a caller-chosen
initial capacity). Reach for this by default.

### Creating and destroying

```c
Table t;
vGetTable(&t, /* initial capacity */ 8, /* arena bytes, 0 = use malloc */ 0);
// ... use it ...
vDropTable(&t);   // frees every owned payload, then the backing array
```

The third argument to `vGetTable` optionally maps a shared internal memory
arena (`g_MasterArena`) the first time it's called with a non-zero size;
subsequent calls share that same arena. Pass `0` if you don't need it —
`pAllocArena()` silently falls back to plain `malloc()` when no arena is
mapped.

### Pushing values

```c
bPushInt(&t, 42);
bPushFloat(&t, 3.14f);
bPushChar(&t, 'A');

int x = 7;
bPushPtr(&t, &x, TYPE_INT_STAR);          // wraps a pointer
bPushStruct(&t, &myStruct, sizeof(myStruct)); // deep-copies a struct
bPushFunc(&t, (void*)my_function);        // stores as-is, never freed
bPushFile(&t, fopen("log.txt", "w"));     // stores as-is, never closed
```

Macros exist for every primitive/pointer combination (`PUSH_INT`,
`PUSH_INT_PTR`, `PUSH_INT_DPTR`, `PUSH_INT_TPTR`, `PUSH_STRUCT`, ...) —
these are thin wrappers, useful mainly for consistent call-site style.

`bPushList()` pushes several `(type, value)` pairs in one variadic call.

### Reading values

```c
TableSlot_DA slot = sGetSlotAtTable(&t, 0);
short type = sGetTypeAtTable(&t, 0);

TableSlot_DA typed;
if (bTryGetSlotAtTable(&t, 0, TYPE_INT, &typed))
    printf("%d\n", *(int*)typed.ptr);
```

### Removing values

```c
TableSlot_DA popped = sPopSlot(&t);   // removes and returns the top slot
bRemoveAtTable(&t, 3);                // removes by index, shifts the rest left
```

`sPopSlot()` has an important asymmetry: for **primitives**, it deep-copies
the value out to a fresh allocation and frees the original in place before
returning — you own the returned copy and must eventually free it yourself
via `vFreeArena()`. For **everything else** (pointers, structs, func/file
handles), it hands back the original pointer as-is; ownership transfers to
you.

### Query and aggregate helpers

```c
iCountOfTypeTable(&t, TYPE_INT);
bContainsTypeTable(&t, TYPE_STRUCT);
iFindTypeTable(&t, TYPE_CHAR);
iSumIntTable(&t);   dSumFloatTable(&t);
bMinIntTable(&t, &out);   bMaxIntTable(&t, &out);
```

### Stack-like operations

```c
bDupTopTable(&t);    // duplicates the top slot: [A,B] -> [A,B,B']
bSwapTopTable(&t);   // swaps the top two slots
vReverseTable(&t);   // reverses slot order in place
```

`bDupTopTable()` refuses `TYPE_STRUCT`/`TYPE_UNION` — see
[Known limitations](#known-limitations-and-sharp-edges).

### Bulk operations

```c
int arr[] = {1,2,3};
iPushIntArrayTable(&t, arr, 3);      // grows automatically as needed
iPushFloatArrayTable(&t, farr, n);

Table clone;
vGetTable(&clone, 4, 0);
bCloneTable(&clone, &t);             // deep-clones every clonable slot

vReserveTable(&t, 16);                // pre-grow by N slots in one realloc
vShrinkToFitTable(&t);                 // realloc down to exactly `count` slots
```

### Insert/remove at arbitrary index

```c
bInsertAtTable(&t, 0, someSlot);   // O(n): shifts everything right
bRemoveAtTable(&t, 0);              // O(n): frees if owned, shifts left
```

---

## 2. TableStack (fixed stack array)

**Header type:** `TableStack` · **Slot type:** `TableSlot_SA`

A fixed-capacity array that lives entirely on the caller's stack (or
wherever the caller declares it) — **zero heap allocation**, ever. Values
that fit in a pointer's width (`sizeof(void*)`, so 8 bytes on 64-bit) are
stored **inline**, punned directly into the slot's `ptr` field via
`memcpy`. There is no growth: once full, pushes fail.

### Creating

```c
vGetTableStack(myStack, 32);   // macro: declares the backing array AND the
                                 // TableStack struct, named `myStack`, with
                                 // capacity 32 - both live in this scope
```

`vGetTableStackDefault(name)` uses `TABLE_STACK_DEFAULT_CAPACITY` (32 by
default) instead of a caller-supplied number.

### Pushing and popping

```c
bPushIntStack(&myStack, 5);
bPushFloatStack(&myStack, 1.5f);
bPushStructStack(&myStack, &small, sizeof(small)); // must be <= sizeof(void*)

TableSlot_SA top = sPopSlotStack(&myStack);
bDropTopStack(&myStack);   // same, but discards the value (cheaper)
```

`bPushStructStack`/`bPushUnionStack` refuse anything larger than
`sizeof(void*)` — there's no separate byte arena in this variant, only the
one inline cell per slot. Use the heap-based `Table` for larger
structs/unions.

### Reading, querying, bulk ops

Same shape as `Table`'s equivalents, with a `Stack` suffix:
`sGetSlotAtStack`, `bTryGetSlotAtStackTyped`, `iCountOfTypeStack`,
`iSumIntStack`, `bMinIntStack`/`bMaxIntStack`, `vReverseStack`,
`iPushIntArrayStack`, `bCloneTableStack`. `bCloneTableStack` requires the
destination to already have enough capacity — it never grows anything.

### Teardown

```c
vDropTableStack(&myStack);   // zeroes the slots; nothing to free
```

---

## 3. TableList (heap doubly-linked list)

**Header type:** `TableList` · **Slot type:** `TableSlot_LL` (held in `tNode`)

A conventional heap-allocated doubly-linked list. No fixed capacity, no
resizing concept — it grows one `malloc`'d node at a time.

### Creating and destroying

```c
TableList list;
tFormLinkedTable(&list);
// ... use it ...
tDestroyList(&list);   // frees every node
```

### Pushing, popping, peeking

```c
tPushIntBack(&list, 1);
tPushIntFront(&list, 0);
int v = tPopIntFront(&list);
TableSlot_LL front = sPeekFront(&list);   // read without removing
```

Every primitive and pointer/struct/union/func/file type has matching
`tPush*Back`/`tPush*Front`/`tGet*At`/`tPop*Front`/`tPop*Back`/
`tInsert*At`/`tRemove*At`/`tFind*` functions, plus convenience macros
(`PUSH_INT`, `GET_INT`, `POP_INT`, `INSERT_INT`, `REMOVE_INT`, `FIND_INT`,
...) — see `table.h` for the full set.

### Type-checked getters

```c
int out;
if (bTryGetIntAt(&list, 0, &out)) { ... }
```

Available for every primitive type (`bTryGetFloatAt`, `bTryGetDoubleAt`,
`bTryGetCharAt`, `bTryGetLongAt`, `bTryGetShortAt`).

### Structural operations

```c
tInsertAt(&list, 2, someSlot);   // O(n) traversal to position
tRemoveAt(&list, 2);
vReverseList(&list);              // O(n), reverses in place
```

Note: `TableList` never deep-copies struct/union/pointer payloads — pushing
a struct pointer stores the pointer as-is (`tPushStructBack` etc.). The
list does not own or free what these pointers point to.

---

## 4. StackTableList (pool-backed linked list)

**Header type:** `StackTableList` · **Slot type:** `TableSlot_STLL` (held in `sNode`)

Behaves exactly like `TableList` from the outside (doubly-linked, push/pop
either end, insert/remove/find by index), but every node comes from a
**fixed internal pool** (`sNode pool[STACK_TABLE_CAPACITY]`, 100 by
default) via an intrusive free-list, instead of individual `malloc()`
calls. Still **zero per-node heap allocation**.

### Creating and destroying

```c
StackTableList list;
tStackFormLinkedTable(&list);    // also rebuilds the internal free-list
// ... use it ...
tStackDestroyList(&list);         // returns every node to the pool
```

### API shape

Identical in spirit to `TableList`, with an `S_`-prefixed macro set and
`tStack`-prefixed functions: `tStackPushIntBack`, `tStackGetIntAt`,
`S_PUSH_INT`, `S_GET_INT`, `bStackIsListFull` (checks whether the pool is
exhausted), etc.

The pool has a hard ceiling of `STACK_TABLE_CAPACITY` live nodes across the
list's lifetime at any one instant — once exhausted, `tStackCreateNode`
fails and prints a fatal-error message rather than growing.

---

## 5. TableMap (hash map)

**Header type:** `TableMap` · **Slot type:** `TableSlot_HM`

A hash map with separate chaining, backed by a **fixed-size internal pool**
(`HASH_POOL_CAPACITY`, 1024 by default) and a fixed bucket array
(`HASH_TABLE_SIZE`, 67 by default). Keys are the **addresses of the values
themselves** — inserting a value hands you back that address as the key
via `out_key`.

### Creating and destroying

```c
TableMap map;
vFormHashMap(&map);
// ... use it ...
vDropHashMap(&map);   // frees every owned value, resets to empty
```

### Inserting and looking up

```c
void* ageKey = NULL;
tMapInsertInt(&map, 30, &ageKey);        // mallocs its own copy of 30

TableSlot_HM node = sMapGetNode(&map, ageKey);
if (node.type == TYPE_INT)
    printf("%d\n", *(int*)node.ptr);

TableSlot_HM typed;
if (bTryMapGetTyped(&map, ageKey, TYPE_INT, &typed)) { ... }
```

Typed insert functions exist for every primitive and pointer category
(`tMapInsertFloat`, `tMapInsertDouble`, `tMapInsertPtr`, `tMapInsertDPtr`,
`tMapInsertTPtr`, `tMapInsertStruct`, `tMapInsertUnion`, `tMapInsertFunc`,
`tMapInsertFile`), plus a raw `tMapInsert()` for advanced use where you
supply your own already-allocated payload and type tag directly — see the
warning below.

> **Important:** `tMapInsert()` (the raw form) does **not** allocate
> anything — it stores whatever pointer you give it, tagged with whatever
> type you give it. If that type is an "owned" type (see
> [Ownership](#ownership-who-frees-what)), `bMapRemove()`/`vDropHashMap()`
> will eventually call `free()` on it. **Never pass a stack address as an
> owned type through the raw `tMapInsert()`** — only heap-allocated
> pointers are safe there. Prefer the typed `tMapInsert<Type>()` helpers,
> which allocate the copy for you and get this right automatically.

### Removing

```c
bMapRemove(&map, ageKey);   // unlinks, frees if owned, returns the pool slot
```

### Query and diagnostics

```c
iCountMap(&map);   bIsEmptyMap(&map);   bIsFullMap(&map);
iCountOfTypeMap(&map, TYPE_INT);
iSumIntMap(&map);   dSumFloatMap(&map);
dLoadFactorMap(&map);
iBucketLengthMap(&map, someBucketIndex);
iMaxBucketLengthMap(&map);
bPointerHashIsSuspect(&map);   // heuristic: is the hash distributing poorly?
vRehashStatsMap(&map);          // prints a bucket-length histogram
```

### Custom hashing

```c
uint32_t my_hash(void* p) { /* ... */ }
vSetHashFnMap(&map, my_hash);   // NULL restores the default (uDefaultPointerHash)
```

### Clone and merge

```c
TableMap clone;
bCloneTableMap(&clone, &map);

TableMap merged;
vFormHashMap(&merged);
int merged_count, skipped_count;
bMapMergeInto(&merged, &map, &merged_count, &skipped_count);
```

Both functions skip `TYPE_STRUCT`/`TYPE_UNION` entries (unsizable — see
[Known limitations](#known-limitations-and-sharp-edges)) with a warning,
incrementing `skipped_count` in `bMapMergeInto`'s case.

**Dedup rule in `bMapMergeInto`:** non-owned types (func/file pointers) are
deduplicated by **pointer identity**, because they're never copied — the
same pointer really is the same entry. Owned types (ints, floats, structs,
...) are deduplicated by **value equality** (`memcmp` of the payload
bytes), because every merge produces a fresh heap copy at a new address —
identity comparison could never recognize "this value was already merged
in" for these types. This is why the two categories use different
comparison strategies: it's the only rule that gives `bMapMergeInto`
correct set-union semantics for both.

---

## Memory footprint reference

All figures below assume a 64-bit build (8-byte pointers) with default
capacities (`HASH_POOL_CAPACITY = 1024`, `HASH_TABLE_SIZE = 67`,
`STACK_TABLE_CAPACITY = 100`).

### Slot sizes (identical across every container, due to pointer alignment)

| Slot struct | Size |
|---|---|
| `TableSlot_DA`, `TableSlot_SA`, `TableSlot_LL`, `TableSlot_STLL` | 16 bytes |
| `TableSlot_HM` | 16 bytes |

### Linked-list node sizes

| Node | Size |
|---|---|
| `tNode` (TableList) | 32 bytes |
| `sNode` (StackTableList) | 32 bytes |

### Fixed cost per container instance (before storing anything)

| Container | Fixed/metadata cost |
|---|---|
| `Table` | ~16 bytes (metadata only; backing array grows separately) |
| `TableStack` (capacity N) | ~16 bytes metadata + `N × 16` bytes backing array |
| `TableList` | ~32 bytes |
| `StackTableList` | **~3,240 bytes** (`100 × 32` node pool + a few pointers) |
| `TableMap` | **~16,536 bytes** (`1024 × 16` slot pool + `67 × 2` buckets + metadata) |

### Marginal cost per stored `int`

| Container | Extra bytes per `int` |
|---|---|
| `Table` | ~16 (slot) + ~16–32 (separate heap block + allocator overhead) |
| `TableStack` | ~0 — stored inline in the already-counted slot |
| `TableList` | ~32 (one heap-allocated node) |
| `StackTableList` | ~0 — comes from the pool already paid for |
| `TableMap` | ~16 (slot, pre-paid in fixed cost) + ~16–32 (separate heap block) |

**Practical implication:** `TableMap` costs roughly 5× more just to exist
than `StackTableList`, and well over 1,000× more than an empty `Table` or
`TableList`. That fixed cost is paid once per `TableMap` instance,
regardless of how many entries you actually use — worth knowing before
declaring many small maps, or any map at all on a memory-constrained
target.

For comparison, general-purpose managed-language collections carry a
comparable or larger per-value tax for a different reason: Python's `list`
and Java's `ArrayList`/`HashMap` both pay an object-header cost (roughly
12–16 bytes of pure bookkeeping — refcount + type pointer, or mark word +
class pointer) on *every* boxed value, even a plain integer, before any of
your actual data is stored. VOIDSTAR/TABLE never pays that tax; its cost is
either the 16-byte slot plus a genuine heap allocation (`Table`/`TableMap`),
or nothing at all beyond the slot (`TableStack`/`StackTableList`).

---

## Known limitations and sharp edges

- **`TYPE_STRUCT`/`TYPE_UNION` size is never recorded on the slot.**
  `TableSlot_DA`/`TableSlot_HM` only store a `ptr` and a `type` tag — the
  byte size used to `malloc`/`memcpy` a struct or union at push time is
  never retained afterward. This means any operation that needs to safely
  duplicate a value by size — `bDupTopTable`, `bCloneTable`,
  `bCloneTableMap`, `bMapMergeInto` — cannot handle these two types and
  will skip them (with a printed warning) rather than risk copying the
  wrong number of bytes. If you need a struct/union to survive a clone or
  merge, push a fresh copy yourself with `bPushStruct`/`bPushUnion` /
  `tMapInsertStruct`/`tMapInsertUnion` afterward instead.

- **Fixed capacities fail rather than grow.** `TableStack`,
  `StackTableList`, and `TableMap` all have compile-time-fixed capacities.
  Exceeding any of them causes the relevant push/insert/create-node call to
  fail (returning `false`/`NULL` and printing an error) rather than
  silently growing. Size these generously up front if you're not sure how
  much you'll need, or use the unbounded `Table`/`TableList` instead.

- **`bMapMergeInto`'s two dedup strategies are intentional, not
  inconsistent.** See the note under [TableMap](#5-tablemap-hash-map) —
  identity-based dedup is correct for non-owned (raw pointer) types, and
  value-based dedup is necessary for owned (copied) types, because a
  second merge from the same source can never produce the same address for
  an owned value.

- **Ownership conventions are enforced by discipline, not the type
  system.** Passing a stack address to `tMapInsert()`/`bPushPtr()` tagged
  as an owned type, or forgetting that `TYPE_FUNC_PTR`/`TYPE_FILE_PTR` are
  never freed for you, are both real ways to crash or leak. Check
  `bIsOwnedTypeTable()`/`bIsOwnedTypeMap()` if you're ever unsure whether a
  given type tag means "the container will free this" or "you will."

- **`table.cpp` currently requires a C++ compiler.** A handful of return
  statements use bare brace-initialization (`return { NULL, TYPE_EMPTY };`),
  which is valid C++ but not valid strict C99/C11. Build as C++, or convert
  those sites to compound literals (`return (TableSlot_DA){ NULL,
  TYPE_EMPTY };`) first if a pure-C build is required.

---

## Build notes

```
# MSVC
cl HelloTable.cpp

# GCC / Clang

g++ HelloTable.cpp -ltable -o HelloTable.exe

Two optional compile-time macros affect memory footprint and are worth
knowing about if you need to tune them for your target:

```c
#define HASH_POOL_CAPACITY 1024     // TableMap's fixed entry pool size
#define HASH_TABLE_SIZE 67          // TableMap's bucket count
#define STACK_TABLE_CAPACITY 100    // StackTableList's fixed node pool size
#define TABLE_STACK_DEFAULT_CAPACITY 32  // default for vGetTableStackDefault
```

Define these before including `table.h` to override the defaults.

