#include "belady.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

BeladyResult *belady_detect(const Trace *trace, PageReplacementPolicy *policy, int min_frames, int max_frames)
{
    if (!trace || !policy || min_frames <= 0 || max_frames < min_frames) {
        return NULL;
    }

    BeladyResult *res = (BeladyResult *)malloc(sizeof(BeladyResult));
    if (!res) return NULL;

    int num_points = max_frames - min_frames + 1;
    res->policy_name = policy->name;
    res->is_stack_algorithm = policy->is_stack_algorithm;
    res->min_frames = min_frames;
    res->max_frames = max_frames;
    res->anomaly_detected = false;
    res->anomalous_frame_f1 = -1;
    res->anomalous_frame_f2 = -1;
    res->faults_at_f1 = 0;
    res->faults_at_f2 = 0;

    res->fault_counts = (int *)calloc(num_points, sizeof(int));
    res->hit_ratios = (double *)calloc(num_points, sizeof(double));

    for (int f = min_frames; f <= max_frames; f++) {
        PageSimConfig cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.num_frames = f;
        cfg.page_size = 4096;
        cfg.verbose = false;
        cfg.track_timeline = false;
        cfg.dirty_aware = true;
        cfg.timer_interval = 0;

        PageSim *sim = pagesim_create(&cfg, policy);
        if (!sim) continue;

        pagesim_replay(sim, trace->refs, trace->count);

        int idx = f - min_frames;
        res->fault_counts[idx] = (int)sim->stats.faults;
        res->hit_ratios[idx] = sim->stats.hit_ratio;

        pagesim_destroy(sim);
    }

    /* Check if faults ever strictly increased as frame count increased */
    for (int i = 0; i < num_points - 1; i++) {
        int f1 = min_frames + i;
        int f2 = f1 + 1;
        int faults1 = res->fault_counts[i];
        int faults2 = res->fault_counts[i + 1];

        if (faults2 > faults1) {
            res->anomaly_detected = true;
            res->anomalous_frame_f1 = f1;
            res->anomalous_frame_f2 = f2;
            res->faults_at_f1 = faults1;
            res->faults_at_f2 = faults2;
            break;
        }
    }

    return res;
}

void belady_result_free(BeladyResult *result)
{
    if (!result) return;
    if (result->fault_counts) free(result->fault_counts);
    if (result->hit_ratios) free(result->hit_ratios);
    free(result);
}

void belady_result_print(const BeladyResult *res)
{
    if (!res) return;

    printf("\n============================================================\n");
    printf(" Bélády's Anomaly Analysis: Policy [%s] (%s)\n", res->policy_name, res->is_stack_algorithm ? "Stack Algorithm" : "Non-Stack Algorithm");
    printf("============================================================\n");
    printf(" Frames | Page Faults | Hit Ratio | Status\n");
    printf("------------------------------------------------------------\n");

    int num_points = res->max_frames - res->min_frames + 1;
    for (int i = 0; i < num_points; i++) {
        int f = res->min_frames + i;
        int faults = res->fault_counts[i];
        double hr = res->hit_ratios[i];

        const char *flag = "";
        if (i > 0 && faults > res->fault_counts[i - 1]) {
            flag = " <-- [ANOMALY: FAULTS INCREASED!]";
        } else if (i > 0 && faults < res->fault_counts[i - 1]) {
            flag = " (decreased)";
        } else if (i > 0) {
            flag = " (same)";
        }

        printf("  %5d |    %8d |   %6.2f%% | %s\n", f, faults, hr * 100.0, flag);
    }
    printf("------------------------------------------------------------\n");

    if (res->anomaly_detected) {
        printf(" [!] CONCLUSION: Bélády's Anomaly CONFIRMED for %s!\n", res->policy_name);
        printf("     Increasing memory from %d to %d frames caused faults to rise from %d to %d (+%d faults)!\n",
               res->anomalous_frame_f1, res->anomalous_frame_f2,
               res->faults_at_f1, res->faults_at_f2,
               res->faults_at_f2 - res->faults_at_f1);
        printf("     Explanation: %s is not a stack algorithm; the set of resident pages\n", res->policy_name);
        printf("     with %d frames is not guaranteed to be a superset of that with %d frames.\n", res->anomalous_frame_f2, res->anomalous_frame_f1);
    } else {
        printf(" [*] CONCLUSION: No anomaly detected in range [%d..%d] for %s.\n", res->min_frames, res->max_frames, res->policy_name);
        if (res->is_stack_algorithm) {
            printf("     Theoretical Guarantee: %s is a Stack Algorithm satisfying the inclusion property\n", res->policy_name);
            printf("     M(k) subset of M(k+1) at all times; provably immune to Bélády's anomaly.\n");
        }
    }
    printf("============================================================\n");
}

void belady_compare_all(const Trace *trace, int min_frames, int max_frames, const char *csv_export_path)
{
    if (!trace || min_frames <= 0 || max_frames < min_frames) return;

    PageReplacementPolicy *policies[] = {
        policy_fifo_get(),
        policy_lru_get(),
        policy_clock_get(),
        policy_optimal_get(),
        policy_aging_get()
    };
    int n_policies = 5;

    BeladyResult *results[5];
    for (int i = 0; i < n_policies; i++) {
        results[i] = belady_detect(trace, policies[i], min_frames, max_frames);
    }

    printf("\n========================================================================================\n");
    printf(" Multi-Policy Fault Comparison across Frame Counts [%d .. %d]\n", min_frames, max_frames);
    printf(" Trace: %s (Total References: %lu)\n", trace->description, (unsigned long)trace->count);
    printf("========================================================================================\n");
    printf(" Frames |   FIFO   |   LRU    |  Clock   | Optimal  |  Aging   | Anomaly Note\n");
    printf("----------------------------------------------------------------------------------------\n");

    int num_points = max_frames - min_frames + 1;
    for (int p = 0; p < num_points; p++) {
        int f = min_frames + p;
        int fifo_f   = results[0]->fault_counts[p];
        int lru_f    = results[1]->fault_counts[p];
        int clock_f  = results[2]->fault_counts[p];
        int opt_f    = results[3]->fault_counts[p];
        int aging_f  = results[4]->fault_counts[p];

        const char *note = "";
        if (p > 0 && fifo_f > results[0]->fault_counts[p - 1]) {
            note = "<-- FIFO Anomaly!";
        }

        printf("  %5d | %8d | %8d | %8d | %8d | %8d | %s\n",
               f, fifo_f, lru_f, clock_f, opt_f, aging_f, note);
    }
    printf("========================================================================================\n");

    if (csv_export_path) {
        FILE *fp = fopen(csv_export_path, "w");
        if (fp) {
            fprintf(fp, "frames,fifo,lru,clock,optimal,aging\n");
            for (int p = 0; p < num_points; p++) {
                int f = min_frames + p;
                fprintf(fp, "%d,%d,%d,%d,%d,%d\n",
                        f,
                        results[0]->fault_counts[p],
                        results[1]->fault_counts[p],
                        results[2]->fault_counts[p],
                        results[3]->fault_counts[p],
                        results[4]->fault_counts[p]);
            }
            fclose(fp);
            printf(" [i] CSV export saved to: %s\n", csv_export_path);
        }
    }

    for (int i = 0; i < n_policies; i++) {
        belady_result_free(results[i]);
    }
}
