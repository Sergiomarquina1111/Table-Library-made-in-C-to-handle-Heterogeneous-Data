#ifndef T_LL_DEF_H
#define T_LL_DEF_H

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

// Doubly linked node wrapping a TableSlot
typedef struct tNode
{
    TableSlot slot;      // the stored value
    struct tNode* next;  // link to next node
    struct tNode* prev;  // link to previous node
} tNode;

// Doubly linked list container (heap-based variant)
typedef struct TableList
{
    tNode* tNodeHead; // first node
    tNode* tNodeTail; // last node
    tNode* tNodePrev; // reserved / cursor node for iteration state
    int count;        // number of nodes currently stored
} TableList;

// ============================================================================
// 1. CONSTRUCTION / DESTRUCTION
// ============================================================================

void tFormLinkedTable(TableList* list);                 // initialize an empty list
tNode* tCreateNode(TableSlot payload);                    // allocate and populate a node
void tDestroyList(TableList* list);                        // free every node in the list
void vPrintList(const TableList* list);                     // debug-print list contents
bool bIsListEmpty(const TableList* list);                     // check if list has no nodes

// ============================================================================
// 2. RAW PUSH / POP / PEEK (O(1))
// ============================================================================

void tPushBack(TableList* list, TableSlot payload);        // insert raw slot at tail
void tPushFront(TableList* list, TableSlot payload);         // insert raw slot at head
TableSlot tPopFront(TableList* list);                          // remove and return head slot
TableSlot tPopBack(TableList* list);                             // remove and return tail slot
TableSlot sPeekFront(const TableList* list);                       // read head slot without removing
TableSlot sPeekBack(const TableList* list);                          // read tail slot without removing
TableSlot tGetNodeAt(const TableList* list, int target_index);        // random-access read by index

// ============================================================================
// 3. TARGETED TOPOLOGY MODIFICATION (generic)
// ============================================================================

void tInsertAt(TableList* list, int index, TableSlot payload); // insert raw slot at index
TableSlot tRemoveAt(TableList* list, int index);                 // remove raw slot at index
void vReverseList(TableList* list);                                // reverse node order in place

// ============================================================================
// 4. INT WRAPPERS (Tag: TYPE_INT)
// ============================================================================

void tPushIntBack(TableList* list, int value);            // push int to tail
void tPushIntFront(TableList* list, int value);             // push int to head
int tGetIntAt(const TableList* list, int index);               // read int by index
int tPopIntFront(TableList* list);                                // pop int from head
int tPopIntBack(TableList* list);                                   // pop int from tail
void tPushIntListBack(TableList* list, int count, ...);              // bulk push variadic ints
void tInsertIntAt(TableList* list, int index, int value);              // insert int at index
int tRemoveIntAt(TableList* list, int index);                            // remove int at index
int tFindInt(const TableList* list, int target);                           // linear search for int

// ============================================================================
// 5. FLOAT WRAPPERS (Tag: TYPE_FLOAT)
// ============================================================================

void tPushFloatBack(TableList* list, float value);        // push float to tail
void tPushFloatFront(TableList* list, float value);          // push float to head
float tGetFloatAt(const TableList* list, int index);             // read float by index
float tPopFloatFront(TableList* list);                              // pop float from head
float tPopFloatBack(TableList* list);                                 // pop float from tail
void tInsertFloatAt(TableList* list, int index, float value);          // insert float at index
float tRemoveFloatAt(TableList* list, int index);                        // remove float at index
int tFindFloat(const TableList* list, float target);                       // linear search for float

// ============================================================================
// 6. DOUBLE WRAPPERS (Tag: TYPE_DOUBLE)
// ============================================================================

