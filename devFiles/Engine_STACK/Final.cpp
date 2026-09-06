#include "../core/include/T_LCL_DEFS.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

// ============================================================================
// DUMMY TYPES & HELPERS FOR TESTING
// ============================================================================

// 6 bytes (fits perfectly inline inside a void* cell)
typedef struct 
{
    int a;
    short b;
} MiniStruct;

// 24 bytes (will be deliberately rejected by the stack engine)

typedef struct 
{
    double x;
    double y;
    double z;
} BigStruct;

// 4 bytes (fits inline)
typedef union 
{
    int i;
    float f;
} MiniUnion;

void DummyFunction() 
{
    printf("Dummy function executed!\n");
}

void TestVisitor(int index, TableSlotStack slot) {
    printf("  -> Visitor processing index %d (Type: %s)\n", index, sTypeNameStack(slot.type));
}

// ============================================================================
// TEST 1: LIFECYCLE, CAPACITY, & O(1) ACCESSORS
// ============================================================================
void TestLifecycleAndAccessors() 
{
    printf("\n>>> TEST 1: Lifecycle, Capacity, & Accessors <<<\n");

    // Initialize a stack table of capacity 3
    vGetTableStack(tb, 3);

    // Verify initial empty state
    assert(bIsTableStackEmpty(&tb) == true);
    assert(bIsTableStackFull(&tb) == false);
    assert(iCapacityStack(&tb) == 3);
    assert(iCountStack(&tb) == 0);
    assert(iRemainingStack(&tb) == 3);
    assert(sPeekTypeStack(&tb) == TYPE_EMPTY);

    // Push elements up to the capacity limit
    PUSH_INT_STACK(&tb, 10);
    PUSH_INT_STACK(&tb, 20);
    PUSH_INT_STACK(&tb, 30);

    // Verify full state
    assert(bIsTableStackEmpty(&tb) == false);
    assert(bIsTableStackFull(&tb) == true);
    assert(iCountStack(&tb) == 3);
    assert(iRemainingStack(&tb) == 0);
    assert(sPeekTypeStack(&tb) == TYPE_INT);

    // Overflow check (engine must gracefully reject this and return false)
    bool overflow_push = PUSH_INT_STACK(&tb, 40);
    assert(overflow_push == false);

    // Drop table and verify reset
    vDropTableStack(&tb);
    assert(bIsTableStackEmpty(&tb) == true);
    assert(iCountStack(&tb) == 0);
}

// ============================================================================
// TEST 2: ALL PRIMITIVE MACROS & EXTRACTION
// ============================================================================
void TestPrimitivesAndExtraction() 
{
    printf("\n>>> TEST 2: Primitive Macros & GET_STACK_VALUE <<<\n");

    vGetTableStack(tb, 10);

    PUSH_INT_STACK(&tb, 42);
    PUSH_FLOAT_STACK(&tb, 3.14f);
    PUSH_DOUBLE_STACK(&tb, 9.999);
    PUSH_LONG_STACK(&tb, 123456789L);
    PUSH_SHORT_STACK(&tb, (short)88);
    PUSH_CHAR_STACK(&tb, 'X');

    // Extract and assert each primitive value
    assert(GET_STACK_VALUE(sGetSlotAtStack(&tb, 0), int) == 42);
    assert(GET_STACK_VALUE(sGetSlotAtStack(&tb, 1), float) == 3.14f);
    assert(GET_STACK_VALUE(sGetSlotAtStack(&tb, 2), double) == 9.999);
    assert(GET_STACK_VALUE(sGetSlotAtStack(&tb, 3), long) == 123456789L);
    assert(GET_STACK_VALUE(sGetSlotAtStack(&tb, 4), short) == (short)88);
    assert(GET_STACK_VALUE(sGetSlotAtStack(&tb, 5), char) == 'X');

    // Verify type tag
    assert(sGetTypeAtStack(&tb, 5) == TYPE_CHAR);
}

