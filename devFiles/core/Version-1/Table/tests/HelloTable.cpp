/* ============================================================================
   hello_table.cpp — "Hello, World!" for the VOIDSTAR/TABLE library.

   This is NOT a test suite - it's a short, friendly walkthrough for someone
   picking up the library for the very first time. It touches EVERY container
   type table.h offers, one at a time, using only the most common, everyday
   operations on each - just enough for a new user to read top to bottom and
   come away knowing "oh, that's how you use this one."

   The five containers, in the order they appear below:
     1. Table            - heap-based dynamic array (the one you'll reach
                            for most often)
     2. TableStack        - fixed-capacity array that lives on the stack
                            (no heap allocation at all)
     3. TableList          - heap-based doubly-linked list
     4. StackTableList      - doubly-linked list backed by a fixed pool
                              (no per-node heap allocation)
     5. TableMap              - hash map keyed by pointer identity

   Build (MSVC):
       cl.exe hello_table.cpp ../src/table.cpp

   Build (GCC/Clang, as C++):
       g++ -std=c++11 hello_table.cpp ../src/table.cpp -o hello_table

   For the exhaustive function-by-function coverage test, see test1.cpp
   instead - this file is deliberately small and skips almost everything.
   ============================================================================ */

#include <table.h>
#include <stdio.h>

#pragma comment(lib , "table.lib")

static void print_banner(const char* title)
{
    printf("\n========================================\n");
    printf(" %s\n", title);
    printf("========================================\n");
}

/* ============================================================================
   1. Table — heap-based dynamic array
   ============================================================================ */
static void hello_table(void)
{
    print_banner("1. Table (heap dynamic array)");

    /* Create a table. The second argument is how many slots to start with
       (it grows on its own after that). The third reserves a shared memory
       arena for internal use - 0 just means "use plain malloc," which is
       fine for an example this small. */
    Table myTable;
    vGetTable(&myTable, 8, 0);

    /* A Table can hold ints, floats, chars, strings, your own structs -
       almost anything - all mixed together in the same table. */
    bPushInt(&myTable, 42);
    bPushFloat(&myTable, 3.14f);
    bPushChar(&myTable, 'A');

    const char* greeting = "Hello from Table!";
    bPushPtr(&myTable, (void*)greeting, TYPE_CHAR_STAR);

    printf("Pushed 4 values in. Here's everything in the table:\n");
    for (int i = 0; i < myTable.count; i++)
    {
        TableSlot_DA slot = sGetSlotAtTable(&myTable, i);
        printf("  [%d] type=%-6s  ", i, sTypeNameTable(slot.type));

        switch (slot.type)
        {
        case TYPE_INT:       printf("value=%d\n", *(int*)slot.ptr);          break;
        case TYPE_FLOAT:     printf("value=%.2f\n", *(float*)slot.ptr);      break;
        case TYPE_CHAR:      printf("value='%c'\n", *(char*)slot.ptr);       break;
        case TYPE_CHAR_STAR: printf("value=\"%s\"\n", *(char**)slot.ptr);    break;
        default:             printf("(not printed by this example)\n");     break;
        }
    }

    printf("Does it contain an int? %s\n",
        bContainsTypeTable(&myTable, TYPE_INT) ? "Yes" : "No");

    /* Always clean up when you're done - same idea as fclose() or free(). */
    vDropTable(&myTable);
    printf("Table dropped.\n");
}

/* ============================================================================
   2. TableStack — fixed-capacity array that lives on the stack
   ============================================================================ */
static void hello_table_stack(void)
{
    print_banner("2. TableStack (fixed stack array)");

    /* vGetTableStack is a macro: it declares the backing array and the
       TableStack struct for you, right here on the stack - no malloc at
       all. The number is the fixed capacity; once it's full, pushes fail
       rather than growing (there's nowhere for it to grow into). */
    vGetTableStack(myStack, 8);

    bPushIntStack(&myStack, 100);
    bPushFloatStack(&myStack, 2.5f);
    bPushCharStack(&myStack, 'S');

    printf("Pushed 3 values in. Here's everything in the stack:\n");
    vPrintTableStack(&myStack);

    printf("How many values are in it? %d\n", iCountStack(&myStack));

    /* No heap memory was used, so there's nothing to free - dropping just
       resets the slots back to empty so the stack can be reused. */
    vDropTableStack(&myStack);
    printf("Stack reset.\n");
}

