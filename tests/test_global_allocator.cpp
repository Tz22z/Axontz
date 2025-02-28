#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include "axontzz/allocator_interface.h"

// 声明全局API
namespace memplumber {
    namespace global {
        AllocatorInterface::AllocatorStats get_global_allocator_stats();
        bool is_pointer_owned_by_global_allocator(void* ptr);
    }
}

void test_global_new_delete() {
    std::cout << "Testing global new/delete override..." << std::endl;
    
    auto initial_stats = memplumber::global::get_global_allocator_stats();
    std::cout << "Initial allocations: " << initial_stats.allocation_count << std::endl;
    
    // 测试基本的 new/delete
    int* ptr = new int(42);
    assert(ptr != nullptr);
    assert(*ptr == 42);
    
    auto after_new_stats = memplumber::global::get_global_allocator_stats();
    std::cout << "After new allocations: " << after_new_stats.allocation_count << std::endl;
    assert(after_new_stats.allocation_count > initial_stats.allocation_count);
    
    delete ptr;
    
    auto after_delete_stats = memplumber::global::get_global_allocator_stats();
    std::cout << "After delete deallocations: " << after_delete_stats.deallocation_count << std::endl;
    
    std::cout << "Global new/delete test passed!" << std::endl;
}

void test_global_array_new_delete() {
    std::cout << "Testing global array new[]/delete[]..." << std::endl;
    
    auto initial_stats = memplumber::global::get_global_allocator_stats();
    
    // 测试数组 new[]/delete[]
    int* arr = new int[100];
    assert(arr != nullptr);
    
    // 填充数组
    for (int i = 0; i < 100; ++i) {
        arr[i] = i;
    }
    
    // 验证数组内容
    for (int i = 0; i < 100; ++i) {
        assert(arr[i] == i);
    }
    
    auto after_array_new_stats = memplumber::global::get_global_allocator_stats();
    assert(after_array_new_stats.allocation_count > initial_stats.allocation_count);
    
    delete[] arr;
    
    std::cout << "Global array new[]/delete[] test passed!" << std::endl;
}

void test_stl_containers() {
    std::cout << "Testing STL containers with global allocator..." << std::endl;
    
    auto initial_stats = memplumber::global::get_global_allocator_stats();
    
    // 测试 std::vector (会使用全局分配器)
    {
        std::vector<int> vec;
        for (int i = 0; i < 1000; ++i) {
            vec.push_back(i);
        }
        
        // 验证内容
        for (size_t i = 0; i < vec.size(); ++i) {
            assert(vec[i] == static_cast<int>(i));
        }
        
        std::cout << "Vector size: " << vec.size() << std::endl;
    } // vector 析构
    
    // 测试 std::string (会使用全局分配器)
    {
        std::string str = "Hello, MemPlumber! This is a longer string to test allocation.";
        assert(str.length() > 0);
        std::cout << "String: " << str.substr(0, 20) << "..." << std::endl;
    } // string 析构
    
    auto final_stats = memplumber::global::get_global_allocator_stats();
    std::cout << "Total allocations: " << final_stats.allocation_count << std::endl;
    std::cout << "Total deallocations: " << final_stats.deallocation_count << std::endl;
    
    assert(final_stats.allocation_count > initial_stats.allocation_count);
    
    std::cout << "STL containers test passed!" << std::endl;
}

void test_nothrow_new() {
    std::cout << "Testing nothrow new..." << std::endl;
    
    // 测试 nothrow new
    int* ptr = new(std::nothrow) int(123);
    assert(ptr != nullptr);
    assert(*ptr == 123);
    
    delete ptr;
    
    // 测试 nothrow array new
    int* arr = new(std::nothrow) int[50];
    assert(arr != nullptr);
    
    delete[] arr;
    
    std::cout << "Nothrow new test passed!" << std::endl;
}

void test_allocation_stats() {
    std::cout << "Testing allocation statistics..." << std::endl;
    
    auto stats = memplumber::global::get_global_allocator_stats();
    
    std::cout << "=== Global Allocator Statistics ===" << std::endl;
    std::cout << "Total allocated: " << stats.total_allocated << " bytes" << std::endl;
    std::cout << "Total deallocated: " << stats.total_deallocated << " bytes" << std::endl;
    std::cout << "Current usage: " << stats.current_usage << " bytes" << std::endl;
    std::cout << "Allocation count: " << stats.allocation_count << std::endl;
    std::cout << "Deallocation count: " << stats.deallocation_count << std::endl;
    std::cout << "Failed allocations: " << stats.failed_allocations << std::endl;
    std::cout << "Fragmentation ratio: " << stats.fragmentation_ratio << std::endl;
    std::cout << "===================================" << std::endl;
    
    std::cout << "Statistics test completed!" << std::endl;
}

int main() {
    std::cout << "=== Global Allocator Tests ===" << std::endl;
    
    try {
        test_global_new_delete();
        std::cout << std::endl;
        
        test_global_array_new_delete();
        std::cout << std::endl;
        
        test_stl_containers();
        std::cout << std::endl;
        
        test_nothrow_new();
        std::cout << std::endl;
        
        test_allocation_stats();
        
        std::cout << "\n✓ All global allocator tests passed!" << std::endl;
        std::cout << "Global memory allocator is working correctly!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
    
    return 0;
}
