#include "table.h"
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <limits.h>

// Global instance 
MemoryArena g_MasterArena = { NULL, 0 };

#define ARENA_MIN_SPLIT (2 * sizeof(BoundaryTag) + 8)

extern void* pAllocArena(size_t size)
{
    if (size == 0) 
        return NULL;

    if (g_MasterArena.pool == NULL)
        return malloc(size); // arena not mapped for this build - behave like plain malloc

    size_t payload = ARENA_ALIGN(size);
    size_t chunk_needed = payload + 2 * sizeof(BoundaryTag);

    uint8_t* cursor = g_MasterArena.pool;
    uint8_t* end = g_MasterArena.pool + g_MasterArena.capacity;

    while (cursor + sizeof(BoundaryTag) <= end)
    {
        BoundaryTag* head = (BoundaryTag*)cursor;

        if (head->size == 0) break; // corrupted/uninitialized tag - stop rather than loop forever

        if (head->is_free && (size_t)head->size >= chunk_needed)
        {
            size_t remaining = (size_t)head->size - chunk_needed;

            if (remaining >= ARENA_MIN_SPLIT)
            {
                // Shrink this chunk to exactly what was asked for...
                head->size = (uint32_t)chunk_needed;
                head->is_free = 0;
                BoundaryTag* foot = (BoundaryTag*)(cursor + chunk_needed - sizeof(BoundaryTag));
                foot->size = (uint32_t)chunk_needed;
                foot->is_free = 0;

                // ...and carve the leftover into a fresh free chunk right after it.
                BoundaryTag* new_head = (BoundaryTag*)(cursor + chunk_needed);
                new_head->size = (uint32_t)remaining;
                new_head->is_free = 1;
                BoundaryTag* new_foot = (BoundaryTag*)(cursor + chunk_needed + remaining - sizeof(BoundaryTag));
                new_foot->size = (uint32_t)remaining;
                new_foot->is_free = 1;
            }
            else
            {
                // Leftover too small to be useful on its own - hand over the whole block.
                head->is_free = 0;
                BoundaryTag* foot = (BoundaryTag*)(cursor + head->size - sizeof(BoundaryTag));
                foot->is_free = 0;
            }

            return (void*)(cursor + sizeof(BoundaryTag));
        }

        cursor += head->size;
    }

    // Arena exhausted/fragmented - fall back to the OS heap rather than failing the push.
    printf("[pAllocArena] Warning: no arena block >= %zu bytes free, falling back to malloc().\n", chunk_needed);
    return malloc(size);
}

// Free a pointer previously returned by pAllocArena(). Pointers outside the arena's mapped
// range (the malloc() fallback path above, or an arena that was never initialized) are routed
// to plain free() instead - the bounds check is what lets callers use one function regardless
// of which path the allocation actually took.
extern void vFreeArena(void* ptr)
{
    if (ptr == NULL) return;

    uint8_t* payload = (uint8_t*)ptr;
    uint8_t* arena_start = g_MasterArena.pool;
    uint8_t* arena_end = g_MasterArena.pool + g_MasterArena.capacity;

    if (arena_start == NULL || payload < arena_start + sizeof(BoundaryTag) || payload >= arena_end)
    {
        free(ptr);
        return;
    }

    BoundaryTag* head = (BoundaryTag*)(payload - sizeof(BoundaryTag));
    uint8_t* chunk_start = (uint8_t*)head;

    if (head->is_free)
    {
        printf("[vFreeArena] Warning: double-free detected on arena chunk at %p - ignored.\n", ptr);
        return;
    }

    uint32_t size = head->size;
    head->is_free = 1;
    BoundaryTag* foot = (BoundaryTag*)(chunk_start + size - sizeof(BoundaryTag));
    foot->is_free = 1;

    // Coalesce right: if the chunk immediately after this one is free, merge into it.
    uint8_t* right_start = chunk_start + size;
    if (right_start + sizeof(BoundaryTag) <= arena_end)
    {
        BoundaryTag* right_head = (BoundaryTag*)right_start;
        if (right_head->is_free)
        {
            uint32_t merged = size + right_head->size;
            head->size = merged;
            BoundaryTag* merged_foot = (BoundaryTag*)(chunk_start + merged - sizeof(BoundaryTag));
            merged_foot->size = merged;
            merged_foot->is_free = 1;
            size = merged;
        }
    }

    // Coalesce left: if the chunk immediately before this one is free, merge into it.
    // The footer of that left neighbor sits right before our header - O(1) to check.
    if (chunk_start - sizeof(BoundaryTag) >= arena_start)
    {
        BoundaryTag* left_foot = (BoundaryTag*)(chunk_start - sizeof(BoundaryTag));
        if (left_foot->is_free)
        {
            uint32_t left_size = left_foot->size;
            uint8_t* left_start = chunk_start - left_size;
            BoundaryTag* left_head = (BoundaryTag*)left_start;

            uint32_t merged = left_size + size;
            left_head->size = merged;
            left_head->is_free = 1;
            BoundaryTag* merged_foot = (BoundaryTag*)(left_start + merged - sizeof(BoundaryTag));
            merged_foot->size = merged;
            merged_foot->is_free = 1;
        }
    }
}

// True if this type tag's ptr is Table-owned heap memory that must be freed on drop/remove.
static bool bTypeOwnsHeapPtr(short type)
{
    switch (type)
    {
    case TYPE_EMPTY:
    case TYPE_FUNC_PTR:   // raw pointer, not ours to free
    case TYPE_FILE_PTR:   // raw pointer, not ours to free
    case TYPE_STRUCT_PTR: // "User Managed" - never owned
    case TYPE_UNION_PTR:  // "User Managed" - never owned
        return false;
    default:
        return true;
    }
}

// For types with a fixed, compile-time-known payload size, writes it to *out_size and returns
// true. Returns false for TYPE_STRUCT/TYPE_UNION (caller-defined size, not recorded anywhere)
// and for non-owned types (nothing to size in the first place).
static bool bTypeKnownSize(short type, size_t* out_size)
{
    switch (type)
    {
    case TYPE_INT:    *out_size = sizeof(int);    return true;
    case TYPE_FLOAT:  *out_size = sizeof(float);  return true;
    case TYPE_DOUBLE: *out_size = sizeof(double); return true;
    case TYPE_LONG:   *out_size = sizeof(long);   return true;
    case TYPE_SHORT:  *out_size = sizeof(short);  return true;
    case TYPE_CHAR:   *out_size = sizeof(char);   return true;

        // Single-pointer wrapper cells - all the same size regardless of what they point to.
    case TYPE_NULL:
    case TYPE_VOID_STAR:
    case TYPE_INT_STAR:
    case TYPE_FLOAT_STAR:
    case TYPE_CHAR_STAR:
    case TYPE_DOUBLE_STAR:
    case TYPE_LONG_STAR:
    case TYPE_SHORT_STAR:
        *out_size = sizeof(void*);
        return true;

        // Double-pointer wrapper cells.
    case TYPE_NULL_DOUBLE:
    case TYPE_VOID_STAR_DOUBLE:
    case TYPE_INT_STAR_DOUBLE:
    case TYPE_FLOAT_STAR_DOUBLE:
    case TYPE_CHAR_STAR_DOUBLE:
    case TYPE_DOUBLE_STAR_DOUBLE:
    case TYPE_LONG_STAR_DOUBLE:
    case TYPE_SHORT_STAR_DOUBLE:
        *out_size = sizeof(void**);
        return true;

        // Triple-pointer wrapper cells.
    case TYPE_NULL_TRIPLE:
    case TYPE_VOID_STAR_TRIPLE:
    case TYPE_INT_STAR_TRIPLE:
    case TYPE_FLOAT_STAR_TRIPLE:
    case TYPE_CHAR_STAR_TRIPLE:
    case TYPE_DOUBLE_STAR_TRIPLE:
    case TYPE_LONG_STAR_TRIPLE:
    case TYPE_SHORT_STAR_TRIPLE:
        *out_size = sizeof(void***);
        return true;

    default:
        // TYPE_STRUCT / TYPE_UNION (unknown, caller-defined size) and every non-owned type.
        return false;
    }
}

/* ============================== LIFECYCLE ============================== */

// Initialize a Table with room for `total` slots.
extern void vGetTable(Table* obj, int total_slots, size_t arena_bytes)
{
    printf("\n=== Initializing Table ===\n");

    if (obj == NULL || total_slots <= 0) return;

    // 1. Table Array Initialization
    obj->capacity = total_slots;
    obj->count = 0;

    size_t slot_bytes = sizeof(TableSlot_DA) * (size_t)obj->capacity;
    obj->slots = (TableSlot_DA*)malloc(slot_bytes);

    if (!obj->slots) return;

    for (int i = 0; i < obj->capacity; i++)
    {
        obj->slots[i].ptr = NULL;
        obj->slots[i].type = TYPE_EMPTY;
    }

    // 2. Global Arena Initialization (Singleton Check)
    // Only fires if the engine hasn't mapped the contiguous block yet.
    if (g_MasterArena.pool == NULL && arena_bytes > 0)
    {
        g_MasterArena.capacity = ARENA_ALIGN(arena_bytes);
        g_MasterArena.pool = (uint8_t*)malloc(g_MasterArena.capacity);

        if (g_MasterArena.pool)
        {
            // Set Master Header
            BoundaryTag* head = (BoundaryTag*)g_MasterArena.pool;
            head->size = (uint32_t)g_MasterArena.capacity;
            head->is_free = 1;

            // Set Master Footer
            BoundaryTag* foot = (BoundaryTag*)(g_MasterArena.pool + g_MasterArena.capacity - sizeof(BoundaryTag));
            foot->size = (uint32_t)g_MasterArena.capacity;
            foot->is_free = 1;

            printf("[vGetTable] Global Master Arena mapped: %zu bytes.\n", g_MasterArena.capacity);
        }
        else
        {
            printf("[vGetTable] Warning: Failed to allocate Global Master Arena.\n");
        }
    }

    printf("[vGetTable] Table initialized successfully.\n\n");
}

// Print every active slot in a human-readable form. Read-only, never mutates.
extern void vPrintTable(const Table* obj)
{
    if (!obj || !obj->slots)
    {
        printf("[vPrintTable] Error: Table is uninitialized or NULL.\n");
        return;
    }

    printf("\n========================================================================\n");
    printf("  TABLE CONTENTS  |  Count: %-3d  |  Capacity: %-3d\n", obj->count, obj->capacity);
    printf("========================================================================\n");

    if (obj->count == 0)
    {
        printf("  [Empty Table]\n");
        printf("------------------------------------------------------------------------\n\n");
        return;
    }

    for (int i = 0; i < obj->count; i++)
    {
        short type = obj->slots[i].type;
        void* payload = obj->slots[i].ptr;

        printf(" [%02d] ", i);

        if (!payload)
        {
            printf("[NULL PAYLOAD]\n");
            continue;
        }

        switch (type)
        {
            // -- primitives: payload points straight at the deep-copied value --
        case TYPE_INT:
            printf("[TYPE_INT]              \t Value : %d\n", *(int*)payload);
            break;

        case TYPE_FLOAT:
            printf("[TYPE_FLOAT]            \t Value : %.4f\n", *(float*)payload);
            break;

        case TYPE_DOUBLE:
            printf("[TYPE_DOUBLE]           \t Value : %.6lf\n", *(double*)payload);
            break;

        case TYPE_CHAR:
            printf("[TYPE_CHAR]             \t Value : '%c'\n", *(char*)payload);
            break;

        case TYPE_LONG:
            printf("[TYPE_LONG]             \t Value : %ld\n", *(long*)payload);
            break;

        case TYPE_SHORT:
            printf("[TYPE_SHORT]            \t Value : %d\n", *(short*)payload);
            break;

            // -- single pointers: payload is the wrapper cell, deref once more for the real pointer --
        case TYPE_INT_STAR: {
            int* p = *(int**)payload;
            if (p)
                printf("[TYPE_INT_STAR]        \t Addr  : %p -> Val: %d\n", (void*)p, *p);
            else
                printf("[TYPE_INT_STAR]        \t Addr  : NULL\n");
            break;
        }

        case TYPE_FLOAT_STAR: {
            float* p = *(float**)payload;
            if (p)
                printf("[TYPE_FLOAT_STAR]      \t Addr  : %p -> Val: %.4f\n", (void*)p, *p);
            else
                printf("[TYPE_FLOAT_STAR]      \t Addr  : NULL\n");
            break;
        }

        case TYPE_CHAR_STAR: {
            char* p = *(char**)payload;
            if (p)
                printf("[TYPE_CHAR_STAR]       \t Addr  : %p -> Val: '%c'\n", (void*)p, *p);
            else
                printf("[TYPE_CHAR_STAR]       \t Addr  : NULL\n");
            break;
        }

        case TYPE_DOUBLE_STAR: {
            double* p = *(double**)payload;
            if (p)
                printf("[TYPE_DOUBLE_STAR]     \t Addr  : %p -> Val: %.6lf\n", (void*)p, *p);
            else
                printf("[TYPE_DOUBLE_STAR]     \t Addr  : NULL\n");
            break;
        }

        case TYPE_LONG_STAR: {
            long* p = *(long**)payload;
            if (p)
                printf("[TYPE_LONG_STAR]       \t Addr  : %p -> Val: %ld\n", (void*)p, *p);
            else
                printf("[TYPE_LONG_STAR]       \t Addr  : NULL\n");
            break;
        }

        case TYPE_SHORT_STAR: {
            short* p = *(short**)payload;
            if (p)
                printf("[TYPE_SHORT_STAR]      \t Addr  : %p -> Val: %d\n", (void*)p, *p);
            else
                printf("[TYPE_SHORT_STAR]      \t Addr  : NULL\n");
            break;
        }

        case TYPE_VOID_STAR: {
            void* p = *(void**)payload;
            printf("[TYPE_VOID_STAR]       \t Addr  : %p\n", p);
            break;
        }

                           // -- double pointers --
        case TYPE_INT_STAR_DOUBLE: {
            int** dp = *(int***)payload;
            if (dp && *dp)
                printf("[TYPE_INT_STAR_DOUBLE] \t Addr  : %p -> Val: %d\n", (void*)dp, **dp);
            else
                printf("[TYPE_INT_STAR_DOUBLE] \t Addr  : %p (Unresolvable)\n", (void*)dp);
            break;
        }

        case TYPE_FLOAT_STAR_DOUBLE: {
            float** dp = *(float***)payload;
            if (dp && *dp)
                printf("[TYPE_FLOAT_STAR_DOUBLE] \t Addr : %p -> Val: %.4f\n", (void*)dp, **dp);
            else
                printf("[TYPE_FLOAT_STAR_DOUBLE] \t Addr : %p (Unresolvable)\n", (void*)dp);
            break;
        }

        case TYPE_CHAR_STAR_DOUBLE: {
            char** dp = *(char***)payload;
            if (dp && *dp)
                printf("[TYPE_CHAR_STAR_DOUBLE] \t Addr  : %p -> Val: '%c'\n", (void*)dp, **dp);
            else
                printf("[TYPE_CHAR_STAR_DOUBLE] \t Addr  : %p (Unresolvable)\n", (void*)dp);
            break;
        }

                                  // -- triple pointers --
        case TYPE_INT_STAR_TRIPLE: {
            int*** tp = *(int****)payload;
            if (tp && *tp && **tp)
                printf("[TYPE_INT_STAR_TRIPLE] \t Addr  : %p -> Val: %d\n", (void*)tp, ***tp);
            else
                printf("[TYPE_INT_STAR_TRIPLE] \t Addr  : %p (Unresolvable)\n", (void*)tp);
            break;
        }

        case TYPE_VOID_STAR_DOUBLE: {
            void** dp = *(void***)payload;
            printf("[TYPE_VOID_STAR_DOUBLE]\t Addr  : %p\n", (void*)dp);
            break;
        }

        case TYPE_DOUBLE_STAR_DOUBLE: {
            double** dp = *(double***)payload;
            if (dp && *dp)
                printf("[TYPE_DOUBLE_STAR_DOUBLE]\t Addr : %p -> Val: %.6lf\n", (void*)dp, **dp);
            else
                printf("[TYPE_DOUBLE_STAR_DOUBLE]\t Addr : %p (Unresolvable)\n", (void*)dp);
            break;
        }

        case TYPE_LONG_STAR_DOUBLE: {
            long** dp = *(long***)payload;
            if (dp && *dp)
                printf("[TYPE_LONG_STAR_DOUBLE]\t Addr  : %p -> Val: %ld\n", (void*)dp, **dp);
            else
                printf("[TYPE_LONG_STAR_DOUBLE]\t Addr  : %p (Unresolvable)\n", (void*)dp);
            break;
        }

        case TYPE_SHORT_STAR_DOUBLE: {
            short** dp = *(short***)payload;
            if (dp && *dp)
                printf("[TYPE_SHORT_STAR_DOUBLE]\t Addr  : %p -> Val: %d\n", (void*)dp, (int)**dp);
            else
                printf("[TYPE_SHORT_STAR_DOUBLE]\t Addr  : %p (Unresolvable)\n", (void*)dp);
            break;
        }

        case TYPE_VOID_STAR_TRIPLE: {
            void*** tp = *(void****)payload;
            printf("[TYPE_VOID_STAR_TRIPLE]\t Addr  : %p\n", (void*)tp);
            break;
        }

        case TYPE_FLOAT_STAR_TRIPLE: {
            float*** tp = *(float****)payload;
            if (tp && *tp && **tp)
                printf("[TYPE_FLOAT_STAR_TRIPLE]\t Addr  : %p -> Val: %.4f\n", (void*)tp, ***tp);
            else
                printf("[TYPE_FLOAT_STAR_TRIPLE]\t Addr  : %p (Unresolvable)\n", (void*)tp);
            break;
        }

        case TYPE_CHAR_STAR_TRIPLE: {
            char*** tp = *(char****)payload;
            if (tp && *tp && **tp)
                printf("[TYPE_CHAR_STAR_TRIPLE]\t Addr  : %p -> Val: '%c'\n", (void*)tp, ***tp);
            else
                printf("[TYPE_CHAR_STAR_TRIPLE]\t Addr  : %p (Unresolvable)\n", (void*)tp);
            break;
        }

        case TYPE_DOUBLE_STAR_TRIPLE: {
            double*** tp = *(double****)payload;
            if (tp && *tp && **tp)
                printf("[TYPE_DOUBLE_STAR_TRIPLE]\t Addr  : %p -> Val: %.6lf\n", (void*)tp, ***tp);
            else
                printf("[TYPE_DOUBLE_STAR_TRIPLE]\t Addr  : %p (Unresolvable)\n", (void*)tp);
            break;
        }

        case TYPE_LONG_STAR_TRIPLE: {
            long*** tp = *(long****)payload;
            if (tp && *tp && **tp)
                printf("[TYPE_LONG_STAR_TRIPLE]\t Addr  : %p -> Val: %ld\n", (void*)tp, ***tp);
            else
                printf("[TYPE_LONG_STAR_TRIPLE]\t Addr  : %p (Unresolvable)\n", (void*)tp);
            break;
        }

        case TYPE_SHORT_STAR_TRIPLE: {
            short*** tp = *(short****)payload;
            if (tp && *tp && **tp)
                printf("[TYPE_SHORT_STAR_TRIPLE]\t Addr  : %p -> Val: %d\n", (void*)tp, (int)***tp);
            else
                printf("[TYPE_SHORT_STAR_TRIPLE]\t Addr  : %p (Unresolvable)\n", (void*)tp);
            break;
        }

                                 // -- user-defined / complex: no generic way to format contents, print address --
        case TYPE_STRUCT:
            printf("[TYPE_STRUCT]          \t Addr  : %p (User Managed)\n", payload);
            break;

        case TYPE_UNION:
            printf("[TYPE_UNION]           \t Addr  : %p (User Managed)\n", payload);
            break;

        case TYPE_STRUCT_PTR:
            printf("[TYPE_STRUCT_PTR]      \t Addr  : %p\n", payload);
            break;

        case TYPE_UNION_PTR:
            printf("[TYPE_UNION_PTR]       \t Addr  : %p\n", payload);
            break;

        case TYPE_FUNC_PTR:
            printf("[TYPE_FUNC_PTR]        \t Addr  : %p (Code Segment)\n", payload);
            break;

        case TYPE_FILE_PTR:
            printf("[TYPE_FILE_PTR]        \t Addr  : %p (File Stream)\n", payload);
            break;

        default:
            printf("[TYPE_UNKNOWN (%d)]  \t  Addr  : %p\n", type, payload);
            break;
        }
    }

    printf("------------------------------------------------------------------------\n\n");
}

/* ============================== PUSH: core ============================== */
extern bool bPushSlot(Table* obj, TableSlot_DA cobj)
{
    if (obj == NULL)
    {
        printf("[bPushSlot] Error: Invalid collection pointer.\n");
        return false;
    }

    if (obj->count == obj->capacity)
    {
        int new_capacity = (obj->capacity == 0) ? 8 : (obj->capacity * 3) / 2;
        size_t new_size_in_bytes = (size_t)new_capacity * sizeof(TableSlot_DA);

        printf("\n[bPushSlot] Capacity reached (%d). Growing buffer to %d slots (%zu bytes)...\n",
            obj->capacity, new_capacity, new_size_in_bytes);

        TableSlot_DA* new_slots = (TableSlot_DA*)realloc(obj->slots, new_size_in_bytes);
        if (new_slots == NULL)
        {
            printf("[bPushSlot] Error: realloc failed! Cannot grow array buffer.\n");
            return false;
        }

        for (int i = obj->capacity; i < new_capacity; i++)
        {
            new_slots[i].ptr = NULL;
            new_slots[i].type = TYPE_EMPTY;
        }

        obj->slots = new_slots;
        obj->capacity = new_capacity;
        printf("[bPushSlot] Buffer successfully grown to %d slots.\n", obj->capacity);
    }

    obj->slots[obj->count] = cobj;
    printf("[bPushSlot] TableSlot_DA pushed successfully at index %d (Type Tag: %d).\n", obj->count, cobj.type);
    obj->count++;

    return true;
}

/* ============================== PUSH: primitives ============================== */
// Each mallocs a single value on the heap, copies it in, and hands ownership to bPushSlot().

extern bool bPushInt(Table* obj, int val)
{
    int* ptr = (int*)pAllocArena(sizeof(int));
    if (!ptr)
        return false;

    *ptr = val;
    TableSlot_DA node = { ptr, TYPE_INT };
    return bPushSlot(obj, node);
}

extern bool bPushFloat(Table* obj, float val)
{
    float* ptr = (float*)pAllocArena(sizeof(float));
    if (!ptr)
        return false;

    *ptr = val;
    TableSlot_DA node = { ptr, TYPE_FLOAT };
    return bPushSlot(obj, node);
}

extern bool bPushDouble(Table* obj, double val)
{
    double* ptr = (double*)pAllocArena(sizeof(double));
    if (!ptr)
        return false;

    *ptr = val;
    TableSlot_DA node = { ptr, TYPE_DOUBLE };
    return bPushSlot(obj, node);
}

extern bool bPushLong(Table* obj, long val)
{
    long* ptr = (long*)pAllocArena(sizeof(long));
    if (!ptr)
        return false;

    *ptr = val;
    TableSlot_DA node = { ptr, TYPE_LONG };
    return bPushSlot(obj, node);
}

extern bool bPushShort(Table* obj, short val)
{
    short* ptr = (short*)pAllocArena(sizeof(short));
    if (!ptr)
        return false;

    *ptr = val;
    TableSlot_DA node = { ptr, TYPE_SHORT };
    return bPushSlot(obj, node);
}

