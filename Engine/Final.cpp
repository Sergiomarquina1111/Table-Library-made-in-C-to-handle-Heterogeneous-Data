#include "../core/E_LIBFXNS_ARRAY_BASED.cpp"

// 1. Test Struct
typedef struct Player {
    int id;
    float score;
} Player;

// 2. Test Union
typedef union DataPacket {
    int int_val;
    float float_val;
} DataPacket;

// 3. Test Function
void engine_callback() {
    printf("[Callback] Function pointer successfully executed from code segment!\n");
}

int main(void) {
    ElementArray arr;
    InitCollection(&arr, 5);

    printf("\n=== 1. TESTING PRIMITIVES & PUSH_LIST ===\n");
    PUSH_LIST(&arr, 3, TYPE_INT, 999, TYPE_FLOAT, 13.37f, TYPE_CHAR, 'Z');

    printf("\n=== 2. TESTING STRUCT & UNION PUSH ===\n");
    Player player = { 42, 99.9f };
    PUSH_STRUCT(&arr, &player);

    DataPacket packet;
    packet.float_val = 3.14159f;
    PUSH_UNION(&arr, &packet);

    printf("\n=== 3. TESTING FUNCTION & FILE POINTERS ===\n");
    PUSH_FUNC(&arr, &engine_callback);

    FILE* test_file = fopen("voidstar_test.log", "w");
    if (test_file) {
        PUSH_FILE(&arr, test_file);
    }

    // Print out the state of the entire collection
    PrintCollection(&arr);

    printf("\n=== 4. TESTING STRUCT & UNION EXTRACTION MACROS ===\n");
    // Slot 3 was our struct (indices: 0=int, 1=float, 2=char, 3=struct)
    int retrieved_id = GET_STRUCT_MEMBER(&arr.slots[3], offsetof(Player, id), int);
    float retrieved_score = GET_STRUCT_MEMBER(&arr.slots[3], offsetof(Player, score), float);
    printf("Extracted Player -> ID: %d, Score: %.1f\n", retrieved_id, retrieved_score);

    // Slot 4 was our union
    float retrieved_packet = GET_UNION_MEMBER(&arr.slots[4], float);
    printf("Extracted Union Packet (float view): %.5f\n", retrieved_packet);

    printf("\n=== 5. TESTING POP & FUNCTION EXECUTION ===\n");
    // Pop the file pointer
    Element popped_file = pop_element(&arr);
    FILE* f_ptr = (FILE*)popped_file.ptr;
    if (f_ptr) {
        fprintf(f_ptr, "VoidStar Engine File Stream Active.\n");
        fclose(f_ptr);
        printf("[File Test] Log written and closed successfully.\n");
    }

    // Pop the function pointer and execute it
    Element popped_func = pop_element(&arr);
    void (*cb)() = (void (*)())popped_func.ptr;
    cb();

    printf("\n=== 6. CLEANING UP REMAINING COLLECTION ===\n");
    DestroyCollection(&arr);

    return 0;
}