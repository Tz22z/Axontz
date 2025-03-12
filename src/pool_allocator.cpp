#include "axontzz/pool_allocator.h"
#include <iostream>
#include <cassert>
#include <cstring>

namespace memplumber {

PoolAllocator::PoolAllocator(MemorySource& memory_source,
                             size_t object_size,
                             size_t pool_capacity)
    : memory_source_(memory_source)
    , pool_memory_(nullptr)
    , pool_size_(0)
    , object_size_(object_size)
    , pool_capacity_(pool_capacity)
    , used_count_(0)
    , stats_{}
    , last_allocated_index_(0) {

    std::cout << "PoolAllocator 创建：object_size=" << object_size_
              << ", capacity=" << pool_capacity_ << std::endl;

    // 验证参数
    assert(object_size_ > 0);
    assert(pool_capacity_ > 0);

    // 计算池大小
    pool_size_ = object_size_ * pool_capacity_;

    // 从内存源分配池内存
    pool_memory_ = memory_source_.allocate_block(pool_size_);
    if (pool_memory_ == nullptr) {
        throw std::bad_alloc();
    }

    std::cout << "分配池内存：" << pool_memory_ << "（" << pool_size_ << " 字节）" << std::endl;

    // 初始化位图（每个对象1位）
    size_t bitmap_size = (pool_capacity_ + 7) / 8; // 向上取整到字节
    bitmap_.resize(bitmap_size, 0); // 全部初始化为0（空闲）

    std::cout << "位图大小：" << bitmap_size << " 字节（" << pool_capacity_ << " 位）" << std::endl;
    std::cout << "PoolAllocator 初始化完成" << std::endl;
}

PoolAllocator::~PoolAllocator() {
    std::cout << "PoolAllocator 销毁：使用 " << used_count_ << "/" << pool_capacity_ << " 对象" << std::endl;

    if (used_count_ > 0) {
        std::cout << "警告：池中仍有 " << used_count_ << " 个对象未释放" << std::endl;
    }

    // 释放池内存
    if (pool_memory_ != nullptr) {
        memory_source_.deallocate_block(pool_memory_, pool_size_);
    }
}

void* PoolAllocator::allocate(size_t size, size_t alignment) {
    (void)alignment; // Pool 不处理对齐

    // 检查大小
    if (size > object_size_) {
        std::cout << "请求大小 " << size << " 超过对象大小 " << object_size_ << std::endl;
        stats_.failed_allocations++;
        return nullptr;
    }

    // 检查是否已满
    if (is_full()) {
        std::cout << "池已满（" << pool_capacity_ << "/" << pool_capacity_ << "）" << std::endl;
        stats_.failed_allocations++;
        return nullptr;
    }

    // 查找空闲槽位
    size_t index = find_free_slot();
    if (index >= pool_capacity_) {
        std::cout << "找不到空闲槽位（内部错误）" << std::endl;
        stats_.failed_allocations++;
        return nullptr;
    }

    // 标记为已用
    set_bit(index);
    used_count_++;
    last_allocated_index_ = index;

    // 计算对象地址
    void* ptr = index_to_pointer(index);

    // 更新统计
    stats_.total_allocated += object_size_;
    stats_.current_usage += object_size_;
    stats_.allocation_count++;

    std::cout << "分配对象 #" << index << " 地址 " << ptr
              << "（使用：" << used_count_ << "/" << pool_capacity_ << "）" << std::endl;

    return ptr;
}

void PoolAllocator::deallocate(void* ptr, size_t size) {
    (void)size;

    if (ptr == nullptr) {
        return;
    }

    // 验证所有权
    if (!owns(ptr)) {
        std::cout << "警告：指针 " << ptr << " 不属于此池" << std::endl;
        return;
    }

    // 计算索引
    size_t index = pointer_to_index(ptr);
    if (index >= pool_capacity_) {
        std::cout << "警告：索引 " << index << " 超出范围" << std::endl;
        return;
    }

    // 检查是否已经是空闲的
    if (!test_bit(index)) {
        std::cout << "警告：对象 #" << index << " 已经是空闲的（重复释放？）" << std::endl;
        return;
    }

    // 标记为空闲
    clear_bit(index);
    used_count_--;

    // 更新统计
    stats_.total_deallocated += object_size_;
    stats_.current_usage -= object_size_;
    stats_.deallocation_count++;

    std::cout << "释放对象 #" << index << "（使用：" << used_count_
              << "/" << pool_capacity_ << "）" << std::endl;
}

bool PoolAllocator::owns(void* ptr) const {
    if (ptr == nullptr || pool_memory_ == nullptr) {
        return false;
    }

    char* pool_start = static_cast<char*>(pool_memory_);
    char* pool_end = pool_start + pool_size_;
    char* check_ptr = static_cast<char*>(ptr);

    return (check_ptr >= pool_start && check_ptr < pool_end);
}

AllocatorInterface::AllocatorStats PoolAllocator::get_stats() const {
    AllocatorStats stats = stats_;

    // Pool 分配器没有碎片（所有对象大小相同）
    stats.fragmentation_ratio = 0.0;

    return stats;
}

void PoolAllocator::reset_stats() {
    stats_ = AllocatorStats{};
}

bool PoolAllocator::validate() const {
    // 验证使用计数与位图一致
    size_t counted_used = 0;
    for (size_t i = 0; i < pool_capacity_; ++i) {
        if (test_bit(i)) {
            counted_used++;
        }
    }

    if (counted_used != used_count_) {
        std::cout << "验证失败：使用计数不匹配（" << used_count_
                  << " vs " << counted_used << "）" << std::endl;
        return false;
    }

    return true;
}

void PoolAllocator::dump_pool() const {
    std::cout << "=== Pool 分配器转储 ===" << std::endl;
    std::cout << "对象大小：" << object_size_ << " 字节" << std::endl;
    std::cout << "池容量：" << pool_capacity_ << " 对象" << std::endl;
    std::cout << "已使用：" << used_count_ << " 对象（"
              << (used_count_ * 100.0 / pool_capacity_) << "%）" << std::endl;
    std::cout << "空闲：" << get_free_count() << " 对象" << std::endl;
    std::cout << std::endl;

    std::cout << "统计信息：" << std::endl;
    std::cout << "  总分配量：" << stats_.total_allocated << " 字节" << std::endl;
    std::cout << "  总释放量：" << stats_.total_deallocated << " 字节" << std::endl;
    std::cout << "  当前使用量：" << stats_.current_usage << " 字节" << std::endl;
    std::cout << "  分配次数：" << stats_.allocation_count << std::endl;
    std::cout << "  释放次数：" << stats_.deallocation_count << std::endl;
    std::cout << "  失败分配：" << stats_.failed_allocations << std::endl;
    std::cout << std::endl;

    // 显示位图状态（前64个对象）
    if (pool_capacity_ > 0) {
        std::cout << "位图状态（前 " << std::min(pool_capacity_, size_t(64)) << " 个对象）：" << std::endl;
        for (size_t i = 0; i < std::min(pool_capacity_, size_t(64)); ++i) {
            if (i % 32 == 0) {
                std::cout << "  ";
            }
            std::cout << (test_bit(i) ? '1' : '0');
            if ((i + 1) % 8 == 0) {
                std::cout << ' ';
            }
            if ((i + 1) % 32 == 0) {
                std::cout << std::endl;
            }
        }
        std::cout << std::endl;
    }

    std::cout << "==========================" << std::endl;
}

// 私有辅助方法

void PoolAllocator::set_bit(size_t index) {
    size_t byte_index = index / 8;
    size_t bit_index = index % 8;
    bitmap_[byte_index] |= (1 << bit_index);
}

void PoolAllocator::clear_bit(size_t index) {
    size_t byte_index = index / 8;
    size_t bit_index = index % 8;
    bitmap_[byte_index] &= ~(1 << bit_index);
}

bool PoolAllocator::test_bit(size_t index) const {
    size_t byte_index = index / 8;
    size_t bit_index = index % 8;
    return (bitmap_[byte_index] & (1 << bit_index)) != 0;
}

size_t PoolAllocator::find_free_slot() const {
    // 从上次分配位置开始搜索（局部性优化）
    for (size_t i = 0; i < pool_capacity_; ++i) {
        size_t index = (last_allocated_index_ + i + 1) % pool_capacity_;
        if (!test_bit(index)) {
            return index;
        }
    }

    return pool_capacity_; // 未找到
}

size_t PoolAllocator::pointer_to_index(void* ptr) const {
    char* pool_start = static_cast<char*>(pool_memory_);
    char* obj_ptr = static_cast<char*>(ptr);
    size_t offset = obj_ptr - pool_start;
    return offset / object_size_;
}

void* PoolAllocator::index_to_pointer(size_t index) const {
    char* pool_start = static_cast<char*>(pool_memory_);
    return pool_start + (index * object_size_);
}

} // namespace memplumber
