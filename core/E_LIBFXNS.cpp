#include "include/E_DEFS.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

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
extern bool push_int_impl(ElementArray* obj, int val) {
    int* ptr = (int*)malloc(sizeof(int));
    if (!ptr)
        return false;

    *ptr = val;
    Element node = { ptr, TYPE_INT };
    return push_element(obj, node);
}

extern bool push_float_impl(ElementArray* obj, float val) {
    float* ptr = (float*)malloc(sizeof(float));
    if (!ptr)
        return false;

    *ptr = val;
    Element node = { ptr, TYPE_FLOAT };
    return push_element(obj, node);
}

extern bool push_double_impl(ElementArray* obj, double val) {
    double* ptr = (double*)malloc(sizeof(double));
    if (!ptr)
        return false;

    *ptr = val;
    Element node = { ptr, TYPE_DOUBLE };
    return push_element(obj, node);
}

extern bool push_long_impl(ElementArray* obj, long val) {
    long* ptr = (long*)malloc(sizeof(long));
    if (!ptr)
        return false;

    *ptr = val;
    Element node = { ptr, TYPE_LONG };
    return push_element(obj, node);
}

extern bool push_short_impl(ElementArray* obj, short val) {
    short* ptr = (short*)malloc(sizeof(short));
    if (!ptr)
        return false;

    *ptr = val;
    Element node = { ptr, TYPE_SHORT };
    return push_element(obj, node);
}

extern bool push_char_impl(ElementArray* obj, char val) {
    char* ptr = (char*)malloc(sizeof(char));
    if (!ptr)
        return false;

    *ptr = val;
    Element node = { ptr, TYPE_CHAR };
    return push_element(obj, node);
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
    switch (original.type)
    {
        case TYPE_INT:
            new_data = malloc(sizeof(int));
            if (new_data && original.ptr) *(int*)new_data = *(int*)original.ptr;
            printf("[pop_element] Deep-copied TYPE_INT value: %d\n", *(int*)new_data);
            break;
        case TYPE_FLOAT:
            new_data = malloc(sizeof(float));
            if (new_data && original.ptr) *(float*)new_data = *(float*)original.ptr;
            printf("[pop_element] Deep-copied TYPE_FLOAT value: %.2f\n", *(float*)new_data);
            break;
        case TYPE_DOUBLE:
            new_data = malloc(sizeof(double));
            if (new_data && original.ptr) *(double*)new_data = *(double*)original.ptr;
            printf("[pop_element] Deep-copied TYPE_DOUBLE value: %.5f\n", *(double*)new_data);
            break;
        case TYPE_LONG:
            new_data = malloc(sizeof(long));
            if (new_data && original.ptr) *(long*)new_data = *(long*)original.ptr;
            printf("[pop_element] Deep-copied TYPE_LONG value: %ld\n", *(long*)new_data);
            break;
        case TYPE_CHAR:
            new_data = malloc(sizeof(char));
            if (new_data && original.ptr) *(char*)new_data = *(char*)original.ptr;
            printf("[pop_element] Deep-copied TYPE_CHAR value: '%c'\n", *(char*)new_data);
            break;
        case TYPE_SHORT:
            new_data = malloc(sizeof(short));
            if (new_data && original.ptr) *(short*)new_data = *(short*)original.ptr;
            printf("[pop_element] Deep-copied TYPE_SHORT value: %d\n", *(short*)new_data);
            break;
        default:
            printf("[pop_element] Warning: Element present with unknown identifier or EMPTY state.\n");
            break;
    }

   
    if (original.ptr != NULL) {
        free(original.ptr);
        printf("[pop_element] Internal heap payload at slot index %d freed.\n", top_index);
    }

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