void tPushDoubleBack(TableList* list, double value);      // push double to tail
void tPushDoubleFront(TableList* list, double value);        // push double to head
double tGetDoubleAt(const TableList* list, int index);           // read double by index
double tPopDoubleFront(TableList* list);                            // pop double from head
double tPopDoubleBack(TableList* list);                               // pop double from tail
void tPushDoublesBack(TableList* list, int count, ...);                // bulk push variadic doubles
void tInsertDoubleAt(TableList* list, int index, double value);          // insert double at index
double tRemoveDoubleAt(TableList* list, int index);                        // remove double at index
int tFindDouble(const TableList* list, double target);                       // linear search for double

// ============================================================================
// 7. CHAR WRAPPERS (Tag: TYPE_CHAR)
// ============================================================================

void tPushCharBack(TableList* list, char value);           // push char to tail
void tPushCharFront(TableList* list, char value);             // push char to head
char tGetCharAt(const TableList* list, int index);                // read char by index
char tPopCharFront(TableList* list);                                 // pop char from head
char tPopCharBack(TableList* list);                                    // pop char from tail
void tPushCharsBack(TableList* list, int count, ...);                   // bulk push variadic chars
void tInsertCharAt(TableList* list, int index, char value);               // insert char at index
char tRemoveCharAt(TableList* list, int index);                             // remove char at index
int tFindChar(const TableList* list, char target);                            // linear search for char

// ============================================================================
// 8. LONG WRAPPERS (Tag: TYPE_LONG)
// ============================================================================

void tPushLongBack(TableList* list, long value);            // push long to tail
void tPushLongFront(TableList* list, long value);              // push long to head
long tGetLongAt(const TableList* list, int index);                 // read long by index
long tPopLongFront(TableList* list);                                  // pop long from head
long tPopLongBack(TableList* list);                                     // pop long from tail
void tInsertLongAt(TableList* list, int index, long value);              // insert long at index
long tRemoveLongAt(TableList* list, int index);                            // remove long at index
int tFindLong(const TableList* list, long target);                           // linear search for long

// ============================================================================
// 9. SHORT WRAPPERS (Tag: TYPE_SHORT)
// ============================================================================

void tPushShortBack(TableList* list, short value);           // push short to tail
void tPushShortFront(TableList* list, short value);             // push short to head
short tGetShortAt(const TableList* list, int index);                // read short by index
short tPopShortFront(TableList* list);                                 // pop short from head
short tPopShortBack(TableList* list);                                    // pop short from tail
void tInsertShortAt(TableList* list, int index, short value);              // insert short at index
short tRemoveShortAt(TableList* list, int index);                             // remove short at index
int tFindShort(const TableList* list, short target);                            // linear search for short

// ============================================================================
// 10. SINGLE POINTER WRAPPERS (Tags: POINTERS enum, floor = TYPE_NULL)
// ============================================================================

void tPushPointerBack(TableList* list, void* ptr, short pointer_tag);  // push single pointer to tail
void tPushPointerFront(TableList* list, void* ptr, short pointer_tag);   // push single pointer to head
void* tGetPointerAt(const TableList* list, int index);                      // read pointer by index
void* tPopPointerFront(TableList* list);                                       // pop pointer from head
void* tPopPointerBack(TableList* list);                                          // pop pointer from tail
void tInsertPointerAt(TableList* list, int index, void* ptr, short pointer_tag);   // insert pointer at index
void* tRemovePointerAt(TableList* list, int index);                                  // remove pointer at index
int tFindPointer(const TableList* list, void* target_ptr);                             // linear search for pointer

// ============================================================================
// 11. STRUCT / UNION WRAPPERS (Tags: TYPE_STRUCT, TYPE_UNION)
// ============================================================================

void tPushStructBack(TableList* list, void* struct_ptr);   // push struct pointer to tail
void tPushStructFront(TableList* list, void* struct_ptr);    // push struct pointer to head
void* tGetStructAt(const TableList* list, int index);            // read struct pointer by index
void* tPopStructFront(TableList* list);                             // pop struct pointer from head
void* tPopStructBack(TableList* list);                                // pop struct pointer from tail
void tInsertStructAt(TableList* list, int index, void* struct_ptr);     // insert struct pointer at index
void* tRemoveStructAt(TableList* list, int index);                         // remove struct pointer at index
int tFindStruct(const TableList* list, void* target_struct_ptr);             // linear search for struct pointer

