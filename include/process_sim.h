#ifndef PROCESS_SIM_H
#define PROCESS_SIM_H

#include "pagesim.h"
#include "trace.h"
#include "mmu.h"

#define MAX_SIM_PROCESSES 16

typedef enum {
    ALLOC_LOCAL,   /* Process can only replace its own frames */
    ALLOC_GLOBAL   /* Process can replace any frame in the system */
} AllocationScope;

typedef struct SimProcess {
    int pid;
    char name[32];
    PageTable *pagetable;
    int resident_frames_count;
    int max_frames_limit;      /* For local allocation */
    uint64_t faults;
    uint64_t accesses;
} SimProcess;

typedef struct MultiProcessSim {
    int num_processes;
    SimProcess processes[MAX_SIM_PROCESSES];
    PageSim *pagesim;
    AllocationScope scope;
    int quantum;               /* Steps per scheduling quantum */
    int current_proc_idx;
    bool thrashing_detected;
} MultiProcessSim;

MultiProcessSim *proc_sim_create(int num_processes, int total_frames, AllocationScope scope, int quantum);
void proc_sim_destroy(MultiProcessSim *psim);
void proc_sim_run_traces(MultiProcessSim *psim, Trace **traces, int num_traces);
void proc_sim_print_report(const MultiProcessSim *psim);

#endif /* PROCESS_SIM_H */
