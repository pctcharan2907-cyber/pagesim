# PageSim: Week-12 Viva & Live Demonstration Guide

This guide contains the exact steps and commands to reproduce all demonstrations and prove full rubric compliance.

---

## 1. Quick Build & Automated Acceptance Battery
Run the complete automated test suite and live demo in one command:
```bash
cd simulator
make clean
make -j4
make test
make asan
make demo
```

---

## 2. Core Project-Specific Acceptance Demonstrations

### Acceptance Test 1: Policy Correctness on Curated Strings
Compare all policies on the classic textbook benchmark:
```bash
./pagesim -f 3 -t "7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1" --compare
```
**Expected Outcome**:
- **FIFO**: 15 Faults (Hit Ratio: 25.00%)
- **LRU**: 12 Faults (Hit Ratio: 40.00%)
- **Clock**: 14 Faults (Hit Ratio: 30.00%)
- **Optimal**: 9 Faults (Hit Ratio: 55.00% - theoretical best)

---

### Acceptance Test 2: Bélády's Anomaly Detection (FIFO Anomaly)
Demonstrate that FIFO suffers from Bélády's Anomaly on the classic reference sequence while LRU and Optimal do not:
```bash
./pagesim --belady --min-frames 2 --max-frames 6
```
**Expected Output Summary**:
```
  Frames |   FIFO   |   LRU    |  Clock   | Optimal  |  Aging   | Anomaly Note
       3 |        9 |       10 |       10 |        7 |       10 | 
       4 |       10 |        8 |        9 |        6 |        8 | <-- FIFO Anomaly!
```
- Increasing frames from 3 to 4 increases FIFO faults from 9 to 10 (+1 fault).
- LRU and Optimal are strictly non-increasing (Stack Algorithm inclusion property $M(k) \subseteq M(k+1)$).

---

### Acceptance Test 3: Visual Timeline of Frame Occupancy
Display the ASCII timeline showing frame states and hits/faults over time:
```bash
./pagesim -f 3 -t "1 2 3 4 1 2 5 1 2 3 4 5" --timeline -p FIFO
./pagesim -f 4 -t "1 2 3 4 1 2 5 1 2 3 4 5" --timeline -p FIFO
```

---

### Acceptance Test 4: Working-Set Model & Resident Set Fault Curve
Measure fault rates across resident frame allocations and observe the "Working Set Knee":
```bash
./pagesim --workingset --min-frames 2 --max-frames 12 --gen locality --len 200
```
- Generates ASCII curve and reports the inflection point where adding more physical frames yields diminishing returns.

---

### Acceptance Test 5: Backing Store & Dirty-Bit Optimization
Verify that clean pages are dropped without writing to swap:
```bash
./pagesim -f 2 -t "1:R, 2:R, 3:W, 4:R" -v --clean-drops
```
- Pages 1 and 2 are read-only (`R`). When evicted, they are reported as `CLEAN->DROP` with 0 disk write operations.

---

### Acceptance Test 6: Vector Plot Generation
Generate publication-quality SVG charts:
```bash
make plot
```
Outputs:
- `docs/plots/belady_anomaly_curve.svg`: Highlighted curve showing FIFO anomaly spike.
- `docs/plots/working_set_curve.svg`: Characteristic working-set knee curve.

---

## 3. In-Kernel xv6 Demand Paging Demonstration
To run xv6 under QEMU:
```bash
cd ../xv6-riscv
make qemu
```
Inside the xv6 shell:
```bash
$ demandtest
```
This tests:
1. **Lazy Heap Allocation**: Process calls `sbrk(100 * 4096)`, verifying physical memory is NOT allocated until accessed.
2. **Page Fault Trap Handling**: Touching pages triggers load/store page faults and dynamically allocates frames.
3. **Memory Pressure & Eviction**: Allocating pages beyond physical limits triggers Clock eviction to backing store.
4. **Data Integrity**: Reading back swapped pages restores data without corruption.
5. **Dirty Bit Optimization**: Clean pages are dropped without disk writes.
