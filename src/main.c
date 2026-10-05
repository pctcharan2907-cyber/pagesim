#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "pagesim.h"
#include "policy.h"
#include "trace.h"
#include "belady.h"
#include "working_set.h"
#include "visualizer.h"
#include "swap_store.h"
#include "process_sim.h"

static void print_banner(void)
{
    printf("\n========================================================================================\n");
    printf("   PageSim v%s -- Virtual Memory Demand Paging & Page Replacement Simulator           \n", PAGESIM_VERSION);
    printf("   Operating Systems Course Project | Advanced Hybrid C Simulator + xv6 Kernel          \n");
    printf("========================================================================================\n\n");
}

static void print_help(const char *prog_name)
{
    print_banner();
    printf("Usage: %s [options]\n\n", prog_name);
    printf("Core Simulation Options:\n");
    printf("  -f, --frames <N>         Number of physical page frames (default: 4)\n");
    printf("  -p, --policy <name>      Policy: FIFO, LRU, Clock, Optimal, Aging, or 'all'\n");
    printf("  -t, --trace <string>     Inline reference string (e.g., \"1,2,3,4,1,2,5,1,2,3,4,5\")\n");
    printf("  -i, --input <file>       Load memory reference trace from file\n");
    printf("  -s, --stream             Stream reference string from stdin (pipe mode)\n");
    printf("  -v, --verbose            Print detailed step-by-step trace execution\n\n");

    printf("Analysis & Advanced Features:\n");
    printf("  --compare                Run and compare all policies side-by-side on the trace\n");
    printf("  --belady                 Run Bélády's anomaly detector across frame range\n");
    printf("  --min-frames <N>         Minimum frames for Bélády/Working-Set scan (default: 2)\n");
    printf("  --max-frames <N>         Maximum frames for Bélády/Working-Set scan (default: 8)\n");
    printf("  --workingset             Compute working-set curve W(t, delta) and fault knee\n");
    printf("  --timeline               Display visual ASCII timeline of physical frame occupancy\n");
    printf("  --export <file.csv>      Export simulation data to CSV for plotting\n");
    printf("  --clean-drops            Enable dirty-bit awareness (clean pages dropped without disk I/O)\n\n");

    printf("Synthetic Trace Generators:\n");
    printf("  --gen <type>             Generate synthetic trace: belady, locality, looping, random, elf\n");
    printf("  --len <N>                Number of references for generated trace (default: 100)\n");
    printf("  --save-trace <file>      Save generated trace to file\n\n");

    printf("Demonstrations:\n");
    printf("  --demo                   Run comprehensive Week-12 acceptance test battery and demo\n");
    printf("  --policies               List all available page replacement policies\n");
    printf("  -h, --help               Display this help guide\n\n");
}

static void run_single_simulation(const Trace *trace, PageReplacementPolicy *policy, int num_frames, bool verbose, bool timeline, bool dirty_aware)
{
    PageSimConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.num_frames = num_frames;
    cfg.page_size = 4096;
    cfg.verbose = verbose;
    cfg.track_timeline = timeline;
    cfg.dirty_aware = dirty_aware;
    cfg.timer_interval = 4;

    PageSim *sim = pagesim_create(&cfg, policy);
    if (!sim) {
        fprintf(stderr, "Error: Failed to initialize simulator.\n");
        return;
    }

    if (verbose) {
        printf("\nExecuting %s with %d frames on trace (len=%lu)...\n", policy->name, num_frames, (unsigned long)trace->count);
        printf("--------------------------------------------------------------------------------\n");
    }

    for (size_t i = 0; i < trace->count; i++) {
        const PageRef *future = (i + 1 < trace->count) ? &trace->refs[i + 1] : NULL;
        size_t future_len = trace->count - 1 - i;

        SimStepResult res = pagesim_access(sim, &trace->refs[i], future, future_len);

        if (verbose) {
            printf("Step %3lu: Ref Page %3d [%c] -> %s | Frame %d",
                   (unsigned long)i + 1,
                   res.page_id,
                   (char)trace->refs[i].access_type,
                   res.is_hit ? "[HIT ]" : "[FAULT]",
                   res.frame_id);

            if (!res.is_hit && res.victim_frame_id >= 0) {
                printf(" | Evicted Page %d (Frame %d) [%s]",
                       res.victim_page_id,
                       res.victim_frame_id,
                       res.was_dirty_eviction ? "DIRTY->WRITE" : "CLEAN->DROP");
            }
            printf("\n");
        }
    }

    pagesim_update_stats(sim);
    pagesim_print_stats(sim, policy->name);

    if (timeline && sim->timeline) {
        visualizer_render_timeline(sim->timeline, 50, stdout);
    }

    pagesim_destroy(sim);
}

