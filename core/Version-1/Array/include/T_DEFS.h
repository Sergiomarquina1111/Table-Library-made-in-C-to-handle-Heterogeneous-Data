#ifndef T_DEFS_H
#define T_DEFS_H

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Type identification tags for primitives
typedef enum DATA_TYPE
{
    TYPE_EMPTY = 0,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_CHAR,
    TYPE_LONG,
    TYPE_SHORT
} DATA_TYPE;

// Type identification tags for single pointers
typedef enum POINTERS
{
    TYPE_NULL = 10,
    TYPE_VOID_STAR,
    TYPE_INT_STAR,
    TYPE_FLOAT_STAR,
    TYPE_CHAR_STAR,
    TYPE_DOUBLE_STAR,
    TYPE_LONG_STAR,
    TYPE_SHORT_STAR
} POINTERS;

// Type identification tags for double pointers
typedef enum DOUBLE_POINTERS
{
    TYPE_NULL_DOUBLE = 100,
    TYPE_VOID_STAR_DOUBLE,
    TYPE_INT_STAR_DOUBLE,
    TYPE_FLOAT_STAR_DOUBLE,
    TYPE_CHAR_STAR_DOUBLE,
    TYPE_DOUBLE_STAR_DOUBLE,
    TYPE_LONG_STAR_DOUBLE,
    TYPE_SHORT_STAR_DOUBLE
} DOUBLE_POINTERS;

// Type identification tags for triple pointers
typedef enum TRIPLE_POINTERS
{
    TYPE_NULL_TRIPLE = 1000,
    TYPE_VOID_STAR_TRIPLE,
    TYPE_INT_STAR_TRIPLE,
    TYPE_FLOAT_STAR_TRIPLE,
    TYPE_CHAR_STAR_TRIPLE,
    TYPE_DOUBLE_STAR_TRIPLE,
    TYPE_LONG_STAR_TRIPLE,
    TYPE_SHORT_STAR_TRIPLE
} TRIPLE_POINTERS;

// Type identification tags for complex and system types
typedef enum USER_DEFINED
{
    TYPE_NULL_USER_DEFINED = 2000,
    TYPE_STRUCT,
    TYPE_UNION,
    TYPE_STRUCT_PTR,
    TYPE_UNION_PTR,
    TYPE_FUNC_PTR,
    TYPE_FILE_PTR
} USER_DEFINED;

// Core data structure for individual elements
typedef struct
{
    void* ptr;
    short type;
} TableSlot;

// Core dynamic array collection structure
typedef struct
{
    TableSlot* slots;
    int capacity;
    int count;
} Table;

#define ARENA_ALIGN(x) (((x) + 7) & ~7)

typedef struct {
    uint32_t size;     // Total bytes of chunk (Header + Payload + Footer)
    uint32_t is_free;  // 1 = Free, 0 = Active
} BoundaryTag;

// The decoupled, engine-wide global struct
typedef struct {
    uint8_t* pool;
    size_t capacity;
} MemoryArena;

// Global instance - DEFINED in T_LIBFXNS_ARRAY_BASED.cpp, declared here so every translation
// unit that includes this header shares the same arena instead of each getting its own copy.
extern MemoryArena g_MasterArena;

// Arena allocator core - replaces malloc()/free() for Table payloads once vGetTable() has
// mapped the arena (arena_bytes > 0). Falls back to plain malloc()/free() automatically when
// the arena isn't initialized or has no block big enough, so callers never need to know which
// path a given pointer came from.
extern void* pAllocArena(size_t size);
extern void  vFreeArena(void* ptr);

// Callback signature used by vForEachSlotTable()
typedef void (*TableVisitor)(int index, TableSlot slot);

// Lifecycle and core collection API
extern void vGetTable(Table* obj, int total_slots, size_t arena_bytes);
extern void vDropTable(Table* obj);
extern bool bPushSlot(Table* obj, TableSlot cobj);
extern TableSlot sPopSlot(Table* obj);
extern void vPrintTable(const Table* obj);

// Primitive push functions
extern bool bPushInt(Table* obj, int val);
extern bool bPushFloat(Table* obj, float val);
extern bool bPushDouble(Table* obj, double val);
extern bool bPushLong(Table* obj, long val);
extern bool bPushShort(Table* obj, short val);
extern bool bPushChar(Table* obj, char val);

// Pointer push functions
extern bool bPushPtr(Table* obj, void* val, short pointer_type);
extern bool bPushDPtr(Table* obj, void** val, short double_pointer_type);
extern bool bPushTPtr(Table* obj, void*** val, short triple_pointer_type);

// User-defined and system type push functions
extern bool bPushStruct(Table* obj, void* struct_data, size_t size);
extern bool bPushUnion(Table* obj, void* union_ptr, size_t union_size);
extern bool bPushFunc(Table* obj, void* func_ptr);
extern bool bPushFile(Table* obj, FILE* file_ptr);

// Variadic bulk push function
extern bool bPushList(Table* obj, int count, ...);

