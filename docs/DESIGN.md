# PageSim Architecture & Virtual Memory Design Document

## 1. Overview
PageSim is an advanced hybrid virtual-memory simulation engine and real in-kernel demand pager developed for modern operating systems education. The project addresses two fundamental challenges in operating systems:
1. **Memory Virtualization & Page Replacement (Simulator)**: Evaluating how page-fault rates, hit ratios, and memory occupancy behave under diverse page replacement policies (FIFO, LRU, Clock, Optimal, and Aging/NFU), formally detecting and analyzing Bélády’s anomaly, and modeling process working sets.
2. **In-Kernel Demand Paging & Backing Store (xv6-riscv)**: Modifying the xv6-riscv kernel to implement lazy heap growth (`sbrk`), handling load/store page faults in `usertrap`, implementing a second-chance Clock page eviction policy, maintaining backing store swap slots on disk, and executing dirty-bit awareness to eliminate redundant disk writes for clean pages.

---

## 2. Page Replacement Policies & Theoretical Basis

### 2.1 FIFO (First-In, First-Out)
- **Mechanism**: Frames are maintained in arrival order. When a page fault occurs and all frames are occupied, the frame that was brought in earliest (smallest `load_time`) is selected as the victim.
- **Complexity**: $O(1)$ replacement.
- **Pathology (Bélády’s Anomaly)**: FIFO does not satisfy the inclusion property. Allocating more physical frames can paradoxically increase the total number of page faults.

### 2.2 LRU (Least Recently Used)
- **Mechanism**: Exploits temporal locality by replacing the page that has not been accessed for the longest period of time (oldest `last_access_time`).
- **Complexity**: $O(N)$ search over frames or $O(1)$ with doubly-linked recency list.
- **Stack Algorithm Guarantee**: LRU is a member of the class of **Stack Algorithms**. For any reference string, the set of resident pages in $M$ frames at any time $t$ is a subset of the set of resident pages in $M+1$ frames:
  $$M(t, k) \subseteq M(t, k+1)$$
  Therefore, page faults can never strictly increase when frame count increases.

### 2.3 Clock (Second-Chance Page Replacement)
- **Mechanism**: Circular list of frames with a moving clock hand. Each frame maintains a hardware reference bit ($A$ / `referenced`).
  - When a victim is needed:
    - If the frame at the clock hand has $A = 1$, the OS gives it a second chance: it clears $A = 0$ and advances the hand.
    - If the frame has $A = 0$, this frame is selected as the victim and the hand advances.
  - Periodic timer interrupts clear reference bits across all frames, preventing all bits from remaining 1 indefinitely under sustained load.
- **Hardware Realism**: Approximates LRU at $O(1)$ amortized overhead without requiring full access timestamps.

### 2.4 Optimal (Bélády's MIN Algorithm)
- **Mechanism**: Inspects future references in the trace. Chooses the frame whose next reference is furthest in the future (or never referenced again).
- **Theoretical Role**: Serves as the absolute mathematical lower bound on page faults for any offline reference string. Provably optimal. Immune to Bélády's anomaly.

### 2.5 Aging / NFU (Stretch Goal)
- **Mechanism**: Software approximation of LRU using an 8-bit shift register per frame.
  - On memory access: hardware sets reference bit $A = 1$.
  - On periodic timer interrupt: the OS shifts the age register right by 1 bit (`age >>= 1`), and shifts $A$ into the most significant bit (`age |= (A << 7)`), then resets $A = 0$.
  - Victim selection: the frame with the minimum numeric age value has been unreferenced for the most recent clock ticks and is evicted first.

---

## 3. Bélády's Anomaly Formal Analysis

### 3.1 The Classic Counterexample
László Bélády discovered in 1969 that FIFO page replacement can experience more page faults when allocated more physical memory.

Consider the classic reference sequence:
$$\sigma = [1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5]$$