extern bool bPushChar(Table* obj, char val)
{
    char* ptr = (char*)pAllocArena(sizeof(char));
    if (!ptr)
        return false;

    *ptr = val;
    TableSlot_DA node = { ptr, TYPE_CHAR };
    return bPushSlot(obj, node);
}

/* ============================== PUSH: pointers ============================== */
// Wrapper cell holds the caller's pointer; Table owns the wrapper, not what it points to.

extern bool bPushPtr(Table* obj, void* val, short pointer_type)
{
    void** ptr = (void**)pAllocArena(sizeof(void*));
    if (!ptr)
        return false;

    *ptr = val;

    TableSlot_DA node = { ptr, pointer_type };
    return bPushSlot(obj, node);
}

extern bool bPushDPtr(Table* obj, void** val, short double_pointer_type)
{
    void*** ptr = (void***)pAllocArena(sizeof(void**));
    if (!ptr)
        return false;

    *ptr = val;

    TableSlot_DA node = { ptr, double_pointer_type };
    return bPushSlot(obj, node);
}

extern bool bPushTPtr(Table* obj, void*** val, short triple_pointer_type)
{
    void**** ptr = (void****)pAllocArena(sizeof(void***));
    if (!ptr)
        return false;

    *ptr = val;

    TableSlot_DA node = { ptr, triple_pointer_type };
    return bPushSlot(obj, node);
}

/* ============================== PUSH: user-defined / complex ============================== */

// Deep-copies `size` bytes onto the heap and pushes as TYPE_STRUCT; caller's original is untouched.
// Note: `size` is only used here to malloc/memcpy - it is NOT stored in the slot afterward.
extern bool bPushStruct(Table* obj, void* struct_data, size_t size)
{
    if (!obj || struct_data == NULL || size == 0)
        return false;

    void* ptr = pAllocArena(size);
    if (!ptr)
    {
        printf("[bPushStruct] Error: could not allocate %zu bytes on heap.\n", size);
        return false;
    }

    memcpy(ptr, struct_data, size);

    TableSlot_DA node = { ptr, TYPE_STRUCT };
    return bPushSlot(obj, node);
}

// Same deep-copy contract as bPushStruct, tagged TYPE_UNION.
extern bool bPushUnion(Table* obj, void* union_ptr, size_t union_size)
{
    if (obj == NULL || union_ptr == NULL || union_size == 0)
        return false;

    void* heap_payload = pAllocArena(union_size);
    if (heap_payload == NULL)
    {
        printf("[bPushUnion] Error: heap allocation failed for union (%zu bytes).\n", union_size);
        return false;
    }
    memcpy(heap_payload, union_ptr, union_size);

    TableSlot_DA node = { heap_payload, TYPE_UNION };
    return bPushSlot(obj, node);
}

// Stores the function pointer AS-IS (zero allocation) - never freed by the Table.
extern bool bPushFunc(Table* obj, void* func_ptr)
{
    if (obj == NULL || func_ptr == NULL)
        return false;

    TableSlot_DA node = { func_ptr, TYPE_FUNC_PTR };
    return bPushSlot(obj, node);
}

// Stores the FILE* handle AS-IS (zero allocation) - Table never closes/frees it.
extern bool bPushFile(Table* obj, FILE* file_ptr)
{
    if (obj == NULL || file_ptr == NULL)
        return false;

    TableSlot_DA node = { (void*)file_ptr, TYPE_FILE_PTR };
    return bPushSlot(obj, node);
}

// Variadic: push `count` (type, value) pairs in one call.
extern bool bPushList(Table* obj, int count, ...)
{
    if (!obj || count <= 0)
        return false;

    va_list args;
    va_start(args, count);

    for (int i = 0; i < count; i++)
    {
        short type = (short)va_arg(args, int);

        switch (type)
        {
        case TYPE_INT:
            bPushInt(obj, va_arg(args, int));
            break;

        case TYPE_FLOAT:
            // float args promote to double when passed through '...'
            bPushFloat(obj, (float)va_arg(args, double));
            break;

        case TYPE_DOUBLE:
            bPushDouble(obj, va_arg(args, double));
            break;

        case TYPE_CHAR:
            bPushChar(obj, (char)va_arg(args, int));
            break;

        case TYPE_LONG:
            bPushLong(obj, va_arg(args, long));
            break;

        case TYPE_SHORT:
            bPushShort(obj, (short)va_arg(args, int));
            break;

        case TYPE_INT_STAR:
        case TYPE_FLOAT_STAR:
        case TYPE_CHAR_STAR:
        case TYPE_DOUBLE_STAR:
        case TYPE_LONG_STAR:
        case TYPE_SHORT_STAR:
        case TYPE_VOID_STAR:
            bPushPtr(obj, va_arg(args, void*), type);
            break;

        case TYPE_INT_STAR_DOUBLE:
        case TYPE_FLOAT_STAR_DOUBLE:
        case TYPE_CHAR_STAR_DOUBLE:
        case TYPE_DOUBLE_STAR_DOUBLE:
        case TYPE_LONG_STAR_DOUBLE:
        case TYPE_SHORT_STAR_DOUBLE:
        case TYPE_VOID_STAR_DOUBLE:
            bPushDPtr(obj, va_arg(args, void**), type);
            break;

        case TYPE_INT_STAR_TRIPLE:
        case TYPE_FLOAT_STAR_TRIPLE:
        case TYPE_CHAR_STAR_TRIPLE:
        case TYPE_DOUBLE_STAR_TRIPLE:
        case TYPE_LONG_STAR_TRIPLE:
        case TYPE_SHORT_STAR_TRIPLE:
        case TYPE_VOID_STAR_TRIPLE:
            bPushTPtr(obj, va_arg(args, void***), type);
            break;

        case TYPE_STRUCT:
        {
            void* struct_ptr = va_arg(args, void*);
            size_t struct_size = va_arg(args, size_t);
            bPushStruct(obj, struct_ptr, struct_size);
            break;
        }

        case TYPE_UNION:
        {
            void* union_ptr = va_arg(args, void*);
            size_t union_size = va_arg(args, size_t);
            bPushUnion(obj, union_ptr, union_size);
            break;
        }

        default:
            // Unknown tag - skip (does not consume extra va_arg; malformed calls will desync).
            break;
        }
    }

    va_end(args);
    return true;
}

/* ============================== POP ============================== */

// Remove and return the top slot. Primitives: fresh owned copy returned, original freed here.
// Everything else: original raw pointer handed back, ownership transferred to caller as-is.
extern TableSlot_DA sPopSlot(Table* obj)
{
    printf("\n=== Popping TableSlot_DA ===\n");
    if (obj == NULL || obj->count == 0)
    {
        printf("[sPopSlot] Underflow Error: Cannot pop from an empty or NULL collection!\n");
        return { NULL, TYPE_EMPTY };
    }

    obj->count--;
    int top_index = obj->count;
    TableSlot_DA original = obj->slots[top_index];

    printf("[sPopSlot] Extracting top element at index %d (Type Tag: %d)...\n", top_index, original.type);

    void* new_data = NULL;
    bool is_primitive = false;

    switch (original.type)
    {
        // Primitives: deep-copy out, then free the original. Guarded against malloc failure.
    case TYPE_INT:
        new_data = pAllocArena(sizeof(int));
        if (new_data)
        {
            if (original.ptr) *(int*)new_data = *(int*)original.ptr;
            printf("[sPopSlot] Deep-copied TYPE_INT value: %d\n", *(int*)new_data);
        }
        else
        {
            printf("[sPopSlot] Error: malloc failed while popping TYPE_INT.\n");
        }
        is_primitive = true;
        break;

    case TYPE_FLOAT:
        new_data = pAllocArena(sizeof(float));
        if (new_data)
        {
            if (original.ptr) *(float*)new_data = *(float*)original.ptr;
            printf("[sPopSlot] Deep-copied TYPE_FLOAT value: %.2f\n", *(float*)new_data);
        }
        else
        {
            printf("[sPopSlot] Error: malloc failed while popping TYPE_FLOAT.\n");
        }
        is_primitive = true;
        break;

    case TYPE_DOUBLE:
        new_data = pAllocArena(sizeof(double));
        if (new_data)
        {
            if (original.ptr) *(double*)new_data = *(double*)original.ptr;
            printf("[sPopSlot] Deep-copied TYPE_DOUBLE value: %.5f\n", *(double*)new_data);
        }
        else
        {
            printf("[sPopSlot] Error: malloc failed while popping TYPE_DOUBLE.\n");
        }
        is_primitive = true;
        break;

    case TYPE_LONG:
        new_data = pAllocArena(sizeof(long));
        if (new_data)
        {
            if (original.ptr) *(long*)new_data = *(long*)original.ptr;
            printf("[sPopSlot] Deep-copied TYPE_LONG value: %ld\n", *(long*)new_data);
        }
        else
        {
            printf("[sPopSlot] Error: malloc failed while popping TYPE_LONG.\n");
        }
        is_primitive = true;
        break;

    case TYPE_CHAR:
        new_data = pAllocArena(sizeof(char));
        if (new_data)
        {
            if (original.ptr) *(char*)new_data = *(char*)original.ptr;
            printf("[sPopSlot] Deep-copied TYPE_CHAR value: '%c'\n", *(char*)new_data);
        }
        else
        {
            printf("[sPopSlot] Error: malloc failed while popping TYPE_CHAR.\n");
        }
        is_primitive = true;
        break;

    case TYPE_SHORT:
        new_data = pAllocArena(sizeof(short));
        if (new_data)
        {
            if (original.ptr) *(short*)new_data = *(short*)original.ptr;
            printf("[sPopSlot] Deep-copied TYPE_SHORT value: %d\n", *(short*)new_data);
        }
        else
        {
            printf("[sPopSlot] Error: malloc failed while popping TYPE_SHORT.\n");
        }
        is_primitive = true;
        break;

        // Single pointers: hand back the wrapper's raw pointer as-is.
    case TYPE_NULL:
    case TYPE_VOID_STAR:
    case TYPE_INT_STAR:
    case TYPE_FLOAT_STAR:
    case TYPE_CHAR_STAR:
    case TYPE_DOUBLE_STAR:
    case TYPE_LONG_STAR:
    case TYPE_SHORT_STAR:
        new_data = original.ptr;
        printf("[sPopSlot] Single pointer extracted (Tag: %d). Passing raw pointer.\n", original.type);
        break;

        // Double pointers: hand back the wrapper's raw pointer as-is.
    case TYPE_NULL_DOUBLE:
    case TYPE_VOID_STAR_DOUBLE:
    case TYPE_INT_STAR_DOUBLE:
    case TYPE_FLOAT_STAR_DOUBLE:
    case TYPE_CHAR_STAR_DOUBLE:
    case TYPE_DOUBLE_STAR_DOUBLE:
    case TYPE_LONG_STAR_DOUBLE:
    case TYPE_SHORT_STAR_DOUBLE:
        new_data = original.ptr;
        printf("[sPopSlot] Double pointer extracted (Tag: %d). Passing raw pointer.\n", original.type);
        break;

        // Triple pointers: hand back the wrapper's raw pointer as-is.
    case TYPE_NULL_TRIPLE:
    case TYPE_VOID_STAR_TRIPLE:
    case TYPE_INT_STAR_TRIPLE:
    case TYPE_FLOAT_STAR_TRIPLE:
    case TYPE_CHAR_STAR_TRIPLE:
    case TYPE_DOUBLE_STAR_TRIPLE:
    case TYPE_LONG_STAR_TRIPLE:
    case TYPE_SHORT_STAR_TRIPLE:
        new_data = original.ptr;
        printf("[sPopSlot] Triple pointer extracted (Tag: %d). Passing raw pointer.\n", original.type);
        break;

        // User-defined / complex: hand back the raw pointer as-is.
    case TYPE_NULL_USER_DEFINED:
    case TYPE_STRUCT:
    case TYPE_UNION:
    case TYPE_STRUCT_PTR:
    case TYPE_UNION_PTR:
    case TYPE_FUNC_PTR:
    case TYPE_FILE_PTR:
        new_data = original.ptr;
        printf("[sPopSlot] Complex/User-Defined type extracted (Tag: %d). Passing raw pointer.\n", original.type);
        break;

    case TYPE_EMPTY:
        printf("[sPopSlot] Warning: Slot is marked as TYPE_EMPTY.\n");
        break;

    default:
        printf("[sPopSlot] Warning: TableSlot_DA present with unknown identifier or EMPTY state.\n");
        break;
    }

    // Only primitives were deep-copied above, so only they leave an original block to free.
    if (is_primitive && original.ptr != NULL)
    {
        vFreeArena(original.ptr);
        printf("[sPopSlot] Internal heap payload at slot index %d freed.\n", top_index);
    }

    obj->slots[top_index].ptr = NULL;
    obj->slots[top_index].type = TYPE_EMPTY;

    printf("[sPopSlot] Slot index %d sanitized. Remaining active count: %d\n", top_index, obj->count);

    return { new_data, original.type };
}

/* ============================== TEARDOWN ============================== */

// Free every owned payload plus the backing buffer, reset to an empty reusable state.
// Only frees slots whose type is actually Table-owned (see bTypeOwnsHeapPtr) - avoids freeing
// FUNC_PTR/FILE_PTR/STRUCT_PTR/UNION_PTR (raw, non-owned pointers), which would be undefined behavior.
extern void vDropTable(Table* obj)
{
    if (obj == NULL || obj->slots == NULL)
    {
        printf("[vDropTable] Warning: NULL pointer provided or array already empty.\n");
        return;
    }

    printf("\n--- Dropping Table ---\n");
    printf("[vDropTable] Deallocating %d active element payload(s)...\n", obj->count);

    for (int i = 0; i < obj->count; i++)
    {
        if (obj->slots[i].ptr != NULL && bTypeOwnsHeapPtr(obj->slots[i].type))
        {
            vFreeArena(obj->slots[i].ptr);
            obj->slots[i].ptr = NULL;
            printf(" -> Freed heap payload at index %d\n", i);
        }
        else if (obj->slots[i].ptr != NULL)
        {
            printf(" -> Skipped index %d (Tag: %d, not owned by Table)\n", i, obj->slots[i].type);
        }
    }

    free(obj->slots);
    obj->slots = NULL;
    obj->count = 0;
    obj->capacity = 0;

    printf("[vDropTable] Main array memory freed and metadata reset to 0.\n");
    printf("--- Table Successfully Dropped ---\n\n");
}

/* ============================== STACK-MANIPULATION PARITY (see TableStack) ============================== */

// Push a deep copy of the current top slot: [A, B] -> [A, B, B']. Works for any type with a
// known fixed size (primitives, pointer wrapper cells) or any non-owned type (shares the
// reference, since the Table never owned it anyway). Explicitly refuses TYPE_STRUCT/TYPE_UNION -
// their size isn't recorded anywhere, so there's no safe way to know how many bytes to copy.
extern bool bDupTopTable(Table* obj)
{
    if (obj == NULL || obj->count == 0)
        return false;

    TableSlot_DA top = obj->slots[obj->count - 1];

    if (!bTypeOwnsHeapPtr(top.type) || top.ptr == NULL)
    {
        TableSlot_DA copy = { top.ptr, top.type };
        return bPushSlot(obj, copy);
    }

    size_t sz;
    if (!bTypeKnownSize(top.type, &sz))
    {
        printf("[bDupTopTable] Error: cannot safely duplicate a TYPE_STRUCT/TYPE_UNION slot - "
            "its size was never recorded (TableSlot_DA only stores ptr+type). "
            "Push a fresh copy yourself with bPushStruct()/bPushUnion() instead.\n");
        return false;
    }

    void* copy_ptr = pAllocArena(sz);
    if (!copy_ptr)
    {
        printf("[bDupTopTable] Error: malloc failed while duplicating %zu bytes.\n", sz);
        return false;
    }
    memcpy(copy_ptr, top.ptr, sz);

    TableSlot_DA copy = { copy_ptr, top.type };
    return bPushSlot(obj, copy);
}

// Swap the top two slots in place (just swaps the TableSlot_DA structs - no allocation needed).
extern bool bSwapTopTable(Table* obj)
{
    if (obj == NULL || obj->count < 2)
        return false;

    TableSlot_DA tmp = obj->slots[obj->count - 1];
    obj->slots[obj->count - 1] = obj->slots[obj->count - 2];
    obj->slots[obj->count - 2] = tmp;
    return true;
}

extern int iCountOfTypeTable(const Table* obj, short type)
{
    if (obj == NULL) return 0;

    int n = 0;
    for (int i = 0; i < obj->count; i++)
        if (obj->slots[i].type == type) n++;
    return n;
}

extern bool bContainsTypeTable(const Table* obj, short type)
{
    if (obj == NULL) return false;

    for (int i = 0; i < obj->count; i++)
        if (obj->slots[i].type == type) return true;
    return false;
}

extern int iFindTypeTable(const Table* obj, short type)
{
    if (obj == NULL) return -1;

    for (int i = 0; i < obj->count; i++)
        if (obj->slots[i].type == type) return i;
    return -1;
}

extern int iSumIntTable(const Table* obj)
{
    if (obj == NULL) return 0;

    int sum = 0;
    for (int i = 0; i < obj->count; i++)
        if (obj->slots[i].type == TYPE_INT && obj->slots[i].ptr)
            sum += *(int*)obj->slots[i].ptr;
    return sum;
}

extern double dSumFloatTable(const Table* obj)
{
    if (obj == NULL) return 0.0;

    double sum = 0.0;
    for (int i = 0; i < obj->count; i++)
        if (obj->slots[i].type == TYPE_FLOAT && obj->slots[i].ptr)
            sum += (double)(*(float*)obj->slots[i].ptr);
    return sum;
}

extern bool bMinIntTable(const Table* obj, int* out)
{
    if (obj == NULL || out == NULL) return false;

    bool found = false;
    int best = 0;
    for (int i = 0; i < obj->count; i++)
    {
        if (obj->slots[i].type != TYPE_INT || !obj->slots[i].ptr) continue;
        int v = *(int*)obj->slots[i].ptr;
        if (!found || v < best) { best = v; found = true; }
    }
    if (found) *out = best;
    return found;
}

extern bool bMaxIntTable(const Table* obj, int* out)
{
    if (obj == NULL || out == NULL) return false;

    bool found = false;
    int best = 0;
    for (int i = 0; i < obj->count; i++)
    {
        if (obj->slots[i].type != TYPE_INT || !obj->slots[i].ptr) continue;
        int v = *(int*)obj->slots[i].ptr;
        if (!found || v > best) { best = v; found = true; }
    }
    if (found) *out = best;
    return found;
}

// Reverse slot order in place: [A, B, C] -> [C, B, A]. Just swaps TableSlot_DA structs, no allocation.
extern void vReverseTable(Table* obj)
{
    if (obj == NULL || obj->count < 2) return;

    int lo = 0, hi = obj->count - 1;
    while (lo < hi)
    {
        TableSlot_DA tmp = obj->slots[lo];
        obj->slots[lo] = obj->slots[hi];
        obj->slots[hi] = tmp;
        lo++; hi--;
    }
}

// Push as many ints from `arr` as requested - unlike the stack version, this GROWS as needed
// instead of stopping at whatever capacity happened to be pre-declared.
extern int iPushIntArrayTable(Table* obj, const int* arr, int n)
{
    if (obj == NULL || arr == NULL || n <= 0) return 0;

    int pushed = 0;
    for (int i = 0; i < n; i++)
        if (bPushInt(obj, arr[i])) pushed++;

    printf("[iPushIntArrayTable] Pushed %d/%d int(s) (buffer grows automatically).\n", pushed, n);
    return pushed;
}

extern int iPushFloatArrayTable(Table* obj, const float* arr, int n)
{
    if (obj == NULL || arr == NULL || n <= 0) return 0;

    int pushed = 0;
    for (int i = 0; i < n; i++)
        if (bPushFloat(obj, arr[i])) pushed++;

    printf("[iPushFloatArrayTable] Pushed %d/%d float(s) (buffer grows automatically).\n", pushed, n);
    return pushed;
}

// Deep-clones every slot from `src` into `dest`, growing `dest` automatically as needed.
// TYPE_STRUCT/TYPE_UNION slots are skipped with a warning (same size-tracking limitation as
// bDupTopTable) rather than cloned incorrectly - everything else is copied safely.
extern bool bCloneTable(Table* dest, const Table* src)
{
    if (dest == NULL || src == NULL) return false;

    printf("\n[bCloneTable] Cloning from source table (%d slot(s))...\n", src->count);

    for (int i = 0; i < src->count; i++)
    {
        TableSlot_DA s = src->slots[i];

        if (!bTypeOwnsHeapPtr(s.type) || s.ptr == NULL)
        {
            // Non-owned reference (func/file/struct-ptr/union-ptr) - copy the pointer itself,
            // it was never Table-owned to begin with.
            TableSlot_DA copy = { s.ptr, s.type };
            if (!bPushSlot(dest, copy))
            {
                printf("[bCloneTable] Error: push failed at source index %d.\n", i);
                return false;
            }
            continue;
        }

        size_t sz;
        if (!bTypeKnownSize(s.type, &sz))
        {
            printf(" -> Skipped index %d: TYPE_STRUCT/TYPE_UNION size isn't tracked, can't clone safely.\n", i);
            continue;
        }

        void* copy_ptr = pAllocArena(sz);
        if (!copy_ptr)
        {
            printf("[bCloneTable] Error: malloc failed duplicating %zu bytes at index %d.\n", sz, i);
            return false;
        }
        memcpy(copy_ptr, s.ptr, sz);

        TableSlot_DA copy = { copy_ptr, s.type };
        if (!bPushSlot(dest, copy))
        {
            vFreeArena(copy_ptr);
            printf("[bCloneTable] Error: push failed at source index %d.\n", i);
            return false;
        }
    }

    printf("[bCloneTable] Clone pass complete.\n\n");
    return true;
}

// Read (without removing) the slot at `index`. Returns a TYPE_EMPTY slot if out of range.
extern TableSlot_DA sGetSlotAtTable(const Table* obj, int index)
{
    if (obj == NULL || index < 0 || index >= obj->count)
    {
        printf("[sGetSlotAtTable] Error: index %d out of range (count=%d).\n", index, obj ? obj->count : -1);
        return { NULL, TYPE_EMPTY };
    }
    return obj->slots[index];
}

extern short sGetTypeAtTable(const Table* obj, int index)
{
    if (obj == NULL || index < 0 || index >= obj->count)
        return TYPE_EMPTY;
    return obj->slots[index].type;
}

// Type-checked slot access: validates the index AND the stored type tag before
// handing back the slot. Returns false (leaving *out untouched) on an
// out-of-range index or a type-tag mismatch; returns true and copies the slot
// into *out on success.
extern bool bTryGetSlotAtTable(const Table* obj, int index, short expected_type, TableSlot_DA* out)
{
    if (obj == NULL || out == NULL || index < 0 || index >= obj->count)
    {
        printf("[bTryGetSlotAtTable] Error: index %d out of range (count=%d).\n", index, obj ? obj->count : -1);
        return false;
    }
    if (obj->slots[index].type != expected_type)
    {
        printf("[bTryGetSlotAtTable] Type mismatch at index %d: expected %d, found %d.\n",
            index, expected_type, obj->slots[index].type);
        return false;
    }
    *out = obj->slots[index];
    return true;
}

// Public wrapper around the module's private heap-ownership rule, so callers can
// reason about push/drop semantics for a type without reading the .c source.
extern bool bIsOwnedTypeTable(short type)
{
    return bTypeOwnsHeapPtr(type);
}

