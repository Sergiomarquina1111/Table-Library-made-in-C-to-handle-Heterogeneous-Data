#include "T_HSH.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

extern uint32_t uDefaultPointerHash(void* payload)
{
    uintptr_t x = (uintptr_t)payload;
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return (uint32_t)x;
}

static int iComputeBucketIndex(const TableMap* map, void* payload)
{
    uint32_t h = map->hash_fn ? map->hash_fn(payload) : uDefaultPointerHash(payload);
    return (int)(h % HASH_TABLE_SIZE);
}

extern void vSetHashFnMap(TableMap* map, uint32_t(*hash_fn)(void*))
{
    if (map == NULL)
        return;
    map->hash_fn = hash_fn;
}

static bool bTypeOwnsHeapPtr(short type)
{
    switch (type)
    {
    case TYPE_EMPTY:
    case TYPE_FUNC_PTR:
    case TYPE_FILE_PTR:
    case TYPE_STRUCT_PTR:
    case TYPE_UNION_PTR:
        return false;
    default:
        return true;
    }
}

extern void vFormHashMap(TableMap* map)
{
    if (map == NULL)
        return;

    for (int i = 0; i < HASH_TABLE_SIZE; i++)
    {
        map->buckets[i] = INDEX_NULL;
    }

    for (int i = 0; i < HASH_POOL_CAPACITY - 1; i++)
    {
        map->pool[i].next = (uint16_t)(i + 1);
        map->pool[i].prev = INDEX_NULL;
        map->pool[i].ptr = NULL;
        map->pool[i].type = TYPE_EMPTY;
    }

    int last = HASH_POOL_CAPACITY - 1;
    map->pool[last].next = INDEX_NULL;
    map->pool[last].prev = INDEX_NULL;
    map->pool[last].ptr = NULL;
    map->pool[last].type = TYPE_EMPTY;

    map->hFreeListHead = 0;
    map->active_elements = 0;
    map->hash_fn = NULL;

    printf("\n=== Initializing TableMap ===\n");
    printf("[vFormHashMap] Initialization successful. Capacity: %d | Table Size: %d\n\n", HASH_POOL_CAPACITY, HASH_TABLE_SIZE);
}

extern void vDropHashMap(TableMap* map)
{
    if (map == NULL)
        return;

    int counted = 0;
    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < map->active_elements; i++)
    {
        if (map->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;
        if (map->pool[i].ptr != NULL && bTypeOwnsHeapPtr(map->pool[i].type))
            free(map->pool[i].ptr);
    }

    vFormHashMap(map);
    printf("[vDropHashMap] TableMap dropped and owned payloads released.\n");
}

static int iInsertRawInternal(TableMap* map, void* payload, short type)
{
    if (map == NULL || map->hFreeListHead == INDEX_NULL || payload == NULL)
        return INT_MIN;

    int bucket_idx = iComputeBucketIndex(map, payload);

    if (map->buckets[bucket_idx] != INDEX_NULL)
    {
        uint16_t curr_idx = map->buckets[bucket_idx];
        do
        {
            if (map->pool[curr_idx].ptr == payload)
            {
                return ~bucket_idx;
            }
            curr_idx = map->pool[curr_idx].next;
        } while (curr_idx != map->buckets[bucket_idx]);
    }

    uint16_t new_idx = map->hFreeListHead;
    map->hFreeListHead = map->pool[new_idx].next;

    map->pool[new_idx].ptr = payload;
    map->pool[new_idx].type = type;
    map->active_elements++;

    if (map->buckets[bucket_idx] == INDEX_NULL)
    {
        map->pool[new_idx].next = new_idx;
        map->pool[new_idx].prev = new_idx;
    }
    else
    {
        uint16_t head_idx = map->buckets[bucket_idx];
        uint16_t tail_idx = map->pool[head_idx].prev;

        map->pool[new_idx].next = head_idx;
        map->pool[new_idx].prev = tail_idx;

        map->pool[head_idx].prev = new_idx;
        map->pool[tail_idx].next = new_idx;
    }

    map->buckets[bucket_idx] = new_idx;

    return bucket_idx;
}

