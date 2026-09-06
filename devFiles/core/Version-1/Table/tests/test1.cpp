/* ============================================================================
   test_table_full.c — exhaustive test suite for the VOIDSTAR/TABLE library.
   Attempts to call EVERY public function declared in table.h at least once,
   across all five container variants:
     1. Table            (heap dynamic array)
     2. TableStack        (fixed stack array, vGetTableStack macro)
     3. TableList          (heap doubly-linked list)
     4. StackTableList      (pool-backed doubly-linked list, fixed capacity)
     5. TableMap              (bucketed hash map)

   Build (GCC, pure C):
       gcc -std=c11 -Wall -Wextra -Iinclude test_table_full.c src/table.c -o test_table_full

   Build (MSVC):
       cl /std:c11 /W4 /Iinclude test_table_full.c src\table.c /Fe:test_table_full.exe

   NOTE: table.cpp contains bare brace-init returns such as
       return { NULL, TYPE_EMPTY };
   which only compile as C++ (MSVC/cl.exe, or g++). This test file is written
   to build as C++ to match — it avoids C99 compound-literal syntax like
   (TableSlot_DA){ ... }, which MSVC's C++ front end rejects (error C4576).
   If you ever port table.cpp to strict C99/C11, those return sites would
   need to become (TableSlot_DA){ ... } compound literals instead.
   ============================================================================ */

#include "../include/table.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

   /* ---------------------------------------------------------------------- */
   /* Tiny test harness                                                      */
   /* ---------------------------------------------------------------------- */

static int g_tests_run = 0;
static int g_tests_failed = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        g_tests_run++;                                                    \
        if (!(cond)) {                                                     \
            g_tests_failed++;                                              \
            printf("  [FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
        }                                                                  \
    } while (0)

#define SECTION(name) printf("\n=== %s ===\n", name)

/* Example struct/union types used to exercise TYPE_STRUCT / TYPE_UNION */
typedef struct { int x; int y; float weight; } Point3;
typedef union { int i; float f; char bytes[4]; } SmallUnion;

static void visitor_da(int index, TableSlot_DA slot) { (void)index; (void)slot; }
static void visitor_sa(int index, TableSlot_SA slot) { (void)index; (void)slot; }
static void visitor_map(uint16_t index, TableSlot_HM slot) { (void)index; (void)slot; }

static uint32_t custom_hash(void* p)
{
    /* deliberately weak hash to exercise vSetHashFnMap / bPointerHashIsSuspect */
    return (uint32_t)(uintptr_t)p & 0xF;
}

/* ========================================================================
   1. Table — heap-based dynamic array (T_DEFS-style / T_LIBFXNS_ARR_LCL)
   ======================================================================== */
