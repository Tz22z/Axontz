#pragma once

#include "allocator_interface.h"
#include "memory_source.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace memplumber {

/**
 * Pool 分配器（对象池）
 *
 * 预分配固定数量的固定大小对象，使用位图管理空闲状态
 * 比 Slab 更简单，但容量固定，适合已知对象数量的场景
 *
 * 核心特性：
 * - O(1) 分配和释放（位图查找）
 * - 零外部碎片（所有对象大小相同）
 * - 零内部碎片（精确的对象大小）
 * - 内存紧凑（位图开销小）
 * - 容量固定（预分配）
 *
 * 数据结构：
 * - 连续的对象数组
 * - 位图标记空闲/已用状态
 * - 简单的线性扫描查找空闲槽位
 *
 * 性能特征：
 * - 分配：O(n) 最坏情况（位图扫描），实际接近 O(1)
 * - 释放：O(1)（直接索引）
 * - 空间开销：每个对象约 1 bit
 *
 * 使用场景：
 * - 连接池（固定数量的连接）
 * - 线程池（固定数量的线程对象）
 * - 对象缓存（已知最大数量）
 * - 游戏实体池（预分配所有实体）
 */
class PoolAllocator : public AllocatorInterface {
public:
    /**
     * 构造函数
     * @param memory_source: 内存源
     * @param object_size: 对象大小（字节）
     * @param pool_capacity: 池容量（对象数量）
     */
    explicit PoolAllocator(MemorySource& memory_source,
                          size_t object_size,
                          size_t pool_capacity);

    ~PoolAllocator() override;

    // AllocatorInterface 实现
    void* allocate(size_t size, size_t alignment = sizeof(void*)) override;
    void deallocate(void* ptr, size_t size = 0) override;
    bool owns(void* ptr) const override;
    AllocatorStats get_stats() const override;
    void reset_stats() override;
    const char* get_name() const override { return "PoolAllocator"; }

    /**
     * 获取对象大小
     */
    size_t get_object_size() const { return object_size_; }

    /**
     * 获取池容量
     */
    size_t get_capacity() const { return pool_capacity_; }

    /**
     * 获取当前使用数量
     */
    size_t get_used_count() const { return used_count_; }

    /**
     * 获取空闲数量
     */
    size_t get_free_count() const { return pool_capacity_ - used_count_; }

    /**
     * 检查池是否已满
     */
    bool is_full() const { return used_count_ >= pool_capacity_; }

    /**
     * 检查池是否为空
     */
    bool is_empty() const { return used_count_ == 0; }

    /**
     * 验证池状态
     */
    bool validate() const;

    /**
     * 转储池状态
     */
    void dump_pool() const;

private:
    /**
     * 位图辅助函数
     */

    // 设置位（标记为已用）
    void set_bit(size_t index);

    // 清除位（标记为空闲）
    void clear_bit(size_t index);

    // 测试位（检查是否已用）
    bool test_bit(size_t index) const;

    // 查找第一个空闲槽位
    size_t find_free_slot() const;

    // 将指针转换为索引
    size_t pointer_to_index(void* ptr) const;

    // 将索引转换为指针
    void* index_to_pointer(size_t index) const;

    MemorySource& memory_source_;
    void* pool_memory_;            // 池内存起始地址
    size_t pool_size_;             // 池总大小（字节）
    size_t object_size_;           // 对象大小（字节）
    size_t pool_capacity_;         // 池容量（对象数）
    size_t used_count_;            // 已使用对象数

    std::vector<uint8_t> bitmap_;  // 位图（每个对象1位）

    AllocatorStats stats_;
    size_t last_allocated_index_;  // 上次分配的索引（优化查找）

    // 禁用拷贝
    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;
};

} // namespace memplumber
