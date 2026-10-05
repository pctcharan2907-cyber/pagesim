#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "pagesim.h"
#include "trace.h"
#include "working_set.h"

static void test_working_set_window_calculation(void)
{
    printf("[TEST] Testing Working Set Window calculation...\n");

    /* Reference string: 1 2 3 1 2 4 */
    Trace *tr = trace_parse_string("1 2 3 1 2 4");
    assert(tr != NULL);
    assert(tr->count == 6);

    /* For delta = 1: each window size must be exactly 1 */
    for (size_t t = 0; t < tr->count; t++) {
        assert(working_set_size_at(tr, t, 1) == 1);
    }

    /* For delta = 3:
     * t=0: [1] -> 1
     * t=1: [1, 2] -> 2
     * t=2: [1, 2, 3] -> 3
     * t=3: [2, 3, 1] -> 3
     * t=4: [3, 1, 2] -> 3
     * t=5: [1, 2, 4] -> 3
     */
    assert(working_set_size_at(tr, 0, 3) == 1);
    assert(working_set_size_at(tr, 1, 3) == 2);
    assert(working_set_size_at(tr, 2, 3) == 3);
    assert(working_set_size_at(tr, 3, 3) == 3);
    assert(working_set_size_at(tr, 4, 3) == 3);
    assert(working_set_size_at(tr, 5, 3) == 3);

    trace_free(tr);
    printf("  -> PASS: Working set window sizes match theoretical expectation!\n\n");
}

static void test_resident_set_curve(void)
{
    printf("[TEST] Testing Resident Set Fault Curve...\n");

    Trace *tr = trace_gen_locality(100, 3, 10, 0.85);
    assert(tr != NULL);

    ResidentSetCurve *curve = working_set_fault_curve(tr, policy_lru_get(), 2, 8);
    assert(curve != NULL);
    assert(curve->num_points == 7);

    /* Verify faults are monotonically non-increasing for LRU */
    for (int i = 0; i < curve->num_points - 1; i++) {
        assert(curve->faults[i] >= curve->faults[i + 1]);
    }

    resident_set_curve_free(curve);
    trace_free(tr);
    printf("  -> PASS: Resident set fault curve is monotonically valid!\n\n");
}

int main(void)
{
    printf("====================================================\n");
    printf(" Running Working-Set Model Test Suite              \n");
    printf("====================================================\n\n");

    test_working_set_window_calculation();
    test_resident_set_curve();

    printf("====================================================\n");
    printf(" ALL WORKING-SET TESTS PASSED!                     \n");
    printf("====================================================\n");
    return 0;
}