// Call visitor(index, slot) for every active slot, in order.
extern void vForEachSlotTable(const Table* obj, TableVisitor visitor)
{
    if (obj == NULL || visitor == NULL) return;

    for (int i = 0; i < obj->count; i++)
        visitor(i, obj->slots[i]);
}

// Human-readable name for a type tag, for logging/debug output.
extern const char* sTypeNameTable(short type)
{
    switch (type)
    {
    case TYPE_EMPTY:      return "EMPTY";
    case TYPE_INT:        return "INT";
    case TYPE_FLOAT:      return "FLOAT";
    case TYPE_DOUBLE:     return "DOUBLE";
    case TYPE_CHAR:       return "CHAR";
    case TYPE_LONG:       return "LONG";
    case TYPE_SHORT:      return "SHORT";
    case TYPE_STRUCT:     return "STRUCT";
    case TYPE_UNION:      return "UNION";
    case TYPE_FUNC_PTR:   return "FUNC_PTR";
    case TYPE_FILE_PTR:   return "FILE_PTR";
    case TYPE_VOID_STAR:  return "VOID*";
    case TYPE_INT_STAR:   return "INT*";
    case TYPE_FLOAT_STAR: return "FLOAT*";
    default:              return "POINTER/OTHER";
    }
}

/* ============================== HEAP-ONLY EXTRAS ============================== */
// Everything below relies on realloc/malloc/memmove and has no equivalent on a fixed-size
// stack array - none of these need a per-slot size, only the array-level capacity/count.

// Realloc the backing buffer down to exactly `count` slots, releasing any wasted headroom
// left over from growth. A stack array can't do this - its size is fixed by the caller's
// stack frame for the life of the scope.
extern void vShrinkToFitTable(Table* obj)
{
    if (obj == NULL || obj->slots == NULL) return;

    if (obj->count == obj->capacity)
    {
        printf("[vShrinkToFitTable] Already tight (Capacity == Count == %d). Nothing to do.\n", obj->count);
        return;
    }

    if (obj->count == 0)
    {
        // Nothing active - free the buffer entirely rather than realloc to a 0-size block.
        free(obj->slots);
        obj->slots = NULL;
        obj->capacity = 0;
        printf("[vShrinkToFitTable] Table was empty - buffer freed entirely.\n");
        return;
    }

    size_t new_bytes = (size_t)obj->count * sizeof(TableSlot_DA);
    TableSlot_DA* shrunk = (TableSlot_DA*)realloc(obj->slots, new_bytes);
    if (shrunk == NULL)
    {
        // Shrinking realloc failing is rare but not fatal - the old, larger buffer is still valid.
        printf("[vShrinkToFitTable] Warning: realloc failed, buffer left at old size.\n");
        return;
    }

    obj->slots = shrunk;
    obj->capacity = obj->count;
    printf("[vShrinkToFitTable] Shrunk buffer to exactly %d slot(s).\n", obj->capacity);
}

// Pre-grow the buffer by `additional_capacity` slots in one realloc, instead of letting
// bPushSlot() grow it geometrically (1.5x) across many separate calls. Use before a big batch
// of pushes when you know roughly how many are coming - it turns several reallocs into one.
extern bool vReserveTable(Table* obj, int additional_capacity)
{
    if (obj == NULL || additional_capacity <= 0) return false;

    int new_capacity = obj->capacity + additional_capacity;
    size_t new_bytes = (size_t)new_capacity * sizeof(TableSlot_DA);

    TableSlot_DA* grown = (TableSlot_DA*)realloc(obj->slots, new_bytes);
    if (grown == NULL)
    {
        printf("[vReserveTable] Error: realloc failed while reserving %d extra slot(s).\n", additional_capacity);
        return false;
    }

    for (int i = obj->capacity; i < new_capacity; i++)
    {
        grown[i].ptr = NULL;
        grown[i].type = TYPE_EMPTY;
    }

    obj->slots = grown;
    obj->capacity = new_capacity;
    printf("[vReserveTable] Reserved %d extra slot(s) - capacity now %d.\n", additional_capacity, obj->capacity);
    return true;
}

// Insert `node` at `index`, shifting everything from `index` onward one slot to the right
// (growing the buffer first if needed). A stack array could shift in place too, but only up
// to its fixed capacity - here there's no such ceiling.
extern bool bInsertAtTable(Table* obj, int index, TableSlot_DA node)
{
    if (obj == NULL || obj->slots == NULL || index < 0 || index > obj->count)
    {
        printf("[bInsertAtTable] Error: index %d out of range (count=%d).\n", index, obj ? obj->count : -1);
        return false;
    }

    if (obj->count == obj->capacity)
    {
        // Same 1.5x geometric growth as bPushSlot(), so a caller doing mid-array inserts
        // doesn't silently fall back to the old additive +5-per-grow behavior.
        int new_capacity = (obj->capacity == 0) ? 8 : (obj->capacity * 3) / 2;
        if (!vReserveTable(obj, new_capacity - obj->capacity))
            return false;
    }

    memmove(&obj->slots[index + 1], &obj->slots[index], (size_t)(obj->count - index) * sizeof(TableSlot_DA));
    obj->slots[index] = node;
    obj->count++;

    printf("[bInsertAtTable] Inserted at index %d (Type Tag: %d). New count: %d\n", index, node.type, obj->count);
    return true;
}

// Remove the slot at `index`, freeing it if Table-owned, then shift everything after it left
// by one. This is an O(n) operation - fine for occasional mid-array edits, not a hot-path op.
extern bool bRemoveAtTable(Table* obj, int index)
{
    if (obj == NULL || index < 0 || index >= obj->count)
    {
        printf("[bRemoveAtTable] Error: index %d out of range (count=%d).\n", index, obj ? obj->count : -1);
        return false;
    }

    if (obj->slots[index].ptr != NULL && bTypeOwnsHeapPtr(obj->slots[index].type))
        vFreeArena(obj->slots[index].ptr);

    memmove(&obj->slots[index], &obj->slots[index + 1], (size_t)(obj->count - index - 1) * sizeof(TableSlot_DA));
    obj->count--;

    obj->slots[obj->count].ptr = NULL;
    obj->slots[obj->count].type = TYPE_EMPTY;

    printf("[bRemoveAtTable] Removed index %d. New count: %d\n", index, obj->count);
    return true;
}
/* ============================================================================
   MODULE: ARRAY-BASED LOCAL/STACK TABLE  (TableStack / TableSlot_SA)
   Originally: T_LIBFXNS_ARR_LCL.cpp
   ============================================================================ */



extern bool bPushSlotStack(TableStack* obj, TableSlot_SA node)
{
    if (obj == NULL)
    {
        printf("[bPushSlotStack] Error: Invalid table pointer.\n");
        return false;
    }

    if (obj->count >= obj->capacity)
    {
        printf("[bPushSlotStack] Error: Table full (Capacity: %d). "
            "Growing is not supported for stack-only storage - declare a bigger "
            "vGetTableStack(name, N), or use the heap-based Table instead.\n", obj->capacity);
        return false;
    }

    obj->slots[obj->count] = node;

    printf("[bPushSlotStack] TableSlot_SA pushed successfully at index %d (Type Tag: %d).\n", obj->count, node.type);
    obj->count++;

    return true;
}

/**
 * push_raw_bytes (internal) - Copy `size` raw bytes of `val` directly into
 * the storage cell at obj->slots[obj->count].ptr, tag it with `type`, and
 * bump the count. This is how every PRIMITIVE gets pushed (see the "two storage
 * styles" note in the file header).
 *
 * Rejects anything wider than a single cell (sizeof(void*)) - that's the
 * hard ceiling for what a stack-only slot can hold.
 */
static bool push_raw_bytes(TableStack* obj, const void* val, size_t size, short type)
{
    if (obj == NULL)
    {
        printf("[push_raw_bytes] Error: Invalid table pointer.\n");
        return false;
    }
    if (obj->count >= obj->capacity)
    {
        printf("[push_raw_bytes] Error: Table full (Capacity: %d). Growing not supported.\n", obj->capacity);
        return false;
    }
    if (size > sizeof(void*))
    {
        printf("[push_raw_bytes] Error: value is %zu bytes, exceeds sizeof(void*) (%zu). "
            "A single stack cell can't hold it - use the heap-based Table instead.\n",
            size, sizeof(void*));
        return false;
    }

    obj->slots[obj->count].ptr = NULL;         /* zero the cell first so any
                                                   unused trailing bytes are
                                                   deterministic, not garbage */
    memcpy(&obj->slots[obj->count].ptr, val, size);
    obj->slots[obj->count].type = type;

    printf("[bPushSlotStack] TableSlot_SA pushed successfully at index %d (Type Tag: %d).\n", obj->count, type);
    obj->count++;
    return true;
}

/**
 * push_raw_bytes_silent (internal) - Identical to push_raw_bytes(), minus
 * the printf and minus the descriptive error messages. Used by bulk
 * operations (e.g. iPushIntArrayStack) that push many elements per call
 * and would otherwise flood stdout with one line per element.
 */
static bool push_raw_bytes_silent(TableStack* obj, const void* val, size_t size, short type)
{
    if (obj == NULL || obj->count >= obj->capacity || size > sizeof(void*))
        return false;

    obj->slots[obj->count].ptr = NULL;
    memcpy(&obj->slots[obj->count].ptr, val, size);
    obj->slots[obj->count].type = type;
    obj->count++;
    return true;
}

/* ----------------------------------------------------------------------
 * PUSH: primitives - each is a one-line call into push_raw_bytes() with
 * the right size/type tag.
 * ---------------------------------------------------------------------- */

extern bool bPushIntStack(TableStack* obj, int val)
{
    return push_raw_bytes(obj, &val, sizeof(val), TYPE_INT);
}

extern bool bPushFloatStack(TableStack* obj, float val)
{
    return push_raw_bytes(obj, &val, sizeof(val), TYPE_FLOAT);
}

extern bool bPushDoubleStack(TableStack* obj, double val)
{
    return push_raw_bytes(obj, &val, sizeof(val), TYPE_DOUBLE);
}

extern bool bPushLongStack(TableStack* obj, long val)
{
    return push_raw_bytes(obj, &val, sizeof(val), TYPE_LONG);
}

extern bool bPushShortStack(TableStack* obj, short val)
{
    return push_raw_bytes(obj, &val, sizeof(val), TYPE_SHORT);
}

extern bool bPushCharStack(TableStack* obj, char val)
{
    return push_raw_bytes(obj, &val, sizeof(val), TYPE_CHAR);
}

/* ----------------------------------------------------------------------
 * PUSH: pointers - the cell holds the pointer VALUE itself (not a wrapper
 * like the heap-based Table uses), since a pointer already fits in one
 * word. The Table does not own, and will never free, what these point to.
 * ---------------------------------------------------------------------- */

extern bool bPushPtrStack(TableStack* obj, void* val, short pointer_type)
{
    return push_raw_bytes(obj, &val, sizeof(val), pointer_type);
}

extern bool bPushDPtrStack(TableStack* obj, void** val, short double_pointer_type)
{
    return push_raw_bytes(obj, &val, sizeof(val), double_pointer_type);
}

extern bool bPushTPtrStack(TableStack* obj, void*** val, short triple_pointer_type)
{
    return push_raw_bytes(obj, &val, sizeof(val), triple_pointer_type);
}

/* ----------------------------------------------------------------------
 * PUSH: small structs / unions / function & file pointers
 * ---------------------------------------------------------------------- */

 /**
  * bPushStructStack - Copy up to sizeof(void*) bytes of a struct directly
  * into one cell. Unlike the heap-based Table (which can hold a struct of
  * any size via malloc), this variant is hard-capped: anything bigger than
  * one machine word simply does not fit here.
  */
extern bool bPushStructStack(TableStack* obj, void* struct_data, size_t size)
{
    if (struct_data == NULL || size == 0)
        return false;

    if (size > sizeof(void*))
    {
        printf("[bPushStructStack] Error: struct is %zu bytes, exceeds sizeof(void*) (%zu) - "
            "this variant has no separate byte arena, only one cell per slot. "
            "Use the heap-based Table (bPushStruct) instead.\n", size, sizeof(void*));
        return false;
    }
    return push_raw_bytes(obj, struct_data, size, TYPE_STRUCT);
}

/**
 * bPushUnionStack - Same one-cell-sized-ceiling contract as
 * bPushStructStack(), but tagged TYPE_UNION.
 */
extern bool bPushUnionStack(TableStack* obj, void* union_data, size_t union_size)
{
    if (union_data == NULL || union_size == 0)
        return false;

    if (union_size > sizeof(void*))
    {
        printf("[bPushUnionStack] Error: union is %zu bytes, exceeds sizeof(void*) (%zu) - "
            "this variant has no separate byte arena, only one cell per slot. "
            "Use the heap-based Table (bPushUnion) instead.\n", union_size, sizeof(void*));
        return false;
    }
    return push_raw_bytes(obj, union_data, union_size, TYPE_UNION);
}

extern bool bPushFuncStack(TableStack* obj, void* func_ptr)
{
    if (func_ptr == NULL)
        return false;
    return push_raw_bytes(obj, &func_ptr, sizeof(func_ptr), TYPE_FUNC_PTR);
}

extern bool bPushFileStack(TableStack* obj, FILE* fp)
{
    if (fp == NULL)
        return false;
    void* raw = (void*)fp;
    return push_raw_bytes(obj, &raw, sizeof(raw), TYPE_FILE_PTR);
}

/* ============================================================================
 *  Pop / drop-top / reset
 * ============================================================================ */

 /**
  * sPopSlotStack - Remove and return the top slot.
  *
  * Because nothing here was ever heap-allocated, there is no free() to
  * perform - we just copy the cell out, hand it back by value, and zero
  * the vacated cell so stale data can't be misread later.
  */
extern TableSlot_SA sPopSlotStack(TableStack* obj)
{
    printf("\n=== Popping Slot (stack) ===\n");

    if (obj == NULL || obj->count == 0)
    {
        printf("[sPopSlotStack] Underflow Error: Cannot pop from an empty or NULL table!\n");
        TableSlot_SA empty = { NULL, TYPE_EMPTY };
        return empty;
    }

    obj->count--;
    int top = obj->count;

    TableSlot_SA out = obj->slots[top];

    printf("[sPopSlotStack] Extracted element at index %d (Type Tag: %d).\n", top, out.type);

    obj->slots[top].ptr = NULL;
    obj->slots[top].type = TYPE_EMPTY;

    printf("[sPopSlotStack] Slot index %d sanitized. Remaining active count: %d\n", top, obj->count);

    return out;
}

/**
 * bDropTopStack - Same effect as sPopSlotStack() but silent, and doesn't
 * hand back the value - use this when you just want the top gone and
 * don't care what was there (cheaper: no value to copy or print).
 */
extern bool bDropTopStack(TableStack* obj)
{
    if (obj == NULL || obj->count == 0)
        return false;

    obj->count--;
    obj->slots[obj->count].ptr = NULL;
    obj->slots[obj->count].type = TYPE_EMPTY;
    return true;
}

/**
 * vDropTableStack - Reset the whole table to empty. Since every slot lives
 * on the caller's stack (not the heap), this is just a memset + count
 * reset - there's nothing to free.
 */
extern void vDropTableStack(TableStack* obj)
{
    if (obj == NULL)
    {
        printf("[vDropTableStack] Warning: NULL pointer provided.\n");
        return;
    }

    printf("\n--- Dropping Table (stack) ---\n");
    printf("[vDropTableStack] Zeroing %d active slot(s).\n", obj->count);

    if (obj->slots && obj->capacity > 0)
        memset(obj->slots, 0, (size_t)obj->capacity * sizeof(TableSlot_SA));
    obj->count = 0;

    printf("--- Table (stack) Reset Complete ---\n\n");
}

/* ============================================================================
 *  Random access
 * ============================================================================ */

 /**
  * sGetSlotAtStack - Read (without removing) the slot at `index`.
  * @return the slot, or a TYPE_EMPTY slot if `index` is out of range.
  */
extern TableSlot_SA sGetSlotAtStack(const TableStack* obj, int index)
{
    if (obj == NULL || index < 0 || index >= obj->count)
    {
        printf("[sGetSlotAtStack] Error: index %d out of range (count=%d).\n", index, obj ? obj->count : -1);
        TableSlot_SA empty = { NULL, TYPE_EMPTY };
        return empty;
    }
    return obj->slots[index];
}

/**
 * sGetTypeAtStack - Just the type tag at `index`, without building a full
 * TableSlot_SA. Returns TYPE_EMPTY for an out-of-range index.
 */
extern short sGetTypeAtStack(const TableStack* obj, int index)
{
    if (obj == NULL || index < 0 || index >= obj->count)
        return TYPE_EMPTY;
    return obj->slots[index].type;
}

/**
 * bTryGetSlotAtStackTyped - Type-checked slot access. Validates the index AND
 * the stored type tag before handing back the slot. Returns false (leaving
 * *out untouched) on an out-of-range index or a type-tag mismatch; returns
 * true and copies the slot into *out on success.
 */
extern bool bTryGetSlotAtStackTyped(const TableStack* obj, int index, short expected_type, TableSlot_SA* out)
{
    if (obj == NULL || out == NULL || index < 0 || index >= obj->count)
    {
        printf("[bTryGetSlotAtStackTyped] Error: index %d out of range (count=%d).\n", index, obj ? obj->count : -1);
        return false;
    }
    if (obj->slots[index].type != expected_type)
    {
        printf("[bTryGetSlotAtStackTyped] Type mismatch at index %d: expected %d, found %d.\n",
            index, expected_type, obj->slots[index].type);
        return false;
    }
    *out = obj->slots[index];
    return true;
}

/* ============================================================================
 *  Stack-manipulation ops
 * ============================================================================ */

 /**
  * bDupTopStack - Push a copy of the current top slot on top of itself,
  * e.g. [A, B] -> [A, B, B]. Fails silently (returns false) if the table
  * is empty or already full.
  */
extern bool bDupTopStack(TableStack* obj)
{
    if (obj == NULL || obj->count == 0 || obj->count >= obj->capacity)
        return false;

    obj->slots[obj->count] = obj->slots[obj->count - 1];
    obj->count++;
    return true;
}

/**
 * bSwapTopStack - Swap the top two slots in place, e.g. [A, B] -> [A's new
 * value is B, B's new value is A]. Requires at least 2 elements.
 */
extern bool bSwapTopStack(TableStack* obj)
{
    if (obj == NULL || obj->count < 2)
        return false;

    int a = obj->count - 1, b = obj->count - 2;
    TableSlot_SA tmp = obj->slots[a];
    obj->slots[a] = obj->slots[b];
    obj->slots[b] = tmp;
    return true;
}

/* ============================================================================
 *  Query / aggregate ops - each is a single O(n) pass, no redundant scans
 * ============================================================================ */

extern int iCountOfTypeStack(const TableStack* obj, short type)
{
    if (obj == NULL) return 0;

    int n = 0;
    for (int i = 0; i < obj->count; i++)
        if (obj->slots[i].type == type) n++;
    return n;
}

extern bool bContainsTypeStack(const TableStack* obj, short type)
{
    if (obj == NULL) return false;

    for (int i = 0; i < obj->count; i++)
        if (obj->slots[i].type == type) return true; /* early exit - no need to scan the rest */
    return false;
}

extern int iFindTypeStack(const TableStack* obj, short type)
{
    if (obj == NULL) return -1;

    for (int i = 0; i < obj->count; i++)
        if (obj->slots[i].type == type) return i; /* early exit: index of first match */
    return -1;
}

extern int iSumIntStack(const TableStack* obj)
{
    if (obj == NULL) return 0;

    int sum = 0;
    for (int i = 0; i < obj->count; i++)
        if (obj->slots[i].type == TYPE_INT)
            /* Reinterpret the inline cell as an int* and read through it -
               see the "two storage styles" note at the top of this file. */
            sum += *(int*)&obj->slots[i].ptr;
    return sum;
}

extern double dSumFloatStack(const TableStack* obj)
{
    if (obj == NULL) return 0.0;

    double sum = 0.0;
    for (int i = 0; i < obj->count; i++)
        if (obj->slots[i].type == TYPE_FLOAT)
            sum += (double)(*(float*)&obj->slots[i].ptr);
    return sum;
}

extern bool bMinIntStack(const TableStack* obj, int* out)
{
    if (obj == NULL || out == NULL) return false;

    bool found = false;
    int best = 0;
    for (int i = 0; i < obj->count; i++)
    {
        if (obj->slots[i].type != TYPE_INT) continue;
        int v = *(int*)&obj->slots[i].ptr;
        if (!found || v < best) { best = v; found = true; }
    }
    if (found) *out = best;
    return found;
}

extern bool bMaxIntStack(const TableStack* obj, int* out)
{
    if (obj == NULL || out == NULL) return false;

    bool found = false;
    int best = 0;
    for (int i = 0; i < obj->count; i++)
    {
        if (obj->slots[i].type != TYPE_INT) continue;
        int v = *(int*)&obj->slots[i].ptr;
        if (!found || v > best) { best = v; found = true; }
    }
    if (found) *out = best;
    return found;
}

/* ============================================================================
 *  Bulk ops
 * ============================================================================ */

 /**
  * vReverseStack - Reverse the slot order in place, e.g. [A, B, C] -> [C, B, A].
  */
extern void vReverseStack(TableStack* obj)
{
    if (obj == NULL || obj->count < 2) return;

    int lo = 0, hi = obj->count - 1;
    while (lo < hi)
    {
        TableSlot_SA tmp = obj->slots[lo];
        obj->slots[lo] = obj->slots[hi];
        obj->slots[hi] = tmp;
        lo++; hi--;
    }
}

/**
 * iPushIntArrayStack - Push as many ints from `arr` as will fit (silently,
 * one printf for the whole batch instead of one per element).
 * @return how many elements actually got pushed (may be less than `n` if
 *         the table didn't have enough remaining capacity).
 */
extern int iPushIntArrayStack(TableStack* obj, const int* arr, int n)
{
    if (obj == NULL || arr == NULL || n <= 0) return 0;

    int space = obj->capacity - obj->count;
    int to_push = (n < space) ? n : space;

    for (int i = 0; i < to_push; i++)
        push_raw_bytes_silent(obj, &arr[i], sizeof(int), TYPE_INT);

    printf("[iPushIntArrayStack] Bulk-pushed %d/%d int(s).\n", to_push, n);
    return to_push;
}

/**
 * iPushFloatArrayStack - Same contract as iPushIntArrayStack(), for floats.
 */
extern int iPushFloatArrayStack(TableStack* obj, const float* arr, int n)
{
    if (obj == NULL || arr == NULL || n <= 0) return 0;

    int space = obj->capacity - obj->count;
    int to_push = (n < space) ? n : space;

    for (int i = 0; i < to_push; i++)
        push_raw_bytes_silent(obj, &arr[i], sizeof(float), TYPE_FLOAT);

    printf("[iPushFloatArrayStack] Bulk-pushed %d/%d float(s).\n", to_push, n);
    return to_push;
}

/**
 * bCloneTableStack - Copy all active slots from `src` into `dest` via a
 * couple of bulk memcpy calls (not a per-element loop - much cheaper).
 * `dest` must already have enough capacity to hold `src->count` slots;
 * this never grows `dest`.
 */
extern bool bCloneTableStack(TableStack* dest, const TableStack* src)
{
    if (dest == NULL || src == NULL) return false;

    if (dest->capacity < src->count)
    {
        printf("[bCloneTableStack] Error: destination capacity (%d) can't hold source count (%d).\n",
            dest->capacity, src->count);
        return false;
    }

    memcpy(dest->slots, src->slots, (size_t)src->count * sizeof(TableSlot_SA));
    dest->count = src->count;

    printf("[bCloneTableStack] Cloned %d slot(s).\n", src->count);
    return true;
}

