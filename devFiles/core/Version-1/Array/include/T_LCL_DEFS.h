#ifndef T_LCL_DEFS_H
#define T_LCL_DEFS_H

#include "T_DEFS.h"
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>


static_assert(sizeof(void*) >= sizeof(double), "TableStack assumes a 64-bit build (sizeof(void*) >= 8) so double/long fit in one cell.");

/* TableSlot (ptr + type) is reused directly from T_DEFS.h - the stack variant
   no longer needs its own slot type. For primitives/pointers <= sizeof(void*),
   the payload is stored INLINE in the `ptr` field itself (reinterpreted via
   GET_STACK_VALUE), not behind a separate malloc'd allocation like the
   heap-based Table uses - see the "two storage styles" note in the .cpp file. */

typedef struct
{
    TableSlot* slots;  /* points at a caller-declared TableSlot arr[N], OR at
                           the backing array supplied by vGetTableStack() below */
    int        capacity;  /* fixed at declare time, chosen per-instance */
    int        count;
} TableStack;

typedef void (*TableStackVisitor)(int index, TableSlot slot);

/* Manual construction - no macro required:
 *     TableSlot arr[5] = { {0} };
 *     TableStack ts = { arr, 5, 0 };
 * The macro below is just sugar over the same pattern. */
#define vGetTableStack(name, CAP)                                   \
    TableSlot   name##_backing[(CAP)] = { { 0 } };  /* TYPE_EMPTY == 0 */ \
    TableStack  name = { name##_backing, (CAP), 0 };                \
    printf("\n=== Initializing Table (stack) ===\n");               \
    printf("[vGetTableStack] Table initialized (Capacity: %d).\n\n", (CAP))

#ifndef TABLE_STACK_DEFAULT_CAPACITY
#define TABLE_STACK_DEFAULT_CAPACITY 32
#endif
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

static inline int iCountStack(const TableStack* obj)      /* Python len() */
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

static inline TableSlot sPeekSlotStack(const TableStack* obj)
{
    TableSlot empty = { NULL, TYPE_EMPTY };
    if (obj == NULL || obj->count == 0) return empty;
    return obj->slots[obj->count - 1];   /* untouched, unlike sPopSlotStack */
}

#define TABLE_STACK_LEN(tb)  ((tb).count)
#define TABLE_STACK_CAP(tb)  ((tb).capacity)

extern bool       bPushSlotStack(TableStack* obj, TableSlot node);
extern TableSlot  sPopSlotStack(TableStack* obj);
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
extern void vPrintSlotStack(TableSlot slot);
extern bool bPushListStack(TableStack* obj, int count, ...);
extern void vForEachSlotStack(const TableStack* obj, TableStackVisitor visitor);


extern TableSlot sGetSlotAtStack(const TableStack* obj, int index);
extern short      sGetTypeAtStack(const TableStack* obj, int index);

extern bool bDupTopStack(TableStack* obj);    /* push a copy of the current top */
extern bool bSwapTopStack(TableStack* obj);   /* swap the top two slots         */

extern int  iCountOfTypeStack(const TableStack* obj, short type);
extern bool bContainsTypeStack(const TableStack* obj, short type);
extern int  iFindTypeStack(const TableStack* obj, short type);   /* -1 if not found */
extern int    iSumIntStack(const TableStack* obj);
extern double dSumFloatStack(const TableStack* obj);
extern bool bMinIntStack(const TableStack* obj, int* out);
extern bool bMaxIntStack(const TableStack* obj, int* out);

extern void vReverseStack(TableStack* obj);
extern int  iPushIntArrayStack(TableStack* obj, const int* arr, int n);      /* returns how many actually fit */
extern int  iPushFloatArrayStack(TableStack* obj, const float* arr, int n);
extern bool bCloneTableStack(TableStack* dest, const TableStack* src);       /* dest->capacity must be >= src->count */

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

#endif