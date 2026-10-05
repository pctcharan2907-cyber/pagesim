#include "policy.h"
#include <limits.h>

static int optimal_select_victim(PageSim *sim, int new_page_id, const PageRef *future_refs, size_t future_len)
{
    (void)new_page_id;

    int victim = -1;
    size_t furthest_next_use = 0;

    for (int i = 0; i < sim->config.num_frames; i++) {
        if (!sim->frames[i].occupied) {
            return i;
        }

        int page_in_frame = sim->frames[i].page_id;
        int pid_in_frame = sim->frames[i].pid;
        size_t next_use = SIZE_MAX;

        if (future_refs && future_len > 0) {
            for (size_t k = 0; k < future_len; k++) {
                if (future_refs[k].page_id == page_in_frame && future_refs[k].pid == pid_in_frame) {
                    next_use = k;
                    break;
                }
            }
        }

        /* If this page is NEVER used again, it is the best possible candidate */
        if (next_use == SIZE_MAX) {
            return i;
        }

        if (next_use > furthest_next_use) {
            furthest_next_use = next_use;
            victim = i;
        }
    }

    return (victim >= 0) ? victim : 0;
}

static PageReplacementPolicy optimal_policy = {
    .name = "Optimal",
    .description = "Bélády's MIN optimal replacement baseline. Replaces the page unreferenced for furthest future.",
    .is_stack_algorithm = true, /* Stack algorithm: Provably optimal lower-bound, immune to Bélády's anomaly */
    .init = NULL,
    .select_victim = optimal_select_victim,
    .on_access = NULL,
    .on_page_in = NULL,
    .on_timer_tick = NULL,
    .destroy = NULL
};

PageReplacementPolicy *policy_optimal_get(void)
{
    return &optimal_policy;
}
