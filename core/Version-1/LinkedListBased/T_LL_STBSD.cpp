#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>
#include "../LinkedListBased/include/T_LL_STACK_DEF.h" // adjust path to wherever you place this header in your project

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
sNode* tStackCreateNode(StackTableList* list, TableSlot payload)
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
void tStackPushBack(StackTableList* list, TableSlot payload)
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
void tStackPushFront(StackTableList* list, TableSlot payload)
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
TableSlot tStackPopFront(StackTableList* list)
{
    if (list->sNodeHead == NULL)
    {
        printf("[tStackPopFront] Error: List is empty.\n"); // guard
        TableSlot emptySlot = { NULL, 0 };
        return emptySlot;
    }

    sNode* nodeToRemove = list->sNodeHead;              // target node
    TableSlot extractedPayload = nodeToRemove->slot; // copy out payload

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
TableSlot tStackPopBack(StackTableList* list)
{
    if (list->sNodeTail == NULL)
    {
        printf("[tStackPopBack] Error: List is empty.\n"); // guard
        TableSlot emptySlot = { NULL, 0 };
        return emptySlot;
    }

    sNode* nodeToRemove = list->sNodeTail;              // target node
    TableSlot extractedPayload = nodeToRemove->slot; // copy out payload

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
TableSlot sStackPeekFront(const StackTableList* list)
{
    if (bStackIsListEmpty(list))
    {
        printf("[sStackPeekFront] Warning: List is empty.\n"); // guard
        TableSlot emptySlot = { NULL, 0 };
        return emptySlot;
    }

    return list->sNodeHead->slot; // copy of payload, topology untouched
}

// read tail slot without removing
TableSlot sStackPeekBack(const StackTableList* list)
{
    if (bStackIsListEmpty(list))
    {
        printf("[sStackPeekBack] Warning: List is empty.\n"); // guard
        TableSlot emptySlot = { NULL, 0 };
        return emptySlot;
    }

    return list->sNodeTail->slot; // O(1) jump directly to tail
}

// random-access read by index
TableSlot tStackGetNodeAt(const StackTableList* list, int target_index)
{
    if (list == NULL || list->sNodeHead == NULL)
    {
        printf("[tStackGetNodeAt] Error: List is empty.\n"); // guard
        TableSlot empty = { NULL, 0 };
        return empty;
    }

    if (target_index < 0 || target_index >= list->count)
    {
        printf("[tStackGetNodeAt] Error: Index out of bounds.\n"); // bounds check
        TableSlot empty = { NULL, 0 };
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
void tStackInsertAt(StackTableList* list, int index, TableSlot payload)
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
TableSlot tStackRemoveAt(StackTableList* list, int index)
{
    if (index <= 0) return tStackPopFront(list);            // boundary optimization: front
    if (index >= list->count - 1) return tStackPopBack(list); // boundary optimization: back

    sNode* current = list->sNodeHead; // traverse to target position
    for (int i = 0; i < index; i++)
    {
        current = current->next;
    }

    TableSlot extractedPayload = current->slot; // copy out payload

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
    TableSlot slot;
    slot.data = (void*)(intptr_t)value; // cast int directly into pointer cell
    slot.type = TYPE_INT;
    tStackPushBack(list, slot);
}

// push int to head
void tStackPushIntFront(StackTableList* list, int value)
{
    TableSlot slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_INT;
    tStackPushFront(list, slot);
}

// read int by index
int tStackGetIntAt(const StackTableList* list, int index)
{
    TableSlot slot = tStackGetNodeAt(list, index);
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
    TableSlot slot = tStackPopFront(list);
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
    TableSlot slot = tStackPopBack(list);
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
    TableSlot slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_INT;
    tStackInsertAt(list, index, slot);
}

// remove int at index
int tStackRemoveIntAt(StackTableList* list, int index)
{
    TableSlot slot = tStackRemoveAt(list, index);
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

    TableSlot slot;
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

    TableSlot slot;
    slot.data = packer.v;
    slot.type = TYPE_FLOAT;
    tStackPushFront(list, slot);
}

// read float by index
float tStackGetFloatAt(const StackTableList* list, int index)
{
    TableSlot slot = tStackGetNodeAt(list, index);
    if (slot.type != TYPE_FLOAT) return 0.0f; // guard

    union { void* v; float f; } unpacker;
    unpacker.v = slot.data;
    return unpacker.f;
}

// pop float from head
float tStackPopFloatFront(StackTableList* list)
{
    TableSlot slot = tStackPopFront(list);
    if (slot.type != TYPE_FLOAT) return 0.0f; // guard
    union { void* v; float f; } unpacker;
    unpacker.v = slot.data;
    return unpacker.f;
}

// pop float from tail
float tStackPopFloatBack(StackTableList* list)
{
    TableSlot slot = tStackPopBack(list);
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

    TableSlot slot;
    slot.data = packer.v;
    slot.type = TYPE_FLOAT;
    tStackInsertAt(list, index, slot);
}

// remove float at index
float tStackRemoveFloatAt(StackTableList* list, int index)
{
    TableSlot slot = tStackRemoveAt(list, index);
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

    TableSlot slot;
    slot.data = packer.v;
    slot.type = TYPE_DOUBLE;
    tStackPushBack(list, slot);
}

// push double to head
void tStackPushDoubleFront(StackTableList* list, double value)
{
    union { double d; void* v; } packer;
    packer.d = value;

    TableSlot slot;
    slot.data = packer.v;
    slot.type = TYPE_DOUBLE;
    tStackPushFront(list, slot);
}

// read double by index
double tStackGetDoubleAt(const StackTableList* list, int index)
{
    TableSlot slot = tStackGetNodeAt(list, index);
    if (slot.type != TYPE_DOUBLE) return 0.0; // guard

    union { void* v; double d; } unpacker;
    unpacker.v = slot.data;
    return unpacker.d;
}

// pop double from head
double tStackPopDoubleFront(StackTableList* list)
{
    TableSlot slot = tStackPopFront(list);
    if (slot.type != TYPE_DOUBLE) return 0.0; // guard

    union { void* v; double d; } unpacker;
    unpacker.v = slot.data;
    return unpacker.d;
}

// pop double from tail
double tStackPopDoubleBack(StackTableList* list)
{
    TableSlot slot = tStackPopBack(list);
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

    TableSlot slot;
    slot.data = packer.v;
    slot.type = TYPE_DOUBLE;
    tStackInsertAt(list, index, slot);
}

// remove double at index
double tStackRemoveDoubleAt(StackTableList* list, int index)
{
    TableSlot slot = tStackRemoveAt(list, index);
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
    TableSlot slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_CHAR;
    tStackPushBack(list, slot);
}

// push char to head
void tStackPushCharFront(StackTableList* list, char value)
{
    TableSlot slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_CHAR;
    tStackPushFront(list, slot);
}

// read char by index
char tStackGetCharAt(const StackTableList* list, int index)
{
    TableSlot slot = tStackGetNodeAt(list, index);
    if (slot.type != TYPE_CHAR) return '\0'; // guard
    return (char)(intptr_t)slot.data;
}

// pop char from head
char tStackPopCharFront(StackTableList* list)
{
    TableSlot slot = tStackPopFront(list);
    if (slot.type != TYPE_CHAR) return '\0'; // guard
    return (char)(intptr_t)slot.data;
}

// pop char from tail
char tStackPopCharBack(StackTableList* list)
{
    TableSlot slot = tStackPopBack(list);
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
    TableSlot slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_CHAR;
    tStackInsertAt(list, index, slot);
}

// remove char at index
char tStackRemoveCharAt(StackTableList* list, int index)
{
    TableSlot slot = tStackRemoveAt(list, index);
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
    TableSlot slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_LONG;
    tStackPushBack(list, slot);
}

// push long to head
void tStackPushLongFront(StackTableList* list, long value)
{
    TableSlot slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_LONG;
    tStackPushFront(list, slot);
}

// read long by index
long tStackGetLongAt(const StackTableList* list, int index)
{
    TableSlot slot = tStackGetNodeAt(list, index);
    if (slot.type != TYPE_LONG) return 0; // guard
    return (long)(intptr_t)slot.data;
}

// pop long from head
long tStackPopLongFront(StackTableList* list)
{
    TableSlot slot = tStackPopFront(list);
    if (slot.type != TYPE_LONG) return 0; // guard
    return (long)(intptr_t)slot.data;
}

// pop long from tail
long tStackPopLongBack(StackTableList* list)
{
    TableSlot slot = tStackPopBack(list);
    if (slot.type != TYPE_LONG) return 0; // guard
    return (long)(intptr_t)slot.data;
}

// insert long at index
void tStackInsertLongAt(StackTableList* list, int index, long value)
{
    TableSlot slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_LONG;
    tStackInsertAt(list, index, slot);
}

// remove long at index
long tStackRemoveLongAt(StackTableList* list, int index)
{
    TableSlot slot = tStackRemoveAt(list, index);
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
    TableSlot slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_SHORT;
    tStackPushBack(list, slot);
}

// push short to head
void tStackPushShortFront(StackTableList* list, short value)
{
    TableSlot slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_SHORT;
    tStackPushFront(list, slot);
}

// read short by index
short tStackGetShortAt(const StackTableList* list, int index)
{
    TableSlot slot = tStackGetNodeAt(list, index);
    if (slot.type != TYPE_SHORT) return 0; // guard
    return (short)(intptr_t)slot.data;
}

// pop short from head
short tStackPopShortFront(StackTableList* list)
{
    TableSlot slot = tStackPopFront(list);
    if (slot.type != TYPE_SHORT) return 0; // guard
    return (short)(intptr_t)slot.data;
}

// pop short from tail
short tStackPopShortBack(StackTableList* list)
{
    TableSlot slot = tStackPopBack(list);
    if (slot.type != TYPE_SHORT) return 0; // guard
    return (short)(intptr_t)slot.data;
}

// insert short at index
void tStackInsertShortAt(StackTableList* list, int index, short value)
{
    TableSlot slot;
    slot.data = (void*)(intptr_t)value;
    slot.type = TYPE_SHORT;
    tStackInsertAt(list, index, slot);
}

// remove short at index
short tStackRemoveShortAt(StackTableList* list, int index)
{
    TableSlot slot = tStackRemoveAt(list, index);
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
// 10. GENERIC POINTER WRAPPERS (Tags >= TYPE_NULL)
// ============================================================================

// push pointer to tail
void tStackPushPointerBack(StackTableList* list, void* ptr, short pointer_tag)
{
    TableSlot slot;
    slot.data = ptr;          // address fits perfectly in void*
    slot.type = pointer_tag;
    tStackPushBack(list, slot);
}

// push pointer to head
void tStackPushPointerFront(StackTableList* list, void* ptr, short pointer_tag)
{
    TableSlot slot;
    slot.data = ptr;
    slot.type = pointer_tag;
    tStackPushFront(list, slot);
}

// read pointer by index
void* tStackGetPointerAt(const StackTableList* list, int index)
{
    TableSlot slot = tStackGetNodeAt(list, index);
    if (slot.type < TYPE_NULL) return NULL; // reject primitives
    return slot.data;
}

// pop pointer from head
void* tStackPopPointerFront(StackTableList* list)
{
    TableSlot slot = tStackPopFront(list);
    if (slot.type < TYPE_NULL) return NULL; // reject primitives
    return slot.data;
}

// pop pointer from tail
void* tStackPopPointerBack(StackTableList* list)
{
    TableSlot slot = tStackPopBack(list);
    if (slot.type < TYPE_NULL) return NULL; // reject primitives
    return slot.data;
}

// insert pointer at index
void tStackInsertPointerAt(StackTableList* list, int index, void* ptr, short pointer_tag)
{
    TableSlot slot;
    slot.data = ptr;
    slot.type = pointer_tag;
    tStackInsertAt(list, index, slot);
}

// remove pointer at index
void* tStackRemovePointerAt(StackTableList* list, int index)
{
    TableSlot slot = tStackRemoveAt(list, index);
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
    TableSlot slot;
    slot.data = struct_ptr;
    slot.type = TYPE_STRUCT;
    tStackPushBack(list, slot);
}

// push struct pointer to head
void tStackPushStructFront(StackTableList* list, void* struct_ptr)
{
    TableSlot slot;
    slot.data = struct_ptr;
    slot.type = TYPE_STRUCT;
    tStackPushFront(list, slot);
}

// read struct pointer by index
void* tStackGetStructAt(const StackTableList* list, int index)
{
    TableSlot slot = tStackGetNodeAt(list, index);
    if (slot.type != TYPE_STRUCT) return NULL; // guard
    return slot.data;
}

// pop struct pointer from head
void* tStackPopStructFront(StackTableList* list)
{
    TableSlot slot = tStackPopFront(list);
    if (slot.type != TYPE_STRUCT) return NULL; // guard
    return slot.data;
}

// pop struct pointer from tail
void* tStackPopStructBack(StackTableList* list)
{
    TableSlot slot = tStackPopBack(list);
    if (slot.type != TYPE_STRUCT) return NULL; // guard
    return slot.data;
}

// insert struct pointer at index
void tStackInsertStructAt(StackTableList* list, int index, void* struct_ptr)
{
    TableSlot slot;
    slot.data = struct_ptr;
    slot.type = TYPE_STRUCT;
    tStackInsertAt(list, index, slot);
}

// remove struct pointer at index
void* tStackRemoveStructAt(StackTableList* list, int index)
{
    TableSlot slot = tStackRemoveAt(list, index);
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
    TableSlot slot;
    slot.data = union_ptr;
    slot.type = TYPE_UNION;
    tStackPushBack(list, slot);
}

// push union pointer to head
void tStackPushUnionFront(StackTableList* list, void* union_ptr)
{
    TableSlot slot;
    slot.data = union_ptr;
    slot.type = TYPE_UNION;
    tStackPushFront(list, slot);
}

// read union pointer by index
void* tStackGetUnionAt(const StackTableList* list, int index)
{
    TableSlot slot = tStackGetNodeAt(list, index);
    if (slot.type != TYPE_UNION) return NULL; // guard
    return slot.data;
}

// pop union pointer from head
void* tStackPopUnionFront(StackTableList* list)
{
    TableSlot slot = tStackPopFront(list);
    if (slot.type != TYPE_UNION) return NULL; // guard
    return slot.data;
}

// pop union pointer from tail
void* tStackPopUnionBack(StackTableList* list)
{
    TableSlot slot = tStackPopBack(list);
    if (slot.type != TYPE_UNION) return NULL; // guard
    return slot.data;
}

// insert union pointer at index
void tStackInsertUnionAt(StackTableList* list, int index, void* union_ptr)
{
    TableSlot slot;
    slot.data = union_ptr;
    slot.type = TYPE_UNION;
    tStackInsertAt(list, index, slot);
}

// remove union pointer at index
void* tStackRemoveUnionAt(StackTableList* list, int index)
{
    TableSlot slot = tStackRemoveAt(list, index);
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
    TableSlot slot;
    slot.data = func_ptr;
    slot.type = TYPE_FUNC_PTR;
    tStackPushBack(list, slot);
}

// push function pointer to head
void tStackPushFuncPtrFront(StackTableList* list, void* func_ptr)
{
    TableSlot slot;
    slot.data = func_ptr;
    slot.type = TYPE_FUNC_PTR;
    tStackPushFront(list, slot);
}

// read function pointer by index
void* tStackGetFuncPtrAt(const StackTableList* list, int index)
{
    TableSlot slot = tStackGetNodeAt(list, index);
    if (slot.type != TYPE_FUNC_PTR) return NULL; // guard
    return slot.data;
}

// pop function pointer from head
void* tStackPopFuncPtrFront(StackTableList* list)
{
    TableSlot slot = tStackPopFront(list);
    if (slot.type != TYPE_FUNC_PTR) return NULL; // guard
    return slot.data;
}

// pop function pointer from tail
void* tStackPopFuncPtrBack(StackTableList* list)
{
    TableSlot slot = tStackPopBack(list);
    if (slot.type != TYPE_FUNC_PTR) return NULL; // guard
    return slot.data;
}

// insert function pointer at index
void tStackInsertFuncPtrAt(StackTableList* list, int index, void* func_ptr)
{
    TableSlot slot;
    slot.data = func_ptr;
    slot.type = TYPE_FUNC_PTR;
    tStackInsertAt(list, index, slot);
}

// remove function pointer at index
void* tStackRemoveFuncPtrAt(StackTableList* list, int index)
{
    TableSlot slot = tStackRemoveAt(list, index);
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
    TableSlot slot;
    slot.data = (void*)file_stream;
    slot.type = TYPE_FILE_PTR;
    tStackPushBack(list, slot);
}

// push FILE* to head
void tStackPushFilePtrFront(StackTableList* list, FILE* file_stream)
{
    TableSlot slot;
    slot.data = (void*)file_stream;
    slot.type = TYPE_FILE_PTR;
    tStackPushFront(list, slot);
}

// read FILE* by index
FILE* tStackGetFilePtrAt(const StackTableList* list, int index)
{
    TableSlot slot = tStackGetNodeAt(list, index);
    if (slot.type != TYPE_FILE_PTR) return NULL; // guard
    return (FILE*)slot.data;
}

// pop FILE* from head
FILE* tStackPopFilePtrFront(StackTableList* list)
{
    TableSlot slot = tStackPopFront(list);
    if (slot.type != TYPE_FILE_PTR) return NULL; // guard
    return (FILE*)slot.data;
}

// pop FILE* from tail
FILE* tStackPopFilePtrBack(StackTableList* list)
{
    TableSlot slot = tStackPopBack(list);
    if (slot.type != TYPE_FILE_PTR) return NULL; // guard
    return (FILE*)slot.data;
}

// insert FILE* at index
void tStackInsertFilePtrAt(StackTableList* list, int index, FILE* file_stream)
{
    TableSlot slot;
    slot.data = (void*)file_stream;
    slot.type = TYPE_FILE_PTR;
    tStackInsertAt(list, index, slot);
}

// remove FILE* at index
FILE* tStackRemoveFilePtrAt(StackTableList* list, int index)
{
    TableSlot slot = tStackRemoveAt(list, index);
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