#include "include/T_LCL_DEFS.h"
#include <string.h>
#include <stdarg.h>


extern bool bPushSlotStack(TableStack* obj, TableSlot node)
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

    printf("[bPushSlotStack] TableSlot pushed successfully at index %d (Type Tag: %d).\n", obj->count, node.type);
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

    printf("[bPushSlotStack] TableSlot pushed successfully at index %d (Type Tag: %d).\n", obj->count, type);
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
extern TableSlot sPopSlotStack(TableStack* obj)
{
    printf("\n=== Popping Slot (stack) ===\n");

    if (obj == NULL || obj->count == 0)
    {
        printf("[sPopSlotStack] Underflow Error: Cannot pop from an empty or NULL table!\n");
        TableSlot empty = { NULL, TYPE_EMPTY };
        return empty;
    }

    obj->count--;
    int top = obj->count;

    TableSlot out = obj->slots[top];

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
        memset(obj->slots, 0, (size_t)obj->capacity * sizeof(TableSlot));
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
extern TableSlot sGetSlotAtStack(const TableStack* obj, int index)
{
    if (obj == NULL || index < 0 || index >= obj->count)
    {
        printf("[sGetSlotAtStack] Error: index %d out of range (count=%d).\n", index, obj ? obj->count : -1);
        TableSlot empty = { NULL, TYPE_EMPTY };
        return empty;
    }
    return obj->slots[index];
}

/**
 * sGetTypeAtStack - Just the type tag at `index`, without building a full
 * TableSlot. Returns TYPE_EMPTY for an out-of-range index.
 */
extern short sGetTypeAtStack(const TableStack* obj, int index)
{
    if (obj == NULL || index < 0 || index >= obj->count)
        return TYPE_EMPTY;
    return obj->slots[index].type;
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
    TableSlot tmp = obj->slots[a];
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
        TableSlot tmp = obj->slots[lo];
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

    memcpy(dest->slots, src->slots, (size_t)src->count * sizeof(TableSlot));
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
extern void vPrintSlotStack(TableSlot slot)
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