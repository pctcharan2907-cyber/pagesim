#ifndef POLICY_H
#define POLICY_H

#include "pagesim.h"

/* Generic interface for page replacement algorithms */
typedef struct PageReplacementPolicy {
    const char *name;
    const char *description;
    bool is_stack_algorithm; /* Stack algorithms (LRU, Optimal) never suffer Bélády's anomaly */

    /* Initialize policy-specific structures */
    void (*init)(PageSim *sim);

    /* Select victim frame index (0 .. num_frames - 1).
     * future_refs and future_len provide lookahead for Optimal (MIN) policy. */
    int (*select_victim)(PageSim *sim, int new_page_id, const PageRef *future_refs, size_t future_len);

    /* Called on page hit or after page-in */
    void (*on_access)(PageSim *sim, int frame_id, bool is_write);

    /* Called when a page is mapped into a frame */
    void (*on_page_in)(PageSim *sim, int frame_id, int page_id, bool is_write);

    /* Called periodically to simulate timer interrupts (Clock reference bit reset, Aging shift) */
    void (*on_timer_tick)(PageSim *sim);

    /* Free policy-specific data */
    void (*destroy)(PageSim *sim);
} PageReplacementPolicy;

/* Policy constructors */
PageReplacementPolicy *policy_fifo_get(void);
PageReplacementPolicy *policy_lru_get(void);
PageReplacementPolicy *policy_clock_get(void);
PageReplacementPolicy *policy_optimal_get(void);
PageReplacementPolicy *policy_aging_get(void);

/* Policy registry helpers */
PageReplacementPolicy *policy_find_by_name(const char *name);
void policy_print_all(void);
int policy_get_count(void);
PageReplacementPolicy *policy_get_by_index(int index);

#endif /* POLICY_H */
