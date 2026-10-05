#include "working_set.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int working_set_size_at(const Trace *trace, size_t t, int delta)
{
    if (!trace || trace->count == 0 || delta <= 0 || t >= trace->count) {
        return 0;
    }

    size_t start = (t >= (size_t)delta) ? (t - (size_t)delta + 1) : 0;
    size_t window_len = t - start + 1;

    int unique_pages[512];
    int unique_count = 0;

    for (size_t i = start; i <= t; i++) {
        int page = trace->refs[i].page_id;
        bool already_seen = false;

        for (int u = 0; u < unique_count; u++) {
            if (unique_pages[u] == page) {
                already_seen = true;
                break;
            }
        }

        if (!already_seen && unique_count < 512) {
            unique_pages[unique_count++] = page;
        }
    }

    (void)window_len;
    return unique_count;
}

WorkingSetCurve *working_set_compute_curve(const Trace *trace, int min_delta, int max_delta, int step_delta)
{
    if (!trace || trace->count == 0 || min_delta <= 0 || max_delta < min_delta || step_delta <= 0) {
        return NULL;
    }

    int num_points = (max_delta - min_delta) / step_delta + 1;
    WorkingSetCurve *curve = (WorkingSetCurve *)malloc(sizeof(WorkingSetCurve));
    if (!curve) return NULL;

    curve->min_delta = min_delta;
    curve->max_delta = max_delta;
    curve->step_delta = step_delta;
    curve->num_points = num_points;

    curve->deltas = (int *)malloc(num_points * sizeof(int));
    curve->avg_working_set_sizes = (double *)malloc(num_points * sizeof(double));
    curve->max_working_set_sizes = (int *)malloc(num_points * sizeof(int));
    curve->min_working_set_sizes = (int *)malloc(num_points * sizeof(int));

    for (int idx = 0; idx < num_points; idx++) {
        int delta = min_delta + idx * step_delta;
        curve->deltas[idx] = delta;

        uint64_t sum_sizes = 0;
        int max_s = 0;
        int min_s = 999999;

        for (size_t t = 0; t < trace->count; t++) {
            int ws = working_set_size_at(trace, t, delta);
            sum_sizes += ws;
            if (ws > max_s) max_s = ws;
            if (ws < min_s) min_s = ws;
        }

        curve->avg_working_set_sizes[idx] = (double)sum_sizes / (double)trace->count;
        curve->max_working_set_sizes[idx] = max_s;
        curve->min_working_set_sizes[idx] = (min_s == 999999) ? 0 : min_s;
    }

    return curve;
}

void working_set_curve_free(WorkingSetCurve *curve)
{
    if (!curve) return;
    if (curve->deltas) free(curve->deltas);
    if (curve->avg_working_set_sizes) free(curve->avg_working_set_sizes);
    if (curve->max_working_set_sizes) free(curve->max_working_set_sizes);
    if (curve->min_working_set_sizes) free(curve->min_working_set_sizes);
    free(curve);
}

void working_set_curve_print(const WorkingSetCurve *curve)
{
    if (!curve) return;

    printf("\n============================================================\n");
    printf(" Working-Set Model: Window Size (Delta) vs Working Set Size W(t, Delta)\n");
    printf("============================================================\n");
    printf(" Window Delta |  Avg W(t) Pages | Min Pages | Max Pages\n");
    printf("------------------------------------------------------------\n");

    for (int i = 0; i < curve->num_points; i++) {
        printf("    %8d  |      %8.2f   |  %7d  |  %7d\n",
               curve->deltas[i],
               curve->avg_working_set_sizes[i],
               curve->min_working_set_sizes[i],
               curve->max_working_set_sizes[i]);
    }
    printf("============================================================\n");
}

ResidentSetCurve *working_set_fault_curve(const Trace *trace, PageReplacementPolicy *policy, int min_frames, int max_frames)
{
    if (!trace || !policy || min_frames <= 0 || max_frames < min_frames) {
        return NULL;
    }

    int num_points = max_frames - min_frames + 1;
    ResidentSetCurve *curve = (ResidentSetCurve *)malloc(sizeof(ResidentSetCurve));
    if (!curve) return NULL;

    curve->policy_name = policy->name;
    curve->min_frames = min_frames;
    curve->max_frames = max_frames;
    curve->num_points = num_points;
    curve->frames = (int *)malloc(num_points * sizeof(int));
    curve->faults = (int *)malloc(num_points * sizeof(int));
    curve->hit_ratios = (double *)malloc(num_points * sizeof(double));

    for (int i = 0; i < num_points; i++) {
        int f = min_frames + i;
        curve->frames[i] = f;

        PageSimConfig cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.num_frames = f;
        cfg.page_size = 4096;
        cfg.track_timeline = false;
        cfg.dirty_aware = true;

        PageSim *sim = pagesim_create(&cfg, policy);
        if (sim) {
            pagesim_replay(sim, trace->refs, trace->count);
            curve->faults[i] = (int)sim->stats.faults;
            curve->hit_ratios[i] = sim->stats.hit_ratio;
            pagesim_destroy(sim);
        } else {
            curve->faults[i] = 0;
            curve->hit_ratios[i] = 0.0;
        }
    }

    return curve;
}

