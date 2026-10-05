#include "policy.h"
#include <limits.h>

static int lru_select_victim(PageSim *sim, int new_page_id, const PageRef *future_refs, size_t future_len)
{
    (void)new_page_id;
    (void)future_refs;
    (void)future_len;

    int victim = -1;
    uint64_t oldest_access_time = UINT64_MAX;

    for (int i = 0; i < sim->config.num_frames; i++) {
        if (!sim->frames[i].occupied) {
            return i;
        }
        if (sim->frames[i].last_access_time < oldest_access_time) {
            oldest_access_time = sim->frames[i].last_access_time;
            victim = i;
        }
    }

    return (victim >= 0) ? victim : 0;
}

static PageReplacementPolicy lru_policy = {
    .name = "LRU",
    .description = "Least Recently Used page replacement. Replaces the page unreferenced for longest time.",
    .is_stack_algorithm = true, /* Stack algorithm: Provably immune to Bélády's anomaly */
    .init = NULL,
    .select_victim = lru_select_victim,
    .on_access = NULL,
    .on_page_in = NULL,
    .on_timer_tick = NULL,
    .destroy = NULL
};

PageReplacementPolicy *policy_lru_get(void)
{
    return &lru_policy;
}