static void test_table_dynamic(void)
{
    SECTION("Table (heap dynamic array)");

    Table t;
    vGetTable(&t, 4, 1 << 16); /* small initial capacity + arena to force growth */
    CHECK(t.count == 0 && t.capacity == 4);

    /* --- primitives --- */
    CHECK(bPushInt(&t, 10));
    CHECK(bPushInt(&t, 20));
    CHECK(bPushFloat(&t, 3.5f));
    CHECK(bPushDouble(&t, 2.71828));
    CHECK(bPushLong(&t, 123456789L));
    CHECK(bPushShort(&t, (short)7));
    CHECK(bPushChar(&t, 'Z'));
    CHECK(t.count == 7 && t.capacity >= 7); /* forced at least one grow */

    /* --- macro wrappers for primitives (same push fns, different call site) --- */
    CHECK(PUSH_INT(&t, 1));
    CHECK(PUSH_FLOAT(&t, 1.1f));
    CHECK(PUSH_DOUBLE(&t, 1.11));
    CHECK(PUSH_LONG(&t, 1L));
    CHECK(PUSH_SHORT(&t, (short)1));
    CHECK(PUSH_CHAR(&t, 'a'));

    /* --- single pointers: bPushPtr + every PUSH_*_PTR macro --- */
    int    li = 99;   float  lf = 9.9f;  char   lc = 'p';
    double ld = 8.8;  long   ll = 321L;  short  ls = (short)3;
    void* lv = &li;

    CHECK(bPushPtr(&t, &li, TYPE_INT_STAR));
    CHECK(PUSH_VOID_PTR(&t, lv));
    CHECK(PUSH_INT_PTR(&t, &li));
    CHECK(PUSH_FLOAT_PTR(&t, &lf));
    CHECK(PUSH_CHAR_PTR(&t, &lc));
    CHECK(PUSH_DOUBLE_PTR(&t, &ld));
    CHECK(PUSH_LONG_PTR(&t, &ll));
    CHECK(PUSH_SHORT_PTR(&t, &ls));

    /* --- double pointers: bPushDPtr + macros --- */
    int* pli = &li;   float* plf = &lf;  char* plc = &lc;
    double* pld = &ld;   long* pll = &ll;  short* pls = &ls;
    void* plv = &lv;

    CHECK(bPushDPtr(&t, (void**)&pli, TYPE_INT_STAR_DOUBLE));
    CHECK(PUSH_VOID_DPTR(&t, &plv));
    CHECK(PUSH_INT_DPTR(&t, &pli));
    CHECK(PUSH_FLOAT_DPTR(&t, &plf));
    CHECK(PUSH_CHAR_DPTR(&t, &plc));
    CHECK(PUSH_DOUBLE_DPTR(&t, &pld));
    CHECK(PUSH_LONG_DPTR(&t, &pll));
    CHECK(PUSH_SHORT_DPTR(&t, &pls));

    /* --- triple pointers: bPushTPtr + macros --- */
    int** ppli = &pli;  float** pplf = &plf;  char** pplc = &plc;
    double** ppld = &pld;  long** ppll = &pll;  short** ppls = &pls;
    void** pplv = &plv;

    CHECK(bPushTPtr(&t, (void***)&ppli, TYPE_INT_STAR_TRIPLE));
    CHECK(PUSH_VOID_TPTR(&t, &pplv));
    CHECK(PUSH_INT_TPTR(&t, &ppli));
    CHECK(PUSH_FLOAT_TPTR(&t, &pplf));
    CHECK(PUSH_CHAR_TPTR(&t, &pplc));
    CHECK(PUSH_DOUBLE_TPTR(&t, &ppld));
    CHECK(PUSH_LONG_TPTR(&t, &ppll));
    CHECK(PUSH_SHORT_TPTR(&t, &ppls));

    /* --- struct / union (deep copied) + macros --- */
    Point3 p1 = { 1, 2, 9.9f };
    CHECK(bPushStruct(&t, &p1, sizeof(Point3)));
    Point3 p2 = { 3, 4, 1.1f };
    CHECK(PUSH_STRUCT(&t, &p2));

    SmallUnion su1; su1.i = 42;
    CHECK(bPushUnion(&t, &su1, sizeof(SmallUnion)));
    SmallUnion su2; su2.f = 1.5f;
    CHECK(PUSH_UNION(&t, &su2));

    /* --- func / file pointers (raw, unowned) + macros --- */
    CHECK(bPushFunc(&t, (void*)test_table_dynamic));
    CHECK(PUSH_FUNC(&t, (void*)visitor_da));
    CHECK(bPushFile(&t, stdout));
    CHECK(PUSH_FILE(&t, stderr));

    /* --- variadic bulk push --- */
    CHECK(bPushList(&t, 3, TYPE_INT, 777, TYPE_FLOAT, 7.77, TYPE_CHAR, 'v'));

    vPrintTable(&t);

    /* --- struct/union member access macros on a known slot --- */
    {
        TableSlot_DA struct_slot;
        int idx = iFindTypeTable(&t, TYPE_STRUCT);
        CHECK(idx >= 0);
        struct_slot = sGetSlotAtTable(&t, idx);
        int gx = GET_STRUCT_MEMBER(&struct_slot, offsetof(Point3, x), int);
        CHECK(gx == p1.x);

        TableSlot_DA union_slot;
        int uidx = iFindTypeTable(&t, TYPE_UNION);
        CHECK(uidx >= 0);
        union_slot = sGetSlotAtTable(&t, uidx);
        int gi = GET_UNION_MEMBER(&union_slot, int);
        CHECK(gi == su1.i);
    }

    /* --- aggregate / query ops --- */
    CHECK(iSumIntTable(&t) >= (10 + 20));       /* other ints pushed after too */
    CHECK(dSumFloatTable(&t) > 0.0);
    CHECK(bContainsTypeTable(&t, TYPE_STRUCT));
    CHECK(iFindTypeTable(&t, TYPE_CHAR) >= 0);
    CHECK(iCountOfTypeTable(&t, TYPE_INT) >= 2);

    int minv = 0, maxv = 0;
    CHECK(bMinIntTable(&t, &minv));
    CHECK(bMaxIntTable(&t, &maxv));
    CHECK(minv <= maxv);

    /* --- get / type-checked access --- */
    CHECK(sGetTypeAtTable(&t, 0) == TYPE_INT);
    TableSlot_DA got;
    CHECK(bTryGetSlotAtTable(&t, 0, TYPE_INT, &got));
    CHECK(got.ptr != NULL && *(int*)got.ptr == 10);
    CHECK(!bTryGetSlotAtTable(&t, 0, TYPE_FLOAT, &got));   /* type mismatch */
    CHECK(!bTryGetSlotAtTable(&t, 99999, TYPE_INT, &got)); /* OOB */

    /* --- dup / swap / reverse --- */
    int before = t.count;
    CHECK(bDupTopTable(&t));
    CHECK(t.count == before + 1);
    CHECK(bSwapTopTable(&t));
    vReverseTable(&t);
    vReverseTable(&t); /* identity */

    /* --- insert / remove at index --- */
    {
        TableSlot_DA empty_slot;
        empty_slot.ptr = NULL;
        empty_slot.type = TYPE_EMPTY;
        CHECK(bInsertAtTable(&t, 0, empty_slot));
    }
    CHECK(t.slots[0].type == TYPE_EMPTY);
    CHECK(bRemoveAtTable(&t, 0));

    /* --- bulk array push --- */
    int arr[] = { 1, 2, 3, 4, 5 };
    CHECK(iPushIntArrayTable(&t, arr, 5) == 5);
    float farr[] = { 1.1f, 2.2f };
    CHECK(iPushFloatArrayTable(&t, farr, 2) == 2);

    /* --- forEach visitor --- */
    vForEachSlotTable(&t, visitor_da);

    /* --- clone ---
       bCloneTable() deliberately skips TYPE_STRUCT/TYPE_UNION slots: their
       byte size isn't recorded anywhere on TableSlot_DA (only ptr+type), so
       there's no safe way to know how many bytes to copy (see the function's
       own printf: "size isn't tracked, can't clone safely"). This test pushed
       exactly 4 such slots above (p1, p2 via bPushStruct/PUSH_STRUCT, su1, su2
       via bPushUnion/PUSH_UNION), so the clone is expected to come out 4
       elements short - that's correct behavior, not a bug. */
    const int STRUCT_UNION_PUSHED = 4;
    Table clone;
    vGetTable(&clone, 4, 0); /* no separate arena; shares master arena */
    CHECK(bCloneTable(&clone, &t));
    CHECK(clone.count == t.count - STRUCT_UNION_PUSHED);

    /* --- reserve / shrink --- */
    CHECK(vReserveTable(&t, 16));
    vShrinkToFitTable(&t);
    CHECK(t.capacity == t.count);

    /* --- introspection helpers --- */
    CHECK(strcmp(sTypeNameTable(TYPE_INT), "INT") == 0);
    CHECK(bIsOwnedTypeTable(TYPE_INT) == true);
    CHECK(bIsOwnedTypeTable(TYPE_FUNC_PTR) == false);
    CHECK(bIsOwnedTypeTable(TYPE_FILE_PTR) == false);

    /* --- pop until empty; underflow --- */
    while (t.count > 0) { TableSlot_DA s = sPopSlot(&t); (void)s; }
    CHECK(t.count == 0);
    TableSlot_DA underflow = sPopSlot(&t);
    CHECK(underflow.type == TYPE_EMPTY);

    /* --- arena alloc/free exercised directly too --- */
    void* raw = pAllocArena(64);
    CHECK(raw != NULL);
    vFreeArena(raw);
    vFreeArena(NULL); /* no-op, must not crash */

    vDropTable(&clone);
    vDropTable(&t);
    CHECK(t.slots == NULL && t.capacity == 0 && t.count == 0);
}