void tPushUnionBack(TableList* list, void* union_ptr);      // push union pointer to tail
void tPushUnionFront(TableList* list, void* union_ptr);       // push union pointer to head
void* tGetUnionAt(const TableList* list, int index);              // read union pointer by index
void* tPopUnionFront(TableList* list);                               // pop union pointer from head
void* tPopUnionBack(TableList* list);                                  // pop union pointer from tail
void tInsertUnionAt(TableList* list, int index, void* union_ptr);       // insert union pointer at index
void* tRemoveUnionAt(TableList* list, int index);                          // remove union pointer at index
int tFindUnion(const TableList* list, void* target_union_ptr);               // linear search for union pointer

// ============================================================================
// 12. SYSTEM RESOURCE WRAPPERS (Tags: TYPE_FUNC_PTR, TYPE_FILE_PTR)
// ============================================================================

void tPushFuncPtrBack(TableList* list, void* func_ptr);     // push function pointer to tail
void tPushFuncPtrFront(TableList* list, void* func_ptr);      // push function pointer to head
void* tGetFuncPtrAt(const TableList* list, int index);            // read function pointer by index
void* tPopFuncPtrFront(TableList* list);                             // pop function pointer from head
void* tPopFuncPtrBack(TableList* list);                                // pop function pointer from tail
void tInsertFuncPtrAt(TableList* list, int index, void* func_ptr);       // insert function pointer at index
void* tRemoveFuncPtrAt(TableList* list, int index);                         // remove function pointer at index
int tFindFuncPtr(const TableList* list, void* target_func_ptr);               // linear search for function pointer

void tPushFilePtrBack(TableList* list, FILE* file_stream);   // push FILE* to tail
void tPushFilePtrFront(TableList* list, FILE* file_stream);    // push FILE* to head
FILE* tGetFilePtrAt(const TableList* list, int index);             // read FILE* by index
FILE* tPopFilePtrFront(TableList* list);                              // pop FILE* from head
FILE* tPopFilePtrBack(TableList* list);                                 // pop FILE* from tail
void tInsertFilePtrAt(TableList* list, int index, FILE* file_stream);     // insert FILE* at index
FILE* tRemoveFilePtrAt(TableList* list, int index);                          // remove FILE* at index
int tFindFilePtr(const TableList* list, FILE* target_stream);                   // linear search for FILE*

// ============================================================================
// 13. MACRO API (The High-Level Interface)
// ============================================================================

// --- GENERIC TOPOLOGY & RAW SLOTS ---
#define PUSH_RAW_BACK(list, slot)       tPushBack(list, slot)
#define PUSH_RAW_FRONT(list, slot)      tPushFront(list, slot)
#define POP_RAW_FRONT(list)             tPopFront(list)
#define POP_RAW_BACK(list)              tPopBack(list)
#define PEEK_RAW_FRONT(list)            sPeekFront(list)
#define PEEK_RAW_BACK(list)             sPeekBack(list)
#define GET_RAW_AT(list, index)         tGetNodeAt(list, index)
#define INSERT_RAW_AT(list, idx, slot)  tInsertAt(list, idx, slot)
#define REMOVE_RAW_AT(list, idx)        tRemoveAt(list, idx)

#define DESTROY_LIST(list)              tDestroyList(list)
#define PRINT_LIST(list)                vPrintList(list)
#define REVERSE_LIST(list)              vReverseList(list)
#define IS_EMPTY(list)                  bIsListEmpty(list)

