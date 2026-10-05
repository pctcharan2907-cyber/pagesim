#ifndef VISUALIZER_H
#define VISUALIZER_H

#include "pagesim.h"
#include <stdio.h>

/* Per-step snapshot of physical frame occupancy */
typedef struct TimelineStep {
    uint64_t step_index;
    int page_id;
    AccessType access_type;
    bool is_hit;
    int frame_accessed;
    int victim_frame;
    int victim_page;
    bool was_dirty;
    int *frame_pages;          /* Array of size num_frames: page in each frame (-1 if empty) */
    bool *frame_dirty;         /* Dirty bit per frame */
    bool *frame_ref;           /* Reference bit per frame */
} TimelineStep;

/* Visual Timeline collector */
typedef struct VisualTimeline {
    int num_frames;
    size_t capacity;
    size_t count;
    TimelineStep *steps;
} VisualTimeline;

VisualTimeline *visualizer_timeline_create(int num_frames, size_t capacity);
void visualizer_timeline_free(VisualTimeline *vt);
void visualizer_timeline_record(VisualTimeline *vt, const SimStepResult *result, const Frame *frames, int num_frames, AccessType type, uint64_t step);

/* Terminal UI rendering */
void visualizer_render_timeline(const VisualTimeline *vt, size_t max_steps_to_show, FILE *out);
void visualizer_render_scorecard(const SimStats *stats_array, const char **policy_names, int count, FILE *out);
void visualizer_render_ascii_plot(const int *x_vals, const int *y_vals, int count, const char *title, const char *x_label, const char *y_label, FILE *out);

#endif /* VISUALIZER_H */