extern int tMapInsert(TableMap* map, void* payload, short type)
{
    return iInsertRawInternal(map, payload, type);
}

extern int tMapInsertInt(TableMap* map, int val, void** out_key)
{
    int* copy = (int*)malloc(sizeof(int));
    if (copy == NULL)
        return INT_MIN;
    *copy = val;

    int result = iInsertRawInternal(map, copy, TYPE_INT);
    if (result == INT_MIN || result < 0)
    {
        free(copy);
        if (out_key != NULL)
            *out_key = NULL;
        return result;
    }

    if (out_key != NULL)
        *out_key = copy;
    return result;
}

extern int tMapInsertFloat(TableMap* map, float val, void** out_key)
{
    float* copy = (float*)malloc(sizeof(float));
    if (copy == NULL)
        return INT_MIN;
    *copy = val;

    int result = iInsertRawInternal(map, copy, TYPE_FLOAT);
    if (result == INT_MIN || result < 0)
    {
        free(copy);
        if (out_key != NULL)
            *out_key = NULL;
        return result;
    }

    if (out_key != NULL)
        *out_key = copy;
    return result;
}

extern int tMapInsertDouble(TableMap* map, double val, void** out_key)
{
    double* copy = (double*)malloc(sizeof(double));
    if (copy == NULL)
        return INT_MIN;
    *copy = val;

    int result = iInsertRawInternal(map, copy, TYPE_DOUBLE);
    if (result == INT_MIN || result < 0)
    {
        free(copy);
        if (out_key != NULL)
            *out_key = NULL;
        return result;
    }

    if (out_key != NULL)
        *out_key = copy;
    return result;
}

extern int tMapInsertLong(TableMap* map, long val, void** out_key)
{
    long* copy = (long*)malloc(sizeof(long));
    if (copy == NULL)
        return INT_MIN;
    *copy = val;

    int result = iInsertRawInternal(map, copy, TYPE_LONG);
    if (result == INT_MIN || result < 0)
    {
        free(copy);
        if (out_key != NULL)
            *out_key = NULL;
        return result;
    }

    if (out_key != NULL)
        *out_key = copy;
    return result;
}

extern int tMapInsertShort(TableMap* map, short val, void** out_key)
{
    short* copy = (short*)malloc(sizeof(short));
    if (copy == NULL)
        return INT_MIN;
    *copy = val;

    int result = iInsertRawInternal(map, copy, TYPE_SHORT);
    if (result == INT_MIN || result < 0)
    {
        free(copy);
        if (out_key != NULL)
            *out_key = NULL;
        return result;
    }

    if (out_key != NULL)
        *out_key = copy;
    return result;
}

extern int tMapInsertChar(TableMap* map, char val, void** out_key)
{
    char* copy = (char*)malloc(sizeof(char));
    if (copy == NULL)
        return INT_MIN;
    *copy = val;

    int result = iInsertRawInternal(map, copy, TYPE_CHAR);
    if (result == INT_MIN || result < 0)
    {
        free(copy);
        if (out_key != NULL)
            *out_key = NULL;
        return result;
    }

    if (out_key != NULL)
        *out_key = copy;
    return result;
}

extern int tMapInsertPtr(TableMap* map, void* val, short pointer_type, void** out_key)
{
    void** wrapper = (void**)malloc(sizeof(void*));
    if (wrapper == NULL)
        return INT_MIN;
    *wrapper = val;

    int result = iInsertRawInternal(map, wrapper, pointer_type);
    if (result == INT_MIN || result < 0)
    {
        free(wrapper);
        if (out_key != NULL)
            *out_key = NULL;
        return result;
    }

    if (out_key != NULL)
        *out_key = wrapper;
    return result;
}

extern int tMapInsertDPtr(TableMap* map, void** val, short double_pointer_type, void** out_key)
{
    void*** wrapper = (void***)malloc(sizeof(void**));
    if (wrapper == NULL)
        return INT_MIN;
    *wrapper = val;

    int result = iInsertRawInternal(map, wrapper, double_pointer_type);
    if (result == INT_MIN || result < 0)
    {
        free(wrapper);
        if (out_key != NULL)
            *out_key = NULL;
        return result;
    }

    if (out_key != NULL)
        *out_key = wrapper;
    return result;
}

