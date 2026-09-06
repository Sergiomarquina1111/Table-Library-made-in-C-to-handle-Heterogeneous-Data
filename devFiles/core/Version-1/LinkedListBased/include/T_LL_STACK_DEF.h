#ifndef T_LL_STACK_DEF_H
#define T_LL_STACK_DEF_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// TYPE IDENTIFICATION TAGS
// ============================================================================

// Type identification tags for primitives
typedef enum DATA_TYPE
{
    TYPE_EMPTY = 0,  // uninitialized / empty slot
    TYPE_INT,        // int payload
    TYPE_FLOAT,      // float payload
    TYPE_DOUBLE,     // double payload
    TYPE_CHAR,       // char payload
    TYPE_LONG,       // long payload
    TYPE_SHORT       // short payload
} DATA_TYPE;

// Type identification tags for single pointers
typedef enum POINTERS
{
    TYPE_NULL = 10,     // untyped / null pointer -- also the floor tag for "is a pointer"
    TYPE_VOID_STAR,     // void*
    TYPE_INT_STAR,      // int*
    TYPE_FLOAT_STAR,    // float*
    TYPE_CHAR_STAR,     // char*
    TYPE_DOUBLE_STAR,   // double*
    TYPE_LONG_STAR,     // long*
    TYPE_SHORT_STAR     // short*
} POINTERS;

// Type identification tags for double pointers
typedef enum DOUBLE_POINTERS
{
    TYPE_NULL_DOUBLE = 100,     // untyped / null double pointer
    TYPE_VOID_STAR_DOUBLE,      // void**
    TYPE_INT_STAR_DOUBLE,       // int**
    TYPE_FLOAT_STAR_DOUBLE,     // float**
    TYPE_CHAR_STAR_DOUBLE,      // char**
    TYPE_DOUBLE_STAR_DOUBLE,    // double**
    TYPE_LONG_STAR_DOUBLE,      // long**
    TYPE_SHORT_STAR_DOUBLE      // short**
} DOUBLE_POINTERS;

// Type identification tags for triple pointers
typedef enum TRIPLE_POINTERS
{
    TYPE_NULL_TRIPLE = 1000,    // untyped / null triple pointer
    TYPE_VOID_STAR_TRIPLE,      // void***
    TYPE_INT_STAR_TRIPLE,       // int***
    TYPE_FLOAT_STAR_TRIPLE,     // float***
    TYPE_CHAR_STAR_TRIPLE,      // char***
    TYPE_DOUBLE_STAR_TRIPLE,    // double***
    TYPE_LONG_STAR_TRIPLE,      // long***
    TYPE_SHORT_STAR_TRIPLE      // short***
} TRIPLE_POINTERS;

// Type identification tags for complex and system types
typedef enum USER_DEFINED
{
    TYPE_NULL_USER_DEFINED = 2000, // untyped / null user-defined slot
    TYPE_STRUCT,                    // struct value
    TYPE_UNION,                     // union value
    TYPE_STRUCT_PTR,                 // struct pointer
    TYPE_UNION_PTR,                   // union pointer
    TYPE_FUNC_PTR,                     // function pointer
    TYPE_FILE_PTR                       // FILE* stream
} USER_DEFINED;

// ============================================================================
// CORE DATA STRUCTURES
// ============================================================================

// Tagged, type-erased value cell
typedef struct TableSlot
{
    void* data; // raw payload, reinterpreted per `type`
    short type; // type tag identifying how to interpret `data`
} TableSlot;

// Maximum number of nodes a StackTableList can ever hold.
// The entire node pool lives inline inside the container (no heap calls,
// no malloc/free) so this cap is fixed at compile time.
#define STACK_TABLE_CAPACITY 100

// Doubly linked node wrapping a TableSlot
typedef struct sNode
{
    TableSlot slot;      // the stored value
    struct sNode* next;  // link to next node (or, while unused, next free slot in the pool)
    struct sNode* prev;  // link to previous node (unused while node sits in the free list)
} sNode;

