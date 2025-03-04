#pragma once

#include "allocator_interface.h"
#include "memory_source.h"
#include <cstddef>
#include <cstdint>

namespace memplumber {

/**
 * Slab 分配器实现
 *
 * 经典内核风格分配器，专为固定大小对象分配优化
 * 灵感来自 Solaris/Linux 内核的 slab 分配器设计
 *
 * 核心特性：
 * - O(1) 分配和释放
 * - 同尺寸对象的最小碎片化
 * - 缓存友好的对象布局
 * - 每个 slab 的自由列表用于快速查找
 * - 需要时自动扩展 slab
 *
 * 架构：
 * - 每个 SlabAllocator 管理一种固定大小的对象
 * - 内存被划分为"slabs"（从 MemorySource 获取的大块）
 * - 每个 slab 包含多个固定大小的"槽位"用于对象
 * - 自由槽位通过侵入式自由列表跟踪
 *
 * 性能特征：
 * - 分配：O(1) - 从自由列表弹出
 * - 释放：O(1) - 推入自由列表
 * - 空间开销：每个自由槽位约 8 字节 + slab 元数据
 * - 在对象尺寸类内无外部碎片
 *
 * 使用场景：
 * - 游戏引擎实体（全部相同大小）
 * - 网络数据包缓冲区
 * - 数据库记录缓存
 * - 任何涉及大量同尺寸对象的场景
 */
class SlabAllocator : public AllocatorInterface {
public:
    /**
     * 构造函数
     * @param memory_source: 从操作系统获取内存的源
     * @param object_size: 要分配的对象的固定大小（必须 >= sizeof(void*)）
     * @param objects_per_slab: 每个 slab 中的对象数量（默认：64）
     */
    explicit SlabAllocator(MemorySource& memory_source,
                          size_t object_size,
                          size_t objects_per_slab = 64);

    ~SlabAllocator() override;

    // AllocatorInterface 实现
    void* allocate(size_t size, size_t alignment = sizeof(void*)) override;
    void deallocate(void* ptr, size_t size = 0) override;
    bool owns(void* ptr) const override;
    AllocatorStats get_stats() const override;
    void reset_stats() override;
    const char* get_name() const override { return "SlabAllocator"; }

    /**
     * 获取此分配器管理的固定对象大小
     */
    size_t get_object_size() const { return object_size_; }

    /**
     * 获取每个 slab 的对象数量
     */
    size_t get_objects_per_slab() const { return objects_per_slab_; }

    /**
     * 获取当前 slab 数量
     */
    size_t get_slab_count() const { return slab_count_; }

    /**
     * 检查 slab 分配器健康状态
     */
    bool validate() const;

    /**
     * 调试转储 slab 状态
     */
    void dump_slabs() const;

private:
    /**
     * Slab 元数据结构
     * 放置在每个 slab 的开头
     */
    struct SlabHeader {
        void* slab_start;        // 此 slab 的起始地址
        size_t slab_size;        // 此 slab 的总大小（字节）
        size_t objects_in_use;   // 此 slab 中已分配的对象数量
        size_t total_objects;    // 此 slab 可容纳的总对象数
        SlabHeader* next_slab;   // 链中的下一个 slab
        void* free_list_head;    // 此 slab 中自由槽位的头部
    };

    /**
     * 自由槽位结构（就地存储在自由槽位中）
     * 使用侵入式列表设计 - 自由槽位本身保存指针
     */
    struct FreeSlot {
        FreeSlot* next;  // 下一个自由槽位
    };

    // 最小对象大小必须能容纳一个指针
    static constexpr size_t MIN_OBJECT_SIZE = sizeof(void*);

    MemorySource& memory_source_;
    size_t object_size_;           // 每个对象的固定大小
    size_t actual_object_size_;    // 对齐后的对象大小
    size_t objects_per_slab_;      // 每个 slab 的对象数
    size_t slab_count_;            // 当前 slab 数量