extern int tMapInsertTPtr(TableMap* map, void*** val, short triple_pointer_type, void** out_key)
{
    void**** wrapper = (void****)malloc(sizeof(void***));
    if (wrapper == NULL)
        return INT_MIN;
    *wrapper = val;

    int result = iInsertRawInternal(map, wrapper, triple_pointer_type);
    if (result == INT_MIN || result < 0)
    {
        free(wrapper);
        if (out_key != NULL)
            *out_key = NULL;
        return result;
    }

    if (out_key != NULL)
        *out_key = wrapper;
    return result;
}

extern int tMapInsertStruct(TableMap* map, void* struct_data, size_t size, void** out_key)
{
    if (struct_data == NULL || size == 0)
        return INT_MIN;

    void* copy = malloc(size);
    if (copy == NULL)
        return INT_MIN;
    memcpy(copy, struct_data, size);

    int result = iInsertRawInternal(map, copy, TYPE_STRUCT);
    if (result == INT_MIN || result < 0)
    {
        free(copy);
        if (out_key != NULL)
            *out_key = NULL;
        return result;
    }

    if (out_key != NULL)
        *out_key = copy;
    return result;
}

extern int tMapInsertUnion(TableMap* map, void* union_ptr, size_t union_size, void** out_key)
{
    if (union_ptr == NULL || union_size == 0)
        return INT_MIN;

    void* copy = malloc(union_size);
    if (copy == NULL)
        return INT_MIN;
    memcpy(copy, union_ptr, union_size);

    int result = iInsertRawInternal(map, copy, TYPE_UNION);
    if (result == INT_MIN || result < 0)
    {
        free(copy);
        if (out_key != NULL)
            *out_key = NULL;
        return result;
    }

    if (out_key != NULL)
        *out_key = copy;
    return result;
}

extern int tMapInsertFunc(TableMap* map, void* func_ptr, void** out_key)
{
    int result = iInsertRawInternal(map, func_ptr, TYPE_FUNC_PTR);
    if (out_key != NULL)
        *out_key = (result == INT_MIN || result < 0) ? NULL : func_ptr;
    return result;
}

extern int tMapInsertFile(TableMap* map, FILE* file_ptr, void** out_key)
{
    int result = iInsertRawInternal(map, (void*)file_ptr, TYPE_FILE_PTR);
    if (out_key != NULL)
        *out_key = (result == INT_MIN || result < 0) ? NULL : (void*)file_ptr;
    return result;
}

extern bool bMapRemove(TableMap* map, void* payload)
{
    if (map == NULL || payload == NULL)
        return false;

    int bucket_idx = iComputeBucketIndex(map, payload);

    if (map->buckets[bucket_idx] == INDEX_NULL)
        return false;

    uint16_t head_idx = map->buckets[bucket_idx];
    uint16_t curr_idx = head_idx;

    do
    {
        if (map->pool[curr_idx].ptr == payload)
        {
            uint16_t next_idx = map->pool[curr_idx].next;
            uint16_t prev_idx = map->pool[curr_idx].prev;

            if (next_idx == curr_idx)
            {
                map->buckets[bucket_idx] = INDEX_NULL;
            }
            else
            {
                map->pool[prev_idx].next = next_idx;
                map->pool[next_idx].prev = prev_idx;

                if (head_idx == curr_idx)
                    map->buckets[bucket_idx] = next_idx;
            }

            if (bTypeOwnsHeapPtr(map->pool[curr_idx].type))
                free(map->pool[curr_idx].ptr);

            map->pool[curr_idx].ptr = NULL;
            map->pool[curr_idx].type = TYPE_EMPTY;
            map->pool[curr_idx].prev = INDEX_NULL;
            map->pool[curr_idx].next = map->hFreeListHead;
            map->hFreeListHead = curr_idx;
            map->active_elements--;

            return true;
        }

        curr_idx = map->pool[curr_idx].next;
    } while (curr_idx != head_idx);

    return false;
}