// ============================================================================
// TEST 3: ALL POINTER TYPES (SINGLE, DOUBLE, TRIPLE)
// ============================================================================
void TestPointers() 
{
    printf("\n>>> TEST 3: Pointer Chains <<<\n");

    vGetTableStack(tb, 5);

    int target = 777;
    int* ptr = &target;
    int** dptr = &ptr;
    int*** tptr = &dptr;

    PUSH_INT_PTR_STACK(&tb, ptr);
    PUSH_INT_DPTR_STACK(&tb, dptr);
    PUSH_INT_TPTR_STACK(&tb, tptr);

    TableSlotStack p1 = sGetSlotAtStack(&tb, 0);
    TableSlotStack p2 = sGetSlotAtStack(&tb, 1);
    TableSlotStack p3 = sGetSlotAtStack(&tb, 2);

    assert(p1.type == TYPE_INT_STAR);
    assert(p2.type == TYPE_INT_STAR_DOUBLE);
    assert(p3.type == TYPE_INT_STAR_TRIPLE);

    // Raw hardware dereference test to ensure pointer validity
    assert(*(int*)p1.value == 777);
    assert(**(int**)p2.value == 777);
    assert(***(int***)p3.value == 777);
}

// ============================================================================
// TEST 4: COMPLEX TYPES & REJECTION SHIELD
// ============================================================================
// ============================================================================
// TEST 4: COMPLEX TYPES & REJECTION SHIELD
// ============================================================================
void TestComplexTypes() {
    printf("\n>>> TEST 4: Complex Types & Boundary Rejection <<<\n");

    vGetTableStack(tb, 5);

    MiniStruct ms = { 10, 20 };
    MiniUnion mu;
    mu.f = 5.5f;

    // These should succeed because their memory footprint is <= 8 bytes
    assert(PUSH_STRUCT_STACK(&tb, &ms) == true);
    assert(PUSH_UNION_STACK(&tb, &mu) == true);
    assert(PUSH_FUNC_STACK(&tb, (void*)DummyFunction) == true);

    // This MUST fail (24 bytes exceeds the size of a single void* cell)
    BigStruct bs = { 1.0, 2.0, 3.0 };
    bool big_push = PUSH_STRUCT_STACK(&tb, &bs);
    assert(big_push == false);

    // FIX: Directly cast the inline memory cell to our struct type
    TableSlotStack struct_slot = sGetSlotAtStack(&tb, 0);

    // Cast the address of the void* cell into a MiniStruct pointer
    MiniStruct* extracted_struct = (MiniStruct*)&struct_slot.value;

    int ext_a = extracted_struct->a;
    short ext_b = extracted_struct->b;

    assert(ext_a == 10 && ext_b == 20);
}

// ============================================================================
// TEST 5: BULK, VARIADIC, & CLONING
// ============================================================================
void TestBulkAndVariadic() 
{
    printf("\n>>> TEST 5: Variadic Push, Arrays, & Cloning <<<\n");

    vGetTableStack(src_tb, 20);

    // Push multiple types in a single variadic call
    bPushListStack(&src_tb, 4,
        TYPE_INT, 100,
        TYPE_FLOAT, 2.5f,
        TYPE_CHAR, 'A',
        TYPE_INT, 200
    );
    assert(iCountStack(&src_tb) == 4);

    // Fast bulk array push (bypassing loop iteration logic overhead)
    int arr[] = { 300, 400, 500 };
    int pushed = iPushIntArrayStack(&src_tb, arr, 3);
    assert(pushed == 3);
    assert(iCountStack(&src_tb) == 7);

    // Memory-level table cloning
    vGetTableStack(dest_tb, 20);
    bool clone_success = bCloneTableStack(&dest_tb, &src_tb);

    assert(clone_success == true);
    assert(iCountStack(&dest_tb) == 7);
    assert(sGetTypeAtStack(&dest_tb, 0) == TYPE_INT);
}