/* ============================================================================
 *  Debug / introspection
 * ============================================================================ */

 /**
  * sTypeNameStack - Human-readable name for a type tag, for logging/debug
  * output. Anything not explicitly listed (the less common pointer /
  * struct-pointer / union-pointer tags) falls back to a generic label
  * rather than being left unhandled.
  */
extern const char* sTypeNameStack(short type)
{
    switch (type)
    {
    case TYPE_EMPTY:      return "EMPTY";
    case TYPE_INT:        return "INT";
    case TYPE_FLOAT:      return "FLOAT";
    case TYPE_DOUBLE:     return "DOUBLE";
    case TYPE_CHAR:       return "CHAR";
    case TYPE_LONG:       return "LONG";
    case TYPE_SHORT:      return "SHORT";
    case TYPE_STRUCT:     return "STRUCT";
    case TYPE_UNION:      return "UNION";
    case TYPE_FUNC_PTR:   return "FUNC_PTR";
    case TYPE_FILE_PTR:   return "FILE_PTR";
    case TYPE_VOID_STAR:  return "VOID*";
    case TYPE_INT_STAR:   return "INT*";
    case TYPE_FLOAT_STAR: return "FLOAT*";
    default:              return "POINTER/OTHER";
    }
}

/**
 * vPrintSlotStack - Print one slot's type + value/address in a readable
 * form. Shared by vPrintTableStack() below and available standalone for
 * printing a single popped/peeked slot.
 */
extern void vPrintSlotStack(TableSlot_SA slot)
{
    switch (slot.type)
    {
    case TYPE_INT:    printf("[TYPE_INT]              \t Value : %d\n", GET_STACK_VALUE(slot, int)); break;
    case TYPE_FLOAT:  printf("[TYPE_FLOAT]            \t Value : %.4f\n", GET_STACK_VALUE(slot, float)); break;
    case TYPE_DOUBLE: printf("[TYPE_DOUBLE]           \t Value : %.6lf\n", GET_STACK_VALUE(slot, double)); break;
    case TYPE_CHAR:   printf("[TYPE_CHAR]             \t Value : '%c'\n", GET_STACK_VALUE(slot, char)); break;
    case TYPE_LONG:   printf("[TYPE_LONG]             \t Value : %ld\n", GET_STACK_VALUE(slot, long)); break;
    case TYPE_SHORT:  printf("[TYPE_SHORT]            \t Value : %d\n", GET_STACK_VALUE(slot, short)); break;
    case TYPE_STRUCT: printf("[TYPE_STRUCT]           \t (inline cell, <= %zu bytes)\n", sizeof(void*)); break;
    case TYPE_UNION:  printf("[TYPE_UNION]            \t (inline cell, <= %zu bytes)\n", sizeof(void*)); break;
    case TYPE_FUNC_PTR: printf("[TYPE_FUNC_PTR]         \t Addr  : %p\n", slot.ptr); break;
    case TYPE_FILE_PTR: printf("[TYPE_FILE_PTR]         \t Addr  : %p\n", slot.ptr); break;
    case TYPE_EMPTY:  printf("[TYPE_EMPTY]\n"); break;
    default:          printf("[%-9s, Tag %-4d] \t Addr  : %p\n", sTypeNameStack(slot.type), slot.type, slot.ptr); break;
    }
}

/**
 * vPrintTableStack - Dump every active slot to stdout (diagnostic only,
 * never mutates the table). Delegates each line to vPrintSlotStack().
 */
extern void vPrintTableStack(const TableStack* obj)
{
    if (!obj)
    {
        printf("[vPrintTableStack] Error: Table is NULL.\n");
        return;
    }

    printf("\n========================================================================\n");
    printf("  TABLE CONTENTS (stack)  |  Count: %-3d  |  Capacity: %-3d\n", obj->count, obj->capacity);
    printf("========================================================================\n");

    if (obj->count == 0)
    {
        printf("  [Empty Table]\n");
        printf("------------------------------------------------------------------------\n\n");
        return;
    }

    for (int i = 0; i < obj->count; i++)
    {
        printf(" [%02d] ", i);
        vPrintSlotStack(obj->slots[i]);
    }

    printf("------------------------------------------------------------------------\n\n");
}

/**
 * vForEachSlotStack - Call `visitor(index, slot)` for every active slot,
 * in order. A simple, allocation-free way to let external code iterate
 * without reaching into the table's internals directly.
 */
extern void vForEachSlotStack(const TableStack* obj, TableStackVisitor visitor)
{
    if (obj == NULL || visitor == NULL) return;

    for (int i = 0; i < obj->count; i++)
        visitor(i, obj->slots[i]);
}

/**
 * bPushListStack - Variadic convenience: push `count` (type, value) pairs
 * in one call. Stack-only counterpart of bPushList() - same calling
 * convention and same default-argument-promotion caveats apply (float ->
 * double, short/char -> int, etc.).
 */
extern bool bPushListStack(TableStack* obj, int count, ...)
{
    if (!obj || count <= 0) return false;

    va_list args;
    va_start(args, count);

    for (int i = 0; i < count; i++)
    {
        short type = (short)va_arg(args, int);

        switch (type)
        {
        case TYPE_INT:    bPushIntStack(obj, va_arg(args, int)); break;
        case TYPE_FLOAT:  bPushFloatStack(obj, (float)va_arg(args, double)); break;
        case TYPE_DOUBLE: bPushDoubleStack(obj, va_arg(args, double)); break;
        case TYPE_CHAR:   bPushCharStack(obj, (char)va_arg(args, int)); break;
        case TYPE_LONG:   bPushLongStack(obj, va_arg(args, long)); break;
        case TYPE_SHORT:  bPushShortStack(obj, (short)va_arg(args, int)); break;

        case TYPE_INT_STAR: case TYPE_FLOAT_STAR: case TYPE_CHAR_STAR:
        case TYPE_DOUBLE_STAR: case TYPE_LONG_STAR: case TYPE_SHORT_STAR: case TYPE_VOID_STAR:
            bPushPtrStack(obj, va_arg(args, void*), type);
            break;

        case TYPE_INT_STAR_DOUBLE: case TYPE_FLOAT_STAR_DOUBLE: case TYPE_CHAR_STAR_DOUBLE:
        case TYPE_DOUBLE_STAR_DOUBLE: case TYPE_LONG_STAR_DOUBLE: case TYPE_SHORT_STAR_DOUBLE: case TYPE_VOID_STAR_DOUBLE:
            bPushDPtrStack(obj, va_arg(args, void**), type);
            break;

        case TYPE_INT_STAR_TRIPLE: case TYPE_FLOAT_STAR_TRIPLE: case TYPE_CHAR_STAR_TRIPLE:
        case TYPE_DOUBLE_STAR_TRIPLE: case TYPE_LONG_STAR_TRIPLE: case TYPE_SHORT_STAR_TRIPLE: case TYPE_VOID_STAR_TRIPLE:
            bPushTPtrStack(obj, va_arg(args, void***), type);
            break;

        case TYPE_STRUCT:
        {
            void* sp = va_arg(args, void*);
            size_t sz = va_arg(args, size_t);
            bPushStructStack(obj, sp, sz);
            break;
        }
        case TYPE_UNION:
        {
            void* up = va_arg(args, void*);
            size_t sz = va_arg(args, size_t);
            bPushUnionStack(obj, up, sz);
            break;
        }
        default:
            break;
        }
    }

    va_end(args);
    return true;
}
/* ============================================================================
   MODULE: LINKED-LIST BASED TABLE  (TableList / TableSlot_LL)
   Originally: T_LL_BASED.cpp
   ============================================================================ */


   // ============================================================================
   // 1. CONSTRUCTION / DESTRUCTION
   // ============================================================================

   // initialize an empty list
void tFormLinkedTable(TableList* list)
{
    if (list == NULL) return; // guard against null container

    list->tNodeHead = NULL;  // no nodes yet
    list->tNodeTail = NULL;  // no nodes yet
    list->count = 0;    // zero nodes
}

// allocate and populate a node
tNode* tCreateNode(TableSlot_LL payload)
{
    tNode* newNode = (tNode*)malloc(sizeof(tNode)); // grab heap memory
    if (newNode == NULL)
    {
        printf("[tCreateNode] Fatal Error: Heap memory allocation failed!\n"); // report OOM
        return NULL;
    }

    newNode->slot = payload; // store the payload
    newNode->next = NULL;    // no forward link yet
    newNode->prev = NULL;    // no backward link yet
    return newNode;
}

// free every node in the list
void tDestroyList(TableList* list)
{
    if (list == NULL || list->tNodeHead == NULL) return; // nothing to free

    tNode* current = list->tNodeHead; // start at head
    tNode* nextNode = NULL;      // holder for forward link
    int nodesFreed = 0;          // teardown counter

    while (current != NULL)
    {
        nextNode = current->next; // save next before freeing current
        free(current);            // release node memory
        current = nextNode;       // advance
        nodesFreed++;             // tally
    }

    list->tNodeHead = NULL;  // sterilize container
    list->tNodeTail = NULL;  // sterilize container
    list->count = 0;    // sterilize container

    printf("[tDestroyList] Teardown complete. %d node(s) freed.\n", nodesFreed); // report
}

// debug-print list contents
void vPrintList(const TableList* list)
{
    if (bIsListEmpty(list))
    {
        printf("=== TableList Contents (Count: 0) ===\n"); // empty case
        return;
    }

    printf("=== TableList Contents (Count: %d) ===\n", list->count); // header

    tNode* current = list->tNodeHead; // start at head
    int index = 0;                // display index

    while (current != NULL)
    {
        printf("  [Node %02d] Type Tag: %d\n", index, current->slot.type); // print tag
        current = current->next; // walk forward
        index++;                 // bump index
    }

    printf("======================================\n"); // footer
}

// check if list has no nodes
bool bIsListEmpty(const TableList* list)
{
    return (list == NULL || list->tNodeHead == NULL); // null container or empty chain counts as empty
}

// ============================================================================
// 2. RAW PUSH / POP / PEEK (O(1))
// ============================================================================

// insert raw slot at tail
void tPushBack(TableList* list, TableSlot_LL payload)
{
    tNode* newNode = tCreateNode(payload); // build node
    if (newNode == NULL) return;

    if (list->tNodeHead == NULL)
    {
        list->tNodeHead = newNode; // first node becomes head
        list->tNodeTail = newNode; // and tail
    }
    else
    {
        list->tNodeTail->next = newNode; // link old tail forward
        newNode->prev = list->tNodeTail; // link new node backward
        list->tNodeTail = newNode;       // advance tail
    }

    list->count++; // grow count
}

// insert raw slot at head
void tPushFront(TableList* list, TableSlot_LL payload)
{
    tNode* newNode = tCreateNode(payload); // build node
    if (newNode == NULL) return;

    if (list->tNodeHead == NULL)
    {
        list->tNodeHead = newNode; // first node becomes head
        list->tNodeTail = newNode; // and tail
    }
    else
    {
        list->tNodeHead->prev = newNode; // link old head backward
        newNode->next = list->tNodeHead; // link new node forward
        list->tNodeHead = newNode;       // advance head
    }

    list->count++; // grow count
}

// remove and return head slot
TableSlot_LL tPopFront(TableList* list)
{
    if (list->tNodeHead == NULL)
    {
        printf("[tPopFront] Error: List is empty.\n"); // guard
        TableSlot_LL emptySlot = { NULL, 0 };
        return emptySlot;
    }

    tNode* nodeToRemove = list->tNodeHead;              // target node
    TableSlot_LL extractedPayload = nodeToRemove->slot; // copy out payload

    if (list->tNodeHead == list->tNodeTail)
    {
        list->tNodeHead = NULL; // list becomes empty
        list->tNodeTail = NULL;
    }
    else
    {
        list->tNodeHead = list->tNodeHead->next; // advance head
        list->tNodeHead->prev = NULL;       // sever backward link
    }

    free(nodeToRemove); // release node memory
    list->count--;      // shrink count
    return extractedPayload;
}

// remove and return tail slot
TableSlot_LL tPopBack(TableList* list)
{
    if (list->tNodeTail == NULL)
    {
        printf("[tPopBack] Error: List is empty.\n"); // guard
        TableSlot_LL emptySlot = { NULL, 0 };
        return emptySlot;
    }

    tNode* nodeToRemove = list->tNodeTail;              // target node
    TableSlot_LL extractedPayload = nodeToRemove->slot; // copy out payload

    if (list->tNodeHead == list->tNodeTail)
    {
        list->tNodeHead = NULL; // list becomes empty
        list->tNodeTail = NULL;
    }
    else
    {
        list->tNodeTail = list->tNodeTail->prev; // retreat tail
        list->tNodeTail->next = NULL;       // sever forward link
    }

    free(nodeToRemove); // release node memory
    list->count--;      // shrink count
    return extractedPayload;
}

// read head slot without removing
TableSlot_LL sPeekFront(const TableList* list)
{
    if (bIsListEmpty(list))
    {
        printf("[sPeekFront] Warning: List is empty.\n"); // guard
        TableSlot_LL emptySlot = { NULL, 0 };
        return emptySlot;
    }

    return list->tNodeHead->slot; // copy of payload, topology untouched
}

// read tail slot without removing
TableSlot_LL sPeekBack(const TableList* list)
{
    if (bIsListEmpty(list))
    {
        printf("[sPeekBack] Warning: List is empty.\n"); // guard
        TableSlot_LL emptySlot = { NULL, 0 };
        return emptySlot;
    }

    return list->tNodeTail->slot; // O(1) jump directly to tail
}

// random-access read by index
TableSlot_LL tGetNodeAt(const TableList* list, int target_index)
{
    if (list == NULL || list->tNodeHead == NULL)
    {
        printf("[tGetNodeAt] Error: List is empty.\n"); // guard
        TableSlot_LL empty = { NULL, 0 };
        return empty;
    }

    if (target_index < 0 || target_index >= list->count)
    {
        printf("[tGetNodeAt] Error: Index out of bounds.\n"); // bounds check
        TableSlot_LL empty = { NULL, 0 };
        return empty;
    }

    tNode* current = list->tNodeHead; // start at head
    for (int i = 0; i < target_index; i++)
    {
        current = current->next; // walk to target
    }

    return current->slot;
}

// ============================================================================
// 3. TARGETED TOPOLOGY MODIFICATION (generic)
// ============================================================================

// insert raw slot at index
void tInsertAt(TableList* list, int index, TableSlot_LL payload)
{
    if (index <= 0)
    {
        tPushFront(list, payload); // boundary optimization: front
        return;
    }
    if (index >= list->count)
    {
        tPushBack(list, payload); // boundary optimization: back
        return;
    }

    tNode* current = list->tNodeHead; // traverse to target position
    for (int i = 0; i < index; i++)
    {
        current = current->next;
    }

    tNode* newNode = tCreateNode(payload); // spawn new node
    if (newNode == NULL) return;

    newNode->prev = current->prev; // wire backward link
    newNode->next = current;       // wire forward link

    current->prev->next = newNode; // splice in before current
    current->prev = newNode;       // splice in before current

    list->count++; // grow count
}

// remove raw slot at index
TableSlot_LL tRemoveAt(TableList* list, int index)
{
    if (index <= 0) return tPopFront(list);            // boundary optimization: front
    if (index >= list->count - 1) return tPopBack(list); // boundary optimization: back

    tNode* current = list->tNodeHead; // traverse to target position
    for (int i = 0; i < index; i++)
    {
        current = current->next;
    }

    TableSlot_LL extractedPayload = current->slot; // copy out payload

    current->prev->next = current->next; // bridge repair
    current->next->prev = current->prev; // bridge repair

    free(current);  // release node memory
    list->count--;  // shrink count

    return extractedPayload;
}

// reverse node order in place
void vReverseList(TableList* list)
{
    if (bIsListEmpty(list) || list->count == 1) return; // nothing to reverse

    tNode* current = list->tNodeHead; // start walking from head
    tNode* temp = NULL;          // scratch pointer

    while (current != NULL)
    {
        temp = current->prev;         // save old prev before overwrite
        current->prev = current->next; // flip backward link
        current->next = temp;          // flip forward link
        current = current->prev;       // advance using the (now-flipped) link
    }

    temp = list->tNodeHead;    // swap master head/tail pointers
    list->tNodeHead = list->tNodeTail;
    list->tNodeTail = temp;

    printf("[vReverseList] Topology successfully inverted.\n"); // report
}

// ============================================================================
// 4. INT WRAPPERS (Tag: TYPE_INT)
// ============================================================================

// push int to tail
void tPushIntBack(TableList* list, int value)
{
    TableSlot_LL slot;
    slot.data = (void*)(intptr_t)value; // cast int directly into pointer cell
    slot.type = TYPE_INT;
    tPushBack(list, slot);
}

// push int to head
void tPushIntFront(TableList* list, int value)
{
    TableSlot_LL slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_INT;
    tPushFront(list, slot);
}

// read int by index
int tGetIntAt(const TableList* list, int index)
{
    TableSlot_LL slot = tGetNodeAt(list, index);
    if (slot.type != TYPE_INT)
    {
        printf("[tGetIntAt] Type mismatch at index %d.\n", index); // guard
        return 0;
    }
    return (int)(intptr_t)slot.data;
}

// pop int from head
int tPopIntFront(TableList* list)
{
    TableSlot_LL slot = tPopFront(list);
    if (slot.type != TYPE_INT)
    {
        printf("[tPopIntFront] Error: Popped slot is not an integer.\n"); // guard
        return 0;
    }
    return (int)(intptr_t)slot.data;
}

// pop int from tail
int tPopIntBack(TableList* list)
{
    TableSlot_LL slot = tPopBack(list);
    if (slot.type != TYPE_INT)
    {
        printf("[tPopIntBack] Error: Popped slot is not an integer.\n"); // guard
        return 0;
    }
    return (int)(intptr_t)slot.data;
}

// bulk push variadic ints
void tPushIntListBack(TableList* list, int count, ...)
{
    va_list args;
    va_start(args, count);

    for (int i = 0; i < count; i++)
    {
        int val = va_arg(args, int); // pull next int
        tPushIntBack(list, val);     // reuse safe wrapper
    }

    va_end(args);
    printf("[tPushIntsBack] Bulk-pushed %d int(s).\n", count); // report
}

// insert int at index
void tInsertIntAt(TableList* list, int index, int value)
{
    TableSlot_LL slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_INT;
    tInsertAt(list, index, slot);
}

// remove int at index
int tRemoveIntAt(TableList* list, int index)
{
    TableSlot_LL slot = tRemoveAt(list, index);
    if (slot.type != TYPE_INT) return 0; // guard
    return (int)(intptr_t)slot.data;
}

// linear search for int
int tFindInt(const TableList* list, int target)
{
    if (bIsListEmpty(list)) return -1;

    tNode* current = list->tNodeHead; // start at head
    int index = 0;

    while (current != NULL)
    {
        if (current->slot.type == TYPE_INT) // only compare tagged ints
        {
            int val = (int)(intptr_t)current->slot.data;
            if (val == target) return index; // found
        }
        current = current->next;
        index++;
    }
    return -1; // not found
}

// ============================================================================
// 5. FLOAT WRAPPERS (Tag: TYPE_FLOAT)
// ============================================================================

// push float to tail
void tPushFloatBack(TableList* list, float value)
{
    union { float f; void* v; } packer;
    packer.v = NULL;   // zero out full 64 bits first
    packer.f = value;  // inject the 32-bit float

    TableSlot_LL slot;
    slot.data = packer.v;
    slot.type = TYPE_FLOAT;
    tPushBack(list, slot);
}

// push float to head
void tPushFloatFront(TableList* list, float value)
{
    union { float f; void* v; } packer;
    packer.v = NULL;
    packer.f = value;

    TableSlot_LL slot;
    slot.data = packer.v;
    slot.type = TYPE_FLOAT;
    tPushFront(list, slot);
}

// read float by index
float tGetFloatAt(const TableList* list, int index)
{
    TableSlot_LL slot = tGetNodeAt(list, index);
    if (slot.type != TYPE_FLOAT) return 0.0f; // guard

    union { void* v; float f; } unpacker;
    unpacker.v = slot.data;
    return unpacker.f;
}

// pop float from head
float tPopFloatFront(TableList* list)
{
    TableSlot_LL slot = tPopFront(list);
    if (slot.type != TYPE_FLOAT) return 0.0f; // guard
    union { void* v; float f; } unpacker;
    unpacker.v = slot.data;
    return unpacker.f;
}

// pop float from tail
float tPopFloatBack(TableList* list)
{
    TableSlot_LL slot = tPopBack(list);
    if (slot.type != TYPE_FLOAT) return 0.0f; // guard
    union { void* v; float f; } unpacker;
    unpacker.v = slot.data;
    return unpacker.f;
}

// insert float at index
void tInsertFloatAt(TableList* list, int index, float value)
{
    union { float f; void* v; } packer;
    packer.v = NULL;
    packer.f = value;

    TableSlot_LL slot;
    slot.data = packer.v;
    slot.type = TYPE_FLOAT;
    tInsertAt(list, index, slot);
}

// remove float at index
float tRemoveFloatAt(TableList* list, int index)
{
    TableSlot_LL slot = tRemoveAt(list, index);
    if (slot.type != TYPE_FLOAT) return 0.0f; // guard
    union { void* v; float f; } unpacker;
    unpacker.v = slot.data;
    return unpacker.f;
}

// linear search for float
int tFindFloat(const TableList* list, float target)
{
    if (bIsListEmpty(list)) return -1;
    tNode* current = list->tNodeHead;
    int index = 0;

    while (current != NULL)
    {
        if (current->slot.type == TYPE_FLOAT)
        {
            union { void* v; float f; } unpacker;
            unpacker.v = current->slot.data;
            if (unpacker.f == target) return index; // found (exact compare)
        }
        current = current->next;
        index++;
    }
    return -1; // not found
}

// ============================================================================
// 6. DOUBLE WRAPPERS (Tag: TYPE_DOUBLE)
// ============================================================================

// push double to tail
void tPushDoubleBack(TableList* list, double value)
{
    union { double d; void* v; } packer;
    packer.d = value;

    TableSlot_LL slot;
    slot.data = packer.v;
    slot.type = TYPE_DOUBLE;
    tPushBack(list, slot);
}

// push double to head
void tPushDoubleFront(TableList* list, double value)
{
    union { double d; void* v; } packer;
    packer.d = value;

    TableSlot_LL slot;
    slot.data = packer.v;
    slot.type = TYPE_DOUBLE;
    tPushFront(list, slot);
}

// read double by index
double tGetDoubleAt(const TableList* list, int index)
{
    TableSlot_LL slot = tGetNodeAt(list, index);
    if (slot.type != TYPE_DOUBLE) return 0.0; // guard

    union { void* v; double d; } unpacker;
    unpacker.v = slot.data;
    return unpacker.d;
}

// pop double from head
double tPopDoubleFront(TableList* list)
{
    TableSlot_LL slot = tPopFront(list);
    if (slot.type != TYPE_DOUBLE) return 0.0; // guard

    union { void* v; double d; } unpacker;
    unpacker.v = slot.data;
    return unpacker.d;
}

// pop double from tail
double tPopDoubleBack(TableList* list)
{
    TableSlot_LL slot = tPopBack(list);
    if (slot.type != TYPE_DOUBLE) return 0.0; // guard

    union { void* v; double d; } unpacker;
    unpacker.v = slot.data;
    return unpacker.d;
}

