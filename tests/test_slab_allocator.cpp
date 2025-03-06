#include "axontzz/slab_allocator.h"
#include "axontzz/memory_source.h"
#include <iostream>
#include <vector>
#include <cassert>

using namespace memplumber;

// 简单测试对象结构
struct TestObject {
    int id;
    double value;
    char data[16];
};

void test_slab_allocator_creation() {
    std::cout << "测试 SlabAllocator 创建..." << std::endl;

    MemorySource mem_source;
    SlabAllocator allocator(mem_source, sizeof(TestObject), 32);

    assert(allocator.get_object_size() == sizeof(TestObject));
    assert(allocator.get_objects_per_slab() == 32);
    assert(allocator.get_slab_count() == 1); // 应该有一个初始 slab

    auto stats = allocator.get_stats();
    assert(stats.allocation_count == 0);
    assert(stats.current_usage == 0);

    std::cout << "✓ SlabAllocator 创建测试通过！" << std::endl;
}

void test_simple_allocation() {
    std::cout << "\n测试简单分配..." << std::endl;

    MemorySource mem_source;
    SlabAllocator allocator(mem_source, sizeof(TestObject), 32);

    // 分配单个对象
    void* ptr = allocator.allocate(sizeof(TestObject));
    assert(ptr != nullptr);
    assert(allocator.owns(ptr));

    auto stats = allocator.get_stats();
    assert(stats.allocation_count == 1);
    assert(stats.current_usage == sizeof(TestObject));

    // 写入数据
    TestObject* obj = static_cast<TestObject*>(ptr);
    obj->id = 42;
    obj->value = 3.14159;

    // 验证数据
    assert(obj->id == 42);
    assert(obj->value == 3.14159);

    // 释放对象
    allocator.deallocate(ptr, sizeof(TestObject));

    stats = allocator.get_stats();
    assert(stats.deallocation_count == 1);
    assert(stats.current_usage == 0);

    std::cout << "✓ 简单分配测试通过！" << std::endl;
}

void test_multiple_allocations() {
    std::cout << "\n测试多次分配..." << std::endl;

    MemorySource mem_source;
    SlabAllocator allocator(mem_source, sizeof(TestObject), 16);

    const int num_objects = 10;
    std::vector<void*> pointers;

    // 分配多个对象
    for (int i = 0; i < num_objects; ++i) {
        void* ptr = allocator.allocate(sizeof(TestObject));
        assert(ptr != nullptr);
        assert(allocator.owns(ptr));

        TestObject* obj = static_cast<TestObject*>(ptr);
        obj->id = i;
        obj->value = i * 1.5;

        pointers.push_back(ptr);
    }

    auto stats = allocator.get_stats();
    assert(stats.allocation_count == num_objects);
    assert(stats.current_usage == num_objects * sizeof(TestObject));

    // 验证所有对象
    for (int i = 0; i < num_objects; ++i) {
        TestObject* obj = static_cast<TestObject*>(pointers[i]);
        assert(obj->id == i);
        assert(obj->value == i * 1.5);
    }

    // 释放所有对象
    for (void* ptr : pointers) {
        allocator.deallocate(ptr, sizeof(TestObject));
    }

    stats = allocator.get_stats();
    assert(stats.deallocation_count == num_objects);
    assert(stats.current_usage == 0);

    std::cout << "✓ 多次分配测试通过！" << std::endl;
}

void test_slab_expansion() {
    std::cout << "\n测试 slab 扩展..." << std::endl;

    MemorySource mem_source;
    const size_t objects_per_slab = 8;
    SlabAllocator allocator(mem_source, sizeof(TestObject), objects_per_slab);

    assert(allocator.get_slab_count() == 1);

    std::vector<void*> pointers;

    // 分配超过一个 slab 容量的对象
    const size_t total_objects = objects_per_slab + 5;
    for (size_t i = 0; i < total_objects; ++i) {
        void* ptr = allocator.allocate(sizeof(TestObject));
        assert(ptr != nullptr);
        pointers.push_back(ptr);
    }

    // 应该至少有 2 个 slabs
    assert(allocator.get_slab_count() >= 2);

    auto stats = allocator.get_stats();
    assert(stats.allocation_count == total_objects);

    // 清理
    for (void* ptr : pointers) {
        allocator.deallocate(ptr, sizeof(TestObject));
    }

    std::cout << "✓ Slab 扩展测试通过！" << std::endl;
}

