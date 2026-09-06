#ifndef TABLE_H
#define TABLE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>
#include <string.h>
#include <stddef.h>
#include <limits.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#define INDEX_NULL 0xFFFF

#ifndef HASH_POOL_CAPACITY
#define HASH_POOL_CAPACITY 1024
#endif

#if HASH_POOL_CAPACITY >= INDEX_NULL
#error "HASH_POOL_CAPACITY must be strictly less than 65535 to preserve INDEX_NULL sentinel."
#endif

#ifndef HASH_TABLE_SIZE
#define HASH_TABLE_SIZE 67
#endif

#define STACK_TABLE_CAPACITY 100

#define ARENA_ALIGN(x) (((x) + 7) & ~7)

#ifndef TABLE_STACK_DEFAULT_CAPACITY
#define TABLE_STACK_DEFAULT_CAPACITY 32
#endif

typedef struct TableSlot_DA
{
    void* ptr;
    short type;
} TableSlot_DA;

typedef struct
{
    TableSlot_DA* slots;
    int capacity;
    int count;
} Table;

typedef struct
{
    uint32_t size;
    uint32_t is_free;
} BoundaryTag;

typedef struct
{
    uint8_t* pool;
    size_t capacity;
} MemoryArena;

extern MemoryArena g_MasterArena;

extern void* pAllocArena(size_t size);
extern void  vFreeArena(void* ptr);

typedef void (*TableVisitor)(int index, TableSlot_DA slot);

extern void vGetTable(Table* obj, int total_slots, size_t arena_bytes);
extern void vDropTable(Table* obj);
extern bool bPushSlot(Table* obj, TableSlot_DA cobj);
extern TableSlot_DA sPopSlot(Table* obj);
extern void vPrintTable(const Table* obj);

extern bool bPushInt(Table* obj, int val);
extern bool bPushFloat(Table* obj, float val);
extern bool bPushDouble(Table* obj, double val);
extern bool bPushLong(Table* obj, long val);
extern bool bPushShort(Table* obj, short val);
extern bool bPushChar(Table* obj, char val);

extern bool bPushPtr(Table* obj, void* val, short pointer_type);
extern bool bPushDPtr(Table* obj, void** val, short double_pointer_type);
extern bool bPushTPtr(Table* obj, void*** val, short triple_pointer_type);

extern bool bPushStruct(Table* obj, void* struct_data, size_t size);
extern bool bPushUnion(Table* obj, void* union_ptr, size_t union_size);
extern bool bPushFunc(Table* obj, void* func_ptr);
extern bool bPushFile(Table* obj, FILE* file_ptr);

extern bool bPushList(Table* obj, int count, ...);

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
extern TableSlot_DA   sGetSlotAtTable(const Table* obj, int index);
extern short          sGetTypeAtTable(const Table* obj, int index);
extern void           vForEachSlotTable(const Table* obj, TableVisitor visitor);
extern const char* sTypeNameTable(short type);

extern void vShrinkToFitTable(Table* obj);
extern bool vReserveTable(Table* obj, int additional_capacity);
extern bool bInsertAtTable(Table* obj, int index, TableSlot_DA node);
extern bool bRemoveAtTable(Table* obj, int index);

#define PUSH_INT(obj_ptr, val)             bPushInt((obj_ptr), (val))
#define PUSH_FLOAT(obj_ptr, val)           bPushFloat((obj_ptr), (val))
#define PUSH_DOUBLE(obj_ptr, val)          bPushDouble((obj_ptr), (val))
#define PUSH_LONG(obj_ptr, val)            bPushLong((obj_ptr), (val))
#define PUSH_SHORT(obj_ptr, val)           bPushShort((obj_ptr), (val))
#define PUSH_CHAR(obj_ptr, val)            bPushChar((obj_ptr), (val))

#define PUSH_VOID_PTR(obj, val)            bPushPtr((obj), (void*)(val), TYPE_VOID_STAR)
#define PUSH_INT_PTR(obj, val)             bPushPtr((obj), (void*)(val), TYPE_INT_STAR)
#define PUSH_FLOAT_PTR(obj, val)           bPushPtr((obj), (void*)(val), TYPE_FLOAT_STAR)
#define PUSH_CHAR_PTR(obj, val)            bPushPtr((obj), (void*)(val), TYPE_CHAR_STAR)
#define PUSH_DOUBLE_PTR(obj, val)          bPushPtr((obj), (void*)(val), TYPE_DOUBLE_STAR)
#define PUSH_LONG_PTR(obj, val)            bPushPtr((obj), (void*)(val), TYPE_LONG_STAR)
#define PUSH_SHORT_PTR(obj, val)           bPushPtr((obj), (void*)(val), TYPE_SHORT_STAR)

#define PUSH_VOID_DPTR(obj, val)           bPushDPtr((obj), (void**)(val), TYPE_VOID_STAR_DOUBLE)
#define PUSH_INT_DPTR(obj, val)            bPushDPtr((obj), (void**)(val), TYPE_INT_STAR_DOUBLE)
#define PUSH_FLOAT_DPTR(obj, val)          bPushDPtr((obj), (void**)(val), TYPE_FLOAT_STAR_DOUBLE)
#define PUSH_CHAR_DPTR(obj, val)           bPushDPtr((obj), (void**)(val), TYPE_CHAR_STAR_DOUBLE)
#define PUSH_DOUBLE_DPTR(obj, val)         bPushDPtr((obj), (void**)(val), TYPE_DOUBLE_STAR_DOUBLE)
#define PUSH_LONG_DPTR(obj, val)           bPushDPtr((obj), (void**)(val), TYPE_LONG_STAR_DOUBLE)
#define PUSH_SHORT_DPTR(obj, val)          bPushDPtr((obj), (void**)(val), TYPE_SHORT_STAR_DOUBLE)

#define PUSH_VOID_TPTR(obj, val)           bPushTPtr((obj), (void***)(val), TYPE_VOID_STAR_TRIPLE)
#define PUSH_INT_TPTR(obj, val)            bPushTPtr((obj), (void***)(val), TYPE_INT_STAR_TRIPLE)
#define PUSH_FLOAT_TPTR(obj, val)          bPushTPtr((obj), (void***)(val), TYPE_FLOAT_STAR_TRIPLE)
#define PUSH_CHAR_TPTR(obj, val)           bPushTPtr((obj), (void***)(val), TYPE_CHAR_STAR_TRIPLE)
#define PUSH_DOUBLE_TPTR(obj, val)         bPushTPtr((obj), (void***)(val), TYPE_DOUBLE_STAR_TRIPLE)
#define PUSH_LONG_TPTR(obj, val)           bPushTPtr((obj), (void***)(val), TYPE_LONG_STAR_TRIPLE)
#define PUSH_SHORT_TPTR(obj, val)          bPushTPtr((obj), (void***)(val), TYPE_SHORT_STAR_TRIPLE)

