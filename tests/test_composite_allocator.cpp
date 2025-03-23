#include "axontzz/composite_allocator.h"
#include "axontzz/memory_source.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <cstring>

using namespace memplumber;

void test_composite_creation() {
    std::cout << "测试 CompositeAllocator 创建..." << std::endl;

    MemorySource mem_source;
    CompositeAllocator allocator(mem_source);

    assert(allocator.get_small_threshold() == 4096);
    assert(allocator.get_large_threshold() == 256 * 1024);

    auto stats = allocator.get_stats();
    assert(stats.allocation_count == 0);
    assert(stats.current_usage == 0);

    std::cout << "✓ CompositeAllocator 创建测试通过！" << std::endl;
}

void test_small_objects() {
    std::cout << "\n测试小对象分配（通过 Slab）..." << std::endl;

    MemorySource mem_source;
    CompositeAllocator allocator(mem_source);

    // 分配各种小对象
    std::vector<void*> pointers;
    size_t sizes[] = {8, 16, 32, 64, 128, 256, 512, 1024, 2048};

    for (size_t size : sizes) {
        void* ptr = allocator.allocate(size);
        assert(ptr != nullptr);
        assert(allocator.owns(ptr));

        // 写入数据验证
        std::memset(ptr, 0xAB, size);

        pointers.push_back(ptr);
    }

    auto stats = allocator.get_stats();
    assert(stats.allocation_count == sizeof(sizes) / sizeof(sizes[0]));

    // 清理
    for (size_t i = 0; i < pointers.size(); ++i) {
        allocator.deallocate(pointers[i], sizes[i]);
    }

    stats = allocator.get_stats();
    assert(stats.current_usage == 0);

    std::cout << "✓ 小对象测试通过！" << std::endl;
}

void test_medium_objects() {
    std::cout << "\n测试中等对象分配（通过 FreeList）..." << std::endl;

    MemorySource mem_source;
    CompositeAllocator allocator(mem_source);

    // 分配中等大小的对象（5KB - 200KB）
    std::vector<std::pair<size_t, void*>> allocations;
    size_t sizes[] = {5 * 1024, 16 * 1024, 64 * 1024, 128 * 1024, 200 * 1024};

    for (size_t size : sizes) {
        void* ptr = allocator.allocate(size);
        assert(ptr != nullptr);
        assert(allocator.owns(ptr));

        // 写入模式
        char* data = static_cast<char*>(ptr);
        for (size_t i = 0; i < std::min(size, size_t(1024)); ++i) {
            data[i] = static_cast<char>(i & 0xFF);
        }

        allocations.push_back({size, ptr});
    }

    auto stats = allocator.get_stats();
    assert(stats.allocation_count == sizeof(sizes) / sizeof(sizes[0]));

    // 验证数据
    for (const auto& alloc : allocations) {
        char* data = static_cast<char*>(alloc.second);
        for (size_t i = 0; i < std::min(alloc.first, size_t(1024)); ++i) {
            assert(data[i] == static_cast<char>(i & 0xFF));
        }
    }

    // 清理
    for (const auto& alloc : allocations) {
        allocator.deallocate(alloc.second, alloc.first);
    }

    std::cout << "✓ 中等对象测试通过！" << std::endl;
}

void test_large_objects() {
    std::cout << "\n测试大对象分配（直接 mmap）..." << std::endl;

    MemorySource mem_source;
    CompositeAllocator allocator(mem_source);

    // 分配大对象（> 256KB）
    std::vector<std::pair<size_t, void*>> allocations;
    size_t sizes[] = {300 * 1024, 512 * 1024, 1024 * 1024};

    for (size_t size : sizes) {
        void* ptr = allocator.allocate(size);
        assert(ptr != nullptr);
        assert(allocator.owns(ptr));

        // 写入魔术数字
        size_t* magic = static_cast<size_t*>(ptr);
        *magic = 0xDEADBEEF;

        allocations.push_back({size, ptr});
    }

    auto detailed = allocator.get_detailed_stats();
    assert(detailed.large_objects.allocation_count == sizeof(sizes) / sizeof(sizes[0]));

    // 验证数据
    for (const auto& alloc : allocations) {
        size_t* magic = static_cast<size_t*>(alloc.second);
        assert(*magic == 0xDEADBEEF);
    }

    // 清理
    for (const auto& alloc : allocations) {
        allocator.deallocate(alloc.second, alloc.first);
    }

    detailed = allocator.get_detailed_stats();
    assert(detailed.large_objects.current_usage == 0);

    std::cout << "✓ 大对象测试通过！" << std::endl;
}