// Doubly linked list container (fixed-capacity, stack-allocatable variant)
//
// Every node this list can ever use lives inline in `pool` — the whole
// container (list + up to 100 nodes) can be declared on the stack with
// `StackTableList list;` and needs zero heap allocations for its lifetime.
// `sFreeListHead` threads the currently-unused pool slots together (via
// each node's `next` pointer) so grabbing/returning a node is an O(1)
// pointer swap instead of a malloc()/free() call.
typedef struct StackTableList
{
    sNode  pool[STACK_TABLE_CAPACITY]; // fixed inline node storage (no heap!)
    sNode* sFreeListHead;             // head of the free-node stack within `pool`
    sNode* sNodeHead;                 // first *active* node
    sNode* sNodeTail;                 // last *active* node
    sNode* sNodePrev;                 // reserved / cursor node for iteration state
    int count;                        // number of nodes currently stored
} StackTableList;

// ============================================================================
// 1. CONSTRUCTION / DESTRUCTION
// ============================================================================

void tStackFormLinkedTable(StackTableList* list);                 // initialize an empty list (links the pool's free list)
sNode* tStackCreateNode(StackTableList* list, TableSlot payload); // claim a node from the pool and populate it
void tStackDestroyList(StackTableList* list);                        // return every node to the pool (no heap frees)
void vStackPrintList(const StackTableList* list);                     // debug-print list contents
bool bStackIsListEmpty(const StackTableList* list);                     // check if list has no nodes
bool bStackIsListFull(const StackTableList* list);                       // check if the 100-node pool is exhausted

// ============================================================================
// 2. RAW PUSH / POP / PEEK (O(1))
// ============================================================================

void tStackPushBack(StackTableList* list, TableSlot payload);        // insert raw slot at tail
void tStackPushFront(StackTableList* list, TableSlot payload);         // insert raw slot at head
TableSlot tStackPopFront(StackTableList* list);                          // remove and return head slot
TableSlot tStackPopBack(StackTableList* list);                             // remove and return tail slot
TableSlot sStackPeekFront(const StackTableList* list);                       // read head slot without removing
TableSlot sStackPeekBack(const StackTableList* list);                          // read tail slot without removing
TableSlot tStackGetNodeAt(const StackTableList* list, int target_index);        // random-access read by index

// ============================================================================
// 3. TARGETED TOPOLOGY MODIFICATION (generic)
// ============================================================================

void tStackInsertAt(StackTableList* list, int index, TableSlot payload); // insert raw slot at index
TableSlot tStackRemoveAt(StackTableList* list, int index);                 // remove raw slot at index
void vStackReverseList(StackTableList* list);                                // reverse node order in place

// ============================================================================
// 4. INT WRAPPERS (Tag: TYPE_INT)
// ============================================================================

void tStackPushIntBack(StackTableList* list, int value);            // push int to tail
void tStackPushIntFront(StackTableList* list, int value);             // push int to head
int tStackGetIntAt(const StackTableList* list, int index);               // read int by index
int tStackPopIntFront(StackTableList* list);                                // pop int from head
int tStackPopIntBack(StackTableList* list);                                   // pop int from tail
void tStackPushIntListBack(StackTableList* list, int count, ...);              // bulk push variadic ints
void tStackInsertIntAt(StackTableList* list, int index, int value);              // insert int at index
int tStackRemoveIntAt(StackTableList* list, int index);                            // remove int at index
int tStackFindInt(const StackTableList* list, int target);                           // linear search for int

// ============================================================================
// 5. FLOAT WRAPPERS (Tag: TYPE_FLOAT)
// ============================================================================

void tStackPushFloatBack(StackTableList* list, float value);        // push float to tail
void tStackPushFloatFront(StackTableList* list, float value);          // push float to head
float tStackGetFloatAt(const StackTableList* list, int index);             // read float by index
float tStackPopFloatFront(StackTableList* list);                              // pop float from head
float tStackPopFloatBack(StackTableList* list);                                 // pop float from tail
void tStackInsertFloatAt(StackTableList* list, int index, float value);          // insert float at index
float tStackRemoveFloatAt(StackTableList* list, int index);                        // remove float at index
int tStackFindFloat(const StackTableList* list, float target);                       // linear search for float

// ============================================================================
// 6. DOUBLE WRAPPERS (Tag: TYPE_DOUBLE)
// ============================================================================

