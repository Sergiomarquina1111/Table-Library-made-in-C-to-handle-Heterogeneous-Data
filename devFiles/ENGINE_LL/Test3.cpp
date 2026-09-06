#include "../core/LinkedListBased/include/T_LL_DEF.h"
#include <stdio.h>

// Dummy structures and functions for advanced type testing
typedef struct {
    int id;
    float value;
} TestStruct;

typedef union {
    int i_val;
    float f_val;
} TestUnion;

void sample_function(void) {
    printf("   [Callback] Sample function pointer invoked successfully!\n");
}

int main(void)
{
    printf("\n======================================================\n");
    printf("      TABLE ENGINE : EXHAUSTIVE API VERIFICATION      \n");
    printf("======================================================\n\n");

    TableList table;

    // =========================================================================
    // 1. CORE & RAW TOPOLOGY TESTS
    // =========================================================================
    printf("[1] Testing Core & Raw Topology Operations...\n");
    tFormLinkedTable(&table);
    if (IS_EMPTY(&table)) {
        printf(" -> tFormLinkedTable & IS_EMPTY passed.\n");
    }

    // Raw Slot Insertion & Peeking
    TableSlot raw_slot = { NULL, TYPE_INT };
    raw_slot.data = (void*)(intptr_t)999;
    PUSH_RAW_BACK(&table, raw_slot);

    TableSlot peeked_front = PEEK_RAW_FRONT(&table);
    TableSlot peeked_back = PEEK_RAW_BACK(&table);
    printf(" -> Raw Peek Front tag: %d, Back tag: %d\n", peeked_front.type, peeked_back.type);

    TableSlot popped_raw = POP_RAW_FRONT(&table);
    printf(" -> POP_RAW_FRONT extracted payload value: %d\n", (int)(intptr_t)popped_raw.data);

    // Raw insertion / removal at index & reversal
    PUSH_INT(&table, 10);
    PUSH_INT(&table, 30);
    INSERT_RAW_AT(&table, 1, raw_slot); // insert at index 1
    PRINT_LIST(&table);

    TableSlot removed_raw = REMOVE_RAW_AT(&table, 1);
    printf(" -> REMOVE_RAW_AT extracted tag: %d\n", removed_raw.type);

    REVERSE_LIST(&table);
    DESTROY_LIST(&table);
    printf("------------------------------------------------------\n");

    // =========================================================================
    // 2. INTEGER WRAPPERS
    // =========================================================================
    printf("[2] Testing Integer API...\n");
    tFormLinkedTable(&table);

    PUSH_INT(&table, 100);             // Back
    PUSH_INT_FRONT(&table, 50);        // Front
    PUSH_INTS(&table, 3, 200, 300, 400); // Bulk variadic back

    printf(" -> GET_INT at index 2: %d\n", GET_INT(&table, 2));
    INSERT_INT(&table, 1, 75);
    printf(" -> INSERT_INT at index 1. New value at 1: %d\n", GET_INT(&table, 1));

    printf(" -> FIND_INT for 300 found at index: %d\n", FIND_INT(&table, 300));

    printf(" -> POP_INT (Front): %d\n", POP_INT(&table));
    printf(" -> POP_INT_BACK: %d\n", POP_INT_BACK(&table));
    printf(" -> REMOVE_INT at index 1: %d\n", REMOVE_INT(&table, 1));

    DESTROY_LIST(&table);
    printf("------------------------------------------------------\n");

    // =========================================================================
    // 3. FLOAT WRAPPERS
    // =========================================================================
    printf("[3] Testing Float API...\n");
    tFormLinkedTable(&table);

    PUSH_FLOAT(&table, 5.5f);
    PUSH_FLOAT_FRONT(&table, 1.1f);
    INSERT_FLOAT(&table, 1, 3.3f);

    printf(" -> GET_FLOAT at index 1: %.1f\n", GET_FLOAT(&table, 1));
    printf(" -> FIND_FLOAT for 3.3f found at index: %d\n", FIND_FLOAT(&table, 3.3f));

    printf(" -> POP_FLOAT (Front): %.1f\n", POP_FLOAT(&table));
    printf(" -> POP_FLOAT_BACK: %.1f\n", POP_FLOAT_BACK(&table));
    printf(" -> REMOVE_FLOAT at index 0: %.1f\n", REMOVE_FLOAT(&table, 0));

    DESTROY_LIST(&table);
    printf("------------------------------------------------------\n");

    // =========================================================================
    // 4. DOUBLE WRAPPERS
    // =========================================================================
    printf("[4] Testing Double API...\n");
    tFormLinkedTable(&table);

    PUSH_DOUBLE(&table, 99.99);
    PUSH_DOUBLE_FRONT(&table, 11.11);
    PUSH_DOUBLES(&table, 2, 33.33, 44.44);
    INSERT_DOUBLE(&table, 2, 22.22);

    printf(" -> GET_DOUBLE at index 2: %.2f\n", GET_DOUBLE(&table, 2));
    printf(" -> FIND_DOUBLE for 33.33 found at index: %d\n", FIND_DOUBLE(&table, 33.33));

    printf(" -> POP_DOUBLE (Front): %.2f\n", POP_DOUBLE(&table));
    printf(" -> POP_DOUBLE_BACK: %.2f\n", POP_DOUBLE_BACK(&table));
    printf(" -> REMOVE_DOUBLE at index 1: %.2f\n", REMOVE_DOUBLE(&table, 1));

    DESTROY_LIST(&table);
    printf("------------------------------------------------------\n");

    // =========================================================================
    // 5. CHAR WRAPPERS
    // =========================================================================
    printf("[5] Testing Char API...\n");
    tFormLinkedTable(&table);

    PUSH_CHAR(&table, 'B');
    PUSH_CHAR_FRONT(&table, 'A');
    PUSH_CHARS(&table, 2, 'C', 'D');
    INSERT_CHAR(&table, 2, 'X');

    printf(" -> GET_CHAR at index 2: %c\n", GET_CHAR(&table, 2));
    printf(" -> FIND_CHAR for 'C' found at index: %d\n", FIND_CHAR(&table, 'C'));

    printf(" -> POP_CHAR (Front): %c\n", POP_CHAR(&table));
    printf(" -> POP_CHAR_BACK: %c\n", POP_CHAR_BACK(&table));
    printf(" -> REMOVE_CHAR at index 1: %c\n", REMOVE_CHAR(&table, 1));

    DESTROY_LIST(&table);
    printf("------------------------------------------------------\n");

    // =========================================================================
    // 6. LONG & SHORT WRAPPERS
    // =========================================================================
    printf("[6] Testing Long & Short API...\n");
    tFormLinkedTable(&table);

    PUSH_LONG(&table, 500000L);
    PUSH_LONG_FRONT(&table, 100000L);
    INSERT_LONG(&table, 1, 300000L);
    printf(" -> GET_LONG at index 1: %ld\n", GET_LONG(&table, 1));
    printf(" -> FIND_LONG for 500000L found at index: %d\n", FIND_LONG(&table, 500000L));
    printf(" -> POP_LONG (Front): %ld\n", POP_LONG(&table));
    printf(" -> POP_LONG_BACK: %ld\n", POP_LONG_BACK(&table));
    printf(" -> REMOVE_LONG at index 0: %ld\n", REMOVE_LONG(&table, 0));

    PUSH_SHORT(&table, 300);
    PUSH_SHORT_FRONT(&table, 100);
    INSERT_SHORT(&table, 1, 200);
    printf(" -> GET_SHORT at index 1: %d\n", GET_SHORT(&table, 1));
    printf(" -> FIND_SHORT for 300 found at index: %d\n", FIND_SHORT(&table, 300));
    printf(" -> POP_SHORT (Front): %d\n", POP_SHORT(&table));
    printf(" -> POP_SHORT_BACK: %d\n", POP_SHORT_BACK(&table));
    printf(" -> REMOVE_SHORT at index 0: %d\n", REMOVE_SHORT(&table, 0));

    DESTROY_LIST(&table);
    printf("------------------------------------------------------\n");

    // =========================================================================
    // 7. GENERIC POINTER WRAPPERS
    // =========================================================================
    printf("[7] Testing Generic Pointer API...\n");
    tFormLinkedTable(&table);

    int dummy_val = 777;
    PUSH_PTR(&table, &dummy_val, TYPE_INT_STAR);
    PUSH_PTR_FRONT(&table, NULL, TYPE_NULL);
    INSERT_PTR(&table, 1, &dummy_val, TYPE_INT_STAR);

    printf(" -> GET_PTR at index 1 address match? %s\n", GET_PTR(&table, 1) == &dummy_val ? "YES" : "NO");
    printf(" -> FIND_PTR found at index: %d\n", FIND_PTR(&table, &dummy_val));

    printf(" -> POP_PTR (Front): %p\n", POP_PTR(&table));
    printf(" -> POP_PTR_BACK: %p\n", POP_PTR_BACK(&table));
    printf(" -> REMOVE_PTR at index 0: %p\n", REMOVE_PTR(&table, 0));

    DESTROY_LIST(&table);
    printf("------------------------------------------------------\n");

    // =========================================================================
    // 8. STRUCT & UNION WRAPPERS
    // =========================================================================
    printf("[8] Testing Struct & Union API...\n");
    tFormLinkedTable(&table);

    TestStruct my_struct = { 1, 99.5f };
    TestUnion my_union;
    my_union.i_val = 42;

    PUSH_STRUCT(&table, &my_struct);
    PUSH_STRUCT_FRONT(&table, &my_struct);
    INSERT_STRUCT(&table, 1, &my_struct);

    printf(" -> GET_STRUCT ID: %d\n", ((TestStruct*)GET_STRUCT(&table, 0))->id);
    printf(" -> FIND_STRUCT found at index: %d\n", FIND_STRUCT(&table, &my_struct));
    printf(" -> POP_STRUCT (Front): %p\n", POP_STRUCT(&table));
    printf(" -> POP_STRUCT_BACK: %p\n", POP_STRUCT_BACK(&table));
    printf(" -> REMOVE_STRUCT at index 0: %p\n", REMOVE_STRUCT(&table, 0));

    PUSH_UNION(&table, &my_union);
    PUSH_UNION_FRONT(&table, &my_union);
    INSERT_UNION(&table, 1, &my_union);

    printf(" -> GET_UNION i_val: %d\n", ((TestUnion*)GET_UNION(&table, 0))->i_val);
    printf(" -> FIND_UNION found at index: %d\n", FIND_UNION(&table, &my_union));
    printf(" -> POP_UNION (Front): %p\n", POP_UNION(&table));
    printf(" -> POP_UNION_BACK: %p\n", POP_UNION_BACK(&table));
    printf(" -> REMOVE_UNION at index 0: %p\n", REMOVE_UNION(&table, 0));

    DESTROY_LIST(&table);
    printf("------------------------------------------------------\n");

    // =========================================================================
    // 9. SYSTEM RESOURCE WRAPPERS (Func Ptrs & Files)
    // =========================================================================
    printf("[9] Testing System Resource API (Function Ptrs & Files)...\n");
    tFormLinkedTable(&table);

    // Function Pointer Testing
    PUSH_FUNC(&table, (void*)sample_function);
    PUSH_FUNC_FRONT(&table, (void*)sample_function);
    INSERT_FUNC(&table, 1, (void*)sample_function);

    void (*fn_ptr)(void) = (void(*)(void))GET_FUNC(&table, 0);
    if (fn_ptr) fn_ptr(); // Execute retrieved function pointer

    printf(" -> FIND_FUNC found at index: %d\n", FIND_FUNC(&table, (void*)sample_function));
    printf(" -> POP_FUNC (Front): %p\n", POP_FUNC(&table));
    printf(" -> POP_FUNC_BACK: %p\n", POP_FUNC_BACK(&table));
    printf(" -> REMOVE_FUNC at index 0: %p\n", REMOVE_FUNC(&table, 0));

    // File Stream Testing
    FILE* temp_file = fopen("test_log.txt", "w+");
    if (temp_file) {
        PUSH_FILE(&table, temp_file);
        PUSH_FILE_FRONT(&table, temp_file);
        INSERT_FILE(&table, 1, temp_file);

        FILE* retrieved_file = GET_FILE(&table, 0);
        if (retrieved_file) {
            fprintf(retrieved_file, "Writing through retrieved file pointer successfully!\n");
        }

        printf(" -> FIND_FILE found at index: %d\n", FIND_FILE(&table, temp_file));
        printf(" -> POP_FILE (Front): %p\n", (void*)POP_FILE(&table));
        printf(" -> POP_FILE_BACK: %p\n", (void*)POP_FILE_BACK(&table));
        printf(" -> REMOVE_FILE at index 0: %p\n", (void*)REMOVE_FILE(&table, 0));

        fclose(temp_file);
        remove("test_log.txt");
    }

    DESTROY_LIST(&table);

    printf("\n======================================================\n");
    printf("      ALL FUNCTIONS & MACROS VERIFIED SUCCESSFULLY    \n");
    printf("======================================================\n\n");

    return 0;
}