#define PUSH_STRUCT(obj, val)              bPushStruct((obj), (void*)(val), sizeof(*(val)))
#define PUSH_UNION(obj, union_ptr)         bPushUnion((obj), (void*)(union_ptr), sizeof(*(union_ptr)))
#define PUSH_FUNC(obj, func_ptr)           bPushFunc((obj), (void*)(func_ptr))
#define PUSH_FILE(obj, fp)                 bPushFile((obj), (fp))

#define GET_STRUCT_MEMBER(elem_ptr, offset, type) (*(type*)((char*)((elem_ptr)->ptr) + (offset)))
#define GET_UNION_MEMBER(elem_ptr, type)          (*(type*)((elem_ptr)->ptr))

// Type-checked slot access - validates the index AND the stored type tag before
// handing back the slot, instead of trusting the caller to already know what's
// there. Returns false (leaving *out untouched) on an out-of-range index or a
// type-tag mismatch; returns true and copies the slot into *out on success. Use
// this before GET_STRUCT_MEMBER/GET_UNION_MEMBER or a raw ->ptr cast whenever the
// expected type isn't already guaranteed by surrounding logic.
extern bool bTryGetSlotAtTable(const Table* obj, int index, short expected_type, TableSlot_DA* out);

// Exposes the table's heap-ownership rule for a given type tag: true if a slot of
// this type owns a heap allocation that vDropTable()/vFreeArena() must free, false
// if it's a raw/user-managed pointer the library never touches. Lets callers reason
// about push/drop semantics for a type without having to read the .cpp source.
extern bool bIsOwnedTypeTable(short type);

static_assert(sizeof(void*) >= sizeof(double), "TableStack assumes a 64-bit build (sizeof(void*) >= 8) so double/long fit in one cell.");

typedef struct TableSlot_SA
{
    void* ptr;
    short type;
} TableSlot_SA;

typedef struct
{
    TableSlot_SA* slots;
    int        capacity;
    int        count;
} TableStack;

typedef void (*TableStackVisitor)(int index, TableSlot_SA slot);

