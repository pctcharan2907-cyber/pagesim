#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "pagesim.h"
#include "policy.h"
#include "trace.h"

/*
 * Test Standard Textbook Reference String (Silberschatz OS Concepts):
 * Reference string: 7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1 (20 references)
 * Physical frames: 3
 *
 * Expected Results:
 * - FIFO:    15 faults (hit ratio = 5/20 = 25%)
 * - LRU:     12 faults (hit ratio = 8/20 = 40%)
 * - Optimal:  9 faults (hit ratio = 11/20 = 55%)
 */
static void test_silberschatz_reference_string(void)
{
    printf("[TEST] Running Silberschatz Textbook Benchmark (20 refs, 3 frames)...\n");

    const char *ref_str = "7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1";
    Trace *tr = trace_parse_string(ref_str);
    assert(tr != NULL);
    assert(tr->count == 20);

    PageSimConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.num_frames = 3;
    cfg.page_size = 4096;
    cfg.dirty_aware = true;

    /* 1. Test FIFO */
    PageSim *s_fifo = pagesim_create(&cfg, policy_fifo_get());
    pagesim_replay(s_fifo, tr->refs, tr->count);
    printf("  FIFO Faults: %lu (Expected: 15)\n", (unsigned long)s_fifo->stats.faults);
    assert(s_fifo->stats.faults == 15);
    assert(s_fifo->stats.hits == 5);
    pagesim_destroy(s_fifo);

    /* 2. Test LRU */
    PageSim *s_lru = pagesim_create(&cfg, policy_lru_get());
    pagesim_replay(s_lru, tr->refs, tr->count);
    printf("  LRU Faults:  %lu (Expected: 12)\n", (unsigned long)s_lru->stats.faults);
    assert(s_lru->stats.faults == 12);
    assert(s_lru->stats.hits == 8);
    pagesim_destroy(s_lru);

    /* 3. Test Optimal */
    PageSim *s_opt = pagesim_create(&cfg, policy_optimal_get());
    pagesim_replay(s_opt, tr->refs, tr->count);
    printf("  OPT Faults:  %lu (Expected: 9)\n", (unsigned long)s_opt->stats.faults);
    assert(s_opt->stats.faults == 9);
    assert(s_opt->stats.hits == 11);
    pagesim_destroy(s_opt);

    /* 4. Test Clock */
    PageSim *s_clk = pagesim_create(&cfg, policy_clock_get());
    pagesim_replay(s_clk, tr->refs, tr->count);
    printf("  Clock Faults:%lu\n", (unsigned long)s_clk->stats.faults);
    assert(s_clk->stats.faults <= 15); /* Clock should perform roughly comparable to LRU/FIFO */
    pagesim_destroy(s_clk);

    /* 5. Test Aging */
    PageSim *s_aging = pagesim_create(&cfg, policy_aging_get());
    pagesim_replay(s_aging, tr->refs, tr->count);
    printf("  Aging Faults:%lu\n", (unsigned long)s_aging->stats.faults);
    assert(s_aging->stats.faults <= 15);
    pagesim_destroy(s_aging);

    trace_free(tr);
    printf("  -> PASS: All textbook policy fault counts verified!\n\n");
}

/*
 * Test Tanenbaum Reference String:
 * 0 1 2 3 0 1 4 0 1 2 3 4 with 3 frames
 */
static void test_tanenbaum_reference_string(void)
{
    printf("[TEST] Running Tanenbaum Reference String (12 refs, 3 frames)...\n");

    const char *ref_str = "0 1 2 3 0 1 4 0 1 2 3 4";
    Trace *tr = trace_parse_string(ref_str);
    assert(tr != NULL);
    assert(tr->count == 12);

    PageSimConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.num_frames = 3;

    PageSim *s_opt = pagesim_create(&cfg, policy_optimal_get());
    pagesim_replay(s_opt, tr->refs, tr->count);
    printf("  Optimal Faults: %lu (Expected: 7)\n", (unsigned long)s_opt->stats.faults);
    assert(s_opt->stats.faults == 7);
    pagesim_destroy(s_opt);

    PageSim *s_lru = pagesim_create(&cfg, policy_lru_get());
    pagesim_replay(s_lru, tr->refs, tr->count);
    printf("  LRU Faults:     %lu (Expected: 10)\n", (unsigned long)s_lru->stats.faults);
    assert(s_lru->stats.faults == 10);
    pagesim_destroy(s_lru);

    trace_free(tr);
    printf("  -> PASS: Tanenbaum benchmark verified!\n\n");
}

int main(void)
{
    printf("====================================================\n");
    printf(" Running Policy Correctness Test Suite             \n");
    printf("====================================================\n\n");

    test_silberschatz_reference_string();
    test_tanenbaum_reference_string();

    printf("====================================================\n");
    printf(" ALL POLICY TESTS PASSED SUCCESSFULLY!             \n");
    printf("====================================================\n");
    return 0;
}
