#include "../core/LinkedListBased/include/T_LL_DEF.h"
#include <stdio.h>

int main(void)
{
    printf("\n[TEST 1] INITIALIZATION =========================\n");
    TableList myList;
    tFormLinkedTable(&myList);
    printf("Engine initialized. Is Empty? %s\n", IS_EMPTY(&myList) ? "YES" : "NO");

    printf("\n[TEST 2] BULK INSERTION & FRONTIERS ===========\n");
    // Push ints in bulk to the back
    PUSH_INTS(&myList, 4, 10, 20, 30, 40);
    // Push a float to the very front
    PUSH_FLOAT_FRONT(&myList, 3.14f);
    // Push a char to the very back
    PUSH_CHAR(&myList, 'X');

    PRINT_LIST(&myList);
    // Expected Layout: [3.14f] -> [10] -> [20] -> [30] -> [40] -> ['X']

    printf("\n[TEST 3] MID-CHAIN TARGETING ==================\n");
    printf("Splicing integer '999' directly into Index 3...\n");
    INSERT_INT(&myList, 3, 999);
    PRINT_LIST(&myList);

    printf("\n[TEST 4] SEARCH & DISCOVERY ===================\n");
    int target_idx = FIND_INT(&myList, 999);
    printf("Requested search for '999'. Engine found it at index: %d\n", target_idx);

    printf("\n[TEST 5] TARGETED EXTRACTION ==================\n");
    if (target_idx != -1) {
        printf("Extracting value from index %d...\n", target_idx);
        int extracted = REMOVE_INT(&myList, target_idx);
        printf("Successfully extracted: %d\n", extracted);
    }
    PRINT_LIST(&myList);

    printf("\n[TEST 6] TOPOLOGICAL INVERSION ================\n");
    REVERSE_LIST(&myList);
    PRINT_LIST(&myList);

    printf("\n[TEST 7] DESTRUCTION & MEMORY TEARDOWN ========\n");
    DESTROY_LIST(&myList);
    printf("Final Empty Check: %s\n", IS_EMPTY(&myList) ? "YES" : "NO");

    return 0;
}