// --- INTS ---
#define PUSH_INT(list, val)             tPushIntBack(list, val)
#define PUSH_INT_FRONT(list, val)       tPushIntFront(list, val)
#define POP_INT(list)                   tPopIntFront(list)
#define POP_INT_BACK(list)              tPopIntBack(list)
#define GET_INT(list, index)            tGetIntAt(list, index)
#define PUSH_INTS(list, count, ...)     tPushIntListBack(list, count, __VA_ARGS__)
#define INSERT_INT(list, index, val)    tInsertIntAt(list, index, val)
#define REMOVE_INT(list, index)         tRemoveIntAt(list, index)
#define FIND_INT(list, target)          tFindInt(list, target)

// --- FLOATS ---
#define PUSH_FLOAT(list, val)           tPushFloatBack(list, val)
#define PUSH_FLOAT_FRONT(list, val)     tPushFloatFront(list, val)
#define POP_FLOAT(list)                 tPopFloatFront(list)
#define POP_FLOAT_BACK(list)            tPopFloatBack(list)
#define GET_FLOAT(list, index)          tGetFloatAt(list, index)
#define INSERT_FLOAT(list, index, val)  tInsertFloatAt(list, index, val)
#define REMOVE_FLOAT(list, index)       tRemoveFloatAt(list, index)
#define FIND_FLOAT(list, target)        tFindFloat(list, target)

// --- DOUBLES ---
#define PUSH_DOUBLE(list, val)          tPushDoubleBack(list, val)
#define PUSH_DOUBLE_FRONT(list, val)    tPushDoubleFront(list, val)
#define POP_DOUBLE(list)                tPopDoubleFront(list)
#define POP_DOUBLE_BACK(list)           tPopDoubleBack(list)
#define GET_DOUBLE(list, index)         tGetDoubleAt(list, index)
#define PUSH_DOUBLES(list, count, ...)  tPushDoublesBack(list, count, __VA_ARGS__)
#define INSERT_DOUBLE(list, idx, val)   tInsertDoubleAt(list, idx, val)
#define REMOVE_DOUBLE(list, index)      tRemoveDoubleAt(list, index)
#define FIND_DOUBLE(list, target)       tFindDouble(list, target)

// --- CHARS ---
#define PUSH_CHAR(list, val)            tPushCharBack(list, val)
#define PUSH_CHAR_FRONT(list, val)      tPushCharFront(list, val)
#define POP_CHAR(list)                  tPopCharFront(list)
#define POP_CHAR_BACK(list)             tPopCharBack(list)
#define GET_CHAR(list, index)           tGetCharAt(list, index)
#define PUSH_CHARS(list, count, ...)    tPushCharsBack(list, count, __VA_ARGS__)
#define INSERT_CHAR(list, index, val)   tInsertCharAt(list, index, val)
#define REMOVE_CHAR(list, index)        tRemoveCharAt(list, index)
#define FIND_CHAR(list, target)         tFindChar(list, target)

// --- LONGS ---
#define PUSH_LONG(list, val)            tPushLongBack(list, val)
#define PUSH_LONG_FRONT(list, val)      tPushLongFront(list, val)
#define POP_LONG(list)                  tPopLongFront(list)
#define POP_LONG_BACK(list)             tPopLongBack(list)
#define GET_LONG(list, index)           tGetLongAt(list, index)
#define INSERT_LONG(list, index, val)   tInsertLongAt(list, index, val)
#define REMOVE_LONG(list, index)        tRemoveLongAt(list, index)
#define FIND_LONG(list, target)         tFindLong(list, target)

// --- SHORTS ---
#define PUSH_SHORT(list, val)           tPushShortBack(list, val)
#define PUSH_SHORT_FRONT(list, val)     tPushShortFront(list, val)
#define POP_SHORT(list)                 tPopShortFront(list)
#define POP_SHORT_BACK(list)            tPopShortBack(list)
#define GET_SHORT(list, index)          tGetShortAt(list, index)
#define INSERT_SHORT(list, index, val)  tInsertShortAt(list, index, val)
#define REMOVE_SHORT(list, index)       tRemoveShortAt(list, index)
#define FIND_SHORT(list, target)        tFindShort(list, target)

