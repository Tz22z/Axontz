#include "axontzz/free_list_allocator.h"
#include "axontzz/memory_source.h"
#include <iostream>
#include <cassert>

using namespace memplumber;

void test_basic_allocator_creation() {
    std::cout << "Testing FreeListAllocator creation..." << std::endl;
    
    MemorySource memory_source;
    FreeListAllocator allocator(memory_source, 4096);
    
    // Test basic properties
    assert(allocator.get_name() != nullptr);
    std::cout << "Allocator name: " << allocator.get_name() << std::endl;
    
    auto stats = allocator.get_stats();
    assert(stats.allocation_count == 0);
    assert(stats.current_usage == 0);
    
    std::cout << "Basic creation test passed!" << std::endl;
}

void test_simple_allocation() {
    std::cout << "Testing simple allocation..." << std::endl;
    
    MemorySource memory_source;
    FreeListAllocator allocator(memory_source, 4096);
    
    // Test allocation
    void* ptr = allocator.allocate(256);
    assert(ptr != nullptr);
    std::cout << "Allocated 256 bytes at: " << ptr << std::endl;
    
    // Check stats
    auto stats = allocator.get_stats();
    assert(stats.allocation_count == 1);
    assert(stats.current_usage == 256);
    
    // Test ownership
    assert(allocator.owns(ptr) == true);
    assert(allocator.owns(nullptr) == false);
    
    // Test deallocation
    allocator.deallocate(ptr, 256);
    stats = allocator.get_stats();
    assert(stats.deallocation_count == 1);
    assert(stats.current_usage == 0);
    
    std::cout << "Simple allocation test passed!" << std::endl;
}

void test_multiple_allocations() {
    std::cout << "Testing multiple allocations..." << std::endl;
    
    MemorySource memory_source;
    FreeListAllocator allocator(memory_source, 4096);
    
    const int num_allocs = 5;
    void* ptrs[num_allocs];
    size_t sizes[] = {64, 128, 256, 512, 1024};
    
    // Allocate multiple blocks
    for (int i = 0; i < num_allocs; ++i) {
        ptrs[i] = allocator.allocate(sizes[i]);
        assert(ptrs[i] != nullptr);
        std::cout << "Allocated " << sizes[i] << " bytes at: " << ptrs[i] << std::endl;
    }
    
    // Check stats
    auto stats = allocator.get_stats();
    assert(stats.allocation_count == num_allocs);
    
    size_t total_expected = 0;
    for (int i = 0; i < num_allocs; ++i) {
        total_expected += sizes[i];
    }
    assert(stats.current_usage == total_expected);
    
    // Deallocate all
    for (int i = 0; i < num_allocs; ++i) {
        allocator.deallocate(ptrs[i], sizes[i]);
    }
    
    stats = allocator.get_stats();
    assert(stats.deallocation_count == num_allocs);
    assert(stats.current_usage == 0);
    
    std::cout << "Multiple allocations test passed!" << std::endl;
}

void test_stats_and_debugging() {
    std::cout << "Testing stats and debugging..." << std::endl;
    
    MemorySource memory_source;
    FreeListAllocator allocator(memory_source, 4096);
    
    // Test stats reset
    allocator.allocate(100);
    allocator.reset_stats();
    auto stats = allocator.get_stats();
    assert(stats.allocation_count == 0);
    
    // Test dump function
    allocator.dump_free_list();
    
    std::cout << "Stats and debugging test passed!" << std::endl;
}

int main() {
    std::cout << "=== FreeListAllocator Basic Tests ===" << std::endl;
    
    try {
        test_basic_allocator_creation();
        test_simple_allocation();
        test_multiple_allocations();
        test_stats_and_debugging();
        
        std::cout << "\n✓ All FreeListAllocator tests passed!" << std::endl;
        std::cout << "Ready for next iteration of development." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
    
    return 0;
}
