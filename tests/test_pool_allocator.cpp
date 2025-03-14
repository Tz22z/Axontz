#include "axontzz/pool_allocator.h"
#include "axontzz/memory_source.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <cstring>

using namespace memplumber;

void test_pool_creation() {
    std::cout << "测试 PoolAllocator 创建..." << std::endl;

    MemorySource mem_source;
    PoolAllocator pool(mem_source, 64, 100);

    assert(pool.get_object_size() == 64);
    assert(pool.get_capacity() == 100);
    assert(pool.get_used_count() == 0);
    assert(pool.get_free_count() == 100);
    assert(pool.is_empty());
    assert(!pool.is_full());

    std::cout << "✓ PoolAllocator 创建测试通过！" << std::endl;
}

void test_simple_allocation() {
    std::cout << "\n测试简单分配..." << std::endl;

    MemorySource mem_source;
    PoolAllocator pool(mem_source, 128, 50);

    void* ptr = pool.allocate(128);
    assert(ptr != nullptr);
    assert(pool.owns(ptr));
    assert(pool.get_used_count() == 1);
    assert(pool.get_free_count() == 49);

    pool.deallocate(ptr, 128);
    assert(pool.get_used_count() == 0);
    assert(pool.get_free_count() == 50);

    std::cout << "✓ 简单分配测试通过！" << std::endl;
}

void test_fill_pool() {
    std::cout << "\n测试填满池..." << std::endl;

    MemorySource mem_source;
    const size_t capacity = 20;
    PoolAllocator pool(mem_source, 64, capacity);

    std::vector<void*> pointers;

    // 填满池
    for (size_t i = 0; i < capacity; ++i) {
        void* ptr = pool.allocate(64);
        assert(ptr != nullptr);
        pointers.push_back(ptr);
    }

    assert(pool.is_full());
    assert(pool.get_used_count() == capacity);

    // 尝试再分配（应该失败）
    void* overflow = pool.allocate(64);
    assert(overflow == nullptr);

    // 清空池
    for (void* ptr : pointers) {
        pool.deallocate(ptr, 64);
    }

    assert(pool.is_empty());

    std::cout << "✓ 填满池测试通过！" << std::endl;
}

void test_alternating_alloc_dealloc() {
    std::cout << "\n测试交替分配释放..." << std::endl;

    MemorySource mem_source;
    PoolAllocator pool(mem_source, 32, 10);

    std::vector<void*> pointers;

    // 分配5个
    for (int i = 0; i < 5; ++i) {
        void* ptr = pool.allocate(32);
        assert(ptr != nullptr);
        pointers.push_back(ptr);
    }

    assert(pool.get_used_count() == 5);

    // 释放第1、3个（索引0、2）
    pool.deallocate(pointers[0], 32);
    pool.deallocate(pointers[2], 32);

    assert(pool.get_used_count() == 3);

    // 再分配2个（应该重用刚释放的）
    void* new1 = pool.allocate(32);
    void* new2 = pool.allocate(32);

    assert(new1 != nullptr);
    assert(new2 != nullptr);
    assert(pool.get_used_count() == 5);

    // 清理
    pool.deallocate(pointers[1], 32);
    pool.deallocate(pointers[3], 32);
    pool.deallocate(pointers[4], 32);
    pool.deallocate(new1, 32);
    pool.deallocate(new2, 32);

    assert(pool.is_empty());

    std::cout << "✓ 交替分配释放测试通过！" << std::endl;
}

void test_pattern_write() {
    std::cout << "\n测试数据写入..." << std::endl;

    MemorySource mem_source;
    PoolAllocator pool(mem_source, 256, 10);

    struct TestData {
        int id;
        double value;
        char name[32];
    };

    static_assert(sizeof(TestData) <= 256, "TestData 太大");

    std::vector<void*> pointers;

    // 分配并写入数据
    for (int i = 0; i < 5; ++i) {
        void* ptr = pool.allocate(sizeof(TestData));
        assert(ptr != nullptr);

        TestData* data = static_cast<TestData*>(ptr);
        data->id = i;
        data->value = i * 3.14;
        snprintf(data->name, sizeof(data->name), "Object_%d", i);

        pointers.push_back(ptr);
    }

    // 验证数据
    for (size_t i = 0; i < pointers.size(); ++i) {
        TestData* data = static_cast<TestData*>(pointers[i]);
        assert(data->id == static_cast<int>(i));
        assert(data->value == i * 3.14);

        char expected[32];
        snprintf(expected, sizeof(expected), "Object_%zu", i);
        assert(strcmp(data->name, expected) == 0);
    }

    // 清理
    for (void* ptr : pointers) {
        pool.deallocate(ptr, sizeof(TestData));
    }

    std::cout << "✓ 数据写入测试通过！" << std::endl;
}

