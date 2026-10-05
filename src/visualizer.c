#include "visualizer.h"
#include <stdlib.h>
#include <string.h>

VisualTimeline *visualizer_timeline_create(int num_frames, size_t capacity)
{
    if (num_frames <= 0 || capacity == 0) return NULL;

    VisualTimeline *vt = (VisualTimeline *)malloc(sizeof(VisualTimeline));
    if (!vt) return NULL;

    vt->num_frames = num_frames;
    vt->capacity = capacity;
    vt->count = 0;
    vt->steps = (TimelineStep *)calloc(capacity, sizeof(TimelineStep));
    if (!vt->steps) {
        free(vt);
        return NULL;
    }

    return vt;
}

void visualizer_timeline_free(VisualTimeline *vt)
{
    if (!vt) return;
    if (vt->steps) {
        for (size_t i = 0; i < vt->count; i++) {
            if (vt->steps[i].frame_pages) free(vt->steps[i].frame_pages);
            if (vt->steps[i].frame_dirty) free(vt->steps[i].frame_dirty);
            if (vt->steps[i].frame_ref)   free(vt->steps[i].frame_ref);
        }
        free(vt->steps);
    }
    free(vt);
}

void visualizer_timeline_record(VisualTimeline *vt, const SimStepResult *res, const Frame *frames, int num_frames, AccessType type, uint64_t step)
{
    if (!vt || !res || !frames || vt->count >= vt->capacity) return;

    TimelineStep *ts = &vt->steps[vt->count];
    ts->step_index = step;
    ts->page_id = res->page_id;
    ts->access_type = type;
    ts->is_hit = res->is_hit;
    ts->frame_accessed = res->frame_id;
    ts->victim_frame = res->victim_frame_id;
    ts->victim_page = res->victim_page_id;
    ts->was_dirty = res->was_dirty_eviction;

    ts->frame_pages = (int *)malloc(num_frames * sizeof(int));
    ts->frame_dirty = (bool *)malloc(num_frames * sizeof(bool));
    ts->frame_ref   = (bool *)malloc(num_frames * sizeof(bool));

    for (int i = 0; i < num_frames; i++) {
        ts->frame_pages[i] = frames[i].occupied ? frames[i].page_id : -1;
        ts->frame_dirty[i] = frames[i].dirty;
        ts->frame_ref[i]   = frames[i].referenced;
    }

    vt->count++;
}

void visualizer_render_timeline(const VisualTimeline *vt, size_t max_steps_to_show, FILE *out)
{
    if (!vt || !out || vt->count == 0) return;

    size_t n = vt->count;
    if (max_steps_to_show > 0 && n > max_steps_to_show) {
        n = max_steps_to_show;
    }

    fprintf(out, "\n========================================================================================\n");
    fprintf(out, " Visual Timeline of Frame Occupancy over Reference String (Showing %lu / %lu steps)\n",
            (unsigned long)n, (unsigned long)vt->count);
    fprintf(out, "========================================================================================\n");

    /* Step numbers row */
    fprintf(out, " Step     : ");
    for (size_t i = 0; i < n; i++) {
        fprintf(out, "%3lu ", (unsigned long)vt->steps[i].step_index);
    }
    fprintf(out, "\n");

    /* Reference Page row */
    fprintf(out, " Page Ref : ");
    for (size_t i = 0; i < n; i++) {
        fprintf(out, "%3d ", vt->steps[i].page_id);
    }
    fprintf(out, "\n");

    /* Separator line */
    fprintf(out, "------------");
    for (size_t i = 0; i < n; i++) {
        fprintf(out, "----");
    }
    fprintf(out, "\n");

    /* Frame occupancy rows */
    for (int f = 0; f < vt->num_frames; f++) {
        fprintf(out, " Frame %2d : ", f);
        for (size_t i = 0; i < n; i++) {
            int p = vt->steps[i].frame_pages[f];
            if (p < 0) {
                fprintf(out, "  . ");
            } else {
                bool is_current = (vt->steps[i].frame_accessed == f);
                char dirty_flag = vt->steps[i].frame_dirty[f] ? '*' : ' ';
                if (is_current) {
                    fprintf(out, "[%d%c]", p, dirty_flag);
                } else {
                    fprintf(out, " %d%c ", p, dirty_flag);
                }
            }
        }
        fprintf(out, "\n");
    }

    /* Separator line */
    fprintf(out, "------------");
    for (size_t i = 0; i < n; i++) {
        fprintf(out, "----");
    }
    fprintf(out, "\n");

    /* Status row: Hit or Fault */
    fprintf(out, " Status   : ");
    for (size_t i = 0; i < n; i++) {
        if (vt->steps[i].is_hit) {
            fprintf(out, "  H ");
        } else {
            fprintf(out, "  F ");
        }
    }
    fprintf(out, "\n");

    /* Eviction / victim row */
    fprintf(out, " Eviction : ");
    for (size_t i = 0; i < n; i++) {
        if (!vt->steps[i].is_hit && vt->steps[i].victim_frame >= 0) {
            if (vt->steps[i].was_dirty) {
                fprintf(out, " dW "); /* Dirty Write */
            } else {
                fprintf(out, " cD "); /* Clean Drop */
            }
        } else {
            fprintf(out, "    ");
        }
    }
    fprintf(out, "\n");

    fprintf(out, "----------------------------------------------------------------------------------------\n");
    fprintf(out, " Legend: H = Hit, F = Fault, [x*] = Current access (dirty), dW = Dirty writeout, cD = Clean drop\n");
    fprintf(out, "========================================================================================\n\n");
}

