#include "axontzz/composite_allocator.h"
#include <iostream>
#include <cassert>

namespace memplumber {

CompositeAllocator::CompositeAllocator(MemorySource& memory_source,
                                       size_t small_threshold,
                                       size_t large_threshold)
    : memory_source_(memory_source)
    , slab_cache_(nullptr)
    , free_list_(nullptr)
    , small_threshold_(small_threshold)
    , large_threshold_(large_threshold)
    , large_stats_{} {

    std::cout << "CompositeAllocator 创建：" << std::endl;
    std::cout << "  小对象阈值：" << small_threshold_ << " 字节" << std::endl;
    std::cout << "  大对象阈值：" << large_threshold_ << " 字节" << std::endl;

    // 验证阈值
    assert(small_threshold_ < large_threshold_);
    assert(small_threshold_ > 0);

    // 创建子分配器
    std::cout << "  创建 SlabCache（小对象）..." << std::endl;
    slab_cache_ = new SlabCache(memory_source_);

    std::cout << "  创建 FreeListAllocator（中等对象）..." << std::endl;
    free_list_ = new FreeListAllocator(memory_source_, 512 * 1024); // 512KB 初始块，支持更大的分配

    std::cout << "CompositeAllocator 初始化完成" << std::endl;
}

CompositeAllocator::~CompositeAllocator() {
    std::cout << "CompositeAllocator 销毁" << std::endl;

    // 删除子分配器
    delete slab_cache_;
    delete free_list_;

    // 注意：大对象应该已经被释放
    if (large_stats_.current_usage > 0) {
        std::cout << "警告：仍有 " << large_stats_.current_usage
                  << " 字节的大对象未释放" << std::endl;
    }
}

void* CompositeAllocator::allocate(size_t size, size_t alignment) {
    if (size == 0) {
        return nullptr;
    }

    AllocatorType type = determine_allocator_type(size);
    void* ptr = nullptr;

    switch (type) {
        case AllocatorType::SLAB:
            std::cout << "路由到 SlabCache：" << size << " 字节" << std::endl;
            ptr = slab_cache_->allocate(size, alignment);
            break;

        case AllocatorType::FREELIST:
            std::cout << "路由到 FreeListAllocator：" << size << " 字节" << std::endl;
            ptr = free_list_->allocate(size, alignment);
            break;

        case AllocatorType::DIRECT:
            std::cout << "路由到直接分配（mmap）：" << size << " 字节" << std::endl;
            ptr = allocate_large(size, alignment);
            break;
    }

    return ptr;
}

void CompositeAllocator::deallocate(void* ptr, size_t size) {
    if (ptr == nullptr) {
        return;
    }

    // 尝试按顺序检查各个分配器
    // 1. 检查是否是大对象
    if (is_large_object(ptr)) {
        std::cout << "释放大对象：" << ptr << std::endl;
        deallocate_large(ptr);
        return;
    }

    // 2. 检查 SlabCache
    if (slab_cache_->owns(ptr)) {
        std::cout << "从 SlabCache 释放：" << ptr << std::endl;
        slab_cache_->deallocate(ptr, size);
        return;
    }

    // 3. 检查 FreeListAllocator
    if (free_list_->owns(ptr)) {
        std::cout << "从 FreeListAllocator 释放：" << ptr << std::endl;
        free_list_->deallocate(ptr, size);
        return;
    }

    std::cout << "警告：无法确定指针 " << ptr << " 的所有者" << std::endl;
}

bool CompositeAllocator::owns(void* ptr) const {
    if (ptr == nullptr) {
        return false;
    }

    return is_large_object(ptr) ||
           slab_cache_->owns(ptr) ||
           free_list_->owns(ptr);
}

AllocatorInterface::AllocatorStats CompositeAllocator::get_stats() const {
    auto slab_stats = slab_cache_->get_stats();
    auto freelist_stats = free_list_->get_stats();

    AllocatorStats aggregate;
    aggregate.total_allocated = slab_stats.total_allocated +
                                freelist_stats.total_allocated +
                                large_stats_.total_allocated;
    aggregate.total_deallocated = slab_stats.total_deallocated +
                                  freelist_stats.total_deallocated +
                                  large_stats_.total_deallocated;
    aggregate.current_usage = slab_stats.current_usage +
                             freelist_stats.current_usage +
                             large_stats_.current_usage;
    aggregate.allocation_count = slab_stats.allocation_count +
                                freelist_stats.allocation_count +
                                large_stats_.allocation_count;
    aggregate.deallocation_count = slab_stats.deallocation_count +
                                   freelist_stats.deallocation_count +
                                   large_stats_.deallocation_count;
    aggregate.failed_allocations = slab_stats.failed_allocations +
                                   freelist_stats.failed_allocations +
                                   large_stats_.failed_allocations;

    // 计算加权平均碎片率
    size_t total_usage = aggregate.current_usage;
    if (total_usage > 0) {
        aggregate.fragmentation_ratio =
            (slab_stats.fragmentation_ratio * slab_stats.current_usage +
             freelist_stats.fragmentation_ratio * freelist_stats.current_usage +
             large_stats_.fragmentation_ratio * large_stats_.current_usage) / total_usage;
    } else {
        aggregate.fragmentation_ratio = 0.0;
    }

    return aggregate;
}

void CompositeAllocator::reset_stats() {
    slab_cache_->reset_stats();
    free_list_->reset_stats();
    large_stats_ = AllocatorStats{};
}

CompositeAllocator::DetailedStats CompositeAllocator::get_detailed_stats() const {
    DetailedStats stats;
    stats.small_objects = slab_cache_->get_stats();
    stats.medium_objects = free_list_->get_stats();
    stats.large_objects = large_stats_;
    stats.aggregate = get_stats();
    return stats;
}

void CompositeAllocator::dump_allocator_state() const {
    std::cout << "=== CompositeAllocator 状态转储 ===" << std::endl;
    std::cout << "配置：" << std::endl;
    std::cout << "  小对象阈值：" << small_threshold_ << " 字节" << std::endl;
    std::cout << "  大对象阈值：" << large_threshold_ << " 字节" << std::endl;
    std::cout << std::endl;

    auto detailed = get_detailed_stats();

    std::cout << "聚合统计：" << std::endl;
    std::cout << "  总分配量：" << detailed.aggregate.total_allocated << " 字节" << std::endl;
    std::cout << "  总释放量：" << detailed.aggregate.total_deallocated << " 字节" << std::endl;
    std::cout << "  当前使用量：" << detailed.aggregate.current_usage << " 字节" << std::endl;
    std::cout << "  分配次数：" << detailed.aggregate.allocation_count << std::endl;
    std::cout << "  释放次数：" << detailed.aggregate.deallocation_count << std::endl;
    std::cout << "  失败分配：" << detailed.aggregate.failed_allocations << std::endl;
    std::cout << "  碎片率：" << (detailed.aggregate.fragmentation_ratio * 100.0) << "%" << std::endl;
    std::cout << std::endl;

    std::cout << "小对象（SlabCache）：" << std::endl;
    std::cout << "  当前使用量：" << detailed.small_objects.current_usage << " 字节" << std::endl;
    std::cout << "  分配次数：" << detailed.small_objects.allocation_count << std::endl;
    std::cout << "  碎片率：" << (detailed.small_objects.fragmentation_ratio * 100.0) << "%" << std::endl;
    std::cout << std::endl;

    std::cout << "中等对象（FreeList）：" << std::endl;
    std::cout << "  当前使用量：" << detailed.medium_objects.current_usage << " 字节" << std::endl;
    std::cout << "  分配次数：" << detailed.medium_objects.allocation_count << std::endl;
    std::cout << "  碎片率：" << (detailed.medium_objects.fragmentation_ratio * 100.0) << "%" << std::endl;
    std::cout << std::endl;

    std::cout << "大对象（直接mmap）：" << std::endl;
    std::cout << "  当前使用量：" << detailed.large_objects.current_usage << " 字节" << std::endl;
    std::cout << "  分配次数：" << detailed.large_objects.allocation_count << std::endl;
    std::cout << "  碎片率：" << (detailed.large_objects.fragmentation_ratio * 100.0) << "%" << std::endl;

    std::cout << "===================================" << std::endl;
}

bool CompositeAllocator::validate() const {
    // 验证所有子分配器
    // SlabCache 没有 validate 方法，跳过
    // FreeListAllocator 有 validate_free_list 方法
    return free_list_->validate_free_list();
}

// 私有辅助方法

CompositeAllocator::AllocatorType CompositeAllocator::determine_allocator_type(size_t size) const {
    if (size <= small_threshold_) {
        return AllocatorType::SLAB;
    } else if (size <= large_threshold_) {
        return AllocatorType::FREELIST;
    } else {
        return AllocatorType::DIRECT;
    }
}

void* CompositeAllocator::allocate_large(size_t size, size_t alignment) {
    // 计算总大小（包含头部）
    size_t header_size = sizeof(LargeBlockHeader);
    size_t total_size = header_size + size + alignment;

    // 从OS获取内存
    void* mem = memory_source_.allocate_block(total_size);
    if (mem == nullptr) {
        large_stats_.failed_allocations++;
        return nullptr;
    }

    // 对齐用户指针
    char* aligned_ptr = static_cast<char*>(mem) + header_size;
    if (alignment > 0) {
        uintptr_t addr = reinterpret_cast<uintptr_t>(aligned_ptr);
        uintptr_t aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
        aligned_ptr = reinterpret_cast<char*>(aligned_addr);
    }

    // 写入头部（在用户指针之前）
    LargeBlockHeader* header = reinterpret_cast<LargeBlockHeader*>(aligned_ptr - header_size);
    header->size = size;
    header->total_size = total_size;
    header->type = AllocatorType::DIRECT;

    // 更新统计
    large_stats_.total_allocated += size;
    large_stats_.current_usage += size;
    large_stats_.allocation_count++;

    std::cout << "大对象分配成功：" << size << " 字节，地址 " << (void*)aligned_ptr << std::endl;

    return aligned_ptr;
}

void CompositeAllocator::deallocate_large(void* ptr) {
    if (ptr == nullptr) {
        return;
    }

    // 读取头部
    size_t header_size = sizeof(LargeBlockHeader);
    LargeBlockHeader* header = reinterpret_cast<LargeBlockHeader*>(
        static_cast<char*>(ptr) - header_size);

    // 验证这是一个大对象
    if (header->type != AllocatorType::DIRECT) {
        std::cout << "错误：指针 " << ptr << " 不是大对象" << std::endl;
        return;
    }

    size_t size = header->size;
    size_t total_size = header->total_size;

    // 计算原始内存块的起始位置
    void* mem = static_cast<char*>(ptr) - header_size;

    // 归还给OS
    memory_source_.deallocate_block(mem, total_size);

    // 更新统计
    large_stats_.total_deallocated += size;
    large_stats_.current_usage -= size;
    large_stats_.deallocation_count++;

    std::cout << "大对象释放成功：" << size << " 字节" << std::endl;
}

bool CompositeAllocator::is_large_object(void* ptr) const {
    if (ptr == nullptr) {
        return false;
    }

    // 尝试读取头部
    // 注意：这个检查不是100%可靠，因为可能读取到无效内存
    // 在实际生产环境中，应该维护一个大对象的注册表
    try {
        size_t header_size = sizeof(LargeBlockHeader);
        LargeBlockHeader* header = reinterpret_cast<LargeBlockHeader*>(
            static_cast<char*>(ptr) - header_size);

        return header->type == AllocatorType::DIRECT;
    } catch (...) {
        return false;
    }
}

} // namespace memplumber
