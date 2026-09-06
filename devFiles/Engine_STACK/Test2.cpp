#include "../core/include/T_LCL_DEFS.h"
#include <stdio.h>

typedef struct { int id; float score; } Player;   /* 8 bytes -> fits one cell */

/* Callback for vForEachSlotStack - the loop itself is the library's job now,
   this is just per-slot logic. */
void print_slot(int index, TableSlotStack slot)
{
    switch (slot.type)
    {
    case TYPE_INT:   printf("   [%d] int   = %d\n", index, GET_STACK_VALUE(slot, int));   break;
    case TYPE_FLOAT: printf("   [%d] float = %.2f\n", index, GET_STACK_VALUE(slot, float)); break;
    default:         printf("   [%d] (type tag %d)\n", index, slot.type); break;
    }
}

int main(void)
{
    /* One call: name + capacity, nothing else needed before you can push. */
    vGetTableStack(tb, 8);

    printf("Empty? %s | Full? %s | Remaining: %d\n", bIsTableStackEmpty(&tb) ? "yes" : "no",  bIsTableStackFull(&tb) ? "yes" : "no", iRemainingStack(&tb));

    PUSH_INT_STACK(&tb, 10);
    PUSH_INT_STACK(&tb, 20);
    PUSH_FLOAT_STACK(&tb, 3.5f);

    printf("\nAfter 3 pushes - Empty? %s | Remaining: %d\n",
        bIsTableStackEmpty(&tb) ? "yes" : "no", iRemainingStack(&tb));

    /* Peek: look at the top without touching it */
    TableSlotStack top = sPeekSlotStack(&tb);
    printf("Peeked top (type %d) -> float value = %.2f\n", top.type, GET_STACK_VALUE(top, float));
    printf("Count after peek (should be unchanged): %d\n", tb.count);

    /* forEach: no manual loop needed to walk every slot */
    printf("\nWalking the table with vForEachSlotStack:\n");
    vForEachSlotStack(&tb, print_slot);

    /* Fill it to capacity and prove bIsTableStackFull catches it */
    while (!bIsTableStackFull(&tb)) PUSH_INT_STACK(&tb, 0);
    printf("\nFull now? %s (capacity %d, count %d)\n",
        bIsTableStackFull(&tb) ? "yes" : "no", tb.capacity, tb.count);

    vDropTableStack(&tb);
    printf("Empty after drop? %s\n", bIsTableStackEmpty(&tb) ? "yes" : "no");

    /* A second table, different capacity, same one-call setup pattern */
    vGetTableStack(small, 2);
    PUSH_INT_STACK(&small, 1);
    PUSH_INT_STACK(&small, 2);
    printf("\nsmall.capacity=%d, full=%s\n", small.capacity, bIsTableStackFull(&small) ? "yes" : "no");

    vDropTableStack(&small);
    return 0;
}