/* ============================================================================
   3. TableList — heap-based doubly-linked list
   ============================================================================ */
static void hello_table_list(void)
{
    print_banner("3. TableList (heap doubly-linked list)");

    /* Unlike Table, a TableList has no fixed backing buffer at all - it's
       just nodes linked together, so it never needs to "grow." */
    TableList myList;
    tFormLinkedTable(&myList);

    tPushIntBack(&myList, 1);
    tPushIntBack(&myList, 2);
    tPushIntFront(&myList, 0); /* now: 0, 1, 2 */

    printf("Pushed 0, 1, 2 in. Here's the list:\n");
    vPrintList(&myList);

    printf("Value at index 1: %d\n", tGetIntAt(&myList, 1));
    printf("How many nodes? %d\n", myList.count);

    /* Frees every node in the list. */
    tDestroyList(&myList);
    printf("List destroyed.\n");
}

/* ============================================================================
   4. StackTableList — doubly-linked list backed by a fixed pool
   ============================================================================ */
static void hello_stack_table_list(void)
{
    print_banner("4. StackTableList (pool-backed linked list)");

    /* This behaves like TableList (a doubly-linked list you can push/pop
       from either end), but every node comes from a fixed-size internal
       pool instead of individual heap allocations - so there's still no
       per-node malloc happening under the hood. */
    StackTableList myPoolList;
    tStackFormLinkedTable(&myPoolList);

    tStackPushIntBack(&myPoolList, 10);
    tStackPushIntBack(&myPoolList, 20);
    tStackPushIntFront(&myPoolList, 5); /* now: 5, 10, 20 */

    printf("Pushed 5, 10, 20 in. Here's the list:\n");
    vStackPrintList(&myPoolList);

    printf("Value at index 2: %d\n", tStackGetIntAt(&myPoolList, 2));
    printf("How many nodes? %d\n", myPoolList.count);

    /* Returns every node back to the internal pool for reuse. */
    tStackDestroyList(&myPoolList);
    printf("List destroyed (nodes returned to the pool).\n");
}

/* ============================================================================
   5. TableMap — hash map keyed by pointer identity
   ============================================================================ */
static void hello_table_map(void)
{
    print_banner("5. TableMap (hash map)");

    /* A TableMap doesn't take a capacity argument - its pool size is fixed
       at compile time via HASH_POOL_CAPACITY (1024 by default). */
    TableMap myMap;
    vFormHashMap(&myMap);

    /* Each insert malloc's its own copy and hands you back the key (the
       address of that copy) through out_key - hang onto it, since that's
       what you'll use to look the value up again later. */
    void* ageKey = NULL;
    void* scoreKey = NULL;
    tMapInsertInt(&myMap, 30, &ageKey);
    tMapInsertFloat(&myMap, 99.5f, &scoreKey);

    printf("Inserted 2 values. Here's the map:\n");
    vPrintHashMap(&myMap);

    TableSlot_HM node = sMapGetNode(&myMap, ageKey);
    if (node.type == TYPE_INT)
        printf("Looked up 'age' by its key: %d\n", *(int*)node.ptr);

    printf("How many values are in it? %d\n", iCountMap(&myMap));

    /* Frees every value the map owns and resets it to empty. */
    vDropHashMap(&myMap);
    printf("Map dropped.\n");
}

/* ============================================================================
   main
   ============================================================================ */
int main(void)
{
    printf("Hello, VOIDSTAR/TABLE! This tour visits all five containers.\n");

    hello_table();
    hello_table_stack();
    hello_table_list();
    hello_stack_table_list();
    hello_table_map();

    printf("\nThat's all five - goodbye!\n");
    return 0;
}