// bulk push variadic doubles
void tPushDoublesBack(TableList* list, int count, ...)
{
    va_list args;
    va_start(args, count);
    for (int i = 0; i < count; i++)
    {
        tPushDoubleBack(list, va_arg(args, double)); // pull and push each double
    }
    va_end(args);
}

// insert double at index
void tInsertDoubleAt(TableList* list, int index, double value)
{
    union { double d; void* v; } packer;
    packer.d = value;

    TableSlot_LL slot;
    slot.data = packer.v;
    slot.type = TYPE_DOUBLE;
    tInsertAt(list, index, slot);
}

// remove double at index
double tRemoveDoubleAt(TableList* list, int index)
{
    TableSlot_LL slot = tRemoveAt(list, index);
    if (slot.type != TYPE_DOUBLE) return 0.0; // guard

    union { void* v; double d; } unpacker;
    unpacker.v = slot.data;
    return unpacker.d;
}

// linear search for double
int tFindDouble(const TableList* list, double target)
{
    if (bIsListEmpty(list)) return -1;
    tNode* current = list->tNodeHead;
    int index = 0;

    while (current != NULL)
    {
        if (current->slot.type == TYPE_DOUBLE)
        {
            union { void* v; double d; } unpacker;
            unpacker.v = current->slot.data;
            if (unpacker.d == target) return index; // found (exact compare, consider epsilon for precision-critical use)
        }
        current = current->next;
        index++;
    }
    return -1; // not found
}

// ============================================================================
// 7. CHAR WRAPPERS (Tag: TYPE_CHAR)
// ============================================================================

// push char to tail
void tPushCharBack(TableList* list, char value)
{
    TableSlot_LL slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_CHAR;
    tPushBack(list, slot);
}

// push char to head
void tPushCharFront(TableList* list, char value)
{
    TableSlot_LL slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_CHAR;
    tPushFront(list, slot);
}

// read char by index
char tGetCharAt(const TableList* list, int index)
{
    TableSlot_LL slot = tGetNodeAt(list, index);
    if (slot.type != TYPE_CHAR) return '\0'; // guard
    return (char)(intptr_t)slot.data;
}

// pop char from head
char tPopCharFront(TableList* list)
{
    TableSlot_LL slot = tPopFront(list);
    if (slot.type != TYPE_CHAR) return '\0'; // guard
    return (char)(intptr_t)slot.data;
}

// pop char from tail
char tPopCharBack(TableList* list)
{
    TableSlot_LL slot = tPopBack(list);
    if (slot.type != TYPE_CHAR) return '\0'; // guard
    return (char)(intptr_t)slot.data;
}

// bulk push variadic chars
void tPushCharsBack(TableList* list, int count, ...)
{
    va_list args;
    va_start(args, count);
    for (int i = 0; i < count; i++)
    {
        tPushCharBack(list, (char)va_arg(args, int)); // chars promote to int through varargs
    }
    va_end(args);
}

// insert char at index
void tInsertCharAt(TableList* list, int index, char value)
{
    TableSlot_LL slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_CHAR;
    tInsertAt(list, index, slot);
}

// remove char at index
char tRemoveCharAt(TableList* list, int index)
{
    TableSlot_LL slot = tRemoveAt(list, index);
    if (slot.type != TYPE_CHAR) return '\0'; // guard
    return (char)(intptr_t)slot.data;
}

// linear search for char
int tFindChar(const TableList* list, char target)
{
    if (bIsListEmpty(list)) return -1;
    tNode* current = list->tNodeHead;
    int index = 0;

    while (current != NULL)
    {
        if (current->slot.type == TYPE_CHAR)
        {
            if ((char)(intptr_t)current->slot.data == target) return index; // found
        }
        current = current->next;
        index++;
    }
    return -1; // not found
}

// ============================================================================
// 8. LONG WRAPPERS (Tag: TYPE_LONG)
// ============================================================================

// push long to tail
void tPushLongBack(TableList* list, long value)
{
    TableSlot_LL slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_LONG;
    tPushBack(list, slot);
}

// push long to head
void tPushLongFront(TableList* list, long value)
{
    TableSlot_LL slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_LONG;
    tPushFront(list, slot);
}

// read long by index
long tGetLongAt(const TableList* list, int index)
{
    TableSlot_LL slot = tGetNodeAt(list, index);
    if (slot.type != TYPE_LONG) return 0; // guard
    return (long)(intptr_t)slot.data;
}

// pop long from head
long tPopLongFront(TableList* list)
{
    TableSlot_LL slot = tPopFront(list);
    if (slot.type != TYPE_LONG) return 0; // guard
    return (long)(intptr_t)slot.data;
}

// pop long from tail
long tPopLongBack(TableList* list)
{
    TableSlot_LL slot = tPopBack(list);
    if (slot.type != TYPE_LONG) return 0; // guard
    return (long)(intptr_t)slot.data;
}

// insert long at index
void tInsertLongAt(TableList* list, int index, long value)
{
    TableSlot_LL slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_LONG;
    tInsertAt(list, index, slot);
}

// remove long at index
long tRemoveLongAt(TableList* list, int index)
{
    TableSlot_LL slot = tRemoveAt(list, index);
    if (slot.type != TYPE_LONG) return 0; // guard
    return (long)(intptr_t)slot.data;
}

// linear search for long
int tFindLong(const TableList* list, long target)
{
    if (bIsListEmpty(list)) return -1;
    tNode* current = list->tNodeHead;
    int index = 0;

    while (current != NULL)
    {
        if (current->slot.type == TYPE_LONG)
        {
            if ((long)(intptr_t)current->slot.data == target) return index; // found
        }
        current = current->next;
        index++;
    }
    return -1; // not found
}

// ============================================================================
// 9. SHORT WRAPPERS (Tag: TYPE_SHORT)
// ============================================================================

// push short to tail
void tPushShortBack(TableList* list, short value)
{
    TableSlot_LL slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_SHORT;
    tPushBack(list, slot);
}

// push short to head
void tPushShortFront(TableList* list, short value)
{
    TableSlot_LL slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_SHORT;
    tPushFront(list, slot);
}

// read short by index
short tGetShortAt(const TableList* list, int index)
{
    TableSlot_LL slot = tGetNodeAt(list, index);
    if (slot.type != TYPE_SHORT) return 0; // guard
    return (short)(intptr_t)slot.data;
}

// pop short from head
short tPopShortFront(TableList* list)
{
    TableSlot_LL slot = tPopFront(list);
    if (slot.type != TYPE_SHORT) return 0; // guard
    return (short)(intptr_t)slot.data;
}

// pop short from tail
short tPopShortBack(TableList* list)
{
    TableSlot_LL slot = tPopBack(list);
    if (slot.type != TYPE_SHORT) return 0; // guard
    return (short)(intptr_t)slot.data;
}

// insert short at index
void tInsertShortAt(TableList* list, int index, short value)
{
    TableSlot_LL slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_SHORT;
    tInsertAt(list, index, slot);
}

// remove short at index
short tRemoveShortAt(TableList* list, int index)
{
    TableSlot_LL slot = tRemoveAt(list, index);
    if (slot.type != TYPE_SHORT) return 0; // guard
    return (short)(intptr_t)slot.data;
}

// linear search for short
int tFindShort(const TableList* list, short target)
{
    if (bIsListEmpty(list)) return -1;
    tNode* current = list->tNodeHead;
    int index = 0;

    while (current != NULL)
    {
        if (current->slot.type == TYPE_SHORT)
        {
            if ((short)(intptr_t)current->slot.data == target) return index; // found
        }
        current = current->next;
        index++;
    }
    return -1; // not found
}

// ============================================================================
// 4b. TYPE-CHECKED PRIMITIVE GETTERS - safe alternatives to the plain get
//     functions above. Each validates the stored type tag before handing back
//     a value; on an out-of-range index or a type mismatch, returns false and
//     leaves *out untouched instead of silently reinterpreting the wrong bytes
//     (or returning an indistinguishable 0/0.0/'\0' fallback like the plain
//     getters do).
// ============================================================================

bool bTryGetIntAt(const TableList* list, int index, int* out)
{
    if (out == NULL) return false;
    TableSlot_LL slot = tGetNodeAt(list, index);
    if (slot.type != TYPE_INT) return false;
    *out = (int)(intptr_t)slot.data;
    return true;
}

bool bTryGetFloatAt(const TableList* list, int index, float* out)
{
    if (out == NULL) return false;
    TableSlot_LL slot = tGetNodeAt(list, index);
    if (slot.type != TYPE_FLOAT) return false;
    union { void* v; float f; } unpacker;
    unpacker.v = slot.data;
    *out = unpacker.f;
    return true;
}

bool bTryGetDoubleAt(const TableList* list, int index, double* out)
{
    if (out == NULL) return false;
    TableSlot_LL slot = tGetNodeAt(list, index);
    if (slot.type != TYPE_DOUBLE) return false;
    union { void* v; double d; } unpacker;
    unpacker.v = slot.data;
    *out = unpacker.d;
    return true;
}

bool bTryGetCharAt(const TableList* list, int index, char* out)
{
    if (out == NULL) return false;
    TableSlot_LL slot = tGetNodeAt(list, index);
    if (slot.type != TYPE_CHAR) return false;
    *out = (char)(intptr_t)slot.data;
    return true;
}

bool bTryGetLongAt(const TableList* list, int index, long* out)
{
    if (out == NULL) return false;
    TableSlot_LL slot = tGetNodeAt(list, index);
    if (slot.type != TYPE_LONG) return false;
    *out = (long)(intptr_t)slot.data;
    return true;
}

bool bTryGetShortAt(const TableList* list, int index, short* out)
{
    if (out == NULL) return false;
    TableSlot_LL slot = tGetNodeAt(list, index);
    if (slot.type != TYPE_SHORT) return false;
    *out = (short)(intptr_t)slot.data;
    return true;
}

// ============================================================================
// 10. GENERIC POINTER WRAPPERS (Tags >= TYPE_NULL)
// ============================================================================

// push pointer to tail
void tPushPointerBack(TableList* list, void* ptr, short pointer_tag)
{
    TableSlot_LL slot;
    slot.data = ptr;          // address fits perfectly in void*
    slot.type = pointer_tag;
    tPushBack(list, slot);
}

// push pointer to head
void tPushPointerFront(TableList* list, void* ptr, short pointer_tag)
{
    TableSlot_LL slot;
    slot.data = ptr;
    slot.type = pointer_tag;
    tPushFront(list, slot);
}

// read pointer by index
void* tGetPointerAt(const TableList* list, int index)
{
    TableSlot_LL slot = tGetNodeAt(list, index);
    if (slot.type < TYPE_NULL) return NULL; // reject primitives
    return slot.data;
}

// pop pointer from head
void* tPopPointerFront(TableList* list)
{
    TableSlot_LL slot = tPopFront(list);
    if (slot.type < TYPE_NULL) return NULL; // reject primitives
    return slot.data;
}

// pop pointer from tail
void* tPopPointerBack(TableList* list)
{
    TableSlot_LL slot = tPopBack(list);
    if (slot.type < TYPE_NULL) return NULL; // reject primitives
    return slot.data;
}

// insert pointer at index
void tInsertPointerAt(TableList* list, int index, void* ptr, short pointer_tag)
{
    TableSlot_LL slot;
    slot.data = ptr;
    slot.type = pointer_tag;
    tInsertAt(list, index, slot);
}

// remove pointer at index
void* tRemovePointerAt(TableList* list, int index)
{
    TableSlot_LL slot = tRemoveAt(list, index);
    if (slot.type < TYPE_NULL) return NULL; // reject primitives
    return slot.data;
}

// linear search for pointer
int tFindPointer(const TableList* list, void* target_ptr)
{
    if (bIsListEmpty(list)) return -1;
    tNode* current = list->tNodeHead;
    int index = 0;

    while (current != NULL)
    {
        if (current->slot.type >= TYPE_NULL && current->slot.data == target_ptr)
        {
            return index; // found
        }
        current = current->next;
        index++;
    }
    return -1; // not found
}

// ============================================================================
// 11. STRUCT / UNION WRAPPERS (Tags: TYPE_STRUCT, TYPE_UNION)
// ============================================================================

// push struct pointer to tail
void tPushStructBack(TableList* list, void* struct_ptr)
{
    TableSlot_LL slot;
    slot.data = struct_ptr;
    slot.type = TYPE_STRUCT;
    tPushBack(list, slot);
}

// push struct pointer to head
void tPushStructFront(TableList* list, void* struct_ptr)
{
    TableSlot_LL slot;
    slot.data = struct_ptr;
    slot.type = TYPE_STRUCT;
    tPushFront(list, slot);
}

// read struct pointer by index
void* tGetStructAt(const TableList* list, int index)
{
    TableSlot_LL slot = tGetNodeAt(list, index);
    if (slot.type != TYPE_STRUCT) return NULL; // guard
    return slot.data;
}

// pop struct pointer from head
void* tPopStructFront(TableList* list)
{
    TableSlot_LL slot = tPopFront(list);
    if (slot.type != TYPE_STRUCT) return NULL; // guard
    return slot.data;
}

// pop struct pointer from tail
void* tPopStructBack(TableList* list)
{
    TableSlot_LL slot = tPopBack(list);
    if (slot.type != TYPE_STRUCT) return NULL; // guard
    return slot.data;
}

// insert struct pointer at index
void tInsertStructAt(TableList* list, int index, void* struct_ptr)
{
    TableSlot_LL slot;
    slot.data = struct_ptr;
    slot.type = TYPE_STRUCT;
    tInsertAt(list, index, slot);
}

// remove struct pointer at index
void* tRemoveStructAt(TableList* list, int index)
{
    TableSlot_LL slot = tRemoveAt(list, index);
    if (slot.type != TYPE_STRUCT) return NULL; // guard
    return slot.data;
}

// linear search for struct pointer
int tFindStruct(const TableList* list, void* target_struct_ptr)
{
    if (bIsListEmpty(list)) return -1;
    tNode* current = list->tNodeHead;
    int index = 0;

    while (current != NULL)
    {
        if (current->slot.type == TYPE_STRUCT && current->slot.data == target_struct_ptr) return index; // found
        current = current->next;
        index++;
    }
    return -1; // not found
}

// push union pointer to tail
void tPushUnionBack(TableList* list, void* union_ptr)
{
    TableSlot_LL slot;
    slot.data = union_ptr;
    slot.type = TYPE_UNION;
    tPushBack(list, slot);
}

// push union pointer to head
void tPushUnionFront(TableList* list, void* union_ptr)
{
    TableSlot_LL slot;
    slot.data = union_ptr;
    slot.type = TYPE_UNION;
    tPushFront(list, slot);
}

// read union pointer by index
void* tGetUnionAt(const TableList* list, int index)
{
    TableSlot_LL slot = tGetNodeAt(list, index);
    if (slot.type != TYPE_UNION) return NULL; // guard
    return slot.data;
}

// pop union pointer from head
void* tPopUnionFront(TableList* list)
{
    TableSlot_LL slot = tPopFront(list);
    if (slot.type != TYPE_UNION) return NULL; // guard
    return slot.data;
}

// pop union pointer from tail
void* tPopUnionBack(TableList* list)
{
    TableSlot_LL slot = tPopBack(list);
    if (slot.type != TYPE_UNION) return NULL; // guard
    return slot.data;
}

// insert union pointer at index
void tInsertUnionAt(TableList* list, int index, void* union_ptr)
{
    TableSlot_LL slot;
    slot.data = union_ptr;
    slot.type = TYPE_UNION;
    tInsertAt(list, index, slot);
}

// remove union pointer at index
void* tRemoveUnionAt(TableList* list, int index)
{
    TableSlot_LL slot = tRemoveAt(list, index);
    if (slot.type != TYPE_UNION) return NULL; // guard
    return slot.data;
}

// linear search for union pointer
int tFindUnion(const TableList* list, void* target_union_ptr)
{
    if (bIsListEmpty(list)) return -1;
    tNode* current = list->tNodeHead;
    int index = 0;

    while (current != NULL)
    {
        if (current->slot.type == TYPE_UNION && current->slot.data == target_union_ptr) return index; // found
        current = current->next;
        index++;
    }
    return -1; // not found
}

// ============================================================================
// 12. SYSTEM RESOURCE WRAPPERS (Tags: TYPE_FUNC_PTR, TYPE_FILE_PTR)
// ============================================================================

// push function pointer to tail
void tPushFuncPtrBack(TableList* list, void* func_ptr)
{
    TableSlot_LL slot;
    slot.data = func_ptr;
    slot.type = TYPE_FUNC_PTR;
    tPushBack(list, slot);
}

// push function pointer to head
void tPushFuncPtrFront(TableList* list, void* func_ptr)
{
    TableSlot_LL slot;
    slot.data = func_ptr;
    slot.type = TYPE_FUNC_PTR;
    tPushFront(list, slot);
}

// read function pointer by index
void* tGetFuncPtrAt(const TableList* list, int index)
{
    TableSlot_LL slot = tGetNodeAt(list, index);
    if (slot.type != TYPE_FUNC_PTR) return NULL; // guard
    return slot.data;
}

// pop function pointer from head
void* tPopFuncPtrFront(TableList* list)
{
    TableSlot_LL slot = tPopFront(list);
    if (slot.type != TYPE_FUNC_PTR) return NULL; // guard
    return slot.data;
}

// pop function pointer from tail
void* tPopFuncPtrBack(TableList* list)
{
    TableSlot_LL slot = tPopBack(list);
    if (slot.type != TYPE_FUNC_PTR) return NULL; // guard
    return slot.data;
}

// insert function pointer at index
void tInsertFuncPtrAt(TableList* list, int index, void* func_ptr)
{
    TableSlot_LL slot;
    slot.data = func_ptr;
    slot.type = TYPE_FUNC_PTR;
    tInsertAt(list, index, slot);
}

// remove function pointer at index
void* tRemoveFuncPtrAt(TableList* list, int index)
{
    TableSlot_LL slot = tRemoveAt(list, index);
    if (slot.type != TYPE_FUNC_PTR) return NULL; // guard
    return slot.data;
}

// linear search for function pointer
int tFindFuncPtr(const TableList* list, void* target_func_ptr)
{
    if (bIsListEmpty(list)) return -1;
    tNode* current = list->tNodeHead;
    int index = 0;

    while (current != NULL)
    {
        if (current->slot.type == TYPE_FUNC_PTR && current->slot.data == target_func_ptr) return index; // found
        current = current->next;
        index++;
    }
    return -1; // not found
}

// push FILE* to tail
void tPushFilePtrBack(TableList* list, FILE* file_stream)
{
    TableSlot_LL slot;
    slot.data = (void*)file_stream;
    slot.type = TYPE_FILE_PTR;
    tPushBack(list, slot);
}

// push FILE* to head
void tPushFilePtrFront(TableList* list, FILE* file_stream)
{
    TableSlot_LL slot;
    slot.data = (void*)file_stream;
    slot.type = TYPE_FILE_PTR;
    tPushFront(list, slot);
}

// read FILE* by index
FILE* tGetFilePtrAt(const TableList* list, int index)
{
    TableSlot_LL slot = tGetNodeAt(list, index);
    if (slot.type != TYPE_FILE_PTR) return NULL; // guard
    return (FILE*)slot.data;
}

// pop FILE* from head
FILE* tPopFilePtrFront(TableList* list)
{
    TableSlot_LL slot = tPopFront(list);
    if (slot.type != TYPE_FILE_PTR) return NULL; // guard
    return (FILE*)slot.data;
}

// pop FILE* from tail
FILE* tPopFilePtrBack(TableList* list)
{
    TableSlot_LL slot = tPopBack(list);
    if (slot.type != TYPE_FILE_PTR) return NULL; // guard
    return (FILE*)slot.data;
}

// insert FILE* at index
void tInsertFilePtrAt(TableList* list, int index, FILE* file_stream)
{
    TableSlot_LL slot;
    slot.data = (void*)file_stream;
    slot.type = TYPE_FILE_PTR;
    tInsertAt(list, index, slot);
}

// remove FILE* at index
FILE* tRemoveFilePtrAt(TableList* list, int index)
{
    TableSlot_LL slot = tRemoveAt(list, index);
    if (slot.type != TYPE_FILE_PTR) return NULL; // guard
    return (FILE*)slot.data;
}

// linear search for FILE*
int tFindFilePtr(const TableList* list, FILE* target_stream)
{
    if (bIsListEmpty(list)) return -1;
    tNode* current = list->tNodeHead;
    int index = 0;

    while (current != NULL)
    {
        if (current->slot.type == TYPE_FILE_PTR && current->slot.data == (void*)target_stream) return index; // found
        current = current->next;
        index++;
    }
    return -1; // not found
}
/* ============================================================================
   MODULE: LINKED-LIST BASED STACK-ALLOCATABLE TABLE  (StackTableList / TableSlot_STLL)
   Originally: T_LL_STBSD.cpp
   ============================================================================ */


   // ============================================================================
   // 1. CONSTRUCTION / DESTRUCTION
   // ============================================================================

   // initialize an empty list AND thread every pool slot into the free list.
   // This is the only place the free list gets (re)built from scratch.
void tStackFormLinkedTable(StackTableList* list)
{
    if (list == NULL) return; // guard against null container

    // Chain pool[0] -> pool[1] -> ... -> pool[STACK_TABLE_CAPACITY-1] -> NULL
    // via the `next` pointer. This is the same field used for DLL topology
    // once a node is "checked out" -- while a node sits in the free list its
    // `next` means "next free slot", not "next list node".
    for (int i = 0; i < STACK_TABLE_CAPACITY - 1; i++)
    {
        list->pool[i].next = &list->pool[i + 1];
        list->pool[i].prev = NULL;
        list->pool[i].slot.data = NULL;
        list->pool[i].slot.type = TYPE_EMPTY;
    }
    list->pool[STACK_TABLE_CAPACITY - 1].next = NULL; // last slot terminates the free list
    list->pool[STACK_TABLE_CAPACITY - 1].prev = NULL;
    list->pool[STACK_TABLE_CAPACITY - 1].slot.data = NULL;
    list->pool[STACK_TABLE_CAPACITY - 1].slot.type = TYPE_EMPTY;

    list->sFreeListHead = &list->pool[0]; // every slot starts out free
    list->sNodeHead = NULL;  // no active nodes yet
    list->sNodeTail = NULL;  // no active nodes yet
    list->sNodePrev = NULL;
    list->count = 0;    // zero nodes
}

// Claim a node from the pool's free list and populate it. O(1), no heap
// call: this is just popping the head off an intrusive free-list stack.
sNode* tStackCreateNode(StackTableList* list, TableSlot_STLL payload)
{
    if (list == NULL) return NULL; // guard against null container

    if (list->sFreeListHead == NULL)
    {
        // Pool exhausted -- all 100 slots are currently in use.
        printf("[tStackCreateNode] Fatal Error: Stack is full (max %d nodes)!\n", STACK_TABLE_CAPACITY);
        return NULL;
    }

    sNode* newNode = list->sFreeListHead;   // pop the head of the free list
    list->sFreeListHead = newNode->next;    // advance free list

    newNode->slot = payload; // store the payload
    newNode->next = NULL;    // no forward link yet
    newNode->prev = NULL;    // no backward link yet
    return newNode;
}

// Return a checked-out node to the pool's free list. O(1), no heap call.
// This is the pool equivalent of free() used everywhere the malloc-based
// list called free(node) -- always call this instead of touching the pool
// or sFreeListHead directly.
static void vStackReleaseNode(StackTableList* list, sNode* node)
{
    if (list == NULL || node == NULL) return; // guard

    node->slot.data = NULL;   // sterilize the slot so stale data can't leak
    node->slot.type = TYPE_EMPTY;
    node->prev = NULL;
    node->next = list->sFreeListHead; // push back onto the free list
    list->sFreeListHead = node;
}

