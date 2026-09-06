/* ============================================================================
 * demo_table_stack_full.cpp
 *
 * Exercises EVERY function, inline helper, and macro declared in
 * T_LCL_DEFS.h, backed by T_LIBFXNS_ARR_LCL.cpp.
 *
 * Build (adjust include path to wherever T_LCL_DEFS.h / T_DEFS.h live):
 *   g++ -std=c++17 -Iinclude demo_table_stack_full.cpp T_LIBFXNS_ARR_LCL.cpp -o demo
 * ============================================================================ */

#include "../core/Array/include/T_LCL_DEFS.h"   /* adjust path, e.g. "include/T_LCL_DEFS.h" */
#include <stdio.h>

typedef struct { int id; short qty; }  SmallOrder;   /* <= sizeof(void*)   */
typedef union { int i; float f; }     SmallUnion;   /* <= sizeof(void*)   */

static void demo_func(void) { printf("   (demo_func was called through a popped function pointer)\n"); }

/* Visitor used with vForEachSlotStack(). */
static void print_visitor(int index, TableSlotStack slot)
{
    printf("   visit[%02d] -> ", index);
    vPrintSlotStack(slot);
}

int main(void)
{
    /* ---------- vGetTableStack / vGetTableStackDefault -------------------- */
    vGetTableStack(orders, 24);          /* explicit-capacity table          */
    vGetTableStackDefault(scratch);      /* default-capacity (32) table      */
    vGetTableStack(orders_copy, 24);     /* clone destination                */

    /* ---------- inline helpers (on an empty table) ------------------------ */
    printf("Empty? %s | Full? %s | Remaining: %d | Count: %d | Capacity: %d\n",
        bIsTableStackEmpty(&orders) ? "yes" : "no",
        bIsTableStackFull(&orders) ? "yes" : "no",
        iRemainingStack(&orders), iCountStack(&orders), iCapacityStack(&orders));

    /* ---------- bPushSlotStack (low-level manual push) --------------------- */
    int manual_int = 7;
    TableSlotStack manual_node = { 0 };
    memcpy(&manual_node.value, &manual_int, sizeof(manual_int));
    manual_node.type = TYPE_INT;
    bPushSlotStack(&orders, manual_node);

    /* ---------- typed push functions --------------------------------------- */
    bPushIntStack(&orders, 42);
    bPushFloatStack(&orders, 3.14f);
    bPushDoubleStack(&orders, 2.71828);
    bPushLongStack(&orders, 123456789L);
    bPushShortStack(&orders, (short)256);
    bPushCharStack(&orders, 'X');

    /* ---------- pointer / double-pointer / triple-pointer pushes ----------- */
    int val = 99;
    int* p = &val;
    int** pp = &p;
    int*** ppp = &pp;
    bPushPtrStack(&orders, &val, TYPE_INT_STAR);
    bPushDPtrStack(&orders, (void**)pp, TYPE_INT_STAR_DOUBLE);
    bPushTPtrStack(&orders, (void***)ppp, TYPE_INT_STAR_TRIPLE);

    /* ...and the equivalent convenience macros (same functions underneath) */
    float ptr_float = 1.0f;
    PUSH_INT_PTR_STACK(&orders, &val);
    PUSH_FLOAT_PTR_STACK(&orders, &ptr_float);
    PUSH_INT_DPTR_STACK(&orders, pp);
    PUSH_INT_TPTR_STACK(&orders, ppp);
    PUSH_VOID_PTR_STACK(&orders, &val);

    /* ---------- struct / union / func-ptr / file-ptr pushes ---------------- */
    SmallOrder order = { 1001, 5 };
    SmallUnion su; su.i = 77;
    bPushStructStack(&orders, &order, sizeof(order));
    bPushUnionStack(&orders, &su, sizeof(su));
    bPushFuncStack(&orders, (void*)demo_func);

    FILE* tmp = tmpfile();
    bPushFileStack(&orders, tmp);

    /* same three via macros */
    PUSH_STRUCT_STACK(&orders, &order);
    PUSH_UNION_STACK(&orders, &su);
    PUSH_FUNC_STACK(&orders, demo_func);
    if (tmp) PUSH_FILE_STACK(&orders, tmp);

    /* ---------- variadic bulk push: bPushListStack -------------------------- */
    bPushListStack(&orders, 4,
        TYPE_INT, 5,
        TYPE_FLOAT, 1.5,          /* promoted to double, cast back inside */
        TYPE_CHAR, 'A',
        TYPE_LONG, 999L);

    /* ---------- bulk array pushes: iPushIntArrayStack / iPushFloatArrayStack */
    int int_batch[] = { 10, 20, 30 };
    float float_batch[] = { 1.1f, 2.2f };
    int ipushed = iPushIntArrayStack(&scratch, int_batch, 3);
    int fpushed = iPushFloatArrayStack(&scratch, float_batch, 2);
    printf("Bulk-pushed %d int(s) and %d float(s) into `scratch`.\n", ipushed, fpushed);

    /* ---------- introspection / printing ------------------------------------ */
    vPrintTableStack(&orders);
    printf("TABLE_STACK_LEN: %d | TABLE_STACK_CAP: %d\n", TABLE_STACK_LEN(orders), TABLE_STACK_CAP(orders));
    printf("sTypeNameStack(TYPE_INT) = %s\n", sTypeNameStack(TYPE_INT));

    /* ---------- peek (type + full slot) -------------------------------------- */
    printf("Peek type: %s\n", sTypeNameStack(sPeekTypeStack(&orders)));
    TableSlotStack peeked = sPeekSlotStack(&orders);
    vPrintSlotStack(peeked);

    /* ---------- random access: sGetSlotAtStack / sGetTypeAtStack ------------- */
    TableSlotStack slot0 = sGetSlotAtStack(&orders, 0);
    printf("Slot 0 type: %s\n", sTypeNameStack(sGetTypeAtStack(&orders, 0)));
    vPrintSlotStack(slot0);

    /* ---------- stack manipulation: dup / swap / drop ------------------------ */
    bDupTopStack(&orders);
    bSwapTopStack(&orders);
    bDropTopStack(&orders);

    /* ---------- query / aggregate functions ----------------------------------- */
    printf("Count of INT slots: %d\n", iCountOfTypeStack(&orders, TYPE_INT));
    printf("Contains TYPE_CHAR: %s\n", bContainsTypeStack(&orders, TYPE_CHAR) ? "yes" : "no");
    printf("First INT at index: %d\n", iFindTypeStack(&orders, TYPE_INT));
    printf("Sum of ints: %d\n", iSumIntStack(&orders));
    printf("Sum of floats: %.4f\n", dSumFloatStack(&orders));

    int min_v, max_v;
    if (bMinIntStack(&orders, &min_v) && bMaxIntStack(&orders, &max_v))
        printf("Min int: %d | Max int: %d\n", min_v, max_v);

    /* ---------- iteration: vForEachSlotStack ------------------------------------ */
    printf("Iterating with a visitor:\n");
    vForEachSlotStack(&orders, print_visitor);

    /* ---------- bulk ops: vReverseStack / bCloneTableStack ---------------------- */
    vReverseStack(&orders);
    printf("After reverse:\n");
    vPrintTableStack(&orders);

    if (bCloneTableStack(&orders_copy, &orders))
        printf("Cloned into orders_copy, count = %d\n", iCountStack(&orders_copy));

    /* ---------- GET_STACK_VALUE macro -------------------------------------------- */
    TableSlotStack an_int_slot = sGetSlotAtStack(&orders_copy, iFindTypeStack(&orders_copy, TYPE_INT));
    printf("GET_STACK_VALUE extracted int: %d\n", GET_STACK_VALUE(an_int_slot, int));

    /* ---------- pop / drop everything --------------------------------------------- */
    printf("Draining `orders` via sPopSlotStack:\n");
    while (!bIsTableStackEmpty(&orders))
    {
        TableSlotStack s = sPopSlotStack(&orders);
        vPrintSlotStack(s);
    }

    /* ---------- reset the rest ------------------------------------------------------ */
    vDropTableStack(&scratch);
    vDropTableStack(&orders_copy);

    if (tmp) fclose(tmp);
    return 0;
}