// --- GENERIC POINTERS ---
#define PUSH_PTR(list, ptr, tag)        tPushPointerBack(list, ptr, tag)
#define PUSH_PTR_FRONT(list, ptr, tag)  tPushPointerFront(list, ptr, tag)
#define POP_PTR(list)                   tPopPointerFront(list)
#define POP_PTR_BACK(list)              tPopPointerBack(list)
#define GET_PTR(list, index)            tGetPointerAt(list, index)
#define INSERT_PTR(list, idx, ptr, tag) tInsertPointerAt(list, idx, ptr, tag)
#define REMOVE_PTR(list, index)         tRemovePointerAt(list, index)
#define FIND_PTR(list, ptr)             tFindPointer(list, ptr)

// --- STRUCTS ---
#define PUSH_STRUCT(list, ptr)          tPushStructBack(list, ptr)
#define PUSH_STRUCT_FRONT(list, ptr)    tPushStructFront(list, ptr)
#define POP_STRUCT(list)                tPopStructFront(list)
#define POP_STRUCT_BACK(list)           tPopStructBack(list)
#define GET_STRUCT(list, index)         tGetStructAt(list, index)
#define INSERT_STRUCT(list, index, ptr) tInsertStructAt(list, index, ptr)
#define REMOVE_STRUCT(list, index)      tRemoveStructAt(list, index)
#define FIND_STRUCT(list, ptr)          tFindStruct(list, ptr)

// --- UNIONS ---
#define PUSH_UNION(list, ptr)           tPushUnionBack(list, ptr)
#define PUSH_UNION_FRONT(list, ptr)     tPushUnionFront(list, ptr)
#define POP_UNION(list)                 tPopUnionFront(list)
#define POP_UNION_BACK(list)            tPopUnionBack(list)
#define GET_UNION(list, index)          tGetUnionAt(list, index)
#define INSERT_UNION(list, index, ptr)  tInsertUnionAt(list, index, ptr)
#define REMOVE_UNION(list, index)       tRemoveUnionAt(list, index)
#define FIND_UNION(list, ptr)           tFindUnion(list, ptr)

// --- SYSTEM RESOURCES (FUNC PTRS & FILES) ---
#define PUSH_FUNC(list, ptr)            tPushFuncPtrBack(list, ptr)
#define PUSH_FUNC_FRONT(list, ptr)      tPushFuncPtrFront(list, ptr)
#define POP_FUNC(list)                  tPopFuncPtrFront(list)
#define POP_FUNC_BACK(list)             tPopFuncPtrBack(list)
#define GET_FUNC(list, index)           tGetFuncPtrAt(list, index)
#define INSERT_FUNC(list, index, ptr)   tInsertFuncPtrAt(list, index, ptr)
#define REMOVE_FUNC(list, index)        tRemoveFuncPtrAt(list, index)
#define FIND_FUNC(list, ptr)            tFindFuncPtr(list, ptr)

#define PUSH_FILE(list, stream)         tPushFilePtrBack(list, stream)
#define PUSH_FILE_FRONT(list, stream)   tPushFilePtrFront(list, stream)
#define POP_FILE(list)                  tPopFilePtrFront(list)
#define POP_FILE_BACK(list)             tPopFilePtrBack(list)
#define GET_FILE(list, index)           tGetFilePtrAt(list, index)
#define INSERT_FILE(list, idx, stream)  tInsertFilePtrAt(list, idx, stream)
#define REMOVE_FILE(list, index)        tRemoveFilePtrAt(list, index)
#define FIND_FILE(list, stream)         tFindFilePtr(list, stream)

#endif // T_LL_DEF_H