// "Free" every node in the list -- for the pool-backed stack version this
// means returning every slot to the free list, not calling free(). Since
// tStackFormLinkedTable already does a full, from-scratch rebuild of the
// free list, teardown is just re-running that (after reporting the count).
void tStackDestroyList(StackTableList* list)
{
    if (list == NULL || list->sNodeHead == NULL) return; // nothing to free

    int nodesFreed = list->count; // every active node returns to the pool

    tStackFormLinkedTable(list); // rebuilds the free list and zeroes topology

    printf("[tStackDestroyList] Teardown complete. %d node(s) returned to the pool.\n", nodesFreed); // report
}

// debug-print list contents
void vStackPrintList(const StackTableList* list)
{
    if (bStackIsListEmpty(list))
    {
        printf("=== StackTableList Contents (Count: 0) ===\n"); // empty case
        return;
    }

    printf("=== StackTableList Contents (Count: %d) ===\n", list->count); // header

    sNode* current = list->sNodeHead; // start at head
    int index = 0;                // display index

    while (current != NULL)
    {
        printf("  [Node %02d] Type Tag: %d\n", index, current->slot.type); // print tag
        current = current->next; // walk forward
        index++;                 // bump index
    }

    printf("======================================\n"); // footer
}

// check if list has no nodes
bool bStackIsListEmpty(const StackTableList* list)
{
    return (list == NULL || list->sNodeHead == NULL); // null container or empty chain counts as empty
}

// check if the fixed 100-node pool has no free slots left
bool bStackIsListFull(const StackTableList* list)
{
    return (list == NULL || list->sFreeListHead == NULL || list->count >= STACK_TABLE_CAPACITY);
}

// ============================================================================
// 2. RAW PUSH / POP / PEEK (O(1))
// ============================================================================

// insert raw slot at tail
void tStackPushBack(StackTableList* list, TableSlot_STLL payload)
{
    sNode* newNode = tStackCreateNode(list, payload); // build node
    if (newNode == NULL) return;

    if (list->sNodeHead == NULL)
    {
        list->sNodeHead = newNode; // first node becomes head
        list->sNodeTail = newNode; // and tail
    }
    else
    {
        list->sNodeTail->next = newNode; // link old tail forward
        newNode->prev = list->sNodeTail; // link new node backward
        list->sNodeTail = newNode;       // advance tail
    }

    list->count++; // grow count
}

// insert raw slot at head
void tStackPushFront(StackTableList* list, TableSlot_STLL payload)
{
    sNode* newNode = tStackCreateNode(list, payload); // build node
    if (newNode == NULL) return;

    if (list->sNodeHead == NULL)
    {
        list->sNodeHead = newNode; // first node becomes head
        list->sNodeTail = newNode; // and tail
    }
    else
    {
        list->sNodeHead->prev = newNode; // link old head backward
        newNode->next = list->sNodeHead; // link new node forward
        list->sNodeHead = newNode;       // advance head
    }

    list->count++; // grow count
}

// remove and return head slot
TableSlot_STLL tStackPopFront(StackTableList* list)
{
    if (list->sNodeHead == NULL)
    {
        printf("[tStackPopFront] Error: List is empty.\n"); // guard
        TableSlot_STLL emptySlot = { NULL, 0 };
        return emptySlot;
    }

    sNode* nodeToRemove = list->sNodeHead;              // target node
    TableSlot_STLL extractedPayload = nodeToRemove->slot; // copy out payload

    if (list->sNodeHead == list->sNodeTail)
    {
        list->sNodeHead = NULL; // list becomes empty
        list->sNodeTail = NULL;
    }
    else
    {
        list->sNodeHead = list->sNodeHead->next; // advance head
        list->sNodeHead->prev = NULL;       // sever backward link
    }

    vStackReleaseNode(list, nodeToRemove); // return node to pool
    list->count--;      // shrink count
    return extractedPayload;
}

// remove and return tail slot
TableSlot_STLL tStackPopBack(StackTableList* list)
{
    if (list->sNodeTail == NULL)
    {
        printf("[tStackPopBack] Error: List is empty.\n"); // guard
        TableSlot_STLL emptySlot = { NULL, 0 };
        return emptySlot;
    }

    sNode* nodeToRemove = list->sNodeTail;              // target node
    TableSlot_STLL extractedPayload = nodeToRemove->slot; // copy out payload

    if (list->sNodeHead == list->sNodeTail)
    {
        list->sNodeHead = NULL; // list becomes empty
        list->sNodeTail = NULL;
    }
    else
    {
        list->sNodeTail = list->sNodeTail->prev; // retreat tail
        list->sNodeTail->next = NULL;       // sever forward link
    }

    vStackReleaseNode(list, nodeToRemove); // return node to pool
    list->count--;      // shrink count
    return extractedPayload;
}

// read head slot without removing
TableSlot_STLL sStackPeekFront(const StackTableList* list)
{
    if (bStackIsListEmpty(list))
    {
        printf("[sStackPeekFront] Warning: List is empty.\n"); // guard
        TableSlot_STLL emptySlot = { NULL, 0 };
        return emptySlot;
    }

    return list->sNodeHead->slot; // copy of payload, topology untouched
}

// read tail slot without removing
TableSlot_STLL sStackPeekBack(const StackTableList* list)
{
    if (bStackIsListEmpty(list))
    {
        printf("[sStackPeekBack] Warning: List is empty.\n"); // guard
        TableSlot_STLL emptySlot = { NULL, 0 };
        return emptySlot;
    }

    return list->sNodeTail->slot; // O(1) jump directly to tail
}

// random-access read by index
TableSlot_STLL tStackGetNodeAt(const StackTableList* list, int target_index)
{
    if (list == NULL || list->sNodeHead == NULL)
    {
        printf("[tStackGetNodeAt] Error: List is empty.\n"); // guard
        TableSlot_STLL empty = { NULL, 0 };
        return empty;
    }

    if (target_index < 0 || target_index >= list->count)
    {
        printf("[tStackGetNodeAt] Error: Index out of bounds.\n"); // bounds check
        TableSlot_STLL empty = { NULL, 0 };
        return empty;
    }

    sNode* current = list->sNodeHead; // start at head
    for (int i = 0; i < target_index; i++)
    {
        current = current->next; // walk to target
    }

    return current->slot;
}

// ============================================================================
// 3. TARGETED TOPOLOGY MODIFICATION (generic)
// ============================================================================

// insert raw slot at index
void tStackInsertAt(StackTableList* list, int index, TableSlot_STLL payload)
{
    if (index <= 0)
    {
        tStackPushFront(list, payload); // boundary optimization: front
        return;
    }
    if (index >= list->count)
    {
        tStackPushBack(list, payload); // boundary optimization: back
        return;
    }

    sNode* current = list->sNodeHead; // traverse to target position
    for (int i = 0; i < index; i++)
    {
        current = current->next;
    }

    sNode* newNode = tStackCreateNode(list, payload); // spawn new node
    if (newNode == NULL) return;

    newNode->prev = current->prev; // wire backward link
    newNode->next = current;       // wire forward link

    current->prev->next = newNode; // splice in before current
    current->prev = newNode;       // splice in before current

    list->count++; // grow count
}

// remove raw slot at index
TableSlot_STLL tStackRemoveAt(StackTableList* list, int index)
{
    if (index <= 0) return tStackPopFront(list);            // boundary optimization: front
    if (index >= list->count - 1) return tStackPopBack(list); // boundary optimization: back

    sNode* current = list->sNodeHead; // traverse to target position
    for (int i = 0; i < index; i++)
    {
        current = current->next;
    }

    TableSlot_STLL extractedPayload = current->slot; // copy out payload

    current->prev->next = current->next; // bridge repair
    current->next->prev = current->prev; // bridge repair

    vStackReleaseNode(list, current);  // return node to pool
    list->count--;  // shrink count

    return extractedPayload;
}

// reverse node order in place
void vStackReverseList(StackTableList* list)
{
    if (bStackIsListEmpty(list) || list->count == 1) return; // nothing to reverse

    sNode* current = list->sNodeHead; // start walking from head
    sNode* temp = NULL;          // scratch pointer

    while (current != NULL)
    {
        temp = current->prev;         // save old prev before overwrite
        current->prev = current->next; // flip backward link
        current->next = temp;          // flip forward link
        current = current->prev;       // advance using the (now-flipped) link
    }

    temp = list->sNodeHead;    // swap master head/tail pointers
    list->sNodeHead = list->sNodeTail;
    list->sNodeTail = temp;

    printf("[vStackReverseList] Topology successfully inverted.\n"); // report
}

// ============================================================================
// 4. INT WRAPPERS (Tag: TYPE_INT)
// ============================================================================

// push int to tail
void tStackPushIntBack(StackTableList* list, int value)
{
    TableSlot_STLL slot;
    slot.data = (void*)(intptr_t)value; // cast int directly into pointer cell
    slot.type = TYPE_INT;
    tStackPushBack(list, slot);
}

// push int to head
void tStackPushIntFront(StackTableList* list, int value)
{
    TableSlot_STLL slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_INT;
    tStackPushFront(list, slot);
}

// read int by index
int tStackGetIntAt(const StackTableList* list, int index)
{
    TableSlot_STLL slot = tStackGetNodeAt(list, index);
    if (slot.type != TYPE_INT)
    {
        printf("[tStackGetIntAt] Type mismatch at index %d.\n", index); // guard
        return 0;
    }
    return (int)(intptr_t)slot.data;
}

// pop int from head
int tStackPopIntFront(StackTableList* list)
{
    TableSlot_STLL slot = tStackPopFront(list);
    if (slot.type != TYPE_INT)
    {
        printf("[tStackPopIntFront] Error: Popped slot is not an integer.\n"); // guard
        return 0;
    }
    return (int)(intptr_t)slot.data;
}

// pop int from tail
int tStackPopIntBack(StackTableList* list)
{
    TableSlot_STLL slot = tStackPopBack(list);
    if (slot.type != TYPE_INT)
    {
        printf("[tStackPopIntBack] Error: Popped slot is not an integer.\n"); // guard
        return 0;
    }
    return (int)(intptr_t)slot.data;
}

// bulk push variadic ints
void tStackPushIntListBack(StackTableList* list, int count, ...)
{
    va_list args;
    va_start(args, count);

    for (int i = 0; i < count; i++)
    {
        int val = va_arg(args, int); // pull next int
        tStackPushIntBack(list, val);     // reuse safe wrapper
    }

    va_end(args);
    printf("[tStackPushIntListBack] Bulk-pushed %d int(s).\n", count); // report
}

// insert int at index
void tStackInsertIntAt(StackTableList* list, int index, int value)
{
    TableSlot_STLL slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_INT;
    tStackInsertAt(list, index, slot);
}

// remove int at index
int tStackRemoveIntAt(StackTableList* list, int index)
{
    TableSlot_STLL slot = tStackRemoveAt(list, index);
    if (slot.type != TYPE_INT) return 0; // guard
    return (int)(intptr_t)slot.data;
}

// linear search for int
int tStackFindInt(const StackTableList* list, int target)
{
    if (bStackIsListEmpty(list)) return -1;

    sNode* current = list->sNodeHead; // start at head
    int index = 0;

    while (current != NULL)
    {
        if (current->slot.type == TYPE_INT) // only compare tagged ints
        {
            int val = (int)(intptr_t)current->slot.data;
            if (val == target) return index; // found
        }
        current = current->next;
        index++;
    }
    return -1; // not found
}

// ============================================================================
// 5. FLOAT WRAPPERS (Tag: TYPE_FLOAT)
// ============================================================================

// push float to tail
void tStackPushFloatBack(StackTableList* list, float value)
{
    union { float f; void* v; } packer;
    packer.v = NULL;   // zero out full 64 bits first
    packer.f = value;  // inject the 32-bit float

    TableSlot_STLL slot;
    slot.data = packer.v;
    slot.type = TYPE_FLOAT;
    tStackPushBack(list, slot);
}

// push float to head
void tStackPushFloatFront(StackTableList* list, float value)
{
    union { float f; void* v; } packer;
    packer.v = NULL;
    packer.f = value;

    TableSlot_STLL slot;
    slot.data = packer.v;
    slot.type = TYPE_FLOAT;
    tStackPushFront(list, slot);
}

// read float by index
float tStackGetFloatAt(const StackTableList* list, int index)
{
    TableSlot_STLL slot = tStackGetNodeAt(list, index);
    if (slot.type != TYPE_FLOAT) return 0.0f; // guard

    union { void* v; float f; } unpacker;
    unpacker.v = slot.data;
    return unpacker.f;
}

// pop float from head
float tStackPopFloatFront(StackTableList* list)
{
    TableSlot_STLL slot = tStackPopFront(list);
    if (slot.type != TYPE_FLOAT) return 0.0f; // guard
    union { void* v; float f; } unpacker;
    unpacker.v = slot.data;
    return unpacker.f;
}

// pop float from tail
float tStackPopFloatBack(StackTableList* list)
{
    TableSlot_STLL slot = tStackPopBack(list);
    if (slot.type != TYPE_FLOAT) return 0.0f; // guard
    union { void* v; float f; } unpacker;
    unpacker.v = slot.data;
    return unpacker.f;
}

// insert float at index
void tStackInsertFloatAt(StackTableList* list, int index, float value)
{
    union { float f; void* v; } packer;
    packer.v = NULL;
    packer.f = value;

    TableSlot_STLL slot;
    slot.data = packer.v;
    slot.type = TYPE_FLOAT;
    tStackInsertAt(list, index, slot);
}

// remove float at index
float tStackRemoveFloatAt(StackTableList* list, int index)
{
    TableSlot_STLL slot = tStackRemoveAt(list, index);
    if (slot.type != TYPE_FLOAT) return 0.0f; // guard
    union { void* v; float f; } unpacker;
    unpacker.v = slot.data;
    return unpacker.f;
}

// linear search for float
int tStackFindFloat(const StackTableList* list, float target)
{
    if (bStackIsListEmpty(list)) return -1;
    sNode* current = list->sNodeHead;
    int index = 0;

    while (current != NULL)
    {
        if (current->slot.type == TYPE_FLOAT)
        {
            union { void* v; float f; } unpacker;
            unpacker.v = current->slot.data;
            if (unpacker.f == target) return index; // found (exact compare)
        }
        current = current->next;
        index++;
    }
    return -1; // not found
}

// ============================================================================
// 6. DOUBLE WRAPPERS (Tag: TYPE_DOUBLE)
// ============================================================================

// push double to tail
void tStackPushDoubleBack(StackTableList* list, double value)
{
    union { double d; void* v; } packer;
    packer.d = value;

    TableSlot_STLL slot;
    slot.data = packer.v;
    slot.type = TYPE_DOUBLE;
    tStackPushBack(list, slot);
}

// push double to head
void tStackPushDoubleFront(StackTableList* list, double value)
{
    union { double d; void* v; } packer;
    packer.d = value;

    TableSlot_STLL slot;
    slot.data = packer.v;
    slot.type = TYPE_DOUBLE;
    tStackPushFront(list, slot);
}

// read double by index
double tStackGetDoubleAt(const StackTableList* list, int index)
{
    TableSlot_STLL slot = tStackGetNodeAt(list, index);
    if (slot.type != TYPE_DOUBLE) return 0.0; // guard

    union { void* v; double d; } unpacker;
    unpacker.v = slot.data;
    return unpacker.d;
}

// pop double from head
double tStackPopDoubleFront(StackTableList* list)
{
    TableSlot_STLL slot = tStackPopFront(list);
    if (slot.type != TYPE_DOUBLE) return 0.0; // guard

    union { void* v; double d; } unpacker;
    unpacker.v = slot.data;
    return unpacker.d;
}

// pop double from tail
double tStackPopDoubleBack(StackTableList* list)
{
    TableSlot_STLL slot = tStackPopBack(list);
    if (slot.type != TYPE_DOUBLE) return 0.0; // guard

    union { void* v; double d; } unpacker;
    unpacker.v = slot.data;
    return unpacker.d;
}

// bulk push variadic doubles
void tStackPushDoublesBack(StackTableList* list, int count, ...)
{
    va_list args;
    va_start(args, count);
    for (int i = 0; i < count; i++)
    {
        tStackPushDoubleBack(list, va_arg(args, double)); // pull and push each double
    }
    va_end(args);
}

// insert double at index
void tStackInsertDoubleAt(StackTableList* list, int index, double value)
{
    union { double d; void* v; } packer;
    packer.d = value;

    TableSlot_STLL slot;
    slot.data = packer.v;
    slot.type = TYPE_DOUBLE;
    tStackInsertAt(list, index, slot);
}

// remove double at index
double tStackRemoveDoubleAt(StackTableList* list, int index)
{
    TableSlot_STLL slot = tStackRemoveAt(list, index);
    if (slot.type != TYPE_DOUBLE) return 0.0; // guard

    union { void* v; double d; } unpacker;
    unpacker.v = slot.data;
    return unpacker.d;
}

// linear search for double
int tStackFindDouble(const StackTableList* list, double target)
{
    if (bStackIsListEmpty(list)) return -1;
    sNode* current = list->sNodeHead;
    int index = 0;

    while (current != NULL)
    {
        if (current->slot.type == TYPE_DOUBLE)
        {
            union { void* v; double d; } unpacker;
            unpacker.v = current->slot.data;
            if (unpacker.d == target) return index; // found (exact compare, consider epsilon for precision-critical use)
        }
        current = current->next;
        index++;
    }
    return -1; // not found
}

// ============================================================================
// 7. CHAR WRAPPERS (Tag: TYPE_CHAR)
// ============================================================================

// push char to tail
void tStackPushCharBack(StackTableList* list, char value)
{
    TableSlot_STLL slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_CHAR;
    tStackPushBack(list, slot);
}

// push char to head
void tStackPushCharFront(StackTableList* list, char value)
{
    TableSlot_STLL slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_CHAR;
    tStackPushFront(list, slot);
}

// read char by index
char tStackGetCharAt(const StackTableList* list, int index)
{
    TableSlot_STLL slot = tStackGetNodeAt(list, index);
    if (slot.type != TYPE_CHAR) return '\0'; // guard
    return (char)(intptr_t)slot.data;
}

// pop char from head
char tStackPopCharFront(StackTableList* list)
{
    TableSlot_STLL slot = tStackPopFront(list);
    if (slot.type != TYPE_CHAR) return '\0'; // guard
    return (char)(intptr_t)slot.data;
}

// pop char from tail
char tStackPopCharBack(StackTableList* list)
{
    TableSlot_STLL slot = tStackPopBack(list);
    if (slot.type != TYPE_CHAR) return '\0'; // guard
    return (char)(intptr_t)slot.data;
}

// bulk push variadic chars
void tStackPushCharsBack(StackTableList* list, int count, ...)
{
    va_list args;
    va_start(args, count);
    for (int i = 0; i < count; i++)
    {
        tStackPushCharBack(list, (char)va_arg(args, int)); // chars promote to int through varargs
    }
    va_end(args);
}

// insert char at index
void tStackInsertCharAt(StackTableList* list, int index, char value)
{
    TableSlot_STLL slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_CHAR;
    tStackInsertAt(list, index, slot);
}

// remove char at index
char tStackRemoveCharAt(StackTableList* list, int index)
{
    TableSlot_STLL slot = tStackRemoveAt(list, index);
    if (slot.type != TYPE_CHAR) return '\0'; // guard
    return (char)(intptr_t)slot.data;
}

// linear search for char
int tStackFindChar(const StackTableList* list, char target)
{
    if (bStackIsListEmpty(list)) return -1;
    sNode* current = list->sNodeHead;
    int index = 0;

    while (current != NULL)
    {
        if (current->slot.type == TYPE_CHAR)
        {
            if ((char)(intptr_t)current->slot.data == target) return index; // found
        }
        current = current->next;
        index++;
    }
    return -1; // not found
}

// ============================================================================
// 8. LONG WRAPPERS (Tag: TYPE_LONG)
// ============================================================================

// push long to tail
void tStackPushLongBack(StackTableList* list, long value)
{
    TableSlot_STLL slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_LONG;
    tStackPushBack(list, slot);
}

// push long to head
void tStackPushLongFront(StackTableList* list, long value)
{
    TableSlot_STLL slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_LONG;
    tStackPushFront(list, slot);
}

// read long by index
long tStackGetLongAt(const StackTableList* list, int index)
{
    TableSlot_STLL slot = tStackGetNodeAt(list, index);
    if (slot.type != TYPE_LONG) return 0; // guard
    return (long)(intptr_t)slot.data;
}

// pop long from head
long tStackPopLongFront(StackTableList* list)
{
    TableSlot_STLL slot = tStackPopFront(list);
    if (slot.type != TYPE_LONG) return 0; // guard
    return (long)(intptr_t)slot.data;
}

// pop long from tail
long tStackPopLongBack(StackTableList* list)
{
    TableSlot_STLL slot = tStackPopBack(list);
    if (slot.type != TYPE_LONG) return 0; // guard
    return (long)(intptr_t)slot.data;
}

// insert long at index
void tStackInsertLongAt(StackTableList* list, int index, long value)
{
    TableSlot_STLL slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_LONG;
    tStackInsertAt(list, index, slot);
}

// remove long at index
long tStackRemoveLongAt(StackTableList* list, int index)
{
    TableSlot_STLL slot = tStackRemoveAt(list, index);
    if (slot.type != TYPE_LONG) return 0; // guard
    return (long)(intptr_t)slot.data;
}

// linear search for long
int tStackFindLong(const StackTableList* list, long target)
{
    if (bStackIsListEmpty(list)) return -1;
    sNode* current = list->sNodeHead;
    int index = 0;

    while (current != NULL)
    {
        if (current->slot.type == TYPE_LONG)
        {
            if ((long)(intptr_t)current->slot.data == target) return index; // found
        }
        current = current->next;
        index++;
    }
    return -1; // not found
}

// ============================================================================
// 9. SHORT WRAPPERS (Tag: TYPE_SHORT)
// ============================================================================

// push short to tail
void tStackPushShortBack(StackTableList* list, short value)
{
    TableSlot_STLL slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_SHORT;
    tStackPushBack(list, slot);
}

// push short to head
void tStackPushShortFront(StackTableList* list, short value)
{
    TableSlot_STLL slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_SHORT;
    tStackPushFront(list, slot);
}

// read short by index
short tStackGetShortAt(const StackTableList* list, int index)
{
    TableSlot_STLL slot = tStackGetNodeAt(list, index);
    if (slot.type != TYPE_SHORT) return 0; // guard
    return (short)(intptr_t)slot.data;
}

// pop short from head
short tStackPopShortFront(StackTableList* list)
{
    TableSlot_STLL slot = tStackPopFront(list);
    if (slot.type != TYPE_SHORT) return 0; // guard
    return (short)(intptr_t)slot.data;
}

// pop short from tail
short tStackPopShortBack(StackTableList* list)
{
    TableSlot_STLL slot = tStackPopBack(list);
    if (slot.type != TYPE_SHORT) return 0; // guard
    return (short)(intptr_t)slot.data;
}

// insert short at index
void tStackInsertShortAt(StackTableList* list, int index, short value)
{
    TableSlot_STLL slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_SHORT;
    tStackInsertAt(list, index, slot);
}

// remove short at index
short tStackRemoveShortAt(StackTableList* list, int index)
{
    TableSlot_STLL slot = tStackRemoveAt(list, index);
    if (slot.type != TYPE_SHORT) return 0; // guard
    return (short)(intptr_t)slot.data;
}