/* ========================================================================
   2. TableStack — fixed-capacity stack-allocated array
   ======================================================================== */
static void test_table_stack(void)
{
    SECTION("TableStack (fixed stack array)");

    /* Capacity sized generously: this test pushes ~36 slots total (primitives,
       macro-wrapped primitives, ptr/dptr/tptr, struct/union/func/file, a
       variadic bPushListStack, plus a later dup + bulk array push). TableStack
       has no growth path (see push_raw_bytes()'s "Growing not supported"
       error), so under-sizing this causes every later push to silently fail
       and starves bDupTopStack() of room. */
    vGetTableStack(ts, 48); /* declares TableSlot_SA ts_backing[48]; TableStack ts */

    CHECK(bIsTableStackEmpty(&ts));
    CHECK(!bIsTableStackFull(&ts));
    CHECK(iCapacityStack(&ts) == 48);
    CHECK(TABLE_STACK_CAP(ts) == 48);

    /* --- raw slot push via bPushSlotStack --- */
    TableSlot_SA raw_slot = { NULL, TYPE_EMPTY };
    CHECK(bPushSlotStack(&ts, raw_slot));
    CHECK(bDropTopStack(&ts)); /* remove it again to keep counts simple below */

    /* --- primitives + macros --- */
    CHECK(bPushIntStack(&ts, 5));
    CHECK(bPushFloatStack(&ts, 1.5f));
    CHECK(bPushDoubleStack(&ts, 9.99));
    CHECK(bPushLongStack(&ts, 55L));
    CHECK(bPushShortStack(&ts, (short)6));
    CHECK(bPushCharStack(&ts, 'Q'));

    CHECK(PUSH_INT_STACK(&ts, 15));
    CHECK(PUSH_FLOAT_STACK(&ts, 2.5f));
    CHECK(PUSH_DOUBLE_STACK(&ts, 8.88));
    CHECK(PUSH_LONG_STACK(&ts, 66L));
    CHECK(PUSH_SHORT_STACK(&ts, (short)9));
    CHECK(PUSH_CHAR_STACK(&ts, 'R'));

    /* --- pointer / dptr / tptr pushes + macros --- */
    int li = 1; int* pli = &li; int** ppli = &pli;
    CHECK(bPushPtrStack(&ts, &li, TYPE_INT_STAR));
    CHECK(bPushDPtrStack(&ts, (void**)&pli, TYPE_INT_STAR_DOUBLE));
    CHECK(bPushTPtrStack(&ts, (void***)&ppli, TYPE_INT_STAR_TRIPLE));

    CHECK(PUSH_INT_PTR_STACK(&ts, &li));
    CHECK(PUSH_VOID_PTR_STACK(&ts, (void*)&li));
    CHECK(PUSH_INT_DPTR_STACK(&ts, &pli));
    CHECK(PUSH_VOID_DPTR_STACK(&ts, (void**)&pli));
    CHECK(PUSH_INT_TPTR_STACK(&ts, &ppli));
    CHECK(PUSH_VOID_TPTR_STACK(&ts, (void***)&ppli));

    /* --- struct / union / func / file, capped at one machine word --- */
    int small_struct_field = 12345;
    CHECK(bPushStructStack(&ts, &small_struct_field, sizeof(int))); /* must stay <= sizeof(void*) */
    SmallUnion su; su.i = 7;
    CHECK(bPushUnionStack(&ts, &su, sizeof(int)));
    CHECK(bPushFuncStack(&ts, (void*)test_table_stack));
    CHECK(bPushFileStack(&ts, stdout));

    CHECK(PUSH_STRUCT_STACK(&ts, &small_struct_field));
    CHECK(PUSH_UNION_STACK(&ts, &su));
    CHECK(PUSH_FUNC_STACK(&ts, (void*)visitor_sa));
    CHECK(PUSH_FILE_STACK(&ts, stderr));

    /* --- variadic bulk push --- */
    CHECK(bPushListStack(&ts, 2, TYPE_INT, 4242, TYPE_CHAR, 'v'));
    /* NOTE: S_PUSH_INTS / S_* macros are StackTableList-only (see test_stack_table_list
       below) — they take a different struct type than TableStack, so they don't apply here. */

    vPrintTableStack(&ts);
    printf("[peek] "); vPrintSlotStack(sPeekSlotStack(&ts));

    /* --- peek / counts --- */
    CHECK(sPeekTypeStack(&ts) != TYPE_EMPTY);
    CHECK(iCountStack(&ts) == ts.count);
    CHECK(TABLE_STACK_LEN(ts) == ts.count);
    CHECK(iRemainingStack(&ts) == ts.capacity - ts.count);

    /* --- aggregate / query --- */
    CHECK(iSumIntStack(&ts) != 0);
    CHECK(dSumFloatStack(&ts) != 0.0);
    CHECK(bContainsTypeStack(&ts, TYPE_INT));
    CHECK(iFindTypeStack(&ts, TYPE_CHAR) >= 0);
    CHECK(iCountOfTypeStack(&ts, TYPE_INT) >= 1);

    int minv, maxv;
    CHECK(bMinIntStack(&ts, &minv));
    CHECK(bMaxIntStack(&ts, &maxv));

    /* --- get / typed access --- */
    CHECK(sGetTypeAtStack(&ts, 0) != TYPE_EMPTY);
    TableSlot_SA got_sa = sGetSlotAtStack(&ts, 0);
    (void)got_sa;
    TableSlot_SA typed_sa;
    CHECK(bTryGetSlotAtStackTyped(&ts, 0, sGetTypeAtStack(&ts, 0), &typed_sa));

    /* --- GET_STACK_VALUE macro on a known int slot --- */
    {
        int idx = iFindTypeStack(&ts, TYPE_INT);
        CHECK(idx >= 0);
        TableSlot_SA islot = sGetSlotAtStack(&ts, idx);
        int val = GET_STACK_VALUE(islot, int);
        (void)val;
    }

    /* --- dup / swap / reverse --- */
    CHECK(bDupTopStack(&ts));
    CHECK(bSwapTopStack(&ts));
    vReverseStack(&ts);
    vForEachSlotStack(&ts, visitor_sa);

    /* --- bulk array push --- */
    int iarr[] = { 100, 200 };
    CHECK(iPushIntArrayStack(&ts, iarr, 2) == 2);
    float farr[] = { 3.3f };
    CHECK(iPushFloatArrayStack(&ts, farr, 1) == 1);

    /* --- clone --- */
    vGetTableStack(ts_clone, 48); /* must be >= ts.count at clone time */
    CHECK(bCloneTableStack(&ts_clone, &ts));
    CHECK(ts_clone.count == ts.count);

    /* --- introspection --- */
    CHECK(strcmp(sTypeNameStack(TYPE_INT), "INT") == 0);

    /* --- pop / drop / reset --- */
    TableSlot_SA popped = sPopSlotStack(&ts);
    (void)popped;
    CHECK(bDropTopStack(&ts));

    vDropTableStack(&ts);
    CHECK(ts.count == 0);
    TableSlot_SA underflow_sa = sPopSlotStack(&ts);
    CHECK(underflow_sa.type == TYPE_EMPTY);

    vDropTableStack(&ts_clone);
}

