#ifndef T_HSH_H
#define T_HSH_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

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

typedef struct TableSlot
{
    void* ptr;
    uint16_t next;
    uint16_t prev;
    short type;
} TableSlot;

typedef struct TableMap
{
    TableSlot pool[HASH_POOL_CAPACITY];
    uint16_t buckets[HASH_TABLE_SIZE];
    uint16_t hFreeListHead;
    int active_elements;
    uint32_t(*hash_fn)(void*);
} TableMap;

typedef void (*TableMapVisitor)(uint16_t index, TableSlot slot);

extern void vFormHashMap(TableMap* map);
extern void vDropHashMap(TableMap* map);
extern void vPrintHashMap(const TableMap* map);

extern int       tMapInsert(TableMap* map, void* payload, short type);
extern bool      bMapRemove(TableMap* map, void* payload);
extern bool      tMapContains(TableMap* map, void* payload);
extern TableSlot sMapGetNode(const TableMap* map, void* payload);
extern void      tMapClear(TableMap* map);

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

extern bool bMapFindFirst(const TableMap* map, short type, TableSlot* out);
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

#endif