void test_mixed_allocations() {
    std::cout << "\n测试混合分配（小/中/大）..." << std::endl;

    MemorySource mem_source;
    CompositeAllocator allocator(mem_source);

    std::vector<std::pair<size_t, void*>> allocations;

    // 混合分配各种大小
    size_t sizes[] = {
        16,           // 小
        64,           // 小
        2048,         // 小
        8 * 1024,     // 中
        32 * 1024,    // 中
        128 * 1024,   // 中
        300 * 1024,   // 大
        600 * 1024    // 大
    };

    for (size_t size : sizes) {
        void* ptr = allocator.allocate(size);
        assert(ptr != nullptr);
        assert(allocator.owns(ptr));
        allocations.push_back({size, ptr});
    }

    auto detailed = allocator.get_detailed_stats();
    assert(detailed.small_objects.allocation_count > 0);
    assert(detailed.medium_objects.allocation_count > 0);
    assert(detailed.large_objects.allocation_count > 0);

    std::cout << "分配统计：" << std::endl;
    std::cout << "  小对象：" << detailed.small_objects.allocation_count << " 次" << std::endl;
    std::cout << "  中等对象：" << detailed.medium_objects.allocation_count << " 次" << std::endl;
    std::cout << "  大对象：" << detailed.large_objects.allocation_count << " 次" << std::endl;

    // 清理
    for (const auto& alloc : allocations) {
        allocator.deallocate(alloc.second, alloc.first);
    }

    auto stats = allocator.get_stats();
    assert(stats.current_usage == 0);

    std::cout << "✓ 混合分配测试通过！" << std::endl;
}

void test_interleaved_operations() {
    std::cout << "\n测试交错分配和释放..." << std::endl;

    MemorySource mem_source;
    CompositeAllocator allocator(mem_source);

    std::vector<void*> pointers;

    // 分配一些对象
    for (int i = 0; i < 10; ++i) {
        size_t size = (i + 1) * 1024; // 1KB到10KB
        void* ptr = allocator.allocate(size);
        assert(ptr != nullptr);
        pointers.push_back(ptr);
    }

    // 释放部分对象
    allocator.deallocate(pointers[2], 3 * 1024);
    allocator.deallocate(pointers[5], 6 * 1024);
    allocator.deallocate(pointers[8], 9 * 1024);

    // 再分配一些新对象
    void* new1 = allocator.allocate(2 * 1024);
    void* new2 = allocator.allocate(7 * 1024);

    assert(new1 != nullptr);
    assert(new2 != nullptr);

    // 清理所有
    allocator.deallocate(pointers[0], 1 * 1024);
    allocator.deallocate(pointers[1], 2 * 1024);
    allocator.deallocate(pointers[3], 4 * 1024);
    allocator.deallocate(pointers[4], 5 * 1024);
    allocator.deallocate(pointers[6], 7 * 1024);
    allocator.deallocate(pointers[7], 8 * 1024);
    allocator.deallocate(pointers[9], 10 * 1024);
    allocator.deallocate(new1, 2 * 1024);
    allocator.deallocate(new2, 7 * 1024);

    std::cout << "✓ 交错操作测试通过！" << std::endl;
}