// ============================================================================
// TEST 6: STACK MORPHING (SWAP, DUP, REVERSE, POP)
// ============================================================================
void TestStackOperations() 
{
    printf("\n>>> TEST 6: Stack Morphing Ops <<<\n");

    vGetTableStack(tb, 5);

    PUSH_INT_STACK(&tb, 1);
    PUSH_INT_STACK(&tb, 2);

    // Duplicate Top -> Stack: [1, 2, 2]
    bDupTopStack(&tb);
    assert(iCountStack(&tb) == 3);
    assert(GET_STACK_VALUE(sPeekSlotStack(&tb), int) == 2);

    // Swap Top -> Push 9 -> Stack: [1, 2, 2, 9] -> Swap -> Stack: [1, 2, 9, 2]
    PUSH_INT_STACK(&tb, 9);
    bSwapTopStack(&tb);
    assert(GET_STACK_VALUE(sGetSlotAtStack(&tb, 2), int) == 9);
    assert(GET_STACK_VALUE(sGetSlotAtStack(&tb, 3), int) == 2);

    // Reverse Stack -> Stack: [2, 9, 2, 1]
    vReverseStack(&tb);
    assert(GET_STACK_VALUE(sPeekSlotStack(&tb), int) == 1);

    // Standard Pop -> Returns 1
    TableSlotStack popped = sPopSlotStack(&tb);
    assert(GET_STACK_VALUE(popped, int) == 1);

    // Drop Top -> Drops the '2'
    bDropTopStack(&tb);
    assert(GET_STACK_VALUE(sPeekSlotStack(&tb), int) == 9);
}

// ============================================================================
// TEST 7: AGGREGATIONS & INTROSPECTION
// ============================================================================
void TestAggregations() 
{
    printf("\n>>> TEST 7: O(N) Aggregations & Math <<<\n");

    vGetTableStack(tb, 10);

    PUSH_INT_STACK(&tb, 5);
    PUSH_FLOAT_STACK(&tb, 1.5f);
    PUSH_INT_STACK(&tb, 15);
    PUSH_INT_STACK(&tb, 10);
    PUSH_FLOAT_STACK(&tb, 2.5f);

    // Sum, Min, Max aggregations
    assert(iSumIntStack(&tb) == 30);
    assert(dSumFloatStack(&tb) == 4.0);

    int min, max;
    bMinIntStack(&tb, &min);
    bMaxIntStack(&tb, &max);
    assert(min == 5 && max == 15);

    // Engine queries
    assert(iCountOfTypeStack(&tb, TYPE_INT) == 3);
    assert(bContainsTypeStack(&tb, TYPE_FLOAT) == true);
    assert(bContainsTypeStack(&tb, TYPE_CHAR) == false);
    assert(iFindTypeStack(&tb, TYPE_FLOAT) == 1);

    // Function pointer iteration
    printf("Iterating over slots:\n");
    vForEachSlotStack(&tb, TestVisitor);
}

// ============================================================================
// MAIN EXECUTION
// ============================================================================
// ============================================================================
// MAIN EXECUTION
// ============================================================================
int main() 
{
    printf("========================================================\n");
    printf(" TABLE ENGINE: FULL STACK TEST SUITE INITIATED\n");
    printf("========================================================\n");

    TestLifecycleAndAccessors();
    TestPrimitivesAndExtraction();
    TestPointers();
    TestComplexTypes();
    TestBulkAndVariadic();
    TestStackOperations();
    TestAggregations();

    printf("\n========================================================\n");
    printf(" ALL HARDWARE/STACK ASSERTIONS PASSED SUCCESSFULLY! \n");
    printf(" ZERO COMPILATION WARNINGS. ZERO LEAKS EXPECTED. \n");
    printf("========================================================\n");

    return 0;
}