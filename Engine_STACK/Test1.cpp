#include "../core/include/T_LCL_DEFS.h"
#include <stdio.h>
#include <string.h>

typedef struct 
{ 
    int id; 
    float score; 
} Player;   /* 8 bytes -> fits one cell */

void sample_func(void) 
{ 
    printf("   -> called through TYPE_FUNC_PTR!\n"); 
}

int main(void)
{
    /* Two tables, two different capacities, chosen right here at the call
       site - no global STACK_ARRAY_CAPACITY involved anymore. */
    TABLE_STACK_DECLARE(small_tb, 4);     /* only 4 slots  */
    TABLE_STACK_DECLARE(big_tb, 128);   /* 128 slots     */
    TABLE_STACK_DECLARE_DEFAULT(default_tb); /* uses TABLE_STACK_DEFAULT_CAPACITY (32) */

    vGetTableStack(&small_tb);
    vGetTableStack(&big_tb);
    vGetTableStack(&default_tb);

    printf("small_tb.capacity = %d\n", small_tb.capacity);
    printf("big_tb.capacity   = %d\n", big_tb.capacity);
    printf("default_tb.capacity = %d\n\n", default_tb.capacity);

    /* Fill small_tb to its (tiny) limit and prove overflow is rejected */
    for (int i = 0; i < 4; i++) 
    {
        PUSH_INT_STACK(&small_tb, i * 10);
        bool overflowed = PUSH_INT_STACK(&small_tb, 999);
    }

    printf("\nsmall_tb overflow push -> %s\n", overflowed ? "ACCEPTED (bug!)" : "correctly rejected");

    /* big_tb can hold what small_tb couldn't */
    for (int i = 0; i < 50; i++) 
    {
        PUSH_INT_STACK(&big_tb, i);
        printf("big_tb count after 50 pushes = %d (capacity %d)\n", big_tb.count, big_tb.capacity);
    }
    
    Player p = { 7, 91.5f };
    PUSH_STRUCT_STACK(&default_tb, &p);
    PUSH_FUNC_STACK(&default_tb, (void*)sample_func);
    vPrintTableStack(&default_tb);

    TableSlotStack e = sPopSlotStack(&default_tb);
    if (e.type == TYPE_FUNC_PTR) { void (*fn)(void) = (void (*)(void))e.value; fn(); }

    vDropTableStack(&small_tb);
    vDropTableStack(&big_tb);
    vDropTableStack(&default_tb);
    return 0;
}