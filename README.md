# PageSim — Demand Paging & Page Replacement Simulator

[![Language: C11](https://img.shields.io/badge/Language-C11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Platform: Linux/Ubuntu](https://img.shields.io/badge/Platform-Ubuntu%20%7C%20RISC--V-orange.svg)](https://ubuntu.com)
[![Status: ASan-Clean](https://img.shields.io/badge/Memory-ASan--Clean%20(0%20leaks)-brightgreen.svg)]()

> **"Make memory bigger than RAM: page faults, replacement policies, and a real pager in a kernel."**

PageSim is an advanced virtual memory simulation system paired with an in-kernel demand paging implementation for **xv6-riscv**. It models physical frames, reference strings, page replacement algorithms, working sets, and backing stores.

---

## Features
- **Page Replacement Policies**:
  - **FIFO** (First-In, First-Out)
  - **LRU** (Least Recently Used - stack algorithm)
  - **Clock** (Second-Chance with hardware reference bit emulation)
  - **Optimal (MIN)** (Bélády's theoretical lower-bound baseline)
  - **Aging / NFU** (Shift-register LRU approximation with timer ticks)
- **Bélády's Anomaly Detection**:
  - Demonstrates the anomaly for FIFO on classic strings (3 frames $\to$ 9 faults, 4 frames $\to$ 10 faults).
  - Proves stack algorithm immunity for LRU and Optimal.
- **Working-Set Model**:
  - Computes sliding window working set $W(t, \Delta)$ and average working set size $\overline{w}(\Delta)$.
  - Analyzes fault rate vs allocated frames to identify the **Working Set Knee**.
- **Visual Timeline**:
  - Full ASCII matrix rendering physical frame contents, hit/fault indicators, and dirty/clean eviction markers over time.
- **Dirty-Bit Awareness & Backing Store**:
  - Dropping clean pages with zero disk I/O; writing dirty pages out to backing store slots.
- **In-Kernel Demand Paging in xv6-riscv**:
  - Lazy heap growth via `sbrk`.
  - Load/store page-fault trap handling in `usertrap`.
  - Second-chance Clock eviction under memory pressure to disk swap blocks.

---

## Quick Start
```bash
# Build PageSim
make clean && make -j4

# Run all unit tests
make test

# Verify AddressSanitizer safety (0 leaks, 0 errors)
make asan

# Run the complete automated demo battery
make demo

# Generate publication-grade SVG vector charts
make plot
```

---

## Command Reference
| Option | Description | Example |
|:---|:---|:---|
| `-f, --frames <N>` | Number of physical frames | `./pagesim -f 4` |
| `-p, --policy <name>` | Replacement policy (FIFO, LRU, Clock, Optimal, Aging) | `./pagesim -p LRU` |
| `-t, --trace <str>` | Inline reference string | `./pagesim -t "1 2 3 4 1 2 5 1 2 3 4 5"` |
| `-i, --input <file>` | Load trace file | `./pagesim -i traces/belady_classic.trace` |
| `--compare` | Compare all policies side-by-side | `./pagesim --compare` |
| `--belady` | Scan for Bélády's anomaly across frame range | `./pagesim --belady --min-frames 2 --max-frames 6` |
| `--workingset` | Compute working-set curve and knee | `./pagesim --workingset` |
| `--timeline` | Display ASCII frame occupancy timeline | `./pagesim --timeline` |
| `--clean-drops` | Enable dirty-bit clean drop optimization | `./pagesim --clean-drops` |
| `--demo` | Run full Week-12 acceptance test battery | `./pagesim --demo` |

---

## Directory Layout
```
simulator/
├── Makefile                # Build, test, asan, demo, and plot targets
├── README.md               # Project documentation
├── milestone_demo.txt      # Quick demo runbook
├── docs/                   # Architecture design, syllabus alignment, and viva guides
│   ├── DESIGN.md
│   ├── SYLLABUS_ALIGNMENT.md
│   ├── DEMO.md
│   └── plots/              # Generated SVG vector plots
├── include/                # Modular headers
│   ├── pagesim.h
│   ├── policy.h
│   ├── mmu.h
│   ├── trace.h
│   ├── belady.h
│   ├── working_set.h
│   ├── visualizer.h
│   ├── swap_store.h
│   └── process_sim.h
├── src/                    # Implementation sources
│   ├── main.c
│   ├── pagesim.c
│   ├── mmu.c
│   ├── policy.c
│   ├── policy_fifo.c
│   ├── policy_lru.c
│   ├── policy_clock.c
│   ├── policy_optimal.c
│   ├── policy_aging.c
│   ├── trace.c
│   ├── belady.c
│   ├── working_set.c
│   ├── visualizer.c
│   ├── swap_store.c
│   └── process_sim.c
├── tests/                  # Test suites
│   ├── test_policies.c
│   ├── test_belady.c
│   ├── test_workingset.c
│   └── test_traces.c
├── traces/                 # Curated reference string workloads
│   ├── belady_classic.trace
│   ├── locality_80_20.trace
│   ├── looping_sequential.trace
│   └── elf_program.trace
└── scripts/                # Plotting utilities
    └── plot_curves.py
```

---

## Authors & Course Context
Developed for **CS2104E: Operating Systems** (Advanced Hybrid Track).
Author: Paul charan tej (`pctcharan2907@gmail.com`).