#define vGetTableStack(name, CAP) \
    TableSlot_SA   name##_backing[(CAP)] = { { 0 } }; \
    TableStack  name = { name##_backing, (CAP), 0 }; \
    printf("\n=== Initializing Table (stack) ===\n"); \
    printf("[vGetTableStack] Table initialized (Capacity: %d).\n\n", (CAP))

#define vGetTableStackDefault(name) vGetTableStack(name, TABLE_STACK_DEFAULT_CAPACITY)

static inline bool bIsTableStackEmpty(const TableStack* obj)
{
    return (obj == NULL) || (obj->count == 0);
}

static inline bool bIsTableStackFull(const TableStack* obj)
{
    return (obj != NULL) && (obj->count >= obj->capacity);
}

static inline int iRemainingStack(const TableStack* obj)
{
    return obj ? (obj->capacity - obj->count) : 0;
}

static inline int iCountStack(const TableStack* obj)
{
    return obj ? obj->count : 0;
}

static inline int iCapacityStack(const TableStack* obj)
{
    return obj ? obj->capacity : 0;
}

static inline short sPeekTypeStack(const TableStack* obj)
{
    if (obj == NULL || obj->count == 0) return TYPE_EMPTY;
    return obj->slots[obj->count - 1].type;
}

static inline TableSlot_SA sPeekSlotStack(const TableStack* obj)
{
    TableSlot_SA empty = { NULL, TYPE_EMPTY };
    if (obj == NULL || obj->count == 0) return empty;
    return obj->slots[obj->count - 1];
}

#define TABLE_STACK_LEN(tb)  ((tb).count)
#define TABLE_STACK_CAP(tb)  ((tb).capacity)

extern bool       bPushSlotStack(TableStack* obj, TableSlot_SA node);
extern TableSlot_SA  sPopSlotStack(TableStack* obj);
extern bool       bDropTopStack(TableStack* obj);
extern void       vDropTableStack(TableStack* obj);

extern bool bPushIntStack(TableStack* obj, int val);
extern bool bPushFloatStack(TableStack* obj, float val);
extern bool bPushDoubleStack(TableStack* obj, double val);
extern bool bPushLongStack(TableStack* obj, long val);
extern bool bPushShortStack(TableStack* obj, short val);
extern bool bPushCharStack(TableStack* obj, char val);
extern bool bPushPtrStack(TableStack* obj, void* val, short pointer_type);
extern bool bPushDPtrStack(TableStack* obj, void** val, short double_pointer_type);
extern bool bPushTPtrStack(TableStack* obj, void*** val, short triple_pointer_type);
extern bool bPushStructStack(TableStack* obj, void* struct_data, size_t size);
extern bool bPushUnionStack(TableStack* obj, void* union_data, size_t union_size);
extern bool bPushFuncStack(TableStack* obj, void* func_ptr);
extern bool bPushFileStack(TableStack* obj, FILE* fp);

extern void vPrintTableStack(const TableStack* obj);
extern void vPrintSlotStack(TableSlot_SA slot);
extern bool bPushListStack(TableStack* obj, int count, ...);
extern void vForEachSlotStack(const TableStack* obj, TableStackVisitor visitor);

extern TableSlot_SA sGetSlotAtStack(const TableStack* obj, int index);
extern short        sGetTypeAtStack(const TableStack* obj, int index);

extern bool bDupTopStack(TableStack* obj);
extern bool bSwapTopStack(TableStack* obj);

extern int  iCountOfTypeStack(const TableStack* obj, short type);
extern bool bContainsTypeStack(const TableStack* obj, short type);
extern int  iFindTypeStack(const TableStack* obj, short type);
extern int    iSumIntStack(const TableStack* obj);
extern double dSumFloatStack(const TableStack* obj);
extern bool bMinIntStack(const TableStack* obj, int* out);
extern bool bMaxIntStack(const TableStack* obj, int* out);

extern void vReverseStack(TableStack* obj);
extern int  iPushIntArrayStack(TableStack* obj, const int* arr, int n);
extern int  iPushFloatArrayStack(TableStack* obj, const float* arr, int n);
extern bool bCloneTableStack(TableStack* dest, const TableStack* src);

extern const char* sTypeNameStack(short type);

#define PUSH_INT_STACK(obj_ptr, val)             bPushIntStack((obj_ptr), (val))
#define PUSH_FLOAT_STACK(obj_ptr, val)           bPushFloatStack((obj_ptr), (val))
#define PUSH_DOUBLE_STACK(obj_ptr, val)          bPushDoubleStack((obj_ptr), (val))
#define PUSH_LONG_STACK(obj_ptr, val)            bPushLongStack((obj_ptr), (val))
#define PUSH_SHORT_STACK(obj_ptr, val)           bPushShortStack((obj_ptr), (val))
#define PUSH_CHAR_STACK(obj_ptr, val)            bPushCharStack((obj_ptr), (val))

#define PUSH_INT_PTR_STACK(obj, val)             bPushPtrStack((obj), (void*)(val), TYPE_INT_STAR)
#define PUSH_FLOAT_PTR_STACK(obj, val)           bPushPtrStack((obj), (void*)(val), TYPE_FLOAT_STAR)
#define PUSH_CHAR_PTR_STACK(obj, val)            bPushPtrStack((obj), (void*)(val), TYPE_CHAR_STAR)
#define PUSH_DOUBLE_PTR_STACK(obj, val)          bPushPtrStack((obj), (void*)(val), TYPE_DOUBLE_STAR)
#define PUSH_LONG_PTR_STACK(obj, val)            bPushPtrStack((obj), (void*)(val), TYPE_LONG_STAR)
#define PUSH_SHORT_PTR_STACK(obj, val)           bPushPtrStack((obj), (void*)(val), TYPE_SHORT_STAR)
#define PUSH_VOID_PTR_STACK(obj, val)            bPushPtrStack((obj), (void*)(val), TYPE_VOID_STAR)

#define PUSH_INT_DPTR_STACK(obj, val)            bPushDPtrStack((obj), (void**)(val), TYPE_INT_STAR_DOUBLE)
#define PUSH_FLOAT_DPTR_STACK(obj, val)          bPushDPtrStack((obj), (void**)(val), TYPE_FLOAT_STAR_DOUBLE)
#define PUSH_CHAR_DPTR_STACK(obj, val)           bPushDPtrStack((obj), (void**)(val), TYPE_CHAR_STAR_DOUBLE)
#define PUSH_DOUBLE_DPTR_STACK(obj, val)         bPushDPtrStack((obj), (void**)(val), TYPE_DOUBLE_STAR_DOUBLE)
#define PUSH_LONG_DPTR_STACK(obj, val)           bPushDPtrStack((obj), (void**)(val), TYPE_LONG_STAR_DOUBLE)
#define PUSH_SHORT_DPTR_STACK(obj, val)          bPushDPtrStack((obj), (void**)(val), TYPE_SHORT_STAR_DOUBLE)
#define PUSH_VOID_DPTR_STACK(obj, val)           bPushDPtrStack((obj), (void**)(val), TYPE_VOID_STAR_DOUBLE)

#define PUSH_INT_TPTR_STACK(obj, val)            bPushTPtrStack((obj), (void***)(val), TYPE_INT_STAR_TRIPLE)
#define PUSH_FLOAT_TPTR_STACK(obj, val)          bPushTPtrStack((obj), (void***)(val), TYPE_FLOAT_STAR_TRIPLE)
#define PUSH_CHAR_TPTR_STACK(obj, val)           bPushTPtrStack((obj), (void***)(val), TYPE_CHAR_STAR_TRIPLE)
#define PUSH_DOUBLE_TPTR_STACK(obj, val)         bPushTPtrStack((obj), (void***)(val), TYPE_DOUBLE_STAR_TRIPLE)
#define PUSH_LONG_TPTR_STACK(obj, val)           bPushTPtrStack((obj), (void***)(val), TYPE_LONG_STAR_TRIPLE)
#define PUSH_SHORT_TPTR_STACK(obj, val)          bPushTPtrStack((obj), (void***)(val), TYPE_SHORT_STAR_TRIPLE)
#define PUSH_VOID_TPTR_STACK(obj, val)           bPushTPtrStack((obj), (void***)(val), TYPE_VOID_STAR_TRIPLE)

#define PUSH_STRUCT_STACK(obj, val)              bPushStructStack((obj), (void*)(val), sizeof(*(val)))
#define PUSH_UNION_STACK(obj, union_ptr)         bPushUnionStack((obj), (void*)(union_ptr), sizeof(*(union_ptr)))
#define PUSH_FUNC_STACK(obj, func_ptr)           bPushFuncStack((obj), (void*)(func_ptr))
#define PUSH_FILE_STACK(obj, fp)                 bPushFileStack((obj), (fp))

#define GET_STACK_VALUE(elem, T) (*(T*)&((elem).ptr))

// Type-checked slot access for TableStack - same contract as bTryGetSlotAtTable():
// false (untouched *out) on bad index or type-tag mismatch, true + copied slot on
// success. Use before GET_STACK_VALUE whenever the expected type isn't already
// guaranteed by surrounding logic.
extern bool bTryGetSlotAtStackTyped(const TableStack* obj, int index, short expected_type, TableSlot_SA* out);

typedef struct TableSlot_LL
{
    void* data;
    short type;
} TableSlot_LL;

typedef struct tNode
{
    TableSlot_LL slot;
    struct tNode* next;
    struct tNode* prev;
} tNode;

typedef struct TableList
{
    tNode* tNodeHead;
    tNode* tNodeTail;
    tNode* tNodePrev;
    int count;
} TableList;

void tFormLinkedTable(TableList* list);
tNode* tCreateNode(TableSlot_LL payload);
void tDestroyList(TableList* list);
void vPrintList(const TableList* list);
bool bIsListEmpty(const TableList* list);

void tPushBack(TableList* list, TableSlot_LL payload);
void tPushFront(TableList* list, TableSlot_LL payload);
TableSlot_LL tPopFront(TableList* list);
TableSlot_LL tPopBack(TableList* list);
TableSlot_LL sPeekFront(const TableList* list);
TableSlot_LL sPeekBack(const TableList* list);
TableSlot_LL tGetNodeAt(const TableList* list, int target_index);

void tInsertAt(TableList* list, int index, TableSlot_LL payload);
TableSlot_LL tRemoveAt(TableList* list, int index);
void vReverseList(TableList* list);

void tPushIntBack(TableList* list, int value);
void tPushIntFront(TableList* list, int value);
int tGetIntAt(const TableList* list, int index);
int tPopIntFront(TableList* list);
int tPopIntBack(TableList* list);
void tPushIntListBack(TableList* list, int count, ...);
void tInsertIntAt(TableList* list, int index, int value);
int tRemoveIntAt(TableList* list, int index);
int tFindInt(const TableList* list, int target);

void tPushFloatBack(TableList* list, float value);
void tPushFloatFront(TableList* list, float value);
float tGetFloatAt(const TableList* list, int index);
float tPopFloatFront(TableList* list);
float tPopFloatBack(TableList* list);
void tInsertFloatAt(TableList* list, int index, float value);
float tRemoveFloatAt(TableList* list, int index);
int tFindFloat(const TableList* list, float target);

void tPushDoubleBack(TableList* list, double value);
void tPushDoubleFront(TableList* list, double value);
double tGetDoubleAt(const TableList* list, int index);
double tPopDoubleFront(TableList* list);
double tPopDoubleBack(TableList* list);
void tPushDoublesBack(TableList* list, int count, ...);
void tInsertDoubleAt(TableList* list, int index, double value);
double tRemoveDoubleAt(TableList* list, int index);
int tFindDouble(const TableList* list, double target);

void tPushCharBack(TableList* list, char value);
void tPushCharFront(TableList* list, char value);
char tGetCharAt(const TableList* list, int index);
char tPopCharFront(TableList* list);
char tPopCharBack(TableList* list);
void tPushCharsBack(TableList* list, int count, ...);
void tInsertCharAt(TableList* list, int index, char value);
char tRemoveCharAt(TableList* list, int index);
int tFindChar(const TableList* list, char target);

void tPushLongBack(TableList* list, long value);
void tPushLongFront(TableList* list, long value);
long tGetLongAt(const TableList* list, int index);
long tPopLongFront(TableList* list);
long tPopLongBack(TableList* list);
void tInsertLongAt(TableList* list, int index, long value);
long tRemoveLongAt(TableList* list, int index);
int tFindLong(const TableList* list, long target);

void tPushShortBack(TableList* list, short value);
void tPushShortFront(TableList* list, short value);
short tGetShortAt(const TableList* list, int index);
short tPopShortFront(TableList* list);
short tPopShortBack(TableList* list);
void tInsertShortAt(TableList* list, int index, short value);
short tRemoveShortAt(TableList* list, int index);
int tFindShort(const TableList* list, short target);

// Type-checked primitive getters - safe alternatives to the plain get functions
// above. Each validates the stored type tag before handing back a value; on an
// out-of-range index or a type mismatch, returns false and leaves *out untouched
// instead of silently reinterpreting the wrong bytes.
bool bTryGetIntAt(const TableList* list, int index, int* out);
bool bTryGetFloatAt(const TableList* list, int index, float* out);
bool bTryGetDoubleAt(const TableList* list, int index, double* out);
bool bTryGetCharAt(const TableList* list, int index, char* out);
bool bTryGetLongAt(const TableList* list, int index, long* out);
bool bTryGetShortAt(const TableList* list, int index, short* out);

void tPushPointerBack(TableList* list, void* ptr, short pointer_tag);
void tPushPointerFront(TableList* list, void* ptr, short pointer_tag);
void* tGetPointerAt(const TableList* list, int index);
void* tPopPointerFront(TableList* list);
void* tPopPointerBack(TableList* list);
void tInsertPointerAt(TableList* list, int index, void* ptr, short pointer_tag);
void* tRemovePointerAt(TableList* list, int index);
int tFindPointer(const TableList* list, void* target_ptr);

void tPushStructBack(TableList* list, void* struct_ptr);
void tPushStructFront(TableList* list, void* struct_ptr);
void* tGetStructAt(const TableList* list, int index);
void* tPopStructFront(TableList* list);
void* tPopStructBack(TableList* list);
void tInsertStructAt(TableList* list, int index, void* struct_ptr);
void* tRemoveStructAt(TableList* list, int index);
int tFindStruct(const TableList* list, void* target_struct_ptr);

void tPushUnionBack(TableList* list, void* union_ptr);
void tPushUnionFront(TableList* list, void* union_ptr);
void* tGetUnionAt(const TableList* list, int index);
void* tPopUnionFront(TableList* list);
void* tPopUnionBack(TableList* list);
void tInsertUnionAt(TableList* list, int index, void* union_ptr);
void* tRemoveUnionAt(TableList* list, int index);
int tFindUnion(const TableList* list, void* target_union_ptr);

void tPushFuncPtrBack(TableList* list, void* func_ptr);
void tPushFuncPtrFront(TableList* list, void* func_ptr);
void* tGetFuncPtrAt(const TableList* list, int index);
void* tPopFuncPtrFront(TableList* list);
void* tPopFuncPtrBack(TableList* list);
void tInsertFuncPtrAt(TableList* list, int index, void* func_ptr);
void* tRemoveFuncPtrAt(TableList* list, int index);
int tFindFuncPtr(const TableList* list, void* target_func_ptr);

void tPushFilePtrBack(TableList* list, FILE* file_stream);
void tPushFilePtrFront(TableList* list, FILE* file_stream);
FILE* tGetFilePtrAt(const TableList* list, int index);
FILE* tPopFilePtrFront(TableList* list);
FILE* tPopFilePtrBack(TableList* list);
void tInsertFilePtrAt(TableList* list, int index, FILE* file_stream);
FILE* tRemoveFilePtrAt(TableList* list, int index);
int tFindFilePtr(const TableList* list, FILE* target_stream);

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

#define PUSH_INT_LL(list, val)          tPushIntBack(list, val)
#define PUSH_INT_FRONT(list, val)       tPushIntFront(list, val)
#define POP_INT(list)                   tPopIntFront(list)
#define POP_INT_BACK(list)              tPopIntBack(list)
#define GET_INT(list, index)            tGetIntAt(list, index)
#define PUSH_INTS(list, count, ...)     tPushIntListBack(list, count, __VA_ARGS__)
#define INSERT_INT(list, index, val)    tInsertIntAt(list, index, val)
#define REMOVE_INT(list, index)         tRemoveIntAt(list, index)
#define FIND_INT(list, target)          tFindInt(list, target)

#define PUSH_FLOAT_LL(list, val)        tPushFloatBack(list, val)
#define PUSH_FLOAT_FRONT(list, val)     tPushFloatFront(list, val)
#define POP_FLOAT(list)                 tPopFloatFront(list)
#define POP_FLOAT_BACK(list)            tPopFloatBack(list)
#define GET_FLOAT(list, index)          tGetFloatAt(list, index)
#define INSERT_FLOAT(list, index, val)  tInsertFloatAt(list, index, val)
#define REMOVE_FLOAT(list, index)       tRemoveFloatAt(list, index)
#define FIND_FLOAT(list, target)        tFindFloat(list, target)

#define PUSH_DOUBLE_LL(list, val)       tPushDoubleBack(list, val)
#define PUSH_DOUBLE_FRONT(list, val)    tPushDoubleFront(list, val)
#define POP_DOUBLE(list)                tPopDoubleFront(list)
#define POP_DOUBLE_BACK(list)           tPopDoubleBack(list)
#define GET_DOUBLE(list, index)         tGetDoubleAt(list, index)
#define PUSH_DOUBLES(list, count, ...)  tPushDoublesBack(list, count, __VA_ARGS__)
#define INSERT_DOUBLE(list, idx, val)   tInsertDoubleAt(list, idx, val)
#define REMOVE_DOUBLE(list, index)      tRemoveDoubleAt(list, index)
#define FIND_DOUBLE(list, target)       tFindDouble(list, target)

#define PUSH_CHAR_LL(list, val)         tPushCharBack(list, val)
#define PUSH_CHAR_FRONT(list, val)      tPushCharFront(list, val)
#define POP_CHAR(list)                  tPopCharFront(list)
#define POP_CHAR_BACK(list)             tPopCharBack(list)
#define GET_CHAR(list, index)           tGetCharAt(list, index)
#define PUSH_CHARS(list, count, ...)    tPushCharsBack(list, count, __VA_ARGS__)
#define INSERT_CHAR(list, index, val)   tInsertCharAt(list, index, val)
#define REMOVE_CHAR(list, index)        tRemoveCharAt(list, index)
#define FIND_CHAR(list, target)         tFindChar(list, target)

#define PUSH_LONG_LL(list, val)         tPushLongBack(list, val)
#define PUSH_LONG_FRONT(list, val)      tPushLongFront(list, val)
#define POP_LONG(list)                  tPopLongFront(list)
#define POP_LONG_BACK(list)             tPopLongBack(list)
#define GET_LONG(list, index)           tGetLongAt(list, index)
#define INSERT_LONG(list, index, val)   tInsertLongAt(list, index, val)
#define REMOVE_LONG(list, index)        tRemoveLongAt(list, index)
#define FIND_LONG(list, target)         tFindLong(list, target)

#define PUSH_SHORT_LL(list, val)        tPushShortBack(list, val)
#define PUSH_SHORT_FRONT(list, val)     tPushShortFront(list, val)
#define POP_SHORT(list)                 tPopShortFront(list)
#define POP_SHORT_BACK(list)            tPopShortBack(list)
#define GET_SHORT(list, index)          tGetShortAt(list, index)
#define INSERT_SHORT(list, index, val)  tInsertShortAt(list, index, val)
#define REMOVE_SHORT(list, index)       tRemoveShortAt(list, index)
#define FIND_SHORT(list, target)        tFindShort(list, target)

#define PUSH_PTR(list, ptr, tag)        tPushPointerBack(list, ptr, tag)
#define PUSH_PTR_FRONT(list, ptr, tag)  tPushPointerFront(list, ptr, tag)
#define POP_PTR(list)                   tPopPointerFront(list)
#define POP_PTR_BACK(list)              tPopPointerBack(list)
#define GET_PTR(list, index)            tGetPointerAt(list, index)
#define INSERT_PTR(list, idx, ptr, tag) tInsertPointerAt(list, idx, ptr, tag)
#define REMOVE_PTR(list, index)         tRemovePointerAt(list, index)
#define FIND_PTR(list, ptr)             tFindPointer(list, ptr)

#define PUSH_STRUCT_LL(list, ptr)       tPushStructBack(list, ptr)
#define PUSH_STRUCT_FRONT(list, ptr)    tPushStructFront(list, ptr)
#define POP_STRUCT(list)                tPopStructFront(list)
#define POP_STRUCT_BACK(list)           tPopStructBack(list)
#define GET_STRUCT(list, index)         tGetStructAt(list, index)
#define INSERT_STRUCT(list, index, ptr) tInsertStructAt(list, index, ptr)
#define REMOVE_STRUCT(list, index)      tRemoveStructAt(list, index)
#define FIND_STRUCT(list, ptr)          tFindStruct(list, ptr)

#define PUSH_UNION_LL(list, ptr)        tPushUnionBack(list, ptr)
#define PUSH_UNION_FRONT(list, ptr)     tPushUnionFront(list, ptr)
#define POP_UNION(list)                 tPopUnionFront(list)
#define POP_UNION_BACK(list)            tPopUnionBack(list)
#define GET_UNION(list, index)          tGetUnionAt(list, index)
#define INSERT_UNION(list, index, ptr)  tInsertUnionAt(list, index, ptr)
#define REMOVE_UNION(list, index)       tRemoveUnionAt(list, index)
#define FIND_UNION(list, ptr)           tFindUnion(list, ptr)

#define PUSH_FUNC_LL(list, ptr)         tPushFuncPtrBack(list, ptr)
#define PUSH_FUNC_FRONT(list, ptr)      tPushFuncPtrFront(list, ptr)
#define POP_FUNC(list)                  tPopFuncPtrFront(list)
#define POP_FUNC_BACK(list)             tPopFuncPtrBack(list)
#define GET_FUNC(list, index)           tGetFuncPtrAt(list, index)
#define INSERT_FUNC(list, index, ptr)   tInsertFuncPtrAt(list, index, ptr)
#define REMOVE_FUNC(list, index)        tRemoveFuncPtrAt(list, index)
#define FIND_FUNC(list, ptr)            tFindFuncPtr(list, ptr)

#define PUSH_FILE_LL(list, stream)      tPushFilePtrBack(list, stream)
#define PUSH_FILE_FRONT(list, stream)   tPushFilePtrFront(list, stream)
#define POP_FILE(list)                  tPopFilePtrFront(list)
#define POP_FILE_BACK(list)             tPopFilePtrBack(list)
#define GET_FILE(list, index)           tGetFilePtrAt(list, index)
#define INSERT_FILE(list, idx, stream)  tInsertFilePtrAt(list, idx, stream)
#define REMOVE_FILE(list, index)        tRemoveFilePtrAt(list, index)
#define FIND_FILE(list, stream)         tFindFilePtr(list, stream)

typedef struct TableSlot_STLL
{
    void* data;
    short type;
} TableSlot_STLL;

typedef struct sNode
{
    TableSlot_STLL slot;
    struct sNode* next;
    struct sNode* prev;
} sNode;

typedef struct StackTableList
{
    sNode  pool[STACK_TABLE_CAPACITY];
    sNode* sFreeListHead;
    sNode* sNodeHead;
    sNode* sNodeTail;
    sNode* sNodePrev;
    int count;
} StackTableList;

void tStackFormLinkedTable(StackTableList* list);
sNode* tStackCreateNode(StackTableList* list, TableSlot_STLL payload);
void tStackDestroyList(StackTableList* list);
void vStackPrintList(const StackTableList* list);
bool bStackIsListEmpty(const StackTableList* list);
bool bStackIsListFull(const StackTableList* list);

void tStackPushBack(StackTableList* list, TableSlot_STLL payload);
void tStackPushFront(StackTableList* list, TableSlot_STLL payload);
TableSlot_STLL tStackPopFront(StackTableList* list);
TableSlot_STLL tStackPopBack(StackTableList* list);
TableSlot_STLL sStackPeekFront(const StackTableList* list);
TableSlot_STLL sStackPeekBack(const StackTableList* list);
TableSlot_STLL tStackGetNodeAt(const StackTableList* list, int target_index);

void tStackInsertAt(StackTableList* list, int index, TableSlot_STLL payload);
TableSlot_STLL tStackRemoveAt(StackTableList* list, int index);
void vStackReverseList(StackTableList* list);

void tStackPushIntBack(StackTableList* list, int value);
void tStackPushIntFront(StackTableList* list, int value);
int tStackGetIntAt(const StackTableList* list, int index);
int tStackPopIntFront(StackTableList* list);
int tStackPopIntBack(StackTableList* list);
void tStackPushIntListBack(StackTableList* list, int count, ...);
void tStackInsertIntAt(StackTableList* list, int index, int value);
int tStackRemoveIntAt(StackTableList* list, int index);
int tStackFindInt(const StackTableList* list, int target);

void tStackPushFloatBack(StackTableList* list, float value);
void tStackPushFloatFront(StackTableList* list, float value);
float tStackGetFloatAt(const StackTableList* list, int index);
float tStackPopFloatFront(StackTableList* list);
float tStackPopFloatBack(StackTableList* list);
void tStackInsertFloatAt(StackTableList* list, int index, float value);
float tStackRemoveFloatAt(StackTableList* list, int index);
int tStackFindFloat(const StackTableList* list, float target);

void tStackPushDoubleBack(StackTableList* list, double value);
void tStackPushDoubleFront(StackTableList* list, double value);
double tStackGetDoubleAt(const StackTableList* list, int index);
double tStackPopDoubleFront(StackTableList* list);
double tStackPopDoubleBack(StackTableList* list);
void tStackPushDoublesBack(StackTableList* list, int count, ...);
void tStackInsertDoubleAt(StackTableList* list, int index, double value);
double tStackRemoveDoubleAt(StackTableList* list, int index);
int tStackFindDouble(const StackTableList* list, double target);

void tStackPushCharBack(StackTableList* list, char value);
void tStackPushCharFront(StackTableList* list, char value);
char tStackGetCharAt(const StackTableList* list, int index);
char tStackPopCharFront(StackTableList* list);
char tStackPopCharBack(StackTableList* list);
void tStackPushCharsBack(StackTableList* list, int count, ...);
void tStackInsertCharAt(StackTableList* list, int index, char value);
char tStackRemoveCharAt(StackTableList* list, int index);
int tStackFindChar(const StackTableList* list, char target);

void tStackPushLongBack(StackTableList* list, long value);
void tStackPushLongFront(StackTableList* list, long value);
long tStackGetLongAt(const StackTableList* list, int index);
long tStackPopLongFront(StackTableList* list);
long tStackPopLongBack(StackTableList* list);
void tStackInsertLongAt(StackTableList* list, int index, long value);
long tStackRemoveLongAt(StackTableList* list, int index);
int tStackFindLong(const StackTableList* list, long target);

void tStackPushShortBack(StackTableList* list, short value);
void tStackPushShortFront(StackTableList* list, short value);
short tStackGetShortAt(const StackTableList* list, int index);
short tStackPopShortFront(StackTableList* list);
short tStackPopShortBack(StackTableList* list);
void tStackInsertShortAt(StackTableList* list, int index, short value);
short tStackRemoveShortAt(StackTableList* list, int index);
int tStackFindShort(const StackTableList* list, short target);

// Type-checked primitive getters - safe alternatives to the plain get functions
// above. Same contract as TableList's bTryGet*At(): false + untouched *out on a
// bad index or type-tag mismatch, true + written *out on success.
bool bStackTryGetIntAt(const StackTableList* list, int index, int* out);
bool bStackTryGetFloatAt(const StackTableList* list, int index, float* out);
bool bStackTryGetDoubleAt(const StackTableList* list, int index, double* out);
bool bStackTryGetCharAt(const StackTableList* list, int index, char* out);
bool bStackTryGetLongAt(const StackTableList* list, int index, long* out);
bool bStackTryGetShortAt(const StackTableList* list, int index, short* out);

void tStackPushPointerBack(StackTableList* list, void* ptr, short pointer_tag);
void tStackPushPointerFront(StackTableList* list, void* ptr, short pointer_tag);
void* tStackGetPointerAt(const StackTableList* list, int index);
void* tStackPopPointerFront(StackTableList* list);
void* tStackPopPointerBack(StackTableList* list);
void tStackInsertPointerAt(StackTableList* list, int index, void* ptr, short pointer_tag);
void* tStackRemovePointerAt(StackTableList* list, int index);
int tStackFindPointer(const StackTableList* list, void* target_ptr);

void tStackPushStructBack(StackTableList* list, void* struct_ptr);
void tStackPushStructFront(StackTableList* list, void* struct_ptr);
void* tStackGetStructAt(const StackTableList* list, int index);
void* tStackPopStructFront(StackTableList* list);
void* tStackPopStructBack(StackTableList* list);
void tStackInsertStructAt(StackTableList* list, int index, void* struct_ptr);
void* tStackRemoveStructAt(StackTableList* list, int index);
int tStackFindStruct(const StackTableList* list, void* target_struct_ptr);

void tStackPushUnionBack(StackTableList* list, void* union_ptr);
void tStackPushUnionFront(StackTableList* list, void* union_ptr);
void* tStackGetUnionAt(const StackTableList* list, int index);
void* tStackPopUnionFront(StackTableList* list);
void* tStackPopUnionBack(StackTableList* list);
void tStackInsertUnionAt(StackTableList* list, int index, void* union_ptr);
void* tStackRemoveUnionAt(StackTableList* list, int index);
int tStackFindUnion(const StackTableList* list, void* target_union_ptr);

void tStackPushFuncPtrBack(StackTableList* list, void* func_ptr);
void tStackPushFuncPtrFront(StackTableList* list, void* func_ptr);
void* tStackGetFuncPtrAt(const StackTableList* list, int index);
void* tStackPopFuncPtrFront(StackTableList* list);
void* tStackPopFuncPtrBack(StackTableList* list);
void tStackInsertFuncPtrAt(StackTableList* list, int index, void* func_ptr);
void* tStackRemoveFuncPtrAt(StackTableList* list, int index);
int tStackFindFuncPtr(const StackTableList* list, void* target_func_ptr);

void tStackPushFilePtrBack(StackTableList* list, FILE* file_stream);
void tStackPushFilePtrFront(StackTableList* list, FILE* file_stream);
FILE* tStackGetFilePtrAt(const StackTableList* list, int index);
FILE* tStackPopFilePtrFront(StackTableList* list);
FILE* tStackPopFilePtrBack(StackTableList* list);
void tStackInsertFilePtrAt(StackTableList* list, int index, FILE* file_stream);
FILE* tStackRemoveFilePtrAt(StackTableList* list, int index);
int tStackFindFilePtr(const StackTableList* list, FILE* target_stream);

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

#define S_PUSH_INT(list, val)             tStackPushIntBack(list, val)
#define S_PUSH_INT_FRONT(list, val)       tStackPushIntFront(list, val)
#define S_POP_INT(list)                   tStackPopIntFront(list)
#define S_POP_INT_BACK(list)              tStackPopIntBack(list)
#define S_GET_INT(list, index)            tStackGetIntAt(list, index)
#define S_PUSH_INTS(list, count, ...)     tStackPushIntListBack(list, count, __VA_ARGS__)
#define S_INSERT_INT(list, index, val)    tStackInsertIntAt(list, index, val)
#define S_REMOVE_INT(list, index)         tStackRemoveIntAt(list, index)
#define S_FIND_INT(list, target)          tStackFindInt(list, target)

#define S_PUSH_FLOAT(list, val)           tStackPushFloatBack(list, val)
#define S_PUSH_FLOAT_FRONT(list, val)     tStackPushFloatFront(list, val)
#define S_POP_FLOAT(list)                 tStackPopFloatFront(list)
#define S_POP_FLOAT_BACK(list)            tStackPopFloatBack(list)
#define S_GET_FLOAT(list, index)          tStackGetFloatAt(list, index)
#define S_INSERT_FLOAT(list, index, val)  tStackInsertFloatAt(list, index, val)
#define S_REMOVE_FLOAT(list, index)       tStackRemoveFloatAt(list, index)
#define S_FIND_FLOAT(list, target)        tStackFindFloat(list, target)

#define S_PUSH_DOUBLE(list, val)          tStackPushDoubleBack(list, val)
#define S_PUSH_DOUBLE_FRONT(list, val)    tStackPushDoubleFront(list, val)
#define S_POP_DOUBLE(list)                tStackPopDoubleFront(list)
#define S_POP_DOUBLE_BACK(list)           tStackPopDoubleBack(list)
#define S_GET_DOUBLE(list, index)         tStackGetDoubleAt(list, index)
#define S_PUSH_DOUBLES(list, count, ...)  tStackPushDoublesBack(list, count, __VA_ARGS__)
#define S_INSERT_DOUBLE(list, idx, val)   tStackInsertDoubleAt(list, idx, val)
#define S_REMOVE_DOUBLE(list, index)      tStackRemoveDoubleAt(list, index)
#define S_FIND_DOUBLE(list, target)       tStackFindDouble(list, target)

#define S_PUSH_CHAR(list, val)            tStackPushCharBack(list, val)
#define S_PUSH_CHAR_FRONT(list, val)      tStackPushCharFront(list, val)
#define S_POP_CHAR(list)                  tStackPopCharFront(list)
#define S_POP_CHAR_BACK(list)             tStackPopCharBack(list)
#define S_GET_CHAR(list, index)           tStackGetCharAt(list, index)
#define S_PUSH_CHARS(list, count, ...)    tStackPushCharsBack(list, count, __VA_ARGS__)
#define S_INSERT_CHAR(list, index, val)   tStackInsertCharAt(list, index, val)
#define S_REMOVE_CHAR(list, index)        tStackRemoveCharAt(list, index)
#define S_FIND_CHAR(list, target)         tStackFindChar(list, target)

#define S_PUSH_LONG(list, val)            tStackPushLongBack(list, val)
#define S_PUSH_LONG_FRONT(list, val)      tStackPushLongFront(list, val)
#define S_POP_LONG(list)                  tStackPopLongFront(list)
#define S_POP_LONG_BACK(list)             tStackPopLongBack(list)
#define S_GET_LONG(list, index)           tStackGetLongAt(list, index)
#define S_INSERT_LONG(list, index, val)   tStackInsertLongAt(list, index, val)
#define S_REMOVE_LONG(list, index)        tStackRemoveLongAt(list, index)
#define S_FIND_LONG(list, target)         tStackFindLong(list, target)

#define S_PUSH_SHORT(list, val)           tStackPushShortBack(list, val)
#define S_PUSH_SHORT_FRONT(list, val)     tStackPushShortFront(list, val)
#define S_POP_SHORT(list)                 tStackPopShortFront(list)
#define S_POP_SHORT_BACK(list)            tStackPopShortBack(list)
#define S_GET_SHORT(list, index)          tStackGetShortAt(list, index)
#define S_INSERT_SHORT(list, index, val)  tStackInsertShortAt(list, index, val)
#define S_REMOVE_SHORT(list, index)       tStackRemoveShortAt(list, index)
#define S_FIND_SHORT(list, target)        tStackFindShort(list, target)

#define S_PUSH_PTR(list, ptr, tag)        tStackPushPointerBack(list, ptr, tag)
#define S_PUSH_PTR_FRONT(list, ptr, tag)  tStackPushPointerFront(list, ptr, tag)
#define S_POP_PTR(list)                   tStackPopPointerFront(list)
#define S_POP_PTR_BACK(list)              tStackPopPointerBack(list)
#define S_GET_PTR(list, index)            tStackGetPointerAt(list, index)
#define S_INSERT_PTR(list, idx, ptr, tag) tStackInsertPointerAt(list, idx, ptr, tag)
#define S_REMOVE_PTR(list, index)         tStackRemovePointerAt(list, index)
#define S_FIND_PTR(list, ptr)             tStackFindPointer(list, ptr)

#define S_PUSH_STRUCT(list, ptr)          tStackPushStructBack(list, ptr)
#define S_PUSH_STRUCT_FRONT(list, ptr)    tStackPushStructFront(list, ptr)
#define S_POP_STRUCT(list)                tStackPopStructFront(list)
#define S_POP_STRUCT_BACK(list)           tStackPopStructBack(list)
#define S_GET_STRUCT(list, index)         tStackGetStructAt(list, index)
#define S_INSERT_STRUCT(list, index, ptr) tStackInsertStructAt(list, index, ptr)
#define S_REMOVE_STRUCT(list, index)      tStackRemoveStructAt(list, index)
#define S_FIND_STRUCT(list, ptr)          tStackFindStruct(list, ptr)

#define S_PUSH_UNION(list, ptr)           tStackPushUnionBack(list, ptr)
#define S_PUSH_UNION_FRONT(list, ptr)     tStackPushUnionFront(list, ptr)
#define S_POP_UNION(list)                 tStackPopUnionFront(list)
#define S_POP_UNION_BACK(list)            tStackPopUnionBack(list)
#define S_GET_UNION(list, index)          tStackGetUnionAt(list, index)
#define S_INSERT_UNION(list, index, ptr)  tStackInsertUnionAt(list, index, ptr)
#define S_REMOVE_UNION(list, index)       tStackRemoveUnionAt(list, index)
#define S_FIND_UNION(list, ptr)           tStackFindUnion(list, ptr)

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

typedef struct TableSlot_HM
{
    void* ptr;
    uint16_t next;
    uint16_t prev;
    short type;
} TableSlot_HM;

typedef struct TableMap
{
    TableSlot_HM pool[HASH_POOL_CAPACITY];
    uint16_t buckets[HASH_TABLE_SIZE];
    uint16_t hFreeListHead;
    int active_elements;
    uint32_t(*hash_fn)(void*);
} TableMap;

typedef void (*TableMapVisitor)(uint16_t index, TableSlot_HM slot);

extern void vFormHashMap(TableMap* map);
extern void vDropHashMap(TableMap* map);
extern void vPrintHashMap(const TableMap* map);

extern int       tMapInsert(TableMap* map, void* payload, short type);
extern bool      bMapRemove(TableMap* map, void* payload);
extern bool      tMapContains(TableMap* map, void* payload);
extern TableSlot_HM sMapGetNode(const TableMap* map, void* payload);
extern void      tMapClear(TableMap* map);

// Type-checked slot lookup - same contract as bTryGetSlotAtTable(): false
// (untouched *out) if the payload isn't found or its stored type tag doesn't
// match expected_type; true + copied slot on success. Use before GET_MAP_VALUE
// whenever the expected type isn't already guaranteed by surrounding logic.
extern bool bTryMapGetTyped(const TableMap* map, void* payload, short expected_type, TableSlot_HM* out);

// Exposes the map's heap-ownership rule for a given type tag: true if a slot of
// this type owns a heap allocation that vDropHashMap()/bMapRemove() must free,
// false if it's a raw/user-managed pointer the library never touches.
extern bool bIsOwnedTypeMap(short type);

extern int  tMapInsertInt(TableMap* map, int val, void** out_key);
extern int  tMapInsertFloat(TableMap* map, float val, void** out_key);
extern int  tMapInsertDouble(TableMap* map, double val, void** out_key);
extern int  tMapInsertLong(TableMap* map, long val, void** out_key);
extern int  tMapInsertShort(TableMap* map, short val, void** out_key);
extern int  tMapInsertChar(TableMap* map, char val, void** out_key);

extern int  tMapInsertPtr(TableMap* map, void* val, short pointer_type, void** out_key);
extern int  tMapInsertDPtr(TableMap* map, void** val, short double_pointer_type, void** out_key);
extern int  tMapInsertTPtr(TableMap* map, void*** val, short triple_pointer_type, void** out_key);

extern int  tMapInsertStruct(TableMap* map, void* struct_data, size_t size, void** out_key);
extern int  tMapInsertUnion(TableMap* map, void* union_ptr, size_t union_size, void** out_key);
extern int  tMapInsertFunc(TableMap* map, void* func_ptr, void** out_key);
extern int  tMapInsertFile(TableMap* map, FILE* file_ptr, void** out_key);

extern int  iCountMap(const TableMap* map);
extern int  iCapacityMap(const TableMap* map);
extern bool bIsEmptyMap(const TableMap* map);
extern bool bIsFullMap(const TableMap* map);

extern int  iCountOfTypeMap(const TableMap* map, short type);
extern bool bContainsTypeMap(const TableMap* map, short type);
extern int  iFindTypeMap(const TableMap* map, short type);

extern int    iSumIntMap(const TableMap* map);
extern double dSumFloatMap(const TableMap* map);
extern bool   bMinIntMap(const TableMap* map, int* out);
extern bool   bMaxIntMap(const TableMap* map, int* out);

extern void vForEachNodeMap(const TableMap* map, TableMapVisitor visitor);
extern void tMapProcessActive(TableMap* map, TableMapVisitor visitor);

extern double dLoadFactorMap(const TableMap* map);
extern int    iFreeSlotsRemainingMap(const TableMap* map);
extern int    iBucketLengthMap(const TableMap* map, int bucket_idx);
extern int    iMaxBucketLengthMap(const TableMap* map);
extern bool   bMapReplaceType(TableMap* map, void* payload, short new_type);
extern int    iBucketOfMap(const TableMap* map, void* payload);
extern bool   bCloneTableMap(TableMap* dest, const TableMap* src);

extern const char* sTypeNameMap(short type);

extern uint32_t uDefaultPointerHash(void* payload);
extern void     vSetHashFnMap(TableMap* map, uint32_t(*hash_fn)(void*));

extern bool bMapFindFirst(const TableMap* map, short type, TableSlot_HM* out);
extern int  iMapKeysOfType(const TableMap* map, short type, void** out_keys, int max);
extern void vRehashStatsMap(const TableMap* map);
extern bool bMapMergeInto(TableMap* dest, const TableMap* src, int* out_merged, int* out_skipped);
extern bool bPointerHashIsSuspect(const TableMap* map);

#define MAP_INSERT_INT(map_ptr, val, out_key)               tMapInsertInt((map_ptr), (val), (out_key))
#define MAP_INSERT_FLOAT(map_ptr, val, out_key)              tMapInsertFloat((map_ptr), (val), (out_key))
#define MAP_INSERT_DOUBLE(map_ptr, val, out_key)             tMapInsertDouble((map_ptr), (val), (out_key))
#define MAP_INSERT_LONG(map_ptr, val, out_key)               tMapInsertLong((map_ptr), (val), (out_key))
#define MAP_INSERT_SHORT(map_ptr, val, out_key)              tMapInsertShort((map_ptr), (val), (out_key))
#define MAP_INSERT_CHAR(map_ptr, val, out_key)               tMapInsertChar((map_ptr), (val), (out_key))

#define MAP_INSERT_VOID_PTR(map_ptr, val, out_key)      tMapInsertPtr((map_ptr), (void*)(val), TYPE_VOID_STAR, (out_key))
#define MAP_INSERT_INT_PTR(map_ptr, val, out_key)       tMapInsertPtr((map_ptr), (void*)(val), TYPE_INT_STAR, (out_key))
#define MAP_INSERT_FLOAT_PTR(map_ptr, val, out_key)     tMapInsertPtr((map_ptr), (void*)(val), TYPE_FLOAT_STAR, (out_key))
#define MAP_INSERT_CHAR_PTR(map_ptr, val, out_key)      tMapInsertPtr((map_ptr), (void*)(val), TYPE_CHAR_STAR, (out_key))
#define MAP_INSERT_DOUBLE_PTR(map_ptr, val, out_key)    tMapInsertPtr((map_ptr), (void*)(val), TYPE_DOUBLE_STAR, (out_key))
#define MAP_INSERT_LONG_PTR(map_ptr, val, out_key)      tMapInsertPtr((map_ptr), (void*)(val), TYPE_LONG_STAR, (out_key))
#define MAP_INSERT_SHORT_PTR(map_ptr, val, out_key)     tMapInsertPtr((map_ptr), (void*)(val), TYPE_SHORT_STAR, (out_key))

#define MAP_INSERT_STRUCT(map_ptr, val, out_key)        tMapInsertStruct((map_ptr), (void*)(val), sizeof(*(val)), (out_key))
#define MAP_INSERT_UNION(map_ptr, union_ptr, out_key)   tMapInsertUnion((map_ptr), (void*)(union_ptr), sizeof(*(union_ptr)), (out_key))
#define MAP_INSERT_FUNC(map_ptr, func_ptr, out_key)     tMapInsertFunc((map_ptr), (void*)(func_ptr), (out_key))
#define MAP_INSERT_FILE(map_ptr, fp, out_key)           tMapInsertFile((map_ptr), (fp), (out_key))

#define GET_MAP_VALUE(node_ptr, type) (*(type*)((node_ptr)->ptr))

#ifdef __cplusplus
}
#endif

#endif