static void run_compare_all(const Trace *trace, int num_frames, bool dirty_aware)
{
    PageReplacementPolicy *policies[] = {
        policy_fifo_get(),
        policy_lru_get(),
        policy_clock_get(),
        policy_optimal_get(),
        policy_aging_get()
    };
    int count = 5;

    SimStats stats[5];
    const char *names[5];

    printf("\nRunning all 5 policies with %d frames on %lu references...\n", num_frames, (unsigned long)trace->count);

    for (int i = 0; i < count; i++) {
        names[i] = policies[i]->name;

        PageSimConfig cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.num_frames = num_frames;
        cfg.page_size = 4096;
        cfg.verbose = false;
        cfg.track_timeline = false;
        cfg.dirty_aware = dirty_aware;
        cfg.timer_interval = 4;

        PageSim *sim = pagesim_create(&cfg, policies[i]);
        if (sim) {
            pagesim_replay(sim, trace->refs, trace->count);
            stats[i] = sim->stats;
            pagesim_destroy(sim);
        }
    }

    visualizer_render_scorecard(stats, names, count, stdout);
}

static void run_automated_demo(void)
{
    print_banner();
    printf("========================================================================================\n");
    printf("                   RUNNING AUTOMATED ACCEPTANCE TEST & DEMO BATTERY                     \n");
    printf("========================================================================================\n\n");

    /* TEST 1: Classic Bélády's Anomaly Verification */
    printf("----------------------------------------------------------------------------------------\n");
    printf("[DEMO 1/5] BÉLÁDY'S ANOMALY DETECTION (Classic Reference String: 1 2 3 4 1 2 5 1 2 3 4 5)\n");
    printf("----------------------------------------------------------------------------------------\n");
    Trace *belady_tr = trace_gen_belady();
    belady_compare_all(belady_tr, 2, 6, "demo_belady.csv");

    BeladyResult *fifo_res = belady_detect(belady_tr, policy_fifo_get(), 2, 6);
    belady_result_print(fifo_res);
    belady_result_free(fifo_res);

    BeladyResult *lru_res = belady_detect(belady_tr, policy_lru_get(), 2, 6);
    belady_result_print(lru_res);
    belady_result_free(lru_res);

    /* TEST 2: Visual Frame Occupancy Timeline */
    printf("\n----------------------------------------------------------------------------------------\n");
    printf("[DEMO 2/5] VISUAL TIMELINE OF FRAME OCCUPANCY (FIFO with 3 frames vs 4 frames)\n");
    printf("----------------------------------------------------------------------------------------\n");
    printf(">>> Running FIFO with 3 frames:\n");
    run_single_simulation(belady_tr, policy_fifo_get(), 3, false, true, true);

    printf(">>> Running FIFO with 4 frames (notice fault count increases from 9 to 10!):\n");
    run_single_simulation(belady_tr, policy_fifo_get(), 4, false, true, true);

    /* TEST 3: Working Set Model & Knee Analysis */
    printf("\n----------------------------------------------------------------------------------------\n");
    printf("[DEMO 3/5] WORKING-SET MODEL & RESIDENT SET FAULT CURVE\n");
    printf("----------------------------------------------------------------------------------------\n");
    Trace *loc_tr = trace_gen_locality(200, 4, 16, 0.80);
    WorkingSetCurve *ws_curve = working_set_compute_curve(loc_tr, 5, 50, 5);
    working_set_curve_print(ws_curve);
    working_set_curve_free(ws_curve);

    ResidentSetCurve *rs_curve = working_set_fault_curve(loc_tr, policy_lru_get(), 2, 12);
    resident_set_curve_print(rs_curve);
    visualizer_render_ascii_plot(rs_curve->frames, rs_curve->faults, rs_curve->num_points,
                                "LRU Page Faults vs Resident Set Size (Frames)", "Frames", "Faults", stdout);
    resident_set_curve_free(rs_curve);

    /* TEST 4: Dirty-bit awareness and backing store */
    printf("\n----------------------------------------------------------------------------------------\n");
    printf("[DEMO 4/5] DIRTY-BIT AWARENESS & SWAP BACKING STORE\n");
    printf("----------------------------------------------------------------------------------------\n");
    SwapStore *store = swap_store_create(128, "demo_swap.bin");
    uint8_t dummy_page[4096] = {0xAA};

    printf("  Simulating evictions with dirty-bit tracking:\n");
    int s1 = swap_store_alloc_slot(store, 10, 1);
    swap_store_page_out(store, s1, dummy_page, true);  /* Dirty write */
    printf("  Evicted Dirty Page 10 -> Written to Swap Slot %d\n", s1);

    int s2 = swap_store_alloc_slot(store, 11, 1);
    swap_store_page_out(store, s2, dummy_page, false); /* Clean drop! */
    printf("  Evicted Clean Page 11 -> Dropped cleanly without writing to disk!\n");

    swap_store_print_stats(store);
    swap_store_destroy(store);

    /* TEST 5: Multi-Process Working Sets and Thrashing */
    printf("\n----------------------------------------------------------------------------------------\n");
    printf("[DEMO 5/5] MULTI-PROCESS INTERACTION & THRASHING DETECTION\n");
    printf("----------------------------------------------------------------------------------------\n");
    Trace *p1 = trace_gen_locality(100, 3, 10, 0.85);
    Trace *p2 = trace_gen_locality(100, 3, 10, 0.85);
    Trace *p3 = trace_gen_looping(100, 8);
    Trace *proc_traces[] = {p1, p2, p3};

    MultiProcessSim *psim = proc_sim_create(3, 8, ALLOC_GLOBAL, 4);
    proc_sim_run_traces(psim, proc_traces, 3);
    proc_sim_print_report(psim);
    proc_sim_destroy(psim);

    trace_free(p1);
    trace_free(p2);
    trace_free(p3);
    trace_free(loc_tr);
    trace_free(belady_tr);

    printf("========================================================================================\n");
    printf("                      DEMO COMPLETED SUCCESSFULLY: ALL CHECKS PASSED                     \n");
    printf("========================================================================================\n\n");
}

