#include "mmu.h"
#include <stdlib.h>
#include <string.h>

PageTable *mmu_pagetable_create(int pid, size_t max_vpns)
{
    if (max_vpns == 0) max_vpns = 4096;

    PageTable *pt = (PageTable *)malloc(sizeof(PageTable));
    if (!pt) return NULL;

    pt->pid = pid;
    pt->num_entries = max_vpns;
    pt->entries = (pte_t *)calloc(max_vpns, sizeof(pte_t));
    pt->swap_slots = (int *)malloc(max_vpns * sizeof(int));

    if (!pt->entries || !pt->swap_slots) {
        if (pt->entries) free(pt->entries);
        if (pt->swap_slots) free(pt->swap_slots);
        free(pt);
        return NULL;
    }

    for (size_t i = 0; i < max_vpns; i++) {
        pt->swap_slots[i] = -1;
    }

    return pt;
}

void mmu_pagetable_destroy(PageTable *pt)
{
    if (!pt) return;
    if (pt->entries) free(pt->entries);
    if (pt->swap_slots) free(pt->swap_slots);
    free(pt);
}

void mmu_map_page(PageTable *pt, uint64_t vpn, uint64_t pfn, uint64_t flags)
{
    if (!pt || vpn >= pt->num_entries) return;
    pt->entries[vpn] = (pfn << 10) | (flags & 0x3FF) | PTE_V;
}

void mmu_unmap_page(PageTable *pt, uint64_t vpn)
{
    if (!pt || vpn >= pt->num_entries) return;
    pt->entries[vpn] = 0;
}

pte_t *mmu_lookup(PageTable *pt, uint64_t vpn)
{
    if (!pt || vpn >= pt->num_entries) return NULL;
    return &pt->entries[vpn];
}

bool mmu_translate(PageTable *pt, uint64_t va, bool is_write, uint64_t *out_pa, bool *out_fault)
{
    if (!pt || !out_pa || !out_fault) return false;

    uint64_t vpn = mmu_vpn(va);
    uint64_t offset = mmu_offset(va);

    if (vpn >= pt->num_entries) {
        *out_fault = true;
        return false;
    }

    pte_t *pte = &pt->entries[vpn];
    if (!(*pte & PTE_V)) {
        /* Page is either not mapped or swapped out */
        *out_fault = true;
        return false;
    }

    /* Check write permission if this is a write access */
    if (is_write && !(*pte & PTE_W)) {
        *out_fault = true;
        return false;
    }

    /* Update hardware bits: Accessed and Dirty */
    *pte |= PTE_A;
    if (is_write) {
        *pte |= PTE_D;
    }

    uint64_t pfn = (*pte) >> 10;
    *out_pa = (pfn << MMU_PAGE_SHIFT) | offset;
    *out_fault = false;
    return true;
}