extern bool tMapContains(TableMap* map, void* payload)
{
    if (map == NULL || payload == NULL)
        return false;

    int bucket_idx = iComputeBucketIndex(map, payload);

    if (map->buckets[bucket_idx] == INDEX_NULL)
        return false;

    uint16_t curr_idx = map->buckets[bucket_idx];
    do
    {
        if (map->pool[curr_idx].ptr == payload)
        {
            return true;
        }
        curr_idx = map->pool[curr_idx].next;
    } while (curr_idx != map->buckets[bucket_idx]);

    return false;
}

extern TableSlot sMapGetNode(const TableMap* map, void* payload)
{
    TableSlot empty = { NULL, INDEX_NULL, INDEX_NULL, TYPE_EMPTY };

    if (map == NULL || payload == NULL)
        return empty;

    int bucket_idx = iComputeBucketIndex(map, payload);

    if (map->buckets[bucket_idx] == INDEX_NULL)
        return empty;

    uint16_t head_idx = map->buckets[bucket_idx];
    uint16_t curr_idx = head_idx;

    do
    {
        if (map->pool[curr_idx].ptr == payload)
            return map->pool[curr_idx];
        curr_idx = map->pool[curr_idx].next;
    } while (curr_idx != head_idx);

    return empty;
}

extern void tMapClear(TableMap* map)
{
    if (map == NULL)
        return;
    vDropHashMap(map);
}

extern int iCountMap(const TableMap* map)
{
    return map ? map->active_elements : 0;
}

extern int iCapacityMap(const TableMap* map)
{
    (void)map;
    return HASH_POOL_CAPACITY;
}

extern bool bIsEmptyMap(const TableMap* map)
{
    return (map == NULL) || (map->active_elements == 0);
}

extern bool bIsFullMap(const TableMap* map)
{
    return (map != NULL) && (map->hFreeListHead == INDEX_NULL);
}

extern int iCountOfTypeMap(const TableMap* map, short type)
{
    if (map == NULL)
        return 0;

    int counted = 0;
    int matches = 0;

    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < map->active_elements; i++)
    {
        if (map->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;
        if (map->pool[i].type == type)
            matches++;
    }

    return matches;
}

extern bool bContainsTypeMap(const TableMap* map, short type)
{
    return iFindTypeMap(map, type) != -1;
}

extern int iFindTypeMap(const TableMap* map, short type)
{
    if (map == NULL)
        return -1;

    int counted = 0;

    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < map->active_elements; i++)
    {
        if (map->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;
        if (map->pool[i].type == type)
            return (int)i;
    }

    return -1;
}

extern int iSumIntMap(const TableMap* map)
{
    if (map == NULL)
        return 0;

    int sum = 0;
    int counted = 0;

    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < map->active_elements; i++)
    {
        if (map->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;
        if (map->pool[i].type == TYPE_INT && map->pool[i].ptr != NULL)
            sum += *(int*)map->pool[i].ptr;
    }

    return sum;
}

extern double dSumFloatMap(const TableMap* map)
{
    if (map == NULL)
        return 0.0;

    double sum = 0.0;
    int counted = 0;

    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < map->active_elements; i++)
    {
        if (map->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;
        if (map->pool[i].type == TYPE_FLOAT && map->pool[i].ptr != NULL)
            sum += (double)(*(float*)map->pool[i].ptr);
    }

    return sum;
}

extern bool bMinIntMap(const TableMap* map, int* out)
{
    if (map == NULL || out == NULL)
        return false;

    bool found = false;
    int minimum = 0;
    int counted = 0;

    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < map->active_elements; i++)
    {
        if (map->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;
        if (map->pool[i].type == TYPE_INT && map->pool[i].ptr != NULL)
        {
            int val = *(int*)map->pool[i].ptr;
            if (!found || val < minimum)
            {
                minimum = val;
                found = true;
            }
        }
    }

    if (found)
        *out = minimum;

    return found;
}

extern bool bMaxIntMap(const TableMap* map, int* out)
{
    if (map == NULL || out == NULL)
        return false;

    bool found = false;
    int maximum = 0;
    int counted = 0;

    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < map->active_elements; i++)
    {
        if (map->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;
        if (map->pool[i].type == TYPE_INT && map->pool[i].ptr != NULL)
        {
            int val = *(int*)map->pool[i].ptr;
            if (!found || val > maximum)
            {
                maximum = val;
                found = true;
            }
        }
    }

    if (found)
        *out = maximum;

    return found;
}

