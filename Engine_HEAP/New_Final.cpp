#include "../core/include/T_DEFS.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// --- Helper Data & Callbacks for Testing ---

struct SampleStruct {
    int id;
    double score;
};

void sampleFunction() {
    printf("   [Callback] Executing sampleFunction()\n");
}

void printVisitorCallback(int index, TableSlot slot) {
    printf("   [Visitor] Slot #%d -> Type Tag: %d (%s)\n",
        index, slot.type, sTypeNameTable(slot.type));
}

// --- Test Modules ---

void test_initialization_and_primitives() {
    printf("=========================================\n");
    printf(" TEST 1: Initialization & Primitives\n");
    printf("=========================================\n");

    Table tbl;
    vGetTable(&tbl, 3); // Initialize with initial capacity of 3

    // Push primitives
    bPushInt(&tbl, 42);
    bPushFloat(&tbl, 3.1415f);
    bPushChar(&tbl, 'Z');
    bPushDouble(&tbl, 2.71828);

    vPrintTable(&tbl);

    // Verify pops
    TableSlot top = sPopSlot(&tbl);
    assert(top.type == TYPE_DOUBLE);
    if (top.ptr) {
        printf("Popped Double Value: %.5f\n", *(double*)top.ptr);
        free(top.ptr); // Clean up memory returned by pop for primitives
    }

    vPrintTable(&tbl);
    vDropTable(&tbl);
}

void test_complex_and_pointers() {
    printf("=========================================\n");
    printf(" TEST 2: Complex Types & Function Pointers\n");
    printf("=========================================\n");

    Table tbl;
    vGetTable(&tbl, 5);

    // Struct push (Deep copy)
    SampleStruct s1 = { 101, 98.5 };
    bPushStruct(&tbl, &s1, sizeof(SampleStruct));

    // Function pointer push (Raw pointer)
    bPushFunc(&tbl, (void*)sampleFunction);

    // Pointer wrapper push
    int target_val = 999;
    bPushPtr(&tbl, &target_val, TYPE_INT_STAR);

    vPrintTable(&tbl);

    // Verify function pointer execution from table
    TableSlot func_slot = sGetSlotAtTable(&tbl, 1);
    if (func_slot.type == TYPE_FUNC_PTR && func_slot.ptr) {
        void (*func_ptr)() = (void (*)())func_slot.ptr;
        func_ptr();
    }

    vDropTable(&tbl);
}

void test_variadic_and_arrays() {
    printf("=========================================\n");
    printf(" TEST 3: Variadic Pushes & Array Ingestion\n");
    printf("=========================================\n");

    Table tbl;
    vGetTable(&tbl, 2);

    // Batch push using variadic list
    bPushList(&tbl, 3, TYPE_INT, 100, TYPE_FLOAT, 5.5f, TYPE_CHAR, 'A');

    // Push array of ints
    int numbers[] = { 10, 20, 30, 40 };
    iPushIntArrayTable(&tbl, numbers, 4);

    vPrintTable(&tbl);
    vDropTable(&tbl);
}

void test_stack_parity_and_math() {
    printf("=========================================\n");
    printf(" TEST 4: Stack Ops, Math & Queries\n");
    printf("=========================================\n");

    Table tbl;
    vGetTable(&tbl, 10);

    bPushInt(&tbl, 15);
    bPushInt(&tbl, 5);
    bPushInt(&tbl, 45);
    bPushInt(&tbl, 25);

    printf("Count of TYPE_INT: %d\n", iCountOfTypeTable(&tbl, TYPE_INT));
    printf("Sum of Ints: %d\n", iSumIntTable(&tbl));

    int min_val = 0, max_val = 0;
    if (bMinIntTable(&tbl, &min_val)) printf("Min Int: %d\n", min_val);
    if (bMaxIntTable(&tbl, &max_val)) printf("Max Int: %d\n", max_val);

    // Test Dup & Swap
    printf("\n-- Duplicating Top & Swapping --\n");
    bDupTopTable(&tbl);   // Duplicate 25
    bSwapTopTable(&tbl);  // Swap top two
    vPrintTable(&tbl);

    // Reverse Table
    printf("\n-- Reversing Table --\n");
    vReverseTable(&tbl);
    vPrintTable(&tbl);

    vDropTable(&tbl);
}

void test_buffer_management_and_cloning() {
    printf("=========================================\n");
    printf(" TEST 5: Buffer Allocation, Insert/Remove & Cloning\n");
    printf("=========================================\n");

    Table src;
    vGetTable(&src, 10);

    bPushInt(&src, 100);
    bPushInt(&src, 300);

    // Insert at index 1
    int* val_mid = (int*)malloc(sizeof(int));
    *val_mid = 200;
    TableSlot mid_node = { val_mid, TYPE_INT };
    bInsertAtTable(&src, 1, mid_node);

    vPrintTable(&src);

    // Reserve extra capacity and shrink to fit
    vReserveTable(&src, 10);
    printf("Capacity after reserve: %d\n", src.capacity);

    vShrinkToFitTable(&src);
    printf("Capacity after shrink-to-fit: %d\n", src.capacity);

    // Clone table
    Table dest;
    vGetTable(&dest, 2);
    bCloneTable(&dest, &src);

    printf("\n-- Cloned Table Output --\n");
    vPrintTable(&dest);

    // Foreach Visitor
    printf("-- Testing Foreach Visitor --\n");
    vForEachSlotTable(&dest, printVisitorCallback);

    // Remove item at index 0
    bRemoveAtTable(&dest, 0);
    vPrintTable(&dest);

    vDropTable(&src);
    vDropTable(&dest);
}

// --- Main Entry Point ---

int main() {
    printf("=========================================\n");
    printf("      STARTING TABLE LIBRARY TESTS       \n");
    printf("=========================================\n\n");

    test_initialization_and_primitives();
    test_complex_and_pointers();
    test_variadic_and_arrays();
    test_stack_parity_and_math();
    test_buffer_management_and_cloning();

    printf("=========================================\n");
    printf("       ALL TESTS EXECUTED SUCCESSFULLY   \n");
    printf("=========================================\n");

    return 0;
}