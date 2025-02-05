#include "axontzz/memory_source.h"
#include <iostream>
#include <cassert>
#include <cstring>

using namespace memplumber;

void test_memory_source() {
    std::cout << "Testing MemorySource..." << std::endl;
    
    MemorySource memory_source;
    
    // Test basic allocation
    size_t test_size = 4096; // One page
    void* ptr = memory_source.allocate_block(test_size);
    assert(ptr != nullptr);
    
    // Test that we can write to the memory
    memset(ptr, 0xAA, test_size);
    
    // Verify the memory content
    unsigned char* bytes = static_cast<unsigned char*>(ptr);
    for (size_t i = 0; i < test_size; ++i) {
        assert(bytes[i] == 0xAA);
    }
    
    // Check statistics
    auto stats = memory_source.get_stats();
    assert(stats.allocation_count == 1);
    assert(stats.current_usage >= test_size);
    
    // Test deallocation
    memory_source.deallocate_block(ptr, test_size);
    stats = memory_source.get_stats();
    assert(stats.deallocation_count == 1);
    assert(stats.current_usage == 0);
    
    std::cout << "MemorySource tests passed!" << std::endl;
}

void test_page_alignment() {
    std::cout << "Testing page alignment..." << std::endl;
    
    MemorySource memory_source;
    size_t page_size = memory_source.get_page_size();
    
    // Test various sizes get aligned properly
    assert(memory_source.align_to_page(1) == page_size);
    assert(memory_source.align_to_page(page_size) == page_size);
    assert(memory_source.align_to_page(page_size + 1) == page_size * 2);
    
    std::cout << "Page alignment tests passed!" << std::endl;
}

void test_large_allocations() {
    std::cout << "Testing large allocations..." << std::endl;
    
    MemorySource memory_source;
    
    // Test allocating multiple large blocks
    constexpr size_t large_size = 1024 * 1024; // 1MB
    constexpr int num_blocks = 10;
    
    void* blocks[num_blocks];
    
    for (int i = 0; i < num_blocks; ++i) {
        blocks[i] = memory_source.allocate_block(large_size);
        assert(blocks[i] != nullptr);
        
        // Write unique pattern to each block
        uint32_t* words = static_cast<uint32_t*>(blocks[i]);
        for (size_t j = 0; j < large_size / sizeof(uint32_t); ++j) {
            words[j] = static_cast<uint32_t>(i * 1000000 + j);
        }
    }
    
    // Verify all blocks have correct data
    for (int i = 0; i < num_blocks; ++i) {
        uint32_t* words = static_cast<uint32_t*>(blocks[i]);
        for (size_t j = 0; j < large_size / sizeof(uint32_t); ++j) {
            assert(words[j] == static_cast<uint32_t>(i * 1000000 + j));
        }
    }
    
    // Clean up
    for (int i = 0; i < num_blocks; ++i) {
        memory_source.deallocate_block(blocks[i], large_size);
    }
    
    auto stats = memory_source.get_stats();
    assert(stats.current_usage == 0);
    
    std::cout << "Large allocation tests passed!" << std::endl;
}

int main() {
    std::cout << "=== MemPlumber Basic Tests ===" << std::endl;
    
    try {
        test_memory_source();
        test_page_alignment();
        test_large_allocations();
        
        std::cout << "\n✓ All basic tests passed!" << std::endl;
        std::cout << "Foundation is solid - ready for allocator implementation." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
    
    return 0;
}