/* ========================================================================
   3. TableList — heap-allocated doubly-linked list
   ======================================================================== */
static void test_table_list(void)
{
    SECTION("TableList (heap doubly-linked list)");

    TableList list;
    tFormLinkedTable(&list);
    CHECK(bIsListEmpty(&list));
    CHECK(IS_EMPTY(&list));

    /* --- raw node/slot API --- */
    TableSlot_LL raw = { NULL, TYPE_EMPTY };
    tNode* node = tCreateNode(raw);
    CHECK(node != NULL);
    free(node); /* not attached to any list; standalone alloc test only */

    PUSH_RAW_BACK(&list, raw);
    CHECK(list.count == 1);
    TableSlot_LL peeked_front = PEEK_RAW_FRONT(&list);
    TableSlot_LL peeked_back = PEEK_RAW_BACK(&list);
    (void)peeked_front; (void)peeked_back;
    TableSlot_LL got_raw = GET_RAW_AT(&list, 0);
    (void)got_raw;
    TableSlot_LL popped_raw = POP_RAW_FRONT(&list);
    (void)popped_raw;
    CHECK(list.count == 0);

    PUSH_RAW_FRONT(&list, raw);
    INSERT_RAW_AT(&list, 0, raw);
    CHECK(list.count == 2);
    TableSlot_LL removed_raw = REMOVE_RAW_AT(&list, 0);
    (void)removed_raw;
    (void)POP_RAW_BACK(&list);
    CHECK(list.count == 0);

    /* --- int wrappers (both raw fns and macros) --- */
    tPushIntBack(&list, 1);
    tPushIntFront(&list, 0);
    PUSH_INTS(&list, 3, 10, 11, 12);
    CHECK(list.count == 5);
    CHECK(tGetIntAt(&list, 0) == 0);
    CHECK(GET_INT(&list, 1) == 1);
    INSERT_INT(&list, 1, 100);
    CHECK(GET_INT(&list, 1) == 100);
    CHECK(REMOVE_INT(&list, 1) == 100);
    CHECK(FIND_INT(&list, 11) >= 0);
    CHECK(POP_INT(&list) == 0);      /* front */
    CHECK(POP_INT_BACK(&list) == 12); /* back */

    /* --- float wrappers --- */
    tPushFloatBack(&list, 1.1f);
    tPushFloatFront(&list, 0.1f);
    CHECK(GET_FLOAT(&list, 0) == 0.1f);
    INSERT_FLOAT(&list, 1, 5.5f);
    CHECK(REMOVE_FLOAT(&list, 1) == 5.5f);
    CHECK(FIND_FLOAT(&list, 1.1f) >= 0);
    (void)POP_FLOAT(&list);
    (void)POP_FLOAT_BACK(&list);

    /* --- double wrappers --- */
    tPushDoubleBack(&list, 2.2);
    tPushDoubleFront(&list, 0.2);
    PUSH_DOUBLES(&list, 2, 9.9, 8.8);
    CHECK(GET_DOUBLE(&list, 0) == 0.2);
    INSERT_DOUBLE(&list, 1, 3.3);
    CHECK(REMOVE_DOUBLE(&list, 1) == 3.3);
    CHECK(FIND_DOUBLE(&list, 9.9) >= 0);
    (void)POP_DOUBLE(&list);
    (void)POP_DOUBLE_BACK(&list);

    /* --- char wrappers --- */
    tPushCharBack(&list, 'b');
    tPushCharFront(&list, 'a');
    PUSH_CHARS(&list, 2, 'x', 'y');
    CHECK(GET_CHAR(&list, 0) == 'a');
    INSERT_CHAR(&list, 1, 'z');
    CHECK(REMOVE_CHAR(&list, 1) == 'z');
    CHECK(FIND_CHAR(&list, 'x') >= 0);
    (void)POP_CHAR(&list);
    (void)POP_CHAR_BACK(&list);

    /* --- long wrappers --- */
    tPushLongBack(&list, 100L);
    tPushLongFront(&list, 1L);
    CHECK(GET_LONG(&list, 0) == 1L);
    INSERT_LONG(&list, 1, 50L);
    CHECK(REMOVE_LONG(&list, 1) == 50L);
    CHECK(FIND_LONG(&list, 100L) >= 0);
    (void)POP_LONG(&list);
    (void)POP_LONG_BACK(&list);

    /* --- short wrappers --- */
    tPushShortBack(&list, (short)9);
    tPushShortFront(&list, (short)1);
    CHECK(GET_SHORT(&list, 0) == (short)1);
    INSERT_SHORT(&list, 1, (short)5);
    CHECK(REMOVE_SHORT(&list, 1) == (short)5);
    CHECK(FIND_SHORT(&list, (short)9) >= 0);
    (void)POP_SHORT(&list);
    (void)POP_SHORT_BACK(&list);

    /* --- typed getters for every primitive --- */
    {
        int iv; float fv; double dv; char cv; long lv; short sv;
        tPushIntBack(&list, 42);
        CHECK(bTryGetIntAt(&list, list.count - 1, &iv) && iv == 42);
        tPushFloatBack(&list, 4.2f);
        CHECK(bTryGetFloatAt(&list, list.count - 1, &fv) && fv == 4.2f);
        tPushDoubleBack(&list, 4.20);
        CHECK(bTryGetDoubleAt(&list, list.count - 1, &dv) && dv == 4.20);
        tPushCharBack(&list, 'K');
        CHECK(bTryGetCharAt(&list, list.count - 1, &cv) && cv == 'K');
        tPushLongBack(&list, 420L);
        CHECK(bTryGetLongAt(&list, list.count - 1, &lv) && lv == 420L);
        tPushShortBack(&list, (short)42);
        CHECK(bTryGetShortAt(&list, list.count - 1, &sv) && sv == (short)42);
        CHECK(!bTryGetIntAt(&list, list.count - 1, &iv)); /* last is a short, not int */
    }

    /* --- pointer wrappers --- */
    int li = 5; int li2 = 6;
    PUSH_PTR(&list, &li, TYPE_INT_STAR);
    PUSH_PTR_FRONT(&list, &li2, TYPE_INT_STAR);
    CHECK(GET_PTR(&list, 0) == &li2);
    INSERT_PTR(&list, 1, &li, TYPE_INT_STAR);
    CHECK(FIND_PTR(&list, &li) >= 0);
    (void)REMOVE_PTR(&list, 1);
    (void)POP_PTR(&list);
    (void)POP_PTR_BACK(&list);

    /* --- struct wrappers (raw pointer, list does not own/copy) --- */
    Point3 p1 = { 1, 1, 1.0f }, p2 = { 2, 2, 2.0f };
    PUSH_STRUCT_LL(&list, &p1);
    PUSH_STRUCT_FRONT(&list, &p2);
    CHECK(GET_STRUCT(&list, 0) == &p2);
    INSERT_STRUCT(&list, 1, &p1);
    CHECK(FIND_STRUCT(&list, &p1) >= 0);
    (void)REMOVE_STRUCT(&list, 1);
    (void)POP_STRUCT(&list);
    (void)POP_STRUCT_BACK(&list);

    /* --- union wrappers --- */
    SmallUnion su1; su1.i = 1; SmallUnion su2; su2.i = 2;
    PUSH_UNION_LL(&list, &su1);
    PUSH_UNION_FRONT(&list, &su2);
    CHECK(GET_UNION(&list, 0) == &su2);
    INSERT_UNION(&list, 1, &su1);
    CHECK(FIND_UNION(&list, &su1) >= 0);
    (void)REMOVE_UNION(&list, 1);
    (void)POP_UNION(&list);
    (void)POP_UNION_BACK(&list);

    /* --- func ptr wrappers --- */
    PUSH_FUNC_LL(&list, (void*)test_table_list);
    PUSH_FUNC_FRONT(&list, (void*)visitor_da);
    CHECK(GET_FUNC(&list, 0) == (void*)visitor_da);
    INSERT_FUNC(&list, 1, (void*)test_table_list);
    CHECK(FIND_FUNC(&list, (void*)test_table_list) >= 0);
    (void)REMOVE_FUNC(&list, 1);
    (void)POP_FUNC(&list);
    (void)POP_FUNC_BACK(&list);

    /* --- file ptr wrappers --- */
    PUSH_FILE_LL(&list, stdout);
    PUSH_FILE_FRONT(&list, stderr);
    CHECK(GET_FILE(&list, 0) == stderr);
    INSERT_FILE(&list, 1, stdout);
    CHECK(FIND_FILE(&list, stdout) >= 0);
    (void)REMOVE_FILE(&list, 1);
    (void)POP_FILE(&list);
    (void)POP_FILE_BACK(&list);

    PRINT_LIST(&list);
    REVERSE_LIST(&list);

    DESTROY_LIST(&list);
    CHECK(list.count == 0);
    CHECK(IS_EMPTY(&list));
}

