#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>
#include "../LinkedListBased/include/T_LL_DEF.h"

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
tNode* tCreateNode(TableSlot payload)
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
void tPushBack(TableList* list, TableSlot payload)
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
void tPushFront(TableList* list, TableSlot payload)
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
TableSlot tPopFront(TableList* list)
{
    if (list->tNodeHead == NULL)
    {
        printf("[tPopFront] Error: List is empty.\n"); // guard
        TableSlot emptySlot = { NULL, 0 };
        return emptySlot;
    }

    tNode* nodeToRemove = list->tNodeHead;              // target node
    TableSlot extractedPayload = nodeToRemove->slot; // copy out payload

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
TableSlot tPopBack(TableList* list)
{
    if (list->tNodeTail == NULL)
    {
        printf("[tPopBack] Error: List is empty.\n"); // guard
        TableSlot emptySlot = { NULL, 0 };
        return emptySlot;
    }

    tNode* nodeToRemove = list->tNodeTail;              // target node
    TableSlot extractedPayload = nodeToRemove->slot; // copy out payload

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
TableSlot sPeekFront(const TableList* list)
{
    if (bIsListEmpty(list))
    {
        printf("[sPeekFront] Warning: List is empty.\n"); // guard
        TableSlot emptySlot = { NULL, 0 };
        return emptySlot;
    }

    return list->tNodeHead->slot; // copy of payload, topology untouched
}

// read tail slot without removing
TableSlot sPeekBack(const TableList* list)
{
    if (bIsListEmpty(list))
    {
        printf("[sPeekBack] Warning: List is empty.\n"); // guard
        TableSlot emptySlot = { NULL, 0 };
        return emptySlot;
    }

    return list->tNodeTail->slot; // O(1) jump directly to tail
}

// random-access read by index
TableSlot tGetNodeAt(const TableList* list, int target_index)
{
    if (list == NULL || list->tNodeHead == NULL)
    {
        printf("[tGetNodeAt] Error: List is empty.\n"); // guard
        TableSlot empty = { NULL, 0 };
        return empty;
    }

    if (target_index < 0 || target_index >= list->count)
    {
        printf("[tGetNodeAt] Error: Index out of bounds.\n"); // bounds check
        TableSlot empty = { NULL, 0 };
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
void tInsertAt(TableList* list, int index, TableSlot payload)
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
TableSlot tRemoveAt(TableList* list, int index)
{
    if (index <= 0) return tPopFront(list);            // boundary optimization: front
    if (index >= list->count - 1) return tPopBack(list); // boundary optimization: back

    tNode* current = list->tNodeHead; // traverse to target position
    for (int i = 0; i < index; i++)
    {
        current = current->next;
    }

    TableSlot extractedPayload = current->slot; // copy out payload

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
    TableSlot slot;
    slot.data = (void*)(intptr_t)value; // cast int directly into pointer cell
    slot.type = TYPE_INT;
    tPushBack(list, slot);
}

// push int to head
void tPushIntFront(TableList* list, int value)
{
    TableSlot slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_INT;
    tPushFront(list, slot);
}

// read int by index
int tGetIntAt(const TableList* list, int index)
{
    TableSlot slot = tGetNodeAt(list, index);
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
    TableSlot slot = tPopFront(list);
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
    TableSlot slot = tPopBack(list);
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
    TableSlot slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_INT;
    tInsertAt(list, index, slot);
}

// remove int at index
int tRemoveIntAt(TableList* list, int index)
{
    TableSlot slot = tRemoveAt(list, index);
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

    TableSlot slot;
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

    TableSlot slot;
    slot.data = packer.v;
    slot.type = TYPE_FLOAT;
    tPushFront(list, slot);
}

// read float by index
float tGetFloatAt(const TableList* list, int index)
{
    TableSlot slot = tGetNodeAt(list, index);
    if (slot.type != TYPE_FLOAT) return 0.0f; // guard

    union { void* v; float f; } unpacker;
    unpacker.v = slot.data;
    return unpacker.f;
}

// pop float from head
float tPopFloatFront(TableList* list)
{
    TableSlot slot = tPopFront(list);
    if (slot.type != TYPE_FLOAT) return 0.0f; // guard
    union { void* v; float f; } unpacker;
    unpacker.v = slot.data;
    return unpacker.f;
}

// pop float from tail
float tPopFloatBack(TableList* list)
{
    TableSlot slot = tPopBack(list);
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

    TableSlot slot;
    slot.data = packer.v;
    slot.type = TYPE_FLOAT;
    tInsertAt(list, index, slot);
}

// remove float at index
float tRemoveFloatAt(TableList* list, int index)
{
    TableSlot slot = tRemoveAt(list, index);
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

    TableSlot slot;
    slot.data = packer.v;
    slot.type = TYPE_DOUBLE;
    tPushBack(list, slot);
}

// push double to head
void tPushDoubleFront(TableList* list, double value)
{
    union { double d; void* v; } packer;
    packer.d = value;

    TableSlot slot;
    slot.data = packer.v;
    slot.type = TYPE_DOUBLE;
    tPushFront(list, slot);
}

// read double by index
double tGetDoubleAt(const TableList* list, int index)
{
    TableSlot slot = tGetNodeAt(list, index);
    if (slot.type != TYPE_DOUBLE) return 0.0; // guard

    union { void* v; double d; } unpacker;
    unpacker.v = slot.data;
    return unpacker.d;
}

// pop double from head
double tPopDoubleFront(TableList* list)
{
    TableSlot slot = tPopFront(list);
    if (slot.type != TYPE_DOUBLE) return 0.0; // guard

    union { void* v; double d; } unpacker;
    unpacker.v = slot.data;
    return unpacker.d;
}

// pop double from tail
double tPopDoubleBack(TableList* list)
{
    TableSlot slot = tPopBack(list);
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

    TableSlot slot;
    slot.data = packer.v;
    slot.type = TYPE_DOUBLE;
    tInsertAt(list, index, slot);
}

// remove double at index
double tRemoveDoubleAt(TableList* list, int index)
{
    TableSlot slot = tRemoveAt(list, index);
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
    TableSlot slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_CHAR;
    tPushBack(list, slot);
}

// push char to head
void tPushCharFront(TableList* list, char value)
{
    TableSlot slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_CHAR;
    tPushFront(list, slot);
}

// read char by index
char tGetCharAt(const TableList* list, int index)
{
    TableSlot slot = tGetNodeAt(list, index);
    if (slot.type != TYPE_CHAR) return '\0'; // guard
    return (char)(intptr_t)slot.data;
}

// pop char from head
char tPopCharFront(TableList* list)
{
    TableSlot slot = tPopFront(list);
    if (slot.type != TYPE_CHAR) return '\0'; // guard
    return (char)(intptr_t)slot.data;
}

// pop char from tail
char tPopCharBack(TableList* list)
{
    TableSlot slot = tPopBack(list);
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
    TableSlot slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_CHAR;
    tInsertAt(list, index, slot);
}

// remove char at index
char tRemoveCharAt(TableList* list, int index)
{
    TableSlot slot = tRemoveAt(list, index);
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
    TableSlot slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_LONG;
    tPushBack(list, slot);
}

// push long to head
void tPushLongFront(TableList* list, long value)
{
    TableSlot slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_LONG;
    tPushFront(list, slot);
}

// read long by index
long tGetLongAt(const TableList* list, int index)
{
    TableSlot slot = tGetNodeAt(list, index);
    if (slot.type != TYPE_LONG) return 0; // guard
    return (long)(intptr_t)slot.data;
}

// pop long from head
long tPopLongFront(TableList* list)
{
    TableSlot slot = tPopFront(list);
    if (slot.type != TYPE_LONG) return 0; // guard
    return (long)(intptr_t)slot.data;
}

// pop long from tail
long tPopLongBack(TableList* list)
{
    TableSlot slot = tPopBack(list);
    if (slot.type != TYPE_LONG) return 0; // guard
    return (long)(intptr_t)slot.data;
}

// insert long at index
void tInsertLongAt(TableList* list, int index, long value)
{
    TableSlot slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_LONG;
    tInsertAt(list, index, slot);
}

// remove long at index
long tRemoveLongAt(TableList* list, int index)
{
    TableSlot slot = tRemoveAt(list, index);
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
    TableSlot slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_SHORT;
    tPushBack(list, slot);
}

// push short to head
void tPushShortFront(TableList* list, short value)
{
    TableSlot slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_SHORT;
    tPushFront(list, slot);
}

// read short by index
short tGetShortAt(const TableList* list, int index)
{
    TableSlot slot = tGetNodeAt(list, index);
    if (slot.type != TYPE_SHORT) return 0; // guard
    return (short)(intptr_t)slot.data;
}

// pop short from head
short tPopShortFront(TableList* list)
{
    TableSlot slot = tPopFront(list);
    if (slot.type != TYPE_SHORT) return 0; // guard
    return (short)(intptr_t)slot.data;
}

// pop short from tail
short tPopShortBack(TableList* list)
{
    TableSlot slot = tPopBack(list);
    if (slot.type != TYPE_SHORT) return 0; // guard
    return (short)(intptr_t)slot.data;
}

// insert short at index
void tInsertShortAt(TableList* list, int index, short value)
{
    TableSlot slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_SHORT;
    tInsertAt(list, index, slot);
}

// remove short at index
short tRemoveShortAt(TableList* list, int index)
{
    TableSlot slot = tRemoveAt(list, index);
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
// 10. GENERIC POINTER WRAPPERS (Tags >= TYPE_NULL)
// ============================================================================

// push pointer to tail
void tPushPointerBack(TableList* list, void* ptr, short pointer_tag)
{
    TableSlot slot;
    slot.data = ptr;          // address fits perfectly in void*
    slot.type = pointer_tag;
    tPushBack(list, slot);
}

// push pointer to head
void tPushPointerFront(TableList* list, void* ptr, short pointer_tag)
{
    TableSlot slot;
    slot.data = ptr;
    slot.type = pointer_tag;
    tPushFront(list, slot);
}

// read pointer by index
void* tGetPointerAt(const TableList* list, int index)
{
    TableSlot slot = tGetNodeAt(list, index);
    if (slot.type < TYPE_NULL) return NULL; // reject primitives
    return slot.data;
}

// pop pointer from head
void* tPopPointerFront(TableList* list)
{
    TableSlot slot = tPopFront(list);
    if (slot.type < TYPE_NULL) return NULL; // reject primitives
    return slot.data;
}

// pop pointer from tail
void* tPopPointerBack(TableList* list)
{
    TableSlot slot = tPopBack(list);
    if (slot.type < TYPE_NULL) return NULL; // reject primitives
    return slot.data;
}

// insert pointer at index
void tInsertPointerAt(TableList* list, int index, void* ptr, short pointer_tag)
{
    TableSlot slot;
    slot.data = ptr;
    slot.type = pointer_tag;
    tInsertAt(list, index, slot);
}

// remove pointer at index
void* tRemovePointerAt(TableList* list, int index)
{
    TableSlot slot = tRemoveAt(list, index);
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
    TableSlot slot;
    slot.data = struct_ptr;
    slot.type = TYPE_STRUCT;
    tPushBack(list, slot);
}

// push struct pointer to head
void tPushStructFront(TableList* list, void* struct_ptr)
{
    TableSlot slot;
    slot.data = struct_ptr;
    slot.type = TYPE_STRUCT;
    tPushFront(list, slot);
}

// read struct pointer by index
void* tGetStructAt(const TableList* list, int index)
{
    TableSlot slot = tGetNodeAt(list, index);
    if (slot.type != TYPE_STRUCT) return NULL; // guard
    return slot.data;
}

// pop struct pointer from head
void* tPopStructFront(TableList* list)
{
    TableSlot slot = tPopFront(list);
    if (slot.type != TYPE_STRUCT) return NULL; // guard
    return slot.data;
}

// pop struct pointer from tail
void* tPopStructBack(TableList* list)
{
    TableSlot slot = tPopBack(list);
    if (slot.type != TYPE_STRUCT) return NULL; // guard
    return slot.data;
}

// insert struct pointer at index
void tInsertStructAt(TableList* list, int index, void* struct_ptr)
{
    TableSlot slot;
    slot.data = struct_ptr;
    slot.type = TYPE_STRUCT;
    tInsertAt(list, index, slot);
}

// remove struct pointer at index
void* tRemoveStructAt(TableList* list, int index)
{
    TableSlot slot = tRemoveAt(list, index);
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
    TableSlot slot;
    slot.data = union_ptr;
    slot.type = TYPE_UNION;
    tPushBack(list, slot);
}

// push union pointer to head
void tPushUnionFront(TableList* list, void* union_ptr)
{
    TableSlot slot;
    slot.data = union_ptr;
    slot.type = TYPE_UNION;
    tPushFront(list, slot);
}

// read union pointer by index
void* tGetUnionAt(const TableList* list, int index)
{
    TableSlot slot = tGetNodeAt(list, index);
    if (slot.type != TYPE_UNION) return NULL; // guard
    return slot.data;
}

// pop union pointer from head
void* tPopUnionFront(TableList* list)
{
    TableSlot slot = tPopFront(list);
    if (slot.type != TYPE_UNION) return NULL; // guard
    return slot.data;
}

// pop union pointer from tail
void* tPopUnionBack(TableList* list)
{
    TableSlot slot = tPopBack(list);
    if (slot.type != TYPE_UNION) return NULL; // guard
    return slot.data;
}

// insert union pointer at index
void tInsertUnionAt(TableList* list, int index, void* union_ptr)
{
    TableSlot slot;
    slot.data = union_ptr;
    slot.type = TYPE_UNION;
    tInsertAt(list, index, slot);
}

// remove union pointer at index
void* tRemoveUnionAt(TableList* list, int index)
{
    TableSlot slot = tRemoveAt(list, index);
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
    TableSlot slot;
    slot.data = func_ptr;
    slot.type = TYPE_FUNC_PTR;
    tPushBack(list, slot);
}

// push function pointer to head
void tPushFuncPtrFront(TableList* list, void* func_ptr)
{
    TableSlot slot;
    slot.data = func_ptr;
    slot.type = TYPE_FUNC_PTR;
    tPushFront(list, slot);
}

// read function pointer by index
void* tGetFuncPtrAt(const TableList* list, int index)
{
    TableSlot slot = tGetNodeAt(list, index);
    if (slot.type != TYPE_FUNC_PTR) return NULL; // guard
    return slot.data;
}

// pop function pointer from head
void* tPopFuncPtrFront(TableList* list)
{
    TableSlot slot = tPopFront(list);
    if (slot.type != TYPE_FUNC_PTR) return NULL; // guard
    return slot.data;
}

// pop function pointer from tail
void* tPopFuncPtrBack(TableList* list)
{
    TableSlot slot = tPopBack(list);
    if (slot.type != TYPE_FUNC_PTR) return NULL; // guard
    return slot.data;
}

// insert function pointer at index
void tInsertFuncPtrAt(TableList* list, int index, void* func_ptr)
{
    TableSlot slot;
    slot.data = func_ptr;
    slot.type = TYPE_FUNC_PTR;
    tInsertAt(list, index, slot);
}

// remove function pointer at index
void* tRemoveFuncPtrAt(TableList* list, int index)
{
    TableSlot slot = tRemoveAt(list, index);
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
    TableSlot slot;
    slot.data = (void*)file_stream;
    slot.type = TYPE_FILE_PTR;
    tPushBack(list, slot);
}

// push FILE* to head
void tPushFilePtrFront(TableList* list, FILE* file_stream)
{
    TableSlot slot;
    slot.data = (void*)file_stream;
    slot.type = TYPE_FILE_PTR;
    tPushFront(list, slot);
}

// read FILE* by index
FILE* tGetFilePtrAt(const TableList* list, int index)
{
    TableSlot slot = tGetNodeAt(list, index);
    if (slot.type != TYPE_FILE_PTR) return NULL; // guard
    return (FILE*)slot.data;
}

// pop FILE* from head
FILE* tPopFilePtrFront(TableList* list)
{
    TableSlot slot = tPopFront(list);
    if (slot.type != TYPE_FILE_PTR) return NULL; // guard
    return (FILE*)slot.data;
}

// pop FILE* from tail
FILE* tPopFilePtrBack(TableList* list)
{
    TableSlot slot = tPopBack(list);
    if (slot.type != TYPE_FILE_PTR) return NULL; // guard
    return (FILE*)slot.data;
}

// insert FILE* at index
void tInsertFilePtrAt(TableList* list, int index, FILE* file_stream)
{
    TableSlot slot;
    slot.data = (void*)file_stream;
    slot.type = TYPE_FILE_PTR;
    tInsertAt(list, index, slot);
}

// remove FILE* at index
FILE* tRemoveFilePtrAt(TableList* list, int index)
{
    TableSlot slot = tRemoveAt(list, index);
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