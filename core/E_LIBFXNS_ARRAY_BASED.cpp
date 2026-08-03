#include "include/E_DEFS.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>

extern void InitCollection(ElementArray* obj, int total)
{
    printf("\n=== Initializing Collection ===\n");
    if (obj == NULL)
    {
        printf("[InitCollection] Error: Target ElementArray pointer is NULL.\n");
        return;
    }

    *obj = { NULL, 0, 0 };

    obj->capacity = total;
    size_t bytes_to_alloc = sizeof(Element) * obj->capacity;
    printf("[InitCollection] Allocating initial buffer for %d slots (%zu bytes)...\n", obj->capacity, bytes_to_alloc);

    obj->slots = (Element*)malloc(bytes_to_alloc);
    obj->count = 0;

    if (obj->slots == NULL)
    {
        printf("[InitCollection] Error: malloc call failed! Exiting initialization.\n");
        obj->capacity = 0;
        obj->count = 0;
        return;
    }

    // Sanitize initial slots
    for (int i = 0; i < obj->capacity; i++) {
        obj->slots[i].ptr = NULL;
        obj->slots[i].type = TYPE_EMPTY;
    }

    printf("[InitCollection] Collection initialized successfully (Capacity: %d, Active Count: %d).\n\n", obj->capacity, obj->count);
}

extern void PrintCollection(const ElementArray* obj)
{
    if (!obj || !obj->slots) {
        printf("[PrintCollection] Error: Collection is uninitialized or NULL.\n");
        return;
    }

    printf("\n========================================================================\n");
    printf("  COLLECTION CONTENTS  |  Count: %-3d  |  Capacity: %-3d\n", obj->count, obj->capacity);
    printf("========================================================================\n");

    if (obj->count == 0) {
        printf("  [Empty Collection]\n");
        printf("------------------------------------------------------------------------\n\n");
        return;
    }

    for (int i = 0; i < obj->count; i++) {
        short type = obj->slots[i].type;
        void* payload = obj->slots[i].ptr;

        printf(" [%02d] ", i);

        if (!payload) {
            printf("[NULL PAYLOAD]\n");
            continue;
        }

        switch (type) {

            /* ==================== 1. PRIMITIVES ==================== */

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


            /* ==================== 2. SINGLE POINTERS ==================== */

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


                           /* ==================== 3. DOUBLE POINTERS ==================== */

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


                                  /* ==================== 4. TRIPLE POINTERS ==================== */

        case TYPE_INT_STAR_TRIPLE: {
            int*** tp = *(int****)payload;
            if (tp && *tp && **tp)
                printf("[TYPE_INT_STAR_TRIPLE] \t Addr  : %p -> Val: %d\n", (void*)tp, ***tp);
            else
                printf("[TYPE_INT_STAR_TRIPLE] \t Addr  : %p (Unresolvable)\n", (void*)tp);
            break;
        }


                                 /* ==================== 5. USER DEFINED & COMPLEX TYPES ==================== */

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


            /* ==================== DEFAULT / UNKNOWN ==================== */

        default:
            printf("[TYPE_UNKNOWN (%d)]  \t  Addr  : %p\n", type, payload);
            break;
        }
    }

    printf("------------------------------------------------------------------------\n\n");
}

extern bool push_element(ElementArray* obj, Element cobj) {
    if (obj == NULL || obj->slots == NULL) {
        printf("[push_element] Error: Invalid collection pointer.\n");
        return false;
    }

    if (obj->count == obj->capacity) {
        int new_capacity = obj->capacity + 5;
        size_t new_size_in_bytes = new_capacity * sizeof(Element);

        printf("\n[push_element] Capacity reached (%d). Growing buffer to %d slots (%zu bytes)...\n",
            obj->capacity, new_capacity, new_size_in_bytes);

        Element* new_slots = (Element*)realloc(obj->slots, new_size_in_bytes);
        if (new_slots == NULL) {
            printf("[push_element] Error: realloc failed! Cannot grow array buffer.\n");
            return false;
        }

        for (int i = obj->capacity; i < new_capacity; i++) {
            new_slots[i].ptr = NULL;
            new_slots[i].type = TYPE_EMPTY;
        }

        obj->slots = new_slots;
        obj->capacity = new_capacity;
        printf("[push_element] Buffer successfully grown to %d slots.\n", obj->capacity);
    }

    obj->slots[obj->count] = cobj;
    printf("[push_element] Element pushed successfully at index %d (Type Tag: %d).\n", obj->count, cobj.type);
    obj->count++;

    return true;
}

extern bool push_int_impl(ElementArray* obj, int val) 
{
    int* ptr = (int*)malloc(sizeof(int));
    if (!ptr)
        return false;

    *ptr = val;
    Element node = { ptr, TYPE_INT };
    return push_element(obj, node);
}