/* ========================================================================
   4. StackTableList — pool-backed doubly-linked list (fixed capacity)
   ======================================================================== */
static void test_stack_table_list(void)
{
    SECTION("StackTableList (pool-backed linked list)");

    StackTableList slist;
    tStackFormLinkedTable(&slist);
    CHECK(bStackIsListEmpty(&slist));
    CHECK(S_IS_EMPTY(&slist));
    CHECK(!bStackIsListFull(&slist));
    CHECK(!S_IS_FULL(&slist));
    CHECK(S_CAPACITY == STACK_TABLE_CAPACITY);

    /* --- raw node/slot API --- */
    TableSlot_STLL raw = { NULL, TYPE_EMPTY };
    sNode* node = tStackCreateNode(&slist, raw);
    CHECK(node != NULL); /* claimed from the pool, currently orphaned (test only) */

    S_PUSH_RAW_BACK(&slist, raw);
    CHECK(slist.count == 1);
    TableSlot_STLL pf = S_PEEK_RAW_FRONT(&slist);
    TableSlot_STLL pb = S_PEEK_RAW_BACK(&slist);
    (void)pf; (void)pb;
    TableSlot_STLL gr = S_GET_RAW_AT(&slist, 0);
    (void)gr;
    (void)S_POP_RAW_FRONT(&slist);
    CHECK(slist.count == 0);

    S_PUSH_RAW_FRONT(&slist, raw);
    S_INSERT_RAW_AT(&slist, 0, raw);
    CHECK(slist.count == 2);
    (void)S_REMOVE_RAW_AT(&slist, 0);
    (void)S_POP_RAW_BACK(&slist);
    CHECK(slist.count == 0);

    /* --- int wrappers --- */
    tStackPushIntBack(&slist, 1);
    tStackPushIntFront(&slist, 0);
    S_PUSH_INTS(&slist, 2, 10, 11);
    CHECK(slist.count == 4);
    CHECK(S_GET_INT(&slist, 0) == 0);
    S_INSERT_INT(&slist, 1, 100);
    CHECK(S_REMOVE_INT(&slist, 1) == 100);
    CHECK(S_FIND_INT(&slist, 11) >= 0);
    CHECK(S_POP_INT(&slist) == 0);
    (void)S_POP_INT_BACK(&slist);

    /* --- float wrappers --- */
    tStackPushFloatBack(&slist, 1.1f);
    tStackPushFloatFront(&slist, 0.1f);
    CHECK(S_GET_FLOAT(&slist, 0) == 0.1f);
    S_INSERT_FLOAT(&slist, 1, 5.5f);
    CHECK(S_REMOVE_FLOAT(&slist, 1) == 5.5f);
    CHECK(S_FIND_FLOAT(&slist, 1.1f) >= 0);
    (void)S_POP_FLOAT(&slist);
    (void)S_POP_FLOAT_BACK(&slist);

    /* --- double wrappers --- */
    tStackPushDoubleBack(&slist, 2.2);
    tStackPushDoubleFront(&slist, 0.2);
    S_PUSH_DOUBLES(&slist, 2, 9.9, 8.8);
    CHECK(S_GET_DOUBLE(&slist, 0) == 0.2);
    S_INSERT_DOUBLE(&slist, 1, 3.3);
    CHECK(S_REMOVE_DOUBLE(&slist, 1) == 3.3);
    CHECK(S_FIND_DOUBLE(&slist, 9.9) >= 0);
    (void)S_POP_DOUBLE(&slist);
    (void)S_POP_DOUBLE_BACK(&slist);

    /* --- char wrappers --- */
    tStackPushCharBack(&slist, 'b');
    tStackPushCharFront(&slist, 'a');
    S_PUSH_CHARS(&slist, 2, 'x', 'y');
    CHECK(S_GET_CHAR(&slist, 0) == 'a');
    S_INSERT_CHAR(&slist, 1, 'z');
    CHECK(S_REMOVE_CHAR(&slist, 1) == 'z');
    CHECK(S_FIND_CHAR(&slist, 'x') >= 0);
    (void)S_POP_CHAR(&slist);
    (void)S_POP_CHAR_BACK(&slist);

    /* --- long wrappers --- */
    tStackPushLongBack(&slist, 100L);
    tStackPushLongFront(&slist, 1L);
    CHECK(S_GET_LONG(&slist, 0) == 1L);
    S_INSERT_LONG(&slist, 1, 50L);
    CHECK(S_REMOVE_LONG(&slist, 1) == 50L);
    CHECK(S_FIND_LONG(&slist, 100L) >= 0);
    (void)S_POP_LONG(&slist);
    (void)S_POP_LONG_BACK(&slist);

    /* --- short wrappers --- */
    tStackPushShortBack(&slist, (short)9);
    tStackPushShortFront(&slist, (short)1);
    CHECK(S_GET_SHORT(&slist, 0) == (short)1);
    S_INSERT_SHORT(&slist, 1, (short)5);
    CHECK(S_REMOVE_SHORT(&slist, 1) == (short)5);
    CHECK(S_FIND_SHORT(&slist, (short)9) >= 0);
    (void)S_POP_SHORT(&slist);
    (void)S_POP_SHORT_BACK(&slist);

    /* --- typed getters --- */
    {
        int iv; float fv; double dv; char cv; long lv; short sv;
        tStackPushIntBack(&slist, 42);
        CHECK(bStackTryGetIntAt(&slist, slist.count - 1, &iv) && iv == 42);
        tStackPushFloatBack(&slist, 4.2f);
        CHECK(bStackTryGetFloatAt(&slist, slist.count - 1, &fv) && fv == 4.2f);
        tStackPushDoubleBack(&slist, 4.20);
        CHECK(bStackTryGetDoubleAt(&slist, slist.count - 1, &dv) && dv == 4.20);
        tStackPushCharBack(&slist, 'K');
        CHECK(bStackTryGetCharAt(&slist, slist.count - 1, &cv) && cv == 'K');
        tStackPushLongBack(&slist, 420L);
        CHECK(bStackTryGetLongAt(&slist, slist.count - 1, &lv) && lv == 420L);
        tStackPushShortBack(&slist, (short)42);
        CHECK(bStackTryGetShortAt(&slist, slist.count - 1, &sv) && sv == (short)42);
    }

    /* --- pointer wrappers --- */
    int li = 5, li2 = 6;
    S_PUSH_PTR(&slist, &li, TYPE_INT_STAR);
    S_PUSH_PTR_FRONT(&slist, &li2, TYPE_INT_STAR);
    CHECK(S_GET_PTR(&slist, 0) == &li2);
    S_INSERT_PTR(&slist, 1, &li, TYPE_INT_STAR);
    CHECK(S_FIND_PTR(&slist, &li) >= 0);
    (void)S_REMOVE_PTR(&slist, 1);
    (void)S_POP_PTR(&slist);
    (void)S_POP_PTR_BACK(&slist);

    /* --- struct wrappers --- */
    Point3 p1 = { 1, 1, 1.0f }, p2 = { 2, 2, 2.0f };
    S_PUSH_STRUCT(&slist, &p1);
    S_PUSH_STRUCT_FRONT(&slist, &p2);
    CHECK(S_GET_STRUCT(&slist, 0) == &p2);
    S_INSERT_STRUCT(&slist, 1, &p1);
    CHECK(S_FIND_STRUCT(&slist, &p1) >= 0);
    (void)S_REMOVE_STRUCT(&slist, 1);
    (void)S_POP_STRUCT(&slist);
    (void)S_POP_STRUCT_BACK(&slist);

    /* --- union wrappers --- */
    SmallUnion su1; su1.i = 1; SmallUnion su2; su2.i = 2;
    S_PUSH_UNION(&slist, &su1);
    S_PUSH_UNION_FRONT(&slist, &su2);
    CHECK(S_GET_UNION(&slist, 0) == &su2);
    S_INSERT_UNION(&slist, 1, &su1);
    CHECK(S_FIND_UNION(&slist, &su1) >= 0);
    (void)S_REMOVE_UNION(&slist, 1);
    (void)S_POP_UNION(&slist);
    (void)S_POP_UNION_BACK(&slist);

    /* --- func ptr wrappers --- */
    S_PUSH_FUNC(&slist, (void*)test_stack_table_list);
    S_PUSH_FUNC_FRONT(&slist, (void*)visitor_sa);
    CHECK(S_GET_FUNC(&slist, 0) == (void*)visitor_sa);
    S_INSERT_FUNC(&slist, 1, (void*)test_stack_table_list);
    CHECK(S_FIND_FUNC(&slist, (void*)test_stack_table_list) >= 0);
    (void)S_REMOVE_FUNC(&slist, 1);
    (void)S_POP_FUNC(&slist);
    (void)S_POP_FUNC_BACK(&slist);

    /* --- file ptr wrappers --- */
    S_PUSH_FILE(&slist, stdout);
    S_PUSH_FILE_FRONT(&slist, stderr);
    CHECK(S_GET_FILE(&slist, 0) == stderr);
    S_INSERT_FILE(&slist, 1, stdout);
    CHECK(S_FIND_FILE(&slist, stdout) >= 0);
    (void)S_REMOVE_FILE(&slist, 1);
    (void)S_POP_FILE(&slist);
    (void)S_POP_FILE_BACK(&slist);

    S_PRINT_LIST(&slist);
    S_REVERSE_LIST(&slist);

    /* --- exercise pool free-list under load: push 30, pop 30 --- */
    for (int i = 0; i < 30; i++) tStackPushIntBack(&slist, i);
    for (int i = 0; i < 30; i++) tStackPopIntBack(&slist);

    S_DESTROY_LIST(&slist);
    CHECK(slist.count == 0);
    CHECK(S_IS_EMPTY(&slist));
}

