#ifndef BELADY_H
#define BELADY_H

#include "pagesim.h"
#include "policy.h"
#include "trace.h"

/* Result of running Bélády's anomaly detection */
typedef struct BeladyResult {
    const char *policy_name;
    bool is_stack_algorithm;
    int min_frames;
    int max_frames;
    int *fault_counts;         /* Array of size (max_frames - min_frames + 1) */
    double *hit_ratios;
    bool anomaly_detected;
    int anomalous_frame_f1;    /* Lower frame count */
    int anomalous_frame_f2;    /* Higher frame count where faults INCREASED */
    int faults_at_f1;
    int faults_at_f2;
} BeladyResult;

/* Detect Bélády's anomaly for a given policy and trace across a frame range */
BeladyResult *belady_detect(const Trace *trace, PageReplacementPolicy *policy, int min_frames, int max_frames);
void belady_result_free(BeladyResult *result);
void belady_result_print(const BeladyResult *result);

/* Run Bélády analysis across all registered policies */
void belady_compare_all(const Trace *trace, int min_frames, int max_frames, const char *csv_export_path);

#endif /* BELADY_H */