void test_memory_reuse() {
    std::cout << "\n测试内存重用..." << std::endl;

    MemorySource mem_source;
    SlabAllocator allocator(mem_source, sizeof(TestObject), 16);

    // 第一次分配
    void* ptr1 = allocator.allocate(sizeof(TestObject));
    assert(ptr1 != nullptr);

    // 释放
    allocator.deallocate(ptr1, sizeof(TestObject));

    // 第二次分配应该重用相同的内存
    void* ptr2 = allocator.allocate(sizeof(TestObject));
    assert(ptr2 != nullptr);
    assert(ptr2 == ptr1); // 应该是相同的地址

    allocator.deallocate(ptr2, sizeof(TestObject));

    std::cout << "✓ 内存重用测试通过！" << std::endl;
}

void test_interleaved_alloc_dealloc() {
    std::cout << "\n测试交错分配和释放..." << std::endl;

    MemorySource mem_source;
    SlabAllocator allocator(mem_source, sizeof(TestObject), 16);

    std::vector<void*> pointers;

    // 分配一些对象
    for (int i = 0; i < 5; ++i) {
        void* ptr = allocator.allocate(sizeof(TestObject));
        assert(ptr != nullptr);
        pointers.push_back(ptr);
    }

    // 释放中间的一些对象
    allocator.deallocate(pointers[1], sizeof(TestObject));
    allocator.deallocate(pointers[3], sizeof(TestObject));

    // 再分配新对象（应该重用刚释放的槽位）
    void* new_ptr1 = allocator.allocate(sizeof(TestObject));
    void* new_ptr2 = allocator.allocate(sizeof(TestObject));

    assert(new_ptr1 != nullptr);
    assert(new_ptr2 != nullptr);

    // 清理
    allocator.deallocate(pointers[0], sizeof(TestObject));
    allocator.deallocate(pointers[2], sizeof(TestObject));
    allocator.deallocate(pointers[4], sizeof(TestObject));
    allocator.deallocate(new_ptr1, sizeof(TestObject));
    allocator.deallocate(new_ptr2, sizeof(TestObject));

    auto stats = allocator.get_stats();
    assert(stats.current_usage == 0);

    std::cout << "✓ 交错分配释放测试通过！" << std::endl;
}

void test_slab_cache_creation() {
    std::cout << "\n测试 SlabCache 创建..." << std::endl;

    MemorySource mem_source;
    SlabCache cache(mem_source);

    assert(cache.get_num_classes() == 9); // 默认 9 个尺寸类

    auto stats = cache.get_stats();
    assert(stats.allocation_count == 0);

    std::cout << "✓ SlabCache 创建测试通过！" << std::endl;
}

void test_slab_cache_routing() {
    std::cout << "\n测试 SlabCache 路由..." << std::endl;

    MemorySource mem_source;
    SlabCache cache(mem_source);

    // 测试不同大小的分配
    std::vector<std::pair<size_t, void*>> allocations;

    // 小对象
    void* ptr1 = cache.allocate(8);
    assert(ptr1 != nullptr);
    allocations.push_back({8, ptr1});

    // 中等对象
    void* ptr2 = cache.allocate(100);
    assert(ptr2 != nullptr);
    allocations.push_back({100, ptr2});

    // 大对象
    void* ptr3 = cache.allocate(2000);
    assert(ptr3 != nullptr);
    allocations.push_back({2000, ptr3});

    // 验证所有权
    for (const auto& alloc : allocations) {
        assert(cache.owns(alloc.second));
    }

    // 清理
    for (const auto& alloc : allocations) {
        cache.deallocate(alloc.second, alloc.first);
    }

    auto stats = cache.get_stats();
    assert(stats.current_usage == 0);

    std::cout << "✓ SlabCache 路由测试通过！" << std::endl;
}

