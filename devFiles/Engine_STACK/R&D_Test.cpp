#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include "../core/Array/include/T_LCL_DEFS.h"

/* ----------------------------------------------------------------------
 * Memory Testing & Assert Logging Framework
 * ---------------------------------------------------------------------- */

typedef struct {
    int total;
    int passed;
    int failed;
} MemoryTestMetrics;

static MemoryTestMetrics g_mem_metrics = { 0, 0, 0 };

static void print_suite_header(const char* suite_name) {
    printf("\n========================================================================\n");
    printf("  MEMORY AUDIT SUITE: %s\n", suite_name);
    printf("========================================================================\n");
}

static void log_mem_test(bool pass, const char* test_id, const char* name, const char* detail) {
    g_mem_metrics.total++;
    if (pass) {
        g_mem_metrics.passed++;
        printf("  [PASS] %-8s | %-44s | %s\n", test_id, name, detail ? detail : "OK");
    }
    else {
        g_mem_metrics.failed++;
        printf("  [FAIL] %-8s | %-44s | %s\n", test_id, name, detail ? detail : "MEMORY ASSERTON FAILED");
    }
}

/* Struct definitions for exact byte boundary checks */
typedef struct { uint8_t b1; } Struct1Byte;
typedef struct { uint32_t i1; } Struct4Byte;
typedef struct { uint64_t u1; } Struct8Byte;
typedef struct { uint64_t u1; uint8_t b1; } Struct9Byte; /* Exceeds sizeof(void*) on 64-bit */

/* ----------------------------------------------------------------------
 * Module 01: Bit-Level Inline Storage & Endianness
 * ---------------------------------------------------------------------- */
static void test_mem_inline_raw_bytes(void) {
    print_suite_header("01. Bit-Level Inline Storage Verification");
    vGetTableStack(stk, 5);

    int32_t test_int = 0x12345678;
    bPushIntStack(&stk, test_int);

    TableSlot slot = sPeekSlotStack(&stk);

    /* Verify directly that the slot.ptr cell holds the exact raw bytes */
    bool byte_match = (memcmp(&slot.ptr, &test_int, sizeof(test_int)) == 0);
    log_mem_test(byte_match, "MEM-0101", "32-bit Integer raw byte storage match", "Exact match in slot.ptr cell");

    double test_dbl = 3.141592653589793;
    bPushDoubleStack(&stk, test_dbl);
    TableSlot dbl_slot = sPeekSlotStack(&stk);

    bool dbl_match = (memcmp(&dbl_slot.ptr, &test_dbl, sizeof(test_dbl)) == 0);
    log_mem_test(dbl_match, "MEM-0102", "64-bit Double raw byte storage match", "Full cell word populated");

    vDropTableStack(&stk);
}

static void test_mem_cell_isolation(void) {
    print_suite_header("02. Cell Isolation & Dirty Memory Zeroing");
    vGetTableStack(stk, 5);

    /* Push 8-byte dirty pattern filling all cell bytes using public API */
    uint64_t dirty_pattern = 0xFFFFFFFFFFFFFFFFULL;
    bPushDoubleStack(&stk, *(double*)&dirty_pattern);
    sPopSlotStack(&stk); /* Vacates slot 0 */

    /* Push 1-byte char into slot 0 */
    char c = 'A';
    bPushCharStack(&stk, c);

    TableSlot slot = sPeekSlotStack(&stk);
    const unsigned char* raw_bytes = (const unsigned char*)&slot.ptr;

    /* Verify byte 0 is 'A' and remaining bytes 1..7 are strictly 0x00 */
    bool clean = (raw_bytes[0] == 'A');
    for (size_t i = 1; i < sizeof(void*); i++) {
        if (raw_bytes[i] != 0x00) {
            clean = false;
            break;
        }
    }

    log_mem_test(clean, "MEM-0201", "Zero-before-write cell isolation", "Trailing cell bytes sanitized to 0x00");

    vDropTableStack(&stk);
}

/* ----------------------------------------------------------------------
 * Module 03: Machine Word Boundary Enforcements (<= sizeof(void*))
 * ---------------------------------------------------------------------- */
static void test_mem_word_size_limits(void) {
    print_suite_header("03. Word Size Capping (<= sizeof(void*))");
    vGetTableStack(stk, 5);

    Struct1Byte s1 = { 0xAB };
    Struct4Byte s4 = { 0x12345678 };
    Struct8Byte s8 = { 0x1122334455667788ULL };
    Struct9Byte s9 = { 0x1122334455667788ULL, 0x99 };

    log_mem_test(PUSH_STRUCT_STACK(&stk, &s1), "MEM-0301", "1-byte Struct push", "Accepted");
    log_mem_test(PUSH_STRUCT_STACK(&stk, &s4), "MEM-0302", "4-byte Struct push", "Accepted");
    log_mem_test(PUSH_STRUCT_STACK(&stk, &s8), "MEM-0303", "8-byte Struct push (Max Word Size)", "Accepted");

    bool over_limit = PUSH_STRUCT_STACK(&stk, &s9);
    log_mem_test(!over_limit, "MEM-0304", "9-byte Struct push rejection (> 8 bytes)", "Rejected safely");

    vDropTableStack(&stk);
}