extern void vForEachNodeMap(const TableMap* map, TableMapVisitor visitor)
{
    if (map == NULL || visitor == NULL)
        return;

    int counted = 0;

    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < map->active_elements; i++)
    {
        if (map->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;
        visitor(i, map->pool[i]);
    }
}

extern void tMapProcessActive(TableMap* map, TableMapVisitor visitor)
{
    if (map == NULL || visitor == NULL || map->active_elements == 0)
        return;

    int counted = 0;

    for (uint16_t i = 0; i < HASH_POOL_CAPACITY; i++)
    {
        if (counted >= map->active_elements)
            break;

        if (map->pool[i].type != TYPE_EMPTY)
        {
            visitor(i, map->pool[i]);
            counted++;
        }
    }
}

extern double dLoadFactorMap(const TableMap* map)
{
    if (map == NULL)
        return 0.0;
    return (double)map->active_elements / (double)HASH_TABLE_SIZE;
}

extern int iFreeSlotsRemainingMap(const TableMap* map)
{
    if (map == NULL)
        return 0;
    return HASH_POOL_CAPACITY - map->active_elements;
}

extern int iBucketLengthMap(const TableMap* map, int bucket_idx)
{
    if (map == NULL || bucket_idx < 0 || bucket_idx >= HASH_TABLE_SIZE)
        return 0;

    if (map->buckets[bucket_idx] == INDEX_NULL)
        return 0;

    int length = 0;
    uint16_t head_idx = map->buckets[bucket_idx];
    uint16_t curr_idx = head_idx;

    do
    {
        length++;
        curr_idx = map->pool[curr_idx].next;
    } while (curr_idx != head_idx);

    return length;
}

extern int iMaxBucketLengthMap(const TableMap* map)
{
    if (map == NULL)
        return 0;

    int max_len = 0;
    for (int i = 0; i < HASH_TABLE_SIZE; i++)
    {
        int len = iBucketLengthMap(map, i);
        if (len > max_len)
            max_len = len;
    }

    return max_len;
}

extern bool bMapReplaceType(TableMap* map, void* payload, short new_type)
{
    if (map == NULL || payload == NULL)
        return false;

    int bucket_idx = iComputeBucketIndex(map, payload);

    if (map->buckets[bucket_idx] == INDEX_NULL)
        return false;

    uint16_t head_idx = map->buckets[bucket_idx];
    uint16_t curr_idx = head_idx;

    do
    {
        if (map->pool[curr_idx].ptr == payload)
        {
            map->pool[curr_idx].type = new_type;
            return true;
        }
        curr_idx = map->pool[curr_idx].next;
    } while (curr_idx != head_idx);

    return false;
}

extern int iBucketOfMap(const TableMap* map, void* payload)
{
    if (map == NULL || payload == NULL)
        return -1;
    return iComputeBucketIndex(map, payload);
}

extern bool bCloneTableMap(TableMap* dest, const TableMap* src)
{
    if (dest == NULL || src == NULL)
        return false;

    vFormHashMap(dest);

    int counted = 0;
    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < src->active_elements; i++)
    {
        if (src->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;

        if (src->pool[i].ptr == NULL || !bTypeOwnsHeapPtr(src->pool[i].type))
        {
            if (iInsertRawInternal(dest, src->pool[i].ptr, src->pool[i].type) == INT_MIN)
                return false;
            continue;
        }

        size_t sz = 0;
        switch (src->pool[i].type)
        {
        case TYPE_INT:    sz = sizeof(int);    break;
        case TYPE_FLOAT:  sz = sizeof(float);  break;
        case TYPE_DOUBLE: sz = sizeof(double); break;
        case TYPE_LONG:   sz = sizeof(long);   break;
        case TYPE_SHORT:  sz = sizeof(short);  break;
        case TYPE_CHAR:   sz = sizeof(char);   break;
        case TYPE_VOID_STAR:
        case TYPE_INT_STAR:
        case TYPE_FLOAT_STAR:
        case TYPE_CHAR_STAR:
        case TYPE_DOUBLE_STAR:
        case TYPE_LONG_STAR:
        case TYPE_SHORT_STAR:
            sz = sizeof(void*);
            break;
        default:
            continue;
        }

        void* copy = malloc(sz);
        if (copy == NULL)
            return false;
        memcpy(copy, src->pool[i].ptr, sz);

        if (iInsertRawInternal(dest, copy, src->pool[i].type) == INT_MIN)
        {
            free(copy);
            return false;
        }
    }

    return true;
}

