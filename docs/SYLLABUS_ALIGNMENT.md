# 12-Week Course Syllabus Alignment & Artifact Matrix

This document maps the implementation of **PageSim** to the 12-week Operating Systems syllabus, demonstrating the progressive engineering increments shipped week-by-week.

| Week | Course Syllabus Focus | What Was Built in PageSim & xv6 | Source Modules & Verification |
|:---:|:---|:---|:---|
| **W1** | Syscall boundary · user vs kernel mode · REPL & repo setup | Clean modular project repository, frame model, reference string structures, and baseline FIFO policy with page fault counting. | `src/pagesim.c`, `src/policy_fifo.c`, `include/pagesim.h` |
| **W2** | C toolchain · memory model · stack, heap & the linker | Formalized address-to-page translation (`VPN`, offset), RISC-V SV39 PTE bit representations (`PTE_V`, `PTE_R`, `PTE_W`, `PTE_X`, `PTE_U`, `PTE_A`, `PTE_D`, `PTE_S`). | `include/mmu.h`, `src/mmu.c`, `kernel/riscv.h` |
| **W3** | Lexing & parsing · the process/session table · console I/O | Trace lexer/parser for inline strings, memory address traces, file reader, and pluggable policy dispatch table. | `src/trace.c`, `src/policy.c`, `include/trace.h`, `include/policy.h` |
| **W4** | Processes · fork / exec / wait · CPU scheduling policies | Multi-process working set simulator: per-process frame partitions, round-robin scheduling interaction, and system thrashing detection. | `src/process_sim.c`, `include/process_sim.h` |
| **W5** | exec & PATH resolution · built-ins · ELF loading | Memory access trace generator derived from ELF segment layout (text execution loops, data array scans, and stack frames). | `src/trace.c` (`trace_gen_elf_pattern`), `traces/elf_program.trace` |
| **W6** | Signals · asynchronous control · timer interrupts & alarms | Simulated timer interrupt events: periodic reference-bit clear for Clock policy and periodic shift-and-insert for Aging policy. | `src/policy_clock.c` (`on_timer_tick`), `src/policy_aging.c` |
| **W7** | Pipes & file descriptors · producer/consumer plumbing | Streaming trace reader from standard input (`trace_stream_open`, `--stream`), allowing infinite reference strings without loading into RAM. | `src/trace.c` (`TraceStream`), `src/main.c` (`--stream`) |
| **W8** | Virtual memory · page tables · copy-on-write · ASan | Core replacement algorithms: LRU, Second-Chance Clock, and Optimal (MIN). AddressSanitizer verified 100% leak-free (`make asan`). Bélády anomaly detection. | `src/policy_lru.c`, `src/policy_clock.c`, `src/policy_optimal.c`, `src/belady.c` |
| **W9** | Files · descriptors · redirection · inodes & on-disk layout | Backing store abstraction: file-backed swap store, slot allocation, page-out/page-in, and dirty-bit optimization (clean page drop). | `src/swap_store.c`, `include/swap_store.h`, `kernel/swap.c` |
| **W10** | Threads · mutual exclusion · race conditions · parallel speedup | Benchmark comparison of all policies across workloads; resident-set size vs page fault curves; identification of the "working set knee". | `src/working_set.c`, `scripts/plot_curves.py` |
| **W11** | Semaphores · deadlock · coordination · job lifecycle | In-kernel demand paging in xv6-riscv: lazy `sbrk`, `usertrap` load/store page fault handling, Clock page eviction, and swap-in. | `kernel/trap.c`, `kernel/sysproc.c`, `kernel/vm.c`, `kernel/swap.c` |
| **W12** | Integration · packaging & hosting · test battery · demo | Full integration test battery, ASCII frame occupancy timeline visualizer, comprehensive documentation, and live demo script. | `tests/`, `docs/DESIGN.md`, `docs/DEMO.md`, `Makefile` |