void test_statistics() {
    std::cout << "\n测试统计信息..." << std::endl;

    MemorySource mem_source;
    PoolAllocator pool(mem_source, 128, 10);

    auto stats = pool.get_stats();
    assert(stats.allocation_count == 0);
    assert(stats.deallocation_count == 0);

    void* ptr1 = pool.allocate(128);
    void* ptr2 = pool.allocate(128);

    stats = pool.get_stats();
    assert(stats.allocation_count == 2);
    assert(stats.current_usage == 256);

    pool.deallocate(ptr1, 128);
    pool.deallocate(ptr2, 128);

    stats = pool.get_stats();
    assert(stats.deallocation_count == 2);
    assert(stats.current_usage == 0);
    assert(stats.fragmentation_ratio == 0.0); // Pool 无碎片

    std::cout << "✓ 统计信息测试通过！" << std::endl;
}

void test_validation() {
    std::cout << "\n测试验证功能..." << std::endl;

    MemorySource mem_source;
    PoolAllocator pool(mem_source, 64, 20);

    assert(pool.validate());

    void* ptr1 = pool.allocate(64);
    void* ptr2 = pool.allocate(64);

    assert(pool.validate());

    pool.deallocate(ptr1, 64);

    assert(pool.validate());

    pool.deallocate(ptr2, 64);

    assert(pool.validate());

    std::cout << "✓ 验证功能测试通过！" << std::endl;
}

void test_dump_state() {
    std::cout << "\n测试状态转储..." << std::endl;

    MemorySource mem_source;
    PoolAllocator pool(mem_source, 32, 64);

    // 分配一些对象
    void* ptr1 = pool.allocate(32);
    void* ptr2 = pool.allocate(32);
    void* ptr3 = pool.allocate(32);

    std::cout << "\n--- PoolAllocator 状态 ---" << std::endl;
    pool.dump_pool();

    pool.deallocate(ptr1, 32);
    pool.deallocate(ptr2, 32);
    pool.deallocate(ptr3, 32);

    std::cout << "✓ 状态转储测试通过！" << std::endl;
}

void test_size_mismatch() {
    std::cout << "\n测试大小不匹配..." << std::endl;

    MemorySource mem_source;
    PoolAllocator pool(mem_source, 64, 10);

    // 尝试分配超过对象大小的内存
    void* ptr = pool.allocate(128);
    assert(ptr == nullptr); // 应该失败

    auto stats = pool.get_stats();
    assert(stats.failed_allocations == 1);

    std::cout << "✓ 大小不匹配测试通过！" << std::endl;
}

void test_large_pool() {
    std::cout << "\n测试大容量池..." << std::endl;

    MemorySource mem_source;
    const size_t large_capacity = 1000;
    PoolAllocator pool(mem_source, 128, large_capacity);

    std::vector<void*> pointers;

    // 分配一半
    for (size_t i = 0; i < large_capacity / 2; ++i) {
        void* ptr = pool.allocate(128);
        assert(ptr != nullptr);
        pointers.push_back(ptr);
    }

    assert(pool.get_used_count() == large_capacity / 2);

    // 清理
    for (void* ptr : pointers) {
        pool.deallocate(ptr, 128);
    }

    assert(pool.is_empty());

    std::cout << "✓ 大容量池测试通过！" << std::endl;
}

int main() {
    std::cout << "=== PoolAllocator 测试 ===" << std::endl;

    try {
        test_pool_creation();
        test_simple_allocation();
        test_fill_pool();
        test_alternating_alloc_dealloc();
        test_pattern_write();
        test_statistics();
        test_validation();
        test_dump_state();
        test_size_mismatch();
        test_large_pool();

        std::cout << "\n✓ 所有 PoolAllocator 测试通过！" << std::endl;
        std::cout << "Pool 分配器已准备就绪。" << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "测试失败，异常：" << e.what() << std::endl;
        return 1;
    }
}
