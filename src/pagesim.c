#include "pagesim.h"
#include "policy.h"
#include "visualizer.h"
#include <string.h>

PageSim *pagesim_create(const PageSimConfig *config, PageReplacementPolicy *policy)
{
    if (!config || !policy || config->num_frames <= 0) {
        return NULL;
    }

    PageSim *sim = (PageSim *)calloc(1, sizeof(PageSim));
    if (!sim) {
        return NULL;
    }

    sim->config = *config;
    sim->policy = policy;
    sim->current_time = 0;

    sim->frames = (Frame *)calloc(sim->config.num_frames, sizeof(Frame));
    if (!sim->frames) {
        free(sim);
        return NULL;
    }

    for (int i = 0; i < sim->config.num_frames; i++) {
        sim->frames[i].frame_id = i;
        sim->frames[i].occupied = false;
        sim->frames[i].page_id = -1;
        sim->frames[i].pid = 0;
        sim->frames[i].dirty = false;
        sim->frames[i].referenced = false;
        sim->frames[i].load_time = 0;
        sim->frames[i].last_access_time = 0;
        sim->frames[i].age_register = 0;
        sim->frames[i].swap_slot = -1;
    }

    if (sim->config.track_timeline) {
        sim->timeline = visualizer_timeline_create(sim->config.num_frames, 1024);
    }

    if (sim->policy && sim->policy->init) {
        sim->policy->init(sim);
    }

    return sim;
}

void pagesim_destroy(PageSim *sim)
{
    if (!sim) {
        return;
    }

    if (sim->policy && sim->policy->destroy) {
        sim->policy->destroy(sim);
    }

    if (sim->timeline) {
        visualizer_timeline_free(sim->timeline);
        sim->timeline = NULL;
    }

    if (sim->frames) {
        free(sim->frames);
        sim->frames = NULL;
    }

    free(sim);
}

void pagesim_reset(PageSim *sim)
{
    if (!sim) {
        return;
    }

    if (sim->policy && sim->policy->destroy) {
        sim->policy->destroy(sim);
    }

    sim->current_time = 0;
    memset(&sim->stats, 0, sizeof(SimStats));

    for (int i = 0; i < sim->config.num_frames; i++) {
        sim->frames[i].occupied = false;
        sim->frames[i].page_id = -1;
        sim->frames[i].pid = 0;
        sim->frames[i].dirty = false;
        sim->frames[i].referenced = false;
        sim->frames[i].load_time = 0;
        sim->frames[i].last_access_time = 0;
        sim->frames[i].age_register = 0;
        sim->frames[i].swap_slot = -1;
    }

    if (sim->timeline) {
        visualizer_timeline_free(sim->timeline);
        sim->timeline = visualizer_timeline_create(sim->config.num_frames, 1024);
    }

    if (sim->policy && sim->policy->init) {
        sim->policy->init(sim);
    }
}

int pagesim_find_frame_with_page(const PageSim *sim, int page_id, int pid)
{
    if (!sim) return -1;
    for (int i = 0; i < sim->config.num_frames; i++) {
        if (sim->frames[i].occupied &&
            sim->frames[i].page_id == page_id &&
            sim->frames[i].pid == pid) {
            return i;
        }
    }
    return -1;
}

int pagesim_find_free_frame(const PageSim *sim)
{
    if (!sim) return -1;
    for (int i = 0; i < sim->config.num_frames; i++) {
        if (!sim->frames[i].occupied) {
            return i;
        }
    }
    return -1;
}

