#ifndef WORKING_SET_H
#define WORKING_SET_H

#include "pagesim.h"
#include "trace.h"
#include "policy.h"

/* Structure to hold working set statistics across window sizes Delta */
typedef struct WorkingSetCurve {
    int min_delta;
    int max_delta;
    int step_delta;
    int num_points;
    int *deltas;
    double *avg_working_set_sizes;
    int *max_working_set_sizes;
    int *min_working_set_sizes;
} WorkingSetCurve;

/* Resident Set Size vs Faults curve */
typedef struct ResidentSetCurve {
    const char *policy_name;
    int min_frames;
    int max_frames;
    int num_points;
    int *frames;
    int *faults;
    double *hit_ratios;
} ResidentSetCurve;

/* Calculate working set size W(t, delta) at logical time t */
int working_set_size_at(const Trace *trace, size_t t, int delta);

/* Compute working set curve as delta varies */
WorkingSetCurve *working_set_compute_curve(const Trace *trace, int min_delta, int max_delta, int step_delta);
void working_set_curve_free(WorkingSetCurve *curve);
void working_set_curve_print(const WorkingSetCurve *curve);

/* Compute resident set size vs faults curve */
ResidentSetCurve *working_set_fault_curve(const Trace *trace, PageReplacementPolicy *policy, int min_frames, int max_frames);
void resident_set_curve_free(ResidentSetCurve *curve);
void resident_set_curve_print(const ResidentSetCurve *curve);

/* Export working-set curves to CSV for plotting */
int working_set_export_csv(const Trace *trace, int min_frames, int max_frames, const char *csv_filename);

#endif /* WORKING_SET_H */