void tStackPushDoubleBack(StackTableList* list, double value);      // push double to tail
void tStackPushDoubleFront(StackTableList* list, double value);        // push double to head
double tStackGetDoubleAt(const StackTableList* list, int index);           // read double by index
double tStackPopDoubleFront(StackTableList* list);                            // pop double from head
double tStackPopDoubleBack(StackTableList* list);                               // pop double from tail
void tStackPushDoublesBack(StackTableList* list, int count, ...);                // bulk push variadic doubles
void tStackInsertDoubleAt(StackTableList* list, int index, double value);          // insert double at index
double tStackRemoveDoubleAt(StackTableList* list, int index);                        // remove double at index
int tStackFindDouble(const StackTableList* list, double target);                       // linear search for double

// ============================================================================
// 7. CHAR WRAPPERS (Tag: TYPE_CHAR)
// ============================================================================

void tStackPushCharBack(StackTableList* list, char value);           // push char to tail
void tStackPushCharFront(StackTableList* list, char value);             // push char to head
char tStackGetCharAt(const StackTableList* list, int index);                // read char by index
char tStackPopCharFront(StackTableList* list);                                 // pop char from head
char tStackPopCharBack(StackTableList* list);                                    // pop char from tail
void tStackPushCharsBack(StackTableList* list, int count, ...);                   // bulk push variadic chars
void tStackInsertCharAt(StackTableList* list, int index, char value);               // insert char at index
char tStackRemoveCharAt(StackTableList* list, int index);                             // remove char at index
int tStackFindChar(const StackTableList* list, char target);                            // linear search for char

// ============================================================================
// 8. LONG WRAPPERS (Tag: TYPE_LONG)
// ============================================================================

void tStackPushLongBack(StackTableList* list, long value);            // push long to tail
void tStackPushLongFront(StackTableList* list, long value);              // push long to head
long tStackGetLongAt(const StackTableList* list, int index);                 // read long by index
long tStackPopLongFront(StackTableList* list);                                  // pop long from head
long tStackPopLongBack(StackTableList* list);                                     // pop long from tail
void tStackInsertLongAt(StackTableList* list, int index, long value);              // insert long at index
long tStackRemoveLongAt(StackTableList* list, int index);                            // remove long at index
int tStackFindLong(const StackTableList* list, long target);                           // linear search for long

// ============================================================================
// 9. SHORT WRAPPERS (Tag: TYPE_SHORT)
// ============================================================================

void tStackPushShortBack(StackTableList* list, short value);           // push short to tail
void tStackPushShortFront(StackTableList* list, short value);             // push short to head
short tStackGetShortAt(const StackTableList* list, int index);                // read short by index
short tStackPopShortFront(StackTableList* list);                                 // pop short from head
short tStackPopShortBack(StackTableList* list);                                    // pop short from tail
void tStackInsertShortAt(StackTableList* list, int index, short value);              // insert short at index
short tStackRemoveShortAt(StackTableList* list, int index);                             // remove short at index
int tStackFindShort(const StackTableList* list, short target);                            // linear search for short

// ============================================================================
// 10. SINGLE POINTER WRAPPERS (Tags: POINTERS enum, floor = TYPE_NULL)
// ============================================================================

void tStackPushPointerBack(StackTableList* list, void* ptr, short pointer_tag);  // push single pointer to tail
void tStackPushPointerFront(StackTableList* list, void* ptr, short pointer_tag);   // push single pointer to head
void* tStackGetPointerAt(const StackTableList* list, int index);                      // read pointer by index
void* tStackPopPointerFront(StackTableList* list);                                       // pop pointer from head
void* tStackPopPointerBack(StackTableList* list);                                          // pop pointer from tail
void tStackInsertPointerAt(StackTableList* list, int index, void* ptr, short pointer_tag);   // insert pointer at index
void* tStackRemovePointerAt(StackTableList* list, int index);                                  // remove pointer at index
int tStackFindPointer(const StackTableList* list, void* target_ptr);                             // linear search for pointer

// ============================================================================
// 11. STRUCT / UNION WRAPPERS (Tags: TYPE_STRUCT, TYPE_UNION)
// ============================================================================

void tStackPushStructBack(StackTableList* list, void* struct_ptr);   // push struct pointer to tail
void tStackPushStructFront(StackTableList* list, void* struct_ptr);    // push struct pointer to head
void* tStackGetStructAt(const StackTableList* list, int index);            // read struct pointer by index
void* tStackPopStructFront(StackTableList* list);                             // pop struct pointer from head
void* tStackPopStructBack(StackTableList* list);                                // pop struct pointer from tail
void tStackInsertStructAt(StackTableList* list, int index, void* struct_ptr);     // insert struct pointer at index
void* tStackRemoveStructAt(StackTableList* list, int index);                         // remove struct pointer at index
int tStackFindStruct(const StackTableList* list, void* target_struct_ptr);             // linear search for struct pointer

