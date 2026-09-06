#include "../core/T_LIBFXNS_ARRAY_BASED.cpp" // Core Table implementation

// ============================================================================
// USER-DEFINED TYPES & CALLBACKS FOR TESTING
// ============================================================================

typedef struct Vector3 {
    float x, y, z;
} Vector3;

typedef union ColorUnion {
    unsigned int rgba;
    unsigned char channels[4];
} ColorUnion;

// Callback function to test TYPE_FUNC_PTR execution
void exhaustive_callback(void) {
    printf("[Callback Execution] Function pointer called successfully from code segment!\n");
}

// ============================================================================
// MAIN TEST SUITE
// ============================================================================

int main(void) {
    Table table;

    // ------------------------------------------------------------------------
    // 1. INITIALIZATION
    // ------------------------------------------------------------------------
    printf("\n=== 1. INITIALIZING TABLE (Capacity: 5) ===\n");
    vGetTable(&table, 5);

    // ------------------------------------------------------------------------
    // 2. VARIADIC PUSH VIA bPushList (PRIMITIVES)
    // ------------------------------------------------------------------------
    printf("\n=== 2. PUSHING PRIMITIVES VIA bPushList ===\n");
    // Note: Capacity is 5, pushing 6 items will trigger automatic buffer growth!
    bPushList(&table, 6,
        TYPE_INT, 42,
        TYPE_FLOAT, 3.14f,
        TYPE_DOUBLE, 2.718281828,
        TYPE_CHAR, 'X',
        TYPE_LONG, 123456789L,
        TYPE_SHORT, (short)3276
    );

    // ------------------------------------------------------------------------
    // 3. PUSHING SINGLE, DOUBLE, & TRIPLE POINTERS
    // ------------------------------------------------------------------------
    printf("\n=== 3. PUSHING POINTER WRAPPERS ===\n");
    int raw_int = 500;
    float raw_float = 99.99f;
    char raw_char = 'A';
    double raw_double = 1.2345;
    long raw_long = 987654L;
    short raw_short = 123;
    void* raw_void = &raw_int;

    // Single pointers
    bPushPtr(&table, &raw_int, TYPE_INT_STAR);
    bPushPtr(&table, &raw_float, TYPE_FLOAT_STAR);
    bPushPtr(&table, &raw_char, TYPE_CHAR_STAR);
    bPushPtr(&table, &raw_double, TYPE_DOUBLE_STAR);
    bPushPtr(&table, &raw_long, TYPE_LONG_STAR);
    bPushPtr(&table, &raw_short, TYPE_SHORT_STAR);
    bPushPtr(&table, raw_void, TYPE_VOID_STAR);

    // Double & Triple pointers
    int* p_int = &raw_int;
    int** pp_int = &p_int;

    bPushDPtr(&table, (void**)&p_int, TYPE_INT_STAR_DOUBLE);
    bPushTPtr(&table, (void***)&pp_int, TYPE_INT_STAR_TRIPLE);

    // ------------------------------------------------------------------------
    // 4. PUSHING DEEP-COPIED STRUCTS & UNIONS
    // ------------------------------------------------------------------------
    printf("\n=== 4. PUSHING STRUCTS & UNIONS (DEEP COPIES) ===\n");
    Vector3 pos = { 10.5f, 20.0f, -5.2f };
    bPushStruct(&table, &pos, sizeof(Vector3));

    ColorUnion col;
    col.rgba = 0xFF00FF00; // Bright Green
    bPushUnion(&table, &col, sizeof(ColorUnion));

    // ------------------------------------------------------------------------
    // 5. PUSHING NON-OWNED HANDLES (FUNCTION & FILE POINTERS)
    // ------------------------------------------------------------------------
    printf("\n=== 5. PUSHING CODE & OS HANDLES ===\n");
    bPushFunc(&table, (void*)&exhaustive_callback);

    FILE* test_log = fopen("table_test.log", "w");
    if (test_log) {
        bPushFile(&table, test_log);
    }

    // ------------------------------------------------------------------------
    // 6. INSPECT TABLE STATE
    // ------------------------------------------------------------------------
    printf("\n=== 6. INSPECTING COMPLETE TABLE STATE ===\n");
    vPrintTable(&table);

    // ------------------------------------------------------------------------
    // 7. TESTING LIFO POP BEHAVIOR & POINTER RECOVERY
    // ------------------------------------------------------------------------
    printf("\n=== 7. TESTING LIFO POP BEHAVIOR ===\n");

    // Pop #1: Top item is FILE* handle
    TableSlot slot_file = sPopSlot(&table);
    if (slot_file.type == TYPE_FILE_PTR && slot_file.ptr != NULL) {
        FILE* fp = (FILE*)slot_file.ptr;
        fprintf(fp, "[Table Test] Stream log successfully written via popped slot handle.\n");
        fclose(fp);
        printf(" -> File handle retrieved, written to, and closed safely.\n");
    }

    // Pop #2: Next item is Function Pointer
    TableSlot slot_func = sPopSlot(&table);
    if (slot_func.type == TYPE_FUNC_PTR && slot_func.ptr != NULL) {
        void (*cb)(void) = (void (*)(void))slot_func.ptr;
        cb(); // Execute function pointer directly from code segment
    }

    // Pop #3: Next item is Deep-Copied Union
    TableSlot slot_union = sPopSlot(&table);
    if (slot_union.type == TYPE_UNION && slot_union.ptr != NULL) {
        ColorUnion* popped_col = (ColorUnion*)slot_union.ptr;
        printf(" -> Popped Union RGBA: 0x%08X\n", popped_col->rgba);
        // Note: sPopSlot transfers ownership of non-primitive pointers to caller!
        free(slot_union.ptr);
    }

    // Pop #4: Next item is Deep-Copied Struct
    TableSlot slot_struct = sPopSlot(&table);
    if (slot_struct.type == TYPE_STRUCT && slot_struct.ptr != NULL) {
        Vector3* popped_vec = (Vector3*)slot_struct.ptr;
        printf(" -> Popped Vector3 Coordinates: X=%.1f, Y=%.1f, Z=%.1f\n",
            popped_vec->x, popped_vec->y, popped_vec->z);
        free(slot_struct.ptr); // Clean up caller-owned popped struct
    }

    // Pop #5: Primitive deep-copy test
    TableSlot slot_triple_ptr = sPopSlot(&table);
    printf(" -> Popped slot type tag %d (Triple Pointer wrapper handed back as-is).\n", slot_triple_ptr.type);

    // ------------------------------------------------------------------------
    // 8. FINAL CLEANUP & DROP
    // ------------------------------------------------------------------------
    printf("\n=== 8. TEARDOWN & DROPPING REMAINING TABLE ===\n");
    // vDropTable will free remaining primitives, pointer wrappers, and struct heap memory
    // while safely skipping raw non-owned pointers (like function/file pointers).
    vDropTable(&table);

    return 0;
}