#include "../core/E_LIBFXNS_ARRAY_BASED.cpp"

// User-defined test structures
typedef struct Vector3 {
    float x, y, z;
} Vector3;

typedef union ColorUnion {
    unsigned int rgba;
    unsigned char channels[4];
} ColorUnion;

// Test callback function for TYPE_FUNC_PTR
void exhaustive_callback() {
    printf("[Callback] Function pointer executed successfully from code segment!\n");
}

int main(void) {
    ElementArray arr;
    InitCollection(&arr, 5);

    printf("\n=== 1. PUSHING PRIMITIVES VIA PUSH_LIST ===\n");
    // Testing INT, FLOAT, DOUBLE, CHAR, LONG, SHORT
    PUSH_LIST(&arr, 6,
        TYPE_INT, 42,
        TYPE_FLOAT, 3.14f,
        TYPE_DOUBLE, 2.718281828,
        TYPE_CHAR, 'X',
        TYPE_LONG, 123456789L,
        TYPE_SHORT, 3276
    );

    printf("\n=== 2. PUSHING SINGLE POINTERS ===\n");
    int raw_int = 500;
    float raw_float = 99.99f;
    char raw_char = 'A';
    double raw_double = 1.2345;
    long raw_long = 987654L;
    short raw_short = 123;
    void* raw_void = &raw_int;

    push_ptr_impl(&arr, &raw_int, TYPE_INT_STAR);
    push_ptr_impl(&arr, &raw_float, TYPE_FLOAT_STAR);
    push_ptr_impl(&arr, &raw_char, TYPE_CHAR_STAR);
    push_ptr_impl(&arr, &raw_double, TYPE_DOUBLE_STAR);
    push_ptr_impl(&arr, &raw_long, TYPE_LONG_STAR);
    push_ptr_impl(&arr, &raw_short, TYPE_SHORT_STAR);
    push_ptr_impl(&arr, raw_void, TYPE_VOID_STAR);

    printf("\n=== 3. PUSHING DOUBLE & TRIPLE POINTERS ===\n");
    int* p_int = &raw_int;
    int** pp_int = &p_int;
    int*** ppp_int = &pp_int;

    push_dptr_impl(&arr, (void**)&p_int, TYPE_INT_STAR_DOUBLE);
    push_tptr_impl(&arr, (void***)&pp_int, TYPE_INT_STAR_TRIPLE);

    printf("\n=== 4. PUSHING STRUCTS & UNIONS ===\n");
    Vector3 pos = { 10.5f, 20.0f, -5.2f };
    PUSH_STRUCT(&arr, &pos);

    ColorUnion col;
    col.rgba = 0xFF00FF00;
    PUSH_UNION(&arr, &col);

    printf("\n=== 5. PUSHING FUNCTION & FILE POINTERS ===\n");
    PUSH_FUNC(&arr, &exhaustive_callback);

    FILE* test_log = fopen("exhaustive_test.log", "w");
    if (test_log) {
        PUSH_FILE(&arr, test_log);
    }

    // Print out the absolute state of the entire collection
    PrintCollection(&arr);

    printf("\n=== 6. TESTING MACROS & EXTRACTION ===\n");
    // Find where TYPE_STRUCT and TYPE_UNION landed dynamically or access them by known indices
    // Let's verify macro retrieval for the struct (Vector3)
    // (Note: Struct is located at index 17 based on our push sequence)
    float vx = GET_STRUCT_MEMBER(&arr.slots[17], offsetof(Vector3, x), float);
    float vy = GET_STRUCT_MEMBER(&arr.slots[17], offsetof(Vector3, y), float);
    printf("Extracted Vector3 -> X: %.1f, Y: %.1f\n", vx, vy);

    // Verify macro retrieval for the union (ColorUnion)
    unsigned int retrieved_rgba = GET_UNION_MEMBER(&arr.slots[18], unsigned int);
    printf("Extracted ColorUnion -> RGBA: 0x%08X\n", retrieved_rgba);

    printf("\n=== 7. TESTING POP BEHAVIOR FOR CODE & FILES ===\n");
    // Pop File Pointer
    Element popped_file = pop_element(&arr);
    FILE* fp = (FILE*)popped_file.ptr;
    if (fp) {
        fprintf(fp, "Exhaustive VoidStar test stream active.\n");
        fclose(fp);
        printf("[File Test] File successfully written and closed.\n");
    }

    // Pop Function Pointer & Execute
    Element popped_func = pop_element(&arr);
    void (*cb)() = (void (*)())popped_func.ptr;
    if (cb) {
        cb();
    }

    printf("\n=== 8. FINAL SYSTEM CLEANUP ===\n");
    DestroyCollection(&arr);

    return 0;
}