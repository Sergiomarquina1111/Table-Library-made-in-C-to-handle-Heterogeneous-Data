#include "../core/T_LIBFXNS_ARR_LCL.cpp" // Core TableStack implementation

// ============================================================================
// USER-DEFINED TYPES & CALLBACKS FOR STACK TESTING
// ============================================================================

// Fits inside a single machine word cell (sizeof(SmallPoint) <= sizeof(void*))
typedef struct SmallPoint {
    short x, y; // 4 bytes total
} SmallPoint;

// Oversized struct used to demonstrate stack cell overflow guard
typedef struct LargeVector {
    float x, y, z, w; // 16 bytes total (exceeds sizeof(void*))
} LargeVector;

typedef union ByteUnion {
    int raw;
    unsigned char bytes[4];
} ByteUnion;

// Visitor function callback for vForEachSlotStack
void sample_visitor(int index, TableSlotStack slot) {
    printf("  [Visitor] Slot %02d | Type Tag: %-2d (%s)\n",
        index, slot.type, sTypeNameStack(slot.type));
}

// Callback function for testing TYPE_FUNC_PTR
void stack_callback(void) {
    printf("[Stack Callback] Function pointer executed directly from stack slot!\n");
}

// Helper macro to declare a stack table instance
#define DECLARE_STACK_TABLE(name, cap) \
    void* name##_data[cap]; \
    short name##_types[cap]; \
    TableStack name = { name##_data, name##_types, 0, cap }

// ============================================================================
// MAIN TEST SUITE
// ============================================================================

int main(void) {
    // Declare a stack-allocated table with capacity for 20 slots (0 mallocs)
    DECLARE_STACK_TABLE(stack, 20);

    // ------------------------------------------------------------------------
    // 1. VARIADIC & PRIMITIVE PUSHES
    // ------------------------------------------------------------------------
    printf("\n=== 1. PUSHING PRIMITIVES VIA bPushListStack ===\n");
    bPushListStack(&stack, 6,
        TYPE_INT, 10,
        TYPE_INT, 25,
        TYPE_INT, 5,
        TYPE_FLOAT, 3.14f,
        TYPE_FLOAT, 1.86f,
        TYPE_CHAR, 'Z'
    );

    vPrintTableStack(&stack);

    // ------------------------------------------------------------------------
    // 2. QUERY & MATHEMATICAL AGGREGATIONS
    // ------------------------------------------------------------------------
    printf("\n=== 2. TESTING QUERY & MATHEMATICAL AGGREGATIONS ===\n");
    int int_count = iCountOfTypeStack(&stack, TYPE_INT);
    bool has_float = bContainsTypeStack(&stack, TYPE_FLOAT);
    int first_char_idx = iFindTypeStack(&stack, TYPE_CHAR);

    int sum_i = iSumIntStack(&stack);
    double sum_f = dSumFloatStack(&stack);

    int min_val = 0, max_val = 0;
    bMinIntStack(&stack, &min_val);
    bMaxIntStack(&stack, &max_val);

    printf(" -> Count of TYPE_INT     : %d\n", int_count);
    printf(" -> Contains TYPE_FLOAT   : %s\n", has_float ? "YES" : "NO");
    printf(" -> Index of TYPE_CHAR    : %d\n", first_char_idx);
    printf(" -> Sum of Ints           : %d\n", sum_i);
    printf(" -> Sum of Floats         : %.2f\n", sum_f);
    printf(" -> Min Int / Max Int     : %d / %d\n", min_val, max_val);

    // ------------------------------------------------------------------------
    // 3. STACK MANIPULATIONS (DUP, SWAP, REVERSE)
    // ------------------------------------------------------------------------
    printf("\n=== 3. TESTING STACK MANIPULATION OPS ===\n");
    printf("--- Duplicating Top Slot ---");
    bDupTopStack(&stack);
    vPrintTableStack(&stack);

    printf("--- Swapping Top Two Slots ---");
    bSwapTopStack(&stack);
    vPrintTableStack(&stack);

    printf("--- Reversing Complete Stack In-Place ---");
    vReverseStack(&stack);
    vPrintTableStack(&stack);

    // ------------------------------------------------------------------------
    // 4. INLINE STRUCT, UNION, CODE & HANDLE PUSHES
    // ------------------------------------------------------------------------
    printf("\n=== 4. TESTING INLINE DATA & HARD CEILING GUARDS ===\n");

    // A. Small Struct test (Fits within sizeof(void*))
    SmallPoint pt = { 12, 34 };
    if (bPushStructStack(&stack, &pt, sizeof(SmallPoint))) {
        printf(" -> SmallPoint (%zu bytes) pushed successfully.\n", sizeof(SmallPoint));
    }

    // B. Union test
    ByteUnion bu;
    bu.raw = 0x12345678;
    bPushUnionStack(&stack, &bu, sizeof(ByteUnion));

    // C. Guard Test: Attempting to push an oversized struct (> sizeof(void*))
    LargeVector big_vec = { 1.0f, 2.0f, 3.0f, 4.0f };
    printf(" -> Testing boundary error guard for LargeVector (%zu bytes):\n    ", sizeof(LargeVector));
    bool guard_result = bPushStructStack(&stack, &big_vec, sizeof(LargeVector));
    printf(" -> Overflow Guard Triggered / Push Blocked: %s\n", !guard_result ? "SUCCESS" : "FAILED");

    // D. Function & File handles
    bPushFuncStack(&stack, (void*)&stack_callback);

    FILE* test_file = fopen("stack_test.log", "w");
    if (test_file) {
        bPushFileStack(&stack, test_file);
    }

    vPrintTableStack(&stack);

    // ------------------------------------------------------------------------
    // 5. BULK ARRAY PUSHES & CLONING
    // ------------------------------------------------------------------------
    printf("\n=== 5. TESTING BULK ARRAYS & CLONING ===\n");
    int raw_arr[3] = { 100, 200, 300 };
    iPushIntArrayStack(&stack, raw_arr, 3);

    // Clone into a second stack buffer
    DECLARE_STACK_TABLE(cloned_stack, 20);
    bCloneTableStack(&cloned_stack, &stack);

    printf("\n--- Verified Cloned Stack Output ---");
    vPrintTableStack(&cloned_stack);

    // ------------------------------------------------------------------------
    // 6. VISITOR PATTERN ITERATION
    // ------------------------------------------------------------------------
    printf("\n=== 6. TESTING VISITOR ITERATION (vForEachSlotStack) ===\n");
    vForEachSlotStack(&stack, sample_visitor);

    // ------------------------------------------------------------------------
    // 7. LIFO POPPING & HANDLE EXECUTION
    // ------------------------------------------------------------------------
    printf("\n=== 7. TESTING LIFO POP BEHAVIOR ===\n");

    // Pop Bulk Array Elements
    sPopSlotStack(&stack); // 300
    sPopSlotStack(&stack); // 200
    sPopSlotStack(&stack); // 100

    // Pop File Handle & Write
    TableSlotStack file_slot = sPopSlotStack(&stack);
    if (file_slot.type == TYPE_FILE_PTR && file_slot.value != NULL) {
        FILE* fp = (FILE*)file_slot.value;
        fprintf(fp, "[Stack Test] Verified zero-allocation handle execution.\n");
        fclose(fp);
        printf(" -> Stream log written and closed safely.\n");
    }

    // Pop Function Pointer & Execute
    TableSlotStack func_slot = sPopSlotStack(&stack);
    if (func_slot.type == TYPE_FUNC_PTR && func_slot.value != NULL) {
        void (*cb)(void) = (void (*)(void))func_slot.value;
        cb();
    }

    // ------------------------------------------------------------------------
    // 8. TEARDOWN & RESET (0 FREES)
    // ------------------------------------------------------------------------
    printf("\n=== 8. ZERO-ALLOCATION STACK RESET ===\n");
    vDropTableStack(&stack);
    vDropTableStack(&cloned_stack);

    return 0;
}