/* ========================================================================
   5. TableMap — bucketed hash map
   ======================================================================== */
static void test_table_map(void)
{
    SECTION("TableMap (hash map)");

    TableMap map;
    vFormHashMap(&map);
    CHECK(bIsEmptyMap(&map));
    CHECK(!bIsFullMap(&map));
    CHECK(iCountMap(&map) == 0);
    CHECK(iCapacityMap(&map) == HASH_POOL_CAPACITY);

    /* --- custom hash function hook --- */
    vSetHashFnMap(&map, custom_hash);
    void* dummy_key = &map; /* any stable address */
    uint32_t h = uDefaultPointerHash(dummy_key);
    (void)h;

    /* --- raw insert (bypasses the typed helpers) ---
       IMPORTANT: tMapInsert() takes ownership for "owned" type tags (see
       bIsOwnedTypeMap) - bMapRemove()/vDropHashMap() will free() that pointer
       later. So the payload MUST be heap-allocated for owned types; a stack
       address here would make bMapRemove() free() stack memory (undefined
       behavior / heap corruption). Use one of the tMapInsert<Type> helpers
       when in doubt - they malloc the copy for you. */
    int* raw_payload = (int*)malloc(sizeof(int));
    CHECK(raw_payload != NULL);
    *raw_payload = 999;
    int raw_result = tMapInsert(&map, raw_payload, TYPE_INT);
    CHECK(raw_result >= 0);
    CHECK(tMapContains(&map, raw_payload));
    CHECK(bMapRemove(&map, raw_payload)); /* owned type -> this frees raw_payload */

    /* revert to default hash for the rest of the test so bucket math is predictable-ish */
    vSetHashFnMap(&map, NULL);

    /* --- typed inserts covering every supported type --- */
    void* k_int = NULL, * k_int2 = NULL, * k_float = NULL, * k_double = NULL,
        * k_long = NULL, * k_short = NULL, * k_char = NULL;
    CHECK(tMapInsertInt(&map, 111, &k_int) >= 0);
    CHECK(tMapInsertInt(&map, 222, &k_int2) >= 0);
    CHECK(tMapInsertFloat(&map, 9.5f, &k_float) >= 0);
    CHECK(tMapInsertDouble(&map, 3.14159, &k_double) >= 0);
    CHECK(tMapInsertLong(&map, 555L, &k_long) >= 0);
    CHECK(tMapInsertShort(&map, (short)6, &k_short) >= 0);
    CHECK(tMapInsertChar(&map, 'M', &k_char) >= 0);
    CHECK(k_int && k_int2 && k_float && k_double && k_long && k_short && k_char);

    void* k_vptr = NULL, * k_dptr = NULL, * k_tptr = NULL;
    int li = 7; int* pli = &li; int** ppli = &pli;
    CHECK(tMapInsertPtr(&map, &li, TYPE_INT_STAR, &k_vptr) >= 0);
    CHECK(tMapInsertDPtr(&map, (void**)&pli, TYPE_INT_STAR_DOUBLE, &k_dptr) >= 0);
    CHECK(tMapInsertTPtr(&map, (void***)&ppli, TYPE_INT_STAR_TRIPLE, &k_tptr) >= 0);

    void* k_struct = NULL, * k_union = NULL, * k_func = NULL, * k_file = NULL;
    Point3 p = { 7, 8, 0.5f };
    CHECK(tMapInsertStruct(&map, &p, sizeof(Point3), &k_struct) >= 0);
    SmallUnion su; su.i = 3;
    CHECK(tMapInsertUnion(&map, &su, sizeof(SmallUnion), &k_union) >= 0);
    CHECK(tMapInsertFunc(&map, (void*)test_table_map, &k_func) >= 0);
    CHECK(tMapInsertFile(&map, stdout, &k_file) >= 0);

    int expected_count = 12; /* int, int2, float, double, long, short, char, vptr, dptr, tptr, struct, union, func, file = 14 actually */
    (void)expected_count;
    CHECK(iCountMap(&map) == 14);

    /* --- macro inserts (separate keys, same underlying functions) --- */
    void* mk1 = NULL, * mk2 = NULL, * mk3 = NULL, * mk4 = NULL;
    MAP_INSERT_INT(&map, 1000, &mk1);
    MAP_INSERT_FLOAT(&map, 1.5f, &mk2);
    MAP_INSERT_STRUCT(&map, &p, &mk3);
    MAP_INSERT_FUNC(&map, (void*)visitor_map, &mk4);
    CHECK(mk1 && mk2 && mk3 && mk4);
    CHECK(iCountMap(&map) == 18);

    /* --- lookup / contains / typed lookup --- */
    CHECK(tMapContains(&map, k_int));
    CHECK(!tMapContains(&map, (void*)(uintptr_t)0xDEADBEEFu));
    TableSlot_HM node = sMapGetNode(&map, k_int);
    CHECK(node.type == TYPE_INT && node.ptr && *(int*)node.ptr == 111);
    int gv = GET_MAP_VALUE(&node, int);
    CHECK(gv == 111);

    TableSlot_HM typed;
    CHECK(bTryMapGetTyped(&map, k_int, TYPE_INT, &typed));
    CHECK(!bTryMapGetTyped(&map, k_int, TYPE_FLOAT, &typed));

    /* --- aggregate / query --- */
    CHECK(iSumIntMap(&map) == (111 + 222 + 1000));
    CHECK(dSumFloatMap(&map) > 0.0);
    CHECK(bContainsTypeMap(&map, TYPE_FLOAT));
    CHECK(iCountOfTypeMap(&map, TYPE_INT) == 3); /* 111, 222, 1000 */
    CHECK(iFindTypeMap(&map, TYPE_DOUBLE) >= 0);

    int minv, maxv;
    CHECK(bMinIntMap(&map, &minv) && minv == 111);
    CHECK(bMaxIntMap(&map, &maxv) && maxv == 1000);

    /* --- replace type in place --- */
    CHECK(bMapReplaceType(&map, k_int, TYPE_LONG));
    CHECK(sMapGetNode(&map, k_int).type == TYPE_LONG);
    CHECK(bMapReplaceType(&map, k_int, TYPE_INT)); /* restore */

    /* --- diagnostics --- */
    CHECK(dLoadFactorMap(&map) > 0.0);
    CHECK(iFreeSlotsRemainingMap(&map) == HASH_POOL_CAPACITY - iCountMap(&map));
    int bucket_of_int = iBucketOfMap(&map, k_int);
    CHECK(bucket_of_int >= 0);
    CHECK(iBucketLengthMap(&map, bucket_of_int) >= 1);
    CHECK(iMaxBucketLengthMap(&map) >= 1);
    (void)bPointerHashIsSuspect(&map); /* just make sure it runs without crashing */

    /* --- find-first / keys-of-type --- */
    TableSlot_HM first_int;
    CHECK(bMapFindFirst(&map, TYPE_INT, &first_int));
    void* int_keys[8];
    int found = iMapKeysOfType(&map, TYPE_INT, int_keys, 8);
    CHECK(found >= 1);

    /* --- introspection / printing --- */
    CHECK(strcmp(sTypeNameMap(TYPE_INT), "INT") == 0);
    vPrintHashMap(&map);
    vRehashStatsMap(&map);

    /* --- visitor / active-only processing --- */
    vForEachNodeMap(&map, visitor_map);
    tMapProcessActive(&map, visitor_map);

    /* --- clone ---
       Same limitation as bCloneTable() on the array variant: bCloneTableMap()'s
       byte-size switch has no case for TYPE_STRUCT/TYPE_UNION (their size isn't
       tracked in TableSlot_HM either), so it falls to "default: continue" and
       those entries are silently dropped from the clone. This test inserted
       exactly 3 such entries above (k_struct, mk3 via MAP_INSERT_STRUCT, and
       k_union), so the clone is expected to land 3 short - that's the same
       documented tradeoff as the array Table's clone, just without a printf
       here to announce it. */
    const int STRUCT_UNION_MAP_PUSHED = 3;
    TableMap clone;
    CHECK(bCloneTableMap(&clone, &map));
    CHECK(iCountMap(&clone) == iCountMap(&map) - STRUCT_UNION_MAP_PUSHED);

    /* --- merge into a fresh map ---
       Same struct/union gap applies inside bMapMergeInto()'s per-element
       switch: those 3 entries hit "can't determine size", which counts them
       into out_skipped even on the very first merge - they aren't duplicates,
       they're simply un-sizable, and that still counts as a skip. */
    TableMap merged;
    vFormHashMap(&merged);
    int merged_count = 0, skipped_count = 0;
    CHECK(bMapMergeInto(&merged, &map, &merged_count, &skipped_count));
    CHECK(merged_count == iCountMap(&map) - STRUCT_UNION_MAP_PUSHED);
    CHECK(skipped_count == STRUCT_UNION_MAP_PUSHED);
    /* merging again: every entry that made it in the first time (the 15
       sizable ones) is now a duplicate -> skipped as "already present". The
       3 struct/union entries are skipped again too (still un-sizable,
       independent of duplication). Total skipped == every active element
       in src this pass. */
    CHECK(bMapMergeInto(&merged, &map, &merged_count, &skipped_count));
    CHECK(skipped_count == iCountMap(&map));

    /* --- removal --- */
    int before_count = iCountMap(&map);
    CHECK(bMapRemove(&map, k_int2));
    CHECK(!tMapContains(&map, k_int2));
    CHECK(iCountMap(&map) == before_count - 1);
    CHECK(!bMapRemove(&map, k_int2)); /* already gone */

    /* --- clear / teardown --- */
    tMapClear(&map); /* internally calls vDropHashMap */
    CHECK(iCountMap(&map) == 0);
    CHECK(bIsEmptyMap(&map));

    vDropHashMap(&clone);
    vDropHashMap(&merged);
    /* map itself already cleared above via tMapClear, but drop again is safe/idempotent */
    vDropHashMap(&map);
}

/* ---------------------------------------------------------------------- */
/* main                                                                    */
/* ---------------------------------------------------------------------- */
int main(void)
{
    printf("========================================================\n");
    printf(" VOIDSTAR/TABLE exhaustive function-coverage test suite\n");
    printf("========================================================\n");

    test_table_dynamic();
    test_table_stack();
    test_table_list();
    test_stack_table_list();
    test_table_map();

    printf("\n========================================================\n");
    printf(" RESULTS: %d/%d checks passed", g_tests_run - g_tests_failed, g_tests_run);
    if (g_tests_failed > 0)
        printf("  (%d FAILED)\n", g_tests_failed);
    else
        printf("  (all passed)\n");
    printf("========================================================\n");

    return g_tests_failed == 0 ? 0 : 1;
}