/* ----------------------------------------------------------------------
 * Module 04: Memory Sanitization on Pop and Drop
 * ---------------------------------------------------------------------- */
static void test_mem_sanitization(void) {
    print_suite_header("04. Memory Sanitization on Pop & Table Drop");
    vGetTableStack(stk, 5);

    bPushIntStack(&stk, 0xDEADBEEF);

    /* Audit pop memory cleanup */
    sPopSlotStack(&stk);

    /* Memory at index 0 should now be ptr=NULL, type=TYPE_EMPTY */
    TableSlot slot_after_pop = stk.slots[0];
    bool pop_sanitized = (slot_after_pop.ptr == NULL) && (slot_after_pop.type == TYPE_EMPTY);
    log_mem_test(pop_sanitized, "MEM-0401", "sPopSlotStack cell sanitization", "Slot reset to NULL / TYPE_EMPTY");

    /* Fill table and drop whole stack */
    bPushIntStack(&stk, 100);
    bPushIntStack(&stk, 200);
    vDropTableStack(&stk);

    bool full_zeroed = true;
    for (int i = 0; i < stk.capacity; i++) {
        if (stk.slots[i].ptr != NULL || stk.slots[i].type != TYPE_EMPTY) {
            full_zeroed = false;
            break;
        }
    }
    log_mem_test(full_zeroed, "MEM-0402", "vDropTableStack complete memset zeroing", "All backing slots memset to 0");
}

/* ----------------------------------------------------------------------
 * Module 05: Address Preservation & 64-Bit Pointer Fidelity
 * ---------------------------------------------------------------------- */
static void test_mem_pointer_fidelity(void) {
    print_suite_header("05. Pointer Address Bit Preservation");
    vGetTableStack(stk, 5);

    uint64_t target_var = 0xCAFEBABE12345678ULL;
    void* orig_addr = (void*)&target_var;

    PUSH_VOID_PTR_STACK(&stk, orig_addr);
    TableSlot slot = sPeekSlotStack(&stk);

    log_mem_test(slot.ptr == orig_addr, "MEM-0501", "Pointer exact address preservation", "Address bits unmodified");

    vDropTableStack(&stk);
}

/* ----------------------------------------------------------------------
 * Module 06: Local Stack Guard & Boundary Overrun Defense
 * ---------------------------------------------------------------------- */
static void test_mem_stack_guard_overrun(void) {
    print_suite_header("06. Stack Frame Guard & Overrun Defense");

    /* Declare canary variables surrounding stack table memory */
    volatile uint64_t guard_pre = 0xAAAAAAAAAAAAAAAAULL;
    vGetTableStack(stk, 3);
    volatile uint64_t guard_post = 0xBBBBBBBBBBBBBBBBULL;

    /* Intentional attempt to overfill stack capacity */
    bPushIntStack(&stk, 10);
    bPushIntStack(&stk, 20);
    bPushIntStack(&stk, 30);
    bPushIntStack(&stk, 40); /* Overflow push attempt */

    bool guards_intact = (guard_pre == 0xAAAAAAAAAAAAAAAAULL) && (guard_post == 0xBBBBBBBBBBBBBBBBULL);
    log_mem_test(guards_intact, "MEM-0601", "Adjacent stack frame memory preservation", "Canary guards intact");

    vDropTableStack(&stk);
}

/* ----------------------------------------------------------------------
 * Main Execution Entry Point
 * ---------------------------------------------------------------------- */
int main(void) {
    printf("========================================================================\n");
    printf("  BEGINNING TABLESTACK MEMORY & BYTE-LEVEL AUDIT SUITE\n");
    printf("========================================================================\n");

    test_mem_inline_raw_bytes();
    test_mem_cell_isolation();
    test_mem_word_size_limits();
    test_mem_sanitization();
    test_mem_pointer_fidelity();
    test_mem_stack_guard_overrun();

    printf("\n========================================================================\n");
    printf("  MEMORY AUDIT SUMMARY FOR RESEARCH REVIEW\n");
    printf("========================================================================\n");
    printf("  Total Memory Audits Executed : %d\n", g_mem_metrics.total);
    printf("  Passed                       : %d\n", g_mem_metrics.passed);
    printf("  Failed                       : %d\n", g_mem_metrics.failed);
    printf("  Memory Integrity Pass Rate   : %.2f%%\n", g_mem_metrics.total > 0 ? ((float)g_mem_metrics.passed / g_mem_metrics.total) * 100.0f : 0.0f);
    printf("========================================================================\n\n");

    return g_mem_metrics.failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}