void tStackPushUnionBack(StackTableList* list, void* union_ptr);      // push union pointer to tail
void tStackPushUnionFront(StackTableList* list, void* union_ptr);       // push union pointer to head
void* tStackGetUnionAt(const StackTableList* list, int index);              // read union pointer by index
void* tStackPopUnionFront(StackTableList* list);                               // pop union pointer from head
void* tStackPopUnionBack(StackTableList* list);                                  // pop union pointer from tail
void tStackInsertUnionAt(StackTableList* list, int index, void* union_ptr);       // insert union pointer at index
void* tStackRemoveUnionAt(StackTableList* list, int index);                          // remove union pointer at index
int tStackFindUnion(const StackTableList* list, void* target_union_ptr);               // linear search for union pointer

// ============================================================================
// 12. SYSTEM RESOURCE WRAPPERS (Tags: TYPE_FUNC_PTR, TYPE_FILE_PTR)
// ============================================================================

void tStackPushFuncPtrBack(StackTableList* list, void* func_ptr);     // push function pointer to tail
void tStackPushFuncPtrFront(StackTableList* list, void* func_ptr);      // push function pointer to head
void* tStackGetFuncPtrAt(const StackTableList* list, int index);            // read function pointer by index
void* tStackPopFuncPtrFront(StackTableList* list);                             // pop function pointer from head
void* tStackPopFuncPtrBack(StackTableList* list);                                // pop function pointer from tail
void tStackInsertFuncPtrAt(StackTableList* list, int index, void* func_ptr);       // insert function pointer at index
void* tStackRemoveFuncPtrAt(StackTableList* list, int index);                         // remove function pointer at index
int tStackFindFuncPtr(const StackTableList* list, void* target_func_ptr);               // linear search for function pointer

void tStackPushFilePtrBack(StackTableList* list, FILE* file_stream);   // push FILE* to tail
void tStackPushFilePtrFront(StackTableList* list, FILE* file_stream);    // push FILE* to head
FILE* tStackGetFilePtrAt(const StackTableList* list, int index);             // read FILE* by index
FILE* tStackPopFilePtrFront(StackTableList* list);                              // pop FILE* from head
FILE* tStackPopFilePtrBack(StackTableList* list);                                 // pop FILE* from tail
void tStackInsertFilePtrAt(StackTableList* list, int index, FILE* file_stream);     // insert FILE* at index
FILE* tStackRemoveFilePtrAt(StackTableList* list, int index);                          // remove FILE* at index
int tStackFindFilePtr(const StackTableList* list, FILE* target_stream);                   // linear search for FILE*

// ============================================================================
// 13. MACRO API (The High-Level Interface)
// ============================================================================

// --- GENERIC TOPOLOGY & RAW SLOTS ---
#define S_PUSH_RAW_BACK(list, slot)       tStackPushBack(list, slot)
#define S_PUSH_RAW_FRONT(list, slot)      tStackPushFront(list, slot)
#define S_POP_RAW_FRONT(list)             tStackPopFront(list)
#define S_POP_RAW_BACK(list)              tStackPopBack(list)
#define S_PEEK_RAW_FRONT(list)            sStackPeekFront(list)
#define S_PEEK_RAW_BACK(list)             sStackPeekBack(list)
#define S_GET_RAW_AT(list, index)         tStackGetNodeAt(list, index)
#define S_INSERT_RAW_AT(list, idx, slot)  tStackInsertAt(list, idx, slot)
#define S_REMOVE_RAW_AT(list, idx)        tStackRemoveAt(list, idx)

#define S_DESTROY_LIST(list)              tStackDestroyList(list)
#define S_PRINT_LIST(list)                vStackPrintList(list)
#define S_REVERSE_LIST(list)              vStackReverseList(list)
#define S_IS_EMPTY(list)                  bStackIsListEmpty(list)
#define S_IS_FULL(list)                   bStackIsListFull(list)
#define S_CAPACITY                        STACK_TABLE_CAPACITY