void test_slab_cache_mixed_sizes() {
    std::cout << "\n测试 SlabCache 混合大小..." << std::endl;

    MemorySource mem_source;
    SlabCache cache(mem_source);

    std::vector<std::pair<size_t, void*>> allocations;

    // 分配各种大小
    size_t sizes[] = {16, 32, 64, 128, 256, 512, 1024};
    for (size_t size : sizes) {
        for (int i = 0; i < 3; ++i) {
            void* ptr = cache.allocate(size);
            assert(ptr != nullptr);
            allocations.push_back({size, ptr});
        }
    }

    auto stats = cache.get_stats();
    assert(stats.allocation_count == sizeof(sizes) / sizeof(sizes[0]) * 3);

    // 释放所有
    for (const auto& alloc : allocations) {
        cache.deallocate(alloc.second, alloc.first);
    }

    stats = cache.get_stats();
    assert(stats.current_usage == 0);

    std::cout << "✓ SlabCache 混合大小测试通过！" << std::endl;
}

void test_validation() {
    std::cout << "\n测试验证功能..." << std::endl;

    MemorySource mem_source;
    SlabAllocator allocator(mem_source, sizeof(TestObject), 16);

    // 初始状态应该有效
    assert(allocator.validate());

    // 分配一些对象
    std::vector<void*> pointers;
    for (int i = 0; i < 5; ++i) {
        void* ptr = allocator.allocate(sizeof(TestObject));
        pointers.push_back(ptr);
    }

    // 仍然应该有效
    assert(allocator.validate());

    // 释放对象
    for (void* ptr : pointers) {
        allocator.deallocate(ptr, sizeof(TestObject));
    }

    // 仍然应该有效
    assert(allocator.validate());

    std::cout << "✓ 验证功能测试通过！" << std::endl;
}

void test_dump_functions() {
    std::cout << "\n测试转储功能..." << std::endl;

    MemorySource mem_source;
    SlabAllocator allocator(mem_source, sizeof(TestObject), 8);

    // 分配一些对象
    void* ptr1 = allocator.allocate(sizeof(TestObject));
    void* ptr2 = allocator.allocate(sizeof(TestObject));

    std::cout << "\n--- SlabAllocator 状态 ---" << std::endl;
    allocator.dump_slabs();

    allocator.deallocate(ptr1, sizeof(TestObject));
    allocator.deallocate(ptr2, sizeof(TestObject));

    // 测试 SlabCache
    SlabCache cache(mem_source);
    void* cptr1 = cache.allocate(64);
    void* cptr2 = cache.allocate(256);

    std::cout << "\n--- SlabCache 状态 ---" << std::endl;
    cache.dump_cache_stats();

    cache.deallocate(cptr1, 64);
    cache.deallocate(cptr2, 256);

    std::cout << "✓ 转储功能测试通过！" << std::endl;
}

int main() {
    std::cout << "=== Slab 分配器测试 ===" << std::endl;

    try {
        test_slab_allocator_creation();
        test_simple_allocation();
        test_multiple_allocations();
        test_slab_expansion();
        test_memory_reuse();
        test_interleaved_alloc_dealloc();
        test_slab_cache_creation();
        test_slab_cache_routing();
        test_slab_cache_mixed_sizes();
        test_validation();
        test_dump_functions();

        std::cout << "\n✓ 所有 Slab 分配器测试通过！" << std::endl;
        std::cout << "准备进入下一轮开发。" << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "测试失败，异常：" << e.what() << std::endl;
        return 1;
    }
}