void test_custom_thresholds() {
    std::cout << "\n测试自定义阈值..." << std::endl;

    MemorySource mem_source;
    // 自定义阈值：小对象≤2KB，大对象>128KB
    CompositeAllocator allocator(mem_source, 2048, 128 * 1024);

    assert(allocator.get_small_threshold() == 2048);
    assert(allocator.get_large_threshold() == 128 * 1024);

    // 测试边界情况
    void* ptr1 = allocator.allocate(2048);    // 小对象边界
    void* ptr2 = allocator.allocate(2049);    // 中等对象边界
    void* ptr3 = allocator.allocate(128 * 1024);     // 中等对象边界
    void* ptr4 = allocator.allocate(128 * 1024 + 1); // 大对象边界

    assert(ptr1 != nullptr);
    assert(ptr2 != nullptr);
    assert(ptr3 != nullptr);
    assert(ptr4 != nullptr);

    allocator.deallocate(ptr1, 2048);
    allocator.deallocate(ptr2, 2049);
    allocator.deallocate(ptr3, 128 * 1024);
    allocator.deallocate(ptr4, 128 * 1024 + 1);

    std::cout << "✓ 自定义阈值测试通过！" << std::endl;
}

void test_detailed_stats() {
    std::cout << "\n测试详细统计信息..." << std::endl;

    MemorySource mem_source;
    CompositeAllocator allocator(mem_source);

    // 各分配一些对象
    void* small = allocator.allocate(512);
    void* medium = allocator.allocate(64 * 1024);
    void* large = allocator.allocate(512 * 1024);

    auto detailed = allocator.get_detailed_stats();

    assert(detailed.small_objects.allocation_count > 0);
    assert(detailed.medium_objects.allocation_count > 0);
    assert(detailed.large_objects.allocation_count > 0);

    assert(detailed.aggregate.allocation_count ==
           detailed.small_objects.allocation_count +
           detailed.medium_objects.allocation_count +
           detailed.large_objects.allocation_count);

    allocator.deallocate(small, 512);
    allocator.deallocate(medium, 64 * 1024);
    allocator.deallocate(large, 512 * 1024);

    std::cout << "✓ 详细统计信息测试通过！" << std::endl;
}

void test_dump_state() {
    std::cout << "\n测试状态转储..." << std::endl;

    MemorySource mem_source;
    CompositeAllocator allocator(mem_source);

    // 分配一些对象
    void* ptr1 = allocator.allocate(100);
    void* ptr2 = allocator.allocate(10 * 1024);
    void* ptr3 = allocator.allocate(500 * 1024);

    std::cout << "\n--- CompositeAllocator 状态 ---" << std::endl;
    allocator.dump_allocator_state();

    allocator.deallocate(ptr1, 100);
    allocator.deallocate(ptr2, 10 * 1024);
    allocator.deallocate(ptr3, 500 * 1024);

    std::cout << "✓ 状态转储测试通过！" << std::endl;
}

void test_validation() {
    std::cout << "\n测试验证功能..." << std::endl;

    MemorySource mem_source;
    CompositeAllocator allocator(mem_source);

    // 初始状态应该有效
    assert(allocator.validate());

    // 分配一些对象
    void* ptr1 = allocator.allocate(256);
    void* ptr2 = allocator.allocate(16 * 1024);

    assert(allocator.validate());

    // 释放对象
    allocator.deallocate(ptr1, 256);
    allocator.deallocate(ptr2, 16 * 1024);

    assert(allocator.validate());

    std::cout << "✓ 验证功能测试通过！" << std::endl;
}

int main() {
    std::cout << "=== CompositeAllocator 测试 ===" << std::endl;

    try {
        test_composite_creation();
        test_small_objects();
        test_medium_objects();
        test_large_objects();
        test_mixed_allocations();
        test_interleaved_operations();
        test_custom_thresholds();
        test_detailed_stats();
        test_dump_state();
        test_validation();

        std::cout << "\n✓ 所有 CompositeAllocator 测试通过！" << std::endl;
        std::cout << "组合分配器已准备就绪，可进入生产环境。" << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "测试失败，异常：" << e.what() << std::endl;
        return 1;
    }
}
