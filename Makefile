# MemPlumber Memory Allocator
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g -Iinclude
SRCDIR = src
INCDIR = include
TESTDIR = tests
BINDIR = bin

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(BINDIR)/%.o)

# Test files
TEST_SOURCES = $(wildcard $(TESTDIR)/*.cpp)
TEST_OBJECTS = $(TEST_SOURCES:$(TESTDIR)/%.cpp=$(BINDIR)/%.o)

.PHONY: all clean test test-basic test-allocator test-reuse test-global test-slab test-composite test-pool test-benchmark

all: $(BINDIR)/test_basic $(BINDIR)/test_free_list_allocator $(BINDIR)/test_memory_reuse $(BINDIR)/test_global_allocator $(BINDIR)/test_slab_allocator $(BINDIR)/test_composite_allocator $(BINDIR)/test_pool_allocator $(BINDIR)/test_benchmark

test: test-basic test-allocator test-reuse test-global test-slab test-composite test-pool test-benchmark

test-basic: $(BINDIR)/test_basic
	@echo "Running basic tests..."
	./$(BINDIR)/test_basic

test-allocator: $(BINDIR)/test_free_list_allocator
	@echo "Running allocator tests..."
	./$(BINDIR)/test_free_list_allocator

test-reuse: $(BINDIR)/test_memory_reuse
	@echo "Running memory reuse tests..."
	./$(BINDIR)/test_memory_reuse

test-global: $(BINDIR)/test_global_allocator
	@echo "Running global allocator tests..."
	./$(BINDIR)/test_global_allocator

test-slab: $(BINDIR)/test_slab_allocator
	@echo "Running slab allocator tests..."
	./$(BINDIR)/test_slab_allocator

test-composite: $(BINDIR)/test_composite_allocator
	@echo "Running composite allocator tests..."
	./$(BINDIR)/test_composite_allocator

test-pool: $(BINDIR)/test_pool_allocator
	@echo "Running pool allocator tests..."
	./$(BINDIR)/test_pool_allocator

test-benchmark: $(BINDIR)/test_benchmark
	@echo "Running benchmark tests..."
	./$(BINDIR)/test_benchmark

$(BINDIR)/test_basic: $(OBJECTS) $(BINDIR)/test_basic.o | $(BINDIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BINDIR)/test_free_list_allocator: $(OBJECTS) $(BINDIR)/test_free_list_allocator.o | $(BINDIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BINDIR)/test_memory_reuse: $(OBJECTS) $(BINDIR)/test_memory_reuse.o | $(BINDIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BINDIR)/test_global_allocator: $(OBJECTS) $(BINDIR)/test_global_allocator.o | $(BINDIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BINDIR)/test_slab_allocator: $(OBJECTS) $(BINDIR)/test_slab_allocator.o | $(BINDIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BINDIR)/test_composite_allocator: $(OBJECTS) $(BINDIR)/test_composite_allocator.o | $(BINDIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BINDIR)/test_pool_allocator: $(OBJECTS) $(BINDIR)/test_pool_allocator.o | $(BINDIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BINDIR)/test_benchmark: $(OBJECTS) $(BINDIR)/test_benchmark.o | $(BINDIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BINDIR)/%.o: $(SRCDIR)/%.cpp | $(BINDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BINDIR)/%.o: $(TESTDIR)/%.cpp | $(BINDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BINDIR)/*

$(BINDIR):
	mkdir -p $(BINDIR)