// linear search for short
int tStackFindShort(const StackTableList* list, short target)
{
    if (bStackIsListEmpty(list)) return -1;
    sNode* current = list->sNodeHead;
    int index = 0;

    while (current != NULL)
    {
        if (current->slot.type == TYPE_SHORT)
        {
            if ((short)(intptr_t)current->slot.data == target) return index; // found
        }
        current = current->next;
        index++;
    }
    return -1; // not found
}

// ============================================================================
// 9b. TYPE-CHECKED PRIMITIVE GETTERS - safe alternatives to the plain get
//     functions above. Same contract as TableList's bTryGet*At(): false +
//     untouched *out on a bad index or type-tag mismatch, true + written
//     *out on success.
// ============================================================================

bool bStackTryGetIntAt(const StackTableList* list, int index, int* out)
{
    if (out == NULL) return false;
    TableSlot_STLL slot = tStackGetNodeAt(list, index);
    if (slot.type != TYPE_INT) return false;
    *out = (int)(intptr_t)slot.data;
    return true;
}

bool bStackTryGetFloatAt(const StackTableList* list, int index, float* out)
{
    if (out == NULL) return false;
    TableSlot_STLL slot = tStackGetNodeAt(list, index);
    if (slot.type != TYPE_FLOAT) return false;
    union { void* v; float f; } unpacker;
    unpacker.v = slot.data;
    *out = unpacker.f;
    return true;
}

bool bStackTryGetDoubleAt(const StackTableList* list, int index, double* out)
{
    if (out == NULL) return false;
    TableSlot_STLL slot = tStackGetNodeAt(list, index);
    if (slot.type != TYPE_DOUBLE) return false;
    union { void* v; double d; } unpacker;
    unpacker.v = slot.data;
    *out = unpacker.d;
    return true;
}

bool bStackTryGetCharAt(const StackTableList* list, int index, char* out)
{
    if (out == NULL) return false;
    TableSlot_STLL slot = tStackGetNodeAt(list, index);
    if (slot.type != TYPE_CHAR) return false;
    *out = (char)(intptr_t)slot.data;
    return true;
}

bool bStackTryGetLongAt(const StackTableList* list, int index, long* out)
{
    if (out == NULL) return false;
    TableSlot_STLL slot = tStackGetNodeAt(list, index);
    if (slot.type != TYPE_LONG) return false;
    *out = (long)(intptr_t)slot.data;
    return true;
}

bool bStackTryGetShortAt(const StackTableList* list, int index, short* out)
{
    if (out == NULL) return false;
    TableSlot_STLL slot = tStackGetNodeAt(list, index);
    if (slot.type != TYPE_SHORT) return false;
    *out = (short)(intptr_t)slot.data;
    return true;
}

// ============================================================================
// 10. GENERIC POINTER WRAPPERS (Tags >= TYPE_NULL)
// ============================================================================

// push pointer to tail
void tStackPushPointerBack(StackTableList* list, void* ptr, short pointer_tag)
{
    TableSlot_STLL slot;
    slot.data = ptr;          // address fits perfectly in void*
    slot.type = pointer_tag;
    tStackPushBack(list, slot);
}

// push pointer to head
void tStackPushPointerFront(StackTableList* list, void* ptr, short pointer_tag)
{
    TableSlot_STLL slot;
    slot.data = ptr;
    slot.type = pointer_tag;
    tStackPushFront(list, slot);
}

// read pointer by index
void* tStackGetPointerAt(const StackTableList* list, int index)
{
    TableSlot_STLL slot = tStackGetNodeAt(list, index);
    if (slot.type < TYPE_NULL) return NULL; // reject primitives
    return slot.data;
}

// pop pointer from head
void* tStackPopPointerFront(StackTableList* list)
{
    TableSlot_STLL slot = tStackPopFront(list);
    if (slot.type < TYPE_NULL) return NULL; // reject primitives
    return slot.data;
}

// pop pointer from tail
void* tStackPopPointerBack(StackTableList* list)
{
    TableSlot_STLL slot = tStackPopBack(list);
    if (slot.type < TYPE_NULL) return NULL; // reject primitives
    return slot.data;
}

// insert pointer at index
void tStackInsertPointerAt(StackTableList* list, int index, void* ptr, short pointer_tag)
{
    TableSlot_STLL slot;
    slot.data = ptr;
    slot.type = pointer_tag;
    tStackInsertAt(list, index, slot);
}

// remove pointer at index
void* tStackRemovePointerAt(StackTableList* list, int index)
{
    TableSlot_STLL slot = tStackRemoveAt(list, index);
    if (slot.type < TYPE_NULL) return NULL; // reject primitives
    return slot.data;
}

// linear search for pointer
int tStackFindPointer(const StackTableList* list, void* target_ptr)
{
    if (bStackIsListEmpty(list)) return -1;
    sNode* current = list->sNodeHead;
    int index = 0;

    while (current != NULL)
    {
        if (current->slot.type >= TYPE_NULL && current->slot.data == target_ptr)
        {
            return index; // found
        }
        current = current->next;
        index++;
    }
    return -1; // not found
}

// ============================================================================
// 11. STRUCT / UNION WRAPPERS (Tags: TYPE_STRUCT, TYPE_UNION)
// ============================================================================

// push struct pointer to tail
void tStackPushStructBack(StackTableList* list, void* struct_ptr)
{
    TableSlot_STLL slot;
    slot.data = struct_ptr;
    slot.type = TYPE_STRUCT;
    tStackPushBack(list, slot);
}

// push struct pointer to head
void tStackPushStructFront(StackTableList* list, void* struct_ptr)
{
    TableSlot_STLL slot;
    slot.data = struct_ptr;
    slot.type = TYPE_STRUCT;
    tStackPushFront(list, slot);
}

// read struct pointer by index
void* tStackGetStructAt(const StackTableList* list, int index)
{
    TableSlot_STLL slot = tStackGetNodeAt(list, index);
    if (slot.type != TYPE_STRUCT) return NULL; // guard
    return slot.data;
}

// pop struct pointer from head
void* tStackPopStructFront(StackTableList* list)
{
    TableSlot_STLL slot = tStackPopFront(list);
    if (slot.type != TYPE_STRUCT) return NULL; // guard
    return slot.data;
}

// pop struct pointer from tail
void* tStackPopStructBack(StackTableList* list)
{
    TableSlot_STLL slot = tStackPopBack(list);
    if (slot.type != TYPE_STRUCT) return NULL; // guard
    return slot.data;
}

// insert struct pointer at index
void tStackInsertStructAt(StackTableList* list, int index, void* struct_ptr)
{
    TableSlot_STLL slot;
    slot.data = struct_ptr;
    slot.type = TYPE_STRUCT;
    tStackInsertAt(list, index, slot);
}

// remove struct pointer at index
void* tStackRemoveStructAt(StackTableList* list, int index)
{
    TableSlot_STLL slot = tStackRemoveAt(list, index);
    if (slot.type != TYPE_STRUCT) return NULL; // guard
    return slot.data;
}

// linear search for struct pointer
int tStackFindStruct(const StackTableList* list, void* target_struct_ptr)
{
    if (bStackIsListEmpty(list)) return -1;
    sNode* current = list->sNodeHead;
    int index = 0;

    while (current != NULL)
    {
        if (current->slot.type == TYPE_STRUCT && current->slot.data == target_struct_ptr) return index; // found
        current = current->next;
        index++;
    }
    return -1; // not found
}

// push union pointer to tail
void tStackPushUnionBack(StackTableList* list, void* union_ptr)
{
    TableSlot_STLL slot;
    slot.data = union_ptr;
    slot.type = TYPE_UNION;
    tStackPushBack(list, slot);
}

// push union pointer to head
void tStackPushUnionFront(StackTableList* list, void* union_ptr)
{
    TableSlot_STLL slot;
    slot.data = union_ptr;
    slot.type = TYPE_UNION;
    tStackPushFront(list, slot);
}

// read union pointer by index
void* tStackGetUnionAt(const StackTableList* list, int index)
{
    TableSlot_STLL slot = tStackGetNodeAt(list, index);
    if (slot.type != TYPE_UNION) return NULL; // guard
    return slot.data;
}

// pop union pointer from head
void* tStackPopUnionFront(StackTableList* list)
{
    TableSlot_STLL slot = tStackPopFront(list);
    if (slot.type != TYPE_UNION) return NULL; // guard
    return slot.data;
}

// pop union pointer from tail
void* tStackPopUnionBack(StackTableList* list)
{
    TableSlot_STLL slot = tStackPopBack(list);
    if (slot.type != TYPE_UNION) return NULL; // guard
    return slot.data;
}

// insert union pointer at index
void tStackInsertUnionAt(StackTableList* list, int index, void* union_ptr)
{
    TableSlot_STLL slot;
    slot.data = union_ptr;
    slot.type = TYPE_UNION;
    tStackInsertAt(list, index, slot);
}

// remove union pointer at index
void* tStackRemoveUnionAt(StackTableList* list, int index)
{
    TableSlot_STLL slot = tStackRemoveAt(list, index);
    if (slot.type != TYPE_UNION) return NULL; // guard
    return slot.data;
}

// linear search for union pointer
int tStackFindUnion(const StackTableList* list, void* target_union_ptr)
{
    if (bStackIsListEmpty(list)) return -1;
    sNode* current = list->sNodeHead;
    int index = 0;

    while (current != NULL)
    {
        if (current->slot.type == TYPE_UNION && current->slot.data == target_union_ptr) return index; // found
        current = current->next;
        index++;
    }
    return -1; // not found
}

// ============================================================================
// 12. SYSTEM RESOURCE WRAPPERS (Tags: TYPE_FUNC_PTR, TYPE_FILE_PTR)
// ============================================================================

// push function pointer to tail
void tStackPushFuncPtrBack(StackTableList* list, void* func_ptr)
{
    TableSlot_STLL slot;
    slot.data = func_ptr;
    slot.type = TYPE_FUNC_PTR;
    tStackPushBack(list, slot);
}

// push function pointer to head
void tStackPushFuncPtrFront(StackTableList* list, void* func_ptr)
{
    TableSlot_STLL slot;
    slot.data = func_ptr;
    slot.type = TYPE_FUNC_PTR;
    tStackPushFront(list, slot);
}

// read function pointer by index
void* tStackGetFuncPtrAt(const StackTableList* list, int index)
{
    TableSlot_STLL slot = tStackGetNodeAt(list, index);
    if (slot.type != TYPE_FUNC_PTR) return NULL; // guard
    return slot.data;
}

// pop function pointer from head
void* tStackPopFuncPtrFront(StackTableList* list)
{
    TableSlot_STLL slot = tStackPopFront(list);
    if (slot.type != TYPE_FUNC_PTR) return NULL; // guard
    return slot.data;
}

// pop function pointer from tail
void* tStackPopFuncPtrBack(StackTableList* list)
{
    TableSlot_STLL slot = tStackPopBack(list);
    if (slot.type != TYPE_FUNC_PTR) return NULL; // guard
    return slot.data;
}

// insert function pointer at index
void tStackInsertFuncPtrAt(StackTableList* list, int index, void* func_ptr)
{
    TableSlot_STLL slot;
    slot.data = func_ptr;
    slot.type = TYPE_FUNC_PTR;
    tStackInsertAt(list, index, slot);
}

// remove function pointer at index
void* tStackRemoveFuncPtrAt(StackTableList* list, int index)
{
    TableSlot_STLL slot = tStackRemoveAt(list, index);
    if (slot.type != TYPE_FUNC_PTR) return NULL; // guard
    return slot.data;
}

// linear search for function pointer
int tStackFindFuncPtr(const StackTableList* list, void* target_func_ptr)
{
    if (bStackIsListEmpty(list)) return -1;
    sNode* current = list->sNodeHead;
    int index = 0;

    while (current != NULL)
    {
        if (current->slot.type == TYPE_FUNC_PTR && current->slot.data == target_func_ptr) return index; // found
        current = current->next;
        index++;
    }
    return -1; // not found
}

// push FILE* to tail
void tStackPushFilePtrBack(StackTableList* list, FILE* file_stream)
{
    TableSlot_STLL slot;
    slot.data = (void*)file_stream;
    slot.type = TYPE_FILE_PTR;
    tStackPushBack(list, slot);
}

// push FILE* to head
void tStackPushFilePtrFront(StackTableList* list, FILE* file_stream)
{
    TableSlot_STLL slot;
    slot.data = (void*)file_stream;
    slot.type = TYPE_FILE_PTR;
    tStackPushFront(list, slot);
}

// read FILE* by index
FILE* tStackGetFilePtrAt(const StackTableList* list, int index)
{
    TableSlot_STLL slot = tStackGetNodeAt(list, index);
    if (slot.type != TYPE_FILE_PTR) return NULL; // guard
    return (FILE*)slot.data;
}

// pop FILE* from head
FILE* tStackPopFilePtrFront(StackTableList* list)
{
    TableSlot_STLL slot = tStackPopFront(list);
    if (slot.type != TYPE_FILE_PTR) return NULL; // guard
    return (FILE*)slot.data;
}

// pop FILE* from tail
FILE* tStackPopFilePtrBack(StackTableList* list)
{
    TableSlot_STLL slot = tStackPopBack(list);
    if (slot.type != TYPE_FILE_PTR) return NULL; // guard
    return (FILE*)slot.data;
}

// insert FILE* at index
void tStackInsertFilePtrAt(StackTableList* list, int index, FILE* file_stream)
{
    TableSlot_STLL slot;
    slot.data = (void*)file_stream;
    slot.type = TYPE_FILE_PTR;
    tStackInsertAt(list, index, slot);
}

// remove FILE* at index
FILE* tStackRemoveFilePtrAt(StackTableList* list, int index)
{
    TableSlot_STLL slot = tStackRemoveAt(list, index);
    if (slot.type != TYPE_FILE_PTR) return NULL; // guard
    return (FILE*)slot.data;
}

// linear search for FILE*
int tStackFindFilePtr(const StackTableList* list, FILE* target_stream)
{
    if (bStackIsListEmpty(list)) return -1;
    sNode* current = list->sNodeHead;
    int index = 0;

    while (current != NULL)
    {
        if (current->slot.type == TYPE_FILE_PTR && current->slot.data == (void*)target_stream) return index; // found
        current = current->next;
        index++;
    }
    return -1; // not found
}
/* ============================================================================
   MODULE: HASH TABLE  (TableMap / TableSlot_HM)
   Originally: T_HSH.cpp
   ============================================================================ */


extern uint32_t uDefaultPointerHash(void* payload)
{
    uintptr_t x = (uintptr_t)payload;
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return (uint32_t)x;
}

static int iComputeBucketIndex(const TableMap* map, void* payload)
{
    uint32_t h = map->hash_fn ? map->hash_fn(payload) : uDefaultPointerHash(payload);
    return (int)(h % HASH_TABLE_SIZE);
}

extern void vSetHashFnMap(TableMap* map, uint32_t(*hash_fn)(void*))
{
    if (map == NULL)
        return;
    map->hash_fn = hash_fn;
}

static bool bTypeOwnsHeapPtrMap(short type)
{
    switch (type)
    {
    case TYPE_EMPTY:
    case TYPE_FUNC_PTR:
    case TYPE_FILE_PTR:
    case TYPE_STRUCT_PTR:
    case TYPE_UNION_PTR:
        return false;
    default:
        return true;
    }
}

extern void vFormHashMap(TableMap* map)
{
    if (map == NULL)
        return;

    for (int i = 0; i < HASH_TABLE_SIZE; i++)
    {
        map->buckets[i] = INDEX_NULL;
    }

    for (int i = 0; i < HASH_POOL_CAPACITY - 1; i++)
    {
        map->pool[i].next = (uint16_t)(i + 1);
        map->pool[i].prev = INDEX_NULL;
        map->pool[i].ptr = NULL;
        map->pool[i].type = TYPE_EMPTY;
    }

    int last = HASH_POOL_CAPACITY - 1;
    map->pool[last].next = INDEX_NULL;
    map->pool[last].prev = INDEX_NULL;
    map->pool[last].ptr = NULL;
    map->pool[last].type = TYPE_EMPTY;

    map->hFreeListHead = 0;
    map->active_elements = 0;
    map->hash_fn = NULL;

    printf("\n=== Initializing TableMap ===\n");
    printf("[vFormHashMap] Initialization successful. Capacity: %d | Table Size: %d\n\n", HASH_POOL_CAPACITY, HASH_TABLE_SIZE);
}

extern void vDropHashMap(TableMap* map)
{
    if (map == NULL)
        return;

    int counted = 0;
    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < map->active_elements; i++)
    {
        if (map->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;
        if (map->pool[i].ptr != NULL && bTypeOwnsHeapPtrMap(map->pool[i].type))
            free(map->pool[i].ptr);
    }

    vFormHashMap(map);
    printf("[vDropHashMap] TableMap dropped and owned payloads released.\n");
}

static int iInsertRawInternal(TableMap* map, void* payload, short type)
{
    if (map == NULL || map->hFreeListHead == INDEX_NULL || payload == NULL)
        return INT_MIN;

    int bucket_idx = iComputeBucketIndex(map, payload);

    if (map->buckets[bucket_idx] != INDEX_NULL)
    {
        uint16_t curr_idx = map->buckets[bucket_idx];
        do
        {
            if (map->pool[curr_idx].ptr == payload)
            {
                return ~bucket_idx;
            }
            curr_idx = map->pool[curr_idx].next;
        } while (curr_idx != map->buckets[bucket_idx]);
    }

    uint16_t new_idx = map->hFreeListHead;
    map->hFreeListHead = map->pool[new_idx].next;

    map->pool[new_idx].ptr = payload;
    map->pool[new_idx].type = type;
    map->active_elements++;

    if (map->buckets[bucket_idx] == INDEX_NULL)
    {
        map->pool[new_idx].next = new_idx;
        map->pool[new_idx].prev = new_idx;
    }
    else
    {
        uint16_t head_idx = map->buckets[bucket_idx];
        uint16_t tail_idx = map->pool[head_idx].prev;

        map->pool[new_idx].next = head_idx;
        map->pool[new_idx].prev = tail_idx;

        map->pool[head_idx].prev = new_idx;
        map->pool[tail_idx].next = new_idx;
    }

    map->buckets[bucket_idx] = new_idx;

    return bucket_idx;
}

extern int tMapInsert(TableMap* map, void* payload, short type)
{
    return iInsertRawInternal(map, payload, type);
}

extern int tMapInsertInt(TableMap* map, int val, void** out_key)
{
    int* copy = (int*)malloc(sizeof(int));
    if (copy == NULL)
        return INT_MIN;
    *copy = val;

    int result = iInsertRawInternal(map, copy, TYPE_INT);
    if (result == INT_MIN || result < 0)
    {
        free(copy);
        if (out_key != NULL)
            *out_key = NULL;
        return result;
    }

    if (out_key != NULL)
        *out_key = copy;
    return result;
}

extern int tMapInsertFloat(TableMap* map, float val, void** out_key)
{
    float* copy = (float*)malloc(sizeof(float));
    if (copy == NULL)
        return INT_MIN;
    *copy = val;

    int result = iInsertRawInternal(map, copy, TYPE_FLOAT);
    if (result == INT_MIN || result < 0)
    {
        free(copy);
        if (out_key != NULL)
            *out_key = NULL;
        return result;
    }

    if (out_key != NULL)
        *out_key = copy;
    return result;
}

extern int tMapInsertDouble(TableMap* map, double val, void** out_key)
{
    double* copy = (double*)malloc(sizeof(double));
    if (copy == NULL)
        return INT_MIN;
    *copy = val;

    int result = iInsertRawInternal(map, copy, TYPE_DOUBLE);
    if (result == INT_MIN || result < 0)
    {
        free(copy);
        if (out_key != NULL)
            *out_key = NULL;
        return result;
    }

    if (out_key != NULL)
        *out_key = copy;
    return result;
}

extern int tMapInsertLong(TableMap* map, long val, void** out_key)
{
    long* copy = (long*)malloc(sizeof(long));
    if (copy == NULL)
        return INT_MIN;
    *copy = val;

    int result = iInsertRawInternal(map, copy, TYPE_LONG);
    if (result == INT_MIN || result < 0)
    {
        free(copy);
        if (out_key != NULL)
            *out_key = NULL;
        return result;
    }

    if (out_key != NULL)
        *out_key = copy;
    return result;
}

extern int tMapInsertShort(TableMap* map, short val, void** out_key)
{
    short* copy = (short*)malloc(sizeof(short));
    if (copy == NULL)
        return INT_MIN;
    *copy = val;

    int result = iInsertRawInternal(map, copy, TYPE_SHORT);
    if (result == INT_MIN || result < 0)
    {
        free(copy);
        if (out_key != NULL)
            *out_key = NULL;
        return result;
    }

    if (out_key != NULL)
        *out_key = copy;
    return result;
}

extern int tMapInsertChar(TableMap* map, char val, void** out_key)
{
    char* copy = (char*)malloc(sizeof(char));
    if (copy == NULL)
        return INT_MIN;
    *copy = val;

    int result = iInsertRawInternal(map, copy, TYPE_CHAR);
    if (result == INT_MIN || result < 0)
    {
        free(copy);
        if (out_key != NULL)
            *out_key = NULL;
        return result;
    }

    if (out_key != NULL)
        *out_key = copy;
    return result;
}

extern int tMapInsertPtr(TableMap* map, void* val, short pointer_type, void** out_key)
{
    void** wrapper = (void**)malloc(sizeof(void*));
    if (wrapper == NULL)
        return INT_MIN;
    *wrapper = val;

    int result = iInsertRawInternal(map, wrapper, pointer_type);
    if (result == INT_MIN || result < 0)
    {
        free(wrapper);
        if (out_key != NULL)
            *out_key = NULL;
        return result;
    }

    if (out_key != NULL)
        *out_key = wrapper;
    return result;
}

extern int tMapInsertDPtr(TableMap* map, void** val, short double_pointer_type, void** out_key)
{
    void*** wrapper = (void***)malloc(sizeof(void**));
    if (wrapper == NULL)
        return INT_MIN;
    *wrapper = val;

    int result = iInsertRawInternal(map, wrapper, double_pointer_type);
    if (result == INT_MIN || result < 0)
    {
        free(wrapper);
        if (out_key != NULL)
            *out_key = NULL;
        return result;
    }

    if (out_key != NULL)
        *out_key = wrapper;
    return result;
}

extern int tMapInsertTPtr(TableMap* map, void*** val, short triple_pointer_type, void** out_key)
{
    void**** wrapper = (void****)malloc(sizeof(void***));
    if (wrapper == NULL)
        return INT_MIN;
    *wrapper = val;

    int result = iInsertRawInternal(map, wrapper, triple_pointer_type);
    if (result == INT_MIN || result < 0)
    {
        free(wrapper);
        if (out_key != NULL)
            *out_key = NULL;
        return result;
    }

    if (out_key != NULL)
        *out_key = wrapper;
    return result;
}

extern int tMapInsertStruct(TableMap* map, void* struct_data, size_t size, void** out_key)
{
    if (struct_data == NULL || size == 0)
        return INT_MIN;

    void* copy = malloc(size);
    if (copy == NULL)
        return INT_MIN;
    memcpy(copy, struct_data, size);

    int result = iInsertRawInternal(map, copy, TYPE_STRUCT);
    if (result == INT_MIN || result < 0)
    {
        free(copy);
        if (out_key != NULL)
            *out_key = NULL;
        return result;
    }

    if (out_key != NULL)
        *out_key = copy;
    return result;
}