SimStepResult pagesim_access(PageSim *sim, const PageRef *ref, const PageRef *future_refs, size_t future_len)
{
    SimStepResult res;
    memset(&res, 0, sizeof(res));
    res.page_id = ref->page_id;
    res.victim_frame_id = -1;
    res.victim_page_id = -1;

    if (!sim || !ref) {
        return res;
    }

    sim->current_time++;
    sim->stats.total_accesses++;

    bool is_write = (ref->access_type == ACCESS_WRITE);
    if (is_write) {
        sim->stats.writes++;
    } else {
        sim->stats.reads++;
    }

    int existing_frame = pagesim_find_frame_with_page(sim, ref->page_id, ref->pid);

    if (existing_frame >= 0) {
        /* Page HIT */
        res.is_hit = true;
        res.frame_id = existing_frame;
        sim->stats.hits++;

        Frame *f = &sim->frames[existing_frame];
        f->last_access_time = sim->current_time;
        f->referenced = true;
        if (is_write) {
            f->dirty = true;
        }

        if (sim->policy && sim->policy->on_access) {
            sim->policy->on_access(sim, existing_frame, is_write);
        }
    } else {
        /* Page FAULT */
        res.is_hit = false;
        sim->stats.faults++;
        sim->stats.page_ins++;

        int target_frame = pagesim_find_free_frame(sim);

        if (target_frame >= 0) {
            /* Unoccupied frame available (cold start) */
            res.frame_id = target_frame;
        } else {
            /* Eviction required */
            int victim_frame = sim->policy->select_victim(sim, ref->page_id, future_refs, future_len);
            if (victim_frame < 0 || victim_frame >= sim->config.num_frames) {
                victim_frame = 0; /* Fallback safe */
            }

            Frame *v = &sim->frames[victim_frame];
            res.victim_frame_id = victim_frame;
            res.victim_page_id = v->page_id;

            if (v->dirty) {
                res.was_dirty_eviction = true;
                sim->stats.dirty_page_outs++;
            } else {
                res.was_clean_drop = true;
                sim->stats.clean_page_drops++;
            }

            target_frame = victim_frame;
            res.frame_id = target_frame;
        }

        /* Map new page into target frame */
        Frame *f = &sim->frames[target_frame];
        f->occupied = true;
        f->page_id = ref->page_id;
        f->pid = ref->pid;
        f->load_time = sim->current_time;
        f->last_access_time = sim->current_time;
        f->referenced = true;
        f->dirty = is_write;
        f->age_register = 0x80; /* Initial high age for Aging policy */

        if (sim->policy && sim->policy->on_page_in) {
            sim->policy->on_page_in(sim, target_frame, ref->page_id, is_write);
        }
    }

    /* Periodic timer tick simulation (for reference bit decay / Aging) */
    if (sim->config.timer_interval > 0 && (sim->current_time % (uint64_t)sim->config.timer_interval == 0)) {
        if (sim->policy && sim->policy->on_timer_tick) {
            sim->policy->on_timer_tick(sim);
        }
    }

    /* Record timeline snapshot if enabled */
    if (sim->timeline) {
        visualizer_timeline_record(sim->timeline, &res, sim->frames, sim->config.num_frames, ref->access_type, sim->current_time);
    }

    return res;
}

void pagesim_replay(PageSim *sim, const PageRef *refs, size_t count)
{
    if (!sim || !refs || count == 0) return;

    for (size_t i = 0; i < count; i++) {
        const PageRef *future = (i + 1 < count) ? &refs[i + 1] : NULL;
        size_t future_len = count - 1 - i;
        pagesim_access(sim, &refs[i], future, future_len);
    }

    pagesim_update_stats(sim);
}

void pagesim_update_stats(PageSim *sim)
{
    if (!sim) return;
    if (sim->stats.total_accesses > 0) {
        sim->stats.hit_ratio = (double)sim->stats.hits / (double)sim->stats.total_accesses;
        sim->stats.fault_ratio = (double)sim->stats.faults / (double)sim->stats.total_accesses;
    } else {
        sim->stats.hit_ratio = 0.0;
        sim->stats.fault_ratio = 0.0;
    }
}

void pagesim_print_stats(const PageSim *sim, const char *policy_label)
{
    if (!sim) return;

    const char *label = policy_label ? policy_label : (sim->policy ? sim->policy->name : "Unknown");
    printf("\n+-------------------------------------------------------+\n");
    printf("| Simulation Results: %-33s |\n", label);
    printf("+-------------------------------------------------------+\n");
    printf("| Total Frames:         %-31d |\n", sim->config.num_frames);
    printf("| Total Page Accesses:  %-31lu |\n", (unsigned long)sim->stats.total_accesses);
    printf("| Page Hits:            %-31lu |\n", (unsigned long)sim->stats.hits);
    printf("| Page Faults:          %-31lu |\n", (unsigned long)sim->stats.faults);
    printf("| Hit Ratio:            %6.2f%%                          |\n", sim->stats.hit_ratio * 100.0);
    printf("| Fault Ratio:          %6.2f%%                          |\n", sim->stats.fault_ratio * 100.0);
    printf("| Dirty Page-Outs:      %-31lu |\n", (unsigned long)sim->stats.dirty_page_outs);
    printf("| Clean Page Drops:     %-31lu |\n", (unsigned long)sim->stats.clean_page_drops);
    printf("+-------------------------------------------------------+\n");
}
