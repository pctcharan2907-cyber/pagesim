CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude -O2 -g
ASAN_FLAGS = -fsanitize=address,undefined -g

SRC = src/pagesim.c \
      src/mmu.c \
      src/policy.c \
      src/policy_fifo.c \
      src/policy_lru.c \
      src/policy_clock.c \
      src/policy_optimal.c \
      src/policy_aging.c \
      src/trace.c \
      src/belady.c \
      src/working_set.c \
      src/visualizer.c \
      src/swap_store.c \
      src/process_sim.c

MAIN_SRC = src/main.c
TARGET = pagesim

TEST_POLICIES = tests/test_policies
TEST_BELADY   = tests/test_belady
TEST_WS       = tests/test_workingset
TEST_TRACES   = tests/test_traces

all: $(TARGET)

$(TARGET): $(SRC) $(MAIN_SRC)
	$(CC) $(CFLAGS) $(SRC) $(MAIN_SRC) -o $(TARGET)

test: $(TEST_POLICIES) $(TEST_BELADY) $(TEST_WS) $(TEST_TRACES)
	@echo "=================================================="
	@echo "            RUNNING ALL UNIT TESTS                "
	@echo "=================================================="
	./$(TEST_POLICIES)
	./$(TEST_BELADY)
	./$(TEST_WS)
	./$(TEST_TRACES)
	@echo "=================================================="
	@echo "       ALL UNIT TESTS PASSED SUCCESSFULLY!        "
	@echo "=================================================="

$(TEST_POLICIES): tests/test_policies.c $(SRC)
	$(CC) $(CFLAGS) tests/test_policies.c $(SRC) -o $(TEST_POLICIES)

$(TEST_BELADY): tests/test_belady.c $(SRC)
	$(CC) $(CFLAGS) tests/test_belady.c $(SRC) -o $(TEST_BELADY)

$(TEST_WS): tests/test_workingset.c $(SRC)
	$(CC) $(CFLAGS) tests/test_workingset.c $(SRC) -o $(TEST_WS)

$(TEST_TRACES): tests/test_traces.c $(SRC)
	$(CC) $(CFLAGS) tests/test_traces.c $(SRC) -o $(TEST_TRACES)

asan:
	$(CC) $(CFLAGS) $(ASAN_FLAGS) $(SRC) tests/test_policies.c -o test_asan_policies
	$(CC) $(CFLAGS) $(ASAN_FLAGS) $(SRC) tests/test_belady.c -o test_asan_belady
	$(CC) $(CFLAGS) $(ASAN_FLAGS) $(SRC) tests/test_workingset.c -o test_asan_ws
	$(CC) $(CFLAGS) $(ASAN_FLAGS) $(SRC) tests/test_traces.c -o test_asan_traces
	./test_asan_policies
	./test_asan_belady
	./test_asan_ws
	./test_asan_traces
	@echo "=================================================="
	@echo " ASan-clean verification: 0 LEAKS, 0 ERRORS!     "
	@echo "=================================================="
	rm -f test_asan_*

demo: $(TARGET)
	./$(TARGET) --demo

plot:
	python3 scripts/plot_curves.py

clean:
	rm -f $(TARGET) $(TEST_POLICIES) $(TEST_BELADY) $(TEST_WS) $(TEST_TRACES)
	rm -f *.o test_asan_* demo_belady.csv demo_swap.bin swapfile.bin
	rm -rf docs/plots/*.png

.PHONY: all test asan demo plot clean