extern int tMapInsertUnion(TableMap* map, void* union_ptr, size_t union_size, void** out_key)
{
    if (union_ptr == NULL || union_size == 0)
        return INT_MIN;

    void* copy = malloc(union_size);
    if (copy == NULL)
        return INT_MIN;
    memcpy(copy, union_ptr, union_size);

    int result = iInsertRawInternal(map, copy, TYPE_UNION);
    if (result == INT_MIN || result < 0)
    {
        free(copy);
        if (out_key != NULL)
            *out_key = NULL;
        return result;
    }

    if (out_key != NULL)
        *out_key = copy;
    return result;
}

extern int tMapInsertFunc(TableMap* map, void* func_ptr, void** out_key)
{
    int result = iInsertRawInternal(map, func_ptr, TYPE_FUNC_PTR);
    if (out_key != NULL)
        *out_key = (result == INT_MIN || result < 0) ? NULL : func_ptr;
    return result;
}

extern int tMapInsertFile(TableMap* map, FILE* file_ptr, void** out_key)
{
    int result = iInsertRawInternal(map, (void*)file_ptr, TYPE_FILE_PTR);
    if (out_key != NULL)
        *out_key = (result == INT_MIN || result < 0) ? NULL : (void*)file_ptr;
    return result;
}

extern bool bMapRemove(TableMap* map, void* payload)
{
    if (map == NULL || payload == NULL)
        return false;

    int bucket_idx = iComputeBucketIndex(map, payload);

    if (map->buckets[bucket_idx] == INDEX_NULL)
        return false;

    uint16_t head_idx = map->buckets[bucket_idx];
    uint16_t curr_idx = head_idx;

    do
    {
        if (map->pool[curr_idx].ptr == payload)
        {
            uint16_t next_idx = map->pool[curr_idx].next;
            uint16_t prev_idx = map->pool[curr_idx].prev;

            if (next_idx == curr_idx)
            {
                map->buckets[bucket_idx] = INDEX_NULL;
            }
            else
            {
                map->pool[prev_idx].next = next_idx;
                map->pool[next_idx].prev = prev_idx;

                if (head_idx == curr_idx)
                    map->buckets[bucket_idx] = next_idx;
            }

            if (bTypeOwnsHeapPtrMap(map->pool[curr_idx].type))
                free(map->pool[curr_idx].ptr);

            map->pool[curr_idx].ptr = NULL;
            map->pool[curr_idx].type = TYPE_EMPTY;
            map->pool[curr_idx].prev = INDEX_NULL;
            map->pool[curr_idx].next = map->hFreeListHead;
            map->hFreeListHead = curr_idx;
            map->active_elements--;

            return true;
        }

        curr_idx = map->pool[curr_idx].next;
    } while (curr_idx != head_idx);

    return false;
}

extern bool tMapContains(TableMap* map, void* payload)
{
    if (map == NULL || payload == NULL)
        return false;

    int bucket_idx = iComputeBucketIndex(map, payload);

    if (map->buckets[bucket_idx] == INDEX_NULL)
        return false;

    uint16_t curr_idx = map->buckets[bucket_idx];
    do
    {
        if (map->pool[curr_idx].ptr == payload)
        {
            return true;
        }
        curr_idx = map->pool[curr_idx].next;
    } while (curr_idx != map->buckets[bucket_idx]);

    return false;
}

extern TableSlot_HM sMapGetNode(const TableMap* map, void* payload)
{
    TableSlot_HM empty = { NULL, INDEX_NULL, INDEX_NULL, TYPE_EMPTY };

    if (map == NULL || payload == NULL)
        return empty;

    int bucket_idx = iComputeBucketIndex(map, payload);

    if (map->buckets[bucket_idx] == INDEX_NULL)
        return empty;

    uint16_t head_idx = map->buckets[bucket_idx];
    uint16_t curr_idx = head_idx;

    do
    {
        if (map->pool[curr_idx].ptr == payload)
            return map->pool[curr_idx];
        curr_idx = map->pool[curr_idx].next;
    } while (curr_idx != head_idx);

    return empty;
}

// Type-checked slot lookup: false (untouched *out) if the payload isn't found
// or its stored type tag doesn't match expected_type; true + copied slot on
// success. TYPE_EMPTY is the map's own "not found" sentinel (see sMapGetNode),
// so a lookup for TYPE_EMPTY itself is treated as "not found" too.
extern bool bTryMapGetTyped(const TableMap* map, void* payload, short expected_type, TableSlot_HM* out)
{
    if (out == NULL) return false;
    TableSlot_HM slot = sMapGetNode(map, payload);
    if (slot.type == TYPE_EMPTY || slot.type != expected_type) return false;
    *out = slot;
    return true;
}

// Public wrapper around the module's private heap-ownership rule, so callers can
// reason about push/drop semantics for a type without reading the .c source.
extern bool bIsOwnedTypeMap(short type)
{
    return bTypeOwnsHeapPtrMap(type);
}

extern void tMapClear(TableMap* map)
{
    if (map == NULL)
        return;
    vDropHashMap(map);
}

extern int iCountMap(const TableMap* map)
{
    return map ? map->active_elements : 0;
}

extern int iCapacityMap(const TableMap* map)
{
    (void)map;
    return HASH_POOL_CAPACITY;
}

extern bool bIsEmptyMap(const TableMap* map)
{
    return (map == NULL) || (map->active_elements == 0);
}

extern bool bIsFullMap(const TableMap* map)
{
    return (map != NULL) && (map->hFreeListHead == INDEX_NULL);
}

extern int iCountOfTypeMap(const TableMap* map, short type)
{
    if (map == NULL)
        return 0;

    int counted = 0;
    int matches = 0;

    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < map->active_elements; i++)
    {
        if (map->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;
        if (map->pool[i].type == type)
            matches++;
    }

    return matches;
}

extern bool bContainsTypeMap(const TableMap* map, short type)
{
    return iFindTypeMap(map, type) != -1;
}

extern int iFindTypeMap(const TableMap* map, short type)
{
    if (map == NULL)
        return -1;

    int counted = 0;

    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < map->active_elements; i++)
    {
        if (map->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;
        if (map->pool[i].type == type)
            return (int)i;
    }

    return -1;
}

extern int iSumIntMap(const TableMap* map)
{
    if (map == NULL)
        return 0;

    int sum = 0;
    int counted = 0;

    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < map->active_elements; i++)
    {
        if (map->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;
        if (map->pool[i].type == TYPE_INT && map->pool[i].ptr != NULL)
            sum += *(int*)map->pool[i].ptr;
    }

    return sum;
}

extern double dSumFloatMap(const TableMap* map)
{
    if (map == NULL)
        return 0.0;

    double sum = 0.0;
    int counted = 0;

    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < map->active_elements; i++)
    {
        if (map->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;
        if (map->pool[i].type == TYPE_FLOAT && map->pool[i].ptr != NULL)
            sum += (double)(*(float*)map->pool[i].ptr);
    }

    return sum;
}

extern bool bMinIntMap(const TableMap* map, int* out)
{
    if (map == NULL || out == NULL)
        return false;

    bool found = false;
    int minimum = 0;
    int counted = 0;

    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < map->active_elements; i++)
    {
        if (map->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;
        if (map->pool[i].type == TYPE_INT && map->pool[i].ptr != NULL)
        {
            int val = *(int*)map->pool[i].ptr;
            if (!found || val < minimum)
            {
                minimum = val;
                found = true;
            }
        }
    }

    if (found)
        *out = minimum;

    return found;
}

extern bool bMaxIntMap(const TableMap* map, int* out)
{
    if (map == NULL || out == NULL)
        return false;

    bool found = false;
    int maximum = 0;
    int counted = 0;

    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < map->active_elements; i++)
    {
        if (map->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;
        if (map->pool[i].type == TYPE_INT && map->pool[i].ptr != NULL)
        {
            int val = *(int*)map->pool[i].ptr;
            if (!found || val > maximum)
            {
                maximum = val;
                found = true;
            }
        }
    }

    if (found)
        *out = maximum;

    return found;
}

extern void vForEachNodeMap(const TableMap* map, TableMapVisitor visitor)
{
    if (map == NULL || visitor == NULL)
        return;

    int counted = 0;

    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < map->active_elements; i++)
    {
        if (map->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;
        visitor(i, map->pool[i]);
    }
}

extern void tMapProcessActive(TableMap* map, TableMapVisitor visitor)
{
    if (map == NULL || visitor == NULL || map->active_elements == 0)
        return;

    int counted = 0;

    for (uint16_t i = 0; i < HASH_POOL_CAPACITY; i++)
    {
        if (counted >= map->active_elements)
            break;

        if (map->pool[i].type != TYPE_EMPTY)
        {
            visitor(i, map->pool[i]);
            counted++;
        }
    }
}

extern double dLoadFactorMap(const TableMap* map)
{
    if (map == NULL)
        return 0.0;
    return (double)map->active_elements / (double)HASH_TABLE_SIZE;
}

extern int iFreeSlotsRemainingMap(const TableMap* map)
{
    if (map == NULL)
        return 0;
    return HASH_POOL_CAPACITY - map->active_elements;
}

extern int iBucketLengthMap(const TableMap* map, int bucket_idx)
{
    if (map == NULL || bucket_idx < 0 || bucket_idx >= HASH_TABLE_SIZE)
        return 0;

    if (map->buckets[bucket_idx] == INDEX_NULL)
        return 0;

    int length = 0;
    uint16_t head_idx = map->buckets[bucket_idx];
    uint16_t curr_idx = head_idx;

    do
    {
        length++;
        curr_idx = map->pool[curr_idx].next;
    } while (curr_idx != head_idx);

    return length;
}

extern int iMaxBucketLengthMap(const TableMap* map)
{
    if (map == NULL)
        return 0;

    int max_len = 0;
    for (int i = 0; i < HASH_TABLE_SIZE; i++)
    {
        int len = iBucketLengthMap(map, i);
        if (len > max_len)
            max_len = len;
    }

    return max_len;
}

extern bool bMapReplaceType(TableMap* map, void* payload, short new_type)
{
    if (map == NULL || payload == NULL)
        return false;

    int bucket_idx = iComputeBucketIndex(map, payload);

    if (map->buckets[bucket_idx] == INDEX_NULL)
        return false;

    uint16_t head_idx = map->buckets[bucket_idx];
    uint16_t curr_idx = head_idx;

    do
    {
        if (map->pool[curr_idx].ptr == payload)
        {
            map->pool[curr_idx].type = new_type;
            return true;
        }
        curr_idx = map->pool[curr_idx].next;
    } while (curr_idx != head_idx);

    return false;
}

extern int iBucketOfMap(const TableMap* map, void* payload)
{
    if (map == NULL || payload == NULL)
        return -1;
    return iComputeBucketIndex(map, payload);
}

extern bool bCloneTableMap(TableMap* dest, const TableMap* src)
{
    if (dest == NULL || src == NULL)
        return false;

    vFormHashMap(dest);

    int counted = 0;
    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < src->active_elements; i++)
    {
        if (src->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;

        if (src->pool[i].ptr == NULL || !bTypeOwnsHeapPtrMap(src->pool[i].type))
        {
            if (iInsertRawInternal(dest, src->pool[i].ptr, src->pool[i].type) == INT_MIN)
                return false;
            continue;
        }

        size_t sz = 0;
        switch (src->pool[i].type)
        {
        case TYPE_INT:    sz = sizeof(int);    break;
        case TYPE_FLOAT:  sz = sizeof(float);  break;
        case TYPE_DOUBLE: sz = sizeof(double); break;
        case TYPE_LONG:   sz = sizeof(long);   break;
        case TYPE_SHORT:  sz = sizeof(short);  break;
        case TYPE_CHAR:   sz = sizeof(char);   break;
        case TYPE_VOID_STAR:
        case TYPE_INT_STAR:
        case TYPE_FLOAT_STAR:
        case TYPE_CHAR_STAR:
        case TYPE_DOUBLE_STAR:
        case TYPE_LONG_STAR:
        case TYPE_SHORT_STAR:
            sz = sizeof(void*);
            break;
        case TYPE_VOID_STAR_DOUBLE:
        case TYPE_INT_STAR_DOUBLE:
        case TYPE_FLOAT_STAR_DOUBLE:
        case TYPE_CHAR_STAR_DOUBLE:
        case TYPE_DOUBLE_STAR_DOUBLE:
        case TYPE_LONG_STAR_DOUBLE:
        case TYPE_SHORT_STAR_DOUBLE:
            sz = sizeof(void**);
            break;
        case TYPE_VOID_STAR_TRIPLE:
        case TYPE_INT_STAR_TRIPLE:
        case TYPE_FLOAT_STAR_TRIPLE:
        case TYPE_CHAR_STAR_TRIPLE:
        case TYPE_DOUBLE_STAR_TRIPLE:
        case TYPE_LONG_STAR_TRIPLE:
        case TYPE_SHORT_STAR_TRIPLE:
            sz = sizeof(void***);
            break;
        case TYPE_STRUCT:
        case TYPE_UNION:
            printf("[bCloneTableMap] Warning: skipped pool index %u - TYPE_STRUCT/TYPE_UNION "
                "size isn't tracked in TableSlot_HM, can't clone safely.\n", i);
            continue;
        default:
            printf("[bCloneTableMap] Warning: skipped pool index %u - unrecognized type tag %d, "
                "can't determine size to clone safely.\n", i, src->pool[i].type);
            continue;
        }

        void* copy = malloc(sz);
        if (copy == NULL)
            return false;
        memcpy(copy, src->pool[i].ptr, sz);

        if (iInsertRawInternal(dest, copy, src->pool[i].type) == INT_MIN)
        {
            free(copy);
            return false;
        }
    }

    return true;
}

extern bool bMapFindFirst(const TableMap* map, short type, TableSlot_HM* out)
{
    if (map == NULL || out == NULL)
        return false;

    int counted = 0;
    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < map->active_elements; i++)
    {
        if (map->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;
        if (map->pool[i].type == type)
        {
            *out = map->pool[i];
            return true;
        }
    }

    return false;
}

extern int iMapKeysOfType(const TableMap* map, short type, void** out_keys, int max)
{
    if (map == NULL || out_keys == NULL || max <= 0)
        return 0;

    int counted = 0;
    int filled = 0;

    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < map->active_elements && filled < max; i++)
    {
        if (map->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;
        if (map->pool[i].type == type)
        {
            out_keys[filled] = map->pool[i].ptr;
            filled++;
        }
    }

    return filled;
}

extern void vRehashStatsMap(const TableMap* map)
{
    if (map == NULL)
    {
        printf("[vRehashStatsMap] Error: TableMap is NULL.\n");
        return;
    }

    int max_len = 0;
    int used_buckets = 0;
    long total_len = 0;

    printf("\n========================================================================\n");
    printf("  TABLEMAP BUCKET HISTOGRAM  |  Buckets: %-3d  |  Active: %-3d\n", HASH_TABLE_SIZE, map->active_elements);
    printf("========================================================================\n");

    for (int i = 0; i < HASH_TABLE_SIZE; i++)
    {
        int len = iBucketLengthMap(map, i);
        if (len == 0)
            continue;

        used_buckets++;
        total_len += len;
        if (len > max_len)
            max_len = len;

        printf(" [bucket %02d] ", i);
        for (int j = 0; j < len; j++)
            printf("#");
        printf(" (%d)\n", len);
    }

    double avg = used_buckets > 0 ? (double)total_len / (double)used_buckets : 0.0;
    printf("------------------------------------------------------------------------\n");
    printf(" Used buckets: %d/%d  |  Longest chain: %d  |  Avg chain (used): %.2f\n", used_buckets, HASH_TABLE_SIZE, max_len, avg);
    printf("------------------------------------------------------------------------\n\n");
}

// Internal helper: does `dest` already contain an owned-type entry of type `type`
// whose payload bytes equal src_ptr's first `sz` bytes? Used by bMapMergeInto to
// dedup owned/copied values by content instead of by pointer identity (a merged
// copy always lives at a new address, so identity comparison can never recognize
// "this value was already merged in").
static bool bMapContainsEqualValue(const TableMap* dest, short type, const void* src_ptr, size_t sz)
{
    if (dest == NULL || src_ptr == NULL)
        return false;

    int counted = 0;
    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < dest->active_elements; i++)
    {
        if (dest->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;
        if (dest->pool[i].type == type && dest->pool[i].ptr != NULL &&
            memcmp(dest->pool[i].ptr, src_ptr, sz) == 0)
        {
            return true;
        }
    }

    return false;
}

extern bool bMapMergeInto(TableMap* dest, const TableMap* src, int* out_merged, int* out_skipped)
{
    if (out_merged != NULL)
        *out_merged = 0;
    if (out_skipped != NULL)
        *out_skipped = 0;

    if (dest == NULL || src == NULL)
        return false;

    int counted = 0;
    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < src->active_elements; i++)
    {
        if (src->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;

        // Non-owned types (func/file ptrs) are inserted by identity, so identity
        // comparison is the correct, and only meaningful, dedup check for them.
        if (src->pool[i].ptr == NULL || !bTypeOwnsHeapPtrMap(src->pool[i].type))
        {
            if (tMapContains(dest, src->pool[i].ptr))
            {
                if (out_skipped != NULL)
                    (*out_skipped)++;
                continue;
            }

            if (iInsertRawInternal(dest, src->pool[i].ptr, src->pool[i].type) == INT_MIN)
                return false;
            if (out_merged != NULL)
                (*out_merged)++;
            continue;
        }

        // Owned types: figure out the payload size up front (needed either way -
        // to check for an existing equal value, and to malloc the copy if we
        // decide to insert one).
        size_t sz = 0;
        switch (src->pool[i].type)
        {
        case TYPE_INT:    sz = sizeof(int);    break;
        case TYPE_FLOAT:  sz = sizeof(float);  break;
        case TYPE_DOUBLE: sz = sizeof(double); break;
        case TYPE_LONG:   sz = sizeof(long);   break;
        case TYPE_SHORT:  sz = sizeof(short);  break;
        case TYPE_CHAR:   sz = sizeof(char);   break;
        case TYPE_VOID_STAR:
        case TYPE_INT_STAR:
        case TYPE_FLOAT_STAR:
        case TYPE_CHAR_STAR:
        case TYPE_DOUBLE_STAR:
        case TYPE_LONG_STAR:
        case TYPE_SHORT_STAR:
            sz = sizeof(void*);
            break;
        case TYPE_VOID_STAR_DOUBLE:
        case TYPE_INT_STAR_DOUBLE:
        case TYPE_FLOAT_STAR_DOUBLE:
        case TYPE_CHAR_STAR_DOUBLE:
        case TYPE_DOUBLE_STAR_DOUBLE:
        case TYPE_LONG_STAR_DOUBLE:
        case TYPE_SHORT_STAR_DOUBLE:
            sz = sizeof(void**);
            break;
        case TYPE_VOID_STAR_TRIPLE:
        case TYPE_INT_STAR_TRIPLE:
        case TYPE_FLOAT_STAR_TRIPLE:
        case TYPE_CHAR_STAR_TRIPLE:
        case TYPE_DOUBLE_STAR_TRIPLE:
        case TYPE_LONG_STAR_TRIPLE:
        case TYPE_SHORT_STAR_TRIPLE:
            sz = sizeof(void***);
            break;
        case TYPE_STRUCT:
        case TYPE_UNION:
            printf("[bMapMergeInto] Warning: skipped pool index %u - TYPE_STRUCT/TYPE_UNION "
                "size isn't tracked in TableSlot_HM, can't merge safely.\n", i);
            if (out_skipped != NULL)
                (*out_skipped)++;
            continue;
        default:
            printf("[bMapMergeInto] Warning: skipped pool index %u - unrecognized type tag %d, "
                "can't determine size to merge safely.\n", i, src->pool[i].type);
            if (out_skipped != NULL)
                (*out_skipped)++;
            continue;
        }

        // Value-based dedup: has an equal value of this type already been merged
        // into dest? (Necessary because every merged owned-type copy lives at a
        // fresh address - pointer identity can never match src's original.)
        if (bMapContainsEqualValue(dest, src->pool[i].type, src->pool[i].ptr, sz))
        {
            if (out_skipped != NULL)
                (*out_skipped)++;
            continue;
        }

        void* copy = malloc(sz);
        if (copy == NULL)
            return false;
        memcpy(copy, src->pool[i].ptr, sz);

        if (iInsertRawInternal(dest, copy, src->pool[i].type) == INT_MIN)
        {
            free(copy);
            return false;
        }

        if (out_merged != NULL)
            (*out_merged)++;
    }

    return true;
}

extern bool bPointerHashIsSuspect(const TableMap* map)
{
    if (map == NULL || map->active_elements < HASH_TABLE_SIZE)
        return false;

    double expected_avg = (double)map->active_elements / (double)HASH_TABLE_SIZE;
    int max_len = iMaxBucketLengthMap(map);

    return (double)max_len > (expected_avg * 3.0) && max_len >= 6;
}

extern const char* sTypeNameMap(short type)
{
    switch (type)
    {
    case TYPE_EMPTY:            return "EMPTY";
    case TYPE_INT:               return "INT";
    case TYPE_FLOAT:             return "FLOAT";
    case TYPE_DOUBLE:            return "DOUBLE";
    case TYPE_CHAR:              return "CHAR";
    case TYPE_LONG:              return "LONG";
    case TYPE_SHORT:             return "SHORT";
    case TYPE_STRUCT:            return "STRUCT";
    case TYPE_UNION:             return "UNION";
    case TYPE_STRUCT_PTR:        return "STRUCT_PTR";
    case TYPE_UNION_PTR:         return "UNION_PTR";
    case TYPE_FUNC_PTR:          return "FUNC_PTR";
    case TYPE_FILE_PTR:          return "FILE_PTR";
    case TYPE_VOID_STAR:         return "VOID*";
    case TYPE_INT_STAR:          return "INT*";
    case TYPE_FLOAT_STAR:        return "FLOAT*";
    case TYPE_CHAR_STAR:         return "CHAR*";
    case TYPE_DOUBLE_STAR:       return "DOUBLE*";
    case TYPE_LONG_STAR:         return "LONG*";
    case TYPE_SHORT_STAR:        return "SHORT*";
    case TYPE_VOID_STAR_DOUBLE:  return "VOID**";
    case TYPE_INT_STAR_DOUBLE:   return "INT**";
    case TYPE_FLOAT_STAR_DOUBLE: return "FLOAT**";
    case TYPE_CHAR_STAR_DOUBLE:  return "CHAR**";
    case TYPE_DOUBLE_STAR_DOUBLE:return "DOUBLE**";
    case TYPE_LONG_STAR_DOUBLE:  return "LONG**";
    case TYPE_SHORT_STAR_DOUBLE: return "SHORT**";
    default:                     return "POINTER/OTHER";
    }
}

extern void vPrintHashMap(const TableMap* map)
{
    if (map == NULL)
    {
        printf("[vPrintHashMap] Error: TableMap is NULL.\n");
        return;
    }

    printf("\n========================================================================\n");
    printf("  TABLEMAP CONTENTS  |  Active: %-3d  |  Capacity: %-3d  |  Buckets: %-3d\n", map->active_elements, HASH_POOL_CAPACITY, HASH_TABLE_SIZE);
    printf("========================================================================\n");

    if (map->active_elements == 0)
    {
        printf("  [Empty TableMap]\n");
        printf("------------------------------------------------------------------------\n\n");
        return;
    }

    int counted = 0;

    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < map->active_elements; i++)
    {
        if (map->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;
        printf(" [pool %04u] type=%-10s ptr=%p next=%u prev=%u\n",
            i, sTypeNameMap(map->pool[i].type), map->pool[i].ptr,
            map->pool[i].next, map->pool[i].prev);
    }

    printf("------------------------------------------------------------------------\n\n");
}