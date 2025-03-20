#pragma once

#include "allocator_interface.h"
#include "memory_source.h"
#include "slab_allocator.h"
#include "free_list_allocator.h"
#include <cstddef>

namespace memplumber {

/**
 * 组合分配器（Composite Allocator）
 *
 * 这是一个智能路由分配器，根据请求的大小自动选择最合适的底层分配器。
 * 类似于 jemalloc 和 tcmalloc 的分层策略。
 *
 * 分配策略：
 * - 小对象（≤ 4KB）：使用 SlabCache（O(1) 性能，低碎片）
 * - 中等对象（4KB - 256KB）：使用 FreeListAllocator（灵活的可变大小）
 * - 大对象（> 256KB）：直接使用 mmap（避免内部碎片）
 *
 * 核心优势：
 * - 自动选择最优分配策略
 * - 为不同工作负载优化
 * - 统一的接口，隐藏底层复杂性
 * - 聚合统计信息跨所有分配器
 *
 * 使用场景：
 * - 通用内存分配（替代 malloc/new）
 * - 混合工作负载（小/中/大对象混合）
 * - 需要自动优化的应用
 */
class CompositeAllocator : public AllocatorInterface {
public:
    /**
     * 构造函数
     * @param memory_source: 底层内存源（用于所有子分配器）
     * @param small_threshold: 小对象阈值（默认：4KB）
     * @param large_threshold: 大对象阈值（默认：256KB）
     */
    explicit CompositeAllocator(MemorySource& memory_source,
                               size_t small_threshold = 4096,
                               size_t large_threshold = 256 * 1024);

    ~CompositeAllocator() override;

    // AllocatorInterface 实现
    void* allocate(size_t size, size_t alignment = sizeof(void*)) override;
    void deallocate(void* ptr, size_t size = 0) override;
    bool owns(void* ptr) const override;
    AllocatorStats get_stats() const override;
    void reset_stats() override;
    const char* get_name() const override { return "CompositeAllocator"; }

    /**
     * 获取各分配器的独立统计信息
     */
    struct DetailedStats {
        AllocatorStats small_objects;   // 小对象（Slab）统计
        AllocatorStats medium_objects;  // 中等对象（FreeList）统计
        AllocatorStats large_objects;   // 大对象（直接mmap）统计
        AllocatorStats aggregate;       // 聚合统计
    };

    DetailedStats get_detailed_stats() const;

    /**
     * 获取阈值配置
     */
    size_t get_small_threshold() const { return small_threshold_; }
    size_t get_large_threshold() const { return large_threshold_; }

    /**
     * 转储详细信息
     */
    void dump_allocator_state() const;

    /**
     * 验证所有子分配器状态
     */
    bool validate() const;

private:
    /**
     * 分配器类型枚举
     */
    enum class AllocatorType {
        SLAB,       // 小对象
        FREELIST,   // 中等对象
        DIRECT      // 大对象（直接mmap）
    };

    /**
     * 大对象分配的元数据
     * 存储在分配块的开头
     */
    struct LargeBlockHeader {
        size_t size;           // 分配的大小
        size_t total_size;     // 包含头部的总大小
        AllocatorType type;    // 标记为 DIRECT
    };

    MemorySource& memory_source_;
    SlabCache* slab_cache_;              // 小对象分配器
    FreeListAllocator* free_list_;       // 中等对象分配器

    size_t small_threshold_;             // 小对象/中等对象边界
    size_t large_threshold_;             // 中等对象/大对象边界

    // 大对象统计（直接mmap的）
    AllocatorStats large_stats_;

    // 辅助方法

    /**
     * 确定应该使用哪个分配器
     */
    AllocatorType determine_allocator_type(size_t size) const;

    /**
     * 直接从OS分配大对象
     */
    void* allocate_large(size_t size, size_t alignment);

    /**
     * 释放直接分配的大对象
     */
    void deallocate_large(void* ptr);

    /**
     * 检查指针是否是大对象
     */
    bool is_large_object(void* ptr) const;

    // 禁用拷贝
    CompositeAllocator(const CompositeAllocator&) = delete;
    CompositeAllocator& operator=(const CompositeAllocator&) = delete;
};

} // namespace memplumber
