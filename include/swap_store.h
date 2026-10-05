#ifndef SWAP_STORE_H
#define SWAP_STORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define SWAP_MAX_SLOTS 4096
#define SWAP_PAGE_SIZE 4096

typedef struct SwapSlot {
    bool in_use;
    int page_id;
    int pid;
    uint8_t data[SWAP_PAGE_SIZE];
} SwapSlot;

typedef struct SwapStore {
    size_t total_slots;
    size_t used_slots;
    uint64_t total_writes;
    uint64_t total_reads;
    uint64_t clean_drops_avoided;
    char backing_file_path[256];
    bool use_disk_file;
    SwapSlot *slots;
} SwapStore;

SwapStore *swap_store_create(size_t total_slots, const char *backing_file);
void swap_store_destroy(SwapStore *store);

int swap_store_alloc_slot(SwapStore *store, int page_id, int pid);
void swap_store_free_slot(SwapStore *store, int slot_id);

/* Page-out: writes data to slot. If is_dirty is false and page is clean, can drop without I/O */
bool swap_store_page_out(SwapStore *store, int slot_id, const void *page_data, bool is_dirty);

/* Page-in: reads data from slot */
bool swap_store_page_in(SwapStore *store, int slot_id, void *out_data);

void swap_store_print_stats(const SwapStore *store);

#endif /* SWAP_STORE_H */
