#include "../core/include/T_DEFS.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>

// Global instance 
MemoryArena g_MasterArena = { NULL, 0 };

#define ARENA_MIN_SPLIT (2 * sizeof(BoundaryTag) + 8)

extern void* pAllocArena(size_t size)
{
    if (size == 0) return NULL;

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

    size_t slot_bytes = sizeof(TableSlot) * (size_t)obj->capacity;
    obj->slots = (TableSlot*)malloc(slot_bytes);

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
extern bool bPushSlot(Table* obj, TableSlot cobj)
{
    if (obj == NULL)
    {
        printf("[bPushSlot] Error: Invalid collection pointer.\n");
        return false;
    }

    if (obj->count == obj->capacity)
    {
        int new_capacity = (obj->capacity == 0) ? 8 : (obj->capacity * 3) / 2;
        size_t new_size_in_bytes = (size_t)new_capacity * sizeof(TableSlot);

        printf("\n[bPushSlot] Capacity reached (%d). Growing buffer to %d slots (%zu bytes)...\n",
            obj->capacity, new_capacity, new_size_in_bytes);

        TableSlot* new_slots = (TableSlot*)realloc(obj->slots, new_size_in_bytes);
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
    printf("[bPushSlot] TableSlot pushed successfully at index %d (Type Tag: %d).\n", obj->count, cobj.type);
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
    TableSlot node = { ptr, TYPE_INT };
    return bPushSlot(obj, node);
}

extern bool bPushFloat(Table* obj, float val)
{
    float* ptr = (float*)pAllocArena(sizeof(float));
    if (!ptr)
        return false;

    *ptr = val;
    TableSlot node = { ptr, TYPE_FLOAT };
    return bPushSlot(obj, node);
}

extern bool bPushDouble(Table* obj, double val)
{
    double* ptr = (double*)pAllocArena(sizeof(double));
    if (!ptr)
        return false;

    *ptr = val;
    TableSlot node = { ptr, TYPE_DOUBLE };
    return bPushSlot(obj, node);
}

extern bool bPushLong(Table* obj, long val)
{
    long* ptr = (long*)pAllocArena(sizeof(long));
    if (!ptr)
        return false;

    *ptr = val;
    TableSlot node = { ptr, TYPE_LONG };
    return bPushSlot(obj, node);
}

extern bool bPushShort(Table* obj, short val)
{
    short* ptr = (short*)pAllocArena(sizeof(short));
    if (!ptr)
        return false;

    *ptr = val;
    TableSlot node = { ptr, TYPE_SHORT };
    return bPushSlot(obj, node);
}

extern bool bPushChar(Table* obj, char val)
{
    char* ptr = (char*)pAllocArena(sizeof(char));
    if (!ptr)
        return false;

    *ptr = val;
    TableSlot node = { ptr, TYPE_CHAR };
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

    TableSlot node = { ptr, pointer_type };
    return bPushSlot(obj, node);
}

extern bool bPushDPtr(Table* obj, void** val, short double_pointer_type)
{
    void*** ptr = (void***)pAllocArena(sizeof(void**));
    if (!ptr)
        return false;

    *ptr = val;

    TableSlot node = { ptr, double_pointer_type };
    return bPushSlot(obj, node);
}

extern bool bPushTPtr(Table* obj, void*** val, short triple_pointer_type)
{
    void**** ptr = (void****)pAllocArena(sizeof(void***));
    if (!ptr)
        return false;

    *ptr = val;

    TableSlot node = { ptr, triple_pointer_type };
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

    TableSlot node = { ptr, TYPE_STRUCT };
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

    TableSlot node = { heap_payload, TYPE_UNION };
    return bPushSlot(obj, node);
}

// Stores the function pointer AS-IS (zero allocation) - never freed by the Table.
extern bool bPushFunc(Table* obj, void* func_ptr)
{
    if (obj == NULL || func_ptr == NULL)
        return false;

    TableSlot node = { func_ptr, TYPE_FUNC_PTR };
    return bPushSlot(obj, node);
}

// Stores the FILE* handle AS-IS (zero allocation) - Table never closes/frees it.
extern bool bPushFile(Table* obj, FILE* file_ptr)
{
    if (obj == NULL || file_ptr == NULL)
        return false;

    TableSlot node = { (void*)file_ptr, TYPE_FILE_PTR };
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
extern TableSlot sPopSlot(Table* obj)
{
    printf("\n=== Popping TableSlot ===\n");
    if (obj == NULL || obj->count == 0)
    {
        printf("[sPopSlot] Underflow Error: Cannot pop from an empty or NULL collection!\n");
        return TableSlot{ NULL, TYPE_EMPTY };
    }

    obj->count--;
    int top_index = obj->count;
    TableSlot original = obj->slots[top_index];

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
        printf("[sPopSlot] Warning: TableSlot present with unknown identifier or EMPTY state.\n");
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

    return TableSlot{ new_data, original.type };
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

    TableSlot top = obj->slots[obj->count - 1];

    if (!bTypeOwnsHeapPtr(top.type) || top.ptr == NULL)
    {
        TableSlot copy = { top.ptr, top.type };
        return bPushSlot(obj, copy);
    }

    size_t sz;
    if (!bTypeKnownSize(top.type, &sz))
    {
        printf("[bDupTopTable] Error: cannot safely duplicate a TYPE_STRUCT/TYPE_UNION slot - "
            "its size was never recorded (TableSlot only stores ptr+type). "
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

    TableSlot copy = { copy_ptr, top.type };
    return bPushSlot(obj, copy);
}

// Swap the top two slots in place (just swaps the TableSlot structs - no allocation needed).
extern bool bSwapTopTable(Table* obj)
{
    if (obj == NULL || obj->count < 2)
        return false;

    TableSlot tmp = obj->slots[obj->count - 1];
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

// Reverse slot order in place: [A, B, C] -> [C, B, A]. Just swaps TableSlot structs, no allocation.
extern void vReverseTable(Table* obj)
{
    if (obj == NULL || obj->count < 2) return;

    int lo = 0, hi = obj->count - 1;
    while (lo < hi)
    {
        TableSlot tmp = obj->slots[lo];
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
        TableSlot s = src->slots[i];

        if (!bTypeOwnsHeapPtr(s.type) || s.ptr == NULL)
        {
            // Non-owned reference (func/file/struct-ptr/union-ptr) - copy the pointer itself,
            // it was never Table-owned to begin with.
            TableSlot copy = { s.ptr, s.type };
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

        TableSlot copy = { copy_ptr, s.type };
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
extern TableSlot sGetSlotAtTable(const Table* obj, int index)
{
    if (obj == NULL || index < 0 || index >= obj->count)
    {
        printf("[sGetSlotAtTable] Error: index %d out of range (count=%d).\n", index, obj ? obj->count : -1);
        return TableSlot{ NULL, TYPE_EMPTY };
    }
    return obj->slots[index];
}

extern short sGetTypeAtTable(const Table* obj, int index)
{
    if (obj == NULL || index < 0 || index >= obj->count)
        return TYPE_EMPTY;
    return obj->slots[index].type;
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

    size_t new_bytes = (size_t)obj->count * sizeof(TableSlot);
    TableSlot* shrunk = (TableSlot*)realloc(obj->slots, new_bytes);
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
    size_t new_bytes = (size_t)new_capacity * sizeof(TableSlot);

    TableSlot* grown = (TableSlot*)realloc(obj->slots, new_bytes);
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
extern bool bInsertAtTable(Table* obj, int index, TableSlot node)
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

    memmove(&obj->slots[index + 1], &obj->slots[index], (size_t)(obj->count - index) * sizeof(TableSlot));
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

    memmove(&obj->slots[index], &obj->slots[index + 1], (size_t)(obj->count - index - 1) * sizeof(TableSlot));
    obj->count--;

    obj->slots[obj->count].ptr = NULL;
    obj->slots[obj->count].type = TYPE_EMPTY;

    printf("[bRemoveAtTable] Removed index %d. New count: %d\n", index, obj->count);
    return true;
}