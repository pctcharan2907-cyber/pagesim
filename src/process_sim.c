#include "process_sim.h"
#include "policy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

MultiProcessSim *proc_sim_create(int num_processes, int total_frames, AllocationScope scope, int quantum)
{
    if (num_processes <= 0 || total_frames <= 0) return NULL;
    if (num_processes > MAX_SIM_PROCESSES) num_processes = MAX_SIM_PROCESSES;

    MultiProcessSim *psim = (MultiProcessSim *)calloc(1, sizeof(MultiProcessSim));
    if (!psim) return NULL;

    psim->num_processes = num_processes;
    psim->scope = scope;
    psim->quantum = (quantum > 0) ? quantum : 4;
    psim->current_proc_idx = 0;
    psim->thrashing_detected = false;

    PageSimConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.num_frames = total_frames;
    cfg.page_size = 4096;
    cfg.dirty_aware = true;
    cfg.track_timeline = false;

    psim->pagesim = pagesim_create(&cfg, policy_lru_get());

    int per_proc_frames = total_frames / num_processes;
    if (per_proc_frames == 0) per_proc_frames = 1;

    for (int i = 0; i < num_processes; i++) {
        psim->processes[i].pid = i + 1;
        snprintf(psim->processes[i].name, sizeof(psim->processes[i].name), "proc_%d", i + 1);
        psim->processes[i].pagetable = mmu_pagetable_create(i + 1, 1024);
        psim->processes[i].max_frames_limit = per_proc_frames;
        psim->processes[i].resident_frames_count = 0;
        psim->processes[i].faults = 0;
        psim->processes[i].accesses = 0;
    }

    return psim;
}

void proc_sim_destroy(MultiProcessSim *psim)
{
    if (!psim) return;

    for (int i = 0; i < psim->num_processes; i++) {
        if (psim->processes[i].pagetable) {
            mmu_pagetable_destroy(psim->processes[i].pagetable);
            psim->processes[i].pagetable = NULL;
        }
    }

    if (psim->pagesim) {
        pagesim_destroy(psim->pagesim);
        psim->pagesim = NULL;
    }

    free(psim);
}

void proc_sim_run_traces(MultiProcessSim *psim, Trace **traces, int num_traces)
{
    if (!psim || !traces || num_traces <= 0) return;

    size_t *cursors = (size_t *)calloc(num_traces, sizeof(size_t));
    if (!cursors) return;

    bool any_active = true;

    while (any_active) {
        any_active = false;

        for (int p = 0; p < psim->num_processes; p++) {
            if (p >= num_traces) continue;

            Trace *t = traces[p];
            size_t *cursor = &cursors[p];

            for (int q = 0; q < psim->quantum; q++) {
                if (*cursor < t->count) {
                    any_active = true;
                    PageRef ref = t->refs[*cursor];
                    ref.pid = psim->processes[p].pid;

                    psim->processes[p].accesses++;

                    SimStepResult res = pagesim_access(psim->pagesim, &ref, NULL, 0);
                    if (!res.is_hit) {
                        psim->processes[p].faults++;
                    }

                    (*cursor)++;
                } else {
                    break;
                }
            }
        }
    }

    free(cursors);

    /* Detect Thrashing: if global fault ratio > 65% under interleaved workload */
    pagesim_update_stats(psim->pagesim);
    if (psim->pagesim->stats.fault_ratio > 0.65) {
        psim->thrashing_detected = true;
    }
}

void proc_sim_print_report(const MultiProcessSim *psim)
{
    if (!psim) return;

    printf("\n======================================================================\n");
    printf(" Multi-Process Working Set & CPU Scheduling Interaction Report\n");
    printf(" Scope: %s | Quantum: %d steps | Total Physical Frames: %d\n",
           psim->scope == ALLOC_LOCAL ? "Local (Per-process partition)" : "Global (Dynamic sharing)",
           psim->quantum,
           psim->pagesim->config.num_frames);
    printf("======================================================================\n");
    printf(" PID | Process Name | Total Accesses | Page Faults | Fault Ratio\n");
    printf("----------------------------------------------------------------------\n");

    for (int i = 0; i < psim->num_processes; i++) {
        const SimProcess *p = &psim->processes[i];
        double fr = (p->accesses > 0) ? ((double)p->faults / (double)p->accesses) : 0.0;
        printf("  %2d | %-12s |       %8lu |    %8lu |      %6.2f%%\n",
               p->pid, p->name, (unsigned long)p->accesses, (unsigned long)p->faults, fr * 100.0);
    }
    printf("----------------------------------------------------------------------\n");
    printf(" Aggregate System Stats: %lu Accesses, %lu Faults (System Fault Rate: %.2f%%)\n",
           (unsigned long)psim->pagesim->stats.total_accesses,
           (unsigned long)psim->pagesim->stats.faults,
           psim->pagesim->stats.fault_ratio * 100.0);

    if (psim->thrashing_detected) {
        printf(" [!] WARNING: SYSTEM THRASHING DETECTED!\n");
        printf("     Combined working sets of active processes exceed total physical frames!\n");
        printf("     Recommendation: Suspend one or more processes (decrease multiprogramming degree).\n");
    } else {
        printf(" [*] System memory pressure is stable. No severe thrashing.\n");
    }
    printf("======================================================================\n\n");
}
