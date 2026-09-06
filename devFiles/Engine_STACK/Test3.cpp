#include "../core/include/T_LCL_DEFS.h"
#include <stdio.h>

void print_slot_cb(int index, TableSlotStack slot)
{
    printf("   [%d] %s\n", index, sTypeNameStack(slot.type));
}

int main(void)
{
    vGetTableStack(tb, 16);

    /* --- bulk array push, like Python's list.extend() --- */
    int    ints[] = { 5, 3, 9, 1, 7 };
    float  floats[] = { 1.5f, 2.5f };
    int pushed_i = iPushIntArrayStack(&tb, ints, 5);
    int pushed_f = iPushFloatArrayStack(&tb, floats, 2);
    printf("Bulk-pushed %d ints, %d floats. len(tb) = %d\n", pushed_i, pushed_f, iCountStack(&tb));

    /* --- query ops --- */
    printf("\ncontains INT?   %s\n", bContainsTypeStack(&tb, TYPE_INT) ? "yes" : "no");
    printf("count of INT:   %d\n", iCountOfTypeStack(&tb, TYPE_INT));
    printf("first INT at:   %d\n", iFindTypeStack(&tb, TYPE_INT));
    printf("sum of ints:    %d\n", iSumIntStack(&tb));
    printf("sum of floats:  %.2f\n", dSumFloatStack(&tb));

    int mn, mx;
    if (bMinIntStack(&tb, &mn) && bMaxIntStack(&tb, &mx))
        printf("min int: %d, max int: %d\n", mn, mx);

    /* --- random access, like tb[i] --- */
    TableSlotStack third = sGetSlotAtStack(&tb, 2);
    printf("\ntb[2] = %s -> %d\n", sTypeNameStack(third.type), GET_STACK_VALUE(third, int));

    /* --- Forth-style stack ops --- */
    printf("\nBefore dup/swap, len=%d\n", iCountStack(&tb));
    bDupTopStack(&tb);
    printf("After bDupTopStack, len=%d, top=%s\n", iCountStack(&tb), sTypeNameStack(sPeekTypeStack(&tb)));
    bSwapTopStack(&tb);
    printf("After bSwapTopStack, top is now: %s\n", sTypeNameStack(sPeekTypeStack(&tb)));
    bDropTopStack(&tb);
    printf("After bDropTopStack (silent pop), len=%d\n", iCountStack(&tb));

    /* --- reverse, like Python's list.reverse() --- */
    printf("\nBefore reverse:\n");
    vForEachSlotStack(&tb, print_slot_cb);
    vReverseStack(&tb);
    printf("After reverse:\n");
    vForEachSlotStack(&tb, print_slot_cb);

    /* --- clone into a second table --- */
    vGetTableStack(clone_tb, 20);
    bool cloned = bCloneTableStack(&clone_tb, &tb);
    printf("\nCloned? %s | clone_tb.count = %d\n", cloned ? "yes" : "no", clone_tb.count);

    /* --- clone into a too-small table, should fail cleanly --- */
    vGetTableStack(tiny_tb, 2);
    bool clone_fail = bCloneTableStack(&tiny_tb, &tb);
    printf("Clone into too-small table -> %s\n", clone_fail ? "ACCEPTED (bug!)" : "correctly rejected");

    vDropTableStack(&tb);
    vDropTableStack(&clone_tb);
    vDropTableStack(&tiny_tb);
    return 0;
}