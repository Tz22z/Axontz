#include "axontzz/slab_allocator.h"
#include <cassert>
#include <iostream>
#include <algorithm>
#include <cstring>

namespace memplumber {

SlabAllocator::SlabAllocator(MemorySource& memory_source,
                             size_t object_size,
                             size_t objects_per_slab)
    : memory_source_(memory_source)
    , object_size_(std::max(object_size, MIN_OBJECT_SIZE))
    , actual_object_size_(align_size(object_size_, sizeof(void*)))
    , objects_per_slab_(objects_per_slab)
    , slab_count_(0)
    , slab_list_head_(nullptr)
    , global_free_list_(nullptr)
    , stats_{} {

    std::cout << "SlabAllocator 创建：object_size=" << object_size_
              << ", actual_size=" << actual_object_size_
              << ", objects_per_slab=" << objects_per_slab_ << std::endl;

    // 验证参数
    assert(objects_per_slab_ > 0);
    assert(actual_object_size_ >= MIN_OBJECT_SIZE);

    // 分配初始 slab
    SlabHeader* initial_slab = allocate_new_slab();
    if (initial_slab == nullptr) {
        throw std::bad_alloc();
    }

    std::cout << "SlabAllocator 初始化完成" << std::endl;
}

SlabAllocator::~SlabAllocator() {
    std::cout << "SlabAllocator 销毁：分配了 " << slab_count_ << " 个 slabs" << std::endl;

    // 释放所有 slabs
    SlabHeader* current = slab_list_head_;
    while (current != nullptr) {
        SlabHeader* next = current->next_slab;
        std::cout << "释放 slab 位于 " << current->slab_start
                  << "（大小：" << current->slab_size << " 字节）" << std::endl;
        memory_source_.deallocate_block(current->slab_start, current->slab_size);
        current = next;
    }
}

void* SlabAllocator::allocate(size_t size, size_t alignment) {
    // Slab 分配器只处理其固定的对象大小
    if (size > object_size_) {
        std::cout << "请求大小 " << size << " 超过对象大小 "
                  << object_size_ << std::endl;
        stats_.failed_allocations++;
        return nullptr;
    }

    // 检查是否需要扩展（没有可用的自由槽位）
    if (global_free_list_ == nullptr) {
        std::cout << "自由列表为空，分配新 slab" << std::endl;
        SlabHeader* new_slab = allocate_new_slab();
        if (new_slab == nullptr) {
            std::cout << "分配新 slab 失败" << std::endl;
            stats_.failed_allocations++;
            return nullptr;
        }
    }

    // 从自由列表弹出
    void* ptr = remove_from_free_list();
    if (ptr == nullptr) {
        stats_.failed_allocations++;
        return nullptr;
    }

    // 更新 slab 的使用计数
    SlabHeader* slab = find_slab_for_pointer(ptr);
    if (slab != nullptr) {
        slab->objects_in_use++;
    }

    // 更新统计信息
    stats_.total_allocated += object_size_;
    stats_.current_usage += object_size_;
    stats_.allocation_count++;

    std::cout << "已分配 " << object_size_ << " 字节，地址 " << ptr
              << "（使用量：" << stats_.current_usage << "）" << std::endl;

    return ptr;
}

void SlabAllocator::deallocate(void* ptr, size_t size) {
    if (ptr == nullptr) {
        return;
    }

    std::cout << "释放对象，地址 " << ptr << std::endl;

    // 验证所有权
    if (!owns(ptr)) {
        std::cout << "警告：尝试释放不属于此分配器的指针" << std::endl;
        return;
    }

    // 查找拥有此指针的 slab
    SlabHeader* slab = find_slab_for_pointer(ptr);
    if (slab == nullptr) {
        std::cout << "警告：找不到指针 " << ptr << " 的 slab" << std::endl;
        return;
    }

    // 更新 slab 使用计数
    if (slab->objects_in_use > 0) {
        slab->objects_in_use--;
    }

    // 返回到自由列表
    add_to_free_list(ptr);

    // 更新统计信息
    stats_.total_deallocated += object_size_;
    stats_.current_usage -= object_size_;
    stats_.deallocation_count++;

    std::cout << "释放成功（使用量：" << stats_.current_usage << "）" << std::endl;
}

bool SlabAllocator::owns(void* ptr) const {
    return find_slab_for_pointer(ptr) != nullptr;
}

AllocatorInterface::AllocatorStats SlabAllocator::get_stats() const {
    return stats_;
}

void SlabAllocator::reset_stats() {
    stats_ = AllocatorStats{};
}

bool SlabAllocator::validate() const {
    // 基本验证：检查 slab 列表完整性
    size_t counted_slabs = 0;
    SlabHeader* current = slab_list_head_;

    while (current != nullptr) {
        counted_slabs++;

        // 验证 slab 头部
        if (current->slab_start == nullptr) {
            std::cout << "验证失败：Slab 的起始指针为空" << std::endl;
            return false;
        }

        if (current->objects_in_use > current->total_objects) {
            std::cout << "验证失败：Slab 使用的对象数超过总数" << std::endl;
            return false;
        }

        current = current->next_slab;
    }

    if (counted_slabs != slab_count_) {
        std::cout << "验证失败：Slab 计数不匹配" << std::endl;
        return false;
    }

    return true;
}

void SlabAllocator::dump_slabs() const {
    std::cout << "=== Slab 分配器转储 ===" << std::endl;
    std::cout << "对象大小：" << object_size_ << " 字节（对齐后："
              << actual_object_size_ << "）" << std::endl;
    std::cout << "每个 slab 的对象数：" << objects_per_slab_ << std::endl;
    std::cout << "总 slab 数：" << slab_count_ << std::endl;
    std::cout << std::endl;

    std::cout << "统计信息：" << std::endl;
    std::cout << "  总分配量：" << stats_.total_allocated << " 字节" << std::endl;
    std::cout << "  总释放量：" << stats_.total_deallocated << " 字节" << std::endl;
    std::cout << "  当前使用量：" << stats_.current_usage << " 字节" << std::endl;
    std::cout << "  分配次数：" << stats_.allocation_count << std::endl;
    std::cout << "  释放次数：" << stats_.deallocation_count << std::endl;
    std::cout << "  失败分配：" << stats_.failed_allocations << std::endl;
    std::cout << std::endl;

    std::cout << "Slab 详情：" << std::endl;
    SlabHeader* current = slab_list_head_;
    size_t slab_index = 0;
    while (current != nullptr) {
        std::cout << "  Slab " << slab_index << "："
                  << current->slab_start << " - "
                  << (static_cast<char*>(current->slab_start) + current->slab_size)
                  << "（" << current->slab_size << " 字节）" << std::endl;
        std::cout << "    使用的对象：" << current->objects_in_use
                  << "/" << current->total_objects << std::endl;

        current = current->next_slab;
        slab_index++;
    }

    std::cout << "===========================" << std::endl;
}

// 私有辅助方法

SlabAllocator::SlabHeader* SlabAllocator::allocate_new_slab() {
    // 计算 slab 大小：
    // SlabHeader + (objects_per_slab * actual_object_size)
    size_t slab_size = sizeof(SlabHeader) + (objects_per_slab_ * actual_object_size_);

    std::cout << "分配新 slab（大小：" << slab_size << " 字节）" << std::endl;

    // 从操作系统获取内存
    void* slab_memory = memory_source_.allocate_block(slab_size);
    if (slab_memory == nullptr) {
        std::cout << "从内存源分配 slab 失败" << std::endl;
        return nullptr;
    }

    std::cout << "获得 slab 内存，地址 " << slab_memory << std::endl;

    // 初始化 slab
    SlabHeader* slab = static_cast<SlabHeader*>(slab_memory);
    initialize_slab(slab, slab_size);

    // 添加到 slab 列表
    slab->next_slab = slab_list_head_;
    slab_list_head_ = slab;
    slab_count_++;

    std::cout << "新 slab 初始化完成，包含 " << slab->total_objects << " 个对象" << std::endl;

    return slab;
}

void SlabAllocator::initialize_slab(SlabHeader* slab, size_t slab_size) {
    slab->slab_start = slab;
    slab->slab_size = slab_size;
    slab->objects_in_use = 0;
    slab->total_objects = objects_per_slab_;
    slab->next_slab = nullptr;
    slab->free_list_head = nullptr;

    // 计算对象区域的起始位置（在头部之后）
    char* object_area = reinterpret_cast<char*>(slab) + sizeof(SlabHeader);

    // 将所有对象槽位初始化为自由并添加到全局自由列表
    for (size_t i = 0; i < objects_per_slab_; ++i) {
        void* slot = object_area + (i * actual_object_size_);
        add_to_free_list(slot);
    }

    std::cout << "初始化 slab：" << objects_per_slab_ << " 个槽位，地址 " << slab << std::endl;
}

void SlabAllocator::add_to_free_list(void* slot) {
    if (slot == nullptr) {
        return;
    }

    // 使用槽位本身存储自由列表指针（侵入式列表）
    FreeSlot* free_slot = static_cast<FreeSlot*>(slot);
    free_slot->next = global_free_list_;
    global_free_list_ = free_slot;
}

void* SlabAllocator::remove_from_free_list() {
    if (global_free_list_ == nullptr) {
        return nullptr;
    }

    FreeSlot* slot = global_free_list_;
    global_free_list_ = slot->next;

    return static_cast<void*>(slot);
}

SlabAllocator::SlabHeader* SlabAllocator::find_slab_for_pointer(void* ptr) const {
    if (ptr == nullptr) {
        return nullptr;
    }

    char* check_ptr = static_cast<char*>(ptr);

    // 搜索所有 slabs
    SlabHeader* current = slab_list_head_;
    while (current != nullptr) {
        char* slab_start = static_cast<char*>(current->slab_start);
        char* slab_end = slab_start + current->slab_size;

        if (check_ptr >= slab_start && check_ptr < slab_end) {
            return current;
        }

        current = current->next_slab;
    }

    return nullptr;
}

size_t SlabAllocator::align_size(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

bool SlabAllocator::is_aligned(void* ptr, size_t alignment) {
    return (reinterpret_cast<uintptr_t>(ptr) & (alignment - 1)) == 0;
}

// ============================================================================
// SlabCache 实现
// ============================================================================

namespace {
    // 默认尺寸类（从 16 到 4096 的 2 的幂）
    constexpr size_t DEFAULT_SIZE_CLASSES[] = {
        16, 32, 64, 128, 256, 512, 1024, 2048, 4096
    };
    constexpr size_t DEFAULT_NUM_CLASSES = sizeof(DEFAULT_SIZE_CLASSES) / sizeof(size_t);
}

SlabCache::SlabCache(MemorySource& memory_source,
                     const size_t* size_classes,
                     size_t num_classes)
    : memory_source_(memory_source)
    , allocators_(nullptr)
    , size_classes_(nullptr)
    , num_classes_(num_classes)
    , aggregate_stats_{} {

    std::cout << "SlabCache 创建，包含 " << num_classes << " 个尺寸类" << std::endl;

    // 为尺寸类和分配器分配数组
    size_classes_ = new size_t[num_classes_];
    allocators_ = new SlabAllocator*[num_classes_];

    // 复制尺寸类
    for (size_t i = 0; i < num_classes_; ++i) {
        size_classes_[i] = size_classes[i];
        allocators_[i] = nullptr;
    }

    // 为每个尺寸类创建 slab 分配器
    for (size_t i = 0; i < num_classes_; ++i) {
        std::cout << "为尺寸类 " << size_classes_[i] << " 创建 SlabAllocator" << std::endl;
        allocators_[i] = new SlabAllocator(memory_source_, size_classes_[i], 64);
    }

    std::cout << "SlabCache 初始化完成" << std::endl;
}

SlabCache::SlabCache(MemorySource& memory_source)
    : SlabCache(memory_source, DEFAULT_SIZE_CLASSES, DEFAULT_NUM_CLASSES) {
}

SlabCache::~SlabCache() {
    std::cout << "SlabCache 销毁" << std::endl;

    // 删除所有 slab 分配器
    if (allocators_ != nullptr) {
        for (size_t i = 0; i < num_classes_; ++i) {
            delete allocators_[i];
        }
        delete[] allocators_;
    }

    // 删除尺寸类数组
    if (size_classes_ != nullptr) {
        delete[] size_classes_;
    }
}

void* SlabCache::allocate(size_t size, size_t alignment) {
    if (size == 0) {
        return nullptr;
    }

    // 查找合适的分配器
    int allocator_index = find_allocator_for_size(size);
    if (allocator_index < 0) {
        std::cout << "没有适合大小 " << size << " 的分配器" << std::endl;
        aggregate_stats_.failed_allocations++;
        return nullptr;
    }

    std::cout << "将 " << size << " 字节的分配路由到尺寸类 "
              << size_classes_[allocator_index] << std::endl;

    // 从合适的 slab 分配器分配
    void* ptr = allocators_[allocator_index]->allocate(size, alignment);

    if (ptr != nullptr) {
        // 使用实际分配的尺寸类大小进行统计
        aggregate_stats_.total_allocated += size_classes_[allocator_index];
        aggregate_stats_.current_usage += size_classes_[allocator_index];
        aggregate_stats_.allocation_count++;
    } else {
        aggregate_stats_.failed_allocations++;
    }

    return ptr;
}

void SlabCache::deallocate(void* ptr, size_t size) {
    if (ptr == nullptr) {
        return;
    }

    // 查找拥有此指针的分配器
    for (size_t i = 0; i < num_classes_; ++i) {
        if (allocators_[i]->owns(ptr)) {
            allocators_[i]->deallocate(ptr, size);

            aggregate_stats_.total_deallocated += size_classes_[i];
            aggregate_stats_.current_usage -= size_classes_[i];
            aggregate_stats_.deallocation_count++;
            return;
        }
    }

    std::cout << "警告：没有分配器拥有指针 " << ptr << std::endl;
}

bool SlabCache::owns(void* ptr) const {
    for (size_t i = 0; i < num_classes_; ++i) {
        if (allocators_[i]->owns(ptr)) {
            return true;
        }
    }
    return false;
}

AllocatorInterface::AllocatorStats SlabCache::get_stats() const {
    return aggregate_stats_;
}

void SlabCache::reset_stats() {
    aggregate_stats_ = AllocatorStats{};
    for (size_t i = 0; i < num_classes_; ++i) {
        allocators_[i]->reset_stats();
    }
}

void SlabCache::dump_cache_stats() const {
    std::cout << "=== SlabCache 统计信息 ===" << std::endl;
    std::cout << "尺寸类数量：" << num_classes_ << std::endl;
    std::cout << std::endl;

    std::cout << "汇总统计信息：" << std::endl;
    std::cout << "  总分配量：" << aggregate_stats_.total_allocated << " 字节" << std::endl;
    std::cout << "  总释放量：" << aggregate_stats_.total_deallocated << " 字节" << std::endl;
    std::cout << "  当前使用量：" << aggregate_stats_.current_usage << " 字节" << std::endl;
    std::cout << "  分配次数：" << aggregate_stats_.allocation_count << std::endl;
    std::cout << "  释放次数：" << aggregate_stats_.deallocation_count << std::endl;
    std::cout << "  失败分配：" << aggregate_stats_.failed_allocations << std::endl;
    std::cout << std::endl;

    std::cout << "各尺寸类统计信息：" << std::endl;
    for (size_t i = 0; i < num_classes_; ++i) {
        auto stats = allocators_[i]->get_stats();
        std::cout << "  尺寸类 " << size_classes_[i] << " 字节：" << std::endl;
        std::cout << "    分配次数：" << stats.allocation_count << std::endl;
        std::cout << "    释放次数：" << stats.deallocation_count << std::endl;
        std::cout << "    当前使用量：" << stats.current_usage << " 字节" << std::endl;
        std::cout << "    Slabs 数量：" << allocators_[i]->get_slab_count() << std::endl;
    }

    std::cout << "============================" << std::endl;
}

int SlabCache::find_allocator_for_size(size_t size) const {
    // 找到能容纳请求大小的最小尺寸类
    for (size_t i = 0; i < num_classes_; ++i) {
        if (size <= size_classes_[i]) {
            return static_cast<int>(i);
        }
    }

    // 大小对于任何分配器都太大
    return -1;
}

} // namespace memplumber