void visualizer_render_scorecard(const SimStats *stats_array, const char **policy_names, int count, FILE *out)
{
    if (!stats_array || !policy_names || count <= 0 || !out) return;

    fprintf(out, "\n+-----------------------------------------------------------------------------------------+\n");
    fprintf(out, "|                               Policy Performance Scorecard                              |\n");
    fprintf(out, "+-----------------------------------------------------------------------------------------+\n");
    fprintf(out, "| Policy     | Accesses |   Hits   |  Faults  | Hit Ratio | Fault Ratio | Dirty Out | Clean Drop |\n");
    fprintf(out, "+------------+----------+----------+----------+-----------+-------------+-----------+------------+\n");

    for (int i = 0; i < count; i++) {
        const SimStats *s = &stats_array[i];
        fprintf(out, "| %-10s | %8lu | %8lu | %8lu |   %6.2f%% |     %6.2f%% |  %8lu |   %8lu |\n",
                policy_names[i],
                (unsigned long)s->total_accesses,
                (unsigned long)s->hits,
                (unsigned long)s->faults,
                s->hit_ratio * 100.0,
                s->fault_ratio * 100.0,
                (unsigned long)s->dirty_page_outs,
                (unsigned long)s->clean_page_drops);
    }

    fprintf(out, "+------------+----------+----------+----------+-----------+-------------+-----------+------------+\n\n");
}

void visualizer_render_ascii_plot(const int *x_vals, const int *y_vals, int count, const char *title, const char *x_label, const char *y_label, FILE *out)
{
    if (!x_vals || !y_vals || count <= 0 || !out) return;

    int max_y = 0;
    int min_y = 999999;
    for (int i = 0; i < count; i++) {
        if (y_vals[i] > max_y) max_y = y_vals[i];
        if (y_vals[i] < min_y) min_y = y_vals[i];
    }

    if (max_y == 0) max_y = 1;

    fprintf(out, "\n%s (ASCII Curve)\n", title);
    fprintf(out, " %s ^\n", y_label);

    int height = 10;
    for (int row = height; row >= 0; row--) {
        int threshold = min_y + (int)((double)row / (double)height * (max_y - min_y));
        fprintf(out, " %5d | ", threshold);

        for (int col = 0; col < count; col++) {
            if (y_vals[col] >= threshold) {
                fprintf(out, "  *  ");
            } else {
                fprintf(out, "     ");
            }
        }
        fprintf(out, "\n");
    }

    fprintf(out, "       +");
    for (int col = 0; col < count; col++) {
        fprintf(out, "-----");
    }
    fprintf(out, " > %s\n", x_label);

    fprintf(out, "         ");
    for (int col = 0; col < count; col++) {
        fprintf(out, " %3d ", x_vals[col]);
    }
    fprintf(out, "\n\n");
}
