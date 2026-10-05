#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "pagesim.h"
#include "trace.h"
#include "policy.h"
#include "swap_store.h"
#include "process_sim.h"

static void test_hex_trace_parsing(void)
{
    printf("[TEST] Testing Hex Virtual Address and R/W Parsing...\n");

    const char *hex_str = "0x00401000:R, 0x00401004:R, 0x10004000:W, 0x7FFF0000:W";
    Trace *tr = trace_parse_string(hex_str);
    assert(tr != NULL);
    assert(tr->count == 4);

    assert(tr->refs[0].page_id == (0x00401000 >> 12));
    assert(tr->refs[0].access_type == ACCESS_READ);

    assert(tr->refs[1].page_id == (0x00401004 >> 12));
    assert(tr->refs[1].access_type == ACCESS_READ);

    assert(tr->refs[2].page_id == (0x10004000 >> 12));
    assert(tr->refs[2].access_type == ACCESS_WRITE);

    assert(tr->refs[3].page_id == (0x7FFF0000 >> 12));
    assert(tr->refs[3].access_type == ACCESS_WRITE);

    trace_free(tr);
    printf("  -> PASS: Hex address parsing with permissions verified!\n\n");
}

static void test_dirty_drop_optimization(void)
{
    printf("[TEST] Testing Dirty-Bit Awareness (Clean Drop Optimization)...\n");

    /* 3 references to page 1 (Read), 2 (Read), 3 (Write), then 4 (Read) */
    Trace *tr = trace_parse_string("1:R, 2:R, 3:W, 4:R");
    assert(tr != NULL);

    PageSimConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.num_frames = 2; /* 2 frames forces evictions */
    cfg.dirty_aware = true;

    PageSim *sim = pagesim_create(&cfg, policy_fifo_get());
    assert(sim != NULL);

    /*
     * Trace trace:
     * Step 1: Ref 1:R -> Frame 0 (Clean)
     * Step 2: Ref 2:R -> Frame 1 (Clean)
     * Step 3: Ref 3:W -> Evict Page 1 (CLEAN). Clean drop avoided disk write! Frame 0 now Page 3 (Dirty).
     * Step 4: Ref 4:R -> Evict Page 2 (CLEAN). Clean drop avoided disk write!
     */
    pagesim_replay(sim, tr->refs, tr->count);

    printf("  Total Faults: %lu\n", (unsigned long)sim->stats.faults);
    printf("  Clean Drops:  %lu\n", (unsigned long)sim->stats.clean_page_drops);
    printf("  Dirty Outs:   %lu\n", (unsigned long)sim->stats.dirty_page_outs);

    assert(sim->stats.faults == 4);
    assert(sim->stats.clean_page_drops == 2);
    assert(sim->stats.dirty_page_outs == 0);

    pagesim_destroy(sim);
    trace_free(tr);
    printf("  -> PASS: Clean page drops correctly avoided backing store I/O!\n\n");
}

static void test_multi_process_simulation(void)
{
    printf("[TEST] Testing Multi-Process Scheduler & Thrashing...\n");

    Trace *t1 = trace_gen_locality(50, 2, 8, 0.90);
    Trace *t2 = trace_gen_locality(50, 2, 8, 0.90);
    Trace *traces[] = {t1, t2};

    MultiProcessSim *psim = proc_sim_create(2, 6, ALLOC_GLOBAL, 2);
    assert(psim != NULL);

    proc_sim_run_traces(psim, traces, 2);
    assert(psim->processes[0].accesses == 50);
    assert(psim->processes[1].accesses == 50);

    proc_sim_destroy(psim);
    trace_free(t1);
    trace_free(t2);
    printf("  -> PASS: Multi-process scheduling completed cleanly!\n\n");
}

int main(void)
{
    printf("====================================================\n");
    printf(" Running Trace & System Integration Test Suite     \n");
    printf("====================================================\n\n");

    test_hex_trace_parsing();
    test_dirty_drop_optimization();
    test_multi_process_simulation();

    printf("====================================================\n");
    printf(" ALL TRACE & INTEGRATION TESTS PASSED!             \n");
    printf("====================================================\n");
    return 0;
}