extern bool push_float_impl(ElementArray* obj, float val) 
{
    float* ptr = (float*)malloc(sizeof(float));
    if (!ptr)
        return false;

    *ptr = val;
    Element node = { ptr, TYPE_FLOAT };
    return push_element(obj, node);
}

extern bool push_double_impl(ElementArray* obj, double val) 
{
    double* ptr = (double*)malloc(sizeof(double));
    if (!ptr)
        return false;

    *ptr = val;
    Element node = { ptr, TYPE_DOUBLE };
    return push_element(obj, node);
}

extern bool push_long_impl(ElementArray* obj, long val) 
{
    long* ptr = (long*)malloc(sizeof(long));
    if (!ptr)
        return false;

    *ptr = val;
    Element node = { ptr, TYPE_LONG };
    return push_element(obj, node);
}

extern bool push_short_impl(ElementArray* obj, short val) 
{
    short* ptr = (short*)malloc(sizeof(short));
    if (!ptr)
        return false;

    *ptr = val;
    Element node = { ptr, TYPE_SHORT };
    return push_element(obj, node);
}

extern bool push_char_impl(ElementArray* obj, char val) 
{
    char* ptr = (char*)malloc(sizeof(char));
    if (!ptr)
        return false;

    *ptr = val;
    Element node = { ptr, TYPE_CHAR };
    return push_element(obj, node);
}

extern bool push_ptr_impl(ElementArray* obj, void* val, short pointer_type)
{
    void** ptr = (void**)malloc(sizeof(void*));

    if (!ptr)
        return false;

    *ptr = val;

    Element node = { ptr, pointer_type };
    return push_element(obj, node);
}

extern bool push_dptr_impl(ElementArray* obj, void** val, short double_pointer_type)
{
    void*** ptr = (void***)malloc(sizeof(void**));
    if (!ptr)
        return false;

    *ptr = val;

    Element node = { ptr, double_pointer_type };
    return push_element(obj, node);
}

extern bool push_tptr_impl(ElementArray* obj, void*** val, short triple_pointer_type)
{
    void**** ptr = (void****)malloc(sizeof(void***));
    if (!ptr)
        return false;

    *ptr = val;

    Element node = { ptr, triple_pointer_type };
    return push_element(obj, node);
}

extern bool push_struct_impl(ElementArray* obj, void* struct_data, size_t size)
{
    if (!obj || struct_data == NULL || size == 0) {
        return false;
    }

    void* ptr = malloc(size);

    if (!ptr)
    {
        printf("Could not allocate on heap\n");
        return false;
    }

    memcpy(ptr, struct_data, size);

    Element node = { ptr, TYPE_STRUCT };
    return push_element(obj, node);
}

extern bool push_union_impl(ElementArray* obj, void* union_ptr, size_t union_size)
{
    // Step 1: Gate Check
    if (obj == NULL || union_ptr == NULL || union_size == 0) {
        return false;
    }

    // Step 2: Deep Copy the Union Payload
    void* heap_payload = malloc(union_size);
    if (heap_payload == NULL) {
        printf("[push_union] Fatal: Heap allocation failed for union!\n");
        return false;
    }
    memcpy(heap_payload, union_ptr, union_size);

    // Step 3: Tag and Push
    Element node = { heap_payload, TYPE_UNION };
    return push_element(obj, node);
}

extern bool push_func_impl(ElementArray* obj, void* func_ptr)
{
    if (obj == NULL || func_ptr == NULL) {
        return false;
    }

    Element node = { func_ptr, TYPE_FUNC_PTR };

    return push_element(obj, node);
}

extern bool push_file_impl(ElementArray* obj, FILE* file_ptr)
{
    // Step 1: Safety Check
    if (obj == NULL || file_ptr == NULL) {
        return false;
    }

    // Step 2: Direct Assignment (Zero Allocation)
    Element node = { (void*)file_ptr, TYPE_FILE_PTR };

    // Step 3: Push it to the array
    return push_element(obj, node);
}

