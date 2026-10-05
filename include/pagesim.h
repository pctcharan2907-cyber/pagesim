#ifndef PAGESIM_H
#define PAGESIM_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define PAGESIM_VERSION "1.0.0"
#define DEFAULT_FRAME_COUNT 4
#define DEFAULT_PAGE_SIZE 4096
#define MAX_FRAMES 1024
#define MAX_PAGES 65536

/* Memory access types */
typedef enum {
    ACCESS_READ  = 'R',
    ACCESS_WRITE = 'W',
    ACCESS_EXEC  = 'X'
} AccessType;

/* Single page reference in a reference string or memory trace */
typedef struct PageRef {
    int page_id;               /* Virtual Page Number (VPN) */
    AccessType access_type;    /* 'R', 'W', or 'X' */
    uint64_t virtual_addr;     /* Full virtual address (if derived from address trace) */
    int pid;                   /* Process ID (for multi-process traces) */
    uint64_t timestamp;        /* Reference sequence number / logical time */
} PageRef;

/* Physical Frame entry in the frame table */
typedef struct Frame {
    int frame_id;              /* Physical Frame Number (PFN) */
    bool occupied;             /* true if allocated to a page */
    int page_id;               /* Virtual page number mapped here */
    int pid;                   /* Owner process ID */
    bool dirty;                /* Modified bit (PTE_D) */
    bool referenced;           /* Reference bit (PTE_A / second-chance bit) */
    uint64_t load_time;        /* Timestamp when brought into frame (FIFO) */
    uint64_t last_access_time; /* Timestamp of most recent access (LRU) */
    uint8_t age_register;      /* Shift register for Aging / NFU policy */
    int swap_slot;             /* Backing store slot (-1 if none) */
} Frame;

/* Simulation event result for a single reference */
typedef struct SimStepResult {
    int page_id;
    bool is_hit;
    int frame_id;              /* Frame accessed or newly allocated */
    int victim_frame_id;       /* -1 if no eviction occurred */
    int victim_page_id;        /* -1 if no victim */
    bool was_dirty_eviction;   /* true if victim had to be written back */
    bool was_clean_drop;       /* true if victim was dropped without disk write */
} SimStepResult;

/* Aggregate simulation statistics */
typedef struct SimStats {
    uint64_t total_accesses;
    uint64_t hits;
    uint64_t faults;
    uint64_t reads;
    uint64_t writes;
    uint64_t dirty_page_outs;  /* Backing store writes */
    uint64_t clean_page_drops; /* Zero I/O drops */
    uint64_t page_ins;         /* Backing store reads */
    double hit_ratio;
    double fault_ratio;
} SimStats;

/* Simulator configuration */
typedef struct PageSimConfig {
    int num_frames;
    int page_size;
    bool dirty_aware;          /* Enable dirty-bit awareness (clean drop optimization) */
    bool verbose;              /* Print per-step details */
    bool track_timeline;       /* Record frame occupancy matrix for visualizer */
    int timer_interval;        /* Frequency of timer ticks for Aging/Clock reset */
} PageSimConfig;

/* Forward declarations */
struct PageReplacementPolicy;
struct VisualTimeline;

/* Main simulator context */
typedef struct PageSim {
    PageSimConfig config;
    Frame *frames;
    SimStats stats;
    uint64_t current_time;
    struct PageReplacementPolicy *policy;
    void *policy_data;         /* Policy-specific state (queues, rings, etc.) */
    struct VisualTimeline *timeline;
} PageSim;

/* Core Simulator Lifecycle API */
PageSim *pagesim_create(const PageSimConfig *config, struct PageReplacementPolicy *policy);
void pagesim_destroy(PageSim *sim);
void pagesim_reset(PageSim *sim);

/* Reference Replay API */
SimStepResult pagesim_access(PageSim *sim, const PageRef *ref, const PageRef *future_refs, size_t future_len);
void pagesim_replay(PageSim *sim, const PageRef *refs, size_t count);

/* Helpers */
int pagesim_find_frame_with_page(const PageSim *sim, int page_id, int pid);
int pagesim_find_free_frame(const PageSim *sim);
void pagesim_update_stats(PageSim *sim);
void pagesim_print_stats(const PageSim *sim, const char *policy_label);

#endif /* PAGESIM_H */
