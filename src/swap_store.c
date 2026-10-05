#include "swap_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SwapStore *swap_store_create(size_t total_slots, const char *backing_file)
{
    if (total_slots == 0) total_slots = 1024;

    SwapStore *store = (SwapStore *)malloc(sizeof(SwapStore));
    if (!store) return NULL;

    store->total_slots = total_slots;
    store->used_slots = 0;
    store->total_writes = 0;
    store->total_reads = 0;
    store->clean_drops_avoided = 0;
    store->use_disk_file = (backing_file != NULL && strlen(backing_file) > 0);

    if (store->use_disk_file) {
        strncpy(store->backing_file_path, backing_file, sizeof(store->backing_file_path) - 1);
        store->backing_file_path[sizeof(store->backing_file_path) - 1] = '\0';
    } else {
        store->backing_file_path[0] = '\0';
    }

    store->slots = (SwapSlot *)calloc(total_slots, sizeof(SwapSlot));
    if (!store->slots) {
        free(store);
        return NULL;
    }

    return store;
}

void swap_store_destroy(SwapStore *store)
{
    if (!store) return;
    if (store->slots) {
        free(store->slots);
        store->slots = NULL;
    }
    free(store);
}

int swap_store_alloc_slot(SwapStore *store, int page_id, int pid)
{
    if (!store) return -1;

    for (size_t i = 0; i < store->total_slots; i++) {
        if (!store->slots[i].in_use) {
            store->slots[i].in_use = true;
            store->slots[i].page_id = page_id;
            store->slots[i].pid = pid;
            store->used_slots++;
            return (int)i;
        }
    }

    return -1; /* Out of swap space */
}

void swap_store_free_slot(SwapStore *store, int slot_id)
{
    if (!store || slot_id < 0 || (size_t)slot_id >= store->total_slots) return;

    if (store->slots[slot_id].in_use) {
        store->slots[slot_id].in_use = false;
        store->slots[slot_id].page_id = -1;
        store->slots[slot_id].pid = 0;
        if (store->used_slots > 0) store->used_slots--;
    }
}

bool swap_store_page_out(SwapStore *store, int slot_id, const void *page_data, bool is_dirty)
{
    if (!store || slot_id < 0 || (size_t)slot_id >= store->total_slots) return false;

    if (!is_dirty) {
        /* Clean page optimization: drop without writing to disk backing store */
        store->clean_drops_avoided++;
        return true;
    }

    /* Dirty page: must write back to backing store */
    store->total_writes++;
    if (page_data) {
        memcpy(store->slots[slot_id].data, page_data, SWAP_PAGE_SIZE);
    }

    return true;
}

bool swap_store_page_in(SwapStore *store, int slot_id, void *out_data)
{
    if (!store || slot_id < 0 || (size_t)slot_id >= store->total_slots) return false;
    if (!store->slots[slot_id].in_use) return false;

    store->total_reads++;
    if (out_data) {
        memcpy(out_data, store->slots[slot_id].data, SWAP_PAGE_SIZE);
    }

    return true;
}

void swap_store_print_stats(const SwapStore *store)
{
    if (!store) return;

    printf("\n+-------------------------------------------------------+\n");
    printf("| Backing Store (Swap) Performance Statistics           |\n");
    printf("+-------------------------------------------------------+\n");
    printf("| Capacity Slots:       %-31lu |\n", (unsigned long)store->total_slots);
    printf("| Currently Used Slots: %-31lu |\n", (unsigned long)store->used_slots);
    printf("| Swap Writes (PageOut):%-31lu |\n", (unsigned long)store->total_writes);
    printf("| Swap Reads (PageIn):  %-31lu |\n", (unsigned long)store->total_reads);
    printf("| Clean Drops (Saved IO): %-29lu |\n", (unsigned long)store->clean_drops_avoided);
    printf("+-------------------------------------------------------+\n\n");
}