extern bool PUSH_LIST(ElementArray* obj, int count, ...)
{
    if (!obj || count <= 0)
        return false;

    va_list args;
    va_start(args, count);

    for (int i = 0; i < count; i++) {
        
        short type = (short)va_arg(args, int);

        switch (type) {

            /* --- PRIMITIVES --- */
        case TYPE_INT:
            push_int_impl(obj, va_arg(args, int));
            break;

        case TYPE_FLOAT:
            // Note: float arguments promote to double in variadic functions
            push_float_impl(obj, (float)va_arg(args, double));
            break;

        case TYPE_DOUBLE:
            push_double_impl(obj, va_arg(args, double));
            break;

        case TYPE_CHAR:
            push_char_impl(obj, (char)va_arg(args, int));
            break;

        case TYPE_LONG:
            push_long_impl(obj, va_arg(args, long));
            break;

        case TYPE_SHORT:
            push_short_impl(obj, (short)va_arg(args, int));
            break;


            /* --- SINGLE POINTERS --- */
        case TYPE_INT_STAR:
        case TYPE_FLOAT_STAR:
        case TYPE_CHAR_STAR:
        case TYPE_DOUBLE_STAR:
        case TYPE_LONG_STAR:
        case TYPE_SHORT_STAR:
        case TYPE_VOID_STAR:
            push_ptr_impl(obj, va_arg(args, void*), type);
            break;


            /* --- DOUBLE POINTERS --- */
        case TYPE_INT_STAR_DOUBLE:
        case TYPE_FLOAT_STAR_DOUBLE:
        case TYPE_CHAR_STAR_DOUBLE:
        case TYPE_DOUBLE_STAR_DOUBLE:
        case TYPE_LONG_STAR_DOUBLE:
        case TYPE_SHORT_STAR_DOUBLE:
        case TYPE_VOID_STAR_DOUBLE:
            push_dptr_impl(obj, va_arg(args, void**), type);
            break;


            /* --- TRIPLE POINTERS --- */
        case TYPE_INT_STAR_TRIPLE:
        case TYPE_FLOAT_STAR_TRIPLE:
        case TYPE_CHAR_STAR_TRIPLE:
        case TYPE_DOUBLE_STAR_TRIPLE:
        case TYPE_LONG_STAR_TRIPLE:
        case TYPE_SHORT_STAR_TRIPLE:
        case TYPE_VOID_STAR_TRIPLE:
            push_tptr_impl(obj, va_arg(args, void***), type);
            break;
        case TYPE_STRUCT:
        {
            void* struct_ptr = va_arg(args, void*); // Read the pointer
            size_t struct_size = va_arg(args, size_t); // Read the size
            push_struct_impl(obj, struct_ptr, struct_size);
            break;
        }
        case TYPE_UNION:
        {
            void* union_ptr = va_arg(args, void*);
            size_t union_size = va_arg(args, size_t);
            push_union_impl(obj, union_ptr, union_size);
            break;
        }

        default:
            break;
        }
    }

    va_end(args);
    return true;
}

