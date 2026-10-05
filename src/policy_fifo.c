#include "policy.h"
#include <limits.h>

typedef struct FifoData {
    int next_victim_idx;
} FifoData;

static void fifo_init(PageSim *sim)
{
    FifoData *data = (FifoData *)malloc(sizeof(FifoData));
    if (data) {
        data->next_victim_idx = 0;
        sim->policy_data = data;
    }
}

static int fifo_select_victim(PageSim *sim, int new_page_id, const PageRef *future_refs, size_t future_len)
{
    (void)new_page_id;
    (void)future_refs;
    (void)future_len;

    /* Select the frame with the earliest load_time (First-In) */
    int victim = -1;
    uint64_t oldest_load_time = UINT64_MAX;

    for (int i = 0; i < sim->config.num_frames; i++) {
        if (!sim->frames[i].occupied) {
            return i;
        }
        if (sim->frames[i].load_time < oldest_load_time) {
            oldest_load_time = sim->frames[i].load_time;
            victim = i;
        }
    }

    return (victim >= 0) ? victim : 0;
}

static void fifo_destroy(PageSim *sim)
{
    if (sim->policy_data) {
        free(sim->policy_data);
        sim->policy_data = NULL;
    }
}

static PageReplacementPolicy fifo_policy = {
    .name = "FIFO",
    .description = "First-In, First-Out page replacement. Replaces the oldest loaded page.",
    .is_stack_algorithm = false, /* Subject to Bélády's Anomaly! */
    .init = fifo_init,
    .select_victim = fifo_select_victim,
    .on_access = NULL,
    .on_page_in = NULL,
    .on_timer_tick = NULL,
    .destroy = fifo_destroy
};

PageReplacementPolicy *policy_fifo_get(void)
{
    return &fifo_policy;
}