// Stack-manipulation and query API
extern bool bDupTopTable(Table* obj);
extern bool bSwapTopTable(Table* obj);
extern int  iCountOfTypeTable(const Table* obj, short type);
extern bool bContainsTypeTable(const Table* obj, short type);
extern int  iFindTypeTable(const Table* obj, short type);
extern int    iSumIntTable(const Table* obj);
extern double dSumFloatTable(const Table* obj);
extern bool bMinIntTable(const Table* obj, int* out);
extern bool bMaxIntTable(const Table* obj, int* out);
extern void vReverseTable(Table* obj);
extern int  iPushIntArrayTable(Table* obj, const int* arr, int n);
extern int  iPushFloatArrayTable(Table* obj, const float* arr, int n);
extern bool bCloneTable(Table* dest, const Table* src);
extern TableSlot   sGetSlotAtTable(const Table* obj, int index);
extern short        sGetTypeAtTable(const Table* obj, int index);
extern void         vForEachSlotTable(const Table* obj, TableVisitor visitor);
extern const char* sTypeNameTable(short type);

// Heap-only capacity management and mid-array editing API
extern void vShrinkToFitTable(Table* obj);
extern bool vReserveTable(Table* obj, int additional_capacity);
extern bool bInsertAtTable(Table* obj, int index, TableSlot node);
extern bool bRemoveAtTable(Table* obj, int index);

// Developer macro API for primitives
#define PUSH_INT(obj_ptr, val)             bPushInt((obj_ptr), (val))
#define PUSH_FLOAT(obj_ptr, val)           bPushFloat((obj_ptr), (val))
#define PUSH_DOUBLE(obj_ptr, val)          bPushDouble((obj_ptr), (val))
#define PUSH_LONG(obj_ptr, val)            bPushLong((obj_ptr), (val))
#define PUSH_SHORT(obj_ptr, val)           bPushShort((obj_ptr), (val))
#define PUSH_CHAR(obj_ptr, val)            bPushChar((obj_ptr), (val))

// Developer macro API for single pointers
#define PUSH_VOID_PTR(obj, val)            bPushPtr((obj), (void*)(val), TYPE_VOID_STAR)
#define PUSH_INT_PTR(obj, val)             bPushPtr((obj), (void*)(val), TYPE_INT_STAR)
#define PUSH_FLOAT_PTR(obj, val)           bPushPtr((obj), (void*)(val), TYPE_FLOAT_STAR)
#define PUSH_CHAR_PTR(obj, val)            bPushPtr((obj), (void*)(val), TYPE_CHAR_STAR)
#define PUSH_DOUBLE_PTR(obj, val)          bPushPtr((obj), (void*)(val), TYPE_DOUBLE_STAR)
#define PUSH_LONG_PTR(obj, val)            bPushPtr((obj), (void*)(val), TYPE_LONG_STAR)
#define PUSH_SHORT_PTR(obj, val)           bPushPtr((obj), (void*)(val), TYPE_SHORT_STAR)

// Developer macro API for double pointers
#define PUSH_VOID_DPTR(obj, val)           bPushDPtr((obj), (void**)(val), TYPE_VOID_STAR_DOUBLE)
#define PUSH_INT_DPTR(obj, val)            bPushDPtr((obj), (void**)(val), TYPE_INT_STAR_DOUBLE)
#define PUSH_FLOAT_DPTR(obj, val)          bPushDPtr((obj), (void**)(val), TYPE_FLOAT_STAR_DOUBLE)
#define PUSH_CHAR_DPTR(obj, val)           bPushDPtr((obj), (void**)(val), TYPE_CHAR_STAR_DOUBLE)
#define PUSH_DOUBLE_DPTR(obj, val)         bPushDPtr((obj), (void**)(val), TYPE_DOUBLE_STAR_DOUBLE)
#define PUSH_LONG_DPTR(obj, val)           bPushDPtr((obj), (void**)(val), TYPE_LONG_STAR_DOUBLE)
#define PUSH_SHORT_DPTR(obj, val)          bPushDPtr((obj), (void**)(val), TYPE_SHORT_STAR_DOUBLE)

// Developer macro API for triple pointers
#define PUSH_VOID_TPTR(obj, val)           bPushTPtr((obj), (void***)(val), TYPE_VOID_STAR_TRIPLE)
#define PUSH_INT_TPTR(obj, val)            bPushTPtr((obj), (void***)(val), TYPE_INT_STAR_TRIPLE)
#define PUSH_FLOAT_TPTR(obj, val)          bPushTPtr((obj), (void***)(val), TYPE_FLOAT_STAR_TRIPLE)
#define PUSH_CHAR_TPTR(obj, val)           bPushTPtr((obj), (void***)(val), TYPE_CHAR_STAR_TRIPLE)
#define PUSH_DOUBLE_TPTR(obj, val)         bPushTPtr((obj), (void***)(val), TYPE_DOUBLE_STAR_TRIPLE)
#define PUSH_LONG_TPTR(obj, val)           bPushTPtr((obj), (void***)(val), TYPE_LONG_STAR_TRIPLE)
#define PUSH_SHORT_TPTR(obj, val)          bPushTPtr((obj), (void***)(val), TYPE_SHORT_STAR_TRIPLE)

// Complex type & system interop macros
#define PUSH_STRUCT(obj, val)              bPushStruct((obj), (void*)(val), sizeof(*(val)))
#define PUSH_UNION(obj, union_ptr)         bPushUnion((obj), (void*)(union_ptr), sizeof(*(union_ptr)))
#define PUSH_FUNC(obj, func_ptr)           bPushFunc((obj), (void*)(func_ptr))
#define PUSH_FILE(obj, fp)                 bPushFile((obj), (fp))

// Memory extraction and dereference API
#define GET_STRUCT_MEMBER(elem_ptr, offset, type) (*(type*)((char*)((elem_ptr)->ptr) + (offset)))
#define GET_UNION_MEMBER(elem_ptr, type)          (*(type*)((elem_ptr)->ptr))

#endif 
// T_DEFS_H