extern Element pop_element(ElementArray* obj)
{
    printf("\n=== Popping Element ===\n");
    if (obj == NULL || obj->count == 0)
    {
        printf("[pop_element] Underflow Error: Cannot pop from an empty or NULL collection!\n");
        return Element{ NULL, TYPE_EMPTY };
    }

    obj->count--;
    int top_index = obj->count;
    Element original = obj->slots[top_index];

    printf("[pop_element] Extracting top element at index %d (Type Tag: %d)...\n", top_index, original.type);

    void* new_data = NULL;
    bool is_primitive = false; // <-- The shield

    switch (original.type)
    {
        // ==========================================
        // PRIMITIVES (Deep Copy & Free Original)
        // ==========================================
    case TYPE_INT:
        new_data = malloc(sizeof(int));
        if (new_data && original.ptr) *(int*)new_data = *(int*)original.ptr;
        printf("[pop_element] Deep-copied TYPE_INT value: %d\n", *(int*)new_data);
        is_primitive = true;
        break;
    case TYPE_FLOAT:
        new_data = malloc(sizeof(float));
        if (new_data && original.ptr) *(float*)new_data = *(float*)original.ptr;
        printf("[pop_element] Deep-copied TYPE_FLOAT value: %.2f\n", *(float*)new_data);
        is_primitive = true;
        break;
    case TYPE_DOUBLE:
        new_data = malloc(sizeof(double));
        if (new_data && original.ptr) *(double*)new_data = *(double*)original.ptr;
        printf("[pop_element] Deep-copied TYPE_DOUBLE value: %.5f\n", *(double*)new_data);
        is_primitive = true;
        break;
    case TYPE_LONG:
        new_data = malloc(sizeof(long));
        if (new_data && original.ptr) *(long*)new_data = *(long*)original.ptr;
        printf("[pop_element] Deep-copied TYPE_LONG value: %ld\n", *(long*)new_data);
        is_primitive = true;
        break;
    case TYPE_CHAR:
        new_data = malloc(sizeof(char));
        if (new_data && original.ptr) *(char*)new_data = *(char*)original.ptr;
        printf("[pop_element] Deep-copied TYPE_CHAR value: '%c'\n", *(char*)new_data);
        is_primitive = true;
        break;
    case TYPE_SHORT:
        new_data = malloc(sizeof(short));
        if (new_data && original.ptr) *(short*)new_data = *(short*)original.ptr;
        printf("[pop_element] Deep-copied TYPE_SHORT value: %d\n", *(short*)new_data);
        is_primitive = true;
        break;

        // ==========================================
        // SINGLE POINTERS (Pass Raw Pointer)
        // ==========================================
    case TYPE_NULL:
    case TYPE_VOID_STAR:
    case TYPE_INT_STAR:
    case TYPE_FLOAT_STAR:
    case TYPE_CHAR_STAR:
    case TYPE_DOUBLE_STAR:
    case TYPE_LONG_STAR:
    case TYPE_SHORT_STAR:
        new_data = original.ptr;
        printf("[pop_element] Single pointer extracted (Tag: %d). Passing raw pointer.\n", original.type);
        break;

        // ==========================================
        // DOUBLE POINTERS (Pass Raw Pointer)
        // ==========================================
    case TYPE_NULL_DOUBLE:
    case TYPE_VOID_STAR_DOUBLE:
    case TYPE_INT_STAR_DOUBLE:
    case TYPE_FLOAT_STAR_DOUBLE:
    case TYPE_CHAR_STAR_DOUBLE:
    case TYPE_DOUBLE_STAR_DOUBLE:
    case TYPE_LONG_STAR_DOUBLE:
    case TYPE_SHORT_STAR_DOUBLE:
        new_data = original.ptr;
        printf("[pop_element] Double pointer extracted (Tag: %d). Passing raw pointer.\n", original.type);
        break;

        // ==========================================
        // TRIPLE POINTERS (Pass Raw Pointer)
        // ==========================================
    case TYPE_NULL_TRIPLE:
    case TYPE_VOID_STAR_TRIPLE:
    case TYPE_INT_STAR_TRIPLE:
    case TYPE_FLOAT_STAR_TRIPLE:
    case TYPE_CHAR_STAR_TRIPLE:
    case TYPE_DOUBLE_STAR_TRIPLE:
    case TYPE_LONG_STAR_TRIPLE:
    case TYPE_SHORT_STAR_TRIPLE:
        new_data = original.ptr;
        printf("[pop_element] Triple pointer extracted (Tag: %d). Passing raw pointer.\n", original.type);
        break;

        // ==========================================
        // USER DEFINED & COMPLEX TYPES (Pass Raw Pointer)
        // ==========================================
    case TYPE_NULL_USER_DEFINED:
    case TYPE_STRUCT:
    case TYPE_UNION:
    case TYPE_STRUCT_PTR:
    case TYPE_UNION_PTR:
    case TYPE_FUNC_PTR:
    case TYPE_FILE_PTR:
        new_data = original.ptr;
        printf("[pop_element] Complex/User-Defined type extracted (Tag: %d). Passing raw pointer.\n", original.type);
        break;

    case TYPE_EMPTY:
        printf("[pop_element] Warning: Slot is marked as TYPE_EMPTY.\n");
        break;

    default:
        printf("[pop_element] Warning: Element present with unknown identifier or EMPTY state.\n");
        break;
    }

    // Only free if it was a primitive that we successfully deep-copied
    if (is_primitive && original.ptr != NULL) {
        free(original.ptr);
        printf("[pop_element] Internal heap payload at slot index %d freed.\n", top_index);
    }

    // Sanitize the slot so the array forgets about it entirely
    obj->slots[top_index].ptr = NULL;
    obj->slots[top_index].type = TYPE_EMPTY;

    printf("[pop_element] Slot index %d sanitized. Remaining active count: %d\n", top_index, obj->count);

    return Element{ new_data, original.type };
}


extern void DestroyCollection(ElementArray* obj)
{
    // Gate Check
    if (obj == NULL || obj->slots == NULL)
    {
        printf("[DestroyCollection] Warning: NULL pointer provided or array already empty.\n");
        return;
    }

    printf("\n--- Destroying Collection ---\n");
    printf("[DestroyCollection] Deallocating %d active element payload(s)...\n", obj->count);

    // Freeing the insiders first
    for (int i = 0; i < obj->count; i++)
    {
        if (obj->slots[i].ptr != NULL)
        {
            free(obj->slots[i].ptr);
            obj->slots[i].ptr = NULL;
            printf(" -> Freed heap payload at index %d\n", i);
        }
    }

    // Now the big Giant
    free(obj->slots);
    obj->slots = NULL;
    obj->count = 0;
    obj->capacity = 0;

    printf("[DestroyCollection] Main array memory freed and metadata reset to 0.\n");
    printf("--- Collection Successfully Destroyed ---\n\n");
}