// --- INTS ---
#define S_PUSH_INT(list, val)             tStackPushIntBack(list, val)
#define S_PUSH_INT_FRONT(list, val)       tStackPushIntFront(list, val)
#define S_POP_INT(list)                   tStackPopIntFront(list)
#define S_POP_INT_BACK(list)              tStackPopIntBack(list)
#define S_GET_INT(list, index)            tStackGetIntAt(list, index)
#define S_PUSH_INTS(list, count, ...)     tStackPushIntListBack(list, count, __VA_ARGS__)
#define S_INSERT_INT(list, index, val)    tStackInsertIntAt(list, index, val)
#define S_REMOVE_INT(list, index)         tStackRemoveIntAt(list, index)
#define S_FIND_INT(list, target)          tStackFindInt(list, target)

// --- FLOATS ---
#define S_PUSH_FLOAT(list, val)           tStackPushFloatBack(list, val)
#define S_PUSH_FLOAT_FRONT(list, val)     tStackPushFloatFront(list, val)
#define S_POP_FLOAT(list)                 tStackPopFloatFront(list)
#define S_POP_FLOAT_BACK(list)            tStackPopFloatBack(list)
#define S_GET_FLOAT(list, index)          tStackGetFloatAt(list, index)
#define S_INSERT_FLOAT(list, index, val)  tStackInsertFloatAt(list, index, val)
#define S_REMOVE_FLOAT(list, index)       tStackRemoveFloatAt(list, index)
#define S_FIND_FLOAT(list, target)        tStackFindFloat(list, target)

// --- DOUBLES ---
#define S_PUSH_DOUBLE(list, val)          tStackPushDoubleBack(list, val)
#define S_PUSH_DOUBLE_FRONT(list, val)    tStackPushDoubleFront(list, val)
#define S_POP_DOUBLE(list)                tStackPopDoubleFront(list)
#define S_POP_DOUBLE_BACK(list)           tStackPopDoubleBack(list)
#define S_GET_DOUBLE(list, index)         tStackGetDoubleAt(list, index)
#define S_PUSH_DOUBLES(list, count, ...)  tStackPushDoublesBack(list, count, __VA_ARGS__)
#define S_INSERT_DOUBLE(list, idx, val)   tStackInsertDoubleAt(list, idx, val)
#define S_REMOVE_DOUBLE(list, index)      tStackRemoveDoubleAt(list, index)
#define S_FIND_DOUBLE(list, target)       tStackFindDouble(list, target)

// --- CHARS ---
#define S_PUSH_CHAR(list, val)            tStackPushCharBack(list, val)
#define S_PUSH_CHAR_FRONT(list, val)      tStackPushCharFront(list, val)
#define S_POP_CHAR(list)                  tStackPopCharFront(list)
#define S_POP_CHAR_BACK(list)             tStackPopCharBack(list)
#define S_GET_CHAR(list, index)           tStackGetCharAt(list, index)
#define S_PUSH_CHARS(list, count, ...)    tStackPushCharsBack(list, count, __VA_ARGS__)
#define S_INSERT_CHAR(list, index, val)   tStackInsertCharAt(list, index, val)
#define S_REMOVE_CHAR(list, index)        tStackRemoveCharAt(list, index)
#define S_FIND_CHAR(list, target)         tStackFindChar(list, target)

// --- LONGS ---
#define S_PUSH_LONG(list, val)            tStackPushLongBack(list, val)
#define S_PUSH_LONG_FRONT(list, val)      tStackPushLongFront(list, val)
#define S_POP_LONG(list)                  tStackPopLongFront(list)
#define S_POP_LONG_BACK(list)             tStackPopLongBack(list)
#define S_GET_LONG(list, index)           tStackGetLongAt(list, index)
#define S_INSERT_LONG(list, index, val)   tStackInsertLongAt(list, index, val)
#define S_REMOVE_LONG(list, index)        tStackRemoveLongAt(list, index)
#define S_FIND_LONG(list, target)         tStackFindLong(list, target)

// --- SHORTS ---
#define S_PUSH_SHORT(list, val)           tStackPushShortBack(list, val)
#define S_PUSH_SHORT_FRONT(list, val)     tStackPushShortFront(list, val)
#define S_POP_SHORT(list)                 tStackPopShortFront(list)
#define S_POP_SHORT_BACK(list)            tStackPopShortBack(list)
#define S_GET_SHORT(list, index)          tStackGetShortAt(list, index)
#define S_INSERT_SHORT(list, index, val)  tStackInsertShortAt(list, index, val)
#define S_REMOVE_SHORT(list, index)       tStackRemoveShortAt(list, index)
#define S_FIND_SHORT(list, target)        tStackFindShort(list, target)