extern bool bMapFindFirst(const TableMap* map, short type, TableSlot* out)
{
    if (map == NULL || out == NULL)
        return false;

    int counted = 0;
    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < map->active_elements; i++)
    {
        if (map->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;
        if (map->pool[i].type == type)
        {
            *out = map->pool[i];
            return true;
        }
    }

    return false;
}

extern int iMapKeysOfType(const TableMap* map, short type, void** out_keys, int max)
{
    if (map == NULL || out_keys == NULL || max <= 0)
        return 0;

    int counted = 0;
    int filled = 0;

    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < map->active_elements && filled < max; i++)
    {
        if (map->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;
        if (map->pool[i].type == type)
        {
            out_keys[filled] = map->pool[i].ptr;
            filled++;
        }
    }

    return filled;
}

extern void vRehashStatsMap(const TableMap* map)
{
    if (map == NULL)
    {
        printf("[vRehashStatsMap] Error: TableMap is NULL.\n");
        return;
    }

    int max_len = 0;
    int used_buckets = 0;
    long total_len = 0;

    printf("\n========================================================================\n");
    printf("  TABLEMAP BUCKET HISTOGRAM  |  Buckets: %-3d  |  Active: %-3d\n", HASH_TABLE_SIZE, map->active_elements);
    printf("========================================================================\n");

    for (int i = 0; i < HASH_TABLE_SIZE; i++)
    {
        int len = iBucketLengthMap(map, i);
        if (len == 0)
            continue;

        used_buckets++;
        total_len += len;
        if (len > max_len)
            max_len = len;

        printf(" [bucket %02d] ", i);
        for (int j = 0; j < len; j++)
            printf("#");
        printf(" (%d)\n", len);
    }

    double avg = used_buckets > 0 ? (double)total_len / (double)used_buckets : 0.0;
    printf("------------------------------------------------------------------------\n");
    printf(" Used buckets: %d/%d  |  Longest chain: %d  |  Avg chain (used): %.2f\n", used_buckets, HASH_TABLE_SIZE, max_len, avg);
    printf("------------------------------------------------------------------------\n\n");
}

extern bool bMapMergeInto(TableMap* dest, const TableMap* src, int* out_merged, int* out_skipped)
{
    if (out_merged != NULL)
        *out_merged = 0;
    if (out_skipped != NULL)
        *out_skipped = 0;

    if (dest == NULL || src == NULL)
        return false;

    int counted = 0;
    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < src->active_elements; i++)
    {
        if (src->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;

        if (tMapContains(dest, src->pool[i].ptr))
        {
            if (out_skipped != NULL)
                (*out_skipped)++;
            continue;
        }

        if (src->pool[i].ptr == NULL || !bTypeOwnsHeapPtr(src->pool[i].type))
        {
            if (iInsertRawInternal(dest, src->pool[i].ptr, src->pool[i].type) == INT_MIN)
                return false;
            if (out_merged != NULL)
                (*out_merged)++;
            continue;
        }

        size_t sz = 0;
        switch (src->pool[i].type)
        {
        case TYPE_INT:    sz = sizeof(int);    break;
        case TYPE_FLOAT:  sz = sizeof(float);  break;
        case TYPE_DOUBLE: sz = sizeof(double); break;
        case TYPE_LONG:   sz = sizeof(long);   break;
        case TYPE_SHORT:  sz = sizeof(short);  break;
        case TYPE_CHAR:   sz = sizeof(char);   break;
        case TYPE_VOID_STAR:
        case TYPE_INT_STAR:
        case TYPE_FLOAT_STAR:
        case TYPE_CHAR_STAR:
        case TYPE_DOUBLE_STAR:
        case TYPE_LONG_STAR:
        case TYPE_SHORT_STAR:
            sz = sizeof(void*);
            break;
        default:
            continue;
        }

        void* copy = malloc(sz);
        if (copy == NULL)
            return false;
        memcpy(copy, src->pool[i].ptr, sz);

        if (iInsertRawInternal(dest, copy, src->pool[i].type) == INT_MIN)
        {
            free(copy);
            return false;
        }

        if (out_merged != NULL)
            (*out_merged)++;
    }

    return true;
}

