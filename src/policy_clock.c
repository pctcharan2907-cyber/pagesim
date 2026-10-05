#include "policy.h"

typedef struct ClockData {
    int hand;
} ClockData;

static void clock_init(PageSim *sim)
{
    ClockData *data = (ClockData *)malloc(sizeof(ClockData));
    if (data) {
        data->hand = 0;
        sim->policy_data = data;
    }
}

static int clock_select_victim(PageSim *sim, int new_page_id, const PageRef *future_refs, size_t future_len)
{
    (void)new_page_id;
    (void)future_refs;
    (void)future_len;

    ClockData *data = (ClockData *)sim->policy_data;
    if (!data) return 0;

    int n = sim->config.num_frames;
    int inspected = 0;

    while (inspected < 2 * n + 2) {
        int current_frame = data->hand;
        Frame *f = &sim->frames[current_frame];

        if (!f->occupied) {
            data->hand = (current_frame + 1) % n;
            return current_frame;
        }

        if (f->referenced) {
            /* Second chance: clear reference bit and advance hand */
            f->referenced = false;
            data->hand = (current_frame + 1) % n;
        } else {
            /* Victim found: zero reference bit */
            data->hand = (current_frame + 1) % n;
            return current_frame;
        }

        inspected++;
    }

    /* Fallback */
    int victim = data->hand;
    data->hand = (data->hand + 1) % n;
    return victim;
}

static void clock_on_timer_tick(PageSim *sim)
{
    /* Simulate hardware timer interrupt: periodic reference-bit clear (Milestone 6) */
    for (int i = 0; i < sim->config.num_frames; i++) {
        sim->frames[i].referenced = false;
    }
}

static void clock_destroy(PageSim *sim)
{
    if (sim->policy_data) {
        free(sim->policy_data);
        sim->policy_data = NULL;
    }
}

static PageReplacementPolicy clock_policy = {
    .name = "Clock",
    .description = "Second-chance Clock replacement using a circular pointer and reference bit.",
    .is_stack_algorithm = false,
    .init = clock_init,
    .select_victim = clock_select_victim,
    .on_access = NULL,
    .on_page_in = NULL,
    .on_timer_tick = clock_on_timer_tick,
    .destroy = clock_destroy
};

PageReplacementPolicy *policy_clock_get(void)
{
    return &clock_policy;
}