- **With 3 Physical Frames**:
  - Faults: **9** (Hits: 3, at steps 8, 9, 12).
- **With 4 Physical Frames**:
  - Faults: **10** (Hits: 2, at steps 5, 6).
  - Paradox: Increasing memory by 33% increases page faults by 11%!

### 3.2 Proof of Stack Algorithm Immunity
A replacement algorithm is defined as a *Stack Algorithm* if the set of pages in an $m$-frame memory is always a subset of the pages in an $(m+1)$-frame memory:
$$\forall t, \quad S(t, m) \subset S(t, m+1)$$
Because Optimal and LRU track priority orderings that do not depend on the total frame capacity $m$, any page hit that occurs with $m$ frames is guaranteed to also be a hit with $m+1$ frames. Thus:
$$\text{Faults}(m+1) \le \text{Faults}(m)$$

---

## 4. Working-Set Model & Thrashing

### 4.1 Denning's Working-Set Theory
Peter Denning (1968) formalized program locality:
- At logical time $t$, with working-set parameter $\Delta$ (window size), the Working Set $W(t, \Delta)$ is the set of unique pages referenced in the interval $[t - \Delta + 1, t]$.
- The working set size is $w(t, \Delta) = |W(t, \Delta)|$.

### 4.2 The "Working-Set Knee"
Plotting page faults against resident frame count yields a characteristic curve:
1. **Severe Thrashing Region**: When allocated frames $F < w(t, \Delta)$, page faults are catastrophically high because pages are evicted before being reused.
2. **The Knee**: The inflection point where $F \approx w(t, \Delta)$.
3. **Diminishing Returns Region**: For $F > w(t, \Delta)$, allocating additional frames yields negligible fault reductions.

---

## 5. Backing Store & Dirty-Bit Optimization

### 5.1 Architecture
Physical frames can either be **Clean** ($D = 0$) or **Dirty** ($D = 1$).
- When a page is modified by a store instruction, the MMU/kernel sets $D = 1$.
- When a victim is evicted:
  - If **Dirty**: the OS must write the 4KB page out to a disk swap slot before freeing the physical frame.
  - If **Clean**: the page has not been modified since it was loaded or zeroed. The OS drops the page immediately by clearing the valid bit ($V = 0$) **without issuing any disk write I/O**.
- When the page is subsequently refaulted:
  - If it was written to swap, it is paged back in from its assigned slot.
  - If clean and unmapped, it is freshly zeroed or re-read from its original binary backing.

---

## 6. In-Kernel xv6-riscv Demand Paging Architecture

### 6.1 Lazy Heap Growth (`sbrk`)
In standard xv6, `sys_sbrk(n)` calls `growproc(n)` which immediately allocates physical frames using `kalloc()` and maps them into the page table via `mappages()`.

Under Demand Paging:
1. When `n > 0`, `sys_sbrk()` increments `p->sz` and returns immediately without allocating physical memory.
2. No page table entries are created at this time.

### 6.2 Page Fault Handler in `usertrap`
When the CPU executes a load (`scause == 13`) or store (`scause == 15`) instruction accessing an unmapped virtual address $va$:
1. The hardware traps to `usertrap()` in `kernel/trap.c`.
2. The kernel verifies:
   - $va < p\text{->sz}$
   - $va \ge \text{PGROUNDDOWN}(p\text{->trapframe->sp}) - \text{PGSIZE}$ (valid heap or growing stack).
3. The kernel walks the page table:
   - **Case A: Swapped Page** (`*pte & PTE_S`):
     Reads the page back from swap block into a newly allocated frame, restores original permissions, updates PTE with `PTE_V`, and flushes TLB (`sfence_vma()`).
   - **Case B: Lazy Heap Page**:
     Allocates a new physical frame with `kalloc()`, zeroes it with `memset()`, maps it with `PTE_V | PTE_U | PTE_R | PTE_W`, and resumes user execution!