// --- GENERIC POINTERS ---
#define S_PUSH_PTR(list, ptr, tag)        tStackPushPointerBack(list, ptr, tag)
#define S_PUSH_PTR_FRONT(list, ptr, tag)  tStackPushPointerFront(list, ptr, tag)
#define S_POP_PTR(list)                   tStackPopPointerFront(list)
#define S_POP_PTR_BACK(list)              tStackPopPointerBack(list)
#define S_GET_PTR(list, index)            tStackGetPointerAt(list, index)
#define S_INSERT_PTR(list, idx, ptr, tag) tStackInsertPointerAt(list, idx, ptr, tag)
#define S_REMOVE_PTR(list, index)         tStackRemovePointerAt(list, index)
#define S_FIND_PTR(list, ptr)             tStackFindPointer(list, ptr)

// --- STRUCTS ---
#define S_PUSH_STRUCT(list, ptr)          tStackPushStructBack(list, ptr)
#define S_PUSH_STRUCT_FRONT(list, ptr)    tStackPushStructFront(list, ptr)
#define S_POP_STRUCT(list)                tStackPopStructFront(list)
#define S_POP_STRUCT_BACK(list)           tStackPopStructBack(list)
#define S_GET_STRUCT(list, index)         tStackGetStructAt(list, index)
#define S_INSERT_STRUCT(list, index, ptr) tStackInsertStructAt(list, index, ptr)
#define S_REMOVE_STRUCT(list, index)      tStackRemoveStructAt(list, index)
#define S_FIND_STRUCT(list, ptr)          tStackFindStruct(list, ptr)

// --- UNIONS ---
#define S_PUSH_UNION(list, ptr)           tStackPushUnionBack(list, ptr)
#define S_PUSH_UNION_FRONT(list, ptr)     tStackPushUnionFront(list, ptr)
#define S_POP_UNION(list)                 tStackPopUnionFront(list)
#define S_POP_UNION_BACK(list)            tStackPopUnionBack(list)
#define S_GET_UNION(list, index)          tStackGetUnionAt(list, index)
#define S_INSERT_UNION(list, index, ptr)  tStackInsertUnionAt(list, index, ptr)
#define S_REMOVE_UNION(list, index)       tStackRemoveUnionAt(list, index)
#define S_FIND_UNION(list, ptr)           tStackFindUnion(list, ptr)

// --- SYSTEM RESOURCES (FUNC PTRS & FILES) ---
#define S_PUSH_FUNC(list, ptr)            tStackPushFuncPtrBack(list, ptr)
#define S_PUSH_FUNC_FRONT(list, ptr)      tStackPushFuncPtrFront(list, ptr)
#define S_POP_FUNC(list)                  tStackPopFuncPtrFront(list)
#define S_POP_FUNC_BACK(list)             tStackPopFuncPtrBack(list)
#define S_GET_FUNC(list, index)           tStackGetFuncPtrAt(list, index)
#define S_INSERT_FUNC(list, index, ptr)   tStackInsertFuncPtrAt(list, index, ptr)
#define S_REMOVE_FUNC(list, index)        tStackRemoveFuncPtrAt(list, index)
#define S_FIND_FUNC(list, ptr)            tStackFindFuncPtr(list, ptr)

#define S_PUSH_FILE(list, stream)         tStackPushFilePtrBack(list, stream)
#define S_PUSH_FILE_FRONT(list, stream)   tStackPushFilePtrFront(list, stream)
#define S_POP_FILE(list)                  tStackPopFilePtrFront(list)
#define S_POP_FILE_BACK(list)             tStackPopFilePtrBack(list)
#define S_GET_FILE(list, index)           tStackGetFilePtrAt(list, index)
#define S_INSERT_FILE(list, idx, stream)  tStackInsertFilePtrAt(list, idx, stream)
#define S_REMOVE_FILE(list, index)        tStackRemoveFilePtrAt(list, index)
#define S_FIND_FILE(list, stream)         tStackFindFilePtr(list, stream)

#endif // T_LL_STACK_DEF_H