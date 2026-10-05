#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "pagesim.h"
#include "policy.h"
#include "trace.h"
#include "belady.h"

/*
 * Verification of Bélády's Anomaly (1969 László Bélády):
 * On the classic reference string:
 *   1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5
 *
 * FIFO Fault Counts:
 * - 1 Frame:  12 faults
 * - 2 Frames: 12 faults
 * - 3 Frames:  9 faults
 * - 4 Frames: 10 faults  <-- ANOMALY! Increasing memory increases page faults!
 *
 * Stack Algorithm Guarantee:
 * - LRU:     3 frames = 10 faults, 4 frames = 8 faults (Monotonically decreasing)
 * - Optimal: 3 frames =  7 faults, 4 frames = 6 faults (Monotonically decreasing)
 */
static void test_classic_belady_anomaly(void)
{
    printf("[TEST] Verifying Bélády's Anomaly on Classic 12-ref string...\n");

    Trace *tr = trace_gen_belady();
    assert(tr != NULL);
    assert(tr->count == 12);

    /* 1. Verify FIFO Anomaly */
    BeladyResult *res_fifo = belady_detect(tr, policy_fifo_get(), 3, 4);
    assert(res_fifo != NULL);
    printf("  FIFO with 3 frames: %d faults\n", res_fifo->fault_counts[0]);
    printf("  FIFO with 4 frames: %d faults\n", res_fifo->fault_counts[1]);

    assert(res_fifo->fault_counts[0] == 9);
    assert(res_fifo->fault_counts[1] == 10);
    assert(res_fifo->anomaly_detected == true);
    assert(res_fifo->anomalous_frame_f1 == 3);
    assert(res_fifo->anomalous_frame_f2 == 4);
    printf("  -> PASS: Bélády's anomaly detected for FIFO (3 frames: 9 faults -> 4 frames: 10 faults)!\n");
    belady_result_free(res_fifo);

    /* 2. Verify LRU is immune (Stack Algorithm) */
    BeladyResult *res_lru = belady_detect(tr, policy_lru_get(), 3, 4);
    assert(res_lru != NULL);
    printf("  LRU with 3 frames:  %d faults\n", res_lru->fault_counts[0]);
    printf("  LRU with 4 frames:  %d faults\n", res_lru->fault_counts[1]);

    assert(res_lru->fault_counts[0] == 10);
    assert(res_lru->fault_counts[1] == 8);
    assert(res_lru->anomaly_detected == false);
    printf("  -> PASS: LRU exhibits strictly monotonic behavior (no anomaly)!\n");
    belady_result_free(res_lru);

    /* 3. Verify Optimal is immune (Stack Algorithm) */
    BeladyResult *res_opt = belady_detect(tr, policy_optimal_get(), 3, 4);
    assert(res_opt != NULL);
    printf("  Optimal with 3 frames: %d faults\n", res_opt->fault_counts[0]);
    printf("  Optimal with 4 frames: %d faults\n", res_opt->fault_counts[1]);

    assert(res_opt->fault_counts[0] == 7);
    assert(res_opt->fault_counts[1] == 6);
    assert(res_opt->anomaly_detected == false);
    printf("  -> PASS: Optimal exhibits strictly monotonic behavior (no anomaly)!\n");
    belady_result_free(res_opt);

    trace_free(tr);
    printf("\n");
}

int main(void)
{
    printf("====================================================\n");
    printf(" Running Bélády's Anomaly Test Suite               \n");
    printf("====================================================\n\n");

    test_classic_belady_anomaly();

    printf("====================================================\n");
    printf(" ALL BÉLÁDY'S ANOMALY TESTS PASSED!                \n");
    printf("====================================================\n");
    return 0;
}
