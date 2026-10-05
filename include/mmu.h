#ifndef MMU_H
#define MMU_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MMU_PAGE_SHIFT  12
#define MMU_PAGE_SIZE   (1 << MMU_PAGE_SHIFT) /* 4096 bytes */
#define MMU_PAGE_MASK   (MMU_PAGE_SIZE - 1)

/* RISC-V SV39 compatible Page Table Entry (PTE) flags */
#define PTE_V (1UL << 0) /* Valid: page is resident in physical memory */
#define PTE_R (1UL << 1) /* Readable */
#define PTE_W (1UL << 2) /* Writable */
#define PTE_X (1UL << 3) /* Executable */
#define PTE_U (1UL << 4) /* User-mode accessible */
#define PTE_G (1UL << 5) /* Global mapping */
#define PTE_A (1UL << 6) /* Accessed (set by hardware/OS on read/write) */
#define PTE_D (1UL << 7) /* Dirty (set by hardware/OS on write) */
#define PTE_S (1UL << 8) /* Swapped: custom supervisor flag (page on backing store) */

typedef uint64_t pte_t;

/* Virtual Address Breakdown */
static inline uint64_t mmu_vpn(uint64_t va) {
    return va >> MMU_PAGE_SHIFT;
}

static inline uint64_t mmu_offset(uint64_t va) {
    return va & MMU_PAGE_MASK;
}

static inline uint64_t mmu_round_down(uint64_t va) {
    return va & ~MMU_PAGE_MASK;
}

static inline uint64_t mmu_round_up(uint64_t va) {
    return (va + MMU_PAGE_SIZE - 1) & ~MMU_PAGE_MASK;
}

/* Simulated Page Table for a process */
typedef struct PageTable {
    int pid;
    size_t num_entries;
    pte_t *entries;            /* Indexed by VPN */
    int *swap_slots;           /* Backing store slot per VPN */
} PageTable;

PageTable *mmu_pagetable_create(int pid, size_t max_vpns);
void mmu_pagetable_destroy(PageTable *pt);
void mmu_map_page(PageTable *pt, uint64_t vpn, uint64_t pfn, uint64_t flags);
void mmu_unmap_page(PageTable *pt, uint64_t vpn);
pte_t *mmu_lookup(PageTable *pt, uint64_t vpn);
bool mmu_translate(PageTable *pt, uint64_t va, bool is_write, uint64_t *out_pa, bool *out_fault);

#endif /* MMU_H */