void resident_set_curve_free(ResidentSetCurve *curve)
{
    if (!curve) return;
    if (curve->frames) free(curve->frames);
    if (curve->faults) free(curve->faults);
    if (curve->hit_ratios) free(curve->hit_ratios);
    free(curve);
}

void resident_set_curve_print(const ResidentSetCurve *curve)
{
    if (!curve) return;

    printf("\n============================================================\n");
    printf(" Resident Set Size vs Fault Curve (Working Set Knee Analysis)\n");
    printf(" Policy: %s\n", curve->policy_name);
    printf("============================================================\n");
    printf(" Frames (m) | Page Faults | Fault Ratio | Hit Ratio\n");
    printf("------------------------------------------------------------\n");

    int prev_faults = 0;
    int knee_frame = -1;
    double max_drop = 0.0;

    for (int i = 0; i < curve->num_points; i++) {
        double fr = 1.0 - curve->hit_ratios[i];
        printf("   %6d   |   %8d  |   %6.2f%%   |  %6.2f%%\n",
               curve->frames[i],
               curve->faults[i],
               fr * 100.0,
               curve->hit_ratios[i] * 100.0);

        if (i > 0) {
            double drop = (double)(prev_faults - curve->faults[i]);
            if (drop > max_drop) {
                max_drop = drop;
                knee_frame = curve->frames[i];
            }
        }
        prev_faults = curve->faults[i];
    }
    printf("------------------------------------------------------------\n");
    if (knee_frame > 0) {
        printf(" [*] Working Set Knee detected at approx %d frames\n", knee_frame);
        printf("     (allocating frames beyond this point yields steep diminishing returns)\n");
    }
    printf("============================================================\n");
}

int working_set_export_csv(const Trace *trace, int min_frames, int max_frames, const char *csv_filename)
{
    if (!trace || !csv_filename) return -1;

    FILE *fp = fopen(csv_filename, "w");
    if (!fp) return -1;

    fprintf(fp, "frames,fifo_faults,lru_faults,clock_faults,opt_faults,fifo_hit_pct,lru_hit_pct,clock_hit_pct,opt_hit_pct\n");

    PageReplacementPolicy *fifo = policy_fifo_get();
    PageReplacementPolicy *lru  = policy_lru_get();
    PageReplacementPolicy *clk  = policy_clock_get();
    PageReplacementPolicy *opt  = policy_optimal_get();

    for (int f = min_frames; f <= max_frames; f++) {
        PageSimConfig cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.num_frames = f;
        cfg.dirty_aware = true;

        PageSim *s_fifo = pagesim_create(&cfg, fifo);
        PageSim *s_lru  = pagesim_create(&cfg, lru);
        PageSim *s_clk  = pagesim_create(&cfg, clk);
        PageSim *s_opt  = pagesim_create(&cfg, opt);

        pagesim_replay(s_fifo, trace->refs, trace->count);
        pagesim_replay(s_lru,  trace->refs, trace->count);
        pagesim_replay(s_clk,  trace->refs, trace->count);
        pagesim_replay(s_opt,  trace->refs, trace->count);

        fprintf(fp, "%d,%lu,%lu,%lu,%lu,%.2f,%.2f,%.2f,%.2f\n",
                f,
                (unsigned long)s_fifo->stats.faults,
                (unsigned long)s_lru->stats.faults,
                (unsigned long)s_clk->stats.faults,
                (unsigned long)s_opt->stats.faults,
                s_fifo->stats.hit_ratio * 100.0,
                s_lru->stats.hit_ratio * 100.0,
                s_clk->stats.hit_ratio * 100.0,
                s_opt->stats.hit_ratio * 100.0);

        pagesim_destroy(s_fifo);
        pagesim_destroy(s_lru);
        pagesim_destroy(s_clk);
        pagesim_destroy(s_opt);
    }

    fclose(fp);
    return 0;
}
