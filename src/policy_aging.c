#include "policy.h"

static void aging_init(PageSim *sim)
{
    for (int i = 0; i < sim->config.num_frames; i++) {
        sim->frames[i].age_register = 0;
    }
}

static int aging_select_victim(PageSim *sim, int new_page_id, const PageRef *future_refs, size_t future_len)
{
    (void)new_page_id;
    (void)future_refs;
    (void)future_len;

    int victim = -1;
    uint8_t min_age = 0xFF;

    for (int i = 0; i < sim->config.num_frames; i++) {
        if (!sim->frames[i].occupied) {
            return i;
        }

        if (sim->frames[i].age_register < min_age) {
            min_age = sim->frames[i].age_register;
            victim = i;
        }
    }

    return (victim >= 0) ? victim : 0;
}

static void aging_on_access(PageSim *sim, int frame_id, bool is_write)
{
    (void)is_write;
    sim->frames[frame_id].referenced = true;
}

static void aging_on_page_in(PageSim *sim, int frame_id, int page_id, bool is_write)
{
    (void)page_id;
    (void)is_write;
    sim->frames[frame_id].referenced = true;
    sim->frames[frame_id].age_register = 0x80; /* Highest priority for brand new page */
}

static void aging_on_timer_tick(PageSim *sim)
{
    /* Clock tick: shift aging registers right by 1, insert reference bit into MSB */
    for (int i = 0; i < sim->config.num_frames; i++) {
        Frame *f = &sim->frames[i];
        if (f->occupied) {
            f->age_register >>= 1;
            if (f->referenced) {
                f->age_register |= 0x80;
                f->referenced = false;
            }
        }
    }
}

static PageReplacementPolicy aging_policy = {
    .name = "Aging",
    .description = "Aging / NFU algorithm. Periodic reference-bit shift-register LRU approximation.",
    .is_stack_algorithm = false,
    .init = aging_init,
    .select_victim = aging_select_victim,
    .on_access = aging_on_access,
    .on_page_in = aging_on_page_in,
    .on_timer_tick = aging_on_timer_tick,
    .destroy = NULL
};

PageReplacementPolicy *policy_aging_get(void)
{
    return &aging_policy;
}