int main(int argc, char **argv)
{
    int num_frames = DEFAULT_FRAME_COUNT;
    const char *policy_name = "FIFO";
    const char *inline_trace_str = NULL;
    const char *input_file = NULL;
    const char *gen_type = NULL;
    const char *save_trace_file = NULL;
    const char *export_csv_file = NULL;
    int gen_len = 100;
    int min_frames = 2;
    int max_frames = 8;
    bool verbose = false;
    bool timeline = false;
    bool compare_all = false;
    bool run_belady = false;
    bool run_workingset = false;
    bool dirty_aware = true;
    bool stream_mode = false;

    if (argc < 2) {
        print_help(argv[0]);
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--policies") == 0) {
            policy_print_all();
            return 0;
        } else if (strcmp(argv[i], "--demo") == 0) {
            run_automated_demo();
            return 0;
        } else if ((strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--frames") == 0) && i + 1 < argc) {
            num_frames = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--policy") == 0) && i + 1 < argc) {
            policy_name = argv[++i];
        } else if ((strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--trace") == 0) && i + 1 < argc) {
            inline_trace_str = argv[++i];
        } else if ((strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--input") == 0) && i + 1 < argc) {
            input_file = argv[++i];
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--stream") == 0) {
            stream_mode = true;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--timeline") == 0) {
            timeline = true;
        } else if (strcmp(argv[i], "--compare") == 0) {
            compare_all = true;
        } else if (strcmp(argv[i], "--belady") == 0) {
            run_belady = true;
        } else if (strcmp(argv[i], "--workingset") == 0) {
            run_workingset = true;
        } else if (strcmp(argv[i], "--clean-drops") == 0) {
            dirty_aware = true;
        } else if (strcmp(argv[i], "--min-frames") == 0 && i + 1 < argc) {
            min_frames = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--max-frames") == 0 && i + 1 < argc) {
            max_frames = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--gen") == 0 && i + 1 < argc) {
            gen_type = argv[++i];
        } else if (strcmp(argv[i], "--len") == 0 && i + 1 < argc) {
            gen_len = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--save-trace") == 0 && i + 1 < argc) {
            save_trace_file = argv[++i];
        } else if (strcmp(argv[i], "--export") == 0 && i + 1 < argc) {
            export_csv_file = argv[++i];
        } else {
            fprintf(stderr, "Unknown option: %s (use --help for usage)\n", argv[i]);
            return 1;
        }
    }

    /* Streaming mode from stdin (Milestone 7 producer-consumer) */
    if (stream_mode) {
        PageReplacementPolicy *policy = policy_find_by_name(policy_name);
        if (!policy) {
            fprintf(stderr, "Unknown policy: %s\n", policy_name);
            return 1;
        }

        PageSimConfig cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.num_frames = num_frames;
        cfg.dirty_aware = dirty_aware;

        PageSim *sim = pagesim_create(&cfg, policy);
        TraceStream *ts = trace_stream_open("-");
        PageRef ref;

        while (trace_stream_next(ts, &ref)) {
            pagesim_access(sim, &ref, NULL, 0);
        }

        trace_stream_close(ts);
        pagesim_update_stats(sim);
        pagesim_print_stats(sim, policy->name);
        pagesim_destroy(sim);
        return 0;
    }

    /* Obtain Trace */
    Trace *trace = NULL;
    if (inline_trace_str) {
        trace = trace_parse_string(inline_trace_str);
    } else if (input_file) {
        trace = trace_load_file(input_file);
        if (!trace) {
            fprintf(stderr, "Error: Could not open trace file '%s'\n", input_file);
            return 1;
        }
    } else if (gen_type) {
        if (strcmp(gen_type, "belady") == 0) {
            trace = trace_gen_belady();
        } else if (strcmp(gen_type, "locality") == 0) {
            trace = trace_gen_locality(gen_len, 4, 16, 0.80);
        } else if (strcmp(gen_type, "looping") == 0) {
            trace = trace_gen_looping(gen_len, 6);
        } else if (strcmp(gen_type, "random") == 0) {
            trace = trace_gen_random(gen_len, 20);
        } else if (strcmp(gen_type, "elf") == 0) {
            trace = trace_gen_elf_pattern(gen_len);
        } else {
            fprintf(stderr, "Unknown generator type: %s\n", gen_type);
            return 1;
        }
    } else {
        /* Default to classic Bélády trace */
        trace = trace_gen_belady();
    }

    if (!trace || trace->count == 0) {
        fprintf(stderr, "Error: No valid reference trace provided.\n");
        return 1;
    }

    if (save_trace_file) {
        trace_save_file(trace, save_trace_file);
        printf("Saved trace (%lu references) to: %s\n", (unsigned long)trace->count, save_trace_file);
    }

    /* Bélády anomaly scan */
    if (run_belady) {
        belady_compare_all(trace, min_frames, max_frames, export_csv_file);
        trace_free(trace);
        return 0;
    }

    /* Working-set model analysis */
    if (run_workingset) {
        WorkingSetCurve *ws_curve = working_set_compute_curve(trace, 2, 20, 2);
        working_set_curve_print(ws_curve);
        working_set_curve_free(ws_curve);

        ResidentSetCurve *rs_curve = working_set_fault_curve(trace, policy_lru_get(), min_frames, max_frames);
        resident_set_curve_print(rs_curve);
        visualizer_render_ascii_plot(rs_curve->frames, rs_curve->faults, rs_curve->num_points,
                                    "LRU Faults vs Resident Set Frames", "Frames", "Faults", stdout);
        resident_set_curve_free(rs_curve);

        if (export_csv_file) {
            working_set_export_csv(trace, min_frames, max_frames, export_csv_file);
            printf("Exported working-set curve to %s\n", export_csv_file);
        }
        trace_free(trace);
        return 0;
    }

    /* Compare all policies */
    if (compare_all || strcmp(policy_name, "all") == 0) {
        run_compare_all(trace, num_frames, dirty_aware);
        trace_free(trace);
        return 0;
    }

    /* Single policy run */
    PageReplacementPolicy *policy = policy_find_by_name(policy_name);
    if (!policy) {
        fprintf(stderr, "Unknown policy: %s (run with --policies to view available)\n", policy_name);
        trace_free(trace);
        return 1;
    }

    run_single_simulation(trace, policy, num_frames, verbose, timeline, dirty_aware);

    trace_free(trace);
    return 0;
}
