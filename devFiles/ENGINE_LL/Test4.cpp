#include <cstdio>
#include <cstring>
#include <cassert>
#include "../core/LinkedListBased/include/T_LL_STACK_DEF.h"

static int tests_run = 0;
static int tests_passed = 0;

#define CHECK(cond, msg) do { \
    tests_run++; \
    if (cond) { tests_passed++; printf("  [PASS] %s\n", msg); } \
    else      { printf("  [FAIL] %s  (line %d)\n", msg, __LINE__); } \
} while (0)

void section(const char* title)
{
    printf("\n=== %s ===\n", title);
}

int main()
{
    StackTableList list; // entirely stack-allocated, pool included

    // ------------------------------------------------------------------
    section("Construction / empty-state checks");
    // ------------------------------------------------------------------
    tStackFormLinkedTable(&list);
    CHECK(S_IS_EMPTY(&list), "fresh list is empty");
    CHECK(!S_IS_FULL(&list), "fresh list is not full");
    CHECK(list.count == 0, "fresh list count == 0");
    CHECK(S_CAPACITY == 100, "S_CAPACITY macro resolves to 100");

    // ------------------------------------------------------------------
    section("INT wrappers");
    // ------------------------------------------------------------------
    S_PUSH_INT(&list, 10);
    S_PUSH_INT(&list, 20);
    S_PUSH_INT_FRONT(&list, 5);
    CHECK(list.count == 3, "int push count == 3");
    CHECK(S_GET_INT(&list, 0) == 5, "front int is 5");
    CHECK(S_GET_INT(&list, 1) == 10, "middle int is 10");
    CHECK(S_GET_INT(&list, 2) == 20, "back int is 20");
    CHECK(S_FIND_INT(&list, 10) == 1, "S_FIND_INT locates value");
    CHECK(S_FIND_INT(&list, 999) == -1, "S_FIND_INT misses absent value");
    S_INSERT_INT(&list, 1, 777);
    CHECK(S_GET_INT(&list, 1) == 777, "insert-at-index works");
    int removed = S_REMOVE_INT(&list, 1);
    CHECK(removed == 777, "remove-at-index returns correct value");
    CHECK(list.count == 3, "count restored after insert+remove");
    CHECK(S_POP_INT(&list) == 5, "pop front int");
    CHECK(S_POP_INT_BACK(&list) == 20, "pop back int");
    CHECK(list.count == 1, "count == 1 after pops");
    tStackDestroyList(&list);

    // ------------------------------------------------------------------
    section("FLOAT / DOUBLE wrappers (bit-packing correctness)");
    // ------------------------------------------------------------------
    S_PUSH_FLOAT(&list, 3.14159f);
    S_PUSH_FLOAT(&list, -2.5f);
    CHECK(S_GET_FLOAT(&list, 0) == 3.14159f, "float round-trips exactly");
    CHECK(S_GET_FLOAT(&list, 1) == -2.5f, "negative float round-trips");
    CHECK(S_FIND_FLOAT(&list, -2.5f) == 1, "S_FIND_FLOAT locates value");
    tStackDestroyList(&list);

    S_PUSH_DOUBLE(&list, 2.718281828459045);
    S_INSERT_DOUBLE(&list, 1, 1.41421356);
    CHECK(S_GET_DOUBLE(&list, 0) == 2.718281828459045, "double round-trips exactly");
    CHECK(S_GET_DOUBLE(&list, 1) == 1.41421356, "inserted double round-trips");
    tStackDestroyList(&list);

    // ------------------------------------------------------------------
    section("CHAR / LONG / SHORT wrappers");
    // ------------------------------------------------------------------
    S_PUSH_CHAR(&list, 'A');
    S_PUSH_CHAR(&list, 'Z');
    CHECK(S_GET_CHAR(&list, 0) == 'A', "char round-trips");
    CHECK(S_FIND_CHAR(&list, 'Z') == 1, "S_FIND_CHAR locates value");
    tStackDestroyList(&list);

    // NOTE: `long` is platform-dependent -- 64-bit on Linux/macOS (LP64) but
    // only 32-bit on Windows (LLP64), even in 64-bit builds. Use a value
    // that safely fits in the C-guaranteed minimum 32-bit range so this
    // test is portable across both.
    long safe_long_value = 2000000000L; // fits comfortably within INT32/LONG32 range
    S_PUSH_LONG(&list, safe_long_value);
    CHECK(S_GET_LONG(&list, 0) == safe_long_value, "long round-trips");
    tStackDestroyList(&list);

    S_PUSH_SHORT(&list, (short)-12345);
    CHECK(S_GET_SHORT(&list, 0) == (short)-12345, "short round-trips");
    tStackDestroyList(&list);

    // ------------------------------------------------------------------
    section("Generic pointer / struct / union / func / file wrappers");
    // ------------------------------------------------------------------
    int dummy = 42;
    S_PUSH_PTR(&list, &dummy, TYPE_INT_STAR);
    CHECK(S_GET_PTR(&list, 0) == &dummy, "generic pointer round-trips");
    tStackDestroyList(&list);

    struct Point { int x, y; } p = { 1, 2 };
    S_PUSH_STRUCT(&list, &p);
    CHECK(S_GET_STRUCT(&list, 0) == &p, "struct pointer round-trips");
    CHECK(((Point*)S_GET_STRUCT(&list, 0))->x == 1, "struct contents intact through the pool");
    tStackDestroyList(&list);

    void* fake_func = (void*)main; // just need a non-null function pointer
    S_PUSH_FUNC(&list, fake_func);
    CHECK(S_GET_FUNC(&list, 0) == fake_func, "function pointer round-trips");
    tStackDestroyList(&list);

    // ------------------------------------------------------------------
    section("Topology: insert / remove / reverse across the middle");
    // ------------------------------------------------------------------
    for (int i = 0; i < 5; i++) S_PUSH_INT(&list, i);   // 0 1 2 3 4
    S_REVERSE_LIST(&list);                              // 4 3 2 1 0
    CHECK(S_GET_INT(&list, 0) == 4 && S_GET_INT(&list, 4) == 0, "reverse flips order");
    tStackRemoveAt(&list, 2); // remove middle element (value 2)
    CHECK(list.count == 4, "count after middle removal");
    CHECK(S_GET_INT(&list, 2) == 1, "topology correctly bridged after middle removal");
    tStackDestroyList(&list);

    // ------------------------------------------------------------------
    section("Capacity limit: fill to exactly 100, verify overflow rejection");
    // ------------------------------------------------------------------
    for (int i = 0; i < STACK_TABLE_CAPACITY; i++) S_PUSH_INT(&list, i);
    CHECK(list.count == STACK_TABLE_CAPACITY, "filled to exactly 100 nodes");
    CHECK(S_IS_FULL(&list), "S_IS_FULL reports true at capacity");

    printf("  -- attempting 101st push (expect a printed error below) --\n");
    S_PUSH_INT(&list, -1);
    CHECK(list.count == STACK_TABLE_CAPACITY, "101st push rejected, count unchanged");
    CHECK(S_GET_INT(&list, 99) == 99, "existing 100th element untouched by rejected push");

    // ------------------------------------------------------------------
    section("Pool recycling: drain fully, then refill fully (proves reuse, not one-shot)");
    // ------------------------------------------------------------------
    for (int i = 0; i < STACK_TABLE_CAPACITY; i++) S_POP_INT(&list);
    CHECK(S_IS_EMPTY(&list), "list empty after full drain");
    CHECK(!S_IS_FULL(&list), "not full after drain");

    for (int i = 0; i < STACK_TABLE_CAPACITY; i++) S_PUSH_INT(&list, i * 3);
    CHECK(list.count == STACK_TABLE_CAPACITY, "refilled to 100 after drain");
    CHECK(S_GET_INT(&list, 50) == 150, "refilled values are correct, not stale");
    tStackDestroyList(&list);

    // ------------------------------------------------------------------
    section("Repeated drain/refill cycles (stress the free list)");
    // ------------------------------------------------------------------
    bool cycles_ok = true;
    for (int cycle = 0; cycle < 20; cycle++)
    {
        for (int i = 0; i < STACK_TABLE_CAPACITY; i++) S_PUSH_INT(&list, i);
        if (list.count != STACK_TABLE_CAPACITY) cycles_ok = false;
        for (int i = 0; i < STACK_TABLE_CAPACITY; i++)
        {
            int v = S_POP_INT(&list);
            if (v != i) cycles_ok = false;
        }
        if (!S_IS_EMPTY(&list)) cycles_ok = false;
    }
    CHECK(cycles_ok, "20 fill/drain cycles all correct (no leaked or corrupted slots)");

    // ------------------------------------------------------------------
    section("Multiple independent stack-allocated lists (no shared state)");
    // ------------------------------------------------------------------
    StackTableList listA, listB;
    tStackFormLinkedTable(&listA);
    tStackFormLinkedTable(&listB);
    S_PUSH_INT(&listA, 111);
    S_PUSH_INT(&listB, 222);
    CHECK(S_GET_INT(&listA, 0) == 111 && S_GET_INT(&listB, 0) == 222,
        "two independent stack lists don't share pool state");
    CHECK(listA.count == 1 && listB.count == 1, "independent counts");

    // ------------------------------------------------------------------
    printf("\n================================\n");
    printf("RESULTS: %d / %d tests passed\n", tests_passed, tests_run);
    printf("================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}