    SlabHeader* slab_list_head_;   // slab 列表头部
    FreeSlot* global_free_list_;   // 跨所有 slab 的全局自由列表

    AllocatorStats stats_;

    // 内部辅助方法

    /**
     * 从 MemorySource 分配新 slab
     * @return: 指向新 slab 头部的指针，失败时返回 nullptr
     */
    SlabHeader* allocate_new_slab();

    /**
     * 初始化新分配的 slab
     * @param slab: 指向 slab 内存的指针
     * @param slab_size: slab 的大小（字节）
     */
    void initialize_slab(SlabHeader* slab, size_t slab_size);

    /**
     * 将槽位添加到全局自由列表
     * @param slot: 指向自由槽位的指针
     */
    void add_to_free_list(void* slot);

    /**
     * 从全局自由列表移除槽位
     * @return: 指向被移除槽位的指针，列表为空时返回 nullptr
     */
    void* remove_from_free_list();

    /**
     * 查找拥有给定指针的 slab
     * @param ptr: 要检查的指针
     * @return: 指向 slab 头部的指针，未找到时返回 nullptr
     */
    SlabHeader* find_slab_for_pointer(void* ptr) const;

    /**
     * 将大小对齐到所需边界
     */
    static size_t align_size(size_t size, size_t alignment);

    /**
     * 检查指针是否对齐
     */
    static bool is_aligned(void* ptr, size_t alignment);

    // 禁用拷贝
    SlabAllocator(const SlabAllocator&) = delete;
    SlabAllocator& operator=(const SlabAllocator&) = delete;
};

/**
 * SlabCache：管理不同尺寸类的多个 SlabAllocator
 *
 * 这类似于 Linux 的 kmalloc 缓存系统
 * 通过将请求路由到适当大小的 slab 分配器来提供通用分配
 *
 * 示例尺寸类：16, 32, 64, 128, 256, 512, 1024, 2048, 4096 字节
 */
class SlabCache : public AllocatorInterface {
public:
    /**
     * 使用自定义尺寸类的构造函数
     * @param memory_source: 所有 slab 分配器的内存源
     * @param size_classes: 要为其创建分配器的对象大小数组
     * @param num_classes: 尺寸类的数量
     */
    explicit SlabCache(MemorySource& memory_source,
                      const size_t* size_classes,
                      size_t num_classes);

    /**
     * 使用默认尺寸类的构造函数
     * 默认：16, 32, 64, 128, 256, 512, 1024, 2048, 4096
     */
    explicit SlabCache(MemorySource& memory_source);

    ~SlabCache() override;

    // AllocatorInterface 实现
    void* allocate(size_t size, size_t alignment = sizeof(void*)) override;
    void deallocate(void* ptr, size_t size = 0) override;
    bool owns(void* ptr) const override;
    AllocatorStats get_stats() const override;
    void reset_stats() override;
    const char* get_name() const override { return "SlabCache"; }

    /**
     * 获取尺寸类数量
     */
    size_t get_num_classes() const { return num_classes_; }

    /**
     * 转储所有尺寸类的统计信息
     */
    void dump_cache_stats() const;

private:
    MemorySource& memory_source_;
    SlabAllocator** allocators_;   // slab 分配器数组
    size_t* size_classes_;         // 尺寸类数组
    size_t num_classes_;           // 尺寸类数量

    AllocatorStats aggregate_stats_;

    /**
     * 查找给定大小的合适 slab 分配器
     * @param size: 请求的分配大小
     * @return: allocators_ 数组的索引，如果太大则返回 -1
     */
    int find_allocator_for_size(size_t size) const;

    /**
     * 初始化默认尺寸类
     */
    void initialize_default_classes();

    // 禁用拷贝
    SlabCache(const SlabCache&) = delete;
    SlabCache& operator=(const SlabCache&) = delete;
};

} // namespace memplumber
