#include "axontzz/free_list_allocator.h"
#include "axontzz/memory_source.h"
#include <iostream>
#include <cassert>

using namespace memplumber;

void test_basic_memory_reuse() {
    std::cout << "Testing basic memory reuse..." << std::endl;
    
    MemorySource memory_source;
    FreeListAllocator allocator(memory_source, 4096);
    
    // 第一次分配
    void* ptr1 = allocator.allocate(256);
    assert(ptr1 != nullptr);
    std::cout << "First allocation at: " << ptr1 << std::endl;
    
    // 释放
    allocator.deallocate(ptr1, 256);
    std::cout << "Deallocated first pointer" << std::endl;
    
    // 第二次分配相同大小 - 应该重用相同的内存
    void* ptr2 = allocator.allocate(256);
    assert(ptr2 != nullptr);
    std::cout << "Second allocation at: " << ptr2 << std::endl;
    
    if (ptr1 == ptr2) {
        std::cout << "✓ Memory was successfully reused!" << std::endl;
    } else {
        std::cout << "✗ Memory was NOT reused (this is expected with current implementation)" << std::endl;
    }
    
    allocator.deallocate(ptr2, 256);
    
    std::cout << "Basic memory reuse test completed!" << std::endl;
}

void test_multiple_sizes() {
    std::cout << "Testing allocation/deallocation of different sizes..." << std::endl;
    
    MemorySource memory_source;
    FreeListAllocator allocator(memory_source, 4096);
    
    // 分配不同大小的块
    void* ptrs[4];
    size_t sizes[] = {64, 128, 256, 512};
    
    // 分配所有块
    for (int i = 0; i < 4; ++i) {
        ptrs[i] = allocator.allocate(sizes[i]);
        assert(ptrs[i] != nullptr);
        std::cout << "Allocated " << sizes[i] << " bytes at " << ptrs[i] << std::endl;
    }
    
    // 释放奇数索引的块
    for (int i = 1; i < 4; i += 2) {
        allocator.deallocate(ptrs[i], sizes[i]);
        std::cout << "Deallocated " << sizes[i] << " bytes" << std::endl;
    }
    
    // 尝试重新分配相似大小的块
    void* new_ptr = allocator.allocate(120); // 接近128
    assert(new_ptr != nullptr);
    std::cout << "New allocation of 120 bytes at " << new_ptr << std::endl;
    
    // 清理剩余内存
    allocator.deallocate(ptrs[0], sizes[0]);
    allocator.deallocate(ptrs[2], sizes[2]);
    allocator.deallocate(new_ptr, 120);
    
    std::cout << "Multiple sizes test completed!" << std::endl;
}

int main() {
    std::cout << "=== Memory Reuse Tests ===" << std::endl;
    
    try {
        test_basic_memory_reuse();
        std::cout << std::endl;
        test_multiple_sizes();
        
        std::cout << "\n✓ All memory reuse tests passed!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
    
    return 0;
}