extern bool bPointerHashIsSuspect(const TableMap* map)
{
    if (map == NULL || map->active_elements < HASH_TABLE_SIZE)
        return false;

    double expected_avg = (double)map->active_elements / (double)HASH_TABLE_SIZE;
    int max_len = iMaxBucketLengthMap(map);

    return (double)max_len > (expected_avg * 3.0) && max_len >= 6;
}

extern const char* sTypeNameMap(short type)
{
    switch (type)
    {
    case TYPE_EMPTY:            return "EMPTY";
    case TYPE_INT:               return "INT";
    case TYPE_FLOAT:             return "FLOAT";
    case TYPE_DOUBLE:            return "DOUBLE";
    case TYPE_CHAR:              return "CHAR";
    case TYPE_LONG:              return "LONG";
    case TYPE_SHORT:             return "SHORT";
    case TYPE_STRUCT:            return "STRUCT";
    case TYPE_UNION:             return "UNION";
    case TYPE_STRUCT_PTR:        return "STRUCT_PTR";
    case TYPE_UNION_PTR:         return "UNION_PTR";
    case TYPE_FUNC_PTR:          return "FUNC_PTR";
    case TYPE_FILE_PTR:          return "FILE_PTR";
    case TYPE_VOID_STAR:         return "VOID*";
    case TYPE_INT_STAR:          return "INT*";
    case TYPE_FLOAT_STAR:        return "FLOAT*";
    case TYPE_CHAR_STAR:         return "CHAR*";
    case TYPE_DOUBLE_STAR:       return "DOUBLE*";
    case TYPE_LONG_STAR:         return "LONG*";
    case TYPE_SHORT_STAR:        return "SHORT*";
    case TYPE_VOID_STAR_DOUBLE:  return "VOID**";
    case TYPE_INT_STAR_DOUBLE:   return "INT**";
    case TYPE_FLOAT_STAR_DOUBLE: return "FLOAT**";
    case TYPE_CHAR_STAR_DOUBLE:  return "CHAR**";
    case TYPE_DOUBLE_STAR_DOUBLE:return "DOUBLE**";
    case TYPE_LONG_STAR_DOUBLE:  return "LONG**";
    case TYPE_SHORT_STAR_DOUBLE: return "SHORT**";
    default:                     return "POINTER/OTHER";
    }
}

extern void vPrintHashMap(const TableMap* map)
{
    if (map == NULL)
    {
        printf("[vPrintHashMap] Error: TableMap is NULL.\n");
        return;
    }

    printf("\n========================================================================\n");
    printf("  TABLEMAP CONTENTS  |  Active: %-3d  |  Capacity: %-3d  |  Buckets: %-3d\n", map->active_elements, HASH_POOL_CAPACITY, HASH_TABLE_SIZE);
    printf("========================================================================\n");

    if (map->active_elements == 0)
    {
        printf("  [Empty TableMap]\n");
        printf("------------------------------------------------------------------------\n\n");
        return;
    }

    int counted = 0;

    for (uint16_t i = 0; i < HASH_POOL_CAPACITY && counted < map->active_elements; i++)
    {
        if (map->pool[i].type == TYPE_EMPTY)
            continue;

        counted++;
        printf(" [pool %04u] type=%-10s ptr=%p next=%u prev=%u\n",
            i, sTypeNameMap(map->pool[i].type), map->pool[i].ptr,
            map->pool[i].next, map->pool[i].prev);
    }

    printf("------------------------------------------------------------------------\n\n");
}